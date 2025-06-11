#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "core_allvars.h"
#include "core_logging.h"
#include "core_mymalloc.h"
#include "core_module_system.h"
#include "core_galaxy_extensions.h"

/* Global extension registry */
struct galaxy_extension_registry *global_extension_registry = NULL;

/**
 * Initialize the galaxy extension system
 * 
 * Sets up the global extension registry and prepares it for property registration.
 * 
 * @return 0 on success, error code on failure
 */
int galaxy_extension_system_initialize(void) {
    /* Check if already initialized */
    if (global_extension_registry != NULL) {
        LOG_WARNING("Galaxy extension system already initialized");
        return MODULE_STATUS_ALREADY_INITIALIZED;
    }
    
    /* Allocate memory for the registry */
    global_extension_registry = (struct galaxy_extension_registry *)mymalloc(sizeof(struct galaxy_extension_registry));
    if (global_extension_registry == NULL) {
        LOG_ERROR("Failed to allocate memory for galaxy extension registry");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize registry fields */
    global_extension_registry->num_extensions = 0;
    global_extension_registry->num_module_extensions = 0;
    
    /* Initialize extension slots */
    memset(global_extension_registry->extensions, 0, sizeof(global_extension_registry->extensions));
    memset(global_extension_registry->module_extensions, 0, sizeof(global_extension_registry->module_extensions));
    
    LOG_INFO("Galaxy extension system initialized");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up the galaxy extension system
 * 
 * Releases resources used by the extension system.
 * 
 * @return 0 on success, error code on failure
 */
int galaxy_extension_system_cleanup(void) {
    if (global_extension_registry == NULL) {
        LOG_WARNING("Galaxy extension system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Free the registry */
    myfree(global_extension_registry);
    global_extension_registry = NULL;
    
    LOG_INFO("Galaxy extension system cleaned up");
    return MODULE_STATUS_SUCCESS;
}

/**
 * Register a galaxy property extension
 * 
 * Adds a property extension to the global registry and assigns it an ID.
 * 
 * @param property Pointer to the property definition
 * @return Extension ID on success, negative error code on failure
 */
int galaxy_extension_register(galaxy_property_t *property) {
    if (global_extension_registry == NULL) {
        /* Auto-initialize if not done already */
        int status = galaxy_extension_system_initialize();
        if (status != MODULE_STATUS_SUCCESS) {
            return status;
        }
    }
    
    /* Validate property */
    if (!galaxy_extension_validate_property(property)) {
        LOG_ERROR("Invalid galaxy property definition");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Check for space in registry */
    if (global_extension_registry->num_extensions >= MAX_GALAXY_EXTENSIONS) {
        LOG_ERROR("Galaxy extension registry is full (max %d extensions)", MAX_GALAXY_EXTENSIONS);
        return MODULE_STATUS_ERROR;
    }
    
    /* Check if property name already exists */
    for (int i = 0; i < global_extension_registry->num_extensions; i++) {
        if (strcmp(global_extension_registry->extensions[i].name, property->name) == 0) {
            LOG_ERROR("Galaxy property '%s' already registered", property->name);
            return MODULE_STATUS_ERROR;
        }
    }
    
    /* Find or create module extension entry */
    int module_ext_index = -1;
    for (int i = 0; i < global_extension_registry->num_module_extensions; i++) {
        if (global_extension_registry->module_extensions[i].module_id == property->module_id) {
            module_ext_index = i;
            break;
        }
    }
    
    if (module_ext_index == -1) {
        /* Create new module extension entry */
        if (global_extension_registry->num_module_extensions >= MAX_MODULES) {
            LOG_ERROR("Too many modules with extensions (max %d)", MAX_MODULES);
            return MODULE_STATUS_ERROR;
        }
        
        module_ext_index = global_extension_registry->num_module_extensions;
        global_extension_registry->module_extensions[module_ext_index].module_id = property->module_id;
        global_extension_registry->module_extensions[module_ext_index].first_extension = global_extension_registry->num_extensions;
        global_extension_registry->module_extensions[module_ext_index].num_extensions = 0;
        global_extension_registry->num_module_extensions++;
    }
    
    /* Assign extension ID */
    int extension_id = global_extension_registry->num_extensions;
    property->extension_id = extension_id;
    
    /* Copy property definition to registry */
    memcpy(&global_extension_registry->extensions[extension_id], property, sizeof(galaxy_property_t));
    global_extension_registry->num_extensions++;
    
    /* Update module extension count */
    global_extension_registry->module_extensions[module_ext_index].num_extensions++;
    
    /* Interval-based debug logging (first 5 registrations only) */
    static int debug_count_registrations = 0;
    debug_count_registrations++;
    if (debug_count_registrations <= 5) {
        if (debug_count_registrations == 5) {
            LOG_INFO("Registered galaxy property '%s' (module %d) with ID %d (further messages suppressed)", 
                     property->name, property->module_id, extension_id);
        } else {
            LOG_INFO("Registered galaxy property '%s' (module %d) with ID %d", 
                     property->name, property->module_id, extension_id);
        }
    }
    
    return extension_id;
}

/**
 * Unregister a galaxy property extension
 * 
 * Removes a property extension from the global registry.
 * 
 * @param extension_id ID of the extension to unregister
 * @return 0 on success, error code on failure
 */
int galaxy_extension_unregister(int extension_id) {
    if (global_extension_registry == NULL) {
        LOG_ERROR("Galaxy extension system not initialized");
        return MODULE_STATUS_NOT_INITIALIZED;
    }
    
    /* Validate extension ID */
    if (extension_id < 0 || extension_id >= global_extension_registry->num_extensions) {
        LOG_ERROR("Invalid galaxy extension ID: %d", extension_id);
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Get property details */
    galaxy_property_t *property = &global_extension_registry->extensions[extension_id];
    const char *property_name = property->name;
    int module_id = property->module_id;
    
    /* Find module extension entry */
    int module_ext_index = -1;
    for (int i = 0; i < global_extension_registry->num_module_extensions; i++) {
        if (global_extension_registry->module_extensions[i].module_id == module_id) {
            module_ext_index = i;
            break;
        }
    }
    
    if (module_ext_index == -1) {
        LOG_ERROR("Module %d not found in extension registry", module_id);
        return MODULE_STATUS_ERROR;
    }
    
    /* Decrement module extension count */
    global_extension_registry->module_extensions[module_ext_index].num_extensions--;
    
    /* If this was the last extension for this module, remove the module entry */
    if (global_extension_registry->module_extensions[module_ext_index].num_extensions == 0) {
        /* Shift remaining entries down */
        for (int i = module_ext_index; i < global_extension_registry->num_module_extensions - 1; i++) {
            global_extension_registry->module_extensions[i] = global_extension_registry->module_extensions[i + 1];
        }
        global_extension_registry->num_module_extensions--;
    }
    
    /* Clear the extension entry */
    memset(&global_extension_registry->extensions[extension_id], 0, sizeof(galaxy_property_t));
    
    /* We don't reduce num_extensions or reorganize the array to keep IDs stable */
    
    LOG_INFO("Unregistered galaxy property '%s' (module %d) with ID %d", 
             property_name, module_id, extension_id);
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Initialize extension data for a galaxy
 * 
 * Allocates and initializes extension data for a galaxy.
 * 
 * @param galaxy Pointer to the galaxy to initialize
 * @return 0 on success, error code on failure
 */
int galaxy_extension_initialize(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("NULL galaxy pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    if (galaxy->extension_data != NULL) {
        /* Already initialized, cleanup first */
        galaxy_extension_cleanup(galaxy);
    }
    
    /* If no global registry, nothing to initialize */
    if (global_extension_registry == NULL || global_extension_registry->num_extensions == 0) {
        galaxy->extension_data = NULL;
        galaxy->num_extensions = 0;
        galaxy->extension_flags = 0;
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Allocate memory for extension data pointers */
    galaxy->extension_data = (void **)mymalloc(
        global_extension_registry->num_extensions * sizeof(void *));
    
    if (galaxy->extension_data == NULL) {
        LOG_ERROR("Failed to allocate memory for galaxy extension data");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize all pointers to NULL */
    memset(galaxy->extension_data, 0, global_extension_registry->num_extensions * sizeof(void *));
    galaxy->num_extensions = global_extension_registry->num_extensions;
    galaxy->extension_flags = 0;
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Clean up extension data for a galaxy
 * 
 * Releases extension data associated with a galaxy.
 * 
 * @param galaxy Pointer to the galaxy to clean up
 * @return 0 on success, error code on failure
 */
int galaxy_extension_cleanup(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("NULL galaxy pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    if (galaxy->extension_data == NULL) {
        /* Nothing to do */
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Free allocated extension data */
    for (int i = 0; i < galaxy->num_extensions; i++) {
        if (galaxy->extension_data[i] != NULL) {
            myfree(galaxy->extension_data[i]);
            galaxy->extension_data[i] = NULL;
        }
    }
    
    /* Free extension data pointer array */
    myfree(galaxy->extension_data);
    galaxy->extension_data = NULL;
    galaxy->num_extensions = 0;
    galaxy->extension_flags = 0;
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Get extension data for a galaxy property
 * 
 * Retrieves a pointer to the extension data for a specific property.
 * Also allocates memory for the property if it hasn't been allocated yet.
 * 
 * @param galaxy Pointer to the galaxy
 * @param extension_id ID of the extension to retrieve
 * @return Pointer to the extension data, or NULL if not found
 */
void *galaxy_extension_get_data(const struct GALAXY *galaxy, int extension_id) {
    if (galaxy == NULL || galaxy->extension_data == NULL) {
        return NULL;
    }
    
    if (extension_id < 0 || extension_id >= galaxy->num_extensions) {
        LOG_ERROR("Invalid galaxy extension ID: %d", extension_id);
        return NULL;
    }
    
    /* Check if data is already allocated */
    if (galaxy->extension_data[extension_id] != NULL) {
        return galaxy->extension_data[extension_id];
    }
    
    /* Allocate memory for the extension data if it's not allocated yet */
    if (global_extension_registry != NULL && extension_id < global_extension_registry->num_extensions) {
        const galaxy_property_t *property = &global_extension_registry->extensions[extension_id];
        
        /* Cast away const to modify the galaxy's extension data */
        struct GALAXY *mutable_galaxy = (struct GALAXY *)galaxy;
        
        // Use standard allocation for now until we fully implement the diagnostic system
        mutable_galaxy->extension_data[extension_id] = mymalloc(property->size);
        if (mutable_galaxy->extension_data[extension_id] == NULL) {
            LOG_ERROR("Failed to allocate memory for galaxy extension data (ID %d)", extension_id);
            return NULL;
        }
        
        /* Set the extension flag bit */
        mutable_galaxy->extension_flags |= (1ULL << extension_id);
        
        /* Initialize to zero if requested */
        if (property->flags & PROPERTY_FLAG_INITIALIZE) {
            memset(mutable_galaxy->extension_data[extension_id], 0, property->size);
        }
        
        return mutable_galaxy->extension_data[extension_id];
    }
    
    return NULL;
}

/**
 * Get extension data for a galaxy property by module ID
 * 
 * Retrieves a pointer to the extension data for a module.
 * 
 * @param galaxy Pointer to the galaxy
 * @param module_id ID of the module that owns the extension
 * @param extension_offset Offset within the module's extensions (0 for first extension)
 * @return Pointer to the extension data, or NULL if not found
 */
void *galaxy_extension_get_data_by_module(const struct GALAXY *galaxy, int module_id, int extension_offset) {
    if (galaxy == NULL || galaxy->extension_data == NULL || global_extension_registry == NULL) {
        return NULL;
    }
    
    /* Find module extension entry */
    int module_ext_index = -1;
    for (int i = 0; i < global_extension_registry->num_module_extensions; i++) {
        if (global_extension_registry->module_extensions[i].module_id == module_id) {
            module_ext_index = i;
            break;
        }
    }
    
    if (module_ext_index == -1) {
        LOG_ERROR("Module %d not found in extension registry", module_id);
        return NULL;
    }
    
    /* Check extension offset */
    if (extension_offset < 0 || extension_offset >= global_extension_registry->module_extensions[module_ext_index].num_extensions) {
        LOG_ERROR("Invalid extension offset %d for module %d", extension_offset, module_id);
        return NULL;
    }
    
    /* Get extension ID */
    int first_extension = global_extension_registry->module_extensions[module_ext_index].first_extension;
    int extension_id = first_extension + extension_offset;
    
    /* Get extension data */
    return galaxy_extension_get_data(galaxy, extension_id);
}

/**
 * Get extension data for a galaxy property by name
 * 
 * Retrieves a pointer to the extension data for a named property.
 * 
 * @param galaxy Pointer to the galaxy
 * @param property_name Name of the property to retrieve
 * @return Pointer to the extension data, or NULL if not found
 */
void *galaxy_extension_get_data_by_name(const struct GALAXY *galaxy, const char *property_name) {
    if (galaxy == NULL || galaxy->extension_data == NULL || global_extension_registry == NULL || property_name == NULL) {
        return NULL;
    }
    
    /* Find property by name */
    for (int i = 0; i < global_extension_registry->num_extensions; i++) {
        if (strcmp(global_extension_registry->extensions[i].name, property_name) == 0) {
            return galaxy_extension_get_data(galaxy, i);
        }
    }
    
    LOG_ERROR("Galaxy property '%s' not found", property_name);
    return NULL;
}

/**
 * Find a galaxy property by name
 * 
 * Looks up a property in the extension registry by name.
 * 
 * @param property_name Name of the property to find
 * @return Pointer to the property definition, or NULL if not found
 */
const galaxy_property_t *galaxy_extension_find_property(const char *property_name) {
    if (global_extension_registry == NULL || property_name == NULL) {
        return NULL;
    }
    
    /* Find property by name */
    for (int i = 0; i < global_extension_registry->num_extensions; i++) {
        if (strcmp(global_extension_registry->extensions[i].name, property_name) == 0) {
            return &global_extension_registry->extensions[i];
        }
    }
    
    return NULL;
}

/**
 * Find a galaxy property by extension ID
 * 
 * Looks up a property in the extension registry by ID.
 * 
 * @param extension_id ID of the extension to find
 * @return Pointer to the property definition, or NULL if not found
 */
const galaxy_property_t *galaxy_extension_find_property_by_id(int extension_id) {
    if (global_extension_registry == NULL) {
        return NULL;
    }
    
    if (extension_id < 0 || extension_id >= global_extension_registry->num_extensions) {
        LOG_ERROR("Invalid galaxy extension ID: %d", extension_id);
        return NULL;
    }
    
    return &global_extension_registry->extensions[extension_id];
}

/**
 * Find galaxy properties by module ID
 * 
 * Gets all properties registered by a specific module.
 * 
 * @param module_id ID of the module
 * @param properties Output array to store property pointers
 * @param max_properties Maximum number of properties to return
 * @return Number of properties found, or negative error code
 */
int galaxy_extension_find_properties_by_module(int module_id, const galaxy_property_t **properties, int max_properties) {
    if (global_extension_registry == NULL || properties == NULL || max_properties <= 0) {
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Find module extension entry */
    int module_ext_index = -1;
    for (int i = 0; i < global_extension_registry->num_module_extensions; i++) {
        if (global_extension_registry->module_extensions[i].module_id == module_id) {
            module_ext_index = i;
            break;
        }
    }
    
    if (module_ext_index == -1) {
        /* No properties registered for this module */
        return 0;
    }
    
    /* Get properties */
    int first_extension = global_extension_registry->module_extensions[module_ext_index].first_extension;
    int num_extensions = global_extension_registry->module_extensions[module_ext_index].num_extensions;
    int count = 0;
    
    for (int i = 0; i < num_extensions && count < max_properties; i++) {
        int extension_id = first_extension + i;
        if (extension_id < global_extension_registry->num_extensions) {
            properties[count++] = &global_extension_registry->extensions[extension_id];
        }
    }
    
    return count;
}

/**
 * Copy extension data from one galaxy to another
 * 
 * Copies all extension data from the source galaxy to the destination galaxy.
 * 
 * @param dest Pointer to the destination galaxy
 * @param src Pointer to the source galaxy
 * @return 0 on success, error code on failure
 */
int galaxy_extension_copy(struct GALAXY *dest, const struct GALAXY *src) {
    if (dest == NULL || src == NULL) {
        LOG_ERROR("NULL galaxy pointer");
        return MODULE_STATUS_INVALID_ARGS;
    }
    
    /* Clean up existing extensions in destination */
    galaxy_extension_cleanup(dest);
    
    /* If source has no extensions, we're done */
    if (src->extension_data == NULL || src->num_extensions == 0) {
        dest->extension_data = NULL;
        dest->num_extensions = 0;
        dest->extension_flags = 0;
        return MODULE_STATUS_SUCCESS;
    }
    
    /* Allocate memory for extension data pointers */
    dest->extension_data = (void **)mymalloc(src->num_extensions * sizeof(void *));
    if (dest->extension_data == NULL) {
        LOG_ERROR("Failed to allocate memory for galaxy extension data");
        return MODULE_STATUS_OUT_OF_MEMORY;
    }
    
    /* Initialize all pointers to NULL */
    for (int i = 0; i < src->num_extensions; i++) {
        dest->extension_data[i] = NULL;
    }
    
    dest->num_extensions = src->num_extensions;
    dest->extension_flags = src->extension_flags;
    
    /* Copy extension data */
    for (int i = 0; i < src->num_extensions; i++) {
        /* Only copy extensions with the flag bit set */
        if ((src->extension_flags & (1ULL << i)) == 0) {
            continue;
        }
        
        /* Skip NULL extensions */
        if (src->extension_data[i] == NULL) {
            continue;
        }
        
        /* Get property definition */
        const galaxy_property_t *property = NULL;
        
        if (global_extension_registry != NULL && i < global_extension_registry->num_extensions) {
            property = &global_extension_registry->extensions[i];
        } else {
            /* If we can't determine the size, we can't safely copy */
            LOG_WARNING("Skipping extension data copy - extension system not initialized");
            continue;
        }
        
        if (property == NULL) {
            LOG_ERROR("No property definition found for extension ID %d", i);
            continue;
        }
        
        /* Allocate memory for extension data */
        char copy_desc[128];
        snprintf(copy_desc, sizeof(copy_desc), "galaxy_ext_copy_%d", i);
        dest->extension_data[i] = malloc(property->size);
        if (dest->extension_data[i] == NULL) {
            LOG_ERROR("Failed to allocate memory for galaxy extension data (ID %d)", i);
            continue;
        }
        
        /* Copy extension data */
        memcpy(dest->extension_data[i], src->extension_data[i], property->size);
    }
    
    return MODULE_STATUS_SUCCESS;
}

/**
 * Validate a galaxy property definition
 * 
 * Checks that a property definition is valid and properly formed.
 * 
 * @param property Pointer to the property definition to validate
 * @return true if the property is valid, false otherwise
 */
bool galaxy_extension_validate_property(const galaxy_property_t *property) {
    if (property == NULL) {
        LOG_ERROR("NULL galaxy property pointer");
        return false;
    }
    
    /* Check required fields */
    if (property->name[0] == '\0') {
        LOG_ERROR("Galaxy property name cannot be empty");
        return false;
    }
    
    if (property->size == 0) {
        LOG_ERROR("Galaxy property size cannot be zero");
        return false;
    }
    
    if (property->module_id < 0) {
        LOG_ERROR("Invalid module ID for galaxy property: %d", property->module_id);
        return false;
    }
    
    /* Check data type is valid */
    if (property->type < 0 || property->type >= PROPERTY_TYPE_MAX) {
        LOG_ERROR("Invalid data type for galaxy property: %d", property->type);
        return false;
    }
    
    /* Check serialization functions */
    if ((property->flags & PROPERTY_FLAG_SERIALIZE) != 0) {
        if (property->serialize == NULL || property->deserialize == NULL) {
            LOG_ERROR("Serializable galaxy property must have serialize and deserialize functions");
            return false;
        }
    }
    
    return true;
}
