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
#include "../physics/model_misc.h"
#include "core_mymalloc.h"
#include "core_galaxy_extensions.h"

#include "../io/save_gals_binary.h"
#include "../io/io_interface.h"

#ifdef HDF5
#include "../io/save_gals_hdf5.h"
#endif

/**
 * @brief Map I/O interface error codes to SAGE error types
 * 
 * @param io_error Error code from the I/O interface
 * @return Corresponding SAGE error type
 * 
 * @note Currently unused, will be integrated with error handling paths in future refinements.
 * This function is part of the comprehensive error handling framework planned for the I/O system.
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
 * @note Currently unused, will be integrated with error handling paths in future refinements.
 * This function provides standardized error logging for the I/O system.
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
static int io_initialized = 0;

// Open up all the required output files and remember their file handles.  These are placed into
// `save_info` for access later.
int32_t initialize_galaxy_files(const int rank, const struct forest_info *forest_info, struct save_info *save_info, const struct params *run_params)
{
    int32_t status;

    if(run_params->simulation.NumSnapOutputs > ABSOLUTEMAXSNAPS) {
        fprintf(stderr,"Error: Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots\n",
                run_params->simulation.NumSnapOutputs, ABSOLUTEMAXSNAPS);
        fprintf(stderr,"To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile\n");
        return INVALID_OPTION_IN_PARAMS;
    }

#if USE_IO_INTERFACE
    // Initialize I/O system if needed
    if (!io_initialized) {
        status = io_init();
        if (status != 0) {
            fprintf(stderr, "Error: Failed to initialize I/O interface system: %s\n", 
                    io_get_error_message());
            return INVALID_OPTION_IN_PARAMS;
        }
        io_initialized = 1;
    }
    
    // Initialize io_handler_data
    save_info->io_handler.using_io_interface = 0;
    save_info->io_handler.handler = NULL;
    save_info->io_handler.format_data = NULL;
    
    // Get handler for the specified format
    int format_id = io_map_output_format_to_format_id(run_params->io.OutputFormat);
    if (format_id >= 0) {
        save_info->io_handler.handler = io_get_handler_by_id(format_id);
        if (save_info->io_handler.handler != NULL) {
            // Construct output filename
            char filename[MAX_STRING_LEN];
            snprintf(filename, MAX_STRING_LEN, "%s/%s", 
                    run_params->io.OutputDir, run_params->io.FileNameGalaxies);
            
            // Initialize the handler
            status = save_info->io_handler.handler->initialize(filename, (struct params *)run_params, 
                                                            &save_info->io_handler.format_data);
            if (status == 0) {
                save_info->io_handler.using_io_interface = 1;
                return EXIT_SUCCESS;
            } else {
                // Handle initialization error
                fprintf(stderr, "Error initializing I/O handler: %s\n", 
                        io_get_error_message());
                save_info->io_handler.using_io_interface = 0;
                // Fall through to traditional approach
            }
        } else {
            // Handler not found, fall through to traditional approach
            fprintf(stderr, "Warning: No I/O handler found for format %d, falling back to traditional approach\n", 
                    format_id);
        }
    }
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
      fprintf(stderr, "Error: Unknown OutputFormat in `initialize_galaxy_files()`.\n");
      status = INVALID_OPTION_IN_PARAMS;
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
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `OutputGalOrder`\n", numgals);
        return MALLOC_FAILURE;
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
                fprintf(stderr,"Error: For galaxy number %d, expected mergeintoID to be within [0, %d) "
                        "but found mergeintoID = %d instead\n", gal_idx, numgals, mergeID);
                fprintf(stderr,"Additional debugging info\n");
                fprintf(stderr,"task_forestnr = %"PRId64" snapshot = %d halonr = %d MostBoundID = %lld\n",
                        task_forestnr, halogal[gal_idx].SnapNum, halogal[gal_idx].HaloNr, halos[halogal[gal_idx].HaloNr].MostBoundID);
                return -1;
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
    // Check if using I/O interface
    if (save_info->io_handler.using_io_interface && save_info->io_handler.handler != NULL) {
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
                fprintf(stderr, "Error writing galaxies using I/O interface: %s\n", 
                        io_get_error_message());
                // Do not fall through to traditional approach; if the handler failed, there's likely an issue
            } else {
                // If successful, we're done
                myfree(OutputGalOrder);
                return EXIT_SUCCESS;
            }
        }
    }
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
        fprintf(stderr, "Unknown OutputFormat in `save_galaxies()`.\n");
        status = INVALID_OPTION_IN_PARAMS;
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
    // Check if using I/O interface
    if (save_info->io_handler.using_io_interface && save_info->io_handler.handler != NULL) {
        // If the handler supports cleanup, use it
        if (save_info->io_handler.handler->cleanup != NULL) {
            // Call handler's cleanup function
            status = save_info->io_handler.handler->cleanup(save_info->io_handler.format_data);
            
            if (status != 0) {
                fprintf(stderr, "Error finalizing galaxy files using I/O interface: %s\n", 
                        io_get_error_message());
                // Fall through to traditional approach as a last resort
            } else {
                // If successful, clean up the handler data
                save_info->io_handler.handler = NULL;
                save_info->io_handler.format_data = NULL;
                save_info->io_handler.using_io_interface = 0;
                return EXIT_SUCCESS;
            }
        }
    }
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
        fprintf(stderr, "Error: Unknown OutputFormat in `finalize_galaxy_files()`.\n");
        status = INVALID_OPTION_IN_PARAMS;
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
