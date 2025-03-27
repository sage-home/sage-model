# Phase 4: Module Invocation - Implementation Report

## Overview

Phase 4 of the Module Dependencies Framework implementation adds the critically important module invocation functionality, enabling modules to call functions in other modules at runtime. This is the core feature that enables direct module-to-module communication while maintaining a clean architecture and enforcing dependencies.

## Files Modified

1. **`/src/core/core_module_callback.h`**:
   - Added function declaration for `module_invoke` function
   - Documented the purpose and parameters for the function

2. **`/src/core/core_module_callback.c`**:
   - Implemented the `module_invoke` function
   - Added type-specific function calling based on return type
   - Implemented module lookup by name or type
   - Added integration with the call stack system

3. **`/tests/test_module_callback.c`**:
   - Added comprehensive tests for the `module_invoke` function
   - Extended the test with multiple modules to test cross-module calls
   - Added tests for different function return types
   - Added error case testing

## Key Features Implemented

### 1. Dynamic Function Invocation

- **Module Lookup**: Functions can be called by either module type or name
- **Return Type Handling**: Properly handle different function return types:
  - FUNCTION_TYPE_VOID: For functions that don't return a value
  - FUNCTION_TYPE_INT: For functions that return an integer
  - FUNCTION_TYPE_DOUBLE: For functions that return a floating-point number
  - FUNCTION_TYPE_POINTER: For functions that return a pointer
- **Result Management**: Optionally store the result of the function call

### 2. Dependency Enforcement

- **Call Validation**: Verify that the caller has declared a dependency on the target module
- **Type Checking**: Ensure that named modules are of the expected type
- **Circular Dependency Prevention**: Prevent infinite loops by checking the call stack

### 3. Error Handling

- **Comprehensive Error Reporting**: Provide detailed error messages for:
  - Missing modules
  - Invalid function names
  - Circular dependencies
  - Type mismatches
  - Missing dependencies
- **Status Codes**: Return meaningful status codes that indicate the cause of failures

### 4. Call Stack Integration

- **Call Frame Management**: Automatically push/pop call frames to track invocations
- **Context Passing**: Pass context information between modules
- **Trace Generation**: Provide detailed call traces for debugging

## Usage Example

The `module_invoke` function can be used as follows:

```c
/* Call a function in the active cooling module */
double cooling_result;
cooling_args_t args = { .galaxy_index = 42, .dt = 0.25 };
context_t context = { .value = 100 };

int status = module_invoke(
    my_module_id,                /* Caller module ID */
    MODULE_TYPE_COOLING,         /* Type of module to call */
    NULL,                        /* Use active module of this type */
    "calculate_cooling",         /* Function name */
    &context,                    /* Context */
    &args,                       /* Function arguments */
    &cooling_result              /* Where to store the result */
);

if (status == MODULE_STATUS_SUCCESS) {
    printf("Cooling result: %.2f\n", cooling_result);
} else {
    printf("Failed to invoke cooling function: %d\n", status);
}
```

Or to call a specific named module:

```c
/* Call a function in a specific named module */
status = module_invoke(
    my_module_id,                /* Caller module ID */
    MODULE_TYPE_UNKNOWN,         /* Not specifying type */
    "SpecificCoolingModule",     /* Use this specific module */
    "calculate_cooling",         /* Function name */
    &context,                    /* Context */
    &args,                       /* Function arguments */
    &cooling_result              /* Where to store the result */
);
```

## Testing

The implementation was thoroughly tested using the `test_module_callback` program, which verifies:

1. Invoking functions by module type
2. Invoking functions by module name
3. Different function return types (void, int, double, pointer)
4. Error handling for invalid function names
5. Error handling for missing dependencies
6. Error handling for circular dependencies

All tests pass successfully, confirming the correct implementation of the module invocation system.

## Integration with Existing Codebase

The implementation integrates seamlessly with the existing codebase, building upon the foundation established in the previous phases:

- **Phase 1: Basic Data Structures** - Provided the core structures for module callbacks
- **Phase 2: Dependency Management** - Added runtime dependency validation
- **Phase 3: Call Stack Management** - Enhanced the call stack with diagnostics and validation
- **Phase 4: Module Invocation** - Added the critical function invocation capabilities

Together, these components provide a robust and flexible module communication system that allows modules to interact directly while maintaining clean architecture.

## Next Steps

With Phase 4 complete, the Module Dependencies Framework is now fully functional. The next phases in the overall refactoring plan should focus on:

1. **Phase 5: Pipeline Integration** - Update the pipeline execution to leverage the module callback system
2. **Phase 6: Advanced Performance Optimization** - Optimize the callback system for high-performance scenarios
3. **Phase 7: Documentation and Tools** - Provide comprehensive documentation and tools for module development

## Conclusion

The Phase 4 implementation completes the core functionality of the Module Dependencies Framework. With the `module_invoke` function, modules can now dynamically call functions in other modules at runtime, with proper dependency enforcement and error handling. This is a significant milestone in the SAGE refactoring plan, enabling more flexible and modular physics implementations.