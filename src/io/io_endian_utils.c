#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io_endian_utils.h"

/**
 * @brief Union for converting between float and uint32_t
 */
typedef union {
    float f;
    uint32_t i;
} float_uint32_t;

/**
 * @brief Union for converting between double and uint64_t
 */
typedef union {
    double d;
    uint64_t i;
} double_uint64_t;

/**
 * @brief Check if the system is little-endian
 *
 * @return true if the system is little-endian, false otherwise
 */
bool is_little_endian(void) {
    uint16_t value = 0x1234;
    uint8_t *ptr = (uint8_t*)&value;
    return (ptr[0] == 0x34);
}

/**
 * @brief Check if the system is big-endian
 *
 * @return true if the system is big-endian, false otherwise
 */
bool is_big_endian(void) {
    uint16_t value = 0x1234;
    uint8_t *ptr = (uint8_t*)&value;
    return (ptr[0] == 0x12);
}

/**
 * @brief Get the system's endianness
 *
 * @return ENDIAN_LITTLE, ENDIAN_BIG, or ENDIAN_UNKNOWN
 */
enum endian_type get_system_endianness(void) {
    if (is_little_endian()) {
        return ENDIAN_LITTLE;
    } else if (is_big_endian()) {
        return ENDIAN_BIG;
    } else {
        return ENDIAN_UNKNOWN;
    }
}

/**
 * @brief Swap bytes in a 16-bit value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
uint16_t swap_bytes_uint16(uint16_t value) {
    return ((value & 0xFF00) >> 8) | 
           ((value & 0x00FF) << 8);
}

/**
 * @brief Swap bytes in a 32-bit value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
uint32_t swap_bytes_uint32(uint32_t value) {
    return ((value & 0xFF000000) >> 24) | 
           ((value & 0x00FF0000) >> 8) | 
           ((value & 0x0000FF00) << 8) | 
           ((value & 0x000000FF) << 24);
}

/**
 * @brief Swap bytes in a 64-bit value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
uint64_t swap_bytes_uint64(uint64_t value) {
    return ((value & 0xFF00000000000000ULL) >> 56) | 
           ((value & 0x00FF000000000000ULL) >> 40) | 
           ((value & 0x0000FF0000000000ULL) >> 24) | 
           ((value & 0x000000FF00000000ULL) >> 8) | 
           ((value & 0x00000000FF000000ULL) << 8) | 
           ((value & 0x0000000000FF0000ULL) << 24) | 
           ((value & 0x000000000000FF00ULL) << 40) | 
           ((value & 0x00000000000000FFULL) << 56);
}

/**
 * @brief Swap bytes in a float value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
float swap_bytes_float(float value) {
    float_uint32_t in, out;
    in.f = value;
    out.i = swap_bytes_uint32(in.i);
    return out.f;
}

/**
 * @brief Swap bytes in a double value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
double swap_bytes_double(double value) {
    double_uint64_t in, out;
    in.d = value;
    out.i = swap_bytes_uint64(in.i);
    return out.d;
}

/**
 * @brief Convert a 16-bit value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
uint16_t host_to_network_uint16(uint16_t value) {
    if (is_little_endian()) {
        return swap_bytes_uint16(value);
    }
    return value;
}

/**
 * @brief Convert a 32-bit value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
uint32_t host_to_network_uint32(uint32_t value) {
    if (is_little_endian()) {
        return swap_bytes_uint32(value);
    }
    return value;
}

/**
 * @brief Convert a 64-bit value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
uint64_t host_to_network_uint64(uint64_t value) {
    if (is_little_endian()) {
        return swap_bytes_uint64(value);
    }
    return value;
}

/**
 * @brief Convert a 16-bit value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
uint16_t network_to_host_uint16(uint16_t value) {
    if (is_little_endian()) {
        return swap_bytes_uint16(value);
    }
    return value;
}

/**
 * @brief Convert a 32-bit value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
uint32_t network_to_host_uint32(uint32_t value) {
    if (is_little_endian()) {
        return swap_bytes_uint32(value);
    }
    return value;
}

/**
 * @brief Convert a 64-bit value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
uint64_t network_to_host_uint64(uint64_t value) {
    if (is_little_endian()) {
        return swap_bytes_uint64(value);
    }
    return value;
}

/**
 * @brief Convert a float value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
float host_to_network_float(float value) {
    if (is_little_endian()) {
        return swap_bytes_float(value);
    }
    return value;
}

/**
 * @brief Convert a double value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
double host_to_network_double(double value) {
    if (is_little_endian()) {
        return swap_bytes_double(value);
    }
    return value;
}

/**
 * @brief Convert a float value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
float network_to_host_float(float value) {
    if (is_little_endian()) {
        return swap_bytes_float(value);
    }
    return value;
}

/**
 * @brief Convert a double value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
double network_to_host_double(double value) {
    if (is_little_endian()) {
        return swap_bytes_double(value);
    }
    return value;
}

/**
 * @brief Swap bytes in an array of 16-bit values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
void swap_bytes_uint16_array(uint16_t *array, size_t count) {
    for (size_t i = 0; i < count; i++) {
        array[i] = swap_bytes_uint16(array[i]);
    }
}

/**
 * @brief Swap bytes in an array of 32-bit values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
void swap_bytes_uint32_array(uint32_t *array, size_t count) {
    for (size_t i = 0; i < count; i++) {
        array[i] = swap_bytes_uint32(array[i]);
    }
}

/**
 * @brief Swap bytes in an array of 64-bit values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
void swap_bytes_uint64_array(uint64_t *array, size_t count) {
    for (size_t i = 0; i < count; i++) {
        array[i] = swap_bytes_uint64(array[i]);
    }
}

/**
 * @brief Swap bytes in an array of float values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
void swap_bytes_float_array(float *array, size_t count) {
    for (size_t i = 0; i < count; i++) {
        array[i] = swap_bytes_float(array[i]);
    }
}

/**
 * @brief Swap bytes in an array of double values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
void swap_bytes_double_array(double *array, size_t count) {
    for (size_t i = 0; i < count; i++) {
        array[i] = swap_bytes_double(array[i]);
    }
}

/**
 * @brief Convert a specific number of bytes between endianness
 *
 * @param data Pointer to the data to swap
 * @param size Size of each element in bytes (must be 2, 4, or 8)
 * @param count Number of elements to swap
 * @return 0 on success, -1 on invalid size
 */
int swap_endianness(void *data, size_t size, size_t count) {
    switch (size) {
        case 2:
            swap_bytes_uint16_array((uint16_t*)data, count);
            break;
        case 4:
            swap_bytes_uint32_array((uint32_t*)data, count);
            break;
        case 8:
            swap_bytes_uint64_array((uint64_t*)data, count);
            break;
        default:
            return -1; // Invalid size
    }
    return 0;
}
