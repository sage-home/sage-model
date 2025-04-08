#pragma once

#include <stdlib.h>
#include <stdbool.h>

// Forward declaration - full definition is in core_allvars.h
struct GALAXY;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file core_memory_pool.h
 * @brief Memory pooling system for efficient GALAXY structure allocation
 *
 * This file provides a pooling mechanism to reduce allocation overhead and
 * memory fragmentation when allocating many GALAXY structures.
 */

/**
 * @brief Default initial capacity for memory pools
 */
#define MEMORY_POOL_DEFAULT_INITIAL_CAPACITY 1024

/**
 * @brief Default block size (galaxies per block)
 */
#define MEMORY_POOL_DEFAULT_BLOCK_SIZE 256

/**
 * @brief Default free list initial capacity
 */
#define MEMORY_POOL_DEFAULT_FREE_LIST_CAPACITY 256

/**
 * @brief Growth factor for pool expansion
 */
#define MEMORY_POOL_GROWTH_FACTOR 1.5f

/**
 * @brief Memory pool structure
 *
 * Manages pre-allocated blocks of GALAXY structures and tracks free galaxies.
 */
struct memory_pool;

/**
 * @brief Create a new memory pool
 *
 * @param initial_capacity Initial number of galaxies to pre-allocate
 * @param block_size Number of galaxies per allocation block
 * @return Pointer to the new memory pool, or NULL on failure
 */
extern struct memory_pool* galaxy_pool_create(size_t initial_capacity, size_t block_size);

/**
 * @brief Allocate a GALAXY structure from the pool
 *
 * Allocates a GALAXY structure from the pool, expanding the pool if necessary.
 * The returned GALAXY will have its extension tracking fields initialized,
 * but no extension data allocated yet.
 *
 * @param pool Memory pool to allocate from
 * @return Pointer to a GALAXY structure, or NULL on failure
 */
extern struct GALAXY* galaxy_pool_alloc(struct memory_pool* pool);

/**
 * @brief Return a GALAXY structure to the pool
 *
 * Returns a GALAXY structure to the pool for reuse. The function will call
 * galaxy_extension_cleanup() to release any extension data before returning
 * the GALAXY to the free list.
 *
 * @param pool Memory pool to return to
 * @param galaxy GALAXY structure to return
 */
extern void galaxy_pool_free(struct memory_pool* pool, struct GALAXY* galaxy);

/**
 * @brief Destroy a memory pool
 *
 * Releases all memory used by the pool and its blocks.
 *
 * @param pool Memory pool to destroy
 */
extern void galaxy_pool_destroy(struct memory_pool* pool);

/**
 * @brief Get pool usage statistics
 *
 * @param pool Memory pool to query
 * @param total_capacity Will be set to total capacity
 * @param currently_used Will be set to number of galaxies currently in use
 * @param allocation_count Will be set to total number of allocations
 * @param peak_usage Will be set to peak number of galaxies in use
 * @return True if statistics were successfully retrieved, false otherwise
 */
extern bool galaxy_pool_stats(struct memory_pool* pool, 
                             size_t* total_capacity,
                             size_t* currently_used,
                             size_t* allocation_count,
                             size_t* peak_usage);

/**
 * @brief Global pool functions for simplified usage
 */

/**
 * @brief Initialize the global galaxy pool
 *
 * Creates the global galaxy pool for use with the simplified allocation functions.
 * This function is called during SAGE initialization if the EnableGalaxyMemoryPool
 * runtime parameter is enabled.
 *
 * @return 0 on success, non-zero on failure
 */
extern int galaxy_pool_initialize(void);

/**
 * @brief Clean up the global galaxy pool
 *
 * Destroys the global galaxy pool and releases all associated memory.
 * This function is called during SAGE finalization.
 *
 * @return 0 on success, non-zero on failure
 */
extern int galaxy_pool_cleanup(void);

/**
 * @brief Allocate a GALAXY from the global pool
 *
 * Simplified function that uses the global pool for allocation.
 * If the global pool is not initialized or is disabled, falls back to using mymalloc.
 *
 * @return Pointer to a GALAXY structure, or NULL on failure
 */
extern struct GALAXY* galaxy_alloc(void);

/**
 * @brief Free a GALAXY to the global pool
 *
 * Simplified function that returns a GALAXY to the global pool.
 * If the global pool is not initialized or is disabled, falls back to using myfree.
 *
 * @param galaxy GALAXY structure to free
 */
extern void galaxy_free(struct GALAXY* galaxy);

/**
 * @brief Check if the global pool is enabled
 *
 * @return True if the global pool is initialized and enabled, false otherwise
 */
extern bool galaxy_pool_is_enabled(void);

#ifdef __cplusplus
}
#endif