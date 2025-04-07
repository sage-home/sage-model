# Buffer Manager Guide

## Overview

The Buffer Manager provides an efficient way to manage I/O operations in SAGE, reducing the number of system calls and improving performance. It's part of Phase 3.3 (Memory Optimization) of the SAGE refactoring plan.

Key features include:
- Configurable buffer sizes with dynamic resizing
- Callback-based architecture for flexibility
- Support for both read and write operations
- Automatic buffer growth based on usage patterns
- Integration with the I/O interface system

## Configuration

The Buffer Manager can be configured through runtime parameters in the SAGE parameter file:

```
BufferSizeInitialMB  4   # Initial buffer size in MB (default: 4)
BufferSizeMinMB      1   # Minimum buffer size in MB (default: 1)
BufferSizeMaxMB      32  # Maximum buffer size in MB (default: 32)
```

These parameters are optional. If not specified, the default values shown above will be used.

## API Reference

### Data Structures

#### Buffer Configuration

```c
struct io_buffer_config {
    size_t initial_size;           // Initial buffer size in bytes
    size_t min_size;               // Minimum buffer size in bytes
    size_t max_size;               // Maximum buffer size in bytes
    float growth_factor;           // Growth factor for auto-resize
    bool auto_resize;              // Enable/disable automatic resizing
    int resize_threshold_percent;  // Threshold percentage to trigger resize
};
```

#### Buffer Object

The `struct io_buffer` is opaque to users. You interact with it through the provided API functions.

### Callback Functions

```c
typedef int (*io_write_fn)(int fd, const void* buffer, size_t size, off_t offset, void* context);
typedef ssize_t (*io_read_fn)(int fd, void* buffer, size_t size, off_t offset, void* context);
```

These function pointers define the interface for actual I/O operations. The Buffer Manager calls these functions when it needs to perform actual disk operations.

### Functions

#### Creating and Destroying Buffers

```c
struct io_buffer* buffer_create(
    struct io_buffer_config* config, 
    int fd, 
    off_t initial_offset, 
    io_write_fn write_callback, 
    void* write_context
);

int buffer_destroy(struct io_buffer* buffer);
```

#### Writing and Reading Data

```c
int buffer_write(struct io_buffer* buffer, const void* data, size_t size);

ssize_t buffer_read(
    struct io_buffer* buffer,
    io_read_fn read_callback,
    void* read_context,
    void* dest, 
    size_t size
);
```

#### Buffer Management

```c
int buffer_flush(struct io_buffer* buffer);
int buffer_resize(struct io_buffer* buffer, size_t new_size);
size_t buffer_get_capacity(struct io_buffer* buffer);
size_t buffer_get_used(struct io_buffer* buffer);
```

#### Helper Functions

```c
struct io_buffer_config buffer_config_default(
    int initial_size_mb, 
    int min_size_mb, 
    int max_size_mb
);
```

## Integration Example

Here's an example of how to integrate the Buffer Manager with the I/O interface:

```c
#include "io_buffer_manager.h"
#include "core_utils.h"

// Custom write callback for binary output
static int binary_write_callback(int fd, const void* buffer, size_t size, 
                              off_t offset, void* context) {
    return mypwrite(fd, buffer, size, offset);
}

// In your initialization code:
struct io_buffer_config config = buffer_config_default(
    params->runtime.BufferSizeInitialMB,
    params->runtime.BufferSizeMinMB,
    params->runtime.BufferSizeMaxMB
);

struct io_buffer* buffer = buffer_create(
    &config, 
    file_descriptor, 
    0, 
    binary_write_callback, 
    NULL
);

// Using the buffer:
buffer_write(buffer, data, data_size);

// When done:
buffer_flush(buffer);
buffer_destroy(buffer);
```

## Performance Considerations

- Buffer sizes should be tuned based on the typical I/O patterns of your workload
- For large sequential writes, larger buffers generally improve performance
- For random access patterns, smaller buffers may be more appropriate
- The automatic resizing feature adjusts buffer sizes based on usage, but manual tuning may still be beneficial for specific workloads

## Testing

The Buffer Manager includes a comprehensive test suite that verifies:
- Buffer creation and destruction
- Writing data through the buffer
- Automatic buffer resizing
- Reading data through the buffer

Run the tests with:

```bash
cd tests
make -f Makefile.buffer_manager run
```