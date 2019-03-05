#pragma once

#include "core_allvars.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* API for sage */
    extern int init_sage(const int ThisTask, const char *param_file, struct params *run_params);
    extern int run_sage(const int ThisTask, const int NTasks, struct params *run_params);

#ifdef __cplusplus
}
#endif



    
