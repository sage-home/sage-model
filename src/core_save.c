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

#include "io/save_gals_binary.h"

#ifdef HDF5
#include "io/save_gals_hdf5.h"
#endif

#define TREE_MUL_FAC        (1000000000LL)
#define THISTASK_MUL_FAC      (1000000000000000LL)

int32_t initialize_galaxy_files(const int rank, const int ntrees, struct save_info *save_info, const struct params *run_params)
{
    int32_t status;

    if(run_params->NOUT > ABSOLUTEMAXSNAPS) {
        fprintf(stderr,"Error: Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots\n",
                run_params->NOUT, ABSOLUTEMAXSNAPS);
        fprintf(stderr,"To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile\n");
        return INVALID_OPTION_IN_PARAMS;
    }

    switch(run_params->OutputFormat) {

    case(binary):
      status = initialize_binary_galaxy_files(rank, ntrees, save_info, run_params);
      break;

    case(hdf5):
      status = initialize_hdf5_galaxy_files(rank, ntrees, save_info, run_params);
      break; 

    default:
      fprintf(stderr, "Error: Unknown OutputFormat.\n");
      status = INVALID_OPTION_IN_PARAMS;
      break;

    }

    return status; 
}


void save_galaxies(const int ThisTask, const int tree, const int numgals, struct halo_data *halos,
                   struct halo_aux_data *haloaux, struct GALAXY *halogal, int **treengals, int *totgalaxies,
                   struct save_info *save_info, const struct params *run_params)
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

      // Prepare all the galaxies for output.
      for(int i = 0; i < numgals; i++) {
          if(haloaux[i].output_snap_n < 0) continue;
          int n = haloaux[i].output_snap_n;

          switch(run_params->OutputFormat) {

          case(binary):;
              // Here we move the offset pointer depending upon the number of galaxies processed up to this point. 
              struct GALAXY_OUTPUT *galaxy_output = all_outputgals + cumul_output_ngal[n] + num_gals_processed[n];
              prepare_galaxy_for_output(ThisTask, tree, &halogal[i], galaxy_output, halos, haloaux, halogal, run_params);
              break;

          case(hdf5):;
              struct GALAXY_OUTPUT hdf5_galaxy_output;
              prepare_galaxy_for_output(ThisTask, tree, &halogal[i], &hdf5_galaxy_output, halos, haloaux, halogal, run_params);

              save_info->buffer_output_gals[n][save_info->num_gals_in_buffer[n]] = hdf5_galaxy_output;
              save_info->num_gals_in_buffer[n]++;

              //fprintf(stderr, "num_gals Snap %d = %d\n", n, save_info->num_gals_in_buffer[n]);
              if(save_info->num_gals_in_buffer[n] == save_info->buffer_size) {

                // To save the galaxies, we must first extend the size of the dataset to accomodate the new data.
                herr_t status;
                hsize_t dims_extend[1];
                dims_extend[0] = (hsize_t) save_info->buffer_size;

                hsize_t old_dims[1];
                old_dims[0] = (hsize_t) save_info->gals_written_snap[n];
                hsize_t new_dims[1];
                new_dims[0] = old_dims[0] + (hsize_t) save_info->buffer_size;
                hid_t dataset_id = save_info->dataset_ids[0];

                fprintf(stderr, "Gals written %"PRId64"\n", save_info->gals_written_snap[n]);
                status = H5Dset_extent(dataset_id, new_dims);
                hid_t filespace = H5Dget_space(dataset_id);
                status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL);

                hid_t memspace = H5Screate_simple(1, dims_extend, NULL);

                int32_t *tmp_buffer;
                tmp_buffer = calloc(sizeof(*(tmp_buffer)), save_info->buffer_size);
                for(int32_t j = 0; j < save_info->buffer_size; j++) {
                  tmp_buffer[j] = save_info->buffer_output_gals[n][j].Len;
                }
                status = H5Dwrite(dataset_id, H5T_NATIVE_INT, memspace, filespace, H5P_DEFAULT, tmp_buffer);
                status = H5Dclose(dataset_id);

                save_info->num_gals_in_buffer[n] = 0; 
                free(tmp_buffer);

              } 

              break;

          default:
              fprintf(stderr, "Uknown OutputFormat.\n");
              ABORT(10341);
          }

          num_gals_processed[n]++;
          totgalaxies[n]++;
          treengals[n][tree]++;	      
      }    

      // Now perform one write action for each redshift output.
      for(int n=0;n<run_params->NOUT;n++) {

          // Shift the offset pointer depending upon how many galaxies have been written out.
          struct GALAXY_OUTPUT *galaxy_output = all_outputgals + cumul_output_ngal[n];

          // Then write out the chunk of galaxies for this redshift output.
          
          switch(run_params->OutputFormat) {

          case(binary):;
              ssize_t nwritten = mywrite(save_info->save_fd[n], galaxy_output, sizeof(struct GALAXY_OUTPUT)*OutputGalCount[n]);
              if (nwritten != (ssize_t) (sizeof(struct GALAXY_OUTPUT)*OutputGalCount[n])) { 
                  fprintf(stderr, "Error: Failed to write out the galaxy struct for galaxies within file %d. "
                                  "Meant to write out %d elements with a total of %"PRId64" bytes (%zu bytes for each element). "
                                  "However, I wrote out a total of %"PRId64" bytes.\n",
                                  n, OutputGalCount[n], sizeof(struct GALAXY_OUTPUT)*OutputGalCount[n], sizeof(struct GALAXY_OUTPUT),
                                  nwritten);
                  perror(NULL);
                  ABORT(FILE_WRITE_ERROR);            
              }

          case(hdf5):
              // Add the galaxy_output to the buffer.
              // Update the counter.
              // If we're at or beyond the buffer, size, extend and write.
              //fprintf(stderr, "NOT IMPLEMENTED\n");
              break;

          default:
              fprintf(stderr, "Uknown OutputFormat.\n");
              ABORT(10341);
          }
      }

      // don't forget to free the workspace.
      free(OutputGalOrder);
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


int finalize_galaxy_file(const int ntrees, const int *totgalaxies, const int **treengals, struct save_info *save_info, const struct params *run_params) 
{
    if(run_params->NOUT > ABSOLUTEMAXSNAPS) {
        fprintf(stderr,"Error: Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots\n",
                run_params->NOUT, ABSOLUTEMAXSNAPS);
        fprintf(stderr,"To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile\n");
        ABORT(INVALID_OPTION_IN_PARAMS);
    }

    for(int n = 0; n < run_params->NOUT; n++) {
        // file must already be open.
        XASSERT( save_info->save_fd[n] > 0, EXIT_FAILURE, "Error: for output # %d, output file pointer is NULL\n", n);

        mypwrite(save_info->save_fd[n], &ntrees, sizeof(ntrees), 0);
        mypwrite(save_info->save_fd[n], &totgalaxies[n], sizeof(int), sizeof(int));
        mypwrite(save_info->save_fd[n], treengals[n], sizeof(int)*ntrees, sizeof(int) + sizeof(int));
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
        close( save_info->save_fd[n] );
        save_info->save_fd[n] = -1;
    }

    return EXIT_SUCCESS;
}

