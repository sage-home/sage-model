#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "core_allvars.h"
#include "core_types.h"  /* For common type definitions */

/**
 * @file core_module_callback.h
 * @brief Module Callback System for SAGE
 * 
 * This file defines the module callback system that allows direct module-to-module
 * communication while maintaining a clean architecture. It enables modules to declare
 * dependencies on other modules and call functions in those modules at runtime.
 * 
 * The callback system supports:
 * - Declaring dependencies between modules
 * - Registering functions for other modules to call
 * - Invoking functions in other modules
 * - Tracking the call stack for diagnostics
 * - Detecting and preventing circular dependencies
 * - Error propagation between modules
 * 
 * Usage Example:
 * 
 * 1. Initialize the system:
 *    ```c
 *    int status = module_callback_system_initialize();
 *    ```
 * 
 * 2. Register callable functions from your module:
 *    ```c
 *    module_register_function(module_id, "function_name", function_ptr,
 *                            FUNCTION_TYPE_DOUBLE, "double (void*, void*)",
 *                            "Description of the function");
 *    ```
 * 
 * 3. Declare dependencies on other modules:
 *    ```c
 *    module_declare_simple_dependency(module_id, MODULE_TYPE_COOLING, 
 *                                   NULL, false);
 *    ```
 * 
 * 4. Call functions in other modules:
 *    ```c
 *    double result = 0.0;
 *    int error_code = 0;
 *    int status = module_invoke(calling_module_id, MODULE_TYPE_COOLING, 
 *                             NULL, "get_cooling_rate", &error_code,
 *                             arg1, arg2, &result);
 *    ```
 * 
 * 5. Check for errors:
 *    ```c
 *    if (status != MODULE_STATUS_SUCCESS || error_code != 0) {
 *        // Handle error
 *    }
 *    ```
 * 
 * Note: The module callback system must be initialized before use,
 * and modules must register their functions before other modules can call them.
 * Dependencies must be declared to maintain architectural integrity.
 */

/* Maximum numbers */
#define MAX_MODULE_FUNCTIONS 32
#define MAX_FUNCTION_NAME 64
#define MAX_CALL_DEPTH 32
#define MAX_DEPENDENCY_NAME 64

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
    
    /* Version constraints as strings (kept for backward compatibility) */
    char min_version_str[32];            /* Minimum version as string */
    char max_version_str[32];            /* Maximum version as string */
    
    /* Parsed version constraints */
    struct module_version min_version;   /* Parsed minimum version */
    struct module_version max_version;   /* Parsed maximum version */
    bool has_parsed_versions;            /* Whether versions have been parsed */
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
    
    /* Error information */
    int error_code;                  /* Error code (0 if no error) */
    char error_message[MAX_ERROR_MESSAGE]; /* Error message (if any) */
    bool has_error;                  /* Whether this frame has an error */
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
 */
void module_callback_system_cleanup(void);

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
 * Removes the most recent invocation record and returns it.
 * 
 * @return Pointer to the popped frame, or NULL if stack is empty
 */
module_call_frame_t *module_call_stack_pop(void);

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
 * Get the current call stack trace with error information
 * 
 * Returns a formatted string representation of the call stack including error details.
 * 
 * @param buffer Output buffer for the call stack trace
 * @param buffer_size Size of the output buffer
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_get_trace_with_errors(char *buffer, size_t buffer_size);

/**
 * Set error information for a frame in the call stack
 * 
 * Associates error details with a specific call frame.
 * 
 * @param frame_index Index of the frame to set error on (0 is oldest, depth-1 is newest)
 * @param error_code Error code to set
 * @param error_message Error message to set
 * @return MODULE_STATUS_SUCCESS on success, error code on failure
 */
int module_call_stack_set_frame_error(int frame_index, int error_code, 
                                     const char *error_message);

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
 * Calls a registered function in a module. This is the main entry point for
 * the module callback system.
 * 
 * @param caller_id ID of the calling module
 * @param module_type Type of module to invoke (can be MODULE_TYPE_UNKNOWN if module_name is provided)
 * @param module_name Optional specific module name (can be NULL to use active module of type)
 * @param function_name Name of function to call
 * @param context Context to pass to function (typically used to return error code)
 * @param args Arguments to pass to the function
 * @param result Optional pointer to store result (type depends on function)
 * @return 0 on success, error code on failure
 * 
 * @note The meaning of the @p context parameter depends on the invoked function,
 *       but it typically points to an integer that will be set to an error code
 *       if the function fails. The return value of module_invoke only indicates
 *       whether the invocation mechanics succeeded, not whether the called function
 *       completed successfully. You should always check the context parameter for
 *       error codes even if module_invoke returns MODULE_STATUS_SUCCESS.
 * 
 * @note The type of @p args and @p result depends on the function being called.
 *       It is the caller's responsibility to ensure that these parameters have
 *       the correct type for the function being called. Incorrect types can lead
 *       to memory corruption or crashes.
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

/* Get the current call frame */
module_call_frame_t *module_call_stack_current(void);

/* Set error information for the current frame */
void module_call_set_error(
    int error_code,
    const char *error_message
);

/* Clear error information for the current frame */
void module_call_clear_error(void);

/* Note: module_execute_with_callback is declared in core_pipeline_system.h 
 * to avoid circular dependencies
 */

/**
 * Error Handling in the Module Callback System
 * 
 * The callback system provides several mechanisms for error handling:
 * 
 * 1. Return values: Functions in the callback system return status codes
 *    (MODULE_STATUS_SUCCESS on success, error code on failure).
 * 
 * 2. Context parameter: When calling module_invoke(), a context parameter
 *    is passed that can be used to store error codes from the called function.
 * 
 * 3. Call stack errors: Errors can be set on the call stack using
 *    module_call_set_error() and retrieved using module_call_stack_get_frame().
 * 
 * 4. Module errors: Each module maintains its own error state that can be
 *    accessed using module_get_last_error().
 * 
 * Best Practices:
 * 
 * - Always check both the return value of module_invoke() and the context
 *   parameter for errors.
 * - Use module_call_set_error() to set detailed error information from
 *   within called functions.
 * - Propagate errors up the call chain rather than silently ignoring them.
 * - When declaring dependencies, consider whether they are required or
 *   optional and handle missing dependencies gracefully.
 */

#ifdef __cplusplus
}
#endif