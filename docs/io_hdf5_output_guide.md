# HDF5 Output Handler Guide

## Overview

The HDF5 Output Handler is an implementation of the I/O interface for writing galaxy data in the SAGE HDF5 format. It provides a standardized approach to galaxy output with support for extended properties and robust resource management.

## Features

- **Hierarchical Structure**: Organizes data in a logical hierarchy with groups for snapshots and datasets for properties
- **Extended Property Support**: Fully supports the galaxy extension system for module-specific properties
- **Metadata-Rich**: Uses HDF5's attribute system to store comprehensive metadata about each property
- **Resource Management**: Implements robust handle tracking to prevent resource leaks
- **Cross-Platform Compatibility**: Works consistently across different operating systems and architectures
- **Performance Optimized**: Uses buffered writing and chunking for better performance with large datasets

## File Structure

The HDF5 output format organizes data in a hierarchical structure:

```
[HDF5 File]
├── Header/                  (Metadata group)
│   ├── Version             (Format version)
│   ├── Cosmology/          (Cosmological parameters)
│   ├── Units/              (Unit definitions)
│   └── Simulation/         (Simulation parameters)
├── Snap_z0.000/            (Snapshot group, one per redshift)
│   ├── Type                (Galaxy type dataset)
│   ├── GalaxyIndex         (Galaxy ID dataset)
│   ├── StellarMass         (Each dataset has attributes for units/description)
│   ├── ...                 (Other standard properties)
│   └── ExtendedProperties/ (Group for extended properties)
│       ├── Property1       (Module-specific property)
│       ├── Property2       (Each with metadata attributes)
│       └── ...
└── Snap_z0.500/            (Another snapshot group)
    └── ...
```

## Usage

### Including the Handler

```c
#include "io/io_hdf5_output.h"
```

### Initializing the Handler

```c
// Initialize the I/O system
io_init();

// Register the HDF5 output handler
io_hdf5_output_init();

// Get the handler
struct io_interface *handler = io_get_hdf5_output_handler();

// Initialize with a filename and parameters
void *format_data = NULL;
handler->initialize("galaxies", params, &format_data);
```

### Writing Galaxies

```c
// Write galaxies to the output file
handler->write_galaxies(galaxies, ngals, save_info, format_data);
```

### Cleanup

```c
// Clean up resources
handler->cleanup(format_data);
```

## Implementation Details

### Extended Properties

Extended properties are stored in a separate group within each snapshot to keep the structure clean. Each property has its own dataset with attributes describing:

- Property name
- Data type
- Description
- Units
- Source module

### Resource Management

The handler uses the HDF5 tracking utilities to ensure all HDF5 handles are properly managed:

- Handles are tracked when created with `HDF5_TRACK_*` macros
- Handles are closed in reverse order of creation
- A final cleanup pass ensures no handles are leaked

### Error Handling

The handler uses the I/O interface error system for standardized error reporting:

- Error codes are standardized across all format handlers
- Detailed error messages provide context for debugging
- Error conditions trigger proper resource cleanup

## Data Types

The handler supports various data types for galaxy properties:

- Basic types (int32, int64, float, double, bool)
- Array types with fixed dimensions
- Compound types for structured data

## Performance Considerations

- Uses buffered writing to reduce I/O operations
- Implements chunking for large datasets
- Supports compression for reduced file size (optional)

## Compatibility

The HDF5 output format is compatible with various analysis tools:

- Python (h5py, PyTables)
- MATLAB
- IDL
- C/C++ HDF5 libraries
- HDF5 command-line tools (h5dump, h5ls, etc.)
