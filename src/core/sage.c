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
#include "core_snapshot_indexing.h"
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
 * @brief Snapshot-based forest evolution processor
 * @param forestnr Forest number to process
 * @param save_info Galaxy count tracking structure
 * @param forest_info Forest distribution information
 * @param run_params SAGE parameters structure
 * @return EXIT_SUCCESS on success, error code on failure
 * 
 * Called by: run_sage() (main processing loop)
 * Calls: load_forest() - read merger tree data
 *        snapshot_indices_init() - create snapshot indexing
 *        snapshot_indices_build() - build O(1) FOF access
 *        process_fof_group() - process FOF groups
 *        save_galaxies() - write galaxy data
 *        galaxy_array_free() - release galaxy memory
 */
static int32_t sage_per_forest(const int64_t forestnr, struct save_info *save_info,
                               struct forest_info *forest_info, struct params *run_params)
{
    begin_tree_memory_scope();

    struct halo_data *Halo = NULL;
    struct halo_aux_data *HaloAux = NULL;

    const int64_t nhalos = load_forest(run_params, forestnr, &Halo, forest_info);
    if(nhalos < 0) {
        fprintf(stderr, "Error loading forest %"PRId64"\n", forestnr);
        end_tree_memory_scope();
        return nhalos;
    }

    HaloAux = mymalloc(nhalos * sizeof(HaloAux[0]));
    for(int i = 0; i < nhalos; i++) {
        HaloAux[i].NGalaxies = 0;
    }

    // Build snapshot indexing
    struct forest_snapshot_indices snapshot_indices;
    int32_t status = snapshot_indices_init(&snapshot_indices, run_params->simulation.SimMaxSnaps, nhalos);
    if (status != EXIT_SUCCESS) {
        LOG_ERROR("Failed to initialize snapshot indices for forest %ld", forestnr);
        myfree(HaloAux);
        myfree(Halo);
        end_tree_memory_scope();
        return EXIT_FAILURE;
    }
    
    status = snapshot_indices_build(&snapshot_indices, Halo, nhalos);
    if (status != EXIT_SUCCESS) {
        LOG_ERROR("Failed to build snapshot indices for forest %ld", forestnr);
        snapshot_indices_cleanup(&snapshot_indices);
        myfree(HaloAux);
        myfree(Halo);
        end_tree_memory_scope();
        return EXIT_FAILURE;
    }

    // Snapshot-based processing with bounded memory
    GalaxyArray* galaxies_prev_snap = galaxy_array_new();
    GalaxyArray* galaxies_this_snap = NULL;
    int32_t galaxycounter = 0;

    for (int snapshot = 0; snapshot < run_params->simulation.SimMaxSnaps; ++snapshot) {
        galaxies_this_snap = galaxy_array_new();

        for(int i = 0; i < nhalos; i++) {
            HaloAux[i].NGalaxies = 0;
        }

        int32_t fof_count;
        const int32_t *fof_roots = snapshot_indices_get_fof_groups(&snapshot_indices, snapshot, &fof_count);
        
        for (int i = 0; i < fof_count; ++i) {
            int fof_halonr = fof_roots[i];
            
            status = process_fof_group(fof_halonr, galaxies_prev_snap, galaxies_this_snap,
                                       Halo, HaloAux, &galaxycounter, run_params);
            if (status != EXIT_SUCCESS) {
                galaxy_array_free(&galaxies_prev_snap);
                galaxy_array_free(&galaxies_this_snap);
                snapshot_indices_cleanup(&snapshot_indices);
                myfree(HaloAux);
                myfree(Halo);
                end_tree_memory_scope();
                return status;
            }
        }

        // Save galaxies for this snapshot
        int snap_ngal = galaxy_array_get_count(galaxies_this_snap);
        if (snap_ngal > 0) {
            struct GALAXY *snap_gals = galaxy_array_get_raw_data(galaxies_this_snap);
            
            for (int i = 0; i < snap_ngal; i++) {
                GALAXY_PROP_SnapNum(&snap_gals[i]) = snapshot;
            }
            
            status = save_galaxies(forestnr, snap_ngal, Halo, forest_info, HaloAux, snap_gals, save_info, run_params);
            if (status != EXIT_SUCCESS) {
                LOG_ERROR("Failed to save %d galaxies for snapshot %d in forest %ld", snap_ngal, snapshot, forestnr);
                galaxy_array_free(&galaxies_prev_snap);
                galaxy_array_free(&galaxies_this_snap);
                snapshot_indices_cleanup(&snapshot_indices);
                myfree(HaloAux);
                myfree(Halo);
                end_tree_memory_scope();
                return status;
            }
        }

        // Buffer swap for next snapshot
        galaxy_array_free(&galaxies_prev_snap);
        galaxies_prev_snap = galaxies_this_snap;
    }

    // Cleanup
    galaxy_array_free(&galaxies_prev_snap);
    snapshot_indices_cleanup(&snapshot_indices);
    myfree(HaloAux);
    myfree(Halo);
    end_tree_memory_scope();

    return EXIT_SUCCESS;
}


