/**
 * @file galaxy_array.h
 * @brief Safe dynamic array for GALAXY structs with proper pointer management
 * 
 * This module provides a safe abstraction for managing dynamic arrays of GALAXY
 * structs. It handles the complexity of memory reallocation while preserving
 * the integrity of properties pointers, eliminating the class of memory corruption
 * bugs caused by unsafe realloc operations on galaxy arrays.
 */

#pragma once

#include "core_allvars.h"

// Opaque struct to hide implementation details from the rest of the code.
typedef struct GalaxyArray GalaxyArray;

/**
 * @brief Creates a new, empty galaxy array with a default initial capacity.
 * @return A pointer to the new GalaxyArray, or NULL on failure.
 */
GalaxyArray* galaxy_array_new(void);

/**
 * @brief Frees the galaxy array and all the GALAXY structs it contains.
 * This function safely handles freeing the properties of each galaxy.
 * @param arr The GalaxyArray to free.
 */
void galaxy_array_free(GalaxyArray* arr);

/**
 * @brief Appends a copy of a galaxy to the array.
 * This function handles memory reallocation safely and transparently.
 * @param arr The GalaxyArray to append to.
 * @param galaxy A pointer to the galaxy to be copied and appended.
 * @param p A pointer to the simulation parameters, required for deep copying.
 * @return The index of the newly appended galaxy, or -1 on failure.
 */
int galaxy_array_append(GalaxyArray* arr, const struct GALAXY* galaxy, const struct params* p);

/**
 * @brief Gets a pointer to the galaxy at a specific index.
 * @param arr The GalaxyArray to access.
 * @param index The index of the galaxy.
 * @return A pointer to the GALAXY struct, or NULL if the index is out of bounds.
 */
struct GALAXY* galaxy_array_get(GalaxyArray* arr, int index);

/**
 * @brief Gets the number of galaxies currently stored in the array.
 * @param arr The GalaxyArray.
 * @return The number of galaxies.
 */
int galaxy_array_get_count(const GalaxyArray* arr);

/**
 * @brief Gets a direct pointer to the underlying contiguous C-array of galaxies.
 * @param arr The GalaxyArray.
 * @return A direct pointer to the galaxy data.
 * @warning Use with caution. The returned pointer may become invalid if the
 *          array is modified (e.g., by galaxy_array_append). Intended for
 *          performance-critical loops that only read data.
 */
struct GALAXY* galaxy_array_get_raw_data(GalaxyArray* arr);