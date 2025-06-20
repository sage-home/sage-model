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
