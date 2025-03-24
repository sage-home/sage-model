<!-- Purpose: Record technical decisions -->
<!-- Update Rules:
- Add new milestones at the bottom
- 100-word limit per entry! 
- Include:
  • Decision date and phase identifier
  • Rationale
  • Impact assessment
-->

2025-03-17: Implement Evolution Context Structure
- Rationale: The context structure encapsulates all state needed during galaxy evolution in a single object, reducing global state while maintaining performance.
- Impact: Cleaner function signatures, improved testability, and preparation for the plugin system. Makes the code easier to reason about by explicitly showing data dependencies.

2025-03-18: Directory Structure Reorganization
- Rationale: Organizing by core/physics/io creates a clear separation of concerns while maintaining the conceptual model of the simulation.
- Impact: Improved code navigation, better isolation of changes, and clearer dependencies between components. Supports future modularization efforts.

2025-03-19: Remove GSL Dependencies
- Rationale: GSL was only used for a single numerical integration that could be easily replaced with a simple implementation. Removing it reduces external dependencies.
- Impact: Simplified build process, reduced installation requirements, and elimination of potential compatibility issues. Small performance impact on integration accuracy, but negligible for overall simulation results.

2025-03-20: Adopt End-to-End Testing Approach
- Rationale: End-to-end tests comparing against reference outputs ensure scientific validity is preserved during refactoring, which is the primary concern.
- Impact: Greater confidence in refactoring changes, ability to detect regressions quickly, and documentation of expected behavior. Provides a safety net for more aggressive architectural changes.

2025-03-21: Prioritize Enhanced Error Logging Over Unit Testing
- Rationale: For scientific code, end-to-end validation against known benchmarks is more critical than unit testing. Enhanced error logging and runtime assertions provide most of the benefits of unit tests with less overhead, while maintaining focus on scientific correctness.
- Impact: Simplifies development process while still ensuring code quality. Allows faster progress on core refactoring goals. Defers formal unit testing until Phase 2 when module interfaces become more critical. Will require implementation of robust logging and assertion mechanisms.

2025-03-24: [Phase 1.3] Implement Makefile-Integrated Logging System
- Rationale: Using the existing VERBOSE flag from the Makefile provides a clean integration point for logging without requiring parameter file modifications, simplifying adoption and reducing potential issues.
- Impact: Enhanced error diagnostics while maintaining compatibility with existing build process. Enables context-specific logging with minimal code changes. Provides mechanism for more detailed reporting during debugging.

2025-03-24: [Phase 1.3] Standardize Verbosity Output Structure
- Rationale: Clean, consistent output regardless of verbose mode improves user experience. Always showing parameters and progress provides essential information while controlling detail level.
- Impact: Better usability for both normal users and developers. Clearer distinction between basic output and debug information. Preserves core functionality and test compatibility.

2025-03-25: [Phase 1.3] Focus on Numerical Validation for Parameters
- Rationale: Scientific freedom requires allowing extreme parameter values that might be invalid in normal scenarios. Our approach validates numerical properties (no NaN/Infinity) and internal consistency without limiting scientific exploration.
- Impact: Maintains flexibility for researchers while catching actual bugs and implementation errors. Prevents crashes from invalid numerical states while preserving the ability to run experimental simulations.

2025-03-26: [Phase 1.3] Defer Performance Benchmarking to Phase 6
- Rationale: With significant code changes still ahead in the refactoring process, implementing benchmarks now would provide limited value as they would quickly become outdated. More meaningful performance analysis can be conducted when the codebase structure is stabilized.
- Impact: Allows focus on completing the core architecture work without diverting resources to premature optimization. Will use a code profiler on the finalized structure rather than embedding benchmarking points in evolving code for more accurate performance insights.

2025-03-26: [Phase 1.3] Skip Enhanced End-to-End Test Diagnostics
- Rationale: The current combination of enhanced error logging, binary comparison through sagediff.py, knowledge of recent changes made, and starting from a known-good state provides sufficient debugging capabilities for current needs.
- Impact: Resources can be focused on completing core documentation tasks rather than expanding test infrastructure. The existing approach of using enhanced logging within SAGE in conjunction with the test_sage.sh verification is adequate for identifying and resolving issues during refactoring.

2025-03-27: [Phase 1.4] Use Existing Documents for Architecture Documentation
- Rationale: The comprehensive sage-refactoring-plan.md already provides detailed architectural guidance including interface definitions, data structures, and implementation approaches. Creating separate architecture documentation would duplicate information and potentially become outdated during implementation.
- Impact: Avoids redundant documentation and focuses effort on actual implementation. The sage-refactoring-plan.md will serve as the authoritative architecture document, while project-state-log.md provides current component relationships, and inline code comments document specific implementations.

2025-03-27: [Phase 1.4] Defer Detailed Developer Guide
- Rationale: A comprehensive developer guide would be more valuable after implementing the plugin architecture, as it would contain concrete examples and actual interface implementations rather than theoretical designs that might change during implementation.
- Impact: Postpones detailed developer documentation to when it will provide maximum value with minimal revisions. Basic documentation needs are still met through inline comments, architectural documentation in sage-refactoring-plan.md, and the existing README.
