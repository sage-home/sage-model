#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h"
#include "../core/core_galaxy_extensions.h"
#include "io_interface.h"
#include "io_endian_utils.h"

/**
 * @file io_property_serialization.h
 * @brief Galaxy extended property serialization utilities
 *
 * This file provides utilities for serializing and deserializing
 * extended galaxy properties. These utilities are used by both binary
 * and HDF5 output handlers to save and load module-specific galaxy properties.
 */

/**
 * @brief Property serialization version
 * 
 * Increment this when the serialization format changes in a way that breaks
 * backward compatibility.
 */
#define PROPERTY_SERIALIZATION_VERSION 1

/**
 * @brief Magic marker to identify extended property data
 * 
 * This marker appears in binary files to indicate the start of extended property data.
 */
#define PROPERTY_SERIALIZATION_MAGIC 0x45585450  // "EXTP" in ASCII hex

/**
 * @brief Maximum property array size for serialization
 */
#define MAX_SERIALIZED_ARRAY_SIZE 1024

/**
 * @brief Property serialization filter flags
 * 
 * These flags control which properties are serialized.
 */
enum property_serialization_filter {
    SERIALIZE_ALL       = 0,        /**< Serialize all properties */
    SERIALIZE_EXPLICIT  = (1 << 0),  /**< Only serialize properties with PROPERTY_FLAG_SERIALIZE */
    SERIALIZE_EXCLUDE_DERIVED = (1 << 1)  /**< Exclude properties with PROPERTY_FLAG_DERIVED */
};

/**
 * @brief Serialized property metadata
 * 
 * Contains information about a serialized property.
 */
struct serialized_property_meta {
    char name[MAX_PROPERTY_NAME];          /**< Property name */
    enum galaxy_property_type type;        /**< Property data type */
    size_t size;                           /**< Size in bytes */
    char units[MAX_PROPERTY_UNITS];        /**< Units (if applicable) */
    char description[MAX_PROPERTY_DESCRIPTION]; /**< Property description */
    uint32_t flags;                        /**< Property flags */
    int64_t offset;                        /**< Offset within property data block */
};

/**
 * @brief Property serialization context
 * 
 * Contains information and state for serializing/deserializing properties.
 */
struct property_serialization_context {
    int num_properties;                     /**< Number of properties to serialize */
    struct serialized_property_meta *properties; /**< Array of property metadata */
    size_t total_size_per_galaxy;           /**< Total size of all properties per galaxy */
    int *property_id_map;                   /**< Maps serialized property index to extension_id */
    void *buffer;                           /**< Temporary buffer for data conversion */
    size_t buffer_size;                     /**< Size of temporary buffer */
    bool endian_swap;                       /**< Whether endian swapping is needed */
    int version;                            /**< Serialization format version */
    uint32_t filter_flags;                  /**< Serialization filter flags */
};

/**
 * @brief Initialize property serialization context
 * 
 * Sets up a context for serializing/deserializing properties.
 * 
 * @param ctx Context to initialize
 * @param filter_flags Flags controlling which properties to serialize
 * @return 0 on success, non-zero on failure
 */
int property_serialization_init(struct property_serialization_context *ctx, uint32_t filter_flags);

/**
 * @brief Add properties to be serialized
 * 
 * Scans the galaxy extension registry and adds properties to the context
 * based on the filter flags.
 * 
 * @param ctx Serialization context
 * @return 0 on success, non-zero on failure
 */
int property_serialization_add_properties(struct property_serialization_context *ctx);

/**
 * @brief Clean up serialization context
 * 
 * Releases resources used by the serialization context.
 * 
 * @param ctx Serialization context
 */
void property_serialization_cleanup(struct property_serialization_context *ctx);

/**
 * @brief Resize the temporary buffer if needed
 * 
 * Ensures the temporary buffer is large enough for the requested size.
 * 
 * @param ctx Serialization context
 * @param size Required buffer size
 * @return 0 on success, non-zero on failure
 */
int property_serialization_ensure_buffer(struct property_serialization_context *ctx, size_t size);

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
                            void *output_buffer);

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
                              const void *input_buffer);

/**
 * @brief Calculate total size of serialized property data per galaxy
 * 
 * @param ctx Serialization context
 * @return Total size in bytes
 */
size_t property_serialization_data_size(struct property_serialization_context *ctx);

/**
 * @brief Create a binary property header
 * 
 * Creates a binary representation of the property metadata.
 * 
 * @param ctx Serialization context
 * @param output_buffer Output buffer for the header
 * @param buffer_size Size of the output buffer
 * @return Size of the header on success, negative value on failure
 */
int64_t property_serialization_create_header(struct property_serialization_context *ctx,
                                           void *output_buffer,
                                           size_t buffer_size);

/**
 * @brief Parse a binary property header
 * 
 * Extracts property metadata from a binary header.
 * 
 * @param ctx Serialization context
 * @param input_buffer Input buffer containing the header
 * @param buffer_size Size of the input buffer
 * @return 0 on success, non-zero on failure
 */
int property_serialization_parse_header(struct property_serialization_context *ctx,
                                      const void *input_buffer,
                                      size_t buffer_size);

/**
 * @brief Type-specific serializer for int32 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_int32(const void *src, void *dest, int count);

/**
 * @brief Type-specific serializer for int64 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_int64(const void *src, void *dest, int count);

/**
 * @brief Type-specific serializer for uint32 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_uint32(const void *src, void *dest, int count);

/**
 * @brief Type-specific serializer for uint64 values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_uint64(const void *src, void *dest, int count);

/**
 * @brief Type-specific serializer for float values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_float(const void *src, void *dest, int count);

/**
 * @brief Type-specific serializer for double values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_double(const void *src, void *dest, int count);

/**
 * @brief Type-specific serializer for boolean values
 * 
 * @param src Source data
 * @param dest Destination buffer
 * @param count Number of elements
 */
void serialize_bool(const void *src, void *dest, int count);

/**
 * @brief Type-specific deserializer for int32 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_int32(const void *src, void *dest, int count);

/**
 * @brief Type-specific deserializer for int64 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_int64(const void *src, void *dest, int count);

/**
 * @brief Type-specific deserializer for uint32 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_uint32(const void *src, void *dest, int count);

/**
 * @brief Type-specific deserializer for uint64 values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_uint64(const void *src, void *dest, int count);

/**
 * @brief Type-specific deserializer for float values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_float(const void *src, void *dest, int count);

/**
 * @brief Type-specific deserializer for double values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_double(const void *src, void *dest, int count);

/**
 * @brief Type-specific deserializer for boolean values
 * 
 * @param src Source buffer
 * @param dest Destination data
 * @param count Number of elements
 */
void deserialize_bool(const void *src, void *dest, int count);

/**
 * @brief Get default serializer for a property type
 * 
 * @param type Property type
 * @return Serializer function pointer, or NULL if not available
 */
void (*property_serialization_get_default_serializer(enum galaxy_property_type type))(const void *, void *, int);

/**
 * @brief Get default deserializer for a property type
 * 
 * @param type Property type
 * @return Deserializer function pointer, or NULL if not available
 */
void (*property_serialization_get_default_deserializer(enum galaxy_property_type type))(const void *, void *, int);

#ifdef __cplusplus
}
#endif