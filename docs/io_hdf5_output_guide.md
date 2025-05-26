# HDF5 Output Handler Guide

## Overview

The HDF5 Output Handler is an implementation of the I/O interface for writing galaxy data in the SAGE HDF5 format. It provides a standardized approach to galaxy output with support for property-based serialization, output transformers, and robust resource management.

**Note**: This document covers how to *use* the HDF5 output functionality through the I/O interface. For detailed information about the internal implementation of the HDF5 output engine, see [HDF5 Galaxy Output System](hdf5_galaxy_output.md).

## Features

- **Hierarchical Structure**: Organizes data in a logical hierarchy with groups for snapshots and datasets for properties
- **Property-Based Output**: Uses the centralized property system to determine output fields dynamically
- **Output Transformers**: Supports custom transformation functions for unit conversion and derived values
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
│   ├── ...                 (Other properties)
│   └── ...                 (Properties with output transformers)
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

### Property-Based Output System

The HDF5 output handler uses a property-based approach for determining output fields:

1. Properties are defined in `properties.yaml` with the `output: true` flag to indicate they should be included in output
2. The `generate_field_metadata()` function reads property metadata to create HDF5 field definitions
3. Property definitions include names, descriptions, units, and data types
4. The `output_transformer_function` in property definitions specifies custom transformation functions

### Output Transformer System

The Output Transformer system allows for custom property transformations during output:

1. **Property Definition**: In `properties.yaml`, properties can specify an output transformer function:
   ```yaml
   - name: Cooling
     type: double
     output: true
     output_transformer_function: "transform_output_Cooling"
   ```

2. **Transformer Functions**: Implemented in `physics_output_transformers.c`, these functions apply transformations:
   ```c
   int transform_output_Cooling(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                              void *output_buffer_element_ptr, const struct params *run_params) {
       // Get the raw cooling value
       double raw_cooling = get_double_property(galaxy, output_prop_id, 0.0);
       
       // Apply log10 scaling and unit conversion
       if (raw_cooling > 0.0) {
           *(float*)output_buffer_element_ptr = (float)log10(raw_cooling * 
               run_params->units.UnitEnergy_in_cgs / run_params->units.UnitTime_in_s);
       } else {
           *(float*)output_buffer_element_ptr = 0.0f;
       }
       
       return 0;
   }
   ```

3. **Dispatcher**: The `dispatch_property_transformer()` function routes properties to their transformers

Types of transformers include:
- **Unit Conversion**: Convert from internal units to output units
- **Logarithmic Scaling**: Apply log10 or other scaling functions
- **Array-to-Scalar Derivation**: Calculate summary values from array properties
- **Metallicity Calculation**: Derive metallicity from metal and gas masses

### Core-Physics Property Separation

The HDF5 output handler respects the core-physics separation principles:

1. **Core Properties**: Accessed directly via `GALAXY_PROP_*` macros in `prepare_galaxy_for_hdf5_output()`
2. **Physics Properties**: Accessed via the generic property system (`get_float_property()`, etc.)
3. **Property Dispatch**: Uses the property dispatcher to transform properties according to their types

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
- Dynamic arrays with runtime-determined sizes

## Performance Considerations

- Uses buffered writing to reduce I/O operations
- Implements chunking for large datasets
- Supports compression for reduced file size (optional)
- The property system adapts to varying memory layout patterns for efficiency

## Compatibility

The HDF5 output format is compatible with various analysis tools:

- Python (h5py, PyTables)
- MATLAB
- IDL
- C/C++ HDF5 libraries
- HDF5 command-line tools (h5dump, h5ls, etc.)

## Adding New Output Properties

To add a new property to the HDF5 output:

1. Define the property in `properties.yaml` with the `output: true` flag:
   ```yaml
   - name: MyProperty
     type: float
     initial_value: 0.0
     units: "custom_units"
     description: "My custom property"
     output: true
   ```

2. If special transformation is needed, add an `output_transformer_function` field:
   ```yaml
   output_transformer_function: "transform_output_MyProperty"
   ```

3. Implement the transformer function in `physics_output_transformers.c` if specified

The property will be automatically included in the HDF5 output files with proper metadata.
