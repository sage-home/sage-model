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
 * @brief Get human-readable error message for error code
 */
const char *property_serialization_error_string(enum property_serialization_error error_code) {
    switch (error_code) {
        case PROPERTY_SERIALIZATION_SUCCESS:
            return "Success";
        case PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER:
            return "NULL parameter provided";
        case PROPERTY_SERIALIZATION_ERROR_INVALID_CONTEXT:
            return "Invalid serialization context";
        case PROPERTY_SERIALIZATION_ERROR_PROPERTY_NOT_FOUND:
            return "Property not found";
        case PROPERTY_SERIALIZATION_ERROR_SERIALIZER_NOT_FOUND:
            return "Serializer function not found";
        case PROPERTY_SERIALIZATION_ERROR_BUFFER_TOO_SMALL:
            return "Buffer too small for serialization";
        case PROPERTY_SERIALIZATION_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case PROPERTY_SERIALIZATION_ERROR_INVALID_PROPERTY_TYPE:
            return "Invalid property type";
        case PROPERTY_SERIALIZATION_ERROR_ARRAY_SIZE_MISMATCH:
            return "Array size mismatch";
        case PROPERTY_SERIALIZATION_ERROR_DATA_VALIDATION_FAILED:
            return "Data validation failed";
        default:
            return "Unknown error";
    }
}

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
    if (ctx == NULL) {
        LOG_ERROR("Property serialization context is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (output_buffer == NULL) {
        LOG_ERROR("Output buffer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (ctx->num_properties == 0) {
        LOG_DEBUG("No properties to serialize for galaxy %lld", galaxy->GalaxyIndex);
        return PROPERTY_SERIALIZATION_SUCCESS;
    }
    
    LOG_DEBUG("Serializing %d properties for galaxy %lld", ctx->num_properties, galaxy->GalaxyIndex);
    
    // For each property
    for (int i = 0; i < ctx->num_properties; i++) {
        const struct serialized_property_meta *prop = &ctx->properties[i];
        int extension_id = ctx->property_id_map[i];
        
        LOG_DEBUG("Serializing property %d: '%s' (extension_id=%d, type=%d)", 
                 i, prop->name, extension_id, prop->type);
        
        // Get property data from galaxy
        void *prop_data = galaxy_extension_get_data(galaxy, extension_id);
        if (prop_data == NULL) {
            LOG_DEBUG("Property '%s' not present in galaxy %lld, zeroing data", 
                     prop->name, galaxy->GalaxyIndex);
            memset((char *)output_buffer + prop->offset, 0, prop->size);
            continue;
        }
        
        // Get serializer function
        const galaxy_property_t *reg_prop = galaxy_extension_find_property_by_id(extension_id);
        void (*serialize_fn)(const void *, void *, int) = NULL;
        
        if (reg_prop && reg_prop->serialize) {
            serialize_fn = reg_prop->serialize;
            LOG_DEBUG("Using property-specific serializer for '%s'", prop->name);
        } else {
            serialize_fn = property_serialization_get_default_serializer(prop->type);
            if (serialize_fn) {
                LOG_DEBUG("Using default serializer for '%s' (type %d)", prop->name, prop->type);
            }
        }
        
        if (serialize_fn != NULL) {
            // Use serializer function
            serialize_fn(prop_data, (char *)output_buffer + prop->offset, 1);
        } else {
            LOG_WARNING("No serializer for property '%s' (type %d), using memcpy", 
                       prop->name, prop->type);
            memcpy((char *)output_buffer + prop->offset, prop_data, prop->size);
        }
    }
    
    LOG_DEBUG("Successfully serialized %d properties for galaxy %lld", 
             ctx->num_properties, galaxy->GalaxyIndex);
    
    return PROPERTY_SERIALIZATION_SUCCESS;
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
    if (ctx == NULL) {
        LOG_ERROR("Property serialization context is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (input_buffer == NULL) {
        LOG_ERROR("Input buffer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (ctx->num_properties == 0) {
        LOG_DEBUG("No properties to deserialize for galaxy %lld", galaxy->GalaxyIndex);
        return PROPERTY_SERIALIZATION_SUCCESS;
    }
    
    LOG_DEBUG("Deserializing %d properties for galaxy %lld", ctx->num_properties, galaxy->GalaxyIndex);
    
    // For each property
    for (int i = 0; i < ctx->num_properties; i++) {
        const struct serialized_property_meta *prop = &ctx->properties[i];
        int extension_id = ctx->property_id_map[i];
        
        LOG_DEBUG("Deserializing property %d: '%s' (extension_id=%d, type=%d)", 
                 i, prop->name, extension_id, prop->type);
        
        // Ensure galaxy has space for this extension
        if (galaxy->extension_data == NULL || extension_id >= galaxy->num_extensions) {
            LOG_DEBUG("Initializing galaxy extension system for galaxy %lld", galaxy->GalaxyIndex);
            if (galaxy_extension_initialize(galaxy) != 0) {
                LOG_ERROR("Failed to initialize galaxy extension system for galaxy %lld", galaxy->GalaxyIndex);
                return PROPERTY_SERIALIZATION_ERROR_MEMORY_ALLOCATION;
            }
        }
        
        // Get or allocate property data in galaxy
        void *prop_data = galaxy_extension_get_data(galaxy, extension_id);
        if (prop_data == NULL) {
            LOG_DEBUG("Allocating extension data for property '%s' (size=%zu)", prop->name, prop->size);
            // Property not present, try to allocate it
            galaxy->extension_data[extension_id] = calloc(1, prop->size);
            if (galaxy->extension_data[extension_id] == NULL) {
                LOG_ERROR("Failed to allocate %zu bytes for property '%s'", prop->size, prop->name);
                return PROPERTY_SERIALIZATION_ERROR_MEMORY_ALLOCATION;
            }
            
            // Mark extension as in use
            galaxy->extension_flags |= (1ULL << extension_id);
            prop_data = galaxy->extension_data[extension_id];
        }
        
        // Get deserializer function
        const galaxy_property_t *reg_prop = galaxy_extension_find_property_by_id(extension_id);
        void (*deserialize_fn)(const void *, void *, int) = NULL;
        
        if (reg_prop && reg_prop->deserialize) {
            deserialize_fn = reg_prop->deserialize;
            LOG_DEBUG("Using property-specific deserializer for '%s'", prop->name);
        } else {
            deserialize_fn = property_serialization_get_default_deserializer(prop->type);
            if (deserialize_fn) {
                LOG_DEBUG("Using default deserializer for '%s' (type %d)", prop->name, prop->type);
            }
        }
        
        if (deserialize_fn != NULL) {
            // Use deserializer function
            deserialize_fn((char *)input_buffer + prop->offset, prop_data, 1);
        } else {
            LOG_WARNING("No deserializer for property '%s' (type %d), using memcpy", 
                       prop->name, prop->type);
            memcpy(prop_data, (char *)input_buffer + prop->offset, prop->size);
        }
    }
    
    LOG_DEBUG("Successfully deserialized %d properties for galaxy %lld", 
             ctx->num_properties, galaxy->GalaxyIndex);
    
    return PROPERTY_SERIALIZATION_SUCCESS;
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
 * @brief Generic array serializer using memcpy
 * 
 * For array properties, we use direct memory copy. The entire array data
 * is copied as-is since arrays are treated as opaque data blobs.
 * Note: The count parameter represents the number of array properties (usually 1),
 * not the number of array elements.
 * 
 * @param src Source array data
 * @param dest Destination buffer
 * @param count Number of array properties (usually 1)
 */
void serialize_array(const void *src, void *dest, int count) {
    // For array properties, we need to determine the size from context
    // Since we don't have direct access to size here, we rely on the caller
    // to ensure the correct amount of data is copied
    // This is typically handled at a higher level in the serialization process
    if (src && dest && count > 0) {
        // The size should be determined by the property metadata at the caller level
        // For now, we assume the caller manages the size correctly
        // This function should not be called directly for arrays
        // Arrays should be handled by the default memcpy path in the main serialization loop
    }
}

/**
 * @brief Generic array deserializer using memcpy
 * 
 * For array properties, we use direct memory copy. The entire array data
 * is copied as-is since arrays are treated as opaque data blobs.
 * Note: The count parameter represents the number of array properties (usually 1),
 * not the number of array elements.
 * 
 * @param src Source buffer data
 * @param dest Destination array
 * @param count Number of array properties (usually 1)
 */
void deserialize_array(const void *src, void *dest, int count) {
    // For array properties, we need to determine the size from context
    // Since we don't have direct access to size here, we rely on the caller
    // to ensure the correct amount of data is copied
    // This is typically handled at a higher level in the deserialization process
    if (src && dest && count > 0) {
        // The size should be determined by the property metadata at the caller level
        // For now, we assume the caller manages the size correctly
        // This function should not be called directly for arrays
        // Arrays should be handled by the default memcpy path in the main deserialization loop
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
            return NULL;  // No default serializer for complex types, fall back to memcpy
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
            return NULL;  // No default deserializer for complex types, fall back to memcpy
    }
}
/**
 * @brief Enhanced galaxy property serialization with buffer size validation
 */
int property_serialize_galaxy_safe(struct property_serialization_context *ctx,
                                  const struct GALAXY *galaxy,
                                  void *output_buffer,
                                  size_t buffer_size) {
    if (ctx == NULL) {
        LOG_ERROR("Property serialization context is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (output_buffer == NULL) {
        LOG_ERROR("Output buffer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    // Check buffer size
    size_t required_size = property_serialization_data_size(ctx);
    if (buffer_size < required_size) {
        LOG_ERROR("Buffer too small: need %zu bytes, got %zu bytes", required_size, buffer_size);
        return PROPERTY_SERIALIZATION_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Use the standard serialization function
    return property_serialize_galaxy(ctx, galaxy, output_buffer);
}

/**
 * @brief Enhanced galaxy property deserialization with buffer size validation
 */
int property_deserialize_galaxy_safe(struct property_serialization_context *ctx,
                                    struct GALAXY *galaxy,
                                    const void *input_buffer,
                                    size_t buffer_size) {
    if (ctx == NULL) {
        LOG_ERROR("Property serialization context is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (galaxy == NULL) {
        LOG_ERROR("Galaxy pointer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (input_buffer == NULL) {
        LOG_ERROR("Input buffer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    // Check buffer size
    size_t required_size = property_serialization_data_size(ctx);
    if (buffer_size < required_size) {
        LOG_ERROR("Buffer too small: need %zu bytes, got %zu bytes", required_size, buffer_size);
        return PROPERTY_SERIALIZATION_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Use the standard deserialization function
    return property_deserialize_galaxy(ctx, galaxy, input_buffer);
}

/**
 * @brief Validate array property data
 */
int validate_array_property_data(const void *data, size_t expected_element_size, 
                                size_t expected_count, const char *property_name) {
    if (data == NULL) {
        LOG_ERROR("Array property '%s': NULL data pointer", property_name ? property_name : "unknown");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (expected_element_size == 0) {
        LOG_ERROR("Array property '%s': Invalid element size (0)", property_name ? property_name : "unknown");
        return PROPERTY_SERIALIZATION_ERROR_INVALID_PROPERTY_TYPE;
    }
    
    if (expected_count == 0) {
        LOG_WARNING("Array property '%s': Zero element count", property_name ? property_name : "unknown");
    }
    
    // Additional validation could be added here (e.g., checking for reasonable array sizes)
    if (expected_count > 10000) {  // Reasonable upper limit
        LOG_WARNING("Array property '%s': Very large array size (%zu elements)", 
                   property_name ? property_name : "unknown", expected_count);
    }
    
    LOG_DEBUG("Array property '%s' validation passed: %zu elements of %zu bytes each",
             property_name ? property_name : "unknown", expected_count, expected_element_size);
    
    return PROPERTY_SERIALIZATION_SUCCESS;
}

/**
 * @brief Serialize array property with size validation
 */
int serialize_array_property(const void *src_array, size_t element_size, size_t count,
                           void *dest_buffer, size_t dest_buffer_size,
                           void (*element_serializer)(const void *, void *, int)) {
    if (src_array == NULL) {
        LOG_ERROR("Source array is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (dest_buffer == NULL) {
        LOG_ERROR("Destination buffer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    size_t required_size = element_size * count;
    if (dest_buffer_size < required_size) {
        LOG_ERROR("Destination buffer too small: need %zu bytes, got %zu bytes", 
                 required_size, dest_buffer_size);
        return PROPERTY_SERIALIZATION_ERROR_BUFFER_TOO_SMALL;
    }
    
    if (element_serializer != NULL) {
        // Use element-specific serializer
        for (size_t i = 0; i < count; i++) {
            const char *src_element = (const char *)src_array + (i * element_size);
            char *dest_element = (char *)dest_buffer + (i * element_size);
            element_serializer(src_element, dest_element, 1);
        }
    } else {
        // Simple memory copy
        memcpy(dest_buffer, src_array, required_size);
    }
    
    LOG_DEBUG("Successfully serialized array: %zu elements of %zu bytes each", count, element_size);
    
    return PROPERTY_SERIALIZATION_SUCCESS;
}

/**
 * @brief Deserialize array property with size validation
 */
int deserialize_array_property(const void *src_buffer, size_t src_buffer_size,
                             void *dest_array, size_t element_size, size_t expected_count,
                             void (*element_deserializer)(const void *, void *, int)) {
    if (src_buffer == NULL) {
        LOG_ERROR("Source buffer is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    if (dest_array == NULL) {
        LOG_ERROR("Destination array is NULL");
        return PROPERTY_SERIALIZATION_ERROR_NULL_PARAMETER;
    }
    
    size_t required_size = element_size * expected_count;
    if (src_buffer_size < required_size) {
        LOG_ERROR("Source buffer too small: need %zu bytes, got %zu bytes", 
                 required_size, src_buffer_size);
        return PROPERTY_SERIALIZATION_ERROR_BUFFER_TOO_SMALL;
    }
    
    if (element_deserializer != NULL) {
        // Use element-specific deserializer
        for (size_t i = 0; i < expected_count; i++) {
            const char *src_element = (const char *)src_buffer + (i * element_size);
            char *dest_element = (char *)dest_array + (i * element_size);
            element_deserializer(src_element, dest_element, 1);
        }
    } else {
        // Simple memory copy
        memcpy(dest_array, src_buffer, required_size);
    }
    
    LOG_DEBUG("Successfully deserialized array: %zu elements of %zu bytes each", expected_count, element_size);
    
    return PROPERTY_SERIALIZATION_SUCCESS;
}
