#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

#include "save_gals_binary.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"
#include "../model_misc.h"

// Local Proto-Types //

int32_t prepare_galaxy_for_output(int32_t filenr, int32_t treenr, struct GALAXY *g, struct GALAXY_OUTPUT *o, struct halo_data *halos,
                                  struct halo_aux_data *haloaux, struct GALAXY *halogal, const struct params *run_params);

// Externally Visible Functions //

int32_t initialize_binary_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info,
                                       const struct params *run_params)
{

    // We open up files for each output. We'll store the file IDs of each of these file. 
    save_info->save_fd = malloc(sizeof(int32_t) * run_params->NOUT);

    char buffer[4*MAX_STRING_LEN + 1];
    /* Open all the output files */
    for(int n = 0; n < run_params->NOUT; n++) {
        snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_z%1.3f_%d", run_params->OutputDir, run_params->FileNameGalaxies,
                 run_params->ZZ[run_params->ListOutputSnaps[n]], filenr);

        /* the last argument sets permissions as "rw-r--r--" (read/write owner, read group, read other)*/
        save_info->save_fd[n] = open(buffer,  O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        CHECK_STATUS_AND_RETURN_ON_FAIL(save_info->save_fd[n], FILE_NOT_FOUND,
                                        "Can't open file %s for initialization.\n", buffer);

        // write out placeholders for the header data.
        const off_t off = (ntrees + 2) * sizeof(int32_t);
        const off_t status = lseek(save_info->save_fd[n], off, SEEK_SET);
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, FILE_WRITE_ERROR,
                                        "Error: Failed to write out %d elements for header information for file %d.\n"
                                        "Attempted to write %"PRId64" bytes\n", ntrees + 2, n, off);
    }

    return EXIT_SUCCESS;
}


int32_t save_binary_galaxies(const int32_t filenr, const int32_t treenr, const int32_t num_gals, const int32_t *OutputGalCount,
                             struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                             struct save_info *save_info, const struct params *run_params)
{

    int32_t status = EXIT_FAILURE;

    // Determine the offset to the block of galaxies for each snapshot.
    int32_t num_output_gals = 0;
    int32_t *num_gals_processed = calloc(run_params->MAXSNAPS, sizeof(*(num_gals_processed)));
    if(num_gals_processed == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `num_gals_proccessed`\n", run_params->MAXSNAPS);
        return MALLOC_FAILURE;
    }

    int32_t *cumul_output_ngal = calloc(run_params->NOUT, sizeof(*(cumul_output_ngal)));
    if(cumul_output_ngal == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `cumul_output_ngal`\n", run_params->NOUT);
        return MALLOC_FAILURE;
    }

    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {
        cumul_output_ngal[snap_idx] = num_output_gals;
        num_output_gals += OutputGalCount[snap_idx];
    }

    // We store all the galaxies to be written for this tree in a single memory block.  Later we
    // will then perform a single write for each snapshot, pointing to the correct position in
    // the block.
    struct GALAXY_OUTPUT *all_outputgals  = calloc(num_output_gals, sizeof(struct GALAXY_OUTPUT));
    if(all_outputgals == NULL) {
        fprintf(stderr,"Error: Could not allocate enough memory to hold all %d output galaxies\n",num_output_gals);
        return MALLOC_FAILURE;
    }

    // Prepare all the galaxies for output.
    for(int32_t gal_idx = 0; gal_idx < num_gals; gal_idx++) {
        if(haloaux[gal_idx].output_snap_n < 0) {
            continue;
        }
        int32_t snap_idx = haloaux[gal_idx].output_snap_n;

        // Here we move the offset pointer depending upon the number of galaxies processed up to this point.
        struct GALAXY_OUTPUT *galaxy_output = all_outputgals + cumul_output_ngal[snap_idx] + num_gals_processed[snap_idx];
        status = prepare_galaxy_for_output(filenr, treenr, &halogal[gal_idx], galaxy_output, halos, haloaux, halogal, run_params);
        if(status != EXIT_SUCCESS) {
          return status;
        }

        // Update the running totals.
        save_info->tot_ngals[snap_idx]++;
        save_info->forest_ngals[snap_idx][treenr]++;
        num_gals_processed[snap_idx]++;
    }

    // Now perform one write action for each redshift output.
    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {

        // Shift the offset pointer depending upon how many galaxies have been written out.
        struct GALAXY_OUTPUT *galaxy_output = all_outputgals + cumul_output_ngal[snap_idx];

        // Then write out the chunk of galaxies for this redshift output.
        ssize_t nwritten = mywrite(save_info->save_fd[snap_idx], galaxy_output, sizeof(struct GALAXY_OUTPUT)*OutputGalCount[snap_idx]);
        if (nwritten != (ssize_t) (sizeof(struct GALAXY_OUTPUT)*OutputGalCount[snap_idx])) {
            fprintf(stderr, "Error: Failed to write out the galaxy struct for galaxies within file %d. "
                            "Meant to write out %d elements with a total of %zu bytes (%zu bytes for each element). "
                            "However, I wrote out a total of %zd bytes.\n",
                            snap_idx, OutputGalCount[snap_idx], sizeof(struct GALAXY_OUTPUT)*OutputGalCount[snap_idx], sizeof(struct GALAXY_OUTPUT),
                            nwritten);
            return FILE_WRITE_ERROR;
        }
    }
    free(all_outputgals);
    free(cumul_output_ngal);
    free(num_gals_processed);

    return EXIT_SUCCESS;
}

int32_t finalize_binary_galaxy_files(const int ntrees, struct save_info *save_info, const struct params *run_params)
{

    int32_t nwritten;

    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {
        // File must already be open.
        CHECK_STATUS_AND_RETURN_ON_FAIL(save_info->save_fd[snap_idx], EXIT_FAILURE,
                                        "Error trying to write to output number %d.\nThe save pointer is %d.\n",
                                        snap_idx, save_info->save_fd[snap_idx]);

        // Write the header data.
        nwritten = mypwrite(save_info->save_fd[snap_idx], &ntrees, sizeof(ntrees), 0);
        if(nwritten != sizeof(int32_t)) {
            fprintf(stderr, "Error: Failed to write out 1 element for the number of trees for the header of file %d.\n"
                            "Wrote %d bytes instead of %zu.\n", snap_idx, nwritten, sizeof(ntrees));
            return FILE_WRITE_ERROR;
        }

        nwritten = mypwrite(save_info->save_fd[snap_idx], &save_info->tot_ngals[snap_idx], sizeof(int32_t), sizeof(int32_t));
        if(nwritten != sizeof(int32_t)) {
            fprintf(stderr, "Error: Failed to write out 1 element for the total number of galaxies for the header of file %d.\n"
                            "Wrote %d bytes instead of %zu.\n", snap_idx, nwritten, sizeof(*(save_info->tot_ngals)));
            return FILE_WRITE_ERROR;
        }

        nwritten = mypwrite(save_info->save_fd[snap_idx], save_info->forest_ngals[snap_idx], sizeof(int32_t)*ntrees, sizeof(int32_t) + sizeof(int32_t));
        if(nwritten != (int32_t) (ntrees * sizeof(int32_t))) {
            fprintf(stderr, "Error: Failed to write out %d elements for the number of galaxies per tree for the header of file %d.\n"
                            "Wrote %d bytes instead of %zu.\n", ntrees, snap_idx, nwritten, sizeof(*(save_info->forest_ngals))*ntrees);
            return FILE_WRITE_ERROR;
        }

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

        // Close the file and clear handle after everything has been written.
        close(save_info->save_fd[snap_idx]);
        save_info->save_fd[snap_idx] = -1;
    }

    free(save_info->save_fd);

    return EXIT_SUCCESS;
}

// Local Functions //

#define TREE_MUL_FAC        (1000000000LL)
#define THISTASK_MUL_FAC      (1000000000000000LL)

int32_t prepare_galaxy_for_output(int32_t filenr, int32_t treenr, struct GALAXY *g, struct GALAXY_OUTPUT *o, struct halo_data *halos,
                                  struct halo_aux_data *haloaux, struct GALAXY *halogal, const struct params *run_params)
{
    o->SnapNum = g->SnapNum;
    if(g->Type < SHRT_MIN || g->Type > SHRT_MAX) {
        fprintf(stderr,"Error: Galaxy type = %d can not be represented in 2 bytes\n", g->Type);
        fprintf(stderr,"Converting galaxy type while saving from integer to short will result in data corruption");
        return EXIT_FAILURE;
    }
    o->Type = g->Type;

    // assume that because there are so many files, the trees per file will be less than 100000
    // required for limits of long long
    if(run_params->LastFile>=10000) {
        if(g->GalaxyNr > TREE_MUL_FAC || treenr > (THISTASK_MUL_FAC/10)/TREE_MUL_FAC) {
            fprintf(stderr, "We assume there is a maximum of 2^64 - 1 trees.  This assumption has been broken.\n"
                            "File number %d\ttree number %d\tGalaxy Number %d\tHalo number %d\n", filenr, treenr,
                            g->GalaxyNr, g->HaloNr);
        }

        o->GalaxyIndex = g->GalaxyNr + TREE_MUL_FAC * treenr + (THISTASK_MUL_FAC/10) * filenr;
        o->CentralGalaxyIndex = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * treenr + (THISTASK_MUL_FAC/10) * filenr;
    } else {
        if(g->GalaxyNr > TREE_MUL_FAC || treenr > THISTASK_MUL_FAC/TREE_MUL_FAC) {
            fprintf(stderr, "We assume there is a maximum of 2^64 - 1 trees.  This assumption has been broken.\n"
                            "File number %d\ttree number %d\tGalaxy Number %d\tHalo number %d\n", filenr, treenr,
                            g->GalaxyNr, g->HaloNr);
        }
        o->GalaxyIndex = g->GalaxyNr + TREE_MUL_FAC * treenr + THISTASK_MUL_FAC * filenr;
        o->CentralGalaxyIndex = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * treenr + THISTASK_MUL_FAC * filenr;
    }

    o->SAGEHaloIndex = g->HaloNr;/* if the original input halonr is required, then use haloaux[halonr].orig_index: MS 29/6/2018 */
    o->SAGETreeIndex = treenr;
    o->SimulationHaloIndex = llabs(halos[g->HaloNr].MostBoundID);

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

    return EXIT_SUCCESS;
}

#undef TREE_MUL_FAC
#undef THISTASK_MUL_FAC

