<!-- Purpose: Record technical decisions -->
<!-- Update Rules:
- Update from the bottom only!
- 100-word limit per entry! 
- Include:
  • Today's date and phase identifier
  • Rationale
  • Impact assessment
-->

2025-03-19: [Phase 1.2] Evolution Context Implementation
- Implemented evolution_context structure to encapsulate galaxy evolution state
- Replaced global variables with context-based parameters
- Added initialization and cleanup functions for proper lifecycle management
- Updated core functions to use context-based approach

2025-03-20: [Phase 1.1] GSL Dependency Removal
- Removed all GSL dependencies from codebase
- Replaced GSL's integration with simple trapezoidal rule implementation
- Updated the Makefile to remove GSL detection and flags
- Ensured identical results with non-GSL approach
- Modified tests to run without GSL dependency

2025-03-21: [Phase 1.3] Testing Strategy Decision
- Evaluated testing needs for scientific code refactoring
- Decided to prioritize enhanced error logging over unit testing framework
- Focused on improving end-to-end test diagnostics
- Planned implementation of runtime assertions for key components
- Adjusted project plan to reflect updated testing strategy

2025-03-21: [Phase 1.3] End-to-End Testing Framework Enhancement
- Completed the review and validation of the comprehensive end-to-end testing framework
- Confirmed framework provides robust test coverage with binary and HDF5 format support
- Validated that existing test_sage.sh provides download of test trees, environment setup, and output comparison
- Verified sagediff.py includes detailed comparison logic with both relative and absolute tolerances
- Determined no additional enhancement required for current project phase

2025-03-24: [Phase 1.3] Enhanced Error Logging System
- Implemented comprehensive error logging framework with severity levels (DEBUG to FATAL)
- Created context-aware logging for galaxy evolution
- Added color-coded terminal output and configurable formatting
- Integrated with the Makefile VERBOSE flag for level control
- Modified src/core/sage.c, core_build_model.c, core_logging.c, core_parameter_views.c, and core_init.c

2025-03-24: [Phase 1.3] Improved SAGE Verbosity Control
- Enhanced user experience by properly handling verbose vs. non-verbose output modes
- Modified logging system to always show parameter file info and progress bar
- Added clean separation between output sections with newlines
- Added "Finished" message upon completion in non-verbose mode
- Modified Makefile, core_logging.c, core_parameter_views.c, core_read_parameter_file.c, sage.c

2025-03-25: [Phase 1.3] Evolution Context Validation
- Implemented comprehensive validation for evolution_context structure
- Added checks for null pointers, array bounds, and numerical consistency
- Integrated validation in initialization and evolution pipeline
- Ensures context remains in a valid state throughout simulation
- Modified src/core/core_init.c, core_init.h, and core_build_model.c

2025-03-25: [Phase 1.3] Parameter Views Validation
- Implemented comprehensive validation for all parameter view types
- Added checks for numerical validity (NaN/Infinity detection)
- Added validation calls at parameter view initialization
- Improved error reporting with detailed diagnostics
- Modified src/core/core_parameter_views.c, core_parameter_views.h, and core_logging.h

2025-03-26: [Phase 1.3] Testing Enhancement Scope Adjustment
- Evaluated the necessity of enhancing end-to-end test diagnostics and adding benchmarking points
- Determined current testing approach with enhanced logging is sufficient for identifying issues
- Decided to skip additional test diagnostics enhancement and defer benchmarking to Phase 6
- Created a "Deferred Work" section in the refactoring plan to track postponed items
- Focused remaining Phase 1.3-1.4 efforts on documentation tasks

2025-03-27: [Phase 1.4] Documentation Strategy Decision
- Reviewed documentation requirements for Phase 1.4 completion
- Determined that sage-refactoring-plan.md provides comprehensive architecture documentation
- Concluded that project-state-log.md addresses component relationships adequately
- Decided to defer detailed developer guide until after plugin architecture implementation
- Marked Phase 1.4 docu

2025-03-24: [Phase 2.1] Implement Physics Module Interfaces Incrementally
- Rationale: Building and testing interfaces one physics module at a time (starting with cooling) allows for iterative refinement of the base interface design before committing to the full architecture. This approach minimizes rework and ensures each interface meets the specific needs of its physics domain.
- Impact: Enables early validation of the module interface approach with a concrete implementation. Cooling was chosen as the first module because it has clear boundaries and minimal dependencies on other physics processes, making it ideal for initial modularization.

2025-03-24: [Phase 2.2] Use Opaque Pointers for Galaxy Extensions
- Rationale: Using void pointers in the galaxy extension mechanism provides maximum flexibility for module developers while maintaining a clean API. This approach allows modules to define their own data structures without changing the core GALAXY struct, supporting backward compatibility.
- Impact: Simplifies the extension mechanism but requires careful memory management and type safety through accessor functions and macros. This design choice enables modules to add arbitrary data to galaxies without affecting existing code or creating version compatibility issues.

2025-03-24: [Phase 2.1] Maintain Backward Compatibility With Wrapper Functions
- Rationale: Implementing compatibility wrappers around existing physics functions allows us to introduce the module system incrementally without breaking existing code. These wrappers translate between the old function signatures and the new module-based approach.
- Impact: Enables phased migration to the new architecture while ensuring all tests continue to pass. Existing code can still call the legacy functions, while new code can interact with the more flexible plugin architecture.

2025-03-24: [Phase 2.2] Opt for Lazy Initialization of Galaxy Extensions
- Rationale: Allocating memory for extension data only when it's first accessed (rather than pre-allocating everything) significantly reduces memory usage, especially when most galaxies don't use all available extensions. This approach avoids memory waste while still providing a clean API.
- Impact: Reduces memory footprint with minimal performance impact since extension data is cached after first access. The implementation uses a bitmap to track which extensions are in use, enabling efficient memory management and copying between galaxies.

2025-03-24: [Phase 2.2] Implement Extension Data Deep Copy
- Rationale: When copying a galaxy, all extension data must be deep copied rather than just copying pointers. This ensures that each galaxy has its own independent set of extension data, preventing memory corruption and unexpected behavior when one galaxy's properties are modified.
- Impact: Slightly increased overhead during galaxy copying operations, but guarantees proper encapsulation and avoids shared references to extension data. This approach maintains consistency with the rest of the SAGE model, where galaxies are independent entities.

2025-03-25: [Phase 2.4] Use Semantic Versioning for Module Compatibility
- Rationale: Implementing semantic versioning (major.minor.patch) allows precise control over module compatibility requirements. This standardized versioning approach enables modules to specify exact version constraints for dependencies, supporting both strict compatibility (exact version match) and flexible constraints (version ranges).
- Impact: Gives module developers fine-grained control over compatibility requirements while providing a clear upgrade path for users. Module consumers can understand which updates are safe to apply (patch), which add functionality (minor), and which may break compatibility (major). This reduces integration issues when combining independently developed modules.

2025-03-25: [Phase 2.4] Implement Default Fallbacks for Module Configuration
- Rationale: Creating a system that gracefully handles missing configuration by applying sensible defaults improves robustness. Instead of failing when parameters are missing, the system continues with pre-defined defaults, making the module system more user-friendly and reducing configuration complexity for simple cases.
- Impact: Simplifies module usage by reducing the amount of required configuration, especially for standard scenarios. Modules remain functional even with minimal configuration, while still allowing detailed customization when needed. This approach balances flexibility with ease of use.

2025-03-26: [Phase 2.6] Use Hierarchical Parameter Organization
- Rationale: Organizing configuration parameters into logical groups (cosmology, physics, io, etc.) improves maintainability and makes the relationship between parameters clearer. This structure matches the codebase organization and makes it easier to identify and modify related parameters.
- Impact: Cleaner parameter organization, better type safety through struct usage, and more intuitive parameter access. The hierarchical structure also helps prevent naming collisions and makes parameter documentation more organized.

2025-03-26: [Phase 2.5-2.6] Mark All Pipeline Steps as Optional During Early Development
- Rationale: During Phase 2.5-2.6, only a small subset of the planned modules have been implemented. Marking all pipeline steps as optional allows for graceful handling of missing modules while preserving the pipeline architecture. This approach enables us to test the pipeline system even before all modules are available.
- Impact: Enables testing and validation of the pipeline structure without requiring all modules to be implemented first. Systems can use the pipeline when modules are available but fall back to traditional implementations when they are not. This facilitates incremental development and refactoring while maintaining a working codebase.

2025-03-26: [Phase 2.5-2.6] Implement Progressive Warning Reduction for Missing Modules
- Rationale: When running SAGE with many missing modules, log files can become flooded with redundant warnings. By showing only the first few warnings and then reducing log level to debug for subsequent messages, we maintain awareness of missing components without overwhelming the logs.
- Impact: Significantly cleaner log output that provides useful information without repetition. Developers still receive important warnings but can focus on unique issues. This approach balances informing users about missing modules while keeping logs manageable during development phases.

2025-03-27: [Phase 2.5-2.6] Implement Pipeline Fallback Mechanism with Traditional Physics Guarantee
- Rationale: Even though a DefaultCooling module exists, physics modules may not be fully migrated during refactoring, causing tests to fail when comparing against benchmark outputs. The solution is to use traditional physics implementations for ALL modules (including cooling) during migration to ensure consistent test behavior.
- Impact: This creates a reliable fallback path that guarantees scientific consistency during the migration period. The development team can test the pipeline architecture without being hindered by partial physics implementation. The traditional physics guarantee can be selectively disabled per module when testing specific module updates.

2025-03-27: [Phase 2] Module Callback System Addition
- Rationale: The original SAGE implementation has tightly coupled physics modules where one module directly calls others (e.g., mergers triggering star formation). A pure pipeline architecture would break these dependencies, compromising scientific accuracy. The Module Callback System preserves these critical interactions while maintaining a clean architecture.
- Impact: Adds a new Phase 2.7 to the refactoring plan, extends the module interface to support dependencies, and creates a standardized invocation mechanism between modules. Preserves the original physics calculation sequences while enabling modular replacement of individual physics components. Increases implementation complexity but ensures scientific consistency.

2025-03-28: [Phase 3] Pipeline Testing Strategy
- Rationale: Despite using traditional physics implementations, updating the pipeline architecture can change the execution order and timing of physics operations. Work should be implemented very carefully and incrementally keeping order and timing in mind, always checking against the benchmark output. 
- Impact: Can create subtle differences in galaxy evolution that cause benchmark test failures. Future work should be extremely careful and maintain awareness of such sensitivities, especially for architectural changes.

2025-03-28: [Phase 3] Enhanced I/O System Implementation
- Rationale: After code analysis, we identified several critical improvements for the I/O system: HDF5 resource leaks, lack of endianness handling, fixed buffer sizes, and inefficient memory allocation. These issues could impact stability, cross-platform compatibility, and performance.
- Impact: Enhanced I/O interface design with explicit resource tracking for HDF5, endianness handling for binary formats, configurable buffer sizes, and optimized allocation strategies. These enhancements will improve robustness, cross-platform compatibility, and performance without significant additional implementation effort.

2025-04-07: [Phase 3.2] Strategy for Unused Functions in I/O Implementation
- Rationale: Several functions introduced in the I/O interface implementation are currently unused or stubs (map_io_error_to_sage_error, log_io_error, flush_galaxy_buffer), leading to compiler warnings. These functions represent planned functionality for future phases, particularly HDF5 buffer flushing (Phase 3.3: Memory Optimization) and comprehensive error handling.
- Impact: We've decided to keep these functions in place with appropriate documentation comments rather than removing them. This provides a clear roadmap for future implementation while maintaining a clean architecture. The unused function warnings are acceptable as they document intended functionality without affecting runtime behavior.
