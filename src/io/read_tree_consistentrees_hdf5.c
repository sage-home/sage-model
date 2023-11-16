#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "read_tree_consistentrees_hdf5.h"
#include "hdf5_read_utils.h"
#include "forest_utils.h"
#include "ctrees_utils.h"

#include "../core_mymalloc.h"
#include "../core_utils.h"


#include <hdf5.h>

struct ctrees_forestinfo {
    int64_t forestid;
    int64_t foresthalosoffset;
    int64_t forestnhalos;
    int64_t forestntrees;
};

// static char galaxy_property_names[num_galaxy_props][MAX_STRING_LEN];
static void get_forest_metadata_filename(char *metadata_filename, const size_t len, struct params *run_params);
static int read_contiguous_forest_ctrees_h5(hid_t h5_forests_group, const hsize_t nhalos, const hsize_t halosoffset,
                                            const char *snap_field_name, const int8_t snap_field_is_double,
                                            struct halo_data *halos);
static void convert_ctrees_conventions_to_lht(struct halo_data *halos, const int64_t nhalos,
                                              const int32_t snap_offset, const double part_mass);


void get_forest_metadata_filename(char *metadata_filename, const size_t len, struct params *run_params)
{
    snprintf(metadata_filename, len, "%s/%s%s", run_params->SimulationDir, run_params->TreeName, run_params->TreeExtension);
    return;
}


/* Externally visible Functions */
int setup_forests_io_ctrees_hdf5(struct forest_info *forests_info, const int ThisTask, const int NTasks, struct params *run_params)
{
    const int firstfile = run_params->FirstFile;
    const int lastfile = run_params->LastFile;
    const int numfiles = lastfile - firstfile + 1;/* This is total number of files to process across all tasks */
    if(numfiles <= 0) {
        fprintf(stderr,"Error: Need at least one file to process. Calculated numfiles = %d (firstfile = %d, lastfile = %d)\n",
                numfiles, run_params->FirstFile, run_params->LastFile);
        return INVALID_OPTION_IN_PARAMS;
    }
    struct ctrees_h5_info *ctr_h5 = &(forests_info->ctr_h5);
    char metadata_fname[4*MAX_STRING_LEN];

    /* Now read in the meta-data info about the forests */
    get_forest_metadata_filename(metadata_fname, sizeof(metadata_fname), run_params);

    ctr_h5->meta_fd = H5Fopen(metadata_fname, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (ctr_h5->meta_fd < 0) {
        fprintf(stderr,"Error: On ThisTask = %d can't open file metadata file '%s'\n", ThisTask, metadata_fname);
        return FILE_NOT_FOUND;
    }

#define READ_CTREES_ATTRIBUTE(hid, groupname, attrname, dst) {           \
            herr_t h5_status = read_attribute(hid, groupname, attrname, (void *) &dst, sizeof(dst)); \
            if(h5_status < 0) {                                         \
                return (int) h5_status;                                 \
            }                                                           \
        }

    int64_t check_totnfiles;
    READ_CTREES_ATTRIBUTE(ctr_h5->meta_fd, "/", "Nfiles", check_totnfiles);
    XRETURN(check_totnfiles >= 1, INVALID_VALUE_READ_FROM_FILE,
            "Error: Expected total number of files to be at least 1. However, reading in from "
            "metadata file ('%s') shows check_totnfiles = %"PRId64"\n. Exiting...\n",
            metadata_fname, check_totnfiles);
    XRETURN(numfiles <= check_totnfiles, INVALID_VALUE_READ_FROM_FILE,
            "Error: The requested number of files to process spans from [%d, %d] for a total %d numfiles\n"
            "However, the original tree file is only split into %"PRId64" files (which is smaller than the requested files)\n"
            "The metadata file is ('%s') \nExiting...\n",
            firstfile, lastfile, numfiles, check_totnfiles, metadata_fname);

    /* If we are not processing all the files, print an info message to -stdout- */
    if(ThisTask == 0) {
        fprintf(stdout, "Info: Processing %d files out of a total of %"PRId64" files written out\n",
                numfiles, check_totnfiles);
        fflush(stdout);
    }
    int64_t totnfiles = lastfile + 1;/* Wastes space but makes for easier indexing */
    ctr_h5->h5_file_groups = mycalloc(totnfiles, sizeof(ctr_h5->h5_file_groups[0]));
    XRETURN(ctr_h5->h5_file_groups != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold the hdf5 file handles for all files being processed on this task each forest (%"PRId64" items each of size %zu bytes)\n",
            totnfiles, sizeof(ctr_h5->h5_file_groups[0]));

    ctr_h5->h5_forests_group = mycalloc(totnfiles, sizeof(ctr_h5->h5_forests_group[0]));
    XRETURN(ctr_h5->h5_forests_group != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold the hdf5 file handles for the forest groups within files being processed on this task (%"PRId64" items each of size %zu bytes)\n",
            totnfiles, sizeof(ctr_h5->h5_forests_group[0]));
    ctr_h5->contig_halo_props = mycalloc(totnfiles, sizeof(ctr_h5->contig_halo_props[0]));
    XRETURN(ctr_h5->contig_halo_props != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold the contiguous halo attribute for each file processed on this task (%"PRId64" items each of size %zu bytes)\n",
            totnfiles, sizeof(ctr_h5->contig_halo_props[0]));

    for(int32_t ifile=firstfile;ifile<=lastfile;ifile++) {
        char file_group_name[MAX_STRING_LEN];
        snprintf(file_group_name, MAX_STRING_LEN-1, "File%d", ifile);
        hid_t h5_file_group = H5Gopen(ctr_h5->meta_fd, file_group_name, H5P_DEFAULT);
        XRETURN(h5_file_group >= 0, -HDF5_ERROR,
                "Error: Could not open the file group = `%s` during the initial setup of the forests\n",
                file_group_name);
        ctr_h5->h5_file_groups[ifile] = h5_file_group;

        hid_t h5_forest_group = H5Gopen(h5_file_group, "Forests", H5P_DEFAULT);
        ctr_h5->h5_forests_group[ifile] = h5_forest_group;
        // fprintf(stderr,"In %s> ifile = %d h5_forest_group = %lu\n", __FUNCTION__, ifile, h5_forest_group);
    }

    int64_t totnforests = 0;
    READ_CTREES_ATTRIBUTE(ctr_h5->meta_fd, "/", "TotNforests", totnforests);
    XRETURN(totnforests >= 1, INVALID_VALUE_READ_FROM_FILE,
            "Error: Expected total number of forests to be at least 1. However, reading in from "
            "metadata file ('%s') shows totnforests = %"PRId64"\n. Exiting...\n",
            metadata_fname, totnforests);

    int64_t *totnforests_per_file = mycalloc(totnfiles, sizeof(*totnforests_per_file));
    XRETURN(totnforests_per_file != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold number of forests per file (%"PRId64" items of size %zu bytes)\n",
            totnfiles, sizeof(*totnforests_per_file));

    totnforests = 0;
    /* Now figure out the number of forests per requested file (there might be more
       forest files but we will ignore forests in those files for this particular run)  */
    for(int32_t ifile=firstfile;ifile<=lastfile;ifile++) {
        char dataset_name[MAX_STRING_LEN];
        snprintf(dataset_name, MAX_STRING_LEN, "File%d", ifile);
        int64_t nforests_this_file;
        READ_CTREES_ATTRIBUTE(ctr_h5->meta_fd, dataset_name, "Nforests", nforests_this_file);

        XRETURN(nforests_this_file >= 1, INVALID_VALUE_READ_FROM_FILE,
                "Error: Expected the number of forests in this file to be at least 1. However, reading in from "
                "forest file # (%d, dataset name = '%s') shows nforests = %"PRId64"\n. Exiting...\n",
                ifile, dataset_name, nforests_this_file);
        totnforests_per_file[ifile] = nforests_this_file;
        totnforests += nforests_this_file;
    }
    // Assign the total number of forests processed across all the input files
    // Note this might be smaller than the actual number of forests in the simulation
    forests_info->totnforests = totnforests;

    const int need_nhalos_per_forest = run_params->ForestDistributionScheme == uniform_in_forests ? 0:1;
    int64_t *nhalos_per_forest = NULL;
    if(need_nhalos_per_forest) {
        nhalos_per_forest = mycalloc(totnforests, sizeof(*nhalos_per_forest));
        for(int32_t ifile=firstfile;ifile<=lastfile;ifile++) {
            const int64_t nforests_this_file = totnforests_per_file[ifile];
            hid_t h5_file_grp = ctr_h5->h5_file_groups[ifile];

            char dataset_name[MAX_STRING_LEN];
            snprintf(dataset_name, MAX_STRING_LEN, "ForestInfo");
            hid_t finfo_dset = H5Dopen2(h5_file_grp, dataset_name, H5P_DEFAULT);
            XRETURN(finfo_dset >= 0, -HDF5_ERROR,
                    "Error encountered when trying to open up dataset '%s' in file %d\n",
                    dataset_name, ifile);
            hid_t nhalos_dtype = H5Tcreate(H5T_COMPOUND, sizeof(int64_t));
            XRETURN(nhalos_dtype >= 0, -HDF5_ERROR,
                    "Error when creating the compound datatype to read in nhalos per forest (file = %d)\n",
                    ifile);
            herr_t status = H5Tinsert(nhalos_dtype, "ForestNhalos", 0, H5T_NATIVE_INT64);
            XRETURN(status >=0, -HDF5_ERROR,
                    "Error when inserting the 'ForestNhalos' field into the compound datatype (file = %d)\n",
                    ifile);

            status = H5Dread(finfo_dset, nhalos_dtype, H5S_ALL, H5S_ALL, H5P_DEFAULT, nhalos_per_forest);
            if(status < 0) {
                fprintf(stderr,"Error when reading the 'ForestNhalos' field out of the %s in file %d\n",
                        dataset_name, ifile);
                return status;
            }
            XRETURN(H5Tclose(nhalos_dtype) >= 0, -HDF5_ERROR,
                    "Error when closing the compound datatype for reading in 'ForestNhalos' field within the '%s' dataset (file = %d)\n",
                    dataset_name, ifile);
            XRETURN(H5Dclose(finfo_dset) >= 0, -HDF5_ERROR,
                    "Error encountered when closing the dataset '%s' in file %d\n",
                    dataset_name, ifile);

            nhalos_per_forest += nforests_this_file;
        }
        //Move back the pointer to the allocated address
        nhalos_per_forest -= totnforests;
    }

    int64_t nforests_this_task, start_forestnum;
    int status = distribute_weighted_forests_over_ntasks(totnforests, nhalos_per_forest,
                                                         run_params->ForestDistributionScheme, run_params->Exponent_Forest_Dist_Scheme,
                                                         NTasks, ThisTask, &nforests_this_task, &start_forestnum);
    if(status != EXIT_SUCCESS) {
        return status;
    }
    if(need_nhalos_per_forest) {
        free(nhalos_per_forest);
    }
    const int64_t end_forestnum = start_forestnum + nforests_this_task; /* not inclusive, i.e., do not process forestnr == end_forestnum */
    /* fprintf(stderr,"Thistask = %d start_forestnum = %"PRId64" end_forestnum = %"PRId64"\n", ThisTask, start_forestnum, end_forestnum); */

    ctr_h5->nforests = nforests_this_task;
    //ctr_h5->start_forestnum = start_forestnum;
    forests_info->nforests_this_task = nforests_this_task;/* Note: Number of forests to process on this task is also stored at the container struct*/

    int64_t *num_forests_to_process_per_file = mycalloc(totnfiles, sizeof(num_forests_to_process_per_file[0]));
    int64_t *start_forestnum_to_process_per_file = mymalloc(totnfiles * sizeof(start_forestnum_to_process_per_file[0]));
    if(num_forests_to_process_per_file == NULL || start_forestnum_to_process_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests that need to be processed per file (on thistask=%d)\n", ThisTask);
        perror(NULL);
        return MALLOC_FAILURE;
    }

    // Now for each task, we know the starting forest number it needs to start reading from.
    // So let's determine what file and forest number within the file each task needs to start/end reading from.
    int start_filenum = -1, end_filenum = -1;
    status = find_start_and_end_filenum(start_forestnum, end_forestnum,
                                        totnforests_per_file, totnforests,
                                        firstfile, lastfile,
                                        ThisTask, NTasks,
                                        num_forests_to_process_per_file, start_forestnum_to_process_per_file,
                                        &start_filenum, &end_filenum);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    /* So we have the correct files */
    ctr_h5->totnfiles = totnfiles;/* the number of files to be processed across all tasks. MS: Still wastes space - set to lastfile + 1 rather than the correct "(lastfile - firstfile + 1)" */
    ctr_h5->start_filenum = start_filenum;
    ctr_h5->end_filenum = end_filenum;

    /* We need to track which file each forest is in for two reasons -- i) to actually read from the correct file and ii) to create unique IDs */
    forests_info->FileNr = mymalloc(nforests_this_task * sizeof(forests_info->FileNr[0]));
    XRETURN(forests_info->FileNr != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold the file numbers for each forest (%"PRId64" items each of size %zu bytes)\n",
            nforests_this_task, sizeof(forests_info->FileNr[0]));
    forests_info->original_treenr = mymalloc(nforests_this_task * sizeof(forests_info->original_treenr[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->original_treenr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->original_treenr", nforests_this_task,
                                     sizeof(*(forests_info->original_treenr)));


    // worthwhile to hoist into a new function
    for(int64_t iforest=0;iforest<nforests_this_task;iforest++) {
        forests_info->FileNr[iforest] = -1;
        forests_info->original_treenr[iforest] = -1;
    }

    /* Now fill up the arrays that are of shape (nforests, ) -- FileNr, original_treenr */
    int32_t curr_filenum = start_filenum;
    int64_t endforestnum_in_currfile = totnforests_per_file[start_filenum] - start_forestnum_to_process_per_file[start_filenum];
    int64_t offset = 0;
    for(int64_t iforest=0;iforest<nforests_this_task;iforest++) {
        if(iforest >= endforestnum_in_currfile) {
            fprintf(stderr,"LOG: ThisTask = %d iforest = %"PRId64" start_filenum = %d, curr_filenum = %d endforestnum_in_currfile = %"PRId64" offset = %"PRId64"\n", run_params->ThisTask, iforest, start_filenum, curr_filenum, endforestnum_in_currfile, offset);
            offset = endforestnum_in_currfile;
            curr_filenum++;
            endforestnum_in_currfile += totnforests_per_file[curr_filenum];
        }
        forests_info->FileNr[iforest] = curr_filenum;
        if(curr_filenum == start_filenum) {
            forests_info->original_treenr[iforest] = iforest + start_forestnum_to_process_per_file[curr_filenum];
        } else {
            forests_info->original_treenr[iforest] = iforest - offset;
        }
    }


    /* Perform consistency checks with all the files */
    for(int32_t ifile=start_filenum;ifile<=end_filenum;ifile++) {
        char contig_attr_name[] = "contiguous-halo-props";
        int8_t contig_halo_props;
        char file_group_name[MAX_STRING_LEN];
        snprintf(file_group_name, MAX_STRING_LEN-1, "File%d", ifile);
        herr_t h5_att_status = read_attribute(ctr_h5->meta_fd, file_group_name, contig_attr_name,
                                   &contig_halo_props, sizeof(contig_halo_props));
        if (h5_att_status < 0) {
            fprintf(stderr,"Error: Could not read attribute '%s' from group '%s'\n", contig_attr_name, file_group_name);
            return (int64_t) h5_att_status;
        }
        ctr_h5->contig_halo_props[ifile] = contig_halo_props;

        double om, ol, little_h;
        READ_CTREES_ATTRIBUTE(ctr_h5->h5_file_groups[ifile], "simulation_params", "Omega_M", om);
        READ_CTREES_ATTRIBUTE(ctr_h5->h5_file_groups[ifile], "simulation_params", "Omega_L", ol);
        READ_CTREES_ATTRIBUTE(ctr_h5->h5_file_groups[ifile], "simulation_params", "hubble", little_h);

        double file_boxsize;
        READ_CTREES_ATTRIBUTE(ctr_h5->h5_file_groups[ifile], "simulation_params", "Boxsize", file_boxsize);

        /* Check that the units specified in the parameter file are very close to these values -> if not, ABORT
        (We could simply call init_sage again here but that will lead to un-necessary intermingling of components that
        should be independent)
        */

        const double maxdiff = 1e-8, maxreldiff = 1e-5; /*numpy.allclose defaults (as of v1.16) */
#define CHECK_AND_ABORT_UNITS_VS_PARAM_FILE( name, variable, param, absdiff, absreldiff) { \
            if(AlmostEqualRelativeAndAbs_double(variable, param, absdiff, absreldiff) != EXIT_SUCCESS) { \
                fprintf(stderr,"Error: Variable %s has value = %g and is different from what is specified in the parameter file = %g\n", \
                        name, variable, param);                             \
                return -1;                                                  \
            }                                                               \
        }

        CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("BoxSize", file_boxsize, run_params->BoxSize, maxdiff, maxreldiff);
        CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Omega_M", om, run_params->Omega, maxdiff, maxreldiff);
        CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Omega_Lambda", ol, run_params->OmegaLambda, maxdiff, maxreldiff);
        CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Little h (hubble parameter)", little_h, run_params->Hubble_h, maxdiff, maxreldiff);

#undef CHECK_AND_ABORT_UNITS_VS_PARAM_FILE
    }
     /* Figure out the appropriate field name for the 'Snapshot number' field -> 'Snap_num' in older CTrees and 'Snap_idx' in newer versions */
    /* MS 29/09/2021: Older versions of CTrees has Snap_num -> which is parsed correctly and  int64_t type
        The Uchuu trees were created for later CTrees version and have the column Snap_idx
        but the uchuutools converter did not correctly interpret the field as an integer and
        the field was written out as a double. However, future versions of the uchuutools converter
        will fix this issue. Therefore, we need to allow for these possibilities:
        # fldname   hdf5_type       c_type
        'Snap_num,  H5_NATIVE_INT64,  int64_t'
        'Snap_idx,  H5_NATIVE_INT64,  int64_t'
        'Snap_idx', H5_NATIVE_DOUBLE, double'
    */

    if(ctr_h5->contig_halo_props[start_filenum]) {
        hid_t h5_forests_group = ctr_h5->h5_forests_group[start_filenum];
        const size_t snap_fieldname_sizeof = sizeof(ctr_h5->snap_field_name);
        char snap_field_name[snap_fieldname_sizeof];
        snprintf(snap_field_name, snap_fieldname_sizeof, "Snap_num");
        if(H5Lexists(h5_forests_group, snap_field_name, H5P_DEFAULT) <= 0) {
            //Snap_num does not exist - lets try the other field name
            snprintf(snap_field_name, snap_fieldname_sizeof, "Snap_idx");
            if(H5Lexists(h5_forests_group, snap_field_name, H5P_DEFAULT) <= 0) {
                fprintf(stderr, "Error: Could not locate the snapshot number field - neither as 'Snap_num' nor as '%s'\n",
                        snap_field_name);
                return -EXIT_FAILURE;
            }
        }
        snprintf(ctr_h5->snap_field_name, snap_fieldname_sizeof, "%s", snap_field_name);
        hid_t snap_dset = H5Dopen2(h5_forests_group, snap_field_name, H5P_DEFAULT);
        XRETURN(snap_dset >= 0, -HDF5_ERROR, "Error encountered when trying to open up snapshot dataset '%s'.\n", snap_field_name);
        const hid_t snap_dtype = H5Dget_type(snap_dset);
        XRETURN(snap_dtype >= 0, -HDF5_ERROR, "Error: Failed to get datatype for snapshot dataset = '%s'.\n", snap_field_name);
        H5T_class_t snap_dtype_class = H5Tget_class(snap_dtype);
        XRETURN(snap_dtype_class >= 0, -HDF5_ERROR,
                "Error: Failed to get the native HDF5 datatype class for snapshot dataset = '%s'.\n", snap_field_name);
        ctr_h5->snap_field_is_double = -1;
        if(snap_dtype_class == H5T_INTEGER) {
            // fprintf(stderr, "Snapshot dataset datatype is 'H5T_INTEGER'.\n");
            ctr_h5->snap_field_is_double = 0;
            /* display size, signed, endianess, etc. */
        } else if(snap_dtype_class == H5T_FLOAT) {
            // fprintf(stderr, "Snapshot dataset datatype is 'H5T_FLOAT'.\n");
            ctr_h5->snap_field_is_double = 1;
        } else {
            fprintf(stderr,"Error: Expected to find that the snapshot field ('%s') to be 'integer' or 'float' "
                    "but that was not the case.\n", snap_field_name);
            return -HDF5_ERROR;
        }
        XRETURN(H5Dclose(snap_dset) >= 0, -HDF5_ERROR, "Error: Could not close snapshot dataset = '%s'.\n", snap_field_name);
        XRETURN(H5Tclose(snap_dtype) >= 0, -HDF5_ERROR, "Error: Failed to close the datatype for the snapshot dataset = '%s'.\n", snap_field_name);
    } else {
        fprintf(stderr, "Error: Halos written as array-of-structs is not supported yet\n");
        return -1;
    }

    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
    forests_info->frac_volume_processed = 0.0;
    for(int32_t filenr = start_filenum; filenr <= end_filenum; filenr++) {
        if(filenr >= totnfiles || filenr < 0) {
            fprintf(stderr,"Error: filenr = %d exceeds totnfiles = %"PRId64"\n", filenr, totnfiles);
            return -1;
        }
        forests_info->frac_volume_processed += (double) num_forests_to_process_per_file[filenr] / (double) totnforests_per_file[filenr];
    }
    forests_info->frac_volume_processed /= (double) run_params->NumSimulationTreeFiles;

    myfree(num_forests_to_process_per_file);
    myfree(start_forestnum_to_process_per_file);
    myfree(totnforests_per_file);

    /* Finally setup the multiplication factors necessary to generate
       unique galaxy indices (across all files, all trees and all tasks) for this run*/
    run_params->FileNr_Mulfac   =  LLONG_MAX/2000;
    run_params->ForestNr_Mulfac =        run_params->FileNr_Mulfac/3000000000LL;

    return EXIT_SUCCESS;
}

#define READ_PARTIAL_FOREST_ARRAY(file_group, field_name, offset, count, buffer, dst_type, error_message) { \
        hid_t h5_dset = H5Dopen2(file_group, field_name, H5P_DEFAULT);  \
        XRETURN(h5_dset >= 0, -HDF5_ERROR,                              \
                "Error encountered when trying to open up dataset %s\n%s\n",\
                field_name, error_message);                             \
        hid_t h5_fspace = H5Dget_space(h5_dset);                        \
        XRETURN(h5_fspace >= 0, -HDF5_ERROR,                            \
                "Error encountered when trying to reserve filespace for dataset %s\n%s\n", \
                field_name, error_message);                             \
        herr_t macro_status = H5Sselect_hyperslab(h5_fspace, H5S_SELECT_SET, offset, NULL, count, NULL); \
        XRETURN(macro_status >= 0, -HDF5_ERROR,                         \
                "Error: Failed to select hyperslab for dataset = %s.\n" \
                "The dimensions of the dataset was %llu offset = %llu.\n%s\n", \
                field_name, (unsigned long long) *count, (unsigned long long) *offset, error_message);            \
        hid_t h5_memspace = H5Screate_simple(1, count, NULL);           \
        XRETURN(h5_memspace >= 0, -HDF5_ERROR,                          \
                "Error: Failed to select hyperslab for dataset = %s.\n" \
                "The dimensions of the dataset was %llu.\n%s\n",        \
                field_name, (unsigned long long) *count, error_message);                     \
        const hid_t h5_dtype = H5Dget_type(h5_dset);                    \
        XRETURN(h5_dtype >= 0, -HDF5_ERROR,                             \
                "Error: Failed to get datatype for dataset = %s.\n"     \
                "The dimensions of the dataset was %llu\n.",            \
                field_name, (unsigned long long) *count);                                    \
        XRETURN(sizeof(dst_type) == H5Tget_size(h5_dtype), -HDF5_ERROR, \
                "Error while reading dataset '%s'\n"                    \
                "The HDF5 attribute has size %zu bytes but the "        \
                "destination has size = %zu bytes.\n%s\n",              \
                field_name, H5Tget_size(h5_dtype),                      \
                sizeof(dst_type), error_message);                       \
        macro_status = H5Dread(h5_dset, h5_dtype, h5_memspace, h5_fspace, H5P_DEFAULT, buffer); \
        XRETURN(macro_status >= 0, FILE_READ_ERROR,                     \
                "Error: Failed to read array for dataset = %s.\n"       \
                "The dimensions of the dataset was %llu, offset = %llu\n%s\n", \
                field_name, (unsigned long long) *count, (unsigned long long) *offset, error_message);            \
        XRETURN(H5Dclose(h5_dset) >= 0, -HDF5_ERROR,                    \
                "Error: Could not close dataset = '%s'.\n"              \
                "The dimensions of the dataset was %llu\n.",            \
                field_name, (unsigned long long) *count);                                    \
        XRETURN(H5Tclose(h5_dtype) >= 0, -HDF5_ERROR,                   \
                "Error: Failed to close the datatype for = %s.\n"       \
                "The dimensions of the dataset was %llu\n.",            \
                field_name, (unsigned long long) *count);                                    \
        XRETURN(H5Sclose(h5_fspace) >= 0, -HDF5_ERROR,                  \
                "Error: Failed to close the filespace for = %s.\n"      \
                "The dimensions of the dataset was %llu\n.",            \
                field_name, (unsigned long long) *count);                                    \
        XRETURN(H5Sclose(h5_memspace) >= 0, -HDF5_ERROR,                \
                "Error: Failed to close the dataspace for = %s.\n"      \
                "The dimensions of the dataset was %llu\n.",            \
                field_name, (unsigned long long) *count);                                    \
    }



int64_t load_forest_ctrees_hdf5(int64_t forestnr, struct halo_data **halos,
                                struct forest_info *forests_info, struct params *run_params)
{
    struct ctrees_h5_info *ctr_h5 = &(forests_info->ctr_h5);
    hid_t meta_fd = ctr_h5->meta_fd;

    /* CHECK: HDF5 file pointer is valid */
    if(meta_fd <= 0 ) {
        fprintf(stderr,"Error: File pointer is NULL (i.e., you need to open the file before reading).\n"\
                "This error should already have been caught before reaching this line\n");
        return -INVALID_FILE_POINTER;
    }

    if(forestnr < 0 || forestnr > ctr_h5->nforests) {
        fprintf(stderr,"Error on ThisTask=%d: forestnr = %"PRId64" should be between [0, %"PRId64")\n",
                run_params->ThisTask, forestnr, ctr_h5->nforests);
        return -INVALID_MEMORY_ACCESS_REQUESTED;
    }

    const int32_t filenum_for_tree = forests_info->FileNr[forestnr];
    const int64_t treenum_in_file = forests_info->original_treenr[forestnr];
    if(filenum_for_tree < ctr_h5->start_filenum || filenum_for_tree > ctr_h5->end_filenum) {
        fprintf(stderr,"Error on ThisTask=%d: For forestnr = %"PRId64" (original treenr = %"PRId64"), the file number = %d "\
                "should be within [%d, %d]\n", run_params->ThisTask, forestnr, treenum_in_file, filenum_for_tree,
                ctr_h5->start_filenum, ctr_h5->end_filenum);
        return -INVALID_MEMORY_ACCESS_REQUESTED;
    }

    char file_group_name[MAX_STRING_LEN];
    snprintf(file_group_name, MAX_STRING_LEN-1, "File%d", filenum_for_tree);
    hid_t h5_file_group = ctr_h5->h5_file_groups[filenum_for_tree];
    if(h5_file_group <= 0 ) {
        fprintf(stderr,"Error: File pointer is NULL (i.e., you need to open the file group '%s' before reading).\n"\
                "This error should already have been caught before reaching this line\n", file_group_name);
        return -INVALID_FILE_POINTER;
    }

    struct ctrees_forestinfo ctrees_finfo;
    const char field_name[] = "ForestInfo";
    const hsize_t count = 1, treenr = treenum_in_file; /* to satisfy the hsize_t vs int64_t compiler warnings + the pointer indirection necessary. MS 14/09/2021 */
    char error_msg[MAX_STRING_LEN];
    snprintf(error_msg, MAX_STRING_LEN, "ERROR MESSAGE: ThisTask = %d filenum_for_tree = %d treenum_in_file = %"PRId64"\n",
             run_params->ThisTask, filenum_for_tree, treenum_in_file);
    READ_PARTIAL_FOREST_ARRAY(h5_file_group, field_name, &treenr, &count, &ctrees_finfo, struct ctrees_forestinfo, error_msg);

    const int64_t halosoffset = ctrees_finfo.foresthalosoffset;
    const int64_t nhalos = ctrees_finfo.forestnhalos;

    /* fprintf(stderr,"forestnr = %"PRId64" meta_fd = %lu treenum = %"PRId64" halosoffset = %"PRId64" nhalos = %"PRId64 " filenr = %d\n", forestnr, meta_fd, treenum_in_file, halosoffset, nhalos, filenum_for_tree); */

    /* okay now we have the offset and the total number of halos in this forest
        Now allocate the memory for the forest_halos and offload to a dedicated (private) loading
        function. This function also needs to account for "SOA" vs "AOS" types
        Note: Uchuu is SOA type
    */
    *halos = mymalloc(sizeof(struct halo_data) * nhalos);//the malloc failure check is done within mymalloc
    if(ctr_h5->contig_halo_props[filenum_for_tree]) {
        hid_t h5_forests_group = ctr_h5->h5_forests_group[filenum_for_tree];
        int status = read_contiguous_forest_ctrees_h5(h5_forests_group,
                                                      nhalos, halosoffset,
                                                      ctr_h5->snap_field_name, ctr_h5->snap_field_is_double,
                                                      *halos);
        if(status < 0) {
            fprintf(stderr,"Error: Could not correctly read the forest data [forestid='%"PRId64"', (file-local) forestnr = %"PRId64", global forestnr = %"PRId64", nhalos = %"PRId64" offset = %"PRId64"] from the file = '%s'. Possible data format issue?\n",
            ctrees_finfo.forestid, treenum_in_file, forestnr, nhalos, halosoffset, file_group_name);
            return status;
        }
    } else {
        fprintf(stderr,"Error: Consistent-trees hdf5 format in AOS format is not supported yet\n");
        return -1;
    }
    /* Done with all the reading for this snapshot */

    const int32_t snap_offset = 0;/* need to figure out how to set this correctly (do not think there is an automatic way to do so): MS 03/08/2018 */
    convert_ctrees_conventions_to_lht(*halos, nhalos, snap_offset, run_params->PartMass);

    return nhalos;
}


#define ASSIGN_TREE_PROPERTY_SINGLEDIM(buffer, buffer_dtype, dest, sage_name) {    \
        buffer_dtype *macro_x = (buffer_dtype *) buffer;                     \
        for (hsize_t i=0; i<nhalos; i++) {                                   \
            dest[i].sage_name = *macro_x;                            \
            macro_x++;                                                       \
        }                                                                    \
    }

#define ASSIGN_TREE_PROPERTY_MULTIDIM(buffer, buffer_dtype, dest, sage_name, dim) { \
        buffer_dtype *macro_x = (buffer_dtype *) buffer;                 \
        for (hsize_t i=0; i<nhalos; i++) {                               \
            dest[i].sage_name[dim] = *macro_x;                   \
            macro_x++;                                                   \
        }                                                                \
    }

#define READ_ASSIGN_TREE_PROP_SINGLE(file_group, field_name, offset, count, buffer, buffer_dtype, dest, sage_name) {  \
        READ_PARTIAL_FOREST_ARRAY(file_group, field_name, offset, count, buffer, buffer_dtype, "NULL"); \
        ASSIGN_TREE_PROPERTY_SINGLEDIM(buffer, buffer_dtype, dest, sage_name);                                        \
}

#define READ_ASSIGN_TREE_PROP_MULTI(file_group, field_name, offset, count, buffer, buffer_dtype, dest, sage_name, dim) {  \
        READ_PARTIAL_FOREST_ARRAY(file_group, field_name, offset, count, buffer, buffer_dtype, "NULL"); \
        ASSIGN_TREE_PROPERTY_MULTIDIM(buffer, buffer_dtype, dest, sage_name, dim);                                               \
}

int read_contiguous_forest_ctrees_h5(hid_t h5_forests_group, const hsize_t nhalos, const hsize_t halosoffset,
                                     const char *snap_field_name, const int8_t snap_field_is_double,
                                     struct halo_data *halos)
{
    void *buffer = malloc(nhalos * sizeof(double)); // The largest data-type will be double.
    XRETURN(buffer != NULL, -MALLOC_FAILURE,
            "Error: Could not allocate memory for %llu halos in the HDF5 buffer. Size requested = %llu bytes\n",
            (unsigned long long) nhalos, (unsigned long long) nhalos * sizeof(double));


    // fprintf(stderr,"IN %s> h5_forests_group = %lu halosoffset = %llu nhalos = %llu\n", __FUNCTION__,  h5_forests_group, halosoffset, nhalos);

    // We now need to read in all the halo fields for this forest.
    // To do so, we read the field into a buffer and then properly slot the field into the Halo struct.

    /* Merger Tree Pointers */
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "Descendant", &halosoffset, &nhalos, buffer, int64_t, halos, Descendant);
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "FirstProgenitor", &halosoffset, &nhalos, buffer, int64_t, halos, FirstProgenitor);
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "NextProgenitor", &halosoffset, &nhalos, buffer, int64_t, halos, NextProgenitor);
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "FirstHaloInFOFgroup", &halosoffset, &nhalos, buffer, int64_t, halos, FirstHaloInFOFgroup);
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group,  "NextHaloInFOFgroup", &halosoffset, &nhalos, buffer, int64_t, halos, NextHaloInFOFgroup);

    /* Halo Properties */
    //READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "SubhaloLen", &halosoffset, &nhalos, buffer, int64_t, halos, Len);
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "M200b", &halosoffset, &nhalos, buffer, double, halos, M_Mean200);
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "Mvir", &halosoffset, &nhalos, buffer, double, halos, Mvir);
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "M200c", &halosoffset, &nhalos, buffer, double, halos, M_TopHat);

    /* Now read the multi-dimensional properties - position, velocity and spin */
    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "x", &halosoffset, &nhalos, buffer, double, halos, Pos, 0);//needs to be converted from kpc/h -> Mpc/h
    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "y", &halosoffset, &nhalos, buffer, double, halos, Pos, 1);//needs to be converted from kpc/h -> Mpc/h
    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "z", &halosoffset, &nhalos, buffer, double, halos, Pos, 2);//needs to be converted from kpc/h -> Mpc/h

    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "vrms", &halosoffset, &nhalos, buffer, double, halos, VelDisp);//km/s
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "vmax", &halosoffset, &nhalos, buffer, double, halos, Vmax);//km/s
    READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, "id", &halosoffset, &nhalos, buffer, int64_t, halos, MostBoundID);//The Ctrees generated haloid is carried through

    if(snap_field_is_double) {
        READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, snap_field_name, &halosoffset, &nhalos, buffer, double, halos, SnapNum);
    } else {
        READ_ASSIGN_TREE_PROP_SINGLE(h5_forests_group, snap_field_name, &halosoffset, &nhalos, buffer, int64_t, halos, SnapNum);
    }

    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "vx", &halosoffset, &nhalos, buffer, double, halos, Vel, 0);//km/s
    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "vy", &halosoffset, &nhalos, buffer, double, halos, Vel, 1);//km/s
    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "vz", &halosoffset, &nhalos, buffer, double, halos, Vel, 2);//km/s

    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "Jx", &halosoffset, &nhalos, buffer, double, halos, Spin, 0);//(kpc/h)(km/s) -> convert to (Mpc)*(km/s). Does it need sqrt(3)?
    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "Jy", &halosoffset, &nhalos, buffer, double, halos, Spin, 1);//(kpc/h)(km/s) -> convert to (Mpc)*(km/s). Does it need sqrt(3)?
    READ_ASSIGN_TREE_PROP_MULTI(h5_forests_group, "Jz", &halosoffset, &nhalos, buffer, double, halos, Spin, 2);//(kpc/h)(km/s) -> convert to (Mpc)*(km/s). Does it need sqrt(3)?

    free(buffer);

    return EXIT_SUCCESS;
}


void convert_ctrees_conventions_to_lht(struct halo_data *halos, const int64_t nhalos,
                                       const int32_t snap_offset, const double part_mass)
{
    const double inv_part_mass = 1.0/part_mass;
    for(int64_t i=0;i<nhalos;i++) {
        const double inv_halo_mass = 1.0/halos->Mvir;
        for(int k=0;k<3;k++) {
            halos->Spin[k] *= inv_halo_mass;
        }
        /* Convert masses to 10^10 Msun/h */
        halos->Mvir  *= 1e-10;
        halos->M_Mean200 *= 1e-10;
        halos->M_TopHat *= 1e-10;

        /* Calculate the (approx.) number of particles in this halo */
        halos->Len   = (int) roundf(halos->Mvir * inv_part_mass);

        /* Initialize other fields to indicate they are not populated */
        halos->FileNr = -1;
        halos->SubhaloIndex = -1;
        halos->SubHalfMass  = -1.0f;

        /* Convert the snapshot index output by Consistent Trees
           into the snapshot number as reported by the simulation */
        halos->SnapNum += snap_offset;
        halos++;
    }
}

void cleanup_forests_io_ctrees_hdf5(struct forest_info *forests_info)
{
    struct ctrees_h5_info *ctr_h5 = &(forests_info->ctr_h5);
    const int32_t firstfile = forests_info->firstfile;
    const int32_t lastfile = forests_info->lastfile;
    for(int32_t ifile=firstfile;ifile<=lastfile;ifile++) {
        H5Gclose(ctr_h5->h5_file_groups[ifile]);
        H5Gclose(ctr_h5->h5_forests_group[ifile]);
    }
    free(ctr_h5->h5_file_groups);
    free(ctr_h5->h5_forests_group);
    free(ctr_h5->contig_halo_props);
    H5Fclose(ctr_h5->meta_fd);
}
