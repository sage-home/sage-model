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
<!-- New decisions for Phase 4 will be added here -->
