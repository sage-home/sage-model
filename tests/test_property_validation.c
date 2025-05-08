#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// Include core headers
#include "../src/core/core_allvars.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/core/core_logging.h"

// Include I/O headers
#include "../src/io/io_interface.h"
#include "../src/io/io_endian_utils.h"
#include "../src/io/io_property_serialization.h"
#include "../src/io/io_validation.h"

// Mock extension registry for testing
struct galaxy_extension_registry mock_registry;

// Mock properties for testing
galaxy_property_t mock_properties[10];

// Mock I/O handlers
struct io_interface binary_handler = {
    .name = "Binary Format Handler (Deprecated)",
    .version = "1.0",
    .format_id = -1, // Binary output format is deprecated
    .capabilities = 0,
    .initialize = NULL,
    .read_forest = NULL,
    .write_galaxies = NULL,
    .cleanup = NULL,
    .close_open_handles = NULL,
    .get_open_handle_count = NULL
};

struct io_interface hdf5_handler = {
    .name = "HDF5 Format Handler",
    .version = "1.0",
    .format_id = IO_FORMAT_HDF5_OUTPUT,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_EXTENDED_PROPS | IO_CAP_METADATA_ATTRS,
    .initialize = NULL,
    .read_forest = NULL,
    .write_galaxies = NULL,
    .cleanup = NULL,
    .close_open_handles = NULL,
    .get_open_handle_count = NULL
};

/**
 * @brief Setup mock extension registry for testing
 */
void setup_mock_registry() {
    // Initialize mock registry
    memset(&mock_registry, 0, sizeof(struct galaxy_extension_registry));
    
    // Initialize mock properties
    int prop_idx = 0;
    
    // Valid float property
    strcpy(mock_properties[prop_idx].name, "TestFloat");
    mock_properties[prop_idx].size = sizeof(float);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_FLOAT;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_float;
    mock_properties[prop_idx].deserialize = deserialize_float;
    strcpy(mock_properties[prop_idx].description, "Test float property");
    strcpy(mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Valid double property
    strcpy(mock_properties[prop_idx].name, "TestDouble");
    mock_properties[prop_idx].size = sizeof(double);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_DOUBLE;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_double;
    mock_properties[prop_idx].deserialize = deserialize_double;
    strcpy(mock_properties[prop_idx].description, "Test double property");
    strcpy(mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Invalid property (missing serialization functions)
    strcpy(mock_properties[prop_idx].name, "InvalidProperty");
    mock_properties[prop_idx].size = sizeof(int32_t);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_INT32;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = NULL;  // Missing serializer
    mock_properties[prop_idx].deserialize = NULL;  // Missing deserializer
    strcpy(mock_properties[prop_idx].description, "Test invalid property");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Invalid property (duplicate name)
    strcpy(mock_properties[prop_idx].name, "TestFloat");  // Duplicate name
    mock_properties[prop_idx].size = sizeof(int64_t);
    mock_properties[prop_idx].module_id = 2;  // Different module
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_INT64;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_int64;
    mock_properties[prop_idx].deserialize = deserialize_int64;
    strcpy(mock_properties[prop_idx].description, "Test duplicate property");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Property with special characters (for HDF5 compatibility testing)
    strcpy(mock_properties[prop_idx].name, "Test/Property+Special!Chars");
    mock_properties[prop_idx].size = sizeof(uint32_t);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_UINT32;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_uint32;
    mock_properties[prop_idx].deserialize = deserialize_uint32;
    strcpy(mock_properties[prop_idx].description, "Test property with special characters");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Large property (for size limit testing)
    strcpy(mock_properties[prop_idx].name, "LargeProperty");
    mock_properties[prop_idx].size = MAX_SERIALIZED_ARRAY_SIZE + 100;  // Exceeds recommended size
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_FLOAT;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_float;
    mock_properties[prop_idx].deserialize = deserialize_float;
    strcpy(mock_properties[prop_idx].description, "Test large property");
    strcpy(mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Struct property (for format compatibility testing)
    strcpy(mock_properties[prop_idx].name, "StructProperty");
    mock_properties[prop_idx].size = 128;  // Arbitrary size
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_STRUCT;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_float;  // Placeholder
    mock_properties[prop_idx].deserialize = deserialize_float;  // Placeholder
    strcpy(mock_properties[prop_idx].description, "Test struct property");
    strcpy(mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Add properties to mock registry
    for (int i = 0; i < prop_idx; i++) {
        mock_registry.extensions[i] = mock_properties[i];
    }
    mock_registry.num_extensions = prop_idx;
    
    // Set global registry pointer to our mock
    global_extension_registry = &mock_registry;
}

/**
 * @brief Test property type validation
 */
void test_property_type_validation() {
    struct validation_context ctx;
    int status;
    
    printf("Testing property type validation...\n");
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test valid property types
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_FLOAT, "test", "FloatProperty");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_INT32, "test", "IntProperty");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test invalid property type
    status = VALIDATE_PROPERTY_TYPE(&ctx, 99, "test", "InvalidProperty");
    assert(status != 0);
    assert(validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test complex property types (struct and array)
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_STRUCT, "test", "StructProperty");
    assert(status == 0);
    assert(validation_has_warnings(&ctx));  // Should have a warning
    assert(!validation_has_errors(&ctx));  // But no errors
    
    validation_reset(&ctx);
    
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_ARRAY, "test", "ArrayProperty");
    assert(status == 0);
    assert(validation_has_warnings(&ctx));  // Should have a warning
    assert(!validation_has_errors(&ctx));  // But no errors
    
    validation_cleanup(&ctx);
    
    printf("Property type validation tests passed\n");
}

/**
 * @brief Test property serialization validation
 */
void test_property_serialization_validation() {
    struct validation_context ctx;
    int status;
    
    printf("Testing property serialization validation...\n");
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test valid property with serialization functions
    status = VALIDATE_PROPERTY_SERIALIZATION(&ctx, &mock_properties[0], "test");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test property without serialization functions
    status = VALIDATE_PROPERTY_SERIALIZATION(&ctx, &mock_properties[2], "test");
    assert(status != 0);
    assert(validation_has_errors(&ctx));
    
    validation_cleanup(&ctx);
    
    printf("Property serialization validation tests passed\n");
}

/**
 * @brief Test property uniqueness validation
 */
void test_property_uniqueness_validation() {
    struct validation_context ctx;
    int status;
    
    printf("Testing property uniqueness validation...\n");
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test unique property name
    status = VALIDATE_PROPERTY_UNIQUENESS(&ctx, &mock_properties[1], "test");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test duplicate property name
    status = VALIDATE_PROPERTY_UNIQUENESS(&ctx, &mock_properties[3], "test");
    assert(status != 0);
    assert(validation_has_errors(&ctx));
    
    validation_cleanup(&ctx);
    
    printf("Property uniqueness validation tests passed\n");
}

/**
 * @brief Test serialization context validation
 */
void test_serialization_context_validation() {
    struct validation_context ctx;
    struct property_serialization_context ser_ctx;
    int status;
    
    printf("Testing serialization context validation...\n");
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Initialize valid serialization context
    memset(&ser_ctx, 0, sizeof(struct property_serialization_context));
    ser_ctx.version = PROPERTY_SERIALIZATION_VERSION;
    ser_ctx.num_properties = 2;
    
    struct serialized_property_meta properties[2];
    memset(properties, 0, sizeof(properties));
    strcpy(properties[0].name, "TestProperty1");
    properties[0].type = PROPERTY_TYPE_FLOAT;
    properties[0].size = sizeof(float);
    properties[0].offset = 0;
    
    strcpy(properties[1].name, "TestProperty2");
    properties[1].type = PROPERTY_TYPE_INT32;
    properties[1].size = sizeof(int32_t);
    properties[1].offset = sizeof(float);
    
    ser_ctx.properties = properties;
    ser_ctx.total_size_per_galaxy = sizeof(float) + sizeof(int32_t);
    
    int property_map[2] = {0, 1};
    ser_ctx.property_id_map = property_map;
    
    // Test valid serialization context
    status = VALIDATE_SERIALIZATION_CONTEXT(&ctx, &ser_ctx, "test");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Create invalid serialization context (wrong version)
    ser_ctx.version = 999;  // Invalid version
    
    // Test invalid serialization context
    status = VALIDATE_SERIALIZATION_CONTEXT(&ctx, &ser_ctx, "test");
    assert(status != 0);
    assert(validation_has_errors(&ctx));
    
    // Cleanup - no need to call property_serialization_cleanup since we didn't allocate memory
    validation_cleanup(&ctx);
    
    printf("Serialization context validation tests passed\n");
}

/**
 * @brief Test binary property compatibility validation
 */
void test_binary_property_compatibility() {
    struct validation_context ctx;
    int status;
    
    printf("Testing binary property compatibility validation...\n");
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test valid property with binary format
    status = VALIDATE_BINARY_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[0], "test");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test property without serialization functions
    status = VALIDATE_BINARY_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[2], "test");
    assert(status != 0);
    assert(validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test struct property (should warn but pass)
    status = VALIDATE_BINARY_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[6], "test");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    assert(validation_has_warnings(&ctx));
    
    validation_reset(&ctx);
    
    // Test large property (should warn but pass for binary)
    status = VALIDATE_BINARY_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[5], "test");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    assert(validation_has_warnings(&ctx));
    
    validation_cleanup(&ctx);
    
    printf("Binary property compatibility tests passed\n");
}

/**
 * @brief Test HDF5 property compatibility validation
 */
void test_hdf5_property_compatibility() {
    struct validation_context ctx;
    int status;
    
    printf("Testing HDF5 property compatibility validation...\n");
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test valid property with HDF5 format
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[0], "test");
    assert(status == 0);
    assert(!validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test property without serialization functions
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[2], "test");
    assert(status != 0);
    assert(validation_has_errors(&ctx));
    
    validation_reset(&ctx);
    
    // Test struct property (should warn for HDF5)
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[6], "test");
    assert(status == 0);
    assert(validation_has_warnings(&ctx));
    
    validation_reset(&ctx);
    
    // Test large property (should warn for HDF5)
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[5], "test");
    assert(status == 0);
    assert(validation_has_warnings(&ctx));
    
    validation_reset(&ctx);
    
    // Test property with special characters (should warn for HDF5)
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &mock_properties[4], "test");
    assert(status == 0);
    assert(validation_has_warnings(&ctx));
    
    validation_cleanup(&ctx);
    
    printf("HDF5 property compatibility tests passed\n");
}

/**
 * @brief Main function
 */
int main() {
    printf("--- Property Validation Tests ---\n");
    
    // Set up mock registry
    setup_mock_registry();
    
    // Run tests
    test_property_type_validation();
    test_property_serialization_validation();
    test_property_uniqueness_validation();
    test_serialization_context_validation();
    test_binary_property_compatibility();
    test_hdf5_property_compatibility();
    
    printf("All tests PASSED\n");
    
    return 0;
}