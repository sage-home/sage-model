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

# Current Phase: 3/7 (I/O Abstraction and Memory Optimization)

## Phase Objectives
- Implement a unified I/O interface that preserves existing functionality
- Create format-specific handlers for different input/output formats
- Add efficient caching strategies for frequently accessed merger tree nodes
- Optimize memory usage for large datasets
- Develop intelligent prefetching for depth-first tree traversal
- Implement memory pooling for galaxy allocations

## Current Progress

### Phase 3.1: I/O Interface Abstraction ✅ COMPLETED
- [x] Design unified I/O interface structure with metadata and capabilities
- [x] Implement common I/O operations (initialize, read_forest, write_galaxies, cleanup)
- [x] Add HDF5 resource tracking for handles
- [x] Create registry for format-specific handlers
- [x] Implement format detection and automatic handler selection
- [x] Add error handling and validation for I/O operations

### Phase 3.2: Format-Specific Implementations ✅ COMPLETED
- [x] Refactor binary format handlers to use the common interface
- [x] Add cross-platform endianness detection and conversion
- [x] Refactor HDF5 format handlers to use the common interface
  - [x] LHalo HDF5 handler framework
  - [x] ConsistentTrees HDF5 handler framework
  - [x] Gadget4 HDF5 handler framework
  - [x] Genesis HDF5 handler framework
- [x] Implement proper HDF5 resource management to prevent handle leaks
- [x] Implement serialization support for extended properties
- [x] Implement binary output handler with extended property support
- [x] Implement HDF5 output handler with extended property support
- [x] Update core save function to use the I/O interface
- [x] Add validation for data consistency across formats
  - [x] Basic galaxy data validation
  - [x] Format-specific validation
  - [x] Extended property validation
  - [x] I/O interface integration

### Phase 3.3: Memory Optimization 🔄 IN PROGRESS
- [ ] Implement configurable buffered reading/writing with runtime-adjustable buffer sizes
- [ ] Create memory mapping options for large files
- [ ] Design efficient caching for frequently accessed halos
- [x] Optimize allocation with geometric growth instead of fixed increments
- [ ] Add prefetching for depth-first traversal
- [ ] Implement memory pooling for galaxy allocations

## Next Actions

### Phase 3.3 Memory Optimization
1. Configurable Buffered I/O: 🔄 STARTING
   - Design a configuration system for buffer sizes that can be adjusted at runtime
   - Implement dynamic buffer adjustment based on available memory and dataset size
   - Develop profiling metrics to evaluate I/O performance with different buffer sizes

2. Memory Mapping: 🔄 PLANNING
   - Research optimal memory mapping strategies for large merger tree files
   - Implement memory mapping options for both binary and HDF5 formats
   - Develop fallback mechanisms for platforms or file systems with limited memory mapping support

3. Tree Traversal Optimization: 🔄 PLANNING
   - Design prefetching system for depth-first tree traversal
   - Implement predictive loading based on tree structure
   - Optimize pointer-chasing patterns to reduce cache misses

## Completion Criteria
- All I/O operations function through the unified interface
- Memory usage is optimized for large merger trees
- Measurable performance improvements in tree traversal and galaxy processing
- Buffer sizes are configurable and automatically adjusted based on workload
- Memory mapping is properly implemented for supported platforms and formats
- Prefetching reduces cache misses during tree traversal
- All tests passing with the new memory optimization system

## Inter-Phase Dependencies
- Phase 1 (Preparatory Refactoring): ✅ COMPLETED
- Phase 2 (Module Interfaces): ✅ COMPLETED
- Phase 3.1-3.2 (I/O Abstraction): ✅ COMPLETED
- Phase 3.3 (Memory Optimization): 🔄 IN PROGRESS
- Phase 4 (Plugin Infrastructure): BLOCKED by Phase 3.3