#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <hdf5.h>
#include <math.h>

#include "save_gals_hdf5.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"
#include "../macros.h"
#include "../model_misc.h"
#include "../sage.h"


#ifdef USE_SAGE_IN_MCMC_MODE
#define NUM_OUTPUT_FIELDS 2
#pragma message "Using SAGE in MCMC mode (will only write " STR(NUM_OUTPUT_FIELDS) " fields into the hdf5 file)"
#else
#define NUM_OUTPUT_FIELDS 54
#endif

#define NUM_GALS_PER_BUFFER 8192

// Local Proto-Types //
static int32_t generate_field_metadata(char (*field_names)[MAX_STRING_LEN], char (*field_descriptions)[MAX_STRING_LEN],
                                       char (*field_units)[MAX_STRING_LEN], hsize_t *field_dtypes);

static int32_t prepare_galaxy_for_hdf5_output(const struct GALAXY *g, struct save_info *save_info,
                                              const int32_t output_snap_idx,  const struct halo_data *halos,
                                              const int64_t task_forestnr,
                                              const int64_t original_treenr,
                                              const struct params *run_params);

static int32_t trigger_buffer_write(const int32_t snap_idx, const int32_t num_to_write, const int64_t num_already_written,
                                    struct save_info *save_info, const struct params *run_params);

static int32_t write_header(hid_t file_id, const struct forest_info *forest_info, const struct params *run_params);



// HDF5 is a self-describing data format.  Each dataset will contain a number of attributes to
// describe properties such as units or number of elements. These macros create attributes for a
// single number or a string.
/* MS: 17/9/2019 -- the group_id has already been checked and should be valid at this point */
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
        herr_t status = H5Awrite(macro_attribute_id, h5_dtype, &(attribute_value)); \
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,       \
                                        "Could not write an attribute.\n" \
                                        "The attribute we wanted to create was '" #attribute_name"' and the HDF5 datatype was '" #h5_dtype".\n"); \
        status = H5Aclose(macro_attribute_id);                          \
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,       \
                                        "Could not close an attribute ID.\n" \
                                        "The attribute we wanted to create was '" #attribute_name"' and the HDF5 datatype was '" #h5_dtype".\n"); \
        status = H5Sclose(macro_dataspace_id);                          \
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,       \
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
    herr_t attr_status = H5Tset_size(atype, stringlen);                 \
    CHECK_STATUS_AND_RETURN_ON_FAIL(attr_status, (int32_t) attr_status, \
                                    "Could not set the total size of a datatype when creating a String attribute.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    attr_status = H5Tset_strpad(atype, H5T_STR_NULLTERM);               \
    CHECK_STATUS_AND_RETURN_ON_FAIL(attr_status, (int32_t) attr_status, \
                                    "Could not set the padding when creating a String attribute.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    hid_t macro_attribute_id = H5Acreate(group_id, attribute_name, atype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
    CHECK_STATUS_AND_RETURN_ON_FAIL(macro_attribute_id, (int32_t) macro_attribute_id, \
                                    "Could not create an attribute ID for string.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    attr_status = H5Awrite(macro_attribute_id, atype, attribute_value); \
    CHECK_STATUS_AND_RETURN_ON_FAIL(attr_status, (int32_t) attr_status, \
                                    "Could not write an attribute.\n"   \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    attr_status = H5Aclose(macro_attribute_id);                         \
    CHECK_STATUS_AND_RETURN_ON_FAIL(attr_status, (int32_t) attr_status,                            \
                                    "Could not close an attribute ID.\n" \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    attr_status = H5Tclose(atype);                                      \
    CHECK_STATUS_AND_RETURN_ON_FAIL(attr_status, (int32_t) attr_status, \
                                    "Could not close atype value.\n"    \
                                    "The attribute we wanted to create was '" #attribute_name"'.\n"); \
    attr_status = H5Sclose(macro_dataspace_id);                         \
    CHECK_STATUS_AND_RETURN_ON_FAIL(attr_status, (int32_t) attr_status, \
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
        herr_t dset_status = H5Dwrite(macro_dataset_id, h5_dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer); \
        CHECK_STATUS_AND_RETURN_ON_FAIL(dset_status, (int32_t) dset_status, \
                                        "Failed to write a dataset for field " #field_name".\n" \
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) file_id, \
                                        (int32_t) macro_dataset_id);    \
        dset_status = H5Dclose(macro_dataset_id);                       \
        CHECK_STATUS_AND_RETURN_ON_FAIL(dset_status, (int32_t) dset_status, \
                                        "Failed to close the dataset for field " #field_name".\n" \
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) file_id, \
                                        (int32_t) macro_dataset_id);    \
        dset_status = H5Sclose(macro_dataspace_id);                     \
        CHECK_STATUS_AND_RETURN_ON_FAIL(dset_status, (int32_t) dset_status, \
                                        "Failed to close the dataspace for field " #field_name".\n" \
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n." \
                                        "The dataspace ID was %d.", (int32_t) dims[0], (int32_t) file_id, \
                                        (int32_t) macro_dataspace_id);  \
    }

// Unlike the binary output where we generate an array of output struct instances, the HDF5 workflow has
// a single output struct (for each snapshot) where the **properties** of the struct are arrays.
// This macro callocs (i.e., allocates and zeros) space for these inner arrays.
#define MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name) {        \
        save_info->buffer_output_gals[snap_idx].field_name = malloc(save_info->buffer_size * sizeof(*(save_info->buffer_output_gals[snap_idx].field_name))); \
        if(save_info->buffer_output_gals[snap_idx].field_name == NULL) { \
            fprintf(stderr, "Could not allocate %d elements for the " #field_name" GALAXY_OUTPUT " \
                    "field for output snapshot " #snap_idx"\n", save_info->buffer_size); \
            return MALLOC_FAILURE;                                      \
        }                                                               \
    }

#define FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name) {     \
        free(save_info->buffer_output_gals[snap_idx].field_name);  \
    }

// Externally Visible Functions //

// Creates the HDF5 file, groups and the datasets.  The heirachy for the HDF5 file is
// File->Group->Datasets.  For example, File->"Snap_43"->"StellarMass"->**Data**.
// The handles for all of these are stored in `save_info` so we can write later.
int32_t initialize_hdf5_galaxy_files(const int filenr, struct save_info *save_info, const struct params *run_params)
{
    char buffer[3*MAX_STRING_LEN];

    // Create the file.
    // Use 3*MAX_STRING_LEN because OutputDir and FileNameGalaxies can be MAX_STRING_LEN.  Add a bit more buffer for the filenr and '.hdf5'.
    snprintf(buffer, 3*MAX_STRING_LEN-1, "%s/%s_%d.hdf5", run_params->OutputDir, run_params->FileNameGalaxies, filenr);

    hid_t file_id = H5Fcreate(buffer, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(file_id, FILE_NOT_FOUND,
                                    "Can't open file %s for initialization.\n", buffer);
    save_info->file_id = file_id;

    // Generate the names, description and HDF5 data types for each of the output fields.
    char field_names[NUM_OUTPUT_FIELDS][MAX_STRING_LEN];
    char field_descriptions[NUM_OUTPUT_FIELDS][MAX_STRING_LEN];
    char field_units[NUM_OUTPUT_FIELDS][MAX_STRING_LEN];
    hsize_t field_dtypes[NUM_OUTPUT_FIELDS];

    generate_field_metadata(field_names, field_descriptions, field_units, field_dtypes);

    save_info->num_output_fields = NUM_OUTPUT_FIELDS;
    save_info->name_output_fields = malloc(NUM_OUTPUT_FIELDS * sizeof(save_info->name_output_fields[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->name_output_fields,
                                     "Failed to allocate %d elements of size %zu for save_info->name_output_fields",
                                     NUM_OUTPUT_FIELDS,
                                     sizeof(char *));

    for(int i=0;i<NUM_OUTPUT_FIELDS;i++) {
        save_info->name_output_fields[i] = malloc(MAX_STRING_LEN * sizeof(save_info->name_output_fields[i][0]));
        CHECK_POINTER_AND_RETURN_ON_NULL(save_info->name_output_fields[i],
                                         "Failed to allocate %d elements of size %zu for save_info->name_output_fields[%d]",
                                         NUM_OUTPUT_FIELDS,
                                         sizeof(char), i);
        memcpy(save_info->name_output_fields[i], field_names[i], MAX_STRING_LEN);
    }
    save_info->field_dtypes = malloc(NUM_OUTPUT_FIELDS * sizeof(save_info->field_dtypes[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->field_dtypes,
                                     "Failed to allocate %d elements of size %zu for save_info->field_dtypes",
                                     NUM_OUTPUT_FIELDS,
                                     sizeof(save_info->field_dtypes[0]));

    // We will have groups for each output snapshot, and then inside those groups, a dataset for
    // each field.
    save_info->group_ids = mymalloc(run_params->NumSnapOutputs * sizeof(save_info->group_ids[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->group_ids,
                                     "Failed to allocate %d elements of size %zu for save_info->group_ids", run_params->NumSnapOutputs,
                                     sizeof(*(save_info->group_ids)));

    // A couple of variables before we enter the loop.
    // JS 17/03/19: I've attempted to put these directly into the function calls and things blew up.
    /* Note from MS: That almost certainly means that there is a bug somewhere here (16/9/2019) */
    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {

        hsize_t dims[1] = {0};
        hsize_t maxdims[1] = {H5S_UNLIMITED};
        hsize_t chunk_dims[1] = {NUM_GALS_PER_BUFFER};
        char full_field_name[2*MAX_STRING_LEN];

        // Create a snapshot group.
        snprintf(full_field_name, 2*MAX_STRING_LEN - 1, "Snap_%d", run_params->ListOutputSnaps[snap_idx]);
        hid_t group_id = H5Gcreate2(file_id, full_field_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                        "Failed to create the %s group.\nThe file ID was %d\n", full_field_name,
                                        (int32_t) file_id);
        save_info->group_ids[snap_idx] = group_id;

        const float redshift = run_params->ZZ[run_params->ListOutputSnaps[snap_idx]];
        CREATE_SINGLE_ATTRIBUTE(group_id, "redshift", redshift, H5T_NATIVE_FLOAT);

        for(int32_t field_idx = 0; field_idx < NUM_OUTPUT_FIELDS; field_idx++) {

            // Then create each field inside.
            snprintf(full_field_name, 2*MAX_STRING_LEN - 1,"Snap_%d/%s", run_params->ListOutputSnaps[snap_idx], field_names[field_idx]);

            /* fprintf(stderr, "Creating field '%s' with description '%s' and unit '%s'\n",
               field_names[field_idx], field_descriptions[field_idx], field_units[field_idx]); */

            hid_t prop = H5Pcreate(H5P_DATASET_CREATE);
            CHECK_STATUS_AND_RETURN_ON_FAIL(prop, (int32_t) prop,
                                            "Could not create the property list for output snapshot number %d.\n", snap_idx);

            // Create a dataspace with 0 dimension.  We will extend the datasets before every write.
            hid_t dataspace_id = H5Screate_simple(1, dims, maxdims);
            CHECK_STATUS_AND_RETURN_ON_FAIL(dataspace_id, (int32_t) dataspace_id,
                                            "Could not create a dataspace for output snapshot number %d.\n"
                                            "The requested initial size was %d with an unlimited maximum upper bound.",
                                            snap_idx, (int32_t) dims[0]);

            // To increase reading/writing speed, we chunk the HDF5 file. --JS
            // MS: That is incorrect. We need a resizeable dataset, and that
            //requires chunking
            herr_t status = H5Pset_chunk(prop, 1, chunk_dims);
            CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                            "Could not set the HDF5 chunking for output snapshot number %d.  Chunk size was %d.\n",
                                            snap_idx, (int32_t) chunk_dims[0]);

            // Now create the dataset.
            hid_t dataset_id = H5Dcreate2(file_id, full_field_name, field_dtypes[field_idx], dataspace_id, H5P_DEFAULT, prop, H5P_DEFAULT);
            CHECK_STATUS_AND_RETURN_ON_FAIL(dataset_id, (int32_t) dataset_id,
                                            "Could not create the '%s' dataset.\n", full_field_name);

            // Set metadata attributes for each dataset.
            CREATE_STRING_ATTRIBUTE(dataset_id, "Description", field_descriptions[field_idx], MAX_STRING_LEN);
            CREATE_STRING_ATTRIBUTE(dataset_id, "Units", field_units[field_idx], MAX_STRING_LEN);

            status = H5Dclose(dataset_id);
            CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                            "Failed to close field number %d for output snapshot number %d\n"
                                            "The dataset ID was %d\n", field_idx, snap_idx,
                                            (int32_t) dataset_id);

            status = H5Pclose(prop);
            CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                            "Failed to close the property list for output snapshot number %d.\n", snap_idx);

            status = H5Sclose(dataspace_id);
            CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                            "Failed to close the dataspace for output snapshot number %d.\n", snap_idx);

        }
    }

    // Now for each snapshot, we process `buffer_count` galaxies into RAM for every snapshot before
    // writing a single chunk. Unlike the binary instance where we have a single GALAXY_OUTPUT
    // struct instance per galaxy, here HDF5_GALAXY_OUTPUT is a **struct of arrays**.
    save_info->buffer_size = NUM_GALS_PER_BUFFER;
    save_info->num_gals_in_buffer = mycalloc(run_params->NumSnapOutputs, sizeof(save_info->num_gals_in_buffer[0])); // Calloced because initially no galaxies in buffer.

    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->num_gals_in_buffer,
                                     "Failed to allocate %d elements of size %zu for save_info->num_gals_in_buffer", run_params->NumSnapOutputs,
                                     sizeof(save_info->num_gals_in_buffer[0]));

    save_info->buffer_output_gals = mymalloc(run_params->NumSnapOutputs * sizeof(save_info->buffer_output_gals[0]));

    CHECK_POINTER_AND_RETURN_ON_NULL(save_info->buffer_output_gals,
                                     "Failed to allocate %d elements of size %zu for save_info->buffer_output_gals", run_params->NumSnapOutputs,
                                     sizeof(save_info->buffer_output_gals[0]));

    // Now we need to malloc all the arrays **inside** the GALAXY_OUTPUT struct.
    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {

        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SnapNum);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Type);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, GalaxyIndex);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, CentralGalaxyIndex);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGEHaloIndex);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGETreeIndex);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SimulationHaloIndex);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TaskForestNr);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeType);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeIntoID);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, mergeIntoSnapNum);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, dT);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posx);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posy);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Posz);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Velx);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vely);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Velz);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spinx);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spiny);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Spinz);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Len);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Mvir);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, CentralMvir);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Rvir);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vvir);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Vmax);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, VelDisp);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, ColdGas);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, StellarMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, BulgeMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, HotGas);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, EjectedMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, BlackHoleMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, ICS);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsColdGas);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsStellarMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsBulgeMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsHotGas);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsEjectedMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, MetalsICS);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrDisk);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrBulge);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrDiskZ);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SfrBulgeZ);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, DiskScaleRadius);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Cooling);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Heating);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, QuasarModeBHaccretionMass);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TimeOfLastMajorMerger);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TimeOfLastMinorMerger);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, OutflowRate);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallMvir);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallVvir);
        MALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, infallVmax);
    }

    return EXIT_SUCCESS;
}

#undef MALLOC_GALAXY_OUTPUT_INNER_ARRAY

// Add all the galaxies for this tree to the buffer.  If we hit the buffer limit, write all the
// galaxies to file.
int32_t save_hdf5_galaxies(const int64_t task_forestnr, const int32_t num_gals, struct forest_info *forest_info,
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
        status = prepare_galaxy_for_hdf5_output(&halogal[gal_idx], save_info, snap_idx, halos, task_forestnr,
                                                forest_info->original_treenr[task_forestnr], run_params);
        if(status != EXIT_SUCCESS) {
            return status;
        }
        save_info->num_gals_in_buffer[snap_idx]++;

        // We can't guarantee that this tree will contain enough galaxies to trigger a write.
        // Hence we need to increment this here.
        save_info->forest_ngals[snap_idx][task_forestnr]++;

        // Check to see if we need to write.
        if(save_info->num_gals_in_buffer[snap_idx] == save_info->buffer_size) {
            status = trigger_buffer_write(snap_idx, save_info->buffer_size, save_info->tot_ngals[snap_idx], save_info, run_params);
            if(status != EXIT_SUCCESS) {
                return status;
            }
        }
    }

    return EXIT_SUCCESS;
}


// We may still have galaxies in the buffer.  Here we write them.  Then fill out the final
// attributes that are required, close all the files and release all the datasets/groups/file.
int32_t finalize_hdf5_galaxy_files(const struct forest_info *forest_info, struct save_info *save_info,
                                   const struct params *run_params)
{

    hid_t group_id = H5Gcreate(save_info->file_id, "/TreeInfo", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                    "Failed to create the TreeInfo group.\nThe file ID was %d\n",
                                    (int32_t) save_info->file_id);

    herr_t h5_status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                    "Failed to close the /TreeInfo group."
                                    "The group ID was %d.\n", (int32_t) group_id);

    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {

        // Attributes can only be 64kb in size (strict rule enforced by the HDF5 group).
        // For larger simulations, we will have so many trees, that the number of galaxies per tree
        // array (`save_info->forest_ngals`) will exceed 64kb.  Hence we will write this data to a
        // dataset rather than into an attribute.
        char field_name[MAX_STRING_LEN];
        char description[MAX_STRING_LEN];
        char unit[MAX_STRING_LEN];

        snprintf(field_name, MAX_STRING_LEN - 1, "/TreeInfo/Snap_%d", run_params->ListOutputSnaps[snap_idx]);
        group_id = H5Gcreate2(save_info->file_id, field_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                        "Failed to create the '%s' group.\n"
                                        "The file ID was %d\n", field_name, (int32_t) save_info->file_id);

        h5_status = H5Gclose(group_id);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to close '%s' group."
                                        "The group ID was %d.\n", field_name, (int32_t) group_id);

        // We still have galaxies remaining in the buffer. Need to write them.
        int32_t num_gals_to_write = save_info->num_gals_in_buffer[snap_idx];

        if(num_gals_to_write > 0) {
            h5_status = trigger_buffer_write(snap_idx, num_gals_to_write,
                                             save_info->tot_ngals[snap_idx], save_info, run_params);
            if(h5_status != EXIT_SUCCESS) {
                return h5_status;
            }

            for(int32_t gal_idx = 0; gal_idx < num_gals_to_write; gal_idx++) {

                // JS: We're going to be a bit sneaky here so we don't need to pass the tree number to this function.
                /* MS: 17/9/2019 -- we have to do it the difficult way! */
                int64_t tree = save_info->buffer_output_gals[snap_idx].TaskForestNr[gal_idx];
                if(tree < 0 || tree >= forest_info->nforests_this_task) {
                    fprintf(stderr,"\nError: at snap_idx = %d -> got tree = %"PRId64". num_gals_to_write = %d\n"
                            "Expecting to get tree in the range [0, %"PRId64"), where the upper limit is the number of forests on THIS task\n",
                            snap_idx, tree, num_gals_to_write, forest_info->nforests_this_task);
                    return EXIT_FAILURE;
                }
                save_info->forest_ngals[snap_idx][tree]++;
            }
        }

        // Write attributes showing how many galaxies we wrote for this snapshot.
        CREATE_SINGLE_ATTRIBUTE(save_info->group_ids[snap_idx], "num_gals", save_info->tot_ngals[snap_idx], H5T_NATIVE_LLONG);


        snprintf(field_name, MAX_STRING_LEN -  1, "TreeInfo/Snap_%d/NumGalsPerTreePerSnap", run_params->ListOutputSnaps[snap_idx]);
        snprintf(description, MAX_STRING_LEN -  1, "The number of galaxies per tree at this snapshot.");
        snprintf(unit, MAX_STRING_LEN-  1, "Unitless");

        // JS: I've tried to put this manually into the function but it keeps hanging...
        hsize_t dims[1];
        dims[0] = forest_info->nforests_this_task;;

        hid_t dataspace_id = H5Screate_simple(1, dims, NULL);
        CHECK_STATUS_AND_RETURN_ON_FAIL(dataspace_id, (int32_t) dataspace_id,
                                        "Could not create a dataspace for the number of galaxies per tree.\n"
                                        "The dimensions of the dataspace was %d\n", (int32_t) dims[0]);

        hid_t dataset_id = H5Dcreate2(save_info->file_id, field_name, H5T_NATIVE_INT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(dataset_id, (int32_t) dataset_id,
                                        "Could not create a dataset for the number of galaxies per tree at snapshot = %d.\n"
                                        "The dimensions of the dataset was %d\nThe file id was %d.\n",
                                        snap_idx, (int32_t) dims[0], (int32_t) save_info->file_id);

        h5_status = H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, save_info->forest_ngals[snap_idx]);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to write a dataset for the number of galaxies per tree at snapshot = %d.\n"
                                        "The dimensions of the dataset was %d.\nThe file ID was %d.\n"
                                        "The dataset ID was %d.",
                                        snap_idx, (int32_t) dims[0], (int32_t) save_info->file_id,
                                        (int32_t) dataset_id);

        h5_status = H5Dclose(dataset_id);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to close the dataset for the number of galaxies per tree.\n"
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n."
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) save_info->file_id,
                                        (int32_t) dataset_id);

        h5_status = H5Sclose(dataspace_id);
        CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                        "Failed to close the dataspace for the number of galaxies per tree.\n"
                                        "The dimensions of the dataset was %d\nThe file ID was %d\n."
                                        "The dataset ID was %d.", (int32_t) dims[0], (int32_t) save_info->file_id,
                                        (int32_t) dataset_id);
    }
    group_id = H5Gopen2(save_info->file_id, "/TreeInfo", H5P_DEFAULT);

    /*MS: Now add in the two attributes about the ID generation scheme */
    CREATE_SINGLE_ATTRIBUTE(group_id, "FileNr_Mulfac", run_params->FileNr_Mulfac, H5T_NATIVE_LLONG);
    CREATE_SINGLE_ATTRIBUTE(group_id, "ForestNr_Mulfac", run_params->ForestNr_Mulfac, H5T_NATIVE_LLONG);

    h5_status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(h5_status, (int32_t) h5_status,
                                    "Failed to close the NumGalsPerTree group."
                                    "The group ID was %d.\n", (int32_t) group_id);


    // Finally let's write some header attributes here.
    // We do this here rather than in ``initialize()`` because we need the number of galaxies per tree.
    group_id = H5Gcreate(save_info->file_id, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                    "Failed to create the Header group.\nThe file ID was %d\n",
                                    (int32_t) save_info->file_id);

    int status = write_header(save_info->file_id, forest_info, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the Header group."
                                    "The group ID was %d.\n", (int32_t) group_id);


    // Now we need to ensure we free all of the HDF5 IDs.  The hierachy is File->Groups->Datasets.
    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {
        // Then close the group.
        status = H5Gclose(save_info->group_ids[snap_idx]);
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                        "Failed to close the group for output snapshot number %d\n"
                                        "The group ID was %d\n", snap_idx, (int32_t) save_info->group_ids[snap_idx]);
    }

    // Finally the file itself.
    status = H5Fclose(save_info->file_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the HDF5 file.\nThe file ID was %d\n",
                                    (int32_t) save_info->file_id);

    myfree(save_info->group_ids);

    for(int32_t i=0;i<save_info->num_output_fields;i++) {
        free(save_info->name_output_fields[i]);
    }
    free(save_info->name_output_fields);
    free(save_info->field_dtypes);

    // Free all the other memory.
    myfree(save_info->num_gals_in_buffer);

    for(int32_t snap_idx = 0; snap_idx < run_params->NumSnapOutputs; snap_idx++) {

        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SnapNum);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, Type);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, GalaxyIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, CentralGalaxyIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGEHaloIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SAGETreeIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, SimulationHaloIndex);
        FREE_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, TaskForestNr);
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

    myfree(save_info->buffer_output_gals);

    return EXIT_SUCCESS;
}

#undef FREE_GALAXY_OUTPUT_INNER_ARRAY


int32_t create_hdf5_master_file(const struct params *run_params)
{
    // Only Task 0 needs to do stuff from here.
    if(run_params->ThisTask > 0) {
        return EXIT_SUCCESS;
    }

    hid_t master_file_id, group_id, root_group_id;
    char master_fname[2*MAX_STRING_LEN + 6];
    herr_t status;

    // Create the file.
    snprintf(master_fname, 2*MAX_STRING_LEN + 5, "%s/%s.hdf5", run_params->OutputDir, run_params->FileNameGalaxies);

    master_file_id = H5Fcreate(master_fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(master_file_id, FILE_NOT_FOUND,
                                    "Can't open file %s for master file writing.\n", master_fname);

    // We will keep track of how many galaxies were saved across all files per snapshot.
    // We do this for each snapshot in the simulation, not only those that are output, to allow easy
    // checking of which snapshots were output.
    int64_t *ngals_allfiles_snap = mycalloc(run_params->SimMaxSnaps, sizeof(*(ngals_allfiles_snap))); // Calloced because initially no galaxies.
    CHECK_POINTER_AND_RETURN_ON_NULL(ngals_allfiles_snap,
                                     "Failed to allocate %d elements of size %zu for ngals_allfiles_snaps.", run_params->SimMaxSnaps,
                                     sizeof(*(ngals_allfiles_snap)));

    // The master file will be accessed as (e.g.,) f["Core0"]["Snap_63"]["StellarMass"].
    // Hence we want to store the external links to the **root** group (i.e., "/").
    root_group_id = H5Gopen2(master_file_id, "/", H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(root_group_id, (int32_t) root_group_id,
                                    "Failed to open the root group for the master file.\nThe file ID was %d\n",
                                    (int32_t) master_file_id);

    // At this point, all the files of all other processors have been created. So iterate over the
    // number of processors and create links within this master file to those files.
    char target_fname[3*MAX_STRING_LEN];
    char core_fname[MAX_STRING_LEN];

    for(int32_t task_idx = 0; task_idx < run_params->NTasks; ++task_idx) {

        snprintf(core_fname, MAX_STRING_LEN - 1, "Core_%d", task_idx);
        snprintf(target_fname, 3*MAX_STRING_LEN - 1, "./%s_%d.hdf5", run_params->FileNameGalaxies, task_idx);

        // Make a symlink to the root of the target file.
        status = H5Lcreate_external(target_fname, "/", root_group_id, core_fname, H5P_DEFAULT, H5P_DEFAULT);
        CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                        "Failed to create an external link to file %s from the master file.\n"
                                        "The group ID was %d and the group name was %s\n", target_fname,
                                        (int32_t) root_group_id, core_fname);
    }

    // We've finished with the linking. Now let's create some attributes and datasets inside the header group.
    group_id = H5Gcreate2(master_file_id, "Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(group_id, (int32_t) group_id,
                                    "Failed to create the Header group for the master file.\nThe file ID was %d\n",
                                    (int32_t) master_file_id);

    // When we're writing the header attributes for the master file, we don't have knowledge of trees.
    // So pass a NULL pointer here instead of `forest_info`.
    status = write_header(master_file_id, NULL, run_params);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    status = H5Gclose(group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the Header group for the master file."
                                    "The group ID was %d\n", (int32_t) group_id);

    // Finished creating links.
    status = H5Gclose(root_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close root group for the master file %s\n"
                                    "The group ID was %d and the file ID was %d\n", master_fname,
                                    (int32_t) root_group_id, (int32_t) master_file_id);

    // JS: Cleanup cause we're considerate programmers.
    status = H5Fclose(master_file_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close the Master HDF5 file.\nThe file ID was %d\n",
                                    (int32_t) master_file_id);

    myfree(ngals_allfiles_snap);

    return EXIT_SUCCESS;

}

// Local Functions //

// Give more transparent control over the names of the fields, their descriptions and what units we
// use.  To allow easy comparison with  the binary output format (e.g., for testing purposes), we
// use identical field names to the script that reads the binary data (e.g., 'tests/sagediff.py').
int32_t generate_field_metadata(char (*field_names)[MAX_STRING_LEN], char (*field_descriptions)[MAX_STRING_LEN],
                                char (*field_units)[MAX_STRING_LEN], hsize_t *field_dtypes)
{

#ifdef USE_SAGE_IN_MCMC_MODE
    char tmp_names[NUM_OUTPUT_FIELDS][MAX_STRING_LEN] = {"SnapNum", "StellarMass"};//, "Mvir"};
    char tmp_descriptions[NUM_OUTPUT_FIELDS][MAX_STRING_LEN] = {"", ""};
    char tmp_units[NUM_OUTPUT_FIELDS][MAX_STRING_LEN] = {"", ""};
    hsize_t tmp_dtype[NUM_OUTPUT_FIELDS] = {H5T_NATIVE_INT, H5T_NATIVE_FLOAT};//, H5T_NATIVE_FLOAT};
#else
    char tmp_names[NUM_OUTPUT_FIELDS][MAX_STRING_LEN] = {"SnapNum", "Type", "GalaxyIndex", "CentralGalaxyIndex", "SAGEHaloIndex",
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
    char tmp_descriptions[NUM_OUTPUT_FIELDS][MAX_STRING_LEN] = {"Snapshot the galaxy is located at.",
                                                                "0: Central galaxy of the main FoF halo. 1: Central of a sub-halo. 2: Orphan galaxy that will merge within the current timestep.",
                                                                "Galaxy ID, unique across all trees and files. Calculated as local galaxy number + tree number * factor + file number * factor ",
                                                                "GalaxyIndex of the central galaxy within this galaxy's FoF group.  Calculated the same as 'GalaxyIndex'.",
                                                                "Halo number from the restructured trees. This is different to the tree file because we order the trees. Note: This is the host halo, not necessarily the main FoF halo.",
                                                                "Tree number this galaxy belongs to.", "Most bound particle ID from the tree files.",
                                                                "Denotes how this galaxy underwent a merger. 0: None. 1: Minor merger. 2: Major merger. 3: Disk instability. 4: Disrupt to intra-cluster stars.",
                                                                "Galaxy ID this galaxy is merging into.",
                                                                "Snapshot number of the galaxy this galaxy is merging into.",
                                                                "Time between this snapshot and when the galaxy was last evolved.",
                                                                "Galaxy spatial x position.", "Galaxy spatial y position.",
                                                                "Galaxy spatial z position.", "Galaxy velocity in x direction.",
                                                                "Galaxy velocity in y direction.", "Galaxy velocity in z direction.",
                                                                "Halo spin in the x direction.", "Halo spin in the y direction.",
                                                                "Halo spin in the z direction.", "Number of particles in this galaxy's halo.",
                                                                "Virial mass of this galaxy's halo.", "Virial mass of the main FoF halo.",
                                                                "Virial radius of this galaxy's halo.", "Virial velocity of this galaxy's halo.",
                                                                "Maximum circular speed for this galaxy's halo.", "Velocity dispersion for this galaxy's halo.",
                                                                "Mass of gas in the cold reseroivr.", "Mass of stars.",
                                                                "Mass of stars in the bulge. Bulge stars are added either through disk instabilities or mergers.",
                                                                "Mass of gas in the hot reservoir.", "Mass of gass in the ejected reseroivr.",
                                                                "Mass of this galaxy's black hole.", "Mass of intra-cluster stars.", "Mass of metals in the cold reseroivr.",
                                                                "Mass of metals in stars.", "Mass of metals in the bulge.",
                                                                "Mass of metals in the hot reservoir.", "Mass of metals in the ejected reseroivr.",
                                                                "Mass of metals in intra-cluster stars.", "Star formation rate within the disk.",
                                                                "Star formation rate within the bulge.", "Average metallicity of star-forming disk gas.",
                                                                "Average metallicity of star-forming bulge gas.", "Disk scale radius based on Mo, Shude & White (1998)",
                                                                "Energy rate for gas cooling in the galaxy.", "Energy rate for gas heating in the galaxy.",
                                                                "Mass that this galaxy's black hole accreted during the last time step.",
                                                                "Time since this galaxy had a major merger.", "Time since this galaxy had a minor merger.",
                                                                "Rate at which cold gas is reheated to hot gas.",
                                                                "Virial mass of this galaxy's halo at the previous timestep.",
                                                                "Virial velocity of this galaxy's halo at the previous timestep.",
                                                                "Maximum circular speed of this galaxy's halo at the previous timestep."};

    char tmp_units[NUM_OUTPUT_FIELDS][MAX_STRING_LEN] = {"Unitless", "Unitless", "Unitless", "Unitless", "Unitless",
                                                         "Unitless", "Unitless", "Unitless", "Unitless",
                                                         "Unitless", "Myr", "Mpc/h", "Mpc/h", "Mpc/h", "km/s", "km/s", "km/s",
                                                         "Mpc * km/s", "Mpc * km/s", "Mpc * km/s", "Unitless", "1.0e10 Msun/h", "1.0e10 Msun/h",
                                                         "Mpc/h", "km/s",
                                                         "km/s", "km/s", "1.0e10 Msun/h", "1.0e10 Msun/h", "1.0e10 Msun/h", "1.0e10 Msun/h", "1.0e10 Msun/h",
                                                         "1.0e10 Msun/h", "1.0e10 Msun/h", "1.0e10 Msun/h", "1.0e10 Msun/h", "1.0e10 Msun/h",
                                                         "1.0e10 Msun/h", "1.0e10 Msun/h", "1.0e10 Msun/h", "Msun/yr", "Msun/yr", "Msun/yr",
                                                         "Msun/yr", "Mpc/h", "erg/s", "erg/s", "1.0e10 Msun/h",
                                                         "Myr", "Myr", "Msun/yr", "1.0e10 Msun/yr", "km/s", "km/s"};

    // These are the HDF5 datatypes for each field.
    hsize_t tmp_dtype[NUM_OUTPUT_FIELDS] = {H5T_NATIVE_INT, H5T_NATIVE_INT, H5T_NATIVE_LLONG, H5T_NATIVE_LLONG, H5T_NATIVE_INT,
                                            H5T_NATIVE_INT, H5T_NATIVE_LLONG, H5T_NATIVE_INT, H5T_NATIVE_INT,
                                            H5T_NATIVE_INT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                            H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_INT, H5T_NATIVE_FLOAT,
                                            H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                            H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                            H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                            H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                            H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT,
                                            H5T_NATIVE_FLOAT, H5T_NATIVE_FLOAT};
#endif
    for(int32_t i = 0; i < NUM_OUTPUT_FIELDS; i++) {
        memcpy(field_names[i], tmp_names[i], MAX_STRING_LEN);
        memcpy(field_descriptions[i], tmp_descriptions[i], MAX_STRING_LEN);
        memcpy(field_units[i], tmp_units[i], MAX_STRING_LEN);
        field_dtypes[i] = tmp_dtype[i];
    }

    return EXIT_SUCCESS;
}

// Take all the properties of the galaxy `*g` and add them to the buffered galaxies
// properties `save_info->buffer_output_gals`.
int32_t prepare_galaxy_for_hdf5_output(const struct GALAXY *g, struct save_info *save_info,
                                       const int32_t output_snap_idx,  const struct halo_data *halos,
                                       const int64_t task_forestnr,
                                       const int64_t original_treenr,
                                       const struct params *run_params)
{

    int64_t gals_in_buffer = save_info->num_gals_in_buffer[output_snap_idx];
    //fprintf(stderr, "Task %d, Snap %d, has %"PRId64" gals in buffer.\n", run_params->ThisTask, output_snap_idx, gals_in_buffer);

    save_info->buffer_output_gals[output_snap_idx].SnapNum[gals_in_buffer] = g->SnapNum;

    if(g->Type < SHRT_MIN || g->Type > SHRT_MAX) {
        fprintf(stderr,"Error: Galaxy type = %d can not be represented in 2 bytes\n", g->Type);
        fprintf(stderr,"Converting galaxy type while saving from integer to short will result in data corruption");
        return EXIT_FAILURE;
    }
    save_info->buffer_output_gals[output_snap_idx].Type[gals_in_buffer] = g->Type;

    save_info->buffer_output_gals[output_snap_idx].GalaxyIndex[gals_in_buffer] = g->GalaxyIndex;
    save_info->buffer_output_gals[output_snap_idx].CentralGalaxyIndex[gals_in_buffer] = g->CentralGalaxyIndex;

    save_info->buffer_output_gals[output_snap_idx].SAGEHaloIndex[gals_in_buffer] = g->HaloNr;
    save_info->buffer_output_gals[output_snap_idx].SAGETreeIndex[gals_in_buffer] = original_treenr;
    save_info->buffer_output_gals[output_snap_idx].SimulationHaloIndex[gals_in_buffer] = llabs(halos[g->HaloNr].MostBoundID);
    save_info->buffer_output_gals[output_snap_idx].TaskForestNr[gals_in_buffer] = task_forestnr;

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


/*MS: 23/9/2019 Yes, there appears to be a NULL pointer dereference in the 'SIZEOF_STRUCT_FIELD' but
  the expression is a compile time constant and there is no invalid memory access. That said, C really shouold
 not allow such constructs! */
#define SIZEOF_STRUCT_FIELD(field)    (sizeof(((struct HDF5_GALAXY_OUTPUT *) NULL)->field[0]))

// We created the datasets (e.g., "Snap_43/StellarMass") with 'infinite' dimensions.
// Before we write, we must extend the current dimensions to account for the new values.
// The basic flow for this is:
// Get the dataset ID -> Extend the dataset to the new dimensions -> Get the filespace of the dataset
// -> Select a block of memory that we will add to the current filespace, this is the hyperslab.
// -> Create a dataspace that will hold the data -> Write the data to the group using the new spaces.
// Please refer to the HDF5 documentation for comprehensive explanations. I've probably butchered this...

/* Assumes 'snap_idx', 'field_idx' are set appropriately before invoking the macro */
#define EXTEND_AND_WRITE_GALAXY_DATASET(field_name) {                   \
    char full_field_name[2*MAX_STRING_LEN];                           \
    snprintf(full_field_name, 2*MAX_STRING_LEN - 1,"Snap_%d/%s", run_params->ListOutputSnaps[snap_idx], save_info->name_output_fields[field_idx]); \
    hid_t dataset_id = H5Dopen2(save_info->file_id, full_field_name, H5P_DEFAULT); \
    if(dataset_id < 0) {                                                \
        fprintf(stderr, "Could not access the " #field_name" dataset for output snapshot %d.\n", snap_idx); \
        return (int32_t) dataset_id;                                    \
    }                                                                   \
    hid_t h5_dtype = H5Dget_type(dataset_id);                           \
    if(SIZEOF_STRUCT_FIELD(field_name) != H5Tget_size(h5_dtype)) {      \
        fprintf(stderr,"Error while writing field " #field_name"\n");   \
        fprintf(stderr,"The HDF5 dataset has size %zu bytes but the struct element has size = %zu bytes\n", \
                H5Tget_size(h5_dtype), SIZEOF_STRUCT_FIELD(field_name)); \
        fprintf(stderr,"Perhaps the size of the struct item needs to be updated?\n"); \
        return -1;                                                      \
    }                                                                   \
    status = H5Dset_extent(dataset_id, new_dims);                       \
    if(status < 0) {                                                    \
        fprintf(stderr, "Could not resize the dimensions of the " #field_name" dataset for output snapshot %d.\n" \
                "The dataset ID value is %d. The new dimension values were %d\n", \
                snap_idx, (int32_t) dataset_id, (int32_t) new_dims[0]); \
        return (int32_t) status;                                        \
    }                                                                   \
    hid_t filespace = H5Dget_space(dataset_id);                         \
    if(filespace < 0) {                                                 \
        fprintf(stderr, "Could not retrieve the dataspace of the " #field_name" dataset for output snapshot %d.\n" \
                "The dataset ID value is %d.\n", snap_idx, (int32_t) dataset_id); \
        return (int32_t) filespace;                                     \
    }                                                                   \
    status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL); \
    if(status < 0) {                                                    \
        fprintf(stderr, "Could not select a hyperslab region to add to the filespace of the " #field_name" dataset for output snapshot %d.\n" \
                "The dataset ID value is %d.\n"                         \
                "The old dimensions were %d and we attempted to extend this by %d elements.\n", snap_idx, (int32_t) dataset_id, \
                (int32_t) old_dims[0], (int32_t) dims_extend[0]);       \
        return (int32_t) status;                                        \
    }                                                                   \
    hid_t memspace = H5Screate_simple(1, dims_extend, NULL);            \
    if(memspace < 0) {                                                  \
        fprintf(stderr, "Could not create a new dataspace for the " #field_name" dataset for output snapshot %d.\n" \
                "The length of the dataspace we attempted to created was %d.\n", snap_idx, (int32_t) dims_extend[0]); \
        return (int32_t) memspace;                                      \
    }                                                                   \
    status = H5Dwrite(dataset_id, h5_dtype, memspace, filespace, H5P_DEFAULT, (save_info->buffer_output_gals[snap_idx]).field_name); \
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,           \
            "Could not write the dataset for the " #field_name" field for output snapshot %d.\n" \
            "The dataset ID value is %d.\n"                             \
            "The old dimensions were %d and we attempting to extend (and write to) this by %d elements.\n" \
            "The HDF5 datatype was #h5_dtype.\n", snap_idx, (int32_t) dataset_id, \
            (int32_t) old_dims[0], (int32_t) dims_extend[0]);           \
    status = H5Dclose(dataset_id);                                      \
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,           \
                                    "Failed to close field number %d for output snapshot number %d\n" \
                                    "The dataset ID was %d\n", field_idx, snap_idx, \
                                    (int32_t) dataset_id);              \
    status = H5Sclose(memspace);                                        \
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status, \
                                    "Could not close the memory space for the " #field_name" dataset for output snapshot %d.\n" \
                                    "The dataset ID value is %d.\n"     \
                                    "The old dimensions were %d and we attempting to extend this by %d elements.\n", \
                                    snap_idx, (int32_t) dataset_id, (int32_t) old_dims[0], (int32_t) dims_extend[0]); \
    status = H5Tclose(h5_dtype);                                        \
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,           \
                                    "Error: Failed to close the datatype for the " #field_name" dataset for output snapshot %d.\n" \
                                    "The dataset ID value is %d.\n"     \
                                    "The old dimensions were %d and we attempting to extend this by %d elements.\n", \
                                    snap_idx, (int32_t) dataset_id, (int32_t) old_dims[0], (int32_t) dims_extend[0]); \
    status = H5Sclose(filespace);                                        \
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,           \
                                    "Could not close the filespace for the " #field_name" dataset for output snapshot %d.\n" \
                                    "The dataset ID value is %d.\n"     \
                                    "The old dimensions were %d and we attempting to extend this by %d elements.\n", \
                                    snap_idx, (int32_t) dataset_id, (int32_t) old_dims[0], (int32_t) dims_extend[0]); \
    field_idx++;                                                        \
}


// Extend the length of each dataset in our file and write the data to it.
// We have to specify the number of items to write `num_to_write` because this function is called
// both when we reach the buffer limit and during finalization where we write the remaining galaxies.
int32_t trigger_buffer_write(const int32_t snap_idx, const int32_t num_to_write, const int64_t num_already_written,
                             struct save_info *save_info, const struct params *run_params)
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
#ifdef USE_SAGE_IN_MCMC_MODE
    EXTEND_AND_WRITE_GALAXY_DATASET(SnapNum);
    EXTEND_AND_WRITE_GALAXY_DATASET(StellarMass);
    // EXTEND_AND_WRITE_GALAXY_DATASET(Mvir);
#else
    EXTEND_AND_WRITE_GALAXY_DATASET(SnapNum);
    EXTEND_AND_WRITE_GALAXY_DATASET(Type);
    EXTEND_AND_WRITE_GALAXY_DATASET(GalaxyIndex);
    EXTEND_AND_WRITE_GALAXY_DATASET(CentralGalaxyIndex);
    EXTEND_AND_WRITE_GALAXY_DATASET(SAGEHaloIndex);
    EXTEND_AND_WRITE_GALAXY_DATASET(SAGETreeIndex);
    EXTEND_AND_WRITE_GALAXY_DATASET(SimulationHaloIndex);
    EXTEND_AND_WRITE_GALAXY_DATASET(mergeType);
    EXTEND_AND_WRITE_GALAXY_DATASET(mergeIntoID);
    EXTEND_AND_WRITE_GALAXY_DATASET(mergeIntoSnapNum);
    EXTEND_AND_WRITE_GALAXY_DATASET(dT);
    EXTEND_AND_WRITE_GALAXY_DATASET(Posx);
    EXTEND_AND_WRITE_GALAXY_DATASET(Posy);
    EXTEND_AND_WRITE_GALAXY_DATASET(Posz);
    EXTEND_AND_WRITE_GALAXY_DATASET(Velx);
    EXTEND_AND_WRITE_GALAXY_DATASET(Vely);
    EXTEND_AND_WRITE_GALAXY_DATASET(Velz);
    EXTEND_AND_WRITE_GALAXY_DATASET(Spinx);
    EXTEND_AND_WRITE_GALAXY_DATASET(Spiny);
    EXTEND_AND_WRITE_GALAXY_DATASET(Spinz);
    EXTEND_AND_WRITE_GALAXY_DATASET(Len);
    EXTEND_AND_WRITE_GALAXY_DATASET(Mvir);
    EXTEND_AND_WRITE_GALAXY_DATASET(CentralMvir);
    EXTEND_AND_WRITE_GALAXY_DATASET(Rvir);
    EXTEND_AND_WRITE_GALAXY_DATASET(Vvir);
    EXTEND_AND_WRITE_GALAXY_DATASET(Vmax);
    EXTEND_AND_WRITE_GALAXY_DATASET(VelDisp);
    EXTEND_AND_WRITE_GALAXY_DATASET(ColdGas);
    EXTEND_AND_WRITE_GALAXY_DATASET(StellarMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(BulgeMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(HotGas);
    EXTEND_AND_WRITE_GALAXY_DATASET(EjectedMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(BlackHoleMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(ICS);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsColdGas);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsStellarMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsBulgeMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsHotGas);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsEjectedMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(MetalsICS);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrDisk);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrBulge);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrDiskZ);
    EXTEND_AND_WRITE_GALAXY_DATASET(SfrBulgeZ);
    EXTEND_AND_WRITE_GALAXY_DATASET(DiskScaleRadius);
    EXTEND_AND_WRITE_GALAXY_DATASET(Cooling);
    EXTEND_AND_WRITE_GALAXY_DATASET(Heating);
    EXTEND_AND_WRITE_GALAXY_DATASET(QuasarModeBHaccretionMass);
    EXTEND_AND_WRITE_GALAXY_DATASET(TimeOfLastMajorMerger);
    EXTEND_AND_WRITE_GALAXY_DATASET(TimeOfLastMinorMerger);
    EXTEND_AND_WRITE_GALAXY_DATASET(OutflowRate);
    EXTEND_AND_WRITE_GALAXY_DATASET(infallMvir);
    EXTEND_AND_WRITE_GALAXY_DATASET(infallVvir);
    EXTEND_AND_WRITE_GALAXY_DATASET(infallVmax);
#endif
    // We've performed a write, so future galaxies will overwrite the old data.
    save_info->num_gals_in_buffer[snap_idx] = 0;
    save_info->tot_ngals[snap_idx] += num_to_write;

    return EXIT_SUCCESS;
}

#undef SIZEOF_STRUCT_FIELD
#undef EXTEND_AND_WRITE_GALAXY_DATASET

int32_t write_header(hid_t file_id, const struct forest_info *forest_info, const struct params *run_params) {

    // Inside the "Header" group, we split the attributes up inside different groups for usability.
    hid_t sim_group_id = H5Gcreate2(file_id, "Header/Simulation", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(sim_group_id, (int32_t) sim_group_id,
                                    "Failed to create the Header/Simulation group.\nThe file ID was %d\n",
                                    (int32_t) file_id);

    hid_t runtime_group_id = H5Gcreate2(file_id, "Header/Runtime", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(runtime_group_id, (int32_t) runtime_group_id,
                                    "Failed to create the Header/Runtime group.\nThe file ID was %d\n",
                                    (int32_t) file_id);

    hid_t misc_group_id = H5Gcreate2(file_id, "Header/Misc", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CHECK_STATUS_AND_RETURN_ON_FAIL(misc_group_id, (int32_t) misc_group_id,
                                    "Failed to create the Header/Miscgroup.\nThe file ID was %d\n",
                                    (int32_t) file_id);

    // Simulation information.
    CREATE_STRING_ATTRIBUTE(sim_group_id, "SimulationDir", &run_params->SimulationDir, strlen(run_params->SimulationDir));
    CREATE_STRING_ATTRIBUTE(sim_group_id, "FileWithSnapList", &run_params->FileWithSnapList, strlen(run_params->FileWithSnapList));
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "LastSnapshotNr", run_params->LastSnapshotNr, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "SimMaxSnaps", run_params->SimMaxSnaps, H5T_NATIVE_INT);

    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "omega_matter", run_params->Omega, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "omega_lambda", run_params->OmegaLambda, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "particle_mass", run_params->PartMass, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "hubble_h", run_params->Hubble_h, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "num_simulation_tree_files", run_params->NumSimulationTreeFiles, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(sim_group_id, "box_size", run_params->BoxSize, H5T_NATIVE_DOUBLE);

    // If we're writing the header attributes for the master file, we don't have knowledge of trees.
    if(forest_info != NULL) {
        CREATE_SINGLE_ATTRIBUTE(sim_group_id, "num_trees_this_file", forest_info->nforests_this_task, H5T_NATIVE_LLONG);
        CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "frac_volume_processed", forest_info->frac_volume_processed, H5T_NATIVE_DOUBLE);
    } else {
        const long long nforests_on_master_file = 0;
        CREATE_SINGLE_ATTRIBUTE(sim_group_id, "num_trees_this_file", nforests_on_master_file, H5T_NATIVE_LLONG);

        const double frac_volume_on_master = (run_params->LastFile - run_params->FirstFile + 1)/(double) run_params->NumSimulationTreeFiles;
        CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "frac_volume_processed", frac_volume_on_master, H5T_NATIVE_DOUBLE);
    }

    // Data and version information.
    CREATE_SINGLE_ATTRIBUTE(misc_group_id, "num_cores", run_params->NTasks, H5T_NATIVE_INT);
    CREATE_STRING_ATTRIBUTE(misc_group_id, "sage_data_version", &SAGE_DATA_VERSION, strlen(SAGE_DATA_VERSION));
    CREATE_STRING_ATTRIBUTE(misc_group_id, "sage_version", &SAGE_VERSION, strlen(SAGE_VERSION));
    CREATE_STRING_ATTRIBUTE(misc_group_id, "git_SHA_reference", &GITREF_STR, strlen(GITREF_STR));

    // Output file info.
    CREATE_STRING_ATTRIBUTE(runtime_group_id, "FileNameGalaxies", &run_params->FileNameGalaxies, strlen(run_params->FileNameGalaxies));
    CREATE_STRING_ATTRIBUTE(runtime_group_id, "OutputDir", &run_params->OutputDir, strlen(run_params->OutputDir));
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FirstFile", run_params->FirstFile, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "LastFile", run_params->LastFile, H5T_NATIVE_INT);

    // Recipe flags.
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "SFprescription", run_params->SFprescription, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "AGNrecipeOn", run_params->AGNrecipeOn, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "SupernovaRecipeOn", run_params->SupernovaRecipeOn, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ReionizationOn", run_params->ReionizationOn, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "DiskInstabilityOn", run_params->DiskInstabilityOn, H5T_NATIVE_INT);

    // Model parameters.
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "SfrEfficiency", run_params->SfrEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FeedbackReheatingEpsilon", run_params->FeedbackReheatingEpsilon, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FeedbackEjectionEfficiency", run_params->FeedbackEjectionEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ReIncorporationFactor", run_params->ReIncorporationFactor, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "RadioModeEfficiency", run_params->RadioModeEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "QuasarModeEfficiency", run_params->QuasarModeEfficiency, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "BlackHoleGrowthRate", run_params->BlackHoleGrowthRate, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ThreshMajorMerger", run_params->ThreshMajorMerger, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "ThresholdSatDisruption", run_params->ThresholdSatDisruption, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "Yield", run_params->Yield, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "RecycleFraction", run_params->RecycleFraction, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "FracZleaveDisk", run_params->FracZleaveDisk, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "Reionization_z0", run_params->Reionization_z0, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "Reionization_zr", run_params->Reionization_zr, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "EnergySN", run_params->EnergySN, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "EtaSN", run_params->EtaSN, H5T_NATIVE_DOUBLE);

    // Misc runtime Parameters.
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "UnitLength_in_cm", run_params->UnitLength_in_cm, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "UnitMass_in_g", run_params->UnitMass_in_g, H5T_NATIVE_DOUBLE);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "UnitVelocity_in_cm_per_s", run_params->UnitVelocity_in_cm_per_s, H5T_NATIVE_DOUBLE);

    // Redshift at each snapshot.
    hsize_t dims[1];
    dims[0] = run_params->Snaplistlen;

    CREATE_AND_WRITE_1D_ARRAY(file_id, "Header/snapshot_redshifts", dims, run_params->ZZ, H5T_NATIVE_DOUBLE);

    // Output snapshots.
    dims[0] = run_params->NumSnapOutputs;

    CREATE_AND_WRITE_1D_ARRAY(file_id, "Header/output_snapshots", dims, run_params->ListOutputSnaps, H5T_NATIVE_INT);
    CREATE_SINGLE_ATTRIBUTE(runtime_group_id, "NumOutputs", run_params->NumSnapOutputs, H5T_NATIVE_INT);

    herr_t status = H5Gclose(sim_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close Header/Simulation group.\n"
                                    "The group ID was %d\n", (int32_t) sim_group_id);

    status = H5Gclose(runtime_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close Header/Runtime group.\n"
                                    "The group ID was %d\n", (int32_t) runtime_group_id);

    status = H5Gclose(misc_group_id);
    CHECK_STATUS_AND_RETURN_ON_FAIL(status, (int32_t) status,
                                    "Failed to close Header/Misc group.\n"
                                    "The group ID was %d\n", (int32_t) misc_group_id);

    return EXIT_SUCCESS;
}

#undef CREATE_AND_WRITE_1D_ARRAY
#undef CREATE_SINGLE_ATTRIBUTE
#undef CREATE_STRING_ATTRIBUTE
