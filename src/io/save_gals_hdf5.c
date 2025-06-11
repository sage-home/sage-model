#include "save_gals_hdf5_internal.h"
#include "../core/core_logging.h"
#include "../core/core_module_system.h"
#include "../core/core_parameters.h"

// Function prototypes for the components were moved to the internal header

/**
 * @brief Process galaxy data into property buffers for HDF5 output following core-physics separation principles
 * 
 * This function transforms galaxy properties into HDF5-compatible buffers with robust error handling.
 * It ensures output files are always generated successfully, even when property data is missing or corrupted,
 * by leveraging the enhanced property transformer that provides scientifically reasonable default values.
 * 
 * @param g The galaxy to process
 * @param save_info HDF5 save information structure containing output buffers
 * @param snap_idx Snapshot index for buffer selection
 * @param halos Halo data array (reserved for future custom transformers)
 * @param task_forestnr Forest number (reserved for future custom transformers)  
 * @param original_treenr Original tree number (reserved for future custom transformers)
 * @param run_params Runtime parameters for property system access
 * @return EXIT_SUCCESS on successful processing, EXIT_FAILURE on critical errors
 */
static int32_t process_galaxy_for_output(const struct GALAXY *g, struct hdf5_save_info *save_info,
                                        const int32_t snap_idx, const struct halo_data *halos __attribute__((unused)),
                                        const int64_t task_forestnr __attribute__((unused)), const int64_t original_treenr __attribute__((unused)),
                                        const struct params *run_params)
{
    // Validate property buffers are allocated
    if (save_info->property_buffers[snap_idx] == NULL) {
        LOG_ERROR("Property buffers not allocated for snapshot %d", snap_idx);
        return EXIT_FAILURE;
    }
    
    // CRITICAL FIX: Check for corrupted properties pointer before processing
    if (g == NULL) {
        LOG_ERROR("NULL galaxy pointer in process_galaxy_for_output");
        return EXIT_FAILURE;
    }
    
    // CRITICAL: Check for corrupted properties pointer - FAIL HARD if detected
    if (g->properties != NULL && ((uintptr_t)g->properties & 0xFFFFFFFF00000000ULL) == 0xFFFFFFFF00000000ULL) {
        LOG_ERROR("FATAL: Corrupted galaxy properties pointer detected: %p for galaxy index %llu", 
                 g->properties, g->GalaxyIndex);
        LOG_ERROR("FATAL: This indicates memory corruption that would produce invalid scientific results");
        LOG_ERROR("FATAL: The simulation must be terminated to prevent data corruption");
        return EXIT_FAILURE;
    }
    
    // CRITICAL: Check for NULL properties - FAIL HARD if detected  
    if (g->properties == NULL) {
        LOG_ERROR("FATAL: Galaxy %llu has NULL properties pointer", g->GalaxyIndex);
        LOG_ERROR("FATAL: This would result in invalid scientific data - terminating");
        return EXIT_FAILURE;
    }
    
    int64_t gals_in_buffer = save_info->num_gals_in_buffer[snap_idx];
    
    // Process each property
    for (int i = 0; i < save_info->num_properties; i++) {
        struct property_buffer_info *buffer = &save_info->property_buffers[snap_idx][i];
        property_id_t prop_id = buffer->prop_id;
        
        // Use dispatch_property_transformer for all properties (core and physics)
        // This ensures consistency and automatically captures all core properties marked for output
        // Determine the element size for pointer arithmetic
        size_t element_size = 0;
        if (buffer->h5_dtype == H5T_NATIVE_FLOAT) {
            element_size = sizeof(float);
        }
        else if (buffer->h5_dtype == H5T_NATIVE_DOUBLE) {
            element_size = sizeof(double);
        }
        else if (buffer->h5_dtype == H5T_NATIVE_INT) {
            element_size = sizeof(int32_t);
        }
        else if (buffer->h5_dtype == H5T_NATIVE_LLONG) {
            element_size = sizeof(int64_t);
        }
        else if (buffer->h5_dtype == H5T_NATIVE_UINT64) {
            element_size = sizeof(uint64_t);
        }
        
        if (element_size > 0) {
            // Get the pointer to the output buffer element for this galaxy's property
            void *output_buffer_element_ptr = ((char*)buffer->data) + (gals_in_buffer * element_size);
            
            // Call the dispatcher to transform the property value for output
            dispatch_property_transformer(g, prop_id, buffer->name, output_buffer_element_ptr, 
                                        run_params, buffer->h5_dtype);
        }
    }
    
    return EXIT_SUCCESS;
}

// Main function to save galaxies to HDF5 format
int32_t save_hdf5_galaxies(const int64_t task_forestnr, const int32_t num_gals, struct forest_info *forest_info,
                          struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                          struct save_info *save_info_base, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;
    // Extract the HDF5-specific structure from the base structure
    struct hdf5_save_info *save_info = (struct hdf5_save_info *)save_info_base->buffer_output_gals;
    
    if (save_info == NULL) {
        LOG_ERROR("HDF5 save info not properly initialized");
        return EXIT_FAILURE;
    }

    for (int32_t gal_idx = 0; gal_idx < num_gals; gal_idx++) {
        // Only processing galaxies at selected snapshots
        if (haloaux[gal_idx].output_snap_n < 0) {
            continue;
        }

        // Add galaxy to buffer
        int32_t snap_idx = haloaux[gal_idx].output_snap_n;
        
        // Validate snap_idx bounds 
        if (snap_idx < 0 || snap_idx >= run_params->simulation.NumSnapOutputs) {
            LOG_ERROR("Invalid snap_idx %d (valid range: 0-%d)", 
                     snap_idx, run_params->simulation.NumSnapOutputs - 1);
            return EXIT_FAILURE;
        }
        
        // Validate that property buffers are allocated for this snapshot
        if (save_info->property_buffers[snap_idx] == NULL) {
            LOG_ERROR("property_buffers[%d] is NULL", snap_idx);
            return EXIT_FAILURE;
        }
        
        // CRITICAL: Check for corrupted properties pointer - FAIL HARD if detected
        if (halogal[gal_idx].properties != NULL && 
            ((uintptr_t)halogal[gal_idx].properties & 0xFFFFFFFF00000000ULL) == 0xFFFFFFFF00000000ULL) {
            
            LOG_ERROR("FATAL: Corrupted galaxy properties pointer detected at gal_idx %d: %p", 
                     gal_idx, halogal[gal_idx].properties);
            LOG_ERROR("FATAL: This indicates memory corruption that would produce invalid scientific results");
            LOG_ERROR("FATAL: The simulation must be terminated to prevent data corruption");
            return EXIT_FAILURE;
        }
        
        // Process galaxy data into property buffers using the HDF5 structure
        status = process_galaxy_for_output(&halogal[gal_idx], save_info, snap_idx, 
                                           halos, task_forestnr, forest_info->original_treenr[task_forestnr], 
                                           run_params);
        if (status != EXIT_SUCCESS) {
            return status;
        }
        
        save_info->num_gals_in_buffer[snap_idx]++;

        // Increment forest_ngals counter for this snapshot and forest
        // Validate task_forestnr bounds (forest_ngals allocated with fixed size)
        const int32_t max_forests = 100000; // Should match initialization value
        if (task_forestnr >= max_forests) {
            LOG_ERROR("task_forestnr %"PRId64" exceeds max_forests %d", 
                     task_forestnr, max_forests);
            return EXIT_FAILURE;
        }
        save_info->forest_ngals[snap_idx][task_forestnr]++;

        // Check if buffer is full and we need to write
        if (save_info->num_gals_in_buffer[snap_idx] == save_info->buffer_size) {
            status = trigger_buffer_write(snap_idx, save_info->buffer_size, save_info->tot_ngals[snap_idx], 
                                        save_info_base, run_params);
            if (status != EXIT_SUCCESS) {
                return status;
            }
        }
    }

    return EXIT_SUCCESS;
}

// Helper function to get parameter value using the auto-generated parameter accessor functions
static int32_t write_parameter_value_to_hdf5(hid_t group_id, const char *param_name, parameter_id_t param_id, 
                                              const char *param_type, const struct params *run_params) {
    // Use the auto-generated parameter accessor functions instead of hardcoded paths
    if (strcmp(param_type, "string") == 0) {
        const char *str_val = get_string_parameter(run_params, param_id, "");
        if (str_val == NULL) {
            // Handle NULL return - this should not happen with default value
            LOG_WARNING("get_string_parameter returned NULL for parameter '%s', using empty string", param_name);
            str_val = "";
        }
        if (strlen(str_val) > 0) {
            size_t str_len = strlen(str_val);
            CREATE_STRING_ATTRIBUTE(group_id, param_name, str_val, str_len);
        } else {
            // Handle empty strings with minimum length of 1
            CREATE_STRING_ATTRIBUTE(group_id, param_name, "", 1);
        }
    } 
    else if (strcmp(param_type, "int") == 0) {
        int int_val = get_int_parameter(run_params, param_id, 0);
        CREATE_SINGLE_ATTRIBUTE(group_id, param_name, int_val, H5T_NATIVE_INT);
    }
    else if (strcmp(param_type, "double") == 0) {
        double double_val = get_double_parameter(run_params, param_id, 0.0);
        CREATE_SINGLE_ATTRIBUTE(group_id, param_name, double_val, H5T_NATIVE_DOUBLE);
    }
    else if (strcmp(param_type, "bool") == 0) {
        // Treat boolean parameters as integers for HDF5
        int int_val = get_int_parameter(run_params, param_id, 0);
        CREATE_SINGLE_ATTRIBUTE(group_id, param_name, int_val, H5T_NATIVE_INT);
    }
    else if (strcmp(param_type, "int[]") == 0) {
        // Special handling for array parameters like ListOutputSnaps
        if (strcmp(param_name, "ListOutputSnaps") == 0) {
            // This is handled separately as a dataset in write_header, skip here
            return EXIT_SUCCESS;
        }
    }
    else {
        LOG_WARNING("Unsupported parameter type '%s' for parameter '%s', skipping", param_type, param_name);
    }
    
    return EXIT_SUCCESS;
}

// Write all core parameters dynamically using the parameter metadata
static int32_t write_core_parameters_to_hdf5(hid_t runtime_group_id, hid_t sim_group_id, const struct params *run_params) {
    extern parameter_meta_t PARAMETER_META[PARAM_COUNT];
    
    for (parameter_id_t param_id = 0; param_id < PARAM_COUNT; param_id++) {
        const parameter_meta_t *meta = &PARAMETER_META[param_id];
        
        // Only process core parameters (skip physics parameters - they have their own system)
        if (strcmp(meta->category, "core") != 0) {
            continue;
        }
        
        // Determine which group to write to (most go to runtime, some to sim)
        hid_t target_group_id = runtime_group_id;
        if (strstr(meta->struct_field, "simulation.") == meta->struct_field ||
            strstr(meta->struct_field, "cosmology.") == meta->struct_field) {
            target_group_id = sim_group_id;
        }
        
        // Use the dynamic parameter writer that leverages auto-generated accessors
        int32_t status = write_parameter_value_to_hdf5(target_group_id, meta->name, param_id, meta->type, run_params);
        if (status != EXIT_SUCCESS) {
            return status;
        }
    }
    
    return EXIT_SUCCESS;
}

// Write all parameters dynamically using the module parameter registries
static int32_t write_physics_parameters_to_hdf5(hid_t runtime_group_id, const struct physics_params *physics __attribute__((unused))) {
    // Use the module system's parameter registries to write all registered parameters
    // This eliminates hardcoded parameter names and maintains core-physics separation
    extern struct module_registry *global_module_registry;
    
    if (global_module_registry == NULL) {
        LOG_DEBUG("No global module registry available, skipping parameter writing");
        return EXIT_SUCCESS;
    }
    
    // Iterate through all registered modules and write their parameters
    for (int i = 0; i < global_module_registry->num_modules; i++) {
        if (!global_module_registry->modules[i].active || 
            global_module_registry->modules[i].parameter_registry == NULL) {
            continue;
        }
        
        module_parameter_registry_t *registry = global_module_registry->modules[i].parameter_registry;
        
        // Write all parameters from this module's registry
        for (int j = 0; j < registry->num_parameters; j++) {
            module_parameter_t *param = &registry->parameters[j];
            
            // Create HDF5 attribute based on parameter type
            switch (param->type) {
                case MODULE_PARAM_TYPE_INT:
                    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, param->name, param->value.int_val, H5T_NATIVE_INT);
                    break;
                    
                case MODULE_PARAM_TYPE_FLOAT:
                    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, param->name, param->value.float_val, H5T_NATIVE_FLOAT);
                    break;
                    
                case MODULE_PARAM_TYPE_DOUBLE:
                    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, param->name, param->value.double_val, H5T_NATIVE_DOUBLE);
                    break;
                    
                case MODULE_PARAM_TYPE_BOOL: {
                    // Write booleans as integers (0 or 1)
                    int32_t bool_as_int = (int32_t)param->value.bool_val;
                    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, param->name, bool_as_int, H5T_NATIVE_INT);
                    break;
                }
                    
                case MODULE_PARAM_TYPE_STRING:
                    CREATE_STRING_ATTRIBUTE(runtime_group_id, param->name, param->value.string_val, strlen(param->value.string_val));
                    break;
                    
                default:
                    LOG_WARNING("Unsupported parameter type %d for parameter '%s', skipping", 
                               param->type, param->name);
                    break;
            }
        }
    }
    
    return EXIT_SUCCESS;
}

// Write header information to HDF5 file
int32_t write_header(hid_t file_id, const struct forest_info *forest_info, const struct params *run_params) {
    // Create header subgroups
    hid_t sim_group_id = H5Gcreate2(file_id, "Header/Simulation", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(sim_group_id, (int32_t) sim_group_id,
                                    "Failed to create the Header/Simulation group.\nThe file ID was %d\n",
                                    (int32_t) file_id);

    hid_t runtime_group_id = H5Gcreate2(file_id, "Header/Runtime", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(runtime_group_id, (int32_t) runtime_group_id,
                                    "Failed to create the Header/Runtime group.\nThe file ID was %d\n",
                                    (int32_t) file_id);

    hid_t misc_group_id = H5Gcreate2(file_id, "Header/Misc", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(misc_group_id, (int32_t) misc_group_id,
                                    "Failed to create the Header/Miscgroup.\nThe file ID was %d\n",
                                    (int32_t) file_id);

    // Write all core parameters dynamically from parameters.yaml metadata
    // This replaces the previous hardcoded parameter sections and ensures
    // all parameters from parameters.yaml are automatically included
    write_core_parameters_to_hdf5(runtime_group_id, sim_group_id, run_params);
    
    // SimMaxSnaps is not in parameters.yaml - write it manually
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "SimMaxSnaps", run_params->simulation.SimMaxSnaps, H5T_NATIVE_INT);

    // If we're writing the header attributes for the master file, we don't have knowledge of trees
    if (forest_info != NULL) {
        CREATE_SINGLE_ATTRIBUTE(sim_group_id, "num_trees_this_file", forest_info->nforests_this_task, H5T_NATIVE_LLONG);
        CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "frac_volume_processed", forest_info->frac_volume_processed, H5T_NATIVE_DOUBLE);
    } else {
        const long long nforests_on_master_file = 0;
        CREATE_SINGLE_ATTRIBUTE(sim_group_id, "num_trees_this_file", nforests_on_master_file, H5T_NATIVE_LLONG);

        const double frac_volume_on_master = (run_params->io.LastFile - run_params->io.FirstFile + 1)/(double) run_params->io.NumSimulationTreeFiles;
        CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "frac_volume_processed", frac_volume_on_master, H5T_NATIVE_DOUBLE);
    }

    // Data and version information
    CREATE_SINGLE_ATTRIBUTE(misc_group_id, "num_cores", run_params->runtime.NTasks, H5T_NATIVE_INT);
    CREATE_STRING_ATTRIBUTE(misc_group_id, "sage_data_version", &SAGE_DATA_VERSION, strlen(SAGE_DATA_VERSION));
    CREATE_STRING_ATTRIBUTE(misc_group_id, "sage_version", &SAGE_VERSION, strlen(SAGE_VERSION));
    CREATE_STRING_ATTRIBUTE(misc_group_id, "git_SHA_reference", &GITREF_STR, strlen(GITREF_STR));

    // Dynamic physics parameters - uses existing parameter system to write all available physics parameters
    write_physics_parameters_to_hdf5(runtime_group_id, &run_params->physics);

    // Redshift at each snapshot
    hsize_t dims[1];
    dims[0] = run_params->simulation.Snaplistlen;

    CREATE_AND_WRITE_1D_ARRAY(file_id, "Header/snapshot_redshifts", dims, run_params->simulation.ZZ, H5T_NATIVE_DOUBLE);

    // Output snapshots
    dims[0] = run_params->simulation.NumSnapOutputs;

    CREATE_AND_WRITE_1D_ARRAY(file_id, "Header/output_snapshots", dims, run_params->simulation.ListOutputSnaps, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "NumOutputs", run_params->simulation.NumSnapOutputs, H5T_NATIVE_INT);

    // Close all header groups
    herr_t status = H5Gclose(sim_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close Header/Simulation group.\n"
                                    "The group ID was %d\n", (int32_t) sim_group_id);

    status = H5Gclose(runtime_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close Header/Runtime group.\n"
                                    "The group ID was %d\n", (int32_t) runtime_group_id);

    status = H5Gclose(misc_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close Header/Misc group.\n"
                                    "The group ID was %d\n", (int32_t) misc_group_id);

    return EXIT_SUCCESS;
}

// Commented out as it's already defined in prepare_galaxy_for_hdf5_output.c
/*
float get_float_array_property(const struct GALAXY *g, property_id_t prop_id, int index, float default_value) {
    // This is a placeholder function that would need real implementation
    // In a complete system, this would access array properties from the property system
    // For now, we'll just return the default value
    return default_value;
}
*/

// Property-based field metadata generation is now in generate_field_metadata.c

// Create HDF5 master file
int32_t create_hdf5_master_file(const struct params *run_params)
{
    // Only Task 0 needs to do stuff from here
    if (run_params->runtime.ThisTask > 0) {
        return EXIT_SUCCESS;
    }

    hid_t master_file_id, group_id, root_group_id;
    char master_fname[2*MAX_STRING_LEN + 6];
    herr_t status;

    // Create the file
    snprintf(master_fname, 2*MAX_STRING_LEN + 5, "%s/%s.hdf5", run_params->io.OutputDir, run_params->io.FileNameGalaxies);

    master_file_id = H5Fcreate(master_fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(master_file_id, FILE_NOT_FOUND,
                                    "Can't open file %s for master file writing.\n", master_fname);

    // We will keep track of how many galaxies were saved across all files per snapshot
    // We do this for each snapshot in the simulation, not only those that are output, to allow easy
    // checking of which snapshots were output
    int64_t *ngals_allfiles_snap = mycalloc(run_params->simulation.SimMaxSnaps, sizeof(*(ngals_allfiles_snap))); // Calloced because initially no galaxies.
    CHECK_POINTER_AND_RETURN_ON_NULL(ngals_allfiles_snap,
                                     "Failed to allocate %d elements of size %zu for ngals_allfiles_snaps.", run_params->simulation.SimMaxSnaps,
                                     sizeof(*(ngals_allfiles_snap)));

    // The master file will be accessed as (e.g.,) f["Core0"]["Snap_63"]["StellarMass"]
    // Hence we want to store the external links to the **root** group (i.e., "/")
    root_group_id = H5Gopen2(master_file_id, "/", H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(root_group_id, (int32_t) root_group_id,
                                    "Failed to open the root group for the master file.\nThe file ID was %d\n",
                                    (int32_t) master_file_id);

    // At this point, all the files of all other processors have been created. So iterate over the
    // number of processors and create links within this master file to those files
    char target_fname[3*MAX_STRING_LEN];
    char core_fname[MAX_STRING_LEN];

    for (int32_t task_idx = 0; task_idx < run_params->runtime.NTasks; ++task_idx) {
        snprintf(core_fname, MAX_STRING_LEN - 1, "Core_%d", task_idx);
        snprintf(target_fname, 3*MAX_STRING_LEN - 1, "./%s_%d.hdf5", run_params->io.FileNameGalaxies, task_idx);

        // Make a symlink to the root of the target file
        status = H5Lcreate_external(target_fname, "/", root_group_id, core_fname, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                        "Failed to create an external link to file %s from the master file.\n"
                                        "The group ID was %d and the group name was %s\n", target_fname,
                                        (int32_t) root_group_id, core_fname);
    }

    // We've finished with the linking. Now let's create some attributes and datasets inside the header group
    group_id = H5Gcreate2(master_file_id, "Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                    "Failed to create the Header group for the master file.\nThe file ID was %d\n",
                                    (int32_t) master_file_id);

    // When we're writing the header attributes for the master file, we don't have knowledge of trees.
    // So pass a NULL pointer here instead of `forest_info`.
    status = write_header(master_file_id, NULL, run_params);
    if (status != EXIT_SUCCESS) {
        return status;
    }

    status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the Header group for the master file."
                                    "The group ID was %d\n", (int32_t) group_id);

    // Finished creating links
    status = H5Gclose(root_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close root group for the master file %s\n"
                                    "The group ID was %d and the file ID was %d\n", master_fname,
                                    (int32_t) root_group_id, (int32_t) master_file_id);

    // Close the file
    status = H5Fclose(master_file_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the Master HDF5 file.\nThe file ID was %d\n",
                                    (int32_t) master_file_id);

    // Free resources
    myfree(ngals_allfiles_snap);

    return EXIT_SUCCESS;
}
