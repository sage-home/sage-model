#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "core_allvars.h"
#include "core_save.h"
#include "core_utils.h"
#include "core_logging.h"
#include "../physics/legacy/model_misc.h"
#include "core_mymalloc.h"
#include "core_galaxy_extensions.h"

#include "../io/save_gals_binary.h"
#include "../io/io_interface.h"
#include "../io/io_validation.h"
#include "../io/io_property_serialization.h"

#ifdef HDF5
#include "../io/save_gals_hdf5.h"
#endif

/**
 * @brief Map I/O interface error codes to SAGE error types
 * 
 * @param io_error Error code from the I/O interface
 * @return Corresponding SAGE error type
 * 
 * This function maps error codes from the I/O interface system to SAGE's own
 * error codes, providing consistent error handling throughout the codebase.
 */
static int32_t map_io_error_to_sage_error(int io_error)
{
    switch (io_error) {
        case IO_ERROR_NONE:
            return EXIT_SUCCESS;
        case IO_ERROR_FILE_NOT_FOUND:
            return FILE_NOT_FOUND;
        case IO_ERROR_FORMAT_ERROR:
            return INVALID_OPTION_IN_PARAMS;
        case IO_ERROR_RESOURCE_LIMIT:
            return OUT_OF_MEMBLOCKS;
        case IO_ERROR_MEMORY_ALLOCATION:
            return MALLOC_FAILURE;
        case IO_ERROR_VALIDATION_FAILED:
            return INVALID_VALUE_READ_FROM_FILE;
#ifdef HDF5
        case IO_ERROR_HANDLE_INVALID:
            return HDF5_ERROR;
#endif
        case IO_ERROR_UNSUPPORTED_OP:
            return FILE_WRITE_ERROR;
        default:
            return INVALID_OPTION_IN_PARAMS;
    }
}

/**
 * @brief Log an I/O interface error with appropriate severity
 * 
 * @param context Context string for the error message
 * @param io_error Error code from the I/O interface
 * 
 * This function logs errors from the I/O interface system with appropriate
 * severity levels, providing context about where the error occurred.
 */
static void log_io_error(const char *context, int io_error)
{
    const char *error_msg = io_get_error_message();
    
    switch (io_error) {
        case IO_ERROR_NONE:
            // No error, nothing to log
            break;
        case IO_ERROR_FILE_NOT_FOUND:
        case IO_ERROR_FORMAT_ERROR:
        case IO_ERROR_MEMORY_ALLOCATION:
        case IO_ERROR_VALIDATION_FAILED:
            // Log as errors
            LOG_ERROR("%s: %s (code %d)", context, error_msg, io_error);
            break;
        case IO_ERROR_RESOURCE_LIMIT:
        case IO_ERROR_HANDLE_INVALID:
            // Log as warnings
            LOG_WARNING("%s: %s (code %d)", context, error_msg, io_error);
            break;
        default:
            // Unknown errors as errors
            LOG_ERROR("%s: Unknown error - %s (code %d)", context, error_msg, io_error);
            break;
    }
}

// Local Proto-Types //
int32_t generate_galaxy_indices(const struct halo_data *halos, const struct halo_aux_data *haloaux,
                                struct GALAXY *halogal, const int64_t numgals,
                                const int64_t treenr, const int32_t filenr,
                                const int64_t filenr_mulfac, const int64_t forestnr_mulfac,const struct params *run_params);

// Externally Visible Functions //

// Global flag to track I/O system initialization
static int io_system_initialized = 0;

// Open up all the required output files and remember their file handles.  These are placed into
// `save_info` for access later.
int32_t initialize_galaxy_files(const int rank, const struct forest_info *forest_info, struct save_info *save_info, const struct params *run_params)
{
    int32_t status;

    if(run_params->simulation.NumSnapOutputs > ABSOLUTEMAXSNAPS) {
        char error_buffer[256];
        snprintf(error_buffer, sizeof(error_buffer),
                "Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots. "
                "To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile",
                run_params->simulation.NumSnapOutputs, ABSOLUTEMAXSNAPS);
        io_set_error(IO_ERROR_RESOURCE_LIMIT, error_buffer);
        log_io_error("initialize_galaxy_files", IO_ERROR_RESOURCE_LIMIT);
        return map_io_error_to_sage_error(IO_ERROR_RESOURCE_LIMIT);
    }

#if USE_IO_INTERFACE
    // Initialize validation context
    struct validation_context val_ctx;
    validation_init(&val_ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Initialize I/O system if needed
    if (!io_system_initialized) {
        status = io_init();
        if (status != 0) {
            log_io_error("initialize_galaxy_files", io_get_last_error());
            return map_io_error_to_sage_error(io_get_last_error());
        }
        io_system_initialized = 1;
    }
    
    // Initialize io_handler_data
    save_info->io_handler.using_io_interface = 0;
    save_info->io_handler.handler = NULL;
    save_info->io_handler.format_data = NULL;
    save_info->io_handler.property_ctx = NULL;
    
    // Get handler for the specified format
    int format_id = io_map_output_format_to_format_id(run_params->io.OutputFormat);
    if (format_id >= 0) {
        save_info->io_handler.handler = io_get_handler_by_id(format_id);
        
        // Validate handler
        if (save_info->io_handler.handler != NULL) {
            // Validate required capabilities for initialization
            enum io_capabilities required_caps[] = {
                IO_CAP_CHUNKED_WRITE,
                IO_CAP_EXTENDED_PROPS
            };
            VALIDATE_FORMAT_CAPABILITIES(&val_ctx, save_info->io_handler.handler, required_caps, 
                                       sizeof(required_caps)/sizeof(required_caps[0]),
                                       "initialize_galaxy_files", "galaxy file initialization");
            
            // Check for extended property support
            if (global_extension_registry != NULL && global_extension_registry->num_extensions > 0) {
                // Check if handler supports extended properties
                if (!io_has_capability(save_info->io_handler.handler, IO_CAP_EXTENDED_PROPS)) {
                    VALIDATION_WARN(&val_ctx, VALIDATION_ERROR_FORMAT_INCOMPATIBLE, 
                                 VALIDATION_CHECK_FORMAT_CAPS, "initialize_galaxy_files",
                                 "Handler does not support extended properties, extensions will be ignored");
                } else {
                    // Allocate property serialization context
                    save_info->io_handler.property_ctx = malloc(sizeof(struct property_serialization_context));
                    if (save_info->io_handler.property_ctx == NULL) {
                        VALIDATION_ERROR(&val_ctx, VALIDATION_ERROR_RESOURCE_LIMIT,
                                      VALIDATION_CHECK_RESOURCE, "initialize_galaxy_files",
                                      "Failed to allocate memory for property serialization context");
                    } else {
                        // Initialize property serialization context
                        if (property_serialization_init(save_info->io_handler.property_ctx, SERIALIZE_EXPLICIT) != 0) {
                            VALIDATION_ERROR(&val_ctx, VALIDATION_ERROR_INTERNAL,
                                          VALIDATION_CHECK_PROPERTY_COMPAT, "initialize_galaxy_files",
                                          "Failed to initialize property serialization context");
                            free(save_info->io_handler.property_ctx);
                            save_info->io_handler.property_ctx = NULL;
                        } else {
                            // Add properties to context
                            if (property_serialization_add_properties(save_info->io_handler.property_ctx) != 0) {
                                VALIDATION_ERROR(&val_ctx, VALIDATION_ERROR_INTERNAL,
                                              VALIDATION_CHECK_PROPERTY_COMPAT, "initialize_galaxy_files",
                                              "Failed to add properties to serialization context");
                                property_serialization_cleanup(save_info->io_handler.property_ctx);
                                free(save_info->io_handler.property_ctx);
                                save_info->io_handler.property_ctx = NULL;
                            } else {
                                // Validate property serialization context
                                VALIDATE_SERIALIZATION_CONTEXT(&val_ctx, save_info->io_handler.property_ctx, 
                                                           "initialize_galaxy_files");
                                
                                // Validate properties against the format
                                for (int i = 0; i < save_info->io_handler.property_ctx->num_properties; i++) {
                                    int ext_id = save_info->io_handler.property_ctx->property_id_map[i];
                                    const galaxy_property_t *property = 
                                        galaxy_extension_find_property_by_id(ext_id);
                                    
                                    if (property != NULL) {
                                        // Validate property serialization
                                        VALIDATE_PROPERTY_SERIALIZATION(&val_ctx, property, "initialize_galaxy_files");
                                        
                                        // Validate property uniqueness
                                        VALIDATE_PROPERTY_UNIQUENESS(&val_ctx, property, "initialize_galaxy_files");
                                        
                                        // Validate property type
                                        VALIDATE_PROPERTY_TYPE(&val_ctx, property->type, 
                                                           "initialize_galaxy_files", property->name);
                                        
                                        // Validate format-specific compatibility
                                        if (save_info->io_handler.handler->format_id == IO_FORMAT_BINARY_OUTPUT) {
                                            VALIDATE_BINARY_PROPERTY_COMPATIBILITY(&val_ctx, property, 
                                                                               "initialize_galaxy_files");
                                        } else if (save_info->io_handler.handler->format_id == IO_FORMAT_HDF5_OUTPUT) {
                                            VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&val_ctx, property, 
                                                                             "initialize_galaxy_files");
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            // Report validation results
            if (validation_has_errors(&val_ctx)) {
                validation_report(&val_ctx);
                validation_cleanup(&val_ctx);
                
                // Clean up property context if it was created
                if (save_info->io_handler.property_ctx != NULL) {
                    property_serialization_cleanup(save_info->io_handler.property_ctx);
                    free(save_info->io_handler.property_ctx);
                    save_info->io_handler.property_ctx = NULL;
                }
                
                return map_io_error_to_sage_error(IO_ERROR_VALIDATION_FAILED);
            }
            
            // Construct output filename
            char filename[MAX_STRING_LEN];
            snprintf(filename, MAX_STRING_LEN, "%s/%s", 
                    run_params->io.OutputDir, run_params->io.FileNameGalaxies);
            
            // Initialize the handler
            status = save_info->io_handler.handler->initialize(filename, (struct params *)run_params, 
                                                            &save_info->io_handler.format_data);
            if (status == 0) {
                save_info->io_handler.using_io_interface = 1;
                validation_cleanup(&val_ctx);
                return EXIT_SUCCESS;
            } else {
                // Handle initialization error
                log_io_error("initialize_galaxy_files", io_get_last_error());
                save_info->io_handler.using_io_interface = 0;
                
                // Clean up property context if it was created
                if (save_info->io_handler.property_ctx != NULL) {
                    property_serialization_cleanup(save_info->io_handler.property_ctx);
                    free(save_info->io_handler.property_ctx);
                    save_info->io_handler.property_ctx = NULL;
                }
                
                // Fall through to traditional approach
            }
        } else {
            // Handler not found, fall through to traditional approach
            char error_buffer[256];
            snprintf(error_buffer, sizeof(error_buffer),
                    "No I/O handler found for format %d, falling back to traditional approach (code %d)", 
                    format_id, run_params->io.OutputFormat);
            io_set_error(IO_ERROR_FORMAT_ERROR, error_buffer);
            
            // Log as warning instead of error to avoid crashing
            LOG_WARNING("No I/O handler found for format %d, falling back to traditional approach (code %d)", 
                      format_id, run_params->io.OutputFormat);
        }
    }
    
    validation_cleanup(&val_ctx);
#endif

    // Fall back to existing approach if handler not available or I/O interface not enabled
    switch(run_params->io.OutputFormat) {
    case(sage_binary):
      status = initialize_binary_galaxy_files(rank, forest_info, save_info, run_params);
      break;

#ifdef HDF5
    case(sage_hdf5):
      status = initialize_hdf5_galaxy_files(rank, save_info, run_params);
      break;
#endif

    default:
      io_set_error(IO_ERROR_FORMAT_ERROR, "Unknown OutputFormat in initialize_galaxy_files()");
      log_io_error("initialize_galaxy_files", IO_ERROR_FORMAT_ERROR);
      status = map_io_error_to_sage_error(IO_ERROR_FORMAT_ERROR);
      break;
    }

    return status;
}


// Write all the galaxy properties to file.
int32_t save_galaxies(const int64_t task_forestnr, const int numgals, struct halo_data *halos,
                      struct forest_info *forest_info,
                      struct halo_aux_data *haloaux, struct GALAXY *halogal,
                      struct save_info *save_info, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;

    // Reset the output galaxy count.
    int32_t OutputGalCount[run_params->simulation.SimMaxSnaps];
    for(int32_t snap_idx = 0; snap_idx < run_params->simulation.SimMaxSnaps; snap_idx++) {
        OutputGalCount[snap_idx] = 0;
    }

    // Track the order in which galaxies are written.
    int32_t *OutputGalOrder = mymalloc(numgals * sizeof(*(OutputGalOrder)));
    if(OutputGalOrder == NULL) {
        char error_buffer[256];
        snprintf(error_buffer, sizeof(error_buffer),
                "Could not allocate memory for %d int elements in array `OutputGalOrder`", 
                numgals);
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, error_buffer);
        log_io_error("save_galaxies", IO_ERROR_MEMORY_ALLOCATION);
        return map_io_error_to_sage_error(IO_ERROR_MEMORY_ALLOCATION);
    }

    for(int32_t gal_idx = 0; gal_idx < numgals; gal_idx++) {
        OutputGalOrder[gal_idx] = -1;
        haloaux[gal_idx].output_snap_n = -1;
    }

    // First update mergeIntoID to point to the correct galaxy in the output.
    for(int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        for(int32_t gal_idx  = 0; gal_idx < numgals; gal_idx++) {
            if(halogal[gal_idx].SnapNum == run_params->simulation.ListOutputSnaps[snap_idx]) {
                OutputGalOrder[gal_idx] = OutputGalCount[snap_idx];
                OutputGalCount[snap_idx]++;
                haloaux[gal_idx].output_snap_n = snap_idx;
            }
        }
    }

    for(int32_t gal_idx = 0; gal_idx < numgals; gal_idx++) {
        const int mergeID = halogal[gal_idx].mergeIntoID;
        if(mergeID > -1) {
            if( ! (mergeID >= 0 && mergeID < numgals) ) {
                char error_buffer[512];
                snprintf(error_buffer, sizeof(error_buffer),
                        "For galaxy number %d, expected mergeintoID to be within [0, %d) but found mergeintoID = %d instead. "
                        "Additional debugging info: task_forestnr = %"PRId64", snapshot = %d, halonr = %d, MostBoundID = %lld",
                        gal_idx, numgals, mergeID, task_forestnr, halogal[gal_idx].SnapNum, 
                        halogal[gal_idx].HaloNr, halos[halogal[gal_idx].HaloNr].MostBoundID);
                io_set_error(IO_ERROR_VALIDATION_FAILED, error_buffer);
                log_io_error("save_galaxies", IO_ERROR_VALIDATION_FAILED);
                return map_io_error_to_sage_error(IO_ERROR_VALIDATION_FAILED);
            }
            halogal[gal_idx].mergeIntoID = OutputGalOrder[halogal[gal_idx].mergeIntoID];
        }
    }

    // Generate a unique GalaxyIndex for each galaxy.  To do this, we need to know a) the tree
    // number **from the original file** and b) the file number the tree is from.  Note: The tree
    // number we need is different from the ``forestnr`` parameter being used to process the forest
    // within SAGE; that ``forestnr`` is **task local** and potentially does **NOT** correspond to
    // the tree number in the original simulation file.

    // When we allocated the trees to each task, we stored the correct tree and file numbers in
    // arrays indexed by the ``forestnr`` parameter.  Furthermore, since all galaxies being
    // processed belong to a single tree (by definition) and because trees cannot be split over
    // multiple files, we can access the tree + file number once and use it for all galaxies being
    // saved.
    int64_t original_treenr = forest_info->original_treenr[task_forestnr];
    int32_t original_filenr = forest_info->FileNr[task_forestnr];

    status = generate_galaxy_indices(halos, haloaux, halogal, numgals,
                                     original_treenr, original_filenr,
                                     run_params->runtime.FileNr_Mulfac, run_params->runtime.ForestNr_Mulfac,
                                     run_params);
    if(status != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

#if USE_IO_INTERFACE
    // Initialize validation context
    struct validation_context val_ctx;
    validation_init(&val_ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Check if using I/O interface
    if (save_info->io_handler.using_io_interface && save_info->io_handler.handler != NULL) {
        // Validate galaxies before saving
        for (int i = 0; i < numgals; i++) {
            // Only validate galaxies that will be written (those with output_snap_n >= 0)
            if (haloaux[i].output_snap_n >= 0) {
                // Validate galaxy data
                status = validation_check_galaxies(&val_ctx, &halogal[i], 1, 
                                               "save_galaxies", VALIDATION_CHECK_GALAXY_DATA);
                
                // Don't check every galaxy if there's an error to avoid too many messages
                if (status != 0) {
                    break;
                }
            }
        }
        
        // Check if validation failed
        if (validation_has_errors(&val_ctx)) {
            validation_report(&val_ctx);
            validation_cleanup(&val_ctx);
            myfree(OutputGalOrder);
            return map_io_error_to_sage_error(IO_ERROR_VALIDATION_FAILED);
        }
        
        // If the handler supports write_galaxies, use it
        if (save_info->io_handler.handler->write_galaxies != NULL) {
            // Count galaxies per snapshot for output
            int ngals_per_snap[run_params->simulation.NumSnapOutputs];
            for (int32_t i = 0; i < run_params->simulation.NumSnapOutputs; i++) {
                ngals_per_snap[i] = OutputGalCount[i];
            }
            
            // Call handler's write_galaxies function
            status = save_info->io_handler.handler->write_galaxies(halogal, numgals, 
                                                                save_info, 
                                                                save_info->io_handler.format_data);
            
            if (status != 0) {
                log_io_error("save_galaxies", io_get_last_error());
                // Do not fall through to traditional approach; if the handler failed, there's likely an issue
                validation_cleanup(&val_ctx);
            } else {
                // If successful, we're done
                validation_cleanup(&val_ctx);
                myfree(OutputGalOrder);
                return EXIT_SUCCESS;
            }
        }
    }
    
    validation_cleanup(&val_ctx);
#endif

    // Fall back to existing approach if not using I/O interface or if it failed
    switch(run_params->io.OutputFormat) {
    case(sage_binary):
        status = save_binary_galaxies(task_forestnr, numgals, OutputGalCount, forest_info,
                                      halos, haloaux, halogal, save_info, run_params);
        break;

#ifdef HDF5
    case(sage_hdf5):
        status = save_hdf5_galaxies(task_forestnr, numgals, forest_info, halos, haloaux, halogal, save_info, run_params);
        break;
#endif

    default:
        io_set_error(IO_ERROR_FORMAT_ERROR, "Unknown OutputFormat in save_galaxies()");
        log_io_error("save_galaxies", IO_ERROR_FORMAT_ERROR);
        status = map_io_error_to_sage_error(IO_ERROR_FORMAT_ERROR);
    }

    myfree(OutputGalOrder);
    return status;
}


// Write any remaining attributes or header information, close all the open files and free all the
// relevant dataspaces.
int32_t finalize_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;

#if USE_IO_INTERFACE
    // Initialize validation context
    struct validation_context val_ctx;
    validation_init(&val_ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Check if using I/O interface
    if (save_info->io_handler.using_io_interface && save_info->io_handler.handler != NULL) {
        // If the handler supports cleanup, use it
        if (save_info->io_handler.handler->cleanup != NULL) {
            // Validate format supports required capabilities
            enum io_capabilities required_caps[] = {
                IO_CAP_CHUNKED_WRITE  // Must support writing galaxies to finalize
            };
            VALIDATE_FORMAT_CAPABILITIES(&val_ctx, save_info->io_handler.handler, required_caps, 
                                       sizeof(required_caps)/sizeof(required_caps[0]),
                                       "finalize_galaxy_files", "galaxy file finalization");
            
            // Check for validation errors
            if (validation_has_errors(&val_ctx)) {
                validation_report(&val_ctx);
                validation_cleanup(&val_ctx);
                return map_io_error_to_sage_error(IO_ERROR_VALIDATION_FAILED);
            }
            
            // Call handler's cleanup function
            status = save_info->io_handler.handler->cleanup(save_info->io_handler.format_data);
            
            if (status != 0) {
                log_io_error("finalize_galaxy_files", io_get_last_error());
                validation_cleanup(&val_ctx);
                // Fall through to traditional approach as a last resort
            } else {
                // If successful, clean up the handler data
                save_info->io_handler.handler = NULL;
                save_info->io_handler.format_data = NULL;
                
                // Clean up property context if it exists
                if (save_info->io_handler.property_ctx != NULL) {
                    property_serialization_cleanup(save_info->io_handler.property_ctx);
                    free(save_info->io_handler.property_ctx);
                    save_info->io_handler.property_ctx = NULL;
                }
                
                save_info->io_handler.using_io_interface = 0;
                validation_cleanup(&val_ctx);
                return EXIT_SUCCESS;
            }
        }
    }
    
    validation_cleanup(&val_ctx);
#endif

    // Fall back to existing approach if not using I/O interface or if it failed
    switch(run_params->io.OutputFormat) {
    case(sage_binary):
        status = finalize_binary_galaxy_files(forest_info, save_info, run_params);
        break;

#ifdef HDF5
    case(sage_hdf5):
        status = finalize_hdf5_galaxy_files(forest_info, save_info, run_params);
        break;
#endif

    default:
        io_set_error(IO_ERROR_FORMAT_ERROR, "Unknown OutputFormat in finalize_galaxy_files()");
        log_io_error("finalize_galaxy_files", IO_ERROR_FORMAT_ERROR);
        status = map_io_error_to_sage_error(IO_ERROR_FORMAT_ERROR);
        break;
    }

    return status;
}

// Local Functions //

// Generate a unique GalaxyIndex for each galaxy based on the file number, the file-local
// tree number and the tree-local galaxy number.  NOTE: Both the file number and the tree number are
// based on the **original simulation files**.  These may be different from the ``forestnr``
// parameter being used to process the forest within SAGE; that ``forestnr`` is **task local** and
// potentially does **NOT** correspond to the tree number in the original simulation file.
int32_t generate_galaxy_indices(const struct halo_data *halos, const struct halo_aux_data *haloaux,
                                struct GALAXY *halogal, const int64_t numgals,
                                const int64_t forestnr, const int32_t filenr,
                                const int64_t filenr_mulfac, const int64_t forestnr_mulfac,
                                const struct params *run_params)
{

    // Now generate the unique index for each galaxy.
    const enum Valid_TreeTypes TreeType = run_params->io.TreeType;
    for(int64_t gal_idx = 0; gal_idx < numgals; ++gal_idx) {
        struct GALAXY *this_gal = &halogal[gal_idx];

        uint32_t GalaxyNr = this_gal->GalaxyNr;
        uint32_t CentralGalaxyNr = halogal[haloaux[halos[this_gal->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr;

        /*MS: check that the mechanism would produce unique galaxyindex within this run (across all tasks and all forests)*/
        if(GalaxyNr > forestnr_mulfac || (filenr_mulfac > 0 && forestnr*forestnr_mulfac > filenr_mulfac)) {
            fprintf(stderr, "When determining a unique Galaxy Number, we assume two things\n"
                    "1. Current galaxy numnber = %u is less than multiplication factor for trees (=%"PRId64")\n"
                    "2. That (the total number of trees * tree multiplication factor = %"PRId64") is less than the file "\
                    "multiplication factor = %"PRId64" (only relevant if file multiplication factor is non-zero).\n"
                    "At least one of these two assumptions have been broken.\n"
                    "Simulation trees file number %d\tOriginal tree number %"PRId64"\tGalaxy Number %d "
                    "forestnr_mulfac = %"PRId64" forestnr*forestnr_mulfac = %"PRId64"\n", GalaxyNr, forestnr_mulfac,
                    forestnr*forestnr_mulfac, filenr_mulfac,
                    filenr, forestnr, GalaxyNr, forestnr_mulfac, forestnr*forestnr_mulfac);
            switch (TreeType)
        {
#ifdef HDF5
        case lhalo_hdf5:
            //MS: 22/07/2021 - Why is firstfile, lastfile still passed even though those could be constructef
            //from run_params (like done within this __FUNCTION__)
            fprintf(stderr,"It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "\
                    "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 264 in src/io/read_tree_lhalo_hdf5.c. "\
                    "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "\
                    "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit\n.");
            break;

        case gadget4_hdf5:
            fprintf(stderr,"It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "\
                    "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 536 in src/io/read_tree_gadget4_hdf5.c. "\
                    "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "\
                    "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.\n");
            break;

        case genesis_hdf5:
            fprintf(stderr,"It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "\
                    "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 492 in src/io/read_tree_genesis_hdf5.c. "\
                    "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "\
                    "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.\n");
            break;

        case consistent_trees_hdf5:
            fprintf(stderr,"It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "\
                    "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 389 in src/io/read_tree_consistentrees_hdf5.c. "\
                    "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "\
                    "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.\n");
            break;
#endif

        case lhalo_binary:
            fprintf(stderr,"It is likely that your tree file contains too many trees or a tree contains too many galaxies, you can increase the maximum number "\
                    "of trees per file with the parameter run_params->runtime.FileNr_Mulfac at l. 226 in src/io/read_tree_lhalo_binary.c. "\
                    "If a tree contains too many galaxies, you can increase run_params->runtime.ForestNr_Mulfac in the same location. "\
                    "If all trees are stored in a single file, FileNr_Mulfac can in principle be set to zero to remove the limit.\n");

            break;

        case consistent_trees_ascii:
            fprintf(stderr,"It is likely that you have a tree with too many galaxies. For consistent trees the number of galaxies per trees "\
                    "is limited for the ID to to fit in 64 bits, see run_params->runtime.ForestNr_Mulfac at l. 319 in src/io/read_tree_consistentrees_ascii.c. "\
                    "If you have not set a finite run_params->runtime.FileNr_Mulfac, this format may not be ideal for your purpose.\n");
            break;

        default:
            fprintf(stderr, "Your tree type has not been included in the switch statement for function ``%s`` in file ``%s``.\n", __FUNCTION__, __FILE__);
            fprintf(stderr, "Please add it there.\n");
            return INVALID_OPTION_IN_PARAMS;
        }
            
          return EXIT_FAILURE;
        }

        /*MS: 23/9/2019 Check if the multiplication will overflow 64-bit integer */
        if((forestnr > 0 && ((uint64_t) forestnr > (0xFFFFFFFFFFFFFFFFULL/(uint64_t) forestnr_mulfac))) ||
           (filenr > 0 && ((uint64_t) filenr > (0xFFFFFFFFFFFFFFFFULL/(uint64_t) filenr_mulfac)))) {
            fprintf(stderr,"Error: While generating an unique Galaxy Index. The multiplication required to "
                    "generate the ID will overflow 64-bit\n"
                    "forestnr = %"PRId64" forestnr_mulfac = %"PRId64" filenr = %d filenr_mulfac = %"PRId64"\n",
                    forestnr, forestnr_mulfac, filenr, filenr_mulfac);

            return EXIT_FAILURE;
        }

        const uint64_t id_from_forestnr = forestnr_mulfac * forestnr;
        const uint64_t id_from_filenr = filenr_mulfac * filenr;

        if(id_from_forestnr > (0xFFFFFFFFFFFFFFFFULL - id_from_filenr)) {
            fprintf(stderr,"Error: While generating an unique Galaxy Index. The addition required to generate the ID will overflow 64-bits \n");
            fprintf(stderr,"id_from_forestnr = %"PRIu64 "id_from_filenr = %"PRIu64"\n", id_from_forestnr, id_from_filenr);
            return EXIT_FAILURE;
        }

        const uint64_t id_from_forest_and_file = id_from_forestnr + id_from_filenr;
        if((GalaxyNr > (0xFFFFFFFFFFFFFFFFULL - id_from_forest_and_file)) ||
           (CentralGalaxyNr > (0xFFFFFFFFFFFFFFFFULL - id_from_forest_and_file))) {
            fprintf(stderr,"Error: While generating an unique Galaxy Index. The addition required to generate the ID will overflow 64-bits \n");
            fprintf(stderr,"id_from_forest_and_file = %"PRIu64" GalaxyNr = %u CentralGalaxyNr = %u\n", id_from_forest_and_file, GalaxyNr, CentralGalaxyNr);
            return EXIT_FAILURE;
        }

        // Everything is good, generate the index.
        this_gal->GalaxyIndex = GalaxyNr + id_from_forest_and_file;
        this_gal->CentralGalaxyIndex= CentralGalaxyNr + id_from_forest_and_file;
    }

    return EXIT_SUCCESS;
}
