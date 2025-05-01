# Enhanced SAGE Model Refactoring Plan

## Goals and Overview

1. **Runtime Functional Modularity**: Implement a dynamic plugin architecture allowing runtime insertion, replacement, reordering, or removal of physics modules without recompilation.
2. **Optimized Merger Tree Processing**: Enhance merger tree handling efficiency via improved memory layout, unified I/O abstraction, and caching, preserving existing formats.

**Overall Timeline**: ~22-28 months total project duration
**Current Status**: Phase 5 (Core Module Migration)

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
*   **4.4 Module Discovery and Loading**: Implement directory scanning, manifest parsing, dependency resolution, and validation. Define API versioning strategy.
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

// Module manifest parsing
struct module_manifest {
    char name[64];
    char version[32];
    enum module_type type;
    char author[64];
    char description[256];
    char library_path[PATH_MAX];
    uint32_t capabilities;
    bool auto_initialize;
    bool auto_activate;
    module_dependency_t dependencies[MAX_DEPENDENCIES];
    int num_dependencies;
};

// Parse module manifest from JSON
int module_parse_manifest(const char* filename, struct module_manifest* manifest) {
    // Open and parse JSON file
    FILE* file = fopen(filename, "r");
    if (!file) return -1;
    
    // Read file content
    char* buffer = read_file_to_string(file);
    fclose(file);
    
    // Parse JSON
    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!root) return -2;
    
    // Extract manifest data
    cJSON* name = cJSON_GetObjectItem(root, "name");
    if (cJSON_IsString(name) && name->valuestring) {
        strncpy(manifest->name, name->valuestring, sizeof(manifest->name) - 1);
    }
    
    // Extract other fields...
    
    cJSON_Delete(root);
    return 0;
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
*   **5.1.2 Pipeline Phase System**: Implement execution phases (HALO, GALAXY, POST, FINAL) in pipeline context to distinguish between calculations that happen at the halo level versus galaxy level
*   **5.1.3 Pipeline Integration**: Transform `evolve_galaxies()` to use the pipeline system with proper phase handling
*   **5.1.4 Event System Integration**: Add event dispatch/handling points
*   **5.1.5 Extension Properties Support**: Add support for reading/writing extension properties
*   **5.1.6 Module Callback Integration**: Integrate module callbacks
*   **5.1.7 Diagnostics**: Add evolution diagnostics

#### Subphase 5.2: Properties Module Architecture Implementation [ARCHITECTURAL SHIFT]
**IMPORTANT NOTE:** After careful evaluation, the implementation strategy for Phase 5.2 has been significantly revised to adopt a "Properties Module" architecture. This approach provides cleaner separation between core infrastructure and physics, treating all physics modules as equal consumers/modifiers of galaxy properties through a centrally-defined mechanism. This architectural shift supersedes the previous piecemeal approach to physics module migration.

##### 5.2.A: Initial Proof-of-Concept [NEW]
*   **5.2.A.1 Performance Baseline**: Establish performance benchmarks for the current implementation
*   **5.2.A.2 Minimal Property Definition**: Create a small subset of essential properties in YAML format
*   **5.2.A.3 Basic Header Generator**: Implement a minimal script to generate accessor macros
*   **5.2.A.4 Core Integration Test**: Adapt a small portion of the core code to use the generated macros
*   **5.2.A.5 Test Module Migration**: Convert one simple module (e.g., cooling) to use the new access pattern
*   **5.2.A.6 Validation**: Verify scientific equivalence with previous implementation
*   **5.2.A.7 Performance Assessment**: Measure performance impact of the new architecture
*   **5.2.A.8 Go/No-Go Decision**: Evaluate results and confirm continuation with full implementation

##### 5.2.B: Define and Integrate Central Property Definition
*   **5.2.B.1 Define Properties Format**: Establish structure and syntax for `properties.yaml`, specifying required fields (`name`, `type`, `initial_value`) and optional fields (`units`, `description`, `output`)
*   **5.2.B.2 Create Header Generation Script**: Implement script to validate definitions and generate `standard_properties.h` with type-safe accessor macros
*   **5.2.B.3 Integrate Header Generation into Build System**: Add Makefile rules to run the generation script during the build process
*   **5.2.B.4 Implement Core Registration**: Create system to read property definitions and register them with the Galaxy Extension system at startup
*   **5.2.B.5 Minimize Core GALAXY Struct**: Remove physical properties from the core definition, keeping only essential identifiers, type/merger info, and extension fields

##### 5.2.C: Integrate New Property System into Core
*   **5.2.C.1 Implement and Use Accessor Macros**: Refactor core code to use the generated `GALAXY_GET_*` and `GALAXY_SET_*` macros
*   **5.2.C.2 Remove Obsolete Core Accessors**: Remove `core_galaxy_accessors.c/h` and `core_parameter_views.c/h` and their usage
*   **5.2.C.3 Refine Core Initialization Logic**: Update galaxy initialization to use the new property system
*   **5.2.C.4 Galaxy Creation and Management**: Ensure all code that creates or manipulates galaxies uses the new property system

##### 5.2.D: Adapt Modules and Interfaces
*   **5.2.D.1 Update Physics Module Interface**: Add hooks for module-specific serialization of private properties
*   **5.2.D.2 Update Migrated Modules**: Adapt existing modules (cooling, infall) to use the new property access system
*   **5.2.D.3 Update Module Template Generator**: Ensure new modules follow the latest patterns
*   **5.2.D.4 Module Dependency Management**: Update the dependency declaration system to work with the new property access pattern

##### 5.2.E: I/O and Output Refactoring
*   **5.2.E.1 Remove GALAXY_OUTPUT Struct**: Delete the static output structure definition
*   **5.2.E.2 Remove prepare_galaxy_for_output Logic**: Eliminate the static mapping code
*   **5.2.E.3 Implement Output Preparation Logic**: Create a new `OutputPreparationModule` (or similar name) running in the `PIPELINE_PHASE_FINAL` to handle unit conversions and derived value calculations previously done in `prepare_galaxy_for_output`, using the standard property accessor macros
*   **5.2.E.4 Update I/O Handlers**: Modify handlers to use the property definition and accessor macros
*   **5.2.E.5 Update Property Serialization**: Adapt serialization for the module-driven approach

#### Example Implementation for Properties Module
```yaml
# properties.yaml - Central property definition
properties:
  - name: HotGas
    type: double
    initial_value: 0.0
    units: "1e10 Msun/h"
    description: "Mass of gas in the hot halo reservoir."
    output: true

  - name: StellarMass
    type: double
    initial_value: 0.0
    units: "1e10 Msun/h"
    description: "Total stellar mass in the galaxy."
    output: true
```

```c
// Generated standard_properties.h
#ifndef STANDARD_PROPERTIES_H
#define STANDARD_PROPERTIES_H

#include "core_galaxy_extensions.h"
#include "core_types.h"

// Hot Gas
#define GALAXY_GET_HOTGAS(galaxy) \
    (*(double*)galaxy_extension_get_data(galaxy, get_standard_property_id_by_name("HotGas")))

#define GALAXY_SET_HOTGAS(galaxy, value) do { \
    double* prop = (double*)galaxy_extension_get_data(galaxy, get_standard_property_id_by_name("HotGas")); \
    *prop = (double)(value); \
} while(0)

// Stellar Mass
#define GALAXY_GET_STELLARMASS(galaxy) \
    (*(double*)galaxy_extension_get_data(galaxy, get_standard_property_id_by_name("StellarMass")))

#define GALAXY_SET_STELLARMASS(galaxy, value) do { \
    double* prop = (double*)galaxy_extension_get_data(galaxy, get_standard_property_id_by_name("StellarMass")); \
    *prop = (double)(value); \
} while(0)

// Other property macros...

#endif // STANDARD_PROPERTIES_H
```

```c
// In a module (e.g., cooling_module.c)
#include "standard_properties.h"

static double calculate_cooling(struct GALAXY *galaxy, double dt, void *module_data) {
    // Get current hot gas and metals
    double hot_gas = GALAXY_GET_HOTGAS(galaxy);
    double metals_hot = GALAXY_GET_METALSHOT(galaxy);
    
    // Calculate cooling rate
    double cooling_rate = compute_rate(hot_gas, metals_hot, dt);
    
    // Apply cooling
    double cooled_gas = cooling_rate * dt;
    GALAXY_SET_HOTGAS(galaxy, hot_gas - cooled_gas);
    GALAXY_SET_COLDGAS(galaxy, GALAXY_GET_COLDGAS(galaxy) + cooled_gas);
    
    return cooling_rate;
}
```

##### 5.2.F: Systematic Physics Module Migration
*   **Goal:** Convert all remaining legacy physics implementations into standalone modules conforming to the new architecture.
*   **Process for each legacy physics area** (Star Formation, Feedback, Mergers, Disk Instability, Reincorporation, AGN, Metals, Misc):
    *   Create new module files (e.g., `src/physics/star_formation_module.c/h`) implementing the `physics_module_interface`.
    *   Transfer the core physics logic from the corresponding `src/physics/legacy/model_*.c` file into the new module's functions (likely mapping to `execute_galaxy_phase` or other appropriate phases).
    *   Include `standard_properties.h` in the new module's C file.
    *   Replace *all* direct `struct GALAXY` field access for physical properties (e.g., `galaxy->StellarMass`) with the generated `GALAXY_GET_*` / `GALAXY_SET_*` macros.
    *   Manage any internal module parameters within the module's `_data` struct (allocated in `initialize`, freed in `cleanup`). Read initial parameter values from `run_params` for now (configuration system integration can refine this later).
    *   Implement the `serialize_properties` / `deserialize_properties` functions *only if* the module defines and uses private, persistent, per-galaxy extension properties that need saving (most physics modules likely won't need this, relying on standard properties). Implement empty stubs returning 0 otherwise.
    *   Implement the `declare_dependencies` function if the module needs to call functions in other modules via `module_invoke`.
    *   Create a module factory function (e.g., `star_formation_module_create()`) that returns the module's interface struct.
    *   Register the new module by calling its factory function during SAGE initialization (e.g., in `core_init.c` or a dedicated physics registration function).
    *   Remove the corresponding legacy file(s) from `src/physics/legacy/` and update the Makefile (`LEGACY_PHYSICS_SRC`).
*   **Migration Sequence Planning**: Define clear order of module migration based on dependencies (e.g., migrate SF/Feedback before Mergers if Mergers calls them).
*   **Common Physics Utilities**: Extract and centralize shared physics calculations (e.g., timescale calculations, density profiles if not property-related) into utility functions or potentially a core utility module, if appropriate, to avoid duplication across the new physics modules.
*   **Module List (to be migrated):**
    *   Star Formation and Feedback
    *   Disk Instability
    *   Reincorporation
    *   AGN Feedback and Black Holes
    *   Metals and Chemical Evolution
    *   Mergers
    *   Miscellaneous physics (determine if needed or absorbed)

*   **5.2.F.1 Migration Sequence Planning**: Define clear order of module migration based on dependencies
*   **5.2.F.2 Common Physics Utilities**: Extract and centralize shared physics calculations into utility modules
*   **5.2.F.3 Star Formation and Feedback**: Migrate star formation and feedback modules
*   **5.2.F.4 Disk Instability**: Migrate disk instability module
*   **5.2.F.5 Reincorporation**: Migrate reincorporation module
*   **5.2.F.6 AGN Feedback and Black Holes**: Migrate AGN-related modules
*   **5.2.F.7 Metals and Chemical Evolution**: Migrate metals tracking and chemical evolution
*   **5.2.F.8 Mergers**: Migrate merger handling module

#### Design Rationale for Properties Module Approach
The Properties Module architecture achieves true physics-agnostic core infrastructure while providing a centralized, well-defined mechanism for modules to interact with shared galaxy state. This approach:

1. **Eliminates Core-Physics Coupling**: Core code works with abstract "extensions" without knowing their physics meaning
2. **Centralizes Property Definition**: All shared properties are defined once in a human-readable file
3. **Ensures Type Safety**: Generated macros provide compile-time type checking
4. **Simplifies Maintenance**: Adding or modifying properties requires changes to a single file
5. **Standardizes Access Patterns**: All modules use identical mechanisms to interact with properties
6. **Removes Ownership Hierarchy**: No module "owns" shared properties used by others
7. **Enables Core Independence**: Core remains unaware of physics implementations or property specifics
8. **Streamlines Output Management**: Property output decisions are declarative in the central definition

#### Key Risks and Mitigation for Properties Module Implementation
- **Risk**: Build-time dependency on the property generation script
  - **Mitigation**: Ensure script is robust, well-tested, and properly integrated with the build system
- **Risk**: Refactoring scope and complexity
  - **Mitigation**: Begin with the proof-of-concept phase to validate approach before full implementation
- **Risk**: Performance overhead from macros and indirect property access
  - **Mitigation**: Perform thorough performance testing, optimize macro expansion, consider caching property IDs
- **Risk**: Scientific inconsistency during transition
  - **Mitigation**: Implement comprehensive validation against reference outputs, migrate physics one module at a time
- **Risk**: File format compatibility issues
  - **Mitigation**: Implement backward compatibility in I/O handlers for reading old file formats

#### Subphase 5.3: Validation and Testing
*   **5.3.1 Property Definition Validation**: Develop tools to validate `properties.yaml` for syntax and consistency
*   **5.3.2 Scientific Output Validation**: Validate results of the fully migrated system against baseline SAGE using reference merger trees
*   **5.3.3 Performance Benchmarks**: Compare performance against baseline and previous implementation
*   **5.3.4 Module Compatibility Testing**: Verify combinations of modules work correctly
*   **5.3.5 Call Graph Validation**: Check for circular dependencies and proper module interactions
*   **5.3.6 I/O Format Validation**: Verify file compatibility and correct serialization/deserialization
*   **5.3.7 Error Handling Testing**: Validate error reporting and recovery mechanisms
*   **5.3.8 Comprehensive Integration Tests**: Test the entire system end-to-end with various configurations

### Phase 6: Advanced Performance Optimization
**Estimated Timeline**: 3-4 months

#### Components
*   **6.1 Memory Layout Enhancement**: Implement Structure-of-Arrays (SoA) for hot-path galaxy data where beneficial. Further optimize memory pooling. Introduce size-segregated pools if needed. Evaluate hardcoded limits like `ABSOLUTEMAXSNAPS` and implement runtime checks or dynamic resizing if necessary.
*   **6.2 Tree Traversal Optimization**: Optimize pointer-chasing patterns (e.g., reducing indirections). Consider non-recursive traversal algorithms. Implement memory-efficient data structures for tree nodes (*Note: focuses on traversal logic/structure, not caching/prefetching of halo data itself, which is unneeded for single-pass traversal*).
*   **6.3 Vectorized Physics Calculations**: Implement SIMD for suitable calculations. Explore batch processing of galaxies. Implement architecture-specific dispatch.
*   **6.4 Parallelization Enhancements**: Refine load balancing. Explore finer-grained parallelism. Optimize MPI communication. Consider hybrid MPI+OpenMP.
*   **6.5 Module Callback Optimization**: Implement callback caching. Optimize paths for frequent interactions. Add profiling for inter-module calls. Consider targeted function inlining.
*   **6.6 Property Access Optimization**: Implement caching for frequently accessed property IDs. Consider specialized batch access for hot-path calculations.

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
        hotpath->ColdGas[i] = GALAXY_GET_COLDGAS(&galaxies[i]);
        hotpath->HotGas[i] = GALAXY_GET_HOTGAS(&galaxies[i]);
        hotpath->StellarMass[i] = GALAXY_GET_STELLARMASS(&galaxies[i]);
        hotpath->BulgeMass[i] = GALAXY_GET_BULGEMASS(&galaxies[i]);
        
        // ... other properties ...
    }
    
    hotpath->count = ngal;
}

// Optimized property access using cached IDs (post implementation)
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
    return *(double*)galaxy_extension_get_data(galaxy, hotgas_prop_id);
}
```

#### Design Rationale
Performance optimization builds on the established modular architecture, focusing on reducing memory access patterns, improving cache utilization, and enabling parallel execution. The Structure-of-Arrays approach can significantly improve performance for hot-path physics calculations by enhancing data locality and enabling SIMD operations. Property access optimizations help minimize the overhead of the extension-based property system while maintaining its modularity benefits.

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
*   **7.3 Property System Guide**: Document the property definition format, header generation, and best practices for property access.
*   **7.4 Visualization and Debugging Tools**: Implement tools for visualizing module dependencies, pipeline configuration, and property access patterns.
*   **7.5 Performance Profiling Tools**: Create specialized tools for analyzing module performance, pipeline bottlenecks, and profiling galaxy evolution.
*   **7.6 Example Modules and Templates**: Provide comprehensive examples and templates for different types of physics modules.
*   **7.7 User Guide**: Document system configuration, module selection, and parameter tuning for end users.
*   **7.8 API Documentation**: Generate comprehensive API documentation for core systems and interfaces.

#### Example Implementation
```python
# Property usage analysis script (conceptual)
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
                # Check read patterns
                if f'GALAXY_GET_{prop.upper()}' in content:
                    property_usage[prop]['read_by'].append(module_name)
                
                # Check write patterns
                if f'GALAXY_SET_{prop.upper()}' in content:
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

## Conclusion

This enhanced refactoring plan provides a comprehensive roadmap for transforming the SAGE model into a truly modular, maintainable system with runtime functional modularity. The Properties Module architecture introduced in Phase 5.2 represents a significant architectural improvement that provides cleaner separation between core infrastructure and physics implementation, enabling independent development and runtime configuration of physics modules.

By centralizing the definition of all persistent per-galaxy physical state, eliminating core dependencies on specific physics knowledge, and providing standardized access patterns, the system achieves the core goal of runtime functional modularity while maintaining scientific accuracy and improving maintainability. The comprehensive testing and validation approach ensures that the refactored system maintains consistency with the original SAGE model while enabling new capabilities for physics exploration and extension.
