# Property-Extension Integration Guide

## Overview

This guide explains how the Property System and Extension System in SAGE interact with each other, providing a comprehensive understanding of their integration points and usage patterns.

## Key Integration Points

The Property System and Extension System interact at several key points:

```
┌─────────────────────────┐      ┌────────────────────────┐
│ Property System         │      │ Extension System       │
│ - properties.yaml       │─────▶│ - galaxy_property_t    │
│ - GALAXY_PROP_* macros  │      │ - extension_data array │
│ - generic accessors     │◀─────│ - extension_flags      │
└─────────────────────────┘      └────────────────────────┘
             │                              │
             ▼                              ▼
┌─────────────────────────┐      ┌────────────────────────┐
│ standard_properties.c   │      │ I/O Serialization      │
│ - property registration │─────▶│ - read/write functions │
│ - property/extension    │      │ - format handlers      │
│   ID mapping            │      │                        │
└─────────────────────────┘      └────────────────────────┘
```

### 1. Registration Process

During SAGE initialization, standard properties from `properties.yaml` are registered with the extension system:

```c
// In core_init.c
status = register_standard_properties();
if (status != 0) {
    LOG_ERROR("Failed to register standard properties");
    return -1;
}
```

The `register_standard_properties()` function in `standard_properties.c` creates and registers a `galaxy_property_t` structure for each property defined in `properties.yaml`:

```c
int register_standard_properties(void)
{
    // Loop through all properties
    for (property_id_t id = 0; id < PROP_COUNT; id++) {
        const property_meta_t *meta = &PROPERTY_META[id];
        
        // Create property definition
        galaxy_property_t property;
        memset(&property, 0, sizeof(property));
        strncpy(property.name, get_property_name(id), sizeof(property.name) - 1);
        
        // Set other property attributes based on metadata
        // ...
        
        // Register with extension system
        int extension_id = galaxy_extension_register(&property);
        
        // Store mapping
        standard_property_to_extension_id[id] = extension_id;
    }
    
    return 0;
}
```

### 2. ID Mapping

A critical integration point is the mapping between property IDs and extension IDs:

```c
// In standard_properties.c
int standard_property_to_extension_id[PROP_COUNT];

// Get extension ID for a standard property
int get_extension_id_for_standard_property(property_id_t property_id)
{
    if (property_id < 0 || property_id >= PROP_COUNT) {
        return -1;
    }
    
    return standard_property_to_extension_id[property_id];
}
```

This mapping allows code to convert between the two ID systems when needed.

### 3. Memory Layout and Access

The Property System and Extension System represent data differently:

- **Property System**: Uses a structured `galaxy_properties_t` for typed field access
- **Extension System**: Uses a flat array of void pointers for extensibility

When a property is accessed, the correct method depends on whether it's a core or physics property:

```c
// Core property (using macro/direct access)
int type = GALAXY_PROP_Type(galaxy);

// Physics property (using generic accessor)
float stellar_mass = get_float_property(galaxy, PROP_StellarMass, 0.0f);
```

Behind the scenes, the generic accessor functions use type-safe dispatcher functions that map property IDs to the correct fields in the `galaxy_properties_t` structure.

### 4. Serialization Integration

For I/O operations, properties are serialized using the extension system's serialization functions:

```c
// In io_property_serialization.c
int serialize_property(const struct GALAXY *galaxy, property_id_t prop_id, void *buffer)
{
    // Get extension ID for this property
    int ext_id = get_extension_id_for_standard_property(prop_id);
    if (ext_id < 0) return -1;
    
    // Get property metadata
    const galaxy_property_t *prop_meta = galaxy_extension_find_property_by_id(ext_id);
    if (!prop_meta) return -2;
    
    // Get extension data
    void *ext_data = galaxy_extension_get_data(galaxy, ext_id);
    if (!ext_data) return -3;
    
    // Use property's serialization function
    prop_meta->serialize(ext_data, buffer, 1);
    
    return 0;
}
```

## Evolution of the Integration

The integration between the Property System and Extension System has evolved over time:

### Original Implementation (Pre-Phase 5)

In the original implementation, properties were directly mapped to extensions:

1. Properties were stored as direct fields in the `GALAXY` structure
2. Extension data pointers directly referenced these fields
3. A now-removed test was designed to validate this direct mapping

```c
/* Map StellarMass (float) */
ext_id = get_extension_id_for_standard_property(PROP_StellarMass);
if (ext_id >= 0) {
    galaxy->extension_data[ext_id] = &(galaxy->StellarMass);
    galaxy->extension_flags |= (1ULL << ext_id);
}
```

### Current Implementation (Post-Phase 5.2)

With the implementation of the Properties Module architecture:

1. Properties are now stored in the `galaxy_properties_t` structure
2. Direct fields for physics properties have been removed from the `GALAXY` structure
3. Extension data pointers reference fields in the `galaxy_properties_t` structure
4. Property access is now guided by the core-physics separation principle

The registration of standard properties with the extension system still occurs, but the mechanism for accessing properties has changed to respect the core-physics separation.

## When to Use Each System

The choice between using the Property System or Extension System depends on the context:

### Use the Property System When:

1. **Accessing Standard Properties**: For properties defined in `properties.yaml`, use the appropriate access pattern:
   - For core properties (`is_core: true`): Use `GALAXY_PROP_*` macros
   - For physics properties (`is_core: false`): Use generic accessors like `get_float_property()`

2. **Implementing Physics Modules**: Physics modules should use the Property System to ensure compatibility with the core-physics separation principle.

3. **Working with Output Transformers**: When preparing properties for output, use the Property System to ensure proper transformation.

### Use the Extension System When:

1. **Adding Custom Data**: For module-specific data not defined in `properties.yaml`, register a new extension and use the extension system to store and access it.

2. **Implementing Serialization**: When writing custom serialization code, use the extension system's serialization functions.

3. **Debugging Property Registration**: When investigating issues with property registration, examine the extension system's internal state.

## Best Practices for Integration

1. **Respect Core-Physics Separation**: Always follow the access patterns defined by the core-physics separation principle.

2. **Don't Mix Access Patterns**: Choose either the Property System or Extension System based on the context, and be consistent.

3. **Understand the Mapping**: Be aware of the mapping between property IDs and extension IDs, especially when debugging.

4. **Use Appropriate Error Handling**: Check return values and handle errors appropriately for both systems.

## Common Issues and Solutions

### Issue: Extension Data Is NULL

**Symptom**: `galaxy_extension_get_data()` returns NULL for a valid extension ID.

**Causes**:
1. The extension hasn't been initialized
2. The extension flag is not set
3. The extension data hasn't been allocated

**Solution**:
```c
// Check if extension is present
if (!(galaxy->extension_flags & (1ULL << ext_id))) {
    // Extension not present, initialize it
    galaxy->extension_flags |= (1ULL << ext_id);
    galaxy->extension_data[ext_id] = calloc(1, property_size);
}
```

### Issue: Property ID Not Found in Extension Mapping

**Symptom**: `get_extension_id_for_standard_property()` returns -1 for a valid property ID.

**Causes**:
1. The property hasn't been registered with the extension system
2. The property ID is out of range

**Solution**:
```c
// Check property registration
property_id_t prop_id = get_property_id_by_name("PropertyName");
if (prop_id == PROP_COUNT) {
    LOG_ERROR("Property not found: %s", "PropertyName");
    return -1;
}

int ext_id = get_extension_id_for_standard_property(prop_id);
if (ext_id < 0) {
    LOG_ERROR("Property not registered with extension system: %s", "PropertyName");
    return -2;
}
```

## Conclusion

The integration between the Property System and Extension System is a key aspect of SAGE's modular architecture. By understanding how these systems work together and following the appropriate access patterns, developers can maintain code that is both flexible and maintainable while respecting the core-physics separation principle.
