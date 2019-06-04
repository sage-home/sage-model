#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>


#include "hdf5_read_utils.h"
#include "../core_allvars.h"

herr_t read_attribute(hid_t fd, const char *group_name, const char *attr_name, const hid_t attr_dtype, void *attribute)
{
    hid_t attr_id = H5Aopen_by_name(fd, group_name, attr_name, H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "Could not open the attribute %s in group %s\n", attr_name, group_name);
        return attr_id;
    }

    herr_t status = H5Aread(attr_id, attr_dtype, attribute);
    if (status < 0) {
        fprintf(stderr, "Could not read the attribute %s in group %s\n", attr_name, group_name);
        H5Eprint(attr_id, stderr);
        return status;
    }

    status = H5Aclose(attr_id);
    if (status < 0) {
        fprintf(stderr, "Error when closing the attribute %s in group %s.\n", attr_name, group_name);
        H5Eprint(attr_id, stderr);
        return status;
    }

    return (herr_t) EXIT_SUCCESS;
}

herr_t read_dataset(hid_t fd, const char *dataset_name, hid_t dataset_id, const hid_t datatype, void *buffer)
{
    int already_open_dataset = dataset_id > 0 ? 1:0;

    if(already_open_dataset == 0) {
        dataset_id = H5Dopen2(fd, dataset_name, H5P_DEFAULT);
        if (dataset_id < 0) {
            fprintf(stderr, "Error encountered when trying to open up dataset %s\n", dataset_name);
            H5Eprint(dataset_id, stderr);
            return FILE_READ_ERROR;
        }
    }

    herr_t status = H5Dread(dataset_id, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
    if(status < 0) {
        fprintf(stderr, "Error encountered when trying to reading dataset %s\n", dataset_name);
        H5Eprint(dataset_id, stderr);
        return FILE_READ_ERROR;
    }

    if(already_open_dataset == 0) {
        status = H5Dclose(dataset_id);
        if(status < 0) {
            fprintf(stderr, "Error encountered when trying to close the open dataset %s\n", dataset_name);
            H5Eprint(dataset_id, stderr);
            return FILE_READ_ERROR;
        }
    }

    return (herr_t) EXIT_SUCCESS;
}
