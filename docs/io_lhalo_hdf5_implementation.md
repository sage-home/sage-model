# LHalo HDF5 Format Handler Implementation Guide

This document describes the implementation of the LHalo HDF5 format handler for the SAGE model. The handler provides a modular, resource-efficient way to read merger trees in the LHalo HDF5 format while integrating with the I/O interface system.

## Files Implemented

1. **`io_lhalo_hdf5.h`** - Header file defining the interface for the LHalo HDF5 format handler
2. **`io_lhalo_hdf5.c`** - Implementation file providing the handler functionality
3. **`tests/test_lhalo_hdf5.c`** - Test file for verifying handler functionality

## Required Makefile Changes

The following changes are needed in the Makefile:

1. Add `io_lhalo_hdf5.c` to the HDF5 source files list:
   ```makefile
   H5_SRC := io/read_tree_lhalo_hdf5.c io/save_gals_hdf5.c io/read_tree_genesis_hdf5.c \
             io/hdf5_read_utils.c io/read_tree_consistentrees_hdf5.c \
             io/read_tree_gadget4_hdf5.c io/io_hdf5_utils.c io/io_lhalo_hdf5.c
   ```

2. Add the new test target to the .PHONY declaration:
   ```makefile
   .PHONY: clean celan celna clena tests all test_extensions test_io_interface test_endian_utils test_lhalo_binary test_lhalo_hdf5
   ```

3. Add the test_lhalo_hdf5 target:
   ```makefile
   test_lhalo_hdf5: tests/test_lhalo_hdf5.c $(SAGELIB)
       $(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_lhalo_hdf5 tests/test_lhalo_hdf5.c -L. -l$(LIBNAME) $(LIBFLAGS)
   ```

4. Update the tests target to include the new test:
   ```makefile
   tests: $(EXEC) test_io_interface test_endian_utils test_lhalo_binary test_lhalo_hdf5
       ./tests/test_sage.sh
       ./tests/test_io_interface
       ./tests/test_endian_utils
       ./tests/test_lhalo_binary
       ./tests/test_lhalo_hdf5
   ```

## Integration with I/O Interface

The implementation adds the following to the I/O interface system:

1. **Handler Registration**: LHalo HDF5 handler is registered during `io_init()`
2. **Format Detection**: Added capability to detect LHalo HDF5 format files
3. **Resource Management**: Proper tracking and cleanup of HDF5 file handles

## Implementation Details

### Format-Specific Data Structure

The handler uses a format-specific data structure to hold state:

```c
struct lhalo_hdf5_data {
    hid_t *file_handles;       /* Open HDF5 file handles (one per forest) */
    hid_t *unique_file_handles; /* Open unique HDF5 file handles */
    int32_t num_open_files;     /* Number of open files */
    int64_t *nhalos_per_forest; /* Number of halos in each forest */
    struct HDF5_METADATA_NAMES metadata_names; /* HDF5 metadata field names */
};
```

### Key Functions

1. **`io_lhalo_hdf5_init()`**: Registers the handler with the I/O interface system
2. **`io_is_lhalo_hdf5()`**: Detects if a file is in LHalo HDF5 format
3. **`io_lhalo_hdf5_initialize()`**: Opens files and reads metadata
4. **`io_lhalo_hdf5_read_forest()`**: Reads a specific forest from HDF5 files
5. **`io_lhalo_hdf5_cleanup()`**: Cleans up resources

### HDF5 Resource Management

The implementation leverages the HDF5 resource tracking system to prevent handle leaks:

1. Uses `HDF5_TRACK_FILE()` and other tracking macros to register handles
2. Calls `hdf5_untrack_handle()` after handles are closed
3. Implements proper cleanup in all code paths, including error conditions

## Testing

The `test_lhalo_hdf5.c` file provides tests for:

1. Format detection
2. Handler registration
3. Handler capabilities

## Usage

To use the LHalo HDF5 handler:

1. Set the tree type to `lhalo_hdf5` in the parameter file
2. The handler will be automatically registered during `io_init()`
3. The appropriate handler will be selected for reading LHalo HDF5 format files
