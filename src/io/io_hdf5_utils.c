#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io_hdf5_utils.h"
#include "io_interface.h"

#ifdef HDF5

/**
 * @brief Maximum number of HDF5 handles that can be tracked
 */
#define MAX_HDF5_HANDLES 1024

/**
 * @brief Maximum length of a source file name
 */
#define MAX_FILENAME_LEN 256

/**
 * @brief HDF5 handle tracking entry
 */
struct hdf5_handle_entry {
    hid_t handle;               /**< HDF5 handle */
    enum hdf5_handle_type type; /**< Type of handle */
    char file[MAX_FILENAME_LEN]; /**< Source file where handle was created */
    int line;                   /**< Source line where handle was created */
    int active;                 /**< Whether this entry is active */
};

/**
 * @brief Array of handle tracking entries
 */
static struct hdf5_handle_entry handle_entries[MAX_HDF5_HANDLES];

/**
 * @brief Number of active handle entries
 */
static int num_active_entries = 0;

/**
 * @brief Initialized flag
 */
static int initialized = 0;

/**
 * @brief Initialize the HDF5 handle tracking system
 * 
 * Must be called before any other HDF5 utilities.
 * 
 * @return 0 on success, non-zero on failure
 */
int hdf5_tracking_init() {
    if (initialized) {
        return 0;  // Already initialized
    }
    
    // Clear all entries
    memset(handle_entries, 0, sizeof(handle_entries));
    for (int i = 0; i < MAX_HDF5_HANDLES; i++) {
        handle_entries[i].handle = -1;  // Invalid handle
        handle_entries[i].active = 0;   // Not active
    }
    
    num_active_entries = 0;
    initialized = 1;
    
    return 0;
}

/**
 * @brief Clean up the HDF5 handle tracking system
 * 
 * Closes any remaining open handles and frees resources.
 * 
 * @return 0 on success, non-zero if some handles couldn't be closed
 */
int hdf5_tracking_cleanup() {
    if (!initialized) {
        return 0;  // Not initialized
    }
    
    // Close any remaining handles
    int result = hdf5_close_all_handles();
    
    // Reset state
    num_active_entries = 0;
    initialized = 0;
    
    return result;
}

/**
 * @brief Find a free entry in the handle array
 * 
 * @return Index of a free entry, or -1 if full
 */
static int find_free_entry() {
    for (int i = 0; i < MAX_HDF5_HANDLES; i++) {
        if (!handle_entries[i].active) {
            return i;
        }
    }
    return -1;  // No free entries
}

/**
 * @brief Find an entry for a specific handle
 * 
 * @param handle Handle to find
 * @return Index of the entry, or -1 if not found
 */
static int find_handle_entry(hid_t handle) {
    for (int i = 0; i < MAX_HDF5_HANDLES; i++) {
        if (handle_entries[i].active && handle_entries[i].handle == handle) {
            return i;
        }
    }
    return -1;  // Handle not found
}

/**
 * @brief Track an HDF5 handle
 * 
 * Registers a handle for tracking. This should be called whenever
 * a new HDF5 handle is created.
 * 
 * @param handle The HDF5 handle to track
 * @param type The type of handle
 * @param file Source file (use __FILE__)
 * @param line Source line (use __LINE__)
 * @return 0 on success, non-zero on failure
 */
int hdf5_track_handle(hid_t handle, enum hdf5_handle_type type, 
                      const char *file, int line) {
    if (!initialized) {
        fprintf(stderr, "HDF5 tracking not initialized\n");
        return -1;
    }
    
    // Don't track invalid handles
    if (handle < 0) {
        return 0;
    }
    
    // Find a free entry
    int entry_idx = find_free_entry();
    if (entry_idx < 0) {
        io_set_error(IO_ERROR_RESOURCE_LIMIT, "Maximum number of HDF5 handles reached");
        return -1;
    }
    
    // Store handle information
    handle_entries[entry_idx].handle = handle;
    handle_entries[entry_idx].type = type;
    handle_entries[entry_idx].line = line;
    handle_entries[entry_idx].active = 1;
    
    // Copy file name with truncation if necessary
    if (file != NULL) {
        strncpy(handle_entries[entry_idx].file, file, MAX_FILENAME_LEN - 1);
        handle_entries[entry_idx].file[MAX_FILENAME_LEN - 1] = '\0';
    } else {
        strcpy(handle_entries[entry_idx].file, "unknown");
    }
    
    num_active_entries++;
    return 0;
}

/**
 * @brief Stop tracking an HDF5 handle
 * 
 * Removes a handle from tracking. This should be called after
 * a handle is closed.
 * 
 * @param handle The HDF5 handle to untrack
 * @return 0 on success, non-zero if handle wasn't found
 */
int hdf5_untrack_handle(hid_t handle) {
    if (!initialized) {
        fprintf(stderr, "HDF5 tracking not initialized\n");
        return -1;
    }
    
    // Find the handle
    int entry_idx = find_handle_entry(handle);
    if (entry_idx < 0) {
        // Handle not found, but not necessarily an error
        // (could have been closed through other means)
        return 0;
    }
    
    // Mark entry as inactive
    handle_entries[entry_idx].active = 0;
    num_active_entries--;
    
    return 0;
}

/**
 * @brief Type-specific close function
 * 
 * @param entry Handle entry to close
 * @return 0 on success, HDF5 error code on failure
 */
static herr_t close_handle_by_type(struct hdf5_handle_entry *entry) {
    herr_t status = 0;
    
    switch (entry->type) {
        case HDF5_HANDLE_FILE:
            status = H5Fclose(entry->handle);
            break;
        case HDF5_HANDLE_GROUP:
            status = H5Gclose(entry->handle);
            break;
        case HDF5_HANDLE_DATASET:
            status = H5Dclose(entry->handle);
            break;
        case HDF5_HANDLE_DATASPACE:
            status = H5Sclose(entry->handle);
            break;
        case HDF5_HANDLE_DATATYPE:
            status = H5Tclose(entry->handle);
            break;
        case HDF5_HANDLE_ATTRIBUTE:
            status = H5Aclose(entry->handle);
            break;
        case HDF5_HANDLE_PROPERTY:
            status = H5Pclose(entry->handle);
            break;
        default:
            fprintf(stderr, "Unknown HDF5 handle type: %d\n", entry->type);
            return -1;
    }
    
    return status;
}

/**
 * @brief Testing mode flag
 * Set to 1 to skip actual handle closing (for testing with mock handles)
 */
static int testing_mode = 0;

/**
 * @brief Set testing mode
 *
 * @param mode 1 to enable testing mode, 0 to disable
 */
void hdf5_set_testing_mode(int mode) {
    testing_mode = mode;
}

/**
 * @brief Close all handles of a specific type
 * 
 * @param type Type of handles to close
 * @return 0 on success, non-zero if some handles couldn't be closed
 */
static int close_handles_of_type(enum hdf5_handle_type type) {
    int result = 0;
    
    for (int i = 0; i < MAX_HDF5_HANDLES; i++) {
        if (handle_entries[i].active && handle_entries[i].type == type) {
            if (testing_mode) {
                // In testing mode, just mark as closed without actually closing
                handle_entries[i].active = 0;
                num_active_entries--;
            } else {
                // Actually close the handle
                herr_t status = close_handle_by_type(&handle_entries[i]);
                if (status < 0) {
                    fprintf(stderr, "Error closing HDF5 handle %ld of type %d from %s:%d\n",
                            (long)handle_entries[i].handle, type,
                            handle_entries[i].file, handle_entries[i].line);
                    result = -1;
                } else {
                    handle_entries[i].active = 0;
                    num_active_entries--;
                }
            }
        }
    }
    
    return result;
}

/**
 * @brief Close all tracked HDF5 handles
 * 
 * Attempts to close all tracked handles in the correct order
 * (children before parents).
 * 
 * @return 0 on success, non-zero if some handles couldn't be closed
 */
int hdf5_close_all_handles() {
    if (!initialized) {
        fprintf(stderr, "HDF5 tracking not initialized\n");
        return -1;
    }
    
    int result = 0;
    
    // Close in the correct order (children before parents)
    // 1. Attributes
    result |= close_handles_of_type(HDF5_HANDLE_ATTRIBUTE);
    
    // 2. Datasets
    result |= close_handles_of_type(HDF5_HANDLE_DATASET);
    
    // 3. Dataspaces
    result |= close_handles_of_type(HDF5_HANDLE_DATASPACE);
    
    // 4. Datatypes
    result |= close_handles_of_type(HDF5_HANDLE_DATATYPE);
    
    // 5. Groups
    result |= close_handles_of_type(HDF5_HANDLE_GROUP);
    
    // 6. Property lists
    result |= close_handles_of_type(HDF5_HANDLE_PROPERTY);
    
    // 7. Files (last)
    result |= close_handles_of_type(HDF5_HANDLE_FILE);
    
    return result;
}

/**
 * @brief Get the number of currently tracked handles
 * 
 * @return The number of tracked handles
 */
int hdf5_get_open_handle_count() {
    if (!initialized) {
        fprintf(stderr, "HDF5 tracking not initialized\n");
        return -1;
    }
    
    return num_active_entries;
}

/**
 * @brief Get string representation of handle type
 * 
 * @param type Handle type
 * @return String representation
 */
static const char *get_type_string(enum hdf5_handle_type type) {
    switch (type) {
        case HDF5_HANDLE_FILE: return "File";
        case HDF5_HANDLE_GROUP: return "Group";
        case HDF5_HANDLE_DATASET: return "Dataset";
        case HDF5_HANDLE_DATASPACE: return "Dataspace";
        case HDF5_HANDLE_DATATYPE: return "Datatype";
        case HDF5_HANDLE_ATTRIBUTE: return "Attribute";
        case HDF5_HANDLE_PROPERTY: return "Property";
        default: return "Unknown";
    }
}

/**
 * @brief Print information about currently tracked handles
 * 
 * @return 0 on success
 */
int hdf5_print_open_handles() {
    if (!initialized) {
        fprintf(stderr, "HDF5 tracking not initialized\n");
        return -1;
    }
    
    printf("Open HDF5 handles (%d):\n", num_active_entries);
    
    if (num_active_entries == 0) {
        printf("  No open handles.\n");
        return 0;
    }
    
    for (int i = 0, count = 0; i < MAX_HDF5_HANDLES; i++) {
        if (handle_entries[i].active) {
            printf("  [%d] Type: %s, Handle: %ld, Created at: %s:%d\n",
                   count++, get_type_string(handle_entries[i].type),
                   (long)handle_entries[i].handle,
                   handle_entries[i].file, handle_entries[i].line);
        }
    }
    
    return 0;
}

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a file handle if it's valid and unregisters it.
 * 
 * @param file_id Pointer to the file handle
 * @return 0 on success, HDF5 error code on failure
 */
herr_t hdf5_check_and_close_file(hid_t *file_id) {
    herr_t status = 0;
    if (file_id != NULL && *file_id >= 0) {
        status = H5Fclose(*file_id);
        if (status >= 0) {
            hdf5_untrack_handle(*file_id);
            *file_id = -1;
        }
    }
    return status;
}

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a group handle if it's valid and unregisters it.
 * 
 * @param group_id Pointer to the group handle
 * @return 0 on success, HDF5 error code on failure
 */
herr_t hdf5_check_and_close_group(hid_t *group_id) {
    herr_t status = 0;
    if (group_id != NULL && *group_id >= 0) {
        status = H5Gclose(*group_id);
        if (status >= 0) {
            hdf5_untrack_handle(*group_id);
            *group_id = -1;
        }
    }
    return status;
}

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a dataset handle if it's valid and unregisters it.
 * 
 * @param dataset_id Pointer to the dataset handle
 * @return 0 on success, HDF5 error code on failure
 */
herr_t hdf5_check_and_close_dataset(hid_t *dataset_id) {
    herr_t status = 0;
    if (dataset_id != NULL && *dataset_id >= 0) {
        status = H5Dclose(*dataset_id);
        if (status >= 0) {
            hdf5_untrack_handle(*dataset_id);
            *dataset_id = -1;
        }
    }
    return status;
}

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a dataspace handle if it's valid and unregisters it.
 * 
 * @param dataspace_id Pointer to the dataspace handle
 * @return 0 on success, HDF5 error code on failure
 */
herr_t hdf5_check_and_close_dataspace(hid_t *dataspace_id) {
    herr_t status = 0;
    if (dataspace_id != NULL && *dataspace_id >= 0) {
        status = H5Sclose(*dataspace_id);
        if (status >= 0) {
            hdf5_untrack_handle(*dataspace_id);
            *dataspace_id = -1;
        }
    }
    return status;
}

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a datatype handle if it's valid and unregisters it.
 * 
 * @param datatype_id Pointer to the datatype handle
 * @return 0 on success, HDF5 error code on failure
 */
herr_t hdf5_check_and_close_datatype(hid_t *datatype_id) {
    herr_t status = 0;
    if (datatype_id != NULL && *datatype_id >= 0) {
        status = H5Tclose(*datatype_id);
        if (status >= 0) {
            hdf5_untrack_handle(*datatype_id);
            *datatype_id = -1;
        }
    }
    return status;
}

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes an attribute handle if it's valid and unregisters it.
 * 
 * @param attribute_id Pointer to the attribute handle
 * @return 0 on success, HDF5 error code on failure
 */
herr_t hdf5_check_and_close_attribute(hid_t *attribute_id) {
    herr_t status = 0;
    if (attribute_id != NULL && *attribute_id >= 0) {
        status = H5Aclose(*attribute_id);
        if (status >= 0) {
            hdf5_untrack_handle(*attribute_id);
            *attribute_id = -1;
        }
    }
    return status;
}

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a property list handle if it's valid and unregisters it.
 * 
 * @param property_id Pointer to the property list handle
 * @return 0 on success, HDF5 error code on failure
 */
herr_t hdf5_check_and_close_property(hid_t *property_id) {
    herr_t status = 0;
    if (property_id != NULL && *property_id >= 0) {
        status = H5Pclose(*property_id);
        if (status >= 0) {
            hdf5_untrack_handle(*property_id);
            *property_id = -1;
        }
    }
    return status;
}

#endif // HDF5