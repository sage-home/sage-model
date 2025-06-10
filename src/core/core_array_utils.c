#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>  // for off_t

// Include core_allvars.h when not in standalone testing mode
#ifndef TESTING_STANDALONE
#include "core_allvars.h"
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

/**
 * @brief Safe galaxy array expansion that preserves properties pointers
 * @param array Pointer to galaxy array pointer
 * @param current_capacity Pointer to current capacity
 * @param min_new_size Minimum new size needed
 * @param num_valid_galaxies Number of galaxies with valid data (for properties preservation)
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
    
    // Step 1: Save all valid properties pointers BEFORE reallocation
    galaxy_properties_t **temp_props = NULL;
    if (num_valid_galaxies > 0) {
        temp_props = mymalloc(num_valid_galaxies * sizeof(galaxy_properties_t*));
        if (!temp_props) {
            LOG_ERROR("Failed to allocate temporary properties pointer array");
            return -1;
        }
        for (int i = 0; i < num_valid_galaxies; i++) {
            temp_props[i] = (*array)[i].properties;
        }
        printf("SEGFAULT-FIX: Saved %d properties pointers before galaxy array reallocation\n", num_valid_galaxies);  // TODO: Remove after segfault resolved
        LOG_DEBUG("Saved %d properties pointers before reallocation", num_valid_galaxies);
    }
    
    // Step 2: Perform the reallocation
    int status = array_expand_default((void **)array, galaxy_size, current_capacity, min_new_size);
    
    // Step 3: Restore properties pointers if the array was moved
    if (status == 0 && old_array != *array && num_valid_galaxies > 0) {
        printf("SEGFAULT-FIX: Galaxy array reallocated from %p to %p - restoring %d properties pointers\n", 
               (void*)old_array, (void*)*array, num_valid_galaxies);  // TODO: Remove after segfault resolved
        LOG_DEBUG("Galaxy array reallocated from %p to %p - restoring %d properties pointers", 
                 old_array, *array, num_valid_galaxies);
        
        // Restore the properties pointers to their new locations
        for (int i = 0; i < num_valid_galaxies; i++) {
            (*array)[i].properties = temp_props[i];
        }
        
        printf("SEGFAULT-FIX: Successfully restored %d properties pointers after reallocation\n", num_valid_galaxies);  // TODO: Remove after segfault resolved
        LOG_DEBUG("Successfully restored %d properties pointers after reallocation", num_valid_galaxies);
    }
    
    // Clean up temporary array
    if (temp_props) {
        myfree(temp_props);
    }
    
    // Initialize any new memory region
    if (status == 0 && *current_capacity > num_valid_galaxies) {
        int new_elements = *current_capacity - num_valid_galaxies;
        memset(&(*array)[num_valid_galaxies], 0, new_elements * sizeof(struct GALAXY));
        LOG_DEBUG("Zero-initialized %d new galaxy slots", new_elements);
    }
    
    return status;
}

/* Old unsafe galaxy_array_expand function has been removed */
