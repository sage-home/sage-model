# Type-Safe Property Access System

## Overview

This document describes the implementation of a type-safe property access system for SAGE using auto-generated dispatcher functions. This approach replaces the previously flawed direct memory manipulation approach that caused type safety issues and incorrect property access.

## Problem Statement

The original implementation used a naive macro-based approach to access struct members by property ID:

```c
#define GALAXY_PROP_BY_ID(g, pid, type) (((type*)(g)->properties)[(pid)])
```

This macro incorrectly treated the struct as an array of elements of type `type`, which doesn't match how C structs are actually laid out in memory. The implementation had several critical flaws:

1. It assumed properties are stored in contiguous memory at fixed offsets
2. It used property IDs as array indices, which is incompatible with the C struct memory layout
3. It lacked type safety, relying on the caller to specify the correct type
4. It wouldn't work with mixed-type struct fields or variable-sized array members

## Solution: Type-Safe Dispatcher Functions

The new implementation replaces the flawed macro with auto-generated type-specific dispatcher functions. Each dispatcher uses a switch statement to map from property IDs to the corresponding struct fields.

### Key Components

1. **Auto-Generated Dispatchers**: The code generator (`generate_property_headers.py`) now generates type-specific dispatcher functions:
   - `get_generated_float()` - For accessing float properties
   - `get_generated_int32()` - For accessing int32 properties
   - `get_generated_double()` - For accessing double properties
   - `get_generated_float_array_element()` - For accessing float array elements
   - `get_generated_int32_array_element()` - For accessing int32 array elements
   - `get_generated_double_array_element()` - For accessing double array elements
   - `get_generated_array_size()` - For retrieving array sizes

2. **Modified Property Utility Functions**: The property utility functions (`get_float_property()`, etc.) now use these dispatchers instead of the flawed macro.

3. **Removed GALAXY_PROP_BY_ID**: The problematic macro has been removed entirely.

## Implementation Details

### Dispatcher Function Example

```c
float get_generated_float(const galaxy_properties_t *props, property_id_t prop_id, float default_value) {
    if (!props) return default_value;
    switch (prop_id) {
        case PROP_Mvir: return props->Mvir;
        case PROP_Rvir: return props->Rvir;
        // Other float properties...
        default: return default_value;
    }
}
```

### Property Utility Function Example

```c
float get_float_property(const struct GALAXY *galaxy, property_id_t prop_id, float default_value) {
    sage_assert(galaxy != NULL, "Galaxy pointer cannot be NULL in get_float_property.");
    sage_assert(galaxy->properties != NULL, "Galaxy properties pointer cannot be NULL in get_float_property.");

    if (prop_id < 0 || prop_id >= (property_id_t)MAX_GALAXY_PROPERTIES) {
        LOG_ERROR("Invalid property ID %d requested for galaxy %lld.", prop_id, GALAXY_PROP_GalaxyNr(galaxy));
        return default_value;
    }
    
    // Use the generated dispatcher instead of GALAXY_PROP_BY_ID
    return get_generated_float(galaxy->properties, prop_id, default_value);
}
```

## Benefits

1. **Type Safety**: Each dispatcher function is specifically typed to handle only properties of the correct type.

2. **Correct Memory Access**: The switch statement directly accesses struct fields by name, following the correct C struct memory layout.

3. **Simplified Error Handling**: Property ID validation and error handling are centralized in the utility functions.

4. **Better Performance**: Using direct struct field access is more efficient than attempting pointer arithmetic on struct members.

5. **Maintainability**: The code generator automates the creation of dispatcher functions, ensuring they stay in sync with property definitions.

## Verification

The implementation has been verified with a standalone test (`test_dispatcher_access.c`) that confirms:

1. Direct property access and generic function-based access return identical values
2. This works for both scalar properties and array properties
3. Array size retrieval works correctly

## Current Implementation Status (June 2025)

### ✅ Production Implementation

The type-safe property dispatcher system is now fully implemented and in production use:

#### Function Simplification Achievement
- **init_galaxy()**: Now uses auto-generated `initialize_all_properties()` function instead of manual field setting
- **deep_copy_galaxy()**: Simplified to use `copy_galaxy_properties()` for all property handling  
- **Property-First Architecture**: Functions trust the auto-generated property system for data management
- **Eliminated Manual Synchronization**: Removed 150+ lines of redundant property initialization code

#### Core-Physics Separation Compliance
- **Core Properties**: Direct access via GALAXY_PROP_* macros for `is_core: true` properties
- **Physics Properties**: Generic accessors with availability checking for all physics properties
- **MergTime Migration**: Successfully moved from core to physics property using generic access patterns

## Future Enhancements

In the future, we could further enhance this system by:

1. Adding type checking at compile-time using C11's _Generic selectors
2. Implementing property-specific validation logic in each dispatcher case
3. Adding optimized bulk property access for common patterns
4. Creating specialized dispatchers for frequently accessed property groups
