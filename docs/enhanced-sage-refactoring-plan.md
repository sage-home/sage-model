# Enhanced SAGE Model Refactoring Plan

## Goals and Overview

1. **Core-Physics Separation and Modularity**: Implement a modular architecture where core infrastructure is physics-agnostic and physics modules are self-contained units with standardized interfaces, allowing for clear separation of concerns and easier modification or replacement of physics components at compile time.
2. **Optimized Merger Tree Processing**: Enhance merger tree handling efficiency via improved memory layout, unified I/O abstraction, and caching, preserving existing formats.

**Overall Timeline**: ~22-28 months total project duration
**Current Status**: Phase 5.2.G (Physics Module Migration)

**⚠️ ARCHITECTURAL UPDATE (June 2025):**
- **Placeholder modules removed**: All placeholder modules deleted except `placeholder_empty_module.c/.h` (kept as template)
- **Manifest/discovery systems removed**: Simplified to self-registering modules only, removed ~1000+ lines of unused infrastructure
- **Explicit property build system implemented**: `make physics-free`, `make full-physics`, `make custom-physics CONFIG=file.json` for build-time property control

## Current Architecture Assessment

### Strengths
- Modular organization with core functionality and physics modules
- Efficient depth-first traversal of merger trees
- Effective multi-file organization for large datasets
- Parallel processing support via MPI

### Challenges
- Static physics module implementation requiring recompilation for changes
- Tight coupling between physics modules
- Monolithic galaxy evolution process in `evolve_galaxies()`
- Global state and parameter passing
- Resource management issues in I/O operations, particularly HDF5 handles
- Fixed buffer sizes and inefficient memory allocation strategies

## Detailed Refactoring Strategy

### Phase 1: Preparatory Refactoring (Foundation) [COMPLETED]
**Estimated Timeline**: 3 months

#### Components
*   **1.1 Code Organization**: Ensure separation of core, physics, I/O. Extract physics functions. Consistent naming.
*   **1.2 Global State Reduction**: Encapsulate parameters. Refactor signatures for explicit parameter passing. Implement init/cleanup routines.
*   **1.3 Comprehensive Testing Framework**: Establish validation tests (scientific output), unit tests (core), and performance benchmarks (deferred to Phase 6).
*   **1.4 Documentation Enhancement**: Update inline docs, create architectural docs, document data flow.
*   **1.5 Centralize Utilities**: Identify and consolidate common physics calculations (timescales, radii, densities) into utility functions.

#### Example Implementation
```c
// Before: Global state and implicit parameter passing
double cooling_recipe(int p, double dt) {
    // Access global galaxy array and parameter structures
    cooling_rate = calculate_cooling_rate(ColdGas[p], Metallicity[p]);
    return cooling_rate;
}

// After: Explicit parameter passing, no global state
double cooling_recipe(int p, double dt, struct GALAXY *galaxies, struct params *parameters) {
    cooling_rate = calculate_cooling_rate(galaxies[p].ColdGas, galaxies[p].Metallicity,
                                         parameters->cooling_table);
    return cooling_rate;
}

// Centralized utility function
double calculate_cooling_timescale(double temp, double metallicity, double density) {
    // Standardized calculation available to all modules
    return cooling_function(temp, metallicity) / (density * specific_heat);
}
```

#### Design Rationale
Reducing global state and making parameter passing explicit creates cleaner interfaces for modularization. The separation of physics calculations from their specific use cases facilitates code reuse and makes the system easier to maintain. Utility functions provide a single source of truth for common calculations.

#### Key Risks and Mitigation
- **Risk**: Scientific inaccuracy during refactoring
  - **Mitigation**: Implement comprehensive validation tests comparing results with the original codebase
- **Risk**: Breaking existing functionality during reorganization
  - **Mitigation**: Apply incremental changes with testing at each step

### Phase 2: Enhanced Module Interface Definition [COMPLETED]
**Estimated Timeline**: 5-6 months

#### Components
*   **2.1 Base Module Interfaces**: Define standard interfaces for module types (e.g., `base_module`, `cooling_module`).
    ```c
    struct base_module {
        char name[MAX_MODULE_NAME]; char version[MAX_MODULE_VERSION]; char author[MAX_MODULE_AUTHOR];
        int module_id; enum module_type type;
        int (*initialize)(struct params *params, void **module_data);
        int (*cleanup)(void *module_data);
        int (*configure)(void *module_data, const char *cfg_name, const char *cfg_value); // Optional
        int last_error; char error_message[MAX_ERROR_MESSAGE];
        struct module_manifest *manifest;
        module_function_registry_t *function_registry;
        module_dependency_t *dependencies; int num_dependencies;
    };
    ```
*   **2.2 Galaxy Property Extension Mechanism**: Add to `GALAXY` struct: `void **extension_data; int num_extensions; uint64_t extension_flags;`. Implement property registration, memory management, access APIs.
*   **2.3 Event-Based Communication System**: Define event types and structure. Implement register/dispatch mechanisms and handlers.
*   **2.4 Module Registry System**: Implement central registry tracking loaded modules, handles, interfaces, active status.
*   **2.5 Module Pipeline System**: Implement configurable pipeline for execution order.
*   **2.6 Hybrid Configuration System**: Retain `core_read_parameter_file.c` for core parameters. Utilize JSON for structural configuration.
*   **2.7 Module Callback System**: Implement inter-module calls, dependency declaration, call stack tracking, context preservation, and circular dependency checks.

#### Example Implementation
```c
// Galaxy Property Extension System
typedef struct {
    char name[32];           // Property name
    size_t size;             // Size in bytes
    int module_id;           // Which module owns this
    // Serialization functions
    void (*serialize)(void *src, void *dest, int count);
    void (*deserialize)(void *src, void *dest, int count);
    // Metadata
    char description[128];
    char units[32];
} galaxy_property_t;

// Register a new property extension
int register_galaxy_property(const galaxy_property_t *property) {
    // Add to property registry
    int prop_id = property_registry_add(property);
    // Return property ID for later access
    return prop_id;
}

// Access extension data for a galaxy
void *galaxy_extension_get_data(struct GALAXY *galaxy, int prop_id) {
    if (prop_id >= galaxy->num_extensions || !(galaxy->extension_flags & (1ULL << prop_id))) {
        return NULL;  // Extension not present
    }
    return galaxy->extension_data[prop_id];
}

// Event System
enum galaxy_event_type_t {
    EVENT_COLD_GAS_UPDATED,
    EVENT_STAR_FORMATION_OCCURRED,
    EVENT_FEEDBACK_APPLIED,
    // Other events...
};

typedef struct {
    galaxy_event_type_t type;
    int galaxy_index;
    int step;
    void *data;
    size_t data_size;
} galaxy_event_t;

// Register event handler
void event_register_handler(galaxy_event_type_t event_type,
                          int (*handler)(galaxy_event_t*, void*),
                          void *user_data) {
    // Add handler to registry
}

// Dispatch event
void event_dispatch(galaxy_event_t *event) {
    // Call all registered handlers
}

// Module Callback System
int module_invoke(enum module_type type, const char* module_name,
                const char* function_name, void* context,
                void* args, void* result) {
    // Find module
    // Lookup function
    // Push to call stack
    // Call function and capture result
    // Pop from call stack
    return status;
}
```

#### Design Rationale
The module interface structure establishes a consistent contract for all physics components while allowing specialized behavior per physics domain. The extension mechanism enables adding properties without modifying the core GALAXY structure, which is essential for plugin development without recompilation. The event system facilitates loose coupling between modules, letting them react to changes without direct dependencies. The callback system provides a controlled mechanism for necessary direct inter-module communication.

#### Key Risks and Mitigation
- **Risk**: Complex module interactions leading to bugs or circular dependencies
  - **Mitigation**: Implement call stack tracking and circular dependency detection
- **Risk**: Performance overhead from extension mechanism
  - **Mitigation**: Optimize property access patterns, consider caching for frequently accessed properties
- **Risk**: Event handling complexity
  - **Mitigation**: Provide clear documentation on event usage patterns, implement event debugging tools

### Phase 3: I/O Abstraction and Optimization [COMPLETED]
**Estimated Timeline**: 2-3 months

#### Components
*   **3.1 I/O Interface Abstraction**: Define unified `io_interface` (init, read_forest, write_galaxies, cleanup, resource mgmt). Implement handler registry and format detection. Standardize I/O error reporting. Include unification of galaxy output preparation logic.
*   **3.2 Format-Specific Implementations**: Refactor existing I/O into handlers implementing the `io_interface`. Add serialization for extended properties. Implement cross-platform binary endianness handling and robust HDF5 resource tracking/cleanup.
*   **3.3 Memory Optimization**: Implement configurable buffered I/O. Add memory mapping options. Use geometric growth for dynamic array reallocation. Implement memory pooling for galaxy structs.


#### Example Implementation
```c
// I/O Interface definition
struct io_interface {
    // Metadata
    const char* name;
    const char* version;
    int format_id;
    uint32_t capabilities;

    // Core operations
    int (*initialize)(const char* filename, struct params* params);
    int (*read_forest)(int64_t forestnr, struct halo_data** halos, struct forest_info* forest_info);
    int (*write_galaxies)(struct galaxy_output* galaxies, int ngals, struct save_info* save_info);
    int (*cleanup)();

    // Resource management
    int (*close_open_handles)();
    int (*get_open_handle_count)();
};

// HDF5 handle tracking for preventing resource leaks
struct hdf5_handle_tracker {
    hid_t handles[MAX_HDF5_HANDLES];
    int handle_types[MAX_HDF5_HANDLES];  // File, group, dataset, etc.
    int num_handles;
};

int hdf5_track_handle(hid_t handle, int handle_type) {
    if (g_hdf5_tracker.num_handles >= MAX_HDF5_HANDLES) {
        return -1; // Error: too many handles
    }
    g_hdf5_tracker.handles[g_hdf5_tracker.num_handles] = handle;
    g_hdf5_tracker.handle_types[g_hdf5_tracker.num_handles] = handle_type;
    g_hdf5_tracker.num_handles++;
    return 0;
}

int hdf5_close_all_handles() {
    // Close in reverse order of creation (children before parents)
    for (int i = g_hdf5_tracker.num_handles - 1; i >= 0; i--) {
        switch (g_hdf5_tracker.handle_types[i]) {
            case HDF5_HANDLE_FILE:
                H5Fclose(g_hdf5_tracker.handles[i]);
                break;
            case HDF5_HANDLE_GROUP:
                H5Gclose(g_hdf5_tracker.handles[i]);
                break;
            // Other types...
        }
    }
    g_hdf5_tracker.num_handles = 0;
    return 0;
}

// Memory-efficient array expansion with geometric growth
int expand_galaxy_array(struct GALAXY** galaxy_array, int* current_size, int min_new_size) {
    int new_size = *current_size;

    // Calculate new size with geometric growth (multiply by 1.5)
    while (new_size < min_new_size) {
        new_size = (int)(new_size * 1.5) + 1;
    }

    // Reallocate the array
    struct GALAXY* new_array = myrealloc(*galaxy_array, new_size * sizeof(struct GALAXY));
    if (new_array == NULL) {
        return -1;  // Error
    }

    *galaxy_array = new_array;
    *current_size = new_size;
    return 0;  // Success
}
```

#### Design Rationale
A unified I/O interface allows support for multiple formats without duplicating code. Robust resource tracking, particularly for HDF5, prevents memory leaks that have been problematic in the past. The geometric growth strategy for arrays significantly improves performance by reducing reallocation frequency. Memory pooling reduces fragmentation and allocation overhead for frequently created/destroyed galaxy objects.

#### Key Risks and Mitigation
- **Risk**: Resource leaks in HDF5 handling
  - **Mitigation**: Implement comprehensive tracking system for HDF5 handles
- **Risk**: Cross-platform compatibility issues
  - **Mitigation**: Add proper endianness detection and conversion for binary formats
- **Risk**: Performance regression from abstraction
  - **Mitigation**: Carefully optimize critical I/O paths, implement buffering strategies

### Phase 4: Advanced Plugin Infrastructure [COMPLETED]
**Estimated Timeline**: 3-4 months

#### Components
*   **4.1 Dynamic Library Loading**: Implement cross-platform loading (`dlopen`/`LoadLibrary`), symbol lookup, and unloading with specific error handling.
*   **4.2 Module Development Framework**: Provide module template generation, a validation framework, and debugging utilities.
*   **4.3 Parameter Tuning System**: Implement parameter registration/validation within modules, runtime modification capabilities, and parameter file import/export. Add bounds checking.
*   **4.4 Module Discovery and Loading**: Implement directory scanning, dependency resolution, and validation. Define API versioning strategy. **NOTE**: Manifest parsing removed (June 2025) - simplified to self-registering modules only.
*   **4.5 Error Handling**: Implement comprehensive module error reporting, call stack tracing, and standardized logging per module.

#### Example Implementation
```c
// Cross-platform dynamic library loading
#ifdef _WIN32
#include <windows.h>
typedef HMODULE module_handle_t;
#else
#include <dlfcn.h>
typedef void* module_handle_t;
#endif

module_handle_t load_module(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_LAZY);
#endif
}

void* get_symbol(module_handle_t handle, const char* symbol_name) {
#ifdef _WIN32
    return GetProcAddress(handle, symbol_name);
#else
    return dlsym(handle, symbol_name);
#endif
}

void unload_module(module_handle_t handle) {
#ifdef _WIN32
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif
}

// **NOTE**: Module manifest system removed (June 2025) - simplified to self-registering modules only
// Future dynamic loading would use simplified module structures without manifest parsing
}

// Parameter tuning system
typedef struct {
    char name[64];
    enum {
        PARAM_TYPE_INT,
        PARAM_TYPE_FLOAT,
        PARAM_TYPE_DOUBLE,
        PARAM_TYPE_BOOL,
        PARAM_TYPE_STRING
    } type;
    union {
        struct { int min; int max; } int_range;
        struct { float min; float max; } float_range;
        struct { double min; double max; } double_range;
    } limits;
    union {
        int int_val;
        float float_val;
        double double_val;
        bool bool_val;
        char string_val[128];
    } value;
    bool has_limits;
    char description[256];
    char units[32];
} module_parameter_t;

int module_register_parameter(int module_id, const module_parameter_t* param) {
    // Add parameter to registry for this module
    return 0;
}

int module_set_parameter_double(int module_id, const char* name, double value) {
    // Locate parameter in registry
    module_parameter_t* param = find_parameter(module_id, name);
    if (!param || param->type != PARAM_TYPE_DOUBLE) return -1;

    // Check bounds if applicable
    if (param->has_limits) {
        if (value < param->limits.double_range.min ||
            value > param->limits.double_range.max) {
            return -2; // Out of bounds
        }
    }

    // Set value
    param->value.double_val = value;
    return 0;
}
```

#### Design Rationale
Cross-platform library loading abstracts OS-specific details, enabling consistent module handling across systems. The manifest system provides clear metadata and dependency information for each module. Parameter tuning with bounds checking prevents invalid configurations. Standardized error handling improves debuggability when modules interact in complex ways.

#### Key Risks and Mitigation
- **Risk**: Platform-specific loading issues
  - **Mitigation**: Thorough testing on all target platforms, robust error handling
- **Risk**: Security concerns with dynamic loading
  - **Mitigation**: Implement signature verification for modules, load from trusted locations only
- **Risk**: Complex dependency resolution
  - **Mitigation**: Build visualization tools, implement robust cycle detection

### Phase 5: Core Module Migration
**Estimated Timeline**: 6-7 months

#### Subphase 5.1: Refactoring Main Evolution Loop [COMPLETED]
*   **5.1.1 Merger Event Queue Implementation**: Implement event queue approach for mergers in traditional architecture
*   **5.1.2 Pipeline Phase System**: Implement execution phases (HALO, GALAXY, POST, FINAL) in pipeline context
*   **5.1.3 Pipeline Integration**: Transform `evolve_galaxies()` to use the pipeline system with proper phase handling
*   **5.1.4 Event System Integration**: Add event dispatch/handling points
*   **5.1.5 Extension Properties Support**: Add support for reading/writing extension properties
*   **5.1.6 Module Callback Integration**: Integrate module callbacks
*   **5.1.7 Diagnostics**: Add evolution diagnostics

#### Subphase 5.2: Properties Module Architecture Implementation [ARCHITECTURAL SHIFT]
**Description**: This subphase implements the "Properties Module" architecture. It centralizes the definition of all persistent physical galaxy properties into `properties.yaml`, generates type-safe accessor macros (`GALAXY_PROP_*`), and establishes the infrastructure for modules to interact with galaxy state via these properties. The core SAGE infrastructure becomes completely physics-agnostic, able to run with no physics modules at all.

##### 5.2.B: Central Property Definition & Infrastructure ✅ COMPLETED
*   **5.2.B.0: Establish Performance Baseline**: Run and record performance benchmarks using the codebase *before* these changes.
*   **5.2.B.1 Define Properties Format**: Establish structure and syntax for `properties.yaml`, including dynamic array support.
*   **5.2.B.2 Create Header Generation Script**: Implement script (`generate_property_headers.py`) to generate `core_properties.h` (with `galaxy_properties_t` struct and `GALAXY_PROP_*` macros) and `core_properties.c` (with metadata and lifecycle functions).
*   **5.2.B.3 Integrate Header Generation into Build System**: Add Makefile rules to run the generation script.
*   **5.2.B.4 Implement Core Registration**: Create system (`standard_properties.c/h`) to register standard properties with the Galaxy Extension system.
*   **5.2.B.5 Implement Memory Management**: Ensure `allocate/free/copy/reset_galaxy_properties` correctly handle dynamic arrays based on runtime parameters.
*   **5.2.B.6 Implement Synchronization Functions**: ~~Create `sync_direct_to_properties` and `sync_properties_to_direct` in `core_properties_sync.c/h` to copy data between direct fields and the `properties` struct.~~ **SUPERSEDED**: Single source of truth architecture implemented - no synchronization needed.

##### 5.2.C: Core Integration & Synchronization ✅ COMPLETED
*   **5.2.C.1 Integrate Synchronization Calls**: Insert calls to synchronization functions around module execution points in the pipeline executor (`physics_pipeline_executor.c`).
*   **5.2.C.2 Ensure Core Galaxy Lifecycle Management**: Update `init_galaxy`, galaxy copying (mergers, progenitors), and destruction logic to correctly manage the `galaxy->properties` struct lifecycle (`allocate_`, `copy_`, `free_`, `reset_galaxy_properties`).
*   **5.2.C.3 Refine Core Initialization Logic**: Verify `init_galaxy` correctly initializes both direct fields (for legacy compatibility) and calls `reset_galaxy_properties`.

##### 5.2.D: Module Adaptation ✅ COMPLETED
*   **5.2.D.1 Update Migrated Modules**: Adapt existing migrated modules (e.g., cooling, infall) to use `GALAXY_PROP_*` macros exclusively.
*   **5.2.D.2 Update Module Template Generator**: Ensure `core_module_template.c/h` generates code compatible with the property system.
*   **5.2.D.3 Revise Module Dependency Management**: Update dependency system if property-based interactions require changes (likely minimal).
*   **5.2.D.4 Update Physics Module Interface**: Add hooks for module-specific serialization/deserialization of *private* properties (if any module needs them).

##### 5.2.E: I/O System Update ✅ COMPLETED
*   **5.2.E.1 Remove GALAXY_OUTPUT Struct**: Delete the static `GALAXY_OUTPUT` struct definition.
*   **5.2.E.2 Remove prepare_galaxy_for_output Logic**: Eliminate the static mapping code.
*   **5.2.E.3 Implement Output Preparation Module**: Create a module running in `PIPELINE_PHASE_FINAL` to handle unit conversions/derived values using property-based transformers.
*   **5.2.E.4 Remove Binary Output Format Support**: Standardize on HDF5 output. Update relevant code and documentation.
*   **5.2.E.5 Update HDF5 I/O Handler**: Modify HDF5 handler to read property metadata and use property-based serialization for writing galaxy data.
*   **5.2.E.6 Enhance HDF5 Serialization**: Adapt HDF5 serialization to support dynamic arrays and module-specific properties through the property system.

##### 5.2.F: Core-Physics Separation ✅ LARGELY COMPLETED
**Goal:** Achieve complete independence between core infrastructure and physics to create a truly physics-agnostic system.
*   **5.2.F.1 Core Isolation** ✅ COMPLETED: Removed ALL physics dependencies from core infrastructure. Removed all direct physics calls in `evolve_galaxies.c`. Removed physics-related include statements from core files. Created minimal properties definition with only core infrastructure properties. **UPDATED**: Eliminated dual property system - all direct fields removed from GALAXY struct, achieving single source of truth architecture with property system only. Updated all core code to be physics-property-agnostic.
*   **5.2.F.2 Empty Pipeline Validation** ✅ COMPLETED: Created empty placeholder modules for essential pipeline points. Registered empty modules in the pipeline. Configured pipeline to execute all phases with no physics operations. Tested that the system can run end-to-end with empty properties.yaml. Implemented tests verifying core independence from physics. Documented the physics-free model baseline. Optimized memory management.
*   **5.2.F.3 Legacy Code Removal** ✅ COMPLETED: Removed all legacy physics implementation files. Updated build system to remove legacy components. Cleaned up remaining legacy references. Removed synchronization infrastructure after verification. Completed configuration-driven pipeline creation for full core-physics separation.

**Note**: Some architectural issues remain in systems like evolution diagnostics that contain hardcoded physics property knowledge and violate core-physics separation principles. These should be addressed before proceeding to Phase 5.2.G.

##### 5.2.G: Physics Module Migration ⏳ PENDING
**Goal:** With the core now completely physics-agnostic, implement physics modules as pure add-ons.
*   **5.2.G.1 Physics Foundation**: Develop standard physics utility functions independent of core code. Create common physics constants and conversion factors. Implement shared calculation libraries for physics modules. Establish module-to-module communication protocols. Create physics property definitions separate from core properties.
*   **5.2.G.2 Module Implementation**: Determine optimal module sequence based on dependencies. Implement and validate each physics module (Star Formation & Feedback, Disk Instability, Reincorporation, AGN Feedback & Black Holes, Metals & Chemical Evolution, Mergers). Optimize property access patterns if performance analysis indicates benefit.

#### Subphase 5.3: Comprehensive Validation ⏳ PENDING
After completely separating the core from physics and implementing all physics modules:
*   **5.3.1 Integration Testing**: Develop complete end-to-end integration tests for all module combinations.
*   **5.3.2 Scientific Validation**: Implement scientific validation comparing new modules to baseline SAGE.
*   **5.3.3 Compatibility Testing**: Create module compatibility test matrix.
*   **5.3.4 Performance Analysis**: Develop performance benchmarks comparing to original implementation.
*   **5.3.5 I/O Validation**: Test file format compatibility and I/O across module configurations.
*   **5.3.6 Dependency Analysis**: Validate module dependency chains using call graph analysis.
*   **5.3.7 Error Handling**: Test error handling and recovery mechanisms.
*   **5.3.8 Documentation**: Create comprehensive documentation of the validation approach.

### Phase 6: Advanced Performance Optimization
**Estimated Timeline**: 3-4 months

#### Components
*   **6.1 Memory Layout Enhancement**: Implement Structure-of-Arrays (SoA) for hot-path galaxy properties. Optimize memory pooling. Review and optimize dynamic limits (e.g., `ABSOLUTEMAXSNAPS`).
*   **6.2 Tree Traversal Optimization**: Optimize pointer-chasing in merger tree traversal. Implement non-recursive traversal options. Develop memory-efficient tree node representations.
*   **6.3 Physics Calculation Optimization**: Implement vectorized calculations (SIMD) for physics modules. Create batch processing for similar galaxies. Develop architecture-specific dispatch.
*   **6.4 Parallelization Enhancements**: Refine load balancing for distributed processing. Implement finer-grained parallelism. Optimize MPI communication. Develop hybrid MPI+OpenMP approach.
*   **6.5 Module System Optimization**: Optimize callback mechanisms. Cache frequent module interactions. Implement profiling for module operations. Consider strategic function inlining.
*   **6.6 Property Access Optimization**: Cache property IDs for frequent access. Implement batch property access for group operations on multiple galaxies.

#### Example Implementation
```c
// Structure of Arrays for hot-path galaxy properties
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

// Conversion functions
void convert_AoS_to_SoA(struct GALAXY *galaxies, int ngal, galaxy_hotpath_data_t *hotpath) {
    // Ensure capacity is sufficient
    if (hotpath->capacity < ngal) {
        // Reallocate arrays
        hotpath->ColdGas = realloc(hotpath->ColdGas, ngal * sizeof(float));
        hotpath->HotGas = realloc(hotpath->HotGas, ngal * sizeof(float));
        // ... other fields ...
        hotpath->capacity = ngal;
    }

    // Copy data to SoA layout
    for (int i = 0; i < ngal; i++) {
        // CORRECTED: Physics properties must use generic accessors
        hotpath->ColdGas[i] = get_float_property(&galaxies[i], PROP_ColdGas, 0.0);
        hotpath->HotGas[i] = get_float_property(&galaxies[i], PROP_HotGas, 0.0);
        hotpath->StellarMass[i] = get_float_property(&galaxies[i], PROP_StellarMass, 0.0);
        hotpath->BulgeMass[i] = get_float_property(&galaxies[i], PROP_BulgeMass, 0.0);

        // ... other properties ...
    }

    hotpath->count = ngal;
}

// Optimized property access using cached IDs
static int hotgas_prop_id = -1;
static int coldgas_prop_id = -1;

void init_property_id_cache() {
    hotgas_prop_id = get_standard_property_id_by_name("HotGas");
    coldgas_prop_id = get_standard_property_id_by_name("ColdGas");
    // ... other properties ...
}

double get_hot_gas_optimized(struct GALAXY *galaxy) {
    if (hotgas_prop_id == -1) {
        hotgas_prop_id = get_standard_property_id_by_name("HotGas");
    }
    // Direct property access using cached ID
    return GALAXY_PROP_BY_ID(galaxy, hotgas_prop_id, double);
}
```

#### Design Rationale
Performance optimization builds on the established modular architecture, focusing on reducing memory access patterns, improving cache utilization, and enabling parallel execution. The Structure-of-Arrays approach can significantly improve performance for hot-path physics calculations by enhancing data locality and enabling SIMD operations. Property access optimizations help minimize the overhead of the property system while maintaining its modularity benefits.

#### Key Risks and Mitigation
- **Risk**: Optimization complexity interfering with modularity
  - **Mitigation**: Maintain clear separation between infrastructure optimization and physics implementation
- **Risk**: Platform-specific optimizations limiting portability
  - **Mitigation**: Implement architecture detection and dispatch mechanisms
- **Risk**: Performance regression in less common usage patterns
  - **Mitigation**: Maintain comprehensive benchmarks covering diverse use cases

### Phase 7: Documentation and Tools
**Estimated Timeline**: 2-3 months

#### Components
*   **7.1 Comprehensive Architecture Guide**: Document core components, module system, pipeline phases, property system, and interfaces.
*   **7.2 Module Development Guide**: Create detailed guides for developing physics modules, galaxy property extensions, and integration with the pipeline.
*   **7.3 Property System Guide**: Document `properties.yaml`, generation script, access patterns, and best practices.
*   **7.4 Visualization and Debugging Tools**: Implement tools for visualizing module dependencies, pipeline configuration, and property usage across modules.
*   **7.5 Performance Profiling Tools**: Create specialized tools for analyzing module performance, pipeline bottlenecks, and property access patterns.
*   **7.6 Example Modules and Templates**: Provide comprehensive examples of each module type, including documentation, tests, and sample configurations.
*   **7.7 User Guide**: Document system configuration, module selection, parameter tuning, and runtime operation.
*   **7.8 API Documentation**: Generate comprehensive API documentation for both core infrastructure and module interfaces.

#### Example Implementation
```python
# Property usage analysis script
def analyze_property_usage():
    property_usage = {}
    modules = []

    # Read property definitions
    with open('properties.yaml') as f:
        properties = yaml.safe_load(f)['properties']
        for prop in properties:
            property_usage[prop['name']] = {
                'read_by': [],
                'written_by': [],
                'serialized': prop.get('output', False)
            }

    # Scan module source files
    for module_file in glob.glob('src/physics/*.c'):
        module_name = os.path.basename(module_file).split('.')[0]
        modules.append(module_name)

        with open(module_file) as f:
            content = f.read()

            for prop in property_usage:
                # Check read patterns (using GALAXY_PROP_*)
                if f'GALAXY_PROP_{prop["name"]}' in content:
                    # Simple check, might need refinement to distinguish read/write
                    property_usage[prop]['read_by'].append(module_name)
                    property_usage[prop]['written_by'].append(module_name)

    # Generate usage matrix visualization
    visualize_property_usage(properties, modules, property_usage)

    # Check for potential conflicts
    check_write_conflicts(property_usage)

    # Report properties with no readers or writers
    report_unused_properties(property_usage)
```

#### Design Rationale
Comprehensive documentation and tools ensure the modular architecture is understood and utilized effectively. Documentation is structured to cover both architectural understanding and practical usage. Tools for analyzing property usage and module interactions help identify potential issues early in development and support maintenance. The guides make the system accessible to new developers and users.

#### Key Risks and Mitigation
- **Risk**: Documentation becoming outdated as implementation evolves
  - **Mitigation**: Integrate documentation updates into development workflow, automate where possible
- **Risk**: Steep learning curve for new contributors
  - **Mitigation**: Provide detailed examples, templates, and step-by-step guides
- **Risk**: Insufficient adoption of analysis tools
  - **Mitigation**: Integrate tools into CI/CD pipeline, make them part of standard development process

## Deferred Work - General List

*   **Performance Benchmarking**: Moved from Phase 1 to Phase 5/6. Profiling on the refactored structure is more valuable than embedding benchmarks during heavy changes.
*   **Enhanced End-to-End Test Diagnostics**: Skipped. Current logging + `sagediff.py` deemed sufficient.
*   **Dynamic Memory Defragmentation**: Deferred (post-project). Focus on preventative measures (pooling, allocation strategy) first.
*   **Full SIMD Vectorization**: Deferred (post-project). Implement for critical hotspots in Phase 6.3, full vectorization later if needed.
*   **Testing Approach Documentation**: Document the rationale behind different testing approaches rather than forcing consistency across the entire test suite.
*   **Test Build Simplification**: Consider addressing during a dedicated testing infrastructure improvement phase to consolidate Makefiles and standardize build process.
*   **Dynamic Library Loading Limits**: Consider making `MAX_LOADED_LIBRARIES` (64) configurable or dynamically expandable. The current limit may be restrictive for complex systems with many modules.

## Deferred Work - From Phase 5

The following tasks have been deferred to later consideration after completing the main refactoring phases:

### Evolution Diagnostics Refactoring

**Description**: Refactor evolution diagnostics to be more flexible with pipeline phases.

**Rationale**: The current implementation is too rigid and doesn't gracefully handle phase values outside its expected range.

**Implementation Tasks**:
- Modify `evolution_diagnostics.h` to define a more flexible phase representation
- Update phase validation to use an enum range check or bitfield rather than a hard-coded range
- Add a runtime phase registration mechanism so modules can define their own phases

**Example**:
```c
// Example of a more flexible phase representation
typedef enum {
    DIAG_PHASE_UNKNOWN = 0,
    DIAG_PHASE_HALO = 1,
    DIAG_PHASE_GALAXY = 2,
    DIAG_PHASE_POST = 3,
    DIAG_PHASE_FINAL = 4,
    DIAG_PHASE_CUSTOM_BEGIN = 8,
    DIAG_PHASE_CUSTOM_END = 15,
    DIAG_PHASE_MAX = 16
} diagnostic_phase_t;
```

### Event Data Protocol Enhancement

**Description**: Define a clear protocol for what event data may contain, with explicit support for diagnostics.

**Rationale**: The current approach of trying to extract diagnostics from events without a clear protocol is error-prone.

**Implementation Tasks**:
- Define a standard event data header structure indicating whether events contain diagnostics information
- Update all event emission sites to include this header
- Use tagged unions or type codes to safely handle different event data formats

**Example**:
```c
// Example of a standard event header
typedef struct {
    uint32_t type_code;               // Code indicating event data type
    uint32_t flags;                   // Event-specific flags
    bool has_pipeline_context;        // Explicit flag for pipeline context
    bool has_diagnostics;             // Explicit flag for diagnostics
    void *pipeline_context;           // Optional pipeline context (NULL if not present)
    void *diagnostics;                // Optional diagnostics context (NULL if not present)
    size_t payload_size;              // Size of event-specific payload after header
    // Followed by event-specific payload data
} event_data_header_t;
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
| Circular dependencies | Medium | Medium | Static dependency analysis; runtime cycle detection; careful module design |
| Module callback overhead | Medium | Medium | Optimize callback paths; cache frequent calls; profile and tune callback system |
| Cross-platform compatibility issues | Medium | Medium | Implement proper endianness handling; abstract platform-specific code; standardized error handling |
| HDF5 resource leaks | Medium | High | Implement comprehensive resource tracking; validate handle management; add cleanup checks |
| Memory allocation inefficiency | Medium | Medium | Implement geometric growth strategies; optimize pooled allocations; reduce fragmentation |

## Conclusion

This enhanced refactoring plan provides a comprehensive roadmap for transforming the SAGE model into a truly modular, maintainable system with runtime functional modularity. The **Properties Module architecture** introduced in Phase 5.2 represents a significant architectural improvement that provides cleaner separation between core infrastructure and physics implementation, enabling independent development and runtime configuration of physics modules.

By centralizing the definition of all persistent per-galaxy physical state, eliminating core dependencies on specific physics knowledge, and providing standardized access patterns, the system achieves the core goal of runtime functional modularity while maintaining scientific accuracy and improving maintainability. The comprehensive testing and validation approach ensures that the refactored system maintains consistency with the original SAGE model while enabling new capabilities for physics exploration and extension.