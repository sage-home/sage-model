#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>  // for off_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file io_memory_map.h
 * @brief Memory mapping service for efficient file access
 * 
 * Usage:
 * 
 * // Check if memory mapping is available
 * if (mmap_is_available()) {
 *     // Set up mapping options
 *     struct mmap_options options = mmap_default_options();
 *     options.mode = MMAP_READ_ONLY;
 *     
 *     // Map a file
 *     struct mmap_region* region = mmap_file("myfile.dat", -1, &options);
 *     if (region != NULL) {
 *         // Get pointer to mapped memory
 *         void* data = mmap_get_pointer(region);
 *         size_t size = mmap_get_size(region);
 *         
 *         // Use the data...
 *         
 *         // Clean up when finished
 *         mmap_unmap(region);
 *     }
 * }
 *
 * This file provides a cross-platform memory mapping API for efficient access
 * to large files. It abstracts platform-specific details (POSIX vs Windows)
 * and provides a simple interface for mapping, accessing, and unmapping files.
 */

/**
 * @brief Access mode for memory mapping
 */
enum mmap_access_mode {
    MMAP_READ_ONLY     /**< Read-only mapping (most common for input files) */
    /* Other modes like READ_WRITE could be added in the future if needed */
};

/**
 * @brief Options for memory mapping
 */
struct mmap_options {
    enum mmap_access_mode mode;  /**< Access mode for the mapping */
    size_t mapping_size;         /**< Size to map (0 for entire file) */
    off_t offset;                /**< Starting offset in file (must be page-aligned) */
};

/**
 * @brief Memory mapping region information
 *
 * Opaque structure - implementation details are hidden in the C file
 */
struct mmap_region;

/**
 * @brief Create a memory mapping of a file
 *
 * Maps a file into memory for efficient access.
 *
 * @param filename File to map (can be NULL if valid fd is provided)
 * @param fd File descriptor (can be -1 if valid filename is provided)
 * @param options Mapping options
 * @return Pointer to mapping region or NULL on error
 */
extern struct mmap_region* mmap_file(const char* filename, int fd, struct mmap_options* options);

/**
 * @brief Get a pointer to the mapped memory
 *
 * @param region Memory mapping region
 * @return Pointer to mapped memory or NULL on error
 */
extern void* mmap_get_pointer(struct mmap_region* region);

/**
 * @brief Get the size of the mapped memory
 *
 * @param region Memory mapping region
 * @return Size of the mapped memory in bytes or 0 on error
 */
extern size_t mmap_get_size(struct mmap_region* region);

/**
 * @brief Unmap a memory mapping
 *
 * Releases resources associated with a memory mapping.
 *
 * @param region Memory mapping region to unmap
 * @return 0 on success, non-zero on failure
 */
extern int mmap_unmap(struct mmap_region* region);

/**
 * @brief Check if memory mapping is supported on this platform
 *
 * @return true if supported, false otherwise
 */
extern bool mmap_is_available(void);

/**
 * @brief Get the last error message
 *
 * @return Error message string
 */
extern const char* mmap_get_error(void);

/**
 * @brief Create default mapping options
 *
 * Initializes options with reasonable defaults.
 *
 * @return Default mapping options
 */
extern struct mmap_options mmap_default_options(void);

#ifdef __cplusplus
}
#endif
