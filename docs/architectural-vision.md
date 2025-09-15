# SAGE Architectural Vision

**Purpose**: Comprehensive architectural guidance for SAGE's modern, modular galaxy evolution framework  
**Audience**: Developers, users, and contributors working with SAGE  
**Status**: Foundational principles guiding current development and future capabilities  

---

## Vision Statement

SAGE is being transformed from a monolithic galaxy evolution model into a **physics-agnostic core with runtime-configurable physics modules**, enabling scientific flexibility, maintainability, and extensibility while preserving exact scientific accuracy.

This architecture enables researchers to easily experiment with different physics combinations, developers to work independently on core infrastructure and physics modules, and the system to evolve gracefully as new scientific understanding emerges.

---

## 8 Core Architectural Principles

These principles guide all design decisions and implementation choices in SAGE:

### 1. Physics-Agnostic Core Infrastructure

**Principle**: The core SAGE infrastructure has zero knowledge of specific physics implementations.

**Requirements**:
- Core systems (memory management, I/O, tree processing, configuration) operate independently of physics
- Physics modules interact with core only through well-defined interfaces  
- Core can execute successfully with no physics modules loaded (physics-free mode)
- Physics modules are pure add-ons that extend core functionality

**Benefits**: Enables independent development of physics and infrastructure, simplifies testing, allows physics module hot-swapping, and reduces complexity in core systems.

**Implementation Example**:
```c
// Core system - no physics knowledge
int process_merger_tree(struct merger_tree *tree, struct pipeline *pipeline) {
    for (int snapshot = 0; snapshot < tree->n_snapshots; snapshot++) {
        struct galaxy_list *galaxies = load_galaxies_at_snapshot(tree, snapshot);
        
        // Execute configured modules - core doesn't know what they do
        pipeline_execute_snapshot_phase(pipeline, galaxies, snapshot);
        
        save_galaxies_if_output_snapshot(galaxies, snapshot);
        cleanup_galaxy_list(galaxies);
    }
    return 0;
}
```

### 2. Runtime Modularity

**Principle**: Physics module combinations should be configurable at runtime without recompilation.

**Requirements**:
- Module selection via configuration files, not compile-time flags
- Modules self-register and declare their capabilities/dependencies
- Pipeline execution adapts dynamically to loaded module set
- Empty pipeline configurations supported for core-only execution

**Benefits**: Scientific flexibility for different research questions, easier experimentation with physics combinations, deployment flexibility, and simplified testing of different physics scenarios.

**Implementation Example**:
```json
{
  "modules": {
    "cooling": {"enabled": true, "efficiency": 1.5},
    "starformation": {"enabled": true, "threshold": 0.1},
    "agn_feedback": {"enabled": false}
  }
}
```

### 3. Metadata-Driven Architecture

**Principle**: System structure should be defined by metadata rather than hardcoded implementations.

**Requirements**:
- Galaxy properties defined in metadata files, not hardcoded structs
- Parameters defined in metadata with automatic validation generation
- Build system generates type-safe code from metadata
- Output formats adapt automatically to available properties

**Benefits**: Reduces code duplication, eliminates manual synchronization between different representations, enables build-time optimization, and simplifies maintenance by having single source of truth.

**Implementation Example**:
```yaml
# properties.yaml - Single source of truth
ColdGas:
  type: float
  category: physics
  description: "Cold gas mass"
  units: "1e10 Msun/h"
  required_by: [starformation]
  provided_by: [cooling]
```

### 4. Single Source of Truth

**Principle**: Galaxy data should have one authoritative representation with consistent access patterns.

**Requirements**:
- Eliminate dual property systems and synchronization code
- All galaxy data access through unified property system
- Type-safe property access with compile-time optimization
- Consistent property lifecycle management

**Benefits**: Eliminates synchronization bugs between different representations, simplifies debugging by having single data path, reduces memory overhead, and improves performance through unified access patterns.

**Implementation Example**:
```c
// Type-safe property access - generated from metadata
#define GALAXY_GET_COLDGAS(g) ((g)->properties.ColdGas)
#define GALAXY_SET_COLDGAS(g, val) do { \
    (g)->properties.ColdGas = (val); \
    (g)->modified_mask |= PROPERTY_MASK_COLDGAS; \
} while(0)
```

### 5. Unified Processing Model

**Principle**: SAGE should have one consistent, well-understood method for processing merger trees.

**Requirements**:
- Single tree traversal algorithm that handles all scientific requirements
- Consistent galaxy inheritance and property calculation methods
- Robust orphan galaxy handling preventing data loss
- Clear separation between tree traversal logic and physics calculations

**Benefits**: Eliminates complexity from maintaining multiple processing modes, simplifies validation by having single algorithm to verify, reduces bug surface area, and makes the system easier to understand and modify.

### 6. Memory Efficiency and Safety

**Principle**: Memory usage should be bounded, predictable, and safe.

**Requirements**:
- Bounded memory usage that doesn't grow with total simulation size
- Automatic memory management for galaxy arrays and properties
- Tree-scoped allocation with bulk cleanup
- Memory leak detection and prevention

**Benefits**: Handles large simulations reliably without running out of memory, reduces debugging overhead by preventing memory-related bugs, improves performance predictability, and enables processing of simulations larger than available RAM.

**Implementation Example**:
```c
// Scope-based memory management
void process_forest(int forest_id) {
    memory_scope_t scope;
    memory_scope_enter(&scope);
    
    struct GALAXY *galaxies = sage_malloc(1000 * sizeof(struct GALAXY));
    memory_scope_register_cleanup(sage_free, galaxies);
    
    // ... process forest ...
    
    memory_scope_exit();  // All memory automatically freed
}
```

### 7. Format-Agnostic I/O

**Principle**: SAGE should support multiple input/output formats through unified interfaces.

**Requirements**:
- Common interface for all tree reading operations
- Property-based output generation that adapts to available data
- Cross-platform compatibility and endianness handling
- Graceful fallback mechanisms for unsupported operations

**Benefits**: Scientific compatibility with different simulation codes and analysis tools, future-proofing against format changes, simplified validation across different formats, and easier integration with external tools.

### 8. Type Safety and Validation

**Principle**: Data access should be type-safe with automatic validation.

**Requirements**:
- Type-safe property access generated from metadata
- Automatic bounds checking and validation
- Fail-fast behavior on invalid data access
- Clear error reporting and debugging information

**Benefits**: Reduces runtime errors by catching problems at compile-time, improves debugging experience with clear error messages, catches problems early in development cycle, and increases confidence in scientific accuracy.

---

## Implementation Philosophy

### Metadata-Driven Development
- **Single Source of Truth**: YAML metadata prevents synchronization bugs between different code representations
- **Code Generation**: Automatically generate type-safe C code from metadata definitions
- **Build Integration**: Code generation integrated into build system, not manual step

### Physics-Agnostic Core  
- **Zero Physics Knowledge**: Core infrastructure has no understanding of specific physics processes
- **Interface-Based Interaction**: Physics modules interact with core only through well-defined interfaces
- **Independent Development**: Core infrastructure and physics modules can be developed independently

### Type Safety First
- **Compile-Time Validation**: Catch errors at compile-time rather than runtime
- **Generated Access Patterns**: Type-safe property access generated from metadata
- **IDE Integration**: Full autocomplete, go-to-definition, and refactoring support

### Standard Tools
- **Industry Standards**: Leverage proven tools (CMake, HDF5, JSON) rather than custom solutions
- **Professional Workflow**: Modern development environment with IDE integration
- **Debugging Support**: All standard debugging tools work out of the box

---

## Target Architecture

### System Components

```
┌─────────────────────────────────────────────────────────────┐
│                    SAGE Application                        │
├─────────────────────────────────────────────────────────────┤
│  Configuration System     │  Module System                  │
│  - JSON/Legacy .par       │  - Runtime loading              │
│  - Schema validation      │  - Dependency resolution        │
├─────────────────────────────────────────────────────────────┤
│                  Physics-Agnostic Core                     │
│  ┌─────────────────┬─────────────────┬─────────────────┐   │
│  │ Memory Mgmt     │ Property System │ I/O System      │   │
│  │ - Scoped alloc  │ - Type-safe     │ - Format unified│   │
│  │ - Auto cleanup  │ - Generated     │ - Cross-platform│   │
│  └─────────────────┴─────────────────┴─────────────────┘   │
│  ┌─────────────────┬─────────────────┬─────────────────┐   │
│  │ Tree Processing │ Pipeline Exec   │ Test Framework  │   │
│  │ - Unified model │ - Configurable  │ - Multi-level   │   │
│  │ - Inheritance   │ - Module phases │ - Scientific    │   │
│  └─────────────────┴─────────────────┴─────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    Physics Modules                         │
│  ┌─────────────────┬─────────────────┬─────────────────┐   │
│  │ Cooling         │ Star Formation  │ AGN Feedback    │   │
│  │ Mergers         │ Reincorporation │ Disk Instability│   │
│  └─────────────────┴─────────────────┴─────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

1. **Configuration Loading**: JSON/legacy parameter files loaded and validated
2. **Module Registration**: Physics modules self-register based on configuration  
3. **Pipeline Creation**: Execution pipeline built from registered modules
4. **Tree Processing**: Core loads and processes merger trees using unified algorithm
5. **Module Execution**: Physics modules execute in dependency-resolved order
6. **Output Generation**: Property-based output adapts to available properties

---

## Success Criteria

### Technical Success
- **Architecture Validation**: Physics modules can be added/removed without core changes
- **Performance Validation**: Runtime performance within 10% of legacy implementation  
- **Memory Validation**: Bounded memory usage for large simulations
- **I/O Validation**: Successful processing of all supported tree formats

### Scientific Success
- **Accuracy Validation**: Identical scientific results to legacy implementation
- **Completeness Validation**: All legacy functionality preserved and accessible
- **Flexibility Validation**: Different physics combinations produce expected results
- **Robustness Validation**: System handles edge cases and error conditions gracefully

### Developer Experience Success
- **Migration Success**: Existing users can transition with minimal effort
- **Development Success**: New physics modules can be developed independently
- **Maintenance Success**: System maintainable by development team
- **Documentation Success**: Users and developers can understand and use the system

---

## Quality Attributes

### Maintainability
- **Modularity**: Clear separation of concerns with well-defined interfaces
- **Documentation**: Comprehensive documentation for developers and users
- **Code Quality**: Professional coding standards with consistent style
- **Testing**: Automated testing covering all major functionality

### Extensibility  
- **Module Development**: Clear patterns for adding new physics modules
- **Format Support**: Straightforward process for adding new I/O formats
- **Property Extension**: Easy addition of new galaxy properties
- **Configuration**: Flexible configuration system supporting new use cases

### Reliability
- **Error Handling**: Robust error detection and recovery mechanisms
- **Validation**: Comprehensive input and state validation
- **Memory Safety**: Automatic memory management preventing leaks and corruption
- **Debugging**: Clear error messages and debugging capabilities

### Usability
- **Scientific Workflow**: Intuitive configuration for different scientific use cases
- **Performance Analysis**: Built-in tools for understanding system performance
- **Debugging Support**: Clear diagnostics for troubleshooting problems
- **Migration Path**: Smooth transition from legacy usage patterns

---

## Design Constraints

### Scientific Accuracy Preservation
All architectural changes must preserve the exact scientific behavior of the legacy implementation:
- Preservation of original galaxy evolution algorithms
- Identical property calculations and inheritance rules
- Exact merger tree traversal ordering
- Consistent numerical precision and rounding

### Legacy Compatibility
The transformed system maintains compatibility with:
- Existing parameter file formats (.par files)
- Analysis tools expecting specific output formats
- Simulation data from various merger tree codes
- Existing scientific validation datasets

### Performance Constraints  
The new architecture does not significantly degrade performance:
- Memory usage comparable or better than legacy implementation
- Runtime performance within 10% of legacy implementation
- I/O performance maintained or improved
- Compilation time remains reasonable

---

## Current Implementation Status

### Phase 1: Foundation ✅ Complete
- Comprehensive property and parameter schemas created
- CMake-based build system with out-of-tree builds
- Code generation infrastructure producing type-safe property access
- Professional testing framework with scientific validation

### Phase 2A: Core/Physics Separation 🏗️ Partially Complete
- **✅ Task 2A.1 Complete**: Physics Module Interface Design - Clean interfaces established
- **🚧 In Progress**: Removing architectural violations in legacy code  
- **🚧 In Progress**: Ensuring core infrastructure operates independently of physics
- **🚧 In Progress**: Integration of physics modules with new interface

### Phase 2B-2D: Planned
- **Phase 2B**: Property Migration - Core Properties
- **Phase 2C**: Property Migration - Physics Properties  
- **Phase 2D**: Runtime Module System

---

## Conclusion

This architectural vision transforms SAGE into a modern, maintainable scientific software framework while preserving the scientific rigor that makes it valuable to the astrophysics community. The eight core principles provide a clear foundation for all development decisions, ensuring the system remains focused on its primary goals: scientific accuracy, developer productivity, and long-term maintainability.

The key insight driving this transformation is that **scientific accuracy and architectural elegance are not mutually exclusive**. By applying proven software engineering principles to scientific computing, we create a system that accelerates scientific discovery through improved flexibility, reliability, and maintainability.

Success will be measured not just by technical metrics, but by the system's ability to enable new scientific insights through easier experimentation, more reliable results, and faster development of new physics models.