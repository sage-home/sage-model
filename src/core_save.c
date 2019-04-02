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

int32_t generate_galaxy_index(const int32_t tree_filenr, const int32_t treenr, const int32_t GalaxyNr,
                              const int32_t CentralGalaxyNr, const int32_t LastFile, int64_t *GalaxyIndex,
                              int64_t *CentralGalaxyIndex);

// Externally Visible Functions //

// Open up all the required output files and remember their file handles.  These are placed into
// `save_info` for access later.
int32_t initialize_galaxy_files(const int rank, const struct forest_info *forest_info, struct save_info *save_info, const struct params *run_params)
{
    int32_t status;

    if(run_params->NOUT > ABSOLUTEMAXSNAPS) {
        fprintf(stderr,"Error: Attempting to write snapshot = '%d' will exceed allocated memory space for '%d' snapshots\n",
                run_params->NOUT, ABSOLUTEMAXSNAPS);
        fprintf(stderr,"To fix this error, simply increase the value of `ABSOLUTEMAXSNAPS` and recompile\n");
        return INVALID_OPTION_IN_PARAMS;
    }

    switch(run_params->OutputFormat) {

    case(sage_binary):
      status = initialize_binary_galaxy_files(rank, forest_info, save_info, run_params);
      break;

#ifdef HDF5
    case(sage_hdf5):
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


// Write all the galaxy properties to file.
int32_t save_galaxies(const int treenr, const int numgals, struct halo_data *halos,
                      struct halo_aux_data *haloaux, struct GALAXY *halogal,
                      struct save_info *save_info, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;

    // Reset the output galaxy count.
    int32_t OutputGalCount[run_params->MAXSNAPS];
    for(int32_t snap_idx = 0; snap_idx < run_params->MAXSNAPS; snap_idx++) {
        OutputGalCount[snap_idx] = 0;
    }

    // Track the order in which galaxies are written.
    int32_t *OutputGalOrder = calloc(numgals, sizeof(*(OutputGalOrder)));
    if(OutputGalOrder == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for %d int elements in array `OutputGalOrder`\n", numgals);
        return MALLOC_FAILURE;
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


    // Generate a unique GalaxyIndex for each galaxy.
    for(int32_t gal_idx = 0; gal_idx < numgals; gal_idx++) {
        int32_t CentralGalaxyNr = haloaux[halos[halogal[gal_idx].HaloNr].FirstHaloInFOFgroup].FirstGalaxy;
        status = generate_galaxy_index(halos[halogal[gal_idx].HaloNr].FileNr, treenr,
                                       halogal[gal_idx].GalaxyNr, CentralGalaxyNr, run_params->LastFile,
                                       &halogal[gal_idx].GalaxyIndex, &halogal[gal_idx].CentralGalaxyIndex);
        if(status != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
    }

    // All of the tracking arrays set up, time to perform the actual writing.
    switch(run_params->OutputFormat) {

    case(sage_binary):
        status = save_binary_galaxies(treenr, numgals, OutputGalCount, halos, haloaux,
                                      halogal, save_info, run_params);
        break;

#ifdef HDF5
    case(sage_hdf5):
        status = save_hdf5_galaxies(treenr, numgals, halos, haloaux, halogal, save_info, run_params);
        break;
#endif

    default:
        fprintf(stderr, "Uknown OutputFormat in `save_galaxies()`.\n");
        status = INVALID_OPTION_IN_PARAMS;

    }

    free(OutputGalOrder);

    return status;
}


// Write any remaining attributes or header information, close all the open files and free all the
// relevant dataspaces.
int32_t finalize_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info, const struct params *run_params) 
{

    int32_t status = EXIT_FAILURE;

    switch(run_params->OutputFormat) {

    case(sage_binary):
        status = finalize_binary_galaxy_files(forest_info, save_info, run_params);
        break;

#ifdef HDF5
    case(sage_hdf5):
        status = finalize_hdf5_galaxy_files(forest_info, save_info, run_params);
        break;
#endif

    default:
        fprintf(stderr, "Error: Unknown OutputFormat in `finalize_galaxy_files()`.\n");
        status = INVALID_OPTION_IN_PARAMS;
        break;
    }

    return status;
}

// Local Functions //

#define TREE_MUL_FAC        (1000000000LL)
#define THISTASK_MUL_FAC      (1000000000000000LL)

int32_t generate_galaxy_index(const int32_t tree_filenr, const int32_t treenr, const int32_t GalaxyNr,
                              const int32_t CentralGalaxyNr, const int32_t LastFile,
                              int64_t *GalaxyIndex, int64_t *CentralGalaxyIndex){

    // Assume that there are so many files, that there aren't as many trees.
    // Required for 64 bit limit.
    if(LastFile>=10000) {
        if(GalaxyNr > TREE_MUL_FAC || treenr > (THISTASK_MUL_FAC/10)/TREE_MUL_FAC) {
            fprintf(stderr, "We assume there is a maximum of 2^64 - 1 trees.  This assumption has been broken.\n"
                            "File number %d\ttree number %d\tGalaxy Number %d\n", tree_filenr, treenr,
                            GalaxyNr);
            return EXIT_FAILURE;
        }

        *GalaxyIndex = GalaxyNr + TREE_MUL_FAC * treenr + (THISTASK_MUL_FAC/10) * tree_filenr;
        *CentralGalaxyIndex = CentralGalaxyNr + TREE_MUL_FAC * treenr + (THISTASK_MUL_FAC/10) * tree_filenr;
        } else {
        if(GalaxyNr > TREE_MUL_FAC || treenr > THISTASK_MUL_FAC/TREE_MUL_FAC) {
            fprintf(stderr, "We assume there is a maximum of 2^64 - 1 trees.  This assumption has been broken.\n"
                            "File number %d\ttree number %d\tGalaxy Number %d\n", tree_filenr, treenr,
                            GalaxyNr);
            return EXIT_FAILURE;
        }

        *GalaxyIndex = GalaxyNr + TREE_MUL_FAC * treenr + THISTASK_MUL_FAC * tree_filenr;
        *CentralGalaxyIndex = CentralGalaxyNr + TREE_MUL_FAC * treenr + THISTASK_MUL_FAC * tree_filenr;        
    }

    return EXIT_SUCCESS;
}

#undef TREE_MUL_FAC
#undef THISTASK_MUL_FAC
