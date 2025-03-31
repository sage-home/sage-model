# SAGE Model Refactoring Plan (Revised)

## Table of Contents
1. [Executive Summary](#executive-summary)
2. [Current Architecture Assessment](#current-architecture-assessment)
3. [Detailed Refactoring Strategy](#detailed-refactoring-strategy)
   - [Phase 1: Preparatory Refactoring](#phase-1-preparatory-refactoring-foundation)
   - [Phase 2: Enhanced Module Interface Definition](#phase-2-enhanced-module-interface-definition)
   - [Phase 3: I/O Abstraction and Optimization](#phase-3-io-abstraction-and-optimization)
   - [Phase 4: Advanced Plugin Infrastructure](#phase-4-advanced-plugin-infrastructure)
   - [Phase 5: Core Module Migration](#phase-5-core-module-migration)
   - [Phase 6: Advanced Performance Optimization](#phase-6-advanced-performance-optimization)
   - [Phase 7: Documentation and Tools](#phase-7-documentation-and-tools)
4. [Implementation Plan](#implementation-plan)
5. [Runtime Module System: Practical Examples](#runtime-module-system-practical-examples)
6. [Risk Assessment and Mitigation](#risk-assessment-and-mitigation)
7. [Deferred Work](#deferred-work)
8. [Conclusion](#conclusion)

## Executive Summary

This document outlines a comprehensive plan for refactoring the SAGE (Semi-Analytic Galaxy Evolution) codebase to achieve two primary goals:

1. **Runtime Functional Modularity**: Implementing a dynamic plugin architecture that allows physics modules to be inserted, replaced, reordered, or removed at runtime without recompilation.

2. **Optimized Merger Tree Processing**: Enhancing the efficiency of merger tree handling through improved memory layout, unified I/O abstraction, and intelligent caching strategies while preserving existing data structures and formats.

The plan includes advanced features to maximize flexibility and performance:

- **Galaxy Property Extension Mechanism**: Allowing modules to add custom properties to galaxies without modifying core structures
- **Module Callback System**: Enabling modules to directly invoke other modules while maintaining a clean architecture
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
- Inter-module dependencies that complicate modularization
- Resource management issues in I/O operations, particularly HDF5 handles
- Lack of cross-platform endianness handling for binary formats
- Fixed buffer sizes and inefficient memory allocation strategies

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

#### 2.7 Module Callback System
- Implement a mechanism for inter-module function calls:
  ```c
  // Module dependency declarations
  typedef struct {
      char module_type[32];          // Type of module dependency
      char module_name[64];          // Optional specific module name
      bool required;                 // Whether dependency is required
  } module_dependency_t;
  
  // Add to base_module structure
  struct base_module {
      // ... existing fields ...
      
      // Dependencies
      module_dependency_t *dependencies;
      int num_dependencies;
  };
  
  // Module invocation function
  int module_invoke(
      enum module_type type,
      const char* module_name,      // Optional, NULL for active module of type
      const char* function_name,    // Name of function to call
      void* context,                // Context to pass to function
      void* args,                   // Arguments specific to function
      void* result                  // Optional result pointer
  );
  
  // Call stack management
  typedef struct {
      int caller_module_id;
      int callee_module_id;
      const char* function_name;
      void* context;
  } module_call_frame_t;
  
  typedef struct {
      module_call_frame_t frames[MAX_CALL_DEPTH];
      int depth;
  } module_call_stack_t;
  ```
- Create stack management for tracking module invocations
- Implement context preservation during calls
- Add validation for circular dependencies

### Phase 3: I/O Abstraction and Optimization

#### 3.1 I/O Interface Abstraction
- Create a unified I/O interface that preserves all existing functionality:
  ```c
  struct io_interface {
      // Metadata
      const char* name;           // Name of the I/O handler
      const char* version;        // Version string
      int format_id;              // Unique format identifier
      uint32_t capabilities;      // Bitmap of supported features
      
      // Core operations with error handling
      int (*initialize)(const char* filename, struct params* params);
      int (*read_forest)(int64_t forestnr, struct halo_data** halos, struct forest_info* forest_info);
      int (*write_galaxies)(struct galaxy_output* galaxies, int ngals, struct save_info* save_info);
      int (*cleanup)();
      
      // Resource management (primarily for HDF5)
      int (*close_open_handles)();
      int (*get_open_handle_count)();
  };
  ```
- Implement registry for format handlers with automatic format detection:
  ```c
  struct io_registry {
      struct {
          struct io_interface* handler;
          bool active;
      } handlers[MAX_IO_HANDLERS];
      int num_handlers;
      int active_handler;
  };
  
  // Registration and detection functions
  int io_register_handler(struct io_interface* handler);
  int io_set_active_handler(int format_id);
  int io_detect_format(const char* filename);
  ```
- Add validation for handler compatibility and functionality
- Design error reporting specific to I/O operations
- Implement robust exception handling for all I/O operations

#### 3.2 Format-Specific Implementations
- Refactor existing I/O code into format-specific implementations of the interface
- Implement serialization support for extended properties
- Add cross-platform endianness detection and conversion for binary formats:
  ```c
  // Endianness detection and conversion
  typedef enum {
      ENDIAN_LITTLE,
      ENDIAN_BIG,
      ENDIAN_UNKNOWN
  } endianness_t;
  
  endianness_t detect_system_endianness(void);
  void swap_endianness_int32(int32_t* value);
  void swap_endianness_int64(int64_t* value);
  void swap_endianness_float(float* value);
  void swap_endianness_double(double* value);
  
  // Array conversion helpers
  void convert_endianness_array_int32(int32_t* array, size_t count, endianness_t source, endianness_t target);
  void convert_endianness_array_float(float* array, size_t count, endianness_t source, endianness_t target);
  ```
- Implement robust HDF5 resource tracking and cleanup for preventing handle leaks:
  ```c
  // HDF5 handle tracking
  struct hdf5_handle_tracker {
      hid_t handles[MAX_HDF5_HANDLES];
      int handle_types[MAX_HDF5_HANDLES];  // File, group, dataset, etc.
      int num_handles;
  };
  
  int hdf5_track_handle(hid_t handle, int handle_type);
  int hdf5_untrack_handle(hid_t handle);
  int hdf5_close_all_handles(void);
  int hdf5_get_open_handle_count(void);
  ```
- Create format conversion utilities where appropriate
- Implement standardized error reporting for each format

#### 3.3 Memory Optimization
- Implement configurable buffered reading/writing with runtime-adjustable buffer sizes:
  ```c
  struct io_buffer_config {
      size_t read_buffer_size;      // Size for read operations
      size_t write_buffer_size;     // Size for write operations
      bool use_direct_io;           // Use O_DIRECT if available
      int buffer_alignment;         // Buffer alignment (for direct I/O)
      int flush_threshold;          // Percentage full before auto-flush
  };
  
  int io_set_buffer_config(const struct io_buffer_config* config);
  struct io_buffer_config io_get_default_buffer_config(void);
  ```
- Create memory mapping options for large files:
  ```c
  // Memory mapping for large files
  struct mmap_region {
      void* data;                  // Mapped memory region
      size_t size;                 // Size of mapped region
      int64_t file_offset;         // Offset in file
      int flags;                   // Access flags
  };
  
  struct mmap_region* io_mmap_file_region(const char* filename, int64_t offset, size_t size, int flags);
  int io_munmap_region(struct mmap_region* region);
  ```
- Develop efficient caching strategies for frequently accessed halos:
  ```c
  // Halo caching system
  struct halo_cache {
      struct {
          int64_t key;              // Composite key (forestID + haloID)
          struct halo_data* data;   // Cached halo data
          int64_t last_access;      // Access counter for LRU
          int ref_count;            // Reference count for shared access
      } entries[MAX_HALO_CACHE_SIZE];
      int num_entries;
      int64_t access_counter;
      int hit_count;                // Cache statistics
      int miss_count;
  };
  
  struct halo_data* halo_cache_get(int64_t forest_id, int64_t halo_id);
  int halo_cache_put(int64_t forest_id, int64_t halo_id, struct halo_data* data);
  int halo_cache_remove(int64_t forest_id, int64_t halo_id);
  float halo_cache_hit_rate(void);
  ```
- Optimize allocation strategies with geometric growth instead of fixed increments:
  ```c
  // Growth factor for dynamic arrays
  #define ARRAY_GROWTH_FACTOR 1.5
  
  // Example function for galaxy array growth
  int expand_galaxy_array(struct GALAXY** galaxy_array, int* current_size, int min_new_size) {
      int new_size = *current_size;
      
      // Calculate new size with geometric growth
      while (new_size < min_new_size) {
          new_size = (int)(new_size * ARRAY_GROWTH_FACTOR) + 1;
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
- Implement memory pooling for galaxy allocations:
  ```c
  // Galaxy memory pool
  struct galaxy_pool {
      struct GALAXY* galaxies;      // Pre-allocated galaxy array
      int total_size;               // Total number of galaxies in pool
      int* free_indices;            // Stack of free indices
      int num_free;                 // Number of free slots
  };
  
  struct galaxy_pool* galaxy_pool_create(int initial_size);
  struct GALAXY* galaxy_pool_allocate(struct galaxy_pool* pool);
  void galaxy_pool_free(struct galaxy_pool* pool, struct GALAXY* galaxy);
  void galaxy_pool_destroy(struct galaxy_pool* pool);
  ```

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
- Add error handling specific to dynamic loading:
  ```c
  // Error handling for dynamic loading
  typedef struct {
      int code;                    // Error code
      char message[256];           // Detailed error message
      char module_path[PATH_MAX];  // Path to module that caused error
      char symbol_name[128];       // Symbol name if relevant
  } module_load_error_t;
  
  const char* module_get_last_error_message(void);
  module_load_error_t module_get_last_error(void);
  void module_clear_error(void);
  ```
- Implement platform-specific path handling and library name resolution

#### 4.2 Module Development Framework
- Create template generation for new modules:
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
  
  int module_generate_template(const struct module_template_params* params, const char* output_dir);
  ```
- Implement module validation framework:
  ```c
  // Module validation system
  typedef struct {
      int severity;                // WARN, ERROR, etc.
      char message[256];           // Detailed message
      char component[64];          // Which component failed
      char file[128];              // Source file
      int line;                    // Source line
  } module_validation_issue_t;
  
  typedef struct {
      module_validation_issue_t issues[MAX_VALIDATION_ISSUES];
      int num_issues;
      int error_count;             // Number of actual errors (vs. warnings)
  } module_validation_result_t;
  
  module_validation_result_t module_validate(const char* module_path);
  void module_print_validation_results(const module_validation_result_t* results);
  ```
- Develop debugging utilities:
  ```c
  // Module debugging utilities
  int module_enable_debug(int module_id);
  int module_disable_debug(int module_id);
  int module_set_breakpoint(int module_id, const char* function_name);
  int module_debug_trace_calls(int module_id, bool enable);
  void module_debug_print_state(int module_id);
  ```

#### 4.3 Parameter Tuning System
- Implement parameter registration and validation:
  ```c
  // Parameter tuning system
  typedef struct {
      char name[64];               // Parameter name
      enum {                       // Parameter type
          PARAM_TYPE_INT,
          PARAM_TYPE_FLOAT,
          PARAM_TYPE_DOUBLE,
          PARAM_TYPE_BOOL,
          PARAM_TYPE_STRING
      } type;
      union {                      // Parameter limits
          struct { int min; int max; } int_range;
          struct { float min; float max; } float_range;
          struct { double min; double max; } double_range;
      } limits;
      union {                      // Current value
          int int_val;
          float float_val;
          double double_val;
          bool bool_val;
          char string_val[128];
      } value;
      bool has_limits;             // Whether limits are enforced
      char description[256];       // Parameter description
      char units[32];              // Units (if applicable)
  } module_parameter_t;
  
  int module_register_parameter(int module_id, const module_parameter_t* param);
  int module_get_parameter(int module_id, const char* name, module_parameter_t* param);
  int module_set_parameter(int module_id, const char* name, const void* value);
  ```
- Create runtime modification capabilities:
  ```c
  // Runtime parameter modification
  int module_set_parameter_int(int module_id, const char* name, int value);
  int module_set_parameter_float(int module_id, const char* name, float value);
  int module_set_parameter_double(int module_id, const char* name, double value);
  int module_set_parameter_bool(int module_id, const char* name, bool value);
  int module_set_parameter_string(int module_id, const char* name, const char* value);
  
  // Parameter file/export support
  int module_load_parameters(int module_id, const char* filename);
  int module_save_parameters(int module_id, const char* filename);
  ```
- Add parameter bounds checking:
  ```c
  // Parameter validation
  bool module_validate_parameter(const module_parameter_t* param, const void* value);
  int module_validate_all_parameters(int module_id);
  char* module_get_parameter_validation_errors(int module_id);
  ```

#### 4.4 Module Discovery and Loading
- Implement directory scanning for modules:
  ```c
  // Module discovery
  struct module_scan_result {
      struct {
          char path[PATH_MAX];
          char name[64];
          enum module_type type;
          char version[32];
      } modules[MAX_DISCOVERED_MODULES];
      int num_modules;
  };
  
  struct module_scan_result module_scan_directory(const char* directory);
  int module_auto_discover(const char* base_directory, bool recursive);
  ```
- Create manifest parsing:
  ```c
  // Manifest parsing
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
  
  int module_parse_manifest(const char* filename, struct module_manifest* manifest);
  int module_validate_manifest(const struct module_manifest* manifest);
  ```
- Develop dependency resolution:
  ```c
  // Dependency resolution
  typedef struct {
      struct {
          int module_id;
          module_dependency_t* dependency;
          bool satisfied;
          char error_message[256];
      } dependencies[MAX_TOTAL_DEPENDENCIES];
      int num_dependencies;
      bool all_satisfied;
  } dependency_resolution_result_t;
  
  dependency_resolution_result_t module_resolve_dependencies(int module_id);
  int module_auto_resolve_dependencies(const char* module_base_dir);
  ```
- Add validation of module dependencies:
  ```c
  // Dependency validation
  bool module_is_compatible(int module_id, int dependency_module_id);
  bool module_is_version_compatible(const char* version, const char* min_version, const char* max_version, bool exact_match);
  char* module_get_dependency_errors(int module_id);
  ```

#### 4.5 Error Handling
- Design comprehensive error reporting system:
  ```c
  // Enhanced error handling
  typedef struct {
      int code;                   // Error code
      int severity;               // Severity level
      char message[256];          // Detailed message
      char component[64];         // Component where error occurred
      char function[64];          // Function where error occurred
      char file[128];             // Source file
      int line;                   // Source line
      void* context;              // Context pointer for additional data
  } module_error_t;
  
  void module_error_set(const module_error_t* error);
  module_error_t module_error_get(void);
  void module_error_clear(void);
  const char* module_error_to_string(const module_error_t* error);
  ```
- Implement call stack tracing for debugging module interactions:
  ```c
  // Call stack tracing
  #define MAX_STACK_TRACE_DEPTH 32
  
  typedef struct {
      struct {
          char module_name[64];
          char function[64];
          int module_id;
      } frames[MAX_STACK_TRACE_DEPTH];
      int depth;
  } module_stack_trace_t;
  
  module_stack_trace_t module_get_stack_trace(void);
  char* module_stack_trace_to_string(const module_stack_trace_t* trace);
  void module_log_stack_trace(const module_stack_trace_t* trace);
  ```
- Add standardized logging for module errors:
  ```c
  // Module-specific logging
  void module_log_error(int module_id, const char* format, ...);
  void module_log_warning(int module_id, const char* format, ...);
  void module_log_info(int module_id, const char* format, ...);
  void module_log_debug(int module_id, const char* format, ...);
  
  // Enable/disable logging per module
  int module_set_log_level(int module_id, int level);
  int module_log_to_file(int module_id, const char* filename);
  ```

### Phase 5: Core Module Migration

#### 5.1 Refactoring Main Evolution Loop
- Transform the monolithic `evolve_galaxies()` function to use the pipeline system
- Implement event-based communication
- Add support for extension properties
- Integrate module callback system for inter-module dependencies
- Add detailed diagnostics and validation:
  ```c
  // Evolution diagnostics
  struct evolution_diagnostics {
      int galaxy_index;
      int step;
      double time;
      struct {
          double cold_gas_before;
          double stellar_mass_before;
          double metals_before;
          // ... other properties
          double cold_gas_after;
          double stellar_mass_after;
          double metals_after;
      } property_changes;
      struct {
          char module_name[64];
          int64_t execution_time_ns;  // Nanoseconds
          int num_events_generated;
          int num_callbacks_made;
      } module_stats[MAX_PIPELINE_STEPS];
      int num_modules;
  };
  
  void evolution_log_diagnostics(const struct evolution_diagnostics* diag);
  void evolution_validate_galaxy_changes(const struct evolution_diagnostics* diag);
  ```

#### 5.2 Converting Physics Modules
- Extract each physics component to a standalone module
- Add property registration and event handling
- Implement module-specific data management
- Define and implement module dependencies using the callback system
- Create compatibility layers for transition:
  ```c
  // Legacy compatibility layer
  double cooling_recipe_legacy(int p, double dt);
  void starformation_recipe_legacy(int p, double dt, double *stars, double *metals);
  void reincorporate_gas_legacy(int p, double dt);
  
  // Bridge functions
  int cooling_module_bridge(int p, double dt, double *result);
  int starformation_module_bridge(int p, double dt, double *stars, double *metals);
  int reincorporation_module_bridge(int p, double dt);
  ```

#### 5.3 Validation and Testing
- Create scientific validation tests:
  ```c
  // Scientific validation
  struct validation_metrics {
      double total_stellar_mass;
      double total_cold_gas;
      double total_hot_gas;
      double stellar_mass_function[MASS_BINS];
      double black_hole_mass_function[MASS_BINS];
      double cold_gas_metallicity;
      double hot_gas_metallicity;
      double optical_luminosity_function[MAG_BINS];
      // ... other statistics
  };
  
  void compute_validation_metrics(struct GALAXY *galaxies, int ngal, struct validation_metrics *metrics);
  bool validate_output_against_reference(const struct validation_metrics *metrics, const char *reference_file);
  void output_validation_report(const struct validation_metrics *metrics, const char *output_file);
  ```
- Implement performance benchmarks:
  ```c
  // Performance benchmarking
  struct benchmark_results {
      int64_t total_runtime_ms;
      int64_t io_time_ms;
      int64_t merger_tree_time_ms;
      int64_t galaxy_evolution_time_ms;
      int64_t memory_peak_kb;
      struct {
          char module_name[64];
          int64_t total_time_ms;
          int64_t call_count;
          double time_per_call_ms;
      } module_timings[MAX_MODULES];
      int num_modules;
  };
  
  void run_performance_benchmark(const char *input_file, struct benchmark_results *results);
  void compare_benchmark_results(const struct benchmark_results *results, const struct benchmark_results *baseline);
  void output_benchmark_report(const struct benchmark_results *results, const char *output_file);
  ```
- Develop compatibility tests:
  ```c
  // Module compatibility testing
  struct compatibility_test_result {
      bool is_compatible;
      char error_message[256];
      struct {
          char name[64];
          char version[32];
          bool is_compatible;
          char reason[128];
      } module_results[MAX_MODULES];
      int num_modules;
  };
  
  struct compatibility_test_result test_module_compatibility(int module_id, const char *against_module_dir);
  bool validate_plugin_against_api_version(int module_id, int target_api_version);
  ```
- Add call graph validation to ensure proper module interactions:
  ```c
  // Call graph validation
  struct module_call_graph {
      struct {
          int caller_id;
          int callee_id;
          char function_name[64];
          int call_count;
      } edges[MAX_CALL_EDGES];
      int num_edges;
  };
  
  struct module_call_graph record_module_call_graph(void);
  bool validate_call_graph(const struct module_call_graph *graph);
  void visualize_call_graph(const struct module_call_graph *graph, const char *output_file);
  ```

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
  
  // Functions for SoA conversion
  void convert_AoS_to_SoA(struct GALAXY *galaxies, int ngal, galaxy_hotpath_data_t *hotpath);
  void convert_SoA_to_AoS(galaxy_hotpath_data_t *hotpath, struct GALAXY *galaxies, int ngal);
  void process_galaxies_SoA(galaxy_hotpath_data_t *hotpath, int ngal, void (*process_func)(galaxy_hotpath_data_t*, int, int, void*), void *args);
  ```
- Implement memory pooling for galaxy allocations:
  ```c
  // Enhanced memory pooling
  struct memory_pool {
      void *blocks[MAX_MEMORY_BLOCKS];
      size_t block_size;
      int total_blocks;
      int used_blocks;
      void **free_list;
      int free_list_size;
      size_t total_allocated;
      size_t peak_allocated;
  };
  
  struct memory_pool *create_memory_pool(size_t element_size, int initial_elements);
  void *pool_allocate(struct memory_pool *pool);
  void pool_free(struct memory_pool *pool, void *ptr);
  void destroy_memory_pool(struct memory_pool *pool);
  void pool_stats(struct memory_pool *pool, char *buffer, size_t buffer_size);
  ```
- Reduce memory fragmentation during tree processing:
  ```c
  // Memory defragmentation
  void defragment_galaxy_array(struct GALAXY **galaxies, int *ngal);
  void compact_memory_pool(struct memory_pool *pool);
  void optimize_memory_layout(struct GALAXY *galaxies, int ngal);
  ```
- Implement segregated free lists for different-sized allocations:
  ```c
  // Size-segregated memory pools
  struct size_class {
      size_t size;
      struct memory_pool *pool;
  };
  
  struct memory_allocator {
      struct size_class classes[MAX_SIZE_CLASSES];
      int num_classes;
      struct memory_pool *fallback_pool;
  };
  
  struct memory_allocator *create_memory_allocator(void);
  void *allocator_allocate(struct memory_allocator *allocator, size_t size);
  void allocator_free(struct memory_allocator *allocator, void *ptr, size_t size);
  void destroy_memory_allocator(struct memory_allocator *allocator);
  ```

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
  
  // Prefetching operations
  void prefetch_initialize(prefetch_system_t *system, int buffer_size);
  void prefetch_halo(prefetch_system_t *system, struct halo_data *halo);
  struct halo_data *prefetch_get_next(prefetch_system_t *system);
  void prefetch_cleanup(prefetch_system_t *system);
  ```
- Enhance depth-first traversal algorithms:
  ```c
  // Optimized tree traversal
  void traverse_depth_first_optimized(struct halo_data *halos, int nhalo, void (*process_func)(struct halo_data*, void*), void *args);
  void traverse_breadth_first_optimized(struct halo_data *halos, int nhalo, void (*process_func)(struct halo_data*, void*), void *args);
  void traverse_level_order_optimized(struct halo_data *halos, int nhalo, void (*process_func)(struct halo_data*, void*), void *args);
  ```
- Optimize pointer-chasing patterns for modern CPUs:
  ```c
  // Cache-conscious tree traversal
  void rearrange_tree_cache_friendly(struct halo_data **halos, int nhalo);
  void linearize_tree_in_memory(struct halo_data **halos, int nhalo);
  void traverse_tree_with_software_prefetching(struct halo_data *halos, int nhalo, void (*process_func)(struct halo_data*, void*), void *args);
  ```
- Add support for non-recursive traversal algorithms:
  ```c
  // Non-recursive traversal
  void traverse_depth_first_nonrecursive(struct halo_data *halos, int nhalo, void (*process_func)(struct halo_data*, void*), void *args);
  void traverse_depth_first_morris(struct halo_data *halos, int nhalo, void (*process_func)(struct halo_data*, void*), void *args);
  ```

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
- Create batch processing for galaxies:
  ```c
  // Batch processing for galaxies
  void process_galaxies_in_batches(struct GALAXY *galaxies, int ngal, int batch_size, void (*batch_func)(struct GALAXY**, int, void*), void *args);
  void batch_cooling_calculation(struct GALAXY **batch, int batch_size, double dt);
  void batch_star_formation(struct GALAXY **batch, int batch_size, double dt);
  ```
- Optimize performance-critical algorithms:
  ```c
  // Performance-critical optimizations
  void optimize_cooling_table_access(void);
  void precalculate_common_physics_terms(void);
  void optimize_subhalo_merger_calculations(void);
  ```
- Implement architecture-specific optimizations:
  ```c
  // Architecture-specific optimizations
  #if defined(__AVX2__)
  #include <immintrin.h>
  void calculate_metals_avx2(float *metals, float *yields, int count);
  #elif defined(__SSE4_1__)
  #include <smmintrin.h>
  void calculate_metals_sse41(float *metals, float *yields, int count);
  #else
  void calculate_metals_scalar(float *metals, float *yields, int count);
  #endif
  
  // Dispatch based on CPU capabilities
  void (*calculate_metals)(float*, float*, int) = NULL;
  void initialize_optimized_functions(void) {
      #if defined(__AVX2__)
      calculate_metals = calculate_metals_avx2;
      #elif defined(__SSE4_1__)
      calculate_metals = calculate_metals_sse41;
      #else
      calculate_metals = calculate_metals_scalar;
      #endif
  }
  ```

#### 6.4 Parallelization Enhancements
- Refine load balancing for heterogeneous trees:
  ```c
  // Advanced load balancing
  typedef struct {
      int forest_id;
      int tree_count;
      int galaxy_estimate;
      int complexity_score;
  } forest_workload_t;
  
  void estimate_forest_workloads(forest_workload_t *workloads, int num_forests);
  void distribute_forests_evenly(forest_workload_t *workloads, int num_forests, int num_tasks, int *task_assignments);
  ```
- Implement finer-grained parallel processing where beneficial:
  ```c
  // Fine-grained parallelism
  #ifdef _OPENMP
  #include <omp.h>
  
  void process_large_forest_parallel(struct halo_data *halos, int nhalo, int num_threads);
  void calculate_galaxy_properties_parallel(struct GALAXY *galaxies, int ngal);
  #endif
  ```
- Optimize MPI communication patterns:
  ```c
  // MPI optimization
  #ifdef MPI_ON
  #include <mpi.h>
  
  void optimize_mpi_forest_distribution(void);
  void reduce_mpi_communication_overhead(void);
  void implement_non_blocking_io_mpi(void);
  #endif
  ```
- Add hybrid parallelization approach:
  ```c
  // Hybrid parallelization (MPI + OpenMP)
  #if defined(MPI_ON) && defined(_OPENMP)
  void configure_hybrid_parallelization(int mpi_tasks, int openmp_threads_per_task);
  void balance_hybrid_workload(void);
  #endif
  ```

#### 6.5 Module Callback Optimization
- Implement callback caching to reduce lookup overhead:
  ```c
  // Callback caching
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
  
  void *lookup_cached_callback(int caller_id, int callee_id, const char *function_name);
  void cache_callback(int caller_id, int callee_id, const char *function_name, void *function_ptr);
  ```
- Create optimized paths for frequent module interactions:
  ```c
  // Fast-path for common module interactions
  void register_fast_path(int caller_id, int callee_id, const char *function_name);
  void *get_fast_path_function(int caller_id, int callee_id, const char *function_name);
  ```
- Add profiling for inter-module calls to identify bottlenecks:
  ```c
  // Module call profiling
  struct module_call_profile {
      int caller_id;
      int callee_id;
      char function_name[64];
      int64_t total_calls;
      int64_t total_time_ns;
      int64_t min_time_ns;
      int64_t max_time_ns;
      double avg_time_ns;
  };
  
  struct module_profiler {
      struct module_call_profile profiles[MAX_PROFILED_CALLS];
      int num_profiles;
      bool enabled;
  };
  
  void module_profiler_start(void);
  void module_profiler_stop(void);
  void module_profiler_record_call(int caller_id, int callee_id, const char *function_name, int64_t time_ns);
  void module_profiler_report(const char *output_file);
  ```
- Implement inlining for critical module functions:
  ```c
  // Function inlining for critical paths
  typedef void (*module_inline_function_t)(void*, void*, void*);
  
  int module_register_inline_function(int module_id, const char *function_name, module_inline_function_t inline_func);
  module_inline_function_t module_get_inline_function(int module_id, const char *function_name);
  ```

### Phase 7: Documentation and Tools

#### 7.1 Developer Documentation
- Create comprehensive guide for module development:
  ```
  # Module Development Guide
  
  ## Overview
  - Module architecture and lifecycle
  - Interface requirements
  - Extension mechanism
  - Event system
  - Callback system
  
  ## Creating a New Module
  - Using the template generator
  - Required functions
  - Optional features
  - Testing and validation
  
  ## Advanced Topics
  - Performance optimization
  - Memory management
  - Thread safety
  - Debugging
  ```
- Document plugin architecture interfaces:
  ```
  # Plugin Interface Reference
  
  ## Base Module Interface
  - Fields and methods
  - Lifecycle management
  - Error handling
  
  ## Physics-Specific Interfaces
  - Cooling module
  - Star formation module
  - Feedback module
  - AGN module
  - Mergers module
  - Disk instability module
  - Reincorporation module
  - Infall module
  
  ## Extension Mechanism
  - Registering properties
  - Accessing extension data
  - Serialization
  
  ## Event System
  - Event types
  - Registration
  - Handling
  ```
- Provide examples for common customizations:
  ```
  # Customization Examples
  
  ## Custom Cooling Model
  - Implementing temperature-dependent cooling
  - Adding metal-line cooling
  
  ## Custom Star Formation
  - Implementing H2-based star formation
  - Adding star formation thresholds
  
  ## Custom Feedback
  - Variable mass loading factor
  - Energy-driven outflows
  
  ## Cross-Module Integration
  - Using callbacks between modules
  - Sharing data through extensions
  ```
- Create documentation for module dependency management:
  ```
  # Dependency Management Guide
  
  ## Declaring Dependencies
  - Required vs. optional
  - Version constraints
  - Type-based vs. name-based
  
  ## Resolving Dependencies
  - Automatic resolution
  - Manual configuration
  - Troubleshooting
  
  ## Circular Dependency Prevention
  - Identifying cycles
  - Breaking circular dependencies
  - Design patterns for module interaction
  ```

#### 7.2 Scientific Documentation
- Document physical models and implementation details:
  ```
  # Physics Implementation Reference
  
  ## Cooling Model
  - Equations and assumptions
  - Cooling tables
  - Metal-line cooling
  
  ## Star Formation
  - Schmidt-Kennicutt law
  - Threshold conditions
  - IMF implementation
  
  ## Feedback
  - SNe feedback
  - Stellar winds
  - Energy vs. momentum-driven
  
  ## AGN Feedback
  - Radio mode
  - Quasar mode
  - Black hole growth
  ```
- Create validation reports:
  ```
  # Scientific Validation Report
  
  ## Methodology
  - Test datasets
  - Comparison metrics
  - Statistical methods
  
  ## Results
  - Stellar mass function
  - Gas fractions
  - Black hole scaling relations
  - Star formation history
  
  ## Comparison with Observations
  - SDSS comparisons
  - High-redshift comparisons
  - Scaling relations
  ```
- Develop scientific usage guides:
  ```
  # Scientific Usage Guide
  
  ## Parameter Selection
  - Cosmological parameters
  - Free parameters
  - Parameter interdependencies
  
  ## Running Simulations
  - Input preparation
  - Runtime configuration
  - Output analysis
  
  ## Creating New Physics
  - Theoretical framework
  - Implementation strategy
  - Testing approach
  ```
- Add best practices for scientific reproducibility:
  ```
  # Reproducibility Guide
  
  ## Version Control
  - Tagging releases
  - Parameter file management
  - Recording random seeds
  
  ## Documentation
  - Parameter choices
  - Data processing steps
  - Analysis scripts
  
  ## Sharing Results
  - Output formats
  - Metadata inclusion
  - Public repositories
  ```

#### 7.3 Tool Development
- Create template modules for different physics components:
  ```
  # Module Templates
  
  ## Basic Templates
  - Minimal cooling module
  - Basic star formation module
  - Simple feedback module
  
  ## Advanced Templates
  - Multi-phase gas cooling
  - H2-based star formation
  - Energy-driven feedback
  - Two-mode AGN feedback
  ```
- Develop validation tools for module compatibility:
  ```
  # Validation Tools
  
  ## Module Validator
  - Interface compliance checking
  - Memory leak detection
  - Performance analysis
  
  ## Scientific Validator
  - Output comparison
  - Conservation law checking
  - Statistical tests
  ```
- Implement module dependency analyzer:
  ```
  # Dependency Analyzer
  
  ## Static Analysis
  - Dependency graph generation
  - Cycle detection
  - Version compatibility checking
  
  ## Runtime Analysis
  - Call pattern visualization
  - Timing analysis
  - Resource usage monitoring
  ```
- Create module call graph visualization tools:
  ```
  # Call Graph Visualizer
  
  ## Graph Generation
  - Module-level graphs
  - Function-level graphs
  - Dynamic call tracking
  
  ## Visualization
  - Interactive graphs
  - Heatmap generation
  - Time-based visualization
  ```
- Develop benchmarking and profiling tools:
  ```
  # Performance Tools
  
  ## Benchmarking Suite
  - Standard test cases
  - Comparison metrics
  - Historical tracking
  
  ## Profiler
  - Module-level profiling
  - Function-level profiling
  - Memory usage analysis
  
  ## Optimization Advisor
  - Hotspot identification
  - Optimization suggestions
  - Before/after comparison
  ```

## Implementation Plan

### Timeline Estimate

| Phase | Description | Duration (months) | Dependencies |
|-------|-------------|-------------------|-------------|
| 1 | Preparatory Refactoring | 3 | None |
| 2 | Enhanced Module Interface Definition | 5-6 | Phase 1 |
| 3 | I/O Abstraction and Optimization | 2-3 | Phase 1 |
| 4 | Advanced Plugin Infrastructure | 3-4 | Phase 2 |
| 5 | Core Module Migration | 4-5 | Phase 4 |
| 6 | Advanced Performance Optimization | 3-4 | Phases 3, 5 |
| 7 | Documentation and Tools | 2-3 | Phases 1-6 |

**Total Estimated Duration**: 22-28 months

The critical path runs through Phases 1 → 2 → 4 → 5 → 6 → 7, with Phase 3 able to proceed in parallel with Phase 2 after Phase 1 is complete. The addition of the Module Callback System in Phase 2.7 increases the duration of Phase 2 by approximately 1 month.

### Monthly Milestone Plan

#### Months 1-3: Phase 1 (Preparatory Refactoring)
- **Month 1**: Reorganize directory structure, extract physics functions, adopt consistent naming conventions
- **Month 2**: Encapsulate global parameters, refactor function signatures, implement initialization and cleanup routines
- **Month 3**: Enhance error logging, validate evolution context, update inline documentation

#### Months 4-9: Phase 2 (Enhanced Module Interface Definition)
- **Month 4**: Define base module interfaces, implement first physics module interface (cooling)
- **Month 5**: Implement galaxy property extension mechanism, add extension memory management
- **Month 6**: Create event-based communication system, implement registry for physics modules
- **Month 7**: Develop module pipeline system, implement configurable execution order
- **Month 8**: Create configuration system, develop parameter parsing and validation
- **Month 9**: Implement module callback system, add dependency tracking and circular detection

#### Months 7-9: Phase 3 (I/O Abstraction and Optimization)
- **Month 7**: Define I/O interface structure, implement handler registry, add format detection
- **Month 8**: Refactor binary and HDF5 format handlers, add endianness handling, implement resource tracking
- **Month 9**: Optimize memory usage, implement configurable buffering, add caching strategies

#### Months 10-13: Phase 4 (Advanced Plugin Infrastructure)
- **Month 10**: Implement cross-platform library loading, add error handling for dynamic loading
- **Month 11**: Create module template generation, develop validation framework
- **Month 12**: Implement parameter tuning system, add bounds checking and runtime modification
- **Month 13**: Add module discovery and loading, implement dependency resolution and validation

#### Months 14-18: Phase 5 (Core Module Migration)
- **Month 14**: Begin refactoring evolution loop, add pipeline integration
- **Month 15**: Start converting physics modules, implement property registration
- **Month 16**: Add event handling to physics modules, implement module-specific data management
- **Month 17**: Define module dependencies, implement callback integration
- **Month 18**: Create validation tests, develop compatibility checks, add call graph validation

#### Months 19-22: Phase 6 (Advanced Performance Optimization)
- **Month 19**: Optimize memory layouts, implement SoA conversions for hot-path data
- **Month 20**: Enhance tree traversal, add prefetching, optimize pointer-chasing patterns
- **Month 21**: Implement vectorized calculations where appropriate, add batch processing
- **Month 22**: Enhance parallelization, optimize module callbacks, add profiling

#### Months 23-25: Phase 7 (Documentation and Tools)
- **Month 23**: Create developer documentation, detail module development process
- **Month 24**: Document physical models, create validation reports
- **Month 25**: Develop tools for module development, analysis, and debugging

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

### Example 3: Module Callback System for Direct Module Invocation

#### Module Dependencies Declaration:
```c
// In merger module definition
static module_dependency_t merger_dependencies[] = {
    { .module_type = "star_formation", .module_name = NULL, .required = true },
    { .module_type = "disk_instability", .module_name = NULL, .required = false }
};

struct mergers_module mergers_module_impl = {
    .base = {
        .name = "StandardMergers",
        .version = "1.0.0",
        .author = "SAGE Team",
        .initialize = merger_module_initialize,
        .cleanup = merger_module_cleanup,
        .dependencies = merger_dependencies,
        .num_dependencies = 2
    },
    .process_merger = merger_module_process
};
```

#### Direct Module Invocation:
```c
// In merger module implementation
static int merger_module_process(int p, int centralgal, struct GALAXY *galaxies, 
                              struct merger_context *context, void *module_data) {
    // Calculate merger parameters
    float mass_ratio = galaxies[p].StellarMass / galaxies[centralgal].StellarMass;
    
    // Trigger star formation during merger
    if (mass_ratio > 0.1) {
        // Prepare arguments for star formation module
        struct sf_burst_args args = {
            .galaxy_index = p,
            .burst_ratio = mass_ratio * 0.5,
            .merger_triggered = true
        };
        
        // Invoke star formation module directly
        double stars_formed = 0.0;
        int status = module_invoke(
            MODULE_TYPE_STAR_FORMATION,
            NULL,                           // Use active module
            "trigger_burst",                // Function name
            context,                        // Pass current context
            &args,                          // Module-specific arguments
            &stars_formed                   // Result pointer
        );
        
        if (status == 0) {
            LOG_DEBUG("Merger-triggered star formation created %.2f stars", stars_formed);
        } else {
            LOG_WARNING("Failed to trigger star formation burst during merger");
        }
    }
    
    return 0;
}
```

### Example 4: Performance Optimization with Cache-Friendly Structures

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

### Example 5: I/O Optimizations with Endianness Handling and Resource Management

```c
// Binary format implementation with endianness handling
static int binary_write_galaxies(struct galaxy_output* galaxies, int ngals, struct save_info* save_info) {
    // Detect system endianness
    endianness_t system_endian = detect_system_endianness();
    endianness_t file_endian = ENDIAN_LITTLE;  // Format uses little endian
    
    // Open output file with configurable buffer size
    FILE* file = fopen(save_info->filename, "wb");
    if (file == NULL) {
        LOG_ERROR("Failed to open output file: %s", save_info->filename);
        return IO_ERROR_FILE_OPEN;
    }
    
    // Set optimal buffer size
    if (setvbuf(file, NULL, _IOFBF, io_get_buffer_config()->write_buffer_size) != 0) {
        LOG_WARNING("Failed to set buffer size for output file");
    }
    
    // Write header with endianness marker
    binary_file_header_t header;
    init_binary_header(&header, ngals, save_info);
    header.endianness_marker = ENDIANNESS_MARKER;
    
    // Write header directly (no conversion needed for marker)
    fwrite(&header, sizeof(header), 1, file);
    
    // Allocate buffer for conversion if needed
    struct galaxy_output* temp_buffer = NULL;
    if (system_endian != file_endian) {
        temp_buffer = mymalloc(sizeof(struct galaxy_output) * MIN(ngals, CONVERSION_BUFFER_SIZE));
        if (temp_buffer == NULL) {
            LOG_ERROR("Failed to allocate temporary buffer for endianness conversion");
            fclose(file);
            return IO_ERROR_MEMORY;
        }
    }
    
    // Write galaxies in batches with endianness conversion if needed
    for (int i = 0; i < ngals; i += CONVERSION_BUFFER_SIZE) {
        int batch_size = MIN(CONVERSION_BUFFER_SIZE, ngals - i);
        
        if (system_endian != file_endian) {
            // Copy galaxies to temporary buffer and convert endianness
            memcpy(temp_buffer, &galaxies[i], sizeof(struct galaxy_output) * batch_size);
            convert_galaxy_output_endianness(temp_buffer, batch_size, system_endian, file_endian);
            
            // Write converted data
            fwrite(temp_buffer, sizeof(struct galaxy_output), batch_size, file);
        } else {
            // Write directly if endianness matches
            fwrite(&galaxies[i], sizeof(struct galaxy_output), batch_size, file);
        }
    }
    
    // Clean up
    if (temp_buffer != NULL) {
        myfree(temp_buffer);
    }
    
    fclose(file);
    return IO_SUCCESS;
}

// HDF5 format implementation with resource tracking
static int hdf5_write_galaxies(struct galaxy_output* galaxies, int ngals, struct save_info* save_info) {
    // Open or create HDF5 file
    hid_t file_id = H5Fcreate(save_info->filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file_id < 0) {
        LOG_ERROR("Failed to create HDF5 file: %s", save_info->filename);
        return IO_ERROR_FILE_CREATE;
    }
    
    // Track the file handle
    hdf5_track_handle(file_id, HDF5_HANDLE_FILE);
    
    // Create group for this snapshot
    char group_name[128];
    snprintf(group_name, sizeof(group_name), "/Snap_%03d", save_info->snapshot);
    hid_t group_id = H5Gcreate(file_id, group_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (group_id < 0) {
        LOG_ERROR("Failed to create HDF5 group: %s", group_name);
        hdf5_close_all_handles();
        return IO_ERROR_GROUP_CREATE;
    }
    
    // Track the group handle
    hdf5_track_handle(group_id, HDF5_HANDLE_GROUP);
    
    // Write galaxy data with chunking and compression
    herr_t status = write_galaxy_dataset(file_id, group_id, galaxies, ngals);
    if (status < 0) {
        LOG_ERROR("Failed to write galaxy dataset");
        hdf5_close_all_handles();
        return IO_ERROR_DATASET_WRITE;
    }
    
    // Write metadata
    status = write_metadata(file_id, group_id, save_info);
    if (status < 0) {
        LOG_ERROR("Failed to write metadata");
        hdf5_close_all_handles();
        return IO_ERROR_ATTRIBUTE_WRITE;
    }
    
    // Write extension properties if present
    status = write_extension_properties(file_id, group_id, galaxies, ngals);
    if (status < 0) {
        LOG_WARNING("Failed to write some extension properties");
        // Continue anyway
    }
    
    // Close all handles in reverse order of creation
    hdf5_close_all_handles();
    
    // Verify all handles were closed properly
    int remaining = hdf5_get_open_handle_count();
    if (remaining > 0) {
        LOG_WARNING("HDF5 resource leak detected: %d handles still open", remaining);
    }
    
    return IO_SUCCESS;
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
| Circular dependencies | Medium | Medium | Static dependency analysis; runtime cycle detection; careful module design |
| Module callback overhead | Medium | Medium | Optimize callback paths; cache frequent calls; profile and tune callback system |
| Cross-platform compatibility issues | Medium | Medium | Implement proper endianness handling; abstract platform-specific code; standardized error handling |
| HDF5 resource leaks | Medium | High | Implement comprehensive resource tracking; validate handle management; add cleanup checks |
| Memory allocation inefficiency | Medium | Medium | Implement geometric growth strategies; optimize pooled allocations; reduce fragmentation |

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

### Dynamic Memory Defragmentation
- **Original Phase**: 6.1 (Memory Layout Enhancement)
- **New Target Phase**: Post-project optimization if needed
- **Rationale**: Implementation of geometric growth allocation and memory pooling is likely to address most memory fragmentation issues without the complexity of runtime defragmentation.
- **Alternative Approach**: Focus on preventative measures (proper allocation patterns, pooling, etc.) rather than corrective defragmentation. Monitor memory usage patterns after refactoring and implement only if significant fragmentation is observed.

### Full SIMD Vectorization
- **Original Phase**: 6.3 (Vectorized Physics Calculations)
- **New Target Phase**: Limited implementation in Phase 6.3 with full implementation post-project
- **Rationale**: SIMD optimization requires careful algorithm redesign and is highly CPU-specific. Initial implementation will focus on the most critical calculations with the highest potential gain.
- **Alternative Approach**: Implement architecture detection and dispatch to specialized implementations only for the most performance-critical functions. Defer comprehensive vectorization to a future optimization phase.

## Conclusion

This refactoring plan provides a detailed, practical approach to modernizing the SAGE codebase. The enhanced plugin architecture will enable researchers to easily swap physics implementations and reorder processes without recompilation, significantly enhancing flexibility for experimentation. The property extension mechanism will allow for novel physics to be implemented without modifying core structures, and the event system will enable more flexible module interactions.

The newly added Module Callback System addresses a critical limitation in the original design by enabling physics modules to directly invoke other modules when needed, preserving the complex interdependencies present in the original codebase while maintaining a clean architecture. This ensures that scientific accuracy is preserved while still achieving the goals of modularity and extensibility.

The advanced performance optimizations will improve efficiency while maintaining compatibility with existing data formats. The enhanced I/O system with improved resource management, endianness handling, and memory allocation strategies will ensure robust, cross-platform compatible operation.

By following an incremental approach with continuous validation, we can ensure that the scientific integrity of the model is preserved throughout the refactoring process. The comprehensive documentation and development tools will facilitate adoption by the scientific community and encourage collaborative development of new physics modules.

The result will be a more flexible, maintainable, and efficient codebase that facilitates scientific exploration and collaboration while building on the solid foundation of the existing SAGE implementation.