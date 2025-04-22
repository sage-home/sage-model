#pragma once

/**
 * @brief Maximum length of property name
 */
#define MAX_PROPERTY_NAME 64

/**
 * @brief Maximum length of property units string
 */
#define MAX_PROPERTY_UNITS 32

/**
 * @brief Maximum length of property description
 */
#define MAX_PROPERTY_DESCRIPTION 256

/**
 * Property data types supported by the serialization system
 */
enum galaxy_property_type {
    PROPERTY_TYPE_FLOAT = 1,
    PROPERTY_TYPE_DOUBLE,
    PROPERTY_TYPE_INT32,
    PROPERTY_TYPE_INT64,
    PROPERTY_TYPE_UINT32,
    PROPERTY_TYPE_UINT64,
    PROPERTY_TYPE_BOOL,
    PROPERTY_TYPE_STRUCT,
    PROPERTY_TYPE_ARRAY,
    PROPERTY_TYPE_MAX
};