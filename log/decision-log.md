<!-- Purpose: Record critical technical decisions -->
<!-- Update Rules:
- Append new entries to the EOF (use `cat << EOF >> ...etc`)!
- Focus on KEY decisions that impact current and upcoming development
- Only include decisions that are NOT covered in project-state-log.md
- 100-word limit per entry! 
- Include:
  • Today's date and phase identifier
  • Rationale
  • Impact assessment
-->

# Critical Architectural Decisions

## Module System Design

2025-03-24: [Phase 2.2] Use Opaque Pointers for Galaxy Extensions
- Rationale: Using void pointers in the galaxy extension mechanism provides maximum flexibility for module developers while maintaining a clean API. This approach allows modules to define their own data structures without changing the core GALAXY struct, supporting backward compatibility.
- Impact: Simplifies the extension mechanism but requires careful memory management and type safety through accessor functions and macros. This design choice enables modules to add arbitrary data to galaxies without affecting existing code or creating version compatibility issues.

2025-03-24: [Phase 2.2] Implement Extension Data Deep Copy
- Rationale: When copying a galaxy, all extension data must be deep copied rather than just copying pointers. This ensures that each galaxy has its own independent set of extension data, preventing memory corruption and unexpected behavior when one galaxy's properties are modified.
- Impact: Slightly increased overhead during galaxy copying operations, but guarantees proper encapsulation and avoids shared references to extension data. This approach maintains consistency with the rest of the SAGE model, where galaxies are independent entities.

2025-03-25: [Phase 2.4] Use Semantic Versioning for Module Compatibility
- Rationale: Implementing semantic versioning (major.minor.patch) allows precise control over module compatibility requirements. This standardized versioning approach enables modules to specify exact version constraints for dependencies, supporting both strict compatibility (exact version match) and flexible constraints (version ranges).
- Impact: Gives module developers fine-grained control over compatibility requirements while providing a clear upgrade path for users. Module consumers can understand which updates are safe to apply (patch), which add functionality (minor), and which may break compatibility (major).

2025-03-27: [Phase 2] Module Callback System Addition
- Rationale: The original SAGE implementation has tightly coupled physics modules where one module directly calls others (e.g., mergers triggering star formation). A pure pipeline architecture would break these dependencies, compromising scientific accuracy. The Module Callback System preserves these critical interactions while maintaining a clean architecture.
- Impact: Adds a new Phase 2.7 to the refactoring plan, extends the module interface to support dependencies, and creates a standardized invocation mechanism between modules. Preserves the original physics calculation sequences while enabling modular replacement of individual physics components.

## I/O System Improvements

2025-03-28: [Phase 3] Enhanced I/O System Implementation
- Rationale: After code analysis, we identified several critical improvements for the I/O system: HDF5 resource leaks, lack of endianness handling, fixed buffer sizes, and inefficient memory allocation. These issues could impact stability, cross-platform compatibility, and performance.
- Impact: Enhanced I/O interface design with explicit resource tracking for HDF5, endianness handling for binary formats, configurable buffer sizes, and optimized allocation strategies. These enhancements improve robustness, cross-platform compatibility, and performance while supporting plugin-specific data.

2025-04-09: [Phase 3.4] Dynamic Limits Review and Deferral
- Rationale: After analyzing the codebase, we determined that the current value of ABSOLUTEMAXSNAPS (1000) provides ample headroom for typical simulations. Implementing dynamic array sizing for snapshot arrays would require non-trivial changes across multiple files while offering limited immediate benefit.
- Impact: Component 3.4 (Review Dynamic Limits) has been moved to Phase 6.1 (Memory Layout Enhancement) where it aligns better with other memory optimization tasks. This decision allows us to maintain momentum and focus on the more critical plugin infrastructure work without compromising scientific capabilities.

## Evolution Pipeline Refactoring

2025-04-16: [Phase 5.1] Incremental Evolution Pipeline Transformation
- Rationale: The evolution pipeline is central to SAGE's scientific calculations, so a big-bang replacement would be too risky. Instead, we've implemented a hybrid approach where the pipeline system is gradually introduced in parallel to the existing monolithic `evolve_galaxies()` function.
- Impact: Allows continuous validation against reference outputs, minimizes risk to scientific accuracy, and enables us to validate each physics module independently as it's migrated. This reduces risk while maintaining momentum toward the modular architecture.

2025-04-16: [Phase 5.1] Merger Event Queue Approach for Galaxy Evolution
- Rationale: The current `evolve_galaxies()` implementation uses separate loops for physics processing and merger handling to ensure all galaxies see the same pre-merger state. We've implemented an event queue approach that maintains this scientific property while simplifying code structure and aligning with our event-driven pipeline architecture.
- Impact: Provides a cleaner integration path for the pipeline system while preserving scientific consistency. This approach allows us to validate the event-based pattern independently before combining with other architectural changes.

2025-04-17: [Phase 5.1] Pipeline Phase Architecture for Evolution Loop
- Rationale: Analysis of physics processing revealed two distinct calculation scopes: halo-level calculations (like infall) that happen outside the galaxy loop, and galaxy-level calculations (like cooling) that happen for each galaxy. A pure modular pipeline requires a structured way to handle these different scopes.
- Impact: Introduction of execution phases (HALO, GALAXY, POST, FINAL) in the pipeline architecture addresses this critical design need. Modules can now declare which phases they participate in, preserving the scientific model while maintaining modularity.

2025-04-23: [Phase 5.1] Using Pipeline Context for Module Callbacks
- Rationale: Module callbacks required tracking multiple types of context: call stack for circular dependency detection, error propagation between modules, and execution context for the current pipeline. We implemented a unified approach where the pipeline context carries callback information, allowing modules to correctly track dependencies.
- Impact: This design ensures consistent behavior between direct module invocation and pipeline execution, maintains clear error propagation paths, and simplifies integration of callbacks into the pipeline architecture, preserving physics interdependencies while maintaining modularity.

## Property System & Physics Separation

2025-05-01: [Phase 5.2] Adoption of Properties Module Architecture
- Rationale: After evaluating the challenges of multiple physics modules accessing shared galaxy state, we recognized that our current approach had fundamental limitations. The dual implementation approach increased complexity, and module-specific property registration created implicit dependencies. The Properties Module architecture solves these issues through central property definition and complete core-physics decoupling.
- Impact: This represents a significant architectural improvement that simplifies future development while achieving true runtime functional modularity. All physics state is defined in a single source of truth (`properties.yaml`), core infrastructure becomes physics-agnostic, and all modules have equal access to shared properties.

2025-05-01: [Phase 5.2.B] Support for Dynamic Array Properties
- Rationale: Certain physics properties like star formation histories require dynamically-sized arrays that depend on runtime parameters. Static arrays with fixed sizes limit flexibility, while dynamic arrays enable more realistic modeling without arbitrary constraints. HDF5 already has excellent support for variable-length arrays, facilitating implementation.
- Impact: Enhanced property system with support for runtime-sized arrays adds some complexity to memory management and accessors but significantly improves modeling capabilities. This approach eliminates hardcoded limits like STEPS that have constrained the model.

2025-05-01: [Phase 5.2.E] Standardize on HDF5 Output Format
- Rationale: Supporting multiple output formats increases complexity and maintenance burden. HDF5 provides superior capabilities for scientific data with built-in compression, chunking, metadata support, and native handling of complex data types including variable-length arrays. Binary format lacks many of these features and requires custom serialization code.
- Impact: Eliminating binary output format support simplifies the codebase by removing one I/O handler path. Focusing on HDF5 enables better leveraging of its advanced features like hierarchical organization and self-describing datasets, reducing the testing matrix.

2025-05-09: [Phase 5.2.F] Complete Physics-Core Separation Approach
- Rationale: We discovered memory issues caused by running dual physics systems in parallel. Rather than migrating physics components incrementally within the existing structure, we've decided to implement a truly physics-agnostic core first that can run with no physics at all. This achieves total separation between infrastructure and physics and resolves memory issues.
- Impact: This approach allows us to remove all physics dependencies from the core, verify the pipeline runs with an empty properties.yaml, then add physics modules one by one. Each physics component becomes a pure add-on to a functioning core rather than an integral component, implementing true "Runtime Functional Modularity."

## Core-Physics Property Separation (May 2025)

The complete separation between core infrastructure and physics has been implemented:

1.  **Core Properties (`properties.yaml` with `is_core: true`)**:
    *   Direct field access via `GALAXY_PROP_*` macros is appropriate for core code when accessing properties defined in `properties.yaml`.
    *   Managed by core infrastructure and always expected to be present.

2.  **Physics Properties (`properties.yaml` with `is_core: false` or module-specific definitions)**:
    *   MUST be accessed via generic property system functions (e.g., `get_float_property()`, `get_cached_property_id()`) from `core_property_utils.h`.
    *   May be present or absent depending on loaded physics modules.

3.  **Access rules**:
    *   Core code (e.g., in `src/core/`, `src/io/`) MUST NOT use `GALAXY_PROP_*` macros for physics properties. It must use the generic accessors.
    *   Physics modules SHOULD use generic accessors for physics properties (even their own) to promote consistency and facilitate inter-module interactions if ever needed, though they CAN use `GALAXY_PROP_*` for core properties.
    *   All code SHOULD use `get_property_by_id()` (or the typed versions like `get_float_property()`) for physics properties to ensure runtime adaptability.

This separation ensures that core infrastructure has zero compile-time or direct runtime knowledge of specific physics implementations or their associated properties, relying instead on a generic, runtime-discoverable property system.

2025-05-17: [Phase 5.2.F.3] Physics Files Reorganization
- Rationale: After reviewing the codebase, we identified several files that violate the core-physics separation principle. We moved example/template files and non-placeholder physics modules to the `ignore/physics/` directory, and reorganized legacy physics code to ensure a clean separation between core infrastructure and physics implementations.
- Impact: This cleanup strengthens the core-physics separation by removing direct references to specific physics implementations from core code. The `core_pipeline_registry.c` file was updated to work with placeholder modules instead of hardcoded physics implementations, maintaining the principle that core code should have no direct knowledge of physics implementations.

2025-05-17: [Phase 5.2.F.3] Note on Transitional `physics_modules.h` File
- Rationale: The `src/physics/physics_modules.h` file is a transitional component designed to support the core-only build during the Core-Physics Separation phase. It provides a clean interface for placeholder modules while maintaining API compatibility with the original architecture during refactoring.
- Impact: This file is scheduled for removal/replacement after the completion of Phase 5.2.G (Physics Module Migration). In the final architecture, physics modules will register themselves independently through the module system with no need for this centralized include file. Clear documentation has been added to guide future developers.

2025-05-17: [Phase 5.2.F.3] Module Registration and Pipeline Creation Strategy
- Rationale: The pipeline registry code was directly referencing physics modules, violating core-physics separation. Additionally, the `pipeline_create_with_standard_modules()` function was combining default steps AND registered modules, causing test failures where 13 steps were created instead of the expected 2.
- Impact: Updated the pipeline registry to use self-registration of modules via factories, with proper deduplication logic to avoid duplicate modules. Improved capacity tracking using `MAX_MODULES` instead of `MAX_MODULE_FACTORIES` and added a warning message when capacity is exceeded. This ensures the core pipeline registry is completely decoupled from specific physics implementations.

2025-05-18: [Phase 5.2.F.3] Configuration-Driven Pipeline Creation
- Rationale: To fully realize core-physics separation, the pipeline creation process must be decoupled from specific physics implementations. The previous implementation hardcoded module activation, violating the principle that core infrastructure should have no knowledge of specific physics modules.
- Impact: Implementing configuration-driven pipeline creation enables users to define module combinations at runtime without code changes. The core infrastructure now reads the "modules.instances" array from JSON configuration to determine which modules to activate, with appropriate fallback to using all registered modules when no configuration is available. This completes a key aspect of core-physics separation.

2025-05-22: [Phase 5.2.F.3] Module Infrastructure Legacy Code Removal
- Rationale: After systematic investigation of module system components, determined that older core module files are completely unused legacy code with no external function calls or macro usage. However, core_module_callback, core_module_parameter, and core_module_error are actively used infrastructure providing call stack tracking, parameter management, and enhanced error handling respectively.
- Impact: Removes true legacy code while preserving essential infrastructure. Investigation revealed a sophisticated hybrid architecture with multiple layers of module support. This cleanup strengthens the core-physics separation by removing unused components while maintaining the robust infrastructure needed for current and future module development.

2025-05-23: [Phase 5.2.F.3] Evolution Diagnostics Core-Physics Separation Implementation
- Rationale: The evolution diagnostics system contained critical core-physics separation violations that fundamentally compromised the modular architecture goals. The system had hardcoded physics property knowledge in core infrastructure, violating the principle that core components should be physics-agnostic. Complete redesign was required to achieve true runtime functional modularity.
- Impact: Achieves complete core-physics separation for diagnostics system while maintaining all functionality. Core diagnostics now tracks only infrastructure metrics (galaxy counts, phase timing, pipeline performance) while providing framework for physics modules to register their own diagnostic metrics. Physics events properly separated into dedicated files, enabling future physics module communication without core dependencies.
