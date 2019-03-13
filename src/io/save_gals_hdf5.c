#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <hdf5.h>
#include <hdf5_hl.h>

#include "read_tree_binary.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"

// Local Proto-Types //

// Externally Visible Functions //

#define CREATE_HDF5_SINGLE_ATTRIBUTE(group_id, attribute_value, attribute_name, h5_dtype) \
    {                                                                    \
        dataspace_id = H5Screate(H5S_SCALAR);                            \
        attribute_id = H5Acreate(group_id, attribute_name, h5_dtype, dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(attribute_id, h5_dtype, attribute_value);               \
        H5Aclose(attribute_id);                                          \
        H5Sclose(dataspace_id);                                          \
    }

int32_t initialize_hdf5_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info,
                                     const struct params *run_params)
{

    hid_t file_id, group_id, dataspace_id, attribute_id;
    char buffer[4*MAX_STRING_LEN + 1];

    snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_%d.hdf5", run_params->OutputDir, run_params->FileNameGalaxies, filenr);

    file_id = H5Fcreate(buffer, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if(file_id == -1) {
        fprintf(stderr, "\n\nError: Can't open file `%s'\n\n\n", buffer);
        return FILE_NOT_FOUND;
    }

    // Write here all the attributes/headers/information regarding data types of fields.
    save_info->h5_save_fd = file_id;

    group_id = H5Gcreate(file_id, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    CREATE_HDF5_SINGLE_ATTRIBUTE(group_id, &ntrees, "Ntrees", H5T_NATIVE_INT);

    group_id = H5Gcreate(file_id, "/Galaxies", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);


    save_info->dst_size = sizeof(float)*2;
    save_info->offsets = malloc(sizeof(size_t)*2);
    save_info->field_sizes = malloc(sizeof(size_t)*2);
    save_info->field_names = malloc(sizeof(char *)*2);
    save_info->field_types = malloc(sizeof(hid_t)*2);

    float *tmp_data;
    tmp_data = malloc(sizeof(float));
    tmp_data[0] = 134.0;
    tmp_data[1] = -13340.34049;

    save_info->offsets[0] = 0; 
    save_info->field_sizes[0] = sizeof(float);
    save_info->field_names[0] = "Field1";
    save_info->field_types[0] = H5T_NATIVE_FLOAT;

    save_info->offsets[1] = sizeof(float); 
    save_info->field_sizes[1] = sizeof(float);
    save_info->field_names[1] = "Field2";
    save_info->field_types[1] = H5T_NATIVE_FLOAT;

    H5TBmake_table("Data", group_id, "Data", (hsize_t) 2, (hsize_t) 1, save_info->dst_size, save_info->field_names, save_info->offsets, save_info->field_types, (hsize_t) 1, NULL, 1, NULL);
    H5TBwrite_records(group_id, "Data", 0, 1, save_info->dst_size, save_info->offsets, save_info->field_sizes, tmp_data);
    //save_info->offsets[0] = sizeof(float); 
    H5TBappend_records(group_id, "Data", 1, save_info->dst_size, save_info->offsets, save_info->field_sizes, tmp_data);

    return EXIT_SUCCESS; 
}

#undef CREATE_HDF5_SINGLE_ATTRIBUTE

// Local Functions //
