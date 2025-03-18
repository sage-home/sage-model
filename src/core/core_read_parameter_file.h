#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    /* functions in core_read_parameter_file.c */
    extern int read_parameter_file(const char *fname, struct params *run_params);

#ifdef __cplusplus
}
#endif
