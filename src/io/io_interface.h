#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/core_allvars.h" /* For Valid_TreeTypes and Valid_OutputFormat enums */

/**
 * @file io_interface.h
 * @brief Unified I/O interface for SAGE, providing format-agnostic access to data
 *
 * This interface defines a common set of operations for reading tree data and writing
 * galaxy data, abstracting away format-specific details. It provides capabilities for
 * format detection, resource management, and error handling.
 */

/**
 * @brief Capability flags for I/O interfaces
 *
 * These flags indicate what features a particular I/O format supports.
 * Each handler should set the appropriate flags based on its capabilities.
 */
enum io_capabilities {
    IO_CAP_RANDOM_ACCESS    = (1 << 0),  /**< Supports random access to forests */
    IO_CAP_MULTI_FILE       = (1 << 1),  /**< Supports multi-file datasets */
    IO_CAP_METADATA_QUERY   = (1 << 2),  /**< Supports metadata queries */
    IO_CAP_PARALLEL_READ    = (1 << 3),  /**< Supports parallel reading */
    IO_CAP_COMPRESSION      = (1 << 4),  /**< Supports compression */
    IO_CAP_EXTENDED_PROPS   = (1 << 5),  /**< Supports extended properties */
    IO_CAP_APPEND           = (1 << 6),  /**< Supports append operations */
    IO_CAP_CHUNKED_WRITE    = (1 << 7),  /**< Supports chunked writing */
    IO_CAP_METADATA_ATTRS   = (1 << 8)   /**< Supports metadata attributes */
};

/**
 * @brief Error codes for I/O operations
 *
 * Standardized error codes for all I/O operations.
 */
enum io_error_codes {
    IO_ERROR_NONE              = 0,   /**< No error */
    IO_ERROR_FILE_NOT_FOUND    = 1,   /**< File not found */
    IO_ERROR_FORMAT_ERROR      = 2,   /**< File format error */
    IO_ERROR_RESOURCE_LIMIT    = 3,   /**< Resource limit exceeded */
    IO_ERROR_HANDLE_INVALID    = 4,   /**< Invalid handle */
    IO_ERROR_MEMORY_ALLOCATION = 5,   /**< Memory allocation failed */
    IO_ERROR_VALIDATION_FAILED = 6,   /**< Validation failed */
    IO_ERROR_UNSUPPORTED_OP    = 7,   /**< Unsupported operation */
    IO_ERROR_UNKNOWN           = 8    /**< Unknown error */
};

/**
 * @brief Format identifiers for I/O handlers
 *
 * Unique identifiers for each supported format.
 */
enum io_format_types {
    IO_FORMAT_LHALO_BINARY         = 0,  /**< LHalo Binary format */
    IO_FORMAT_LHALO_HDF5           = 1,  /**< LHalo HDF5 format */
    IO_FORMAT_CONSISTENT_TREES_ASCII = 2,  /**< ConsistentTrees ASCII format */
    IO_FORMAT_CONSISTENT_TREES_HDF5  = 3,  /**< ConsistentTrees HDF5 format */
    IO_FORMAT_GADGET4_HDF5         = 4,  /**< Gadget4 HDF5 format */
    IO_FORMAT_GENESIS_HDF5         = 5,  /**< Genesis HDF5 format */
    IO_FORMAT_HDF5_OUTPUT          = 6   /**< HDF5 galaxy output format */
};

// Forward declarations
struct io_interface;

/**
 * @brief I/O Interface definition
 *
 * Core structure defining the interface for all I/O operations.
 * Each format handler should implement this interface.
 */
struct io_interface {
    // Metadata
    const char *name;         /**< Format name */
    const char *version;      /**< Interface version */
    int format_id;            /**< Format identifier */
    uint32_t capabilities;    /**< Capability flags */
    
    // Core operations
    int (*initialize)(const char *filename, struct params *params, void **format_data);
    int64_t (*read_forest)(int64_t forestnr, struct halo_data **halos, 
                          struct forest_info *forest_info, void *format_data);
    int (*write_galaxies)(struct GALAXY *galaxies, int ngals, 
                         struct save_info *save_info, void *format_data);
    int (*cleanup)(void *format_data);
    
    // Resource management
    int (*close_open_handles)(void *format_data);
    int (*get_open_handle_count)(void *format_data);
    
    // Error handling
    int last_error;           /**< Last error code */
    char error_message[256];  /**< Last error message */
};

/**
 * @brief Initialize the I/O interface system
 * 
 * Must be called before any other I/O interface functions.
 * 
 * @return 0 on success, non-zero on failure
 */
extern int io_init();

/**
 * @brief Clean up the I/O interface system
 * 
 * Should be called at program exit to free resources.
 */
extern void io_cleanup();

/**
 * @brief Register a new I/O handler
 * 
 * @param handler Pointer to the handler to register
 * @return 0 on success, non-zero on failure
 */
extern int io_register_handler(struct io_interface *handler);

/**
 * @brief Get a handler by format ID
 * 
 * @param format_id Format identifier
 * @return Pointer to the handler, or NULL if not found
 */
extern struct io_interface *io_get_handler_by_id(int format_id);

/**
 * @brief Map a TreeType enum to a format ID
 * 
 * @param tree_type TreeType from core_allvars.h as integer
 * @return Corresponding format ID
 */
extern int io_map_tree_type_to_format_id(int tree_type);

/**
 * @brief Map an OutputFormat enum to a format ID
 * 
 * @param output_format OutputFormat from core_allvars.h as integer
 * @return Corresponding format ID
 */
extern int io_map_output_format_to_format_id(int output_format);

/**
 * @brief Detect format from file
 * 
 * @param filename File to examine
 * @return Pointer to the handler, or NULL if not detected
 */
extern struct io_interface *io_detect_format(const char *filename);

/**
 * @brief Get the last error code
 * 
 * @return Last error code
 */
extern int io_get_last_error();

/**
 * @brief Get the last error message
 * 
 * @return Last error message
 */
extern const char *io_get_error_message();

/**
 * @brief Set an error
 * 
 * @param error_code Error code
 * @param message Error message
 */
extern void io_set_error(int error_code, const char *message);

/**
 * @brief Clear the last error
 */
extern void io_clear_error();

/**
 * @brief Check if a format supports a capability
 * 
 * @param handler Handler to check
 * @param capability Capability to check for
 * @return true if supported, false otherwise
 */
extern bool io_has_capability(struct io_interface *handler, enum io_capabilities capability);

#ifdef __cplusplus
}
#endif