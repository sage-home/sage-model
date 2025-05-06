/**
 * @file standard_properties.c
 * @brief Implementation of standard property registration
 *
 * This file implements the registration of standard properties
 * with the extension system, providing a bridge between the
 * auto-generated property system and the extension mechanism.
 */

#include <string.h>
#include "core_allvars.h"
#include "core_properties.h"
#include "core_galaxy_extensions.h"
#include "core_logging.h"
#include "standard_properties.h"
#include "macros.h"

/* Mapping from property ID to extension ID */
int standard_property_to_extension_id[PROP_COUNT];

/* Module ID for standard properties */
#define STANDARD_PROPERTIES_MODULE_ID 0

/**
 * @brief Get standard property ID by name
 */
property_id_t get_standard_property_id_by_name(const char *name)
{
    return get_property_id(name);
}

/**
 * @brief Get extension ID for a standard property
 */
int get_extension_id_for_standard_property(property_id_t property_id)
{
    if (property_id < 0 || property_id >= PROP_COUNT) {
        return -1;
    }
    
    return standard_property_to_extension_id[property_id];
}

/* Serialization functions for different property types */
static void serialize_float(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(float));
}

static void deserialize_float(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(float));
}

static void serialize_double(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(double));
}

static void deserialize_double(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(double));
}

static void serialize_int32(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(int32_t));
}

static void deserialize_int32(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(int32_t));
}

static void serialize_int64(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(int64_t));
}

static void deserialize_int64(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(int64_t));
}

static void serialize_uint64(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(uint64_t));
}

static void deserialize_uint64(const void *src, void *dest, int count)
{
    memcpy(dest, src, count * sizeof(uint64_t));
}

/* Register a scalar property */
static int register_scalar_property(property_id_t property_id, 
                                   enum galaxy_property_type type,
                                   void (*serialize_func)(const void*, void*, int),
                                   void (*deserialize_func)(const void*, void*, int))
{
    const char *name = get_property_name(property_id);
    if (name == NULL) {
        LOG_ERROR("Invalid property ID: %d", property_id);
        return -1;
    }
    
    const property_meta_t *meta = &PROPERTY_META[property_id];
    
    /* Create property definition */
    galaxy_property_t property;
    memset(&property, 0, sizeof(property));
    
    /* Set property attributes */
    strncpy(property.name, name, sizeof(property.name) - 1);
    
    /* Set size based on type */
    property.size = (type == PROPERTY_TYPE_FLOAT) ? sizeof(float) :
                    (type == PROPERTY_TYPE_DOUBLE) ? sizeof(double) :
                    (type == PROPERTY_TYPE_INT32) ? sizeof(int32_t) :
                    (type == PROPERTY_TYPE_INT64) ? sizeof(int64_t) :
                    (type == PROPERTY_TYPE_UINT64) ? sizeof(uint64_t) :
                    sizeof(float); /* Default to float size */
    
    property.module_id = STANDARD_PROPERTIES_MODULE_ID;
    property.type = type;
    
    /* Set flags */
    property.flags = PROPERTY_FLAG_INITIALIZE;
    if (meta->output) {
        property.flags |= PROPERTY_FLAG_SERIALIZE;
    }
    if (meta->read_only) {
        property.flags |= PROPERTY_FLAG_READONLY;
    }
    
    /* Set serialization functions */
    property.serialize = serialize_func;
    property.deserialize = deserialize_func;
    
    /* Set metadata */
    strncpy(property.description, meta->description, sizeof(property.description) - 1);
    strncpy(property.units, meta->units, sizeof(property.units) - 1);
    
    /* Register with extension system */
    int extension_id = galaxy_extension_register(&property);
    if (extension_id < 0) {
        LOG_ERROR("Failed to register property '%s', error: %d", name, extension_id);
        return extension_id;
    }
    
    /* Store mapping */
    standard_property_to_extension_id[property_id] = extension_id;
    
    LOG_DEBUG("Registered property '%s' (ID %d) with extension ID %d", 
             name, property_id, extension_id);
    
    return 0;
}

/* Register a fixed-size array property */
static int register_fixed_array_property(property_id_t property_id,
                                        enum galaxy_property_type element_type,
                                        int array_size,
                                        void (*serialize_func)(const void*, void*, int),
                                        void (*deserialize_func)(const void*, void*, int))
{
    const char *name = get_property_name(property_id);
    if (name == NULL) {
        LOG_ERROR("Invalid property ID: %d", property_id);
        return -1;
    }
    
    const property_meta_t *meta = &PROPERTY_META[property_id];
    
    /* Create property definition */
    galaxy_property_t property;
    memset(&property, 0, sizeof(property));
    
    /* Set property attributes */
    strncpy(property.name, name, sizeof(property.name) - 1);
    
    /* Determine size based on element type parameter and array size */
    size_t element_size = (element_type == PROPERTY_TYPE_FLOAT) ? sizeof(float) :
                         (element_type == PROPERTY_TYPE_DOUBLE) ? sizeof(double) :
                         (element_type == PROPERTY_TYPE_INT32) ? sizeof(int32_t) :
                         (element_type == PROPERTY_TYPE_INT64) ? sizeof(int64_t) :
                         (element_type == PROPERTY_TYPE_UINT64) ? sizeof(uint64_t) :
                         sizeof(float);
    
    property.size = element_size * array_size;
    property.module_id = STANDARD_PROPERTIES_MODULE_ID;
    property.type = PROPERTY_TYPE_ARRAY;
    
    /* Set flags */
    property.flags = PROPERTY_FLAG_INITIALIZE;
    if (meta->output) {
        property.flags |= PROPERTY_FLAG_SERIALIZE;
    }
    if (meta->read_only) {
        property.flags |= PROPERTY_FLAG_READONLY;
    }
    
    /* Set serialization functions */
    property.serialize = serialize_func;
    property.deserialize = deserialize_func;
    
    /* Set metadata */
    strncpy(property.description, meta->description, sizeof(property.description) - 1);
    strncpy(property.units, meta->units, sizeof(property.units) - 1);
    
    /* Register with extension system */
    int extension_id = galaxy_extension_register(&property);
    if (extension_id < 0) {
        LOG_ERROR("Failed to register array property '%s', error: %d", name, extension_id);
        return extension_id;
    }
    
    /* Store mapping */
    standard_property_to_extension_id[property_id] = extension_id;
    
    LOG_DEBUG("Registered array property '%s' (ID %d) with extension ID %d", 
             name, property_id, extension_id);
    
    return 0;
}

/* Register a dynamic array property */
static int register_dynamic_array_property(property_id_t property_id,
                                         enum galaxy_property_type element_type,
                                         void (*serialize_func)(const void*, void*, int),
                                         void (*deserialize_func)(const void*, void*, int))
{
    /* Suppress unused parameter warning - DEBUG IF THIS PARAMETER REMAINES UNUSED */
    (void)element_type;
    
    const char *name = get_property_name(property_id);
    if (name == NULL) {
        LOG_ERROR("Invalid property ID: %d", property_id);
        return -1;
    }
    
    const property_meta_t *meta = &PROPERTY_META[property_id];
    
    /* Create property definition */
    galaxy_property_t property;
    memset(&property, 0, sizeof(property));
    
    /* Set property attributes */
    strncpy(property.name, name, sizeof(property.name) - 1);
    
    /* For dynamic arrays, we store both the array pointer and the size */
    /* The actual size is calculated at runtime */
    property.size = sizeof(void*);
    property.module_id = STANDARD_PROPERTIES_MODULE_ID;
    property.type = PROPERTY_TYPE_ARRAY;
    
    /* Set flags */
    property.flags = PROPERTY_FLAG_INITIALIZE;
    if (meta->output) {
        property.flags |= PROPERTY_FLAG_SERIALIZE;
    }
    if (meta->read_only) {
        property.flags |= PROPERTY_FLAG_READONLY;
    }
    
    /* Set serialization functions */
    property.serialize = serialize_func;
    property.deserialize = deserialize_func;
    
    /* Set metadata */
    strncpy(property.description, meta->description, sizeof(property.description) - 1);
    strncpy(property.units, meta->units, sizeof(property.units) - 1);
    
    /* Register with extension system */
    int extension_id = galaxy_extension_register(&property);
    if (extension_id < 0) {
        LOG_ERROR("Failed to register dynamic array property '%s', error: %d", name, extension_id);
        return extension_id;
    }
    
    /* Store mapping */
    standard_property_to_extension_id[property_id] = extension_id;
    
    LOG_DEBUG("Registered dynamic array property '%s' (ID %d) with extension ID %d", 
             name, property_id, extension_id);
    
    return 0;
}

/**
 * @brief Register all standard properties with the extension system
 */
int register_standard_properties(void)
{
    LOG_INFO("Registering standard properties with the extension system");
    
    /* Initialize mapping to -1 (not registered) */
    for (int i = 0; i < PROP_COUNT; i++) {
        standard_property_to_extension_id[i] = -1;
    }
    
    /* Register all properties based on their types */
    int status = 0;
    
    /* Loop through all properties */
    for (property_id_t id = 0; id < PROP_COUNT; id++) {
        const property_meta_t *meta = &PROPERTY_META[id];
        
        /* Handle different property types */
        if (meta->is_array) {
            if (meta->array_dimension > 0) {
                /* Fixed-size array */
                if (strstr(meta->type, "float") != NULL) {
                    status = register_fixed_array_property(id, PROPERTY_TYPE_FLOAT, meta->array_dimension,
                                                        serialize_float, deserialize_float);
                } else if (strstr(meta->type, "double") != NULL) {
                    status = register_fixed_array_property(id, PROPERTY_TYPE_DOUBLE, meta->array_dimension,
                                                        serialize_double, deserialize_double);
                } else if (strstr(meta->type, "int32_t") != NULL) {
                    status = register_fixed_array_property(id, PROPERTY_TYPE_INT32, meta->array_dimension,
                                                        serialize_int32, deserialize_int32);
                } else if (strstr(meta->type, "int64_t") != NULL || strstr(meta->type, "long long") != NULL) {
                    status = register_fixed_array_property(id, PROPERTY_TYPE_INT64, meta->array_dimension,
                                                        serialize_int64, deserialize_int64);
                } else if (strstr(meta->type, "uint64_t") != NULL) {
                    status = register_fixed_array_property(id, PROPERTY_TYPE_UINT64, meta->array_dimension,
                                                        serialize_uint64, deserialize_uint64);
                } else {
                    LOG_WARNING("Unsupported array element type: %s", meta->type);
                    continue;
                }
            } else {
                /* Dynamic array */
                if (strstr(meta->type, "float") != NULL) {
                    status = register_dynamic_array_property(id, PROPERTY_TYPE_FLOAT,
                                                         serialize_float, deserialize_float);
                } else if (strstr(meta->type, "double") != NULL) {
                    status = register_dynamic_array_property(id, PROPERTY_TYPE_DOUBLE,
                                                         serialize_double, deserialize_double);
                } else if (strstr(meta->type, "int32_t") != NULL) {
                    status = register_dynamic_array_property(id, PROPERTY_TYPE_INT32,
                                                         serialize_int32, deserialize_int32);
                } else if (strstr(meta->type, "int64_t") != NULL || strstr(meta->type, "long long") != NULL) {
                    status = register_dynamic_array_property(id, PROPERTY_TYPE_INT64,
                                                         serialize_int64, deserialize_int64);
                } else if (strstr(meta->type, "uint64_t") != NULL) {
                    status = register_dynamic_array_property(id, PROPERTY_TYPE_UINT64,
                                                         serialize_uint64, deserialize_uint64);
                } else {
                    LOG_WARNING("Unsupported dynamic array element type: %s", meta->type);
                    continue;
                }
            }
        } else {
            /* Scalar property */
            if (strstr(meta->type, "float") != NULL) {
                status = register_scalar_property(id, PROPERTY_TYPE_FLOAT,
                                               serialize_float, deserialize_float);
            } else if (strstr(meta->type, "double") != NULL) {
                status = register_scalar_property(id, PROPERTY_TYPE_DOUBLE,
                                               serialize_double, deserialize_double);
            } else if (strstr(meta->type, "int32_t") != NULL || strstr(meta->type, "int") != NULL) {
                status = register_scalar_property(id, PROPERTY_TYPE_INT32,
                                               serialize_int32, deserialize_int32);
            } else if (strstr(meta->type, "int64_t") != NULL || strstr(meta->type, "long long") != NULL) {
                status = register_scalar_property(id, PROPERTY_TYPE_INT64,
                                               serialize_int64, deserialize_int64);
            } else if (strstr(meta->type, "uint64_t") != NULL) {
                status = register_scalar_property(id, PROPERTY_TYPE_UINT64,
                                               serialize_uint64, deserialize_uint64);
            } else {
                LOG_WARNING("Unsupported scalar type: %s", meta->type);
                continue;
            }
        }
        
        /* Check registration status */
        if (status != 0) {
            LOG_ERROR("Failed to register property '%s' (ID %d), error: %d",
                     get_property_name(id), id, status);
            /* Continue with other properties */
        }
    }
    
    LOG_INFO("Registered standard properties with the extension system");
    
    return 0;
}
