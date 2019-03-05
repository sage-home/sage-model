#pragma once

#ifdef __cplusplus
extern "C" {
#endif /* working with c++ compiler */

#include <hdf5.h>

/* for definition of struct halo_data and struct forest_info */    
#include "../core_allvars.h"    

    /* Proto-Types */
    extern void get_forests_filename_lht_hdf5(char *filename, const size_t len, const int filenr);
    extern int setup_forests_io_lht_hdf5(struct forest_info *forests_info, const int firstfile, const int lastfile,
                                         const int ThisTask, const int NTasks);
    extern int64_t load_forest_hdf5(const int32_t forestnr, struct halo_data **halos, struct forest_info *forests_info);
    extern void close_hdf5_file(struct forest_info *forests_info);
    
#ifdef __cplusplus
}
#endif /* working with c++ compiler */
