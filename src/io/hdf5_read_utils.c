#include <stdio.h>
#include <stdlib.h>

#include "hdf5_read_utils.h"

herr_t read_attribute(hid_t fd, const char *group_name, const char *attr_name, void *attribute, const size_t dst_size)
{
    hid_t attr_id = H5Aopen_by_name(fd, group_name, attr_name, H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "Error:Could not open the attribute '%s' in group '%s'\n", attr_name, group_name);
        return attr_id;
    }

    hid_t attr_dtype = H5Aget_type(attr_id);
    if(attr_dtype < 0) {
        fprintf(stderr, "Could not get the datatype for the attribute '%s' in group '%s'\n", attr_name, group_name);
        return attr_dtype;
    }

    if(dst_size != H5Tget_size(attr_dtype)) {
        fprintf(stderr,"Error while reading attribute '%s' within group '%s'\n", attr_name, group_name);
        fprintf(stderr,"The HDF5 attribute has size %zu bytes but the destination has size = %zu bytes\n",
                H5Tget_size(attr_dtype), dst_size);
        fprintf(stderr,"Perhaps the size of the destination datatype needs to be updated?\n");
        return -1;
    }

    herr_t status = H5Aread(attr_id, attr_dtype, attribute);
    if (status < 0) {
        fprintf(stderr, "Could not read the attribute %s in group %s\n", attr_name, group_name);
        H5Eprint(attr_id, stderr);
        return status;
    }

    status = H5Tclose(attr_dtype);
    if (status < 0) {
        fprintf(stderr, "Error when closing the datatype for the attribute '%s' in group '%s'.\n", attr_name, group_name);
        H5Eprint(status, stderr);
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


herr_t read_dataset_shape(hid_t fd, const char *dataset_name, int *ndims, hsize_t **dims)
{
    hid_t dataset_id = H5Dopen(fd, dataset_name, H5P_DEFAULT);
    if(dataset_id < 0) {
        fprintf(stderr, "Error encountered when trying to open up dataset '%s'.\n", dataset_name);
        H5Eprint(dataset_id, stderr);
        return -1;
    }

    hid_t dspace = H5Dget_space(dataset_id);
    if(dspace < 0) {
        fprintf(stderr, "Error encountered when trying to get dataspace for dataset '%s'.\n", dataset_name);
        H5Eprint(dataset_id, stderr);
        return dspace;
    }

    *ndims = H5Sget_simple_extent_ndims(dspace);
    if(*ndims < 0) {
        fprintf(stderr,"Error: Could not get the number of dimensions of the dataset '%s'\n", dataset_name);
        return (herr_t) EXIT_FAILURE;
    }

    *dims = calloc(*ndims, sizeof(hsize_t));
    if(*dims == NULL) {
        fprintf(stderr,"Error: Could not allocate memory for the dataset shape for the dataset '%s'. ndims = %d\n", dataset_name, *ndims);
        return (herr_t) EXIT_FAILURE;
    }

    herr_t status = H5Sget_simple_extent_dims(dspace, *dims, NULL);
    if(status < 0) {
        fprintf(stderr,"Error: Could not get the shape of the dataset '%s'. ndims = %d\n", dataset_name, *ndims);
        H5Eprint(dspace, stderr);
        return status;
    }

    status = H5Sclose(dspace);
    if(status < 0) {
        fprintf(stderr,"Error encountered while trying to close dataspace associated with dataset_name = '%s'\n", dataset_name);
        H5Eprint(dspace, stderr);
        return status;
    }

    status = H5Dclose(dataset_id);
    if(status < 0) {
        fprintf(stderr,"Error encountered while trying to close dataspace associated with dataset_name = '%s'\n", dataset_name);
        H5Eprint(dataset_id, stderr);
        return status;
    }


    return (herr_t) EXIT_SUCCESS;
}



herr_t read_dataset(hid_t fd, const char *dataset_name, hid_t dataset_id, void *buffer, const size_t dst_size, const int check_size)
{
    int already_open_dataset = dataset_id > 0 ? 1:0;

    if(already_open_dataset == 0) {
        dataset_id = H5Dopen2(fd, dataset_name, H5P_DEFAULT);
        if (dataset_id < 0) {
            fprintf(stderr, "Error encountered when trying to open up dataset '%s'.\n", dataset_name);
            H5Eprint(dataset_id, stderr);
            return -1;
        }
    }

    const hid_t h5_dtype = H5Dget_type(dataset_id);
    if(h5_dtype < 0) {
        fprintf(stderr,"Error getting datatype for dataset = '%s'\n", dataset_name);
        H5Eprint(dataset_id, stderr);
        return -1;
    }

    if(check_size && dst_size != H5Tget_size(h5_dtype)) {
        fprintf(stderr,"Error while reading dataset '%s' -- datasize mismatch -- will result in data corruption\n", dataset_name);
        fprintf(stderr,"The HDF5 dataset has items of size %zu bytes while the destination has size = %zu\n",
                H5Tget_size(h5_dtype), dst_size);
        fprintf(stderr,"'Perhaps the size of the destination datatype needs to be updated?\n");
        return -1;
    }


    herr_t status = H5Dread(dataset_id, h5_dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
    if(status < 0) {
        fprintf(stderr, "Error encountered when trying to reading dataset '%s'.\n", dataset_name);
        H5Eprint(dataset_id, stderr);
        return -1;
    }

    status = H5Tclose(h5_dtype);
    if(status < 0) {
        fprintf(stderr, "Error when closing the datatype for the dataset '%s'.\n", dataset_name);
        H5Eprint(status, stderr);
        return status;
    }

    if(already_open_dataset == 0) {
        status = H5Dclose(dataset_id);
        if(status < 0) {
            fprintf(stderr, "Error encountered when trying to close the open dataset '%s'.\n", dataset_name);
            H5Eprint(dataset_id, stderr);
            return -1;
        }
    }

    return (herr_t) EXIT_SUCCESS;
}


int32_t fill_hdf5_metadata_names(struct HDF5_METADATA_NAMES *metadata_names, enum Valid_TreeTypes my_TreeType)
{
    switch (my_TreeType) {
    case lhalo_hdf5:
        snprintf(metadata_names->name_NTrees, MAX_STRING_LEN - 1, "NtreesPerFile"); // Total number of forests within the file.
        snprintf(metadata_names->name_totNHalos, MAX_STRING_LEN - 1, "NhalosPerFile"); // Total number of halos within the file.
        snprintf(metadata_names->name_TreeNHalos, MAX_STRING_LEN - 1, "/Header/TreeNHalos"); // Number of halos per forest within the file.
        snprintf(metadata_names->name_ParticleMass, MAX_STRING_LEN - 1, "ParticleMass");//Particle mass for Dark matter in the sim
        snprintf(metadata_names->name_NumSimulationTreeFiles, MAX_STRING_LEN - 1, "NumberOfOutputFiles");//Particle mass for Dark matter in the sim
        return EXIT_SUCCESS;

    case gadget4_hdf5:
        snprintf(metadata_names->name_NTrees, MAX_STRING_LEN - 1, "Ntrees_ThisFile"); // Total number of forests within the file.
        snprintf(metadata_names->name_totNHalos, MAX_STRING_LEN - 1, "Nhalos_ThisFile"); // Total number of halos within the file.
        // snprintf(metadata_names->name_TreeNHalos, MAX_STRING_LEN - 1, "TreeNHalos"); // Number of halos per forest within the file.
        snprintf(metadata_names->name_ParticleMass, MAX_STRING_LEN - 1, "DOES-NOT-EXIST");//Particle mass for Dark matter in the sim
        snprintf(metadata_names->name_NumSimulationTreeFiles, MAX_STRING_LEN - 1, "NumFiles");//Number of output mergertree files 
        return EXIT_SUCCESS;

    case lhalo_binary:
        fprintf(stderr, "If the file is binary then this function should never be called.  Something's gone wrong...");
        return EXIT_FAILURE;

    default:
        fprintf(stderr, "Your tree type has not been included in the switch statement for ``%s`` in file ``%s``.\n", __FUNCTION__, __FILE__);
        fprintf(stderr, "Please add it there.\n");
        return EXIT_FAILURE;

    }

    return EXIT_FAILURE;
}
