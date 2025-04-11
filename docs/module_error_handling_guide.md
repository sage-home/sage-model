# Module Error Handling System Guide

## Overview

The SAGE Module Error Handling System provides comprehensive error reporting, diagnostics, and debugging capabilities for the SAGE modular architecture. It enhances the existing error handling with detailed contextual information, error history tracking, and user-friendly diagnostic tools.

## Key Components

### 1. Error Context

The error context system maintains a historical record of errors encountered by each module. Key features include:

- **Error History**: Maintains the most recent errors in a circular buffer
- **Detailed Error Information**: Records severity, location, timestamp, and call stack information
- **Compatibility**: Maintains backward compatibility with the original error handling

### 2. Call Stack Tracing

The call stack tracing system provides context for inter-module errors:

- **Call Frames**: Records the calling module, called module, function information, and error details
- **Call Diagnostics**: Provides formatted call stack traces with associated errors for debugging
- **Error Propagation Tracking**: Tracks how errors propagate through the module call hierarchy
- **Error Association**: Allows associating errors with specific call frames
- **Enhanced Traces**: Generates detailed call stack traces with error information
- **Circular Dependency Detection**: Identifies circular dependencies between modules

### 3. Diagnostic Utilities

The diagnostic system generates user-friendly error reports:

- **Comprehensive Reports**: Combines error information and call stack traces
- **Customizable Output**: Configurable level of detail and formatting
- **Module-Specific Diagnostics**: Targeted diagnostics for specific modules

## Usage Guide

### Basic Error Reporting

For basic error reporting with enhanced context, use the `MODULE_ERROR` macro:

```c
MODULE_ERROR(module, MODULE_STATUS_ERROR, "Failed to initialize %s", "subsystem");
```

For warnings, use the `MODULE_WARNING` macro:

```c
MODULE_WARNING(module, MODULE_STATUS_INVALID_ARGS, "Invalid parameter %s", "value");
```

### Retrieving Error Information

To get the latest error from a module:

```c
module_error_info_t error;
module_get_latest_error(module, &error);
```

To get the error history:

```c
module_error_info_t errors[MAX_ERROR_HISTORY];
int num_errors;
module_get_error_history(module, errors, MAX_ERROR_HISTORY, &num_errors);
```

### Working with Call Stack Errors

To associate an error with the current call frame:

```c
int result = some_function_call();
if (result != MODULE_STATUS_SUCCESS) {
    // Get current frame index (top of stack)
    int current_depth = module_call_stack_get_depth();
    if (current_depth > 0) {
        module_call_stack_set_frame_error(current_depth - 1, result, "Error in function call");
    }
    return result;
}
```

To get a detailed call stack trace with error information:

```c
char buffer[2048];
module_call_stack_get_trace_with_errors(buffer, sizeof(buffer));
printf("Call stack with errors: %s\n", buffer);
```

### Generating Diagnostic Reports

For comprehensive error diagnostics:

```c
char buffer[2048];
module_diagnostic_options_t options;
module_diagnostic_options_init(&options);
module_get_comprehensive_diagnostics(module_id, buffer, sizeof(buffer), &options);
printf("%s\n", buffer);
```

## Integration with Module System

The error handling system is automatically initialized when a module is registered and initialized. The internal error context is created during module initialization and is properly cleaned up during module unregistration.

## Example: Handling Errors in a Module

```c
int cooling_module_calculate(int gal_idx, double dt, void *module_data) {
    if (gal_idx < 0) {
        MODULE_ERROR(module, MODULE_STATUS_INVALID_ARGS, "Invalid galaxy index: %d", gal_idx);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    // Calculation code...
    
    if (calculation_failed) {
        MODULE_ERROR(module, MODULE_STATUS_ERROR, "Cooling calculation failed for galaxy %d", gal_idx);
        return MODULE_STATUS_ERROR;
    }
    
    return MODULE_STATUS_SUCCESS;
}
```

## Best Practices

1. **Always Check Return Codes**: Always check the return codes from module functions and handle errors appropriately.

2. **Use the Provided Macros**: Use the `MODULE_ERROR` and `MODULE_WARNING` macros for consistent error reporting.

3. **Include Detailed Error Messages**: Provide specific error messages with relevant context (e.g., parameter values, galaxy indices).

4. **Propagate Errors**: When a function encounters an error from a called function, propagate the error with appropriate context.

5. **Validate Parameters**: Validate function parameters early to provide clear error messages for invalid inputs.

## Implementation Details

The error handling system consists of several components:

- **core_module_error.h/c**: Defines the error context structure and core error handling functions
- **core_module_diagnostics.h/c**: Provides user-friendly diagnostic tools
- **core_module_system.h/c**: Integrates the error system with the module lifecycle

The key structures are:

- `module_error_info_t`: Contains detailed information about a single error
- `module_error_context_t`: Manages a module's error history
- `module_diagnostic_options_t`: Controls the formatting of diagnostic output

## Testing

The error handling system includes comprehensive tests in `tests/test_module_error.c`, which can be compiled and run using the provided Makefile. Run the tests with:

```bash
cd tests
make -f Makefile.module_error run
```
