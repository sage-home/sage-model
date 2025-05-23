# Extension System Guide

## Overview

The Galaxy Extension System provides a flexible mechanism for modules to store and access custom data that isn't part of the standard property set. It enables runtime extension of the `GALAXY` structure without requiring recompilation, supporting the modular architecture of SAGE.

## Architecture

The extension system is built around three core components:

```
┌───────────────────────┐     ┌──────────────────────┐
│ GALAXY Structure      │     │ Extension Registry   │
│ - extension_data[]    │◄────┤ - property metadata  │
│ - num_extensions      │     │ - serialization      │
│ - extension_flags     │     │ - type information   │
└───────────┬───────────┘     └──────────────────────┘
            │                            ▲
            ▼                            │
┌───────────────────────┐     ┌──────────────────────┐
│ Module-Specific Data  │     │ Registration Process │
│ - custom properties   │─────┤ - register_property  │
│ - private state       │     │ - extension IDs      │
└───────────────────────┘     └──────────────────────┘
```

### Core Components

1. **GALAXY Extension Fields**:
   ```c
   struct GALAXY {
       // ... other fields ...
       
       // Extension mechanism
       void **extension_data;    // Array of pointers to module-specific data
       int num_extensions;       // Number of registered extensions
       uint64_t extension_flags; // Bitmap to track which extensions are in use
   };
   ```

2. **Extension Registry**:
   - Maintains metadata about each registered property
   - Provides type-safe serialization functions
   - Maps extension IDs to property metadata

3. **Galaxy Property Structure**:
   ```c
   typedef struct {
       char name[32];           // Property name
       size_t size;             // Size in bytes
       int module_id;           // Which module owns this
       // Serialization functions
       void (*serialize)(void *src, void *dest, int count);
       void (*deserialize)(void *src, void *dest, int count);
       // Metadata
       char description[128];
       char units[32];
       // Flags
       uint32_t flags;          // PROPERTY_FLAG_* values
       enum galaxy_property_type type;
   } galaxy_property_t;
   ```

## Extension System Lifecycle

### Initialization

The extension system is initialized during SAGE startup:

```c
// In core_init.c
int initialize_all_systems(struct params *params) {
    // Initialize other systems...
    
    // Initialize extension system
    status = galaxy_extension_system_initialize();
    if (status != 0) {
        LOG_ERROR("Failed to initialize galaxy extension system");
        return -1;
    }
    
    // Continue initialization...
}
```

### Registration

Properties are registered with the extension system to obtain unique extension IDs:

```c
// Example property registration
galaxy_property_t property;
memset(&property, 0, sizeof(property));
strncpy(property.name, "CustomMass", sizeof(property.name) - 1);
property.size = sizeof(float);
property.module_id = MODULE_ID_CUSTOM;
property.serialize = serialize_float;
property.deserialize = deserialize_float;
property.type = PROPERTY_TYPE_FLOAT;

// Register with extension system
int extension_id = galaxy_extension_register(&property);
```

### Usage

Once registered, extensions can be accessed and modified:

```c
// Set extension data
float *custom_mass = malloc(sizeof(float));
*custom_mass = 1.0e10;
galaxy_extension_set_data(galaxy, extension_id, custom_mass);

// Get extension data
float *mass_ptr = galaxy_extension_get_data(galaxy, extension_id);
```

### Cleanup

Extension data must be freed when no longer needed:

```c
// Cleanup a galaxy's extension data
galaxy_extension_cleanup(galaxy);
```

## Standard Properties and Extensions

The standard properties defined in `properties.yaml` are automatically registered with the extension system during initialization. This creates a mapping between property IDs and extension IDs.

The `standard_properties.c/h` files handle this registration:

```c
// In standard_properties.c
int register_standard_properties(void)
{
    // Initialize mapping to -1 (not registered)
    for (int i = 0; i < PROP_COUNT; i++) {
        standard_property_to_extension_id[i] = -1;
    }
    
    // Register all properties based on their types
    for (property_id_t id = 0; id < PROP_COUNT; id++) {
        const property_meta_t *meta = &PROPERTY_META[id];
        // Register property with extension system
        // ...
        // Store mapping
        standard_property_to_extension_id[id] = extension_id;
    }
    
    return 0;
}
```

## Extension System vs. Property System

While closely related, the extension system and property system serve different purposes:

1. **Property System**:
   - Provides a centralized definition of all galaxy properties
   - Generates type-safe accessors (GALAXY_PROP_* macros)
   - Enforces core-physics separation

2. **Extension System**:
   - Provides runtime extensibility of the GALAXY structure
   - Handles custom data types not in the standard property set
   - Supports modular architecture with plug-in physics components

The property system is the recommended way to access standard properties, while the extension system is primarily used for module-specific custom data.

## Implementation Details

### Extension Flags

The `extension_flags` field in the GALAXY structure is a bitmap that tracks which extensions are in use:

```c
// Check if extension is present
bool has_extension = (galaxy->extension_flags & (1ULL << extension_id)) != 0;

// Set extension flag
galaxy->extension_flags |= (1ULL << extension_id);

// Clear extension flag
galaxy->extension_flags &= ~(1ULL << extension_id);
```

### Type Safety

The extension system provides type-safe serialization and deserialization functions for each supported data type:

```c
// Type-safe serialization for float data
static void serialize_float(void *src, void *dest, int count) {
    memcpy(dest, src, count * sizeof(float));
}

// Type-safe deserialization for float data
static void deserialize_float(void *src, void *dest, int count) {
    memcpy(dest, src, count * sizeof(float));
}
```

### Extension Allocation

When a galaxy is created, the extension system allocates memory for all registered extensions:

```c
int galaxy_extension_initialize(struct GALAXY *galaxy) {
    int num_extensions = get_total_registered_extensions();
    
    galaxy->extension_data = calloc(num_extensions, sizeof(void*));
    galaxy->num_extensions = num_extensions;
    galaxy->extension_flags = 0; // No extensions in use initially
    
    return 0;
}
```

## Best Practices

1. **Use Property System When Possible**: For standard properties defined in `properties.yaml`, use the property system (GALAXY_PROP_* macros for core properties, generic accessors for physics properties).

2. **Use Extensions for Custom Data**: Use the extension system only for module-specific data that isn't part of the standard property set.

3. **Manage Memory Carefully**: Always free extension data when no longer needed to prevent memory leaks.

4. **Respect Type Safety**: Use the appropriate serialization functions for each data type.

5. **Check Extension Flags**: Always check if an extension is present before accessing it.

## Common Pitfalls

1. **Direct Pointer Manipulation**: Avoid directly manipulating extension data pointers, as this can lead to memory leaks or corruption.

2. **Mismatched Types**: Ensure that the data type used when accessing an extension matches the registered type.

3. **Forgetting to Free**: Always call `galaxy_extension_cleanup()` when a galaxy is no longer needed.

4. **Conflicting Extension IDs**: Ensure that each module uses unique extension IDs to avoid conflicts.

## Legacy Considerations

In earlier versions of SAGE, there was a tighter coupling between the property system and extension system, with direct fields in the GALAXY structure mapped to extensions. This approach has been superseded by the more modular property-based architecture.

The core-physics separation principle now guides how properties are accessed, with the extension system playing a supporting role for custom module data rather than being the primary means of accessing standard properties.

## Conclusion

The Galaxy Extension System provides a powerful mechanism for runtime extensibility in SAGE, supporting its modular architecture. By understanding its relationship with the property system and following best practices, developers can leverage this flexibility while maintaining code clarity and preventing memory issues.
