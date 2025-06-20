# SAGE Historical Archive

**Quick lookup for past decisions and changes** - Reference point for understanding project evolution.

## How to Use This Archive

- **Looking for a past decision?** → Search the Decision Timeline below or `archive/decisions-*` files
- **Need to see what changed in a milestone?** → Check Recent Changes Archive or `archive/progress-*` files
- **Understanding phase progression?** → Review Phase Summary
- **Want current state?** → Read the 4 main log files in parent directory

---

## Decision Timeline

*Consolidated from all past decision logs - searchable chronological record*

### Phase 1 Decisions
- **Core-Physics Separation Architecture**: Established complete independence between core infrastructure and physics modules using event-driven communication
- **Property System Foundation**: Implemented YAML-driven property generation with compile-time optimization for different physics configurations

### Phase 2 Decisions  
- **Module Registration System**: Moved from hardcoded to constructor-based automatic module registration with metadata
- **Pipeline Execution Model**: Adopted 4-phase execution (HALO→GALAXY→POST→FINAL) with bit-flag based phase identification

### Phase 3 Decisions
- **I/O Interface Abstraction**: Created unified I/O interface to support multiple tree formats while maintaining backward compatibility
- **Memory Management Strategy**: Implemented tree-scoped dynamic memory expansion to handle large simulation allocations

### Phase 4 Decisions
- **Property Access Control**: Replaced direct property macros with type-safe dispatcher functions to enforce core-physics separation
- **Event System Separation**: Separated physics events from core infrastructure events to maintain architectural independence

### Phase 5.2 Decisions (May-June 2025)
- **Physics-Agnostic Merger Architecture**: Implemented merger event handling using core processor + configurable physics handlers via module_invoke
- **Strict Duplicate Registration Prevention**: Changed module_register_function() to fail on duplicates instead of silent updates, enforcing "fail fast" principle
- **I/O Interface Migration Strategy**: Chose incremental migration with graceful fallback to legacy functions for stability during transition
- **Parameters.yaml Metadata System**: Implemented parameter system following properties.yaml pattern, eliminating 200+ lines of hardcoded arrays
- **Memory Debugging Infrastructure**: Fixed galaxy_array_expand() corruption bug, implemented fail-hard corruption protection preserving scientific data integrity
- **GalaxyArray Property Integration**: Replaced shallow copying with property-based deep copying to resolve memory management issues
- **Forest-Level Flag Management**: Fixed "multiple Type 0 galaxies" error by moving HaloFlag/DoneFlag initialization to forest-level instead of snapshot-level
- **Dual Property System Elimination**: Removed all direct GALAXY struct fields, converting to GALAXY_PROP_* macros as single source of truth
- **Documentation Consolidation**: Transformed 30+ scattered files into 20 focused documents with 4 comprehensive guides

---

## Recent Changes Archive

*Consolidated from all past recent-progress logs - what was modified when*

### Phase 5.2 Key Changes (May-June 2025)
- **Major Files Modified**: `src/core/core_build_model.c` (complete refactoring), `src/core/core_allvars.h` (GALAXY struct cleanup), `src/core/core_array_utils.c` (memory corruption fixes), `src/properties.yaml` (core-physics separation), `src/parameters.yaml` (new metadata system)
- **I/O System**: `src/io/io_interface.*`, `src/io/io_lhalo_hdf5.*`, `src/generate_property_headers.py` (SnapNum preservation fix)
- **Tests Added**: `test_galaxy_array.c`, `test_halo_progenitor_integrity.c`, `test_dynamic_memory_expansion.c`, `test_physics_free_mode.c`
- **Documentation**: Created `docs/architecture.md`, `docs/property-system.md`, `docs/io-system.md`, `docs/testing-guide.md`, `docs/development-guide.md`
- **Archived**: 35+ placeholder modules, 16 consolidated documentation files to timestamped ignore/ directories
- **Milestone**: Complete core-physics separation with working galaxy evolution pipeline, dual property system elimination, comprehensive documentation reorganization

### Phase 4 Key Changes  
- **Files Modified**: `src/core/core_property_dispatcher.*`, `src/physics/physics_events.*`
- **Tests Added**: `test_property_dispatcher_system.c`, `test_physics_events.c`
- **Milestone**: Property access control and event system separation

### Phase 3 Key Changes
- **Files Modified**: `src/io/io_interface.*`, `src/core/core_memory_pool.*`
- **Tests Added**: `test_io_interface.c`, `test_dynamic_memory_expansion.c` 
- **Milestone**: I/O abstraction and memory management improvements

### Phase 2 Key Changes
- **Files Modified**: `src/core/core_module_system.*`, `src/core/core_pipeline_system.*`
- **Tests Added**: `test_module_lifecycle.c`, `test_pipeline.c`
- **Milestone**: Module registration and pipeline execution systems

### Phase 1 Key Changes
- **Files Modified**: `src/core/core_allvars.h`, `src/core/sage.*`, `src/physics/*`
- **Tests Added**: `test_physics_free_mode.c`, `test_core_merger_processor.c`
- **Milestone**: Core-physics separation and property system foundation

---

## Phase Summary

*High-level progression through enhanced refactoring plan*

| Phase | Focus | Status | Key Deliverables |
|-------|--------|--------|------------------|
| **Phase 1** | Core-Physics Separation | ✅ Complete | Independent core infrastructure, physics-free mode |
| **Phase 2** | Module & Pipeline Systems | ✅ Complete | Constructor registration, 4-phase execution |
| **Phase 3** | I/O & Memory Management | ✅ Complete | Unified I/O interface, dynamic memory expansion |
| **Phase 4** | Property & Event Systems | ✅ Complete | Type-safe property access, event separation |
| **Phase 5.2** | Enhanced Refactoring | ✅ Complete | Core-physics separation, dual property elimination, documentation consolidation |
| **Phase 6** | Remaining I/O Formats | 🔒 Blocked | Gadget4, Genesis, ConsistentTrees migration |

---

## Search Keywords

*For quick lookup of specific topics*

**Architecture**: core-physics separation, module system, pipeline phases, event system, dual property elimination  
**Properties**: property dispatcher, YAML generation, access control, extension system, GALAXY_PROP_* macros  
**I/O**: unified interface, HDF5 migration, tree formats, property serialization, SnapNum preservation  
**Memory**: dynamic expansion, tree-scoped allocation, memory pool management, galaxy array corruption fixes  
**Testing**: validation framework, unit tests, physics-free mode, end-to-end validation, heisenbug resolution  
**Configuration**: JSON parsing, module activation, parameter validation, parameters.yaml metadata system  
**Documentation**: consolidation strategy, role-based navigation, professional standards, archive organization  

---

## Archive Maintenance

- **Updated after each phase completion** by consolidating valuable information from completed logs
- **Decision timeline** preserves architectural rationale for future reference  
- **Changes archive** tracks file modifications for debugging and understanding evolution
- **Search keywords** enable quick lookup without reading full context
- **Current logs** in parent directory should reference this archive for historical context rather than repeating information

---

*This archive prevents current logs from becoming overloaded while preserving essential historical context.*