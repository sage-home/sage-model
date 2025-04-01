#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// Include core headers
#include "../src/core/core_allvars.h"
#include "../src/core/core_galaxy_extensions.h"

// Include I/O headers
#include "../src/io/io_interface.h"
#include "../src/io/io_endian_utils.h"
#include "../src/io/io_property_serialization.h"

// Dummy extension properties for testing
float test_float = 3.14159f;
double test_double = 2.71828;
int32_t test_int32 = 42;
int64_t test_int64 = 1234567890123LL;
uint32_t test_uint32 = 0xDEADBEEF;
uint64_t test_uint64 = 0xFEEDFACEDEADBEEFULL;
bool test_bool = true;

// Mock galaxy extension registry
struct galaxy_extension_registry mock_registry;

// Mock galaxy property data
galaxy_property_t mock_properties[10];

// Flag to enable endianness testing
int test_endianness = 1;

/**
 * @brief Setup mock extension registry for testing
 */
void setup_mock_registry() {
    // Initialize mock registry
    memset(&mock_registry, 0, sizeof(struct galaxy_extension_registry));
    
    // Initialize mock properties
    int prop_idx = 0;
    
    // Float property
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
    
    // Double property
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
    
    // Int32 property
    strcpy(mock_properties[prop_idx].name, "TestInt32");
    mock_properties[prop_idx].size = sizeof(int32_t);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_INT32;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_int32;
    mock_properties[prop_idx].deserialize = deserialize_int32;
    strcpy(mock_properties[prop_idx].description, "Test int32 property");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Int64 property
    strcpy(mock_properties[prop_idx].name, "TestInt64");
    mock_properties[prop_idx].size = sizeof(int64_t);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_INT64;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_int64;
    mock_properties[prop_idx].deserialize = deserialize_int64;
    strcpy(mock_properties[prop_idx].description, "Test int64 property");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // UInt32 property
    strcpy(mock_properties[prop_idx].name, "TestUInt32");
    mock_properties[prop_idx].size = sizeof(uint32_t);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_UINT32;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_uint32;
    mock_properties[prop_idx].deserialize = deserialize_uint32;
    strcpy(mock_properties[prop_idx].description, "Test uint32 property");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // UInt64 property
    strcpy(mock_properties[prop_idx].name, "TestUInt64");
    mock_properties[prop_idx].size = sizeof(uint64_t);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_UINT64;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_uint64;
    mock_properties[prop_idx].deserialize = deserialize_uint64;
    strcpy(mock_properties[prop_idx].description, "Test uint64 property");
    strcpy(mock_properties[prop_idx].units, "count");
    prop_idx++;
    
    // Bool property
    strcpy(mock_properties[prop_idx].name, "TestBool");
    mock_properties[prop_idx].size = sizeof(bool);
    mock_properties[prop_idx].module_id = 1;
    mock_properties[prop_idx].extension_id = prop_idx;
    mock_properties[prop_idx].type = PROPERTY_TYPE_BOOL;
    mock_properties[prop_idx].flags = PROPERTY_FLAG_SERIALIZE;
    mock_properties[prop_idx].serialize = serialize_bool;
    mock_properties[prop_idx].deserialize = deserialize_bool;
    strcpy(mock_properties[prop_idx].description, "Test boolean property");
    strcpy(mock_properties[prop_idx].units, "flag");
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
 * @brief Create a test galaxy with extension data
 */
struct GALAXY *create_test_galaxy() {
    struct GALAXY *galaxy = calloc(1, sizeof(struct GALAXY));
    if (galaxy == NULL) {
        return NULL;
    }
    
    // Initialize extension data array
    galaxy->extension_data = calloc(mock_registry.num_extensions, sizeof(void *));
    galaxy->num_extensions = mock_registry.num_extensions;
    galaxy->extension_flags = 0;
    
    if (galaxy->extension_data == NULL) {
        free(galaxy);
        return NULL;
    }
    
    // Add extension data
    for (int i = 0; i < mock_registry.num_extensions; i++) {
        galaxy->extension_data[i] = calloc(1, mock_registry.extensions[i].size);
        if (galaxy->extension_data[i] == NULL) {
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                free(galaxy->extension_data[j]);
            }
            free(galaxy->extension_data);
            free(galaxy);
            return NULL;
        }
        
        // Set extension flag
        galaxy->extension_flags |= (1ULL << i);
        
        // Set test values
        switch (mock_registry.extensions[i].type) {
            case PROPERTY_TYPE_FLOAT:
                *(float *)(galaxy->extension_data[i]) = test_float;
                break;
            case PROPERTY_TYPE_DOUBLE:
                *(double *)(galaxy->extension_data[i]) = test_double;
                break;
            case PROPERTY_TYPE_INT32:
                *(int32_t *)(galaxy->extension_data[i]) = test_int32;
                break;
            case PROPERTY_TYPE_INT64:
                *(int64_t *)(galaxy->extension_data[i]) = test_int64;
                break;
            case PROPERTY_TYPE_UINT32:
                *(uint32_t *)(galaxy->extension_data[i]) = test_uint32;
                break;
            case PROPERTY_TYPE_UINT64:
                *(uint64_t *)(galaxy->extension_data[i]) = test_uint64;
                break;
            case PROPERTY_TYPE_BOOL:
                *(bool *)(galaxy->extension_data[i]) = test_bool;
                break;
            default:
                memset(galaxy->extension_data[i], 0, mock_registry.extensions[i].size);
                break;
        }
    }
    
    return galaxy;
}

/**
 * @brief Free a test galaxy
 */
void free_test_galaxy(struct GALAXY *galaxy) {
    if (galaxy == NULL) {
        return;
    }
    
    if (galaxy->extension_data != NULL) {
        for (int i = 0; i < galaxy->num_extensions; i++) {
            free(galaxy->extension_data[i]);
        }
        free(galaxy->extension_data);
    }
    
    free(galaxy);
}

/**
 * @brief Test property serialization context initialization
 */
void test_context_initialization() {
    struct property_serialization_context ctx;
    
    // Initialize context
    int ret = property_serialization_init(&ctx, SERIALIZE_ALL);
    assert(ret == 0);
    
    // Check context fields
    assert(ctx.version == PROPERTY_SERIALIZATION_VERSION);
    assert(ctx.filter_flags == SERIALIZE_ALL);
    assert(ctx.num_properties == 0);
    assert(ctx.properties == NULL);
    assert(ctx.property_id_map == NULL);
    assert(ctx.total_size_per_galaxy == 0);
    
    // Clean up
    property_serialization_cleanup(&ctx);
    
    printf("Test: Context initialization - PASSED\n");
}

/**
 * @brief Test property serialization add properties functionality
 */
void test_add_properties() {
    struct property_serialization_context ctx;
    
    // Initialize context
    int ret = property_serialization_init(&ctx, SERIALIZE_EXPLICIT);
    assert(ret == 0);
    
    // Add properties
    ret = property_serialization_add_properties(&ctx);
    assert(ret == 0);
    
    // Check context fields
    assert(ctx.num_properties == mock_registry.num_extensions);
    assert(ctx.properties != NULL);
    assert(ctx.property_id_map != NULL);
    assert(ctx.total_size_per_galaxy > 0);
    
    // Check property metadata
    for (int i = 0; i < ctx.num_properties; i++) {
        // Find corresponding property in mock registry
        int ext_id = ctx.property_id_map[i];
        assert(ext_id >= 0 && ext_id < mock_registry.num_extensions);
        
        // Check metadata matches
        assert(strcmp(ctx.properties[i].name, mock_registry.extensions[ext_id].name) == 0);
        assert(ctx.properties[i].type == mock_registry.extensions[ext_id].type);
        assert(ctx.properties[i].size == mock_registry.extensions[ext_id].size);
        assert(strcmp(ctx.properties[i].units, mock_registry.extensions[ext_id].units) == 0);
    }
    
    // Clean up
    property_serialization_cleanup(&ctx);
    
    printf("Test: Add properties - PASSED\n");
}

/**
 * @brief Test property serialization and deserialization
 */
void test_serialization_deserialization() {
    struct property_serialization_context ctx;
    struct GALAXY *source_galaxy = create_test_galaxy();
    assert(source_galaxy != NULL);
    
    // Initialize context
    int ret = property_serialization_init(&ctx, SERIALIZE_ALL);
    assert(ret == 0);
    
    // Add properties
    ret = property_serialization_add_properties(&ctx);
    assert(ret == 0);
    
    // Allocate buffer for serialized properties
    size_t buffer_size = property_serialization_data_size(&ctx);
    void *buffer = calloc(1, buffer_size);
    assert(buffer != NULL);
    
    // Serialize properties
    ret = property_serialize_galaxy(&ctx, source_galaxy, buffer);
    assert(ret == 0);
    
    // Create destination galaxy with no extension data
    struct GALAXY *dest_galaxy = calloc(1, sizeof(struct GALAXY));
    assert(dest_galaxy != NULL);
    
    // Deserialize properties
    ret = property_deserialize_galaxy(&ctx, dest_galaxy, buffer);
    assert(ret == 0);
    
    // Check extension data was properly recreated
    assert(dest_galaxy->extension_data != NULL);
    assert(dest_galaxy->num_extensions >= mock_registry.num_extensions);
    
    // Check property values match
    for (int i = 0; i < mock_registry.num_extensions; i++) {
        void *src_data = source_galaxy->extension_data[i];
        void *dest_data = dest_galaxy->extension_data[i];
        assert(dest_data != NULL);
        
        switch (mock_registry.extensions[i].type) {
            case PROPERTY_TYPE_FLOAT:
                assert(*(float *)src_data == *(float *)dest_data);
                break;
            case PROPERTY_TYPE_DOUBLE:
                assert(*(double *)src_data == *(double *)dest_data);
                break;
            case PROPERTY_TYPE_INT32:
                assert(*(int32_t *)src_data == *(int32_t *)dest_data);
                break;
            case PROPERTY_TYPE_INT64:
                assert(*(int64_t *)src_data == *(int64_t *)dest_data);
                break;
            case PROPERTY_TYPE_UINT32:
                assert(*(uint32_t *)src_data == *(uint32_t *)dest_data);
                break;
            case PROPERTY_TYPE_UINT64:
                assert(*(uint64_t *)src_data == *(uint64_t *)dest_data);
                break;
            case PROPERTY_TYPE_BOOL:
                assert(*(bool *)src_data == *(bool *)dest_data);
                break;
            default:
                assert(memcmp(src_data, dest_data, mock_registry.extensions[i].size) == 0);
                break;
        }
    }
    
    // Clean up
    free(buffer);
    free_test_galaxy(source_galaxy);
    free_test_galaxy(dest_galaxy);
    property_serialization_cleanup(&ctx);
    
    printf("Test: Serialization and deserialization - PASSED\n");
}

/**
 * @brief Test header creation and parsing
 */
void test_header_serialization() {
    struct property_serialization_context src_ctx;
    struct property_serialization_context dest_ctx;
    
    // Initialize source context and add properties
    int ret = property_serialization_init(&src_ctx, SERIALIZE_ALL);
    assert(ret == 0);
    ret = property_serialization_add_properties(&src_ctx);
    assert(ret == 0);
    
    // Create header buffer
    size_t buffer_size = 4096;  // Large enough for test
    void *buffer = calloc(1, buffer_size);
    assert(buffer != NULL);
    
    // Create header
    int64_t header_size = property_serialization_create_header(&src_ctx, buffer, buffer_size);
    assert(header_size > 0);
    
    // Initialize destination context
    ret = property_serialization_init(&dest_ctx, SERIALIZE_ALL);
    assert(ret == 0);
    
    // Parse header
    ret = property_serialization_parse_header(&dest_ctx, buffer, buffer_size);
    assert(ret == 0);
    
    // Check parsed context matches source
    assert(dest_ctx.version == src_ctx.version);
    assert(dest_ctx.num_properties == src_ctx.num_properties);
    assert(dest_ctx.total_size_per_galaxy == src_ctx.total_size_per_galaxy);
    
    // Check property metadata
    for (int i = 0; i < dest_ctx.num_properties; i++) {
        assert(strcmp(dest_ctx.properties[i].name, src_ctx.properties[i].name) == 0);
        assert(dest_ctx.properties[i].type == src_ctx.properties[i].type);
        assert(dest_ctx.properties[i].size == src_ctx.properties[i].size);
        assert(strcmp(dest_ctx.properties[i].units, src_ctx.properties[i].units) == 0);
        assert(dest_ctx.properties[i].offset == src_ctx.properties[i].offset);
    }
    
    // Clean up
    free(buffer);
    property_serialization_cleanup(&src_ctx);
    property_serialization_cleanup(&dest_ctx);
    
    printf("Test: Header serialization and parsing - PASSED\n");
}

/**
 * @brief Test endianness handling
 */
void test_endianness_handling() {
    // Skip test if endianness testing is disabled
    if (!test_endianness) {
        printf("Test: Endianness handling - SKIPPED\n");
        return;
    }
    
    struct property_serialization_context ctx;
    
    // Initialize context
    int ret = property_serialization_init(&ctx, SERIALIZE_ALL);
    assert(ret == 0);
    
    // Add properties
    ret = property_serialization_add_properties(&ctx);
    assert(ret == 0);
    
    // Force opposite endianness to test conversion
    ctx.endian_swap = !ctx.endian_swap;
    
    // Test serialization with endianness conversion
    struct GALAXY *galaxy = create_test_galaxy();
    assert(galaxy != NULL);
    
    // Allocate buffer for serialized properties
    size_t buffer_size = property_serialization_data_size(&ctx);
    void *buffer = calloc(1, buffer_size);
    assert(buffer != NULL);
    
    // Serialize properties
    ret = property_serialize_galaxy(&ctx, galaxy, buffer);
    assert(ret == 0);
    
    // Create header
    size_t header_buffer_size = 4096;
    void *header_buffer = calloc(1, header_buffer_size);
    assert(header_buffer != NULL);
    
    int64_t header_size = property_serialization_create_header(&ctx, header_buffer, header_buffer_size);
    assert(header_size > 0);
    
    // Create new context for deserialization
    struct property_serialization_context dest_ctx;
    ret = property_serialization_init(&dest_ctx, SERIALIZE_ALL);
    assert(ret == 0);
    
    // Parse header (this will detect endianness)
    ret = property_serialization_parse_header(&dest_ctx, header_buffer, header_buffer_size);
    assert(ret == 0);
    
    // Check endianness flag was correctly set
    assert(dest_ctx.endian_swap == ctx.endian_swap);
    
    // Create destination galaxy with no extension data
    struct GALAXY *dest_galaxy = calloc(1, sizeof(struct GALAXY));
    assert(dest_galaxy != NULL);
    
    // Deserialize properties
    ret = property_deserialize_galaxy(&dest_ctx, dest_galaxy, buffer);
    assert(ret == 0);
    
    // Check values were correctly converted
    for (int i = 0; i < mock_registry.num_extensions; i++) {
        void *src_data = galaxy->extension_data[i];
        void *dest_data = dest_galaxy->extension_data[i];
        assert(dest_data != NULL);
        
        switch (mock_registry.extensions[i].type) {
            case PROPERTY_TYPE_FLOAT:
                assert(*(float *)src_data == *(float *)dest_data);
                break;
            case PROPERTY_TYPE_DOUBLE:
                assert(*(double *)src_data == *(double *)dest_data);
                break;
            case PROPERTY_TYPE_INT32:
                assert(*(int32_t *)src_data == *(int32_t *)dest_data);
                break;
            case PROPERTY_TYPE_INT64:
                assert(*(int64_t *)src_data == *(int64_t *)dest_data);
                break;
            case PROPERTY_TYPE_UINT32:
                assert(*(uint32_t *)src_data == *(uint32_t *)dest_data);
                break;
            case PROPERTY_TYPE_UINT64:
                assert(*(uint64_t *)src_data == *(uint64_t *)dest_data);
                break;
            case PROPERTY_TYPE_BOOL:
                assert(*(bool *)src_data == *(bool *)dest_data);
                break;
            default:
                break;
        }
    }
    
    // Clean up
    free(buffer);
    free(header_buffer);
    free_test_galaxy(galaxy);
    free_test_galaxy(dest_galaxy);
    property_serialization_cleanup(&ctx);
    property_serialization_cleanup(&dest_ctx);
    
    printf("Test: Endianness handling - PASSED\n");
}

/**
 * @brief Main function
 */
int main(int argc, char **argv) {
    // Check if endianness testing should be skipped
    if (argc > 1 && strcmp(argv[1], "--no-endianness-test") == 0) {
        test_endianness = 0;
    }
    
    printf("--- Property Serialization Tests ---\n");
    
    // Set up mock registry
    setup_mock_registry();
    
    // Run tests
    test_context_initialization();
    test_add_properties();
    test_serialization_deserialization();
    test_header_serialization();
    test_endianness_handling();
    
    printf("All tests PASSED\n");
    
    return 0;
}