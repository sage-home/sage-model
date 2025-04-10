#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include "core_dynamic_library.h"
#include "core_logging.h"
#include "core_mymalloc.h"

/* Platform-specific includes */
#ifdef DYNAMIC_LIBRARY_WINDOWS
    #include <windows.h>
#else
    #include <dlfcn.h>
    #include <errno.h>
#endif

/* Define PATH_MAX if not already defined */
#ifndef PATH_MAX
    #ifdef DYNAMIC_LIBRARY_WINDOWS
        #define PATH_MAX MAX_PATH
    #else
        #define PATH_MAX 4096
    #endif
#endif

/* Static variables */
static char last_error_message[MAX_DL_ERROR_LENGTH] = {0};

/* Maximum number of simultaneously loaded libraries */
#define MAX_LOADED_LIBRARIES 64

/* Structure to track loaded libraries */
struct dynamic_library {
    #ifdef DYNAMIC_LIBRARY_WINDOWS
        HMODULE handle;
    #else
        void *handle;
    #endif
    char path[PATH_MAX];
    int ref_count;
    bool valid;
};

/* Library tracking registry */
static struct {
    struct dynamic_library libraries[MAX_LOADED_LIBRARIES];
    int count;
    bool initialized;
} library_registry = {0};

/* Internal error setting function */
static void set_error_message(const char *message) {
    if (message) {
        strncpy(last_error_message, message, MAX_DL_ERROR_LENGTH - 1);
        last_error_message[MAX_DL_ERROR_LENGTH - 1] = '\0';
    } else {
        last_error_message[0] = '\0';
    }
}

/* Platform-specific error string retrieval */
static void get_platform_specific_error(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return;
    }
    
    #ifdef DYNAMIC_LIBRARY_WINDOWS
        DWORD error_code = GetLastError();
        if (error_code == 0) {
            strncpy(buffer, "No error", buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return;
        }
        
        LPSTR message_buffer = NULL;
        DWORD message_length = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&message_buffer,
            0,
            NULL
        );
        
        if (message_buffer) {
            /* Copy the formatted message to the output buffer */
            strncpy(buffer, message_buffer, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            
            /* Trim trailing whitespace and newlines */
            char *end = buffer + strlen(buffer) - 1;
            while (end > buffer && (*end == '\r' || *end == '\n' || *end == ' ')) {
                *end = '\0';
                end--;
            }
            
            /* Free the Windows-allocated message buffer */
            LocalFree(message_buffer);
        } else {
            snprintf(buffer, buffer_size, "Windows error code: %lu", error_code);
        }
    #else
        /* For POSIX systems (Linux, macOS), use dlerror() */
        const char *error = dlerror();
        if (error) {
            strncpy(buffer, error, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
        } else {
            strncpy(buffer, "No error", buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
        }
    #endif
}

/* Find library index by path */
static int find_library_by_path(const char *path) {
    for (int i = 0; i < library_registry.count; i++) {
        if (library_registry.libraries[i].valid && 
            strcmp(library_registry.libraries[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

/* Find first free library slot */
static int find_free_library_slot(void) {
    /* First try to find an invalid slot */
    for (int i = 0; i < library_registry.count; i++) {
        if (!library_registry.libraries[i].valid) {
            return i;
        }
    }
    
    /* If no invalid slots, add a new one if possible */
    if (library_registry.count < MAX_LOADED_LIBRARIES) {
        return library_registry.count++;
    }
    
    /* No free slots */
    return -1;
}

/**
 * Initialize the dynamic library system
 * 
 * Sets up internal structures for tracking loaded libraries.
 * Must be called before any other dynamic library functions.
 * 
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_system_initialize(void) {
    if (library_registry.initialized) {
        LOG_DEBUG("Dynamic library system already initialized");
        return DL_SUCCESS;
    }
    
    /* Initialize the registry */
    memset(&library_registry, 0, sizeof(library_registry));
    library_registry.initialized = true;
    
    LOG_INFO("Dynamic library system initialized");
    return DL_SUCCESS;
}

/**
 * Clean up the dynamic library system
 * 
 * Releases resources used by the dynamic library system.
 * Any libraries still loaded will be forcibly unloaded.
 * 
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_system_cleanup(void) {
    if (!library_registry.initialized) {
        LOG_DEBUG("Dynamic library system not initialized");
        return DL_SUCCESS;
    }
    
    /* Unload any remaining libraries */
    for (int i = 0; i < library_registry.count; i++) {
        if (library_registry.libraries[i].valid) {
            LOG_DEBUG("Forcibly unloading library: %s", library_registry.libraries[i].path);
            
            #ifdef DYNAMIC_LIBRARY_WINDOWS
                FreeLibrary(library_registry.libraries[i].handle);
            #else
                dlclose(library_registry.libraries[i].handle);
            #endif
            
            library_registry.libraries[i].valid = false;
        }
    }
    
    /* Reset the registry */
    memset(&library_registry, 0, sizeof(library_registry));
    
    LOG_INFO("Dynamic library system cleaned up");
    return DL_SUCCESS;
}

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
dl_error_t dynamic_library_open(const char *path, dynamic_library_handle_t *handle) {
    /* Parameter validation */
    if (!path || !handle) {
        set_error_message("Invalid arguments to dynamic_library_open");
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Ensure system is initialized */
    if (!library_registry.initialized) {
        dl_error_t error = dynamic_library_system_initialize();
        if (error != DL_SUCCESS) {
            return error;
        }
    }
    
    /* Check if library is already loaded */
    int lib_index = find_library_by_path(path);
    if (lib_index >= 0) {
        /* Increment reference count and return existing handle */
        library_registry.libraries[lib_index].ref_count++;
        *handle = &library_registry.libraries[lib_index];
        LOG_DEBUG("Reusing already loaded library: %s (ref count: %d)", 
                 path, library_registry.libraries[lib_index].ref_count);
        return DL_SUCCESS;
    }
    
    /* Find a free slot */
    lib_index = find_free_library_slot();
    if (lib_index < 0) {
        set_error_message("Maximum number of loaded libraries reached");
        return DL_ERROR_OUT_OF_MEMORY;
    }
    
    /* Load the library */
    #ifdef DYNAMIC_LIBRARY_WINDOWS
        HMODULE lib_handle = LoadLibraryA(path);
        if (!lib_handle) {
            char error_buffer[MAX_DL_ERROR_LENGTH];
            get_platform_specific_error(error_buffer, sizeof(error_buffer));
            set_error_message(error_buffer);
            
            DWORD error_code = GetLastError();
            
            /* Map Windows error codes to our error codes */
            if (error_code == ERROR_FILE_NOT_FOUND || error_code == ERROR_PATH_NOT_FOUND) {
                return DL_ERROR_FILE_NOT_FOUND;
            } else if (error_code == ERROR_ACCESS_DENIED) {
                return DL_ERROR_PERMISSION_DENIED;
            } else if (error_code == ERROR_NOT_ENOUGH_MEMORY) {
                return DL_ERROR_OUT_OF_MEMORY;
            } else if (error_code == ERROR_BAD_EXE_FORMAT) {
                return DL_ERROR_INCOMPATIBLE_BINARY;
            } else if (error_code == ERROR_DLL_NOT_FOUND) {
                return DL_ERROR_DEPENDENCY_MISSING;
            } else {
                return DL_ERROR_UNKNOWN;
            }
        }
    #else
        /* Clear any existing error */
        dlerror();
        
        /* Load the library */
        void *lib_handle = dlopen(path, RTLD_NOW);
        if (!lib_handle) {
            char error_buffer[MAX_DL_ERROR_LENGTH];
            get_platform_specific_error(error_buffer, sizeof(error_buffer));
            set_error_message(error_buffer);
            
            /* Try to determine more specific error based on the error message */
            if (strstr(error_buffer, "No such file") || 
                strstr(error_buffer, "cannot open shared object file")) {
                return DL_ERROR_FILE_NOT_FOUND;
            } else if (strstr(error_buffer, "Permission denied")) {
                return DL_ERROR_PERMISSION_DENIED;
            } else if (strstr(error_buffer, "Cannot allocate memory")) {
                return DL_ERROR_OUT_OF_MEMORY;
            } else if (strstr(error_buffer, "invalid ELF") || 
                       strstr(error_buffer, "wrong ELF class") ||
                       strstr(error_buffer, "mach-o, but wrong architecture")) {
                return DL_ERROR_INCOMPATIBLE_BINARY;
            } else if (strstr(error_buffer, "undefined symbol") ||
                      strstr(error_buffer, "dependent lib")) {
                return DL_ERROR_DEPENDENCY_MISSING;
            } else {
                return DL_ERROR_UNKNOWN;
            }
        }
    #endif
    
    /* Store library information */
    struct dynamic_library *lib = &library_registry.libraries[lib_index];
    lib->handle = lib_handle;
    strncpy(lib->path, path, PATH_MAX - 1);
    lib->path[PATH_MAX - 1] = '\0';
    lib->ref_count = 1;
    lib->valid = true;
    
    /* Return the handle */
    *handle = lib;
    
    LOG_DEBUG("Successfully loaded library: %s", path);
    return DL_SUCCESS;
}

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
                                    void **symbol) {
    /* Parameter validation */
    if (!handle || !symbol_name || !symbol) {
        set_error_message("Invalid arguments to dynamic_library_get_symbol");
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Validate handle */
    if (!handle->valid) {
        set_error_message("Invalid library handle");
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Clear error state */
    #ifndef DYNAMIC_LIBRARY_WINDOWS
        dlerror();
    #endif
    
    /* Look up the symbol */
    #ifdef DYNAMIC_LIBRARY_WINDOWS
        *symbol = (void *)GetProcAddress(handle->handle, symbol_name);
        if (!*symbol) {
            char error_buffer[MAX_DL_ERROR_LENGTH];
            get_platform_specific_error(error_buffer, sizeof(error_buffer));
            snprintf(last_error_message, MAX_DL_ERROR_LENGTH, 
                    "Symbol '%s' not found: %s", symbol_name, error_buffer);
            return DL_ERROR_SYMBOL_NOT_FOUND;
        }
    #else
        *symbol = dlsym(handle->handle, symbol_name);
        
        /* Check for error */
        const char *error = dlerror();
        if (error) {
            snprintf(last_error_message, MAX_DL_ERROR_LENGTH, 
                    "Symbol '%s' not found: %s", symbol_name, error);
            return DL_ERROR_SYMBOL_NOT_FOUND;
        }
        
        /* Some platforms may not set an error but return NULL */
        if (!*symbol) {
            snprintf(last_error_message, MAX_DL_ERROR_LENGTH, 
                    "Symbol '%s' not found or NULL", symbol_name);
            return DL_ERROR_SYMBOL_NOT_FOUND;
        }
    #endif
    
    return DL_SUCCESS;
}

/**
 * Close a dynamic library
 * 
 * Decrements the reference count for a loaded library. If the reference
 * count reaches zero, the library is unloaded.
 * 
 * @param handle Library handle
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_close(dynamic_library_handle_t handle) {
    /* Parameter validation */
    if (!handle) {
        set_error_message("Invalid arguments to dynamic_library_close");
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Validate handle */
    if (!handle->valid) {
        set_error_message("Invalid library handle");
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Decrement reference count */
    handle->ref_count--;
    
    LOG_DEBUG("Decremented reference count for library: %s (new count: %d)", 
             handle->path, handle->ref_count);
    
    /* If reference count reaches zero, unload the library */
    if (handle->ref_count <= 0) {
        LOG_DEBUG("Unloading library: %s", handle->path);
        
        /* Unload the library */
        #ifdef DYNAMIC_LIBRARY_WINDOWS
            if (!FreeLibrary(handle->handle)) {
                char error_buffer[MAX_DL_ERROR_LENGTH];
                get_platform_specific_error(error_buffer, sizeof(error_buffer));
                set_error_message(error_buffer);
                return DL_ERROR_UNKNOWN;
            }
        #else
            if (dlclose(handle->handle) != 0) {
                char error_buffer[MAX_DL_ERROR_LENGTH];
                get_platform_specific_error(error_buffer, sizeof(error_buffer));
                set_error_message(error_buffer);
                return DL_ERROR_UNKNOWN;
            }
        #endif
        
        /* Mark the slot as invalid */
        handle->valid = false;
    }
    
    return DL_SUCCESS;
}

/**
 * Check if a dynamic library is already loaded
 * 
 * Determines if a library at the specified path is currently loaded.
 * 
 * @param path Path to the library file
 * @param is_loaded Output value indicating if the library is loaded
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_is_loaded(const char *path, bool *is_loaded) {
    /* Parameter validation */
    if (!path || !is_loaded) {
        set_error_message("Invalid arguments to dynamic_library_is_loaded");
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Ensure system is initialized */
    if (!library_registry.initialized) {
        *is_loaded = false;
        return DL_SUCCESS;
    }
    
    /* Check if library is loaded */
    int lib_index = find_library_by_path(path);
    *is_loaded = (lib_index >= 0);
    
    return DL_SUCCESS;
}

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
dl_error_t dynamic_library_get_handle(const char *path, dynamic_library_handle_t *handle) {
    /* Parameter validation */
    if (!path || !handle) {
        set_error_message("Invalid arguments to dynamic_library_get_handle");
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Ensure system is initialized */
    if (!library_registry.initialized) {
        set_error_message("Dynamic library system not initialized");
        return DL_ERROR_UNKNOWN;
    }
    
    /* Find the library */
    int lib_index = find_library_by_path(path);
    if (lib_index < 0) {
        snprintf(last_error_message, MAX_DL_ERROR_LENGTH, 
                "Library not loaded: %s", path);
        return DL_ERROR_FILE_NOT_FOUND;
    }
    
    /* Increment reference count and return handle */
    library_registry.libraries[lib_index].ref_count++;
    *handle = &library_registry.libraries[lib_index];
    
    return DL_SUCCESS;
}

/**
 * Get the last error message
 * 
 * Retrieves the most recent error message from dynamic library operations.
 * 
 * @param error_buffer Buffer to store the error message
 * @param buffer_size Size of the error buffer
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_get_error(char *error_buffer, size_t buffer_size) {
    /* Parameter validation */
    if (!error_buffer || buffer_size == 0) {
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Copy the error message */
    strncpy(error_buffer, last_error_message, buffer_size - 1);
    error_buffer[buffer_size - 1] = '\0';
    
    return DL_SUCCESS;
}

/**
 * Get a string representation of an error code
 * 
 * Converts a dynamic library error code to a human-readable string.
 * 
 * @param error Error code
 * @return String representation of the error
 */
const char *dynamic_library_error_string(dl_error_t error) {
    switch (error) {
        case DL_SUCCESS:
            return "Success";
        case DL_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";
        case DL_ERROR_FILE_NOT_FOUND:
            return "File not found";
        case DL_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case DL_ERROR_SYMBOL_NOT_FOUND:
            return "Symbol not found";
        case DL_ERROR_INCOMPATIBLE_BINARY:
            return "Incompatible binary format";
        case DL_ERROR_DEPENDENCY_MISSING:
            return "Dependency missing";
        case DL_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case DL_ERROR_ALREADY_LOADED:
            return "Library already loaded";
        case DL_ERROR_UNKNOWN:
            return "Unknown error";
        default:
            return "Invalid error code";
    }
}

/**
 * Get the platform-specific last error message
 * 
 * Retrieves the raw platform-specific error message for the most recent error.
 * 
 * @param error_buffer Buffer to store the error message
 * @param buffer_size Size of the error buffer
 * @return DL_SUCCESS on success, error code on failure
 */
dl_error_t dynamic_library_get_platform_error(char *error_buffer, size_t buffer_size) {
    /* Parameter validation */
    if (!error_buffer || buffer_size == 0) {
        return DL_ERROR_INVALID_ARGUMENT;
    }
    
    /* Get the platform-specific error */
    get_platform_specific_error(error_buffer, buffer_size);
    
    return DL_SUCCESS;
}
