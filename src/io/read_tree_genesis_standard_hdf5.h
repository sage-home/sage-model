#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

/* for definition of struct halo_data */
#include "../core_allvars.h"

/* Proto-Types */
    extern int setup_forests_io_genesis_hdf5(struct forest_info *forests_info, const int ThisTask, const int NTasks, struct params *run_params);
    extern int64_t load_forest_genesis_hdf5(int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info, struct params *run_params);
    extern void close_genesis_hdf5_file(struct forest_info *forests_info);

#define CONVERSION_FACTOR_FOR_GENESIS_UNIQUE_INDEX      (1000000000000)
#define CONVERT_HALOID_TO_SNAPSHOT(haloid)    (haloid / CONVERSION_FACTOR_FOR_GENESIS_UNIQUE_INDEX )
#define CONVERT_HALOID_TO_INDEX(haloid)       ((haloid %  CONVERSION_FACTOR_FOR_GENESIS_UNIQUE_INDEX) - 1)
#define CONVERT_SNAPSHOT_AND_INDEX_TO_HALOID(snap, index)    (snap*CONVERSION_FACTOR_FOR_GENESIS_UNIQUE_INDEX + index + 1)


#ifdef __cplusplus
}
#endif /* working with c++ compiler */
