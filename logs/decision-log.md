<!-- Purpose: Record technical choices -->
<!-- Update Rules:
- Last 5 decisions only!
  • Decision date 
  • Alternatives considered 
  • Rationale
  • Impact assessment
-->

2025-03-16: Adopt Parameter Views Pattern
- Alternatives: Factory pattern, Strategy pattern, Global parameter registry
- Rationale: Parameter views provide a clean interface for modules while maintaining a single source of truth. They allow each module to access only the parameters it needs without exposing the entire parameter structure.
- Impact: Reduced coupling between physics modules, improved code clarity, and easier parameter management. Facilitates future plugin architecture by standardizing parameter access.

2025-03-17: Implement Evolution Context Structure
- Alternatives: Thread-local storage, Function parameter passing, Global variables
- Rationale: The context structure encapsulates all state needed during galaxy evolution in a single object, reducing global state while maintaining performance.
- Impact: Cleaner function signatures, improved testability, and preparation for the plugin system. Makes the code easier to reason about by explicitly showing data dependencies.

2025-03-18: Directory Structure Reorganization
- Alternatives: Flat structure, Module-based packages, Feature-based organization
- Rationale: Organizing by core/physics/io creates a clear separation of concerns while maintaining the conceptual model of the simulation.
- Impact: Improved code navigation, better isolation of changes, and clearer dependencies between components. Supports future modularization efforts.

2025-03-19: Remove GSL Dependencies
- Alternatives: Keep GSL as optional, Replace with another library, Create abstraction layer
- Rationale: GSL was only used for a single numerical integration that could be easily replaced with a simple implementation. Removing it reduces external dependencies.
- Impact: Simplified build process, reduced installation requirements, and elimination of potential compatibility issues. Small performance impact on integration accuracy, but negligible for overall simulation results.

2025-03-20: Adopt End-to-End Testing Approach
- Alternatives: Unit testing only, Integration testing, Manual verification
- Rationale: End-to-end tests comparing against reference outputs ensure scientific validity is preserved during refactoring, which is the primary concern.
- Impact: Greater confidence in refactoring changes, ability to detect regressions quickly, and documentation of expected behavior. Provides a safety net for more aggressive architectural changes.

2025-03-21: Prioritize Enhanced Error Logging Over Unit Testing
- Alternatives: Comprehensive unit testing framework, Hybrid approach with minimal unit tests
- Rationale: For scientific code, end-to-end validation against known benchmarks is more critical than unit testing. Enhanced error logging and runtime assertions provide most of the benefits of unit tests with less overhead, while maintaining focus on scientific correctness.
- Impact: Simplifies development process while still ensuring code quality. Allows faster progress on core refactoring goals. Defers formal unit testing until Phase 2 when module interfaces become more critical. Will require implementation of robust logging and assertion mechanisms.
