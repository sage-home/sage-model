# Buffer Manager Integration with HDF5 Output

## Overview

This document outlines the plan for integrating the buffer manager component with the HDF5 output handler. This is part of Phase 3.3 (Memory Optimization) of the SAGE refactoring project.

## Current Status

The buffer manager has been implemented and successfully integrated with the binary output handler. The HDF5 output handler structure already includes fields for buffer management, but the actual integration has not been completed.

## Implementation Tasks

### 1. Implement HDF5 Write Callback

Create a HDF5-specific write callback function that will handle the actual HDF5 dataset writing operations:

```c
static int hdf5_write_callback(int fd, const void* buffer, size_t size, off_t offset, void* context);
```

This function will:
- Cast the context to a HDF5-specific context structure
- Determine the current dataset and position from the offset
- Create appropriate memory and file dataspaces
- Use H5Dwrite to write the data to the appropriate dataset

### 2. Modify hdf5_output_initialize

Update the initialization function to:
- Initialize buffer size parameters from `params->runtime`
- Allocate the `io_buffers` array for each snapshot
- Create buffer configurations using `buffer_config_default`
- Initialize buffers with `buffer_create` using the HDF5 callback

### 3. Update hdf5_output_write_galaxies

Modify the write function to:
- Replace direct dataset writing with buffer-based writing
- Handle serialized data for both standard and extended properties
- Track buffer state and position information
- Manage buffer flushing when appropriate

### 4. Implement Buffer Flush Integration

The existing `flush_galaxy_buffer` function should be updated to:
- Use the buffer manager's `buffer_flush` function
- Handle HDF5-specific resource management
- Update dataset dimensions after flushing

### 5. Update hdf5_output_cleanup

Ensure proper cleanup of buffer resources:
- Flush any remaining data in buffers
- Destroy all buffer instances
- Free allocated memory

## Implementation Challenges

### Complex Data Structure

HDF5 uses separate datasets for different galaxy properties, which differs from binary output's sequential write pattern. The implementation will need to:

1. Either use separate buffers for each property dataset, or
2. Serialize data in the buffer and deserialize it in the write callback

The second approach is preferred as it maintains a consistent API with the binary output handler.

### Hyperslab Writing

The write callback needs to handle HDF5's hyperslab writing mechanism. This requires:
- Tracking the current position in each dataset
- Creating proper memory and file dataspaces
- Calculating the correct offset for each write operation

### Extended Properties

The existing serialization system for extended properties should be integrated with the buffer manager to ensure all property data is correctly written to the HDF5 file.

## Testing Strategy

1. Update existing HDF5 output tests to verify buffer integration
2. Create specific tests for buffer flushing and resizing with HDF5
3. Verify that output files match reference outputs with different buffer configurations
4. Test with large datasets to ensure proper handling of buffer resizing

## Performance Considerations

- Choose appropriate initial buffer sizes for HDF5 operations
- Consider the performance impact of serialization/deserialization
- Balance memory usage with write efficiency

## Implementation Timeline

Estimated time for completion: 2-3 days

1. Day 1: Implement write callback and update initialization
2. Day 2: Modify write function and implement flush integration
3. Day 3: Update cleanup, testing, and documentation
