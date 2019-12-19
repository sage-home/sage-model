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
    extern int init_sage(const int ThisTask, const char *param_file, struct params *run_params);
    extern int run_sage(const int ThisTask, const int NTasks, struct params *run_params);
    extern int finalize_sage(struct params *run_params);

#ifdef __cplusplus
}
#endif
