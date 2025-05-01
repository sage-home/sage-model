<!-- Purpose: Record critical technical decisions -->
<!-- Update Rules:
- Focus on KEY decisions that impact current and upcoming development
- Include only decisions that are NOT covered in project-state-log.md
- 100-word limit per entry! 
- Include:
  • Today's date and phase identifier
  • Rationale
  • Impact assessment
-->

# Critical Architectural Decisions

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

# Phase 3 Key Decisions

2025-03-28: [Phase 3] Enhanced I/O System Implementation
- Rationale: After code analysis, we identified several critical improvements for the I/O system: HDF5 resource leaks, lack of endianness handling, fixed buffer sizes, and inefficient memory allocation. These issues could impact stability, cross-platform compatibility, and performance.
- Impact: Enhanced I/O interface design with explicit resource tracking for HDF5, endianness handling for binary formats, configurable buffer sizes, and optimized allocation strategies. These enhancements improve robustness, cross-platform compatibility, and performance while supporting plugin-specific data.

2025-04-09: [Phase 3.4] Dynamic Limits Review and Deferral
- Rationale: After analyzing the codebase, we determined that the current value of ABSOLUTEMAXSNAPS (1000) provides ample headroom for typical simulations. Implementing dynamic array sizing for snapshot arrays would require non-trivial changes across multiple files while offering limited immediate benefit. This work doesn't block the plugin infrastructure development planned for Phase 4.
- Impact: Component 3.4 (Review Dynamic Limits) has been moved to Phase 6.1 (Memory Layout Enhancement) where it aligns better with other memory optimization tasks. This decision allows us to maintain momentum and focus on the more critical plugin infrastructure work without compromising the scientific capabilities of the code.

# Phase 4 Key Decisions

2025-04-11: [Phase 4.4] API Versioning Strategy for Modules
- Rationale: To ensure compatibility between modules and the core, we implemented a dual-constant approach with CORE_API_VERSION (current) and CORE_API_MIN_VERSION (minimum compatible). This allows us to enforce exact matches for critical compatibility while supporting backward compatibility for minor improvements, avoiding unnecessary breakage of existing modules.
- Impact: Provides a clear compatibility model for module developers and users. Modules targeting specific API versions can specify requirements, while the core can maintain compatibility with older modules. This ensures a smoother upgrade path as the system evolves over time while preserving critical compatibility requirements.

2025-04-16: [Phase 4.7] Code Maintenance Approach for Debug System
- Rationale: Identified code duplication in `core_module_debug.c` with identical formatting logic repeated in three places. Created a helper function to centralize this logic and improve maintainability without changing functionality.
- Impact: Establishes a pattern for incremental code quality improvements during the refactoring process. By addressing maintenance issues while implementing new features, we ensure the codebase remains sustainable long-term while meeting immediate functional requirements.

# Phase 5 Key Decisions

2025-04-16: [Phase 5.1] Incremental Evolution Pipeline Transformation
- Rationale: The evolution pipeline is central to SAGE's scientific calculations, so a big-bang replacement would be too risky. Instead, we'll implement a hybrid approach where the pipeline system is gradually introduced in parallel to the existing monolithic `evolve_galaxies()` function.
- Impact: Allows continuous validation against reference outputs, minimizes risk to scientific accuracy, and enables us to validate each physics module independently as it's migrated. This reduces risk while maintaining momentum toward the modular architecture.

2025-04-16: [Phase 5.1] Merger Event Queue Approach for Galaxy Evolution
- Rationale: The current `evolve_galaxies()` implementation uses separate loops for physics processing and merger handling to ensure all galaxies see the same pre-merger state. We've decided to implement an event queue approach that maintains this scientific property while simplifying code structure and aligning with our event-driven pipeline architecture.
- Impact: Provides a cleaner integration path for the pipeline system while preserving scientific consistency. This approach will be implemented before full pipeline integration, allowing us to validate the event-based pattern independently before combining with other architectural changes.

2025-04-16: [Phase 5.1] Careful Parameter Management for Merger Events
- Rationale: When implementing the merger event queue, we discovered that preserving exact numerical results required careful attention to parameter passing. Specifically, we needed to ensure the central galaxy index and merge destination IDs matched the original code exactly.
- Impact: Ensures that the event queue implementation produces results consistent with the previous approach, maintaining scientific accuracy while benefiting from the cleaner code structure. The successful implementation validates our approach to incremental refactoring that preserves scientific outcomes.

2025-04-17: [Phase 5.1] Pipeline Phase Architecture for Evolution Loop
- Rationale: Analysis of physics processing revealed two distinct calculation scopes: halo-level calculations (like infall) that happen outside the galaxy loop, and galaxy-level calculations (like cooling) that happen for each galaxy. A pure modular pipeline requires a structured way to handle these different scopes.
- Impact: Introduction of execution phases (HALO, GALAXY, POST, FINAL) in the pipeline architecture addresses this critical design need. Modules can now declare which phases they participate in, preserving the scientific model while maintaining modularity. This eliminates the need for special cases in the evolution loop while ensuring calculations happen in the correct order.

2025-04-23: [Phase 5.1] Using Pipeline Context for Module Callbacks
- Rationale: Module callbacks required tracking multiple types of context: call stack for circular dependency detection, error propagation between modules, and execution context for the current pipeline. We implemented a unified approach where the pipeline context carries callback information, allowing modules to correctly track dependencies while operating within the pipeline.
- Impact: This design ensures consistent behavior between direct module invocation and pipeline execution, maintains clear error propagation paths, and simplifies integration of callbacks into the pipeline architecture. This preserves the original physics model's interdependencies while maintaining modularity.

2025-04-24: [Phase 5.1.7] Evolution Diagnostics Integration Approach
- Rationale: Implementing evolution diagnostics as a standalone component rather than embedding the functionality directly in the evolution loop provides better maintainability and flexibility. We added the diagnostics structure to the evolution context to make metrics available consistently across the pipeline.
- Impact: This design enables diagnostic data collection with minimal impact on the core evolution logic. It provides a clear, consistent API for performance analysis and scientific validation while maintaining separation of concerns. The integration with the event system enables tracking dynamic event occurrences without modifying physics implementations.

2025-04-24: [Phase 5.1.7] Evolution Diagnostics Phase Representation
- Rationale: The pipeline system uses bit flags (1, 2, 4, 8) for pipeline phases, while the diagnostics system was using sequential integers (0-3) for array indices. Rather than refactoring the entire diagnostics system, we implemented a mapping function that translates between these representations.
- Impact: This approach addresses the immediate issue of invalid phase errors while minimizing code changes and risk. By deferring a more comprehensive refactoring, we maintain project momentum while recognizing the need for a more flexible phase representation in future work.

2025-04-24: [Phase 5.3] Integration Test Implementation Approach
- Rationale: Before beginning the module migration in Phase 5.2, we implemented targeted integration tests to verify the interaction between the pipeline system, event system, and diagnostics within the context of the refactored evolve_galaxies loop. This enables us to catch integration issues early rather than during complex physics module implementation.
- Impact: This approach improves code quality by creating a verifiable testing framework for the phase-based pipeline execution. By using mock modules with controlled phase support, we can verify correct module execution without the complexity of real physics implementations. This reduces debugging complexity in Phase 5.2 as the basic integration machinery is already validated.

2025-04-28: [Phase 5.2] Extension-Based Property Access Method Selection
- Rationale: When implementing the standard physics properties, we chose to use direct pointer access via `galaxy_extension_get_data()` rather than creating a new property-specific get/set function pattern. This approach is more memory-efficient (avoiding temporary double copies) and aligns with the existing extension system design.
- Impact: Provides a consistent pattern for all physics property access while maintaining performance. The implementation still maintains proper error handling and module context segregation. This helps developers understand the memory model more clearly while working with galaxy extensions.

2025-04-28: [Phase 5.2] Single Approach for Physics Modularization
- Rationale: Analysis revealed hard-coded physics in multiple core components, violating our goal of runtime functional modularity. Some of the code uses a dual approach, which we will need to update. Phase 5.2 is about moving away from the traditional physics fallback implementation. All traditional physics modules will need to be ported and be fully self contained within their respective .c/h physics module files.
- Impact: By the end of Phase 5, we will have achieved our primary refactoring goal: "Runtime Functional Modularity: Implement a dynamic plugin architecture allowing runtime insertion, replacement, reordering, or removal of physics modules without recompilation."

2025-04-28: [Phase 5.2] Enforcing Runtime Modularity in Core Systems
- Rationale: Analysis of core initialization code revealed direct dependencies on example implementation code (specifically example_event_handler.h/c). This violates our principle of runtime functional modularity, where core code should initialize infrastructure without embedding specific implementations.
- Impact: Modified core event system initialization to remove these direct dependencies, ensuring proper separation between infrastructure and implementation. Established pattern where event handlers are registered by modules during their initialization phase rather than in core code. This improves consistency with our module architecture and makes the core more maintainable.

2025-04-28: [Phase 5.2] Abandon Dual Implementation Approach for Full Modularity
- Rationale: After analyzing the challenges of maintaining dual implementation paths (modular and traditional), we recognized that fully embracing modularity would accelerate development and simplify the codebase. The transitional dual-path approach was causing increased complexity and hindering progress toward true runtime functional modularity.
- Impact: Created a comprehensive implementation plan to eliminate all legacy fallbacks from the core code. This will enable the framework to run without any physics modules loaded, ensuring complete separation between core infrastructure and physics implementations. The resulting architecture will provide a clean foundation for migrating remaining physics modules one at a time.

2025-04-30: [Phase 5.2] Directory Naming Convention for Migrated Physics Modules
- Rationale: The "old" directory name was misleading as it contained modules that had been actively migrated to the new system, not simply outdated code. Renaming to "migrated" better communicates the purpose of these files as successfully converted implementations.
- Impact: Improves code organization clarity for new developers by using more accurate terminology. This change maintains a clear distinction between unmigrated legacy code (in /legacy) and successfully migrated modules that have been extracted from the core codebase. No functional changes were required as there were no direct code references to this directory.

2025-05-01: [Phase 5.2] Adoption of Properties Module Architecture
- Rationale: After evaluating the challenges of multiple physics modules accessing and modifying shared galaxy state, we recognized that our current approach had fundamental limitations. The dual implementation approach increased complexity, and module-specific property registration created implicit dependencies. The Properties Module architecture solves these issues through central property definition, generated type-safe accessors, and complete decoupling of core from physics knowledge.
- Impact: This represents a significant architectural improvement that simplifies future development while achieving true runtime functional modularity. All physics state is defined in a single source of truth (`properties.yaml`), core infrastructure becomes physics-agnostic, and all modules have equal access to shared properties through a consistent mechanism. The build-time generation step is a reasonable trade-off for the substantial benefits to code clarity, maintainability, and extensibility.

2025-05-01: [Phase 5.2.B] Support for Dynamic Array Properties
- Rationale: Certain physics properties like star formation histories require dynamically-sized arrays that depend on runtime parameters (e.g., the number of output snapshots). Static arrays with fixed sizes limit flexibility, while dynamic arrays enable more realistic modeling without arbitrary constraints. HDF5 already has excellent support for variable-length arrays, facilitating implementation.
- Impact: Enhanced property system with support for runtime-sized arrays adds some complexity to memory management and accessors but significantly improves modeling capabilities. This approach eliminates hardcoded limits like STEPS that have constrained the model. The implementation requires careful memory management and bounds checking but is concentrated in infrastructure code, keeping physics modules straightforward.

2025-05-01: [Phase 5.2.E] Standardize on HDF5 Output Format
- Rationale: Supporting multiple output formats increases complexity and maintenance burden. HDF5 provides superior capabilities for scientific data with built-in compression, chunking, metadata support, and native handling of complex data types including variable-length arrays. Binary format lacks many of these features and requires custom serialization code.
- Impact: Eliminating binary output format support simplifies the codebase by removing one I/O handler path. Focusing on HDF5 enables better leveraging of its advanced features like hierarchical organization and self-describing datasets. This change allows more resources to be directed toward making the HDF5 output robust and feature-rich while reducing the testing matrix.