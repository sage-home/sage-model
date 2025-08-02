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


2025-06-17: [Architecture] Dual Property System Elimination Strategy
- **Decision**: Eliminated dual property system by removing all direct GALAXY struct fields, converting to GALAXY_PROP_* macros as single source of truth
- **Rationale**: Forces complete property system adoption, eliminating synchronization bugs and architectural violations while maintaining performance
- **Impact**: Achieves clean architecture with zero legacy debt. Both physics-free (27 properties) and full-physics (71 properties) modes work correctly

2025-06-20: [Documentation] Professional Documentation Consolidation Strategy
- **Decision**: Consolidated SAGE's 30+ fragmented documentation files into 20 focused documents with 4 comprehensive guides
- **Rationale**: Eliminated significant duplication, poor organization, and navigation issues hindering professional scientific use
- **Impact**: Reduced maintenance burden, established clear ownership boundaries, achieved professional documentation standards with role-based navigation

2025-08-01: [Tree Conversion] SAGE Tree Conversion Plan Strategic Architecture Decision
- **Decision**: Adopted hybrid conversion strategy combining legacy scientific algorithms (control flow, property calculations) with modern infrastructure (property system, memory safety, modular architecture)
- **Rationale**: Avoided wholesale legacy adoption that would lose 5 phases of architectural improvements while preserving scientifically validated tree-based processing algorithms
- **Impact**: Enabled single tree-based processing mode while maintaining property system, memory safety, core-physics separation, and module system benefits

2025-08-01: [Tree Conversion] Legacy Code Integration Strategy  
- **Decision**: Use existing modernized `copy_galaxies_from_progenitors()` function as foundation instead of incomplete tree-based functions (`inherit_galaxies_with_orphans()`, `update_galaxy_for_new_halo()`)
- **Rationale**: Modern snapshot-based function already contained complete legacy scientific algorithms with property system integration and memory safety enhancements
- **Impact**: Avoided recreating proven scientific calculations while gaining modern architectural benefits including enhanced validation and fail-hard behavior

2025-08-02: [Tree Conversion] Dual Processing System Elimination Decision
- **Decision**: Completely removed ProcessingMode parameter and all dual processing infrastructure (tree_*.c/h files, snapshot indexing, mode branching)
- **Rationale**: Dual system complexity violated core conversion objective and created maintenance burden without scientific benefit
- **Impact**: Achieved single unified tree-based processing architecture, eliminated 18 obsolete files, simplified parameter system, reduced code complexity significantly
EOF < /dev/null