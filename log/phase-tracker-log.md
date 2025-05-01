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

#### Phase 5.2.B: Central Property Definition ⏳ PLANNED
- [x] Establish performance baseline for current implementation
- [ ] Define properties format (`properties.yaml`)
- [ ] Create header generation script
- [ ] Integrate header generation into build system
- [ ] Implement core registration of standard properties
- [ ] Minimize core `struct GALAXY` by removing physics fields

#### Phase 5.2.C: Core Integration ⏳ PENDING
- [ ] Implement and use accessor macros in core code
- [ ] Remove obsolete core accessors and parameter views
- [ ] Refine core initialization logic
- [ ] Update galaxy creation and management

#### Phase 5.2.D: Module Adaptation ⏳ PENDING
- [ ] Update physics module interface
- [ ] Update migrated modules (cooling, infall)
- [ ] Update module template generator
- [ ] Revise module dependency management

#### Phase 5.2.E: I/O System Update ⏳ PENDING
- [ ] Remove `GALAXY_OUTPUT` struct 
- [ ] Remove `prepare_galaxy_for_output` logic
- [ ] Implement output preparation module
- [ ] Update I/O handlers (binary, HDF5)
- [ ] Update property serialization system

#### Phase 5.2.F: Physics Module Migration ⏳ PENDING
- [ ] Define physics module migration sequence
- [ ] Centralize common physics utilities
- [ ] Migrate star formation and feedback module
- [ ] Migrate disk instability module
- [ ] Migrate reincorporation module
- [ ] Migrate AGN feedback and black holes modules
- [ ] Migrate metals and chemical evolution tracking
- [ ] Migrate mergers module

### Phase 5.3: Validation and Testing ⏳ PENDING
- [x] Implement integration tests for evolve_galaxies loop phase transitions
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
- Event handling and module callbacks correctly preserve physics interdependencies
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