#pragma once

#include "config.h"

#ifdef CONFIG_JSON_SUPPORT

#ifdef __cplusplus
extern "C" {
#endif

// JSON configuration reading function
extern int config_read_json(config_t *config, const char *filename);

// JSON to params conversion helpers
extern int json_to_params(const void *json, struct params *params);
extern double get_json_double(const void *json_obj, const char *key, double default_val);
extern int get_json_int(const void *json_obj, const char *key, int default_val);
extern void get_json_string(const void *json_obj, const char *key, char *dest, size_t dest_size);

// File reading utility
extern char* read_file_to_string(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_JSON_SUPPORT */