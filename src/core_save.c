#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "core_allvars.h"
#include "core_save.h"
#include "core_utils.h"
#include "model_misc.h"

#define TREE_MUL_FAC        (1000000000LL)
#define THISTASK_MUL_FAC      (1000000000000000LL)

void initialize_galaxy_files(const int rank, const int ntrees, int *save_fd, const struct params *run_params)
{
    if(run_params->NOUT > ABSOLUTEMAXSNAPS) {
        fprintf(stderr,"Error: Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots\n",
                run_params->NOUT, ABSOLUTEMAXSNAPS);
        fprintf(stderr,"To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile\n");
        ABORT(INVALID_OPTION_IN_PARAMS);
    }

    char buffer[4*MAX_STRING_LEN + 1];
    /* Open all the output files */
    for(int n = 0; n < run_params->NOUT; n++) {
        snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_z%1.3f_%d", run_params->OutputDir, run_params->FileNameGalaxies,
                 run_params->ZZ[run_params->ListOutputSnaps[n]], rank);

        /* the last argument sets permissions as "rw-r--r--" (read/write owner, read group, read other)*/
        save_fd[n] = open(buffer,  O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (save_fd[n] < 0) {
            fprintf(stderr, "Error: Can't open file `%s'\n", buffer);
            ABORT(FILE_NOT_FOUND);
        }
        
        // write out placeholders for the header data.
        const off_t off = (ntrees + 2) * sizeof(int32_t);
        const off_t status = lseek(save_fd[n], off, SEEK_SET);
        if(status < 0) {
            fprintf(stderr, "Error: Failed to write out %d elements for header information for file %d. "
                    "Attempted to write %"PRId64" bytes\n", ntrees + 2, n, off);
            perror(NULL);
            ABORT(FILE_WRITE_ERROR);
        }
    }
}


void save_galaxies(const int ThisTask, const int tree, const int numgals, struct halo_data *halos,
                   struct halo_aux_data *haloaux, struct GALAXY *halogal, int **treengals, int *totgalaxies,
                   const int *save_fd, const struct params *run_params)
{
    int OutputGalCount[run_params->MAXSNAPS];
    // reset the output galaxy count and total number of output galaxies
    int cumul_output_ngal[run_params->MAXSNAPS];
    for(int i = 0; i < run_params->MAXSNAPS; i++) {
        OutputGalCount[i] = 0;
        cumul_output_ngal[i] = 0;
    }

    // track the order in which galaxies are written
    int *OutputGalOrder = calloc(numgals, sizeof(OutputGalOrder[0]));
    if(OutputGalOrder == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `OutputGalOrder`\n", numgals);
        ABORT(MALLOC_FAILURE);
    }

    for(int i = 0; i < numgals; i++) {
        OutputGalOrder[i] = -1;
        haloaux[i].output_snap_n = -1;
    }
  
    // first update mergeIntoID to point to the correct galaxy in the output
    for(int n = 0; n < run_params->NOUT; n++) {
        for(int i = 0; i < numgals; i++) {
            if(halogal[i].SnapNum == run_params->ListOutputSnaps[n]) {
                OutputGalOrder[i] = OutputGalCount[n];
                OutputGalCount[n]++;
                haloaux[i].output_snap_n = n;
            }
        }
    }

    int num_output_gals = 0;
    int num_gals_processed[run_params->MAXSNAPS];
    memset(num_gals_processed, 0, sizeof(num_gals_processed[0])*run_params->MAXSNAPS);
    for(int n = 0; n < run_params->NOUT; n++) {
        cumul_output_ngal[n] = num_output_gals;
        num_output_gals += OutputGalCount[n];
    }
    
    for(int i = 0; i < numgals; i++) {
        if(halogal[i].mergeIntoID > -1) {
            halogal[i].mergeIntoID = OutputGalOrder[halogal[i].mergeIntoID];
        }
    }

    struct GALAXY_OUTPUT *all_outputgals  = calloc(num_output_gals, sizeof(struct GALAXY_OUTPUT));
    if(all_outputgals == NULL) {
        fprintf(stderr,"Error: Could not allocate enough memory to hold all %d output galaxies\n",num_output_gals);
        ABORT(MALLOC_FAILURE);
    }

    for(int i = 0; i < numgals; i++) {
        if(haloaux[i].output_snap_n < 0) continue;
        int n = haloaux[i].output_snap_n;
        struct GALAXY_OUTPUT *galaxy_output = all_outputgals + cumul_output_ngal[n] + num_gals_processed[n];
        prepare_galaxy_for_output(ThisTask, tree, &halogal[i], galaxy_output, halos, haloaux, halogal, run_params);
        num_gals_processed[n]++;
        totgalaxies[n]++;
        treengals[n][tree]++;	      
    }    

    /* now write galaxies */
    for(int n=0;n<run_params->NOUT;n++) {
        struct GALAXY_OUTPUT *galaxy_output = all_outputgals + cumul_output_ngal[n];
        mywrite(save_fd[n], galaxy_output, sizeof(struct GALAXY_OUTPUT)*OutputGalCount[n]);
        /* int nwritten = myfwrite(galaxy_output, sizeof(struct GALAXY_OUTPUT), OutputGalCount[n], save_fd[n]); */
        /* if (nwritten != OutputGalCount[n]) { */
        /*     fprintf(stderr, "Error: Failed to write out the galaxy struct for galaxies within file %d. " */
        /*             " Meant to write %d elements but only wrote %d elements.\n", n, OutputGalCount[n], nwritten); */
        /*     perror(NULL); */
        /*     ABORT(FILE_WRITE_ERROR); */
        /* } */
    }

    // don't forget to free the workspace.
    free( OutputGalOrder );
    free(all_outputgals);
}



void prepare_galaxy_for_output(int ThisTask, int tree, struct GALAXY *g, struct GALAXY_OUTPUT *o,
                               struct halo_data *halos,
                               struct halo_aux_data *haloaux, struct GALAXY *halogal,
                               const struct params *run_params)
{
    o->SnapNum = g->SnapNum;
    if(g->Type < SHRT_MIN || g->Type > SHRT_MAX) {
        fprintf(stderr,"Error: Galaxy type = %d can not be represented in 2 bytes\n", g->Type);
        fprintf(stderr,"Converting galaxy type while saving from integer to short will result in data corruption");
        ABORT(EXIT_FAILURE);
    }
    o->Type = g->Type;

    // assume that because there are so many files, the trees per file will be less than 100000
    // required for limits of long long
    if(run_params->LastFile>=10000) {
        assert( g->GalaxyNr < TREE_MUL_FAC ); // breaking tree size assumption
        assert(tree < (THISTASK_MUL_FAC/10)/TREE_MUL_FAC);
        o->GalaxyIndex = g->GalaxyNr + TREE_MUL_FAC * tree + (THISTASK_MUL_FAC/10) * ThisTask;
        assert( (o->GalaxyIndex - g->GalaxyNr - TREE_MUL_FAC*tree)/(THISTASK_MUL_FAC/10) == ThisTask );
        assert( (o->GalaxyIndex - g->GalaxyNr -(THISTASK_MUL_FAC/10)*ThisTask) / TREE_MUL_FAC == tree );
        assert( o->GalaxyIndex - TREE_MUL_FAC*tree - (THISTASK_MUL_FAC/10)*ThisTask == g->GalaxyNr );
        o->CentralGalaxyIndex = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * tree + (THISTASK_MUL_FAC/10) * ThisTask;
    } else {
        assert( g->GalaxyNr < TREE_MUL_FAC ); // breaking tree size assumption
        assert(tree < THISTASK_MUL_FAC/TREE_MUL_FAC);
        o->GalaxyIndex = g->GalaxyNr + TREE_MUL_FAC * tree + THISTASK_MUL_FAC * ThisTask;
        assert( (o->GalaxyIndex - g->GalaxyNr - TREE_MUL_FAC*tree)/THISTASK_MUL_FAC == ThisTask );
        assert( (o->GalaxyIndex - g->GalaxyNr -THISTASK_MUL_FAC*ThisTask) / TREE_MUL_FAC == tree );
        assert( o->GalaxyIndex - TREE_MUL_FAC*tree - THISTASK_MUL_FAC*ThisTask == g->GalaxyNr );
        o->CentralGalaxyIndex = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * tree + THISTASK_MUL_FAC * ThisTask;
    }

#undef TREE_MUL_FAC
#undef THISTASK_MUL_FAC

    o->SAGEHaloIndex = g->HaloNr;/* if the original input halonr is required, then use haloaux[halonr].orig_index: MS 29/6/2018 */
    o->SAGETreeIndex = tree;
    o->SimulationHaloIndex = llabs(halos[g->HaloNr].MostBoundID);
#if 0
    o->isFlyby = halos[g->HaloNr].MostBoundID < 0 ? 1:0;
#endif  

    o->mergeType = g->mergeType;
    o->mergeIntoID = g->mergeIntoID;
    o->mergeIntoSnapNum = g->mergeIntoSnapNum;
    o->dT = g->dT * run_params->UnitTime_in_s / SEC_PER_MEGAYEAR;

    for(int j = 0; j < 3; j++) {
        o->Pos[j] = g->Pos[j];
        o->Vel[j] = g->Vel[j];
        o->Spin[j] = halos[g->HaloNr].Spin[j];
    }
    
    o->Len = g->Len;
    o->Mvir = g->Mvir;
    o->CentralMvir = get_virial_mass(halos[g->HaloNr].FirstHaloInFOFgroup, halos, run_params);
    o->Rvir = get_virial_radius(g->HaloNr, halos, run_params);  // output the actual Rvir, not the maximum Rvir
    o->Vvir = get_virial_velocity(g->HaloNr, halos, run_params);  // output the actual Vvir, not the maximum Vvir
    o->Vmax = g->Vmax;
    o->VelDisp = halos[g->HaloNr].VelDisp;

    o->ColdGas = g->ColdGas;
    o->StellarMass = g->StellarMass;
    o->BulgeMass = g->BulgeMass;
    o->HotGas = g->HotGas;
    o->EjectedMass = g->EjectedMass;
    o->BlackHoleMass = g->BlackHoleMass;
    o->ICS = g->ICS;

    o->MetalsColdGas = g->MetalsColdGas;
    o->MetalsStellarMass = g->MetalsStellarMass;
    o->MetalsBulgeMass = g->MetalsBulgeMass;
    o->MetalsHotGas = g->MetalsHotGas;
    o->MetalsEjectedMass = g->MetalsEjectedMass;
    o->MetalsICS = g->MetalsICS;
  
    o->SfrDisk = 0.0;
    o->SfrBulge = 0.0;
    o->SfrDiskZ = 0.0;
    o->SfrBulgeZ = 0.0;
  
    // NOTE: in Msun/yr
    for(int step = 0; step < STEPS; step++) {
        o->SfrDisk += g->SfrDisk[step] * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
        o->SfrBulge += g->SfrBulge[step] * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
        
        if(g->SfrDiskColdGas[step] > 0.0) {
            o->SfrDiskZ += g->SfrDiskColdGasMetals[step] / g->SfrDiskColdGas[step] / STEPS;
        }
        
        if(g->SfrBulgeColdGas[step] > 0.0) {
            o->SfrBulgeZ += g->SfrBulgeColdGasMetals[step] / g->SfrBulgeColdGas[step] / STEPS;
        }
    }

    o->DiskScaleRadius = g->DiskScaleRadius;

    if (g->Cooling > 0.0) {
        o->Cooling = log10(g->Cooling * run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s);
    } else {
        o->Cooling = 0.0;
    }

    if (g->Heating > 0.0) {
        o->Heating = log10(g->Heating * run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s);
    } else {
        o->Heating = 0.0;
    }

    o->QuasarModeBHaccretionMass = g->QuasarModeBHaccretionMass;

    o->TimeOfLastMajorMerger = g->TimeOfLastMajorMerger * run_params->UnitTime_in_Megayears;
    o->TimeOfLastMinorMerger = g->TimeOfLastMinorMerger * run_params->UnitTime_in_Megayears;
	
    o->OutflowRate = g->OutflowRate * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS;

    //infall properties
    if(g->Type != 0) {
        o->infallMvir = g->infallMvir;
        o->infallVvir = g->infallVvir;
        o->infallVmax = g->infallVmax;
    } else {
        o->infallMvir = 0.0;
        o->infallVvir = 0.0;
        o->infallVmax = 0.0;
    }
}


int finalize_galaxy_file(const int ntrees, const int *totgalaxies, const int **treengals, int *save_fd, const struct params *run_params) 
{
    if(run_params->NOUT > ABSOLUTEMAXSNAPS) {
        fprintf(stderr,"Error: Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots\n",
                run_params->NOUT, ABSOLUTEMAXSNAPS);
        fprintf(stderr,"To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile\n");
        ABORT(INVALID_OPTION_IN_PARAMS);
    }

    for(int n = 0; n < run_params->NOUT; n++) {
        // file must already be open.
        XASSERT( save_fd[n] > 0, EXIT_FAILURE, "Error: for output # %d, output file pointer is NULL\n", n);

        mypwrite(save_fd[n], &ntrees, sizeof(ntrees), 0);
        mypwrite(save_fd[n], &totgalaxies[n], sizeof(int), sizeof(int));
        mypwrite(save_fd[n], treengals[n], sizeof(int)*ntrees, sizeof(int) + sizeof(int));
        /* int nwritten = myfwrite(&ntrees, sizeof(int), 1, save_fd[n]); */
        /* if (nwritten != 1) { */
        /*     fprintf(stderr, "Error: Failed to write out 1 element for the number of trees for the header of file %d.\n" */
        /*             "Only wrote %d elements.\n", n, nwritten); */
        /* } */

        /* nwritten = myfwrite(&totgalaxies[n], sizeof(int), 1, save_fd[n]);  */
        /* if (nwritten != 1) { */
        /*     fprintf(stderr, "Error: Failed to write out 1 element for the number of galaxies for the header of file %d.\n" */
        /*             "Only wrote %d elements.\n", n, nwritten); */
        /* } */
        
        /* nwritten = myfwrite(treengals[n], sizeof(int), ntrees, save_fd[n]); */
        /* if (nwritten != ntrees) { */
        /*     fprintf(stderr, "Error: Failed to write out %d elements for the number of galaxies per tree for the header of file %d.\n" */
        /*             "Only wrote %d elements.\n", ntrees, n, nwritten); */
        /* } */


        // close the file and clear handle after everything has been written
        close( save_fd[n] );
        save_fd[n] = -1;
    }

    return EXIT_SUCCESS;
}

