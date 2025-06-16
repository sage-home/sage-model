#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>  // for off_t

// Include core_allvars.h when not in standalone testing mode
#ifndef TESTING_STANDALONE
#include "core_allvars.h"
#include "core_properties.h"  // For GALAXY_PROP_* macros
#endif

#include "core_mymalloc.h"
#include "core_array_utils.h"

// Define simple printf-based versions of logging functions for testing
// These will be overridden by the real logging functions when linked with the full SAGE code
#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef LOG_DEBUG
#define LOG_DEBUG(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
#endif

#ifndef LOG_WARNING
#define LOG_WARNING(fmt, ...) fprintf(stderr, "WARNING: " fmt "\n", ##__VA_ARGS__)
#endif

/**
 * @file core_array_utils.c
 * @brief Implementation of utility functions for dynamic array management
 */

int array_expand(void **array, size_t element_size, int *current_capacity, 
               int min_new_size, float growth_factor) {
    if (array == NULL || current_capacity == NULL || element_size == 0) {
        LOG_ERROR("Invalid parameters passed to array_expand");
        return -1;
    }
    
    // If the current capacity is already sufficient, no need to resize
    if (*current_capacity >= min_new_size) {
        return 0;
    }
    
    // Ensure the growth factor is reasonable
    if (growth_factor < 1.1f) {
        growth_factor = 1.1f;  // Minimum growth of 10%
    }
    
    // Calculate new size with geometric growth
    int new_capacity = *current_capacity;
    
    if (new_capacity < ARRAY_MIN_SIZE) {
        new_capacity = ARRAY_MIN_SIZE;  // Start with a minimum reasonable size
    }
    
    // Keep growing until we have enough space
    while (new_capacity < min_new_size) {
        new_capacity = (int)(new_capacity * growth_factor) + 1;
    }
    
    LOG_DEBUG("Expanding array from %d to %d elements (%.2f MB)", 
             *current_capacity, new_capacity, 
             (element_size * new_capacity) / (1024.0 * 1024.0));
    
    // Reallocate array with new size
    void *new_array = myrealloc(*array, new_capacity * element_size);
    if (new_array == NULL) {
        LOG_ERROR("Failed to reallocate array memory from %d to %d elements", 
                 *current_capacity, new_capacity);
        return -1;
    }
    
    // Update array pointer and capacity
    *array = new_array;
    *current_capacity = new_capacity;
    
    return 0;
}

int array_expand_default(void **array, size_t element_size, int *current_capacity, int min_new_size) {
    return array_expand(array, element_size, current_capacity, min_new_size, ARRAY_DEFAULT_GROWTH_FACTOR);
}

// Forward declaration for property copying function
#ifndef TESTING_STANDALONE
extern int copy_galaxy_properties(struct GALAXY *dest, const struct GALAXY *src, const struct params *params);
extern void free_galaxy_properties(struct GALAXY *g);
#endif

/**
 * @brief Safe galaxy array expansion that preserves all galaxy data
 * @param array Pointer to galaxy array pointer
 * @param current_capacity Pointer to current capacity
 * @param min_new_size Minimum new size needed
 * @param num_valid_galaxies Number of galaxies with valid data (for preservation)
 * @return 0 on success, negative on error
 */
int galaxy_array_expand(struct GALAXY **array, int *current_capacity, int min_new_size, int num_valid_galaxies) {
    #ifdef TESTING_STANDALONE
    size_t galaxy_size = 1024;  // Reasonable size estimate for testing
    #else
    size_t galaxy_size = sizeof(struct GALAXY);
    #endif
    
    // If the current capacity is already sufficient, no need to resize
    if (*current_capacity >= min_new_size) {
        return 0;
    }
    
    // Save the old array pointer to detect reallocation
    struct GALAXY *old_array = *array;
    
    #ifndef TESTING_STANDALONE
    // Save galaxy data before reallocation to prevent corruption
    struct GALAXY *temp_galaxies = NULL;
    galaxy_properties_t **saved_properties = NULL;
    if (num_valid_galaxies > 0) {
        // Allocate temporary storage for complete galaxy structures
        temp_galaxies = mymalloc_full(num_valid_galaxies * sizeof(struct GALAXY), "ArrayUtils_temp_backup");
        if (!temp_galaxies) {
            LOG_ERROR("Failed to allocate temporary galaxy backup array");
            return -1;
        }
        
        // Allocate temporary storage for properties pointers
        saved_properties = mymalloc(num_valid_galaxies * sizeof(galaxy_properties_t*));
        if (!saved_properties) {
            LOG_ERROR("Failed to allocate temporary properties pointer array");
            myfree(temp_galaxies);
            return -1;
        }
        
        // Save both the complete galaxy data AND the properties pointers
        for (int i = 0; i < num_valid_galaxies; i++) {
            // Save the complete galaxy structure (all direct fields)
            memcpy(&temp_galaxies[i], &(*array)[i], sizeof(struct GALAXY));
            // Save the properties pointer separately (will remain valid across reallocation)
            saved_properties[i] = (*array)[i].properties;
        }
        LOG_DEBUG("Saved complete galaxy data and properties pointers for %d galaxies before reallocation", num_valid_galaxies);
    }
    #endif
    
    // Perform the reallocation
    int status = array_expand_default((void **)array, galaxy_size, current_capacity, min_new_size);
    
    #ifndef TESTING_STANDALONE
    // Restore galaxy data if the array was moved
    if (status == 0 && old_array != *array && num_valid_galaxies > 0) {
        LOG_DEBUG("Galaxy array reallocated from %p to %p - restoring complete galaxy data", 
                 old_array, *array);
        
        // Restore the complete galaxy data and fix the properties pointers
        for (int i = 0; i < num_valid_galaxies; i++) {
            // Restore the complete galaxy structure (all direct fields)
            memcpy(&(*array)[i], &temp_galaxies[i], sizeof(struct GALAXY));
            // Fix the properties pointer to the correct saved address
            (*array)[i].properties = saved_properties[i];
            
            // No sync needed - single source of truth through property system
            // Core properties accessed only via GALAXY_PROP_* macros
        }
        
        LOG_DEBUG("Successfully restored complete galaxy data and properties pointers for %d galaxies", num_valid_galaxies);
    }
    
    // Clean up temporary storage
    if (temp_galaxies) {
        myfree(temp_galaxies);
    }
    if (saved_properties) {
        myfree(saved_properties);
    }
    #else
    // For testing mode, fall back to simple memcpy (no properties to worry about)
    struct GALAXY *temp_galaxies = NULL;
    if (num_valid_galaxies > 0) {
        temp_galaxies = mymalloc_full(num_valid_galaxies * sizeof(struct GALAXY), "ArrayUtils_testing_backup");
        if (!temp_galaxies) {
            LOG_ERROR("Failed to allocate temporary galaxy backup array");
            return -1;
        }
        memcpy(temp_galaxies, *array, num_valid_galaxies * sizeof(struct GALAXY));
        LOG_DEBUG("Saved %d complete galaxies before reallocation", num_valid_galaxies);
    }
    
    if (status == 0 && old_array != *array && num_valid_galaxies > 0) {
        LOG_DEBUG("Galaxy array reallocated from %p to %p - restoring %d complete galaxies", 
                 old_array, *array, num_valid_galaxies);
        
        memcpy(*array, temp_galaxies, num_valid_galaxies * sizeof(struct GALAXY));
        
        LOG_DEBUG("Successfully restored %d complete galaxies after reallocation", num_valid_galaxies);
    }
    
    if (temp_galaxies) {
        myfree(temp_galaxies);
    }
    #endif
    
    // Initialize any new memory region
    if (status == 0 && *current_capacity > num_valid_galaxies) {
        int new_elements = *current_capacity - num_valid_galaxies;
        memset(&(*array)[num_valid_galaxies], 0, new_elements * sizeof(struct GALAXY));
        LOG_DEBUG("Zero-initialized %d new galaxy slots", new_elements);
    }
    
    return status;
}

/* Old unsafe galaxy_array_expand function has been removed */
