# SAGE Model Refactoring Plan

## Executive Summary

This document outlines a comprehensive plan for refactoring the SAGE (Semi-Analytic Galaxy Evolution) codebase to achieve two primary goals:

1. **Runtime Functional Modularity**: Implementing a dynamic plugin architecture that allows physics modules to be inserted, replaced, reordered, or removed at runtime without recompilation.

2. **Optimized Merger Tree Processing**: Enhancing the efficiency of merger tree handling through improved memory layout, unified I/O abstraction, and intelligent caching strategies while preserving existing data structures and formats.

The plan includes advanced features to maximize flexibility and performance:

- **Galaxy Property Extension Mechanism**: Allowing modules to add custom properties to galaxies without modifying core structures
- **Event-Based Communication System**: Facilitating module interaction without direct dependencies
- **Advanced Memory Optimizations**: Implementing cache-friendly data structures and memory pooling
- **Comprehensive Development Tools**: Providing tools for module creation, testing, and validation

This transformation will create a modular, extensible platform for galaxy evolution modeling that reduces barriers to scientific innovation while maintaining high performance.

## Current Architecture Assessment

### Strengths
- Modular organization with core functionality and physics modules
- Efficient depth-first traversal of merger trees
- Effective multi-file organization for large datasets
- Parallel processing support via MPI
- Support for multiple input formats (binary, HDF5, etc.)

### Challenges
- Static physics module implementation requiring recompilation for changes
- Tight coupling between physics modules
- Monolithic galaxy evolution process in `evolve_galaxies()`
- Global state and parameter passing
- Non-optimal memory layout for cache efficiency
- Limited abstraction across different input/output formats
- Duplicate code paths for different formats

## Detailed Refactoring Strategy

### Phase 1: Preparatory Refactoring (Foundation)

#### 1.1 Code Organization
- Reorganize directory structure to separate core, physics, and I/O components
- Extract physics functions from monolithic implementation while maintaining functionality
- Adopt consistent naming conventions across the codebase

#### 1.2 Global State Reduction
- Encapsulate global parameters in configuration structures
- Refactor function signatures to explicitly pass required parameters
- Implement proper initialization and cleanup routines for all modules

#### 1.3 Comprehensive Testing Framework
- Create validation tests for scientific outputs
- Implement unit tests for core functionality
- Develop benchmarks for performance measurement

#### 1.4 Documentation Enhancement
- Update inline documentation with clear purpose for each function
- Create architectural documentation with component relationships
- Document data flow between core and physics modules

### Phase 2: Enhanced Module Interface Definition

#### 2.1 Base Module Interfaces
- Define standard interfaces for each physics module type:
  ```c
  // Base interface that all module types will extend
  struct base_module {
      const char* name;
      const char* version;
      const char* author;
      
      int (*initialize)(struct params *params, void **module_data);
      int (*cleanup)(void *module_data);
  };
  
  // Example: Cooling module interface
  struct cooling_module {
      struct base_module base;
      
      // Core function that all cooling modules must implement
      double (*calculate_cooling)(const int gal, const double dt, 
                                 struct GALAXY *galaxies, void *module_data);
      
      // Optional utility functions
      double (*get_cooling_rate)(double temp, double metallicity, void *module_data);
      double (*get_cooling_radius)(const int gal, struct GALAXY *galaxies, void *module_data);
  };
  ```

#### 2.2 Galaxy Property Extension Mechanism
- Add extension capability to the GALAXY structure:
  ```c
  struct GALAXY {
      // Standard galaxy properties remain unchanged
      int32_t SnapNum;
      int32_t Type;
      float ColdGas;
      // ... other existing properties ...
      
      // Extension mechanism
      void **extension_data;        // Array of pointers to module-specific data
      int num_extensions;           // Number of registered extensions
      uint64_t extension_flags;     // Bitmap to track which extensions are in use
  };
  ```
- Create property registration system:
  ```c
  typedef struct {
      char name[32];                // Property name
      size_t size;                  // Size in bytes
      int module_id;                // Which module owns this
      
      // Serialization functions
      void (*serialize)(void *src, void *dest, int count);
      void (*deserialize)(void *src, void *dest, int count);
      
      // Metadata
      char description[128];
      char units[32];
  } galaxy_property_t;
  ```
- Implement memory management for extensions
- Create access APIs for extension properties

#### 2.3 Event-Based Communication System
- Define event types and structure:
  ```c
  typedef enum {
      EVENT_COLD_GAS_UPDATED,
      EVENT_STAR_FORMATION_OCCURRED,
      EVENT_FEEDBACK_APPLIED,
      EVENT_COOLING_COMPLETED,
      EVENT_AGN_ACTIVITY,
      EVENT_MERGER_DETECTED,
      // ... other events
      EVENT_MAX
  } galaxy_event_type_t;
  
  typedef struct {
      galaxy_event_type_t type;
      int galaxy_index;
      int step;
      void *data;           // Event-specific data
      size_t data_size;
  } galaxy_event_t;
  ```
- Implement registration and dispatch mechanisms
- Create event handlers for module communication

#### 2.4 Module Registry System
- Implement a central registry for physics modules:
  ```c
  typedef struct {
      // Array of loaded modules
      struct {
          char name[64];
          char type[32];
          void* handle;       // dlopen/LoadLibrary handle
          void* interface;    // Pointer to module interface
          int active;         // Whether module is active
      } modules[MAX_MODULES];
      int num_modules;
      
      // Quick lookup for active modules by type
      struct {
          char type[32];
          int module_index;   // Index into modules array
      } active_modules[MAX_MODULE_TYPES];
      int num_types;
  } module_registry_t;
  ```

#### 2.5 Module Pipeline System
- Implement a configurable pipeline for module execution:
  ```c
  typedef struct {
      // Module types in execution order
      struct {
          char type[32];
          char name[64];      // Optional specific module name
      } steps[MAX_PIPELINE_STEPS];
      int num_steps;
  } module_pipeline_t;
  ```

#### 2.6 Configuration System
- Develop a configuration parser for module chains and parameters

### Phase 3: I/O Abstraction and Optimization

#### 3.1 I/O Interface Abstraction
- Create a unified I/O interface that preserves all existing functionality:
  ```c
  struct io_interface {
      int (*initialize)(const char* filename, struct params* params);
      int (*read_forest)(int64_t forestnr, struct halo_data** halos, struct forest_info* forest_info);
      int (*write_galaxies)(struct galaxy_output* galaxies, int ngals, struct save_info* save_info);
      int (*cleanup)();
  };
  ```

#### 3.2 Format-Specific Implementations
- Refactor existing I/O code into format-specific implementations of the interface
- Implement serialization support for extended properties

#### 3.3 Memory Optimization
- Implement buffered reading/writing while preserving existing formats
- Create memory mapping options for large files
- Develop efficient caching strategies for frequently accessed halos

### Phase 4: Advanced Plugin Infrastructure

#### 4.1 Dynamic Library Loading
- Implement cross-platform library loading:
  ```c
  #ifdef _WIN32
  #include <windows.h>
  typedef HMODULE module_handle_t;
  #else
  #include <dlfcn.h>
  typedef void* module_handle_t;
  #endif
  
  module_handle_t load_module(const char* path);
  void* get_symbol(module_handle_t handle, const char* symbol_name);
  void unload_module(module_handle_t handle);
  ```

#### 4.2 Module Development Framework
- Create template generation for new modules
- Implement module validation framework
- Develop debugging utilities

#### 4.3 Parameter Tuning System
- Implement parameter registration and validation
- Create runtime modification capabilities
- Add parameter bounds checking

#### 4.4 Module Discovery and Loading
- Implement directory scanning for modules
- Create manifest parsing
- Develop dependency resolution

#### 4.5 Error Handling
- Design comprehensive error reporting system

### Phase 5: Core Module Migration

#### 5.1 Refactoring Main Evolution Loop
- Transform the monolithic `evolve_galaxies()` function to use the pipeline system
- Implement event-based communication
- Add support for extension properties

#### 5.2 Converting Physics Modules
- Extract each physics component to a standalone module
- Add property registration and event handling
- Implement module-specific data management

#### 5.3 Validation and Testing
- Create scientific validation tests
- Implement performance benchmarks
- Develop compatibility tests

### Phase 6: Advanced Performance Optimization

#### 6.1 Memory Layout Enhancement
- Optimize struct layouts for cache efficiency:
  ```c
  // Structure of arrays for hot-path galaxy properties
  typedef struct {
      float *ColdGas;
      float *HotGas;
      float *StellarMass;
      float *BulgeMass;
      
      float *MetalsColdGas;
      float *MetalsHotGas;
      float *MetalsStellarMass;
      float *MetalsBulgeMass;
      
      int capacity;
      int count;
  } galaxy_hotpath_data_t;
  ```
- Implement memory pooling for galaxy allocations
- Reduce memory fragmentation during tree processing

#### 6.2 Tree Traversal Optimization
- Implement prefetching for depth-first traversal:
  ```c
  // Tree node prefetching system
  typedef struct {
      struct halo_data **prefetch_buffer;
      int *prefetch_indices;
      int buffer_size;
      int num_prefetched;
      int next_index;
  } prefetch_system_t;
  ```
- Enhance depth-first traversal algorithms
- Optimize pointer-chasing patterns for modern CPUs

#### 6.3 Vectorized Physics Calculations
- Implement SIMD-enabled calculations:
  ```c
  #ifdef __SSE4_1__
  #include <smmintrin.h>
  
  // Process cooling for 4 galaxies at once
  void calculate_cooling_vectorized(int *galaxy_indices, int num_galaxies,
                                 double dt, struct GALAXY *galaxies,
                                 double *cooling_results) {
      // SIMD implementation
  }
  #endif
  ```
- Create batch processing for galaxies
- Optimize performance-critical algorithms

#### 6.4 Parallelization Enhancements
- Refine load balancing for heterogeneous trees
- Implement finer-grained parallel processing where beneficial
- Optimize MPI communication patterns

### Phase 7: Documentation and Tools

#### 7.1 Developer Documentation
- Create comprehensive guide for module development
- Document plugin architecture interfaces
- Provide examples for common customizations

#### 7.2 Scientific Documentation
- Document physical models and implementation details
- Create validation reports
- Develop scientific usage guides

#### 7.3 Tool Development
- Create template modules for different physics components
- Develop validation tools for module compatibility
- Implement module dependency analyzer

## Implementation Plan

### Timeline Estimate

| Phase | Description | Duration (months) | Dependencies |
|-------|-------------|-------------------|-------------|
| 1 | Preparatory Refactoring | 3 | None |
| 2 | Enhanced Module Interface Definition | 4-5 | Phase 1 |
| 3 | I/O Abstraction and Optimization | 2-3 | Phase 1 |
| 4 | Advanced Plugin Infrastructure | 3-4 | Phase 2 |
| 5 | Core Module Migration | 4-5 | Phase 4 |
| 6 | Advanced Performance Optimization | 3-4 | Phases 3, 5 |
| 7 | Documentation and Tools | 2-3 | Phases 1-6 |

**Total Estimated Duration**: 21-27 months

The critical path runs through Phases 1 → 2 → 4 → 5 → 6 → 7, with Phase 3 able to proceed in parallel with Phase 2 after Phase 1 is complete.

## Runtime Module System: Practical Examples

### Example 1: Custom Star Formation with Property Extensions

```c
// Extension data structure for a new star formation module
typedef struct {
    float h2_fraction;               // Molecular hydrogen fraction
    float pressure;                  // ISM pressure
    struct {
        float radius;                // Radius of region
        float sfr;                   // Star formation rate
    } regions[MAX_SF_REGIONS];       // Multiple star forming regions
    int num_regions;                 // Number of active regions
} h2_sf_data_t;

// Module implementation
static int sf_module_process(int p, int centralgal, double time, double dt, 
                           int halonr, int step, struct GALAXY *galaxies, 
                           struct params *run_params, void *module_data) {
    // Get access to extension data
    h2_sf_data_t *sf_data = GALAXY_EXT(&galaxies[p], getCurrentModuleId(), h2_sf_data_t);
    
    // Calculate H2 fraction based on metallicity and pressure
    sf_data->pressure = calculate_pressure(p, galaxies);
    sf_data->h2_fraction = calculate_h2_fraction(galaxies[p].MetalsColdGas, 
                                               galaxies[p].ColdGas, 
                                               sf_data->pressure);
    
    // Calculate star formation based on H2
    float h2_mass = sf_data->h2_fraction * galaxies[p].ColdGas;
    float sf_rate = calculate_sf_rate_from_h2(h2_mass, sf_data->pressure);
    
    // Update galaxy properties
    galaxies[p].SfrDisk[step] = sf_rate;
    
    return 0;
}
```

### Example 2: Module Reordering with Event System

#### Configuration File:
```json
{
  "modules": [
    {"name": "infall", "library": "libinfall.so", "enabled": true},
    {"name": "cooling", "library": "libcooling.so", "enabled": true},
    
    // AGN feedback is now BEFORE star formation (was after in original code)
    {"name": "agn_feedback", "library": "libagn.so", "enabled": true},
    
    // Using a new star formation model
    {"name": "star_formation", "library": "libsfr_new.so", "enabled": true}
  ]
}
```

#### Event-Based Communication:
```c
// In the star formation module
static int sf_module_process(int p, int centralgal, double time, double dt, 
                           int halonr, int step, struct GALAXY *galaxies, 
                           void *module_data) {
    // ... calculate star formation ...
    float stars = calculate_stars_formed(p, galaxies, dt);
    
    // Create event data
    star_formation_event_data_t sf_data = {
        .stars_formed = stars,
        .stars_to_disk = stars_disk,
        .stars_to_bulge = stars_bulge,
        .metallicity = metallicity
    };
    
    // Create and dispatch event
    galaxy_event_t event = {
        .type = EVENT_STAR_FORMATION_OCCURRED,
        .galaxy_index = p,
        .step = step,
        .data = &sf_data,
        .data_size = sizeof(sf_data)
    };
    
    event_system_dispatch(global_event_system, &event);
    
    return 0;
}

// In the feedback module
static int handle_star_formation_event(galaxy_event_t *event, void *module_data) {
    star_formation_event_data_t *sf_data = (star_formation_event_data_t*)event->data;
    
    // Calculate feedback based on star formation
    float feedback_energy = sf_data->stars_formed * SUPERNOVA_ENERGY_PER_SOLAR_MASS;
    
    // ... process feedback ...
    
    return 0;
}
```

### Example 3: Performance Optimization with Cache-Friendly Structures

```c
// Transform from Array of Structures to Structure of Arrays for hot path
void optimize_galaxy_layout(struct GALAXY *galaxies, int ngal, galaxy_hotpath_data_t *hotpath) {
    // Allocate structure of arrays
    galaxy_hotpath_init(hotpath, ngal);
    
    // Copy data to cache-friendly layout
    for (int i = 0; i < ngal; i++) {
        hotpath->ColdGas[i] = galaxies[i].ColdGas;
        hotpath->HotGas[i] = galaxies[i].HotGas;
        hotpath->StellarMass[i] = galaxies[i].StellarMass;
        hotpath->BulgeMass[i] = galaxies[i].BulgeMass;
        
        hotpath->MetalsColdGas[i] = galaxies[i].MetalsColdGas;
        hotpath->MetalsHotGas[i] = galaxies[i].MetalsHotGas;
        hotpath->MetalsStellarMass[i] = galaxies[i].MetalsStellarMass;
        hotpath->MetalsBulgeMass[i] = galaxies[i].MetalsBulgeMass;
    }
    
    // Process data using vectorized operations
    process_galaxies_vectorized(hotpath, ngal);
    
    // Copy back to original structure
    for (int i = 0; i < ngal; i++) {
        galaxies[i].ColdGas = hotpath->ColdGas[i];
        galaxies[i].HotGas = hotpath->HotGas[i];
        galaxies[i].StellarMass = hotpath->StellarMass[i];
        galaxies[i].BulgeMass = hotpath->BulgeMass[i];
        
        galaxies[i].MetalsColdGas = hotpath->MetalsColdGas[i];
        galaxies[i].MetalsHotGas = hotpath->MetalsHotGas[i];
        galaxies[i].MetalsStellarMass = hotpath->MetalsStellarMass[i];
        galaxies[i].MetalsBulgeMass = hotpath->MetalsBulgeMass[i];
    }
}
```

## Risk Assessment and Mitigation

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Scientific accuracy compromised | Medium | Critical | Comprehensive validation tests against baseline results; incremental refactoring with validation at each step |
| Performance regression | Medium | High | Benchmarking at each phase; performance profiling; optimization of critical paths |
| Increased complexity | High | Medium | Thorough documentation; clear interfaces; developer guides; training sessions |
| Compatibility issues with existing analysis pipelines | Medium | High | Backward compatibility layers; gradual migration path; support for legacy interfaces |
| Resource limitations | Medium | Medium | Prioritize high-impact changes; modular approach allowing partial implementation |
| Performance overhead of extensions | Medium | High | Profile and optimize extension access; pooled allocations; batch processing |
| Serialization challenges with extensions | Medium | Medium | Thorough testing with real datasets; fallback mechanisms; format versioning |
| Module incompatibility | Medium | High | Strict versioning; dependency validation; compatibility tests |

## Deferred Work

This section documents work items that have been intentionally deferred to later stages of the project or deemed unnecessary in their originally planned form. These decisions help maintain focus on the critical path and ensure efficient use of resources.

### Performance Benchmarking
- **Original Phase**: 1.3 (Testing Enhancement)
- **New Target Phase**: 6 (Advanced Performance Optimization)
- **Rationale**: Implementing benchmark points now would be premature with significant code changes still ahead. Benchmarks would quickly become outdated as the codebase evolves. More value will be gained by applying benchmarking to the refactored codebase.
- **Implementation Plan**: Use a code profiler on the finalized structure rather than embedding benchmarking points in evolving code. This will provide more accurate performance insights once the core architecture is stabilized.
- **Components**: 
  - Development of timing measurement points
  - Baseline metrics for overall execution
  - Module-specific performance tracking
  - Hotspot identification framework

### Enhanced End-to-End Test Diagnostics
- **Original Phase**: 1.3 (Testing Enhancement)
- **Decision**: Skipped - existing approach deemed sufficient
- **Rationale**: The current combination of enhanced error logging, binary comparison through sagediff.py, knowledge of recent changes, and starting from a known-good state provides adequate debugging capabilities without needing to invest additional time in expanding test_sage.sh.
- **Alternative Approach**: Continue to leverage the existing test framework in conjunction with enhanced in-code logging for identifying discrepancies. Focus on using these tools effectively rather than expanding the test infrastructure.

## Conclusion

This refactoring plan provides a detailed, practical approach to modernizing the SAGE codebase. The enhanced plugin architecture will enable researchers to easily swap physics implementations and reorder processes without recompilation, significantly enhancing flexibility for experimentation. The property extension mechanism will allow for novel physics to be implemented without modifying core structures, and the event system will enable more flexible module interactions.

The advanced performance optimizations will improve efficiency while maintaining compatibility with existing data formats. By following an incremental approach with continuous validation, we can ensure that the scientific integrity of the model is preserved throughout the refactoring process.

The result will be a more flexible, maintainable, and efficient codebase that facilitates scientific exploration and collaboration while building on the solid foundation of the existing SAGE implementation.
