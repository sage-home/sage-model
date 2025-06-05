#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../core/core_allvars.h"
#include "io_interface.h"
#include "io_lhalo_binary.h"
#include "io_hdf5_utils.h"
#ifdef HDF5
#include "io_gadget4_hdf5.h"
#include "io_genesis_hdf5.h"
#include "io_consistent_trees_hdf5.h"
#include "io_lhalo_hdf5.h"
#endif
/**
 * @brief Temporary HDF5 handler stub implementations
 * 
 * These stubs provide placeholder implementations for HDF5-based handlers
 * until the full implementations are completed. This approach allows us to:
 * 1. Register handlers with the I/O interface system
 * 2. Test format detection and interface integration
 * 3. Maintain compatibility with the overall architecture
 * 
 * Full implementations will be provided in separate files (io_lhalo_hdf5.c, etc.)
 * as part of the completion of Phase 3.2. These stubs will be removed once the
 * Makefile is updated to include the complete implementations.
 */

// HDF5 handler stub definitions - These will be replaced by full implementations
// Note: LHalo HDF5 handler now implemented in src/io/io_lhalo_hdf5.c

// ConsistentTrees HDF5 handler definition
static struct io_interface consistent_trees_hdf5_handler = {
    .name = "ConsistentTrees HDF5",
    .version = "1.0",
    .format_id = IO_FORMAT_CONSISTENT_TREES_HDF5,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_METADATA_QUERY | IO_CAP_METADATA_ATTRS | IO_CAP_MULTI_FILE,
    
    .initialize = ctrees_hdf5_initialize,
    .read_forest = ctrees_hdf5_read_forest,
    .write_galaxies = NULL,  // Input format doesn't support writing
    .cleanup = ctrees_hdf5_cleanup,
    
    .close_open_handles = ctrees_hdf5_close_open_handles,
    .get_open_handle_count = ctrees_hdf5_get_open_handle_count,
    
    .last_error = IO_ERROR_NONE,
    .error_message = {0}
};

// Gadget4 HDF5 handler definition
static struct io_interface gadget4_hdf5_handler = {
    .name = "Gadget4 HDF5",
    .version = "1.0",
    .format_id = IO_FORMAT_GADGET4_HDF5,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_METADATA_QUERY | IO_CAP_METADATA_ATTRS | IO_CAP_MULTI_FILE,
    
    .initialize = gadget4_hdf5_initialize,
    .read_forest = gadget4_hdf5_read_forest,
    .write_galaxies = NULL,  // Input format doesn't support writing
    .cleanup = gadget4_hdf5_cleanup,
    
    .close_open_handles = gadget4_hdf5_close_open_handles,
    .get_open_handle_count = gadget4_hdf5_get_open_handle_count,
    
    .last_error = IO_ERROR_NONE,
    .error_message = {0}
};

// Genesis HDF5 handler definition
static struct io_interface genesis_hdf5_handler = {
    .name = "Genesis HDF5",
    .version = "1.0",
    .format_id = IO_FORMAT_GENESIS_HDF5,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_METADATA_QUERY | IO_CAP_METADATA_ATTRS | IO_CAP_MULTI_FILE,
    
    .initialize = genesis_hdf5_initialize,
    .read_forest = genesis_hdf5_read_forest,
    .write_galaxies = NULL,  // Input format doesn't support writing
    .cleanup = genesis_hdf5_cleanup,
    
    .close_open_handles = genesis_hdf5_close_open_handles,
    .get_open_handle_count = genesis_hdf5_get_open_handle_count,
    
    .last_error = IO_ERROR_NONE,
    .error_message = {0}
};

// Note: io_lhalo_hdf5_init() and io_is_lhalo_hdf5() now implemented in src/io/io_lhalo_hdf5.c

/**
 * @brief Initialize the ConsistentTrees HDF5 handler stub
 * 
 * Registers the stub handler with the I/O interface system.
 * This will be replaced by a full implementation in a separate file.
 * 
 * @return 0 on success, non-zero on failure
 */
int io_consistent_trees_hdf5_init(void) {
    // Register the stub handler for framework testing
    return io_register_handler(&consistent_trees_hdf5_handler);
}

/**
 * @brief Detect if a file is in ConsistentTrees HDF5 format
 * 
 * Currently uses a basic extension check, will be enhanced with
 * content-based detection in the full implementation.
 * 
 * @param filename File to check
 * @return true if the file appears to be ConsistentTrees HDF5, false otherwise
 */
bool io_is_consistent_trees_hdf5(const char *filename) {
    // Basic extension-based detection with safety checks
    if (filename == NULL || strlen(filename) == 0) {
        return false;
    }
    
    // Reject paths that look suspicious or dangerous, including spaces and special chars
    if (strstr(filename, "../") != NULL || 
        strstr(filename, "/etc/") != NULL ||
        strchr(filename, '\n') != NULL ||
        strchr(filename, '\r') != NULL ||
        strchr(filename, ' ') != NULL ||
        strpbrk(filename, "@#$%^&*()") != NULL) {
        return false;
    }
    
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return false;
    }
    
    // For now, we just check for HDF5 extension
    // In a full implementation, we would check for ConsistentTrees-specific markers
    return (strcmp(ext, ".hdf5") == 0 || strcmp(ext, ".h5") == 0);
}

/**
 * @brief Initialize the Gadget4 HDF5 handler stub
 * 
 * Registers the stub handler with the I/O interface system.
 * This will be replaced by a full implementation in a separate file.
 * 
 * @return 0 on success, non-zero on failure
 */
int io_gadget4_hdf5_init(void) {
    // Register the stub handler for framework testing
    return io_register_handler(&gadget4_hdf5_handler);
}

/**
 * @brief Detect if a file is in Gadget4 HDF5 format
 * 
 * Currently uses a basic extension check, will be enhanced with
 * content-based detection in the full implementation.
 * 
 * @param filename File to check
 * @return true if the file appears to be Gadget4 HDF5, false otherwise
 */
bool io_is_gadget4_hdf5(const char *filename) {
    // Basic extension-based detection with safety checks
    if (filename == NULL || strlen(filename) == 0) {
        return false;
    }
    
    // Reject paths that look suspicious or dangerous
    if (strstr(filename, "../") != NULL || 
        strstr(filename, "/etc/") != NULL ||
        strchr(filename, '\n') != NULL ||
        strchr(filename, '\r') != NULL) {
        return false;
    }
    
    // Reject filenames with suspicious special characters or spaces
    const char *suspicious_chars = "@#$%^&*() "; // Added space character
    for (const char *c = suspicious_chars; *c; c++) {
        if (strchr(filename, *c) != NULL) {
            return false;
        }
    }
    
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return false;
    }
    
    // For now, we just check for HDF5 extension
    // In a full implementation, we would check for Gadget4-specific markers
    return (strcmp(ext, ".hdf5") == 0 || strcmp(ext, ".h5") == 0);
}

/**
 * @brief Initialize the Genesis HDF5 handler stub
 * 
 * Registers the stub handler with the I/O interface system.
 * This will be replaced by a full implementation in a separate file.
 * 
 * @return 0 on success, non-zero on failure
 */
int io_genesis_hdf5_init(void) {
    // Register the stub handler for framework testing
    return io_register_handler(&genesis_hdf5_handler);
}

/**
 * @brief Detect if a file is in Genesis HDF5 format
 * 
 * Currently uses a basic extension check, will be enhanced with
 * content-based detection in the full implementation.
 * 
 * @param filename File to check
 * @return true if the file appears to be Genesis HDF5, false otherwise
 */
bool io_is_genesis_hdf5(const char *filename) {
    // Basic extension-based detection with safety checks
    if (filename == NULL || strlen(filename) == 0) {
        return false;
    }
    
    // Reject paths that look suspicious or dangerous, including spaces and special chars
    if (strstr(filename, "../") != NULL || 
        strstr(filename, "/etc/") != NULL ||
        strchr(filename, '\n') != NULL ||
        strchr(filename, '\r') != NULL ||
        strchr(filename, ' ') != NULL ||
        strpbrk(filename, "@#$%^&*()") != NULL) {
        return false;
    }
    
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return false;
    }
    
    // For now, we just check for HDF5 extension
    // In a full implementation, we would check for Genesis-specific markers
    return (strcmp(ext, ".hdf5") == 0 || strcmp(ext, ".h5") == 0);
}

/**
 * @brief Maximum number of I/O handlers that can be registered
 */
#define MAX_IO_HANDLERS 16

/**
 * @brief Global handler registry
 */
static struct io_interface *handlers[MAX_IO_HANDLERS];

/**
 * @brief Number of registered handlers
 */
static int num_handlers = 0;

/**
 * @brief Global error state
 */
static int last_error = IO_ERROR_NONE;
static char error_message[256] = {0};

/**
 * @brief Initialized flag
 */
static bool initialized = false;

/**
 * @brief Initialize the I/O interface system
 * 
 * Must be called before any other I/O interface functions.
 * 
 * @return 0 on success, non-zero on failure
 */
int io_init() {
    if (initialized) {
        return 0;  // Already initialized
    }
    
    // Reset the handler registry
    num_handlers = 0;
    memset(handlers, 0, sizeof(handlers));
    
    // Reset error state
    io_clear_error();
    
    initialized = true;
    
    // Register built-in handlers
    io_lhalo_binary_init();
    
#ifdef HDF5
    io_lhalo_hdf5_init();
    io_consistent_trees_hdf5_init();
    io_gadget4_hdf5_init();
    io_genesis_hdf5_init();
#endif
    
    return 0;
}

/**
 * @brief Clean up the I/O interface system
 * 
 * Should be called at program exit to free resources.
 */
void io_cleanup() {
    if (!initialized) {
        return;  // Not initialized
    }
    
    // We don't free the handlers here because they're typically statically
    // allocated or managed elsewhere
    num_handlers = 0;
    initialized = false;
}

/**
 * @brief Register a new I/O handler
 * 
 * @param handler Pointer to the handler to register
 * @return 0 on success, non-zero on failure
 */
int io_register_handler(struct io_interface *handler) {
    if (!initialized) {
        io_set_error(IO_ERROR_UNKNOWN, "I/O interface system not initialized");
        return -1;
    }
    
    if (num_handlers >= MAX_IO_HANDLERS) {
        io_set_error(IO_ERROR_RESOURCE_LIMIT, "Maximum number of I/O handlers reached");
        return -1;
    }
    
    if (handler == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL handler passed to io_register_handler");
        return -1;
    }
    
    // Check for duplicates
    for (int i = 0; i < num_handlers; i++) {
        if (handlers[i]->format_id == handler->format_id) {
            io_set_error(IO_ERROR_VALIDATION_FAILED, "Handler with same format_id already registered");
            return -1;
        }
    }
    
    handlers[num_handlers++] = handler;
    return 0;
}

/**
 * @brief Get a handler by format ID
 * 
 * @param format_id Format identifier
 * @return Pointer to the handler, or NULL if not found
 */
struct io_interface *io_get_handler_by_id(int format_id) {
    if (!initialized) {
        io_set_error(IO_ERROR_UNKNOWN, "I/O interface system not initialized");
        return NULL;
    }
    
    for (int i = 0; i < num_handlers; i++) {
        if (handlers[i]->format_id == format_id) {
            return handlers[i];
        }
    }
    
    io_set_error(IO_ERROR_VALIDATION_FAILED, "No handler found with specified format_id");
    return NULL;
}

/**
 * @brief Map a TreeType enum to a format ID
 * 
 * @param tree_type TreeType from core_allvars.h as integer
 * @return Corresponding format ID
 */
int io_map_tree_type_to_format_id(int tree_type) {
    switch (tree_type) {
        case lhalo_binary:
            return IO_FORMAT_LHALO_BINARY;
        case lhalo_hdf5:
            return IO_FORMAT_LHALO_HDF5;
        case consistent_trees_ascii:
            return IO_FORMAT_CONSISTENT_TREES_ASCII;
        case consistent_trees_hdf5:
            return IO_FORMAT_CONSISTENT_TREES_HDF5;
        case gadget4_hdf5:
            return IO_FORMAT_GADGET4_HDF5;
        case genesis_hdf5:
            return IO_FORMAT_GENESIS_HDF5;
        default:
            io_set_error(IO_ERROR_VALIDATION_FAILED, "Unknown tree type");
            return -1;
    }
}


/**
 * @brief Detect format from file
 * 
 * @param filename File to examine
 * @return Pointer to the handler, or NULL if not detected
 */
struct io_interface *io_detect_format(const char *filename) {
    if (!initialized) {
        io_set_error(IO_ERROR_UNKNOWN, "I/O interface system not initialized");
        return NULL;
    }
    
    if (filename == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL filename passed to io_detect_format");
        return NULL;
    }
    
    // Try specific format detection functions first
    if (io_is_lhalo_binary(filename)) {
        return io_get_handler_by_id(IO_FORMAT_LHALO_BINARY);
    }
    
#ifdef HDF5
    if (io_is_lhalo_hdf5(filename)) {
        return io_get_handler_by_id(IO_FORMAT_LHALO_HDF5);
    }
    
    if (io_is_consistent_trees_hdf5(filename)) {
        return io_get_handler_by_id(IO_FORMAT_CONSISTENT_TREES_HDF5);
    }
    
    if (io_is_gadget4_hdf5(filename)) {
        return io_get_handler_by_id(IO_FORMAT_GADGET4_HDF5);
    }
    
    if (io_is_genesis_hdf5(filename)) {
        return io_get_handler_by_id(IO_FORMAT_GENESIS_HDF5);
    }
#endif
    
    // Fall back to extension-based detection
    const char *ext = strrchr(filename, '.');
    if (ext != NULL) {
        if (strcmp(ext, ".hdf5") == 0 || strcmp(ext, ".h5") == 0) {
            // Return first HDF5 handler
            for (int j = 0; j < num_handlers; j++) {
                if (strstr(handlers[j]->name, "HDF5") != NULL) {
                    return handlers[j];
                }
            }
        } else if (strcmp(ext, ".dat") == 0 || strcmp(ext, ".bin") == 0) {
            // Return first binary handler
            for (int j = 0; j < num_handlers; j++) {
                if (strstr(handlers[j]->name, "Binary") != NULL) {
                    return handlers[j];
                }
            }
        }
    }
    
    io_set_error(IO_ERROR_FORMAT_ERROR, "Could not detect format of file");
    return NULL;
}

/**
 * @brief Get the last error code
 * 
 * @return Last error code
 */
int io_get_last_error() {
    return last_error;
}

/**
 * @brief Get the last error message
 * 
 * @return Last error message
 */
const char *io_get_error_message() {
    return error_message;
}

/**
 * @brief Set an error
 * 
 * @param error_code Error code
 * @param message Error message
 */
void io_set_error(int error_code, const char *message) {
    last_error = error_code;
    strncpy(error_message, message, sizeof(error_message) - 1);
    error_message[sizeof(error_message) - 1] = '\0';
}

/**
 * @brief Set an error with formatting
 * 
 * @param error_code Error code
 * @param format Format string
 * @param ... Format arguments
 */
void io_set_error_fmt(int error_code, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_message, sizeof(error_message), format, args);
    va_end(args);
    last_error = error_code;
}

/**
 * @brief Clear the last error
 */
void io_clear_error() {
    last_error = IO_ERROR_NONE;
    error_message[0] = '\0';
}

/**
 * @brief Check if a format supports a capability
 * 
 * @param handler Handler to check
 * @param capability Capability to check for
 * @return true if supported, false otherwise
 */
bool io_has_capability(struct io_interface *handler, enum io_capabilities capability) {
    if (handler == NULL) {
        return false;
    }
    
    return (handler->capabilities & capability) != 0;
}
