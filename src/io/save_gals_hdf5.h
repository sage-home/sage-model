#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif // working with c++ compiler //

#include "../core_allvars.h"

    struct HDF5_GALAXY_OUTPUT  
    {
      int   *SnapNum;

#if 0    
      short Type;
      short isFlyby;
#else
      int *Type;
#endif    

      long long   *GalaxyIndex;
      long long   *CentralGalaxyIndex;
      int   *SAGEHaloIndex;
      int   *SAGETreeIndex;
      long long   *SimulationHaloIndex;
      
      int   *mergeType;  /* 0=none; 1=minor merger; 2=major merger; 3=disk instability; 4=disrupt to ICS */
      int   *mergeIntoID;
      int   *mergeIntoSnapNum;
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
      int   *Len;   
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

    extern int32_t save_hdf5_galaxies(const int32_t treenr, const int32_t num_gals,
                                      struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                                      struct save_info *save_info, const struct params *run_params);

    extern int32_t finalize_hdf5_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info,
                                              const struct params *run_params);

    extern int32_t create_hdf5_master_file(const struct params *run_params);

#ifdef __cplusplus
}
#endif
