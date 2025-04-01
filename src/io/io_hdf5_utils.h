#pragma once

/**
 * @file io_hdf5_utils.h
 * @brief Utilities for tracking and managing HDF5 resources
 *
 * This file provides functionality for tracking HDF5 handles to prevent resource
 * leaks, which have been a common issue in the codebase. It includes utilities for
 * registering, tracking, and properly closing various types of HDF5 handles.
 */

#ifdef HDF5

#include <hdf5.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Types of HDF5 handles that can be tracked
 */
enum hdf5_handle_type {
    HDF5_HANDLE_FILE,      /**< File handle (H5F) */
    HDF5_HANDLE_GROUP,     /**< Group handle (H5G) */
    HDF5_HANDLE_DATASET,   /**< Dataset handle (H5D) */
    HDF5_HANDLE_DATASPACE, /**< Dataspace handle (H5S) */
    HDF5_HANDLE_DATATYPE,  /**< Datatype handle (H5T) */
    HDF5_HANDLE_ATTRIBUTE, /**< Attribute handle (H5A) */
    HDF5_HANDLE_PROPERTY   /**< Property list handle (H5P) */
};

/**
 * @brief Initialize the HDF5 handle tracking system
 * 
 * Must be called before any other HDF5 utilities.
 * 
 * @return 0 on success, non-zero on failure
 */
extern int hdf5_tracking_init();

/**
 * @brief Clean up the HDF5 handle tracking system
 * 
 * Closes any remaining open handles and frees resources.
 * 
 * @return 0 on success, non-zero if some handles couldn't be closed
 */
extern int hdf5_tracking_cleanup();

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
extern int hdf5_track_handle(hid_t handle, enum hdf5_handle_type type, 
                             const char *file, int line);

/**
 * @brief Stop tracking an HDF5 handle
 * 
 * Removes a handle from tracking. This should be called after
 * a handle is closed.
 * 
 * @param handle The HDF5 handle to untrack
 * @return 0 on success, non-zero if handle wasn't found
 */
extern int hdf5_untrack_handle(hid_t handle);

/**
 * @brief Close all tracked HDF5 handles
 * 
 * Attempts to close all tracked handles in the correct order
 * (children before parents).
 * 
 * @return 0 on success, non-zero if some handles couldn't be closed
 */
extern int hdf5_close_all_handles();

/**
 * @brief Get the number of currently tracked handles
 * 
 * @return The number of tracked handles
 */
extern int hdf5_get_open_handle_count();

/**
 * @brief Print information about currently tracked handles
 * 
 * @return 0 on success
 */
extern int hdf5_print_open_handles();

/**
 * @brief Set testing mode for tracking system
 * 
 * In testing mode, handle closing is skipped (for mock handles)
 * 
 * @param mode 1 to enable testing mode, 0 to disable
 */
extern void hdf5_set_testing_mode(int mode);

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a file handle if it's valid and unregisters it.
 * 
 * @param file_id Pointer to the file handle
 * @return 0 on success, HDF5 error code on failure
 */
extern herr_t hdf5_check_and_close_file(hid_t *file_id);

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a group handle if it's valid and unregisters it.
 * 
 * @param group_id Pointer to the group handle
 * @return 0 on success, HDF5 error code on failure
 */
extern herr_t hdf5_check_and_close_group(hid_t *group_id);

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a dataset handle if it's valid and unregisters it.
 * 
 * @param dataset_id Pointer to the dataset handle
 * @return 0 on success, HDF5 error code on failure
 */
extern herr_t hdf5_check_and_close_dataset(hid_t *dataset_id);

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a dataspace handle if it's valid and unregisters it.
 * 
 * @param dataspace_id Pointer to the dataspace handle
 * @return 0 on success, HDF5 error code on failure
 */
extern herr_t hdf5_check_and_close_dataspace(hid_t *dataspace_id);

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a datatype handle if it's valid and unregisters it.
 * 
 * @param datatype_id Pointer to the datatype handle
 * @return 0 on success, HDF5 error code on failure
 */
extern herr_t hdf5_check_and_close_datatype(hid_t *datatype_id);

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes an attribute handle if it's valid and unregisters it.
 * 
 * @param attribute_id Pointer to the attribute handle
 * @return 0 on success, HDF5 error code on failure
 */
extern herr_t hdf5_check_and_close_attribute(hid_t *attribute_id);

/**
 * @brief Check if a handle is valid and close it
 * 
 * Safely closes a property list handle if it's valid and unregisters it.
 * 
 * @param property_id Pointer to the property list handle
 * @return 0 on success, HDF5 error code on failure
 */
extern herr_t hdf5_check_and_close_property(hid_t *property_id);

// Convenience macros for tracking handles
#define HDF5_TRACK_FILE(handle) hdf5_track_handle(handle, HDF5_HANDLE_FILE, __FILE__, __LINE__)
#define HDF5_TRACK_GROUP(handle) hdf5_track_handle(handle, HDF5_HANDLE_GROUP, __FILE__, __LINE__)
#define HDF5_TRACK_DATASET(handle) hdf5_track_handle(handle, HDF5_HANDLE_DATASET, __FILE__, __LINE__)
#define HDF5_TRACK_DATASPACE(handle) hdf5_track_handle(handle, HDF5_HANDLE_DATASPACE, __FILE__, __LINE__)
#define HDF5_TRACK_DATATYPE(handle) hdf5_track_handle(handle, HDF5_HANDLE_DATATYPE, __FILE__, __LINE__)
#define HDF5_TRACK_ATTRIBUTE(handle) hdf5_track_handle(handle, HDF5_HANDLE_ATTRIBUTE, __FILE__, __LINE__)
#define HDF5_TRACK_PROPERTY(handle) hdf5_track_handle(handle, HDF5_HANDLE_PROPERTY, __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif // HDF5
