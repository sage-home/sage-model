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
- Optimize memory usage for large datasets
- Implement efficient buffered I/O for output operations
- Create memory mapping options for large input files
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
- [x] Implement configurable buffered reading/writing with runtime-adjustable buffer sizes
- [x] Create memory mapping options for large files
- [x] Optimize allocation with geometric growth instead of fixed increments
- [ ] Implement memory pooling for galaxy allocations

## Next Actions

### Phase 3.3 Memory Optimization
1. Configurable Buffered I/O: ✅ COMPLETED
   - Created comprehensive buffer manager with configurable sizes and automatic resizing
   - Added support for both binary and HDF5 output formats
   - Implemented runtime parameters for buffer configuration

2. Memory Mapping: ✅ COMPLETED
   - Implemented cross-platform memory mapping for large input files (POSIX/Windows)
   - Added integration with LHalo binary format for efficient file access
   - Created fallback mechanisms to standard I/O when mapping is unavailable or fails
   - Added runtime parameter (EnableMemoryMapping) to control memory mapping

3. Memory Pooling: 🔄 STARTING
   - Design efficient memory pooling system for galaxy structure allocations
   - Implement size-based allocation strategies to reduce fragmentation
   - Create comprehensive tracking and debugging facilities for memory usage

## Completion Criteria
- All I/O operations function through the unified interface
- Memory usage is optimized for large merger trees
- Measurable performance improvements in tree traversal and galaxy processing
- Buffer sizes are configurable and automatically adjusted based on workload
- Memory mapping is properly implemented for supported platforms and formats
- Memory pooling reduces allocation overhead and fragmentation
- All tests passing with the new memory optimization system

## Inter-Phase Dependencies
- Phase 1 (Preparatory Refactoring): ✅ COMPLETED
- Phase 2 (Module Interfaces): ✅ COMPLETED
- Phase 3.1-3.2 (I/O Abstraction): ✅ COMPLETED
- Phase 3.3 (Memory Optimization): 🔄 IN PROGRESS
- Phase 4 (Plugin Infrastructure): BLOCKED by Phase 3.3