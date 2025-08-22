#pragma once

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Legacy .par file reading functions
extern int config_read_legacy_par(config_t *config, const char *filename);

// Internal function for reading parameter files (extracted from existing code)
extern int read_parameter_file_internal(const char *fname, struct params *run_params);

// String to enum conversion functions for legacy compatibility
extern int string_to_tree_type(const char *tree_type_str);
extern int string_to_output_format(const char *output_format_str);
extern int string_to_forest_dist_scheme(const char *forest_dist_str);

#ifdef __cplusplus
}
#endif