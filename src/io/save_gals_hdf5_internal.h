#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

// Include main public header
#include "save_gals_hdf5.h"

// Include core dependencies
#include "../core/core_mymalloc.h"
#include "../core/core_utils.h"
#include "../core/macros.h"
#include "../core/core_property_utils.h"
#include "../core/core_logging.h"
#include "../core/core_property_descriptor.h"
#include "../core/sage.h"

// Include HDF5 headers with proper guard
#ifdef HDF5
#include <hdf5.h>
#endif

// Include physics headers based on build configuration
#ifdef CORE_ONLY
#include "../physics/placeholder_hdf5_save.h"
#include "../physics/placeholder_model_misc.h"
#else
#include "../physics/legacy/model_misc.h"
#endif

// Forward declaration of the total number of properties
extern const int32_t TotGalaxyProperties;

// Define NUM_GALS_PER_BUFFER for use in multiple functions
#define NUM_GALS_PER_BUFFER 8192

// HDF5 is a self-describing data format. Each dataset will contain a number of attributes to
// describe properties such as units or number of elements. These macros create attributes for a
// single number or a string.
#define CREATE_SINGLE_ATTRIBUTE(group_id, attribute_name, attribute_value, h5_dtype) { \
        hid_t macro_dataspace_id = H5Screate(H5S_SCALAR);               \
        CHECK_STATUS_AND_RETURN_ON_FAIL(macro_dataspace_id, (int32_t) macro_dataspace_id, \
                                        "Could not create an attribute dataspace.\n" \
                                        "The attribute we wanted to create was '" #attribute_name"' and the HDF5 datatype was '" #h5_dtype".\n"); \
        if(sizeof(attribute_value) != H5Tget_size(h5_dtype)) {    \
            fprintf(stderr,"Error: attribute " #attribute_name" the C size = %zu does not match the hdf5 datatype size=%zu\n", \
                    sizeof(attribute_value), H5Tget_size(h5_dtype));    \
            return -1;                                                  \
        }                                                               \
        hid_t macro_attribute_id = H5Acreate(group_id, attribute_name, h5_dtype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        CHECK_STATUS_AND_RETURN_ON_FAIL(macro_attribute_id, (int32_t) macro_attribute_id, \
                                        "Could not create an attribute ID.\n" \
                                        "The attribute we wanted to create was '" #attribute_name"' and the HDF5 datatype was '" #h5_dtype".\n"); \
        herr_t h5_macro_status = H5Awrite(macro_attribute_id, h5_dtype, &(attribute_value)); \
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_macro_status, (int32_t) h5_macro_status,       \
                                        "Could not write an attribute.\n" \
                                        "The attribute we wanted to create was '" #attribute_name"' and the HDF5 datatype was '" #h5_dtype".\n"); \
        h5_macro_status = H5Aclose(macro_attribute_id);                          \
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_macro_status, (int32_t) h5_macro_status,       \
                                        "Could not close an attribute ID.\n" \
                                        "The attribute we wanted to create was '" #attribute_name"' and the HDF5 datatype was '" #h5_dtype".\n"); \
        h5_macro_status = H5Sclose(macro_dataspace_id);                          \
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_macro_status, (int32_t) h5_macro_status,       \
                                    "Could not close an attribute dataspace.\n" \
                                        "The attribute we wanted to create was '" #attribute_name"' and the HDF5 datatype was '" #h5_dtype".\n"); \
    }

#define CREATE_STRING_ATTRIBUTE(group_id, attribute_name, attribute_value, stringlen) { \
    hid_t macro_dataspace_id = H5Screate(H5S_SCALAR);                   \
    CHECK_STATUS_AND_RETURN_ON_FAIL(macro_dataspace_id, (int32_t) macro_dataspace_id, \
                                    "Could not create an attribute dataspace for a String.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    hid_t atype = H5Tcopy(H5T_C_S1);                                  \
    CHECK_STATUS_AND_RETURN_ON_FAIL(atype, (int32_t) atype,             \
                                    "Could not copy an existing data type when creating a String attribute.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    herr_t h5_attr_status = H5Tset_size(atype, stringlen);                 \
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_attr_status, (int32_t) h5_attr_status, \
                                    "Could not set the total size of a datatype when creating a String attribute.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    h5_attr_status = H5Tset_strpad(atype, H5T_STR_NULLTERM);               \
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_attr_status, (int32_t) h5_attr_status, \
                                    "Could not set the padding when creating a String attribute.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    hid_t macro_attribute_id = H5Acreate(group_id, attribute_name, atype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
    CHECK_STATUS_AND_RETURN_ON_FAIL(macro_attribute_id, (int32_t) macro_attribute_id, \
                                    "Could not create an attribute ID for string.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    h5_attr_status = H5Awrite(macro_attribute_id, atype, attribute_value); \
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_attr_status, (int32_t) h5_attr_status, \
                                    "Could not write an attribute.\n"   \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    h5_attr_status = H5Aclose(macro_attribute_id);                         \
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_attr_status, (int32_t) h5_attr_status,                            \
                                    "Could not close an attribute ID.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    h5_attr_status = H5Tclose(atype);                                      \
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_attr_status, (int32_t) h5_attr_status, \
                                    "Could not close atype value.\n"    \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    h5_attr_status = H5Sclose(macro_dataspace_id);                         \
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_attr_status, (int32_t) h5_attr_status, \
                                    "Could not close an attribute dataspace when creating a String attribute.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    }

#define CREATE_AND_WRITE_1D_ARRAY(file_id, field_name, dims, buffer, h5_dtype) { \
        if(sizeof(buffer[0]) != H5Tget_size(h5_dtype)) {                \
            fprintf(stderr,"Error: For field " #field_name", the C size = %zu does not match the hdf5 datatype size=%zu\n", \
                    sizeof(buffer[0]),  H5Tget_size(h5_dtype));         \
            return -1;                                                  \
        }                                                               \
        hid_t macro_dataspace_id = H5Screate_simple(1, dims, NULL);     \
        CHECK_STATUS_AND_RETURN_ON_FAIL(macro_dataspace_id, (int32_t) macro_dataspace_id, \
                                        "Could not create a dataspace for field " #field_name".\n" \
                                        "The dimensions of the dataspace was %d\n", (int32_t) dims[0]); \
        hid_t macro_dataset_id = H5Dcreate2(file_id, field_name, h5_dtype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); \
        CHECK_STATUS_AND_RETURN_ON_FAIL(macro_dataset_id, (int32_t) macro_dataset_id, \
                                        "Could not create a dataset for field " #field_name".\n" \
                                        "The dimensions of the dataset was %d\nThe file id was %d\n.", \
                                        (int32_t) dims[0], (int32_t) file_id); \
        herr_t h5_dset_status = H5Dwrite(macro_dataset_id, h5_dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer); \
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_dset_status, (int32_t) h5_dset_status, \
                                        "Failed to write a dataset for field " #field_name".\n" \
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) file_id, \
                                        (int32_t) macro_dataset_id);    \
        h5_dset_status = H5Dclose(macro_dataset_id);                       \
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_dset_status, (int32_t) h5_dset_status, \
                                        "Failed to close the dataset for field " #field_name".\n" \
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) file_id, \
                                        (int32_t) macro_dataset_id);    \
        h5_dset_status = H5Sclose(macro_dataspace_id);                     \
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_dset_status, (int32_t) h5_dset_status, \
                                        "Failed to close the dataspace for field " #field_name".\n" \
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                                        "The dataspace ID was %d.", (int32_t) dims[0], (int32_t) file_id, \
                                        (int32_t) macro_dataspace_id);  \
    }

// Function prototypes for the components
int32_t write_header(hid_t file_id, const struct forest_info *forest_info, const struct params *run_params);
int32_t trigger_buffer_write(const int32_t snap_idx, const int32_t num_to_write, const int64_t num_already_written,
                           struct save_info *save_info, const struct params *run_params);
int32_t prepare_galaxy_for_hdf5_output(const struct GALAXY *g, struct save_info *save_info,
                                     const int32_t output_snap_idx, const struct halo_data *halos,
                                     const int64_t task_forestnr, const int64_t original_treenr,
                                     const struct params *run_params);
void free_property_discovery(struct hdf5_save_info *save_info);
int32_t generate_field_metadata(struct hdf5_save_info *save_info);
int discover_output_properties(struct hdf5_save_info *save_info);
int allocate_output_property(struct hdf5_save_info *save_info, int snap_idx, int prop_idx, int buffer_size);
int free_output_property(struct hdf5_save_info *save_info, int snap_idx, int prop_idx);
int allocate_all_output_properties(struct hdf5_save_info *save_info, int snap_idx);
int free_all_output_properties(struct hdf5_save_info *save_info, int snap_idx);
int32_t initialize_hdf5_galaxy_files(const int filenr, struct save_info *save_info, const struct params *run_params);
int32_t finalize_hdf5_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info,
                                  const struct params *run_params);
