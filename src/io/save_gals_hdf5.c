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

#define NUM_OUTPUT_FIELDS 52

// Local Proto-Types //

int32_t generate_field_metadata(char **field_names, char **field_descriptions, char **field_units);

// Externally Visible Functions //

#define MAX_ATTRIBUTE_LEN 10000

#define CREATE_SINGLE_ATTRIBUTE(group_id, attribute_name, attribute_value,  h5_dtype) {\
        macro_dataspace_id = H5Screate(H5S_SCALAR);                      \
        macro_attribute_id = H5Acreate(group_id, attribute_name, h5_dtype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(macro_attribute_id, h5_dtype, attribute_value);               \
        H5Aclose(macro_attribute_id);                                    \
        H5Sclose(macro_dataspace_id);                                    \
    }

#define CREATE_STRING_ATTRIBUTE(group_id, attribute_name, attribute_value) \
    {                                                                    \
        macro_dataspace_id = H5Screate(H5S_SCALAR);                      \
        atype = H5Tcopy(H5T_C_S1);\
        H5Tset_size(atype, MAX_ATTRIBUTE_LEN-1);\
        H5Tset_strpad(atype, H5T_STR_NULLTERM);\
        macro_attribute_id = H5Acreate(group_id, attribute_name, atype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(macro_attribute_id, atype, attribute_value);               \
        H5Aclose(macro_attribute_id);                                    \
        H5Sclose(macro_dataspace_id);                                    \
    }

int32_t initialize_hdf5_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info,
                                     const struct params *run_params)
{

    hid_t atype;
    hid_t prop, dataset_id, filespace, memspace;
    hid_t file_id, group_id, macro_dataspace_id, macro_attribute_id, dataspace_id;
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

    CREATE_SINGLE_ATTRIBUTE(group_id, "Ntrees", &ntrees, H5T_NATIVE_INT);

    float *tmp_data;
    tmp_data = malloc(sizeof(float));
    tmp_data[0] = 134.0;
    tmp_data[1] = -134.0;
    tmp_data[2] = 104.0;
    tmp_data[3] = 4.0;
    tmp_data[4] = 13414.49;

    hsize_t dims[1] = {5};
    hsize_t dimsext[1] = {5};
    hsize_t new_dims[1];
    hsize_t offset[1];
    hsize_t maxdims[1] = {H5S_UNLIMITED};
    hsize_t chunk_dims[1] = {1};
    herr_t status;

    char *field_names[NUM_OUTPUT_FIELDS];
    char *field_descriptions[NUM_OUTPUT_FIELDS];
    char *field_units[NUM_OUTPUT_FIELDS];

    generate_field_metadata(field_names, field_descriptions, field_units);

    save_info->dataset_ids = malloc(sizeof(hid_t) * NUM_OUTPUT_FIELDS);
    char *name;
    char *description;
    char *unit;
 
    for(int32_t i = 0; i < NUM_OUTPUT_FIELDS; ++i) {

        name = field_names[i];
        description = field_descriptions[i];
        unit = field_units[i];

        prop = H5Pcreate(H5P_DATASET_CREATE);
        dataspace_id = H5Screate_simple(1, dims, maxdims);
        status = H5Pset_chunk(prop, 1, chunk_dims);

        dataset_id = H5Dcreate2(file_id, name, H5T_NATIVE_FLOAT, dataspace_id, H5P_DEFAULT, prop, H5P_DEFAULT);
        save_info->dataset_ids[i] = dataset_id;

        CREATE_STRING_ATTRIBUTE(dataset_id, "Description", description); 
        CREATE_STRING_ATTRIBUTE(dataset_id, "Units", unit); 

        status = H5Pclose(prop);
        status = H5Sclose(dataspace_id);
    }

    char buf[10000];
    snprintf(buf, 9999, "Mass of stars");

    status = H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp_data);

    new_dims[0] = dims[0] + dimsext[0];
    status = H5Dset_extent(dataset_id, new_dims);

    filespace = H5Dget_space(dataset_id);
    
    offset[0] = 5;
    status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, dimsext, NULL);

    memspace = H5Screate_simple(1, dimsext, NULL);

    status = H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, tmp_data); 

    status = H5Dclose(dataset_id);

    if (status == 0)
     printf("RERO\n"); 
    /*
    group_id = H5Gcreate(file_id, "/Galaxies", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);


    save_info->dst_size = sizeof(float)*2;
    save_info->offsets = malloc(sizeof(size_t)*2);
    save_info->field_sizes = malloc(sizeof(size_t)*2);
    save_info->field_names = malloc(sizeof(char *)*2);
    save_info->field_types = malloc(sizeof(hid_t)*2);

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
    */

    return EXIT_SUCCESS; 
}

#undef CREATE_HDF5_SINGLE_ATTRIBUTE

// Local Functions //

int32_t generate_field_metadata(char **field_names, char **field_descriptions, char **field_units) {

    char *tmp_names[NUM_OUTPUT_FIELDS] = {"GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
                                            "SAGETreeIndex", "SimulationHaloIndex", "mergeType", "mergeIntoID",
                                            "mergeIntoSnapNum", "dT", "Posx", "Posy", "Posz", "Velx", "Vely", "Velz",
                                            "Spinx", "Spiny", "Spinz", "Len", "Mvir", "CentralMvir", "Rvir", "Vvir",
                                            "Vmax", "VelDisp", "ColdGas", "StellarMass", "BulgeMass", "HotGas", "EjectedMass",
                                            "BlackHoleMass", "ICS", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                            "MetalsHotGas", "MetalsEjectedMass", "MetalsICS", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                            "SfrBulgeZ", "DiskScaleRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
                                            "TimeOfLastMajorMerger", "TimeOfLastMinorMerger", "OutflowRate", "infallMvir",
                                            "infallVvir", "infallVmax"};

    char *tmp_descriptions[NUM_OUTPUT_FIELDS] = {"GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
                                            "SAGETreeIndex", "SimulationHaloIndex", "mergeType", "mergeIntoID",
                                            "mergeIntoSnapNum", "dT", "Posx", "Posy", "Posz", "Velx", "Vely", "Velz",
                                            "Spinx", "Spiny", "Spinz", "Len", "Mvir", "CentralMvir", "Rvir", "Vvir",
                                            "Vmax", "VelDisp", "ColdGas", "StellarMass", "BulgeMass", "HotGas", "EjectedMass",
                                            "BlackHoleMass", "ICS", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                            "MetalsHotGas", "MetalsEjectedMass", "MetalsICS", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                            "SfrBulgeZ", "DiskScaleRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
                                            "TimeOfLastMajorMerger", "TimeOfLastMinorMerger", "OutflowRate", "infallMvir",
                                            "infallVvir", "infallVmax"};

    char *tmp_units[NUM_OUTPUT_FIELDS] = {"GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
                                            "SAGETreeIndex", "SimulationHaloIndex", "mergeType", "mergeIntoID",
                                            "mergeIntoSnapNum", "dT", "Posx", "Posy", "Posz", "Velx", "Vely", "Velz",
                                            "Spinx", "Spiny", "Spinz", "Len", "Mvir", "CentralMvir", "Rvir", "Vvir",
                                            "Vmax", "VelDisp", "ColdGas", "StellarMass", "BulgeMass", "HotGas", "EjectedMass",
                                            "BlackHoleMass", "ICS", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                            "MetalsHotGas", "MetalsEjectedMass", "MetalsICS", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                            "SfrBulgeZ", "DiskScaleRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
                                            "TimeOfLastMajorMerger", "TimeOfLastMinorMerger", "OutflowRate", "infallMvir",
                                            "infallVvir", "infallVmax"};

    for(int32_t i = 0; i < NUM_OUTPUT_FIELDS; i++) {
        field_names[i] = tmp_names[i];
        field_descriptions[i] = tmp_descriptions[i]; 
        field_units[i] = tmp_units[i]; 
    }

    return EXIT_SUCCESS;
}
