# I/O Interface Migration Guide

## Overview

SAGE implements a unified I/O interface system that abstracts tree reading and galaxy output operations across multiple data formats. This guide documents the current migration status and provides guidance for developers working with I/O operations.

## Architecture

### Unified I/O Interface

The I/O interface provides format-agnostic access to tree data and galaxy output through a standardized API:

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
    
    // Global forest operations (Phase 5.3 addition)
    int (*setup_forests)(struct forest_info *forests_info, int ThisTask, int NTasks, struct params *run_params);
    int (*cleanup_forests)(struct forest_info *forests_info);
    
    // Resource management
    int (*close_open_handles)(void *format_data);
    int (*get_open_handle_count)(void *format_data);
};
```

### Migration Status

**✅ Completed Migrations:**
- **LHalo HDF5**: Complete I/O interface implementation with legacy dependency elimination
- **Galaxy Output**: HDF5 galaxy output uses unified property-based interface
- **Core Infrastructure**: `core_io_tree.c` updated to use I/O interface with legacy fallback

**🔄 Partially Migrated:**
- **Gadget4 HDF5**: Interface structure exists but uses legacy implementation
- **Genesis HDF5**: Interface structure exists but uses legacy implementation  
- **ConsistentTrees HDF5**: Interface structure exists but uses legacy implementation

**⏸️ Legacy Formats:**
- **LHalo Binary**: Still uses legacy direct function calls
- **ConsistentTrees ASCII**: Still uses legacy direct function calls

## Implementation Details

### Phase 5.3 I/O Interface Migration

The Phase 5.3 migration successfully eliminated legacy dependencies for the LHalo HDF5 format by:

1. **Extended I/O Interface**: Added `setup_forests()` and `cleanup_forests()` function pointers for global forest initialization/cleanup operations

2. **Hybrid Core System**: Updated `core_io_tree.c` to try I/O interface first, with graceful fallback to legacy functions:

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

3. **Complete LHalo HDF5 Implementation**: Bridge functions that call legacy implementations while providing the new interface

4. **Legacy Header Elimination**: Removed `read_tree_lhalo_hdf5.h` dependency from `core_io_tree.c`

### Benefits of Migration

**Format-Agnostic Operations**: Core code no longer needs format-specific knowledge
**Improved Testability**: Standardized interface enables comprehensive testing
**Resource Management**: Built-in handle tracking prevents resource leaks  
**Error Handling**: Consistent error reporting across all formats
**Legacy Compatibility**: Graceful fallback ensures existing functionality continues to work

## Developer Guidelines

### Using the I/O Interface

For reading tree data:

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

### Implementing New Format Handlers

To add support for a new tree format:

1. **Create Handler File**: `src/io/io_newformat.c` and `src/io/io_newformat.h`
2. **Implement Interface Functions**: All required function pointers in `io_interface` struct
3. **Register Handler**: Add auto-registration using C constructor attributes
4. **Add Format Mapping**: Update `io_map_tree_type_to_format_id()` in `io_interface.c`
5. **Create Tests**: Comprehensive tests for all interface functions

### Migrating Legacy Formats

To migrate a legacy format to the I/O interface:

1. **Create Interface Implementation**: Bridge existing functions to new interface
2. **Add Setup/Cleanup Functions**: Implement global forest operations
3. **Update Format Registration**: Register with I/O interface system
4. **Test Migration**: Verify compatibility with existing functionality
5. **Remove Legacy Headers**: Eliminate direct includes from `core_io_tree.c`

## Testing

**I/O Interface Tests**: `make test_io_interface` - validates interface system
**Format-Specific Tests**: `make test_lhalo_hdf5` - validates specific format handlers
**Integration Tests**: `make test_integration_workflows` - validates end-to-end I/O workflows
**Physics-Free Tests**: `make test_physics_free_mode` - validates core-physics separation

## Related Documentation

- [HDF5 Output Handler Guide](io_hdf5_output_guide.md) - User guide for HDF5 output
- [HDF5 Galaxy Output System](hdf5_galaxy_output.md) - Internal HDF5 implementation details
- [Core-Physics Separation](core_physics_separation.md) - Architecture overview
- [Testing Architecture Guide](testing_architecture_guide.md) - Testing philosophy and structure

## Future Work

**Complete Migration Goals:**
- Migrate remaining HDF5 formats (Gadget4, Genesis, ConsistentTrees) from bridge functions to full implementations
- Consider migrating binary formats to I/O interface for consistency
- Enhance error handling and resource management across all formats
- Add support for new tree formats through the unified interface