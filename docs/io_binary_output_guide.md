# Binary Output Handler Guide

## Overview

The Binary Output Handler is part of the SAGE I/O abstraction layer, providing a clean interface for writing galaxy data in binary format. It implements the I/O interface defined in Phase 3.1 and supports extended properties developed in Phase 3.2.

## Key Features

- **I/O Interface Compliance**: Implements all required functions from the I/O interface
- **Extended Property Support**: Integrates with the property serialization system
- **Cross-Platform Compatibility**: Proper endianness handling for binary data
- **Resource Management**: Careful tracking and cleanup of file handles and memory
- **Error Handling**: Comprehensive error checking and standardized error codes

## Usage

### Basic Usage

```c
// Initialize I/O system (if not already done)
io_init();

// Get binary output handler
struct io_interface *handler = io_get_binary_output_handler();

// Initialize handler with parameters
void *format_data = NULL;
handler->initialize("output_filename", params, &format_data);

// Write galaxies
handler->write_galaxies(galaxies, num_galaxies, save_info, format_data);

// Clean up
handler->cleanup(format_data);
```

### Advanced Usage with Extended Properties

Extended properties are automatically supported if galaxy extensions are present in the registry:

```c
// Register galaxy extensions (in module initialization)
galaxy_property_t cooling_radius_prop = {
    .name = "cooling_radius",
    .size = sizeof(float),
    .module_id = module_id,
    .extension_id = prop_idx,
    .type = PROPERTY_TYPE_FLOAT,
    .flags = PROPERTY_FLAG_SERIALIZE,
    .serialize = serialize_float,
    .deserialize = deserialize_float,
    .description = "Current cooling radius",
    .units = "kpc"
};
register_galaxy_property(&cooling_radius_prop);

// Set property value in galaxy
float *cooling_radius = galaxy_extension_get_data(galaxy, cooling_radius_prop_id);
if (cooling_radius) {
    *cooling_radius = calculated_value;
}

// These properties will be automatically serialized when writing galaxies
io_get_binary_output_handler()->write_galaxies(galaxies, num_galaxies, save_info, format_data);
```

## File Format

The binary output format with extended properties follows this structure:

1. **Header Section**:
   - Number of forests (int32_t)
   - Total number of galaxies (int32_t)
   - Number of galaxies per forest (int32_t[])
   - Extended property info (if properties are present)

2. **Galaxy Data Section**:
   - Each galaxy is stored as a GALAXY_OUTPUT structure
   - Extended property data follows each galaxy (if properties are present)

3. **Extended Property Section**:
   - Magic marker: "EXTP" (0x45585450)
   - Format version (int)
   - Property metadata with type, size, and attributes

## Integration with Existing Code

The Binary Output Handler is designed to integrate with existing SAGE code:

1. In the Makefile, add io_binary_output.c to the IO_SRC list
2. Include io_binary_output.h in source files that need it
3. Use the handler through the I/O interface system

## Error Handling

The handler uses standardized error codes from the I/O interface:

- `IO_ERROR_NONE`: Operation succeeded
- `IO_ERROR_FILE_NOT_FOUND`: File could not be opened or accessed
- `IO_ERROR_FORMAT_ERROR`: Format-specific issue detected
- `IO_ERROR_MEMORY_ALLOCATION`: Failed to allocate memory
- `IO_ERROR_VALIDATION_FAILED`: Input validation failed
- `IO_ERROR_UNKNOWN`: Unspecified error

Errors can be retrieved using:
```c
int error_code = io_get_last_error();
const char *error_msg = io_get_error_message();
```

## Implementation Details

The Binary Output Handler is implemented in two files:

- `io_binary_output.h`: Header with interface definitions
- `io_binary_output.c`: Implementation of the I/O interface functions

The handler is registered with the I/O interface system during initialization and can be retrieved using `io_get_binary_output_handler()`.

## Cross-Platform Considerations

Binary data is always stored in network byte order (big-endian) for consistency. The handler automatically detects the host system's endianness and performs byte swapping as needed.

## Future Enhancements

Future versions may include:

- Support for incremental writing (appending to existing files)
- Compression for extended property data
- Performance optimizations for large datasets