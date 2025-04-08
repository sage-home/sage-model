#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
    #include <sys/mman.h>
#endif

#ifdef TESTING_STANDALONE
  #define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
  #define LOG_WARNING(fmt, ...) fprintf(stderr, "WARNING: " fmt "\n", ##__VA_ARGS__)
  #define LOG_DEBUG(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)
#else
  #include "../core/core_logging.h"
#endif

#include "io_memory_map.h"

/**
 * @file io_memory_map.c
 * @brief Implementation of memory mapping service
 */

// Error buffer for last error message
static char error_buffer[256] = {0};

// Platform-specific implementation of the mmap_region structure
struct mmap_region {
#ifdef _WIN32
    HANDLE file_handle;
    HANDLE mapping_handle;
#else
    int fd;
#endif
    void* data;              // Pointer to mapped memory (adjusted for alignment)
    void* aligned_base;      // Base address of the mapping (page-aligned)
    size_t size;             // Requested mapping size
    size_t aligned_size;     // Actual mapping size (adjusted for alignment)
    off_t offset;            // Requested offset
    off_t aligned_offset;    // Actual offset (page-aligned)
    off_t page_offset;       // Offset from aligned base to actual data
    enum mmap_access_mode mode;
    bool owns_fd;            // Whether to close fd during cleanup
};

/**
 * @brief Set the last error message
 *
 * @param format Format string (printf style)
 * @param ... Format arguments
 */
static void set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, sizeof(error_buffer), format, args);
    va_end(args);
    
    LOG_ERROR("%s", error_buffer);
}

/**
 * @brief Get the size of a file
 *
 * @param fd File descriptor
 * @return Size in bytes or -1 on error
 */
static off_t get_file_size(int fd) {
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        set_error("fstat failed: %s", strerror(errno));
        return -1;
    }
    return sb.st_size;
}

/**
 * @brief Check if memory mapping is supported on this platform
 *
 * @return true if supported, false otherwise
 */
bool mmap_is_available(void) {
#ifdef _WIN32
    return true;  // Windows supports memory mapping
#else
    return true;  // POSIX systems support memory mapping
#endif
}

/**
 * @brief Get the last error message
 *
 * @return Error message string
 */
const char* mmap_get_error(void) {
    return error_buffer;
}

/**
 * @brief Create default mapping options
 *
 * Initializes options with reasonable defaults.
 *
 * @return Default mapping options
 */
struct mmap_options mmap_default_options(void) {
    struct mmap_options options;
    options.mode = MMAP_READ_ONLY;
    options.mapping_size = 0;  // Map entire file
    options.offset = 0;        // Start from beginning
    return options;
}

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
struct mmap_region* mmap_file(const char* filename, int fd, struct mmap_options* options) {
    if (options == NULL) {
        set_error("NULL options passed to mmap_file");
        return NULL;
    }
    
    // Allocate region structure
    struct mmap_region* region = calloc(1, sizeof(struct mmap_region));
    if (region == NULL) {
        set_error("Failed to allocate memory for mmap_region");
        return NULL;
    }
    
    // Copy options
    region->mode = options->mode;
    region->offset = options->offset;
    
    // Open file if filename is provided but fd is invalid
    if (fd < 0 && filename != NULL) {
        // Open file based on access mode
#ifdef _WIN32
        DWORD access = GENERIC_READ;
        DWORD share_mode = FILE_SHARE_READ;
        
        region->file_handle = CreateFile(
            filename,
            access,
            share_mode,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (region->file_handle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            set_error("Failed to open file '%s': error %lu", filename, error);
            free(region);
            return NULL;
        }
        
        region->owns_fd = true;
#else
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            set_error("Failed to open file '%s': %s", filename, strerror(errno));
            free(region);
            return NULL;
        }
        
        region->fd = fd;
        region->owns_fd = true;
#endif
    } else {
        // Use provided file descriptor
#ifdef _WIN32
        // Convert fd to a Windows handle
        region->file_handle = (HANDLE)_get_osfhandle(fd);
        if (region->file_handle == INVALID_HANDLE_VALUE) {
            set_error("Invalid file handle from descriptor %d", fd);
            free(region);
            return NULL;
        }
#else
        region->fd = fd;
#endif
        region->owns_fd = false;
    }
    
    // Determine file size
    off_t file_size;
#ifdef _WIN32
    LARGE_INTEGER li_size;
    if (!GetFileSizeEx(region->file_handle, &li_size)) {
        DWORD error = GetLastError();
        set_error("Failed to get file size: error %lu", error);
        if (region->owns_fd) {
            CloseHandle(region->file_handle);
        }
        free(region);
        return NULL;
    }
    file_size = li_size.QuadPart;
#else
    file_size = get_file_size(fd);
    if (file_size < 0) {
        if (region->owns_fd) {
            close(region->fd);
        }
        free(region);
        return NULL;
    }
#endif
    
    // Determine mapping size
    size_t mapping_size = options->mapping_size;
    if (mapping_size == 0 || mapping_size > (size_t)(file_size - region->offset)) {
        mapping_size = (size_t)(file_size - region->offset);
    }
    
    if (mapping_size == 0) {
        set_error("Zero mapping size - file may be empty");
#ifdef _WIN32
        if (region->owns_fd) {
            CloseHandle(region->file_handle);
        }
#else
        if (region->owns_fd) {
            close(region->fd);
        }
#endif
        free(region);
        return NULL;
    }
    
    region->size = mapping_size;
    
    // Create the mapping
#ifdef _WIN32
    // In Windows, the offset must be a multiple of the system allocation granularity
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    DWORD allocation_granularity = system_info.dwAllocationGranularity;
    
    // Adjust offset to be a multiple of the allocation granularity
    region->page_offset = region->offset % allocation_granularity;
    region->aligned_offset = region->offset - region->page_offset;
    region->aligned_size = mapping_size + region->page_offset;
    
    // Create file mapping
    DWORD protect = PAGE_READONLY;
    region->mapping_handle = CreateFileMapping(
        region->file_handle,
        NULL,
        protect,
        0,
        0,
        NULL
    );
    
    if (region->mapping_handle == NULL) {
        DWORD error = GetLastError();
        set_error("Failed to create file mapping: error %lu", error);
        if (region->owns_fd) {
            CloseHandle(region->file_handle);
        }
        free(region);
        return NULL;
    }
    
    // Map view of file
    DWORD access = FILE_MAP_READ;
    DWORD offset_high = (DWORD)((region->aligned_offset >> 32) & 0xFFFFFFFF);
    DWORD offset_low = (DWORD)(region->aligned_offset & 0xFFFFFFFF);
    
    region->aligned_base = MapViewOfFile(
        region->mapping_handle,
        access,
        offset_high,
        offset_low,
        region->aligned_size
    );
    
    if (region->aligned_base == NULL) {
        DWORD error = GetLastError();
        set_error("Failed to map view of file: error %lu", error);
        CloseHandle(region->mapping_handle);
        if (region->owns_fd) {
            CloseHandle(region->file_handle);
        }
        free(region);
        return NULL;
    }
    
    // Adjust pointer to account for page offset
    region->data = (char*)region->aligned_base + region->page_offset;
#else
    // In POSIX, the offset must be a multiple of the page size
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096; // Default to 4KB if sysconf fails
    }
    
    // Adjust offset to be a multiple of the page size
    region->page_offset = region->offset % page_size;
    region->aligned_offset = region->offset - region->page_offset;
    region->aligned_size = mapping_size + region->page_offset;
    
    // Create POSIX mapping
    int prot = PROT_READ;
    int flags = MAP_PRIVATE;
    
    region->aligned_base = mmap(
        NULL,
        region->aligned_size,
        prot,
        flags,
        region->fd,
        region->aligned_offset
    );
    
    if (region->aligned_base == MAP_FAILED) {
        set_error("Failed to create memory mapping: %s", strerror(errno));
        if (region->owns_fd) {
            close(region->fd);
        }
        free(region);
        return NULL;
    }
    
    // Adjust pointer to account for page offset
    region->data = (char*)region->aligned_base + region->page_offset;
#endif
    
    return region;
}

/**
 * @brief Get a pointer to the mapped memory
 *
 * @param region Memory mapping region
 * @return Pointer to mapped memory or NULL on error
 */
void* mmap_get_pointer(struct mmap_region* region) {
    if (region == NULL) {
        set_error("NULL region passed to mmap_get_pointer");
        return NULL;
    }
    
    return region->data;
}

/**
 * @brief Get the size of the mapped memory
 *
 * @param region Memory mapping region
 * @return Size of the mapped memory in bytes or 0 on error
 */
size_t mmap_get_size(struct mmap_region* region) {
    if (region == NULL) {
        set_error("NULL region passed to mmap_get_size");
        return 0;
    }
    
    return region->size;
}

/**
 * @brief Unmap a memory mapping
 *
 * Releases resources associated with a memory mapping.
 *
 * @param region Memory mapping region to unmap
 * @return 0 on success, non-zero on failure
 */
int mmap_unmap(struct mmap_region* region) {
    if (region == NULL) {
        set_error("NULL region passed to mmap_unmap");
        return -1;
    }
    
    int result = 0;
    
#ifdef _WIN32
    // Unmap view of file
    if (region->aligned_base != NULL) {
        if (!UnmapViewOfFile(region->aligned_base)) {
            DWORD error = GetLastError();
            set_error("Failed to unmap view of file: error %lu", error);
            result = -1;
        }
    }
    
    // Close mapping handle
    if (region->mapping_handle != NULL) {
        if (!CloseHandle(region->mapping_handle)) {
            DWORD error = GetLastError();
            set_error("Failed to close mapping handle: error %lu", error);
            result = -1;
        }
    }
    
    // Close file handle if we own it
    if (region->owns_fd && region->file_handle != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(region->file_handle)) {
            DWORD error = GetLastError();
            set_error("Failed to close file handle: error %lu", error);
            result = -1;
        }
    }
#else
    // Unmap memory
    if (region->aligned_base != NULL && region->aligned_base != MAP_FAILED) {
        if (munmap(region->aligned_base, region->aligned_size) != 0) {
            set_error("Failed to unmap memory: %s", strerror(errno));
            result = -1;
        }
    }
    
    // Close file descriptor if we own it
    if (region->owns_fd && region->fd >= 0) {
        if (close(region->fd) != 0) {
            set_error("Failed to close file descriptor: %s", strerror(errno));
            result = -1;
        }
    }
#endif
    
    // Free region structure
    free(region);
    
    return result;
}
