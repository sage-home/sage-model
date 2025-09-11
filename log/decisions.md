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

2025-XX-XX: [Phase 1.X] This is an example entry that should be followed
- **Decision**: Brief description of the decision made
- **Rationale**: Why this decision was made (technical/scientific reasons)
- **Impact**: Effect on current and future development

2025-09-11: [Phase 2.2] Python-based property header code generation system
- **Decision**: Implemented YAML-to-C code generation with BUILD_BUG_OR_ZERO compile-time assertions
- **Rationale**: Enables metadata-driven property system with type safety and runtime modularity while maintaining compatibility
- **Impact**: Foundation for core/physics separation; enables future dynamic module loading and optimized memory layouts

2025-09-11: [Phase 2.2] CMake WARNING to STATUS message conversion for dependency resolution  
- **Decision**: Changed cJSON dependency messages from WARNING to STATUS level
- **Rationale**: Dependency resolution is informational, not error condition; reduces noise in build output
- **Impact**: Cleaner build experience; maintains proper severity levels for actual errors vs. status updates
