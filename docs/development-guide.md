# SAGE Development Guide

## Overview

This guide provides comprehensive information for developers working on the SAGE codebase, covering coding standards, development workflows, debugging practices, and contribution guidelines. It assumes familiarity with the SAGE architecture and focuses on practical development tasks.

## Development Environment Setup

### Prerequisites

**Required Tools:**
- GCC or Clang compiler with C99 support
- HDF5 library (1.8.13 or later)
- MPI library (OpenMPI or MPICH) for parallel execution
- Python 3.6+ for code generation and testing
- Git for version control

**Optional Tools:**
- Valgrind for memory debugging
- GDB or LLDB for debugging
- CMake for alternative build configuration
- Doxygen for documentation generation

### Initial Setup

```bash
# Clone repository
git clone <repository-url>
cd sage-model

# Initialize development environment
./first_run.sh                    # Downloads test data and sets up directories

# Verify setup
make clean
make full-physics
./sage input/millennium.par
```

### Development Tools Configuration

**GDB Configuration (`.gdbinit`):**
```gdb
set print pretty on
set print elements 0
set print null-stop on
set breakpoint pending on
```

**Valgrind Configuration:**
```bash
# Memory leak detection
valgrind --leak-check=full --show-leak-kinds=all ./sage input/millennium.par

# Memory error detection
valgrind --tool=memcheck --track-origins=yes ./sage input/millennium.par
```

## Coding Standards

### General Principles

1. **Clarity over Cleverness**: Write code that is easy to understand and maintain
2. **Consistent Style**: Follow established patterns throughout the codebase
3. **Documentation**: Document complex algorithms and architectural decisions
4. **Error Handling**: Always check return values and handle errors gracefully
5. **Memory Management**: Use SAGE's memory management functions consistently

### Code Style

#### Naming Conventions

```c
// Functions: lowercase with underscores
int calculate_cooling_rate(double temperature, double density);

// Variables: lowercase with underscores
double stellar_mass;
int galaxy_count;

// Constants: uppercase with underscores
#define MAX_GALAXY_PROPERTIES 128
#define SOLAR_MASS 1.989e30

// Structures: CamelCase
struct GALAXY;
struct ModuleData;

// Enums: snake_case with descriptive prefix
enum module_type {
    MODULE_TYPE_COOLING,
    MODULE_TYPE_STAR_FORMATION
};
```

#### Function Structure

```c
/**
 * @brief Calculate the cooling rate for a galaxy
 * @param galaxy Pointer to galaxy structure
 * @param temperature Gas temperature in Kelvin
 * @param density Gas density in g/cm^3
 * @return Cooling rate in erg/s, or -1.0 on error
 */
double calculate_cooling_rate(const struct GALAXY *galaxy, double temperature, double density) {
    // Input validation
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer cannot be NULL");
        return -1.0;
    }
    
    if (temperature <= 0.0 || density <= 0.0) {
        LOG_ERROR("Invalid physical parameters: T=%.2e, rho=%.2e", temperature, density);
        return -1.0;
    }
    
    // Implementation
    double cooling_function = compute_cooling_function(temperature);
    double cooling_rate = cooling_function * density * density;
    
    return cooling_rate;
}
```

#### Header File Organization

```c
#ifndef COMPONENT_NAME_H
#define COMPONENT_NAME_H

#include <stdio.h>
#include <stdlib.h>
#include "core/core_allvars.h"

// Forward declarations
struct ComponentData;

// Constants
#define COMPONENT_MAX_SIZE 1024

// Type definitions
typedef struct component_config {
    double parameter1;
    int parameter2;
    bool enabled;
} component_config_t;

// Function declarations
int component_initialize(const char *config_file);
int component_process(struct GALAXY *galaxy, component_config_t *config);
void component_cleanup(void);

#endif /* COMPONENT_NAME_H */
```

### Memory Management

#### Using SAGE Memory Functions

```c
// Use SAGE's memory management functions
#include "core/core_mymalloc.h"

// Allocation
struct GALAXY *galaxies = mymalloc("galaxies", sizeof(struct GALAXY) * ngal);

// Reallocation
galaxies = myrealloc(galaxies, sizeof(struct GALAXY) * new_ngal);

// Deallocation
myfree(galaxies);
```

#### Memory Pool Usage

```c
// For temporary allocations, use memory pools
#include "core/core_memory_pool.h"

memory_pool_t *pool = memory_pool_create(1024 * 1024);  // 1MB pool

void *temp_buffer = memory_pool_alloc(pool, buffer_size);
// Use temp_buffer...

memory_pool_destroy(pool);  // Frees all allocations at once
```

### Error Handling

#### Return Code Conventions

```c
// Standard return codes
#define SUCCESS 0
#define ERROR_INVALID_INPUT -1
#define ERROR_MEMORY_ALLOCATION -2
#define ERROR_FILE_NOT_FOUND -3
#define ERROR_CALCULATION_FAILED -4

// Module-specific return codes
typedef enum {
    MODULE_STATUS_SUCCESS = 0,
    MODULE_STATUS_ERROR = -1,
    MODULE_STATUS_INVALID_ARGS = -2,
    MODULE_STATUS_OUT_OF_MEMORY = -3,
    MODULE_STATUS_ALREADY_INITIALIZED = -4
} module_status_t;
```

#### Error Handling Patterns

```c
int function_with_error_handling(struct GALAXY *galaxy) {
    // Input validation
    if (galaxy == NULL) {
        LOG_ERROR("Invalid galaxy pointer");
        return ERROR_INVALID_INPUT;
    }
    
    // Resource allocation with error checking
    double *buffer = mymalloc("temp_buffer", sizeof(double) * size);
    if (buffer == NULL) {
        LOG_ERROR("Failed to allocate temporary buffer");
        return ERROR_MEMORY_ALLOCATION;
    }
    
    // Operation with error checking
    int status = perform_calculation(galaxy, buffer);
    if (status != SUCCESS) {
        LOG_ERROR("Calculation failed with status %d", status);
        myfree(buffer);
        return status;
    }
    
    // Cleanup
    myfree(buffer);
    return SUCCESS;
}
```

### Logging Guidelines

#### Log Levels

```c
// Use appropriate log levels
LOG_DEBUG("Detailed debugging information: galaxy %lld", galaxy_id);
LOG_INFO("General information: processing %d galaxies", ngal);
LOG_WARNING("Potential issue: mass below threshold (%.2e)", mass);
LOG_ERROR("Error occurred: failed to read file %s", filename);
```

#### Structured Logging

```c
// Include relevant context in log messages
LOG_INFO("Galaxy processing: snapshot=%d, tree=%lld, galaxy=%lld, mass=%.2e", 
         snapshot, tree_id, galaxy_id, stellar_mass);

// Performance logging
double start_time = get_timestamp();
process_galaxies();
double end_time = get_timestamp();
LOG_INFO("Galaxy processing completed: %.2f seconds, %d galaxies", 
         end_time - start_time, ngal);
```

## Development Workflows

### Feature Development

#### 1. Planning Phase

```bash
# Create feature branch
git checkout -b feature/new-cooling-model

# Document the feature
# - Update relevant documentation
# - Create design notes if complex
# - Plan testing strategy
```

#### 2. Implementation Phase

```bash
# Implement in small, testable increments
# - Core functionality first
# - Add tests as you go
# - Maintain backward compatibility

# Regular testing during development
make clean
make full-physics
make tests
```

#### 3. Integration Phase

```bash
# Test with various configurations
make clean && make physics-free && make tests
make clean && make full-physics && make tests
make clean && make custom-physics CONFIG=minimal.json && make tests

# Run end-to-end validation
./test_sage.sh
```

### Bug Fix Workflow

#### 1. Reproduction

```bash
# Create minimal test case
# - Isolate the problem
# - Create reproducible test
# - Document expected vs actual behavior
```

#### 2. Debugging

```bash
# Use appropriate debugging tools
gdb --args ./sage input/millennium.par
(gdb) break suspect_function
(gdb) run

# Memory debugging
valgrind --tool=memcheck ./sage input/millennium.par

# Property access debugging
make clean && make full-physics  # Ensure all properties available
```

#### 3. Fix and Validation

```bash
# Implement fix
# Add regression test
# Verify fix doesn't break other functionality

make tests
./test_sage.sh
```

### Module Development

#### Creating a New Physics Module

**1. Module Structure:**
```c
// src/physics/my_new_module.h
#ifndef MY_NEW_MODULE_H
#define MY_NEW_MODULE_H

#include "core/core_allvars.h"
#include "core/core_module_system.h"

// Module data structure
struct my_module_data {
    double parameter1;
    int parameter2;
    bool initialized;
};

// Module interface
extern struct base_module my_new_module;

// Factory function
struct base_module* my_new_module_factory(void);

#endif
```

**2. Module Implementation:**
```c
// src/physics/my_new_module.c
#include "my_new_module.h"

// Module lifecycle functions
static int my_module_init(struct params *params, void **data_ptr);
static int my_module_cleanup(void *data);
static int my_module_execute_galaxy(void *data, struct pipeline_context *context);

// Module definition
struct base_module my_new_module = {
    .name = "my_new_module",
    .version = "1.0",
    .author = "Developer Name",
    .type = MODULE_TYPE_MISC,
    .initialize = my_module_init,
    .cleanup = my_module_cleanup,
    .execute_galaxy_phase = my_module_execute_galaxy,
    .phases = PIPELINE_PHASE_GALAXY
};

// Factory function
struct base_module* my_new_module_factory(void) {
    return &my_new_module;
}

// Registration
static void __attribute__((constructor)) register_my_module(void) {
    module_register(&my_new_module);
    pipeline_register_module_factory(MODULE_TYPE_MISC, "my_new_module", my_new_module_factory);
}
```

**3. Module Testing:**
```c
// tests/test_my_new_module.c
#include "physics/my_new_module.h"

static void test_module_initialization(void) {
    struct params test_params = {0};
    void *module_data = NULL;
    
    int status = my_new_module.initialize(&test_params, &module_data);
    assert(status == MODULE_STATUS_SUCCESS);
    assert(module_data != NULL);
    
    my_new_module.cleanup(module_data);
}
```

### Property System Development

#### Adding New Properties

**1. Update properties.yaml:**
```yaml
- name: "NewProperty"
  type: "float"
  units: "Msun"
  description: "Description of new property"
  is_core: false
  is_array: false
  output_field: true
```

**2. Regenerate property files:**
```bash
make clean  # Forces regeneration of property files
make full-physics
```

**3. Use new property:**
```c
// Access new property
property_id_t prop_id = get_property_id("NewProperty");
if (has_property(galaxy, prop_id)) {
    float value = get_float_property(galaxy, prop_id, 0.0f);
    set_float_property(galaxy, prop_id, new_value);
}
```

#### Adding Output Transformers

**1. Define transformer function:**
```c
// src/io/output_transformers.c
double transform_new_property(const struct GALAXY *galaxy, property_id_t prop_id) {
    double internal_value = get_double_property(galaxy, prop_id, 0.0);
    return internal_value * conversion_factor;
}
```

**2. Update properties.yaml:**
```yaml
- name: "NewProperty"
  output_transformer_function: "transform_new_property"
```

## Debugging Practices

### Common Debugging Scenarios

#### Memory Issues

```bash
# Memory leaks
valgrind --leak-check=full --show-leak-kinds=all ./sage input/millennium.par

# Uninitialized memory
valgrind --tool=memcheck --track-origins=yes ./sage input/millennium.par

# Buffer overflows
valgrind --tool=memcheck --track-origins=yes --run-libc-freeres=no ./sage input/millennium.par
```

#### Property Access Issues

```c
// Debug property access
property_id_t prop_id = get_property_id("PropertyName");
if (prop_id == PROP_COUNT) {
    LOG_ERROR("Property 'PropertyName' not found");
    // Check if property is defined in current build configuration
}

if (!has_property(galaxy, prop_id)) {
    LOG_ERROR("Property not available for galaxy %lld", GALAXY_PROP_GalaxyNr(galaxy));
    // Check if module providing this property is loaded
}
```

#### Module Loading Issues

```c
// Debug module registration
LOG_DEBUG("Checking module registration for type %d", module_type);
int num_modules = get_num_registered_modules();
for (int i = 0; i < num_modules; i++) {
    struct base_module *module = get_module_by_index(i);
    LOG_DEBUG("Registered module: %s (type %d)", module->name, module->type);
}
```

#### Pipeline Execution Issues

```c
// Debug pipeline execution
void debug_pipeline_execution(struct module_pipeline *pipeline) {
    LOG_DEBUG("Pipeline has %d steps", pipeline->num_steps);
    for (int i = 0; i < pipeline->num_steps; i++) {
        struct pipeline_step *step = &pipeline->steps[i];
        LOG_DEBUG("Step %d: module_type=%d, name=%s, enabled=%d", 
                  i, step->module_type, step->name, step->enabled);
    }
}
```

### GDB Debugging Tips

#### Useful GDB Commands

```gdb
# Set breakpoints
break main
break calculate_cooling_rate
break src/core/core_pipeline_system.c:150

# Conditional breakpoints
break galaxy_evolution if galaxy_id == 12345

# Watch variables
watch stellar_mass
watch *galaxy

# Print structures
print *galaxy
print galaxy->properties

# Call stack
backtrace
frame 2

# Memory examination
x/10i $pc      # Examine 10 instructions at current location
x/32x buffer   # Examine 32 bytes in hex format
```

#### GDB Scripts

Create `.gdb` files for common debugging scenarios:

```gdb
# debug_galaxy.gdb
define debug_galaxy
    if $argc != 1
        help debug_galaxy
    else
        set $gal = (struct GALAXY*)$arg0
        printf "Galaxy %lld: Type=%d, SnapNum=%d\n", $gal->GalaxyNr, $gal->Type, $gal->SnapNum
        printf "Position: (%.2f, %.2f, %.2f)\n", $gal->Pos[0], $gal->Pos[1], $gal->Pos[2]
        if $gal->properties != 0
            printf "Properties available\n"
        else
            printf "No properties\n"
        end
    end
end

document debug_galaxy
Print galaxy information
Usage: debug_galaxy <galaxy_pointer>
end
```

## Testing and Validation

### Unit Test Development

#### Test Structure

```c
// tests/test_new_feature.c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "core/core_allvars.h"
#include "new_feature.h"

static void setup_test_environment(void) {
    // Initialize test environment
    init_memory_management();
    init_property_system();
}

static void cleanup_test_environment(void) {
    // Clean up test environment
    cleanup_property_system();
    cleanup_memory_management();
}

static void test_basic_functionality(void) {
    // Test setup
    struct test_data *data = create_test_data();
    assert(data != NULL);
    
    // Test execution
    int result = new_feature_function(data);
    
    // Test validation
    assert(result == EXPECTED_VALUE);
    assert(data->output_field == EXPECTED_OUTPUT);
    
    // Test cleanup
    destroy_test_data(data);
}

static void test_error_conditions(void) {
    // Test NULL input
    int result = new_feature_function(NULL);
    assert(result == ERROR_INVALID_INPUT);
    
    // Test invalid parameters
    struct test_data invalid_data = {0};
    result = new_feature_function(&invalid_data);
    assert(result == ERROR_INVALID_PARAMETERS);
}

int main(void) {
    printf("Starting test_new_feature...\n");
    
    setup_test_environment();
    
    test_basic_functionality();
    test_error_conditions();
    
    cleanup_test_environment();
    
    printf("All tests passed!\n");
    return EXIT_SUCCESS;
}
```

#### Mock Objects

```c
// Create mock implementations for testing
struct mock_galaxy {
    int64_t GalaxyNr;
    int Type;
    int SnapNum;
    float Pos[3];
    galaxy_properties_t *properties;
};

struct mock_galaxy* create_mock_galaxy(int64_t id) {
    struct mock_galaxy *galaxy = mymalloc("mock_galaxy", sizeof(struct mock_galaxy));
    galaxy->GalaxyNr = id;
    galaxy->Type = 0;
    galaxy->SnapNum = 63;
    galaxy->Pos[0] = galaxy->Pos[1] = galaxy->Pos[2] = 0.0f;
    galaxy->properties = initialize_mock_properties();
    return galaxy;
}
```

### Integration Testing

#### Property System Integration

```c
static void test_property_integration(void) {
    // Test property access patterns
    struct GALAXY *galaxy = create_test_galaxy();
    
    // Core property access
    GALAXY_PROP_GalaxyNr(galaxy) = 12345;
    assert(GALAXY_PROP_GalaxyNr(galaxy) == 12345);
    
    // Physics property access
    property_id_t mass_id = get_property_id("StellarMass");
    if (has_property(galaxy, mass_id)) {
        set_float_property(galaxy, mass_id, 1.0e10f);
        float mass = get_float_property(galaxy, mass_id, 0.0f);
        assert(fabs(mass - 1.0e10f) < 1e-6);
    }
    
    cleanup_test_galaxy(galaxy);
}
```

### Performance Testing

#### Benchmarking Framework

```c
#include <time.h>

double get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void benchmark_function(void) {
    const int iterations = 1000000;
    double start_time = get_timestamp();
    
    for (int i = 0; i < iterations; i++) {
        function_to_benchmark();
    }
    
    double end_time = get_timestamp();
    double total_time = end_time - start_time;
    double time_per_call = total_time / iterations;
    
    printf("Benchmark results:\n");
    printf("Total time: %.6f seconds\n", total_time);
    printf("Time per call: %.9f seconds\n", time_per_call);
    printf("Calls per second: %.0f\n", 1.0 / time_per_call);
}
```

## Documentation Standards

### Code Documentation

#### Function Documentation

```c
/**
 * @brief Calculate the virial radius of a halo
 * 
 * Computes the virial radius using the virial overdensity criterion
 * for the given cosmological parameters.
 * 
 * @param halo_mass Halo mass in units of 10^10 Msun/h
 * @param redshift Redshift of the halo
 * @param cosmology Cosmological parameters structure
 * 
 * @return Virial radius in Mpc/h, or -1.0 on error
 * 
 * @note This function assumes a standard ΛCDM cosmology
 * @warning Returns -1.0 for unphysical input parameters
 * 
 * @see calculate_virial_velocity()
 * @see get_virial_overdensity()
 */
double calculate_virial_radius(double halo_mass, double redshift, 
                              const struct cosmology_params *cosmology);
```

#### Structure Documentation

```c
/**
 * @struct galaxy_properties
 * @brief Container for all galaxy properties
 * 
 * This structure holds all galaxy properties as defined in properties.yaml.
 * Properties are accessed through type-safe accessor functions rather than
 * direct field access to maintain core-physics separation.
 * 
 * @see get_float_property()
 * @see set_float_property()
 */
typedef struct galaxy_properties {
    /* Core properties */
    float Mvir;          /**< Virial mass [10^10 Msun/h] */
    float Rvir;          /**< Virial radius [Mpc/h] */
    
    /* Physics properties (availability depends on loaded modules) */
    float ColdGas;       /**< Cold gas mass [10^10 Msun/h] */
    float StellarMass;   /**< Stellar mass [10^10 Msun/h] */
} galaxy_properties_t;
```

### Architecture Documentation

#### Design Decision Documentation

```markdown
## Design Decision: Property Access Patterns

### Context
The SAGE architecture requires strict separation between core infrastructure 
and physics modules to enable runtime modularity.

### Decision
- Core properties: Direct access via GALAXY_PROP_* macros
- Physics properties: Generic accessors only (get_float_property, etc.)

### Rationale
1. Enables physics-free mode operation
2. Allows runtime property availability checking
3. Maintains type safety through generated dispatch functions
4. Supports modular physics development

### Consequences
- All physics property access must use generic functions
- Build system complexity increased for property generation
- Better separation of concerns and testability
```

## Best Practices

### Performance Considerations

#### Memory Access Patterns

```c
// Good: Sequential access for cache efficiency
for (int i = 0; i < ngal; i++) {
    process_galaxy(&galaxies[i]);
}

// Poor: Random access patterns
for (int i = 0; i < ngal; i++) {
    int random_index = get_random_index();
    process_galaxy(&galaxies[random_index]);
}
```

#### Function Call Optimization

```c
// Good: Cache property IDs for repeated access
static property_id_t stellar_mass_id = PROP_COUNT;
if (stellar_mass_id == PROP_COUNT) {
    stellar_mass_id = get_property_id("StellarMass");
}

// Use cached ID
float mass = get_float_property(galaxy, stellar_mass_id, 0.0f);
```

#### Memory Pool Usage

```c
// Good: Use memory pools for temporary allocations
memory_pool_t *temp_pool = memory_pool_create(pool_size);
for (int i = 0; i < many_iterations; i++) {
    void *temp_buffer = memory_pool_alloc(temp_pool, buffer_size);
    // Use buffer...
    // No need to free individual allocations
}
memory_pool_destroy(temp_pool);  // Frees everything at once
```

### Code Organization

#### Module Organization

```
src/physics/cooling/
├── cooling_interface.h      # Public interface
├── cooling_implementation.c # Core implementation
├── cooling_tables.c         # Data tables and lookup
├── cooling_utils.h          # Utility functions
└── tests/
    ├── test_cooling_basic.c
    └── test_cooling_tables.c
```

#### Header Dependencies

```c
// Minimize header dependencies
#include "core/core_allvars.h"  // Only include what's needed

// Forward declarations instead of includes when possible
struct GALAXY;  // Forward declaration
struct params;  // Forward declaration

// Function that uses forward-declared types
int process_galaxy_data(struct GALAXY *galaxy, struct params *params);
```

## Troubleshooting

### Common Development Issues

#### Build Issues

**Problem**: Property generation fails
```bash
# Solution: Clean and regenerate
make clean
python src/generate_property_headers.py  # Manual generation for debugging
make full-physics
```

**Problem**: Link errors with new modules
```bash
# Solution: Check module registration
grep -r "register.*module" src/  # Find all module registrations
nm -D libsage.so | grep module   # Check symbol exports
```

#### Runtime Issues

**Problem**: Module not found in pipeline
```c
// Debug: Check module registration
LOG_DEBUG("Registered modules:");
for (int i = 0; i < get_num_registered_modules(); i++) {
    struct base_module *mod = get_module_by_index(i);
    LOG_DEBUG("  %s (type %d)", mod->name, mod->type);
}
```

**Problem**: Property access errors
```c
// Debug: Check property availability
property_id_t prop_id = get_property_id("PropertyName");
if (prop_id == PROP_COUNT) {
    LOG_ERROR("Property 'PropertyName' not defined in current build");
    // Check properties.yaml and build configuration
}
```

### Getting Help

#### Documentation Resources

- Architecture documentation: `docs/architecture.md`
- Property system: `docs/property-system.md`
- I/O system: `docs/io-system.md`
- Testing guide: `docs/testing-guide.md`

#### Code Examples

- Module templates: `docs/templates/`
- Test templates: `docs/templates/test_template.c`
- Existing modules: `src/physics/`

#### Debugging Tools

```bash
# Memory debugging
valgrind --tool=memcheck --leak-check=full ./sage input/millennium.par

# Performance profiling
perf record ./sage input/millennium.par
perf report

# Code coverage
gcov -r *.gcda
lcov --capture --directory . --output-file coverage.info
```

This development guide provides the foundation for effective SAGE development. Follow these practices to maintain code quality, ensure architectural consistency, and contribute effectively to the project.