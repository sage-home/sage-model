# Enhanced SAGE Model Refactoring Plan

## Goals and Overview

1. **Runtime Functional Modularity**: Implement a dynamic plugin architecture allowing runtime insertion, replacement, reordering, or removal of physics modules without recompilation.
2. **Optimized Merger Tree Processing**: Enhance merger tree handling efficiency via improved memory layout, unified I/O abstraction, and caching, preserving existing formats.

**Overall Timeline**: ~22-28 months total project duration
**Current Status**: Phase 3 (I/O Abstraction)

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

### Phase 1: Preparatory Refactoring (Foundation) [Partially Complete]
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

### Phase 2: Enhanced Module Interface Definition [Partially Complete]
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

### Phase 3: I/O Abstraction and Optimization [Current Phase]
**Estimated Timeline**: 2-3 months

#### Components
*   **3.1 I/O Interface Abstraction**: Define unified `io_interface` (init, read_forest, write_galaxies, cleanup, resource mgmt). Implement handler registry and format detection. Standardize I/O error reporting. Include unification of galaxy output preparation logic.
*   **3.2 Format-Specific Implementations**: Refactor existing I/O into handlers implementing the `io_interface`. Add serialization for extended properties. Implement cross-platform binary endianness handling and robust HDF5 resource tracking/cleanup.
*   **3.3 Memory Optimization**: Implement configurable buffered I/O. Add memory mapping options. Develop halo caching strategy. Use geometric growth for dynamic array reallocation. Implement memory pooling for galaxy structs.
*   **3.4 Review Dynamic Limits**: Evaluate hardcoded limits like `ABSOLUTEMAXSNAPS` and implement runtime checks or dynamic resizing if necessary.

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

### Phase 4: Advanced Plugin Infrastructure
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
**Estimated Timeline**: 4-5 months

#### Components
*   **5.1 Refactoring Main Evolution Loop**: Transform `evolve_galaxies()` to use the pipeline system. Integrate event dispatch/handling. Add support for reading/writing extension properties. Integrate module callbacks. Add evolution diagnostics.
*   **5.2 Converting Physics Modules**: Extract components into standalone modules implementing the defined interfaces. Add property registration and event triggers. Manage module-specific data. Define/implement dependencies via callbacks.
*   **5.3 Validation and Testing**: Perform scientific validation against baseline SAGE. Implement performance benchmarks. Develop module compatibility tests. Add call graph validation.

#### Example Implementation
```c
// Transformed evolution loop using pipeline
int evolve_galaxies_pipeline(int p, int centralgal, double time, double dt, 
                          int halonr, int step, struct GALAXY *galaxies) {
    // Setup evolution context
    struct evolution_context context = {
        .galaxy_index = p,
        .central_galaxy_index = centralgal,
        .time = time,
        .timestep = dt,
        .halo_number = halonr,
        .step = step,
        .galaxies = galaxies
    };
    
    // Execute each pipeline step
    for (int i = 0; i < pipeline.num_steps; i++) {
        const char* module_type = pipeline.steps[i].type;
        const char* module_name = pipeline.steps[i].name;
        
        // Find active module of this type
        int module_id = registry_find_active_module(module_type, module_name);
        if (module_id < 0) continue;  // Skip if module not found
        
        // Create evolution diagnostic for this step
        struct evolution_diagnostics diag;
        evolution_diagnostics_init(&diag, p, step, time);
        evolution_diagnostics_capture_before(&diag, &galaxies[p]);
        
        // Execute module
        int64_t start_time = get_current_time_ns();
        int result = module_execute(module_id, &context);
        int64_t end_time = get_current_time_ns();
        
        // Capture diagnostics
        evolution_diagnostics_capture_after(&diag, &galaxies[p]);
        diag.module_stats[0].execution_time_ns = end_time - start_time;
        strcpy(diag.module_stats[0].module_name, registry.modules[module_id].name);
        
        // Log diagnostics
        evolution_log_diagnostics(&diag);
        
        // Validate galaxy changes
        evolution_validate_galaxy_changes(&diag);
        
        if (result != 0) {
            // Handle error
            return result;
        }
    }
    
    return 0;
}

// Example of converted physics module (cooling)
struct cooling_module cooling_module_impl = {
    .base = {
        .name = "StandardCooling",
        .version = "1.0.0",
        .author = "SAGE Team",
        .initialize = cooling_module_initialize,
        .cleanup = cooling_module_cleanup
    },
    .calculate_cooling = cooling_module_calculate,
    .get_cooling_radius = cooling_module_get_radius
};

static int cooling_module_initialize(struct params *params, void **module_data) {
    // Allocate module data
    cooling_data_t *data = malloc(sizeof(cooling_data_t));
    if (!data) return -1;
    
    // Initialize module data
    data->cooling_table = load_cooling_table(params->cooling_table_file);
    
    // Register galaxy extensions
    galaxy_property_t cooling_radius_prop = {
        .name = "cooling_radius",
        .size = sizeof(float),
        .module_id = getCurrentModuleId(),
        .serialize = serialize_float,
        .deserialize = deserialize_float,
        .description = "Current cooling radius",
        .units = "kpc"
    };
    data->cooling_radius_prop_id = register_galaxy_property(&cooling_radius_prop);
    
    // Register for events
    event_register_handler(EVENT_AGN_FEEDBACK_APPLIED, 
                         cooling_handle_agn_feedback, data);
    
    *module_data = data;
    return 0;
}

static double cooling_module_calculate(const int gal, const double dt, 
                                     struct GALAXY *galaxies, void *module_data) {
    cooling_data_t *data = (cooling_data_t*)module_data;
    
    // Calculate cooling
    double temp = calculate_temperature(galaxies[gal].HotGas);
    double metallicity = galaxies[gal].MetalsHotGas / galaxies[gal].HotGas;
    double cooling_rate = interpolate_cooling_rate(data->cooling_table, 
                                                 temp, metallicity);
    
    // Store cooling radius in extension
    float *cooling_radius = galaxy_extension_get_data(&galaxies[gal], 
                                                  data->cooling_radius_prop_id);
    if (cooling_radius) {
        *cooling_radius = calculate_cooling_radius(galaxies[gal].HotGas, 
                                                cooling_rate, dt);
    }
    
    return cooling_rate;
}
```

#### Design Rationale
The pipeline-based evolution replaces the monolithic approach, enabling runtime configuration of the galaxy evolution process. Individual physics modules (like cooling) now encapsulate their own data and register for specific events. This separation of concerns allows physics components to be implemented, tested, and reasoned about independently. Extension properties provide storage for module-specific data without modifying the core galaxy structure.

#### Key Risks and Mitigation
- **Risk**: Scientific accuracy during migration
  - **Mitigation**: Implement comprehensive validation against baseline SAGE results
- **Risk**: Performance regression from modularization
  - **Mitigation**: Profile critical paths, optimize inter-module communication
- **Risk**: Complex evolution state management
  - **Mitigation**: Ensure clear context passing, implement diagnostic tracking

### Phase 6: Advanced Performance Optimization
**Estimated Timeline**: 3-4 months

#### Components
*   **6.1 Memory Layout Enhancement**: Implement Structure-of-Arrays (SoA) for hot-path galaxy data where beneficial. Further optimize memory pooling. Introduce size-segregated pools if needed.
*   **6.2 Tree Traversal Optimization**: Implement prefetching for depth-first traversal. Optimize pointer-chasing patterns. Consider non-recursive traversal algorithms.
*   **6.3 Vectorized Physics Calculations**: Implement SIMD for suitable calculations. Explore batch processing of galaxies. Implement architecture-specific dispatch.
*   **6.4 Parallelization Enhancements**: Refine load balancing. Explore finer-grained parallelism. Optimize MPI communication. Consider hybrid MPI+OpenMP.
*   **6.5 Module Callback Optimization**: Implement callback caching. Optimize paths for frequent interactions. Add profiling for inter-module calls. Consider targeted function inlining.

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
        hotpath->ColdGas[i] = galaxies[i].ColdGas;
        hotpath->HotGas[i] = galaxies[i].HotGas;
        hotpath->StellarMass[i] = galaxies[i].StellarMass;
        hotpath->BulgeMass[i] = galaxies[i].BulgeMass;
        
        hotpath->MetalsColdGas[i] = galaxies[i].MetalsColdGas;
        hotpath->MetalsHotGas[i] = galaxies[i].MetalsHotGas;
        hotpath->MetalsStellarMass[i] = galaxies[i].MetalsStellarMass;
        hotpath->MetalsBulgeMass[i] = galaxies[i].MetalsBulgeMass;
    }
    
    hotpath->count = ngal;
}

// Tree traversal with prefetching
typedef struct {
    struct halo_data **prefetch_buffer;
    int *prefetch_indices;
    int buffer_size;
    int num_prefetched;
    int next_index;
} prefetch_system_t;

void prefetch_initialize(prefetch_system_t *system, int buffer_size) {
    system->prefetch_buffer = malloc(buffer_size * sizeof(struct halo_data*));
    system->prefetch_indices = malloc(buffer_size * sizeof(int));
    system->buffer_size = buffer_size;
    system->num_prefetched = 0;
    system->next_index = 0;
}

void prefetch_halo(prefetch_system_t *system, struct halo_data *halo) {
    if (system->num_prefetched < system->buffer_size) {
        // Add to buffer
        system->prefetch_buffer[system->num_prefetched] = halo;
        system->num_prefetched++;
        
        // Use compiler prefetch hint
        __builtin_prefetch(halo, 0, 3);  // Read access, high temporal locality
    }
}

// SIMD-optimized physics calculation (example for cooling)
#ifdef __SSE4_1__
#include <smmintrin.h>

void calculate_cooling_vectorized(galaxy_hotpath_data_t *hotpath, int start_idx, int count) {
    // Process 4 galaxies at once using SSE
    for (int i = start_idx; i < start_idx + count - 3; i += 4) {
        // Load 4 hot gas values
        __m128 hot_gas = _mm_loadu_ps(&hotpath->HotGas[i]);
        // Load 4 metal values
        __m128 metals = _mm_loadu_ps(&hotpath->MetalsHotGas[i]);
        
        // Calculate metallicity (metals / hot gas)
        __m128 metallicity = _mm_div_ps(metals, hot_gas);
        
        // Calculate temperature based on hot gas
        __m128 temperature = calculate_temperature_sse(hot_gas);
        
        // Calculate cooling rate
        __m128 cooling_rate = calculate_cooling_rate_sse(temperature, metallicity);
        
        // Apply cooling to hot gas
        hot_gas = _mm_sub_ps(hot_gas, cooling_rate);
        
        // Store results back
        _mm_storeu_ps(&hotpath->HotGas[i], hot_gas);
    }
    
    // Handle remaining elements (less than 4)
    // ...
}
#endif

// Callback caching system
struct cached_callback {
    int caller_id;
    int callee_id;
    char function_name[64];
    void *function_ptr;
    int64_t last_used;
    int hit_count;
};

struct callback_cache {
    struct cached_callback entries[MAX_CACHED_CALLBACKS];
    int num_entries;
    int64_t access_counter;
    int hit_count;
    int miss_count;
};

void *lookup_cached_callback(struct callback_cache *cache, int caller_id, 
                           int callee_id, const char *function_name) {
    // Look for existing cache entry
    for (int i = 0; i < cache->num_entries; i++) {
        struct cached_callback *entry = &cache->entries[i];
        if (entry->caller_id == caller_id && 
            entry->callee_id == callee_id && 
            strcmp(entry->function_name, function_name) == 0) {
            
            entry->last_used = cache->access_counter++;
            entry->hit_count++;
            cache->hit_count++;
            return entry->function_ptr;
        }
    }
    
    cache->miss_count++;
    return NULL;  // Not found
}
```

#### Design Rationale
Structure-of-Arrays (SoA) layouts improve cache efficiency for vectorized operations by ensuring data is contiguous in memory. Prefetching reduces cache miss penalties during tree traversal by loading data before it's needed. SIMD vectorization exploits modern CPU capabilities for processing multiple data elements simultaneously. Callback caching reduces the overhead of repeated function lookups in the module system.

#### Key Risks and Mitigation
- **Risk**: Complex memory management with SoA/AoS conversions
  - **Mitigation**: Limit conversions to hot code paths, encapsulate conversions in helper functions
- **Risk**: Platform-specific optimization compatibility
  - **Mitigation**: Implement feature detection, provide fallback implementations
- **Risk**: Increased code complexity
  - **Mitigation**: Isolate optimizations behind clean interfaces, use macros for readability

### Phase 7: Documentation and Tools
**Estimated Timeline**: 2-3 months

#### Components
*   **7.1 Developer Documentation**: Create guides for module development, plugin interfaces, extensions, events, callbacks, and dependency management. Provide clear guidance on when to use Events vs Callbacks.
*   **7.2 Scientific Documentation**: Document physical models, create validation reports, scientific usage guides, and reproducibility guidelines.
*   **7.3 Tool Development**: Provide template modules, validation tools (module & scientific), dependency analyzer, call graph visualizer, and benchmarking/profiling tools.

#### Example Implementation
```c
// Template generator for new modules
struct module_template_params {
    char module_name[64];
    enum module_type type;
    char author[64];
    char description[256];
    bool include_serialization;
    bool include_extension_properties;
    bool include_event_handlers;
    module_dependency_t dependencies[MAX_DEPENDENCIES];
    int num_dependencies;
};

int module_generate_template(const struct module_template_params* params, const char* output_dir) {
    // Create directory if it doesn't exist
    mkdir_recursive(output_dir);
    
    // Generate header file
    char header_path[PATH_MAX];
    snprintf(header_path, sizeof(header_path), "%s/%s.h", output_dir, params->module_name);
    FILE* header_file = fopen(header_path, "w");
    if (!header_file) return -1;
    
    // Write header content
    fprintf(header_file, "/**\n");
    fprintf(header_file, " * %s - %s\n", params->module_name, params->description);
    fprintf(header_file, " * Author: %s\n", params->author);
    fprintf(header_file, " */\n\n");
    fprintf(header_file, "#ifndef %s_H\n", strupr(params->module_name));
    fprintf(header_file, "#define %s_H\n\n", strupr(params->module_name));
    
    // Include base module headers
    fprintf(header_file, "#include \"base_module.h\"\n");
    fprintf(header_file, "#include \"%s_module.h\"\n\n", module_type_to_string(params->type));
    
    // Generate module interface
    fprintf(header_file, "// Module interface\n");
    fprintf(header_file, "extern struct %s_module %s_module_impl;\n\n", 
           module_type_to_string(params->type), params->module_name);
    
    // Generate extension property declarations if requested
    if (params->include_extension_properties) {
        fprintf(header_file, "// Extension properties\n");
        fprintf(header_file, "extern int %s_property_ids[MAX_MODULE_PROPERTIES];\n\n", 
               params->module_name);
    }
    
    fprintf(header_file, "#endif // %s_H\n", strupr(params->module_name));
    fclose(header_file);
    
    // Generate implementation file
    // ...
    
    // Generate module data file
    // ...
    
    // Generate test file
    // ...
    
    return 0;
}

// Module validation framework
typedef struct {
    int severity;
    char message[256];
    char component[64];
    char file[128];
    int line;
} module_validation_issue_t;

typedef struct {
    module_validation_issue_t issues[MAX_VALIDATION_ISSUES];
    int num_issues;
    int error_count;
} module_validation_result_t;

module_validation_result_t module_validate(const char* module_path) {
    module_validation_result_t result = {0};
    
    // Check if file exists
    if (!file_exists(module_path)) {
        module_validation_issue_t issue = {
            .severity = VALIDATION_ERROR,
            .component = "file_system",
            .line = 0
        };
        snprintf(issue.message, sizeof(issue.message), "Module file not found: %s", module_path);
        snprintf(issue.file, sizeof(issue.file), "%s", module_path);
        
        result.issues[result.num_issues++] = issue;
        result.error_count++;
        return result;
    }
    
    // Load the module
    module_handle_t handle = load_module(module_path);
    if (!handle) {
        module_validation_issue_t issue = {
            .severity = VALIDATION_ERROR,
            .component = "loader",
            .line = 0
        };
        snprintf(issue.message, sizeof(issue.message), "Failed to load module: %s", 
               get_load_error());
        snprintf(issue.file, sizeof(issue.file), "%s", module_path);
        
        result.issues[result.num_issues++] = issue;
        result.error_count++;
        return result;
    }
    
    // Check for required symbols
    const char* required_symbols[] = {
        "module_get_interface",
        "module_get_manifest",
        "module_initialize",
        "module_cleanup"
    };
    
    for (int i = 0; i < sizeof(required_symbols) / sizeof(required_symbols[0]); i++) {
        void* symbol = get_symbol(handle, required_symbols[i]);
        if (!symbol) {
            module_validation_issue_t issue = {
                .severity = VALIDATION_ERROR,
                .component = "symbols",
                .line = 0
            };
            snprintf(issue.message, sizeof(issue.message), 
                   "Required symbol not found: %s", required_symbols[i]);
            snprintf(issue.file, sizeof(issue.file), "%s", module_path);
            
            result.issues[result.num_issues++] = issue;
            result.error_count++;
        }
    }
    
    // Get and validate manifest
    void* manifest_fn = get_symbol(handle, "module_get_manifest");
    if (manifest_fn) {
        struct module_manifest* (*get_manifest)() = manifest_fn;
        struct module_manifest* manifest = get_manifest();
        
        // Validate manifest
        if (!manifest->name[0]) {
            module_validation_issue_t issue = {
                .severity = VALIDATION_ERROR,
                .component = "manifest",
                .line = 0
            };
            snprintf(issue.message, sizeof(issue.message), "Manifest missing module name");
            snprintf(issue.file, sizeof(issue.file), "%s", module_path);
            
            result.issues[result.num_issues++] = issue;
            result.error_count++;
        }
        
        // More manifest validation...
    }
    
    // Check for memory leaks
    // Validate interface consistency
    // Verify dependencies
    
    // Unload the module
    unload_module(handle);
    
    return result;
}
```

#### Design Rationale
Comprehensive documentation is essential for a modular system to be usable by scientists who might not be familiar with the low-level details. Tools that automate common tasks like module creation and validation reduce barriers to entry for new module developers. The validation framework helps ensure modules meet the expected interface contracts and prevents common issues like memory leaks or missing symbols.

#### Key Risks and Mitigation
- **Risk**: Documentation becoming outdated as the system evolves
  - **Mitigation**: Generate documentation from code where possible, implement documentation checks in CI
- **Risk**: Steep learning curve for new module developers
  - **Mitigation**: Provide comprehensive examples, templates, and validation tools
- **Risk**: Incomplete tooling leading to debugging challenges
  - **Mitigation**: Focus on critical tools first, then expand based on developer feedback

## Deferred Work

*   **Performance Benchmarking**: Moved from Phase 1 to Phase 5/6. Profiling on the refactored structure is more valuable than embedding benchmarks during heavy changes.
*   **Enhanced End-to-End Test Diagnostics**: Skipped. Current logging + `sagediff.py` deemed sufficient.
*   **Dynamic Memory Defragmentation**: Deferred (post-project). Focus on preventative measures (pooling, allocation strategy) first.
*   **Full SIMD Vectorization**: Deferred (post-project). Implement for critical hotspots in Phase 6.3, full vectorization later if needed.

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