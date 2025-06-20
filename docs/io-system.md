# SAGE I/O System Guide

## Overview

The SAGE I/O System provides unified access to multiple tree formats and galaxy output operations through a standardized interface. This document covers both the internal I/O interface implementation and user-facing functionality for reading merger trees and writing galaxy catalogs.

## Architecture

### Unified I/O Interface

The I/O interface abstracts tree reading and galaxy output operations across multiple data formats through a standardized API:

```c
struct io_interface {
    // Metadata
    const char *name;
    const char *version; 
    int format_id;
    uint32_t capabilities;
    
    // Core operations
    int (*initialize)(const char *filename, struct params *params, void **format_data);
    int64_t (*read_forest)(int64_t forestnr, struct halo_data **halos,
                          struct forest_info *forest_info, void *format_data);
    int (*write_galaxies)(struct GALAXY *galaxies, int ngals,
                         struct save_info *save_info, void *format_data);
    int (*cleanup)(void *format_data);
    
    // Global forest operations
    int (*setup_forests)(struct forest_info *forests_info, int ThisTask, int NTasks, struct params *run_params);
    int (*cleanup_forests)(struct forest_info *forests_info);
    
    // Resource management
    int (*close_open_handles)(void *format_data);
    int (*get_open_handle_count)(void *format_data);
};
```

### Benefits of Unified Interface

**Format-Agnostic Operations**: Core code no longer needs format-specific knowledge
**Improved Testability**: Standardized interface enables comprehensive testing
**Resource Management**: Built-in handle tracking prevents resource leaks  
**Error Handling**: Consistent error reporting across all formats
**Legacy Compatibility**: Graceful fallback ensures existing functionality continues to work

## Supported Formats

### Tree Input Formats

#### LHalo Binary
- **Status**: Legacy format using direct function calls
- **File Pattern**: `trees_XXX.Y` where XXX is snapshot number, Y is file number
- **Characteristics**: Efficient binary format with custom endianness handling
- **Use Case**: Large-scale simulations requiring fast I/O

#### LHalo HDF5
- **Status**: ✅ Complete I/O interface implementation with legacy dependency elimination
- **File Pattern**: `trees_XXX.Y.hdf5`
- **Characteristics**: HDF5 format with comprehensive metadata
- **Benefits**: Self-describing, cross-platform compatibility, metadata preservation

#### Gadget4 HDF5
- **Status**: 🔄 Interface structure exists but uses legacy implementation
- **File Pattern**: Gadget4-specific HDF5 structure
- **Characteristics**: Native Gadget4 output format
- **Use Case**: Direct processing of Gadget4 simulation outputs

#### Genesis HDF5
- **Status**: 🔄 Interface structure exists but uses legacy implementation
- **File Pattern**: Genesis-specific HDF5 structure
- **Characteristics**: Genesis simulation format
- **Use Case**: Processing Genesis merger trees

#### ConsistentTrees ASCII
- **Status**: ⏸️ Legacy format using direct function calls
- **File Pattern**: Text-based format
- **Characteristics**: Human-readable ASCII format
- **Use Case**: Small datasets, debugging, cross-validation

#### ConsistentTrees HDF5
- **Status**: 🔄 Interface structure exists but uses legacy implementation
- **File Pattern**: ConsistentTrees HDF5 structure
- **Characteristics**: HDF5 version of ConsistentTrees format
- **Use Case**: Large ConsistentTrees datasets

### Galaxy Output Formats

#### HDF5 Galaxy Output
- **Status**: ✅ Production ready with property-based interface
- **Features**: Dynamic property discovery, hierarchical organization, comprehensive metadata
- **File Structure**: Snapshot-organized groups with complete header information
- **Integration**: Unified with property system for automatic field generation

#### Binary Galaxy Output
- **Status**: ✅ Supported through legacy interface
- **Features**: Compact binary format for large datasets
- **Characteristics**: Platform-specific, efficient storage
- **Use Case**: Legacy compatibility, maximum performance

## Migration Status

### Phase 5.3 I/O Interface Migration

The Phase 5.3 migration successfully eliminated legacy dependencies for the LHalo HDF5 format:

**Completed:**
1. **Extended I/O Interface**: Added `setup_forests()` and `cleanup_forests()` function pointers
2. **Hybrid Core System**: `core_io_tree.c` tries I/O interface first, graceful fallback to legacy
3. **Complete LHalo HDF5 Implementation**: Bridge functions eliminated legacy dependencies
4. **Legacy Header Elimination**: Removed `read_tree_lhalo_hdf5.h` dependency from core

**Implementation Pattern:**
```c
// Try to use I/O interface for setup if available
int format_id = io_map_tree_type_to_format_id((int)TreeType);
struct io_interface *handler = io_get_handler_by_id(format_id);

if (handler != NULL && handler->setup_forests != NULL) {
    // Use I/O interface for setup
    status = handler->setup_forests(forests_info, ThisTask, NTasks, run_params);
} else {
    // Fall back to legacy setup functions for formats without I/O interface
    switch (TreeType) {
        // Legacy format handling...
    }
}
```

## Using the I/O Interface

### Reading Tree Data

```c
#include "io/io_interface.h"

// Get handler for a specific format
int format_id = io_map_tree_type_to_format_id((int)TreeType);
struct io_interface *handler = io_get_handler_by_id(format_id);

// Setup forests (if handler supports it)
if (handler && handler->setup_forests) {
    int status = handler->setup_forests(forests_info, ThisTask, NTasks, run_params);
}

// Read forest data
int64_t nhalos = handler->read_forest(forestnr, &halos, forest_info, format_data);

// Cleanup when done
if (handler && handler->cleanup_forests) {
    handler->cleanup_forests(forests_info);
}
```

### Writing Galaxy Data

```c
// Initialize output handler
struct io_interface *output_handler = io_get_handler_by_id(OUTPUT_FORMAT_HDF5);
void *output_data;
output_handler->initialize(output_filename, run_params, &output_data);

// Write galaxies
struct save_info save_info = {
    .snapshot = current_snapshot,
    .tree_id = tree_id,
    .ngals = ngals_output
};

int status = output_handler->write_galaxies(galaxies, ngals_output, &save_info, output_data);

// Cleanup
output_handler->cleanup(output_data);
```

## HDF5 Galaxy Output System

### Purpose and Features

The HDF5 Galaxy Output System serves as the primary interface for saving galaxy data, addressing several key requirements:

1. **Scientific Accessibility**: Files easily readable by analysis tools (Python, MATLAB, IDL)
2. **Performance**: Efficient writing with optimized buffering and chunking
3. **Metadata Preservation**: Comprehensive metadata including SAGE version, runtime parameters, cosmological parameters
4. **Cross-Platform Compatibility**: Files readable across different architectures
5. **Modularity Support**: Adapts dynamically to available properties based on loaded physics modules

### File Structure

The HDF5 output files use a hierarchical structure:

```
/
├── Header/
│   ├── Simulation/          # Simulation parameters
│   │   ├── BoxSize
│   │   ├── PartMass
│   │   └── ...
│   ├── Runtime/             # Runtime parameters
│   │   ├── TreeType
│   │   ├── OutputDir
│   │   └── ...
│   └── Misc/               # Miscellaneous information
│       ├── SAGEVersion
│       ├── CompilerFlags
│       └── ...
├── Snap_063/               # Snapshot group (e.g., snapshot 63)
│   ├── Mvir                # Galaxy property datasets
│   ├── ColdGas
│   ├── StellarMass
│   └── ...
└── Snap_037/               # Another snapshot
    ├── Mvir
    └── ...
```

### Key Concepts

- **Dynamic Property Discovery**: Output fields determined at runtime based on property definitions
- **Property-Based Serialization**: Galaxy properties accessed through unified property system
- **Output Transformers**: Custom functions transform properties before output (unit conversion, etc.)
- **Buffered Writing**: Galaxies collected in buffers before writing for performance
- **Hierarchical Organization**: Data organized in HDF5 groups by snapshot with comprehensive metadata

### Output Transformers

Output transformers enable custom property transformations before writing:

```c
// Example transformer function
double transform_stellar_mass_to_solar_masses(const struct GALAXY *galaxy, property_id_t prop_id) {
    double mass_internal = get_double_property(galaxy, prop_id, 0.0);
    return mass_internal * UnitMass_in_g / SOLAR_MASS;
}
```

Transformers are defined in property YAML:
```yaml
- name: "StellarMass"
  output_transformer_function: "transform_stellar_mass_to_solar_masses"
```

### Integration with Property System

The HDF5 output system integrates seamlessly with the property system:

1. **Dynamic Field Detection**: Automatically discovers available properties at runtime
2. **Type-Safe Access**: Uses property system's type-safe accessors
3. **Build Configuration Awareness**: Adapts to physics-free, full-physics, or custom builds
4. **Transformer Integration**: Applies output transformers when specified

## Endianness and Cross-Platform Support

### Endianness Handling

SAGE includes comprehensive endianness utilities for cross-platform compatibility:

```c
// Endianness conversion functions
uint32_t swap_uint32(uint32_t value);
uint64_t swap_uint64(uint64_t value);
float swap_float(float value);
double swap_double(double value);

// Detection and conversion
bool is_big_endian(void);
void convert_endianness_if_needed(void *data, size_t element_size, size_t count, bool file_is_big_endian);
```

**Features:**
- Automatic endianness detection
- Selective conversion only when needed
- Support for all numeric types
- Integration with binary I/O operations

### Memory Mapping and Buffering

#### Memory Mapping
- **Cross-platform support**: Windows (`CreateFileMapping`) and Unix (`mmap`)
- **Performance benefits**: Direct memory access for large files
- **Automatic fallback**: Falls back to regular I/O if mapping fails
- **Resource management**: Proper cleanup of mapped regions

#### Buffer Management
- **Dynamic buffer sizing**: Adapts to available memory
- **Chunked processing**: Processes large datasets in manageable chunks
- **Memory pool integration**: Uses SAGE's memory management system
- **Error recovery**: Handles memory allocation failures gracefully

## Performance Optimizations

### Buffered I/O
```c
// Buffer manager for efficient I/O
struct io_buffer_manager {
    void *buffer;
    size_t buffer_size;
    size_t max_buffer_size;
    size_t current_offset;
    bool auto_resize;
};
```

**Benefits:**
- Reduced system call overhead
- Optimized for sequential access patterns
- Automatic buffer resizing based on workload
- Memory pool integration for efficient allocation

### Memory Mapping
```c
// Memory mapping for large file access
struct memory_map {
    void *data;
    size_t size;
    platform_handle_t handle;  // Platform-specific handle
    bool is_mapped;
};
```

**Benefits:**
- Direct memory access to file data
- Reduced memory copying overhead
- Operating system page cache utilization
- Lazy loading of file contents

### HDF5 Optimizations

**Chunking Strategy:**
- Optimal chunk sizes for galaxy data access patterns
- Balanced between compression and access performance
- Snapshot-based chunking for time-series analysis

**Compression:**
- Configurable compression levels
- Automatic selection based on data characteristics
- Balance between file size and I/O performance

## Resource Management

### Handle Tracking

The I/O system includes comprehensive resource management:

```c
// Resource tracking functions
int (*close_open_handles)(void *format_data);
int (*get_open_handle_count)(void *format_data);
```

**Features:**
- Automatic handle tracking for all open files
- Leak detection and reporting
- Graceful cleanup on errors
- Resource usage monitoring

### Memory Management

**Memory Pools:**
- Dedicated pools for I/O operations
- Reduced memory fragmentation
- Predictable memory usage patterns
- Automatic cleanup on completion

**Dynamic Expansion:**
- Automatic buffer growth for large datasets
- Memory pressure handling
- Fallback to disk-based processing when memory constrained

## Error Handling and Validation

### Error Detection

**File Format Validation:**
- Header validation for all supported formats
- Consistency checks between metadata and data
- Corruption detection through checksums
- Graceful handling of malformed files

**Data Validation:**
- Range checks for physical quantities
- Consistency validation between related properties
- Unit validation and conversion
- Missing data detection and handling

### Error Recovery

**Automatic Recovery:**
- Fallback to alternative I/O methods
- Retry mechanisms for transient failures
- Partial data recovery from corrupted files
- Graceful degradation when possible

**Error Reporting:**
- Detailed error messages with context
- Error location tracking (file, line, function)
- Structured error codes for programmatic handling
- Integration with SAGE logging system

## Testing

### I/O System Tests

**Format-Specific Tests:**
- `test_lhalo_binary` - LHalo binary format reading
- `test_lhalo_hdf5` - LHalo HDF5 format with I/O interface
- `test_gadget4_hdf5` - Gadget4 HDF5 format reading
- `test_genesis_hdf5` - Genesis HDF5 format reading
- `test_consistent_trees_hdf5` - ConsistentTrees HDF5 format reading

**System Tests:**
- `test_io_interface` - I/O interface abstraction
- `test_endian_utils` - Cross-platform endianness handling
- `test_hdf5_output` - HDF5 output handler functionality
- `test_io_memory_map` - Memory mapping functionality
- `test_io_buffer_manager` - Buffered I/O system
- `test_validation_framework` - I/O validation mechanisms

### Running I/O Tests

```bash
make io_tests                    # Run all I/O system tests
make test_io_interface          # Test I/O interface abstraction
make test_hdf5_output           # Test HDF5 output functionality
make test_endian_utils          # Test endianness handling
```

## Implementing New Format Handlers

### Handler Implementation

To add support for a new tree format:

1. **Create Handler Files**: `src/io/io_newformat.c` and `src/io/io_newformat.h`
2. **Implement Interface Functions**: All required function pointers in `io_interface` struct
3. **Register Handler**: Add auto-registration using C constructor attributes
4. **Add Format Mapping**: Update `io_map_tree_type_to_format_id()` in `io_interface.c`
5. **Create Tests**: Comprehensive tests for all interface functions

### Example Handler Implementation

```c
// Handler registration
static void __attribute__((constructor)) register_newformat_handler(void) {
    static struct io_interface newformat_interface = {
        .name = "NewFormat",
        .version = "1.0",
        .format_id = FORMAT_NEWFORMAT,
        .capabilities = IO_CAP_READ_TREES | IO_CAP_WRITE_GALAXIES,
        .initialize = newformat_initialize,
        .read_forest = newformat_read_forest,
        .write_galaxies = newformat_write_galaxies,
        .cleanup = newformat_cleanup,
        .setup_forests = newformat_setup_forests,
        .cleanup_forests = newformat_cleanup_forests
    };
    
    io_register_handler(&newformat_interface);
}
```

### Migrating Legacy Formats

To migrate a legacy format to the I/O interface:

1. **Create Interface Implementation**: Bridge existing functions to new interface
2. **Add Setup/Cleanup Functions**: Implement global forest operations
3. **Update Format Registration**: Register with I/O interface system
4. **Test Migration**: Verify compatibility with existing functionality
5. **Remove Legacy Headers**: Eliminate direct includes from `core_io_tree.c`

## Future Development

### Planned Enhancements

**Complete Migration Goals:**
- Migrate remaining HDF5 formats (Gadget4, Genesis, ConsistentTrees) from bridge functions to full implementations
- Consider migrating binary formats to I/O interface for consistency
- Enhance error handling and resource management across all formats
- Add support for new tree formats through the unified interface

**Performance Improvements:**
- Advanced caching strategies for frequently accessed data
- Parallel I/O support for large-scale simulations
- Compression algorithm optimization
- Memory usage optimization for extreme-scale datasets

**Feature Extensions:**
- Streaming I/O for continuous data processing
- Incremental output writing for checkpoint/restart functionality
- Advanced metadata indexing for fast property searches
- Integration with external analysis frameworks

## Troubleshooting

### Common Issues

**Problem**: "Format not supported" errors
**Solution**: Verify the TreeType parameter matches a supported format. Check that the format handler is properly registered.

**Problem**: Memory mapping failures
**Solution**: Check available system memory. The system will automatically fall back to regular I/O if mapping fails.

**Problem**: HDF5 output errors
**Solution**: Verify write permissions on output directory. Check that required properties are available in current build configuration.

**Problem**: Cross-platform compatibility issues
**Solution**: Use HDF5 formats for maximum portability. Binary formats are platform-specific and may require endianness conversion.

### Debugging Tools

**Resource Monitoring:**
```bash
# Check open file handles
./sage --debug-io millennium.par

# Monitor memory usage
valgrind --tool=massif ./sage millennium.par
```

**Validation Tools:**
```bash
# Validate HDF5 output
h5dump -H output_file.hdf5

# Check file integrity
h5check output_file.hdf5
```