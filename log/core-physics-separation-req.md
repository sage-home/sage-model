# Core-Physics Separation Architecture: Requirements Document

## 1. Problem Statement

The SAGE codebase currently contains direct references to specific physics module implementations in core infrastructure components, violating the core-physics separation principle. Specifically, `core_pipeline_registry.c` directly references and imports physics module headers (`infall_module.h` and `cooling_module.h`), creating a tight coupling between core infrastructure and specific physics implementations.

This violates the key principle stated in the Core-Physics Property Separation Principles: "Core infrastructure should have no compile-time or direct runtime knowledge of specific physics implementations or the properties they manage."

## 2. Current Architecture Analysis

### 2.1 Role of Pipeline Registry

The `core_pipeline_registry.c` file serves as a bridge between the core pipeline infrastructure and physics modules by:

1. Registering module factories with the pipeline system
2. Providing a registration mechanism for standard modules via `pipeline_register_standard_modules()`
3. Creating pipelines with standard modules via `pipeline_create_with_standard_modules()`

### 2.2 Current Implementation Issues

```c
// In core_pipeline_registry.c
#include "../physics/infall_module.h"
#include "../physics/cooling_module.h"

void pipeline_register_standard_modules(void) {
    pipeline_register_module_factory(MODULE_TYPE_INFALL, "StandardInfall", infall_module_create);
    pipeline_register_module_factory(MODULE_TYPE_COOLING, "StandardCooling", cooling_module_create);
    // Add other modules as they are implemented
}
```

This implementation directly:
- Imports specific physics module headers into core code
- References specific module creator functions
- Hardcodes which modules are considered "standard"
- Creates a rigid, non-configurable initialization mechanism

### 2.3 Related System Components

The pipeline registry interacts with:

1. **Module Registry System**: Tracks loaded modules, handles, interfaces, and active status
2. **Pipeline System**: Manages execution order of physics operations
3. **Configuration System**: Reads module configuration from input files
4. **Module System**: Defines interfaces for physics module implementations

## 3. Requirements

### 3.1 Functional Requirements

1. **Configuration-Driven Module Registration**
   - FR-1.1: The system MUST use the JSON configuration to determine which modules to register
   - FR-1.2: No specific physics modules should be hardcoded in core code

2. **Pipeline Registry Independence**
   - FR-2.1: `core_pipeline_registry.c` MUST NOT include any physics module headers
   - FR-2.2: `core_pipeline_registry.c` MUST NOT directly reference any physics module implementation

3. **Automated Module Discovery**
   - FR-3.1: The system MUST locate modules specified in the configuration
   - FR-3.2: The system MUST be able to dynamically load specified modules 
   - FR-3.3: The system MUST handle errors gracefully if specified modules cannot be found

4. **Module Factory Registration**
   - FR-4.1: Modules MUST self-register their factory functions
   - FR-4.2: The pipeline registry MUST provide a mechanism for modules to register factory functions

### 3.2 Non-Functional Requirements

1. **Backwards Compatibility**
   - NFR-1.1: Existing configuration files MUST continue to work
   - NFR-1.2: The pipeline system MUST maintain its current interface

2. **Performance**
   - NFR-2.1: Module loading and pipeline creation MUST NOT introduce significant overhead
   - NFR-2.2: Runtime performance MUST be equivalent to the current implementation

3. **Maintainability**
   - NFR-3.1: Changes MUST enforce the core-physics separation principle
   - NFR-3.2: Implementation MUST follow established code style and documentation standards

## 4. Solution Design

### 4.1 Architecture Approach

The solution will implement a clear separation between the pipeline registry and physics modules using an indirect registration mechanism:

1. Physics modules will provide a standard `register_module_factory` function
2. The pipeline registry will use the configuration system to determine which modules to load
3. Modules will self-register during initialization with no direct import from core code

### 4.2 Component Changes

#### 4.2.1 Pipeline Registry Modifications

The `core_pipeline_registry.c` file will be modified to:
- Remove direct physics module includes
- Implement configuration-driven module discovery
- Provide a registration API for modules

```c
// Proposed core_pipeline_registry.c (simplified)
#include "core_pipeline_registry.h"
#include "core_galaxy_accessors.h"
#include "core_logging.h"
#include "core_config_system.h"
#include <string.h>

// Module factory registry
struct module_factory_entry {
    enum module_type type;
    char name[MAX_MODULE_NAME];
    module_factory_fn factory;
};

#define MAX_FACTORIES 64
static struct module_factory_entry module_factories[MAX_FACTORIES];
static int num_factories = 0;

// Public API for modules to register factories
int pipeline_register_module_factory(enum module_type type, const char *name, module_factory_fn factory) {
    if (num_factories >= MAX_FACTORIES) {
        LOG_ERROR("Too many module factories registered, maximum is %d", MAX_FACTORIES);
        return -1;
    }
    
    // Store factory information
    module_factories[num_factories].type = type;
    strncpy(module_factories[num_factories].name, name, MAX_MODULE_NAME - 1);
    module_factories[num_factories].name[MAX_MODULE_NAME - 1] = '\0';
    module_factories[num_factories].factory = factory;
    
    LOG_DEBUG("Registered module factory: type=%d, name=%s", type, name);
    return num_factories++;
}

// Discovers and registers modules based on configuration
void pipeline_register_standard_modules(void) {
    LOG_INFO("Registering modules from configuration");
    
    // Modules register themselves during initialization
    // This function remains for API compatibility
}

// Creates pipeline with modules from configuration
struct module_pipeline *pipeline_create_with_standard_modules(void) {
    // Create default pipeline
    struct module_pipeline *pipeline = pipeline_create_default();
    if (!pipeline) return NULL;

    // Load module configuration
    struct config_context *config = config_get_global_context();
    if (!config) {
        LOG_WARNING("No configuration context available, no modules will be loaded");
        return pipeline;
    }

    // Parse modules section of config
    struct config_section *modules_section = config_get_section(config, "modules");
    if (!modules_section) {
        LOG_WARNING("No 'modules' section in configuration, using auto-registered modules");
    } else {
        // Process module configuration
        // This would activate modules based on configuration
    }

    // Activate modules that have registered factories
    for (int i = 0; i < num_factories; i++) {
        struct base_module *module = module_factories[i].factory();
        if (module) {
            LOG_INFO("Activating module: %s", module_factories[i].name);
            module_register(module);
            module_set_active(module->module_id);
        }
    }
    
    return pipeline;
}
```

#### 4.2.2 Module Self-Registration

Physics modules will implement self-registration during initialization:

```c
// Example for placeholder_cooling_module.c
#include "../core/core_pipeline_registry.h"

// Module factory function
struct base_module *placeholder_cooling_module_create(void) {
    // Implementation...
}

// Auto-registration at startup
static void __attribute__((constructor)) register_module_factory(void) {
    pipeline_register_module_factory(
        MODULE_TYPE_COOLING, 
        "PlaceholderCooling", 
        placeholder_cooling_module_create
    );
}
```

#### 4.2.3 Configuration System Integration

The JSON configuration format will be used to determine which modules to activate:

```json
{
  "modules": {
    "cooling": {
      "module": "PlaceholderCooling",
      "enabled": true,
      "parameters": {
        // Module-specific parameters
      }
    },
    "infall": {
      "module": "PlaceholderInfall",
      "enabled": true,
      "parameters": {
        // Module-specific parameters
      }
    }
  }
}
```

### 4.3 Migration Strategy

The implementation will follow these steps:

1. Update `core_pipeline_registry.c` to remove direct physics module references
2. Modify physics modules to self-register during initialization
3. Update the configuration system to properly specify module activation
4. Update build system to reflect changes
5. Add tests to verify proper module discovery and registration

## 5. Implementation Details

### 5.1 Files to Modify

1. **Core Files**:
   - `src/core/core_pipeline_registry.c` - Remove direct physics includes
   - `src/core/core_pipeline_registry.h` - Update API documentation

2. **Physics Module Files**:
   - `src/physics/placeholder_cooling_module.c` - Add self-registration
   - `src/physics/placeholder_infall_module.c` - Add self-registration
   - (Additional modules as needed)

3. **Configuration Files**:
   - `input/empty_pipeline_config.json` - Update module configuration
   - Other config files as needed

### 5.2 Key APIs

#### 5.2.1 Module Registration API

```c
/**
 * @brief Register a module factory function
 *
 * @param type The module type
 * @param name The module name (must be unique)
 * @param factory Function that creates a module instance
 * @return Index of registered factory or -1 on error
 */
int pipeline_register_module_factory(enum module_type type, const char *name, module_factory_fn factory);
```

#### 5.2.2 Pipeline Creation API

```c
/**
 * @brief Create a pipeline with standard modules
 *
 * Creates a pipeline and activates modules based on configuration.
 * Modules must be registered via pipeline_register_module_factory().
 *
 * @return Initialized pipeline or NULL on error
 */
struct module_pipeline *pipeline_create_with_standard_modules(void);
```

### 5.3 Testing Approach

The implementation will include the following tests:

1. **Unit Tests**:
   - Verify `pipeline_register_module_factory` correctly stores factory information
   - Verify `pipeline_create_with_standard_modules` correctly creates pipeline
   - Test error handling for invalid configurations

2. **Integration Tests**:
   - Verify modules can be activated based on configuration
   - Verify pipeline executes modules in correct order
   - Test with multiple module configurations

3. **Validation Tests**:
   - Verify existing SAGE functionality continues to work correctly
   - Compare results with previous implementation

## 6. Risks and Mitigations

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Breaking existing functionality | High | Medium | Comprehensive testing and validation against known benchmarks |
| Module discovery failures | Medium | Low | Robust error handling and detailed logging |
| Performance regression | Medium | Low | Ensure minimal overhead in module discovery code |
| Configuration incompatibility | Medium | Medium | Provide backwards compatibility and migration documentation |

## 7. Implementation Plan

### 7.1 Phase 1: Core Infrastructure Updates

1. Update `core_pipeline_registry.c` to remove physics module imports
2. Implement module factory registry
3. Update pipeline creation to use registered factories

### 7.2 Phase 2: Module Updates

1. Add self-registration to placeholder modules
2. Verify modules register correctly
3. Update module creation functions if needed

### 7.3 Phase 3: Configuration Integration

1. Update configuration handling
2. Add module discovery based on configuration
3. Implement module activation logic

### 7.4 Phase 4: Testing and Validation

1. Add unit tests for new functionality
2. Verify pipeline correctness with integration tests
3. Validate against existing benchmarks

## 8. Conclusion

This requirements document outlines a comprehensive approach to properly separate core infrastructure from physics implementations, specifically addressing the inappropriate coupling in `core_pipeline_registry.c`. The proposed solution aligns with the larger SAGE refactoring plan and the core-physics separation principles by:

1. Removing direct dependencies on physics modules from core code
2. Implementing a self-registration mechanism for modules
3. Using configuration to determine which modules to activate
4. Maintaining backwards compatibility with existing code

These changes will strengthen the modularity of SAGE, allowing for more flexible physics implementations while maintaining the stability of core infrastructure.
