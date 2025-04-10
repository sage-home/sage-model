#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @file core_dynamic_library.h
 * @brief Cross-platform dynamic library loading for SAGE
 * 
 * This file provides a platform-independent API for loading dynamic libraries,
 * looking up symbols, and handling errors consistently across Windows, Linux, and macOS.
 */

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define DYNAMIC_LIBRARY_WINDOWS
#elif defined(__APPLE__)
    #define DYNAMIC_LIBRARY_APPLE
#elif defined(__linux__) || defined(__unix__)
    #define DYNAMIC_LIBRARY_LINUX
#else
    #error "Unsupported platform for dynamic library loading"
#endif

/* Maximum error message length */
#define MAX_DL_ERROR_LENGTH 256

/**
 * Opaque handle to a dynamic library
 */
typedef struct dynamic_library* dynamic_library_handle_t;

/**
 * Error codes for dynamic library operations
 */
typedef enum {
    DL_SUCCESS = 0,                       /* Operation succeeded */
    DL_ERROR_INVALID_ARGUMENT = -1,       /* Invalid parameter passed to function */
    DL_ERROR_FILE_NOT_FOUND = -2,         /* Library file not found */
    DL_ERROR_PERMISSION_DENIED = -3,      /* Permission denied accessing library */
    DL_ERROR_SYMBOL_NOT_FOUND = -4,       /* Symbol not found in library */
    DL_ERROR_INCOMPATIBLE_BINARY = -5,    /* Library binary incompatible with system */
    DL_ERROR_DEPENDENCY_MISSING = -6,     /* Library dependency missing */
    DL_ERROR_OUT_OF_MEMORY = -7,          /* Out of memory during operation */
    DL_ERROR_ALREADY_LOADED = -8,         /* Library already loaded (duplicate load attempt) */
    DL_ERROR_UNKNOWN = -9                 /* Unknown error occurred */
} dl_error_t;

/**
 * Initialize the dynamic library system
 * 
 * Sets up internal structures for tracking loaded libraries.
 * Must be called before any other dynamic library functions.
 * 
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_system_initialize(void);

/**
 * Clean up the dynamic library system
 * 
 * Releases resources used by the dynamic library system.
 * Any libraries still loaded will be forcibly unloaded.
 * 
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_system_cleanup(void);

/**
 * Load a dynamic library
 * 
 * Loads a dynamic library from the specified path. If the library is
 * already loaded, its reference count is incremented and the existing
 * handle is returned.
 * 
 * @param path Path to the library file
 * @param handle Output pointer for the library handle
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_open(const char *path, dynamic_library_handle_t *handle);

/**
 * Get a symbol from a dynamic library
 * 
 * Looks up a symbol (function or variable) in a loaded dynamic library.
 * 
 * @param handle Library handle
 * @param symbol_name Name of the symbol to look up
 * @param symbol Output pointer for the symbol
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_get_symbol(dynamic_library_handle_t handle, 
                                    const char *symbol_name, 
                                    void **symbol);

/**
 * Close a dynamic library
 * 
 * Decrements the reference count for a loaded library. If the reference
 * count reaches zero, the library is unloaded.
 * 
 * @param handle Library handle
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_close(dynamic_library_handle_t handle);

/**
 * Check if a dynamic library is already loaded
 * 
 * Determines if a library at the specified path is currently loaded.
 * 
 * @param path Path to the library file
 * @param is_loaded Output value indicating if the library is loaded
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_is_loaded(const char *path, bool *is_loaded);

/**
 * Get a handle to an already loaded library
 * 
 * Retrieves a handle to a library that has already been loaded.
 * Increments the reference count for the library.
 * 
 * @param path Path to the library file
 * @param handle Output pointer for the library handle
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_get_handle(const char *path, dynamic_library_handle_t *handle);

/**
 * Get the last error message
 * 
 * Retrieves the most recent error message from dynamic library operations.
 * 
 * @param error_buffer Buffer to store the error message
 * @param buffer_size Size of the error buffer
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_get_error(char *error_buffer, size_t buffer_size);

/**
 * Get a string representation of an error code
 * 
 * Converts a dynamic library error code to a human-readable string.
 * 
 * @param error Error code
 * @return String representation of the error
 */
const char *dynamic_library_error_string(dl_error_t error);

/**
 * Get the platform-specific last error message
 * 
 * Retrieves the raw platform-specific error message for the most recent error.
 * 
 * @param error_buffer Buffer to store the error message
 * @param buffer_size Size of the error buffer
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_get_platform_error(char *error_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
