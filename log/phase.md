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

# Current Phase: 2/7 (Property System Core) - Metadata-Driven Property Management

## Phase Objectives
- **PRIMARY**: Create metadata-driven property system to separate core from physics
- **SECONDARY**: Enable compile-time type safety and runtime flexibility
- Implement YAML-based property definitions
- Build code generation pipeline
- Migrate existing properties to new system while maintaining compatibility

## Current Progress

### Task 2.1: Property Metadata Design
- [ ] Create properties.yaml with core/physics separation
- [ ] Define parameters.yaml for simulation parameters
- [ ] Establish property categorization (core, physics, derived)
- [ ] Document metadata schema and conventions

### Task 2.2: Code Generation System
- [ ] Create generate_property_headers.py script
- [ ] Implement type-safe macro generation
- [ ] Generate property access functions
- [ ] Create property availability checking system
- [ ] Add property iteration support

### Task 2.3: Property API Implementation
- [ ] Create property.h/c with unified property interface
- [ ] Implement compile-time property access macros
- [ ] Add runtime property availability checking
- [ ] Create property initialization/cleanup functions
- [ ] Build property validation framework

### Task 2.4: Core Property Migration
- [ ] Migrate essential galaxy properties (Type, GalaxyNr, HaloNr, etc.)
- [ ] Migrate halo properties (Mvir, Rvir, Vvir, etc.)
- [ ] Update core_allvars.h to use generated structures
- [ ] Adapt core_build_model.c to new property access
- [ ] Ensure backward compatibility during transition

### Task 2.5: Build System Integration
- [ ] Integrate code generation into CMake build
- [ ] Create build configurations for different property sets
- [ ] Add property-based compilation flags
- [ ] Set up IDE support for generated headers
- [ ] Implement incremental generation optimization

## Completion Criteria
**Phase 2 Complete When:**
- Properties defined in YAML, not hardcoded C structs
- Code generation produces type-safe property access
- Core properties migrated to new system
- All tests pass with identical scientific results
- Build system automatically generates property code

**Phase 2 Status**: 0/5 tasks completed

## Inter-Phase Dependencies
- **From Phase 1**: ✅ CMake build system (for code generation integration)
- **From Phase 1**: ✅ Memory abstraction (for property allocation)
- **To Phase 3**: Property system needed for RAII memory management
- **To Phase 4**: Property metadata enables configuration validation
- **To Phase 5**: Property abstraction required for modular physics