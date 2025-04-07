#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>  // for off_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file io_buffer_manager.h
 * @brief Efficient buffered I/O management for SAGE
 *
 * This file provides a configurable buffer management system for I/O operations,
 * allowing for more efficient disk access by reducing the number of system calls.
 * It features dynamic buffer sizing, callback-based I/O operations, and support
 * for different I/O patterns.
 */

/**
 * @brief Minimum buffer size in bytes
 */
#define BUFFER_MIN_SIZE (64 * 1024)  // 64 KB

/**
 * @brief Default buffer growth factor 
 */
#define BUFFER_DEFAULT_GROWTH_FACTOR 1.5f

/**
 * @brief Default resize threshold percentage
 */
#define BUFFER_DEFAULT_RESIZE_THRESHOLD 80

/**
 * @brief Write callback function type
 *
 * Function pointer type for the actual write operation.
 * This allows the buffer manager to be used with different I/O mechanisms.
 *
 * @param fd File descriptor
 * @param buffer Data buffer to write
 * @param size Number of bytes to write
 * @param offset File offset to write at
 * @param context User-provided context data 
 * @return 0 on success, negative value on error
 */
typedef int (*io_write_fn)(int fd, const void* buffer, size_t size, off_t offset, void* context);

/**
 * @brief Read callback function type
 *
 * Function pointer type for the actual read operation.
 * This allows the buffer manager to be used with different I/O mechanisms.
 *
 * @param fd File descriptor
 * @param buffer Buffer to read into
 * @param size Number of bytes to read
 * @param offset File offset to read from
 * @param context User-provided context data
 * @return Number of bytes read on success, negative value on error
 */
typedef ssize_t (*io_read_fn)(int fd, void* buffer, size_t size, off_t offset, void* context);

/**
 * @brief Buffer configuration structure
 *
 * Contains parameters controlling buffer behavior.
 */
struct io_buffer_config {
    size_t initial_size;           /**< Initial buffer size in bytes */
    size_t min_size;               /**< Minimum buffer size in bytes */
    size_t max_size;               /**< Maximum buffer size in bytes */
    float growth_factor;           /**< Growth factor for auto-resize */
    bool auto_resize;              /**< Enable/disable automatic resizing */
    int resize_threshold_percent;  /**< Threshold percentage to trigger resize */
};

/**
 * @brief Forward declaration of buffer structure
 *
 * Actual definition is in the implementation file.
 */
struct io_buffer;

/**
 * @brief Create a new I/O buffer
 *
 * Initializes a new buffer with the specified configuration.
 *
 * @param config Buffer configuration
 * @param fd File descriptor
 * @param initial_offset Initial file offset
 * @param write_callback Function to handle actual write operations
 * @param write_context Context data for write callback
 * @return Pointer to new buffer or NULL on error
 */
extern struct io_buffer* buffer_create(
    struct io_buffer_config* config, 
    int fd, 
    off_t initial_offset, 
    io_write_fn write_callback, 
    void* write_context
);

/**
 * @brief Write data to a buffer
 *
 * Writes data to the buffer, flushing if necessary.
 * This function uses the write callback when the buffer is flushed.
 *
 * @param buffer Buffer to write to
 * @param data Data to write
 * @param size Number of bytes to write
 * @return 0 on success, negative value on error
 */
extern int buffer_write(struct io_buffer* buffer, const void* data, size_t size);

/**
 * @brief Read data using a buffer
 *
 * Reads data through the buffer, filling it as needed.
 * This is an extension point for future read buffering.
 *
 * @param buffer Buffer to read from
 * @param read_callback Function to handle actual read operations
 * @param read_context Context data for read callback
 * @param dest Destination buffer to read into
 * @param size Number of bytes to read
 * @return Number of bytes read, or negative value on error
 */
extern ssize_t buffer_read(
    struct io_buffer* buffer,
    io_read_fn read_callback,
    void* read_context,
    void* dest, 
    size_t size
);

/**
 * @brief Flush buffer contents to disk
 *
 * Forces writing any buffered data to disk using the write callback.
 *
 * @param buffer Buffer to flush
 * @return 0 on success, negative value on error
 */
extern int buffer_flush(struct io_buffer* buffer);

/**
 * @brief Destroy a buffer
 *
 * Flushes any remaining data and frees all resources.
 *
 * @param buffer Buffer to destroy
 * @return 0 on success, negative value on error
 */
extern int buffer_destroy(struct io_buffer* buffer);

/**
 * @brief Get current buffer size
 *
 * @param buffer Buffer to query
 * @return Current capacity in bytes, or 0 on error
 */
extern size_t buffer_get_capacity(struct io_buffer* buffer);

/**
 * @brief Get amount of data currently in buffer
 *
 * @param buffer Buffer to query
 * @return Number of bytes in buffer, or 0 on error
 */
extern size_t buffer_get_used(struct io_buffer* buffer);

/**
 * @brief Resize a buffer
 *
 * Manually changes buffer size. This is primarily for internal use,
 * but can be called directly if needed.
 *
 * @param buffer Buffer to resize
 * @param new_size New size in bytes
 * @return 0 on success, negative value on error
 */
extern int buffer_resize(struct io_buffer* buffer, size_t new_size);

/**
 * @brief Create a default configuration
 *
 * Initializes a configuration structure with reasonable defaults.
 *
 * @param initial_size_mb Initial buffer size in MB
 * @param min_size_mb Minimum buffer size in MB
 * @param max_size_mb Maximum buffer size in MB
 * @return Configuration structure with defaults
 */
extern struct io_buffer_config buffer_config_default(
    int initial_size_mb, 
    int min_size_mb, 
    int max_size_mb
);

#ifdef __cplusplus
}
#endif