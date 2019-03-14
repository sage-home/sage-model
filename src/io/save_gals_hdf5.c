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

#define NUM_OUTPUT_FIELDS 54

// Local Proto-Types //

int32_t generate_field_metadata(char **field_names, char **field_descriptions, char **field_units, hsize_t *field_dtypes);

// Externally Visible Functions //

#define MAX_ATTRIBUTE_LEN 10000
#define NUM_GALS_PER_BUFFER 1000

#define CREATE_SINGLE_ATTRIBUTE(group_id, attribute_name, attribute_value,  h5_dtype) {\
        macro_dataspace_id = H5Screate(H5S_SCALAR);                      \
        macro_attribute_id = H5Acreate(group_id, attribute_name, h5_dtype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(macro_attribute_id, h5_dtype, attribute_value);         \
        H5Aclose(macro_attribute_id);                                    \
        H5Sclose(macro_dataspace_id);                                    \
    }

#define CREATE_STRING_ATTRIBUTE(group_id, attribute_name, attribute_value) {\
        macro_dataspace_id = H5Screate(H5S_SCALAR);                      \
        atype = H5Tcopy(H5T_C_S1);                                       \
        H5Tset_size(atype, MAX_ATTRIBUTE_LEN-1);                         \
        H5Tset_strpad(atype, H5T_STR_NULLTERM);                          \
        macro_attribute_id = H5Acreate(group_id, attribute_name, atype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(macro_attribute_id, atype, attribute_value);            \
        H5Aclose(macro_attribute_id);                                    \
        H5Sclose(macro_dataspace_id);                                    \
    }

#define CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name) { \
    (save_info->buffer_output_gals[snap_idx]).field_name = calloc(save_info->buffer_size, sizeof(*(save_info->buffer_output_gals[snap_idx].field_name)));\
    }

int32_t initialize_hdf5_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info,
                                     const struct params *run_params)
{

    hid_t atype;
    hid_t prop, dataset_id;
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

    hsize_t dims[1] = {0};
    hsize_t maxdims[1] = {H5S_UNLIMITED};
    hsize_t chunk_dims[1] = {NUM_GALS_PER_BUFFER};
    herr_t status;

    char *field_names[NUM_OUTPUT_FIELDS];
    char *field_descriptions[NUM_OUTPUT_FIELDS];
    char *field_units[NUM_OUTPUT_FIELDS];
    hsize_t field_dtypes[NUM_OUTPUT_FIELDS];

    save_info->num_output_fields = NUM_OUTPUT_FIELDS;
    generate_field_metadata(field_names, field_descriptions, field_units, field_dtypes);

    save_info->name_output_fields = field_names;
    save_info->field_dtypes = field_dtypes;

    // First create datasets for each output field. We will have these datasets for EACH snapshot.
    save_info->dataset_ids = malloc(sizeof(hid_t) * NUM_OUTPUT_FIELDS * run_params->NOUT);
    char full_field_name[MAX_STRING_LEN];
    char *name;
    char *description;
    char *unit;
    hsize_t dtype;

    for(int32_t n = 0; n < run_params->NOUT; n++) {

        // First create a snapshot group.
        snprintf(full_field_name, MAX_STRING_LEN - 1, "Snap%d", run_params->ListOutputSnaps[n]);
        H5Gcreate2(file_id, full_field_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); 
        for(int32_t i = 0; i < NUM_OUTPUT_FIELDS; i++) {

            name = field_names[i];
            snprintf(full_field_name, MAX_STRING_LEN-  1,"Snap%d/%s", run_params->ListOutputSnaps[n], name);

            description = field_descriptions[i];
            unit = field_units[i];
            dtype = field_dtypes[i];

            prop = H5Pcreate(H5P_DATASET_CREATE);
            // Create a dataspace with 0 dimension.  We will extend the datasets when necessary.
            dataspace_id = H5Screate_simple(1, dims, maxdims);
            status = H5Pset_chunk(prop, 1, chunk_dims);

            // Need to specify datatype here.
            dataset_id = H5Dcreate2(file_id, full_field_name, save_info->field_dtypes[i], dataspace_id, H5P_DEFAULT, prop, H5P_DEFAULT);

            save_info->dataset_ids[n*NUM_OUTPUT_FIELDS + i] = dataset_id;

            // Set metadata attributes for each dataset.
            CREATE_STRING_ATTRIBUTE(dataset_id, "Description", description); 
            CREATE_STRING_ATTRIBUTE(dataset_id, "Units", unit); 

            status = H5Pclose(prop);
            status = H5Sclose(dataspace_id);
        }
    }

    // Initialize the number of galaxies written for each snapshot.
    save_info->gals_written_snap = malloc(sizeof(*(save_info->gals_written_snap)) * run_params->MAXSNAPS);
    if(save_info->gals_written_snap == NULL) {
        fprintf(stderr, "Could not allocate %.2fMB for the number of galaxies written per snapshot.\n",
                sizeof(*(save_info->gals_written_snap))*run_params->MAXSNAPS / 1024.0);
        return MALLOC_FAILURE;
    }

    for(int32_t i = 0; i < run_params->MAXSNAPS; i++) {
      save_info->gals_written_snap[i] = 0;
    }

    // Now for each snapshot, we process `buffer_count` galaxies into RAM for every snapshot before
    // writing a single chunk. Unlike the binary instance where we have a single GALAXY_OUTPUT
    // struct instance per galaxy, here HDF5_GALAXY_OUTPUT is a **struct of arrays**. 
    save_info->buffer_size = NUM_GALS_PER_BUFFER;
    save_info->num_gals_in_buffer = calloc(run_params->NOUT, sizeof(*(save_info->num_gals_in_buffer))); 
    save_info->buffer_output_gals = calloc(run_params->NOUT, sizeof(struct HDF5_GALAXY_OUTPUT));

    // Now we need to malloc all the arrays **inside** the GALAXY_OUTPUT struct.
    for(int32_t n = 0; n < run_params->NOUT; n++) {

        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SnapNum);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Type);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, GalaxyIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, CentralGalaxyIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SAGEHaloIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SAGETreeIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SimulationHaloIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, mergeType);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, mergeIntoID);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, mergeIntoSnapNum);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, dT);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Posx);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Posy);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Posz);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Velx);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Vely);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Velz);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Spinx);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Spiny);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Spinz);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Len);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Mvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, CentralMvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Rvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Vvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Vmax);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, VelDisp);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, ColdGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, StellarMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, BulgeMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, HotGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, EjectedMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, BlackHoleMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, ICS);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, MetalsColdGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, MetalsStellarMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, MetalsBulgeMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, MetalsHotGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, MetalsEjectedMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, MetalsICS);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SfrDisk);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SfrBulge);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SfrDiskZ);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, SfrBulgeZ);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, DiskScaleRadius);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Cooling);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, Heating);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, QuasarModeBHaccretionMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, TimeOfLastMajorMerger);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, TimeOfLastMinorMerger);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, OutflowRate);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, infallMvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, infallVvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(n, infallVmax);

    }
    //status = H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, tmp_data);

    /*
    hsize_t dimsext[1] = {5};
    hsize_t new_dims[1];
    hsize_t offset[1];
    new_dims[0] = dims[0] + dimsext[0];
    status = H5Dset_extent(dataset_id, new_dims);

    hid_t filespace = H5Dget_space(dataset_id);
    
    offset[0] = 5;
    status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, dimsext, NULL);

    hid_t memspace = H5Screate_simple(1, dimsext, NULL);

    status = H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, memspace, filespace, H5P_DEFAULT, tmp_data); 

    status = H5Dclose(dataset_id);
    */
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

#undef CREATE_SINGLE_ATTRIBUTE
#undef CREATE_STRING_ATTRIBUTE 
#undef CALLOC_GALAXY_OUTPUT_INNER_ARRAY

// Local Functions //

int32_t generate_field_metadata(char **field_names, char **field_descriptions, char **field_units, hsize_t *field_dtypes) {

    // These need to be indentical to the fields in GALAXY_OUTPUT struct.

    char *tmp_names[NUM_OUTPUT_FIELDS] = {"SnapNum", "Type", "GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
                                            "SAGETreeIndex", "SimulationHaloIndex", "mergeType", "mergeIntoID",
                                            "mergeIntoSnapNum", "dT", "Posx", "Posy", "Posz", "Velx", "Vely", "Velz",
                                            "Spinx", "Spiny", "Spinz", "Len", "Mvir", "CentralMvir", "Rvir", "Vvir",
                                            "Vmax", "VelDisp", "ColdGas", "StellarMass", "BulgeMass", "HotGas", "EjectedMass",
                                            "BlackHoleMass", "ICS", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                            "MetalsHotGas", "MetalsEjectedMass", "MetalsICS", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                            "SfrBulgeZ", "DiskScaleRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
                                            "TimeOfLastMajorMerger", "TimeOfLastMinorMerger", "OutflowRate", "infallMvir",
                                            "infallVvir", "infallVmax"};

    char *tmp_descriptions[NUM_OUTPUT_FIELDS] = {"SnapNum", "Type", "GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
                                            "SAGETreeIndex", "SimulationHaloIndex", "mergeType", "mergeIntoID",
                                            "mergeIntoSnapNum", "dT", "Posx", "Posy", "Posz", "Velx", "Vely", "Velz",
                                            "Spinx", "Spiny", "Spinz", "Len", "Mvir", "CentralMvir", "Rvir", "Vvir",
                                            "Vmax", "VelDisp", "ColdGas", "StellarMass", "BulgeMass", "HotGas", "EjectedMass",
                                            "BlackHoleMass", "ICS", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                            "MetalsHotGas", "MetalsEjectedMass", "MetalsICS", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                            "SfrBulgeZ", "DiskScaleRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
                                            "TimeOfLastMajorMerger", "TimeOfLastMinorMerger", "OutflowRate", "infallMvir",
                                            "infallVvir", "infallVmax"};

    char *tmp_units[NUM_OUTPUT_FIELDS] = {"SnapNum", "Type", "GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
                                            "SAGETreeIndex", "SimulationHaloIndex", "mergeType", "mergeIntoID",
                                            "mergeIntoSnapNum", "dT", "Posx", "Posy", "Posz", "Velx", "Vely", "Velz",
                                            "Spinx", "Spiny", "Spinz", "Len", "Mvir", "CentralMvir", "Rvir", "Vvir",
                                            "Vmax", "VelDisp", "ColdGas", "StellarMass", "BulgeMass", "HotGas", "EjectedMass",
                                            "BlackHoleMass", "ICS", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                            "MetalsHotGas", "MetalsEjectedMass", "MetalsICS", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                            "SfrBulgeZ", "DiskScaleRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
                                            "TimeOfLastMajorMerger", "TimeOfLastMinorMerger", "OutflowRate", "infallMvir",
                                            "infallVvir", "infallVmax"};

    // May be able to determine this properly as `tmp_names` has all the output fields.
    // Could then use 'if' statements...
    hsize_t tmp_dtype[NUM_OUTPUT_FIELDS] = {H5T_NATIVE_INT, H5T_NATIVE_INT, H5T_NATIVE_LLONG, H5T_NATIVE_LLONG, H5T_NATIVE_INT, 
                                         H5T_NATIVE_INT, H5T_NATIVE_LLONG, H5T_NATIVE_INT, H5T_NATIVE_INT, 
                                         H5T_NATIVE_INT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                         H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_INT, H5T_NATIVE_FLOAT,
                                         H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                         H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                         H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                         H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                         H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                         H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT};

    for(int32_t i = 0; i < NUM_OUTPUT_FIELDS; i++) {
        field_names[i] = tmp_names[i];
        field_descriptions[i] = tmp_descriptions[i]; 
        field_units[i] = tmp_units[i];
        field_dtypes[i] = tmp_dtype[i];
    }

    return EXIT_SUCCESS;
}
