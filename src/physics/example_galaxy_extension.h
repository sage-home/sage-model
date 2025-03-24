#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "../core/core_module_system.h"
#include "../core/core_galaxy_extensions.h"

/**
 * @file example_galaxy_extension.h
 * @brief Example module demonstrating galaxy property extensions
 * 
 * This file provides an example of how to use the galaxy extension mechanism
 * to add custom properties to galaxies.
 */

/**
 * Example extension data structure
 * 
 * Contains custom properties that will be added to galaxies.
 */
typedef struct {
    float h2_fraction;               /* Molecular hydrogen fraction */
    float pressure;                  /* ISM pressure */
    struct {
        float radius;                /* Radius of region */
        float sfr;                   /* Star formation rate */
    } regions[5];                   /* Multiple star forming regions */
    int num_regions;                 /* Number of active regions */
} example_extension_data_t;

/**
 * Initialize the example extension
 * 
 * Registers the example extension properties with the galaxy extension system.
 * 
 * @param module_id ID of the module that will own the extensions
 * @return Extension ID on success, negative error code on failure
 */
int initialize_example_extension(int module_id);

/**
 * Access example extension data for a galaxy
 * 
 * Gets a pointer to the example extension data for a galaxy, allocating it
 * if it hasn't been allocated yet.
 * 
 * @param galaxy Pointer to the galaxy
 * @param extension_id ID of the extension
 * @return Pointer to the extension data, or NULL if not found
 */
example_extension_data_t *get_example_extension_data(struct GALAXY *galaxy, int extension_id);

/**
 * Demonstrate extension usage
 * 
 * Shows how to use the extension mechanism to add custom properties to galaxies.
 * 
 * @param galaxy Pointer to the galaxy
 * @param extension_id ID of the extension
 */
void demonstrate_extension_usage(struct GALAXY *galaxy, int extension_id);

#ifdef __cplusplus
}
#endif
