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

# Current Phase: 3/7 (I/O Abstraction and Optimization)

## Phase Objectives
- Implement a unified I/O interface that preserves existing functionality
- Create format-specific handlers for different input/output formats
- Add efficient caching strategies for frequently accessed merger tree nodes
- Optimize memory usage for large datasets
- Develop intelligent prefetching for depth-first tree traversal
- Implement memory pooling for galaxy allocations

## Current Progress

### Phase 3.1: I/O Interface Abstraction
- [ ] Design unified I/O interface structure
- [ ] Implement common I/O operations (initialize, read_forest, write_galaxies, cleanup)
- [ ] Create registry for format-specific handlers
- [ ] Implement format detection and automatic handler selection
- [ ] Add error handling and validation for I/O operations

### Phase 3.2: Format-Specific Implementations
- [ ] Refactor binary format handlers to use the common interface
- [ ] Refactor HDF5 format handlers to use the common interface
- [ ] Implement serialization support for extended properties
- [ ] Add validation for data consistency across formats
- [ ] Create format conversion utilities

### Phase 3.3: Memory Optimization
- [ ] Implement buffered reading/writing
- [ ] Create memory mapping options for large files
- [ ] Design efficient caching for frequently accessed halos
- [ ] Add prefetching for depth-first traversal
- [ ] Implement memory pooling for galaxy allocations

## Next Actions

### Phase 3.1 Interface Design
1. I/O Interface Definition:
   - Design a unified I/O interface with common operations (initialize, read_forest, write_galaxies, cleanup)
   - Create a registry for format-specific handlers that can be selected at runtime
   - Develop error handling and validation to ensure data integrity

2. Binary Format Implementation:
   - Refactor the existing binary I/O code into a format-specific implementation of the interface
   - Ensure backward compatibility with existing binary files
   - Implement proper error handling and validation

3. HDF5 Format Implementation:
   - Refactor the existing HDF5 I/O code into a format-specific implementation of the interface
   - Add support for galaxy extensions and custom properties
   - Implement metadata storage for extended properties

4. Memory Optimization Strategy:
   - Design a plan for implementing memory optimization features (buffering, mapping, caching)
   - Identify critical paths for optimization based on profiling data
   - Develop benchmarks to measure the impact of optimizations

## Completion Criteria
- All I/O operations function through the unified interface
- Format-specific handlers implement all required functionality
- Extended properties are properly serialized across formats
- Memory usage is optimized for large merger trees
- Backward compatibility is maintained with existing data files
- Performance improvements are measurable and significant
- All tests passing with the new I/O system

## Inter-Phase Dependencies
- Phase 1 (Preparatory Refactoring): ✅ COMPLETED
- Phase 2.1 (Base Module Interfaces): ✅ COMPLETED
- Phase 2.2 (Galaxy Property Extension): ✅ COMPLETED
- Phase 2.3 (Event System): ✅ COMPLETED
- Phase 2.4 (Module Registry): ✅ COMPLETED
- Phase 2.5-2.6 (Pipeline/Config): ✅ COMPLETED
- Phase 2.7 (Module Callback System): ✅ COMPLETED (all components implemented)
- Phase 3 (I/O Abstraction): 🔄 IN PROGRESS
- Phase 4 (Plugin Infrastructure): BLOCKED by Phase 2.7 and Phase 3

## Reference Material
- I/O Interface: See refactoring plan section 3.1
- Memory Optimization: See refactoring plan section 3.3
- Implementation examples: See refactoring plan "Practical Examples" section 3 and 4