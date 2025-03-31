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
- [ ] Design unified I/O interface structure with metadata and capabilities
- [ ] Implement common I/O operations (initialize, read_forest, write_galaxies, cleanup)
- [ ] Add HDF5 resource tracking for handles
- [ ] Create registry for format-specific handlers
- [ ] Implement format detection and automatic handler selection
- [ ] Add error handling and validation for I/O operations

### Phase 3.2: Format-Specific Implementations
- [ ] Refactor binary format handlers to use the common interface
- [ ] Add cross-platform endianness detection and conversion
- [ ] Refactor HDF5 format handlers to use the common interface
- [ ] Implement proper HDF5 resource management to prevent handle leaks
- [ ] Implement serialization support for extended properties
- [ ] Add validation for data consistency across formats
- [ ] Create format conversion utilities

### Phase 3.3: Memory Optimization
- [ ] Implement configurable buffered reading/writing with runtime-adjustable buffer sizes
- [ ] Create memory mapping options for large files
- [ ] Design efficient caching for frequently accessed halos
- [ ] Optimize allocation with geometric growth instead of fixed increments
- [ ] Add prefetching for depth-first traversal
- [ ] Implement memory pooling for galaxy allocations

## Next Actions

### Phase 3.1 Interface Design
1. I/O Interface Definition:
   - Design a unified I/O interface with common operations and resource management functions
   - Add metadata fields including format capabilities and versioning
   - Create a registry for format-specific handlers with automatic format detection
   - Develop comprehensive error handling and validation specific to I/O operations

2. Binary Format Implementation:
   - Refactor the existing binary I/O code into a format-specific implementation
   - Add cross-platform endianness detection and conversion for binary data
   - Ensure backward compatibility with existing binary files
   - Implement proper error handling and validation

3. HDF5 Format Implementation:
   - Refactor the existing HDF5 I/O code with improved resource management
   - Implement robust handle tracking to prevent resource leaks
   - Add support for galaxy extensions and custom properties
   - Implement metadata storage with proper attribute handling

4. Memory Optimization Strategy:
   - Implement runtime-configurable buffer sizes for I/O operations
   - Optimize galaxy allocation with geometric growth for better performance
   - Design intelligent caching for frequently accessed halo data
   - Develop benchmarks to measure the impact of memory optimizations

## Completion Criteria
- All I/O operations function through the unified interface
- Format-specific handlers implement all required functionality
- Endianness handling ensures cross-platform compatibility for binary formats
- HDF5 resources are properly tracked and cleaned up with no handle leaks
- Extended properties are properly serialized across formats
- Memory allocation is optimized with geometric growth strategies
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