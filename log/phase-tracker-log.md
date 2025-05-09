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
- Implement pipeline execution phases (HALO, GALAXY, POST, FINAL) for handling different scope calculations
- Transform `evolve_galaxies()` to use the pipeline system
- Implement the Properties Module architecture for complete core-physics decoupling
- Create type-safe, centrally-defined galaxy property mechanism
- Achieve complete independence between core infrastructure and physics modules
- Enable the core system to operate correctly with no physics modules
- Migrate physics components as pure add-ons to the core
- Minimize core `struct GALAXY` to remove all physics dependencies
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

#### Phase 5.2.E: I/O System Update ✅ COMPLETED
- [x] Remove `GALAXY_OUTPUT` struct
- [x] Remove `prepare_galaxy_for_output` logic
- [x] Implement output preparation module using `GALAXY_PROP_*` macros
- [x] Remove binary output format support (standardize on HDF5)
- [x] Update HDF5 I/O handler to read property metadata and use `GALAXY_PROP_*` macros for writing
- [x] Enhance HDF5 serialization/deserialization for dynamic arrays and module-specific properties

#### Phase 5.2.F: Core-Physics Separation ⏳ PENDING
This phase achieves true physics-agnostic core infrastructure that can run without any physics modules.

##### Phase 5.2.F.1: Core Isolation ✅ COMPLETED
- [x] Remove ALL physics dependencies from core infrastructure
- [x] Remove all direct physics calls in `evolve_galaxies.c`
- [x] Remove physics-related include statements from core files
- [x] Create minimal properties definition that only includes core infrastructure properties
- [x] Remove direct field synchronization from pipeline executor
- [x] Remove physics fields from `struct GALAXY` definition (`core_allvars.h`)
- [x] Update all core code to be physics-property-agnostic
- [x] Verify pipeline system's structural integrity without physics

##### Phase 5.2.F.2: Empty Pipeline Validation ⏳ PENDING
- [ ] Create empty placeholder modules for essential pipeline points
- [ ] Register empty modules in the pipeline
- [ ] Configure pipeline to execute all phases with no physics operations
- [ ] Test that the system can run end-to-end with empty properties.yaml
- [ ] Implement tests verifying core independence from physics
- [ ] Document the physics-free model baseline
- [ ] Optimize memory management with increased allocation limits

##### Phase 5.2.F.3: Legacy Code Removal ⏳ PENDING
- [ ] Remove all legacy physics implementation files
- [ ] Update build system to remove legacy components
- [ ] Clean up any remaining legacy references
- [ ] Remove all synchronization infrastructure after verifying it's no longer needed
- [ ] Final verification of clean core-physics separation

#### Phase 5.2.G: Physics Module Migration ⏳ PENDING
With the core now completely physics-agnostic, we can implement physics modules as pure add-ons.

##### Phase 5.2.G.1: Physics Foundation ⏳ PENDING
- [ ] Develop standard physics utility functions independent of core code
- [ ] Create common physics constants and conversion factors
- [ ] Implement shared calculation libraries for physics modules
- [ ] Establish module-to-module communication protocols
- [ ] Create physics property definitions separate from core properties

##### Phase 5.2.G.2: Module Implementation ⏳ PENDING
- [ ] Determine optimal module sequence based on dependencies
- [ ] Implement/validate Star Formation & Feedback Module
- [ ] Implement/validate Disk Instability Module
- [ ] Implement/validate Reincorporation Module
- [ ] Implement/validate AGN Feedback & Black Holes Module
- [ ] Implement/validate Metals & Chemical Evolution Module
- [ ] Implement/validate Mergers Module
- [ ] (Optional) Optimize property access patterns if performance analysis indicates benefit

### Phase 5.3: Comprehensive Validation ⏳ PENDING
After completely separating the core from physics and implementing all physics modules:

- [ ] Develop complete end-to-end integration tests for all module combinations
- [ ] Implement scientific validation comparing new modules to baseline SAGE
- [ ] Create module compatibility test matrix
- [ ] Develop performance benchmarks comparing to original implementation
- [ ] Test file format compatibility and I/O across module configurations
- [ ] Validate module dependency chains using call graph analysis
- [ ] Test error handling and recovery mechanisms
- [ ] Create comprehensive documentation of the validation approach

## Completion Criteria
- The main galaxy evolution loop uses the pipeline system with proper phase handling
- The core system operates correctly with empty or minimal properties.yaml
- Core infrastructure runs without any physics modules loaded
- All persistent per-galaxy physics state is defined centrally via `properties.yaml`
- Core infrastructure has zero knowledge of specific physics properties
- All physics components are implemented as pure add-ons to the core
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