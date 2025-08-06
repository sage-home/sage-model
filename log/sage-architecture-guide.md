# SAGE Architecture Guide

**Purpose**: Complete implementation specification for building modern modular SAGE  
**Audience**: Development team and AI developers  
**Scope**: Target architecture, proven patterns, critical improvements, and environment setup  

---

## Implementation Philosophy

### Core Principles
- **Metadata-Driven Development**: Single source of truth prevents synchronization bugs
- **Physics-Agnostic Core**: Zero physics knowledge in core infrastructure
- **Type Safety First**: Catch errors at compile-time, not runtime
- **Standard Tools**: Leverage industry-standard tooling rather than custom solutions

### Success Criteria
- Physics modules configurable at runtime without recompilation
- Identical scientific results to legacy implementation
- Modern development workflow with IDE integration and debugging tools
- Bounded memory usage scalable to large simulations

---

## Project Structure & Organization

### Directory Layout
```
sage-model/
├── src/                        # All source code
│   ├── core/                   # Physics-agnostic infrastructure only
│   ├── physics/                # Physics modules and events
│   ├── io/                     # I/O handlers and unified interface
│   ├── properties.yaml         # Central property definitions
│   ├── parameters.yaml         # Parameter metadata
│   └── generate_property_headers.py  # Code generation script
├── docs/                       # User-facing documentation
│   ├── README.md               # Role-based navigation hub
│   ├── templates/              # Code templates for developers
│   └── [focused guides]        # Architecture, property system, I/O, etc.
├── tests/                      # Comprehensive test suite
│   ├── test_*.c                # Categorized unit tests
│   ├── test_sage.sh            # End-to-end scientific validation
│   └── sagediff.py             # Scientific result comparison
├── build/                      # Out-of-tree build directory
├── input/                      # Configuration and parameter files
├── log/                        # Development process tracking (AI support)
│   ├── README.md               # Logging system guide
│   ├── [current session files] # Active development context
│   └── archive/                # Historical reference
└── CMakeLists.txt              # Modern build system
```

### Key Organizational Principles
- **Separation of Concerns**: Each directory has single, clear responsibility
- **AI Development Support**: Dedicated `log/` directory for persistent context
- **Professional Standards**: Documentation and testing at same level as source
- **Modern Workflow**: Out-of-tree builds, IDE integration, package management ready

---

## Core System Implementations

### 1. Build System Architecture

**Technology**: CMake (replacing Makefile)

**Why CMake**:
- Industry standard for scientific C/C++ projects
- Out-of-tree builds by design prevent source directory pollution
- Excellent IDE integration (CLion, VS Code, Visual Studio)  
- Cross-platform without manual configuration
- Package management integration (vcpkg, Conan)
- Modern tooling integration (sanitizers, static analysis)

**Implementation Pattern**:
```cmake
# Property-driven build configuration
option(SAGE_PHYSICS_FREE "Build core-only mode" OFF)
option(SAGE_FULL_PHYSICS "Build with all physics" ON)
option(SAGE_CUSTOM_PHYSICS "Build with specified modules" OFF)

# Component-based architecture
add_library(sage_core STATIC ${CORE_SOURCES})
add_library(sage_physics STATIC ${PHYSICS_SOURCES})
add_library(sage_io STATIC ${IO_SOURCES})

# Automatic property generation integrated into build
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/generated/core_properties.h
           ${CMAKE_BINARY_DIR}/generated/core_properties.c
    COMMAND ${Python3_EXECUTABLE} generate_property_headers.py 
            --config ${PROPERTIES_CONFIG}
            --output-dir ${CMAKE_BINARY_DIR}/generated
    DEPENDS properties.yaml parameters.yaml generate_property_headers.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
)

# Build targets for different configurations
if(SAGE_PHYSICS_FREE)
    set(PROPERTIES_CONFIG "core_only")
    target_compile_definitions(sage_core PRIVATE PHYSICS_FREE_MODE)
elseif(SAGE_CUSTOM_PHYSICS)
    set(PROPERTIES_CONFIG ${SAGE_PHYSICS_CONFIG_FILE})
    target_link_libraries(sage_core sage_physics)
else()  # SAGE_FULL_PHYSICS
    set(PROPERTIES_CONFIG "full_physics")
    target_link_libraries(sage_core sage_physics)
endif()
```

**Build Commands**:
```bash
# Out-of-tree build workflow
mkdir build && cd build

# Different physics configurations
cmake .. -DSAGE_PHYSICS_FREE=ON                    # Core only
cmake .. -DSAGE_FULL_PHYSICS=ON                    # All physics
cmake .. -DSAGE_CUSTOM_PHYSICS=ON -DSAGE_PHYSICS_CONFIG_FILE=config.json

make -j$(nproc)  # Parallel build
```

### 2. Property System Implementation

**Pattern**: YAML metadata → Python generation → Type-safe C code

**Property Definition Format**:
```yaml
# properties.yaml - Single source of truth
properties:
  # Core properties - always available, direct access
  - name: "Mvir"
    type: "float"
    initial_value: 0.0
    units: "10^10 Msun/h"
    description: "Halo virial mass"
    is_core: true
    output: true
    
  # Physics properties - module-dependent, checked access  
  - name: "ColdGas"
    type: "float"
    initial_value: 0.0
    units: "10^10 Msun/h"
    description: "Cold gas mass"
    is_core: false
    output: true
    
  # Dynamic arrays based on simulation parameters
  - name: "SfrDisk"
    type: "float"
    is_array: true
    array_len: 0  # Dynamic
    size_parameter: "simulation.NumSnapOutputs"
    description: "Star formation rate history in disk"
    is_core: false
    output: true
```

**Generated Code Pattern** (type-safe, compile-time validated):
```c
// Auto-generated core_properties.h
typedef enum {
    PROPERTY_MVIR,
    PROPERTY_COLDGAS,
    PROPERTY_SFRDISK,
    PROPERTY_COUNT
} property_id_t;

// Core properties - direct access, always available
#define GALAXY_GET_MVIR(g)      ((g)->properties.Mvir)
#define GALAXY_SET_MVIR(g, val) do { \
    (g)->properties.Mvir = (val); \
    (g)->modified_mask |= PROPERTY_MASK_MVIR; \
} while(0)

// Physics properties - compile-time safety through build configuration
#if defined(SAGE_MODULE_COOLING) // Set by CMake based on build config
#define GALAXY_GET_COLDGAS(g)      ((g)->properties.ColdGas)
#define GALAXY_SET_COLDGAS(g, val) do { \
    (g)->properties.ColdGas = (val); \
    (g)->modified_mask |= PROPERTY_MASK_COLDGAS; \
} while(0)
#endif

// Runtime availability checking for optional access patterns
static inline float galaxy_get_coldgas_safe(const struct GALAXY *g, float default_val) {
    if (!(g->available_properties & PROPERTY_MASK_COLDGAS)) {
        return default_val;
    }
    return g->properties.ColdGas;
}

// Property availability checking
static inline bool galaxy_has_property(const struct GALAXY *g, property_id_t prop_id) {
    return (g->available_properties & (1ULL << prop_id)) != 0;
}

// Compile-time property iteration
#define FOR_EACH_CORE_PROPERTY(macro) \
    macro(MVIR, Mvir, float, "Halo virial mass")

#define FOR_EACH_PHYSICS_PROPERTY(macro) \
    macro(COLDGAS, ColdGas, float, "Cold gas mass") \
    macro(SFRDISK, SfrDisk, float*, "Star formation rate history")
```

**Benefits**:
- **Compile-time safety**: Accessing unavailable physics properties fails at compile time, not runtime
- **Typos become compile-time errors**: No more `get_property_id("ColGas")` runtime failures  
- **IDE integration**: Autocomplete and go-to-definition work perfectly
- **Zero runtime overhead**: No string processing or runtime property lookups
- **Refactoring support**: Tools can find all property references across codebase
- **Scientific accuracy**: Missing physics modules detected at build time, preventing silent modeling errors

### 3. Memory Management Architecture

**Pattern**: Standard allocators + RAII-style cleanup (replacing custom allocators)

**Why Abstracted Standard Allocators**:
- **Immediate benefits**: All debugging tools work (AddressSanitizer, Valgrind, etc.)
- **Reliability**: No custom allocator bugs to debug, battle-tested standard library
- **RAII-style cleanup**: Automatic resource management in scoped contexts
- **Future flexibility**: Abstraction allows later optimization (memory pools, arenas) without codebase-wide refactoring
- **Zero current cost**: Macros compile to direct standard allocator calls

**Implementation**:
```c
// Abstraction layer for future flexibility
#define sage_malloc(size) malloc(size)
#define sage_free(ptr) free(ptr)

// RAII-style cleanup system using abstracted allocators
typedef struct cleanup_handler {
    void (*cleanup_func)(void *data);
    void *data;
    struct cleanup_handler *next;
} cleanup_handler_t;

// Scope-based memory management
typedef struct memory_scope {
    cleanup_handler_t *cleanup_stack;
    struct memory_scope *parent;
} memory_scope_t;

static memory_scope_t *current_scope = NULL;

void memory_scope_enter(memory_scope_t *scope) {
    scope->cleanup_stack = NULL;
    scope->parent = current_scope;
    current_scope = scope;
}

void memory_scope_register_cleanup(void (*cleanup_func)(void *), void *data) {
    cleanup_handler_t *handler = malloc(sizeof(cleanup_handler_t));
    handler->cleanup_func = cleanup_func;
    handler->data = data;
    handler->next = current_scope->cleanup_stack;
    current_scope->cleanup_stack = handler;
}

void memory_scope_exit(void) {
    while (current_scope->cleanup_stack) {
        cleanup_handler_t *handler = current_scope->cleanup_stack;
        current_scope->cleanup_stack = handler->next;
        
        handler->cleanup_func(handler->data);
        free(handler);
    }
    current_scope = current_scope->parent;
}

// Usage - same scoped benefits, abstracted allocators
void process_forest(int forest_id) {
    memory_scope_t scope;
    memory_scope_enter(&scope);
    
    // Abstracted allocations with automatic cleanup
    struct GALAXY *galaxies = sage_malloc(1000 * sizeof(struct GALAXY));
    memory_scope_register_cleanup(sage_free, galaxies);
    
    struct halo_data *halos = sage_malloc(500 * sizeof(struct halo_data));
    memory_scope_register_cleanup(sage_free, halos);
    
    // ... process forest ...
    
    memory_scope_exit();  // All memory automatically freed
}
```

### 4. Configuration System Architecture

**Pattern**: Unified JSON configuration with schema validation

**Unified Configuration Format**:
```json
{
    "$schema": "https://sage-model.org/config-schema.json",
    "simulation": {
        "boxSize": 100.0,
        "particleMass": 8.6e8,
        "firstFile": 0,
        "lastFile": 7,
        "outputDir": "./output/",
        "numSnapOutputs": 64
    },
    "modules": {
        "cooling": {
            "enabled": true,
            "efficiency": 1.5,
            "parameters": {
                "coolTimeConstant": 0.1
            }
        },
        "starformation": {
            "enabled": true,
            "threshold": 0.1
        }
    },
    "build": {
        "propertySet": "full_physics"  // or "core_only", "custom"
    },
    "legacy": {
        "parameterFile": "millennium.par"  // Migration support
    }
}
```

**Schema Validation**:
```json
{
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "required": ["simulation", "modules"],
    "properties": {
        "simulation": {
            "type": "object",
            "required": ["boxSize", "particleMass"],
            "properties": {
                "boxSize": {"type": "number", "minimum": 0},
                "particleMass": {"type": "number", "minimum": 0}
            }
        },
        "modules": {
            "type": "object",
            "additionalProperties": {
                "type": "object",
                "properties": {
                    "enabled": {"type": "boolean"},
                    "parameters": {"type": "object"}
                }
            }
        }
    }
}
```

### 5. Module System Implementation

**Pattern**: Self-registering modules with explicit dependencies

**Module Definition**:
```c
// Module implementation
typedef struct {
    const char *name;
    const char *version;
    const char **dependencies;  // List of required modules
    uint32_t supported_phases;  // Bitmap of pipeline phases
    
    int (*initialize)(const config_t *config);
    void (*shutdown)(void);
    int (*execute_halo_phase)(struct pipeline_context *ctx);
    int (*execute_galaxy_phase)(struct pipeline_context *ctx);
    int (*execute_post_phase)(struct pipeline_context *ctx);
    int (*execute_final_phase)(struct pipeline_context *ctx);
} module_info_t;

// Self-registration using constructor attributes
static void __attribute__((constructor)) register_cooling_module(void) {
    static const char *dependencies[] = {"core", NULL};
    static module_info_t cooling_module = {
        .name = "cooling",
        .version = "1.0.0",
        .dependencies = dependencies,
        .supported_phases = PIPELINE_PHASE_GALAXY,
        .initialize = cooling_initialize,
        .shutdown = cooling_shutdown,
        .execute_galaxy_phase = cooling_execute_galaxy
    };
    module_register(&cooling_module);
}
```

**Pipeline Execution**:
```c
// Configuration-driven pipeline creation
struct pipeline *create_pipeline_from_config(const config_t *config) {
    struct pipeline *pipeline = pipeline_create();
    
    // Add modules based on configuration
    json_t *modules = json_object_get(config->json, "modules");
    const char *module_name;
    json_t *module_config;
    
    json_object_foreach(modules, module_name, module_config) {
        if (json_boolean_value(json_object_get(module_config, "enabled"))) {
            module_info_t *module = module_registry_find(module_name);
            if (module) {
                pipeline_add_module(pipeline, module, module_config);
            }
        }
    }
    
    // Resolve dependencies and sort by dependency order
    pipeline_resolve_dependencies(pipeline);
    return pipeline;
}
```

### 6. I/O System Architecture

**Pattern**: Unified interface with property-based serialization

**I/O Interface**:
```c
// Unified I/O interface for all formats
typedef struct io_interface {
    const char *name;
    const char *version;
    uint32_t capabilities;
    
    // Core operations
    int (*initialize)(const char *filename, const config_t *config, void **format_data);
    int64_t (*read_forest)(int64_t forestnr, struct halo_data **halos, 
                          struct forest_info *info, void *format_data);
    int (*write_galaxies)(const struct GALAXY *galaxies, int ngals,
                         const struct save_info *info, void *format_data);
    int (*cleanup)(void *format_data);
    
    // Resource management
    int (*get_open_handle_count)(void *format_data);
    int (*close_open_handles)(void *format_data);
} io_interface_t;

// Property-based HDF5 output
int hdf5_write_galaxies(const struct GALAXY *galaxies, int ngals,
                       const struct save_info *info, void *format_data) {
    // Discover available properties at runtime
    property_id_t *output_properties = get_output_property_list(&num_props);
    
    for (int i = 0; i < num_props; i++) {
        property_info_t *prop_info = get_property_info(output_properties[i]);
        
        // Create HDF5 dataset for this property
        hid_t dataset = create_hdf5_dataset(info->group_id, prop_info);
        
        // Write property data with optional transformation
        write_property_data_to_hdf5(dataset, galaxies, ngals, output_properties[i]);
        
        H5Dclose(dataset);
    }
    
    return 0;
}
```

### 7. Testing Framework Architecture

**Pattern**: Categorized test suite with scientific validation

**Test Organization**:
```cmake
# CMake test organization
enable_testing()

# Test categories
set(CORE_TESTS test_pipeline test_memory_scope test_property_system)
set(IO_TESTS test_io_interface test_hdf5_output test_endian_utils)
set(MODULE_TESTS test_module_registry test_module_lifecycle)

# Individual tests
foreach(test ${CORE_TESTS})
    add_executable(${test} tests/${test}.c)
    target_link_libraries(${test} sage_core)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# Test categories
add_custom_target(core_tests DEPENDS ${CORE_TESTS})
add_custom_target(io_tests DEPENDS ${IO_TESTS})
add_custom_target(module_tests DEPENDS ${MODULE_TESTS})
```

**Scientific Validation**:
```bash
# End-to-end scientific validation
./tests/test_sage.sh --config millennium_validation.json
python tests/sagediff.py reference_output.h5 test_output.h5 --tolerance 1e-6
```

---

## AI Development Support System

### CLAUDE.md Template
```markdown
# CLAUDE.md - AI Development Context

## SAGE Model - Semi-Analytic Galaxy Evolution
Modern modular galaxy evolution framework with physics-agnostic core.

## Build System
Primary commands:
- `mkdir build && cd build && cmake .. && make` - Out-of-tree build
- `cmake .. -DSAGE_PHYSICS_FREE=ON` - Core-only build  
- `cmake .. -DSAGE_FULL_PHYSICS=ON` - Full physics build
- `make test` - Run all tests

## Architecture Principles
- Physics-agnostic core infrastructure
- Metadata-driven property/parameter systems
- Type-safe compile-time property access
- Standard allocators with RAII-style cleanup
- Unified configuration with schema validation

## Development Guidelines
- All galaxy data access through property system
- Use memory scopes for automatic cleanup
- Self-registering modules with explicit dependencies
- Comprehensive testing for all components

# User Instructions to always follow
- Use CMake, not Makefiles
- Use standard allocators, not custom allocators
- Use type-safe property access, not string lookups
- Update both YAML files and generated code documentation
- Run full test suite before committing changes
- Follow memory scope patterns for resource management
```

### Logging System Structure
```
log/
├── README.md              # Logging system guide
├── Current Session Files  # Active development context
│   ├── decisions.md       # Architectural decisions with rationale
│   ├── phase.md          # Current development phase and objectives
│   ├── architecture.md   # Current system state snapshot
│   └── progress.md       # Completed milestones and changes
└── archive/              # Historical reference
    ├── decisions-phase*.md
    └── progress-phase*.md
```

---

## Development Environment Setup

### Required Tools
```bash
# Essential build tools
sudo apt-get install cmake build-essential python3 python3-yaml

# HDF5 development libraries
sudo apt-get install libhdf5-dev

# Optional but recommended
sudo apt-get install clang-tidy valgrind gdb

# Python dependencies for code generation
pip3 install pyyaml jsonschema
```

### IDE Configuration
**VS Code** (recommended):
```json
// .vscode/settings.json
{
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureArgs": ["-DSAGE_FULL_PHYSICS=ON"],
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools"
}
```

**CLion**: Open CMakeLists.txt directly, configure build profiles for different physics modes.

### Development Workflow
```bash
# Initial setup
git clone <sage-repo>
cd sage-model

# Create AI development context
mkdir -p log/archive
cp templates/CLAUDE.md ./CLAUDE.md
cp templates/log-README.md ./log/README.md

# Configure build
mkdir build && cd build
cmake .. -DSAGE_FULL_PHYSICS=ON

# Development cycle
make -j$(nproc)          # Build
make test                # Test
../tests/test_sage.sh    # Scientific validation

# Before commits
make test                # All tests must pass
python ../tests/validate_generated_code.py  # Check generated code
```

---

## Potential Implementation Issues & Mitigations

### 1. CMake Learning Curve
**Issue**: Team unfamiliar with CMake  
**Mitigation**: Provide CMake templates and clear examples, CMake is industry standard

### 2. Property System Migration  
**Issue**: Converting string-based to type-safe access  
**Mitigation**: Migration can be gradual, both systems can coexist temporarily

### 3. Memory Management Changes
**Issue**: RAII pattern unfamiliar in C  
**Mitigation**: Provide clear examples and templates, pattern is straightforward

### 4. Configuration Migration
**Issue**: Users have existing .par files  
**Mitigation**: Built-in legacy support and migration tools

### 5. Testing Framework Integration
**Issue**: Integration with CI/CD systems  
**Mitigation**: CMake + CTest is standard, works with all CI systems

---

## Success Metrics

### Technical Metrics
- **Build Time**: < 2 minutes for full build
- **Test Time**: < 30 seconds for unit tests, < 5 minutes for full validation
- **Memory Usage**: Bounded by max galaxies per snapshot, not total simulation size
- **IDE Integration**: Full autocomplete, debugging, and refactoring support

### Scientific Metrics  
- **Accuracy**: Identical results to legacy implementation (within numerical precision)
- **Performance**: Within 10% of legacy runtime performance
- **Scalability**: Handle simulations with >10^8 halos without memory issues

### Developer Experience Metrics
- **Onboarding**: New developers productive within 1 day
- **Debugging**: All standard debugging tools work out of the box  
- **Maintenance**: No specialized knowledge required for common tasks

---

## Conclusion

This implementation guide provides a complete specification for building modern, maintainable SAGE while preserving scientific accuracy. The architecture leverages industry-standard tools and patterns to create a system that is:

- **Type-safe**: Catch errors at compile-time
- **Maintainable**: Standard tools and patterns throughout
- **Scalable**: Modern build system and memory management
- **Developer-friendly**: Excellent IDE integration and debugging support
- **Scientifically accurate**: Preserves all legacy scientific behavior

The focus on proven industry patterns ensures the system will be maintainable and extensible for years to come while providing immediate benefits in developer productivity and system reliability.