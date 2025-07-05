#include <stdio.h>
#include <stdlib.h>
#include "../../src/core/core_allvars.h"

/* Extension system stubs for standalone testing */
struct galaxy_extension_registry *global_extension_registry = NULL;

int galaxy_extension_initialize(struct GALAXY *galaxy) {
    if (galaxy == NULL) return -1;
    
    /* Initialize the extension tracking fields */
    galaxy->extension_data = NULL;
    galaxy->num_extensions = 0;
    galaxy->extension_flags = 0;
    
    /* Initialize properties structure for testing */
    if (galaxy->properties == NULL) {
        galaxy->properties = calloc(1, sizeof(galaxy_properties_t));
        if (galaxy->properties == NULL) return -1;
    }
    
    return 0;
}

int galaxy_extension_cleanup(struct GALAXY *galaxy) {
    if (galaxy == NULL) return -1;
    
    /* Free extension data if present */
    if (galaxy->extension_data != NULL) {
        /* In real implementation, we would free each extension */
        free(galaxy->extension_data);
    }
    
    /* Free properties if present */
    if (galaxy->properties != NULL) {
        /* 
         * In a real implementation, we need to free any dynamic arrays inside the properties
         * structure. For this stub, we'll just free the top-level structure.
         * In production code, this would use the proper property system
         * deallocation functions that handle dynamic arrays.
         */
        free(galaxy->properties);
    }
    
    galaxy->extension_data = NULL;
    galaxy->num_extensions = 0;
    galaxy->extension_flags = 0;
    galaxy->properties = NULL;
    
    return 0;
}

int galaxy_extension_copy(struct GALAXY *dest, const struct GALAXY *src) {
    if (dest == NULL || src == NULL) return -1;
    
    /* In test environment, just copy the flags */
    dest->extension_data = NULL;
    dest->num_extensions = src->num_extensions;
    dest->extension_flags = src->extension_flags;
    
    /* Copy properties if present */
    if (src->properties != NULL && dest->properties == NULL) {
        dest->properties = calloc(1, sizeof(galaxy_properties_t));
        if (dest->properties == NULL) return -1;
        
        /* Copy the actual property data (shallow copy for testing) */
        memcpy(dest->properties, src->properties, sizeof(galaxy_properties_t));
    }
    
    return 0;
}

/* Logging stubs */
void initialize_logging_params_view(void *params_view __attribute__((unused)), 
                                   void *run_params __attribute__((unused))) {
    /* Do nothing in tests */
}