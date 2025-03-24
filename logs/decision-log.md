<!-- Purpose: Record technical decisions -->
<!-- Update Rules:
- Add new milestones at the bottom
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
