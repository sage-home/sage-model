# I/O Interface Abstraction Guide

## Overview

This document provides an overview of the I/O interface abstraction system implemented in Phase 3.1. The system provides a unified interface for reading merger tree data and writing galaxy data, abstracting away format-specific details.

## Key Components

### Core Interface (`io_interface.h`, `io_interface.c`)

The core I/O interface defines:
- A standard interface structure with metadata, operations, and capabilities
- Format identification and capability flags
- A registry for format-specific handlers
- Error handling utilities
- Format detection functions

### HDF5 Resource Tracking (`io_hdf5_utils.h`, `io_hdf5_utils.c`)

The HDF5 resource tracking system:
- Tracks all types of HDF5 handles to prevent resource leaks
- Provides safe handle closure utilities
- Tracks handle creation locations for debugging
- Ensures handles are closed in the correct order

### Galaxy Output Preparation (`io_galaxy_output.h`, `io_galaxy_output.c`)

The galaxy output preparation utilities:
- Extract common logic for preparing galaxies for output
- Generate unique galaxy IDs
- Handle relationships between galaxies
- Ensure consistent output structure

## Handler Implementation Guidelines

When implementing a format-specific handler:

1. Define a handler structure implementing the `io_interface` struct
2. Set appropriate capability flags
3. Implement all required functions
4. Register the handler during initialization
5. Use the HDF5 resource tracking system if applicable
6. Provide detailed error messages for failure cases

## Capabilities

Handlers should set the following capability flags based on supported features:

- `IO_CAP_RANDOM_ACCESS`: Supports random access to forests
- `IO_CAP_MULTI_FILE`: Supports multi-file datasets
- `IO_CAP_METADATA_QUERY`: Supports metadata queries
- `IO_CAP_PARALLEL_READ`: Supports parallel reading
- `IO_CAP_COMPRESSION`: Supports compression
- `IO_CAP_EXTENDED_PROPS`: Supports extended properties
- `IO_CAP_APPEND`: Supports append operations
- `IO_CAP_CHUNKED_WRITE`: Supports chunked writing
- `IO_CAP_METADATA_ATTRS`: Supports metadata attributes

## Error Handling

All I/O operations should use the standard error handling system:

```c
if (error_condition) {
    io_set_error(IO_ERROR_CODE, "Detailed error message");
    return error_code;
}
```

## Implementation Progress

### Completed
- Core I/O interface structure
- Handler registry system
- Error handling utilities
- HDF5 resource tracking system
- Galaxy output preparation utilities
- Test framework for the I/O interface

### In Progress
- Updating existing I/O code to use the new interface

### Pending
- Implementation of format-specific handlers:
  - LHalo Binary handler
  - LHalo HDF5 handler
  - ConsistentTrees ASCII handler
  - ConsistentTrees HDF5 handler
  - Gadget4 HDF5 handler
  - Genesis HDF5 handler
  - Binary Output handler
  - HDF5 Output handler
- Integration with core_io_tree.c and core_save.c

## Testing

The I/O interface includes a test suite in `tests/test_io_interface.c`. This suite tests:
- Handler registration and lookup
- Error handling
- Format ID mapping
- HDF5 resource tracking

To run the tests:
```
make test_io_interface
./tests/test_io_interface
```

## Future Enhancements

- Endianness handling for binary formats
- Memory optimization using geometric growth for arrays
- Configurable buffer sizes for I/O operations
- File format detection based on content rather than extension

## References

- SAGE Refactoring Plan: Phase 3.1 - I/O Interface Abstraction
- HDF5 Documentation: https://portal.hdfgroup.org/display/HDF5/HDF5