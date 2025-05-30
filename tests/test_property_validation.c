/**
 * Test suite for Property Validation System
 * 
 * Tests cover:
 * - Property type validation
 * - Property serialization validation  
 * - Property uniqueness validation
 * - Serialization context validation
 * - HDF5 property compatibility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Include core headers
#include "../src/core/core_allvars.h"
#include "../src/core/core_galaxy_extensions.h"
#include "../src/core/core_logging.h"

// Include I/O headers
#include "../src/io/io_interface.h"
#include "../src/io/io_endian_utils.h"
#include "../src/io/io_property_serialization.h"
#include "../src/io/io_validation.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test fixtures
static struct test_context {
    struct galaxy_extension_registry mock_registry;
    galaxy_property_t mock_properties[10];
    int initialized;
} test_ctx;

// Mock I/O handlers
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
 * @brief Setup test context - called before tests
 */
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize mock properties
    int prop_idx = 0;
    
    // Valid float property
    strcpy(test_ctx.mock_properties[prop_idx].name, "TestFloat");
    test_ctx.mock_properties[prop_idx].size = sizeof(float);
    test_ctx.mock_properties[prop_idx].module_id = 1;
    test_ctx.mock_properties[prop_idx].extension_id = prop_idx;
    test_ctx.mock_properties[prop_idx].type = PROPERTY_TYPE_FLOAT;
    test_ctx.mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    test_ctx.mock_properties[prop_idx].serialize = serialize_float;
    test_ctx.mock_properties[prop_idx].deserialize = deserialize_float;
    strcpy(test_ctx.mock_properties[prop_idx].description, "Test float property");
    strcpy(test_ctx.mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Valid double property
    strcpy(test_ctx.mock_properties[prop_idx].name, "TestDouble");
    test_ctx.mock_properties[prop_idx].size = sizeof(double);
    test_ctx.mock_properties[prop_idx].module_id = 1;
    test_ctx.mock_properties[prop_idx].extension_id = prop_idx;
    test_ctx.mock_properties[prop_idx].type = PROPERTY_TYPE_DOUBLE;
    test_ctx.mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    test_ctx.mock_properties[prop_idx].serialize = serialize_double;
    test_ctx.mock_properties[prop_idx].deserialize = deserialize_double;
    strcpy(test_ctx.mock_properties[prop_idx].description, "Test double property");
    strcpy(test_ctx.mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Invalid property (missing serialization functions)
    strcpy(test_ctx.mock_properties[prop_idx].name, "InvalidProperty");
    test_ctx.mock_properties[prop_idx].size = sizeof(int32_t);
    test_ctx.mock_properties[prop_idx].module_id = 1;
    test_ctx.mock_properties[prop_idx].extension_id = prop_idx;
    test_ctx.mock_properties[prop_idx].type = PROPERTY_TYPE_INT32;
    test_ctx.mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    test_ctx.mock_properties[prop_idx].serialize = NULL;  // Missing serializer
    test_ctx.mock_properties[prop_idx].deserialize = NULL;  // Missing deserializer
    strcpy(test_ctx.mock_properties[prop_idx].description, "Test invalid property");
    strcpy(test_ctx.mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Invalid property (duplicate name)
    strcpy(test_ctx.mock_properties[prop_idx].name, "TestFloat");  // Duplicate name
    test_ctx.mock_properties[prop_idx].size = sizeof(int64_t);
    test_ctx.mock_properties[prop_idx].module_id = 2;  // Different module
    test_ctx.mock_properties[prop_idx].extension_id = prop_idx;
    test_ctx.mock_properties[prop_idx].type = PROPERTY_TYPE_INT64;
    test_ctx.mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    test_ctx.mock_properties[prop_idx].serialize = serialize_int64;
    test_ctx.mock_properties[prop_idx].deserialize = deserialize_int64;
    strcpy(test_ctx.mock_properties[prop_idx].description, "Test duplicate property");
    strcpy(test_ctx.mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Property with special characters (for HDF5 compatibility testing)
    strcpy(test_ctx.mock_properties[prop_idx].name, "Test/Property+Special!Chars");
    test_ctx.mock_properties[prop_idx].size = sizeof(uint32_t);
    test_ctx.mock_properties[prop_idx].module_id = 1;
    test_ctx.mock_properties[prop_idx].extension_id = prop_idx;
    test_ctx.mock_properties[prop_idx].type = PROPERTY_TYPE_UINT32;
    test_ctx.mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    test_ctx.mock_properties[prop_idx].serialize = serialize_uint32;
    test_ctx.mock_properties[prop_idx].deserialize = deserialize_uint32;
    strcpy(test_ctx.mock_properties[prop_idx].description, "Test property with special characters");
    strcpy(test_ctx.mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Large property (for size limit testing)
    strcpy(test_ctx.mock_properties[prop_idx].name, "LargeProperty");
    test_ctx.mock_properties[prop_idx].size = MAX_SERIALIZED_ARRAY_SIZE + 100;  // Exceeds recommended size
    test_ctx.mock_properties[prop_idx].module_id = 1;
    test_ctx.mock_properties[prop_idx].extension_id = prop_idx;
    test_ctx.mock_properties[prop_idx].type = PROPERTY_TYPE_FLOAT;
    test_ctx.mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    test_ctx.mock_properties[prop_idx].serialize = serialize_float;
    test_ctx.mock_properties[prop_idx].deserialize = deserialize_float;
    strcpy(test_ctx.mock_properties[prop_idx].description, "Test large property");
    strcpy(test_ctx.mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Struct property (for format compatibility testing)
    strcpy(test_ctx.mock_properties[prop_idx].name, "StructProperty");
    test_ctx.mock_properties[prop_idx].size = 128;  // Arbitrary size
    test_ctx.mock_properties[prop_idx].module_id = 1;
    test_ctx.mock_properties[prop_idx].extension_id = prop_idx;
    test_ctx.mock_properties[prop_idx].type = PROPERTY_TYPE_STRUCT;
    test_ctx.mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    test_ctx.mock_properties[prop_idx].serialize = serialize_float;  // Placeholder
    test_ctx.mock_properties[prop_idx].deserialize = deserialize_float;  // Placeholder
    strcpy(test_ctx.mock_properties[prop_idx].description, "Test struct property");
    strcpy(test_ctx.mock_properties[prop_idx].units, "dimensionless");
    prop_idx++;
    
    // Add properties to mock registry
    for (int i = 0; i < prop_idx; i++) {
        test_ctx.mock_registry.extensions[i] = test_ctx.mock_properties[i];
    }
    test_ctx.mock_registry.num_extensions = prop_idx;
    
    // Set global registry pointer to our mock
    global_extension_registry = &test_ctx.mock_registry;
    
    test_ctx.initialized = 1;
    return 0;
}

/**
 * @brief Teardown test context - called after tests
 */
static void teardown_test_context(void) {
    global_extension_registry = NULL;
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: Property type validation
 */
static void test_property_type_validation(void) {
    printf("=== Testing property type validation ===\n");
    
    struct validation_context ctx;
    int status;
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test valid property types
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_FLOAT, "test", "FloatProperty");
    TEST_ASSERT(status == 0, "VALIDATE_PROPERTY_TYPE should succeed for PROPERTY_TYPE_FLOAT");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for valid float type");
    
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_INT32, "test", "IntProperty");
    TEST_ASSERT(status == 0, "VALIDATE_PROPERTY_TYPE should succeed for PROPERTY_TYPE_INT32");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for valid int32 type");
    
    validation_reset(&ctx);
    
    // Test invalid property type
    status = VALIDATE_PROPERTY_TYPE(&ctx, 99, "test", "InvalidProperty");
    TEST_ASSERT(status != 0, "VALIDATE_PROPERTY_TYPE should fail for invalid type 99");
    TEST_ASSERT(validation_has_errors(&ctx), "Validation should have errors for invalid type");
    
    validation_reset(&ctx);
    
    // Test complex property types (struct and array)
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_STRUCT, "test", "StructProperty");
    TEST_ASSERT(status == 0, "VALIDATE_PROPERTY_TYPE should succeed for PROPERTY_TYPE_STRUCT");
    TEST_ASSERT(validation_has_warnings(&ctx), "Validation should have warnings for struct type");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for struct type");
    
    validation_reset(&ctx);
    
    status = VALIDATE_PROPERTY_TYPE(&ctx, PROPERTY_TYPE_ARRAY, "test", "ArrayProperty");
    TEST_ASSERT(status == 0, "VALIDATE_PROPERTY_TYPE should succeed for PROPERTY_TYPE_ARRAY");
    TEST_ASSERT(validation_has_warnings(&ctx), "Validation should have warnings for array type");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for array type");
    
    validation_cleanup(&ctx);
}

/**
 * Test: Property serialization validation
 */
static void test_property_serialization_validation(void) {
    printf("\n=== Testing property serialization validation ===\n");
    
    struct validation_context ctx;
    int status;
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test valid property with serialization functions
    status = VALIDATE_PROPERTY_SERIALIZATION(&ctx, &test_ctx.mock_properties[0], "test");
    TEST_ASSERT(status == 0, "VALIDATE_PROPERTY_SERIALIZATION should succeed for property with serialization functions");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for property with serialization functions");
    
    validation_reset(&ctx);
    
    // Test property without serialization functions
    status = VALIDATE_PROPERTY_SERIALIZATION(&ctx, &test_ctx.mock_properties[2], "test");
    TEST_ASSERT(status != 0, "VALIDATE_PROPERTY_SERIALIZATION should fail for property missing serialization functions");
    TEST_ASSERT(validation_has_errors(&ctx), "Validation should have errors for property missing serialization functions");
    
    validation_cleanup(&ctx);
}

/**
 * Test: Property uniqueness validation
 */
static void test_property_uniqueness_validation(void) {
    printf("\n=== Testing property uniqueness validation ===\n");
    
    struct validation_context ctx;
    int status;
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test unique property name
    status = VALIDATE_PROPERTY_UNIQUENESS(&ctx, &test_ctx.mock_properties[1], "test");
    TEST_ASSERT(status == 0, "VALIDATE_PROPERTY_UNIQUENESS should succeed for unique property name");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for unique property name");
    
    validation_reset(&ctx);
    
    // Test duplicate property name
    status = VALIDATE_PROPERTY_UNIQUENESS(&ctx, &test_ctx.mock_properties[3], "test");
    TEST_ASSERT(status != 0, "VALIDATE_PROPERTY_UNIQUENESS should fail for duplicate property name");
    TEST_ASSERT(validation_has_errors(&ctx), "Validation should have errors for duplicate property name");
    
    validation_cleanup(&ctx);
}

/**
 * Test: Serialization context validation
 */
static void test_serialization_context_validation(void) {
    printf("\n=== Testing serialization context validation ===\n");
    
    struct validation_context ctx;
    struct property_serialization_context ser_ctx;
    int status;
    
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
    TEST_ASSERT(status == 0, "VALIDATE_SERIALIZATION_CONTEXT should succeed for valid context");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for valid serialization context");
    
    validation_reset(&ctx);
    
    // Create invalid serialization context (wrong version)
    ser_ctx.version = 999;  // Invalid version
    
    // Test invalid serialization context
    status = VALIDATE_SERIALIZATION_CONTEXT(&ctx, &ser_ctx, "test");
    TEST_ASSERT(status != 0, "VALIDATE_SERIALIZATION_CONTEXT should fail for invalid version");
    TEST_ASSERT(validation_has_errors(&ctx), "Validation should have errors for invalid serialization context version");
    
    validation_cleanup(&ctx);
}

/**
 * Test: HDF5 property compatibility validation
 */
static void test_hdf5_property_compatibility(void) {
    printf("\n=== Testing HDF5 property compatibility validation ===\n");
    
    struct validation_context ctx;
    int status;
    
    // Initialize validation context
    validation_init(&ctx, VALIDATION_STRICTNESS_NORMAL);
    
    // Test valid property with HDF5 format
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &test_ctx.mock_properties[0], "test");
    TEST_ASSERT(status == 0, "VALIDATE_HDF5_PROPERTY_COMPATIBILITY should succeed for standard property");
    TEST_ASSERT(!validation_has_errors(&ctx), "Validation should have no errors for HDF5-compatible property");
    
    validation_reset(&ctx);
    
    // Test property without serialization functions
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &test_ctx.mock_properties[2], "test");
    TEST_ASSERT(status != 0, "VALIDATE_HDF5_PROPERTY_COMPATIBILITY should fail for property without serialization");
    TEST_ASSERT(validation_has_errors(&ctx), "Validation should have errors for property without serialization functions");
    
    validation_reset(&ctx);
    
    // Test struct property (should warn for HDF5)
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &test_ctx.mock_properties[6], "test");
    TEST_ASSERT(status == 0, "VALIDATE_HDF5_PROPERTY_COMPATIBILITY should succeed for struct property");
    TEST_ASSERT(validation_has_warnings(&ctx), "Validation should have warnings for struct property in HDF5");
    
    validation_reset(&ctx);
    
    // Test large property (should warn for HDF5)
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &test_ctx.mock_properties[5], "test");
    TEST_ASSERT(status == 0, "VALIDATE_HDF5_PROPERTY_COMPATIBILITY should succeed for large property");
    TEST_ASSERT(validation_has_warnings(&ctx), "Validation should have warnings for large property in HDF5");
    
    validation_reset(&ctx);
    
    // Test property with special characters (should warn for HDF5)
    status = VALIDATE_HDF5_PROPERTY_COMPATIBILITY(&ctx, &test_ctx.mock_properties[4], "test");
    TEST_ASSERT(status == 0, "VALIDATE_HDF5_PROPERTY_COMPATIBILITY should succeed for property with special chars");
    TEST_ASSERT(validation_has_warnings(&ctx), "Validation should have warnings for property with special characters in HDF5");
    
    validation_cleanup(&ctx);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(int argc, char *argv[]) {
    (void)argc;  // Suppress unused parameter warning
    (void)argv;  // Suppress unused parameter warning
    
    printf("\n========================================\n");
    printf("Starting tests for test_property_validation\n");
    printf("========================================\n\n");
    
    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_property_type_validation();
    test_property_serialization_validation();
    test_property_uniqueness_validation();
    test_serialization_context_validation();
    test_hdf5_property_compatibility();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_property_validation:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
