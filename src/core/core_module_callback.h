#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "core_allvars.h"

/**
 * @file core_module_callback.h
 * @brief Module Callback System for SAGE
 * 
 * This file defines the module callback system that allows direct module-to-module
 * communication while maintaining a clean architecture. It enables modules to declare
 * dependencies on other modules and call functions in those modules at runtime.
 */

/* Maximum numbers */
#define MAX_MODULE_FUNCTIONS 32
#define MAX_FUNCTION_NAME 64
#define MAX_CALL_DEPTH 16
#define MAX_DEPENDENCY_NAME 64

/* Forward declarations */
struct base_module;
struct module_version;

/**
 * Function return type identifiers
 * 
 * Identifies the type of value returned by a function
 */
typedef enum {
    FUNCTION_TYPE_VOID,       /* void function() */
    FUNCTION_TYPE_INT,        /* int function() */
    FUNCTION_TYPE_DOUBLE,     /* double function() */
    FUNCTION_TYPE_POINTER     /* void* function() */
} module_function_type_t;

/**
 * Module function information
 * 
 * Contains information about a function that can be called via the callback system
 */
typedef struct {
    char name[MAX_FUNCTION_NAME];    /* Function name (used for lookup) */
    void *function_ptr;              /* Pointer to the function */
    module_function_type_t return_type; /* Type of value returned by the function */
    const char *signature;           /* Optional signature for type checking */
    const char *description;         /* Optional description */
} module_function_t;

/**
 * Module function registry
 * 
 * Tracks functions that a module makes available for calling by other modules
 */
struct module_function_registry {
    module_function_t functions[MAX_MODULE_FUNCTIONS]; /* Array of registered functions */
    int num_functions;               /* Number of registered functions */
};
typedef struct module_function_registry module_function_registry_t;

/**
 * Dependency declaration structure
 * 
 * Enhanced version of module_dependency for runtime use
 */
struct module_dependency {
    char name[MAX_DEPENDENCY_NAME];      /* Name of module dependency */
    char module_type[32];                /* Type of module dependency */
    bool optional;                       /* Whether dependency is optional */
    bool exact_match;                    /* Require exact version match */
    int type;                            /* Type of module (enum module_type) */
    
    /* These fields use the forward-declared struct module_version */
    /* Will be set to zeros by default since they need special handling */
    char min_version_str[32];            /* Minimum version as string */
    char max_version_str[32];            /* Maximum version as string */
};
typedef struct module_dependency module_dependency_t;

/**
 * Module call frame
 * 
 * Contains information about a single module-to-module call
 */
typedef struct {
    int caller_module_id;            /* ID of the calling module */
    int callee_module_id;            /* ID of the module being called */
    const char *function_name;       /* Name of the function being called */
    void *context;                   /* Context for the call */
} module_call_frame_t;

/**
 * Module call stack
 * 
 * Tracks the chain of module-to-module calls
 */
typedef struct {
    module_call_frame_t frames[MAX_CALL_DEPTH]; /* Stack of call frames */
    int depth;                       /* Current stack depth */
} module_call_stack_t;

/* Global call stack */
extern module_call_stack_t *global_call_stack;

/**
 * Initialize the module callback system
 * 
 * Sets up the global call stack and prepares it for module callbacks.
 * 
 * @return 0 on success, error code on failure
 */
int module_callback_system_initialize(void);

/**
 * Clean up the module callback system
 * 
 * Releases resources used by the callback system.
 * 
 * @return 0 on success, error code on failure
 */
int module_callback_system_cleanup(void);

/**
 * Register a function with a module
 * 
 * Makes a function callable from other modules.
 * 
 * @param module_id ID of the module containing the function
 * @param name Name of the function (used for lookup)
 * @param function_ptr Pointer to the function
 * @param return_type Type of value returned by the function
 * @param signature Optional signature string
 * @param description Optional description
 * @return 0 on success, error code on failure
 */
int module_register_function(
    int module_id,
    const char *name,
    void *function_ptr,
    module_function_type_t return_type,
    const char *signature,
    const char *description
);

/**
 * Push a frame onto the call stack
 * 
 * Records a module invocation.
 * 
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @param function_name Name of the function being called
 * @param context Context for the call
 * @return 0 on success, error code on failure
 */
int module_call_stack_push(
    int caller_id,
    int callee_id,
    const char *function_name,
    void *context
);

/**
 * Pop a frame from the call stack
 * 
 * Removes the most recent invocation record.
 * 
 * @return 0 on success, error code on failure
 */
int module_call_stack_pop(void);

/**
 * Check for circular dependencies
 * 
 * Examines the call stack for loops.
 * 
 * @param module_id ID of the module to check
 * @return true if a circular dependency exists, false otherwise
 */
bool module_call_stack_check_circular(int module_id);

/**
 * Get the current call depth
 * 
 * Returns the number of frames on the call stack.
 * 
 * @return Current call depth
 */
int module_call_stack_get_depth(void);

/**
 * Get the current call stack trace
 * 
 * Returns a formatted string representation of the call stack for debugging.
 * 
 * @param buffer Output buffer for the call stack trace
 * @param buffer_size Size of the output buffer
 * @return 0 on success, error code on failure
 */
int module_call_stack_get_trace(char *buffer, size_t buffer_size);

/**
 * Get information about a specific call frame
 * 
 * Retrieves details about a call frame at the specified depth.
 * 
 * @param depth Index of the call frame to retrieve (0 is the oldest frame)
 * @param frame Output pointer to store the call frame information
 * @return 0 on success, error code on failure
 */
int module_call_stack_get_frame(int depth, module_call_frame_t *frame);

/**
 * Find module in call stack
 * 
 * Searches the call stack for a specific module and returns its position.
 * 
 * @param module_id ID of the module to look for
 * @param as_caller Whether to look for the module as a caller (true) or callee (false)
 * @return Position in the call stack, or -1 if not found
 */
int module_call_stack_find_module(int module_id, bool as_caller);

/**
 * Get the most recent call frame
 * 
 * Returns the call frame at the top of the stack.
 * 
 * @param frame Output pointer to store the call frame information
 * @return 0 on success, error code on failure
 */
int module_call_stack_get_current_frame(module_call_frame_t *frame);

/**
 * Declare a module dependency
 * 
 * Adds a dependency to a module.
 * 
 * @param module_id ID of the module declaring the dependency
 * @param module_type Type of module depended on
 * @param module_name Optional specific module name
 * @param required Whether dependency is required
 * @param min_version_str Minimum version required (can be NULL)
 * @param max_version_str Maximum version allowed (can be NULL)
 * @param exact_match Whether an exact version match is required
 * @return 0 on success, error code on failure
 */
int module_declare_dependency(
    int module_id,
    int module_type,
    const char *module_name,
    bool required,
    const char *min_version_str,
    const char *max_version_str,
    bool exact_match
);

/**
 * Simplified dependency declaration
 * 
 * Adds a basic dependency without version constraints.
 * 
 * @param module_id ID of the module declaring the dependency
 * @param module_type Type of module depended on
 * @param module_name Optional specific module name
 * @param required Whether dependency is required
 * @return 0 on success, error code on failure
 */
int module_declare_simple_dependency(
    int module_id,
    int module_type,
    const char *module_name,
    bool required
);

/**
 * Check if a module call is allowed
 * 
 * Validates if a call from one module to another is valid based on dependencies.
 * 
 * @param caller_id ID of the calling module
 * @param callee_id ID of the module being called
 * @return 0 on success, error code on failure
 */
int module_call_validate(int caller_id, int callee_id);

/**
 * Invoke a function in another module
 * 
 * Calls a registered function in a module.
 * 
 * @param caller_id ID of the calling module
 * @param module_type Type of module to invoke (can be MODULE_TYPE_UNKNOWN if module_name is provided)
 * @param module_name Optional specific module name (can be NULL to use active module of type)
 * @param function_name Name of function to call
 * @param context Context to pass to function
 * @param args Arguments to pass to the function
 * @param result Optional pointer to store result (type depends on function)
 * @return 0 on success, error code on failure
 */
int module_invoke(
    int caller_id,
    int module_type,
    const char *module_name,
    const char *function_name,
    void *context,
    void *args,
    void *result
);

#ifdef __cplusplus
}
#endif