# SAGE Physics Output Transformers Guide

## Purpose and Overview

The Physics Output Transformers system provides the **user-facing API** for physics module developers to define custom output transformations for their properties. This enables modular physics development while maintaining clean separation between core I/O infrastructure and physics-specific logic.

### Why Physics Output Transformers Are Essential

Physics modules often need to:

1. **Convert Units**: Transform internal simulation units (optimised for computation) to physical units (needed for scientific analysis)
2. **Apply Scaling**: Convert values to log scale or other representations for output
3. **Derive Properties**: Calculate derived quantities from multiple internal properties (e.g., metallicities, averages)
4. **Handle Special Cases**: Apply conditional logic based on galaxy properties (e.g., satellite-only fields)

Without output transformers, physics module developers would need to either:
- Store both internal and output representations (wasting memory)
- Modify core I/O code for each new property (violating modularity)
- Accept limitations in how their properties can be represented in output files

### Key Benefits for Physics Module Developers

The Physics Output Transformers system enables:

1. **Custom Property Output**: Define exactly how your physics properties appear in HDF5 files
2. **Unit Flexibility**: Use optimal internal units while providing scientifically meaningful output units  
3. **Derived Properties**: Create output fields calculated from multiple internal properties
4. **Core Independence**: Add new transformations without modifying core SAGE infrastructure
5. **Modular Physics**: Each physics module manages its own output representation

## Architecture Overview

The Physics Output Transformers system provides a clean interface between physics modules and the core I/O system through four main components:

### 1. **User Property Definitions** (`properties.yaml`)
Physics module developers define their properties and optionally specify transformer functions:

```yaml
- name: MyCustomProperty
  type: double
  units: "custom_units" 
  description: "My custom property"
  output: true
  output_transformer_function: "transform_output_MyCustomProperty"
```

### 2. **User Transformer Implementation** (`src/physics/physics_output_transformers.c/.h`)
Physics module developers implement their transformation logic:

```c
int transform_output_MyCustomProperty(const struct GALAXY *galaxy, 
                                     property_id_t output_prop_id,
                                     void *output_buffer_element_ptr,
                                     const struct params *run_params) {
    // Your custom transformation logic here
    // e.g., unit conversion, scaling, derivation
}
```

### 3. **Auto-Generated Dispatch** (`src/core/generated_output_transformers.c`)
The build system automatically generates the dispatcher that routes properties to their transformers.

### 4. **Core I/O Integration** (HDF5 Output System)
The core I/O system calls the dispatcher for each property, remaining completely physics-agnostic.

This architecture ensures that:
- **Physics modules are self-contained**: Each module manages its own output transformations
- **Core I/O remains generic**: No physics-specific knowledge in core infrastructure  
- **User extensibility**: New physics modules can define transformations without touching core code
- **Runtime flexibility**: Default identity transforms work automatically for simple properties

## For Physics Module Developers: How to Use Output Transformers

### When Do You Need Output Transformers?

You need output transformers when your physics properties require any of:
- **Unit conversion** (internal simulation units → physical units)
- **Mathematical transformation** (linear → log scale, normalisation, etc.)
- **Property derivation** (calculate output values from multiple internal properties)
- **Conditional logic** (different output behavior based on galaxy type, etc.)

Properties that don't need transformers will automatically use identity transforms (direct copy to output).

### Step-by-Step Usage Guide

#### 1. Define Your Property with Transformer
In `properties.yaml`, specify the transformer function:

```yaml
- name: MyPhysicsProperty
  type: double
  initial_value: 0.0
  units: "custom_units"
  description: "My custom property" 
  output: true
  read_only: false
  is_core: false
  output_transformer_function: "transform_output_MyPhysicsProperty"
```

#### 2. Implement Your Transformer Function
Add your transformer to `src/physics/physics_output_transformers.c`:

```c
int transform_output_MyPhysicsProperty(const struct GALAXY *galaxy, 
                                      property_id_t output_prop_id,
                                      void *output_buffer_element_ptr, 
                                      const struct params *run_params) {
    // Example: Convert internal units to physical units
    double internal_value = get_double_property(galaxy, output_prop_id, 0.0);
    double physical_value = internal_value * run_params->units.UnitEnergy_in_cgs;
    
    // Write to output buffer
    *((double*)output_buffer_element_ptr) = physical_value;
    
    return 0; // Success
}
```

#### 3. Declare Your Function
Add the declaration to `src/physics/physics_output_transformers.h`:

```c
int transform_output_MyPhysicsProperty(const struct GALAXY *galaxy, 
                                      property_id_t output_prop_id,
                                      void *output_buffer_element_ptr,
                                      const struct params *run_params);
```

#### 4. Build and Test
The build system will automatically:
- Generate the dispatcher code that calls your transformer
- Integrate your transformer into the I/O pipeline
- Handle error cases and provide defaults

### Key Implementation Guidelines

- **Read-only access**: The `galaxy` parameter is read-only - never modify it
- **Error handling**: Return 0 for success, non-zero for errors
- **Type safety**: Cast `output_buffer_element_ptr` to the correct output type
- **Property access**: Use `get_*_property()` functions to access galaxy properties
- **Unit conversion**: Use `run_params->units.*` for standard unit conversions

## Common Transformer Types and Examples

### 1. **Unit Conversion Transformers**
Convert from internal units to output units:

```c
int transform_output_Cooling(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                            void *output_buffer_element_ptr, const struct params *run_params) {
    // Get the raw cooling value
    double raw_cooling = get_double_property(galaxy, output_prop_id, 0.0);
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    
    if (raw_cooling > 0.0) {
        // Convert to log10 scale with unit conversion
        *output_val_ptr = (float)log10(raw_cooling * run_params->units.UnitEnergy_in_cgs / 
                                       run_params->units.UnitTime_in_s);
    } else {
        *output_val_ptr = 0.0f;
    }
    
    return 0;
}
```

### 2. **Array-to-Scalar Derivation**
Calculate a summary value from an array property:

```c
int derive_output_SfrDisk(const struct GALAXY *galaxy, property_id_t output_prop_id, 
                        void *output_buffer_element_ptr, const struct params *run_params) {
    float *output_val_ptr = (float*)output_buffer_element_ptr;
    float tmp_SfrDisk = 0.0f;
    
    // Sum over all array elements and calculate average
    for (int step = 0; step < STEPS; step++) {
        float sfr_disk_val = get_float_array_element_property(galaxy, output_prop_id, step, 0.0f);
        
        // Apply unit conversion for each step
        tmp_SfrDisk += sfr_disk_val * run_params->units.UnitMass_in_g / 
                     run_params->units.UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
    }
    
    *output_val_ptr = tmp_SfrDisk;
    return 0;
}
```

## Role in Core-Physics Separation Architecture

The Physics Output Transformers system is a **critical component** of SAGE's core-physics separation architecture. It enables this separation by:

### **Maintaining Clean Interfaces**
- **Core I/O system**: Remains completely physics-agnostic, handling only generic property dispatch
- **Physics modules**: Own their output transformations without touching core infrastructure
- **Clean boundary**: The transformer function signature provides the contract between core and physics

### **Enabling Modular Physics Development**
- **User extensibility**: Physics module developers can add custom transformations without core modifications
- **Module independence**: Each physics module manages its own output representation
- **Runtime flexibility**: New physics modules work automatically with the existing I/O infrastructure

### **Supporting the Separation Principles**
This system directly supports the core-physics separation principles:
1. **Core Independence**: Core I/O operates without physics knowledge
2. **Physics Modularity**: Physics modules are self-contained and interchangeable  
3. **Runtime Configuration**: Output behavior adapts to available physics modules
4. **Clean Dependencies**: Physics depends on core interfaces, but core doesn't depend on physics implementations

### **Not a Violation - It's the Solution**
These files are **not** a violation of separation principles - they **implement** the separation by:
- Providing the extension mechanism that allows physics modules to customize output
- Keeping physics-specific logic out of core I/O code
- Enabling the core to remain generic while supporting arbitrary physics extensions
- Maintaining the boundary between core infrastructure and physics implementations

The `physics_output_transformers.c/.h` files serve as the **user-facing API** that makes modular physics development possible while preserving architectural separation.

## Testing and Troubleshooting

### Testing
The transformer system is tested by `tests/test_property_system_hdf5.c`. Run tests with:

```bash
make test_property_system_hdf5
./tests/test_property_system_hdf5
```

### Common Issues and Solutions

1. **Missing Transformer Function**: Linker error "undefined reference" 
   - **Solution**: Ensure function is declared in `physics_output_transformers.h` and implemented in `.c`

2. **Type Mismatch Errors**: Data corruption or crashes
   - **Solution**: Ensure output buffer cast matches the expected HDF5 output type

3. **Rebuilding Issues**: Changes don't take effect
   - **Solution**: Clean rebuild to regenerate dispatcher: `make clean && make`

4. **Array Size Issues**: Buffer overruns
   - **Solution**: Use `get_property_array_size()` rather than assuming fixed sizes

## Related Documentation

- See `docs/property_dispatcher_system.md` for details on property access implementation
- See `docs/core_physics_separation.md` for the broader context of core-physics separation
- See `docs/hdf5_galaxy_output.md` for details on the HDF5 output system
- See `docs/io_hdf5_output_guide.md` for user guide on the I/O interface

---

*This guide provides comprehensive information for physics module developers on using the SAGE Physics Output Transformers system.*
