# Phase 3.2 Step 7: Update Core Save Function

## Overview

This step focused on updating the core I/O functions in `core_save.c` to use the new I/O interface while maintaining backward compatibility with the existing implementation. The goal was to enable a seamless transition to the modular I/O architecture without disrupting current functionality.

## Implementation Details

### Core Changes

1. **Structure Updates**:
   - Added `io_handler_data` structure to `core_allvars.h`
   - Added the I/O handler field to the `save_info` structure
   - Added forward declaration of `io_interface` structure

2. **Compile-Time Flag**:
   - Added `USE_IO_INTERFACE` flag in `core_save.h` to allow enabling/disabling the I/O interface approach

3. **Core Functions Update**:
   - Modified `initialize_galaxy_files()` to initialize the I/O system and use I/O handlers
   - Updated `save_galaxies()` to use the I/O interface for writing galaxies
   - Updated `finalize_galaxy_files()` to use the I/O interface for cleanup

4. **Error Handling Framework**:
   - Added `map_io_error_to_sage_error()` function to translate I/O errors to SAGE errors
   - Added `log_io_error()` function for consistent error logging

### Design Decisions

- **Parallel Implementation Approach**: Implemented the I/O interface alongside the existing implementation, allowing for a gradual transition
- **Error Handling Strategy**: Created a framework for comprehensive error handling but deferred full integration
- **Conditional Execution**: Used the `USE_IO_INTERFACE` flag to selectively enable the new approach

## Testing

- Successfully compiled the codebase with the new changes
- All individual component tests pass:
  - `test_io_interface`
  - `test_binary_output`
  - `test_hdf5_output`

## Future Work

1. **Error Handling Integration**:
   - Integrate `map_io_error_to_sage_error()` and `log_io_error()` functions into the error paths
   - Replace direct `fprintf` calls with the standardized error handling

2. **Buffer Management**:
   - Complete the implementation of buffer flushing for the HDF5 output handler
   - This work is planned for Phase 3.3 (Memory Optimization)

3. **Runtime Configuration**:
   - Convert the `USE_IO_INTERFACE` compile-time flag to a runtime parameter

## Conclusion

Step 7 has established the foundation for using the I/O interface throughout the SAGE model. The implementation maintains backward compatibility while setting the stage for full migration to the new architecture. The parallel approach allows for incremental testing and validation, ensuring reliability during the transition period.