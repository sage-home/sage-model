#include "core_properties_sync.h"
#include "core_properties.h"    // For GALAXY_PROP_* macros and PROPERTY_META
#include "core_allvars.h"       // For struct GALAXY definition with direct fields
#include "core_logging.h"       // For logging errors/debug messages
#include <string.h>             // For memcpy

/**
 * @brief This function is now a no-op in the physics-agnostic core
 * 
 * In the Core-Physics Separation model, we no longer need to sync between
 * direct fields and properties, as all properties are managed through the
 * property system. This function exists only for backward compatibility
 * and will be removed in a future release.
 */
void sync_direct_to_properties(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("sync_direct_to_properties: galaxy pointer is NULL.");
        return;
    }
    
    LOG_DEBUG("sync_direct_to_properties is a no-op in core-physics separation for GalaxyNr %d", 
              galaxy->GalaxyNr);
    
    // No syncing needed - Properties access only happens through GALAXY_PROP_* macros
}

/**
 * @brief This function is now a no-op in the physics-agnostic core
 * 
 * In the Core-Physics Separation model, we no longer need to sync between
 * direct fields and properties, as all properties are managed through the
 * property system. This function exists only for backward compatibility
 * and will be removed in a future release.
 */
void sync_properties_to_direct(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        LOG_ERROR("sync_properties_to_direct: galaxy pointer is NULL.");
        return;
    }
    
    LOG_DEBUG("sync_properties_to_direct is a no-op in core-physics separation for GalaxyNr %d", 
              galaxy->GalaxyNr);
    
    // No syncing needed - Properties access only happens through GALAXY_PROP_* macros
}