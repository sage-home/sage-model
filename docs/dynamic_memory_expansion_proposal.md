# Dynamic Memory Expansion Proposal for SAGE

## Problem Statement

SAGE currently suffers from segmentation faults and memory allocation failures when processing large merger trees or running with multiple physics modules. The root cause is a combination of artificial memory limits and poor initial memory allocation sizing that doesn't scale with simulation requirements.

### Current Memory Issues

1. **Hard Block Limit**: `MAXBLOCKS = 50000` in `src/core/core_mymalloc.c` creates an artificial ceiling on memory allocations
2. **Poor Initial Sizing**: `MAXGALFAC = 1` in `src/core/macros.h` severely underestimates galaxy array sizes needed
3. **No Dynamic Expansion**: Block table cannot grow beyond compile-time limits
4. **Memory Fragmentation**: No efficient bulk deallocation between tree processing

### Memory Usage Pattern in SAGE

SAGE is a semi-analytic galaxy formation model that processes N-body merger trees in a specific pattern:

1. **Load Tree**: Read halo merger tree data into memory
2. **Process Tree**: Build all galaxies and evolve them through cosmic time (depth-first traversal)
3. **Output Results**: Write galaxy properties to HDF5 files
4. **Discard Tree**: Free all memory and move to next tree
5. **Repeat**: Process next tree independently

**Key Insight**: Memory usage is **per-tree**, not cumulative across trees. Each tree processing cycle has a peak memory requirement, then everything is discarded.

## Proposed Solution: Tree-Scoped Dynamic Memory Management

### Core Components

1. **Dynamic Block Table Expansion**
2. **Intelligent Initial Sizing**
3. **Tree-Scoped Memory Management**
4. **Improved Error Handling and Monitoring**

### Implementation Strategy

This solution modifies the existing `core_mymalloc.c` system rather than replacing it, ensuring minimal disruption to existing code while removing artificial limits.

## Detailed Implementation Plan

### 1. Dynamic Block Table Expansion

**File**: `src/core/core_mymalloc.c`

**Problem**: Fixed `MAXBLOCKS = 50000` limit prevents large simulations from running.

**Solution**: Make the block table dynamically expandable.

```c
// Current (static):
#define MAXBLOCKS 50000
static void *Table[MAXBLOCKS];
static size_t SizeTable[MAXBLOCKS];

// New (dynamic):
static long MaxBlocks = 10000;  // Start smaller, grow as needed
static void **Table = NULL;     // Dynamically allocated
static size_t *SizeTable = NULL;
```

**Key Changes**:
- Replace static arrays with dynamically allocated arrays
- Add `expand_block_table()` function to double table size when needed
- Initialize tables in new `memory_system_init()` function
- Add memory pressure monitoring to expand proactively

### 2. Intelligent Initial Sizing

**File**: `src/core/macros.h` and tree processing functions

**Problem**: `MAXGALFAC = 1` assumes 1 galaxy per halo, but real ratios can be 5-20x higher.

**Solution**: Estimate memory needs based on tree characteristics before processing.

```c
// Enhanced sizing based on tree analysis
int estimate_tree_memory_needs(struct tree_info *tree) {
    int estimated_galaxies = tree->num_halos * estimate_galaxy_factor(tree);
    int recommended_maxgals = estimated_galaxies * 1.5;  // 50% safety margin
    return recommended_maxgals;
}

int estimate_galaxy_factor(struct tree_info *tree) {
    // Heuristics based on tree depth, halo mass distribution, etc.
    // Conservative estimate: 5-10 galaxies per halo for large trees
    if (tree->num_halos > 100000) return 8;
    if (tree->num_halos > 10000) return 5;
    return 3;
}
```

### 3. Tree-Scoped Memory Management

**Files**: `src/core/core_mymalloc.c`, `src/core/sage.c`

**Problem**: No efficient way to bulk-free memory between trees.

**Solution**: Add memory scoping to track and bulk-free tree-related allocations.

```c
// Memory scope tracking
static long TreeScopeStart = 0;

void begin_tree_memory_scope(void) {
    TreeScopeStart = Nblocks;
    LOG_DEBUG("Starting tree memory scope at block %ld", TreeScopeStart);
}

void end_tree_memory_scope(void) {
    // Free all blocks allocated since scope start
    while (Nblocks > TreeScopeStart) {
        myfree(Table[Nblocks - 1]);
    }
    LOG_DEBUG("Freed %ld blocks in tree scope", Nblocks - TreeScopeStart);
}
```

### 4. Enhanced Memory Monitoring

**File**: `src/core/core_mymalloc.c`

**Problem**: Limited visibility into memory usage patterns and pressure.

**Solution**: Add comprehensive memory monitoring and automatic expansion.

```c
void check_memory_pressure_and_expand(void) {
    // Expand block table if approaching limit
    if (Nblocks >= MaxBlocks - 1000) {  // 1000 block safety margin
        expand_block_table();
    }
    
    // Monitor memory usage patterns
    if (TotMem > HighMarkMem) {
        HighMarkMem = TotMem;
        LOG_INFO("Memory high water mark: %.2f MB with %ld blocks", 
                TotMem / (1024.0 * 1024.0), Nblocks);
    }
}
```

## File-by-File Implementation Guide

### `src/core/core_mymalloc.h`

Add new function declarations:

```c
// Memory system lifecycle
extern int memory_system_init(void);
extern void memory_system_cleanup(void);

// Tree-scoped memory management
extern void begin_tree_memory_scope(void);
extern void end_tree_memory_scope(void);

// Dynamic expansion
extern int expand_block_table(void);
extern void check_memory_pressure_and_expand(void);
```

### `src/core/core_mymalloc.c`

**Major Changes**:
1. Replace static arrays with dynamic allocation
2. Add block table expansion function
3. Implement tree memory scoping
4. Enhanced memory pressure monitoring
5. Add initialization and cleanup functions

### `src/core/macros.h`

**Changes**:
1. Increase `MAXGALFAC` to reasonable default (e.g., 5)
2. Add memory-related constants for expansion thresholds

### `src/core/sage.c`

**Changes**:
1. Call `memory_system_init()` during SAGE initialization
2. Add `begin_tree_memory_scope()` before processing each tree
3. Add `end_tree_memory_scope()` after processing each tree
4. Call `memory_system_cleanup()` during SAGE finalization

### `src/core/core_build_model.c`

**Changes**:
1. Add tree memory estimation before galaxy array allocation
2. Use improved sizing heuristics for initial `maxgals`
3. Enhanced error handling for memory allocation failures

## Expected Results

### Immediate Benefits

1. **No More Segfaults**: Eliminates artificial memory limits that cause crashes
2. **Scalable Memory**: Handles simulations requiring hundreds of MB to GB of memory
3. **Efficient Usage**: Memory released between trees, preventing accumulation
4. **Better Monitoring**: Clear visibility into memory usage patterns

### Performance Characteristics

- **Memory Overhead**: <5% additional overhead for dynamic management
- **Allocation Speed**: Minimal impact on allocation/deallocation performance
- **Memory Efficiency**: Significant improvement in memory utilization
- **Scalability**: Linear scaling with simulation size, no artificial limits

### Support for Large Simulations

- **Full Physics Mode**: Supports 20-30 physics modules with complex galaxy properties
- **Large Trees**: Handles trees with millions of halos and tens of millions of galaxies
- **Memory Cap**: Respects system memory limits (configurable up to 16GB or available RAM)

## Testing Strategy

### Unit Tests

1. **Block Table Expansion**: Test dynamic growth under memory pressure
2. **Tree Scoping**: Verify correct bulk deallocation between trees
3. **Memory Estimation**: Test sizing heuristics with various tree types
4. **Error Handling**: Test behavior at system memory limits

### Integration Tests

1. **Large Tree Processing**: Process trees requiring >1GB memory
2. **Multi-Module Physics**: Run full physics mode with all modules enabled
3. **Long-Running Simulations**: Process thousands of trees to verify no memory leaks
4. **Memory Pressure**: Test behavior when approaching system memory limits

### Performance Benchmarks

1. **Memory Usage**: Compare before/after memory consumption patterns
2. **Processing Speed**: Ensure no significant performance regression
3. **Scalability**: Test with increasingly large simulations

## Migration Path

### Phase 1: Core Infrastructure
- Implement dynamic block table expansion
- Add memory system initialization/cleanup
- Update core allocation functions

### Phase 2: Tree Scoping
- Add tree memory scope management
- Integrate with tree processing loop
- Test with small/medium simulations

### Phase 3: Enhanced Sizing
- Implement intelligent memory estimation
- Update galaxy array sizing heuristics
- Test with large simulations

### Phase 4: Monitoring and Optimization
- Add comprehensive memory monitoring
- Optimize expansion algorithms
- Performance tuning and benchmarking

## Risk Assessment

### Low Risk
- **Backwards Compatibility**: All existing allocation code continues to work
- **Incremental Implementation**: Can be deployed in phases
- **Fallback Options**: System gracefully degrades if expansion fails

### Medium Risk
- **Memory Estimation**: Heuristics may need tuning for different simulation types
- **Performance Impact**: Small overhead from dynamic management

### Mitigation Strategies
- **Extensive Testing**: Comprehensive test suite covering edge cases
- **Conservative Defaults**: Start with safe, generous memory allocations
- **Monitoring**: Detailed logging to identify and fix issues quickly
- **Rollback Plan**: Can revert to static allocation if serious issues arise

## Success Criteria

1. **Functionality**: SAGE runs successfully on large simulations without segfaults
2. **Performance**: <10% performance overhead compared to current system
3. **Scalability**: Successfully processes simulations requiring 5-10x current memory usage
4. **Stability**: No memory leaks or corruption in long-running simulations
5. **Maintainability**: Code remains clean and understandable for future developers

## Future Enhancements

### Potential Optimizations
- **Memory Compression**: Compress inactive tree data to reduce memory footprint
- **Streaming I/O**: Process very large trees in chunks rather than loading entirely
- **NUMA Awareness**: Optimize memory allocation for multi-socket systems
- **Memory-Mapped I/O**: Use mmap for tree data to reduce memory pressure

### Monitoring Improvements
- **Real-time Dashboards**: Web interface for monitoring memory usage during runs
- **Predictive Analytics**: Machine learning to predict optimal memory allocation
- **Automatic Tuning**: Self-adjusting parameters based on simulation characteristics

This proposal provides a robust, scalable solution for SAGE's memory management that respects the semi-analytic model's tree-by-tree processing pattern while removing artificial limits that prevent large-scale simulations.