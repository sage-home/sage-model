#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

#include "save_gals_binary.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"
#include "../model_misc.h"

#ifdef USE_BUFFERED_WRITE
#include "buffered_io.h"
static struct buffered_io *all_buffers = NULL;
#endif


// Local Proto-Types //

static int32_t prepare_galaxy_for_output(struct GALAXY *g, struct GALAXY_OUTPUT *o, struct halo_data *halos,
                                         const int32_t original_treenr, const struct params *run_params);

// Externally Visible Functions //

int32_t initialize_binary_galaxy_files(const int filenr, const struct forest_info *forest_info, struct save_info *save_info,
                                       const struct params *run_params)
{

    const int32_t ntrees = forest_info->nforests_this_task;
    const off_t halo_data_start_offset = (ntrees + 2) * sizeof(int32_t);

    // We open up files for each output. We'll store the file IDs of each of these file.
    save_info->save_fd = mymalloc(run_params->NumSnapOutputs * sizeof(int32_t));

    char buffer[4*MAX_STRING_LEN + 1];

    /* Open all the output files */
    for(int n = 0; n < run_params->NumSnapOutputs; n++) {
        snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_z%1.3f_%d", run_params->OutputDir, run_params->FileNameGalaxies,
                 run_params->ZZ[run_params->ListOutputSnaps[n]], filenr);

        /* the last argument sets permissions as "rw-r--r--" (read/write owner, read group, read other)*/
        save_info->save_fd[n] = open(buffer,  O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        CHECK_STATUS_AND_RETURN_ON_FAIL(save_info->save_fd[n], FILE_NOT_FOUND,
                                        "Can't open file %s for initialization.\n", buffer);

        // write out placeholders for the header data.
        const off_t status = lseek(save_info->save_fd[n], halo_data_start_offset, SEEK_SET);
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, FILE_WRITE_ERROR,
                                        "Error: Failed to write out %d elements for header information for file %d.\n"
                                        "Attempted to write %"PRId64" bytes\n", ntrees + 2, n, halo_data_start_offset);
    }

#ifdef USE_BUFFERED_WRITE
    const size_t buffer_size = 8 * 1024 * 1024; //8 MB
    all_buffers = malloc(sizeof(*all_buffers)*run_params->NumSnapOutputs);
    XRETURN(all_buffers != NULL, -1, "Error: Could not allocate %d elements of size %zu bytes for buffered io\n", 
                                                      run_params->NumSnapOutputs, sizeof(*all_buffers));
    for(int n = 0; n < run_params->NumSnapOutputs; n++) {
        int fd = save_info->save_fd[n];
        int status = setup_buffered_io(&all_buffers[n], buffer_size, fd, halo_data_start_offset);
        if(status != EXIT_SUCCESS) {
            fprintf(stderr,"Error: Could not setup buffered io\n");
            return -1;
        }   
    }
#endif

    return EXIT_SUCCESS;
}


int32_t save_binary_galaxies(const int32_t task_treenr, const int32_t num_gals, const int32_t *OutputGalCount,
                             struct forest_info *forest_info, struct halo_data *halos, struct halo_aux_data *haloaux,
                             struct GALAXY *halogal, struct save_info *save_info, const struct params *run_params)
{

    int32_t status = EXIT_FAILURE;

    // Determine the offset to the block of galaxies for each snapshot.
    int32_t num_output_gals = 0;
    int32_t *num_gals_processed = mycalloc(run_params->SimMaxSnaps, sizeof(*(num_gals_processed)));
    if(num_gals_processed == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `num_gals_proccessed`\n", run_params->SimMaxSnaps);
        return MALLOC_FAILURE;
    }

    int32_t *cumul_output_ngal = mymalloc(run_params->NumSnapOutputs * sizeof(*(cumul_output_ngal)));
    if(cumul_output_ngal == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `cumul_output_ngal`\n", run_params->NumSnapOutputs);
        return MALLOC_FAILURE;
    }

    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {
        cumul_output_ngal[snap_idx] = num_output_gals;
        num_output_gals += OutputGalCount[snap_idx];
    }

    // We store all the galaxies to be written for this tree in a single memory block.  Later we
    // will then perform a single write for each snapshot, pointing to the correct position in
    // the block.
    struct GALAXY_OUTPUT *all_outputgals  = mymalloc(num_output_gals * sizeof(all_outputgals[0]));
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
        status = prepare_galaxy_for_output(&halogal[gal_idx],  galaxy_output, halos,
                                           forest_info->original_treenr[task_treenr], run_params);
        if(status != EXIT_SUCCESS) {
          return status;
        }

        // Update the running totals.
        save_info->tot_ngals[snap_idx]++;
        save_info->forest_ngals[snap_idx][task_treenr]++;
        num_gals_processed[snap_idx]++;
    }

    // Now perform one write action for each redshift output.
    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {

        // Shift the offset pointer depending upon how many galaxies have been written out.
        struct GALAXY_OUTPUT *galaxy_output = all_outputgals + cumul_output_ngal[snap_idx];

        // Then write out the chunk of galaxies for this redshift output.
        const size_t numbytes = sizeof(struct GALAXY_OUTPUT)*OutputGalCount[snap_idx];

#ifdef USE_BUFFERED_WRITE
        status = write_buffered_io(&all_buffers[snap_idx], galaxy_output, numbytes);
        if(status < 0) {
            fprintf(stderr,"Error: Could not write (buffered). snapshot number = %d number of bytes = %zu\n", snap_idx, numbytes);
            return status;
        }
#else
        ssize_t nwritten = mywrite(save_info->save_fd[snap_idx], galaxy_output, numbytes);
        if (nwritten != (ssize_t) numbytes) {
            fprintf(stderr, "Error: Failed to write out the galaxy struct for galaxies within file %d. "
                            "Meant to write out %d elements with a total of %zu bytes (%zu bytes for each element). "
                            "However, I wrote out a total of %zd bytes.\n",
                            snap_idx, OutputGalCount[snap_idx], numbytes, sizeof(struct GALAXY_OUTPUT),
                            nwritten);
            return FILE_WRITE_ERROR;
        }
#endif
    }
    myfree(all_outputgals);
    myfree(cumul_output_ngal);
    myfree(num_gals_processed);

    return EXIT_SUCCESS;
}

int32_t finalize_binary_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info, const struct params *run_params)
{

    int32_t ntrees = forest_info->nforests_this_task;
    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {
        // File must already be open.
        CHECK_STATUS_AND_RETURN_ON_FAIL(save_info->save_fd[snap_idx], EXIT_FAILURE,
                                        "Error trying to write to output number %d.\nThe file handle is %d.\n",
                                        snap_idx, save_info->save_fd[snap_idx]);
#ifdef USE_BUFFERED_WRITE
        int status = cleanup_buffered_io(&all_buffers[snap_idx]);
        if(status != EXIT_SUCCESS) {
            fprintf(stderr,"Error: Could not finalise the output file for snapshot = %d\n", snap_idx);
            return status;
        }
#endif

        // Write the header data.
        int32_t nwritten = mypwrite(save_info->save_fd[snap_idx], &ntrees, sizeof(ntrees), 0);
        if(nwritten != sizeof(int32_t)) {
            fprintf(stderr, "Error: Failed to write out 1 element for the number of trees for the header of file %d.\n"
                            "Wrote %d bytes instead of %zu.\n", snap_idx, nwritten, sizeof(ntrees));
            return FILE_WRITE_ERROR;
        }

        if(save_info->tot_ngals[snap_idx] > INT_MAX) {
            fprintf(stderr,"Error: totngals = %"PRId64" at snapshot index = %d exceeds the 32-bit integer limit\n",
                    save_info->tot_ngals[snap_idx], snap_idx);
            return FILE_WRITE_ERROR;
        }

        const int32_t ng = (int32_t) save_info->tot_ngals[snap_idx];
        nwritten = mypwrite(save_info->save_fd[snap_idx], &ng, sizeof(ng), sizeof(int32_t));
        if(nwritten != sizeof(ng)) {
            fprintf(stderr, "Error: Failed to write out 1 element for the total number of galaxies for the header of file %d.\n"
                            "Wrote %d bytes instead of %zu.\n", snap_idx, nwritten, sizeof(ng));
            return FILE_WRITE_ERROR;
        }

        nwritten = mypwrite(save_info->save_fd[snap_idx], save_info->forest_ngals[snap_idx], sizeof(int32_t)*ntrees, sizeof(int32_t) + sizeof(int32_t));
        if(nwritten != (int32_t) (ntrees * sizeof(int32_t))) {
            fprintf(stderr, "Error: Failed to write out %d elements for the number of galaxies per tree for the header of file %d.\n"
                            "Wrote %d bytes instead of %zu.\n", ntrees, snap_idx, nwritten, sizeof(*(save_info->forest_ngals))*ntrees);
            return FILE_WRITE_ERROR;
        }

        // Close the file and clear handle after everything has been written.
        close(save_info->save_fd[snap_idx]);
        save_info->save_fd[snap_idx] = -1;
    }

    myfree(save_info->save_fd);

#ifdef USE_BUFFERED_WRITE
    free(all_buffers);
#endif

    return EXIT_SUCCESS;
}

// Local Functions //

int32_t prepare_galaxy_for_output(struct GALAXY *g, struct GALAXY_OUTPUT *o, struct halo_data *halos,
                                  const int32_t original_treenr, const struct params *run_params)
{
    if(g == NULL || o == NULL) {
        fprintf(stderr,"Error: Either the input galaxy (address = %p) or the output galaxy (address = %p) is NULL\n", g, o);
        return -1;
    }

    o->SnapNum = g->SnapNum;
    if(g->Type < SHRT_MIN || g->Type > SHRT_MAX) {
        fprintf(stderr,"Error: Galaxy type = %d can not be represented in 2 bytes\n", g->Type);
        fprintf(stderr,"Converting galaxy type while saving from integer to short will result in data corruption");
        return EXIT_FAILURE;
    }
    o->Type = g->Type;

    o->GalaxyIndex = g->GalaxyIndex;
    o->CentralGalaxyIndex = g->CentralGalaxyIndex;

    o->SAGEHaloIndex = g->HaloNr;/* if the original input halonr is required, then use haloaux[halonr].orig_index: MS 29/6/2018 */
    o->SAGETreeIndex = original_treenr;
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
