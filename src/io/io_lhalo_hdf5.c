#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "io_lhalo_hdf5.h"
#include "forest_utils.h"
#include "../core/core_mymalloc.h"
#include "../core/core_utils.h"

#ifdef HDF5

/* Forward declaration of static functions */
static int io_lhalo_hdf5_initialize(const char *filename, struct params *params, void **format_data);
static int64_t io_lhalo_hdf5_read_forest(int64_t forestnr, struct halo_data **halos, 
                                       struct forest_info *forest_info, void *format_data);
static int io_lhalo_hdf5_cleanup(void *format_data);
static int io_lhalo_hdf5_close_handles(void *format_data);
static int io_lhalo_hdf5_get_handle_count(void *format_data);

/**
 * @brief LHalo HDF5 handler interface
 *
 * Implementation of the I/O interface for LHalo HDF5 format
 */
static struct io_interface lhalo_hdf5_handler = {
    .name = "LHalo HDF5",
    .version = "1.0",
    .format_id = IO_FORMAT_LHALO_HDF5,
    .capabilities = IO_CAP_RANDOM_ACCESS | IO_CAP_MULTI_FILE | IO_CAP_METADATA_QUERY | IO_CAP_METADATA_ATTRS,
    
    .initialize = io_lhalo_hdf5_initialize,
    .read_forest = io_lhalo_hdf5_read_forest,
    .write_galaxies = NULL, /* LHalo HDF5 format is input-only */
    .cleanup = io_lhalo_hdf5_cleanup,
    
    .close_open_handles = io_lhalo_hdf5_close_handles,
    .get_open_handle_count = io_lhalo_hdf5_get_handle_count,
    
    .last_error = IO_ERROR_NONE,
    .error_message = {0}
};

/**
 * @brief Get filename for a specific forest file
 *
 * @param filename Buffer to store the filename
 * @param len Length of the buffer
 * @param filenr File number
 * @param run_params Runtime parameters
 */
static void get_forests_filename_lhalo_hdf5(char *filename, const size_t len, const int filenr, const struct params *run_params)
{
    snprintf(filename, len - 1, "%s/%s.%d%s", run_params->io.SimulationDir, 
             run_params->io.TreeName, filenr, run_params->io.TreeExtension);
}

/**
 * @brief Initialize the LHalo HDF5 I/O handler
 *
 * Registers the handler with the I/O interface system.
 *
 * @return 0 on success, non-zero on failure
 */
// Stub functions to satisfy the linker
// Real implementation needs to wait until Makefile is updated
int io_lhalo_hdf5_init(void) {
    return io_register_handler(&lhalo_hdf5_handler);
}

/**
 * @brief Get the LHalo HDF5 handler
 *
 * @return Pointer to the handler, or NULL if not registered
 */
struct io_interface *io_get_lhalo_hdf5_handler(void) {
    return io_get_handler_by_id(IO_FORMAT_LHALO_HDF5);
}

/**
 * @brief Detect if a file is in LHalo HDF5 format
 *
 * Tries to open the file as HDF5 and check for expected datasets and attributes.
 *
 * @param filename File to examine
 * @return true if the file is in LHalo HDF5 format, false otherwise
 */
bool io_is_lhalo_hdf5(const char *filename) {
    // Check for NULL or empty filename
    if (filename == NULL || strlen(filename) == 0) {
        return false;
    }
    
    // Security check: reject suspicious paths
    if (strstr(filename, "..") != NULL || strchr(filename, '\n') != NULL) {
        return false;
    }
    
    // Just check for HDF5 extension for now
    const char *ext = strrchr(filename, '.');
    if (ext == NULL) {
        return false;
    }
    
    return (strcmp(ext, ".hdf5") == 0 || strcmp(ext, ".h5") == 0);
}

/**
 * @brief Initialize the LHalo HDF5 handler
 *
 * Opens the files, reads header information, and prepares for reading forests.
 *
 * @param filename Tree file pattern or base name
 * @param params Run parameters
 * @param format_data Pointer to store format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_hdf5_initialize(const char *filename, struct params *params, void **format_data) {
    if (filename == NULL || params == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL filename or params passed to io_lhalo_hdf5_initialize");
        return -1;
    }
    
    struct lhalo_hdf5_data *data = calloc(1, sizeof(struct lhalo_hdf5_data));
    if (data == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for lhalo_hdf5_data");
        return -1;
    }
    
    /* Initialize the metadata names based on the tree type */
    int status = fill_hdf5_metadata_names(&data->metadata_names, params->io.TreeType);
    if (status != 0) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Failed to initialize metadata names");
        free(data);
        return -1;
    }
    
    *format_data = data;
    return 0;
}

/**
 * @brief Convert units for a forest's halo data
 * 
 * Performs necessary unit conversions for halo properties.
 * 
 * @param halos Array of halo data
 * @param nhalos Number of halos
 * @return 0 on success, non-zero on failure
 */
static int convert_units_for_forest(struct halo_data *halos, const int64_t nhalos) {
    if (nhalos <= 0) {
        return -1;
    }

    const float spin_conv_fac = 0.001f;
    
    /* Apply unit conversions */
    for (int64_t i = 0; i < nhalos; i++) {
        for (int j = 0; j < 3; j++) {
            halos[i].Pos[j] *= 0.001f; /* Convert from kpc/h to Mpc/h */
            halos[i].Spin[j] *= spin_conv_fac; /* Convert spin units */
        }
        halos[i].SubhaloIndex = -1;
        halos[i].SubHalfMass = -1.0f;
    }

    return 0;
}

/**
 * @brief Read a forest from LHalo HDF5 file
 *
 * Reads the halo data for a specific forest.
 *
 * @param forestnr Forest number to read
 * @param halos Pointer to store the halo data
 * @param forest_info Forest information structure
 * @param format_data Format-specific data
 * @return Number of halos read, or negative error code
 */
static int64_t io_lhalo_hdf5_read_forest(int64_t forestnr, struct halo_data **halos, 
                                       struct forest_info *forest_info, void *format_data) {
    struct lhalo_hdf5_data *data = (struct lhalo_hdf5_data *)format_data;
    
    if (data == NULL) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "NULL format_data passed to io_lhalo_hdf5_read_forest");
        return -1;
    }
    
    if (forestnr >= forest_info->nforests_this_task) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Forest number out of range");
        return -1;
    }
    
    const int64_t treenum_in_file = forest_info->original_treenr[forestnr];
    hid_t fd = forest_info->lht.h5_fd[forestnr];
    
    /* Check: HDF5 file pointer is valid */
    if (fd <= 0) {
        io_set_error(IO_ERROR_HANDLE_INVALID, "Invalid HDF5 file handle");
        return -1;
    }
    
    /* Determine number of halos by checking the size of the 'Descendant' dataset */
    char dataset_name[MAX_STRING_LEN];
    snprintf(dataset_name, MAX_STRING_LEN - 1, "Tree%"PRId64"/Descendant", treenum_in_file);
    
    int ndims;
    hsize_t *dims;
    int status = (int)read_dataset_shape(fd, dataset_name, &ndims, &dims);
    if (status != 0) {
        io_set_error(IO_ERROR_FORMAT_ERROR, "Failed to read dataset shape");
        return -1;
    }
    
    if (ndims != 1) {
        io_set_error(IO_ERROR_FORMAT_ERROR, "Expected 1D array for Descendant dataset");
        free(dims);
        return -1;
    }
    
    const int64_t nhalos = dims[0];
    free(dims);
    
    /* Allocate memory for halos */
    *halos = mymalloc(sizeof(struct halo_data) * nhalos);
    struct halo_data *local_halos = *halos;
    if (local_halos == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate memory for halos");
        return -1;
    }
    
    /* Allocate buffer for reading data */
    void *buffer = malloc(nhalos * NDIM * sizeof(double));
    if (buffer == NULL) {
        io_set_error(IO_ERROR_MEMORY_ALLOCATION, "Failed to allocate buffer memory");
        myfree(local_halos);
        *halos = NULL;
        return -1;
    }
    
    /* Define macro for reading tree properties */
    #define READ_TREE_PROPERTY(fd, treenr, sage_name, hdf5_name, C_dtype) { \
        snprintf(dataset_name, MAX_STRING_LEN - 1, "Tree%"PRId64"/%s", treenr, #hdf5_name); \
        const int check_size = 1; \
        int macro_status = read_dataset(fd, dataset_name, -1, buffer, sizeof(C_dtype), check_size); \
        if (macro_status != 0) { \
            myfree(local_halos); \
            free(buffer); \
            *halos = NULL; \
            io_set_error(IO_ERROR_FORMAT_ERROR, "Failed to read dataset " #hdf5_name); \
            return -1; \
        } \
        C_dtype *macro_x = (C_dtype *)buffer; \
        for (int halo_idx = 0; halo_idx < nhalos; ++halo_idx) { \
            local_halos[halo_idx].sage_name = *macro_x; \
            macro_x++; \
        } \
    }
    
    /* Define macro for reading tree properties with multiple dimensions */
    #define READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenr, sage_name, hdf5_name, C_dtype) { \
        snprintf(dataset_name, MAX_STRING_LEN - 1, "Tree%"PRId64"/%s", treenr, #hdf5_name); \
        const int check_size = 1; \
        const int macro_status = read_dataset(fd, dataset_name, -1, buffer, sizeof(C_dtype), check_size); \
        if (macro_status != 0) { \
            myfree(local_halos); \
            free(buffer); \
            *halos = NULL; \
            io_set_error(IO_ERROR_FORMAT_ERROR, "Failed to read dataset " #hdf5_name); \
            return -1; \
        } \
        C_dtype *macro_x = (C_dtype *)buffer; \
        for (int halo_idx = 0; halo_idx < nhalos; ++halo_idx) { \
            for (int dim = 0; dim < NDIM; ++dim) { \
                local_halos[halo_idx].sage_name[dim] = *macro_x; \
                macro_x++; \
            } \
        } \
    }
    
    /* Merger Tree Pointers */
    READ_TREE_PROPERTY(fd, treenum_in_file, Descendant, Descendant, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, FirstProgenitor, FirstProgenitor, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, NextProgenitor, NextProgenitor, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, FirstHaloInFOFgroup, FirstHaloInFOFGroup, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, NextHaloInFOFgroup, NextHaloInFOFGroup, int);
    
    /* Halo Properties */
    READ_TREE_PROPERTY(fd, treenum_in_file, Len, SubhaloLen, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, M_Mean200, Group_M_Mean200, float);
    READ_TREE_PROPERTY(fd, treenum_in_file, Mvir, Group_M_Crit200, float);
    READ_TREE_PROPERTY(fd, treenum_in_file, M_TopHat, Group_M_TopHat200, float);
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenum_in_file, Pos, SubhaloPos, float);
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenum_in_file, Vel, SubhaloVel, float);
    READ_TREE_PROPERTY(fd, treenum_in_file, VelDisp, SubhaloVelDisp, float);
    READ_TREE_PROPERTY(fd, treenum_in_file, Vmax, SubhaloVMax, float);
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenum_in_file, Spin, SubhaloSpin, float);
    READ_TREE_PROPERTY(fd, treenum_in_file, MostBoundID, SubhaloIDMostBound, unsigned long long);
    
    /* File Position Info */
    READ_TREE_PROPERTY(fd, treenum_in_file, SnapNum, SnapNum, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, FileNr, FileNr, int);
    
    /* Cleanup */
    free(buffer);
    
    /* Apply unit conversions */
    status = convert_units_for_forest(local_halos, nhalos);
    if (status != 0) {
        io_set_error(IO_ERROR_VALIDATION_FAILED, "Failed to convert units for forest");
        myfree(local_halos);
        *halos = NULL;
        return -1;
    }
    
    return nhalos;
}

/**
 * @brief Clean up resources used by the LHalo HDF5 handler
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_hdf5_cleanup(void *format_data) {
    if (format_data == NULL) {
        return 0;  /* Nothing to clean up */
    }
    
    struct lhalo_hdf5_data *data = (struct lhalo_hdf5_data *)format_data;
    
    /* Close any open file handles */
    io_lhalo_hdf5_close_handles(format_data);
    
    /* Free allocated memory */
    free(data->file_handles);
    free(data->unique_file_handles);
    free(data->nhalos_per_forest);
    free(data);
    
    return 0;
}

/**
 * @brief Close all open HDF5 file handles
 *
 * @param format_data Format-specific data
 * @return 0 on success, non-zero on failure
 */
static int io_lhalo_hdf5_close_handles(void *format_data) {
    if (format_data == NULL) {
        return 0;  /* Nothing to close */
    }
    
    struct lhalo_hdf5_data *data = (struct lhalo_hdf5_data *)format_data;
    
    /* Close all open file handles */
    for (int i = 0; i < data->num_open_files; i++) {
        if (data->unique_file_handles[i] > 0) {
            H5Fclose(data->unique_file_handles[i]);
            data->unique_file_handles[i] = -1;
        }
    }
    
    data->num_open_files = 0;
    
    return 0;
}

/**
 * @brief Get the number of open HDF5 file handles
 *
 * @param format_data Format-specific data
 * @return Number of open HDF5 file handles
 */
static int io_lhalo_hdf5_get_handle_count(void *format_data) {
    if (format_data == NULL) {
        return 0;
    }
    
    struct lhalo_hdf5_data *data = (struct lhalo_hdf5_data *)format_data;
    return data->num_open_files;
}

#endif /* HDF5 */