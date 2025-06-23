#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_allvars.h"
#include "core_memory_pool.h"
#include "core_mymalloc.h"
#include "core_logging.h"
#include "core_galaxy_extensions.h"

/**
 * Memory pool implementation structure
 */
struct memory_pool {
    size_t block_size;           /* Number of galaxies per block */
    size_t num_blocks;           /* Current number of blocks */
    size_t capacity;             /* Total galaxies capacity */
    size_t used;                 /* Number of galaxies in use */
    
    struct GALAXY **blocks;      /* Array of pointers to blocks */
    struct GALAXY **free_list;   /* Array of pointers to free galaxies */
    size_t free_list_size;       /* Current size of free list */
    size_t free_list_capacity;   /* Allocated capacity of free list */
    
    /* Statistics for monitoring */
    size_t alloc_count;          /* Total allocation count */
    size_t free_count;           /* Total free count */
    size_t peak_usage;           /* Peak usage count */
};

/* Global pool instance */
static struct memory_pool *global_pool = NULL;
static bool pool_enabled = false;

/**
 * Create a new memory pool
 */
struct memory_pool* galaxy_pool_create(size_t initial_capacity, size_t block_size) {
    /* Validate parameters */
    if (initial_capacity == 0) {
        initial_capacity = MEMORY_POOL_DEFAULT_INITIAL_CAPACITY;
    }
    
    if (block_size == 0) {
        block_size = MEMORY_POOL_DEFAULT_BLOCK_SIZE;
    }
    
    /* Adjust initial capacity to be a multiple of block size */
    size_t num_blocks = (initial_capacity + block_size - 1) / block_size;
    initial_capacity = num_blocks * block_size;
    
    /* Allocate pool structure */
    struct memory_pool *pool = (struct memory_pool *) mymalloc(sizeof(struct memory_pool));
    if (pool == NULL) {
        LOG_ERROR("Failed to allocate memory pool structure");
        return NULL;
    }
    
    /* Initialize pool state */
    pool->block_size = block_size;
    pool->num_blocks = 0;
    pool->capacity = 0;
    pool->used = 0;
    pool->free_list_size = 0;
    pool->free_list_capacity = MEMORY_POOL_DEFAULT_FREE_LIST_CAPACITY;
    pool->alloc_count = 0;
    pool->free_count = 0;
    pool->peak_usage = 0;
    
    /* Allocate blocks array */
    pool->blocks = (struct GALAXY **) mymalloc_full(num_blocks * sizeof(struct GALAXY *), "MemoryPool_blocks_array");
    if (pool->blocks == NULL) {
        LOG_ERROR("Failed to allocate block array for memory pool");
        myfree(pool);
        return NULL;
    }
    
    /* Allocate free list array */
    pool->free_list = (struct GALAXY **) mymalloc_full(pool->free_list_capacity * sizeof(struct GALAXY *), "MemoryPool_free_list");
    if (pool->free_list == NULL) {
        LOG_ERROR("Failed to allocate free list array for memory pool");
        myfree(pool->blocks);
        myfree(pool);
        return NULL;
    }
    
    /* Allocate initial blocks */
    for (size_t i = 0; i < num_blocks; i++) {
        pool->blocks[i] = (struct GALAXY *) mymalloc_full(block_size * sizeof(struct GALAXY), "MemoryPool_galaxy_block");
        if (pool->blocks[i] == NULL) {
            /* Clean up already allocated blocks */
            for (size_t j = 0; j < i; j++) {
                myfree(pool->blocks[j]);
            }
            myfree(pool->free_list);
            myfree(pool->blocks);
            myfree(pool);
            
            LOG_ERROR("Failed to allocate block %zu for memory pool", i);
            return NULL;
        }
        
        /* Initialize all galaxies in this block and add them to the free list */
        for (size_t j = 0; j < block_size; j++) {
            struct GALAXY *galaxy = &pool->blocks[i][j];
            
            /* Initialize extension fields */
            galaxy->extension_data = NULL;
            galaxy->num_extensions = 0;
            galaxy->extension_flags = 0;
            
            /* Add to free list */
            if (pool->free_list_size >= pool->free_list_capacity) {
                /* This shouldn't happen with initial allocation but handle it anyway */
                size_t new_capacity = (size_t)(pool->free_list_capacity * MEMORY_POOL_GROWTH_FACTOR) + 1;
                struct GALAXY **new_free_list = (struct GALAXY **) myrealloc(pool->free_list, 
                                                                            new_capacity * sizeof(struct GALAXY *));
                if (new_free_list == NULL) {
                    /* Failed to expand free list, clean up */
                    for (size_t k = 0; k <= i; k++) {
                        myfree(pool->blocks[k]);
                    }
                    myfree(pool->free_list);
                    myfree(pool->blocks);
                    myfree(pool);
                    
                    LOG_ERROR("Failed to expand free list for memory pool");
                    return NULL;
                }
                
                pool->free_list = new_free_list;
                pool->free_list_capacity = new_capacity;
            }
            
            pool->free_list[pool->free_list_size++] = galaxy;
        }
        
        pool->capacity += block_size;
    }
    
    pool->num_blocks = num_blocks;
    
    LOG_DEBUG("Created memory pool with %zu blocks, total capacity: %zu galaxies",
             num_blocks, pool->capacity);
    
    return pool;
}

/**
 * Allocate a GALAXY structure from the pool
 */
struct GALAXY* galaxy_pool_alloc(struct memory_pool* pool) {
    if (pool == NULL) {
        LOG_ERROR("Invalid memory pool");
        return NULL;
    }
    
    struct GALAXY *galaxy = NULL;
    
    /* Check if there are any free galaxies */
    if (pool->free_list_size > 0) {
        /* Pop a galaxy from the free list */
        galaxy = pool->free_list[--pool->free_list_size];
    } else {
        /* Need to allocate a new block */
        size_t new_block_idx = pool->num_blocks;
        struct GALAXY **new_blocks = (struct GALAXY **) myrealloc(pool->blocks, 
                                                                (new_block_idx + 1) * sizeof(struct GALAXY *));
        if (new_blocks == NULL) {
            LOG_ERROR("Failed to expand blocks array for memory pool");
            return NULL;
        }
        
        pool->blocks = new_blocks;
        
        /* Allocate new block */
        pool->blocks[new_block_idx] = (struct GALAXY *) mymalloc_full(pool->block_size * sizeof(struct GALAXY), "MemoryPool_expansion_block");
        if (pool->blocks[new_block_idx] == NULL) {
            LOG_ERROR("Failed to allocate new block for memory pool");
            return NULL;
        }
        
        /* Initialize all galaxies in this block except the first, which we'll return */
        galaxy = &pool->blocks[new_block_idx][0];
        
        /* Add remaining galaxies to free list */
        size_t remaining = pool->block_size - 1;
        if (remaining > 0) {
            /* Check if we need to expand the free list */
            if (pool->free_list_size + remaining > pool->free_list_capacity) {
                size_t new_capacity = (size_t)(pool->free_list_capacity * MEMORY_POOL_GROWTH_FACTOR) + remaining;
                struct GALAXY **new_free_list = (struct GALAXY **) myrealloc(pool->free_list,
                                                                          new_capacity * sizeof(struct GALAXY *));
                if (new_free_list == NULL) {
                    /* Failed to expand free list, but we can still return one galaxy */
                    LOG_WARNING("Failed to expand free list for memory pool, some galaxies in new block may be lost");
                } else {
                    pool->free_list = new_free_list;
                    pool->free_list_capacity = new_capacity;
                    
                    /* Add remaining galaxies to free list */
                    for (size_t i = 1; i < pool->block_size; i++) {
                        struct GALAXY *free_galaxy = &pool->blocks[new_block_idx][i];
                        
                        /* Initialize extension fields */
                        free_galaxy->extension_data = NULL;
                        free_galaxy->num_extensions = 0;
                        free_galaxy->extension_flags = 0;
                        
                        pool->free_list[pool->free_list_size++] = free_galaxy;
                    }
                }
            } else {
                /* Free list is big enough, add remaining galaxies */
                for (size_t i = 1; i < pool->block_size; i++) {
                    struct GALAXY *free_galaxy = &pool->blocks[new_block_idx][i];
                    
                    /* Initialize extension fields */
                    free_galaxy->extension_data = NULL;
                    free_galaxy->num_extensions = 0;
                    free_galaxy->extension_flags = 0;
                    
                    pool->free_list[pool->free_list_size++] = free_galaxy;
                }
            }
        }
        
        pool->num_blocks++;
        pool->capacity += pool->block_size;
        
        LOG_DEBUG("Expanded memory pool to %zu blocks, new capacity: %zu galaxies",
                 pool->num_blocks, pool->capacity);
    }
    
    /* Initialize extension fields */
    galaxy->extension_data = NULL;
    galaxy->num_extensions = 0;
    galaxy->extension_flags = 0;
    
    /* Initialize extensions properly */
    galaxy_extension_initialize(galaxy);
    
    /* Update statistics */
    pool->used++;
    pool->alloc_count++;
    if (pool->used > pool->peak_usage) {
        pool->peak_usage = pool->used;
    }
    
    return galaxy;
}

/**
 * Return a GALAXY structure to the pool
 */
void galaxy_pool_free(struct memory_pool* pool, struct GALAXY* galaxy) {
    if (pool == NULL || galaxy == NULL) {
        return;
    }
    
    /* Clean up extension data */
    galaxy_extension_cleanup(galaxy);
    
    /* Check if we need to expand the free list */
    if (pool->free_list_size >= pool->free_list_capacity) {
        size_t new_capacity = (size_t)(pool->free_list_capacity * MEMORY_POOL_GROWTH_FACTOR) + 1;
        struct GALAXY **new_free_list = (struct GALAXY **) myrealloc(pool->free_list,
                                                                    new_capacity * sizeof(struct GALAXY *));
        if (new_free_list == NULL) {
            LOG_ERROR("Failed to expand free list for memory pool, galaxy will be lost");
            return;
        }
        
        pool->free_list = new_free_list;
        pool->free_list_capacity = new_capacity;
    }
    
    /* Add galaxy to free list */
    pool->free_list[pool->free_list_size++] = galaxy;
    
    /* Update statistics */
    pool->used--;
    pool->free_count++;
}

/**
 * Destroy a memory pool
 */
void galaxy_pool_destroy(struct memory_pool* pool) {
    if (pool == NULL) {
        return;
    }
    
    /* Free all blocks */
    for (size_t i = 0; i < pool->num_blocks; i++) {
        /* Clean up any active extension data in this block */
        for (size_t j = 0; j < pool->block_size; j++) {
            galaxy_extension_cleanup(&pool->blocks[i][j]);
        }
        
        myfree(pool->blocks[i]);
    }
    
    /* Free arrays and pool structure */
    myfree(pool->free_list);
    myfree(pool->blocks);
    myfree(pool);
    
    LOG_DEBUG("Destroyed memory pool");
}

/**
 * Get pool usage statistics
 */
bool galaxy_pool_stats(struct memory_pool* pool, 
                      size_t* total_capacity,
                      size_t* currently_used,
                      size_t* allocation_count,
                      size_t* peak_usage) {
    if (pool == NULL) {
        return false;
    }
    
    if (total_capacity != NULL) {
        *total_capacity = pool->capacity;
    }
    
    if (currently_used != NULL) {
        *currently_used = pool->used;
    }
    
    if (allocation_count != NULL) {
        *allocation_count = pool->alloc_count;
    }
    
    if (peak_usage != NULL) {
        *peak_usage = pool->peak_usage;
    }
    
    return true;
}

/**
 * Initialize the global galaxy pool
 */
int galaxy_pool_initialize(void) {
    if (global_pool != NULL) {
        LOG_WARNING("Global galaxy pool already initialized");
        return 0;
    }
    
    global_pool = galaxy_pool_create(MEMORY_POOL_DEFAULT_INITIAL_CAPACITY, 
                                   MEMORY_POOL_DEFAULT_BLOCK_SIZE);
    if (global_pool == NULL) {
        LOG_ERROR("Failed to create global galaxy pool");
        return -1;
    }
    
    pool_enabled = true;
    LOG_INFO("Global galaxy memory pool initialized with capacity: %zu galaxies", 
            global_pool->capacity);
    
    return 0;
}

/**
 * Clean up the global galaxy pool
 */
int galaxy_pool_cleanup(void) {
    if (global_pool == NULL) {
        return 0;  /* Already cleaned up */
    }
    
    /* Set to NULL first to prevent double cleanup */
    struct memory_pool *pool_to_cleanup = global_pool;
    global_pool = NULL;
    pool_enabled = false;
    
    /* Log statistics before cleanup */
    size_t capacity, used, allocs, peak;
    if (galaxy_pool_stats(pool_to_cleanup, &capacity, &used, &allocs, &peak)) {
        LOG_INFO("Galaxy pool statistics: capacity=%zu, used=%zu, allocs=%zu, peak=%zu",
                capacity, used, allocs, peak);
    }
    
    /* Now safely destroy the pool */
    galaxy_pool_destroy(pool_to_cleanup);
    
    LOG_INFO("Global galaxy memory pool cleaned up");
    
    return 0;
}

/**
 * Allocate a GALAXY from the global pool
 */
struct GALAXY* galaxy_alloc(void) {
    if (global_pool != NULL && pool_enabled) {
        struct GALAXY *galaxy = galaxy_pool_alloc(global_pool);
        if (galaxy != NULL) {
            return galaxy;
        }
        
        /* Fall back to mymalloc if pool allocation failed */
        LOG_WARNING("Galaxy pool allocation failed, falling back to mymalloc");
    }
    
    /* Allocate with mymalloc */
    struct GALAXY *galaxy = (struct GALAXY *) mymalloc_full(sizeof(struct GALAXY), "MemoryPool_fallback_galaxy");
    if (galaxy != NULL) {
        /* Initialize extensions */
        galaxy->extension_data = NULL;
        galaxy->num_extensions = 0;
        galaxy->extension_flags = 0;
        galaxy_extension_initialize(galaxy);
    }
    
    return galaxy;
}

/**
 * Free a GALAXY to the global pool
 */
void galaxy_free(struct GALAXY* galaxy) {
    if (galaxy == NULL) {
        return;
    }
    
    if (global_pool != NULL && pool_enabled) {
        galaxy_pool_free(global_pool, galaxy);
    } else {
        /* Clean up extensions */
        galaxy_extension_cleanup(galaxy);
        myfree(galaxy);
    }
}

/**
 * Check if the global pool is enabled
 */
bool galaxy_pool_is_enabled(void) {
    return global_pool != NULL && pool_enabled;
}
