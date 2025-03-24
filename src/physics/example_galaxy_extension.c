#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../core/core_allvars.h"
#include "../core/core_module_system.h"
#include "../core/core_galaxy_extensions.h"
#include "../core/core_logging.h"

#include "example_galaxy_extension.h"

/* Global extension ID */
static int example_extension_id = -1;

/**
 * Serialize an example extension
 * 
 * Converts extension data to a serialized format for output.
 * 
 * @param src Source extension data
 * @param dest Destination buffer
 * @param count Number of elements to serialize
 */
static void serialize_example_extension(const void *src, void *dest, int count) {
    /* For simplicity in this example, we just copy the data directly */
    memcpy(dest, src, sizeof(example_extension_data_t) * count);
}

/**
 * Deserialize an example extension
 * 
 * Converts serialized data back to extension format.
 * 
 * @param src Source serialized data
 * @param dest Destination extension buffer
 * @param count Number of elements to deserialize
 */
static void deserialize_example_extension(const void *src, void *dest, int count) {
    /* For simplicity in this example, we just copy the data directly */
    memcpy(dest, src, sizeof(example_extension_data_t) * count);
}

/**
 * Initialize the example extension
 * 
 * Registers the example extension properties with the galaxy extension system.
 * 
 * @param module_id ID of the module that will own the extensions
 * @return Extension ID on success, negative error code on failure
 */
int initialize_example_extension(int module_id) {
    /* If already initialized, return the existing ID */
    if (example_extension_id >= 0) {
        return example_extension_id;
    }
    
    /* Create property definition */
    galaxy_property_t property;
    memset(&property, 0, sizeof(property));
    
    /* Set property metadata */
    strncpy(property.name, "ExampleExtension", MAX_PROPERTY_NAME - 1);
    property.size = sizeof(example_extension_data_t);
    property.module_id = module_id;
    property.type = PROPERTY_TYPE_STRUCT;
    property.flags = PROPERTY_FLAG_SERIALIZE | PROPERTY_FLAG_INITIALIZE;
    
    /* Set serialization functions */
    property.serialize = serialize_example_extension;
    property.deserialize = deserialize_example_extension;
    
    /* Set description and units */
    strncpy(property.description, "Example extension data for testing", MAX_PROPERTY_DESCRIPTION - 1);
    strncpy(property.units, "Mixed", MAX_PROPERTY_UNITS - 1);
    
    /* Register the property */
    example_extension_id = galaxy_extension_register(&property);
    
    if (example_extension_id < 0) {
        LOG_ERROR("Failed to register example extension property");
        return example_extension_id;
    }
    
    LOG_INFO("Registered example extension property with ID %d", example_extension_id);
    
    return example_extension_id;
}

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
example_extension_data_t *get_example_extension_data(struct GALAXY *galaxy, int extension_id) {
    /* Use the GALAXY_EXT macro to get the extension data with proper casting */
    return GALAXY_EXT(galaxy, extension_id, example_extension_data_t);
}

/**
 * Demonstrate extension usage
 * 
 * Shows how to use the extension mechanism to add custom properties to galaxies.
 * 
 * @param galaxy Pointer to the galaxy
 * @param extension_id ID of the extension
 */
void demonstrate_extension_usage(struct GALAXY *galaxy, int extension_id) {
    /* Get the extension data */
    example_extension_data_t *ext_data = get_example_extension_data(galaxy, extension_id);
    
    if (ext_data == NULL) {
        LOG_ERROR("Failed to get example extension data for galaxy");
        return;
    }
    
    /* Calculate H2 fraction based on metallicity and gas mass */
    ext_data->pressure = galaxy->Vvir * galaxy->Vvir * galaxy->ColdGas / galaxy->DiskScaleRadius;
    ext_data->h2_fraction = 0.5 * galaxy->MetalsColdGas / (galaxy->ColdGas + 1.0e-10);
    
    /* Ensure fraction is between 0 and 1 */
    if (ext_data->h2_fraction < 0.0) ext_data->h2_fraction = 0.0;
    if (ext_data->h2_fraction > 1.0) ext_data->h2_fraction = 1.0;
    
    /* Set up some example regions */
    ext_data->num_regions = 3;
    
    /* Central region */
    ext_data->regions[0].radius = 0.1 * galaxy->DiskScaleRadius;
    ext_data->regions[0].sfr = 0.5 * galaxy->ColdGas * ext_data->h2_fraction / 1000.0;
    
    /* Middle region */
    ext_data->regions[1].radius = 0.5 * galaxy->DiskScaleRadius;
    ext_data->regions[1].sfr = 0.3 * galaxy->ColdGas * ext_data->h2_fraction / 1000.0;
    
    /* Outer region */
    ext_data->regions[2].radius = 1.0 * galaxy->DiskScaleRadius;
    ext_data->regions[2].sfr = 0.2 * galaxy->ColdGas * ext_data->h2_fraction / 1000.0;
    
    /* Log some information */
    LOG_DEBUG("Example extension for galaxy %d: h2_fraction=%f, pressure=%f, regions=%d",
             galaxy->GalaxyNr, ext_data->h2_fraction, ext_data->pressure, ext_data->num_regions);
}
