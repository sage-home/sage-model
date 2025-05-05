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

int galaxy_array_expand(struct GALAXY **array, int *current_capacity, int min_new_size) {
    // This function needs to be used only after including core_allvars.h
    // Since core_allvars.h is included in all core_*.c files that use GALAXY structs,
    // this should be safe in practice, even though we forward declare in the header.
    #ifdef TESTING_STANDALONE
    size_t galaxy_size = 1024;  // Reasonable size estimate for testing
    #else
    size_t galaxy_size = sizeof(struct GALAXY);
    #endif
    
    // Save the old array pointer to check if it changed during realloc
    struct GALAXY *old_array = *array;
    
    // Expand the array
    int status = array_expand_default((void **)array, galaxy_size, current_capacity, min_new_size);
    
    // Special handling for galaxy properties if array pointer changed during reallocation
    if (status == 0 && old_array != *array) {
        LOG_DEBUG("Galaxy array was reallocated, fixing property pointers");
        
        // Properties requires special handling after reallocation
        // We need to manually update property pointers for already allocated galaxies
        for (int i = 0; i < min_new_size; i++) {
            // Skip galaxies that don't have properties allocated yet
            if ((*array)[i].properties != NULL) {
                // If the property was allocated, make sure it's properly kept through reallocation
                // No need to deep copy - just ensure pointer remains valid
                LOG_DEBUG("Fixing property pointer for galaxy %d", i);
                // We don't need to do anything here - the behavior of realloc should preserve the pointers
                // This space is for any additional fixes if needed
            }
        }
    }
    
    return status;
}
