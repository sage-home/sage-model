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
