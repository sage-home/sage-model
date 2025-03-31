# SAGE Model Refactoring Plan - Condensed & Updated

## Goals

1.  **Runtime Functional Modularity**: Implement a dynamic plugin architecture allowing runtime insertion, replacement, reordering, or removal of physics modules without recompilation.
2.  **Optimized Merger Tree Processing**: Enhance merger tree handling efficiency via improved memory layout, unified I/O abstraction, and caching, preserving existing formats.

## Detailed Refactoring Strategy

### Phase 1: Preparatory Refactoring (Foundation) [Partially Complete]

*   **1.1 Code Organization**: Ensure separation of core, physics, I/O. Extract physics functions. Consistent naming.
*   **1.2 Global State Reduction**: Encapsulate parameters. Refactor signatures for explicit parameter passing. Implement init/cleanup routines.
*   **1.3 Comprehensive Testing Framework**: Establish validation tests (scientific output), unit tests (core), and performance benchmarks (deferred to Phase 6).
*   **1.4 Documentation Enhancement**: Update inline docs, create architectural docs, document data flow.
*   **1.5 Centralize Utilities**: Identify and consolidate common physics calculations (timescales, radii, densities) into utility functions (e.g., `model_misc.c` or `model_utils.c`).

### Phase 2: Enhanced Module Interface Definition [Partially Complete]

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
    // Example specific interface:
    struct cooling_module { struct base_module base; double (*calculate_cooling)(...); ... };
    ```
*   **2.2 Galaxy Property Extension Mechanism**: Add to `GALAXY` struct: `void **extension_data; int num_extensions; uint64_t extension_flags;`. Implement property registration (`galaxy_property_t`), memory management, access APIs. Review `const` correctness of getter/allocator (`galaxy_extension_get_data`).
*   **2.3 Event-Based Communication System**: Define event types (`galaxy_event_type_t`) and structure (`galaxy_event_t`). Implement register/dispatch mechanisms and handlers.
*   **2.4 Module Registry System**: Implement central registry (`module_registry_t`) tracking loaded modules, handles, interfaces, active status.
*   **2.5 Module Pipeline System**: Implement configurable pipeline (`module_pipeline_t`, `pipeline_step`) for execution order.
*   **2.6 Hybrid Configuration System**:
    *   Retain `core_read_parameter_file.c` for core scientific/simulation parameters (human-readable `key value` format). Enhance its parser robustness slightly.
    *   Utilize `core_config_system.c` (JSON) for structural configuration: module paths, pipeline definitions, module selection/activation, complex module-specific params.
    *   Establish clear precedence (JSON overrides param file if overlap exists, though separation is preferred).
    *   Ensure JSON parser is robust (consider replacing custom parser with library like `cJSON`).
*   **2.7 Module Callback System**: Implement inter-module calls (`module_invoke`), dependency declaration (`module_declare_dependency`), call stack tracking (`module_call_stack_t`), context preservation, and circular dependency checks.

### Phase 3: I/O Abstraction and Optimization [Current Phase]

*   **3.1 I/O Interface Abstraction**: Define unified `io_interface` (init, read\_forest, write\_galaxies, cleanup, resource mgmt). Implement handler registry and format detection. Standardize I/O error reporting. **Include unification of galaxy output preparation logic here**: create one generic output prep function called by format-specific writers.
*   **3.2 Format-Specific Implementations**: Refactor existing I/O into handlers implementing the `io_interface`. Add serialization for extended properties. Implement **cross-platform binary endianness handling**. Implement **robust HDF5 resource tracking/cleanup** to prevent leaks.
*   **3.3 Memory Optimization**: Implement configurable **buffered I/O**. Add **memory mapping** options. Develop **halo caching** strategy. Use **geometric growth** for dynamic array reallocation (replace fixed increments). Implement **memory pooling** for galaxy structs. **Remove `core_mymalloc.c/h`** and replace with standard allocators.
*   **3.4 Review Dynamic Limits**: Evaluate hardcoded limits like `ABSOLUTEMAXSNAPS` and implement runtime checks or dynamic resizing if necessary.

### Phase 4: Advanced Plugin Infrastructure

*   **4.1 Dynamic Library Loading**: Implement cross-platform loading (`dlopen`/`LoadLibrary`), symbol lookup (`dlsym`/`GetProcAddress`), and unloading with specific error handling.
*   **4.2 Module Development Framework**: Provide module template generation, a validation framework (interface compliance, memory checks), and debugging utilities (enable/disable debug logging, breakpoints, state printing).
*   **4.3 Parameter Tuning System**: Implement parameter registration/validation within modules, runtime modification capabilities, and parameter file import/export for modules. Add bounds checking.
*   **4.4 Module Discovery and Loading**: Implement directory scanning, manifest parsing (`module_manifest`), dependency resolution (checking against registered modules), and validation. Define API versioning strategy.
*   **4.5 Error Handling**: Implement comprehensive module error reporting (`module_error_t`), call stack tracing (`module_stack_trace_t`), and standardized logging per module.

### Phase 5: Core Module Migration

*   **5.1 Refactoring Main Evolution Loop**: Transform `evolve_galaxies()` to use the pipeline system. Integrate event dispatch/handling. Add support for reading/writing extension properties. Integrate module callbacks. Add evolution diagnostics. Remove legacy fallback paths in pipeline executor. **Remove `core_read_parameter_file.c/h`** completely.
*   **5.2 Converting Physics Modules**: Extract components into standalone modules implementing the defined interfaces. Add property registration and event triggers. Manage module-specific data. Define/implement dependencies via callbacks.
*   **5.3 Validation and Testing**: Perform **scientific validation** against baseline SAGE. Implement **performance benchmarks** (moved from Phase 1). Develop module **compatibility tests**. Add **call graph validation**.

### Phase 6: Advanced Performance Optimization

*   **6.1 Memory Layout Enhancement**: Implement **Structure-of-Arrays (SoA)** for hot-path galaxy data where beneficial. Further optimize memory pooling. Introduce size-segregated pools if needed.
*   **6.2 Tree Traversal Optimization**: Implement **prefetching** for depth-first traversal. Optimize pointer-chasing patterns. Consider non-recursive traversal algorithms.
*   **6.3 Vectorized Physics Calculations**: Implement **SIMD** for suitable calculations (focus on critical hotspots). Explore **batch processing** of galaxies. Implement architecture-specific dispatch.
*   **6.4 Parallelization Enhancements**: Refine **load balancing** (using complexity estimates). Explore **finer-grained parallelism** (OpenMP within forests/steps). Optimize **MPI communication**. Consider **hybrid MPI+OpenMP**.
*   **6.5 Module Callback Optimization**: Implement **callback caching**. Optimize paths for frequent interactions. Add **profiling** for inter-module calls. Consider targeted function inlining.

### Phase 7: Documentation and Tools

*   **7.1 Developer Documentation**: Create guides for module development, plugin interfaces, extensions, events, callbacks, and dependency management. Provide **clear guidance on when to use Events vs Callbacks**.
*   **7.2 Scientific Documentation**: Document physical models, create validation reports, scientific usage guides, and reproducibility guidelines.
*   **7.3 Tool Development**: Provide template modules, validation tools (module & scientific), dependency analyzer, call graph visualizer, and benchmarking/profiling tools.

## Deferred Work

*   **Performance Benchmarking**: Moved from Phase 1 to Phase 5/6. Profiling on the refactored structure is more valuable than embedding benchmarks during heavy changes.
*   **Enhanced End-to-End Test Diagnostics**: Skipped. Current logging + `sagediff.py` deemed sufficient.
*   **Dynamic Memory Defragmentation**: Deferred (post-project). Focus on preventative measures (pooling, allocation strategy) first.
*   **Full SIMD Vectorization**: Deferred (post-project). Implement for critical hotspots in Phase 6.3, full vectorization later if needed.

---