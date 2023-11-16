#pragma once

#include <hdf5.h>
#include "../core_allvars.h"

#ifdef __cplusplus
extern "C" {
#endif

    extern herr_t read_attribute(hid_t fd, const char *group_name, const char *attr_name, void *attribute, const size_t dst_size);
    extern herr_t read_dataset_shape(hid_t fd, const char *dataset_name, int *ndims, hsize_t **dims);
    extern herr_t read_dataset(hid_t fd, const char *dataset_name, hid_t dataset_id, void *buffer, const size_t dst_size, const int check_size);
    extern int32_t fill_hdf5_metadata_names(struct HDF5_METADATA_NAMES *metadata_names, enum Valid_TreeTypes my_TreeType);


// Handy macro
#define READ_PARTIAL_DATASET(fd, group_name, dataset_name, ndim, offset, count, buffer) { \
        hid_t h5_grp = H5Gopen(fd, group_name, H5P_DEFAULT);            \
        XRETURN(h5_grp >= 0, -HDF5_ERROR, "Error: Could not open group = '%s'\n", group_name); \
        hid_t h5_dset = H5Dopen2(h5_grp, dataset_name, H5P_DEFAULT);    \
        XRETURN(h5_dset >= 0, -HDF5_ERROR, "Error: Could not open dataset = '%s' (within group = '%s')\n", dataset_name, group_name); \
        hid_t h5_space = H5Dget_space(h5_dset);                         \
        XRETURN(h5_space >= 0, -HDF5_ERROR, "Error: Could not reserve filespace for open dataset = '%s' (within group = '%s')\n", dataset_name, group_name); \
        hsize_t rank = H5Sget_simple_extent_ndims(h5_space);                                            \
        XRETURN((long long) rank == (long long) ndim, -HDF5_ERROR, "Error: rank = %lld should be equal to ndim = %lld\n", (long long) rank, (long long) ndim);\
        herr_t macro_status = H5Sselect_hyperslab(h5_space, H5S_SELECT_SET, offset, NULL, count, NULL); \
        XRETURN(macro_status >= 0, -HDF5_ERROR,                          \
                "Error: Failed to select hyperslab for dataset = '%s'.\n" \
                "The dimensions of the dataset was %"PRIu64", count[0] = %"PRIu64"\nThe file ID was %d\n.", \
                dataset_name, (uint64_t) ndim, (uint64_t) *count, (int32_t) fd); \
        hid_t h5_memspace = H5Screate_simple(ndim, count, NULL);        \
        XRETURN(h5_memspace >= 0, -HDF5_ERROR,                           \
                "Error: Failed to create memory space for dataset = '%s'.\n" \
                "The dimensions of the dataset was %"PRIu64", count[0] = %"PRIu64"\nThe file ID was %d\n.", \
                dataset_name, (uint64_t) ndim, (uint64_t) *count, (int32_t) fd); \
        const hid_t h5_dtype = H5Dget_type(h5_dset);                    \
        macro_status = H5Dread(h5_dset, h5_dtype, h5_memspace, h5_space, H5P_DEFAULT, buffer); \
        XRETURN(macro_status >= 0, -HDF5_ERROR, "Error: Failed to read array for dataset = '%s'.\n" \
                "The dimensions of the dataset was %"PRIu64", count[0] = %"PRIu64"\nThe file ID was %d\n.", \
                dataset_name, (uint64_t) ndim, (uint64_t) *count, (int32_t) fd); \
        XRETURN(H5Dclose(h5_dset) >= 0, -HDF5_ERROR,                     \
                "Error: Could not close dataset = '%s' (within group = '%s')\n", \
                dataset_name, group_name);                              \
        XRETURN(H5Tclose(h5_dtype) >= 0, -HDF5_ERROR,                    \
                "Error: Failed to close the datatype for = %s.\n"       \
                "The dimensions of the dataset was %"PRIu64", count[0] = %"PRIu64"\nThe file ID was %d\n.", \
                dataset_name, (uint64_t) ndim, (uint64_t) *count, (int32_t) fd); \
        XRETURN(H5Sclose(h5_memspace) >= 0, -HDF5_ERROR,                 \
                "Error: Failed to close the dataspace for = %s.\n"      \
                "The dimensions of the dataset was %"PRIu64", count[0] = %"PRIu64"\nThe file ID was %d\n.", \
                dataset_name, (uint64_t) ndim, (uint64_t) *count, (int32_t) fd); \
        XRETURN(H5Sclose(h5_space) >= 0, -HDF5_ERROR,                    \
                "Error: Failed to close the filespace for = %s.\n"      \
                "The dimensions of the dataset was %"PRIu64", count[0] = %"PRIu64"\nThe file ID was %d\n.", \
                dataset_name, (uint64_t) ndim, (uint64_t) *count, (int32_t) fd); \
        XRETURN(H5Gclose(h5_grp) >= 0, -HDF5_ERROR, "Error: Could not close group = '%s'\n", group_name); \
    }


#ifdef __cplusplus
}
#endif
