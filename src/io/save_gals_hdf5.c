#include "save_gals_hdf5_internal.h"

// Function prototypes for the components were moved to the internal header

// Main function to save galaxies to HDF5 format
int32_t save_hdf5_galaxies(const int64_t task_forestnr, const int32_t num_gals, struct forest_info *forest_info,
                          struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                          struct save_info *save_info_base, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;
    struct hdf5_save_info *save_info = (struct hdf5_save_info *)save_info_base->format_data;

    for (int32_t gal_idx = 0; gal_idx < num_gals; gal_idx++) {
        // Only processing galaxies at selected snapshots
        if (haloaux[gal_idx].output_snap_n < 0) {
            continue;
        }

        // Add galaxy to buffer
        int32_t snap_idx = haloaux[gal_idx].output_snap_n;
        status = prepare_galaxy_for_hdf5_output(&halogal[gal_idx], save_info_base, snap_idx, halos, task_forestnr,
                                              forest_info->original_treenr[task_forestnr], run_params);
        if (status != EXIT_SUCCESS) {
            return status;
        }
        save_info->num_gals_in_buffer[snap_idx]++;

        // Increment forest_ngals counter for this snapshot and forest
        save_info_base->forest_ngals[snap_idx][task_forestnr]++;

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

// Write header information to HDF5 file
static int32_t write_header(hid_t file_id, const struct forest_info *forest_info, const struct params *run_params) {
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

    // Simulation information
    CREATE_STRING_ATTRIBUTE(sim_group_id, "SimulationDir", &run_params->io.SimulationDir, strlen(run_params->io.SimulationDir));
    CREATE_STRING_ATTRIBUTE(sim_group_id, "FileWithSnapList", &run_params->io.FileWithSnapList, strlen(run_params->io.FileWithSnapList));
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "LastSnapshotNr", run_params->simulation.LastSnapshotNr, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "SimMaxSnaps", run_params->simulation.SimMaxSnaps, H5T_NATIVE_INT);

    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "omega_matter", run_params->cosmology.Omega, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "omega_lambda", run_params->cosmology.OmegaLambda, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "particle_mass", run_params->cosmology.PartMass, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "hubble_h", run_params->cosmology.Hubble_h, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "num_simulation_tree_files", run_params->io.NumSimulationTreeFiles, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "box_size", run_params->cosmology.BoxSize, H5T_NATIVE_DOUBLE);

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

    // Output file info
    CREATE_STRING_ATTRIBUTE(runtime_group_id, "FileNameGalaxies", &run_params->io.FileNameGalaxies, strlen(run_params->io.FileNameGalaxies));
    CREATE_STRING_ATTRIBUTE(runtime_group_id, "OutputDir", &run_params->io.OutputDir, strlen(run_params->io.OutputDir));
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FirstFile", run_params->io.FirstFile, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "LastFile", run_params->io.LastFile, H5T_NATIVE_INT);

    // Recipe flags
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "SFprescription", run_params->physics.SFprescription, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "AGNrecipeOn", run_params->physics.AGNrecipeOn, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "SupernovaRecipeOn", run_params->physics.SupernovaRecipeOn, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ReionizationOn", run_params->physics.ReionizationOn, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "DiskInstabilityOn", run_params->physics.DiskInstabilityOn, H5T_NATIVE_INT);

    // Model parameters
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "SfrEfficiency", run_params->physics.SfrEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FeedbackReheatingEpsilon", run_params->physics.FeedbackReheatingEpsilon, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FeedbackEjectionEfficiency", run_params->physics.FeedbackEjectionEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ReIncorporationFactor", run_params->physics.ReIncorporationFactor, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "RadioModeEfficiency", run_params->physics.RadioModeEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "QuasarModeEfficiency", run_params->physics.QuasarModeEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "BlackHoleGrowthRate", run_params->physics.BlackHoleGrowthRate, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ThreshMajorMerger", run_params->physics.ThreshMajorMerger, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ThresholdSatDisruption", run_params->physics.ThresholdSatDisruption, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "Yield", run_params->physics.Yield, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "RecycleFraction", run_params->physics.RecycleFraction, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FracZleaveDisk", run_params->physics.FracZleaveDisk, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "Reionization_z0", run_params->physics.Reionization_z0, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "Reionization_zr", run_params->physics.Reionization_zr, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "EnergySN", run_params->physics.EnergySN, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "EtaSN", run_params->physics.EtaSN, H5T_NATIVE_DOUBLE);

    // Misc runtime Parameters
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "UnitLength_in_cm", run_params->units.UnitLength_in_cm, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "UnitMass_in_g", run_params->units.UnitMass_in_g, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "UnitVelocity_in_cm_per_s", run_params->units.UnitVelocity_in_cm_per_s, H5T_NATIVE_DOUBLE);

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

// Implementation of the property system array accessor
static float get_float_array_property(const struct GALAXY *g, property_id_t prop_id, int index, float default_value) {
    // This is a placeholder function that would need real implementation
    // In a complete system, this would access array properties from the property system
    // For now, we'll just return the default value
    return default_value;
}

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
