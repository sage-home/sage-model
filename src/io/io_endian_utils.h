#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file io_endian_utils.h
 * @brief Utilities for cross-platform endianness handling
 *
 * This file provides utilities for detecting machine endianness and
 * converting data between different byte orders. These functions are
 * essential for ensuring binary files are correctly read and written
 * across different architectures.
 */

/**
 * @brief Endianness types
 */
enum endian_type {
    ENDIAN_LITTLE,  /**< Little-endian (e.g., x86, x86-64, ARM in little-endian mode) */
    ENDIAN_BIG,     /**< Big-endian (e.g., PowerPC, SPARC, ARM in big-endian mode) */
    ENDIAN_UNKNOWN  /**< Could not determine endianness */
};

/**
 * @brief Check if the system is little-endian
 *
 * @return true if the system is little-endian, false otherwise
 */
extern bool is_little_endian(void);

/**
 * @brief Check if the system is big-endian
 *
 * @return true if the system is big-endian, false otherwise
 */
extern bool is_big_endian(void);

/**
 * @brief Get the system's endianness
 *
 * @return ENDIAN_LITTLE, ENDIAN_BIG, or ENDIAN_UNKNOWN
 */
extern enum endian_type get_system_endianness(void);

/**
 * @brief Swap bytes in a 16-bit value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
extern uint16_t swap_bytes_uint16(uint16_t value);

/**
 * @brief Swap bytes in a 32-bit value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
extern uint32_t swap_bytes_uint32(uint32_t value);

/**
 * @brief Swap bytes in a 64-bit value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
extern uint64_t swap_bytes_uint64(uint64_t value);

/**
 * @brief Swap bytes in a float value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
extern float swap_bytes_float(float value);

/**
 * @brief Swap bytes in a double value
 *
 * @param value The value to swap
 * @return The byte-swapped value
 */
extern double swap_bytes_double(double value);

/**
 * @brief Convert a 16-bit value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
extern uint16_t host_to_network_uint16(uint16_t value);

/**
 * @brief Convert a 32-bit value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
extern uint32_t host_to_network_uint32(uint32_t value);

/**
 * @brief Convert a 64-bit value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
extern uint64_t host_to_network_uint64(uint64_t value);

/**
 * @brief Convert a 16-bit value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
extern uint16_t network_to_host_uint16(uint16_t value);

/**
 * @brief Convert a 32-bit value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
extern uint32_t network_to_host_uint32(uint32_t value);

/**
 * @brief Convert a 64-bit value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
extern uint64_t network_to_host_uint64(uint64_t value);

/**
 * @brief Convert a float value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
extern float host_to_network_float(float value);

/**
 * @brief Convert a double value from host to network byte order
 *
 * @param value The value to convert
 * @return The value in network byte order (big-endian)
 */
extern double host_to_network_double(double value);

/**
 * @brief Convert a float value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
extern float network_to_host_float(float value);

/**
 * @brief Convert a double value from network to host byte order
 *
 * @param value The value to convert
 * @return The value in host byte order
 */
extern double network_to_host_double(double value);

/**
 * @brief Swap bytes in an array of 16-bit values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
extern void swap_bytes_uint16_array(uint16_t *array, size_t count);

/**
 * @brief Swap bytes in an array of 32-bit values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
extern void swap_bytes_uint32_array(uint32_t *array, size_t count);

/**
 * @brief Swap bytes in an array of 64-bit values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
extern void swap_bytes_uint64_array(uint64_t *array, size_t count);

/**
 * @brief Swap bytes in an array of float values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
extern void swap_bytes_float_array(float *array, size_t count);

/**
 * @brief Swap bytes in an array of double values
 *
 * @param array The array to swap
 * @param count Number of elements in the array
 */
extern void swap_bytes_double_array(double *array, size_t count);

/**
 * @brief Convert a specific number of bytes between endianness
 *
 * @param data Pointer to the data to swap
 * @param size Size of each element in bytes (must be 2, 4, or 8)
 * @param count Number of elements to swap
 * @return 0 on success, -1 on invalid size
 */
extern int swap_endianness(void *data, size_t size, size_t count);

#ifdef __cplusplus
}
#endif
