<!-- Purpose: Current project phase context -->
<!-- Update Rules:
- 500-word limit! 
- Include: 
  • Phase objectives
  • Current progress as a checklist (keep short)
  • Next actions (more detail - 2-3 sentences)
  • Completion criteria 
  • Inter-phase dependencies
- At major phase completion archive as phase-[X].md and refresh for next phase
-->

# Current Phase: 2.1-2.2/7 (Base Module Interfaces & Galaxy Extensions)

## Phase Objectives
- Complete Phase 2.1: Base Module Interfaces
  - Define standard interfaces for core physics module types
  - Create a consistent API surface for all modules
  - Build basic module lifecycle management
  - Design versioning and compatibility checking
  - Implement error handling and validation

- Complete Phase 2.2: Galaxy Property Extension Mechanism
  - Extend GALAXY structure with extension capability
  - Create property registration and access system
  - Implement memory management for extensions
  - Design serialization interface for extended properties
  - Develop validation for property extensions

## Current Progress

### Phase 2.1: Base Module Interfaces
- [ ] Define base_module structure with common functionality
- [ ] Create module-specific interfaces (cooling, star formation, etc.)
- [ ] Implement module type identification system
- [ ] Design module version compatibility checking
- [ ] Develop module function validation
- [ ] Create module error handling system
- [ ] Implement basic lifecycle (initialize/cleanup) functions

### Phase 2.2: Galaxy Property Extension Mechanism
- [ ] Extend GALAXY structure with extension capabilities
- [ ] Implement galaxy_property_t registration system
- [ ] Create memory management for extension data
- [ ] Design property access macros/functions
- [ ] Implement property serialization interface
- [ ] Develop validation for property extensions
- [ ] Create example implementation with a test module

## Next Actions
1. Create base_module structure: Implement the foundational module interface that will be inherited by all physics modules. This structure will include common fields like name, version, author, and lifecycle methods. Create validation functions to ensure modules are properly formed and initialization functions to prepare module data.

2. Define cooling module interface: Implement the interface for cooling modules as the first specific physics module type. Define required and optional function pointers, implement validation, and create compatibility checking with the existing cooling implementation to facilitate backward compatibility.

3. Add galaxy extension capability: Extend the GALAXY structure to support the property extension mechanism while maintaining binary compatibility with the core model. Implement the memory management required to handle dynamic property extensions and create the access API that will be used by module developers.

## Completion Criteria
- Base module interfaces defined for all physics components
- Clear distinction between required and optional module functions
- Galaxy property extension mechanism functioning correctly
- Extension memory management working efficiently
- Serialization of extended properties designed
- Example implementations validating the approach
- Backward compatibility with existing model maintained

## Inter-Phase Dependencies
- Phase 1 (Preparatory Refactoring): ✅ COMPLETED
- Phase 2.1 (Base Module Interfaces): 🔄 IN PROGRESS
- Phase 2.2 (Galaxy Property Extension): 🔄 IN PROGRESS
- Phase 2.3 (Event System): BLOCKED by current phase
- Phase 2.4-2.6 (Registry/Pipeline/Config): BLOCKED by 2.1-2.3

## Reference Material
- Base Module Interfaces: See refactoring plan section 2.1
- Galaxy Property Extensions: See refactoring plan section 2.2
- Implementation examples: See refactoring plan "Practical Examples" section