<!-- Purpose: Current project phase context -->
<!-- Update Rules:
- 500-word limit! 
- Include: 
  • Phase objectives
  • Current progress as a checklist
  • Completion criteria 
  • Inter-phase dependencies
- At major phase completion archive as phase-[X].md and refresh for next phase
-->

# Current Phase: 5/7 (Core Module Migration)

## Phase Objectives
- Implement pipeline execution phases (HALO, GALAXY, POST) for handling different scope calculations
- Transform `evolve_galaxies()` to use the pipeline system
- Implement the Properties Module architecture for complete core-physics decoupling
- Create type-safe, centrally-defined galaxy property mechanism
- Minimize core `struct GALAXY` to remove physics dependencies
- Refactor modules to use the standardized property access system
- Validate scientific accuracy against baseline SAGE results

## Current Progress

### Phase 5.1: Refactoring Main Evolution Loop ✅ COMPLETED
- [x] Implement merger event queue approach in traditional architecture
- [x] Implement pipeline phase system (HALO, GALAXY, POST, FINAL)
- [x] Transform `evolve_galaxies()` to use the pipeline system with proper phase handling
- [x] Integrate event dispatch/handling points
- [x] Add support for reading/writing extension properties
- [x] Integrate module callbacks
- [x] Enhance error propagation testing and diagnostic logging
- [x] Add evolution diagnostics

### Phase 5.2: Properties Module Implementation ⏳ ARCHITECTURAL SHIFT
**Note**: After careful evaluation, we've decided to replace the piecemeal physics module migration with a more elegant "Properties Module" architecture that provides cleaner separation between core and physics.

#### Phase 5.2.A: Initial Proof-of-Concept ➖ SKIPPED
- [ ] ~~Establish performance baseline for current implementation~~
- [ ] ~~Create minimal property definition for essential properties in YAML~~
- [ ] ~~Implement basic header generation script~~
- [ ] ~~Create core integration test with generated macros~~
- [ ] ~~Convert one simple module to use the new access pattern~~
- [ ] ~~Validate scientific equivalence with previous implementation~~
- [ ] ~~Assess performance impact of the new architecture~~
- [ ] ~~Make go/no-go decision for full implementation~~

#### Phase 5.2.B: Central Property Definition & Infrastructure ✅ COMPLETED
- [x] Establish performance baseline for current implementation
- [x] Define properties format (`properties.yaml`) with dynamic array support
- [x] Create header generation script with dynamic array allocation support
- [x] Integrate header generation into build system
- [x] Implement core registration of standard properties
- [x] Implement memory management for dynamic array properties
- [x] Implement synchronization functions (`sync_direct_to_properties`, `sync_properties_to_direct`) in `core_properties_sync.c/h`

#### Phase 5.2.C: Core Integration & Synchronization ✅ COMPLETED
- [x] Integrate synchronization calls (`sync_direct_to_properties`, `sync_properties_to_direct`) into pipeline execution points (e.g., `physics_pipeline_executor.c`)
- [x] Ensure core galaxy lifecycle functions (`init_galaxy`, copying, freeing) correctly manage the `galaxy->properties` struct allocation, copying, and freeing
- [x] Refine core initialization logic to correctly initialize direct fields AND call `reset_galaxy_properties`

#### Phase 5.2.D: Module Adaptation ✅ COMPLETED
- [x] Update migrated modules (cooling, infall) to use `GALAXY_PROP_*` macros exclusively
- [x] Update module template generator (`core_module_template.c/h`) for new property patterns
- [x] Revise module dependency management for property-based interactions (if needed)

#### Phase 5.2.E: I/O System Update ⏳ IN PROGRESS
- [x] Remove `GALAXY_OUTPUT` struct
- [ ] Remove `prepare_galaxy_for_output` logic
- [ ] Implement output preparation module using `GALAXY_PROP_*` macros
- [ ] Remove binary output format support (standardize on HDF5)
- [ ] Update HDF5 I/O handler to read property metadata and use `GALAXY_PROP_*` macros for writing
- [ ] Enhance HDF5 serialization/deserialization for dynamic arrays and module-specific properties

#### Phase 5.2.F: Physics Module Migration ⏳ PENDING
- [ ] Define physics module migration sequence based on dependencies
- [ ] Centralize common physics utilities (if applicable)
- [ ] Migrate Star Formation & Feedback module (using `GALAXY_PROP_*` macros)
- [ ] Migrate Disk Instability module (using `GALAXY_PROP_*` macros)
- [ ] Migrate Reincorporation module (using `GALAXY_PROP_*` macros)
- [ ] Migrate AGN Feedback & Black Holes module (using `GALAXY_PROP_*` macros)
- [ ] Migrate Metals & Chemical Evolution module (using `GALAXY_PROP_*` macros)
- [ ] Migrate Mergers module (using `GALAXY_PROP_*` macros)
- [ ] Remove corresponding legacy files from `src/physics/legacy/` and Makefile

#### Phase 5.2.G: Final Cleanup & Core Minimization ⏳ PENDING
- [ ] Remove synchronization calls (`sync_direct_to_properties`, `sync_properties_to_direct`) and files (`core_properties_sync.c/h`)
- [ ] Refactor remaining core code (validation, misc utils) to use `GALAXY_PROP_*` macros if any direct field access remains
- [ ] Remove physics fields from `struct GALAXY` definition (`core_allvars.h`)
- [ ] Optimize memory management with increased allocation limits (`MAXBLOCKS`) and proper cleanup of diagnostic code
- [ ] (Optional) Refactor accessor macros/core/module code to directly access `galaxy->properties->FieldName` if performance analysis indicates benefit
  
### Phase 5.3: Validation and Testing ⏳ PENDING
- [ ] Implement integration tests for evolve_galaxies loop phase transitions
- [ ] Implement property definition validation tools
- [ ] Perform scientific validation against baseline SAGE
- [ ] Implement performance benchmarks
- [ ] Develop module compatibility tests
- [ ] Add call graph validation
- [ ] Validate file format compatibility and I/O
- [ ] Test error handling and recovery mechanisms

## Completion Criteria
- The main galaxy evolution loop uses the pipeline system with proper phase handling
- All persistent per-galaxy physics state is defined centrally via `properties.yaml`
- Core infrastructure has no direct knowledge of specific physics properties
- All physics components access galaxy properties via generated macros
- Physics modules are implemented as standalone modules with appropriate phase declarations
- The core GALAXY struct contains only essential identifiers, not physics fields
- Output properties are determined by flags in the central definition
- Property system fully supports dynamic array properties with runtime size determination
- Event handling and module callbacks correctly preserve physics interdependencies
- HDF5 output format is the exclusive output format with enhanced property serialization
- Scientific results match baseline SAGE simulation outputs
- All tests pass, including validation tests

## Inter-Phase Dependencies
- Phase 1 (Preparatory Refactoring): ✅ COMPLETED
- Phase 2 (Module Interfaces): ✅ COMPLETED
- Phase 3 (I/O Abstraction & Memory Optimization): ✅ COMPLETED
- Phase 4 (Plugin Infrastructure): ✅ COMPLETED
- Phase 5 (Core Module Migration): ⏳ IN PROGRESS
- Phase 6 (Advanced Performance Optimization): 🔒 BLOCKED
- Phase 7 (Documentation and Tools): 🔒 BLOCKED