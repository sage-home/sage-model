#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifdef MPI
#include <mpi.h>
#endif

#include "sage.h"
#include "core_allvars.h"
#include "core_init.h"
#include "core_read_parameter_file.h"
#include "core_io_tree.h"
#include "core_mymalloc.h"
#include "../io/io_interface.h"
#include "core_build_model.h"

#include "core_save.h"
#include "core_utils.h"
#include "progressbar.h"
#include "core_logging.h"
#include "galaxy_array.h"

#ifdef HDF5
#include "../io/save_gals_hdf5.h"
#endif

// Forward declarations
static int32_t sage_per_forest(const int64_t forestnr, struct save_info *save_info,
                               struct forest_info *forest_info, struct params *run_params);
static int initialize_sage_systems(struct params *run_params);
static int setup_forest_processing(struct params *run_params, struct forest_info *forest_info, 
                                  int ThisTask, int NTasks);
static int allocate_save_info(struct save_info *save_info, struct params *run_params, int64_t Nforests);
static void cleanup_save_info(struct save_info *save_info, struct params *run_params);
static void cleanup_sage_systems(struct params *run_params, struct forest_info *forest_info);


/**
 * @brief Main SAGE execution orchestrator
 * @param ThisTask MPI task ID
 * @param NTasks Total number of MPI tasks
 * @param param_file Path to parameter file
 * @param params Output pointer to allocated parameters
 * @return EXIT_SUCCESS on success, error code on failure
 * 
 * Called by: External API (main program entry)
 * Calls: read_parameter_file() - parse simulation parameters
 *        initialize_sage_systems() - boot core subsystems
 *        setup_forest_processing() - distribute forests across MPI
 *        init() - initialize simulation constants
 *        allocate_save_info() - create galaxy count arrays
 *        initialize_galaxy_files() - open HDF5 output files
 *        sage_per_forest() - process individual merger trees
 *        finalize_galaxy_files() - close HDF5 files
 *        cleanup_save_info() - free memory arrays
 *        cleanup_sage_systems() - shutdown all subsystems
 */
int run_sage(const int ThisTask, const int NTasks, const char *param_file, void **params)
{
    struct params *run_params = malloc(sizeof(*run_params));
    if(run_params == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for run parameters\n");
        return MALLOC_FAILURE;
    }
    run_params->runtime.ThisTask = ThisTask;
    run_params->runtime.NTasks = NTasks;
    *params = run_params;

    int32_t status = read_parameter_file(param_file, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }
    
    status = initialize_sage_systems(run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    struct timeval tstart;
    gettimeofday(&tstart, NULL);

    struct forest_info forest_info;
    status = setup_forest_processing(run_params, &forest_info, ThisTask, NTasks);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    init(run_params);

    if(forest_info.nforests_this_task == 0) {
        fprintf(stderr, "ThisTask=%d no forests to process...skipping\n", ThisTask);
        goto cleanup;
    }

    const int64_t Nforests = forest_info.nforests_this_task;

    struct save_info save_info;
    status = allocate_save_info(&save_info, run_params, Nforests);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    fprintf(stdout, "\nTask %d working on %"PRId64" forests covering %.3f fraction of the volume\n",
            ThisTask, Nforests, forest_info.frac_volume_processed);
    fflush(stdout);

    status = initialize_galaxy_files(ThisTask, &save_info, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    run_params->runtime.interrupted = 0;
    if(ThisTask == 0) {
        init_my_progressbar(stdout, Nforests, &(run_params->runtime.interrupted));
    }

#if defined(MPI)
    if(NTasks > 1) {
        fprintf(stderr, "Note: Progress bar is approximate in MPI mode\n");
    }
#endif


    // Main forest processing loop
    for(int64_t forestnr = 0; forestnr < Nforests; forestnr++) {
        if(ThisTask == 0) {
            my_progressbar(stdout, forestnr, &(run_params->runtime.interrupted));
            fflush(stdout);
        }

        status = sage_per_forest(forestnr, &save_info, &forest_info, run_params);
        if(status != EXIT_SUCCESS) {
            return status;
        }
    }

    status = finalize_galaxy_files(&forest_info, &save_info, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    cleanup_save_info(&save_info, run_params);

    if(ThisTask == 0) {
        finish_myprogressbar(stdout, &(run_params->runtime.interrupted));
    }

#ifdef VERBOSE
    struct timeval tend;
    gettimeofday(&tend, NULL);
    char *time_string = get_time_string(tstart, tend);
    fprintf(stderr, "ThisTask = %d done processing. Time taken = %s\n", ThisTask, time_string);
    free(time_string);
#else
    fprintf(stdout, "\nFinished\n");
#endif
    fflush(stdout);

cleanup:
    cleanup_sage_systems(run_params, &forest_info);
    return status;
}


/**
 * @brief Final cleanup and master file creation
 * @param params Pointer to SAGE parameters structure
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 * 
 * Called by: External API (after run completion)
 * Calls: create_hdf5_master_file() - generate HDF5 master file
 *        cleanup_logging() - shutdown logging system
 *        free() - release parameter memory
 */
int32_t finalize_sage(void *params)
{
    struct params *run_params = (struct params *) params;
    LOG_INFO("Finalizing SAGE execution");

#ifdef HDF5
    int32_t status = create_hdf5_master_file(run_params);
    #ifdef VERBOSE
    ssize_t nleaks = H5Fget_obj_count(H5F_OBJ_ALL, H5F_OBJ_ALL);
    if(nleaks > 0) {
        fprintf(stderr, "Warning: %zd HDF5 leaks detected\n", nleaks);
    }
    #endif
#else
    LOG_ERROR("HDF5 support is required but not compiled in");
    int32_t status = EXIT_FAILURE;
#endif

    cleanup_logging();
    free(run_params);
    return status;
}

// Helper function implementations

/**
 * @brief Boot core SAGE subsystems
 * @param run_params SAGE parameters structure
 * @return EXIT_SUCCESS on success, error code on failure
 * 
 * Called by: run_sage()
 * Calls: initialize_logging() - setup logging infrastructure
 *        memory_system_init() - initialize custom allocators
 *        io_init() - boot I/O interface system
 */
static int initialize_sage_systems(struct params *run_params)
{
    int32_t status = initialize_logging(run_params);
    if(status != EXIT_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize logging system\n");
    }

    status = memory_system_init();
    if(status != EXIT_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize memory system\n");
        return status;
    }

    status = io_init();
    if(status != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize I/O interface system\n");
        return status;
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Configure forest MPI distribution
 * @param run_params SAGE parameters structure
 * @param forest_info Output forest distribution information
 * @param ThisTask Current MPI task ID
 * @param NTasks Total number of MPI tasks
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 * 
 * Called by: run_sage()
 * Calls: setup_forests_io() - distribute trees across tasks
 */
static int setup_forest_processing(struct params *run_params, struct forest_info *forest_info, 
                                  int ThisTask, int NTasks)
{
    memset(forest_info, 0, sizeof(struct forest_info));
    
    int32_t status = setup_forests_io(run_params, forest_info, ThisTask, NTasks);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    if(forest_info->totnforests < 0 || forest_info->nforests_this_task < 0) {
        fprintf(stderr, "Error: Invalid forest counts\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Create galaxy count tracking arrays
 * @param save_info Output save information structure
 * @param run_params SAGE parameters structure  
 * @param Nforests Number of forests to process
 * @return EXIT_SUCCESS on success, error code on failure
 * 
 * Called by: run_sage()
 * Calls: mycalloc() - allocate memory safely
 *        CHECK_POINTER_AND_RETURN_ON_NULL() - validate allocations
 */
static int allocate_save_info(struct save_info *save_info, struct params *run_params, int64_t Nforests)
{
    save_info->tot_ngals = mycalloc(run_params->simulation.NumSnapOutputs, sizeof(*(save_info->tot_ngals)));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->tot_ngals, "Failed to allocate tot_ngals");

    save_info->forest_ngals = mycalloc(run_params->simulation.NumSnapOutputs, sizeof(*(save_info->forest_ngals)));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->forest_ngals, "Failed to allocate forest_ngals");

    for(int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        save_info->forest_ngals[snap_idx] = mycalloc(Nforests, sizeof(*(save_info->forest_ngals[snap_idx])));
        CHECK_POINTER_AND_RETURN_ON_NULL(save_info->forest_ngals[snap_idx], "Failed to allocate forest_ngals[%d]", snap_idx);
    }

    return EXIT_SUCCESS;
}

/**
 * @brief Free galaxy count tracking arrays
 * @param save_info Save information structure to cleanup
 * @param run_params SAGE parameters structure
 * 
 * Called by: run_sage()
 * Calls: myfree() - safe memory deallocation
 */
static void cleanup_save_info(struct save_info *save_info, struct params *run_params)
{
    for(int snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        myfree(save_info->forest_ngals[snap_idx]);
    }
    myfree(save_info->forest_ngals);
    myfree(save_info->tot_ngals);
}

/**
 * @brief Shutdown all systems in orderly fashion
 * @param run_params SAGE parameters structure
 * @param forest_info Forest distribution information
 * 
 * Called by: run_sage()
 * Calls: cleanup_forests_io() - shutdown forest I/O
 *        io_cleanup() - cleanup I/O interface
 *        cleanup() - general simulation cleanup
 *        memory_system_cleanup() - shutdown custom allocators  
 *        cleanup_logging() - close logging system
 */
static void cleanup_sage_systems(struct params *run_params, struct forest_info *forest_info)
{
    cleanup_forests_io(run_params->io.TreeType, forest_info);
    io_cleanup();
    cleanup(run_params);
    memory_system_cleanup();
    cleanup_logging();
}

/**
 * @brief Hybrid tree-based forest evolution processor with legacy control flow and modern infrastructure
 * 
 * This function implements the proven legacy processing pattern from src-legacy/sage.c:277-392
 * but uses modern memory management, property system, and architectural safeguards. It represents
 * the core of the hybrid conversion: single tree-based processing that combines legacy scientific
 * algorithms with 5 phases of modern architectural improvements.
 * 
 * **Legacy Processing Flow** (from src-legacy/sage.c:277-392):
 * - Load forest data and allocate arrays  
 * - Initialize HaloFlag, NGalaxies, DoneFlag for all halos
 * - Main tree processing: construct_galaxies(0, ...) for root halo
 * - Sub-tree loop: process unreachable sub-trees with construct_galaxies()
 * - Save galaxies and cleanup
 * 
 * **Modern Infrastructure** (preserved from current codebase):
 * - GalaxyArray system instead of dangerous raw array patterns
 * - Property system integration with GALAXY_PROP_* macros
 * - Safe memory management and comprehensive error handling
 * - Module system preservation via evolve_galaxies_wrapper()
 * - Modern save_galaxies() with property-based HDF5 output
 * 
 * **Key Differences from Legacy**:
 * - Uses GalaxyArray instead of raw Gal/HaloGal arrays
 * - Calls hybrid construct_galaxies() with modern infrastructure
 * - Preserves module system and pipeline execution
 * - Enhanced error handling and validation throughout
 * - Safe memory cleanup with corruption detection
 * 
 * @param forestnr Forest number to process
 * @param save_info Galaxy count tracking structure
 * @param forest_info Forest distribution information
 * @param run_params SAGE parameters structure
 * @return EXIT_SUCCESS on success, error code on failure
 * 
 * Called by: run_sage() (main processing loop)
 * Calls: load_forest() - read merger tree data
 *        construct_galaxies() - recursive tree processing with modern infrastructure
 *        save_galaxies() - write galaxy data with property system
 */
static int32_t sage_per_forest(const int64_t forestnr, struct save_info *save_info,
                               struct forest_info *forest_info, struct params *run_params)
{
    begin_tree_memory_scope();

    // **LEGACY PATTERN**: Load forest and declare galaxy/halo arrays
    // From legacy lines 285-289, 312-314: Declare Gal, HaloGal, Halo, HaloAux
    struct halo_data *Halo = NULL;
    struct halo_aux_data *HaloAux = NULL;

    const int64_t nhalos = load_forest(run_params, forestnr, &Halo, forest_info);
    if(nhalos < 0) {
        LOG_ERROR("Error loading forest %"PRId64, forestnr);
        end_tree_memory_scope();
        return nhalos;
    }

    LOG_DEBUG("Using hybrid tree-based processing for forest %ld with %ld halos", forestnr, nhalos);

    // **MODERN ENHANCEMENT**: Use GalaxyArray instead of raw array allocation
    // Legacy used: Gal = mymalloc(maxgals * sizeof(Gal[0])); HaloGal = mymalloc(...)
    GalaxyArray *working_galaxies = galaxy_array_new();
    GalaxyArray *output_galaxies = galaxy_array_new();
    if (!working_galaxies || !output_galaxies) {
        LOG_ERROR("Failed to allocate galaxy arrays for forest %ld", forestnr);
        galaxy_array_free(&working_galaxies);
        galaxy_array_free(&output_galaxies);
        myfree(Halo);
        end_tree_memory_scope();
        return EXIT_FAILURE;
    }

    // **LEGACY PATTERN**: Allocate and initialize HaloAux array
    // From legacy lines 312-323: HaloAux allocation and flag initialization
    HaloAux = mymalloc(nhalos * sizeof(HaloAux[0]));
    if (!HaloAux) {
        LOG_ERROR("Failed to allocate HaloAux array for forest %ld", forestnr);
        galaxy_array_free(&working_galaxies);
        galaxy_array_free(&output_galaxies);
        myfree(Halo);
        end_tree_memory_scope();
        return EXIT_FAILURE;
    }

    // **MODERN ENHANCEMENT**: Create local processing state arrays (legacy used HaloFlag/DoneFlag in halo_aux_data)
    // Modern approach: Use separate arrays for processing state instead of modifying core structures
    bool *DoneFlag = mycalloc(nhalos, sizeof(bool));   // Tree processing completion flags
    int *HaloFlag = mycalloc(nhalos, sizeof(int));     // FOF group processing state
    if (!DoneFlag || !HaloFlag) {
        LOG_ERROR("Failed to allocate processing state arrays for forest %ld", forestnr);
        myfree(DoneFlag);
        myfree(HaloFlag);
        galaxy_array_free(&working_galaxies);
        galaxy_array_free(&output_galaxies);
        myfree(HaloAux);
        myfree(Halo);
        end_tree_memory_scope();
        return EXIT_FAILURE;
    }

    // **LEGACY PATTERN**: Initialize processing flags and halo auxiliary data
    // From legacy lines 316-323: Initialize HaloFlag=0, NGalaxies=0, DoneFlag=0
    for(int i = 0; i < nhalos; i++) {
        HaloFlag[i] = 0;              // FOF group processing state (0=unprocessed, 1=processing, 2=complete)
        HaloAux[i].NGalaxies = 0;     // Galaxy count per halo (keep existing field)
        HaloAux[i].FirstGalaxy = -1;  // Initialize FirstGalaxy to -1 (no galaxies yet)
        DoneFlag[i] = false;          // Tree processing completion flag
    }

    // **LEGACY PATTERN**: Initialize galaxy counters
    // From legacy lines 325, 360: Initialize numgals and galaxycounter
    int numgals = 0;
    int32_t galaxycounter = 0;

    // **LEGACY PATTERN**: Main tree processing  
    // From legacy line 363: construct_galaxies(0, &numgals, &galaxycounter, ...)
    // **MODERN IMPLEMENTATION**: Use hybrid construct_galaxies() with modern infrastructure
    int32_t status = construct_galaxies(0, &numgals, &galaxycounter, 
                                       &working_galaxies, &output_galaxies,
                                       Halo, HaloAux, DoneFlag, HaloFlag, run_params);
    if (status != EXIT_SUCCESS) {
        LOG_ERROR("Failed to construct main tree for forest %ld", forestnr);
        myfree(DoneFlag);
        myfree(HaloFlag);
        galaxy_array_free(&working_galaxies);
        galaxy_array_free(&output_galaxies);
        myfree(HaloAux);
        myfree(Halo);
        end_tree_memory_scope();
        return status;
    }

    // **LEGACY PATTERN**: Process unreachable sub-trees
    // From legacy lines 369-376: Loop through halos, process if DoneFlag == 0
    for(int halonr = 0; halonr < nhalos; halonr++) {
        if(DoneFlag[halonr] == false) {
            status = construct_galaxies(halonr, &numgals, &galaxycounter,
                                      &working_galaxies, &output_galaxies, 
                                      Halo, HaloAux, DoneFlag, HaloFlag, run_params);
            if(status != EXIT_SUCCESS) {
                LOG_ERROR("Failed to construct sub-tree starting at halo %d in forest %ld", halonr, forestnr);
                myfree(DoneFlag);
                myfree(HaloFlag);
                galaxy_array_free(&working_galaxies);
                galaxy_array_free(&output_galaxies);
                myfree(HaloAux);
                myfree(Halo);
                end_tree_memory_scope();
                return status;
            }
        }
    }

    // **LEGACY PATTERN + MODERN I/O**: Save galaxies
    // From legacy line 380: save_galaxies(forestnr, numgals, Halo, ...)
    // **MODERN ENHANCEMENT**: Use property-based save_galaxies() with HDF5 output
    int total_galaxies = galaxy_array_get_count(output_galaxies);
    if (total_galaxies > 0) {
        struct GALAXY *output_gals = galaxy_array_get_raw_data(output_galaxies);
        
        // **LEGACY PATTERN**: FirstGalaxy and NGalaxies should already be set up by evolve_galaxies()
        // The modern evolve_galaxies() function properly sets up these indices during galaxy evolution
        // No manual setup needed here - just verify the setup is correct
        
        status = save_galaxies(forestnr, total_galaxies, Halo, forest_info, HaloAux, 
                              output_gals, save_info, run_params);
        if (status != EXIT_SUCCESS) {
            LOG_ERROR("Failed to save %d galaxies for forest %ld", total_galaxies, forestnr);
            myfree(DoneFlag);
            myfree(HaloFlag);
            galaxy_array_free(&working_galaxies);
            galaxy_array_free(&output_galaxies);
            myfree(HaloAux);
            myfree(Halo);
            end_tree_memory_scope();
            return status;
        }
        
        LOG_DEBUG("Successfully processed forest %ld: %d galaxies saved", forestnr, total_galaxies);
    } else {
        LOG_DEBUG("Forest %ld produced no galaxies", forestnr);
    }

    // **LEGACY PATTERN + MODERN CLEANUP**: Free memory safely
    // From legacy lines 385-389: myfree(Gal); myfree(HaloGal); myfree(HaloAux); myfree(Halo);
    // **MODERN ENHANCEMENT**: Use safe cleanup with corruption detection
    myfree(DoneFlag);
    myfree(HaloFlag);
    galaxy_array_free(&working_galaxies);
    galaxy_array_free(&output_galaxies);
    myfree(HaloAux);
    myfree(Halo);
    end_tree_memory_scope();

    return EXIT_SUCCESS;
}


