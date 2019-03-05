#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
#include "progressbar.h"
#include "core_tree_utils.h"

/* main sage -> not exposed externally */
static void sage_per_forest(const int ThisTask, const int forestnr, int *TotGalaxies, int **ForestNgals, int *save_fd,
                            struct forest_info *forests_info, struct params *run_params);

int init_sage(const int ThisTask, const char *param_file, struct params *run_params)
{
    int status = read_parameter_file(ThisTask, param_file, run_params);
    if(status != EXIT_SUCCESS) {
        ABORT(status);
    }
    init(ThisTask, run_params);

    return EXIT_SUCCESS;
}

int run_sage(const int ThisTask, const int NTasks, struct params *run_params)
{
    struct forest_info forests_info;
    memset(&forests_info, 0, sizeof(struct forest_info));
    forests_info.totnforests = 0;
    forests_info.nforests_this_task = 0;
    /* forests_info.nsnapshots = 0; */
    /* forests_info.totnhalos_per_forest = NULL; */

    char buffer[4*MAX_STRING_LEN + 1];
    snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_z%1.3f_%d", run_params->OutputDir, run_params->FileNameGalaxies, run_params->ZZ[run_params->ListOutputSnaps[0]], ThisTask);

    FILE *fp = fopen(buffer, "w");
    if(fp != NULL) {
        fclose(fp);
    } else {
        fprintf(stderr,"Error: Could not open output file `%s`\n", buffer);
        ABORT(FILE_NOT_FOUND);
    }

    int TotGalaxies[ABSOLUTEMAXSNAPS] = { 0 };
    int *ForestNgals[ABSOLUTEMAXSNAPS] = { NULL };
    int save_fd[ABSOLUTEMAXSNAPS] = { -1 };

    /* setup the forests reading, and then distribute the forests over the Ntasks */
    int status = EXIT_FAILURE;
    status = setup_forests_io(run_params, &forests_info, ThisTask, NTasks);
    if(status != EXIT_SUCCESS) {
        ABORT(status);
    }
    if(forests_info.totnforests < 0 || forests_info.nforests_this_task < 0) {
        fprintf(stderr,"Error: Bug in code totnforests = %"PRId64" and nforests (on this task) = %"PRId64" should both be at least 0\n",
                forests_info.totnforests, forests_info.nforests_this_task);
        ABORT(EXIT_FAILURE);
    }

    if(forests_info.nforests_this_task == 0) {
        fprintf(stderr,"ThisTask=%d no forests to process...skipping\n",ThisTask);
        goto cleanup;
    }

    const int64_t Nforests = forests_info.nforests_this_task;
    
    /* allocate memory for the number of galaxies at each output snapshot */
    for(int n = 0; n < run_params->NOUT; n++) {
        /* using calloc removes the need to zero out the memory explicitly*/
        ForestNgals[n] = mycalloc(Nforests, sizeof(int));/* must be calloc*/
    }
    
    fprintf(stderr,"ThisTask = %d working on %"PRId64" forests\n", ThisTask, Nforests);
    
    /* open all the output files corresponding to this tree file (specified by rank) */
    initialize_galaxy_files(ThisTask, Nforests, save_fd, run_params);

    run_params->interrupted = 0;
    if(NTasks == 1 && ThisTask == 0) {
        init_my_progressbar(stderr, forests_info.totnforests, &(run_params->interrupted));
    }

    int64_t nforests_done = 0;
    for(int64_t forestnr = 0; forestnr < Nforests; forestnr++) {
        if(NTasks == 1 && ThisTask == 0) {
            my_progressbar(stderr, nforests_done, &(run_params->interrupted));
        }
        
        /* the millennium tree is really a collection of trees, viz., a forest */
        sage_per_forest(ThisTask, forestnr, TotGalaxies, ForestNgals, save_fd, &forests_info, run_params);

        nforests_done++;
    }
    finalize_galaxy_file(Nforests, (const int *) TotGalaxies, (const int **) ForestNgals, save_fd, run_params);

    for(int n = 0; n < run_params->NOUT; n++) {
        myfree(ForestNgals[n]);
    }
    
    if(NTasks == 1 && ThisTask == 0) {
        finish_myprogressbar(stderr, &(run_params->interrupted));
    }


cleanup:    
    /* sage is done running -> do the cleanup */
    cleanup_forests_io(run_params->TreeType, &forests_info);
    if(status == EXIT_SUCCESS) {
        
#if 0
        if(HDF5Output) {
            free_hdf5_ids();
            
#ifdef MPI
            // Create a single master HDF5 file with links to the other files...
            MPI_Barrier(MPI_COMM_WORLD);
            if (ThisTask == 0)
#endif
                write_master_file();
        }
#endif /* commented out the section for hdf5 output */    
        
        //free Ages. But first
        //reset Age to the actual allocated address
        run_params->Age--;
        myfree(run_params->Age);
    }

    return status;
}


static void sage_per_forest(const int ThisTask, const int forestnr, int *TotGalaxies, int **ForestNgals, int *save_fd,
                            struct forest_info *forests_info, struct params *run_params)
{
    
    /*  galaxy data  */
    struct GALAXY  *Gal = NULL, *HaloGal = NULL;
    
    /* simulation merger-tree data */
    struct halo_data *Halo = NULL;
    
    /*  auxiliary halo data  */
    struct halo_aux_data  *HaloAux = NULL;
    
    int nfofs_all_snaps[ABSOLUTEMAXSNAPS] = {0};

    /* nhalos is meaning-less for consistent-trees until *AFTER* the forest has been loaded */
    const int64_t nhalos = load_forest(run_params, forestnr, &Halo, forests_info);

    /* /\* need to actually set the nhalos value for CTREES*\/ */
    /* forests_info->totnhalos_per_forest[forestnr] = nhalos; */

    /* re-arrange the halos into a locally horizontal vertical forest */
    int32_t *file_ordering_of_halos=NULL;
    int status = reorder_lhalo_to_lhvt(nhalos, Halo, 0, &file_ordering_of_halos);/* the 3rd parameter is for testing the reorder code */
    if(status != EXIT_SUCCESS) {
        ABORT(status);
    }
    
    int maxgals = (int)(MAXGALFAC * nhalos);
    if(maxgals < 10000) maxgals = 10000;

    HaloAux = mymalloc(nhalos * sizeof(HaloAux[0]));
    HaloGal = mymalloc(maxgals * sizeof(HaloGal[0]));
    Gal = mymalloc(maxgals * sizeof(Gal[0]));/* used to be fof_maxgals instead of maxgals*/

    for(int i = 0; i < nhalos; i++) {
        HaloAux[i].HaloFlag = 0;
        HaloAux[i].NGalaxies = 0;
        HaloAux[i].DoneFlag = 0;
        HaloAux[i].orig_index = file_ordering_of_halos[i];
    }
    free(file_ordering_of_halos);
    /* done with re-ordering the halos into a locally horizontal vertical tree format */
    
    
    /* getting the number of FOF halos at each snapshot */
    get_nfofs_all_snaps(Halo, nhalos, nfofs_all_snaps, ABSOLUTEMAXSNAPS);
    
#if 0        
    for(int halonr = 0; halonr < nhalos; halonr++) {
        fprintf(stderr,"halonr = %d snap = %03d mvir = %14.6e firstfofhalo = %8d nexthalo = %8d\n",
                halonr, Halo[halonr].SnapNum, Halo[halonr].Mvir, Halo[halonr].FirstHaloInFOFgroup, Halo[halonr].NextHaloInFOFgroup);
    }
#endif
    
    int numgals = 0;
    int galaxycounter = 0;

#ifdef PROCESS_LHVT_STYLE
    /* this will be the new processing style --> one snapshot at a time */
    uint32_t ngal = 0;
    for(int snapshot=min_snapshot;snapshot <= max_snapshot; snapshot++) {
        uint32_t nfofs_this_snap = get_nfofs_at_snap(forestnr, snapshot);
        for(int ifof=0;ifof<nfofs_this_snap;ifof++) {
            ngal = process_fof_at_snap(ifof, snapshot, ngal);
        }
    }

#else
    
    /* First run construct_galaxies outside for loop -> takes care of the main tree */
    construct_galaxies(0, &numgals, &galaxycounter, &maxgals, Halo, HaloAux, &Gal, &HaloGal, run_params);
    
    /* But there are sub-trees within one forest file that are not reachable via the recursive routine -> do those as well */
    for(int halonr = 0; halonr < nhalos; halonr++) {
        if(HaloAux[halonr].DoneFlag == 0) {
            construct_galaxies(halonr, &numgals, &galaxycounter, &maxgals, Halo, HaloAux, &Gal, &HaloGal, run_params);
        }
    }

#endif /* PROCESS_LHVT_STYLE */    

    save_galaxies(ThisTask, forestnr, numgals, Halo, HaloAux, HaloGal, (int **) ForestNgals, (int *) TotGalaxies, save_fd, run_params);

    /* free galaxies and the forest */
    myfree(Gal);
    myfree(HaloGal);
    myfree(HaloAux);
    myfree(Halo);
}    


