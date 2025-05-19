#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../core/core_allvars.h"
#include "../core/core_galaxy_extensions.h"
#include "../core/core_mymalloc.h"
#include "../core/core_logging.h"
#include "io_interface.h"
#include "io_endian_utils.h"
#include "io_property_serialization.h"

/**
 * @brief Round up to the nearest multiple for alignment
 * 
 * @param size Size to align
 * @param alignment Alignment boundary
 * @return Aligned size
 */
static size_t align_size(size_t size, size_t alignment) {
    return ((size + alignment - 1) / alignment) * alignment;
}

/**
 * @brief Initialize property serialization context
 * 
 * Sets up a context for serializing/deserializing properties.
 * 
 * @param ctx Context to initialize
 * @param filter_flags Flags controlling which properties to serialize
 * @return 0 on success, non-zero on failure
 */
int property_serialization_init(struct property_serialization_context *ctx, uint32_t filter_flags) {
    if (ctx == NULL) {
        return -1;
    }
    
    // Initialize context
    memset(ctx, 0, sizeof(struct property_serialization_context));
    ctx->version = PROPERTY_SERIALIZATION_VERSION;
    ctx->filter_flags = filter_flags;
    ctx->endian_swap = (get_system_endianness() != ENDIAN_BIG);  // Use big-endian for network order
    
    return 0;
}

/**
 * @brief Add properties to be serialized
 * 
 * Scans the galaxy extension registry and adds properties to the context
 * based on the filter flags.
 * 
 * @param ctx Serialization context
 * @return 0 on success, non-zero on failure
 */
int property_serialization_add_properties(struct property_serialization_context *ctx) {
    if (ctx == NULL || global_extension_registry == NULL) {
        return -1;
    }
    
    // Count properties that match filter criteria
    int num_filtered_properties = 0;
    for (int i = 0; i < global_extension_registry->num_extensions; i++) {
        const galaxy_property_t *prop = &global_extension_registry->extensions[i];
        
        // Skip properties that don't match filter
        if ((ctx->filter_flags & SERIALIZE_EXPLICIT) && 
            !(prop->flags & PROPERTY_FLAG_SERIALIZE)) {
            continue;
        }
        
        if ((ctx->filter_flags & SERIALIZE_EXCLUDE_DERIVED) && 
            (prop->flags & PROPERTY_FLAG_DERIVED)) {
            continue;
        }
        
        num_filtered_properties++;
    }
    
    if (num_filtered_properties == 0) {
        // No properties to serialize
        ctx->num_properties = 0;
        ctx->properties = NULL;
        ctx->property_id_map = NULL;
        ctx->total_size_per_galaxy = 0;
        return 0;
    }
    
    // Allocate arrays for property metadata and ID mapping
    ctx->properties = calloc(num_filtered_properties, sizeof(struct serialized_property_meta));
    ctx->property_id_map = calloc(num_filtered_properties, sizeof(int));
    
    if (ctx->properties == NULL || ctx->property_id_map == NULL) {
        // Clean up on allocation failure
        free(ctx->properties);
        free(ctx->property_id_map);
        ctx->properties = NULL;
        ctx->property_id_map = NULL;
        return -1;
    }
    
    // Populate arrays with filtered properties
    int prop_idx = 0;
    size_t total_size = 0;
    
    for (int i = 0; i < global_extension_registry->num_extensions; i++) {
        const galaxy_property_t *prop = &global_extension_registry->extensions[i];
        
        // Skip properties that don't match filter
        if ((ctx->filter_flags & SERIALIZE_EXPLICIT) && 
            !(prop->flags & PROPERTY_FLAG_SERIALIZE)) {
            continue;
        }
        
        if ((ctx->filter_flags & SERIALIZE_EXCLUDE_DERIVED) && 
            (prop->flags & PROPERTY_FLAG_DERIVED)) {
            continue;
        }
        
        // Align property offset for proper alignment
        total_size = align_size(total_size, 8);  // Align to 8-byte boundary
        
        // Copy property metadata to serialization context
        strncpy(ctx->properties[prop_idx].name, prop->name, MAX_PROPERTY_NAME - 1);
        ctx->properties[prop_idx].type = prop->type;
        ctx->properties[prop_idx].size = prop->size;
        strncpy(ctx->properties[prop_idx].units, prop->units, MAX_PROPERTY_UNITS - 1);
        strncpy(ctx->properties[prop_idx].description, prop->description, MAX_PROPERTY_DESCRIPTION - 1);
        ctx->properties[prop_idx].flags = prop->flags;
        ctx->properties[prop_idx].offset = total_size;
        
        // Store mapping from serialized index to extension ID
        ctx->property_id_map[prop_idx] = i;
        
        // Update total size
        total_size += prop->size;
        
        prop_idx++;
    }
    
    ctx->num_properties = prop_idx;
    ctx->total_size_per_galaxy = total_size;
    
    return 0;
}

/**
 * @brief Clean up serialization context
 * 
 * Releases resources used by the serialization context.
 * 
 * @param ctx Serialization context
 */
void property_serialization_cleanup(struct property_serialization_context *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Free allocated resources
    free(ctx->properties);
    free(ctx->property_id_map);
    free(ctx->buffer);
    
    // Reset context
    memset(ctx, 0, sizeof(struct property_serialization_context));
}

/**
 * @brief Resize the temporary buffer if needed
 * 
 * Ensures the temporary buffer is large enough for the requested size.
 * 
 * @param ctx Serialization context
 * @param size Required buffer size
 * @return 0 on success, non-zero on failure
 */
int property_serialization_ensure_buffer(struct property_serialization_context *ctx, size_t size) {
    if (ctx == NULL) {
        return -1;
    }
    
    // Check if buffer is already large enough
    if (ctx->buffer != NULL && ctx->buffer_size >= size) {
        return 0;
    }
    
    // Allocate new buffer with some extra space to reduce reallocations
    size_t new_size = size * 1.5;
    void *new_buffer = realloc(ctx->buffer, new_size);
    if (new_buffer == NULL) {
        return -1;
    }
    
    ctx->buffer = new_buffer;
    ctx->buffer_size = new_size;
    
    return 0;
}

/**
 * @brief Serialize properties for a galaxy
 * 
 * Copies and formats property data from a galaxy into a buffer.
 * 
 * @param ctx Serialization context
 * @param galaxy Source galaxy
 * @param output_buffer Output buffer for serialized data
 * @return 0 on success, non-zero on failure
 */
int property_serialize_galaxy(struct property_serialization_context *ctx,
                            const struct GALAXY *galaxy,
                            void *output_buffer) {
    if (ctx == NULL || galaxy == NULL || output_buffer == NULL) {
        return -1;
    }
    
    if (ctx->num_properties == 0) {
        // No properties to serialize
        return 0;
    }
    
    // For each property
    for (int i = 0; i < ctx->num_properties; i++) {
        const struct serialized_property_meta *prop = &ctx->properties[i];
        int extension_id = ctx->property_id_map[i];
        
        // Get property data from galaxy
        void *prop_data = galaxy_extension_get_data(galaxy, extension_id);
        if (prop_data == NULL) {
            // Property not present in this galaxy, just zero out the data
            memset((char *)output_buffer + prop->offset, 0, prop->size);
            continue;
        }
        
        // Get serializer function
        const galaxy_property_t *reg_prop = galaxy_extension_find_property_by_id(extension_id);
        void (*serialize_fn)(const void *, void *, int) = NULL;
        
        if (reg_prop && reg_prop->serialize) {
            // Use property-specific serializer if provided
            serialize_fn = reg_prop->serialize;
        } else {
            // Use default serializer for this type
            serialize_fn = property_serialization_get_default_serializer(prop->type);
        }
        
        if (serialize_fn != NULL) {
            // Use serializer function
            serialize_fn(prop_data, (char *)output_buffer + prop->offset, 1);
        } else {
            // No serializer available, just copy the data
            memcpy((char *)output_buffer + prop->offset, prop_data, prop->size);
        }
    }
    
    return 0;
}

/**
 * @brief Deserialize properties for a galaxy
 * 
 * Extracts property data from a buffer and stores it in a galaxy.
 * 
 * @param ctx Serialization context
 * @param galaxy Destination galaxy
 * @param input_buffer Input buffer containing serialized data
 * @return 0 on success, non-zero on failure
 */
int property_deserialize_galaxy(struct property_serialization_context *ctx,
                              struct GALAXY *galaxy,
                              const void *input_buffer) {
    if (ctx == NULL || galaxy == NULL || input_buffer == NULL) {
        return -1;
    }
    
    if (ctx->num_properties == 0) {
        // No properties to deserialize
        return 0;
    }
    
    // For each property
    for (int i = 0; i < ctx->num_properties; i++) {
        const struct serialized_property_meta *prop = &ctx->properties[i];
        int extension_id = ctx->property_id_map[i];
        
        // Ensure galaxy has space for this extension
        if (galaxy->extension_data == NULL || extension_id >= galaxy->num_extensions) {
            if (galaxy_extension_initialize(galaxy) != 0) {
                return -1;
            }
        }
        
        // Get or allocate property data in galaxy
        void *prop_data = galaxy_extension_get_data(galaxy, extension_id);
        if (prop_data == NULL) {
            // Property not present, try to allocate it
            galaxy->extension_data[extension_id] = calloc(1, prop->size);
            if (galaxy->extension_data[extension_id] == NULL) {
                return -1;
            }
            
            // Mark extension as in use
            galaxy->extension_flags |= (1ULL << extension_id);
            prop_data = galaxy->extension_data[extension_id];
        }
        
        // Get deserializer function
        const galaxy_property_t *reg_prop = galaxy_extension_find_property_by_id(extension_id);
        void (*deserialize_fn)(const void *, void *, int) = NULL;
        
        if (reg_prop && reg_prop->deserialize) {
            // Use property-specific deserializer if provided
            deserialize_fn = reg_prop->deserialize;
        } else {
            // Use default deserializer for this type
            deserialize_fn = property_serialization_get_default_deserializer(prop->type);
        }
        
        if (deserialize_fn != NULL) {
            // Use deserializer function
            deserialize_fn((char *)input_buffer + prop->offset, prop_data, 1);
        } else {
            // No deserializer available, just copy the data
            memcpy(prop_data, (char *)input_buffer + prop->offset, prop->size);
        }
    }
    
    return 0;
}

/**
 * @brief Calculate total size of serialized property data per galaxy
 * 
 * @param ctx Serialization context
 * @return Total size in bytes
 */
size_t property_serialization_data_size(struct property_serialization_context *ctx) {
    if (ctx == NULL) {
        return 0;
    }
    
    return ctx->total_size_per_galaxy;
}



/**
 * @brief Type-specific serializer for int32 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_int32(const void *src, void *dest, int count) {
    const int32_t *src_data = (const int32_t *)src;
    int32_t *dest_data = (int32_t *)dest;
    
    // Convert to network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = host_to_network_uint32((uint32_t)src_data[i]);
    }
}

/**
 * @brief Type-specific serializer for int64 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_int64(const void *src, void *dest, int count) {
    const int64_t *src_data = (const int64_t *)src;
    int64_t *dest_data = (int64_t *)dest;
    
    // Convert to network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = host_to_network_uint64((uint64_t)src_data[i]);
    }
}

/**
 * @brief Type-specific serializer for uint32 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_uint32(const void *src, void *dest, int count) {
    const uint32_t *src_data = (const uint32_t *)src;
    uint32_t *dest_data = (uint32_t *)dest;
    
    // Convert to network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = host_to_network_uint32(src_data[i]);
    }
}

/**
 * @brief Type-specific serializer for uint64 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_uint64(const void *src, void *dest, int count) {
    const uint64_t *src_data = (const uint64_t *)src;
    uint64_t *dest_data = (uint64_t *)dest;
    
    // Convert to network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = host_to_network_uint64(src_data[i]);
    }
}

/**
 * @brief Type-specific serializer for float values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_float(const void *src, void *dest, int count) {
    const float *src_data = (const float *)src;
    float *dest_data = (float *)dest;
    
    // Convert to network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = host_to_network_float(src_data[i]);
    }
}

/**
 * @brief Type-specific serializer for double values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_double(const void *src, void *dest, int count) {
    const double *src_data = (const double *)src;
    double *dest_data = (double *)dest;
    
    // Convert to network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = host_to_network_double(src_data[i]);
    }
}

/**
 * @brief Type-specific serializer for boolean values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_bool(const void *src, void *dest, int count) {
    const bool *src_data = (const bool *)src;
    uint8_t *dest_data = (uint8_t *)dest;
    
    // Convert bool to uint8_t (no endianness issues)
    for (int i = 0; i < count; i++) {
        dest_data[i] = src_data[i] ? 1 : 0;
    }
}

/**
 * @brief Type-specific deserializer for int32 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_int32(const void *src, void *dest, int count) {
    const int32_t *src_data = (const int32_t *)src;
    int32_t *dest_data = (int32_t *)dest;
    
    // Convert from network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = (int32_t)network_to_host_uint32((uint32_t)src_data[i]);
    }
}

/**
 * @brief Type-specific deserializer for int64 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_int64(const void *src, void *dest, int count) {
    const int64_t *src_data = (const int64_t *)src;
    int64_t *dest_data = (int64_t *)dest;
    
    // Convert from network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = (int64_t)network_to_host_uint64((uint64_t)src_data[i]);
    }
}

/**
 * @brief Type-specific deserializer for uint32 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_uint32(const void *src, void *dest, int count) {
    const uint32_t *src_data = (const uint32_t *)src;
    uint32_t *dest_data = (uint32_t *)dest;
    
    // Convert from network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = network_to_host_uint32(src_data[i]);
    }
}

/**
 * @brief Type-specific deserializer for uint64 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_uint64(const void *src, void *dest, int count) {
    const uint64_t *src_data = (const uint64_t *)src;
    uint64_t *dest_data = (uint64_t *)dest;
    
    // Convert from network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = network_to_host_uint64(src_data[i]);
    }
}

/**
 * @brief Type-specific deserializer for float values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_float(const void *src, void *dest, int count) {
    const float *src_data = (const float *)src;
    float *dest_data = (float *)dest;
    
    // Convert from network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = network_to_host_float(src_data[i]);
    }
}

/**
 * @brief Type-specific deserializer for double values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_double(const void *src, void *dest, int count) {
    const double *src_data = (const double *)src;
    double *dest_data = (double *)dest;
    
    // Convert from network byte order (big-endian)
    for (int i = 0; i < count; i++) {
        dest_data[i] = network_to_host_double(src_data[i]);
    }
}

/**
 * @brief Type-specific deserializer for boolean values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_bool(const void *src, void *dest, int count) {
    const uint8_t *src_data = (const uint8_t *)src;
    bool *dest_data = (bool *)dest;
    
    // Convert uint8_t to bool (no endianness issues)
    for (int i = 0; i < count; i++) {
        dest_data[i] = src_data[i] != 0;
    }
}

/**
 * @brief Get default serializer for a property type
 * 
 * @param type Property type
 * @return Serializer function pointer, or NULL if not available
 */
void (*property_serialization_get_default_serializer(enum galaxy_property_type type))(const void *, void *, int) {
    switch (type) {
        case PROPERTY_TYPE_FLOAT:
            return serialize_float;
        case PROPERTY_TYPE_DOUBLE:
            return serialize_double;
        case PROPERTY_TYPE_INT32:
            return serialize_int32;
        case PROPERTY_TYPE_INT64:
            return serialize_int64;
        case PROPERTY_TYPE_UINT32:
            return serialize_uint32;
        case PROPERTY_TYPE_UINT64:
            return serialize_uint64;
        case PROPERTY_TYPE_BOOL:
            return serialize_bool;
        default:
            return NULL;  // No default serializer for complex types
    }
}

/**
 * @brief Get default deserializer for a property type
 * 
 * @param type Property type
 * @return Deserializer function pointer, or NULL if not available
 */
void (*property_serialization_get_default_deserializer(enum galaxy_property_type type))(const void *, void *, int) {
    switch (type) {
        case PROPERTY_TYPE_FLOAT:
            return deserialize_float;
        case PROPERTY_TYPE_DOUBLE:
            return deserialize_double;
        case PROPERTY_TYPE_INT32:
            return deserialize_int32;
        case PROPERTY_TYPE_INT64:
            return deserialize_int64;
        case PROPERTY_TYPE_UINT32:
            return deserialize_uint32;
        case PROPERTY_TYPE_UINT64:
            return deserialize_uint64;
        case PROPERTY_TYPE_BOOL:
            return deserialize_bool;
        default:
            return NULL;  // No default deserializer for complex types
    }
}