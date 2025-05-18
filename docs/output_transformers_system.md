# SAGE Output Transformers System

## Purpose and Motivation

The Output Transformers system solves a critical architectural issue in SAGE: separating physics-specific output transformations from the core I/O infrastructure. 

Before this system, the `prepare_galaxy_for_hdf5_output.c` file contained a mix of generic I/O logic and physics-specific transformations like unit conversions and derived property calculations. This violated the separation of concerns principle and made it difficult to:

1. Add new properties that require special output handling
2. Modify existing output transformations without touching the I/O core
3. Maintain a clean architecture where physics modules are responsible for their specific output representations

The Output Transformers system addresses these issues by:

1. Moving physics-specific transformations to dedicated functions in physics modules
2. Using a registry-based approach where properties declare their own transformer functions
3. Implementing a generic dispatch mechanism that keeps the core I/O code physics-agnostic
4. Preserving the original property values in memory while creating transformed representations for output

## Architecture

The Output Transformers architecture consists of four main components:

1. **Property Definitions**: In `properties.yaml`, properties can specify an optional `output_transformer_function` field that names the function responsible for transforming the property for output.

2. **Code Generation**: The `generate_property_headers.py` script processes these definitions and generates a dispatcher function in `generated_output_transformers.c`.

3. **Transformer Functions**: Implemented in a dedicated file `physics_output_transformers.c/.h`, these functions apply physics-specific transformations to properties when preparing output.

4. **Dispatch Mechanism**: The generated `dispatch_property_transformer()` function serves as the central hub that routes each property to its specific transformer or applies a default identity transformation.

This design ensures that:
- Core I/O code remains physics-agnostic
- Each property's output representation is defined close to its physics module
- New transformations can be added without modifying the core I/O code
- Default behaviors are handled gracefully

### Data Flow

Here's the data flow when a galaxy is processed for HDF5 output:

1. `save_galaxies_to_hdf5()` calls `prepare_galaxy_for_hdf5_output()` for each galaxy
2. For core properties, direct access via `GALAXY_PROP_*` macros is performed
3. For physics properties, the process is:
   - Property ID is identified from buffer information
   - Output buffer location for the current galaxy is calculated
   - `dispatch_property_transformer()` is called with the galaxy, property info, and buffer pointer
   - The dispatcher identifies the transformer function (if any) for the property
   - The transformer performs any necessary calculations or conversions and writes to the output buffer
   - If no transformer is specified, a default identity transform copies the value directly

## Implementation Details

### Property Definition

Properties requiring special output handling specify a transformer function in `properties.yaml`:

```yaml
- name: Cooling
  type: double
  initial_value: 0.0
  units: "erg/s"
  description: "Cooling energy rate"
  output: true
  read_only: false
  is_core: false
  output_transformer_function: "transform_output_Cooling"
```

### Transformer Function Signature

All transformer functions follow a standardized signature:

```c
int transform_output_Cooling(
    const struct GALAXY *galaxy,        // Input galaxy (read-only)
    property_id_t output_prop_id,       // ID of the output property
    void *output_buffer_element_ptr,    // Pointer to output buffer location
    const struct params *run_params     // Global parameters for unit conversion, etc.
);
```

This signature provides access to:
- The original galaxy data (read-only)
- The property ID for metadata access
- A direct pointer to the output buffer where the transformed value should be written
- Runtime parameters for unit conversions and other calculations

### Dispatch Mechanism

The auto-generated dispatcher function handles routing to specific transformers:

```c
int dispatch_property_transformer(
    const struct GALAXY *galaxy,
    property_id_t output_prop_id,
    const char *output_prop_name,
    void *output_buffer_element_ptr,
    const struct params *run_params,
    hid_t h5_dtype
) {
    int status = 0;
    
    // Auto-generated if/else chain based on property name
    if (strcmp(output_prop_name, "Cooling") == 0) {
        status = transform_output_Cooling(galaxy, output_prop_id, output_buffer_element_ptr, run_params);
    }
    else if (strcmp(output_prop_name, "SfrDisk") == 0) {
        status = derive_output_SfrDisk(galaxy, output_prop_id, output_buffer_element_ptr, run_params);
    }
    // ...more property-specific handlers...
    else {
        // Default identity transformation
        // (Type-based copy from property to output buffer)
    }
    
    return status;
}
```

### Integration with HDF5 Output

In `prepare_galaxy_for_hdf5_output.c`, the physics-specific transformation logic has been replaced with calls to the dispatcher:

```c
// Handle physics properties via the dispatch_property_transformer
void *output_buffer_element_ptr = ((char*)buffer->data) + (gals_in_buffer * element_size);

int transform_status = dispatch_property_transformer(
    g,
    prop_id,
    buffer->name,
    output_buffer_element_ptr,
    run_params,
    buffer->h5_dtype
);
```

This keeps the I/O code clean and focused on its primary responsibility.

### Type Mapping

The transformer system handles the mapping between internal C types and HDF5 types:

| Internal Type | HDF5 Type         | Default Action in Dispatcher         |
|---------------|-------------------|--------------------------------------|
| float         | H5T_NATIVE_FLOAT  | Cast to float using get_float_property |
| double        | H5T_NATIVE_DOUBLE | Cast to double using get_double_property |
| int32_t       | H5T_NATIVE_INT    | Cast to int32_t using get_int32_property |
| int64_t       | H5T_NATIVE_LLONG  | Cast to int64_t using get_int64_property |

For custom transformations, the function must cast the `output_buffer_element_ptr` to the appropriate type based on the expected HDF5 output type.

### Build Process Integration

The transformer system is integrated into the build process:

1. During the build, `generate_property_headers.py` reads `properties.yaml` and generates:
   - `core_properties.h/.c` for property definitions
   - `generated_output_transformers.c` for the dispatcher

2. The Makefile includes rules to:
   - Regenerate these files when `properties.yaml` changes
   - Include `generated_output_transformers.c` in the compilation process
   - Link everything correctly in the final binary

3. The build system creates a stamp file (`.stamps/generate_properties.stamp`) to track when regeneration is needed

## Usage Guidelines

### Adding a New Property with a Transformer

1. Define the property in `properties.yaml` with the `output_transformer_function` field:
   ```yaml
   - name: MyProperty
     type: float
     initial_value: 0.0
     units: "custom_units"
     description: "My custom property"
     output: true
     output_transformer_function: "transform_output_MyProperty"
   ```

2. Implement the transformer function in `physics_output_transformers.c`:
   ```c
   int transform_output_MyProperty(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                 void *output_buffer_element_ptr, const struct params *run_params) {
       // Get property ID
       property_id_t prop_id = get_cached_property_id("MyProperty");
       
       if (prop_id == PROP_COUNT || !has_property(galaxy, prop_id)) {
           // Property not found, set default
           *((float*)output_buffer_element_ptr) = 0.0f;
           return 0;
       }
       
       // Get the raw value
       float raw_value = get_float_property(galaxy, prop_id, 0.0f);
       float *output_val_ptr = (float*)output_buffer_element_ptr;
       
       // Apply transformation
       *output_val_ptr = transform_my_property(raw_value, run_params);
       
       return 0;
   }
   ```

3. Add the function declaration to `physics_output_transformers.h`:
   ```c
   int transform_output_MyProperty(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                 void *output_buffer_element_ptr, const struct params *run_params);
   ```

4. Rebuild SAGE to regenerate the dispatcher with your new transformer.

### Types of Transformers

There are several common types of transformers:

1. **Unit Conversion Transformers**: Convert from internal units to output units
   ```c
   // Convert from internal energy units to log10(erg/s)
   *output_val_ptr = (float)log10(raw_value * run_params->units.UnitEnergy_in_cgs / 
                                 run_params->units.UnitTime_in_s);
   ```

2. **Array-to-Scalar Derivation**: Calculate a summary value from an array property
   ```c
   // Calculate average SFR from an array of timesteps
   float avg_sfr = 0.0f;
   for (int step = 0; step < STEPS; step++) {
       avg_sfr += get_float_array_element_property(galaxy, sfr_array_id, step, 0.0f) / STEPS;
   }
   *output_val_ptr = avg_sfr;
   ```

3. **Derived Property Calculation**: Calculate a new value from multiple properties
   ```c
   // Calculate metallicity as the ratio of metals to gas
   float metals = get_float_property(galaxy, metals_id, 0.0f);
   float gas = get_float_property(galaxy, gas_id, 1.0f); // Avoid division by zero
   *output_val_ptr = metals / gas;
   ```

### Handling Dynamic Arrays

For properties with dynamic array type (e.g., `float[]`), special handling is required:

1. Use `get_property_array_size()` to determine the array length at runtime
2. Access elements with `get_float_array_element_property()` or similar functions
3. Create a derived output value from the array elements

Example for a dynamic array:

```c
int derive_output_from_dynamic_array(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                                   void *output_buffer_element_ptr, const struct params *run_params) {
    property_id_t array_id = get_cached_property_id("MyDynamicArray");
    
    if (array_id == PROP_COUNT || !has_property(galaxy, array_id)) {
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    // Get array size
    int array_size = get_property_array_size(galaxy, array_id);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    // Example: Calculate average of array elements
    float sum = 0.0f;
    for (int i = 0; i < array_size; i++) {
        sum += get_float_array_element_property(galaxy, array_id, i, 0.0f);
    }
    
    *output_val_ptr = (array_size > 0) ? sum / array_size : 0.0f;
    
    return 0;
}
```

### Best Practices

1. **Error Handling**: Always check if properties exist before accessing them, and provide reasonable defaults.

2. **Type Safety**: Cast output buffer pointers to the correct type based on the property's output format.

3. **Performance**: Keep transformations efficient, especially for array properties.

4. **Units**: Document clearly what unit conversions are applied.

5. **Numerical Stability**: Handle edge cases like log(0) or division by zero.

6. **Memory Safety**: Transformers should never allocate memory or write beyond the provided output buffer.

7. **Consistency**: Maintain consistent transformation patterns across similar properties.

## Real-World Examples

### Logarithmic Transformation with Unit Conversion

From `physics_output_transformers.c`:

```c
int transform_output_Cooling(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                           void *output_buffer_element_ptr, const struct params *run_params) {
    // Get property ID
    property_id_t cooling_id = get_cached_property_id("Cooling");
    
    if (cooling_id == PROP_COUNT || !has_property(galaxy, cooling_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    // Get the raw cooling value
    double raw_cooling = get_double_property(galaxy, cooling_id, 0.0);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    if (raw_cooling > 0.0) {
        // Convert to log10 scale with unit conversion
        *output_val_ptr = (float)log10(raw_cooling * run_params->units.UnitEnergy_in_cgs / 
                                       run_params->units.UnitTime_in_s);
    } else {
        // Handle zero or negative values
        *output_val_ptr = 0.0f;
    }
    
    return 0;
}
```

### Array-to-Scalar Derivation

From `physics_output_transformers.c`:

```c
int derive_output_SfrDisk(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                        void *output_buffer_element_ptr, const struct params *run_params) {
    // Get source property ID for SfrDisk array
    property_id_t sfr_disk_id = get_cached_property_id("SfrDisk");
    
    if (sfr_disk_id == PROP_COUNT || !has_property(galaxy, sfr_disk_id)) {
        // Property not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    float tmp_SfrDisk = 0.0f;
    
    // Sum over all array elements and calculate average
    for (int step = 0; step < STEPS; step++) {
        float sfr_disk_val = get_float_array_element_property(galaxy, sfr_disk_id, step, 0.0f);
        
        // Apply unit conversion for each step
        tmp_SfrDisk += sfr_disk_val * run_params->units.UnitMass_in_g / 
                     run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
    }
    
    *output_val_ptr = tmp_SfrDisk;
    return 0;
}
```

### Metallicity Calculation

From `physics_output_transformers.c`:

```c
int derive_output_SfrDiskZ(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                         void *output_buffer_element_ptr, const struct params *run_params) {
    // Get source property IDs
    property_id_t sfr_disk_cold_gas_id = get_cached_property_id("SfrDiskColdGas");
    property_id_t sfr_disk_cold_gas_metals_id = get_cached_property_id("SfrDiskColdGasMetals");
    
    if (sfr_disk_cold_gas_id == PROP_COUNT || !has_property(galaxy, sfr_disk_cold_gas_id) ||
        sfr_disk_cold_gas_metals_id == PROP_COUNT || !has_property(galaxy, sfr_disk_cold_gas_metals_id)) {
        // Properties not found, set default
        *((float*)output_buffer_element_ptr) = 0.0f;
        return 0;
    }
    
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    float tmp_SfrDiskZ = 0.0f;
    int valid_steps = 0;
    
    // Calculate metallicity for each step and average
    for (int step = 0; step < STEPS; step++) {
        float sfr_disk_cold_gas_val = get_float_array_element_property(galaxy, sfr_disk_cold_gas_id, step, 0.0f);
        float sfr_disk_cold_gas_metals_val = get_float_array_element_property(galaxy, sfr_disk_cold_gas_metals_id, step, 0.0f);
        
        if (sfr_disk_cold_gas_val > 0.0f) {
            tmp_SfrDiskZ += sfr_disk_cold_gas_metals_val / sfr_disk_cold_gas_val;
            valid_steps++;
        }
    }
    
    // Average the metallicity values from valid steps
    if (valid_steps > 0) {
        *output_val_ptr = tmp_SfrDiskZ / valid_steps;
    } else {
        *output_val_ptr = 0.0f;
    }
    
    return 0;
}
```

## Testing

Currently, a basic test framework exists in `tests/test_property_system_hdf5.c`, but it provides only minimal validation. A comprehensive test should:

1. Create mock galaxies with known property values
2. Set up all necessary support structures (params, save_info, etc.)
3. Call `prepare_galaxy_for_hdf5_output()` to process the galaxies
4. Verify that the output buffers contain correctly transformed values

See the "Handover Document for Unit Test Expansion" for detailed information on expanding the test suite.

## Troubleshooting

Common issues and solutions:

1. **Missing Transformer Function**: If a property specifies a transformer that doesn't exist, you'll see an "undefined reference" linker error. Ensure all transformer functions are properly declared in `physics_output_transformers.h` and implemented in `physics_output_transformers.c`.

2. **Type Mismatch Errors**: If the transformer function casts the output buffer pointer to the wrong type, it can cause data corruption or crashes. Ensure the cast matches the expected HDF5 output type.

3. **Rebuilding Issues**: If changes to transformer functions don't seem to take effect, ensure you're rebuilding the codebase completely. The stamp file system may not detect changes to transformer implementations.

4. **Array Size Issues**: For dynamic arrays, ensure you're using `get_property_array_size()` rather than assuming a fixed size. This avoids buffer overruns.

5. **Default Value Handling**: If a property is missing or invalid, return a sensible default value and log an appropriate warning.

## Related Documentation

- See `docs/output_preparation.md` for information on the overall output preparation process.
- See `docs/property_dispatcher_system.md` for details on how property access is implemented.
- See `docs/core_physics_separation.md` for the broader context of separating core and physics code.

## Conclusion

The Output Transformers system provides a clean, modular approach to handling property transformations for output in SAGE. By separating physics-specific transformations from the core I/O code, it enhances maintainability, flexibility, and the overall architecture of the system.

The system makes it easy to:
- Add new output properties with custom transformations
- Modify existing transformations without touching the core code
- Maintain a clear separation between core infrastructure and physics modules
- Handle complex derivations and unit conversions in a type-safe manner

These benefits contribute to the overall goal of making SAGE more modular, maintainable, and extensible.
