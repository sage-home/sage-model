# I/O Validation Guide

## Overview

The I/O validation system provides a comprehensive framework for ensuring data consistency, format compatibility, and error detection throughout SAGE's I/O operations. This guide explains how to use the validation system and extend it for new validation requirements.

## Components

### Core Validation Framework (Step 9.1)

- **Context Management**: `validation_context` structure and related functions
- **Result Collection**: Functions for adding and retrieving validation results
- **Strictness Levels**: RELAXED, NORMAL, and STRICT validation modes
- **Reporting**: Standardized reporting with appropriate log levels

### Galaxy Data Validation (Step 9.2)

- **Data Validation**: Check for NaN, infinity, and other invalid values
- **Reference Validation**: Verify galaxy references and indices
- **Consistency Validation**: Verify logical consistency between fields

### Format-Specific Validation (Step 9.3)

- **Capability Validation**: Verify handler supports required capabilities
- **Binary Format Validation**: Validate binary format compatibility
- **HDF5 Format Validation**: Validate HDF5 format compatibility

### Extended Property Validation (Step 9.4)

- **Property Type Validation**: Verify property types are valid and supported
- **Serialization Validation**: Check for required serialization functions
- **Name Uniqueness**: Ensure property names are unique across modules
- **Format Compatibility**: Verify properties work with selected format

### I/O Interface Integration (Step 9.5)

- **Initialization Validation**: Validate before file creation
- **Galaxy Data Validation**: Validate before writing galaxies
- **Finalization Validation**: Validate before closing files

## Using the Validation System

### 1. Creating a Validation Context

```c
struct validation_context ctx;
validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
```

### 2. Performing Validation Checks

```c
// Check a pointer
VALIDATE_NOT_NULL(&ctx, ptr, "component_name", "Pointer %s should not be NULL", ptr_name);

// Check a numerical value
VALIDATE_FINITE(&ctx, value, "component_name", "Value %s should be finite", value_name);

// Check an array index
VALIDATE_BOUNDS(&ctx, index, 0, size, "component_name", "Index %d out of bounds", index);

// Check a condition
VALIDATE_CONDITION(&ctx, condition, "component_name", "Condition failed: %s", condition_desc);
```

### 3. Checking Validation Results

```c
// Check if validation passed
if (validation_has_errors(&ctx)) {
    validation_report(&ctx);
    validation_cleanup(&ctx);
    return error_code;
}
```

### 4. Property Validation

```c
// Validate property type
VALIDATE_PROPERTY_TYPE(&ctx, property->type, "component_name", property->name);

// Validate property serialization
VALIDATE_PROPERTY_SERIALIZATION(&ctx, property, "component_name");

// Validate property uniqueness
VALIDATE_PROPERTY_UNIQUENESS(&ctx, property, "component_name");

// Validate format compatibility
VALIDATE_BINARY_PROPERTY_COMPATIBILITY(&ctx, property, "component_name");
VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, property, "component_name");
```

### 5. Format Validation

```c
// Define required capabilities
enum io_capabilities required_caps[] = {
    IO_CAP_WRITE_GALAXIES,
    IO_CAP_CREATE_SNAPSHOT
};

// Validate format capabilities
VALIDATE_FORMAT_CAPABILITIES(&ctx, handler, required_caps, 
                           sizeof(required_caps)/sizeof(required_caps[0]),
                           "component_name", "operation_name");

// Validate format-specific compatibility
VALIDATE_BINARY_COMPATIBILITY(&ctx, handler, "component_name");
VALIDATE_HDF5_COMPATIBILITY(&ctx, handler, "component_name");
```

## Extending the Validation System

### Adding New Validation Functions

1. Add function declaration to `io_validation.h`
2. Implement function in `io_validation.c`
3. Add convenience macro to `io_validation.h`
4. Add test cases to `test_io_validation.c`

### Validation Function Template

```c
int validation_check_something(struct validation_context *ctx,
                             /* Parameters */
                             const char *component,
                             const char *file,
                             int line) {
    if (ctx == NULL) {
        return -1;
    }
    
    // Perform validation checks
    if (/* check fails */) {
        VALIDATION_ERROR(ctx, /* error code */, /* check type */,
                       component, /* error message */);
        return -1;
    }
    
    // Optional warning
    if (/* warning condition */) {
        VALIDATION_WARN(ctx, /* error code */, /* check type */,
                      component, /* warning message */);
    }
    
    return 0;
}
```

## Validation Severities

The validation system supports three severity levels:

- **INFO**: Informational messages, always displayed but never cause validation failure
- **WARNING**: Issues that might need attention but don't prevent operation
- **ERROR**: Issues that should be addressed and may prevent operation
- **FATAL**: Critical issues that always prevent operation

## Strictness Levels

The validation system supports three strictness levels:

- **RELAXED**: Skip non-critical checks, warnings ignored
- **NORMAL**: Perform all checks, warnings logged but don't cause failure
- **STRICT**: Perform all checks, warnings treated as errors

## Error Handling

The validation system integrates with SAGE's error handling through:

- `map_io_error_to_sage_error()`: Maps validation errors to SAGE error codes
- `log_io_error()`: Logs I/O errors with appropriate context

## Best Practices

1. **Initialize validation context at the beginning of functions**
2. **Clean up validation context before returning**
3. **Use appropriate severity for each issue**
4. **Provide clear, specific error messages**
5. **Check validation results before proceeding with operations**
6. **Use convenience macros for readability**
7. **Report validation results when errors are found**

## Conclusion

The I/O validation system provides a robust framework for ensuring data integrity, format compatibility, and proper error handling throughout SAGE's I/O operations. By using this system consistently, developers can catch issues early, provide clear error messages, and maintain high data quality.