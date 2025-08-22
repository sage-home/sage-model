# SAGE Property and Parameter Schema Reference

**Version**: 2.1.0  
**Purpose**: Comprehensive reference for SAGE's metadata-driven property and parameter system  
**Audience**: Developers implementing code generation and module system  

---

## Overview

The SAGE schema system transforms the hardcoded property and parameter definitions into a metadata-driven architecture that enables:

- **Runtime modularity**: Properties and parameters available based on loaded physics modules
- **Compile-time optimization**: Build configurations with only required properties
- **Type safety**: Generated code with compile-time property access validation
- **Memory efficiency**: Optimized memory layouts and bounded allocation
- **Dynamic I/O**: Output formats that adapt to available properties

## Schema Architecture

### Core Principles

1. **Module Awareness**: Every property and parameter declares which modules provide/require it
2. **Multi-Dimensional Organization**: Properties organized by module, access pattern, memory layout, and I/O requirements  
3. **Availability Matrix**: Systematic mapping of property availability based on module combinations
4. **Code Generation Ready**: Metadata structured to directly support type-safe code generation
5. **Validation Framework**: Built-in validation rules and dependency checking

### File Structure

```
schema/
├── properties.yaml      # Galaxy property definitions with module dependencies
├── parameters.yaml      # Simulation parameter definitions with inheritance
└── schema-reference.md  # This documentation
```

---

## Property Schema (`properties.yaml`)

### Property Definition Structure

Each property is defined with comprehensive metadata:

```yaml
PropertyName:
  type: float                    # C data type
  category: physics              # Module category (core/physics)
  access_group: baryonic         # How property is typically accessed
  memory_group: physics_block    # Memory layout optimization
  io_group: essential            # I/O inclusion strategy
  description: "Property description"
  units: "1e10 Msun/h"          # Physical units
  range: [0.0, 100.0]           # Valid value range
  required_by: [starformation]  # Modules that need this property
  provided_by: [starformation]  # Modules that calculate this property
  io_mappings:                   # Format-specific output mappings
    hdf5: "OutputFieldName"
    binary: "LegacyFieldName"
```

### Property Categories

#### Core Properties
**Always available** - Physics-agnostic infrastructure properties:
- Identification: `SnapNum`, `Type`, `GalaxyNr`, `HaloNr`, `GalaxyIndex`
- Spatial: `Pos`, `Vel` 
- Halo: `Len`, `Mvir`, `Rvir`, `Vvir`, `Vmax`

#### Physics Properties  
**Conditionally available** - Depend on loaded physics modules:
- Baryonic masses: `ColdGas`, `StellarMass`, `HotGas`, `EjectedMass`
- Metal content: `MetalsColdGas`, `MetalsStellarMass`, etc.
- Star formation: `SfrDisk[]`, `SfrBulge[]` (dynamic arrays)
- Evolution tracking: `mergeType`, `MergTime`, `DiskScaleRadius`

### Multi-Dimensional Organization

#### By Module (`by_module`)
- **core**: Always available (physics-agnostic)
- **cooling**: Cooling/heating physics module
- **starformation**: Star formation and feedback module
- **mergers**: Galaxy merger physics module
- **reincorporation**: Gas reincorporation module
- **disk_instability**: Disk instability module
- **misc**: Miscellaneous physics processes

#### By Access Pattern (`by_access`)
- **identification**: Galaxy ID and tracking properties
- **spatial**: Position, velocity, and spatial properties
- **halo**: Halo-related properties
- **baryonic**: Baryonic mass components
- **metals**: Metal content tracking
- **star_formation**: Star formation history and rates
- **evolution**: Time-dependent evolution properties

#### By Memory Layout (`by_memory`)
- **core_block**: Frequently accessed properties (cache-friendly, always allocated)
- **physics_block**: Physics properties (conditionally allocated based on modules)
- **arrays_block**: Variable-sized arrays (separate allocation)
- **derived_block**: Calculated properties (lazy evaluation)

#### By I/O Strategy (`by_io`)
- **essential**: Always included in output (has `io_mappings`)
- **optional**: Included based on configuration (has `io_mappings`)
- **derived**: Calculated at output time from other properties (has `io_mappings`)
- **internal**: Never output - used only for internal calculations (NO `io_mappings`)

**Critical I/O Rule**: Properties are included in output **if and only if** they define `io_mappings` in the schema. Properties without `io_mappings` are automatically skipped from all output formats.

### Property Availability Matrix

Defines systematic property availability based on module combinations:

```yaml
availability_matrix:
  core_only:
    # Minimal property set for physics-free operation
    includes: [SnapNum, Type, GalaxyNr, ..., Vmax]
    excludes: []
    
  minimal_physics:
    # Basic physics with cooling and star formation
    includes: [core_only, ColdGas, StellarMass, HotGas, EjectedMass]
    modules: [cooling, starformation]
    
  standard_physics:
    # Full physics suite
    includes: [minimal_physics, BulgeMass, BlackHoleMass, ...]
    modules: [cooling, starformation, mergers, reincorporation, disk_instability]
    
  custom:
    # Runtime-determined based on loaded modules
    dynamic: true
```

### Dynamic Array Support

Properties can have variable sizes determined at build/runtime:

```yaml
SfrDisk:
  type: float[]
  array_size: "${STEPS}"        # Dynamic from configuration
  dynamic_array: true
  description: "Star formation rate per integration step"
```

### Memory Layout Specifications

Guides code generation for optimal memory organization:

```yaml
memory_layouts:
  core_struct:
    properties: [SnapNum, Type, GalaxyNr, ...]
    alignment: 64                # Cache line alignment
    
  physics_struct:
    properties: [ColdGas, StellarMass, ...]
    conditional: true            # Only allocated if physics modules loaded
    
  arrays_struct:
    properties: [SfrDisk, SfrBulge, ...]
    dynamic_allocation: true     # Separately allocated arrays
```

### I/O Format Integration

Properties define how they map to different output formats:

```yaml
io_formats:
  hdf5:
    groups:
      "/": [SnapNum, Type, GalaxyNr]
      "/HaloProperties": [Mvir, Rvir, Vvir]
      "/BaryonicProperties": [ColdGas, StellarMass, ...]
      
    derived_fields:
      calculated_sfr_disk:
        source: "SfrDisk"
        transformation: "sum_and_convert_to_rate"
        units: "Msun/yr"
```

---

## Parameter Schema (`parameters.yaml`)

### Parameter Definition Structure

```yaml
ParameterName:
  type: double                   # Parameter data type
  category: starformation        # Module category
  description: "Parameter description"
  units: "dimensionless"        # Physical units
  required_by: [starformation]  # Modules that need this parameter
  default: 0.05                 # Default value
  range: [0.001, 1.0]           # Valid range
  validation:                    # Validation rules
    range_check: true
    dependency: "param1 < param2"
  io_mappings:                   # Format-specific mappings
    par: "SfrEfficiency"         # Legacy .par format
    json: "physics.starformation.efficiency"  # JSON hierarchy
```

### Parameter Categories

- **core**: Always required (physics-agnostic)
- **cosmology**: Cosmological parameters
- **units**: Unit conversion and scaling
- **io**: Input/output configuration
- **simulation**: Simulation setup and control
- **[physics modules]**: Module-specific parameters

### Parameter Inheritance

Hierarchical configuration system:

```yaml
inheritance:
  base:
    description: "Base parameters required for all configurations"
    categories: [core, cosmology, units, io, simulation]
    
  minimal_physics:
    inherits: base
    description: "Minimal physics with cooling and star formation"
    categories: [cooling, starformation]
    
  standard_physics:
    inherits: minimal_physics
    description: "Standard physics suite with all modules"
    categories: [mergers, reincorporation, disk_instability, reionization, misc]
```

### Module Control Parameters

Special parameters that control which physics modules are active:

```yaml
AGNrecipeOn:
  type: int32_t
  category: starformation
  description: "Enable AGN feedback (0=off, 1=on)"
  module_control: true          # Controls module activation
  validation:
    enum_values: [0, 1]
```

### Derived Parameters

Parameters calculated from other parameters:

```yaml
UnitTime_in_s:
  type: double
  derived: true
  calculation: "UnitLength_in_cm / UnitVelocity_in_cm_per_s"
  description: "Time unit conversion to seconds"
```

### Cross-Parameter Validation

Rules ensuring parameter consistency:

```yaml
dependency_rules:
  cosmology_consistency:
    description: "Cosmological parameters must be physically reasonable"
    rules:
      - "Omega + OmegaLambda <= 1.1"
      - "Omega > 0.0"
      - "Hubble_h > 0.0"
    categories: [cosmology]
```

---

## Code Generation Integration

### Generated Code Structure

The schema enables generation of:

1. **Property Access Macros**: Type-safe property access with availability checking
2. **Structure Definitions**: Optimized memory layouts based on schema
3. **Initialization Functions**: Property and parameter setup
4. **Validation Functions**: Runtime checking of property/parameter consistency
5. **I/O Functions**: Dynamic output generation based on available properties

### Example Generated Code Patterns

#### Property Access
```c
// Generated from schema
#define GALAXY_GET_COLDGAS(g) \
    (PHYSICS_MODULE_LOADED(cooling) ? (g)->physics.ColdGas : \
     SAGE_PROPERTY_NOT_AVAILABLE("ColdGas", "cooling"))

// Compile-time availability checking
#if PHYSICS_MODULES_INCLUDE_COOLING
    #define COLDGAS_AVAILABLE 1
#else
    #define COLDGAS_AVAILABLE 0
#endif
```

#### Structure Generation
```c
// Generated core structure (always present)
struct galaxy_core {
    int32_t SnapNum;
    int32_t Type;
    int32_t GalaxyNr;
    // ... other core properties
} __attribute__((aligned(64)));  // Cache line aligned

// Generated physics structure (conditional)
#if PHYSICS_MODULES_LOADED
struct galaxy_physics {
    float ColdGas;
    float StellarMass;
    // ... other physics properties
};
#endif

// Main galaxy structure
struct GALAXY {
    struct galaxy_core core;
#if PHYSICS_MODULES_LOADED
    struct galaxy_physics physics;
#endif
    // Dynamic arrays allocated separately
    float *SfrDisk;    // Size determined by STEPS parameter
    float *SfrBulge;
};
```

#### Parameter Validation
```c
// Generated parameter validation
int validate_parameters(const struct params *p) {
    // Range checking
    if (p->SfrEfficiency < 0.001 || p->SfrEfficiency > 1.0) {
        return SAGE_INVALID_PARAMETER;
    }
    
    // Cross-parameter validation
    if (p->Omega + p->OmegaLambda > 1.1) {
        return SAGE_COSMOLOGY_INCONSISTENT;
    }
    
    // Module-dependent validation
#if PHYSICS_MODULE_REIONIZATION
    if (p->ReionizationOn && p->Reionization_zr >= p->Reionization_z0) {
        return SAGE_REIONIZATION_INCONSISTENT;
    }
#endif
    
    return SAGE_SUCCESS;
}
```

#### Dynamic I/O Generation
```c
// Generated HDF5 output function
int write_galaxies_hdf5(const struct GALAXY *galaxies, int ngals, 
                       const struct params *params) {
    // Core properties (always written)
    write_hdf5_field(file_id, "SnapNum", galaxies, ngals, 
                     offsetof(struct GALAXY, core.SnapNum), H5T_NATIVE_INT32);
    
    // Physics properties (conditionally written)
#if PHYSICS_MODULE_COOLING
    if (module_is_loaded(COOLING_MODULE)) {
        write_hdf5_field(file_id, "ColdGas", galaxies, ngals,
                        offsetof(struct GALAXY, physics.ColdGas), H5T_NATIVE_FLOAT);
    }
#endif
    
    // Derived fields (calculated at output time)
    float *sfr_total = calculate_total_sfr(galaxies, ngals);
    write_hdf5_field(file_id, "StarFormationRate", sfr_total, ngals, 
                     0, H5T_NATIVE_FLOAT);
    sage_free(sfr_total);
    
    return SAGE_SUCCESS;
}
```

### Build System Integration

The schema supports multiple build configurations:

1. **Full Build**: All modules and properties available
2. **Minimal Build**: Only core properties and basic physics
3. **Custom Build**: User-selected module combinations
4. **Debug Build**: Additional validation and debugging properties

Build system generates appropriate compile-time flags:
```makefile
# Generated from schema based on selected modules
CFLAGS += -DPHYSICS_MODULE_COOLING=1
CFLAGS += -DPHYSICS_MODULE_STARFORMATION=1
CFLAGS += -DSTEPS=$(INTEGRATION_STEPS)
CFLAGS += -DSAGE_PROPERTIES_VERSION=\"2.1.0\"
```

---

## Usage Guidelines

### For Module Developers

1. **Adding New Properties**: 
   - Define in `properties.yaml` with appropriate module dependencies
   - Specify memory layout and I/O requirements
   - Add validation rules if needed

2. **Adding New Parameters**:
   - Define in `parameters.yaml` with module category
   - Specify validation rules and dependencies
   - Add to appropriate inheritance hierarchy

3. **Module Integration**:
   - Use generated property access macros
   - Check property availability at runtime if needed
   - Follow memory layout recommendations

### For Build System Integration

1. **Code Generation Pipeline**:
   - Parse schema files during build
   - Generate headers based on selected modules
   - Create validation and I/O functions

2. **Configuration Management**:
   - Support multiple build configurations
   - Generate appropriate compiler flags
   - Handle dynamic array sizing

3. **Testing Integration**:
   - Generate property-aware test frameworks
   - Validate schema consistency
   - Test different module combinations

### For I/O System Integration

1. **Dynamic Output**:
   - Use schema I/O mappings for field generation
   - Support derived field calculations
   - Handle format-specific requirements

2. **Input Validation**:
   - Use parameter validation rules
   - Check module compatibility
   - Validate property consistency

---

## Migration Strategy

### Phase 1: Schema Definition (Current)
- Create comprehensive property and parameter schemas
- Document conventions and usage patterns
- Validate schema completeness against existing code

### Phase 2: Code Generation (Next)
- Implement schema-based code generation
- Generate type-safe property access
- Create validation frameworks

### Phase 3: Core Integration
- Migrate core properties to new system
- Update build system integration
- Maintain backward compatibility

### Phase 4: Module Migration
- Migrate physics modules to use generated code
- Update I/O system for dynamic output
- Implement runtime module loading

### Phase 5: Optimization
- Optimize memory layouts based on schema
- Implement compile-time property optimization
- Add advanced validation features

---

## Schema Validation

### Consistency Checks

The schema system includes built-in validation:

1. **Property Consistency**:
   - All required_by modules exist
   - All provided_by modules exist
   - Memory groups are properly defined
   - I/O mappings are consistent

2. **Parameter Consistency**:
   - All inheritance chains are valid
   - Cross-parameter dependencies are resolvable
   - Module requirements are satisfiable
   - Validation rules are complete

3. **Integration Consistency**:
   - Property/parameter module dependencies align
   - Build configurations are feasible
   - I/O formats support all property types

### Validation Tools

Schema validation should be performed:
- During build (as part of code generation)
- During testing (validate generated code)
- During CI/CD (ensure schema integrity)

---

## Future Enhancements

### Planned Features

1. **Advanced Property Types**:
   - Complex data structures
   - Nested property groups
   - Template-based properties

2. **Enhanced Module System**:
   - Dynamic module loading at runtime
   - Module versioning and compatibility
   - Inter-module communication patterns

3. **Optimization Features**:
   - Property access profiling
   - Memory layout optimization
   - Cache-aware data structures

4. **Validation Enhancements**:
   - Scientific validation rules
   - Performance validation
   - Regression testing integration

### Extensibility

The schema system is designed to be extensible:
- New property types can be added
- New validation rules can be defined
- New I/O formats can be supported
- New optimization strategies can be implemented

This metadata-driven approach provides a solid foundation for SAGE's transformation into a modular, physics-agnostic architecture while maintaining scientific accuracy and performance.