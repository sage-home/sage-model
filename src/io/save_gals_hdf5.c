#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <hdf5.h>
#include <hdf5_hl.h>
#include <math.h>

#include "read_tree_binary.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"
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
// Externally Visible Functions //

#define MAX_ATTRIBUTE_LEN 10000
#define NUM_GALS_PER_BUFFER 1000

#define CREATE_SINGLE_ATTRIBUTE(group_id, attribute_name, attribute_value, h5_dtype) {\
        hid_t macro_dataspace_id = H5Screate(H5S_SCALAR);                      \
        hid_t macro_attribute_id = H5Acreate(group_id, attribute_name, h5_dtype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(macro_attribute_id, h5_dtype, &attribute_value);         \
        H5Aclose(macro_attribute_id);                                    \
        H5Sclose(macro_dataspace_id);                                    \
    }

#define CREATE_STRING_ATTRIBUTE(group_id, attribute_name, attribute_value) {\
        hid_t macro_dataspace_id = H5Screate(H5S_SCALAR);                \
        hid_t atype = H5Tcopy(H5T_C_S1);                                 \
        H5Tset_size(atype, MAX_ATTRIBUTE_LEN-1);                         \
        H5Tset_strpad(atype, H5T_STR_NULLTERM);                          \
        hid_t macro_attribute_id = H5Acreate(group_id, attribute_name, atype, macro_dataspace_id, H5P_DEFAULT, H5P_DEFAULT); \
        H5Awrite(macro_attribute_id, atype, attribute_value);            \
        H5Aclose(macro_attribute_id);                                    \
        H5Sclose(macro_dataspace_id);                                    \
    }

#define CALLOC_GALAXY_OUTPUT_INNER_ARRAY(snap_idx, field_name) { \
    (save_info->buffer_output_gals[snap_idx]).field_name = calloc(save_info->buffer_size, sizeof(*(save_info->buffer_output_gals[snap_idx].field_name)));\
    }

int32_t initialize_hdf5_galaxy_files(const int filenr, struct save_info *save_info, const struct params *run_params)
{

    hid_t prop, dataset_id;
    hid_t file_id, group_id, dataspace_id;
    char buffer[4*MAX_STRING_LEN + 1];

    snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_%d.hdf5", run_params->OutputDir, run_params->FileNameGalaxies, filenr);

    file_id = H5Fcreate(buffer, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if(file_id == -1) {
        fprintf(stderr, "\n\nError: Can't open file `%s'\n\n\n", buffer);
        return FILE_NOT_FOUND;
    }

    // Write here all the attributes/headers/information regarding data types of fields.
    save_info->file_id = file_id;

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
    save_info->group_ids = malloc(sizeof(hid_t) * run_params->NOUT);
    save_info->dataset_ids = malloc(sizeof(hid_t) * NUM_OUTPUT_FIELDS * run_params->NOUT);
    char full_field_name[MAX_STRING_LEN];
    char *name;
    char *description;
    char *unit;
    hsize_t dtype;

    for(int32_t n = 0; n < run_params->NOUT; n++) {

        // First create a snapshot group.
        snprintf(full_field_name, MAX_STRING_LEN - 1, "Snap_%d", run_params->ListOutputSnaps[n]);
        group_id = H5Gcreate2(file_id, full_field_name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        save_info->group_ids[n] = group_id;

        for(int32_t i = 0; i < NUM_OUTPUT_FIELDS; i++) {

            name = field_names[i];
            snprintf(full_field_name, MAX_STRING_LEN-  1,"Snap_%d/%s", run_params->ListOutputSnaps[n], name);

            description = field_descriptions[i];
            unit = field_units[i];
            dtype = field_dtypes[i];

            prop = H5Pcreate(H5P_DATASET_CREATE);
            // Create a dataspace with 0 dimension.  We will extend the datasets when necessary.
            dataspace_id = H5Screate_simple(1, dims, maxdims);
            status = H5Pset_chunk(prop, 1, chunk_dims);

            // Need to specify datatype here.
            dataset_id = H5Dcreate2(file_id, full_field_name, dtype, dataspace_id, H5P_DEFAULT, prop, H5P_DEFAULT);

            save_info->dataset_ids[n*NUM_OUTPUT_FIELDS + i] = dataset_id;

            // Set metadata attributes for each dataset.
            CREATE_STRING_ATTRIBUTE(dataset_id, "Description", description); 
            CREATE_STRING_ATTRIBUTE(dataset_id, "Units", unit); 

            status = H5Pclose(prop);
            status = H5Sclose(dataspace_id);
        }
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

    return EXIT_SUCCESS; 
}


int32_t save_hdf5_galaxies(const int32_t filenr, const int32_t treenr, const int32_t num_gals,
                           struct halo_data *halos, struct halo_aux_data *haloaux, struct GALAXY *halogal,
                           struct save_info *save_info, const struct params *run_params)
{

    int32_t status = EXIT_FAILURE;

    for(int32_t gal_idx = 0; gal_idx < num_gals; gal_idx++) {
        if(haloaux[gal_idx].output_snap_n < 0) {
            continue;
        }

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


int32_t finalize_hdf5_galaxy_files(const int ntrees, struct save_info *save_info,
                                   const struct params *run_params)
{
    herr_t status;

    // I've tried to put this manually into the function but it keeps hanging...
    hsize_t dims[1];
    dims[0] = ntrees;

    for(int32_t snap_idx = 0; snap_idx < run_params->NOUT; snap_idx++) {

        int32_t num_gals_to_write = save_info->num_gals_in_buffer[snap_idx];

        status = trigger_buffer_write(snap_idx, save_info->num_gals_in_buffer[snap_idx],
                                      save_info->tot_ngals[snap_idx], save_info);

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
        hid_t dataset_id = H5Dcreate2(save_info->file_id, field_name, H5T_NATIVE_INT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        status = H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &save_info->forest_ngals[snap_idx]); 

        H5Dclose(dataset_id);
    }

    // Finally let's write some header attributes here.
    // We do this here rather than in ``initialize()`` because we need the number of galaxies per tree.
    hid_t group_id;
    group_id = H5Gcreate(save_info->file_id, "/Header", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    CREATE_SINGLE_ATTRIBUTE(group_id, "Ntrees", ntrees, H5T_NATIVE_INT);

    H5Fclose(save_info->file_id);

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
                                            "BlackHoleMass", "IntraClusterStars", "MetalsColdGas", "MetalsStellarMass", "MetalsBulgeMass",
                                            "MetalsHotGas", "MetalsEjectedMass", "MetalsIntraClusterStars", "SfrDisk", "SfrBulge", "SfrDiskZ",
                                            "SfrBulgeZ", "DiskRadius", "Cooling", "Heating", "QuasarModeBHaccretionMass",
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


#define TREE_MUL_FAC        (1000000000LL)
#define THISTASK_MUL_FAC      (1000000000000000LL)

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

#define EXTEND_AND_WRITE_GALAXY_DATASET(field_name, h5_dtype) { \
    hid_t dataset_id = save_info->dataset_ids[snap_idx*save_info->num_output_fields + field_idx]; \
    status = H5Dset_extent(dataset_id, new_dims); \
    hid_t filespace = H5Dget_space(dataset_id); \
    status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, old_dims, NULL, dims_extend, NULL); \
    hid_t memspace = H5Screate_simple(1, dims_extend, NULL); \
    status = H5Dwrite(dataset_id, h5_dtype, memspace, filespace, H5P_DEFAULT, (save_info->buffer_output_gals[snap_idx]).field_name); \
    field_idx++; \
}

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
    new_dims[0] = old_dims[0] + (hsize_t) num_to_write;

    // This parameter is incremented in every Macro call. It is used to ensure we are
    // accessing the correct dataset.
    int32_t field_idx = 0;

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
    save_info->tot_ngals[snap_idx] += save_info->buffer_size;

    return EXIT_SUCCESS;
}

#undef EXTEND_AND_WRITE_GALAXY_DATASET
