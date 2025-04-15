/**
 * @file test_module_template_validation.h
 * @brief Validation utilities for module template generation tests
 * @author SAGE Development Team
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <limits.h>

/* Define PATH_MAX if not defined */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "../src/core/core_module_template.h"

/* Constants for test directories */
#define TEST_BASE_DIR "./test_module_output"
#define TEST_MIN_DIR "./test_module_output/minimal"
#define TEST_FULL_DIR "./test_module_output/full"
#define TEST_MIXED_DIR "./test_module_output/mixed"

/* Constants for test modules */
#define TEST_MODULE_COOLING "test_cooling_module"
#define TEST_MODULE_COOLING_PREFIX "tcm"
#define TEST_MODULE_STAR_FORMATION "test_sf_module"
#define TEST_MODULE_STAR_FORMATION_PREFIX "tsf"
#define TEST_MODULE_FEEDBACK "test_feedback_module"
#define TEST_MODULE_FEEDBACK_PREFIX "tfb"

/**
 * Read a file into a buffer
 * 
 * @param filepath Path to the file
 * @param buffer Buffer to store the file contents (will be allocated)
 * @param size Pointer to store the file size
 * @return true if file was read successfully, false otherwise
 */
bool read_file_to_buffer(const char *filepath, char **buffer, size_t *size);

/**
 * Check if all patterns are present in content
 * 
 * @param content Content to check
 * @param patterns Array of patterns to look for
 * @param num_patterns Number of patterns
 * @param missing_pattern Output parameter to store the first missing pattern
 * @return true if all patterns are present, false otherwise
 */
bool check_patterns(const char *content, const char **patterns, int num_patterns, const char **missing_pattern);

/**
 * Validate a module header file
 * 
 * @param filepath Path to the header file
 * @param params Template parameters
 * @return true if file content is valid, false otherwise
 */
bool validate_module_header(const char *filepath, const struct module_template_params *params);

/**
 * Validate a module implementation file
 * 
 * @param filepath Path to the implementation file
 * @param params Template parameters
 * @return true if file content is valid, false otherwise
 */
bool validate_module_implementation(const char *filepath, const struct module_template_params *params);

/**
 * Validate a module manifest file
 * 
 * @param filepath Path to the manifest file
 * @param params Template parameters
 * @return true if file content is valid, false otherwise
 */
bool validate_module_manifest(const char *filepath, const struct module_template_params *params);

/**
 * Validate a module makefile
 * 
 * @param filepath Path to the makefile
 * @param params Template parameters
 * @return true if file content is valid, false otherwise
 */
bool validate_module_makefile(const char *filepath, const struct module_template_params *params);

/**
 * Validate a module test file
 * 
 * @param filepath Path to the test file
 * @param params Template parameters
 * @return true if file content is valid, false otherwise
 */
bool validate_module_test_file(const char *filepath, const struct module_template_params *params);

/**
 * Validate a module README file
 * 
 * @param filepath Path to the README file
 * @param params Template parameters
 * @return true if file content is valid, false otherwise
 */
bool validate_module_readme(const char *filepath, const struct module_template_params *params);

/**
 * Setup template parameters for minimal configuration
 * 
 * @param params Template parameters to initialize
 * @param output_dir Output directory
 * @param module_type Module type to use
 */
void setup_minimal_template_params(struct module_template_params *params, const char *output_dir, enum module_type module_type);

/**
 * Setup template parameters for full configuration
 * 
 * @param params Template parameters to initialize
 * @param output_dir Output directory
 * @param module_type Module type to use
 */
void setup_full_template_params(struct module_template_params *params, const char *output_dir, enum module_type module_type);

/**
 * Setup template parameters for mixed configuration
 * 
 * @param params Template parameters to initialize
 * @param output_dir Output directory
 * @param module_type Module type to use
 */
void setup_mixed_template_params(struct module_template_params *params, const char *output_dir, enum module_type module_type);

/**
 * Validate all generated module files
 * 
 * @param params Template parameters
 * @return true if all files are valid, false otherwise
 */
bool validate_all_module_files(const struct module_template_params *params);
