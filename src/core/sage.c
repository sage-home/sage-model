#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h> //open/close
#include <unistd.h> //pwrite

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
#include "core_tree_utils.h"
#include "core_logging.h"
#include "core_event_system.h"
#include "core_pipeline_system.h"
#include "core_config_system.h"

#ifdef HDF5
#include "../io/save_gals_hdf5.h"
#endif

/* main sage -> not exposed externally */
int32_t sage_per_forest(const int64_t forestnr, struct save_info *save_info,
                        struct forest_info *forest_info, struct params *run_params);


int run_sage(const int ThisTask, const int NTasks, const char *param_file, void **params)
{
    struct params *run_params = malloc(sizeof(*run_params));
    if(run_params == NULL) {
        fprintf(stderr,"Error: On ThisTask = %d (out of NTasks = %d), failed to allocate memory "\
                "for the C-struct to to hold the run params. Requested size = %zu bytes...returning\n",
                ThisTask, NTasks, sizeof(*run_params));
        return MALLOC_FAILURE;
    }
    run_params->runtime.ThisTask = ThisTask;
    run_params->runtime.NTasks = NTasks;
    
    *params = run_params;

    int32_t status = read_parameter_file(param_file, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }
    
    /* Initialize the logging system */
    status = initialize_logging(run_params);
    if(status != EXIT_SUCCESS) {
        fprintf(stderr, "Warning: Failed to initialize logging system\n");
        /* Continue execution even if logging initialization fails */
    }

    /* Now start the model */
    struct timeval tstart;
    gettimeofday(&tstart, NULL);

    struct forest_info forest_info;
    memset(&forest_info, 0, sizeof(struct forest_info));
    forest_info.totnforests = 0;
    forest_info.totnhalos = 0;
    forest_info.nforests_this_task = 0;
    forest_info.nhalos_this_task = 0;

    /* Initialize the I/O interface system for Phase 5 migration */
    status = io_init();
    if(status != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to initialize I/O interface system\n");
        return status;
    }

    /* setup the forests reading, and then distribute the forests over the Ntasks */
    status = setup_forests_io(run_params, &forest_info, ThisTask, NTasks);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    if(forest_info.totnforests < 0 || forest_info.nforests_this_task < 0) {
        fprintf(stderr,"Error: Bug in code totnforests = %"PRId64" and nforests (on this task) = %"PRId64" should both be at least 0\n",
                forest_info.totnforests, forest_info.nforests_this_task);
        return EXIT_FAILURE;
    }


    /* If we are here, then we need to run the SAM */
    init(run_params);

    /* init needs to run before (potentially) jumping to 'cleanup' ->
       otherwise unallocated run_params->Age will get freed and result
       in an error. MS 12/10/2022
    */
    if(forest_info.nforests_this_task == 0) {
        fprintf(stderr,"ThisTask=%d no forests to process...skipping\n",ThisTask);
        goto cleanup;
    }

    const int64_t Nforests = forest_info.nforests_this_task;

    struct save_info save_info;
    // Allocate memory for the total number of galaxies for each output snapshot (across all forests).
    // Calloc because we start with no galaxies.
    save_info.tot_ngals = mycalloc(run_params->simulation.NumSnapOutputs, sizeof(*(save_info.tot_ngals)));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info.tot_ngals,
                                     "Failed to allocate %d elements of size %zu for save_info.tot_ngals", run_params->simulation.NumSnapOutputs,
                                     sizeof(*(save_info.tot_ngals)));

    // Allocate memory for the number of galaxies at each output snapshot for each forest.
    save_info.forest_ngals = mycalloc(run_params->simulation.NumSnapOutputs, sizeof(*(save_info.forest_ngals)));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info.forest_ngals,
                                     "Failed to allocate %d elements of size %zu for save_info.tot_ngals", run_params->simulation.NumSnapOutputs,
                                     sizeof(*(save_info.forest_ngals)));

    for(int32_t snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        // Using calloc removes the need to zero out the memory explicitly.
        save_info.forest_ngals[snap_idx] = mycalloc(Nforests, sizeof(*(save_info.forest_ngals[snap_idx])));
        CHECK_POINTER_AND_RETURN_ON_NULL(save_info.forest_ngals[snap_idx],
                                         "Failed to allocate %"PRId64" elements of size %zu for save_info.tot_ngals[%d]", Nforests,
                                         sizeof(*(save_info.forest_ngals[snap_idx])), snap_idx);
    }

    fprintf(stdout,"\nTask %d working on %"PRId64" forests covering %.3f fraction of the volume\n",
            ThisTask, Nforests, forest_info.frac_volume_processed);
    fflush(stdout);

    /* open all the output files corresponding to this tree file (specified by rank) */
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
        fprintf(stderr, "Please Note: The progress bar is not precisely reliable in MPI. "
                "It should be used as a general indicator only.\n");
    }
#endif


    for(int64_t forestnr = 0; forestnr < Nforests; forestnr++) {
        if(ThisTask == 0) {
            my_progressbar(stdout, forestnr, &(run_params->runtime.interrupted));
            fflush(stdout);
        }

        /* the millennium tree is really a collection of trees, viz., a forest */
        status = sage_per_forest(forestnr, &save_info, &forest_info, run_params);
        if(status != EXIT_SUCCESS) {
            return status;
        }
    }

    status = finalize_galaxy_files(&forest_info, &save_info, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    for(int snap_idx = 0; snap_idx < run_params->simulation.NumSnapOutputs; snap_idx++) {
        myfree(save_info.forest_ngals[snap_idx]);
    }
    myfree(save_info.forest_ngals);
    myfree(save_info.tot_ngals);

    if(ThisTask == 0) {
        finish_myprogressbar(stdout, &(run_params->runtime.interrupted));
    }

#ifdef VERBOSE
    struct timeval tend;
    gettimeofday(&tend, NULL);
    char *time_string = get_time_string(tstart, tend);
    fprintf(stderr,"ThisTask = %d done processing all forests assigned. Time taken = %s\n", ThisTask, time_string);
    free(time_string);
    fflush(stdout);
#else
    fprintf(stdout,"\nFinished\n");
    fflush(stdout);
#endif

cleanup:
    /* sage is done running -> do the cleanup */
    cleanup_forests_io(run_params->io.TreeType, &forest_info);
    
    /* Call comprehensive cleanup function */
    cleanup(run_params);
    
    /* Cleanup the logging system */
    cleanup_logging();

    return status;
}


int32_t finalize_sage(void *params)
{
    int32_t status;

    struct params *run_params = (struct params *) params;
    
    /* Log the finalization */
    LOG_INFO("Finalizing SAGE execution");

    // HDF5 is the only supported output format
#ifdef HDF5
    status = create_hdf5_master_file(run_params);
    #ifdef VERBOSE
    /* Check if anything was not cleaned up */
    ssize_t nleaks = H5Fget_obj_count(H5F_OBJ_ALL, H5F_OBJ_ALL);
    if(nleaks > 0) {
        fprintf(stderr,"Warning: Looks like there are %zd leaks associated with the hdf5 files.\n", nleaks);
        #define CHECK_SPECIFIC_HDF5_LEAK(objtype, h5_obj_type) {    \
            nleaks = H5Fget_obj_count(H5F_OBJ_ALL, h5_obj_type);\
            if(nleaks > 0) {\
                fprintf(stderr, "Number of open %s = %zd\n", #objtype, nleaks);\
            }\
        }
        CHECK_SPECIFIC_HDF5_LEAK("files", H5F_OBJ_FILE);
        CHECK_SPECIFIC_HDF5_LEAK("datasets", H5F_OBJ_DATASET);
        CHECK_SPECIFIC_HDF5_LEAK("groups", H5F_OBJ_GROUP);
        CHECK_SPECIFIC_HDF5_LEAK("datatypes", H5F_OBJ_DATATYPE);
        CHECK_SPECIFIC_HDF5_LEAK("attributes", H5F_OBJ_ATTR);
        #undef CHECK_SPECIFIC_HDF5_LEAK
    }
    #endif
#else
    /* HDF5 is required */
    LOG_ERROR("HDF5 support is required but not compiled in");
    status = EXIT_FAILURE;
#endif

    /* Ensure logging is cleaned up before params are freed */
    cleanup_logging();
    
    free(run_params);

    return status;
}

// Local Functions //

int32_t sage_per_forest(const int64_t forestnr, struct save_info *save_info,
                        struct forest_info *forest_info, struct params *run_params)
{
    int32_t status = EXIT_FAILURE;

    /*  galaxy data  */
    struct GALAXY  *Gal = NULL, *HaloGal = NULL;

    /* simulation merger-tree data */
    struct halo_data *Halo = NULL;

    /*  auxiliary halo data  */
    struct halo_aux_data  *HaloAux = NULL;

    /* nhalos is meaning-less for consistent-trees until *AFTER* the forest has been loaded */
    const int64_t nhalos = load_forest(run_params, forestnr, &Halo, forest_info);
    if(nhalos < 0) {
        fprintf(stderr,"Error during loading forestnum =  %"PRId64"...exiting\n", forestnr);
        return nhalos;
    }

#ifdef PROCESS_LHVT_STYLE
#error Processing in Locally-horizontal vertical tree (LHVT) style not implemented yet

    /* re-arrange the halos into a locally horizontal vertical forest */
    int32_t *file_ordering_of_halos=NULL;
    int status = reorder_lhalo_to_lhvt(nhalos, Halo, 0, &file_ordering_of_halos);/* the 3rd parameter is for testing the reorder code */
    if(status != EXIT_SUCCESS) {
        return status;
    }
#endif

    int maxgals = (int)(MAXGALFAC * nhalos);
    if(maxgals < 10000) maxgals = 10000;

    HaloAux = mymalloc(nhalos * sizeof(HaloAux[0]));
    HaloGal = mymalloc(maxgals * sizeof(HaloGal[0]));
    Gal = mymalloc(maxgals * sizeof(Gal[0]));/* used to be fof_maxgals instead of maxgals*/

    for(int i = 0; i < nhalos; i++) {
        HaloAux[i].HaloFlag = 0;
        HaloAux[i].NGalaxies = 0;
        HaloAux[i].DoneFlag = 0;
#ifdef PROCESS_LHVT_STYLE
        HaloAux[i].orig_index = file_ordering_of_halos[i];
#endif
    }

    /* MS: numgals is shared by both LHVT and the standard processing */
    int numgals = 0;

#ifdef PROCESS_LHVT_STYLE
    free(file_ordering_of_halos);
    /* done with re-ordering the halos into a locally horizontal vertical tree format */

    int nfofs_all_snaps[ABSOLUTEMAXSNAPS] = {0};
    /* getting the number of FOF halos at each snapshot */
    status = get_nfofs_all_snaps(Halo, nhalos, nfofs_all_snaps, ABSOLUTEMAXSNAPS);
    if(status != EXIT_SUCCESS) {
        return status;
    }

#if 0
    for(int halonr = 0; halonr < nhalos; halonr++) {
        fprintf(stderr,"halonr = %d snap = %03d mvir = %14.6e firstfofhalo = %8d nexthalo = %8d\n",
                halonr, Halo[halonr].SnapNum, Halo[halonr].Mvir, Halo[halonr].FirstHaloInFOFgroup, Halo[halonr].NextHaloInFOFgroup);
    }
#endif

    /* this will be the new processing style --> one snapshot at a time */
    uint32_t ngal = 0;
    for(int snapshot=min_snapshot;snapshot <= max_snapshot; snapshot++) {
        uint32_t nfofs_this_snap = get_nfofs_at_snap(forestnr, snapshot);
        for(int ifof=0;ifof<nfofs_this_snap;ifof++) {
            ngal = process_fof_at_snap(ifof, snapshot, ngal);
        }
    }

#else /* PROCESS_LHVT_STYLE */

    /*MS: This is the normal SAGE processing on a tree-by-tree (vertical) basis */

    /* Now start the processing */
    int32_t galaxycounter = 0;

    /* First run construct_galaxies outside for loop -> takes care of the main tree */
    status = construct_galaxies(0, &numgals, &galaxycounter, &maxgals, Halo, HaloAux, &Gal, &HaloGal, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    /* But there are sub-trees within one forest file that are not reachable via the recursive routine -> do those as well */
    for(int halonr = 0; halonr < nhalos; halonr++) {
        if(HaloAux[halonr].DoneFlag == 0) {
            status = construct_galaxies(halonr, &numgals, &galaxycounter, &maxgals, Halo, HaloAux, &Gal, &HaloGal, run_params);
            if(status != EXIT_SUCCESS) {
                return status;
            }
        }
    }

#endif /* PROCESS_LHVT_STYLE */

    status = save_galaxies(forestnr, numgals, Halo, forest_info, HaloAux, HaloGal, save_info, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    /* free galaxies and the forest */
    myfree(Gal);
    myfree(HaloGal);
    myfree(HaloAux);
    myfree(Halo);

    return EXIT_SUCCESS;
}


