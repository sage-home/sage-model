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

#define GALAXY_ARRAY_INITIAL_CAPACITY 256

// The internal structure is hidden from the rest of the application.
struct GalaxyArray {
    struct GALAXY* galaxies;
    int count;
    int capacity;
};

// Safe deep copy function that only copies fields that actually exist in GALAXY struct
static inline void safe_deep_copy_galaxy(struct GALAXY *dest, const struct GALAXY *src, const struct params *run_params)
{
    // CRITICAL FIX: Avoid shallow copy that creates shared properties pointers
    // Instead, manually copy each field to prevent dangerous pointer aliasing
    
    // Core galaxy identification
    dest->SnapNum = src->SnapNum;
    dest->Type = src->Type;
    dest->GalaxyNr = src->GalaxyNr;
    dest->CentralGal = src->CentralGal;
    dest->HaloNr = src->HaloNr;
    dest->MostBoundID = src->MostBoundID;
    dest->GalaxyIndex = src->GalaxyIndex;
    dest->CentralGalaxyIndex = src->CentralGalaxyIndex;
    dest->mergeType = src->mergeType;
    dest->mergeIntoID = src->mergeIntoID;
    dest->mergeIntoSnapNum = src->mergeIntoSnapNum;
    dest->dT = src->dT;
    
    // Spatial coordinates and arrays
    for (int i = 0; i < 3; i++) {
        dest->Pos[i] = src->Pos[i];
        dest->Vel[i] = src->Vel[i];
    }
    
    // Core halo properties
    dest->Len = src->Len;
    dest->Mvir = src->Mvir;
    dest->deltaMvir = src->deltaMvir;
    dest->CentralMvir = src->CentralMvir;
    dest->Rvir = src->Rvir;
    dest->Vvir = src->Vvir;
    dest->Vmax = src->Vmax;
    
    // Core merger tracking
    dest->MergTime = src->MergTime;
    
    // Core infall properties
    dest->infallMvir = src->infallMvir;
    dest->infallVvir = src->infallVvir;
    dest->infallVmax = src->infallVmax;
    
    // Extension mechanism - copy extension metadata but not data pointers
    dest->extension_data = NULL;  // Will be reallocated if needed
    dest->num_extensions = 0;
    dest->extension_flags = 0;
    
    // CRITICAL: Initialize properties to NULL before deep copy
    dest->properties = NULL;
    
    // Deep copy the dynamic properties structure
    if (copy_galaxy_properties(dest, src, run_params) != 0) {
        LOG_ERROR("Failed to deep copy galaxy properties during safe copy operation");
        // Don't return error here as we want to continue with partial copy
    }
    
    // Copy extension data (this will just copy flags since the data is accessed on demand)
    galaxy_extension_copy(dest, src);
}

// Internal function to safely expand the array.
static int galaxy_array_expand(GalaxyArray* arr) {
    int new_capacity = arr->capacity == 0 ? GALAXY_ARRAY_INITIAL_CAPACITY : arr->capacity * 2;
    LOG_DEBUG("Expanding galaxy array from %d to %d capacity.", arr->capacity, new_capacity);

    // 1. Before realloc, save all valid `properties` pointers.
    galaxy_properties_t** temp_props = mymalloc(arr->count * sizeof(galaxy_properties_t*));
    if (!temp_props) {
        LOG_ERROR("Failed to allocate temporary properties pointer array for expansion.");
        return -1;
    }
    for (int i = 0; i < arr->count; ++i) {
        temp_props[i] = arr->galaxies[i].properties;
    }

    // 2. Perform the reallocation.
    struct GALAXY* new_array = myrealloc(arr->galaxies, new_capacity * sizeof(struct GALAXY));
    if (!new_array) {
        myfree(temp_props);
        LOG_ERROR("Failed to reallocate galaxy array to new capacity %d.", new_capacity);
        return -1;
    }

    arr->galaxies = new_array;
    arr->capacity = new_capacity;

    // 3. Restore the `properties` pointers to their new locations.
    for (int i = 0; i < arr->count; ++i) {
        arr->galaxies[i].properties = temp_props[i];
    }

    myfree(temp_props);
    return 0; // Success
}

// API Implementation
GalaxyArray* galaxy_array_new(void) {
    GalaxyArray* arr = mymalloc(sizeof(GalaxyArray));
    if (!arr) {
        LOG_ERROR("Failed to allocate GalaxyArray structure");
        return NULL;
    }
    arr->galaxies = NULL;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

void galaxy_array_free(GalaxyArray* arr) {
    if (!arr) return;
    for (int i = 0; i < arr->count; ++i) {
        free_galaxy_properties(&arr->galaxies[i]);
    }
    if (arr->galaxies) {
        myfree(arr->galaxies);
    }
    myfree(arr);
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
    safe_deep_copy_galaxy(dest, galaxy, p);

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
    return arr ? arr->galaxies : NULL;
}