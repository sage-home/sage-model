#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  // for off_t
#include <unistd.h>     // for pwrite/pread
#include <math.h>

// When building tests, we use printf instead of the logging system
#ifdef TESTING_STANDALONE
#define log_message(msg_type, file, line, func, fmt, ...) \
    fprintf(stderr, #msg_type ": %s:%d [%s] " fmt "\n", file, line, func, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_message(ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) log_message(WARNING, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_message(DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#include "../core/core_logging.h"
#endif
#include "../core/core_utils.h"
#include "io_buffer_manager.h"

/**
 * @file io_buffer_manager.c
 * @brief Implementation of buffer manager for efficient I/O
 */

/**
 * @brief I/O buffer structure
 *
 * Contains the buffer data and management information.
 */
struct io_buffer {
    void *data;                   /**< Buffer memory */
    size_t capacity;              /**< Total buffer capacity in bytes */
    size_t used;                  /**< Current bytes used in buffer */
    int fd;                       /**< File descriptor */
    off_t current_offset;         /**< Current file offset */
    io_write_fn write_callback;   /**< Write callback function */
    void *write_context;          /**< Context for write callback */
    bool is_dirty;                /**< Whether buffer contains unflushed data */
    struct io_buffer_config config; /**< Buffer configuration */
};

/**
 * @brief Create a buffer with the given configuration
 *
 * Initializes a new buffer according to the specified parameters.
 * Validates configuration values and allocates memory for the buffer.
 *
 * @param config Buffer configuration
 * @param fd File descriptor
 * @param initial_offset Initial file offset
 * @param write_callback Function to handle write operations
 * @param write_context Context data for write callback
 * @return Pointer to buffer, or NULL on error
 */
struct io_buffer* buffer_create(
    struct io_buffer_config* config, 
    int fd, 
    off_t initial_offset, 
    io_write_fn write_callback, 
    void* write_context
) {
    if (config == NULL || write_callback == NULL || fd < 0) {
        LOG_ERROR("Invalid parameters to buffer_create: config=%p, fd=%d, callback=%p", 
                 config, fd, write_callback);
        return NULL;
    }
    
    // Validate and adjust configuration
    struct io_buffer_config validated_config = *config;
    
    // Ensure minimum buffer size
    if (validated_config.initial_size < BUFFER_MIN_SIZE) {
        LOG_WARNING("Initial buffer size %zu is below minimum. Using %zu bytes instead.", 
                   validated_config.initial_size, (size_t)BUFFER_MIN_SIZE);
        validated_config.initial_size = BUFFER_MIN_SIZE;
    }
    
    // Ensure min_size <= initial_size <= max_size
    if (validated_config.min_size > validated_config.initial_size) {
        LOG_WARNING("Minimum buffer size %zu exceeds initial size %zu. Using initial size as minimum.", 
                   validated_config.min_size, validated_config.initial_size);
        validated_config.min_size = validated_config.initial_size;
    }
    
    if (validated_config.max_size < validated_config.initial_size) {
        LOG_WARNING("Maximum buffer size %zu is below initial size %zu. Using initial size as maximum.", 
                   validated_config.max_size, validated_config.initial_size);
        validated_config.max_size = validated_config.initial_size;
    }
    
    // Validate growth factor
    if (validated_config.growth_factor < 1.1f) {
        LOG_WARNING("Growth factor %.2f is too small. Using 1.1 instead.", 
                   validated_config.growth_factor);
        validated_config.growth_factor = 1.1f;
    }
    
    if (validated_config.growth_factor > 2.0f) {
        LOG_WARNING("Growth factor %.2f is too large. Using 2.0 instead.", 
                   validated_config.growth_factor);
        validated_config.growth_factor = 2.0f;
    }
    
    // Validate resize threshold percentage
    if (validated_config.resize_threshold_percent < 50 || 
        validated_config.resize_threshold_percent > 95) {
        LOG_WARNING("Resize threshold %d%% is outside valid range. Using default %d%%.", 
                   validated_config.resize_threshold_percent, BUFFER_DEFAULT_RESIZE_THRESHOLD);
        validated_config.resize_threshold_percent = BUFFER_DEFAULT_RESIZE_THRESHOLD;
    }
    
    // Allocate buffer structure
    struct io_buffer* buffer = (struct io_buffer*)malloc(sizeof(struct io_buffer));
    if (buffer == NULL) {
        LOG_ERROR("Failed to allocate memory for buffer structure");
        return NULL;
    }
    
    // Initialize buffer
    buffer->data = malloc(validated_config.initial_size);
    if (buffer->data == NULL) {
        LOG_ERROR("Failed to allocate memory for buffer of size %zu bytes", 
                 validated_config.initial_size);
        free(buffer);
        return NULL;
    }
    
    // Initialize buffer state
    buffer->capacity = validated_config.initial_size;
    buffer->used = 0;
    buffer->fd = fd;
    buffer->current_offset = initial_offset;
    buffer->write_callback = write_callback;
    buffer->write_context = write_context;
    buffer->is_dirty = false;
    buffer->config = validated_config;
    
    LOG_DEBUG("Created buffer of %zu bytes (%.2f MB) with fd=%d", 
             buffer->capacity, buffer->capacity / (1024.0 * 1024.0), fd);
    
    return buffer;
}

/**
 * @brief Create a default buffer configuration
 *
 * Initializes a configuration structure with reasonable defaults.
 *
 * @param initial_size_mb Initial buffer size in MB
 * @param min_size_mb Minimum buffer size in MB
 * @param max_size_mb Maximum buffer size in MB
 * @return Configuration structure with defaults
 */
struct io_buffer_config buffer_config_default(
    int initial_size_mb, 
    int min_size_mb, 
    int max_size_mb
) {
    struct io_buffer_config config;
    
    // Convert MB to bytes
    const size_t MB = 1024 * 1024;
    
    // Set defaults with reasonable minimum values
    config.initial_size = (initial_size_mb > 0) ? initial_size_mb * MB : 4 * MB;
    config.min_size = (min_size_mb > 0) ? min_size_mb * MB : 1 * MB;
    config.max_size = (max_size_mb > 0) ? max_size_mb * MB : 32 * MB;
    
    // Initialize with default behavior
    config.growth_factor = BUFFER_DEFAULT_GROWTH_FACTOR;
    config.auto_resize = true;
    config.resize_threshold_percent = BUFFER_DEFAULT_RESIZE_THRESHOLD;
    
    return config;
}

/**
 * @brief Resize a buffer
 *
 * Changes the buffer size, preserving existing content.
 *
 * @param buffer Buffer to resize
 * @param new_size New size in bytes
 * @return 0 on success, negative value on error
 */
int buffer_resize(struct io_buffer* buffer, size_t new_size) {
    if (buffer == NULL || buffer->data == NULL) {
        LOG_ERROR("Invalid buffer passed to buffer_resize");
        return -1;
    }
    
    // Apply size constraints
    if (new_size < buffer->config.min_size) {
        new_size = buffer->config.min_size;
    }
    
    if (new_size > buffer->config.max_size) {
        new_size = buffer->config.max_size;
    }
    
    // Check if resizing is necessary
    if (new_size == buffer->capacity) {
        return 0;  // No change needed
    }
    
    // If shrinking, make sure we have enough room for current data
    if (new_size < buffer->used) {
        LOG_WARNING("Cannot resize buffer to %zu bytes as it contains %zu bytes of data", 
                   new_size, buffer->used);
        new_size = buffer->used;
    }
    
    // Reallocate buffer memory
    void* new_data = realloc(buffer->data, new_size);
    if (new_data == NULL) {
        LOG_ERROR("Failed to resize buffer from %zu to %zu bytes", 
                 buffer->capacity, new_size);
        return -1;
    }
    
    LOG_DEBUG("Resized buffer from %zu to %zu bytes (%.2f MB)", 
             buffer->capacity, new_size, new_size / (1024.0 * 1024.0));
    
    buffer->data = new_data;
    buffer->capacity = new_size;
    
    return 0;
}

/**
 * @brief Flush buffer contents to disk
 *
 * Writes any buffered data to disk and resets buffer usage.
 *
 * @param buffer Buffer to flush
 * @return 0 on success, negative value on error
 */
int buffer_flush(struct io_buffer* buffer) {
    if (buffer == NULL || buffer->data == NULL || buffer->write_callback == NULL) {
        LOG_ERROR("Invalid buffer passed to buffer_flush");
        return -1;
    }
    
    // Nothing to flush
    if (buffer->used == 0 || !buffer->is_dirty) {
        return 0;
    }
    
    // Call write callback to actually write the data
    int result = buffer->write_callback(
        buffer->fd, 
        buffer->data, 
        buffer->used, 
        buffer->current_offset, 
        buffer->write_context
    );
    
    if (result < 0) {
        LOG_ERROR("Write callback failed during buffer_flush with error %d", result);
        return result;
    }
    
    // Update file offset
    buffer->current_offset += buffer->used;
    
    // Reset buffer usage
    buffer->used = 0;
    buffer->is_dirty = false;
    
    LOG_DEBUG("Flushed %zu bytes to fd=%d at offset %lld", 
             buffer->used, buffer->fd, (long long)buffer->current_offset - buffer->used);
    
    return 0;
}

/**
 * @brief Write data to a buffer
 *
 * Adds data to the buffer, flushing if necessary.
 *
 * @param buffer Buffer to write to
 * @param data Data to write
 * @param size Number of bytes to write
 * @return 0 on success, negative value on error
 */
int buffer_write(struct io_buffer* buffer, const void* data, size_t size) {
    if (buffer == NULL || buffer->data == NULL || data == NULL) {
        LOG_ERROR("Invalid parameters to buffer_write: buffer=%p, data=%p", buffer, data);
        return -1;
    }
    
    // Check if data fits in buffer
    if (buffer->used + size <= buffer->capacity) {
        // Data fits, copy it to buffer
        memcpy((char*)buffer->data + buffer->used, data, size);
        buffer->used += size;
        buffer->is_dirty = true;
        
        // Check if buffer is getting full and resize if auto-resize is enabled
        if (buffer->config.auto_resize && 
            buffer->capacity < buffer->config.max_size) {
            
            // Calculate threshold size
            size_t threshold = (buffer->capacity * buffer->config.resize_threshold_percent) / 100;
            
            // If buffer usage exceeds threshold, grow the buffer
            if (buffer->used > threshold) {
                size_t new_size = (size_t)(buffer->capacity * buffer->config.growth_factor);
                if (new_size > buffer->config.max_size) {
                    new_size = buffer->config.max_size;
                }
                
                buffer_resize(buffer, new_size);
                // Continue even if resize fails - we'll just flush more often
            }
        }
        
        return 0;
    } else {
        // Buffer would overflow, flush first
        int flush_result = buffer_flush(buffer);
        if (flush_result < 0) {
            return flush_result;
        }
        
        // If the data is larger than the buffer, write it directly
        if (size > buffer->capacity) {
            // Write data directly, bypassing the buffer
            int result = buffer->write_callback(
                buffer->fd, 
                data, 
                size, 
                buffer->current_offset, 
                buffer->write_context
            );
            
            if (result < 0) {
                LOG_ERROR("Write callback failed during direct write with error %d", result);
                return result;
            }
            
            // Update file offset
            buffer->current_offset += size;
            
            return 0;
        } else {
            // Data now fits in the empty buffer
            memcpy(buffer->data, data, size);
            buffer->used = size;
            buffer->is_dirty = true;
            
            return 0;
        }
    }
}

/**
 * @brief Read data using a buffer
 *
 * This is a placeholder for future read buffering functionality.
 * Currently it directly uses the read callback without buffering.
 *
 * @param buffer Buffer to read from
 * @param read_callback Function to handle read operations
 * @param read_context Context data for read callback
 * @param dest Destination buffer to read into
 * @param size Number of bytes to read
 * @return Number of bytes read, or negative value on error
 */
ssize_t buffer_read(
    struct io_buffer* buffer,
    io_read_fn read_callback,
    void* read_context,
    void* dest, 
    size_t size
) {
    if (buffer == NULL || read_callback == NULL || dest == NULL) {
        LOG_ERROR("Invalid parameters to buffer_read: buffer=%p, callback=%p, dest=%p",
                 buffer, read_callback, dest);
        return -1;
    }
    
    // For now, just directly call the read callback
    // Future versions can implement proper read buffering
    ssize_t bytes_read = read_callback(
        buffer->fd, 
        dest, 
        size, 
        buffer->current_offset, 
        read_context
    );
    
    if (bytes_read > 0) {
        // Update file offset
        buffer->current_offset += bytes_read;
    }
    
    return bytes_read;
}

/**
 * @brief Destroy a buffer
 *
 * Flushes any remaining data and frees all resources.
 *
 * @param buffer Buffer to destroy
 * @return 0 on success, negative value on error
 */
int buffer_destroy(struct io_buffer* buffer) {
    if (buffer == NULL) {
        return 0;  // Nothing to destroy
    }
    
    int result = 0;
    
    // Flush any remaining data
    if (buffer->data != NULL && buffer->used > 0 && buffer->is_dirty) {
        result = buffer_flush(buffer);
    }
    
    // Free resources
    if (buffer->data != NULL) {
        free(buffer->data);
    }
    
    free(buffer);
    
    return result;
}

/**
 * @brief Get current buffer size
 *
 * @param buffer Buffer to query
 * @return Current capacity in bytes, or 0 on error
 */
size_t buffer_get_capacity(struct io_buffer* buffer) {
    if (buffer == NULL) {
        return 0;
    }
    
    return buffer->capacity;
}

/**
 * @brief Get amount of data currently in buffer
 *
 * @param buffer Buffer to query
 * @return Number of bytes in buffer, or 0 on error
 */
size_t buffer_get_used(struct io_buffer* buffer) {
    if (buffer == NULL) {
        return 0;
    }
    
    return buffer->used;
}