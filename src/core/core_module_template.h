#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include "core_module_system.h"

/**
 * @file core_module_template.h
 * @brief Module template generation system for SAGE
 * 
 * This file provides functionality for creating template files for new physics
 * modules. It automates the creation of boilerplate code and ensures module
 * implementations follow the required interface standards.
 */

/**
 * Module template parameters
 * 
 * Contains all parameters required to generate a module template
 */
struct module_template_params {
    char module_name[MAX_MODULE_NAME];         /* Name of the module */
    char module_prefix[MAX_MODULE_NAME];       /* Prefix for function names */
    enum module_type type;                     /* Type of physics module */
    char author[MAX_MODULE_AUTHOR];            /* Module author name */
    char email[MAX_MODULE_AUTHOR];             /* Author email (optional) */
    char description[MAX_MODULE_DESCRIPTION];  /* Module description */
    char version[MAX_MODULE_VERSION];          /* Module version */
    
    /* Template features to include */
    bool include_galaxy_extension;             /* Whether to include galaxy property extension */
    bool include_event_handler;                /* Whether to include event handler */
    bool include_callback_registration;        /* Whether to include callback registration */
    bool include_manifest;                     /* Whether to generate a manifest file */
    bool include_makefile;                     /* Whether to generate a Makefile */
    bool include_test_file;                    /* Whether to generate a test file */
    bool include_readme;                       /* Whether to generate a README file */
    
    /* Output paths */
    char output_dir[PATH_MAX];                 /* Directory for output files */
};

/**
 * Generate a module template
 * 
 * Creates template files for a new module based on the provided parameters.
 * 
 * @param params Template generation parameters
 * @return 0 on success, error code on failure
 */
int module_generate_template(const struct module_template_params *params);

/**
 * Generate a module header file
 * 
 * Creates the header file for a new module.
 * 
 * @param params Template generation parameters
 * @param output_path Output path for the header file
 * @return 0 on success, error code on failure
 */
int module_generate_header(const struct module_template_params *params, const char *output_path);

/**
 * Generate a module implementation file
 * 
 * Creates the implementation file for a new module.
 * 
 * @param params Template generation parameters
 * @param output_path Output path for the implementation file
 * @return 0 on success, error code on failure
 */
int module_generate_implementation(const struct module_template_params *params, const char *output_path);

/**
 * Generate a module manifest file
 * 
 * Creates the manifest file for a new module.
 * 
 * @param params Template generation parameters
 * @param output_path Output path for the manifest file
 * @return 0 on success, error code on failure
 */
int module_generate_manifest(const struct module_template_params *params, const char *output_path);

/**
 * Generate a module test file
 * 
 * Creates a test file for a new module.
 * 
 * @param params Template generation parameters
 * @param output_path Output path for the test file
 * @return 0 on success, error code on failure
 */
int module_generate_test(const struct module_template_params *params, const char *output_path);

/**
 * Generate a module README file
 * 
 * Creates a README file for a new module.
 * 
 * @param params Template generation parameters
 * @param output_path Output path for the README file
 * @return 0 on success, error code on failure
 */
int module_generate_readme(const struct module_template_params *params, const char *output_path);

/**
 * Generate a module Makefile
 * 
 * Creates a Makefile for a new module.
 * 
 * @param params Template generation parameters
 * @param output_path Output path for the Makefile
 * @return 0 on success, error code on failure
 */
int module_generate_makefile(const struct module_template_params *params, const char *output_path);

/**
 * Get the module interface name for a given module type
 * 
 * Returns the name of the interface structure for a module type.
 * 
 * @param type Module type
 * @return Name of the interface structure
 */
const char *module_get_interface_name(enum module_type type);

/**
 * Get the module function signatures for a given module type
 * 
 * Returns an array of function signature strings for a module type.
 * 
 * @param type Module type
 * @param signatures Output array of signature strings
 * @param max_signatures Maximum number of signatures to return
 * @return Number of signatures returned
 */
int module_get_function_signatures(enum module_type type, 
                                  char signatures[][256], 
                                  int max_signatures);

/**
 * Initialize a template parameters structure with default values
 * 
 * Sets reasonable defaults for a module template parameters structure.
 * 
 * @param params Template parameters to initialize
 * @return 0 on success, error code on failure
 */
int module_template_params_init(struct module_template_params *params);

#ifdef __cplusplus
}
#endif