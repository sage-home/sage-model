<!-- Purpose: Record technical decisions -->
<!-- Update Rules:
- Update from the bottom only!
- 100-word limit per entry! 
- Include:
  • Today's date and phase identifier
  • Rationale
  • Impact assessment
-->

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
