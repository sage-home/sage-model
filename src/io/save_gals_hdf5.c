#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <hdf5.h>
#include <hdf5_hl.h>
#include <math.h>

#include "save_gals_hdf5.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"
#include "../macros.h"
#include "../model_misc.h"

#define NUM_OUTPUT_FIELDS 54

// Local Proto-Types //

int32_t generate_field_metadata(char **field_names, char **field_descriptions, char **field_units,
                                hsize_t *field_dtypes);

int32_t prepare_galaxy_for_hdf5_output(int ThisTask, int tree, struct GALAXY *g, struct save_info *save_info,
                                       int32_t output_snap_idx, struct halo_data *halos,
                                       struct halo_aux_data *haloaux, struct GALAXY *halogal,
                                       const struct params *run_params);

int32_t trigger_buffer_write(int32_t snap_idx, int32_t num_to_write, int64_t num_already_written,
                             struct save_info *save_info);

#define MAX_ATTRIBUTE_LEN 10000
#define NUM_GALS_PER_BUFFER 1000

// HDF5 is a self-describing data format.  Each dataset will contain a number of attributes to
// describe properties such as units or number of elements. These macros create attributes for a
// single number or a string.
#define CREATE_SINGLE_ATTRIBUTE(group_id, attribute_name, attribute_value, h5_dtype) { \
    hid_t macro_dataspace_id = H5Screate(H5S_SCALAR);                 \
    CHECK_STATUS(macro_dataspace_id, (int32_t) macro_dataspace_id,    \
                 "Could not create an attribute dataspace.\n"         \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id and the HDF5 datatype was #h5_dtype.\n"); \
    hid_t macro_attribute_id = H5Acreate(group_id, attribute_name, h5_dtype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
    CHECK_STATUS(macro_attribute_id, (int32_t) macro_attribute_Id,    \
                 "Could not create an attribute ID.\n"                \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id and the HDF5 datatype was #h5_dtype.\n"); \
    status = H5Awrite(macro_attribute_id, h5_dtype, &attribute_value);\
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not write an attribute.\n"                    \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id and the HDF5 datatype was #h5_dtype.\n"); \
    status = H5Aclose(macro_attribute_id);                            \
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not close an attribute ID.\n"                 \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id and the HDF5 datatype was #h5_dtype.\n"); \
    status = H5Sclose(macro_dataspace_id);                            \
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not close an attribute dataspace.\n"          \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id and the HDF5 datatype was #h5_dtype.\n"); \
}

#define CREATE_STRING_ATTRIBUTE(group_id, attribute_name, attribute_value) { \
    hid_t macro_dataspace_id = H5Screate(H5S_SCALAR);                 \
    CHECK_STATUS(macro_dataspace_id, (int32_t) macro_dataspace_id,    \
                 "Could not create an attribute dataspace.\n"         \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id and the HDF5 datatype was #h5_dtype.\n"); \
    hid_t atype = H5Tcopy(H5T_C_S1);                                  \
    CHECK_STATUS(atype, (int32_t) atype,                              \
                 "Could not copy an existing data type when creating a String attribute.\n" \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id.\n");                    \
    status = H5Tset_size(atype, MAX_ATTRIBUTE_LEN-1);                 \
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not set the total size of a datatype when creating a String attribute.\n" \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id.\n");                    \
    status = H5Tset_strpad(atype, H5T_STR_NULLTERM);                  \
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not set the padding when creating a String attribute.\n" \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id.\n");                    \
    hid_t macro_attribute_id = H5Acreate(group_id, attribute_name, atype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
    CHECK_STATUS(macro_attribute_id, (int32_t) macro_attribute_id,    \
                 "Could not create an attribute ID.\n"                \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id.\n");                    \
    status = H5Awrite(macro_attribute_id, atype, attribute_value);    \
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not write an attribute.\n"                    \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id.\n");                    \
    status = H5Aclose(macro_attribute_id);                            \
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not close an attribute ID.\n"                 \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id.\n");                    \
    status = H5Sclose(macro_dataspace_id);                            \
    CHECK_STATUS(status, (int32_t) status,                            \
                 "Could not close an attribute dataspace.\n"          \
                 "The attribute we wanted to create was #attribute_name with value #attribute_value.\n" \
                 "The group_id was #group_id #h5_dtype.\n");          \
}

// Unlike the binary output where we generate an array of output struct instances, the HDF5 workflow has
// a single output struct (for each snapshot) where the **properties** of the struct are arrays.
// This macro callocs (i.e., allocates and zeros) space for these inner arrays.
#define CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name) {     \
    save_info->buffer_output_gals[snap_idx].field_name = calloc(save_info->buffer_size, sizeof(*(save_info->buffer_output_gals[snap_idx].field_name))); \
    if(save_info->buffer_output_gals[snap_idx].field_name == NULL) { \
        fprintf(stderr, "Could not allocate %d elements for the #field_name GALAXY_OUTPUT field for output snapshot #snap_idx\n", save_info->buffer_size);\
        return MALLOC_FAILURE;                                       \
    }                                                                \
}

#define FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name) {     \
    free(save_info->buffer_output_gals[snap_idx].field_name);      \
}

// Externally Visible Functions //

// Creates the HDF5 file, groups and the datasets.  The heirachy for the HDF5 file is
// File->Group->Datasets.  For example, File->"Snap_43"->"StellarMass"->**Data**.
// The handles for all of these are stored in `save_info` so we can write later.
int32_t initialize_hdf5_galaxy_files(const int filenr, struct save_info *save_info, const struct params *run_params)
{
    hid_t prop, dataset_id;
    hid_t file_id, group_id, dataspace_id;
    char buffer[3*MAX_STRING_LEN];

    // Create the file.
    // Use 3*MAX_STRING_LEN because OutputDir and FileNameGalaxies can be MAX_STRING_LEN.  Add a bit more buffer for the filenr and '.hdf5'.
    snprintf(buffer, 3*MAX_STRING_LEN-1, "%s/%s_%d.hdf5", run_params->OutputDir, run_params->FileNameGalaxies, filenr);

    file_id = H5Fcreate(buffer, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS(file_id, FILE_NOT_FOUND,
                 "Can't open file %s for initialization.\n", buffer);
    save_info->file_id = file_id;

    // Generate the names, description and HDF5 data types for each of the output fields.
    char *field_names[NUM_OUTPUT_FIELDS];
    char *field_descriptions[NUM_OUTPUT_FIELDS];
    char *field_units[NUM_OUTPUT_FIELDS];
    hsize_t field_dtypes[NUM_OUTPUT_FIELDS];

    generate_field_metadata(field_names, field_descriptions, field_units, field_dtypes);

    save_info->num_output_fields = NUM_OUTPUT_FIELDS;
    save_info->name_output_fields = field_names;
    save_info->field_dtypes = field_dtypes;

    // We will have groups for each output snapshot, and then inside those groups, a dataset for
    // each field.
    save_info->group_ids = malloc(sizeof(hid_t) * run_params->NOUT);
    save_info->dataset_ids = malloc(sizeof(hid_t) * NUM_OUTPUT_FIELDS * run_params->NOUT);

    // A couple of variables before we enter the loop.
    // JS 17/03/19: I've attempted to put these directly into the function calls and things blew up.
    hsize_t dims[1] = {0};
    hsize_t maxdims[1] = {H5S_UNLIMITED};
    hsize_t chunk_dims[1] = {NUM_GALS_PER_BUFFER};
    herr_t status;

    char full_field_name[MAX_STRING_LEN];
    char *name;
    char *description;
    char *unit;
    hsize_t dtype;

    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {

        // Create a snapshot group.
        snprintf(full_field_name, MAX_STRING_LEN - 1, "Snap_%d", run_params->ListOutputSnaps[snap_idx]);
        group_id = H5Gcreate2(file_id, full_field_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS(group_id, (int32_t) group_id,
                     "Failed to create the %s group.\nThe file ID was %d\n", full_field_name,
                     (int32_t) file_id);
        save_info->group_ids[snap_idx] = group_id;

        float redshift = run_params->ZZ[run_params->ListOutputSnaps[snap_idx]];
        CREATE_SINGLE_ATTRIBUTE(group_id, "redshift", redshift, H5T_NATIVE_FLOAT);

        for(int32_t field_idx = 0; field_idx < NUM_OUTPUT_FIELDS; field_idx++) {

            // Then create each field inside.
            name = field_names[field_idx];
            snprintf(full_field_name, MAX_STRING_LEN-  1,"Snap_%d/%s", run_params->ListOutputSnaps[snap_idx], name);

            description = field_descriptions[field_idx];
            unit = field_units[field_idx];
            dtype = field_dtypes[field_idx];

            prop = H5Pcreate(H5P_DATASET_CREATE);
            CHECK_STATUS(prop, (int32_t) prop,
                         "Could not create the property list for output snapshot number %d.\n", snap_idx);

            // Create a dataspace with 0 dimension.  We will extend the datasets before every write.
            dataspace_id = H5Screate_simple(1, dims, maxdims);
            CHECK_STATUS(dataspace_id, (int32_t) dataspace_id,
                         "Could not create a dataspace for output snapshot number %d.\n"
                         "The requested initial size was %d with an unlimited maximum upper bound.",
                         snap_idx, (int32_t) dims[0]);

            // To increase reading/writing speed, we chunk the HDF5 file.
            status = H5Pset_chunk(prop, 1, chunk_dims);
            CHECK_STATUS(status, (int32_t) status,
                         "Could not set the HDF5 chunking for output snapshot number %d.  Chunk size was %d.\n",
                         snap_idx, (int32_t) chunk_dims[0]);

            // Now create the dataset.
            dataset_id = H5Dcreate2(file_id, full_field_name, dtype, dataspace_id, H5P_DEFAULT, prop, H5P_DEFAULT);
            CHECK_STATUS(dataset_id, (int32_t) dataset_id,
                         "Could not create the %s dataset.\n", full_field_name);
            save_info->dataset_ids[snap_idx*NUM_OUTPUT_FIELDS + field_idx] = dataset_id;

            // Set metadata attributes for each dataset.
            CREATE_STRING_ATTRIBUTE(dataset_id, "Description", description); 
            CREATE_STRING_ATTRIBUTE(dataset_id, "Units", unit); 

            status = H5Pclose(prop);
            CHECK_STATUS(status, (int32_t) status,
                         "Failed to close the property list for output snapshot number %d.\n", snap_idx);

            status = H5Sclose(dataspace_id);
            CHECK_STATUS(status, (int32_t) status,
                         "Failed to close the dataspace for output snapshot number %d.\n", snap_idx);
        }
    }

    // Now for each snapshot, we process `buffer_count` galaxies into RAM for every snapshot before
    // writing a single chunk. Unlike the binary instance where we have a single GALAXY_OUTPUT
    // struct instance per galaxy, here HDF5_GALAXY_OUTPUT is a **struct of arrays**. 
    save_info->buffer_size = NUM_GALS_PER_BUFFER;
    save_info->num_gals_in_buffer = calloc(run_params->NOUT, sizeof(*(save_info->num_gals_in_buffer))); 
    save_info->buffer_output_gals = calloc(run_params->NOUT, sizeof(struct HDF5_GALAXY_OUTPUT));

    // Now we need to malloc all the arrays **inside** the GALAXY_OUTPUT struct.
    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {

        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SnapNum);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Type);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, GalaxyIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, CentralGalaxyIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGEHaloIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGETreeIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SimulationHaloIndex);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeType);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeIntoID);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeIntoSnapNum);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, dT);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posx);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posy);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posz);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Velx);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vely);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Velz);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spinx);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spiny);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spinz);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Len);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Mvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, CentralMvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Rvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vmax);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, VelDisp);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, ColdGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, StellarMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, BulgeMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, HotGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, EjectedMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, BlackHoleMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, ICS);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsColdGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsStellarMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsBulgeMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsHotGas);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsEjectedMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsICS);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrDisk);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrBulge);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrDiskZ);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrBulgeZ);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, DiskScaleRadius);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Cooling);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Heating);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, QuasarModeBHaccretionMass);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TimeOfLastMajorMerger);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TimeOfLastMinorMerger);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, OutflowRate);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallMvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallVvir);
        CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallVmax);
    }

    return EXIT_SUCCESS; 
}

#undef CALLOC_GALAXY_OUTPUT_INNER_ARRAY

// Add all the galaxies for this tree to the buffer.  If we hit the buffer limit, write all the
// galaxies to file.
int32_t save_hdf5_galaxies(const int32_t filenr, const int32_t treenr, const int32_t num_gals,
                           struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                           struct save_info *save_info, const struct params *run_params)
{
    int32_t status = EXIT_FAILURE;

    for(int32_t gal_idx = 0; gal_idx < num_gals; gal_idx++) {

        // Only processing galaxies at selected snapshots. This field was generated in `save_galaxies()`.
        if(haloaux[gal_idx].output_snap_n < 0) {
            continue;
        }

        // Add galaxies to buffer.
        int32_t snap_idx = haloaux[gal_idx].output_snap_n;
        prepare_galaxy_for_hdf5_output(filenr, treenr, &halogal[gal_idx], save_info, snap_idx, halos, haloaux, halogal, run_params);
        save_info->num_gals_in_buffer[snap_idx]++;

        // We can't guarantee that this tree will contain enough galaxies to trigger a write.
        // Hence we need to increment this here.
        save_info->forest_ngals[snap_idx][treenr]++;

        // Check to see if we need to write.
        if(save_info->num_gals_in_buffer[snap_idx] == save_info->buffer_size) {
            status = trigger_buffer_write(snap_idx, save_info->buffer_size, save_info->tot_ngals[snap_idx], save_info);
            if(status != EXIT_SUCCESS) {
                return status;
            }
        }
    }

    return EXIT_SUCCESS;
}


// We may still have galaxies in the buffer.  Here we write them.  Then fill out the final
// attributes that are required, close all the files and release all the datasets/groups/file.
int32_t finalize_hdf5_galaxy_files(const int ntrees, struct save_info *save_info,
                                   const struct params *run_params)
{
    herr_t status;

    // I've tried to put this manually into the function but it keeps hanging...
    hsize_t dims[1];
    dims[0] = ntrees;

    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {

        // We still have galaxies remaining in the buffer. Need to write them.
        int32_t num_gals_to_write = save_info->num_gals_in_buffer[snap_idx];

        status = trigger_buffer_write(snap_idx, num_gals_to_write,
                                      save_info->tot_ngals[snap_idx], save_info);
        if(status != EXIT_SUCCESS) {
            return status;
        }

        // We're going to be a bit sneaky here so we don't need to pass the tree number to this function. 
        int32_t tree = save_info->buffer_output_gals[snap_idx].SAGETreeIndex[0];
        save_info->forest_ngals[snap_idx][tree] += num_gals_to_write;

        // Write attributes showing how many galaxies we wrote for this snapshot.
        CREATE_SINGLE_ATTRIBUTE(save_info->group_ids[snap_idx], "ngals", save_info->tot_ngals[snap_idx], H5T_NATIVE_INT);

        // Attributes can only be 64kb in size (strict rule enforced by the HDF5 group).
        // For larger simulations, we will have so many trees, that the number of galaxies per tree
        // array (`save_info->forest_ngals`) will exceed 64kb.  Hence we will write this data to a
        // dataset rather than into an attribute.
        char field_name[MAX_STRING_LEN];
        char description[MAX_STRING_LEN];
        char unit[MAX_STRING_LEN];

        snprintf(field_name, MAX_STRING_LEN -  1, "Snap_%d/NumGalsPerTree", run_params->ListOutputSnaps[snap_idx]);
        snprintf(description, MAX_STRING_LEN -  1, "The number of galaxies per tree.");
        snprintf(unit, MAX_STRING_LEN-  1, "Unitless");

        hid_t dataspace_id = H5Screate_simple(1, dims, NULL);
        CHECK_STATUS(dataspace_id, (int32_t) dataspace_id,
                     "Could not create a dataspace for the number of galaxies per tree.\n"
                     "The dimensions of the dataspace was %d\n", (int32_t) dims[0]);

        hid_t dataset_id = H5Dcreate2(save_info->file_id, field_name, H5T_NATIVE_INT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS(dataspace_id, (int32_t) dataspace_id,
                     "Could not create a dataset for the number of galaxies per tree.\n"
                     "The dimensions of the dataset was %d\nThe file id was %d\n.",
                     (int32_t) dims[0], (int32_t) save_info->file_id);

        status = H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, save_info->forest_ngals[snap_idx]);
        CHECK_STATUS(status, (int32_t) status,
                     "Failed to write a dataset for the number of galaxies per tree.\n"
                     "The dimensions of the dataset was %d\nThe file ID was %d\n."
                     "The dataset ID was %d.", (int32_t) dims[0], (int32_t) save_info->file_id,
                     (int32_t) dataset_id);

        status = H5Dclose(dataset_id);
        CHECK_STATUS(status, (int32_t) status,
                     "Failed to close the dataset for the number of galaxies per tree.\n" 
                     "The dimensions of the dataset was %d\nThe file ID was %d\n."
                     "The dataset ID was %d.", (int32_t) dims[0], (int32_t) save_info->file_id,
                     (int32_t) dataset_id);

        status = H5Sclose(dataspace_id);
        CHECK_STATUS(status, (int32_t) status,
                     "Failed to close the dataspace for the number of galaxies per tree.\n" 
                     "The dimensions of the dataset was %d\nThe file ID was %d\n."
                     "The dataset ID was %d.", (int32_t) dims[0], (int32_t) save_info->file_id,
                     (int32_t) dataset_id);
    }

    // Finally let's write some header attributes here.
    // We do this here rather than in ``initialize()`` because we need the number of galaxies per tree.
    hid_t group_id;
    group_id = H5Gcreate(save_info->file_id, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS(group_id, (int32_t) group_id,
                 "Failed to create the Header group.\nThe file ID was %d\n", (int32_t) save_info->file_id);

    CREATE_SINGLE_ATTRIBUTE(group_id, "Ntrees", ntrees, H5T_NATIVE_INT);

    // Now we need to ensure we free all of the HDF5 IDs.  The heirachy is File->Groups->Datasets.
    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {

        // For each snapshot, close the datasets for each field.
        for(int32_t field_idx = 0; field_idx < save_info->num_output_fields; ++field_idx) {
            int32_t access_idx = snap_idx*NUM_OUTPUT_FIELDS + field_idx;
            status = H5Dclose(save_info->dataset_ids[access_idx]);
            CHECK_STATUS(status, (int32_t) status,
                         "Failed to close field number %d for output snapshot number %d\n"
                         "The dataset ID was %d\n", field_idx, snap_idx,
                         (int32_t) save_info->dataset_ids[access_idx]);
        }

        // Then the group.
        status = H5Gclose(save_info->group_ids[snap_idx]);
        CHECK_STATUS(status, (int32_t) status,
                     "Failed to close the group for output snapshot number %d\n"
                     "The group ID was %d\n", snap_idx, (int32_t) save_info->group_ids[snap_idx]);
    }

    // Finally the file itself.
    status = H5Fclose(save_info->file_id);
    CHECK_STATUS(status, (int32_t) status,
                 "Failed to close the HDF5 file.\nThe file ID was %d\n", (int32_t) save_info->file_id);

    // Then the memory for the IDs.
    free(save_info->dataset_ids);
    free(save_info->group_ids);

    // Free all the other memory.
    free(save_info->num_gals_in_buffer);

    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {

        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SnapNum);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Type);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, GalaxyIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, CentralGalaxyIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGEHaloIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGETreeIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SimulationHaloIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeType);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeIntoID);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeIntoSnapNum);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, dT);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posx);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posy);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posz);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Velx);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vely);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Velz);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spinx);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spiny);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spinz);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Len);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Mvir);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, CentralMvir);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Rvir);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vvir);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vmax);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, VelDisp);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, ColdGas);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, StellarMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, BulgeMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, HotGas);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, EjectedMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, BlackHoleMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, ICS);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsColdGas);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsStellarMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsBulgeMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsHotGas);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsEjectedMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsICS);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrDisk);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrBulge);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrDiskZ);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrBulgeZ);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, DiskScaleRadius);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Cooling);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Heating);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, QuasarModeBHaccretionMass);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TimeOfLastMajorMerger);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TimeOfLastMinorMerger);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, OutflowRate);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallMvir);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallVvir);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallVmax);
    }

    free(save_info->buffer_output_gals);

    return EXIT_SUCCESS;

}

#undef FREE_GALAXY_OUTPUT_INNER_ARRAY


int32_t create_hdf5_master_file(const int32_t ThisTask, const int32_t NTasks, const struct params *run_params)
{
    // Only Task 0 needs to do stuff from here.
    if(ThisTask > 0) {
        fprintf(stderr, "I am Task %d and returning from 'create_hdf5_master_file'.\n", ThisTask);
        return EXIT_SUCCESS;
    }

    hid_t master_file_id, group_id, root_group_id;
    char master_fname[2*MAX_STRING_LEN];
    herr_t status;

    // Create the file.
    snprintf(master_fname, 2*MAX_STRING_LEN, "%s/%s.hdf5", run_params->OutputDir, run_params->FileNameGalaxies);

    master_file_id = H5Fcreate(master_fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS(master_file_id, FILE_NOT_FOUND,
                 "Can't open file %s for master file writing.\n", master_fname);

    // We will keep track of how many galaxies were saved across all files per snapshot.
    int64_t *ngals_allfiles_snap;
    ngals_allfiles_snap = calloc(run_params->MAXSNAPS, sizeof(*(ngals_allfiles_snap)));

    // The master file will be accessed as (e.g.,) f["Core0"]["Snap_63"]["StellarMass"].
    // Hence we want to store the external links to the **root** group (i.e., "/"). 
    root_group_id = H5Gopen2(master_file_id, "/", H5P_DEFAULT);
    CHECK_STATUS(root_group_id, (int32_t) root_group_id,
                 "Failed to open the root group for the master file.\nThe file ID was %d\n",
                 (int32_t) master_file_id);

    // At this point, all the files of all other processors have been created. So iterate over the
    // number of processors and create links within this master file to those files.
    char target_fname[3*MAX_STRING_LEN];
    char core_fname[MAX_STRING_LEN];
    char snap_fname[MAX_STRING_LEN];

    for(int32_t task_idx = 0; task_idx < NTasks; ++task_idx) {

        snprintf(core_fname, MAX_STRING_LEN - 1, "Core_%d", task_idx);
        snprintf(target_fname, 3*MAX_STRING_LEN - 1, "./%s_%d.hdf5", run_params->FileNameGalaxies, task_idx);

        // Make a symlink to the root of the target file.
        status = H5Lcreate_external(target_fname, "/", root_group_id, core_fname, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS(status, (int32_t) status,
                     "Failed to create an external link to file %s from the master file.\n" 
                     "The group ID was %d and the group name was %s\n", target_fname,
                     (int32_t) root_group_id, core_fname);

        // Count number of galaxies saved per snapshot.
        for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; ++snap_idx) {

            snprintf(snap_fname, MAX_STRING_LEN - 1, "Core_%d/Snap_%d", task_idx, run_params->ListOutputSnaps[snap_idx]);
            group_id = H5Gopen(master_file_id, snap_fname, H5P_DEFAULT);
            CHECK_STATUS(group_id, (int32_t) group_id,
                         "Failed to open group %s from within the master file.\nThe file ID was %d\n",
                         snap_fname, (int32_t) master_file_id);

            hid_t attr_id = H5Aopen(group_id, "ngals", H5P_DEFAULT);
            CHECK_STATUS(attr_id, (int32_t) attr_id,
                         "Failed to open the ngals attribute from within the master file.\n"
                         "The group ID was %d for group %s\n", (int32_t) group_id, snap_fname);

            int64_t ngals_this_snap;
            status = H5Aread(attr_id, H5T_NATIVE_LLONG, &ngals_this_snap);
            CHECK_STATUS(status, (int32_t) status,
                         "Failed to read the ngals attribute from within the master file.\n"
                         "The group ID was %d for group %s\n", (int32_t) group_id, snap_fname);

            ngals_allfiles_snap[run_params->ListOutputSnaps[snap_idx]] += ngals_this_snap;

            status = H5Aclose(attr_id);
            CHECK_STATUS(status, (int32_t) status,
                         "Failed to close ngals attribute from within the master file.\n"
                         "The group ID was %d for group %s\n", (int32_t) group_id, snap_fname);

            status = H5Gclose(group_id);
            CHECK_STATUS(status, (int32_t) status,
                         "Failed to close group %s from within the master file.\n"
                         "The group ID was %d.\n", snap_fname, (int32_t) group_id);
        }
    }

    // We've finished with the linking. Now let's create some attributes and datasets inside the header group.
    group_id = H5Gcreate2(master_file_id, "Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS(group_id, (int32_t) group_id,
                 "Failed to create the Header group for the master file.\nThe file ID was %d\n",
                 (int32_t) master_file_id);

    CREATE_SINGLE_ATTRIBUTE(group_id, "Ncores", NTasks, H5T_NATIVE_INT);

    hsize_t dims[1];
    dims[0] = run_params->MAXSNAPS;
    hid_t dataspace_id = H5Screate_simple(1, dims, NULL);
    CHECK_STATUS(dataspace_id, (int32_t) dataspace_id,
                 "Could not create a dataspace for the number of galaxies per snapshot within the master file.\n"
                 "The dimensions of the dataspace was %d\n", (int32_t) dims[0]);

    hid_t dataset_id = H5Dcreate2(master_file_id, "Header/totgals_per_snap", H5T_NATIVE_LLONG, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS(dataset_id, (int32_t) dataset_id,
                 "Could not create a dataset for the number of galaxies per snapshot within the master file.\n"
                 "The dimensions of the dataset was %d\nThe file id was %d\n.",
                 (int32_t) dims[0], (int32_t) master_file_id);

    status = H5Dwrite(dataset_id, H5T_NATIVE_LLONG, H5S_ALL, H5S_ALL, H5P_DEFAULT, ngals_allfiles_snap);
    CHECK_STATUS(status, (int32_t) status,
                 "Failed to write a dataset for the number of galaxies per snapshot within the master file.\n"
                 "The dimensions of the dataset was %d\nThe file ID was %d\n."
                 "The dataset ID was %d.", (int32_t) dims[0], (int32_t) master_file_id,
                 (int32_t) dataset_id);

    status = H5Dclose(dataset_id);
    CHECK_STATUS(status, (int32_t) status,
                 "Failed to close the number of galaxies per snapshot group within the master file.\n"
                 "The dimensions of the dataset was %d\nThe file ID was %d\n."
                 "The dataset ID was %d.", (int32_t) dims[0], (int32_t) master_file_id,
                 (int32_t) dataset_id);

    status = H5Gclose(group_id);
    CHECK_STATUS(status, (int32_t) status,
                 "Failed to close the Header group for master file\n"
                 "The group ID was %d\n", (int32_t) group_id);

    // Finished creating links.
    status = H5Gclose(root_group_id);
    CHECK_STATUS(status, (int32_t) status,
                 "Failed to close root group for the master file %s\n" 
                 "The group ID was %d and the file ID was %d\n", master_fname,
                 (int32_t) root_group_id, (int32_t) master_file_id); 

    // Cleanup cause we're considerate programmers.
    status = H5Fclose(master_file_id);
    CHECK_STATUS(status, (int32_t) status,
                 "Failed to close the Master HDF5 file.\nThe file ID was %d\n",
                 (int32_t) master_file_id);

    free(ngals_allfiles_snap);

    return EXIT_SUCCESS;

}

#undef CREATE_SINGLE_ATTRIBUTE
#undef CREATE_STRING_ATTRIBUTE 

// Local Functions //

// Give more transparent control over the names of the fields, their descriptions and what units we
// use.  To allow easy comparison with  the binary output format (e.g., for testing purposes), we
// use identical field names to the script that reads the binary data (e.g., 'tests/sagediff.py').
int32_t generate_field_metadata(char **field_names, char **field_descriptions, char **field_units, hsize_t *field_dtypes) {

    char *tmp_names[NUM_OUTPUT_FIELDS] = {"SnapNum", "Type", "GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
                                          "SAGETreeIndex", "SimulationHaloIndex", "mergeType", "mergeIntoID",
                                          "mergeIntoSnapNum", "dT", "Posx", "Posy", "Posz", "Velx", "Vely", "Velz",
                                          "Spinx", "Spiny", "Spinz", "Len", "Mvir", "CentralMvir", "Rvir", "Vvir",
                                          "Vmax", "VelDisp", "ColdGas", "StellarMass", "BulgeMass", "HotGas", "EjectedMass",
                                          "BlackHoleMass", "IntraClusterStars", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                          "MetalsHotGas", "MetalsEjectedMass", "MetalsIntraClusterStars", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                          "SfrBulgeZ", "DiskRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
                                          "TimeOfLastMajorMerger", "TimeOfLastMinorMerger", "OutflowRate", "infallMvir",
                                          "infallVvir", "infallVmax"};

    // Must accurately describe what exactly each field is and any special considerations.
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

    // These are the HDF5 datatypes for each field.
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


#define TREE_MUL_FAC        (1000000000LL)
#define THISTASK_MUL_FAC      (1000000000000000LL)

// Take all the properties of the galaxy `*g` and add them to the buffered galaxies
// properties `save_info->buffer_output_gals`.
int32_t prepare_galaxy_for_hdf5_output(int32_t filenr, int32_t treenr, struct GALAXY *g, struct save_info *save_info,
                                       int32_t output_snap_idx,  struct halo_data *halos, struct halo_aux_data *haloaux,
                                       struct GALAXY *halogal, const struct params *run_params)
{

    int64_t gals_in_buffer = save_info->num_gals_in_buffer[output_snap_idx];

    save_info->buffer_output_gals[output_snap_idx].SnapNum[gals_in_buffer] = g->SnapNum;

    if(g->Type < SHRT_MIN || g->Type > SHRT_MAX) {
        fprintf(stderr,"Error: Galaxy type = %d can not be represented in 2 bytes\n", g->Type);
        fprintf(stderr,"Converting galaxy type while saving from integer to short will result in data corruption");
        ABORT(EXIT_FAILURE);
    }
    save_info->buffer_output_gals[output_snap_idx].Type[gals_in_buffer] = g->Type;

    // assume that because there are so many files, the trees per file will be less than 100000
    // required for limits of long long
    if(run_params->LastFile>=10000) {
        save_info->buffer_output_gals[output_snap_idx].GalaxyIndex[gals_in_buffer] = g->GalaxyNr + TREE_MUL_FAC * treenr + (THISTASK_MUL_FAC/10) * filenr;
        save_info->buffer_output_gals[output_snap_idx].CentralGalaxyIndex[gals_in_buffer] = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * treenr + (THISTASK_MUL_FAC/10) * filenr;
        } else {
        save_info->buffer_output_gals[output_snap_idx].GalaxyIndex[gals_in_buffer] = g->GalaxyNr + TREE_MUL_FAC * treenr + THISTASK_MUL_FAC * filenr;
        save_info->buffer_output_gals[output_snap_idx].CentralGalaxyIndex[gals_in_buffer] = halogal[haloaux[halos[g->HaloNr].FirstHaloInFOFgroup].FirstGalaxy].GalaxyNr + TREE_MUL_FAC * treenr + THISTASK_MUL_FAC * filenr;
    }

#undef TREE_MUL_FAC
#undef THISTASK_MUL_FAC

    save_info->buffer_output_gals[output_snap_idx].SAGEHaloIndex[gals_in_buffer] = g->HaloNr; 
    save_info->buffer_output_gals[output_snap_idx].SAGETreeIndex[gals_in_buffer] = treenr; 
    save_info->buffer_output_gals[output_snap_idx].SimulationHaloIndex[gals_in_buffer] = llabs(halos[g->HaloNr].MostBoundID);
#if 0
    o->isFlyby = halos[g->HaloNr].MostBoundID < 0 ? 1:0;
#endif  

    save_info->buffer_output_gals[output_snap_idx].mergeType[gals_in_buffer] = g->mergeType; 
    save_info->buffer_output_gals[output_snap_idx].mergeIntoID[gals_in_buffer] = g->mergeIntoID; 
    save_info->buffer_output_gals[output_snap_idx].mergeIntoSnapNum[gals_in_buffer] = g->mergeIntoSnapNum; 
    save_info->buffer_output_gals[output_snap_idx].dT[gals_in_buffer] = g->dT * run_params->UnitTime_in_s / SEC_PER_MEGAYEAR;

    save_info->buffer_output_gals[output_snap_idx].Posx[gals_in_buffer] = g->Pos[0];
    save_info->buffer_output_gals[output_snap_idx].Posy[gals_in_buffer] = g->Pos[1];
    save_info->buffer_output_gals[output_snap_idx].Posz[gals_in_buffer] = g->Pos[2];

    save_info->buffer_output_gals[output_snap_idx].Velx[gals_in_buffer] = g->Vel[0];
    save_info->buffer_output_gals[output_snap_idx].Vely[gals_in_buffer] = g->Vel[1];
    save_info->buffer_output_gals[output_snap_idx].Velz[gals_in_buffer] = g->Vel[2];

    save_info->buffer_output_gals[output_snap_idx].Spinx[gals_in_buffer] = halos[g->HaloNr].Spin[0]; 
    save_info->buffer_output_gals[output_snap_idx].Spiny[gals_in_buffer] = halos[g->HaloNr].Spin[1]; 
    save_info->buffer_output_gals[output_snap_idx].Spinz[gals_in_buffer] = halos[g->HaloNr].Spin[2];

    save_info->buffer_output_gals[output_snap_idx].Len[gals_in_buffer] = g->Len;
    save_info->buffer_output_gals[output_snap_idx].Mvir[gals_in_buffer] = g->Mvir;
    save_info->buffer_output_gals[output_snap_idx].CentralMvir[gals_in_buffer] = get_virial_mass(halos[g->HaloNr].FirstHaloInFOFgroup, halos, run_params);
    save_info->buffer_output_gals[output_snap_idx].Rvir[gals_in_buffer] = get_virial_radius(g->HaloNr, halos, run_params);  // output the actual Rvir, not the maximum Rvir
    save_info->buffer_output_gals[output_snap_idx].Vvir[gals_in_buffer] = get_virial_velocity(g->HaloNr, halos, run_params);  // output the actual Vvir, not the maximum Vvir
    save_info->buffer_output_gals[output_snap_idx].Vmax[gals_in_buffer] = g->Vmax;
    save_info->buffer_output_gals[output_snap_idx].VelDisp[gals_in_buffer] = halos[g->HaloNr].VelDisp; 

    save_info->buffer_output_gals[output_snap_idx].ColdGas[gals_in_buffer] = g->ColdGas;
    save_info->buffer_output_gals[output_snap_idx].StellarMass[gals_in_buffer] = g->StellarMass;
    save_info->buffer_output_gals[output_snap_idx].BulgeMass[gals_in_buffer] = g->BulgeMass;
    save_info->buffer_output_gals[output_snap_idx].HotGas[gals_in_buffer] = g->HotGas;
    save_info->buffer_output_gals[output_snap_idx].EjectedMass[gals_in_buffer] = g->EjectedMass;
    save_info->buffer_output_gals[output_snap_idx].BlackHoleMass[gals_in_buffer] = g->BlackHoleMass;
    save_info->buffer_output_gals[output_snap_idx].ICS[gals_in_buffer] = g->ICS;

    save_info->buffer_output_gals[output_snap_idx].MetalsColdGas[gals_in_buffer] = g->MetalsColdGas;
    save_info->buffer_output_gals[output_snap_idx].MetalsStellarMass[gals_in_buffer] = g->MetalsStellarMass;
    save_info->buffer_output_gals[output_snap_idx].MetalsBulgeMass[gals_in_buffer] = g->MetalsBulgeMass;
    save_info->buffer_output_gals[output_snap_idx].MetalsHotGas[gals_in_buffer] = g->MetalsHotGas;
    save_info->buffer_output_gals[output_snap_idx].MetalsEjectedMass[gals_in_buffer] = g->MetalsEjectedMass;
    save_info->buffer_output_gals[output_snap_idx].MetalsICS[gals_in_buffer] = g->MetalsICS;

    float tmp_SfrDisk = 0.0;
    float tmp_SfrBulge = 0.0;
    float tmp_SfrDiskZ = 0.0;
    float tmp_SfrBulgeZ = 0.0;

    // NOTE: in Msun/yr
    for(int step = 0; step < STEPS; step++) {
        tmp_SfrDisk += g->SfrDisk[step] * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
        tmp_SfrBulge += g->SfrBulge[step] * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS / STEPS;
        
        if(g->SfrDiskColdGas[step] > 0.0) {
            tmp_SfrDiskZ += g->SfrDiskColdGasMetals[step] / g->SfrDiskColdGas[step] / STEPS;
        }
        
        if(g->SfrBulgeColdGas[step] > 0.0) {
            tmp_SfrBulgeZ += g->SfrBulgeColdGasMetals[step] / g->SfrBulgeColdGas[step] / STEPS;
        }
    }

    save_info->buffer_output_gals[output_snap_idx].SfrDisk[gals_in_buffer] = tmp_SfrDisk;
    save_info->buffer_output_gals[output_snap_idx].SfrBulge[gals_in_buffer] = tmp_SfrBulge;
    save_info->buffer_output_gals[output_snap_idx].SfrDiskZ[gals_in_buffer] = tmp_SfrDiskZ;
    save_info->buffer_output_gals[output_snap_idx].SfrBulgeZ[gals_in_buffer] = tmp_SfrBulgeZ;

    save_info->buffer_output_gals[output_snap_idx].DiskScaleRadius[gals_in_buffer] = g->DiskScaleRadius;

    if (g->Cooling > 0.0) {
        save_info->buffer_output_gals[output_snap_idx].Cooling[gals_in_buffer] = log10(g->Cooling * run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s);
    } else {
        save_info->buffer_output_gals[output_snap_idx].Cooling[gals_in_buffer] = 0.0;
    }

    if (g->Heating > 0.0) {
        save_info->buffer_output_gals[output_snap_idx].Heating[gals_in_buffer] = log10(g->Heating * run_params->UnitEnergy_in_cgs / run_params->UnitTime_in_s);
    } else {
        save_info->buffer_output_gals[output_snap_idx].Heating[gals_in_buffer] = 0.0; 
    }

    save_info->buffer_output_gals[output_snap_idx].QuasarModeBHaccretionMass[gals_in_buffer] = g->QuasarModeBHaccretionMass;

    save_info->buffer_output_gals[output_snap_idx].TimeOfLastMajorMerger[gals_in_buffer] = g->TimeOfLastMajorMerger * run_params->UnitTime_in_Megayears;
    save_info->buffer_output_gals[output_snap_idx].TimeOfLastMinorMerger[gals_in_buffer] = g->TimeOfLastMinorMerger * run_params->UnitTime_in_Megayears;

    save_info->buffer_output_gals[output_snap_idx].OutflowRate[gals_in_buffer] = g->OutflowRate * run_params->UnitMass_in_g / run_params->UnitTime_in_s * SEC_PER_YEAR / SOLAR_MASS;

    //infall properties
    if(g->Type != 0) {
        save_info->buffer_output_gals[output_snap_idx].infallMvir[gals_in_buffer] = g->infallMvir;
        save_info->buffer_output_gals[output_snap_idx].infallVvir[gals_in_buffer] = g->infallVvir;
        save_info->buffer_output_gals[output_snap_idx].infallVmax[gals_in_buffer] = g->infallVmax;
    } else {
        save_info->buffer_output_gals[output_snap_idx].infallMvir[gals_in_buffer] = 0.0;
        save_info->buffer_output_gals[output_snap_idx].infallVvir[gals_in_buffer] = 0.0;
        save_info->buffer_output_gals[output_snap_idx].infallVmax[gals_in_buffer] = 0.0;
    }

    return EXIT_SUCCESS;
}


#undef TREE_MUL_FAC
#undef THISTASK_MUL_FAC

// We created the datasets (e.g., "Snap_43/StellarMass") with 'infinite' dimensions.
// Before we write, we must extend the current dimensions to account for the new values.
// The basic flow for this is:
// Get the dataset ID -> Extend the dataset to the new dimensions -> Get the filespace of the dataset
// -> Select a block of memory that we will add to the current filespace, this is the hyperslab.
// -> Create a dataspace that will hold the data -> Write the data to the group using the new spaces.
// Please refer to the HDF5 documentation for comprehensive explanations. I've probably butchered this...

#define EXTEND_AND_WRITE_GALAXY_DATASET(field_name, h5_dtype) { \
    hid_t dataset_id = save_info->dataset_ids[snap_idx*save_info->num_output_fields + field_idx]; \
    if(dataset_id < 0) {                          \
        fprintf(stderr, "Could not access the #field_name dataset for output snapshot %d.\n" \
                        "The HDF5 datatype was #h5_dtype.\n", snap_idx); \
        return (int32_t) dataset_id;              \
    }                                             \
    status = H5Dset_extent(dataset_id, new_dims); \
    if(status < 0) {                              \
        fprintf(stderr, "Could not resize the dimensions of the #field_name dataset for output snapshot %d.\n" \
                        "The dataset ID value is %d. The new dimension values were #new_dims\n", snap_idx, (int32_t) dataset_id); \
        return (int32_t) status;                  \
    }                                             \
    hid_t filespace = H5Dget_space(dataset_id);   \
    if(filespace < 0) {                           \
        fprintf(stderr, "Could not retrieve the dataspace of the #field_name dataset for output snapshot %d.\n" \
                        "The dataset ID value is %d.\n", snap_idx, (int32_t) dataset_id); \
        return (int32_t) filespace;               \
    }                                             \
    status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL); \
    if(status < 0) {                              \
        fprintf(stderr, "Could not select a hyperslab region to add to the filespace of the #field_name dataset for output snapshot %d.\n" \
                        "The dataset ID value is %d.\n" \
                        "The old dimensions were %d and we attempted to extend this by %d elements.\n", snap_idx, (int32_t) dataset_id, \
                        (int32_t) old_dims[0], (int32_t) dims_extend[0]); \
        return (int32_t) status;                  \
    }                                             \
    hid_t memspace = H5Screate_simple(1, dims_extend, NULL); \
    if(memspace < 0) {                            \
        fprintf(stderr, "Could not create a new dataspace for the #field_name dataset for output snapshot %d.\n" \
                        "The length of the dataspace we attempted to created was %d.\n", snap_idx, (int32_t) dims_extend[0]); \
        return (int32_t) memspace;                \
    }                                             \
    status = H5Dwrite(dataset_id, h5_dtype, memspace, filespace, H5P_DEFAULT, (save_info->buffer_output_gals[snap_idx]).field_name); \
    if(status < 0) {                              \
        fprintf(stderr, "Could not write the dataset for the #field_name field for output snapshot %d.\n" \
                        "The dataset ID value is %d.\n" \
                        "The old dimensions were %d and we attempting to extend (and write to) this by %d elements.\n" \
                        "The HDF5 datatype was #h5_dtype.\n", snap_idx, (int32_t) dataset_id, (int32_t) old_dims[0], (int32_t) dims_extend[0]); \
        return (int32_t) status;                  \
    }                                             \
    status = H5Sclose(memspace);                  \
    if(status < 0) {                              \
        fprintf(stderr, "Could not close the memory space for the #field_name dataset for output snapshot %d.\n" \
                        "The dataset ID value is %d.\n" \
                        "The old dimensions were %d and we attempting to extend this by %d elements.\n", \
                        snap_idx, (int32_t) dataset_id, (int32_t) old_dims[0], (int32_t) dims_extend[0]); \
        return (int32_t) status;                  \
    }                                             \
    field_idx++;                                  \
}

// Extend the length of each dataset in our file and write the data to it.
// We have to specify the number of items to write `num_to_write` because this function is called
// both when we reach the buffer limit and during finalization where we write the remaining galaxies.
int32_t trigger_buffer_write(int32_t snap_idx, int32_t num_to_write, int64_t num_already_written,
                             struct save_info *save_info)
{
    herr_t status;

    // To save the galaxies, we must first extend the size of the dataset to accomodate the new data.

    // JS 16/03/19: I've attempted to put these into the HDF5 function calls directly
    // (rather than specifying as arrays).  However, it causes errors...

    // This is the length which we will extended the dataset by.
    hsize_t dims_extend[1];
    dims_extend[0] = (hsize_t) num_to_write;

    // The previous length of the dataset.
    hsize_t old_dims[1];
    old_dims[0] = (hsize_t) num_already_written;

    // Then this is the new length.
    hsize_t new_dims[1];
    new_dims[0] = old_dims[0] + dims_extend[0]; 

    // This parameter is incremented in every Macro call. It is used to ensure we are
    // accessing the correct dataset.
    int32_t field_idx = 0;

    // We now need to write each property to file.  This is performed in a stack of macros because
    // it's not possible to loop through the members of a struct.
    EXTEND_AND_WRITE_GALAXY_DATASET(SnapNum, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Type, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(GalaxyIndex, H5T_NATIVE_LLONG);
    EXTEND_AND_WRITE_GALAXY_DATASET(CentralGalaxyIndex, H5T_NATIVE_LLONG);
    EXTEND_AND_WRITE_GALAXY_DATASET(SAGEHaloIndex, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(SAGETreeIndex, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(SimulationHaloIndex, H5T_NATIVE_LLONG);
    EXTEND_AND_WRITE_GALAXY_DATASET(mergeType, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(mergeIntoID, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(mergeIntoSnapNum, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(dT, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Posx, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Posy, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Posz, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Velx, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Vely, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Velz, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Spinx, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Spiny, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Spinz, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Len, H5T_NATIVE_INT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Mvir, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(CentralMvir, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Rvir, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Vvir, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Vmax, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(VelDisp, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(ColdGas, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(StellarMass, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(BulgeMass, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(HotGas, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(EjectedMass, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(BlackHoleMass, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(ICS, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsColdGas, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsStellarMass, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsBulgeMass, H5T_NATIVE_FLOAT); 
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsHotGas, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsEjectedMass, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsICS, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrDisk, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrBulge, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrDiskZ, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrBulgeZ, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(DiskScaleRadius, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Cooling, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Heating, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(QuasarModeBHaccretionMass, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(TimeOfLastMajorMerger, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(TimeOfLastMinorMerger, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(OutflowRate, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(infallMvir, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(infallVvir, H5T_NATIVE_FLOAT);
    EXTEND_AND_WRITE_GALAXY_DATASET(infallVmax, H5T_NATIVE_FLOAT);

    // We've performed a write, so future galaxies will overwrite the old data.
    save_info->num_gals_in_buffer[snap_idx] = 0;
    save_info->tot_ngals[snap_idx] += num_to_write; 

    return EXIT_SUCCESS;
}

#undef EXTEND_AND_WRITE_GALAXY_DATASET