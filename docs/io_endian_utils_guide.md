# Endianness Utilities Guide

## Overview

The I/O endianness utilities provide a consistent way to handle cross-platform binary data access. These utilities ensure that binary files created on one architecture (e.g., little-endian x86/x86-64) can be correctly read on another architecture (e.g., big-endian PowerPC).

## Key Features

- Platform-independent endianness detection
- Byte swapping functions for various data types (16/32/64-bit integers, float, double)
- Host-to-network and network-to-host conversion functions
- Array conversion utilities for efficient bulk operations
- Generic endianness swapping for any size of data

## When to Use

You should use these utilities whenever:

1. Reading or writing binary data that might be used on different architectures
2. Implementing file format handlers that need to handle platform differences
3. Serializing data structures to disk or network

## API Reference

### Endianness Detection

```c
bool is_little_endian(void);
bool is_big_endian(void);
enum endian_type get_system_endianness(void);
```

Use these functions to detect the current system's endianness. The `endian_type` enum includes `ENDIAN_LITTLE`, `ENDIAN_BIG`, and `ENDIAN_UNKNOWN` values.

### Byte Swapping

```c
uint16_t swap_bytes_uint16(uint16_t value);
uint32_t swap_bytes_uint32(uint32_t value);
uint64_t swap_bytes_uint64(uint64_t value);
float swap_bytes_float(float value);
double swap_bytes_double(double value);
```

These functions swap the byte order of individual values, converting between big-endian and little-endian representations.

### Host-Network Conversion

```c
uint16_t host_to_network_uint16(uint16_t value);
uint32_t host_to_network_uint32(uint32_t value);
uint64_t host_to_network_uint64(uint64_t value);
float host_to_network_float(float value);
double host_to_network_double(double value);

uint16_t network_to_host_uint16(uint16_t value);
uint32_t network_to_host_uint32(uint32_t value);
uint64_t network_to_host_uint64(uint64_t value);
float network_to_host_float(float value);
double network_to_host_double(double value);
```

These functions convert between the host's native byte order and network byte order (big-endian). Use these when reading/writing binary data for cross-platform compatibility.

### Array Operations

```c
void swap_bytes_uint16_array(uint16_t *array, size_t count);
void swap_bytes_uint32_array(uint32_t *array, size_t count);
void swap_bytes_uint64_array(uint64_t *array, size_t count);
void swap_bytes_float_array(float *array, size_t count);
void swap_bytes_double_array(double *array, size_t count);
```

These functions efficiently swap byte order for arrays of values. Use these when processing bulk data.

### Generic Swapping

```c
int swap_endianness(void *data, size_t size, size_t count);
```

This general-purpose function can swap endianness for any data type of size 2, 4, or 8 bytes. The `size` parameter specifies the size of each element in bytes, and `count` specifies the number of elements to process.

## Usage Examples

### Reading a Binary File

```c
// Reading a 32-bit integer from a binary file
uint32_t value;
fread(&value, sizeof(value), 1, file);
value = network_to_host_uint32(value);  // Convert to host byte order
```

### Writing a Binary File

```c
// Writing a 32-bit integer to a binary file
uint32_t value = 12345;
uint32_t network_value = host_to_network_uint32(value);  // Convert to network byte order
fwrite(&network_value, sizeof(network_value), 1, file);
```

### Processing an Array

```c
// Reading an array of float values from a binary file
float values[100];
fread(values, sizeof(float), 100, file);
swap_bytes_float_array(values, 100);  // Convert to host byte order if needed
```

### Using Generic Swapping

```c
// Reading a mixed structure from a binary file
struct {
    uint64_t id;
    double mass;
    float position[3];
} object;

fread(&object, sizeof(object), 1, file);

// Convert id to host byte order
swap_endianness(&object.id, 8, 1);
// Convert mass to host byte order
swap_endianness(&object.mass, 8, 1);
// Convert position array to host byte order
swap_endianness(object.position, 4, 3);
```

## Best Practices

1. Always use these utilities when reading/writing binary data that might be used across different platforms
2. Use the array functions when processing multiple values of the same type
3. For structured data, consider implementing serialization/deserialization functions that use these utilities
4. Document the endianness of your file formats explicitly
5. Consider using the HDF5 format for complex data structures when possible, as it handles endianness automatically
