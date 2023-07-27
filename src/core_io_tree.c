#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "core_allvars.h"
#include "core_mymalloc.h"
#include "core_io_tree.h"

#include "io/read_tree_lhalo_binary.h"
#include "io/read_tree_consistentrees_ascii.h"

#ifdef HDF5
#include "io/read_tree_lhalo_hdf5.h"
#include "io/read_tree_genesis_hdf5.h"
#include "io/read_tree_consistentrees_hdf5.h"
#include "io/read_tree_gadget4_hdf5.h"
#endif

int setup_forests_io(struct params *run_params, struct forest_info *forests_info,
                     const int ThisTask, const int NTasks)
{
    int status = EXIT_FAILURE;/* initialize to FAIL  */
    forests_info->firstfile = run_params->FirstFile;
    forests_info->lastfile = run_params->LastFile;
    const enum Valid_TreeTypes TreeType = run_params->TreeType;

    /* MS: 21/9/2019 initialise the mulfac's so we can check later
              that these vital factors (required to generate unique galaxy ID's)
              have been setup appropriately  */
    run_params->FileNr_Mulfac = -1;
    run_params->ForestNr_Mulfac = -1;
    forests_info->frac_volume_processed = -1.0;

    switch (TreeType)
        {
#ifdef HDF5
        case lhalo_hdf5:
            //MS: 22/07/2021 - Why is firstfile, lastfile still passed even though those could be constructef
            //from run_params (like done within this __FUNCTION__)
            status = setup_forests_io_lht_hdf5(forests_info, ThisTask, NTasks, run_params);
            break;

        case gadget4_hdf5:
            status = setup_forests_io_gadget4_hdf5(forests_info, ThisTask, NTasks, run_params);
            break;

        case genesis_hdf5:
            status = setup_forests_io_genesis_hdf5(forests_info, ThisTask, NTasks, run_params);
            break;

        case consistent_trees_hdf5:
            status = setup_forests_io_ctrees_hdf5(forests_info, ThisTask, NTasks, run_params);
            break;
#endif

        case lhalo_binary:
            status = setup_forests_io_lht_binary(forests_info, ThisTask, NTasks, run_params);
            break;

        case consistent_trees_ascii:
            status = setup_forests_io_ctrees(forests_info, ThisTask, NTasks, run_params);
            break;

        default:
            fprintf(stderr, "Your tree type has not been included in the switch statement for function ``%s`` in file ``%s``.\n", __FUNCTION__, __FILE__);
            fprintf(stderr, "Please add it there.\n");
            return INVALID_OPTION_IN_PARAMS;
        }

    if(status != EXIT_SUCCESS) {
        return status;
    }

    /*MS: Check that the mechanism to generate unique GalaxyID's was
      initialised correctly in the setup */
    if(run_params->FileNr_Mulfac < 0 || run_params->ForestNr_Mulfac < 0) {
        fprintf(stderr,"Error: Looks like the multiplicative factors to generate unique "
                       "galaxyID's were not setup correctly.\n"
                       "FileNr_Mulfac = %"PRId64" and ForestNr_Mulfac = %"PRId64" should both be >=0\n",
                       run_params->FileNr_Mulfac, run_params->ForestNr_Mulfac);
        return -1;
    }

    if(forests_info->frac_volume_processed <= 0.0 || forests_info->frac_volume_processed > 1.0) {
        fprintf(stderr,"Error: The fraction of the entire simulation volume processed should be in [0.0, 1.0]. Instead, found %g\n",
                forests_info->frac_volume_processed);
        return -1;
    }


    return status;
}


/* This routine is to be called after *ALL* forests have been processed */
void cleanup_forests_io(enum Valid_TreeTypes TreeType, struct forest_info *forests_info)
{
    /* Don't forget to free the open file handle */
    switch (TreeType) {
#ifdef HDF5
    case lhalo_hdf5:
        cleanup_forests_io_lht_hdf5(forests_info);
        break;

    case gadget4_hdf5:
        cleanup_forests_io_gadget4_hdf5(forests_info);
        break;

    case genesis_hdf5:
        cleanup_forests_io_genesis_hdf5(forests_info);
        break;

    case consistent_trees_hdf5:
        cleanup_forests_io_ctrees_hdf5(forests_info);
        break;

#endif

    case lhalo_binary:
        cleanup_forests_io_lht_binary(forests_info);
        break;

    case consistent_trees_ascii:

        /* because consistent trees can only be cleaned up after *ALL* forests
           have been processed (and not on a `per file` basis)
         */
        cleanup_forests_io_ctrees(forests_info);
        break;

    default:
        fprintf(stderr, "Your tree type has not been included in the switch statement for function ``%s`` in file ``%s``.\n", __FUNCTION__, __FILE__);
        fprintf(stderr, "Please add it there.\n");
        ABORT(EXIT_FAILURE);

    }

    // Finally, things that are common across forest types.
    free(forests_info->FileNr);
    free(forests_info->original_treenr);

    return;
}

int64_t load_forest(struct params *run_params, const int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info)
{

    int64_t nhalos;
    const enum Valid_TreeTypes TreeType = run_params->TreeType;

    switch (TreeType) {

#ifdef HDF5
    case lhalo_hdf5:
        nhalos = load_forest_lht_hdf5(forestnr, halos, forests_info);
        break;

    case gadget4_hdf5:
        nhalos = load_forest_gadget4_hdf5(forestnr, halos, forests_info);
        break;

    case genesis_hdf5:
        nhalos = load_forest_genesis_hdf5(forestnr, halos, forests_info, run_params);
        break;

    case consistent_trees_hdf5:
        nhalos = load_forest_ctrees_hdf5(forestnr, halos, forests_info, run_params);
        break;

#endif

    case lhalo_binary:
        nhalos = load_forest_lht_binary(forestnr, halos, forests_info);
        break;

    case consistent_trees_ascii:
        nhalos = load_forest_ctrees(forestnr, halos, forests_info, run_params);
        break;

    default:
        fprintf(stderr, "Your tree type has not been included in the switch statement for ``%s`` in ``%s``.\n",
                __FUNCTION__, __FILE__);
        fprintf(stderr, "Please add it there.\n");
        return -EXIT_FAILURE;
    }

    return nhalos;
}
