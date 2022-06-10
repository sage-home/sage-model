#pragma once

#include "core_allvars.h"

// DO NOT TOUCH THESE TWO DEFINITIONS.
// They are checked when we processed the output. Bad things will happen if you do touch them!
#define SAGE_DATA_VERSION "1.00"
#define SAGE_VERSION "1.00"


#ifdef __cplusplus
extern "C" {
#endif

    /* API for sage */
    extern int run_sage(const int ThisTask, const int NTasks, const char *param_file, void **params);
    extern int finalize_sage(void *params);

    /* API for converting supported mergertre formats to the Millennium (LHaloTree) binary format */
    extern int convert_trees_to_lhalo(const int ThisTask, const int NTasks, const char *param_file, void **params);

#ifdef __cplusplus
}
#endif
