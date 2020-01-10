#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // working with c++ compiler //

#include "../core_allvars.h"

struct HDF5_GALAXY_OUTPUT
{
    int32_t   *SnapNum;

#if 0
    short Type;
    short isFlyby;
#else
    int32_t *Type;
#endif
    
    long long   *GalaxyIndex;
    long long   *CentralGalaxyIndex;

    int32_t   *SAGEHaloIndex;
    int32_t   *SAGETreeIndex;
    
    long long   *SimulationHaloIndex;
    int64_t *TaskForestNr; /* MS: 17/9/2019 -- Tracks the cpu-local forest number */

    int32_t  *mergeType;  /* 0=none; 1=minor merger; 2=major merger; 3=disk instability; 4=disrupt to ICS */
    int32_t   *mergeIntoID;
    int32_t   *mergeIntoSnapNum;
    float *dT;

    /* (sub)halo properties */
    float *Posx;
    float *Posy;
    float *Posz;
    float *Velx;
    float *Vely;
    float *Velz;
    float *Spinx;
    float *Spiny;
    float *Spinz;
    int32_t   *Len;
    float *Mvir;
    float *CentralMvir;
    float *Rvir;
    float *Vvir;
    float *Vmax;
    float *VelDisp;
    
    /* baryonic reservoirs */
    float *ColdGas;
    float *StellarMass;
    float *BulgeMass;
    float *HotGas;
    float *EjectedMass;
    float *BlackHoleMass;
    float *ICS;
    
    /* metals */
    float *MetalsColdGas;
    float *MetalsStellarMass;
    float *MetalsBulgeMass;
    float *MetalsHotGas;
    float *MetalsEjectedMass;
    float *MetalsICS;
    
    /* to calculate magnitudes */
    float *SfrDisk;
    float *SfrBulge;
    float *SfrDiskZ;
    float *SfrBulgeZ;
    
    /* misc */
    float *DiskScaleRadius;
    float *Cooling;
    float *Heating;
    float *QuasarModeBHaccretionMass;
    float *TimeOfLastMajorMerger;
    float *TimeOfLastMinorMerger;
    float *OutflowRate;
    
    /* infall properties */
    float *infallMvir;
    float *infallVvir;
    float *infallVmax;
};
    
    // Proto-Types //
    extern int32_t initialize_hdf5_galaxy_files(const int filenr, struct save_info *save_info, const struct params *run_params);
    
    extern int32_t save_hdf5_galaxies(const int64_t task_forestnr, const int32_t num_gals, struct forest_info *forest_info,
                                      struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                                      struct save_info *save_info, const struct params *run_params);

    extern int32_t finalize_hdf5_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info,
                                              const struct params *run_params);

    extern int32_t create_hdf5_master_file(const struct params *run_params);
    
    
#ifdef __cplusplus
}
#endif
