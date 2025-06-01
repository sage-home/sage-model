/**
 * Test suite for Endianness Utilities
 * 
 * Tests cover:
 * - System endianness detection
 * - Byte swapping for all data types
 * - Host/network conversions
 * - Array processing functions
 * - Error handling and edge cases
 * - Cross-platform compatibility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/io/io_endian_utils.h"

// Test counter for reporting
static int tests_run = 0;
static int tests_passed = 0;

// Helper macro for test assertions
#define TEST_ASSERT(condition, message, ...) do { \
    tests_run++; \
    if (!(condition)) { \
        printf("FAIL: " message "\n", ##__VA_ARGS__); \
        printf("  at %s:%d\n", __FILE__, __LINE__); \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Test fixtures
static struct test_context {
    int initialized;
    // Test data arrays for edge case testing
    uint16_t *test_array_16;
    uint32_t *test_array_32;
    uint64_t *test_array_64;
    size_t array_size;
} test_ctx;

// Setup function - called before tests
static int setup_test_context(void) {
    memset(&test_ctx, 0, sizeof(test_ctx));
    
    // Initialize test arrays for edge case testing
    test_ctx.array_size = 1000;  // Large enough for stress testing
    test_ctx.test_array_16 = calloc(test_ctx.array_size, sizeof(uint16_t));
    test_ctx.test_array_32 = calloc(test_ctx.array_size, sizeof(uint32_t));
    test_ctx.test_array_64 = calloc(test_ctx.array_size, sizeof(uint64_t));    
    if (!test_ctx.test_array_16 || !test_ctx.test_array_32 || !test_ctx.test_array_64) {
        printf("ERROR: Failed to allocate test arrays\n");
        return -1;
    }
    
    // Initialize with test patterns
    for (size_t i = 0; i < test_ctx.array_size; i++) {
        test_ctx.test_array_16[i] = (uint16_t)(0x1234 + i);
        test_ctx.test_array_32[i] = (uint32_t)(0x12345678 + i);
        test_ctx.test_array_64[i] = (uint64_t)(0x123456789ABCDEF0ULL + i);
    }
    
    test_ctx.initialized = 1;
    return 0;
}

// Teardown function - called after tests
static void teardown_test_context(void) {
    if (test_ctx.test_array_16) {
        free(test_ctx.test_array_16);
        test_ctx.test_array_16 = NULL;
    }
    if (test_ctx.test_array_32) {
        free(test_ctx.test_array_32);
        test_ctx.test_array_32 = NULL;
    }
    if (test_ctx.test_array_64) {
        free(test_ctx.test_array_64);
        test_ctx.test_array_64 = NULL;
    }
    test_ctx.initialized = 0;
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * Test: System endianness detection
 * 
 * Validates that the system correctly identifies its byte order and that
 * detection functions are mutually consistent. This is fundamental for
 * cross-platform binary file I/O operations.
 */
static void test_endianness_detection(void) {
    printf("=== Testing system endianness detection ===\n");
    
    enum endian_type system_endian = get_system_endianness();
    printf("Detected system endianness: %s\n", 
           system_endian == ENDIAN_LITTLE ? "Little-endian" : 
           system_endian == ENDIAN_BIG ? "Big-endian" : "Unknown");
    
    // Test consistency between detection functions
    if (system_endian == ENDIAN_LITTLE) {
        TEST_ASSERT(is_little_endian() == true, 
                   "is_little_endian() should return true for little-endian system");
        TEST_ASSERT(is_big_endian() == false, 
                   "is_big_endian() should return false for little-endian system");
    } else if (system_endian == ENDIAN_BIG) {
        TEST_ASSERT(is_little_endian() == false, 
                   "is_little_endian() should return false for big-endian system");
        TEST_ASSERT(is_big_endian() == true, 
                   "is_big_endian() should return true for big-endian system");
    } else {
        TEST_ASSERT(false, "System endianness detection returned ENDIAN_UNKNOWN");
    }
    
    // Verify mutual exclusivity
    TEST_ASSERT(!(is_little_endian() && is_big_endian()), 
               "System cannot be both little-endian and big-endian");
    
    // Verify at least one is true (unless unknown)
    TEST_ASSERT(is_little_endian() || is_big_endian(), 
               "System must be either little-endian or big-endian");
}

/**
 * Test: Individual byte swapping functions
 * 
 * Tests byte swapping for all supported data types using known values
 * and verifies that swapping is reversible (f(f(x)) = x).
 */
static void test_byte_swapping(void) {
    printf("\n=== Testing individual byte swapping functions ===\n");
    
    // Test 16-bit swapping with known values
    uint16_t val16 = 0x1234;
    uint16_t swapped16 = swap_bytes_uint16(val16);
    TEST_ASSERT(swapped16 == 0x3412, 
               "16-bit byte swap failed: expected 0x3412, got 0x%04X", swapped16);
    TEST_ASSERT(swap_bytes_uint16(swapped16) == val16, 
               "16-bit byte swap not reversible: 0x%04X -> 0x%04X -> 0x%04X", 
               val16, swapped16, swap_bytes_uint16(swapped16));
    
    // Test 32-bit swapping with known values
    uint32_t val32 = 0x12345678;
    uint32_t swapped32 = swap_bytes_uint32(val32);
    TEST_ASSERT(swapped32 == 0x78563412, 
               "32-bit byte swap failed: expected 0x78563412, got 0x%08X", swapped32);
    TEST_ASSERT(swap_bytes_uint32(swapped32) == val32, 
               "32-bit byte swap not reversible");
    
    // Test 64-bit swapping with known values
    uint64_t val64 = 0x123456789ABCDEF0ULL;
    uint64_t swapped64 = swap_bytes_uint64(val64);
    TEST_ASSERT(swapped64 == 0xF0DEBC9A78563412ULL, 
               "64-bit byte swap failed: expected 0xF0DEBC9A78563412, got 0x%016llX", 
               (unsigned long long)swapped64);
    TEST_ASSERT(swap_bytes_uint64(swapped64) == val64, 
               "64-bit byte swap not reversible");
    
    // Test float swapping (reversibility only, as bit patterns are complex)
    float val_float = 1.234f;
    float swapped_float = swap_bytes_float(val_float);
    float double_swapped_float = swap_bytes_float(swapped_float);
    TEST_ASSERT(double_swapped_float == val_float, 
               "Float byte swap not reversible: %f -> %f -> %f", 
               val_float, swapped_float, double_swapped_float);
    
    // Test double swapping (reversibility only)
    double val_double = 1.234567890123;
    double swapped_double = swap_bytes_double(val_double);
    double double_swapped_double = swap_bytes_double(swapped_double);
    TEST_ASSERT(double_swapped_double == val_double, 
               "Double byte swap not reversible: %f -> %f -> %f", 
               val_double, swapped_double, double_swapped_double);
    
    // Test edge cases - zero values
    TEST_ASSERT(swap_bytes_uint16(0) == 0, "Swapping zero should yield zero");
    TEST_ASSERT(swap_bytes_uint32(0) == 0, "Swapping zero should yield zero");
    TEST_ASSERT(swap_bytes_uint64(0) == 0, "Swapping zero should yield zero");
    
    // Test edge cases - maximum values
    TEST_ASSERT(swap_bytes_uint16(0xFFFF) == 0xFFFF, "Swapping 0xFFFF should yield 0xFFFF");
    TEST_ASSERT(swap_bytes_uint32(0xFFFFFFFF) == 0xFFFFFFFF, "Swapping 0xFFFFFFFF should yield 0xFFFFFFFF");
    TEST_ASSERT(swap_bytes_uint64(0xFFFFFFFFFFFFFFFFULL) == 0xFFFFFFFFFFFFFFFFULL, 
               "Swapping all 1s should yield all 1s");
}

/**
 * Test: Host/network conversion functions
 * 
 * Tests network byte order conversions for all data types. Network byte order
 * is always big-endian, so conversions should be consistent regardless of
 * host architecture.
 */
static void test_host_network_conversion(void) {
    printf("\n=== Testing host/network conversion functions ===\n");
    
    // Test 16-bit conversion round-trip
    uint16_t val16 = 0x1234;
    uint16_t net16 = host_to_network_uint16(val16);
    uint16_t host16 = network_to_host_uint16(net16);
    TEST_ASSERT(host16 == val16, 
               "16-bit host/network conversion failed: %04X -> %04X -> %04X", 
               val16, net16, host16);
    
    // Test 32-bit conversion round-trip
    uint32_t val32 = 0x12345678;
    uint32_t net32 = host_to_network_uint32(val32);
    uint32_t host32 = network_to_host_uint32(net32);
    TEST_ASSERT(host32 == val32, 
               "32-bit host/network conversion failed: %08X -> %08X -> %08X", 
               val32, net32, host32);
    
    // Test 64-bit conversion round-trip
    uint64_t val64 = 0x123456789ABCDEF0ULL;
    uint64_t net64 = host_to_network_uint64(val64);
    uint64_t host64 = network_to_host_uint64(net64);
    TEST_ASSERT(host64 == val64, 
               "64-bit host/network conversion failed");
    
    // Test float conversion round-trip
    float val_float = 3.14159f;
    float net_float = host_to_network_float(val_float);
    float host_float = network_to_host_float(net_float);
    TEST_ASSERT(host_float == val_float, 
               "Float host/network conversion failed: %f -> %f -> %f", 
               val_float, net_float, host_float);
    
    // Test double conversion round-trip
    double val_double = 3.141592653589793;
    double net_double = host_to_network_double(val_double);
    double host_double = network_to_host_double(net_double);
    TEST_ASSERT(host_double == val_double, 
               "Double host/network conversion failed: %f -> %f -> %f", 
               val_double, net_double, host_double);
    
    // On little-endian systems, network conversion should swap bytes
    // On big-endian systems, network conversion should be no-op
    if (is_little_endian()) {
        TEST_ASSERT(net16 != val16 || val16 == swap_bytes_uint16(val16), 
                   "Little-endian system should swap bytes for network order (unless symmetric value)");
    } else if (is_big_endian()) {
        TEST_ASSERT(net16 == val16, 
                   "Big-endian system should not change values for network order");
    }
}

/**
 * Test: Array conversion functions
 * 
 * Tests bulk array processing for all data types, including edge cases
 * like empty arrays and large datasets.
 */
static void test_array_conversion(void) {
    printf("\n=== Testing array conversion functions ===\n");
    
    // Test small arrays with known values
    uint16_t array16[5] = {0x1234, 0x5678, 0x9ABC, 0xDEF0, 0x1357};
    uint16_t original16[5];
    memcpy(original16, array16, sizeof(array16));
    
    swap_bytes_uint16_array(array16, 5);
    // Verify each element is swapped
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(array16[i] == swap_bytes_uint16(original16[i]), 
                   "16-bit array element %d not correctly swapped: expected 0x%04X, got 0x%04X", 
                   i, swap_bytes_uint16(original16[i]), array16[i]);
    }
    
    // Swap back and verify restoration
    swap_bytes_uint16_array(array16, 5);
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(array16[i] == original16[i], 
                   "16-bit array element %d not restored after double swap", i);
    }
    
    // Test 32-bit array
    uint32_t array32[5] = {0x12345678, 0x9ABCDEF0, 0x13579BDF, 0x2468ACE0, 0xFEDCBA98};
    uint32_t original32[5];
    memcpy(original32, array32, sizeof(array32));
    
    swap_bytes_uint32_array(array32, 5);
    swap_bytes_uint32_array(array32, 5);  // Double swap to restore
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT(array32[i] == original32[i], 
                   "32-bit array element %d not restored after double swap", i);
    }
    
    // Test 64-bit array
    uint64_t array64[3] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL, 0x0123456789ABCDEFULL};
    uint64_t original64[3];
    memcpy(original64, array64, sizeof(array64));
    
    swap_bytes_uint64_array(array64, 3);
    swap_bytes_uint64_array(array64, 3);  // Double swap to restore
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(array64[i] == original64[i], 
                   "64-bit array element %d not restored after double swap", i);
    }
    
    // Test float array
    float array_float[3] = {1.234f, 5.678f, 9.012f};
    float original_float[3];
    memcpy(original_float, array_float, sizeof(array_float));
    
    swap_bytes_float_array(array_float, 3);
    swap_bytes_float_array(array_float, 3);  // Double swap to restore
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(array_float[i] == original_float[i], 
                   "Float array element %d not restored after double swap", i);
    }
    
    // Test double array
    double array_double[3] = {1.234567890123, 5.678901234567, 9.012345678901};
    double original_double[3];
    memcpy(original_double, array_double, sizeof(array_double));
    
    swap_bytes_double_array(array_double, 3);
    swap_bytes_double_array(array_double, 3);  // Double swap to restore
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(array_double[i] == original_double[i], 
                   "Double array element %d not restored after double swap", i);
    }
}

/**
 * Test: Generic endianness swapping function
 * 
 * Tests the generic swap_endianness() function with valid and invalid
 * parameters, including error handling for unsupported sizes.
 */
static void test_generic_endianness_swapping(void) {
    printf("\n=== Testing generic endianness swapping ===\n");
    
    // Test 16-bit values
    uint16_t array16[3] = {0x1234, 0x5678, 0x9ABC};
    uint16_t expected16[3] = {0x3412, 0x7856, 0xBC9A};
    
    int result = swap_endianness(array16, 2, 3);
    TEST_ASSERT(result == 0, "swap_endianness should return 0 for valid 16-bit operation");
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(array16[i] == expected16[i], 
                   "Generic 16-bit swap failed for element %d: expected 0x%04X, got 0x%04X", 
                   i, expected16[i], array16[i]);
    }
    
    // Test 32-bit values
    uint32_t array32[3] = {0x12345678, 0x9ABCDEF0, 0x13579BDF};
    uint32_t expected32[3] = {0x78563412, 0xF0DEBC9A, 0xDF9B5713};
    
    result = swap_endianness(array32, 4, 3);
    TEST_ASSERT(result == 0, "swap_endianness should return 0 for valid 32-bit operation");
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT(array32[i] == expected32[i], 
                   "Generic 32-bit swap failed for element %d: expected 0x%08X, got 0x%08X", 
                   i, expected32[i], array32[i]);
    }
    
    // Test 64-bit values
    uint64_t array64[2] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL};
    uint64_t expected64[2] = {0xF0DEBC9A78563412ULL, 0x1032547698BADCFEULL};
    
    result = swap_endianness(array64, 8, 2);
    TEST_ASSERT(result == 0, "swap_endianness should return 0 for valid 64-bit operation");
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT(array64[i] == expected64[i], 
                   "Generic 64-bit swap failed for element %d", i);
    }
    
    // Test invalid sizes
    uint32_t invalid[3] = {0, 0, 0};
    
    result = swap_endianness(invalid, 3, 3);  // Invalid size
    TEST_ASSERT(result == -1, "swap_endianness should return -1 for invalid size 3");
    
    result = swap_endianness(invalid, 5, 3);  // Invalid size
    TEST_ASSERT(result == -1, "swap_endianness should return -1 for invalid size 5");
    
    result = swap_endianness(invalid, 1, 3);  // Invalid size
    TEST_ASSERT(result == -1, "swap_endianness should return -1 for invalid size 1");
    
    // Test zero count (should succeed but do nothing)
    result = swap_endianness(invalid, 4, 0);
    TEST_ASSERT(result == 0, "swap_endianness should return 0 for zero count");
}

/**
 * Test: Edge cases and error handling
 * 
 * Tests NULL pointer handling, zero-count arrays, and other boundary
 * conditions to ensure robust error handling.
 */
static void test_edge_cases(void) {
    printf("\n=== Testing edge cases and error handling ===\n");
    
    // Test zero-count arrays (should be safe no-ops)
    uint32_t test_array[] = {0x12345678, 0x9ABCDEF0};
    uint32_t original[] = {0x12345678, 0x9ABCDEF0};
    
    swap_bytes_uint16_array((uint16_t*)test_array, 0);
    swap_bytes_uint32_array(test_array, 0);
    swap_bytes_uint64_array((uint64_t*)test_array, 0);
    
    TEST_ASSERT(memcmp(test_array, original, sizeof(original)) == 0, 
               "Zero-count array operations should not modify data");
    
    // Test NULL pointer handling (functions should not crash)
    // Note: These may cause undefined behavior, but shouldn't crash with proper implementation
    printf("Testing NULL pointer handling (should not crash)...\n");
    
    // These operations should be safe no-ops or handled gracefully
    swap_bytes_uint16_array(NULL, 0);
    swap_bytes_uint32_array(NULL, 0);
    swap_bytes_uint64_array(NULL, 0);
    swap_bytes_float_array(NULL, 0);
    swap_bytes_double_array(NULL, 0);
    
    TEST_ASSERT(true, "NULL pointer handling with zero count completed without crash");
    
    // Test generic swapping with NULL (should return error)
    int result = swap_endianness(NULL, 4, 0);
    TEST_ASSERT(result == 0, "swap_endianness(NULL, 4, 0) should succeed (zero count)");
}

/**
 * Test: Large array performance and correctness
 * 
 * Tests processing of large arrays to validate performance and ensure
 * no data corruption occurs with bulk operations.
 */
static void test_large_array_processing(void) {
    printf("\n=== Testing large array processing ===\n");
    
    if (!test_ctx.initialized) {
        TEST_ASSERT(false, "Test context not initialized for large array testing");
        return;
    }
    
    // Test large 16-bit array
    uint16_t *backup16 = malloc(test_ctx.array_size * sizeof(uint16_t));
    TEST_ASSERT(backup16 != NULL, "Failed to allocate backup array for 16-bit test");
    
    if (backup16) {
        memcpy(backup16, test_ctx.test_array_16, test_ctx.array_size * sizeof(uint16_t));
        
        swap_bytes_uint16_array(test_ctx.test_array_16, test_ctx.array_size);
        swap_bytes_uint16_array(test_ctx.test_array_16, test_ctx.array_size);  // Double swap
        
        int matches = memcmp(test_ctx.test_array_16, backup16, test_ctx.array_size * sizeof(uint16_t));
        TEST_ASSERT(matches == 0, "Large 16-bit array not restored after double swap");
        
        free(backup16);
    }
    
    // Test large 32-bit array
    uint32_t *backup32 = malloc(test_ctx.array_size * sizeof(uint32_t));
    TEST_ASSERT(backup32 != NULL, "Failed to allocate backup array for 32-bit test");
    
    if (backup32) {
        memcpy(backup32, test_ctx.test_array_32, test_ctx.array_size * sizeof(uint32_t));
        
        swap_bytes_uint32_array(test_ctx.test_array_32, test_ctx.array_size);
        swap_bytes_uint32_array(test_ctx.test_array_32, test_ctx.array_size);  // Double swap
        
        int matches = memcmp(test_ctx.test_array_32, backup32, test_ctx.array_size * sizeof(uint32_t));
        TEST_ASSERT(matches == 0, "Large 32-bit array not restored after double swap");
        
        free(backup32);
    }
    
    printf("Successfully processed %zu element arrays\n", test_ctx.array_size);
}

//=============================================================================
// Test Runner
//=============================================================================

int main(void) {
    printf("\n========================================\n");
    printf("Starting tests for test_endian_utils\n");
    printf("========================================\n\n");
    
    printf("This test verifies that endianness utilities provide:\n");
    printf("  1. Accurate system endianness detection\n");
    printf("  2. Reliable byte swapping operations for all data types\n");
    printf("  3. Correct host/network byte order conversions\n");
    printf("  4. Robust array processing functions\n");
    printf("  5. Proper error handling for edge cases\n");
    printf("  6. Cross-platform compatibility for binary I/O\n\n");

    // Setup
    if (setup_test_context() != 0) {
        printf("ERROR: Failed to set up test context\n");
        return 1;
    }
    
    // Run tests
    test_endianness_detection();
    test_byte_swapping();
    test_host_network_conversion();
    test_array_conversion();
    test_generic_endianness_swapping();
    test_edge_cases();
    test_large_array_processing();
    
    // Teardown
    teardown_test_context();
    
    // Report results
    printf("\n========================================\n");
    printf("Test results for test_endian_utils:\n");
    printf("  Total tests: %d\n", tests_run);
    printf("  Passed: %d\n", tests_passed);
    printf("  Failed: %d\n", tests_run - tests_passed);
    printf("========================================\n\n");
    
    return (tests_run == tests_passed) ? 0 : 1;
}
