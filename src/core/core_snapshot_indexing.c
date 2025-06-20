/**
 * @file core_snapshot_indexing.c
 * @brief Implementation of efficient indexing structures for snapshot-based halo processing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "core_snapshot_indexing.h"
#include "core_mymalloc.h"
#include "core_logging.h"

/**
 * @brief Initialize the snapshot indexing structure
 */
int snapshot_indices_init(struct forest_snapshot_indices *indices, 
                          int32_t max_snaps, int64_t total_halos)
{
    if (!indices) {
        LOG_ERROR("NULL indices pointer passed to snapshot_indices_init");
        return EXIT_FAILURE;
    }
    
    if (max_snaps <= 0 || total_halos <= 0) {
        LOG_ERROR("Invalid parameters: max_snaps=%d, total_halos=%ld", max_snaps, total_halos);
        return EXIT_FAILURE;
    }
    
    // Initialize the structure
    memset(indices, 0, sizeof(struct forest_snapshot_indices));
    indices->max_snapshots = max_snaps;
    indices->total_halos = total_halos;
    indices->is_initialized = false;
    
    // Allocate arrays for each snapshot
    indices->halos_per_snapshot = mymalloc(max_snaps * sizeof(struct halos_by_snapshot));
    indices->fof_per_snapshot = mymalloc(max_snaps * sizeof(struct fof_groups_by_snapshot));
    
    if (!indices->halos_per_snapshot || !indices->fof_per_snapshot) {
        LOG_ERROR("Failed to allocate memory for snapshot indexing arrays");
        snapshot_indices_cleanup(indices);
        return EXIT_FAILURE;
    }
    
    // Initialize each snapshot's indexing structure
    for (int32_t snap = 0; snap < max_snaps; snap++) {
        // Initialize halo indices for this snapshot
        indices->halos_per_snapshot[snap].halo_indices = NULL;
        indices->halos_per_snapshot[snap].count = 0;
        indices->halos_per_snapshot[snap].capacity = 0;
        
        // Initialize FOF group indices for this snapshot  
        indices->fof_per_snapshot[snap].fof_root_indices = NULL;
        indices->fof_per_snapshot[snap].count = 0;
        indices->fof_per_snapshot[snap].capacity = 0;
    }
    
    LOG_DEBUG("Initialized snapshot indexing for %d snapshots, %ld total halos", 
              max_snaps, total_halos);
    
    return EXIT_SUCCESS;
}

/**
 * @brief Helper function to add a halo index to a snapshot's halo list
 */
static int add_halo_to_snapshot(struct halos_by_snapshot *snap_halos, int32_t halo_idx)
{
    // Check if we need to expand capacity
    if (snap_halos->count >= snap_halos->capacity) {
        int32_t new_capacity = (snap_halos->capacity == 0) ? 16 : snap_halos->capacity * 2;
        int32_t *new_indices;
        
        if (snap_halos->halo_indices == NULL) {
            // Initial allocation
            new_indices = mymalloc(new_capacity * sizeof(int32_t));
        } else {
            // Expand existing allocation
            new_indices = myrealloc(snap_halos->halo_indices, new_capacity * sizeof(int32_t));
        }
        
        if (!new_indices) {
            LOG_ERROR("Failed to expand halo indices array for snapshot");
            return EXIT_FAILURE;
        }
        snap_halos->halo_indices = new_indices;
        snap_halos->capacity = new_capacity;
    }
    
    // Add the halo index
    snap_halos->halo_indices[snap_halos->count] = halo_idx;
    snap_halos->count++;
    
    return EXIT_SUCCESS;
}

/**
 * @brief Helper function to add a FOF group root index to a snapshot's FOF list
 */
static int add_fof_group_to_snapshot(struct fof_groups_by_snapshot *snap_fof, int32_t fof_root_idx)
{
    // Check if we need to expand capacity
    if (snap_fof->count >= snap_fof->capacity) {
        int32_t new_capacity = (snap_fof->capacity == 0) ? 8 : snap_fof->capacity * 2;
        int32_t *new_indices;
        
        if (snap_fof->fof_root_indices == NULL) {
            // Initial allocation
            new_indices = mymalloc(new_capacity * sizeof(int32_t));
        } else {
            // Expand existing allocation
            new_indices = myrealloc(snap_fof->fof_root_indices, new_capacity * sizeof(int32_t));
        }
        
        if (!new_indices) {
            LOG_ERROR("Failed to expand FOF indices array for snapshot");
            return EXIT_FAILURE;
        }
        snap_fof->fof_root_indices = new_indices;
        snap_fof->capacity = new_capacity;
    }
    
    // Add the FOF root index
    snap_fof->fof_root_indices[snap_fof->count] = fof_root_idx;
    snap_fof->count++;
    
    return EXIT_SUCCESS;
}

/**
 * @brief Build the complete indexing structure for a forest
 */
int snapshot_indices_build(struct forest_snapshot_indices *indices,
                          const struct halo_data *halos, int64_t nhalos)
{
    if (!indices || !halos) {
        LOG_ERROR("NULL pointer passed to snapshot_indices_build");
        return EXIT_FAILURE;
    }
    
    if (nhalos <= 0) {
        LOG_WARNING("No halos to process in snapshot_indices_build");
        indices->is_initialized = true;
        return EXIT_SUCCESS;
    }
    
    LOG_DEBUG("Building snapshot indices for %ld halos", nhalos);
    
    // Track which FOF groups we've already processed for each snapshot
    // This prevents duplicates when multiple halos point to the same FOF root
    bool *fof_processed = mymalloc(nhalos * sizeof(bool));
    if (!fof_processed) {
        LOG_ERROR("Failed to allocate memory for FOF tracking array");
        return EXIT_FAILURE;
    }
    memset(fof_processed, false, nhalos * sizeof(bool));
    
    // First pass: collect all halos by snapshot and identify FOF groups
    for (int64_t i = 0; i < nhalos; i++) {
        const struct halo_data *halo = &halos[i];
        int32_t snapshot = halo->SnapNum;
        
        // Validate snapshot number
        if (snapshot < 0 || snapshot >= indices->max_snapshots) {
            LOG_WARNING("Halo %ld has invalid snapshot number %d (max=%d)", 
                       i, snapshot, indices->max_snapshots - 1);
            continue;
        }
        
        // Add this halo to the snapshot's halo list
        if (add_halo_to_snapshot(&indices->halos_per_snapshot[snapshot], (int32_t)i) != EXIT_SUCCESS) {
            LOG_ERROR("Failed to add halo %ld to snapshot %d", i, snapshot);
            myfree(fof_processed);
            return EXIT_FAILURE;
        }
        
        // Check if this halo is a FOF group root
        int32_t fof_root = halo->FirstHaloInFOFgroup;
        if (fof_root == (int32_t)i && !fof_processed[i]) {
            // This halo is the root of its FOF group, add it to the FOF list
            if (add_fof_group_to_snapshot(&indices->fof_per_snapshot[snapshot], (int32_t)i) != EXIT_SUCCESS) {
                LOG_ERROR("Failed to add FOF group %ld to snapshot %d", i, snapshot);
                myfree(fof_processed);
                return EXIT_FAILURE;
            }
            fof_processed[i] = true;
        }
    }
    
    myfree(fof_processed);
    
    // Calculate memory usage
    size_t total_memory = 0;
    int32_t total_halos_indexed = 0;
    int32_t total_fof_groups = 0;
    
    for (int32_t snap = 0; snap < indices->max_snapshots; snap++) {
        total_memory += indices->halos_per_snapshot[snap].capacity * sizeof(int32_t);
        total_memory += indices->fof_per_snapshot[snap].capacity * sizeof(int32_t);
        total_halos_indexed += indices->halos_per_snapshot[snap].count;
        total_fof_groups += indices->fof_per_snapshot[snap].count;
    }
    
    indices->memory_pool_size = total_memory;
    indices->is_initialized = true;
    
    LOG_INFO("Snapshot indexing complete: %d halos across %d snapshots, %d FOF groups, %.2f KB memory",
             total_halos_indexed, indices->max_snapshots, total_fof_groups, 
             total_memory / 1024.0);
    
    return EXIT_SUCCESS;
}

/**
 * @brief Get array of halo indices for a specific snapshot
 */
const int32_t* snapshot_indices_get_halos(const struct forest_snapshot_indices *indices,
                                          int32_t snapshot, int32_t *count)
{
    if (!indices || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    if (!indices->is_initialized) {
        LOG_WARNING("Attempting to use uninitialized snapshot indices");
        *count = 0;
        return NULL;
    }
    
    if (snapshot < 0 || snapshot >= indices->max_snapshots) {
        *count = 0;
        return NULL;
    }
    
    *count = indices->halos_per_snapshot[snapshot].count;
    return indices->halos_per_snapshot[snapshot].halo_indices;
}

/**
 * @brief Get array of FOF group root indices for a specific snapshot
 */
const int32_t* snapshot_indices_get_fof_groups(const struct forest_snapshot_indices *indices,
                                               int32_t snapshot, int32_t *count)
{
    if (!indices || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    if (!indices->is_initialized) {
        LOG_WARNING("Attempting to use uninitialized snapshot indices");
        *count = 0;
        return NULL;
    }
    
    if (snapshot < 0 || snapshot >= indices->max_snapshots) {
        *count = 0;
        return NULL;
    }
    
    *count = indices->fof_per_snapshot[snapshot].count;
    return indices->fof_per_snapshot[snapshot].fof_root_indices;
}

/**
 * @brief Clean up and free all memory used by the indexing structure
 */
void snapshot_indices_cleanup(struct forest_snapshot_indices *indices)
{
    if (!indices) {
        return;
    }
    
    // Free individual snapshot arrays
    if (indices->halos_per_snapshot) {
        for (int32_t snap = 0; snap < indices->max_snapshots; snap++) {
            if (indices->halos_per_snapshot[snap].halo_indices) {
                myfree(indices->halos_per_snapshot[snap].halo_indices);
            }
        }
        myfree(indices->halos_per_snapshot);
    }
    
    if (indices->fof_per_snapshot) {
        for (int32_t snap = 0; snap < indices->max_snapshots; snap++) {
            if (indices->fof_per_snapshot[snap].fof_root_indices) {
                myfree(indices->fof_per_snapshot[snap].fof_root_indices);
            }
        }
        myfree(indices->fof_per_snapshot);
    }
    
    // Reset the structure
    memset(indices, 0, sizeof(struct forest_snapshot_indices));
    
    LOG_DEBUG("Cleaned up snapshot indexing structure");
}

/**
 * @brief Get memory usage statistics for the indexing structure
 */
void snapshot_indices_get_memory_stats(const struct forest_snapshot_indices *indices,
                                       size_t *total_bytes, double *overhead)
{
    if (!indices || !total_bytes || !overhead) {
        if (total_bytes) *total_bytes = 0;
        if (overhead) *overhead = 0.0;
        return;
    }
    
    if (!indices->is_initialized) {
        *total_bytes = 0;
        *overhead = 0.0;
        return;
    }
    
    *total_bytes = indices->memory_pool_size;
    
    // Calculate overhead as percentage of base halo data size
    size_t base_halo_size = indices->total_halos * sizeof(struct halo_data);
    if (base_halo_size > 0) {
        *overhead = (double)(*total_bytes) / base_halo_size * 100.0;
    } else {
        *overhead = 0.0;
    }
}