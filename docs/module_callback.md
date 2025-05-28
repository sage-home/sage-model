# Module Callback System

## Overview
The Module Callback System provides controlled inter-module communication in SAGE, enabling physics modules to interact with each other in a structured manner while maintaining architectural integrity. It offers sophisticated mechanisms for function registration, invocation, call stack tracking, and circular dependency detection.

## Purpose
In a modular architecture like SAGE, individual modules often need to communicate with each other for scientific accuracy. For example, merger events might trigger star formation, or AGN feedback might affect cooling rates. However, direct function calls between modules would create tight coupling and undermine the modularity.

The Module Callback System addresses these challenges by:
1. **Enabling Communication**: Allowing modules to call functions in other modules without direct dependencies
2. **Maintaining Structure**: Enforcing architectural boundaries through declared dependencies
3. **Ensuring Reliability**: Tracking the call stack to detect circular dependencies and monitor execution flow
4. **Providing Diagnostics**: Capturing error information for debugging complex module interactions

This system is particularly important for physics modules that need to maintain scientific consistency while preserving architectural separation.

## Key Concepts
- **Function Registry**: Each module can register functions that other modules can call
- **Dependency Declarations**: Modules explicitly declare which other modules they depend on
- **Call Stack Tracking**: Every inter-module call is tracked for debugging and circular dependency detection
- **Controlled Invocation**: Function calls between modules happen through the controlled `module_invoke()` interface
- **Error Propagation**: Detailed error information is captured and propagated through the call chain
## Data Structures

### Module Function
```c
typedef struct {
    char name[MAX_FUNCTION_NAME];    /* Function name (used for lookup) */
    void *function_ptr;              /* Pointer to the function */
    module_function_type_t return_type; /* Type of value returned by the function */
    const char *signature;           /* Optional signature for type checking */
    const char *description;         /* Optional description */
} module_function_t;
```

### Function Registry
```c
struct module_function_registry {
    module_function_t functions[MAX_MODULE_FUNCTIONS]; /* Array of registered functions */
    int num_functions;               /* Number of registered functions */
};
typedef struct module_function_registry module_function_registry_t;
```

### Module Dependency
```c
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
```
### Call Frame
```c
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
```

### Call Stack
```c
typedef struct {
    module_call_frame_t frames[MAX_CALL_DEPTH]; /* Stack of call frames */
    int depth;                       /* Current stack depth */
} module_call_stack_t;
```

## API Reference

### Initialization

#### `module_callback_system_initialize()`
```c
int module_callback_system_initialize(void);
```
**Purpose**: Initializes the module callback system, creating the global call stack.  
**Returns**: 0 on success, non-zero on failure.  
**Notes**: 
- Must be called before any module callback functions are used
- Called automatically during SAGE initialization

#### `module_callback_system_cleanup()`
```c
void module_callback_system_cleanup(void);
```
**Purpose**: Releases resources used by the callback system.  
**Notes**: 
- Called automatically during SAGE shutdown
- Safe to call multiple times
### Function Registration

#### `module_register_function()`
```c
int module_register_function(
    int module_id,
    const char *name,
    void *function_ptr,
    module_function_type_t return_type,
    const char *signature,
    const char *description
);
```
**Purpose**: Registers a function with a module, making it callable by other modules.  
**Parameters**:
- `module_id`: ID of the module containing the function
- `name`: Name of the function (used for lookup)
- `function_ptr`: Pointer to the function
- `return_type`: Type of value returned by the function
- `signature`: Optional signature string for type checking and automated documentation/binding generation
- `description`: Optional description of what the function does

**Returns**: 0 on success, non-zero on failure.  
**Notes**:
- The `signature` parameter, while optional, is intended for future use in robust type checking and automated documentation or binding generation. Currently stored but not actively validated
- Function names must be unique within each module's registry  
**Example**:
```c
// Register a cooling rate calculation function
int register_cooling_functions(int module_id) {
    return module_register_function(
        module_id,
        "calculate_cooling_rate",
        (void*)calculate_cooling_rate,
        FUNCTION_TYPE_DOUBLE,
        "double (struct GALAXY*, double)",
        "Calculates cooling rate for a galaxy"
    );
}
```

### Call Stack Management

#### `module_call_stack_push()`
```c
int module_call_stack_push(
    int caller_id,
    int callee_id,
    const char *function_name,
    void *context
);
```
**Purpose**: Records a module invocation by pushing a frame onto the call stack.  
**Parameters**:
- `caller_id`: ID of the calling module
- `callee_id`: ID of the module being called
- `function_name`: Name of the function being called
- `context`: Context for the call (typically used for error propagation)

**Returns**: 0 on success, non-zero on failure.  
**Notes**:
- Automatically checks for circular dependencies
- Call stack depth is limited to MAX_CALL_DEPTH (32)
#### `module_call_stack_pop()`
```c
module_call_frame_t *module_call_stack_pop(void);
```
**Purpose**: Removes the most recent invocation record from the call stack.  
**Returns**: Pointer to the popped frame, or NULL if the stack is empty.  
**Notes**:
- Called automatically at the end of module_invoke()
- Always pair with module_call_stack_push() to maintain stack integrity

#### `module_call_stack_check_circular()`
```c
bool module_call_stack_check_circular(int module_id);
```
**Purpose**: Examines the call stack for loops involving the specified module.  
**Parameters**:
- `module_id`: ID of the module to check

**Returns**: true if a circular dependency exists, false otherwise.  
**Notes**:
- Called automatically during module_call_stack_push()
- Can be used proactively to prevent circular dependencies
### Call Stack Debugging

#### `module_call_stack_get_depth()`
```c
int module_call_stack_get_depth(void);
```
**Purpose**: Returns the number of frames on the call stack.  
**Returns**: Current call depth (0 if empty).  
**Notes**: Useful for monitoring call stack usage and detecting memory leaks.

#### `module_call_stack_get_trace()`
```c
int module_call_stack_get_trace(char *buffer, size_t buffer_size);
```
**Purpose**: Produces a formatted string representation of the call stack for debugging.  
**Parameters**:
- `buffer`: Output buffer for the call stack trace
- `buffer_size`: Size of the output buffer

**Returns**: 0 on success, non-zero on failure.  
**Example Output**:
```
Call Stack (depth=3):
  [0] cooling_module -> infall_module.calculate_infall_rate
  [1] infall_module -> agn_module.get_black_hole_mass
  [2] agn_module -> feedback_module.calculate_feedback_energy
```
#### `module_call_stack_get_trace_with_errors()`
```c
int module_call_stack_get_trace_with_errors(char *buffer, size_t buffer_size);
```
**Purpose**: Returns a formatted string representation of the call stack including error details.  
**Parameters**:
- `buffer`: Output buffer for the call stack trace
- `buffer_size`: Size of the output buffer

**Returns**: 0 on success, non-zero on failure.  
**Example Output**:
```
Call Stack (depth=3):
  [0] cooling_module -> infall_module.calculate_infall_rate
  [1] infall_module -> agn_module.get_black_hole_mass [ERROR: -2 "Invalid galaxy index"]
  [2] agn_module -> feedback_module.calculate_feedback_energy
```

### Error Handling

#### `module_call_stack_set_frame_error()`
```c
int module_call_stack_set_frame_error(
    int frame_index, 
    int error_code, 
    const char *error_message
);
```
**Purpose**: Associates error details with a specific call frame.  
**Parameters**:
- `frame_index`: Index of the frame to set error on (0 is oldest, depth-1 is newest)
- `error_code`: Error code to set
- `error_message`: Error message to set

**Returns**: 0 on success, non-zero on failure.
- `error_message`: Error message to set

**Returns**: 0 on success, non-zero on failure.

#### `module_call_set_error()`
```c
void module_call_set_error(int error_code, const char *error_message);
```
**Purpose**: Sets error information for the current frame.  
**Parameters**:
- `error_code`: Error code to set
- `error_message`: Error message to set

**Notes**:
- Convenience function that sets error on the current (top) frame
- Called from within module functions to report errors to callers

#### `module_call_clear_error()`
```c
void module_call_clear_error(void);
```
**Purpose**: Clears error information for the current frame.  
**Notes**: Used to reset error state when handling recoverable errors.
### Dependency Management

#### `module_declare_dependency()`
```c
int module_declare_dependency(
    int module_id,
    int module_type,
    const char *module_name,
    bool required,
    const char *min_version_str,
    const char *max_version_str,
    bool exact_match
);
```
**Purpose**: Adds a dependency to a module with version constraints.  
**Parameters**:
- `module_id`: ID of the module declaring the dependency
- `module_type`: Type of module depended on (e.g., MODULE_TYPE_COOLING)
- `module_name`: Optional specific module name (can be NULL for any module of the type)
- `required`: Whether dependency is required (true) or optional (false)
- `min_version_str`: Minimum version required (can be NULL)
- `max_version_str`: Maximum version allowed (can be NULL)
- `exact_match`: Whether an exact version match is required

**Returns**: 0 on success, non-zero on failure.
#### `module_declare_simple_dependency()`
```c
int module_declare_simple_dependency(
    int module_id,
    int module_type,
    const char *module_name,
    bool required
);
```
**Purpose**: Adds a basic dependency without version constraints.  
**Parameters**:
- `module_id`: ID of the module declaring the dependency
- `module_type`: Type of module depended on
- `module_name`: Optional specific module name
- `required`: Whether dependency is required

**Returns**: 0 on success, non-zero on failure.  
**Example**:
```c
// Declare that this module depends on any cooling module
module_declare_simple_dependency(
    my_module_id,
    MODULE_TYPE_COOLING,
    NULL,  // Any module of this type is acceptable
    true   // This is a required dependency
);
```
#### `module_call_validate()`
```c
int module_call_validate(int caller_id, int callee_id);
```
**Purpose**: Validates if a call from one module to another is valid based on dependencies.  
**Parameters**:
- `caller_id`: ID of the calling module
- `callee_id`: ID of the module being called

**Returns**: 0 if the call is valid, non-zero otherwise.  
**Notes**:
- Called automatically during module_invoke()
- Checks if the caller has declared a dependency on the callee

### Module Invocation

#### `module_invoke()`
```c
int module_invoke(
    int caller_id,
    int module_type,
    const char *module_name,
    const char *function_name,
    void *context,
    void *args,
    void *result
);
```
**Purpose**: Calls a registered function in another module.  
**Parameters**:
- `caller_id`: ID of the calling module
- `module_type`: Type of module to invoke (can be MODULE_TYPE_UNKNOWN if module_name is provided)
- `module_name`: Optional specific module name (can be NULL to use active module of type)
- `function_name`: Name of function to call
- `context`: Context to pass to function (typically used for error code return)
- `args`: Arguments to pass to the function
- `result`: Optional pointer to store result (type depends on function)

**Returns**: 0 on success, non-zero on failure.
**Notes**:
- This is the main entry point for the module callback system
- The `context` parameter is a `void*` pointer whose **type and interpretation are entirely determined by the callee function's signature**. While it typically points to an error code integer by convention, it could be any data structure required by the specific function being called
- The return value indicates whether the invocation mechanics succeeded, not whether the called function completed successfully
- Always check both the return value of module_invoke() and any error code in the context

**Example**:
```c
// Call the cooling rate calculation function in the cooling module
double cooling_rate = 0.0;
int error_code = 0;
int status = module_invoke(
    my_module_id,              // Caller module ID
    MODULE_TYPE_COOLING,       // Type of module to call
    NULL,                      // Use any cooling module (NULL = use active)
    "calculate_cooling_rate",  // Function name
    &error_code,               // Context (for error code)
    &galaxy,                   // Arguments
    &cooling_rate              // Result pointer
);

if (status != 0 || error_code != 0) {
    LOG_ERROR("Cooling calculation failed: %d, %d", status, error_code);
    // Handle error
}
```
## Usage Patterns

### Basic Module Function Registration
```c
// Define module functions
static double calculate_cooling_rate(struct GALAXY *galaxy, double dt) {
    // Implementation
    return cooling_rate;
}

// Register functions during module initialization
static int cooling_module_init(struct params *params, void **data_ptr) {
    // Allocate module data
    struct cooling_module_data *data = calloc(1, sizeof(struct cooling_module_data));
    if (!data) return MODULE_STATUS_MEMORY_ERROR;
    
    // Initialize module data
    // ...
    
    // Register callable functions
    module_register_function(
        cooling_module.module_id,
        "calculate_cooling_rate",
        (void*)calculate_cooling_rate,
        FUNCTION_TYPE_DOUBLE,
        "double (struct GALAXY*, double)",
        "Calculates cooling rate for a galaxy"
    );
    
    *data_ptr = data;
    return MODULE_STATUS_SUCCESS;
}
```
### Declaring Module Dependencies
```c
// Declare dependencies during module registration or initialization
static void register_feedback_module(void) {
    // Register module
    module_register(&feedback_module);
    
    // Declare dependencies
    module_declare_simple_dependency(
        feedback_module.module_id,
        MODULE_TYPE_COOLING,
        NULL,    // Any cooling module
        true     // Required dependency
    );
    
    module_declare_simple_dependency(
        feedback_module.module_id,
        MODULE_TYPE_INFALL,
        NULL,    // Any infall module
        true     // Required dependency
    );
    
    module_declare_dependency(
        feedback_module.module_id,
        MODULE_TYPE_AGN,
        "standard_agn_module",  // Specific module
        false,                  // Optional dependency
        "1.0.0",                // Minimum version
        "2.0.0",                // Maximum version
        false                   // Exact match not required
    );
}
```
### Invoking Functions in Other Modules
```c
// Get black hole accretion rate from AGN module
static double get_black_hole_accretion(
    int my_module_id,
    struct GALAXY *galaxy,
    double dt
) {
    double accretion_rate = 0.0;
    int error_code = 0;
    
    // Create argument structure (depends on function being called)
    struct {
        struct GALAXY *galaxy;
        double dt;
    } args = {galaxy, dt};
    
    // Invoke function in AGN module
    int status = module_invoke(
        my_module_id,
        MODULE_TYPE_AGN,
        NULL,                      // Use active AGN module
        "calculate_accretion_rate",
        &error_code,               // For error reporting
        &args,
        &accretion_rate
    );
    
    // Check for errors
    if (status != 0) {
        LOG_ERROR("Failed to invoke AGN module function: %d", status);
        return 0.0;
    }
    
    if (error_code != 0) {
        LOG_ERROR("AGN module reported error: %d", error_code);
        return 0.0;
    }
    
    return accretion_rate;
}
```
### Error Handling in Called Functions
```c
// Function that will be called by other modules
static double calculate_cooling_rate(
    struct cooling_module_data *data,
    struct GALAXY *galaxy,
    double dt
) {
    // Validate inputs
    if (!galaxy) {
        // Set error information for the caller
        module_call_set_error(-1, "NULL galaxy pointer");
        return 0.0;
    }
    
    if (galaxy->Type == 3) {  // Merged galaxy
        module_call_set_error(-2, "Cannot calculate cooling for merged galaxy");
        return 0.0;
    }
    
    // Continue with normal processing
    double cooling_rate = compute_cooling_rate(data, galaxy, dt);
    
    // Clear any previous error before returning successfully
    module_call_clear_error();
    
    return cooling_rate;
}
```

## Integration with Pipeline System

The module callback system integrates with the pipeline system through the pipeline executor:

```c
// In physics_pipeline_executor.c
int physics_step_executor(
    struct pipeline_step *step,
    struct base_module *module,
    void *module_data,
    struct pipeline_context *context
) {
    // ...
    
    // Push to call stack for tracking
    module_call_stack_push(
        context->pipeline->id,  // Pipeline as caller
        module->module_id,      // Module as callee
        phase_function_name,    // e.g., "execute_galaxy_phase"
        context                 // Execution context
    );
    
    // Execute module function
    int result = module_function(module_data, context);
    
    // Pop from call stack
    module_call_stack_pop();
    
    return result;
}
```
## Error Handling

The module callback system provides several layers of error handling:

### Error Types

1. **Invocation Errors**: When module_invoke() itself fails (cannot find module, function not registered, etc.)
2. **Dependency Errors**: When a module tries to call another without declaring a dependency
3. **Circular Dependency Errors**: When a chain of calls would create a loop
4. **Function Execution Errors**: When the called function completes but reports an error

### Best Practices

1. **Always Check Both Error Sources**:
   ```c
   int error_code = 0;
   int status = module_invoke(..., &error_code, ...);
   if (status != 0 || error_code != 0) {
       // Handle both invocation and function errors
   }
   ```

2. **Set Meaningful Error Codes**:
   ```c
   if (invalid_input) {
       module_call_set_error(MODULE_ERROR_INVALID_INPUT, "Invalid parameter: %s", param_name);
       return 0;
   }
   ```
3. **Clear Errors on Success**:
   ```c
   // Before successful return
   module_call_clear_error();
   return result;
   ```

4. **Use Call Stack Traces for Debugging**:
   ```c
   char trace[2048];
   module_call_stack_get_trace_with_errors(trace, sizeof(trace));
   LOG_DEBUG("Call stack: %s", trace);
   ```

## Performance Considerations

1. **Call Stack Overhead**: Each inter-module call adds overhead for stack management. For performance-critical code, consider bundling multiple operations into a single call.

2. **Function Lookup**: Looking up functions by name has O(n) complexity. For frequently used functions, consider caching the module ID and function name.

3. **Error Handling Overhead**: Error context information adds memory and processing overhead. In performance-critical paths, consider simplified error handling.

4. **Call Depth Limitations**: The maximum call depth is limited to MAX_CALL_DEPTH (32). Extremely deep call chains should be restructured.

## Integration Points
- **Dependencies**: Relies on the module system for module lifecycle management
- **Used By**: All physics modules that need to communicate with each other
- **Events**: Does not emit events but may be used within event handlers
- **Properties**: No direct interaction with the property system
## Testing

The module callback system is validated through the comprehensive test suite in `tests/test_module_callback.c`, which verifies all core functionality:

1. **System Initialization**: Tests proper initialization of the callback system and global call stack
2. **Function Registration**: Validates function registration, duplicate detection, and parameter validation
3. **Call Stack Tracking**: Tests pushing, popping, and depth tracking of the call stack
4. **Circular Dependency Detection**: Verifies detection of both simple (A→A) and complex (A→B→C→A) circular dependencies
5. **Parameter Passing**: Tests that parameters and return values are correctly passed between modules
6. **Error Propagation**: Verifies that errors are properly propagated through call chains
7. **Dependency Management**: Tests dependency declaration and validation with version constraints
8. **Call Stack Diagnostics**: Validates trace generation and error information reporting
9. **Module Unregistration**: Tests proper cleanup when modules are unregistered

The test creates mock modules with intercommunication patterns to thoroughly exercise the callback infrastructure. Run the test with:

```bash
make test_module_callback
./tests/test_module_callback
```

## See Also
- [Module System and Configuration](module_system_and_configuration.md)
- [Pipeline System](pipeline_phases.md)
- [Event System](event_system.md)

---

*Last updated: May 28, 2025*  
*Component version: 1.0*