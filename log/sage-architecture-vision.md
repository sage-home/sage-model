# SAGE Architecture Vision

**Document Purpose**: High-level architectural specification for transforming legacy SAGE into a modern, modular galaxy evolution framework  
**Target Audience**: Development team planning the transformation from legacy codebase  
**Legacy Baseline**: commit ba652705a48b80eae3b6a07675f8c7c938bc1ff6  

---

## Vision Statement

Transform SAGE from a monolithic galaxy evolution model into a **physics-agnostic core with runtime-configurable physics modules**, enabling scientific flexibility, maintainability, and extensibility while preserving exact scientific accuracy.

---

## Core Architectural Principles

### 1. Physics-Agnostic Core Infrastructure

**Principle**: The core SAGE infrastructure should have zero knowledge of specific physics implementations.

**Requirements**:
- Core systems (memory management, I/O, tree processing, configuration) operate independently of physics
- Physics modules interact with core only through well-defined interfaces
- Core can execute successfully with no physics modules loaded (physics-free mode)
- Physics modules are pure add-ons that extend core functionality

**Benefits**: Enables independent development of physics and infrastructure, simplifies testing, allows physics module hot-swapping.

### 2. Runtime Modularity

**Principle**: Physics module combinations should be configurable at runtime without recompilation.

**Requirements**:
- Module selection via configuration files, not compile-time flags
- Modules self-register and declare their capabilities/dependencies
- Pipeline execution adapts dynamically to loaded module set
- Empty pipeline configurations supported for core-only execution

**Benefits**: Scientific flexibility, easier experimentation, deployment flexibility, simplified testing of different physics combinations.

### 3. Metadata-Driven Architecture

**Principle**: System structure should be defined by metadata rather than hardcoded implementations.

**Requirements**:
- Galaxy properties defined in metadata files, not hardcoded structs
- Parameters defined in metadata with automatic validation generation
- Build system generates type-safe code from metadata
- Output formats adapt automatically to available properties

**Benefits**: Reduces code duplication, eliminates manual synchronization, enables build-time optimization, simplifies maintenance.

### 4. Single Source of Truth

**Principle**: Galaxy data should have one authoritative representation with consistent access patterns.

**Requirements**:
- Eliminate dual property systems and synchronization code
- All galaxy data access through unified property system
- Type-safe property access with compile-time optimization
- Consistent property lifecycle management

**Benefits**: Eliminates synchronization bugs, simplifies debugging, reduces memory overhead, improves performance.

### 5. Unified Processing Model

**Principle**: SAGE should have one consistent, well-understood method for processing merger trees.

**Requirements**:
- Single tree traversal algorithm that handles all scientific requirements
- Consistent galaxy inheritance and property calculation methods
- Robust orphan galaxy handling preventing data loss
- Clear separation between tree traversal logic and physics calculations

**Benefits**: Eliminates complexity from maintaining multiple processing modes, simplifies validation, reduces bug surface area.

### 6. Memory Efficiency and Safety

**Principle**: Memory usage should be bounded, predictable, and safe.

**Requirements**:
- Bounded memory usage that doesn't grow with total simulation size
- Automatic memory management for galaxy arrays and properties
- Tree-scoped allocation with bulk cleanup
- Memory leak detection and prevention

**Benefits**: Handles large simulations reliably, reduces debugging overhead, improves performance predictability.

### 7. Format-Agnostic I/O

**Principle**: SAGE should support multiple input/output formats through unified interfaces.

**Requirements**:
- Common interface for all tree reading operations
- Property-based output generation that adapts to available data
- Cross-platform compatibility and endianness handling
- Graceful fallback mechanisms for unsupported operations

**Benefits**: Scientific compatibility with different simulation codes, future-proofing, simplified validation across formats.

### 8. Type Safety and Validation

**Principle**: Data access should be type-safe with automatic validation.

**Requirements**:
- Type-safe property access generated from metadata
- Automatic bounds checking and validation
- Fail-fast behavior on invalid data access
- Clear error reporting and debugging information

**Benefits**: Reduces runtime errors, improves debugging experience, catches problems early in development.

---

## Functional Requirements

### Core Capabilities

**Tree Processing**:
- Read merger trees from multiple formats (LHalo, Gadget4, Genesis, ConsistentTrees)
- Traverse trees using scientifically accurate algorithms
- Handle galaxy inheritance, merger events, and orphan galaxies
- Maintain mass conservation and galaxy counting accuracy

**Module System**:
- Load physics modules at runtime based on configuration
- Execute modules in defined phases with proper dependencies
- Enable inter-module communication for complex physics interactions
- Support module-specific configuration and parameter validation

**Property Management**:
- Define galaxy properties in metadata with type safety
- Support dynamic arrays sized by simulation parameters
- Provide efficient property access with compile-time optimization
- Enable build-time property set optimization for different use cases

**Output System**:
- Generate HDF5 output with hierarchical organization
- Adapt output fields dynamically to available properties
- Support custom property transformations (unit conversion, derived quantities)
- Maintain compatibility with existing analysis tools

**Configuration Management**:
- Support both legacy parameter files and modern configuration formats
- Validate parameters with automatic bounds checking
- Enable runtime module configuration and selection
- Provide clear error messages for invalid configurations

### Performance Requirements

**Memory Usage**: Bounded by maximum galaxies per snapshot, not total simulation size
**Processing Performance**: Comparable to legacy implementation
**I/O Performance**: Efficient handling of large tree files and output datasets
**Scalability**: Support for parallel processing (MPI) without architecture changes

### Scientific Requirements

**Accuracy**: Preserve exact scientific results from legacy implementation
**Validation**: Comprehensive testing at unit, integration, and scientific levels
**Reproducibility**: Identical results across different module configurations when using same physics
**Backwards Compatibility**: Support for existing parameter files and analysis workflows

---

## Design Constraints

### Scientific Accuracy Preservation

All architectural changes must preserve the exact scientific behavior of the legacy implementation. This requires:
- Preservation of original galaxy evolution algorithms
- Identical property calculations and inheritance rules
- Exact merger tree traversal ordering
- Consistent numerical precision and rounding

### Legacy Compatibility

The transformed system should maintain compatibility with:
- Existing parameter file formats
- Analysis tools expecting specific output formats
- Simulation data from various merger tree codes
- Existing scientific validation datasets

### Performance Constraints

The new architecture should not significantly degrade performance:
- Memory usage should be comparable or better than legacy
- Runtime performance should be within 10% of legacy implementation
- I/O performance should be maintained or improved
- Compilation time should remain reasonable

---

## Quality Attributes

### Maintainability

**Modularity**: Clear separation of concerns with well-defined interfaces
**Documentation**: Comprehensive documentation for developers and users
**Code Quality**: Professional coding standards with consistent style
**Testing**: Automated testing covering all major functionality

### Extensibility

**Module Development**: Clear patterns for adding new physics modules
**Format Support**: Straightforward process for adding new I/O formats
**Property Extension**: Easy addition of new galaxy properties
**Configuration**: Flexible configuration system supporting new use cases

### Reliability

**Error Handling**: Robust error detection and recovery mechanisms
**Validation**: Comprehensive input and state validation
**Memory Safety**: Automatic memory management preventing leaks and corruption
**Debugging**: Clear error messages and debugging capabilities

### Usability

**Scientific Workflow**: Intuitive configuration for different scientific use cases
**Performance Analysis**: Built-in tools for understanding system performance
**Debugging Support**: Clear diagnostics for troubleshooting problems
**Migration Path**: Smooth transition from legacy usage patterns

---

## System Boundaries

### In Scope

**Core Infrastructure**: Memory management, tree processing, I/O, configuration, module system
**Essential Physics**: Minimal physics functions required for core operation
**Standard Properties**: Galaxy properties needed for basic scientific functionality
**Basic I/O Formats**: Support for major merger tree and output formats
**Testing Framework**: Comprehensive validation covering architecture and science

### Out of Scope (Future Extensions)

**Advanced Physics Modules**: Complex astrophysical processes beyond basic functionality
**Visualization Tools**: Real-time or interactive visualization capabilities
**Database Integration**: Direct database storage or retrieval capabilities
**Distributed Computing**: Advanced parallelization beyond basic MPI support
**Real-time Processing**: Streaming or real-time data processing capabilities

---

## Success Criteria

### Technical Success

1. **Architecture Validation**: Physics modules can be added/removed without core changes
2. **Performance Validation**: Runtime performance within 10% of legacy implementation
3. **Memory Validation**: Bounded memory usage for large simulations
4. **I/O Validation**: Successful processing of all supported tree formats

### Scientific Success

1. **Accuracy Validation**: Identical scientific results to legacy implementation
2. **Completeness Validation**: All legacy functionality preserved and accessible
3. **Flexibility Validation**: Different physics combinations produce expected results
4. **Robustness Validation**: System handles edge cases and error conditions gracefully

### Usability Success

1. **Migration Success**: Existing users can transition with minimal effort
2. **Development Success**: New physics modules can be developed independently
3. **Maintenance Success**: System can be maintained by development team
4. **Documentation Success**: Users and developers can understand and use the system

---

## Implementation Considerations

### Development Approach

**Incremental Transformation**: Architecture should be implementable in phases
**Validation at Each Step**: Each phase should be scientifically validated before proceeding
**Backwards Compatibility**: Legacy functionality should be preserved throughout transition
**Risk Mitigation**: Critical changes should have fallback mechanisms

### Technology Choices

**Language**: Continue using C for performance and scientific computing compatibility
**Build System**: Enhance existing build system rather than wholesale replacement
**Dependencies**: Minimize new dependencies, leverage existing HDF5 and MPI infrastructure
**Portability**: Maintain cross-platform compatibility (Linux, macOS)

### Testing Strategy

**Multi-Level Validation**: Unit tests for components, integration tests for systems, scientific tests for accuracy
**Regression Prevention**: Comprehensive baseline comparison against legacy results
**Performance Monitoring**: Automated performance regression detection
**Continuous Validation**: Automated testing of all major functionality

---

## Conclusion

This vision describes a transformation of SAGE from a monolithic scientific code into a modern, modular, and maintainable software architecture. The key insight is that **scientific accuracy and architectural elegance are not mutually exclusive** - the system can be both scientifically rigorous and well-engineered.

The transformation should enable:
- **Scientific Flexibility**: Easy experimentation with different physics combinations
- **Development Efficiency**: Independent development of core infrastructure and physics
- **Maintenance Simplicity**: Clear separation of concerns and comprehensive testing
- **Future Extensibility**: Straightforward addition of new capabilities

Success will be measured not just by technical metrics, but by the system's ability to **accelerate scientific discovery** through improved flexibility, reliability, and maintainability while preserving the scientific rigor that makes SAGE valuable to the astrophysics community.

This vision provides the architectural north star for the development team to create their own implementation plan, potentially achieving these goals through a cleaner and more direct path than the evolutionary approach that brought us to this understanding.