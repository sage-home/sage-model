/**
 * @file galaxy_array.c
 * @brief Implementation of safe dynamic array for GALAXY structs
 * 
 * This module provides a safe abstraction for managing dynamic arrays of GALAXY
 * structs. It handles the complexity of memory reallocation while preserving
 * the integrity of properties pointers, eliminating the class of memory corruption
 * bugs caused by unsafe realloc operations on galaxy arrays.
 */

#include "galaxy_array.h"
#include "core_mymalloc.h"
#include "core_logging.h"
#include "core_properties.h"
#include "core_galaxy_extensions.h"
#include "core_build_model.h"

#define GALAXY_ARRAY_INITIAL_CAPACITY 256

// The internal structure is hidden from the rest of the application.
struct GalaxyArray {
    struct GALAXY* galaxies;
    int count;
    int capacity;
};

// Using the simplified deep_copy_galaxy from core_build_model.c
// No need to duplicate complex copying logic here

// Internal function to safely expand the array.
static int galaxy_array_expand(GalaxyArray* arr) {
    int new_capacity = arr->capacity == 0 ? GALAXY_ARRAY_INITIAL_CAPACITY : arr->capacity * 2;
    /* Reduce noise - only log array expansion for first 5 expansions */
    static int expand_count = 0;
    expand_count++;
    if (expand_count <= 5) {
        if (expand_count == 5) {
            LOG_DEBUG("Expanding galaxy array from %d to %d capacity (expansion #%d - further messages suppressed)", arr->capacity, new_capacity, expand_count);
        } else {
            LOG_DEBUG("Expanding galaxy array from %d to %d capacity (expansion #%d)", arr->capacity, new_capacity, expand_count);
        }
    }

    // 1. Before realloc, save all valid `properties` pointers (only if we have existing galaxies).
    galaxy_properties_t** temp_props = NULL;
    if (arr->count > 0) {
        temp_props = mymalloc_full(arr->count * sizeof(galaxy_properties_t*), "GalaxyArray_temp_props");
        if (!temp_props) {
            LOG_ERROR("Failed to allocate temporary properties pointer array for expansion.");
            return -1;
        }
        for (int i = 0; i < arr->count; ++i) {
            temp_props[i] = arr->galaxies[i].properties;
        }
    }

    // 2. Perform the allocation/reallocation.
    struct GALAXY* new_array;
    if (arr->galaxies == NULL) {
        // First allocation
        new_array = mymalloc_full(new_capacity * sizeof(struct GALAXY), "GalaxyArray_expansion");
    } else {
        // Reallocation of existing array
        new_array = myrealloc(arr->galaxies, new_capacity * sizeof(struct GALAXY));
    }
    
    if (!new_array) {
        if (temp_props) myfree(temp_props);
        LOG_ERROR("Failed to allocate/reallocate galaxy array to new capacity %d.", new_capacity);
        return -1;
    }

    arr->galaxies = new_array;
    arr->capacity = new_capacity;

    // 3. Restore the `properties` pointers to their new locations (only if we had existing galaxies).
    if (temp_props) {
        for (int i = 0; i < arr->count; ++i) {
            arr->galaxies[i].properties = temp_props[i];
        }
        myfree(temp_props);
    }

    return 0; // Success
}

// API Implementation
GalaxyArray* galaxy_array_new(void) {
    GalaxyArray* arr = mymalloc_full(sizeof(GalaxyArray), "GalaxyArray_struct");
    if (!arr) {
        LOG_ERROR("Failed to allocate GalaxyArray structure");
        return NULL;
    }
    arr->galaxies = NULL;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

void galaxy_array_free(GalaxyArray** arr_ptr) {
    if (!arr_ptr || !*arr_ptr) return;
    GalaxyArray* arr = *arr_ptr;
    
    for (int i = 0; i < arr->count; ++i) {
        free_galaxy_properties(&arr->galaxies[i]);
    }
    if (arr->galaxies) {
        myfree(arr->galaxies);
    }
    myfree(arr);
    *arr_ptr = NULL;
}

int galaxy_array_append(GalaxyArray* arr, const struct GALAXY* galaxy, const struct params* p) {
    if (!arr) {
        LOG_ERROR("NULL GalaxyArray passed to galaxy_array_append");
        return -1;
    }
    if (!galaxy) {
        LOG_ERROR("NULL galaxy passed to galaxy_array_append");
        return -1;
    }
    if (!p) {
        LOG_ERROR("NULL params passed to galaxy_array_append");
        return -1;
    }
    
    
    if (arr->count >= arr->capacity) {
        if (galaxy_array_expand(arr) != 0) {
            return -1;
        }
    }

    struct GALAXY* dest = &arr->galaxies[arr->count];
    
    // Perform a full, safe deep copy of the GALAXY struct and its properties.
    deep_copy_galaxy(dest, galaxy, p);


    arr->count++;
    return arr->count - 1;
}

struct GALAXY* galaxy_array_get(GalaxyArray* arr, int index) {
    if (!arr || index < 0 || index >= arr->count) {
        return NULL;
    }
    return &arr->galaxies[index];
}

int galaxy_array_get_count(const GalaxyArray* arr) {
    return arr ? arr->count : 0;
}

struct GALAXY* galaxy_array_get_raw_data(GalaxyArray* arr) {
    if (!arr) return NULL;
    
    
    return arr->galaxies;
}