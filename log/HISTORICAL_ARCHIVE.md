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

### Phase 5 Decisions
- **HDF5 Output Migration**: Completed migration of LHalo HDF5 to unified I/O interface with property-based serialization
- **Configuration System Hardening**: Fixed memory corruption issues in JSON configuration parsing with enhanced validation

---

## Recent Changes Archive

*Consolidated from all past recent-progress logs - what was modified when*

### Phase 5 Key Changes
- **Files Modified**: `src/io/io_interface.*`, `src/io/io_lhalo_hdf5.*`, `src/core/core_config_system.*`
- **Tests Added**: `test_io_interface.c`, `test_hdf5_output_validation.c`, `test_config_system.c`
- **Milestone**: Complete I/O interface migration for LHalo HDF5 format

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
| **Phase 5** | HDF5 Migration & Validation | ✅ Complete | LHalo HDF5 migration, enhanced testing |
| **Phase 6** | Remaining I/O Formats | 🚧 Current | Gadget4, Genesis, ConsistentTrees migration |

---

## Search Keywords

*For quick lookup of specific topics*

**Architecture**: core-physics separation, module system, pipeline phases, event system  
**Properties**: property dispatcher, YAML generation, access control, extension system  
**I/O**: unified interface, HDF5 migration, tree formats, property serialization  
**Memory**: dynamic expansion, tree-scoped allocation, memory pool management  
**Testing**: validation framework, unit tests, physics-free mode, end-to-end validation  
**Configuration**: JSON parsing, module activation, parameter validation, memory corruption fixes  

---

## Archive Maintenance

- **Updated after each phase completion** by consolidating valuable information from completed logs
- **Decision timeline** preserves architectural rationale for future reference  
- **Changes archive** tracks file modifications for debugging and understanding evolution
- **Search keywords** enable quick lookup without reading full context
- **Current logs** in parent directory should reference this archive for historical context rather than repeating information

---

*This archive prevents current logs from becoming overloaded while preserving essential historical context.*