# Memory Pool System Guide

## Overview

The memory pool system provides an efficient way to allocate and free GALAXY structures, reducing memory fragmentation and allocation overhead. It is particularly beneficial for SAGE's tree traversal process, which involves frequent allocation and deallocation of galaxy structures.

## Key Features

- Pre-allocation of GALAXY structures in blocks for reduced allocation overhead
- Free list management for efficient reuse of freed galaxies
- Geometric growth of pool capacity to handle varying workloads
- Full compatibility with the galaxy extension system
- Runtime configurable via the `EnableGalaxyMemoryPool` parameter

## Usage

### Runtime Configuration

The memory pool system is enabled by default. To disable it, add the following to your parameter file:

```
EnableGalaxyMemoryPool     0
```

### Programmatic Usage

In most cases, the memory pool is used automatically through the global instance, which is initialized during SAGE startup if enabled. Use the simplified functions:

```c
// Allocate a GALAXY
struct GALAXY *galaxy = galaxy_alloc();

// Free a GALAXY
galaxy_free(galaxy);
```

These functions will use the memory pool if enabled, or fall back to direct `mymalloc`/`myfree` calls if disabled.

### Advanced Usage

For more control, you can create and manage your own memory pools:

```c
// Create a pool with custom capacity and block size
struct memory_pool *pool = galaxy_pool_create(1024, 256);

// Allocate from the pool
struct GALAXY *galaxy = galaxy_pool_alloc(pool);

// Return to the pool
galaxy_pool_free(pool, galaxy);

// Get usage statistics
size_t capacity, used, allocs, peak;
galaxy_pool_stats(pool, &capacity, &used, &allocs, &peak);

// Destroy the pool when done
galaxy_pool_destroy(pool);
```

## Integration with Galaxy Extensions

The memory pool system is fully integrated with the galaxy extension system:

- When a GALAXY is allocated from the pool, `galaxy_extension_initialize()` is called automatically
- When a GALAXY is returned to the pool, `galaxy_extension_cleanup()` is called to free any extension data
- The core GALAXY struct is managed by the pool, while variable-sized extension data is managed separately

## Performance Considerations

- Memory pooling works best for workloads with roughly similar numbers of allocations and frees
- The pool pre-allocates memory, which increases initial memory usage but reduces fragmentation
- For very large simulations, consider adjusting initial pool capacity for optimal performance

## Implementation Details

- GALAXY structures are allocated in blocks of configurable size (default: 256 galaxies per block)
- The free list uses a separate array of pointers to track available galaxies
- When the pool is exhausted, new blocks are allocated with geometric growth
- Statistics are tracked for debugging and performance analysis

## Troubleshooting

If you encounter issues with the memory pooling system:

- Check if the pool is enabled: `if (galaxy_pool_is_enabled()) {...}`
- Examine pool statistics to identify potential resource constraints
- Verify that extension data is being properly cleaned up
- For large simulations, monitor memory usage to ensure appropriate sizing
