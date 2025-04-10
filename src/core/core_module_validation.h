#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "core_module_system.h"

/* Module-specific interface types - forward declarations */
struct cooling_module;
struct star_formation_module;
struct feedback_module;
struct agn_module;
struct mergers_module;
struct disk_instability_module;
struct reincorporation_module;
struct infall_module;
struct misc_module;

/**
 * @file core_module_validation.h
 * @brief Module validation framework for SAGE
 * 
 * This file provides utilities for validating modules to ensure they follow
 * the required interfaces and standards. It supports comprehensive validation
 * of module structure, function signatures, and behavior.
 */

/* Maximum number of validation issues to track */
#define MAX_VALIDATION_ISSUES 64

/* Maximum length of a validation issue message */
#define MAX_VALIDATION_MESSAGE 256

/**
 * Validation issue severity levels
 */
typedef enum {
    VALIDATION_INFO = 0,     /* Informational message */
    VALIDATION_WARNING = 1,  /* Warning (possible issue but not critical) */
    VALIDATION_ERROR = 2     /* Error (critical issue) */
} validation_severity_t;

/**
 * Validation issue structure
 * 
 * Represents a single issue found during module validation
 */
typedef struct {
    validation_severity_t severity;             /* Issue severity */
    char message[MAX_VALIDATION_MESSAGE];       /* Issue description */
    char component[64];                         /* Component where issue was found */
    char file[128];                             /* File where issue was found */
    int line;                                   /* Line number where issue was found */
} module_validation_issue_t;

/**
 * Validation result structure
 * 
 * Contains all issues found during module validation
 */
typedef struct {
    module_validation_issue_t issues[MAX_VALIDATION_ISSUES];  /* Array of validation issues */
    int num_issues;                                          /* Number of issues found */
    int error_count;                                         /* Number of errors found */
    int warning_count;                                       /* Number of warnings found */
    int info_count;                                          /* Number of informational messages */
} module_validation_result_t;

/**
 * Validation options
 * 
 * Controls the validation process
 */
typedef struct {
    bool validate_interface;      /* Whether to validate the module interface */
    bool validate_function_sigs;  /* Whether to validate function signatures */
    bool validate_dependencies;   /* Whether to validate module dependencies */
    bool validate_manifest;       /* Whether to validate the module manifest */
    bool verbose;                 /* Whether to output verbose validation information */
    bool strict;                  /* Whether to treat warnings as errors */
} module_validation_options_t;

/**
 * Initialize a validation result structure
 * 
 * @param result Validation result structure to initialize
 * @return true on success, false on failure
 */
bool module_validation_result_init(module_validation_result_t *result);

/**
 * Add an issue to a validation result
 * 
 * @param result Validation result to add the issue to
 * @param severity Severity of the issue
 * @param message Issue description
 * @param component Component where issue was found
 * @param file File where issue was found
 * @param line Line number where issue was found
 * @return true on success, false on failure
 */
bool module_validation_add_issue(
    module_validation_result_t *result,
    validation_severity_t severity,
    const char *message,
    const char *component,
    const char *file,
    int line
);

/**
 * Format a validation result as text
 * 
 * @param result Validation result to format
 * @param output Output buffer for the formatted text
 * @param size Size of the output buffer
 * @return true on success, false on failure
 */
bool module_validation_result_format(
    const module_validation_result_t *result,
    char *output,
    size_t size
);

/**
 * Initialize validation options with defaults
 * 
 * @param options Validation options to initialize
 * @return true on success, false on failure
 */
bool module_validation_options_init(module_validation_options_t *options);

/**
 * Validate a module's interface
 * 
 * Checks that the module interface is correctly structured and consistent.
 * 
 * @param module Pointer to the module interface
 * @param result Validation result structure
 * @param options Validation options
 * @return true if validation passed, false if validation failed
 */
bool module_validate_interface(
    const struct base_module *module,
    module_validation_result_t *result,
    const module_validation_options_t *options
);

/**
 * Validate a module's function signatures
 * 
 * Checks that the module's function signatures match the expected signatures.
 * 
 * @param module Pointer to the module interface
 * @param result Validation result structure
 * @param options Validation options
 * @return true if validation passed, false if validation failed
 */
bool module_validate_function_signatures(
    const struct base_module *module,
    module_validation_result_t *result,
    const module_validation_options_t *options
);

/**
 * Validate a module's dependencies
 * 
 * Checks that all of a module's dependencies are satisfied.
 * 
 * @param module_id ID of the module to validate
 * @param result Validation result structure
 * @param options Validation options
 * @return true if validation passed, false if validation failed
 */
bool module_validate_dependencies(
    int module_id,
    module_validation_result_t *result,
    const module_validation_options_t *options
);

/**
 * Validate a module manifest
 * 
 * Checks that a module manifest is correctly formed.
 * 
 * @param manifest Pointer to the module manifest
 * @param result Validation result structure
 * @param options Validation options
 * @return true if validation passed, false if validation failed
 */
bool module_validate_manifest_structure(
    const struct module_manifest *manifest,
    module_validation_result_t *result,
    const module_validation_options_t *options
);

/**
 * Validate a module from a file
 * 
 * Loads a module from a file and validates it.
 * 
 * @param path Path to the module file
 * @param result Validation result structure
 * @param options Validation options
 * @return true if validation passed, false if validation failed
 */
bool module_validate_file(
    const char *path,
    module_validation_result_t *result,
    const module_validation_options_t *options
);

/**
 * Validate a module by ID
 * 
 * Validates a module that is already loaded in the system.
 * 
 * @param module_id ID of the module to validate
 * @param result Validation result structure
 * @param options Validation options
 * @return true if validation passed, false if validation failed
 */
bool module_validate_by_id(
    int module_id,
    module_validation_result_t *result,
    const module_validation_options_t *options
);

/**
 * Determine if a validation result contains errors
 * 
 * @param result Validation result to check
 * @param options Validation options (affects treatment of warnings)
 * @return true if result contains errors, false otherwise
 */
bool module_validation_has_errors(
    const module_validation_result_t *result,
    const module_validation_options_t *options
);

#ifdef __cplusplus
}
#endif