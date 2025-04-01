# Galaxy Extended Property Serialization Guide

## Overview

The Extended Property Serialization system provides a mechanism to save and load module-specific galaxy properties that are added via the Galaxy Extension system. This guide explains how the system works and how to integrate it with I/O handlers.

## Implementation Overview

The system consists of three major components:

1. **Core Serialization Utilities**: Provides format-agnostic functionality for serializing and deserializing galaxy extension properties.
2. **Binary Format Integration**: Extends binary output files to include property metadata and data.
3. **HDF5 Format Integration**: Adds galaxy extension properties to HDF5 files using attributes and datasets.

## Key Components

### Property Serialization Context

```c
struct property_serialization_context {
    int num_properties;                     // Number of properties to serialize
    struct serialized_property_meta *properties; // Property metadata
    size_t total_size_per_galaxy;           // Size of all properties per galaxy
    int *property_id_map;                   // Maps serialized property index to extension_id
    void *buffer;                           // Temporary buffer
    size_t buffer_size;                     // Buffer size
    bool endian_swap;                       // Endianness conversion flag
};
```

### Property Metadata

```c
struct serialized_property_meta {
    char name[MAX_PROPERTY_NAME];           // Property name
    enum galaxy_property_type type;         // Property data type
    size_t size;                            // Size in bytes
    char units[MAX_PROPERTY_UNITS];         // Units (if applicable)
    char description[MAX_PROPERTY_DESCRIPTION]; // Property description
    uint32_t flags;                         // Property flags
    int64_t offset;                         // Offset within data block
};
```

## File Format Details

### Binary Format Layout

Binary output files with extended properties have the following structure:

1. **Standard Header Section**
   - Standard galaxy output header
   - Offset to the extended property section

2. **Galaxy Data Section**
   - Standard galaxy data (unchanged)

3. **Extended Property Section**
   - Magic identifier: "EXTP" (0x45585450)
   - Format version (int)
   - Number of properties (int)
   - Total size per galaxy (size_t)
   - For each property:
     - Property name, type, size, units, description, flags, and offset

4. **Extended Property Data Section**
   - For each galaxy, a block of data containing all extended properties
   - Properties stored in the order defined in the metadata

### HDF5 Format Layout

HDF5 output files with extended properties have these additional components:

1. **Extended Properties Group**
   - Top-level group named "ExtendedProperties"

2. **Property Metadata**
   - Group attributes describing version and property count
   - For each property, attributes describing type, units, etc.

3. **Property Datasets**
   - For each property, a dataset with dimension matching the number of galaxies
   - Dataset named after the property
   - Type and attributes matching the property definition

## Integration with I/O Handlers

### Binary Output Integration

To integrate with binary output, the implementation should:

1. Write standard galaxy data first
2. Create a property serialization context and add properties
3. Write the extended property metadata section
4. For each galaxy, serialize its extended properties
5. Update the header with the offset to the extended property section

### HDF5 Output Integration

For HDF5 output, the implementation should:

1. Create the standard galaxy datasets
2. Create an "ExtendedProperties" group
3. For each property, create a dataset with appropriate type
4. For each galaxy, extract and store its property values
5. Add metadata as attributes

## Usage Example

```c
// During output initialization
struct property_serialization_context ctx;
property_serialization_init(&ctx, SERIALIZE_EXPLICIT);
property_serialization_add_properties(&ctx);

// When writing header
int64_t header_size = property_serialization_create_header(&ctx, header_buffer, buffer_size);
fwrite(header_buffer, 1, header_size, file);

// For each galaxy
property_serialize_galaxy(&ctx, galaxy, property_buffer);
fwrite(property_buffer, 1, property_serialization_data_size(&ctx), file);

// Cleanup when done
property_serialization_cleanup(&ctx);
```

## Type-Specific Serialization

The system provides built-in serializers for common data types:

- `serialize_int32` / `deserialize_int32`
- `serialize_int64` / `deserialize_int64`
- `serialize_uint32` / `deserialize_uint32`
- `serialize_uint64` / `deserialize_uint64`
- `serialize_float` / `deserialize_float`
- `serialize_double` / `deserialize_double`
- `serialize_bool` / `deserialize_bool`

Custom serializers can be added by defining functions with this signature:

```c
void (*serialize_fn)(const void *src, void *dest, int count);
void (*deserialize_fn)(const void *src, void *dest, int count);
```

## Endianness Handling

The system automatically handles endianness differences:

- Serializes data in network byte order (big-endian)
- Automatically detects file endianness during reading
- Applies conversions as needed based on the local system's endianness
- Implements magic number detection for robust format checking

## Memory Management

The serialization system carefully manages memory:

- Aligns properties to appropriate boundaries for efficient access
- Allocates buffer space dynamically with geometric growth
- Properly cleans up resources when serialization is complete
- Handles allocation failures gracefully

## Property Filtering

You can control which properties are serialized using filter flags:

- `SERIALIZE_ALL`: Include all registered properties
- `SERIALIZE_EXPLICIT`: Only include properties with the `PROPERTY_FLAG_SERIALIZE` flag
- `SERIALIZE_EXCLUDE_DERIVED`: Skip properties with the `PROPERTY_FLAG_DERIVED` flag

## Error Handling

The system includes robust error handling:

- Validates parameters and contexts before operations
- Reports errors for invalid or inconsistent metadata
- Handles missing properties during deserialization
- Manages version compatibility between files

## Future Extensions

The system is designed to be extended in the future:

- Support for complex data types (structs, arrays)
- Additional serialization formats
- Schema versioning and migration
- Selective property loading