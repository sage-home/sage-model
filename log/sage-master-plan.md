# SAGE Master Implementation Plan
**Version**: 1.0  
**Date**: December 2024  
**Legacy Baseline**: commit ba652705a48b80eae3b6a07675f8c7c938bc1ff6  

---

## Executive Summary

This Master Implementation Plan guides the transformation of SAGE from a monolithic galaxy evolution model into a modern, modular architecture with a physics-agnostic core and runtime-configurable physics modules. The plan is structured in 7 phases, each delivering working functionality while preserving exact scientific accuracy.

### Key Transformation Goals
1. **Build System Modernization**: Replace Makefile with CMake for better IDE integration and dependency management
2. **Type-Safe Property System**: Replace string-based property access with compile-time validated system
3. **Memory Management**: Replace custom allocators with standard allocators + RAII-style cleanup
4. **Unified Configuration**: Single JSON-based configuration system replacing dual .par/internal systems

### Implementation Approach
- **Incremental Migration**: Abstraction layers enable gradual transition
- **Scientific Preservation**: Every phase validates against reference results
- **Risk Mitigation**: Critical areas addressed early with fallback options
- **AI Development Ready**: Tasks sized for 1-3 day implementation sessions

---

## Phase Overview

| Phase | Name | Duration | Entry Criteria | Exit Criteria | Critical Dependencies |
|-------|------|----------|----------------|---------------|---------------------|
| 1 | Infrastructure Foundation | 3 weeks | Legacy codebase analysis complete | CMake build system working, abstraction layers in place | None |
| 2 | Property System Core | 4 weeks | Phase 1 complete, YAML design finalized | Type-safe property generation working | Phase 1 |
| 3 | Memory Management | 3 weeks | Phase 2 complete | Standard allocators integrated, RAII patterns working | Phases 1, 2 |
| 4 | Configuration Unification | 2 weeks | Phase 3 complete | Unified JSON config with legacy support | Phases 1, 2, 3 |
| 5 | Module System Architecture | 4 weeks | Phase 4 complete | Self-registering modules with dependency resolution | Phases 1-4 |
| 6 | I/O System Modernization | 3 weeks | Phase 5 complete | Unified I/O interface with property-based serialization | Phases 1-5 |
| 7 | Validation & Polish | 2 weeks | Phase 6 complete | Full scientific validation, documentation complete | All phases |

**Total Duration**: ~21 weeks (5-6 months)

---

## Critical Path Analysis

### Primary Critical Path
Phase 1 → Phase 2 → Phase 5 → Phase 6 → Phase 7

**Reasoning**: The property system (Phase 2) is fundamental to the module system (Phase 5), which drives the I/O system (Phase 6). These cannot be parallelized effectively.

### Parallelization Opportunities
- **Phase 3** (Memory Management) can begin after Phase 2 core property access is working
- **Phase 4** (Configuration) can start once Phase 1 provides JSON reading infrastructure
- Documentation updates can proceed in parallel with implementation

### Risk Points
1. **Phase 2 Property Generation**: Complex metadata → code generation pipeline
2. **Phase 5 Module Dependencies**: Dependency resolution algorithm complexity
3. **Phase 6 I/O Validation**: Ensuring output compatibility with analysis tools

---

## Phase 1: Infrastructure Foundation

### Objectives
- **Primary**: Establish CMake build system with out-of-tree builds
- **Secondary**: Create abstraction layers for smooth migration

### Entry Criteria
- Development environment setup complete
- Legacy codebase analysis documented
- Team agreement on directory structure

### Tasks

#### Task 1.1: CMake Build System Setup
- **Objective**: Replace Makefile with modern CMake configuration
- **Implementation**: 
  - Create root CMakeLists.txt with project configuration
  - Set up source file discovery and compilation flags
  - Configure HDF5 and MPI detection
  - Enable out-of-tree builds
- **Testing**: Build produces identical binary to Makefile version
- **Documentation**: Update README with CMake build instructions
- **Risk**: Team unfamiliar with CMake - provide templates and examples
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: None

#### Task 1.2: Directory Reorganization
- **Objective**: Implement modern project structure per architecture guide
- **Implementation**:
  - Move source files to proper subdirectories (core/, physics/, io/)
  - Set up docs/ with role-based navigation
  - Create log/ directory for AI development support
  - Establish tests/ structure for categorized testing
- **Testing**: All files accessible, build system finds all sources
- **Documentation**: Create directory structure documentation
- **Risk**: Git history preservation - use git mv commands
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 1.1 partially complete

#### Task 1.3: Memory Abstraction Layer
- **Objective**: Create sage_malloc/sage_free abstractions
- **Implementation**:
  - Create memory.h with allocation macros
  - Replace mymalloc calls with sage_malloc
  - Keep existing mymalloc implementation initially
  - Add allocation tracking for debugging
- **Testing**: Memory tests pass, no leaks detected
- **Documentation**: Document abstraction pattern and migration plan
- **Risk**: Minimal - simple macro replacement initially
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 1.2 complete

#### Task 1.4: Configuration Abstraction Layer
- **Objective**: Create unified config reading interface
- **Implementation**:
  - Design config_t structure for unified access
  - Create config_reader.c with JSON support
  - Add legacy .par file reading to same interface
  - Implement configuration validation framework
- **Testing**: Both JSON and .par files read correctly
- **Documentation**: Configuration format specification
- **Risk**: JSON library selection - use proven solution
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 1.1 complete

#### Task 1.5: Logging System Setup
- **Objective**: Implement development logging for AI support
- **Implementation**:
  - Create log/README.md with system description
  - Set up template files for phases and decisions
  - Implement log rotation/archiving system
  - Create CLAUDE.md with project overview
- **Testing**: Logging creates expected file structure
- **Documentation**: Logging best practices guide
- **Risk**: None - auxiliary system
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 1.2 complete

### Exit Criteria
- CMake build produces working SAGE binary
- All abstraction layers in place and tested
- Legacy .par files still work
- Development logging system operational
- All tests pass with identical scientific results

### Validation
- Run test_sage.sh with both Makefile and CMake builds
- Compare binary outputs byte-for-byte
- Verify IDE integration works (VS Code, CLion)
- Check debug builds with AddressSanitizer

---

## Phase 2: Property System Core

### Objectives
- **Primary**: Implement metadata-driven property system with code generation
- **Secondary**: Enable type-safe, compile-time validated property access

### Entry Criteria
- CMake build system fully operational
- Directory structure reorganized
- Python environment configured for code generation

### Tasks

#### Task 2.1: Property Metadata Design
- **Objective**: Create YAML schema for property definitions
- **Implementation**:
  - Design properties.yaml structure
  - Define core vs physics property categories
  - Specify array properties with size parameters
  - Include units, descriptions, output flags
- **Testing**: YAML validates against schema
- **Documentation**: Property definition guide
- **Risk**: Over-engineering schema - keep it simple
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: None

#### Task 2.2: Code Generation Framework
- **Objective**: Build Python generator for property headers
- **Implementation**:
  - Create generate_property_headers.py
  - Generate type-safe getter/setter macros
  - Create property enumeration and masks
  - Implement compile-time availability checks
- **Testing**: Generated code compiles without warnings
- **Documentation**: Code generation design documentation
- **Risk**: Complex template generation - use proven patterns
- **Effort**: 3 sessions (high complexity)
- **Dependencies**: Task 2.1 complete

#### Task 2.3: Property Migration - Core Properties
- **Objective**: Convert core GALAXY struct members to property system
- **Implementation**:
  - Define core properties in YAML
  - Update GALAXY struct to use generated definitions
  - Replace direct member access with macros
  - Maintain backward compatibility temporarily
- **Testing**: Core property access works identically
- **Documentation**: Migration guide for developers
- **Risk**: Breaking existing code - use incremental approach
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 2.2 complete

#### Task 2.4: Property Migration - Physics Properties
- **Objective**: Convert physics-specific properties
- **Implementation**:
  - Identify all physics-dependent properties
  - Add conditional compilation for physics properties
  - Update physics modules to use new access patterns
  - Remove old property access code
- **Testing**: Physics calculations produce same results
- **Documentation**: Physics property usage examples
- **Risk**: Missing property dependencies - thorough analysis needed
- **Effort**: 3 sessions (high complexity)
- **Dependencies**: Task 2.3 complete

#### Task 2.5: CMake Integration
- **Objective**: Integrate code generation into build system
- **Implementation**:
  - Add custom command for property generation
  - Set up proper dependencies on YAML files
  - Configure different property sets for build modes
  - Enable regeneration on YAML changes
- **Testing**: Build regenerates on YAML changes
- **Documentation**: Build configuration documentation
- **Risk**: CMake complexity - use standard patterns
- **Effort**: 1 session (low complexity)
- **Dependencies**: Tasks 2.2, 2.3 complete

### Exit Criteria
- All galaxy properties defined in YAML
- Type-safe access throughout codebase
- Code generation integrated into build
- No string-based property lookups remain
- Scientific results unchanged

### Validation
- Full regression test suite passes
- No compiler warnings from generated code
- IDE autocomplete works for all properties
- Performance benchmarks show no degradation

---

## Phase 3: Memory Management

### Objectives
- **Primary**: Replace custom allocators with standard + RAII patterns
- **Secondary**: Implement scoped memory management

### Entry Criteria
- Property system fully operational
- All galaxy data access type-safe
- Memory abstraction layer in place

### Tasks

#### Task 3.1: RAII Pattern Implementation
- **Objective**: Create C-style RAII memory management
- **Implementation**:
  - Design memory_scope structure
  - Implement cleanup handler stack
  - Create scope entry/exit functions
  - Add automatic cleanup registration
- **Testing**: Memory freed on scope exit
- **Documentation**: RAII pattern usage guide
- **Risk**: C developers unfamiliar with pattern
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Phase 1 memory abstraction

#### Task 3.2: Forest Processing Memory Scopes
- **Objective**: Apply RAII to forest processing
- **Implementation**:
  - Identify forest-level allocations
  - Wrap forest processing in memory scope
  - Register all allocations for cleanup
  - Remove manual free calls
- **Testing**: No memory leaks in forest processing
- **Documentation**: Forest memory lifecycle
- **Risk**: Missing some allocations - use tools
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 3.1 complete

#### Task 3.3: Remove Custom Allocator
- **Objective**: Replace mymalloc with standard allocators
- **Implementation**:
  - Update sage_malloc to use malloc directly
  - Remove mymalloc.c/h files
  - Update memory tracking if needed
  - Verify all allocations still tracked
- **Testing**: Valgrind shows no leaks
- **Documentation**: Memory management architecture
- **Risk**: Losing memory statistics - add new tracking
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 3.2 complete

#### Task 3.4: Galaxy Array Management
- **Objective**: Implement efficient galaxy array allocation
- **Implementation**:
  - Create galaxy array allocation functions
  - Implement growth strategies for dynamic arrays
  - Add bounds checking in debug mode
  - Optimize for cache performance
- **Testing**: Array operations perform correctly
- **Documentation**: Array management patterns
- **Risk**: Performance regression - benchmark carefully
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Tasks 3.1, 3.2 complete

#### Task 3.5: Memory Profiling Integration
- **Objective**: Add memory usage tracking and reporting
- **Implementation**:
  - Track allocations by category
  - Report memory usage statistics
  - Add peak memory tracking
  - Create memory usage visualization
- **Testing**: Reports accurate memory usage
- **Documentation**: Memory profiling guide
- **Risk**: Overhead concerns - make optional
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 3.3 complete

### Exit Criteria
- All custom allocators removed
- RAII patterns used throughout
- Standard debugging tools work perfectly
- Memory usage bounded and predictable
- No memory leaks detected

### Validation
- Full Valgrind test suite passes
- AddressSanitizer reports no issues
- Memory usage stays constant over long runs
- Performance within 5% of legacy

---

## Phase 4: Configuration Unification

### Objectives
- **Primary**: Create unified JSON configuration system
- **Secondary**: Maintain backward compatibility with .par files

### Entry Criteria
- Configuration abstraction layer in place
- JSON parsing library integrated
- Property system operational

### Tasks

#### Task 4.1: Configuration Schema Design
- **Objective**: Design comprehensive JSON schema
- **Implementation**:
  - Create schema for all configuration sections
  - Define simulation, module, and build configs
  - Add parameter validation rules
  - Include legacy compatibility fields
- **Testing**: Schema validates example configs
- **Documentation**: Configuration reference guide
- **Risk**: Over-complex schema - iterate with users
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: None

#### Task 4.2: JSON Configuration Parser
- **Objective**: Implement robust JSON config reading
- **Implementation**:
  - Parse JSON into config_t structure
  - Implement schema validation
  - Add helpful error messages
  - Support environment variable expansion
- **Testing**: Valid and invalid configs handled correctly
- **Documentation**: Configuration error troubleshooting
- **Risk**: Poor error messages - focus on clarity
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 4.1 complete

#### Task 4.3: Legacy Parameter Migration
- **Objective**: Support existing .par files transparently
- **Implementation**:
  - Detect file format automatically
  - Convert .par to internal config_t
  - Provide migration warnings
  - Create .par to JSON converter tool
- **Testing**: All .par files work unchanged
- **Documentation**: Migration guide from .par to JSON
- **Risk**: Missing .par features - comprehensive testing
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 4.2 complete

#### Task 4.4: Module Configuration Integration
- **Objective**: Enable module-specific configuration
- **Implementation**:
  - Add module config sections to schema
  - Pass config to module initialization
  - Validate module parameters
  - Support dynamic module loading config
- **Testing**: Modules receive correct configuration
- **Documentation**: Module configuration examples
- **Risk**: Module interface changes - plan carefully
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 4.2 complete

### Exit Criteria
- Single configuration system throughout code
- JSON primary format with .par compatibility
- Schema validation catches errors early
- Module configurations properly handled
- Migration tools available

### Validation
- All existing .par files work
- JSON configs produce identical results
- Validation catches common errors
- Error messages helpful and clear

---

## Phase 5: Module System Architecture

### Objectives
- **Primary**: Implement self-registering physics modules
- **Secondary**: Enable runtime module configuration

### Entry Criteria
- Property system complete
- Configuration system unified
- Memory management modernized

### Tasks

#### Task 5.1: Module Interface Design
- **Objective**: Define module structure and lifecycle
- **Implementation**:
  - Create module_info_t structure
  - Define initialization/shutdown interfaces
  - Specify execution phase callbacks
  - Add capability and dependency declarations
- **Testing**: Module interface compiles
- **Documentation**: Module development guide
- **Risk**: Over-complex interface - start simple
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: None

#### Task 5.2: Module Registry Implementation
- **Objective**: Build automatic module registration
- **Implementation**:
  - Create global module registry
  - Use constructor attributes for registration
  - Implement module lookup functions
  - Add module listing capabilities
- **Testing**: Modules register automatically
- **Documentation**: Module registration internals
- **Risk**: Platform compatibility - test thoroughly
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 5.1 complete

#### Task 5.3: Core/Physics Separation
- **Objective**: Move physics code into modules
- **Implementation**:
  - Identify physics vs core functions
  - Create physics module files
  - Move physics functions to modules
  - Update build system for modules
- **Testing**: Physics modules load correctly
- **Documentation**: Core/physics boundary definition
- **Risk**: Unclear boundaries - document decisions
- **Effort**: 3 sessions (high complexity)
- **Dependencies**: Task 5.2 complete

#### Task 5.4: Pipeline Execution Engine
- **Objective**: Build flexible execution pipeline
- **Implementation**:
  - Create pipeline structure
  - Add modules based on configuration
  - Implement phase-based execution
  - Handle module communication
- **Testing**: Pipeline executes all phases
- **Documentation**: Pipeline architecture guide
- **Risk**: Performance overhead - optimize critical path
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 5.3 complete

#### Task 5.5: Dependency Resolution
- **Objective**: Automatic module dependency handling
- **Implementation**:
  - Parse module dependencies
  - Implement topological sort
  - Detect circular dependencies
  - Order pipeline by dependencies
- **Testing**: Dependencies resolved correctly
- **Documentation**: Dependency system explanation
- **Risk**: Complex dependency graphs - limit depth
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 5.4 complete

### Exit Criteria
- All physics in self-contained modules
- Modules register automatically
- Pipeline adapts to module configuration
- Dependencies resolved correctly
- Core has zero physics knowledge

### Validation
- Physics-free mode runs successfully
- Module combinations produce expected results
- No hardcoded physics in core
- Performance comparable to monolithic

---

## Phase 6: I/O System Modernization

### Objectives
- **Primary**: Create unified I/O interface for all formats
- **Secondary**: Implement property-based output generation

### Entry Criteria
- Module system operational
- Property system complete
- All file I/O identified

### Tasks

#### Task 6.1: I/O Interface Design
- **Objective**: Define common I/O interface
- **Implementation**:
  - Create io_interface_t structure
  - Define read/write operations
  - Add format capability flags
  - Include resource management
- **Testing**: Interface supports all formats
- **Documentation**: I/O interface specification
- **Risk**: Missing format requirements - survey all
- **Effort**: 1 session (low complexity)
- **Dependencies**: None

#### Task 6.2: Format Adapter Implementation
- **Objective**: Wrap existing I/O in unified interface
- **Implementation**:
  - Create adapter for each format
  - Implement interface methods
  - Handle format-specific options
  - Add error handling
- **Testing**: All formats work through interface
- **Documentation**: Format adapter patterns
- **Risk**: Interface impedance mismatch - iterate
- **Effort**: 3 sessions (high complexity)
- **Dependencies**: Task 6.1 complete

#### Task 6.3: Property-Based Output
- **Objective**: Generate output from property metadata
- **Implementation**:
  - Query available properties at runtime
  - Create datasets for each property
  - Handle array properties correctly
  - Support property transformations
- **Testing**: Output contains all properties
- **Documentation**: Output customization guide
- **Risk**: Output compatibility - validate carefully
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 6.2 complete

#### Task 6.4: HDF5 Hierarchical Output
- **Objective**: Implement structured HDF5 output
- **Implementation**:
  - Design group hierarchy
  - Add metadata attributes
  - Implement chunking/compression
  - Create property-based datasets
- **Testing**: HDF5 files readable by tools
- **Documentation**: HDF5 output format spec
- **Risk**: Breaking analysis tools - test thoroughly
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 6.3 complete

#### Task 6.5: Output Validation Tools
- **Objective**: Update sagediff.py for new formats
- **Implementation**:
  - Handle multiple output formats
  - Compare property values
  - Report differences clearly
  - Add tolerance controls
- **Testing**: Detects output differences
- **Documentation**: Validation tool usage
- **Risk**: Missing subtle differences - comprehensive tests
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 6.4 complete

### Exit Criteria
- Unified I/O interface operational
- All formats supported
- Property-based output working
- Analysis tools compatible
- Validation tools updated

### Validation
- All input formats read correctly
- Output readable by analysis tools
- Property values match legacy exactly
- Performance acceptable

---

## Phase 7: Validation & Polish

### Objectives
- **Primary**: Comprehensive scientific validation
- **Secondary**: Documentation completion and optimization

### Entry Criteria
- All implementation phases complete
- Basic testing passing
- Documentation framework in place

### Tasks

#### Task 7.1: Scientific Validation Suite
- **Objective**: Ensure exact scientific accuracy
- **Implementation**:
  - Create comprehensive test cases
  - Compare with legacy outputs
  - Test all module combinations
  - Validate edge cases
- **Testing**: All results match legacy
- **Documentation**: Validation report
- **Risk**: Subtle differences - extensive testing
- **Effort**: 3 sessions (high complexity)
- **Dependencies**: All phases complete

#### Task 7.2: Performance Optimization
- **Objective**: Achieve performance parity with legacy
- **Implementation**:
  - Profile critical paths
  - Optimize property access
  - Tune memory allocation
  - Parallelize where beneficial
- **Testing**: Performance benchmarks pass
- **Documentation**: Performance analysis
- **Risk**: Premature optimization - measure first
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: Task 7.1 complete

#### Task 7.3: Documentation Completion
- **Objective**: Comprehensive user and developer docs
- **Implementation**:
  - Complete API documentation
  - Write user guides
  - Create migration guide
  - Add architecture documentation
- **Testing**: Documentation review
- **Documentation**: All guides complete
- **Risk**: Incomplete coverage - systematic review
- **Effort**: 2 sessions (moderate complexity)
- **Dependencies**: None

#### Task 7.4: CI/CD Integration
- **Objective**: Automated testing and validation
- **Implementation**:
  - Set up GitHub Actions
  - Configure test matrix
  - Add performance regression tests
  - Create release automation
- **Testing**: CI/CD pipeline works
- **Documentation**: CI/CD maintenance guide
- **Risk**: Complex test matrix - start simple
- **Effort**: 1 session (low complexity)
- **Dependencies**: Task 7.1 complete

#### Task 7.5: Release Preparation
- **Objective**: Prepare for production release
- **Implementation**:
  - Final code cleanup
  - Update version numbers
  - Create release notes
  - Package for distribution
- **Testing**: Release candidate testing
- **Documentation**: Release documentation
- **Risk**: Missing issues - beta testing
- **Effort**: 1 session (low complexity)
- **Dependencies**: All tasks complete

### Exit Criteria
- All tests passing
- Performance acceptable
- Documentation complete
- CI/CD operational
- Release candidate ready

### Validation
- Full regression suite passes
- Performance within 10% of legacy
- Beta users provide positive feedback
- No critical issues found

---

## Risk Assessment & Mitigation

### Technical Risks

#### High Risk: Property System Complexity
- **Impact**: Core functionality broken
- **Probability**: Medium
- **Mitigation**: 
  - Incremental migration with fallbacks
  - Extensive testing at each step
  - Keep legacy access during transition

#### High Risk: Module Dependency Resolution
- **Impact**: Incorrect physics execution order
- **Probability**: Medium
- **Mitigation**:
  - Limit dependency depth
  - Clear dependency rules
  - Extensive testing of combinations

#### Medium Risk: Output Format Compatibility
- **Impact**: Breaking analysis workflows
- **Probability**: Medium
- **Mitigation**:
  - Maintain legacy format support
  - Extensive validation with real tools
  - Beta testing with users

### Scientific Risks

#### Critical Risk: Numerical Differences
- **Impact**: Invalid scientific results
- **Probability**: Low
- **Mitigation**:
  - Bit-exact comparison testing
  - Regression tests at each phase
  - Scientific validation suite

### Timeline Risks

#### Medium Risk: Underestimated Complexity
- **Impact**: Schedule delays
- **Probability**: High
- **Mitigation**:
  - Conservative estimates
  - Early spike investigations
  - Regular retrospectives

---

## Success Metrics

### Phase-Level Metrics
- **Build Time**: < 2 minutes for full build
- **Test Coverage**: > 80% of new code
- **Performance**: Within 10% of legacy
- **Memory Usage**: No increase vs legacy

### Project-Level Metrics
- **Scientific Accuracy**: Bit-exact with legacy
- **Developer Productivity**: 50% reduction in development time
- **Maintenance Burden**: 70% reduction in bug reports
- **Onboarding Time**: < 1 day for new developers

---

## Resource Requirements

### Development Resources
- **Lead Developer**: 100% time for duration
- **Supporting Developers**: 2 developers at 50% time
- **Scientific Validation**: 1 scientist at 25% time

### Infrastructure Resources
- **CI/CD**: GitHub Actions runners
- **Testing**: Access to reference datasets
- **Development**: Modern IDEs and tools

---

## Implementation Guidelines

### Development Workflow
1. **Phase Planning**: Update log/phase.md with specific tasks
2. **Task Implementation**: Follow test-driven development
3. **Code Review**: All changes reviewed before merge
4. **Validation**: Run regression tests before commits
5. **Documentation**: Update docs with code changes

### Communication
- **Weekly**: Team sync on progress
- **Phase End**: Retrospective and plan adjustment
- **Monthly**: Stakeholder updates

### Quality Standards
- **Code Style**: Consistent formatting enforced
- **Testing**: Comprehensive unit and integration tests
- **Documentation**: Code and user docs in sync
- **Performance**: No regression without justification

---

## Conclusion

This Master Implementation Plan provides a clear, actionable path for transforming SAGE into a modern, maintainable system while preserving its scientific integrity. The phased approach minimizes risk while delivering value incrementally. Success depends on disciplined execution, thorough testing, and maintaining focus on both technical excellence and scientific accuracy.

The transformation will enable SAGE to:
- Support flexible physics configurations without recompilation
- Provide robust, type-safe interfaces preventing common errors  
- Integrate with modern development tools and workflows
- Scale efficiently to larger simulations
- Accelerate scientific discovery through improved maintainability

With careful execution of this plan, SAGE will evolve from a legacy monolithic system into a modern, modular framework ready for the next generation of galaxy evolution research.

---

## Appendix A: Early Risk Investigation Tasks

To de-risk the most complex areas before they become blockers, the following investigation tasks should be completed early:

### Investigation 1: Property System Code Generation (Before Phase 2)
- **Objective**: Validate the YAML → Python → C generation approach
- **Tasks**:
  - Create minimal proof-of-concept generator
  - Test with 5-10 representative properties
  - Verify IDE integration works correctly
  - Confirm compile-time optimization occurs
- **Duration**: 2-3 days
- **Success Criteria**: Generated code compiles, runs, and provides autocomplete

### Investigation 2: Module Self-Registration (Before Phase 5)
- **Objective**: Confirm constructor attribute approach works across platforms
- **Tasks**:
  - Test `__attribute__((constructor))` on Linux, macOS
  - Verify link order doesn't affect registration
  - Test with static and shared libraries
  - Investigate alternative approaches if needed
- **Duration**: 1-2 days
- **Success Criteria**: Modules register reliably on all platforms

### Investigation 3: HDF5 Output Compatibility (Before Phase 6)
- **Objective**: Ensure new property-based output is compatible with analysis tools
- **Tasks**:
  - Survey existing analysis scripts for output dependencies
  - Create prototype property-based HDF5 output
  - Test with common analysis tools
  - Document any required tool updates
- **Duration**: 2-3 days
- **Success Criteria**: Key analysis tools read new output format

---

## Appendix B: Incremental Migration Patterns

### Pattern 1: Dual-Mode Property Access
During Phase 2 transition, support both old and new access patterns:

```c
// Temporary dual support
#ifdef USE_NEW_PROPERTY_SYSTEM
    #define GET_MVIR(g) GALAXY_GET_MVIR(g)
#else
    #define GET_MVIR(g) ((g)->Mvir)
#endif
```

### Pattern 2: Configuration Compatibility Layer
During Phase 4, read both formats transparently:

```c
config_t* read_config(const char* filename) {
    if (has_json_extension(filename)) {
        return read_json_config(filename);
    } else {
        return convert_par_to_config(filename);
    }
}
```

### Pattern 3: Module Gradual Extraction
During Phase 5, use forwarding functions:

```c
// In core code
void calculate_cooling(galaxy* g) {
    #ifdef MODULAR_PHYSICS
        cooling_module->execute(g);
    #else
        model_cooling_heating(g);  // Legacy function
    #endif
}
```

---

## Appendix C: Validation Checkpoints

### Scientific Validation Matrix

| Phase | Validation Test | Expected Result | Critical? |
|-------|----------------|-----------------|-----------|
| 1 | Binary comparison | Identical to Makefile build | Yes |
| 2 | Property values | Bit-exact with legacy | Yes |
| 3 | Memory usage | No increase vs legacy | No |
| 4 | Parameter parsing | Same values loaded | Yes |
| 5 | Module execution order | Same as monolithic | Yes |
| 6 | Output file content | Identical values | Yes |
| 7 | Full simulation | Matches reference | Yes |

### Performance Benchmarks

Each phase must maintain performance within these bounds:
- **Initialization**: < 5% slower than legacy
- **Tree Processing**: < 10% slower than legacy  
- **Galaxy Evolution**: < 5% slower than legacy
- **I/O Operations**: < 10% slower than legacy
- **Total Runtime**: < 10% slower than legacy

---

## Appendix D: Task Estimation Confidence

### Estimation Methodology
- **Low Complexity**: Well-understood, clear implementation path (1 session)
- **Moderate Complexity**: Some unknowns, may need iteration (2 sessions)
- **High Complexity**: Significant unknowns, likely needs research (3 sessions)

### Confidence Adjustments
- Tasks with external dependencies: +50% time buffer
- Tasks modifying core algorithms: +100% time buffer
- Tasks with platform-specific code: +50% time buffer

### Historical Adjustment Factors
Based on similar refactoring projects:
- First phase typically takes 20% longer than estimated
- Middle phases benefit from momentum (on-target)
- Final validation phase often needs 50% more time

---

## Appendix E: Module Development Template

For consistency, all physics modules should follow this template:

```c
// module_[name].c
#include "module_interface.h"

// Module metadata
static const char* dependencies[] = {"core", "properties", NULL};
static const module_metadata_t metadata = {
    .name = "module_name",
    .version = "1.0.0",
    .author = "Developer Name",
    .description = "Module purpose"
};

// Module implementation
static int module_init(const config_t* config) {
    // Initialize module state
    return SUCCESS;
}

static int module_execute(pipeline_context_t* ctx) {
    // Perform physics calculations
    return SUCCESS;
}

// Self-registration
static void __attribute__((constructor)) register_module(void) {
    static module_info_t module = {
        .metadata = &metadata,
        .dependencies = dependencies,
        .init = module_init,
        .execute = module_execute,
    };
    module_register(&module);
}
```

This ensures consistent structure across all modules and simplifies review and maintenance.