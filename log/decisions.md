<!-- Purpose: Record critical technical decisions -->
<!-- Update Rules:
- Append new entries to the EOF (use `cat << EOF >> ...etc`)!
- Focus on KEY decisions that impact current and upcoming development
- Only include decisions that are NOT covered in architecture.md
- 100-word limit per entry! 
- Include:
  • Today's date and phase identifier
  • Rationale
  • Impact assessment
-->

# Critical Architectural Decisions

*Recent critical decisions - historical decisions archived in `archive/decisions-current.md`*

2025-05-27: [Phase 5.2.G] Physics-Agnostic Merger Architecture Decision
- **Decision**: Implemented merger event handling using core processor + configurable physics handlers via module_invoke, rather than direct physics module pipeline execution
- **Rationale**: Enables runtime configuration of merger physics, better testability, and cleaner separation of event triggering from physics implementation
- **Impact**: Merger physics now fully configurable via parameters; core completely agnostic to merger implementation details; enables multiple merger physics modules simultaneously
- **Alternative Rejected**: Direct POST phase execution by physics modules would have coupled event processing to specific module implementations

2025-06-02: [Phase 5.2.G] Strict Duplicate Registration Prevention Policy
- Rationale: Investigation revealed that the module system was incorrectly allowing duplicate function registrations by silently updating existing registrations instead of failing. This "silent recovery" approach hides bugs and violates the "fail fast" principle essential for robust software design. Both module registration and function registration should explicitly prevent duplicates.
- Impact: Changed `module_register_function()` to return `MODULE_STATUS_ERROR` for duplicate function names instead of updating existing registrations. This ensures clean architectural boundaries, helps detect development errors early, and aligns with the modular architecture goal of explicit, controlled interactions. Tests updated to use proper system APIs (`module_initialize()`) rather than bypassing built-in protections.


2025-06-05: [Phase 5.3] I/O Interface Migration Strategy
- Rationale: Chose incremental migration approach with graceful fallback to legacy functions rather than big-bang replacement of all tree reading infrastructure. This allows validation of each format individually while maintaining scientific accuracy and system stability during the transition.
- Impact: LHalo HDF5 format successfully migrated to I/O interface with legacy header dependencies eliminated. Core system (core_io_tree.c) tries I/O interface first, gracefully falls back to legacy functions for non-migrated formats. Enables format-agnostic development while preserving backward compatibility.

2025-06-07: [Architecture] Parameters.yaml Metadata-Driven System Implementation
- Rationale: Following the same architectural pattern as properties.yaml, implemented parameters.yaml to eliminate core-physics separation violations in core_read_parameter_file.c where hardcoded physics parameter names violated SAGE's modular design principles. Modules shouldn't own parameters - like properties.yaml, we need a master parameter list from which modules can draw, ensuring future-proof extensibility.
- Impact: Eliminates 200+ lines of hardcoded parameter arrays, provides type-safe parameter accessors with validation, maintains existing *.par file format compatibility, and uses metadata-driven code generation. Auto-generates parameter system files during build process, enabling runtime-configurable physics with full core-physics separation compliance.

2025-06-08: [Project Management] Log File Renaming for Developer Efficiency
- Rationale: The original log file names (phase-tracker-log.md, recent-progress-log.md, project-state-log.md, decision-log.md) were becoming cumbersome to type frequently during development work. Shorter, clear names improve developer productivity while maintaining semantic clarity about file purposes.
- Impact: Renamed to phase.md, progress.md, architecture.md, decisions.md respectively. Updated all documentation references and maintained clear purpose statements in file headers. Historical logs preserved in archive/ directory with clear transition documentation for continuity.

2025-06-09: [Memory Debugging] Comprehensive Memory Corruption Detection Strategy
- Rationale: Implemented systematic debugging approach for segfault investigation including: 1) Enhanced validation macros to detect corruption patterns, 2) Focused debugging on reallocation points, 3) Root cause analysis of memory layout issues. Fixed critical bug in galaxy_array_expand() that was writing to uninitialized memory by nullifying properties for entire capacity instead of just valid galaxies.
- Impact: Provides comprehensive memory corruption detection infrastructure, fixes galaxy array expansion bug, implements safe deep copying, and validates systematic debugging methodology for complex memory issues. While segfaults persist (requiring further investigation), defensive programming enhancements provide robust debugging foundation for future memory issue resolution.

2025-06-09: [Critical Bug Fix] Scientific Data Integrity Protection Decision
- Rationale: During initial memory corruption fix implementation, accidentally introduced dangerous "graceful degradation" that set corrupted properties to NULL and used default values, effectively masking scientific data corruption to make code run. User immediately identified this as unacceptable scientific practice: "you cannot do this! You are changing scientific data just to make the code run!"
- Impact: Reverted all data corruption masking. System now fails hard (SIGTRAP) when corruption is detected, preserving scientific data integrity. Corruption detection macros provide detailed debugging information while ensuring invalid scientific results never propagate. Validates principle that scientific software must fail rather than produce invalid data.