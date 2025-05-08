#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

#include "save_gals_binary.h"
#include "../core/core_mymalloc.h"
#include "../core/core_utils.h"
#include "../core/core_logging.h"
#include "../physics/legacy/model_misc.h"

// Externally Visible Functions //

int32_t prepare_galaxy_for_output(const struct GALAXY *g, void *o,
                                 const int32_t original_treenr, const struct params *run_params)
{
    LOG_ERROR("Binary output format is no longer supported. Please use HDF5 output format.");
    return EXIT_FAILURE;
}

int32_t initialize_binary_galaxy_files(const int filenr, const struct forest_info *forest_info, 
                                      struct save_info *save_info, const struct params *run_params)
{
    LOG_ERROR("Binary output format is no longer supported. Please use HDF5 output format.");
    return EXIT_FAILURE;
}

int32_t save_binary_galaxies(const int32_t task_treenr, const int32_t num_gals, 
                            const int32_t *OutputGalCount, struct forest_info *forest_info, 
                            struct halo_data *halos, struct halo_aux_data *haloaux, 
                            struct GALAXY *halogal, struct save_info *save_info, 
                            const struct params *run_params)
{
    LOG_ERROR("Binary output format is no longer supported. Please use HDF5 output format.");
    return EXIT_FAILURE;
}

int32_t finalize_binary_galaxy_files(const struct forest_info *forest_info, 
                                    struct save_info *save_info, 
                                    const struct params *run_params)
{
    LOG_ERROR("Binary output format is no longer supported. Please use HDF5 output format.");
    return EXIT_FAILURE;
}
