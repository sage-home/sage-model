<!-- Purpose: Record last 7 technical decisions -->
<!-- Update Rules:
- FIFO queue
- 100-word limit per entry! 
- Include:
  • Decision date and phase identifier
  • Rationale
  • Impact assessment
  • Alternatives considered 
-->

2025-03-17: Implement Evolution Context Structure
- Rationale: The context structure encapsulates all state needed during galaxy evolution in a single object, reducing global state while maintaining performance.
- Impact: Cleaner function signatures, improved testability, and preparation for the plugin system. Makes the code easier to reason about by explicitly showing data dependencies.
- Alternatives considered: Thread-local storage, Function parameter passing, Global variables

2025-03-18: Directory Structure Reorganization
- Rationale: Organizing by core/physics/io creates a clear separation of concerns while maintaining the conceptual model of the simulation.
- Impact: Improved code navigation, better isolation of changes, and clearer dependencies between components. Supports future modularization efforts.
- Alternatives considered: Flat structure, Module-based packages, Feature-based organization

2025-03-19: Remove GSL Dependencies
- Rationale: GSL was only used for a single numerical integration that could be easily replaced with a simple implementation. Removing it reduces external dependencies.
- Impact: Simplified build process, reduced installation requirements, and elimination of potential compatibility issues. Small performance impact on integration accuracy, but negligible for overall simulation results.
- Alternatives considered: Keep GSL as optional, Replace with another library, Create abstraction layer

2025-03-20: Adopt End-to-End Testing Approach
- Rationale: End-to-end tests comparing against reference outputs ensure scientific validity is preserved during refactoring, which is the primary concern.
- Impact: Greater confidence in refactoring changes, ability to detect regressions quickly, and documentation of expected behavior. Provides a safety net for more aggressive architectural changes.
- Alternatives considered: Unit testing only, Integration testing, Manual verification

2025-03-21: Prioritize Enhanced Error Logging Over Unit Testing
- Rationale: For scientific code, end-to-end validation against known benchmarks is more critical than unit testing. Enhanced error logging and runtime assertions provide most of the benefits of unit tests with less overhead, while maintaining focus on scientific correctness.
- Impact: Simplifies development process while still ensuring code quality. Allows faster progress on core refactoring goals. Defers formal unit testing until Phase 2 when module interfaces become more critical. Will require implementation of robust logging and assertion mechanisms.
- Alternatives considered: Comprehensive unit testing framework, Hybrid approach with minimal unit tests

2025-03-24: [Phase 1.3] Implement Makefile-Integrated Logging System
- Rationale: Using the existing VERBOSE flag from the Makefile provides a clean integration point for logging without requiring parameter file modifications, simplifying adoption and reducing potential issues.
- Impact: Enhanced error diagnostics while maintaining compatibility with existing build process. Enables context-specific logging with minimal code changes. Provides mechanism for more detailed reporting during debugging.
- Alternatives considered: Parameter file configuration, Command-line arguments, Separate configuration file

2025-03-24: [Phase 1.3] Standardize Verbosity Output Structure
- Rationale: Clean, consistent output regardless of verbose mode improves user experience. Always showing parameters and progress provides essential information while controlling detail level.
- Impact: Better usability for both normal users and developers. Clearer distinction between basic output and debug information. Preserves core functionality and test compatibility.
- Alternatives considered: Parameter-based verbosity control, Multiple verbosity levels, No changes to current output structure
