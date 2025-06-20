/**
 * @file core_snapshot_indexing.h
 * @brief Efficient indexing structures for snapshot-based halo processing
 * 
 * This module provides data structures and functions to enable efficient
 * snapshot-based processing of merger trees. Instead of the inefficient
 * O(snapshots × halos) nested loop approach, these indices allow O(1)
 * access to halos and FOF groups for any given snapshot.
 * 
 * Key Features:
 * - Snapshot-based halo indexing for direct access
 * - FOF group root identification per snapshot  
 * - Memory-efficient storage with minimal overhead
 * - One-time preprocessing cost for forest-wide benefit
 */

#pragma once

#include "core_allvars.h"
#include <stdint.h>

/**
 * @brief Index structure for halos belonging to a specific snapshot
 * 
 * This structure contains indices of all halos that exist at a particular
 * snapshot, enabling direct access without scanning the entire halo array.
 */
struct halos_by_snapshot {
    int32_t *halo_indices;     /* Array of halo indices for this snapshot */
    int32_t count;             /* Number of halos in this snapshot */
    int32_t capacity;          /* Allocated capacity (for growth) */
};

/**
 * @brief Index structure for FOF group roots at a specific snapshot
 * 
 * This structure contains indices of halo numbers that are FOF group roots
 * (FirstHaloInFOFgroup points to themselves) at a particular snapshot.
 * This enables efficient FOF-level processing without nested searching.
 */
struct fof_groups_by_snapshot {
    int32_t *fof_root_indices;  /* Array of FOF group root halo indices */
    int32_t count;              /* Number of FOF groups in this snapshot */
    int32_t capacity;           /* Allocated capacity (for growth) */
};

/**
 * @brief Complete snapshot indexing structure for a forest
 * 
 * This structure contains all indexing information needed for efficient
 * snapshot-based processing of a merger tree forest.
 */
struct forest_snapshot_indices {
    struct halos_by_snapshot *halos_per_snapshot;    /* Array indexed by snapshot number */
    struct fof_groups_by_snapshot *fof_per_snapshot; /* Array indexed by snapshot number */
    int32_t max_snapshots;                           /* Maximum snapshot number + 1 */
    int64_t total_halos;                            /* Total number of halos in forest */
    
    /* Memory management */
    bool is_initialized;                            /* Whether indices have been built */
    void *memory_pool;                             /* Single allocation for all index arrays */
    size_t memory_pool_size;                       /* Size of the memory pool */
};

/**
 * @brief Initialize the snapshot indexing structure
 * 
 * @param indices     Pointer to the indexing structure to initialize
 * @param max_snaps   Maximum number of snapshots to support
 * @param total_halos Total number of halos in the forest (for memory estimation)
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int snapshot_indices_init(struct forest_snapshot_indices *indices, 
                          int32_t max_snaps, int64_t total_halos);

/**
 * @brief Build the complete indexing structure for a forest
 * 
 * This function processes the halo array once to build all indexing structures.
 * It should be called immediately after loading the forest data.
 * 
 * @param indices     Pointer to the indexing structure (must be initialized)
 * @param halos       Array of halo data for the forest
 * @param nhalos      Number of halos in the forest
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int snapshot_indices_build(struct forest_snapshot_indices *indices,
                          const struct halo_data *halos, int64_t nhalos);

/**
 * @brief Get array of halo indices for a specific snapshot
 * 
 * @param indices     Pointer to the indexing structure
 * @param snapshot    Snapshot number to query
 * @param count       Pointer to store the number of halos (output)
 * @return Pointer to array of halo indices, or NULL if snapshot has no halos
 */
const int32_t* snapshot_indices_get_halos(const struct forest_snapshot_indices *indices,
                                          int32_t snapshot, int32_t *count);

/**
 * @brief Get array of FOF group root indices for a specific snapshot
 * 
 * @param indices     Pointer to the indexing structure  
 * @param snapshot    Snapshot number to query
 * @param count       Pointer to store the number of FOF groups (output)
 * @return Pointer to array of FOF root indices, or NULL if snapshot has no FOF groups
 */
const int32_t* snapshot_indices_get_fof_groups(const struct forest_snapshot_indices *indices,
                                               int32_t snapshot, int32_t *count);

/**
 * @brief Clean up and free all memory used by the indexing structure
 * 
 * @param indices     Pointer to the indexing structure to cleanup
 */
void snapshot_indices_cleanup(struct forest_snapshot_indices *indices);

/**
 * @brief Get memory usage statistics for the indexing structure
 * 
 * @param indices     Pointer to the indexing structure
 * @param total_bytes Pointer to store total memory usage (output)
 * @param overhead    Pointer to store overhead percentage (output)
 */
void snapshot_indices_get_memory_stats(const struct forest_snapshot_indices *indices,
                                       size_t *total_bytes, double *overhead);