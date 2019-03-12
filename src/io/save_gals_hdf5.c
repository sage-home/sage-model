#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <hdf5.h>

#include "read_tree_binary.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"


/* Externally visible Functions */


#define CREATE_HDF5_SINGLE_ATTRIBUTE(group_id, attribute_value, attribute_name, h5_dtype) \
    {                                                                    \
        dataspace_id = H5Screate(H5S_SCALAR);                            \
        attribute_id = H5Acreate(group_id, attribute_name, h5_dtype, dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(attribute_id, h5_dtype, attribute_value);               \
        H5Aclose(attribute_id);                                          \
        H5Sclose(dataspace_id);                                          \
    }

void initialize_hdf5_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info, const struct params *run_params)
{

    hid_t file_id, group_id, dataspace_id, attribute_id;
    char buffer[4*MAX_STRING_LEN + 1];

    snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_%d.hdf5", run_params->OutputDir, run_params->FileNameGalaxies, filenr);

    file_id = H5Fcreate(buffer, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if(file_id == -1) {
        fprintf(stderr, "Error: Can't open file `%s'\n", buffer);
        ABORT(FILE_NOT_FOUND);
    }

    // Write here all the attributes/headers/information regarding data types of fields.
    save_info->h5_save_fd = file_id;

    group_id = H5Gcreate(file_id, "/Header", 0, H5P_DEFAULT, H5P_DEFAULT);

    CREATE_HDF5_SINGLE_ATTRIBUTE(group_id, &ntrees, "Ntrees", H5T_NATIVE_INT);

    ABORT(FILE_NOT_FOUND);
}



#undef CREATE_HDF5_SINGLE_ATTRIBUTE
