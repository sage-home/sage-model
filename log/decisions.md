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

*Recent critical decisions - historical decisions archived in `archive/decisions-phase*.md`*

2025-09-11: [Phase 2.2] Python-based property header code generation system
- **Decision**: Implemented YAML-to-C code generation with BUILD_BUG_OR_ZERO compile-time assertions
- **Rationale**: Enables metadata-driven property system with type safety and runtime modularity while maintaining compatibility
- **Impact**: Foundation for core/physics separation; enables future dynamic module loading and optimized memory layouts

2025-09-11: [Phase 2.2] CMake WARNING to STATUS message conversion for dependency resolution  
- **Decision**: Changed cJSON dependency messages from WARNING to STATUS level
- **Rationale**: Dependency resolution is informational, not error condition; reduces noise in build output
- **Impact**: Cleaner build experience; maintains proper severity levels for actual errors vs. status updates

2025-09-15: [Phase 2A] Master Plan Restructuring - Architecture-First Development
- **Decision**: Rewound to commit 2395ab2 and restructured master plan to implement Core/Physics Separation (Phase 2A) before property migration
- **Rationale**: Legacy plan violated Principle 1 (Physics-Agnostic Core) by applying properties within monolithic architecture; identified direct physics calls in core requiring immediate fix
- **Impact**: Establishes architectural compliance foundation; prevents technical debt accumulation; enables all subsequent modular development on sound principles


2025-09-15: [Task 2A.1] Constructor-based Module Auto-Registration Pattern
- **Decision**: Implemented auto-registration using constructor attributes rather than explicit registration calls
- **Rationale**: Eliminates manual registration overhead, ensures modules are always available when library loads, follows C library patterns
- **Impact**: Simplified module development workflow; robust module discovery; foundation for future dynamic loading systems

2025-09-18: [Phase 2A] Property Generation Script Relocation to src/scripts/
- **Decision**: Moved generate_property_headers.py from scripts/ to src/scripts/ directory
- **Rationale**: Property generation is integral to source compilation, not an external tool; src/scripts/ better reflects its code-generation role versus external utilities
- **Impact**: Cleaner project organization; aligns with source-centric architecture; maintains schema/ separation while recognizing code generation as core infrastructure

2025-09-18: [Phase 2A] CORE_PROP_* Property Naming Convention
- **Decision**: Enhanced branch will use CORE_PROP_* naming for core property access instead of GALAXY_PROP_* (used in refactor branch)
- **Rationale**: GALAXY_PROP_* is architecturally misleading since galaxy properties include both core AND physics properties, but core code should only access core properties
- **Impact**: Clearer architectural boundaries; prevents core code from accidentally accessing physics properties; improves code readability and maintains Principle 1 compliance
