# SAGE Output Preparation Module

## Overview

The Output Preparation Module is responsible for transforming galaxy properties before they are written to output files. This module:

1. Runs in the FINAL pipeline phase after all physics calculations are complete
2. Performs unit conversions (e.g., linear to logarithmic)
3. Calculates derived properties
4. Ensures consistent output values across different output formats

> **Note:** This functionality is tested by `test_property_system_hdf5.c`, which provides comprehensive testing of the property transformer system.

## Features

- Property-based approach using `GALAXY_PROP_*` macros
- Direct integration with the SAGE pipeline system
- Support for dynamic array properties
- Unit conversions and normalization
- Derived property calculation

## Implementation Details

### Module Registration

The module is registered with the pipeline system during initialization:

```c
int init_output_preparation_module(void) {
    // Initialize module structure
    memset(&output_module, 0, sizeof(struct base_module));
    
    // Set module information
    strncpy(output_module.name, "output_preparation", sizeof(output_module.name)-1);
    strncpy(output_module.version, "1.0", sizeof(output_module.version)-1);
    strncpy(output_module.author, "SAGE Team", sizeof(output_module.author)-1);
    
    // Set phase bitmask for FINAL phase
    output_module.phases = PIPELINE_PHASE_FINAL;
    
    // Set execution functions
    output_module.execute_final_phase = output_preparation_execute;
    
    // Register module with the system
    int ret = module_register(&output_module);
    // ...
}
```

### Execution

The module's main function, `output_preparation_execute()`, is called in the FINAL phase of the pipeline execution:

```c
int output_preparation_execute(struct pipeline_context *ctx) {
    // Process each galaxy
    for (int i = 0; i < ctx->num_galaxies; i++) {
        struct GALAXY *galaxy = &ctx->galaxies[i];
        
        // Skip merged galaxies
        if (GALAXY_PROP_Type(galaxy) == 3) {
            continue;
        }
        
        // Apply unit conversions and calculate derived properties
        // ...
    }
}
```

### Common Output Transformations

- Converting temperatures to logarithmic scale
- Normalizing star formation rates
- Calculating specific quantities (e.g., specific star formation rate)
- Cleaning array properties

## Integration with Output System

The Output Preparation Module directly integrates with the HDF5 output format:

1. The HDF5 output system uses property metadata to determine field structure
2. The `generate_field_metadata()` function creates output field descriptions based on property definitions
3. The `GALAXY_PROP_*` macros are used for all property access during output
4. Dynamic array properties are handled using the extension system

## How to Add New Output Properties

1. Define the property in `properties.yaml` with the `output: true` flag
2. Add any necessary unit conversions to the `output_preparation_execute()` function
3. If the property requires complex derivation, add the calculation logic to the same function

## Example: Converting to Logarithmic Scale

```c
// Convert disk temperature from Kelvin to log(K)
if (GALAXY_PROP_DiskTemperature(galaxy) > 0) {
## Example: Unit Conversion for Core Properties

```c
// Apply logarithmic scaling to disk temperature (core property)
if (GALAXY_PROP_DiskTemperature(galaxy) > 0) {
    GALAXY_PROP_DiskTemperature(galaxy) = log10(GALAXY_PROP_DiskTemperature(galaxy));
}
```

## Example: Calculating Derived Properties from Physics Properties

For physics properties, the module must use the generic property system to maintain core-physics separation:

```c
// Calculate specific star formation rate using generic property accessors
property_id_t stellar_mass_id = get_cached_property_id("StellarMass");
property_id_t sfr_disk_id = get_cached_property_id("SfrDisk");
property_id_t specific_sfr_id = get_cached_property_id("SpecificSFR");

if (stellar_mass_id != PROP_COUNT && sfr_disk_id != PROP_COUNT && specific_sfr_id != PROP_COUNT) {
    float stellar_mass = get_float_property(galaxy, stellar_mass_id, 0.0f);
    float sfr_disk = get_float_property(galaxy, sfr_disk_id, 0.0f);
    
    if (stellar_mass > 0.0f && sfr_disk > 0.0f) {
        float specific_sfr = sfr_disk / stellar_mass;
        set_float_property(galaxy, specific_sfr_id, specific_sfr);
    } else {
        set_float_property(galaxy, specific_sfr_id, 0.0f);
    }
}
```

**Note**: This example demonstrates the correct approach for accessing physics properties in output preparation. The module uses generic property accessors rather than direct `GALAXY_PROP_*` macros for physics properties to maintain core-physics separation.

## Migration from Binary Output

The binary output format is now deprecated and removed. All output is now performed using the HDF5 format, which provides:

1. Better compatibility with analysis tools
2. Support for metadata and attributes
3. Hierarchical organization of data
4. Improved portability across platforms infrastructure remains physics-agnostic while allowing for flexible and extensible output formatting.

## Adding New Output Properties

To add a new property to the output:

1. Define the property in `properties.yaml` with the `output: true` flag
2. If special transformation is needed, add an `output_transformer_function` field
3. Implement the transformer function in `physics_output_transformers.c`
4. Update the declaration in `physics_output_transformers.h`

Example:

```yaml
# In properties.yaml
- name: MyCustomProperty
  type: float
  initial_value: 0.0
  units: "custom_units"
  description: "My custom property"
  output: true
  output_transformer_function: "transform_output_MyCustomProperty"
```

```c
// In physics_output_transformers.h
int transform_output_MyCustomProperty(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                    void *output_buffer_element_ptr, const struct params *run_params);

// In physics_output_transformers.c
int transform_output_MyCustomProperty(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                    void *output_buffer_element_ptr, const struct params *run_params) {
    // Get the property value using generic property access
    float raw_value = get_float_property(galaxy, output_prop_id, 0.0f);
    
    // Apply transformation
    *(float*)output_buffer_element_ptr = transform_function(raw_value);
    
    return 0;
}
```

## Standardization on HDF5 Output

The binary output format has been removed, and SAGE now standardizes on HDF5 output for several reasons:

1. **Self-describing format**: HDF5 includes metadata and is more maintainable
2. **Better compatibility**: Widely supported by analysis tools and visualization software
3. **Hierarchical organization**: Better represents the structure of galaxy data
4. **Compression support**: Reduces file size without loss of precision
5. **Property-based integration**: Better supports the property system architecture