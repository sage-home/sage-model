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

// Externally Visible Functions //

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

#ifdef HDF5
    case(hdf5):
      status = initialize_hdf5_galaxy_files(rank, save_info, run_params);
      break; 
#endif

    default:
      fprintf(stderr, "Error: Unknown OutputFormat in `initialize_galaxy_files()`.\n");
      status = INVALID_OPTION_IN_PARAMS;
      break;

    }

    return status; 
}

#define EXTEND_AND_WRITE_GALAXY_DATASET(field_name, h5_dtype) { \
    hid_t dataset_id = save_info->dataset_ids[snap_idx*save_info->num_output_fields + field_idx]; \
    status = H5Dset_extent(dataset_id, new_dims); \
    hid_t filespace = H5Dget_space(dataset_id); \
    status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL); \
    hid_t memspace = H5Screate_simple(1, dims_extend, NULL); \
    status = H5Dwrite(dataset_id, h5_dtype, memspace, filespace, H5P_DEFAULT, (save_info->buffer_output_gals[snap_idx]).field_name); \
    field_idx++; \
}

void save_galaxies(const int ThisTask, const int tree, const int numgals, struct halo_data *halos,
                   struct halo_aux_data *haloaux, struct GALAXY *halogal,
                   struct save_info *save_info, const struct params *run_params)
{

    // Reset the output galaxy count.
    int32_t OutputGalCount[run_params->MAXSNAPS];
    for(int32_t snap_idx = 0; snap_idx < run_params->MAXSNAPS; snap_idx++) {
        OutputGalCount[snap_idx] = 0;
    }

    // Track the order in which galaxies are written.
    int32_t *OutputGalOrder = calloc(numgals, sizeof(*(OutputGalOrder)));
    if(OutputGalOrder == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `OutputGalOrder`\n", numgals);
        ABORT(MALLOC_FAILURE);
    }

    for(int32_t gal_idx = 0; gal_idx < numgals; gal_idx++) {
        OutputGalOrder[gal_idx] = -1;
        haloaux[gal_idx].output_snap_n = -1;
    }

    // First update mergeIntoID to point to the correct galaxy in the output.
    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {
        for(int32_t gal_idx  = 0; gal_idx < numgals; gal_idx++) {
            if(halogal[gal_idx].SnapNum == run_params->ListOutputSnaps[snap_idx]) {
                OutputGalOrder[gal_idx] = OutputGalCount[snap_idx];
                OutputGalCount[snap_idx]++;
                haloaux[gal_idx].output_snap_n = snap_idx;
            }
        }
    }

    for(int32_t gal_idx = 0; gal_idx < numgals; gal_idx++) {
        if(halogal[gal_idx].mergeIntoID > -1) {
            halogal[gal_idx].mergeIntoID = OutputGalOrder[halogal[gal_idx].mergeIntoID];
        }
    }

    switch(run_params->OutputFormat) {

    case(binary):
        save_binary_galaxies(ThisTask, tree, numgals, OutputGalCount, halos, haloaux,
                             halogal, save_info, run_params);
        break;

#ifdef HDF5
    case(hdf5):
        for(int32_t gal_idx = 0; gal_idx < numgals; gal_idx++) {
            if(haloaux[gal_idx].output_snap_n < 0) {
                continue;
            }

            int32_t snap_idx = haloaux[gal_idx].output_snap_n;
            prepare_galaxy_for_hdf5_output(ThisTask, tree, &halogal[gal_idx], save_info, snap_idx, halos, haloaux, halogal, run_params);
            save_info->num_gals_in_buffer[snap_idx]++;

            // We can't guarantee that this tree will contain enough galaxies to trigger a write.
            // Hence we need to increment this here.
            save_info->forest_ngals[snap_idx][tree]++;

            herr_t status;

            if(save_info->num_gals_in_buffer[snap_idx] == save_info->buffer_size) {

                // To save the galaxies, we must first extend the size of the dataset to accomodate the new data.

                // This is the length which we will extended the dataset by.
                hsize_t dims_extend[1];
                dims_extend[0] = (hsize_t) save_info->buffer_size;

                // The previous length of the dataset.
                hsize_t old_dims[1];
                old_dims[0] = (hsize_t) save_info->tot_ngals[snap_idx];

                // Then this is the new length.
                // JS 16/03/19: I've attempted to put these into the HDF5 function calls directly
                // (rather than specifying as arrays).  However, it causes errors...
                hsize_t new_dims[1];
                new_dims[0] = old_dims[0] + (hsize_t) save_info->buffer_size;

                // This parameter is incremented in every Macro call. It is used to ensure we are
                // accessing the correct dataset.
                int32_t field_idx = 0;

                EXTEND_AND_WRITE_GALAXY_DATASET(SnapNum, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Type, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(GalaxyIndex, H5T_NATIVE_LLONG);
                EXTEND_AND_WRITE_GALAXY_DATASET(CentralGalaxyIndex, H5T_NATIVE_LLONG);
                EXTEND_AND_WRITE_GALAXY_DATASET(SAGEHaloIndex, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(SAGETreeIndex, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(SimulationHaloIndex, H5T_NATIVE_LLONG);
                EXTEND_AND_WRITE_GALAXY_DATASET(mergeType, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(mergeIntoID, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(mergeIntoSnapNum, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(dT, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Posx, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Posy, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Posz, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Velx, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Vely, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Velz, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Spinx, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Spiny, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Spinz, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Len, H5T_NATIVE_INT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Mvir, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(CentralMvir, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Rvir, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Vvir, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Vmax, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(VelDisp, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(ColdGas, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(StellarMass, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(BulgeMass, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(HotGas, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(EjectedMass, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(BlackHoleMass, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(ICS, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(MetalsColdGas, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(MetalsStellarMass, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(MetalsBulgeMass, H5T_NATIVE_FLOAT); 
                EXTEND_AND_WRITE_GALAXY_DATASET(MetalsHotGas, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(MetalsEjectedMass, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(MetalsICS, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(SfrDisk, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(SfrBulge, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(SfrDiskZ, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(SfrBulgeZ, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(DiskScaleRadius, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Cooling, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(Heating, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(QuasarModeBHaccretionMass, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(TimeOfLastMajorMerger, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(TimeOfLastMinorMerger, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(OutflowRate, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(infallMvir, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(infallVvir, H5T_NATIVE_FLOAT);
                EXTEND_AND_WRITE_GALAXY_DATASET(infallVmax, H5T_NATIVE_FLOAT);

                // We've performed a write, so future galaxies will overwrite the old data.
                save_info->num_gals_in_buffer[snap_idx] = 0;
                save_info->tot_ngals[snap_idx] += save_info->buffer_size;
            }
        }
        break;
#endif
    default:
        fprintf(stderr, "Uknown OutputFormat when attempting to save.\n");
        ABORT(INVALID_OPTION_IN_PARAMS);

    }
    // don't forget to free the workspace.
    free(OutputGalOrder);

}

#ifdef HDF5
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
    save_info->buffer_output_gals[output_snap_idx].MetalsICS[gals_in_buffer] = g->MetalsICS;

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
#endif

int32_t finalize_galaxy_files(const int32_t ntrees, struct save_info *save_info, const struct params *run_params) 
{

    int32_t status = EXIT_FAILURE;

    switch(run_params->OutputFormat) {

    case(binary):
        status = finalize_binary_galaxy_files(ntrees, save_info, run_params);
        break;

#ifdef HDF5
    case(hdf5):
        status = finalize_hdf5_galaxy_files(ntrees, save_info, run_params);
        break;
#endif

    default:
        fprintf(stderr, "Error: Unknown OutputFormat in `finalize_galaxy_files()`.\n");
        status = INVALID_OPTION_IN_PARAMS;
        break;
    }

    return status;
}

