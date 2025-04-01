#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../src/io/io_endian_utils.h"

/**
 * Test for endianness detection
 */
void test_endianness_detection() {
    printf("Testing endianness detection...\n");
    
    enum endian_type system_endian = get_system_endianness();
    printf("System endianness: %s\n", 
           system_endian == ENDIAN_LITTLE ? "Little-endian" : 
           system_endian == ENDIAN_BIG ? "Big-endian" : "Unknown");
    
    // The endianness detection should be consistent
    if (system_endian == ENDIAN_LITTLE) {
        assert(is_little_endian() == true);
        assert(is_big_endian() == false);
    } else if (system_endian == ENDIAN_BIG) {
        assert(is_little_endian() == false);
        assert(is_big_endian() == true);
    }
    
    printf("Endianness detection tests passed.\n");
}

/**
 * Test for byte swapping functions
 */
void test_byte_swapping() {
    printf("Testing byte swapping functions...\n");
    
    // Test 16-bit swapping
    uint16_t val16 = 0x1234;
    uint16_t swapped16 = swap_bytes_uint16(val16);
    assert(swapped16 == 0x3412);
    assert(swap_bytes_uint16(swapped16) == val16);
    
    // Test 32-bit swapping
    uint32_t val32 = 0x12345678;
    uint32_t swapped32 = swap_bytes_uint32(val32);
    assert(swapped32 == 0x78563412);
    assert(swap_bytes_uint32(swapped32) == val32);
    
    // Test 64-bit swapping
    uint64_t val64 = 0x123456789ABCDEF0ULL;
    uint64_t swapped64 = swap_bytes_uint64(val64);
    assert(swapped64 == 0xF0DEBC9A78563412ULL);
    assert(swap_bytes_uint64(swapped64) == val64);
    
    // Test float swapping
    float val_float = 1.234f;
    float swapped_float = swap_bytes_float(val_float);
    assert(swap_bytes_float(swapped_float) == val_float);
    
    // Test double swapping
    double val_double = 1.234567890123;
    double swapped_double = swap_bytes_double(val_double);
    assert(swap_bytes_double(swapped_double) == val_double);
    
    printf("Byte swapping tests passed.\n");
}

/**
 * Test for host/network conversion functions
 */
void test_host_network_conversion() {
    printf("Testing host/network conversion functions...\n");
    
    // Test 16-bit conversion
    uint16_t val16 = 0x1234;
    uint16_t net16 = host_to_network_uint16(val16);
    assert(network_to_host_uint16(net16) == val16);
    
    // Test 32-bit conversion
    uint32_t val32 = 0x12345678;
    uint32_t net32 = host_to_network_uint32(val32);
    assert(network_to_host_uint32(net32) == val32);
    
    // Test 64-bit conversion
    uint64_t val64 = 0x123456789ABCDEF0ULL;
    uint64_t net64 = host_to_network_uint64(val64);
    assert(network_to_host_uint64(net64) == val64);
    
    // Test float conversion
    float val_float = 1.234f;
    float net_float = host_to_network_float(val_float);
    assert(network_to_host_float(net_float) == val_float);
    
    // Test double conversion
    double val_double = 1.234567890123;
    double net_double = host_to_network_double(val_double);
    assert(network_to_host_double(net_double) == val_double);
    
    printf("Host/network conversion tests passed.\n");
}

/**
 * Test for array conversion functions
 */
void test_array_conversion() {
    printf("Testing array conversion functions...\n");
    
    // Test 16-bit array
    uint16_t array16[5] = {0x1234, 0x5678, 0x9ABC, 0xDEF0, 0x1357};
    uint16_t original16[5];
    memcpy(original16, array16, sizeof(array16));
    
    swap_bytes_uint16_array(array16, 5);
    // Verify each element is swapped
    for (int i = 0; i < 5; i++) {
        assert(array16[i] == swap_bytes_uint16(original16[i]));
    }
    
    // Swap back
    swap_bytes_uint16_array(array16, 5);
    for (int i = 0; i < 5; i++) {
        assert(array16[i] == original16[i]);
    }
    
    // Test 32-bit array
    uint32_t array32[5] = {0x12345678, 0x9ABCDEF0, 0x13579BDF, 0x2468ACE0, 0xFEDCBA98};
    uint32_t original32[5];
    memcpy(original32, array32, sizeof(array32));
    
    swap_bytes_uint32_array(array32, 5);
    // Verify each element is swapped
    for (int i = 0; i < 5; i++) {
        assert(array32[i] == swap_bytes_uint32(original32[i]));
    }
    
    // Swap back
    swap_bytes_uint32_array(array32, 5);
    for (int i = 0; i < 5; i++) {
        assert(array32[i] == original32[i]);
    }
    
    // Test 64-bit array
    uint64_t array64[3] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL, 0x0123456789ABCDEFULL};
    uint64_t original64[3];
    memcpy(original64, array64, sizeof(array64));
    
    swap_bytes_uint64_array(array64, 3);
    // Verify each element is swapped
    for (int i = 0; i < 3; i++) {
        assert(array64[i] == swap_bytes_uint64(original64[i]));
    }
    
    // Swap back
    swap_bytes_uint64_array(array64, 3);
    for (int i = 0; i < 3; i++) {
        assert(array64[i] == original64[i]);
    }
    
    // Test float array
    float array_float[3] = {1.234f, 5.678f, 9.012f};
    float original_float[3];
    memcpy(original_float, array_float, sizeof(array_float));
    
    swap_bytes_float_array(array_float, 3);
    // Can't directly compare floats after byte swapping, so swap back
    swap_bytes_float_array(array_float, 3);
    for (int i = 0; i < 3; i++) {
        assert(array_float[i] == original_float[i]);
    }
    
    // Test double array
    double array_double[3] = {1.234567890123, 5.678901234567, 9.012345678901};
    double original_double[3];
    memcpy(original_double, array_double, sizeof(array_double));
    
    swap_bytes_double_array(array_double, 3);
    // Can't directly compare doubles after byte swapping, so swap back
    swap_bytes_double_array(array_double, 3);
    for (int i = 0; i < 3; i++) {
        assert(array_double[i] == original_double[i]);
    }
    
    printf("Array conversion tests passed.\n");
}

/**
 * Test generic endianness swapping function
 */
void test_generic_endianness_swapping() {
    printf("Testing generic endianness swapping...\n");
    
    // Test 16-bit values
    uint16_t array16[3] = {0x1234, 0x5678, 0x9ABC};
    uint16_t expected16[3] = {0x3412, 0x7856, 0xBC9A};
    uint16_t original16[3];
    memcpy(original16, array16, sizeof(array16));
    
    int result = swap_endianness(array16, 2, 3);
    assert(result == 0);
    for (int i = 0; i < 3; i++) {
        assert(array16[i] == expected16[i]);
    }
    
    // Test 32-bit values
    uint32_t array32[3] = {0x12345678, 0x9ABCDEF0, 0x13579BDF};
    uint32_t expected32[3] = {0x78563412, 0xF0DEBC9A, 0xDF9B5713};
    uint32_t original32[3];
    memcpy(original32, array32, sizeof(array32));
    
    result = swap_endianness(array32, 4, 3);
    assert(result == 0);
    for (int i = 0; i < 3; i++) {
        assert(array32[i] == expected32[i]);
    }
    
    // Test 64-bit values
    uint64_t array64[2] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL};
    uint64_t expected64[2] = {0xF0DEBC9A78563412ULL, 0x1032547698BADCFEULL};
    uint64_t original64[2];
    memcpy(original64, array64, sizeof(array64));
    
    result = swap_endianness(array64, 8, 2);
    assert(result == 0);
    for (int i = 0; i < 2; i++) {
        assert(array64[i] == expected64[i]);
    }
    
    // Test invalid size
    uint32_t invalid[3] = {0, 0, 0};
    result = swap_endianness(invalid, 3, 3);
    assert(result == -1);
    
    printf("Generic endianness swapping tests passed.\n");
}

int main() {
    printf("Running endianness utilities tests...\n");
    
    test_endianness_detection();
    test_byte_swapping();
    test_host_network_conversion();
    test_array_conversion();
    test_generic_endianness_swapping();
    
    printf("All endianness utilities tests passed!\n");
    return 0;
}
