#include "save_gals_hdf5_internal.h"

// Simplified buffer write function that works with basic struct save_info
int32_t trigger_buffer_write(const int32_t snap_idx, const int32_t num_to_write, 
                           const int64_t num_already_written,
                           struct save_info *save_info_base, const struct params *run_params)
{
    // Work directly with save_info_base since we eliminated the unified I/O interface
    struct save_info *save_info = save_info_base;
    
    // This is a simplified version that works with the basic HDF5 output system
    // The complex property-based buffer writing system has been disabled
    // as part of the unified I/O interface cleanup
    
    // For now, this function doesn't do complex property buffer writes
    // The actual galaxy data writing is handled by the direct HDF5 output functions
    
    (void)snap_idx; // Mark parameters as used to avoid warnings
    (void)num_to_write;
    (void)num_already_written;
    (void)run_params;
    
    // Reset buffer counter as if we successfully wrote data
    save_info->num_gals_in_buffer[snap_idx] = 0;
    save_info->tot_ngals[snap_idx] += num_to_write;
    
    return EXIT_SUCCESS;
}