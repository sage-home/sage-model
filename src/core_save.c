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

// Local Proto-Types //
void prepare_galaxy_for_hdf5_output(int ThisTask, int tree, struct GALAXY *g, struct save_info *save_info,
                                    int32_t output_snap_idx,
                                    struct halo_data *halos,
                                    struct halo_aux_data *haloaux, struct GALAXY *halogal,
                                    const struct params *run_params);

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

#define WRITE_GALAXY_DATASET(snap_idx, field_name, h5_dtype) { \
    status = H5Dwrite(dataset_id, h5_dtype, memspace, filespace, H5P_DEFAULT, (save_info->buffer_output_gals[snap_idx]).field_name); \
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

          case(hdf5):
              prepare_galaxy_for_hdf5_output(ThisTask, tree, &halogal[i], save_info, n, halos, haloaux, halogal, run_params);
              save_info->num_gals_in_buffer[n]++;

              //fprintf(stderr, "num_gals Snap %d = %d\n", n, save_info->num_gals_in_buffer[n]);
              if(save_info->num_gals_in_buffer[n] == save_info->buffer_size) {


                  for(int32_t field_idx = 0; field_idx < save_info->num_output_fields; ++field_idx) {
                    // To save the galaxies, we must first extend the size of the dataset to accomodate the new data.
                    herr_t status;
                    hsize_t dims_extend[1];
                    dims_extend[0] = (hsize_t) save_info->buffer_size;

                    hsize_t old_dims[1];
                    old_dims[0] = (hsize_t) save_info->gals_written_snap[n];
                    hsize_t new_dims[1];
                    new_dims[0] = old_dims[0] + (hsize_t) save_info->buffer_size;
                    hid_t dataset_id = save_info->dataset_ids[n*save_info->num_output_fields + field_idx];

                    status = H5Dset_extent(dataset_id, new_dims);

                    hid_t filespace = H5Dget_space(dataset_id);
                    status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL);

                    hid_t memspace = H5Screate_simple(1, dims_extend, NULL);

                    hsize_t dtype = save_info->field_dtypes[field_idx];
                    if(field_idx == 0) {
                      WRITE_GALAXY_DATASET(n, SnapNum, H5T_NATIVE_INT); 
                    } else if (field_idx == 1) {
                      WRITE_GALAXY_DATASET(n, Type, H5T_NATIVE_INT); 
                    } else if (field_idx == 2) {
                      WRITE_GALAXY_DATASET(n, GalaxyIndex, H5T_NATIVE_LLONG); 
                    } else if (field_idx == 3) {
                      WRITE_GALAXY_DATASET(n, CentralGalaxyIndex, H5T_NATIVE_LLONG); 
                    } else if (field_idx == 4) {
                      WRITE_GALAXY_DATASET(n, SAGEHaloIndex, H5T_NATIVE_INT);
                    } else if (field_idx == 5) {
                      WRITE_GALAXY_DATASET(n, SAGETreeIndex, H5T_NATIVE_INT);
                    } else if (field_idx == 6) {
                      WRITE_GALAXY_DATASET(n, SimulationHaloIndex, H5T_NATIVE_INT); 
                    } else if (field_idx == 7) {
                      WRITE_GALAXY_DATASET(n, mergeType, H5T_NATIVE_INT); 
                    } else if (field_idx == 8) {
                      WRITE_GALAXY_DATASET(n, mergeIntoID, H5T_NATIVE_INT); 
                    } else if (field_idx == 9) {
                      WRITE_GALAXY_DATASET(n, mergeIntoSnapNum, H5T_NATIVE_INT);
                    } else if (field_idx == 10) {
                      WRITE_GALAXY_DATASET(n, dT, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 11) {
                      WRITE_GALAXY_DATASET(n, Posx, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 12) {
                      WRITE_GALAXY_DATASET(n, Posy, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 13) {
                      WRITE_GALAXY_DATASET(n, Posz, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 14) {
                      WRITE_GALAXY_DATASET(n, Velx, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 15) {
                      WRITE_GALAXY_DATASET(n, Vely, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 16) {
                      WRITE_GALAXY_DATASET(n, Velz, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 17) {
                      WRITE_GALAXY_DATASET(n, Spinx, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 18) {
                      WRITE_GALAXY_DATASET(n, Spiny, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 19) {
                      WRITE_GALAXY_DATASET(n, Spinz, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 20) {
                      WRITE_GALAXY_DATASET(n, Len, H5T_NATIVE_INT); 
                    } else if (field_idx == 21) {
                      WRITE_GALAXY_DATASET(n, Mvir, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 22) {
                      WRITE_GALAXY_DATASET(n, CentralMvir, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 23) {
                      WRITE_GALAXY_DATASET(n, Rvir, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 24) {
                      WRITE_GALAXY_DATASET(n, Vvir, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 25) {
                      WRITE_GALAXY_DATASET(n, Vmax, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 26) {
                      WRITE_GALAXY_DATASET(n, VelDisp, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 27) {
                      WRITE_GALAXY_DATASET(n, ColdGas, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 28) {
                      WRITE_GALAXY_DATASET(n, StellarMass, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 29) {
                      WRITE_GALAXY_DATASET(n, BulgeMass, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 30) {
                      WRITE_GALAXY_DATASET(n, HotGas, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 31) {
                      WRITE_GALAXY_DATASET(n, EjectedMass, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 32) {
                      WRITE_GALAXY_DATASET(n, BlackHoleMass, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 33) {
                      WRITE_GALAXY_DATASET(n, ICS, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 34) {
                      WRITE_GALAXY_DATASET(n, MetalsColdGas, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 35) {
                      WRITE_GALAXY_DATASET(n, MetalsStellarMass, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 36) {
                      WRITE_GALAXY_DATASET(n, MetalsBulgeMass, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 37) {
                      WRITE_GALAXY_DATASET(n, MetalsHotGas, H5T_NATIVE_FLOAT);
                    } else if (field_idx == 38) {
                      WRITE_GALAXY_DATASET(n, MetalsEjectedMass, H5T_NATIVE_FLOAT);
                    } else if (field_idx == 39) {
                      WRITE_GALAXY_DATASET(n, MetalsICS, H5T_NATIVE_FLOAT);
                    } else if (field_idx == 40) {
                      WRITE_GALAXY_DATASET(n, SfrDisk, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 41) {
                      WRITE_GALAXY_DATASET(n, SfrBulge, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 42) {
                      WRITE_GALAXY_DATASET(n, SfrDiskZ, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 43) {
                      WRITE_GALAXY_DATASET(n, SfrBulgeZ, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 44) {
                      WRITE_GALAXY_DATASET(n, DiskScaleRadius, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 45) {
                      WRITE_GALAXY_DATASET(n, Cooling, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 46) {
                      WRITE_GALAXY_DATASET(n, Heating, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 47) {
                      WRITE_GALAXY_DATASET(n, QuasarModeBHaccretionMass, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 48) {
                      WRITE_GALAXY_DATASET(n, TimeOfLastMajorMerger, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 49) {
                      WRITE_GALAXY_DATASET(n, TimeOfLastMajorMerger, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 50) {
                      WRITE_GALAXY_DATASET(n, OutflowRate, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 51) {
                      WRITE_GALAXY_DATASET(n, infallMvir, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 52) {
                      WRITE_GALAXY_DATASET(n, infallVvir, H5T_NATIVE_FLOAT); 
                    } else if (field_idx == 53) {
                      WRITE_GALAXY_DATASET(n, infallVmax, H5T_NATIVE_FLOAT); 
                    }
                  }

                  save_info->num_gals_in_buffer[n] = 0;
                  save_info->gals_written_snap[n] = save_info->gals_written_snap[n] + save_info->buffer_size; 
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

void prepare_galaxy_for_hdf5_output(int ThisTask, int tree, struct GALAXY *g, struct save_info *save_info,
                                    int32_t output_snap_idx, 
                                    struct halo_data *halos,
                                    struct halo_aux_data *haloaux, struct GALAXY *halogal,
                                    const struct params *run_params)
{

    int64_t gals_in_buffer = save_info->num_gals_in_buffer[output_snap_idx];

    save_info->buffer_output_gals[output_snap_idx].SnapNum[gals_in_buffer] = g->SnapNum;

    if(g->Type < SHRT_MIN || g->Type > SHRT_MAX) {
        fprintf(stderr,"Error: Galaxy type = %d can not be represented in 2 bytes\n", g->Type);
        fprintf(stderr,"Converting galaxy type while saving from integer to short will result in data corruption");
        ABORT(EXIT_FAILURE);
    }
    save_info->buffer_output_gals[output_snap_idx].Type[gals_in_buffer] = g->Type;

    // assume that because there are so many files, the trees per file will be less than 100000
    // required for limits of long long
    if(run_params->LastFile>=10000) {
        save_info->buffer_output_gals[output_snap_idx].GalaxyIndex[gals_in_buffer] = g->GalaxyNr + TREE_MUL_FAC * tree + (THISTASK_MUL_FAC/10) * ThisTask;
        save_info->buffer_output_gals[output_snap_idx].CentralGalaxyIndex[gals_in_buffer] = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * tree + (THISTASK_MUL_FAC/10) * ThisTask;
        } else {
        save_info->buffer_output_gals[output_snap_idx].GalaxyIndex[gals_in_buffer] = g->GalaxyNr + TREE_MUL_FAC * tree + THISTASK_MUL_FAC * ThisTask;
        save_info->buffer_output_gals[output_snap_idx].CentralGalaxyIndex[gals_in_buffer] = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * tree + THISTASK_MUL_FAC * ThisTask;
    }

#undef TREE_MUL_FAC
#undef THISTASK_MUL_FAC

    save_info->buffer_output_gals[output_snap_idx].SAGEHaloIndex[gals_in_buffer] = g->HaloNr; 
    save_info->buffer_output_gals[output_snap_idx].SAGETreeIndex[gals_in_buffer] = tree; 
    save_info->buffer_output_gals[output_snap_idx].SimulationHaloIndex[gals_in_buffer] = llabs(halos[g->HaloNr].MostBoundID);
#if 0
    o->isFlyby = halos[g->HaloNr].MostBoundID < 0 ? 1:0;
#endif  

    save_info->buffer_output_gals[output_snap_idx].mergeType[gals_in_buffer] = g->mergeType; 
    save_info->buffer_output_gals[output_snap_idx].mergeIntoID[gals_in_buffer] = g->mergeIntoID; 
    save_info->buffer_output_gals[output_snap_idx].mergeIntoSnapNum[gals_in_buffer] = g->mergeIntoSnapNum; 
    save_info->buffer_output_gals[output_snap_idx].dT[gals_in_buffer] = g->dT * run_params->UnitTime_in_s / SEC_PER_MEGAYEAR;

    save_info->buffer_output_gals[output_snap_idx].Posx[gals_in_buffer] = g->Pos[0];
    save_info->buffer_output_gals[output_snap_idx].Posy[gals_in_buffer] = g->Pos[1];
    save_info->buffer_output_gals[output_snap_idx].Posz[gals_in_buffer] = g->Pos[2];

    save_info->buffer_output_gals[output_snap_idx].Velx[gals_in_buffer] = g->Vel[0];
    save_info->buffer_output_gals[output_snap_idx].Vely[gals_in_buffer] = g->Vel[1];
    save_info->buffer_output_gals[output_snap_idx].Velz[gals_in_buffer] = g->Vel[2];

    save_info->buffer_output_gals[output_snap_idx].Spinx[gals_in_buffer] = halos[g->HaloNr].Spin[0]; 
    save_info->buffer_output_gals[output_snap_idx].Spiny[gals_in_buffer] = halos[g->HaloNr].Spin[1]; 
    save_info->buffer_output_gals[output_snap_idx].Spinz[gals_in_buffer] = halos[g->HaloNr].Spin[2];

    save_info->buffer_output_gals[output_snap_idx].Len[gals_in_buffer] = g->Len;
    save_info->buffer_output_gals[output_snap_idx].Mvir[gals_in_buffer] = g->Mvir;
    save_info->buffer_output_gals[output_snap_idx].CentralMvir[gals_in_buffer] = get_virial_mass(halos[g->HaloNr].FirstHaloInFOFgroup, halos, run_params);
    save_info->buffer_output_gals[output_snap_idx].Rvir[gals_in_buffer] = get_virial_radius(g->HaloNr, halos, run_params);  // output the actual Rvir, not the maximum Rvir
    save_info->buffer_output_gals[output_snap_idx].Vvir[gals_in_buffer] = get_virial_velocity(g->HaloNr, halos, run_params);  // output the actual Vvir, not the maximum Vvir
    save_info->buffer_output_gals[output_snap_idx].Vmax[gals_in_buffer] = g->Vmax;
    save_info->buffer_output_gals[output_snap_idx].VelDisp[gals_in_buffer] = halos[g->HaloNr].VelDisp; 

    save_info->buffer_output_gals[output_snap_idx].ColdGas[gals_in_buffer] = g->ColdGas;
    save_info->buffer_output_gals[output_snap_idx].StellarMass[gals_in_buffer] = g->StellarMass;
    save_info->buffer_output_gals[output_snap_idx].BulgeMass[gals_in_buffer] = g->BulgeMass;
    save_info->buffer_output_gals[output_snap_idx].HotGas[gals_in_buffer] = g->HotGas;
    save_info->buffer_output_gals[output_snap_idx].EjectedMass[gals_in_buffer] = g->EjectedMass;
    save_info->buffer_output_gals[output_snap_idx].BlackHoleMass[gals_in_buffer] = g->BlackHoleMass;
    save_info->buffer_output_gals[output_snap_idx].ICS[gals_in_buffer] = g->ICS;

    save_info->buffer_output_gals[output_snap_idx].MetalsColdGas[gals_in_buffer] = g->MetalsColdGas;
    save_info->buffer_output_gals[output_snap_idx].MetalsStellarMass[gals_in_buffer] = g->MetalsStellarMass;
    save_info->buffer_output_gals[output_snap_idx].MetalsBulgeMass[gals_in_buffer] = g->MetalsBulgeMass;
    save_info->buffer_output_gals[output_snap_idx].MetalsHotGas[gals_in_buffer] = g->MetalsHotGas;
    save_info->buffer_output_gals[output_snap_idx].MetalsEjectedMass[gals_in_buffer] = g->MetalsEjectedMass;
    save_info->buffer_output_gals[output_snap_idx].ICS[gals_in_buffer] = g->ICS;

    float tmp_SfrDisk = 0.0;
    float tmp_SfrBulge = 0.0;
    float tmp_SfrDiskZ = 0.0;
    float tmp_SfrBulgeZ = 0.0;

    // NOTE: in Msun/yr
    for(int step = 0; step < STEPS; step++) {
        tmp_SfrDisk += g->SfrDisk[step] * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
        tmp_SfrBulge += g->SfrBulge[step] * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
        
        if(g->SfrDiskColdGas[step] > 0.0) {
            tmp_SfrDiskZ += g->SfrDiskColdGasMetals[step] / g->SfrDiskColdGas[step] / STEPS;
        }
        
        if(g->SfrBulgeColdGas[step] > 0.0) {
            tmp_SfrBulgeZ += g->SfrBulgeColdGasMetals[step] / g->SfrBulgeColdGas[step] / STEPS;
        }
    }

    save_info->buffer_output_gals[output_snap_idx].SfrDisk[gals_in_buffer] = tmp_SfrDisk;
    save_info->buffer_output_gals[output_snap_idx].SfrBulge[gals_in_buffer] = tmp_SfrBulge;
    save_info->buffer_output_gals[output_snap_idx].SfrDiskZ[gals_in_buffer] = tmp_SfrDiskZ;
    save_info->buffer_output_gals[output_snap_idx].SfrBulgeZ[gals_in_buffer] = tmp_SfrBulgeZ;

    save_info->buffer_output_gals[output_snap_idx].DiskScaleRadius[gals_in_buffer] = g->DiskScaleRadius;

    if (g->Cooling > 0.0) {
        save_info->buffer_output_gals[output_snap_idx].Cooling[gals_in_buffer] = log10(g->Cooling * run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s);
    } else {
        save_info->buffer_output_gals[output_snap_idx].Cooling[gals_in_buffer] = 0.0;
    }

    if (g->Heating > 0.0) {
        save_info->buffer_output_gals[output_snap_idx].Heating[gals_in_buffer] = log10(g->Heating * run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s);
    } else {
        save_info->buffer_output_gals[output_snap_idx].Heating[gals_in_buffer] = 0.0; 
    }

    save_info->buffer_output_gals[output_snap_idx].QuasarModeBHaccretionMass[gals_in_buffer] = g->QuasarModeBHaccretionMass;

    save_info->buffer_output_gals[output_snap_idx].TimeOfLastMajorMerger[gals_in_buffer] = g->TimeOfLastMajorMerger * run_params->UnitTime_in_Megayears;
    save_info->buffer_output_gals[output_snap_idx].TimeOfLastMinorMerger[gals_in_buffer] = g->TimeOfLastMinorMerger * run_params->UnitTime_in_Megayears;

    save_info->buffer_output_gals[output_snap_idx].OutflowRate[gals_in_buffer] = g->OutflowRate * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS;

    //infall properties
    if(g->Type != 0) {
        save_info->buffer_output_gals[output_snap_idx].infallMvir[gals_in_buffer] = g->infallMvir;
        save_info->buffer_output_gals[output_snap_idx].infallVvir[gals_in_buffer] = g->infallVvir;
        save_info->buffer_output_gals[output_snap_idx].infallVmax[gals_in_buffer] = g->infallVmax;
    } else {
        save_info->buffer_output_gals[output_snap_idx].infallMvir[gals_in_buffer] = 0.0;
        save_info->buffer_output_gals[output_snap_idx].infallVvir[gals_in_buffer] = 0.0;
        save_info->buffer_output_gals[output_snap_idx].infallVmax[gals_in_buffer] = 0.0;
    }

}


int finalize_galaxy_file(const int ntrees, const int *totgalaxies, const int **treengals, struct save_info *save_info, const struct params *run_params) 
{


    switch(run_params->OutputFormat) {

    case(binary):
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
        break;

    case(hdf5):
        // Write the remaining galaxies still in the buffer.
        // Close/free all the datasets.
        // Close the file.
        ABORT(100);
        break;

    default:
        fprintf(stderr, "Uknown OutputFormat.\n");
        ABORT(10341);
    }

    return EXIT_SUCCESS;
}

