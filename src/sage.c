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
#include "core_build_model.h"
#include "core_save.h"
#include "core_utils.h"
#include "progressbar.h"
#include "core_tree_utils.h"

#ifdef HDF5
#include "io/save_gals_hdf5.h"
#endif

/* main sage -> not exposed externally */
int32_t sage_per_forest(const int64_t forestnr, struct save_info *save_info,
                        struct forest_info *forest_info, struct params *run_params);
/* additional functionality to convert *any* support mergertree format into the lhalo-binary format */
int convert_trees_to_lhalo(const int ThisTask, const int NTasks, struct params *run_params, struct forest_info *forest_info);


int run_sage(const int ThisTask, const int NTasks, const char *param_file, void **params)
{
    struct params *run_params = malloc(sizeof(*run_params));
    if(run_params == NULL) {
        fprintf(stderr,"Error: On ThisTask = %d (out of NTasks = %d), failed to allocate memory "\
                "for the C-struct to to hold the run params. Requested size = %zu bytes...returning\n",
                ThisTask, NTasks, sizeof(*run_params));
        return MALLOC_FAILURE;
    }
    run_params->ThisTask = ThisTask;
    run_params->NTasks = NTasks;
    *params = run_params;

    int32_t status = read_parameter_file(param_file, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
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


    // If we're creating a binary output, we need to be careful.
    // The binary output contains an 32 bit header that contains the number of trees processed.
    // Hence let's make sure that the number of trees assigned to this task doesn't exceed an 32 bit number.
    if((run_params->OutputFormat == sage_binary) && (forest_info.nforests_this_task > INT_MAX)) {
        fprintf(stderr, "When creating the binary output, we must write a 32 bit header describing the number of trees processed.\n"
                        "However, task %d is processing %"PRId64" forests which is above the 32 bit limit.\n"
                        "Either change the output format to HDF5 or increase the number of cores processing your trees.\n",
                        ThisTask, forest_info.nforests_this_task);
        return EXIT_FAILURE;
    }

    /* If we are converting the input mergertree into the lhalo-binary format,
       then we just run the relevant converter: MS 12/10/2022 */
    if(run_params->OutputFormat == lhalo_binary_output) {
        return convert_trees_to_lhalo(ThisTask, NTasks, run_params, &forest_info);
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
    save_info.tot_ngals = mycalloc(run_params->NumSnapOutputs, sizeof(*(save_info.tot_ngals)));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info.tot_ngals,
                                     "Failed to allocate %d elements of size %zu for save_info.tot_ngals", run_params->NumSnapOutputs,
                                     sizeof(*(save_info.tot_ngals)));

    // Allocate memory for the number of galaxies at each output snapshot for each forest.
    save_info.forest_ngals = mycalloc(run_params->NumSnapOutputs, sizeof(*(save_info.forest_ngals)));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info.forest_ngals,
                                     "Failed to allocate %d elements of size %zu for save_info.tot_ngals", run_params->NumSnapOutputs,
                                     sizeof(*(save_info.forest_ngals)));

    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {
        // Using calloc removes the need to zero out the memory explicitly.
        save_info.forest_ngals[snap_idx] = mycalloc(Nforests, sizeof(*(save_info.forest_ngals[snap_idx])));
        CHECK_POINTER_AND_RETURN_ON_NULL(save_info.forest_ngals[snap_idx],
                                         "Failed to allocate %"PRId64" elements of size %zu for save_info.tot_ngals[%d]", Nforests,
                                         sizeof(*(save_info.forest_ngals[snap_idx])), snap_idx);
    }

#ifdef VERBOSE
    fprintf(stdout,"Task %d working on %"PRId64" forests covering %.3f fraction of the volume\n",
            ThisTask, Nforests, forest_info.frac_volume_processed);
    fflush(stdout);
#endif

    /* open all the output files corresponding to this tree file (specified by rank) */
    status = initialize_galaxy_files(ThisTask, &forest_info, &save_info, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    run_params->interrupted = 0;
#ifdef VERBOSE
    if(ThisTask == 0) {
        init_my_progressbar(stdout, Nforests, &(run_params->interrupted));
    }
#endif

#if defined(MPI) && defined(VERBOSE)
    if(NTasks > 1) {
        fprintf(stderr, "Please Note: The progress bar is not precisely reliable in MPI. "
                "It should be used as a general indicator only.\n");
    }
#endif


    for(int64_t forestnr = 0; forestnr < Nforests; forestnr++) {
#ifdef VERBOSE
        if(ThisTask == 0) {
            my_progressbar(stdout, forestnr, &(run_params->interrupted));
            fflush(stdout);
        }
#endif

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

    for(int snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {
        myfree(save_info.forest_ngals[snap_idx]);
    }
    myfree(save_info.forest_ngals);
    myfree(save_info.tot_ngals);

#ifdef VERBOSE
    if(ThisTask == 0) {
        finish_myprogressbar(stdout, &(run_params->interrupted));
    }


    struct timeval tend;
    gettimeofday(&tend, NULL);
    char *time_string = get_time_string(tstart, tend);
    fprintf(stderr,"ThisTask = %d done processing all forests assigned. Time taken = %s\n", ThisTask, time_string);
    free(time_string);
    fflush(stdout);
#endif

cleanup:
    /* sage is done running -> do the cleanup */
    cleanup_forests_io(run_params->TreeType, &forest_info);
    if(status == EXIT_SUCCESS) {
        //free Ages. But first
        //reset Age to the actual allocated address
        run_params->Age--;
        myfree(run_params->Age);
    }

    return status;
}


int32_t finalize_sage(void *params)
{

    int32_t status;

    struct params *run_params = (struct params *) params;

    switch(run_params->OutputFormat)
        {
        case(sage_binary):
            {
                status = EXIT_SUCCESS;
                break;
            }

#ifdef HDF5
        case(sage_hdf5):
            {
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
                break;
            }
#endif
            /* For converting input mergertrees into the lhalo-binary format,
               there is no final cleanup required */
        case(lhalo_binary_output):
            {
                status = EXIT_SUCCESS;
                break;
            }

        default:
            {
                status = EXIT_SUCCESS;
                break;
            }
        }

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

/*
For creating the lhalo tree binary output i.e., converting from the input
mergertree format into the lhalotree binary format.
*/

#ifdef USE_BUFFERED_WRITE
#include "io/buffered_io.h"
#endif

int convert_trees_to_lhalo(const int ThisTask, const int NTasks, struct params *run_params, struct forest_info *forest_info)
{
    if(forest_info->nforests_this_task > INT_MAX ||  forest_info->nhalos_this_task > INT_MAX) {
        fprintf(stderr,"Error: Can not correctly cast totnforests (on this task) = %"PRId64" or totnhalos = %"PRId64" "
                       "to fit within a 4-byte integer (as required by the LHaloTree binary format specification). "
                       "Converting fewer input files or adding more parallel cores (currently using %d cores) will "
                       "help alleviate the issue\n",
                        forest_info->nforests_this_task, forest_info->nhalos_this_task, NTasks);
        return EXIT_FAILURE;
    }

    if(forest_info->nforests_this_task == 0) {
        fprintf(stderr,"ThisTask=%d no forests to process...skipping\n",ThisTask);
        return EXIT_SUCCESS;
    }

    /* Now start the conversion */
    struct timeval tstart;
    gettimeofday(&tstart, NULL);

    run_params->interrupted = 0;
#ifdef VERBOSE
    if(ThisTask == 0) {
        init_my_progressbar(stdout, forest_info->nforests_this_task, &(run_params->interrupted));
#ifdef MPI
        if(NTasks > 1) {
            fprintf(stderr, "Please Note: The progress bar is not precisely reliable in MPI. "
                    "It should be used as a general indicator only.\n");
        }
#endif
    }
#endif

    const int64_t nforests_this_task = forest_info->nforests_this_task;
    int64_t totnhalos = 0;

#define PWRITE_64BIT_TO_32BIT(fd, var, offset, kind_of_var)             \
    do {                                                                \
        if(sizeof(var) != sizeof(int64_t)) {                            \
            fprintf(stderr,"Error: Bug in code - this is only "         \
            "meant to work on 64 bit integers\n");                      \
            return EXIT_FAILURE;                                        \
        }                                                               \
        if(var > INT_MAX) {                                             \
            fprintf(stderr,"Error: Number of halos = %"PRId64" does "   \
                            "not fit inside 32 bits "                   \
                            "(context = %s)\n", var, kind_of_var);      \
            return EXIT_FAILURE;                                        \
        }                                                               \
        const int32_t var_int32 = (int32_t) var;                        \
        const ssize_t bytes_written = pwrite(fd, &var_int32,            \
                                             sizeof(var_int32), offset);\
        if(bytes_written != sizeof(var_int32)) {                        \
            fprintf(stderr,"Error: Wrote %zd bytes instead of "         \
                        "the required %zu bytes\n",                     \
                        bytes_written, sizeof(var_int32));              \
            perror(NULL);                                               \
            return EXIT_FAILURE;                                        \
        }                                                               \
    } while (0)

    char buffer[3*MAX_STRING_LEN + 1];
    snprintf(buffer, 3*MAX_STRING_LEN, "%s%s.%d", run_params->OutputDir,
             run_params->FileNameGalaxies, ThisTask);
    int fd = open(buffer, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    XRETURN(fd > 0, EXIT_FAILURE, "Error: Could not open filename = %s\n", buffer);

    //PWRITE does not update the file offset -> will need to be adjusted manually
    PWRITE_64BIT_TO_32BIT(fd, nforests_this_task, 0, "total number of forests");
    PWRITE_64BIT_TO_32BIT(fd, totnhalos, 4, "total number of halos (initial, set to 0)");

    const off_t nhalos_per_forest_bytes = sizeof(int32_t)*nforests_this_task;
    //since the previous writes for totnforests and totnhalos were with pwrite
    //we need to adjust the file offset for those two 32-bit integers
    const off_t halo_data_start_offset = 4 + 4 + nhalos_per_forest_bytes;

    XRETURN(lseek(fd, halo_data_start_offset, SEEK_SET) == halo_data_start_offset,
            EXIT_FAILURE,
            "Error: Could not seek to %llu bytes to write the "
            "start of the halo data from the first forest",
            (unsigned long long) halo_data_start_offset);

#ifdef USE_BUFFERED_WRITE
    const size_t buffer_size = 4 * 1024 * 1024; //4 MB
    struct buffered_io buf_io;
    int status = setup_buffered_io(&buf_io, buffer_size, fd, halo_data_start_offset);
    if(status != EXIT_SUCCESS)
    {
        fprintf(stderr,"Error: Could not setup buffered io\n");
        return -1;
    }
#endif

    /* simulation merger-tree data */
    struct halo_data *Halo = NULL;
    for(int64_t forestnr=0; forestnr < nforests_this_task; forestnr++) {
#ifdef VERBOSE
        if(ThisTask == 0) {
            my_progressbar(stdout, forestnr, &(run_params->interrupted));
            fflush(stdout);
        }
#endif

        /* nhalos is meaning-less for consistent-trees until *AFTER* the forest has been loaded */
        const int64_t nhalos = load_forest(run_params, forestnr, &Halo, forest_info);
        XRETURN(nhalos > 0, nhalos, "Error during loading forestnum =  %"PRId64"...exiting\n", forestnr);
        XRETURN(nhalos <= INT_MAX, EXIT_FAILURE,
                "Error: Number of halos = %"PRId64" must be > 0 *and* also fit inside 32 bits\n",
                nhalos);
        const size_t numbytes = sizeof(struct halo_data)*nhalos;
#ifdef USE_BUFFERED_WRITE
        status = write_buffered_io( &buf_io, Halo, numbytes);
        if(status < 0) {
            fprintf(stderr,"Error: Could not write (buffered). forestnr = %"PRId64" number of bytes = %zu\n", forestnr, numbytes);
            return status;
        }
#else
        mywrite(fd, Halo, numbytes);//write updates the file offset
#endif
        const off_t nh_per_tree_offset = sizeof(int32_t) + sizeof(int32_t) + forestnr * sizeof(int32_t);
        PWRITE_64BIT_TO_32BIT(fd, nhalos, nh_per_tree_offset, "nhalos per tree");//pwrite does not update file offset

        myfree(Halo);
        totnhalos += nhalos;
    }
#ifdef USE_BUFFERED_WRITE
    status = cleanup_buffered_io(&buf_io);
    if(status != EXIT_SUCCESS) {
        fprintf(stderr,"Error: Could not finalise the output file\n");
        return status;
    }
#endif

    /* Check that totnhalos fits within a 4 byte integer and write to the file */
    PWRITE_64BIT_TO_32BIT(fd, totnhalos, 4, "total number of halos in file");

    if(forest_info->nhalos_this_task > 0) {
        XRETURN(totnhalos == forest_info->nhalos_this_task, -1,
                "Error: Expected totnhalos written out = %"PRId64" to "
                "be *exactly* equal to forests_info->nhalos_this_task = %"PRId64"\n",
                totnhalos, forest_info->nhalos_this_task);
    }

    XRETURN(close(fd) == 0, EXIT_FAILURE, "Error while closing the output binary file");

    /* sage is done running -> do the cleanup */
    cleanup_forests_io(run_params->TreeType, forest_info);

#ifdef VERBOSE
    if(ThisTask == 0) {
        finish_myprogressbar(stdout, &(run_params->interrupted));
    }
    fflush(stdout);
    struct timeval tend;
    gettimeofday(&tend, NULL);
    char *time_string = get_time_string(tstart, tend);
    fprintf(stderr,"ThisTask = %d done processing all forests assigned. Time taken = %s\n", ThisTask, time_string);
    free(time_string);
#endif

    return EXIT_SUCCESS;
}
