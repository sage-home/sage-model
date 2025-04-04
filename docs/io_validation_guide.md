# SAGE I/O Validation Framework Guide

## Overview

The I/O Validation Framework is a comprehensive system for ensuring data consistency across different I/O formats in SAGE. It provides utilities for:

- Validating galaxy data (checking for NaN, infinity, logical constraints)
- Verifying format capability compatibility
- Validating extended property serialization
- Checking references and data consistency
- Ensuring proper I/O parameter validation

The framework is designed to provide clear, actionable feedback when validation issues are detected, making it easier to identify and fix problems before they cause runtime errors or corrupted data.

## Core Concepts

### Validation Context

The validation framework works through a `validation_context` structure that:

- Tracks validation results
- Controls validation behavior through strictness levels
- Provides configuration options for validation behavior
- Collects both warnings and errors

### Validation Results

Validation results have:

- Error codes (identifying the specific type of validation failure)
- Severity levels (INFO, WARNING, ERROR, FATAL)
- Check types (categorizing the validation)
- Detailed messages with context information
- Source location information (file, line)

### Strictness Levels

The framework supports three levels of strictness:

- **RELAXED**: Skip non-critical checks, warnings are ignored
- **NORMAL**: Perform all checks, warnings are logged but don't cause failure
- **STRICT**: Perform all checks, warnings are treated as errors

## Using the Validation Framework

### Basic Usage Pattern

```c
// Initialize the validation context
struct validation_context ctx;
validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);

// Perform validations
validation_check_not_null(&ctx, ptr, "Component", __FILE__, __LINE__, "Pointer must not be NULL");
validation_check_finite(&ctx, value, "Component", __FILE__, __LINE__, "Value must be finite");
// ...or use macros
VALIDATE_NOT_NULL(&ctx, ptr, "Component", "Pointer must not be NULL");
VALIDATE_FINITE(&ctx, value, "Component", "Value must be finite");

// Check validation status
if (validation_passed(&ctx)) {
    // Proceed with operation
} else {
    // Report validation failures
    validation_report(&ctx);
    // Handle errors
}

// Clean up
validation_cleanup(&ctx);
```

### Convenience Macros

The framework provides several convenience macros to make validation easier:

- `VALIDATE_CONDITION(ctx, condition, component, format, ...)` - Check a condition
- `VALIDATE_NOT_NULL(ctx, ptr, component, format, ...)` - Check for NULL pointer
- `VALIDATE_FINITE(ctx, value, component, format, ...)` - Check for NaN/Infinity
- `VALIDATE_BOUNDS(ctx, index, min, max, component, format, ...)` - Check array bounds
- `VALIDATE_CAPABILITY(ctx, handler, capability, component, format, ...)` - Check format capability

There are also macros for adding results directly:
- `VALIDATION_WARN(ctx, code, check_type, component, format, ...)`
- `VALIDATION_ERROR(ctx, code, check_type, component, format, ...)`
- `VALIDATION_FATAL(ctx, code, check_type, component, format, ...)`

### Validation in I/O Operations

In the I/O system, validation should be performed at several key points:

1. **Before initialization**:
   - Validate parameters
   - Check file existence and permissions
   - Verify format compatibility

2. **Before reading/writing data**:
   - Validate array sizes and indices
   - Check for NULL pointers
   - Verify capabilities for the operation

3. **After reading data**:
   - Validate data consistency
   - Check for NaN/Infinity values
   - Verify relationships between values

4. **Before finalizing I/O**:
   - Verify all resources are properly tracked
   - Check for any pending operations

## Integration with Error Handling

The validation framework integrates with SAGE's error handling system through:

1. The `validation_map_to_io_error` function, which maps validation errors to I/O errors
2. Logging validation results with appropriate severity
3. Converting validation failures to appropriate return codes

Example integration:

```c
int initialize_handler(const char *filename, struct params *params, void **format_data) {
    struct validation_context ctx;
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Validate parameters
    VALIDATE_NOT_NULL(&ctx, filename, "initialize_handler", "Filename cannot be NULL");
    VALIDATE_NOT_NULL(&ctx, params, "initialize_handler", "Parameters cannot be NULL");
    VALIDATE_NOT_NULL(&ctx, format_data, "initialize_handler", "Format data pointer cannot be NULL");
    
    // Report validation results
    if (!validation_passed(&ctx)) {
        validation_report(&ctx);
        io_set_error(validation_map_to_io_error(ctx.results[0].code), ctx.results[0].message);
        validation_cleanup(&ctx);
        return -1;
    }
    
    // Proceed with initialization...
    validation_cleanup(&ctx);
    return 0;
}
```

## Galaxy Validation

The framework includes comprehensive galaxy validation through the `validation_check_galaxies` function, which checks:

1. Galaxy data values (NaN, infinity, etc.)
2. Galaxy references (valid IDs, etc.)
3. Logical consistency (non-negative masses, etc.)

Example usage:

```c
// Validate galaxies before writing
status = validation_check_galaxies(&ctx, galaxies, num_galaxies, "save_galaxies", VALIDATION_CHECK_CONSISTENCY);
if (status != 0) {
    LOG_ERROR("Galaxy validation failed, aborting save operation");
    // Report detailed validation results
    validation_report(&ctx);
    return -1;
}
```

## Extending the Validation Framework

### Adding New Validation Checks

To add new validation checks:

1. Define a new validation function:
   ```c
   int validation_check_new_condition(struct validation_context *ctx, ...);
   ```

2. Implement the check using `validation_add_result` for failures

3. Add a convenience macro if appropriate:
   ```c
   #define VALIDATE_NEW_CONDITION(ctx, ...) \
       validation_check_new_condition(ctx, ..., __FILE__, __LINE__, ...)
   ```

### Format-Specific Validation

For format-specific validation, you can:

1. Create format-specific validation functions
2. Pass format-specific context through the `custom_context` field
3. Register format-specific validation handlers

## Best Practices

1. **Validate early**: Catch issues before they cause problems
2. **Be specific**: Use detailed error messages that clearly indicate the problem
3. **Be complete**: Check all parameters and conditions
4. **Be efficient**: Only validate what's necessary for the current operation
5. **Be informative**: Use appropriate severity levels and check types
6. **Be flexible**: Configure strictness based on the context
7. **Report clearly**: Always log validation failures with appropriate context

## Conclusion

The I/O Validation Framework provides a robust mechanism for ensuring data consistency and preventing errors in SAGE's I/O operations. By catching issues early and providing clear feedback, it helps maintain the scientific integrity of SAGE's outputs while improving the development experience.
