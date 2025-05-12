/**
 * @file mock_module_template.h
 * @brief Mock replacement for core_module_template.h for testing only
 */
#pragma once

#include <stdbool.h>
#include <limits.h>
#include "../src/core/core_module_system.h"

/* Testing constants */
#define TEST_BASE_DIR "tests/output"
#define TEST_MIN_DIR "tests/output/min"
#define TEST_FULL_DIR "tests/output/full"
#define TEST_MIXED_DIR "tests/output/mixed"
#define TEST_MODULE_COOLING "test_cooling_module"
#define TEST_MODULE_COOLING_PREFIX "test_cooling"
#define TEST_MODULE_SF "test_star_formation_module"
#define TEST_MODULE_SF_PREFIX "test_sf"
#define TEST_MODULE_FEEDBACK "test_feedback_module"
#define TEST_MODULE_FEEDBACK_PREFIX "test_fb"

/* Template parameters */
struct module_template_params {
    char module_name[128];
    char module_prefix[64];
    char author[128];
    char email[128];
    char description[256];
    char version[32];
    enum module_type type;
    bool include_galaxy_extension;
    bool include_event_handler;
    bool include_callback_registration;
    bool include_manifest;
    bool include_makefile;
    bool include_test_file;
    bool include_readme;
    char output_dir[PATH_MAX];
};

/* Mock function declarations */
int module_template_params_init(struct module_template_params *params);
int module_generate_template(struct module_template_params *params);
