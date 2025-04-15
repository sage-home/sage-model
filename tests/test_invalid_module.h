#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../src/core/core_module_system.h"
#include "../src/core/core_logging.h"

/* Define module structures for testing (simplified versions of what's in the module system) */
struct cooling_module {
    struct base_module base;
    double (*calculate_cooling)(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data);
    double (*get_cooling_rate)(int gal_idx, struct GALAXY *galaxies, void *module_data);
};

struct star_formation_module {
    struct base_module base;
    double (*form_stars)(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data);
};

#include "../src/core/core_module_validation.h"

/**
 * @file test_invalid_module.h
 * @brief Helper functions for creating valid and invalid modules for testing
 * 
 * This file provides a set of helper functions that create test modules
 * with specific validation issues for testing the module validation logic.
 */

/* Function prototypes for basic valid/invalid modules */
struct base_module* create_valid_module(void);
struct base_module* create_minimal_valid_module(void);

/* Test cases for module interface issues */
struct base_module* create_module_with_empty_name(void);
struct base_module* create_module_with_empty_version(void);
struct base_module* create_module_with_invalid_type(void);
struct base_module* create_module_with_missing_initialize(void);
struct base_module* create_module_with_missing_cleanup(void);

/* Test cases for module type-specific issues */
struct cooling_module* create_cooling_module_missing_calculate_cooling(void);
struct star_formation_module* create_star_formation_module_missing_form_stars(void);

/* Function prototypes for manifest issues */
struct module_manifest* create_valid_manifest(void);
struct module_manifest* create_manifest_with_empty_name(void);
struct module_manifest* create_manifest_with_empty_version(void);
struct module_manifest* create_manifest_with_invalid_type(void);
struct module_manifest* create_manifest_with_empty_library_path(void);
struct module_manifest* create_manifest_with_invalid_api_version(void);

/* Function prototypes for dependency issues */
struct module_dependency* create_valid_dependency(void);
struct module_dependency* create_dependency_with_no_name_or_type(void);
struct module_dependency* create_dependency_with_invalid_version_constraints(void);

/* Helper functions for module lifecycle */
int dummy_initialize(struct params *params, void **module_data);
int dummy_cleanup(void *module_data);
double dummy_calculate_cooling(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data);
double dummy_get_cooling_rate(int gal_idx, struct GALAXY *galaxies, void *module_data);
double dummy_form_stars(int gal_idx, double dt, struct GALAXY *galaxies, void *module_data);

/* Storage for created modules (to avoid memory leaks) */
extern struct base_module* test_modules[16];
extern int test_module_count;

/* Initialize and cleanup test helper system */
void test_invalid_module_init(void);
void test_invalid_module_cleanup(void);

/* Helper to verify validation errors for a given module */
bool verify_validation_error(struct base_module* module, 
                            const char* expected_error_substr,
                            const char* component);
