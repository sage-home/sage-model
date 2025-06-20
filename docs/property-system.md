# SAGE Property System Guide

## Overview

The SAGE Property System is a metadata-driven architecture that manages galaxy properties through auto-generated C code from YAML definitions. It enables build-time optimization, type safety, and clean separation between core infrastructure and physics implementations.

## Core Concepts

### Metadata-Driven Architecture

SAGE uses **explicit property build system** that controls which galaxy properties are compiled at build-time, optimizing memory usage and execution performance by including only needed properties.

**Key Principle**: Property structures are determined at **build-time**, but module configuration happens at **runtime**.

- **Properties**: Defined in `properties.yaml` and compiled into C structures  
- **Parameters**: Defined in `parameters.yaml` and compiled into accessor functions
- **Modules**: Configured via JSON files and loaded at runtime

## Build System

### Build Targets

#### `make physics-free`
**Purpose**: Core-only execution without any physics modules.

**Properties Generated**: Only core properties marked with `is_core: true`
- Galaxy identification (GalaxyIndex, Type, SnapNum)
- Galaxy lifecycle (merged flag for clean state management)
- Positions and velocities (Pos, Vel)
- Basic halo properties (Mvir, Rvir, etc.)

**Use Cases**:
- Testing core infrastructure
- Property pass-through validation
- Minimal memory usage scenarios
- Core algorithm development

**Memory Impact**: Minimal - only essential properties allocated

#### `make full-physics`
**Purpose**: Maximum compatibility with all available physics modules.

**Properties Generated**: All properties from `properties.yaml`
- Core properties + all physics properties
- Star formation histories (SfrDisk, SfrBulge)
- Gas properties (ColdGas, HotGas, etc.)
- Merger properties (MergTime, mergeType, mergeIntoID, etc.)
- All module-specific properties

**Use Cases**:
- Full scientific simulations
- Module development and testing
- Maximum flexibility for runtime configuration
- Production runs with uncertain module requirements

**Memory Impact**: Maximum - all properties allocated regardless of usage

#### `make custom-physics CONFIG=file.json`
**Purpose**: Optimised build for specific module configurations.

**Properties Generated**: Core properties + properties required by modules specified in the config file

**Use Cases**:
- Production runs with known module requirements
- Optimised memory usage for specific physics combinations
- Reproducible science with explicit property sets

**Example**:
```bash
make custom-physics CONFIG=input/star_formation_only.json
```

### Usage Examples

#### Development Workflow
```bash
# For core infrastructure work
make clean
make physics-free
./sage millennium.par

# For full physics development
make clean  
make full-physics
./sage millennium.par input/full_config.json

# For specific module testing
make clean
make custom-physics CONFIG=input/cooling_only.json
./sage millennium.par input/cooling_only.json
```

#### Production Workflow
```bash
# Optimised build for known configuration
make clean
make custom-physics CONFIG=production_config.json
./sage millennium.par production_config.json
```

### Configuration File Requirements

For `custom-physics` builds, your JSON configuration must specify the modules to be used:

```json
{
  "modules": {
    "instances": [
      {
        "name": "CoolingModule",
        "type": "cooling"
      },
      {
        "name": "StarFormationModule", 
        "type": "star_formation"
      }
    ]
  }
}
```

### Performance Characteristics

| Build Target | Compilation Time | Memory Usage | Runtime Performance | Use Case |
|--------------|------------------|--------------|-------------------|----------|
| `physics-free` | Fastest | Minimal | Fastest | Development/Testing |
| `custom-physics` | Medium | Optimised | Fast | Production |
| `full-physics` | Slowest | Maximum | Good | Development/Flexibility |

## Property Definitions

### YAML Structure

Properties are defined in `src/properties.yaml` with the following structure:

```yaml
properties:
  - name: "PropertyName"
    type: "float|int32_t|double|long long|uint64_t"
    units: "Physical units"
    description: "Detailed description"
    is_core: true|false
    is_array: false|true
    array_len: 3|STEPS|0  # 0 for dynamic arrays
    size_parameter: "simulation.NumSnapOutputs"  # For dynamic arrays
    output_field: true|false
    read_only_level: 0|1|2
    output_transformer_function: "function_name"  # Optional
```

### Property Categories

#### Core Properties (`is_core: true`)
- Fundamental to SAGE infrastructure
- Always present and managed by core infrastructure
- Direct access allowed via `GALAXY_PROP_*` macros in core code
- Examples: galaxy identifiers, positions, velocities, basic halo properties

#### Physics Properties (`is_core: false`)
- Specific to physics processes
- May or may not be present depending on loaded physics modules
- **Must** use generic property accessors from any code
- Examples: gas masses, star formation rates, merger times

### Dynamic Arrays

Properties can be defined as dynamic arrays that resize based on simulation parameters:

```yaml
- name: "SfrDisk"
  type: "float"
  is_array: true
  array_len: 0  # Dynamic array
  size_parameter: "simulation.NumSnapOutputs"
  description: "Star formation rate in disk [Msun/yr]"
```

## Type-Safe Property Access

### Core-Physics Separation Principles

**Access Rules:**

1. **Core Properties**: Direct access using `GALAXY_PROP_*` macros allowed in core code
2. **Physics Properties**: Must use generic property accessors from ALL code (including physics modules)
3. **Availability Checking**: Always check property existence before access

### Implementation Example

```c
// Core properties - direct macro access allowed in core code
GALAXY_PROP_Mvir(galaxy) = 1.5e12f;
int64_t index = GALAXY_PROP_GalaxyIndex(galaxy);
GALAXY_PROP_merged(galaxy) = 0;  // New core property for lifecycle management

// Physics properties - generic accessors only (including from core code)
property_id_t mergtime_id = get_property_id("MergTime");
if (has_property(galaxy, mergtime_id)) {
    set_float_property(galaxy, mergtime_id, 5.2f);
    float mergtime = get_float_property(galaxy, mergtime_id, 999.9f);
}

// Example of availability checking for physics properties
property_id_t coldgas_id = get_property_id("ColdGas"); 
if (has_property(galaxy, coldgas_id)) {
    set_float_property(galaxy, coldgas_id, 2.5e10f);
}
```

### Type-Safe Dispatcher Functions

The property system uses auto-generated type-safe dispatcher functions that replace flawed macro-based approaches:

```c
// Auto-generated dispatcher function example
float get_generated_float(const galaxy_properties_t *props, property_id_t prop_id, float default_value) {
    if (!props) return default_value;
    switch (prop_id) {
        case PROP_Mvir: return props->Mvir;
        case PROP_Rvir: return props->Rvir;
        // Other float properties...
        default: return default_value;
    }
}
```

### Property Utility Functions

```c
// Generic property access functions (from core_property_utils.h)
float get_float_property(const struct GALAXY *galaxy, property_id_t prop_id, float default_value);
int32_t get_int32_property(const struct GALAXY *galaxy, property_id_t prop_id, int32_t default_value);
void set_float_property(struct GALAXY *galaxy, property_id_t prop_id, float value);
void set_int32_property(struct GALAXY *galaxy, property_id_t prop_id, int32_t value);
bool has_property(const struct GALAXY *galaxy, property_id_t prop_id);
property_id_t get_property_id(const char *property_name);
property_id_t get_cached_property_id(const char *property_name);
```

### Array Property Access

```c
// Access array properties
property_id_t sfr_id = get_property_id("SfrDisk");
if (has_property(galaxy, sfr_id)) {
    // Get array size
    int array_size = get_array_size(galaxy, sfr_id);
    
    // Access specific elements
    for (int i = 0; i < array_size; i++) {
        float sfr_value = get_float_array_element(galaxy, sfr_id, i, 0.0f);
        set_float_array_element(galaxy, sfr_id, i, sfr_value * 1.1f);
    }
}
```

## Auto-Generated Files

### Property System Files (from `src/properties.yaml`)

1. **`src/core/core_properties.h`**: Property definitions and accessor declarations
2. **`src/core/core_properties.c`**: Property implementation and type-safe accessors
3. **`src/core/generated_output_transformers.c`**: Output transformation dispatch system

### Parameter System Files (from `src/parameters.yaml`)

1. **`src/core/core_parameters.h`**: Parameter definitions and accessor declarations  
2. **`src/core/core_parameters.c`**: Parameter implementation with validation

### Generation Process

All files are generated by `src/generate_property_headers.py` which:
- Reads YAML metadata definitions
- Generates type-safe C code with comprehensive validation
- Includes proper file headers with generation timestamps
- Integrates with the build system for automatic regeneration

## Output Transformers

### Purpose

Output transformers allow custom property transformations before writing to output files:

- Unit conversions (internal units → output units)
- Derived quantity calculations
- Physics-specific formatting
- Data aggregation or filtering

### Implementation

```yaml
- name: "StellarMass"
  output_transformer_function: "transform_stellar_mass_to_solar_masses"
```

```c
// Example transformer function
double transform_stellar_mass_to_solar_masses(const struct GALAXY *galaxy, property_id_t prop_id) {
    double mass_internal = get_double_property(galaxy, prop_id, 0.0);
    return mass_internal * UnitMass_in_g / SOLAR_MASS;
}
```

The auto-generated `generated_output_transformers.c` provides a dispatch system for all transformer functions.

## Benefits

### Modularity
- Physics modules can be added, removed, or replaced without affecting core
- Different property sets for different physics combinations
- Runtime adaptability to any combination of physics modules

### Performance
- Build-time optimization includes only needed properties
- Type-safe access prevents runtime type errors
- Direct struct field access more efficient than pointer arithmetic

### Memory Efficiency
- Physics-free mode uses minimal memory (core properties only)
- Custom builds include only required properties
- Dynamic arrays resize based on simulation parameters

### Type Safety
- Auto-generated dispatcher functions prevent type errors
- Compile-time validation of property access patterns
- Runtime bounds checking for array access

### Maintainability
- Single source of truth for property definitions (YAML)
- Auto-generated code stays in sync with definitions
- Clear separation between core and physics concerns

## Important Notes

### Recompilation Requirements

**Different property configurations require recompilation.** Always run `make clean` when switching between build targets:

```bash
# CORRECT workflow
make clean
make physics-free
# ... test core functionality
make clean  
make full-physics
# ... test with physics modules
```

### Property Warnings

When properties are missing (e.g., using `physics-free` but referencing physics properties), you may see warnings like:
```
Warning: Unknown property type 'float[STEPS]' for property 'SfrDisk'. Defaulting to float.
```

This is normal and indicates the property system is working correctly - physics properties are not available in physics-free mode.

### Stamp File System

The build system uses stamp files (`.stamps/generate_properties_*.stamp`) to track which property configuration was last generated. The clean target removes these automatically.

## Integration with Module System

The property build system is independent of but compatible with the module system:

1. **Build-time**: Properties are generated based on intended module usage
2. **Runtime**: Modules are loaded based on JSON configuration
3. **Validation**: System ensures loaded modules have their required properties available

## Troubleshooting

### Problem: "Property not found" errors
**Solution**: Ensure your build target includes the required properties. Use `full-physics` for maximum compatibility.

### Problem: Memory usage too high
**Solution**: Use `custom-physics` with a minimal configuration, or `physics-free` for core-only work.

### Problem: Module loading fails
**Solution**: Verify your JSON configuration matches your build target. Modules requiring specific properties need those properties to be built.

### Problem: Build fails after switching targets
**Solution**: Always run `make clean` before switching between different property build targets.

## Testing

### Property System Tests

- `test_property_array_access` - Validates property access follows separation principles
- `test_property_system_hdf5` - Tests property system integration with I/O separation  
- `test_property_access_comprehensive` - Comprehensive validation of property system for core-physics separation
- `test_property_yaml_validation` - Tests YAML property definition validation and parsing
- `test_dispatcher_access` - Tests type-safe dispatcher functions for property access

### Running Property Tests

```bash
make property_tests           # Run all property system tests
make test_property_array_access  # Test property access patterns
make test_property_system_hdf5   # Test HDF5 integration
```

## Future Extensions

The property system provides a foundation for:
- Build-time module validation
- Automated property dependency analysis  
- Performance profiling by property set
- Custom property subsets for specific science cases
- Advanced property transformation pipelines