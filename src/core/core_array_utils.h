#pragma once

#include <stdlib.h>
#include <stdbool.h>

// Forward declaration - full definition is in core_allvars.h
// For standalone testing, test files should define this struct
struct GALAXY;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file core_array_utils.h
 * @brief Utility functions for dynamic array management
 *
 * This file provides functions for more efficient array resizing, including
 * geometric growth strategies to reduce the number of reallocation operations.
 */

/**
 * @brief Default growth factor for dynamic arrays
 *
 * When resizing arrays, they will grow by this factor (e.g., 1.5 means
 * new_size = old_size * 1.5). This reduces the number of reallocations
 * compared to fixed-increment growth.
 */
#define ARRAY_DEFAULT_GROWTH_FACTOR 1.5

/**
 * @brief Minimum size for dynamically allocated arrays
 */
#define ARRAY_MIN_SIZE 16

/**
 * @brief Expand a dynamically allocated array using geometric growth
 *
 * Resizes the array to accommodate at least min_new_size elements, using
 * a geometric growth strategy to reduce the number of reallocations.
 *
 * @param array Pointer to the array pointer (will be updated if reallocated)
 * @param element_size Size of each array element in bytes
 * @param current_capacity Pointer to the current array capacity (will be updated)
 * @param min_new_size Minimum number of elements needed
 * @param growth_factor Factor to grow by (e.g., 1.5 means increase by 50%)
 * @return 0 on success, non-zero on failure
 */
extern int array_expand(void **array, size_t element_size, int *current_capacity, 
                      int min_new_size, float growth_factor);

/**
 * @brief Expand a dynamically allocated array with default growth factor
 *
 * Convenience function that calls array_expand with the default growth factor.
 *
 * @param array Pointer to the array pointer (will be updated if reallocated)
 * @param element_size Size of each array element in bytes
 * @param current_capacity Pointer to the current array capacity (will be updated)
 * @param min_new_size Minimum number of elements needed
 * @return 0 on success, non-zero on failure
 */
extern int array_expand_default(void **array, size_t element_size, int *current_capacity, int min_new_size);

/**
 * @brief Expand a galaxy array using geometric growth
 *
 * Convenience function for resizing GALAXY arrays with the default growth factor.
 *
 * @param array Pointer to the GALAXY array pointer (will be updated if reallocated)
 * @param current_capacity Pointer to the current array capacity (will be updated)
 * @param min_new_size Minimum number of galaxies needed
 * @return 0 on success, non-zero on failure
 */
extern int galaxy_array_expand(struct GALAXY **array, int *current_capacity, int min_new_size);

#ifdef __cplusplus
}
#endif
