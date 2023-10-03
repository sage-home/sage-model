#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#include "read_tree_lhalo_hdf5.h"
#include "hdf5_read_utils.h"
#include "../core_mymalloc.h"
#include "forest_utils.h"

/* Local Proto-Types */
static int convert_units_for_forest(struct halo_data *halos, const int64_t nhalos);
static void get_forests_filename_lht_hdf5(char *filename, const size_t len, const int filenr, const struct params *run_params);


void get_forests_filename_lht_hdf5(char *filename, const size_t len, const int filenr,  const struct params *run_params)
{
    snprintf(filename, len - 1, "%s/%s.%d%s", run_params->SimulationDir, run_params->TreeName, filenr, run_params->TreeExtension);
}


int setup_forests_io_lht_hdf5(struct forest_info *forests_info,
                             const int ThisTask, const int NTasks, struct params *run_params)
{
    const int firstfile = run_params->FirstFile;
    const int lastfile = run_params->LastFile;
    const int numfiles = lastfile - firstfile + 1;
    if(numfiles <= 0) {
        return -1;
    }

    /* wasteful to allocate for lastfile + 1 indices, rather than numfiles; but makes indexing easier */
    int64_t *totnforests_per_file = calloc(lastfile + 1, sizeof(*totnforests_per_file));
    if(totnforests_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests in each file\n");
        perror(NULL);
        return MALLOC_FAILURE;
    }
    struct HDF5_METADATA_NAMES metadata_names;
    int64_t totnforests = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_lht_hdf5(filename, 4*MAX_STRING_LEN, filenr, run_params);
        hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        XRETURN (fd > 0, FILE_NOT_FOUND,
                 "Error: can't open file `%s'\n", filename);

        int status = fill_hdf5_metadata_names(&metadata_names, run_params->TreeType);
        if (status != EXIT_SUCCESS) {
            return -1;
        }

#define READ_LHALO_ATTRIBUTE(hid, groupname, attrname, dst) {           \
            herr_t h5_status = read_attribute(hid, groupname, attrname, (void *) &dst, sizeof(dst)); \
            if(h5_status < 0) {                                         \
                return (int) h5_status;                                 \
            }                                                           \
        }

        //Read in partmass
        if(filenr == firstfile) {
            double partmass;
            READ_LHALO_ATTRIBUTE(fd, "/Header", metadata_names.name_ParticleMass, partmass);

            const double max_diff = 1e-5;
            if(fabs(run_params->PartMass - partmass) >= max_diff) {
                fprintf(stderr,"Error: Parameter file mentions particle mass = %g but the hdf5 file shows particle mass = %g\n",
                        run_params->PartMass, partmass);
                fprintf(stderr,"Diff = %g max. tolerated diff = %g\n", fabs(run_params->PartMass - partmass), max_diff);
                fprintf(stderr,"May be the value in the parameter file needs to be updated?\n");
                return -1;
            }

            //Check if the number of simulation output files have been set correctly
            int32_t numsimulationfiles;
            READ_LHALO_ATTRIBUTE(fd, "/Header", metadata_names.name_NumSimulationTreeFiles, numsimulationfiles);

            if(numsimulationfiles != run_params->NumSimulationTreeFiles) {
                fprintf(stderr,"Error: Parameter file mentions total number of simulation output files = %d but the "
                        "hdf5 field `%s' says %d tree files\n",
                        run_params->NumSimulationTreeFiles, metadata_names.name_NumSimulationTreeFiles, numsimulationfiles);
                fprintf(stderr,"May be the value in the parameter file needs to be updated?\n");
                return -1;
            }
        }

        int32_t nforests;
        READ_LHALO_ATTRIBUTE(fd, "/Header", metadata_names.name_NTrees, nforests);

        totnforests_per_file[filenr] = nforests;
        totnforests += nforests;

        status = H5Fclose(fd);
        if(status < 0) {
            fprintf(stderr,"Error: Could not properly close the hdf5 file for filename = '%s'\n", filename);
            H5Eprint(status, stderr);
            return status;
        }
    }
    forests_info->totnforests = totnforests;


    /* Read in nhalos_per_forest for *ALL* files */
    const int need_nhalos_per_forest = run_params->ForestDistributionScheme == uniform_in_forests ? 0:1;
    int64_t *nhalos_per_forest = NULL;
    if(need_nhalos_per_forest) {
        nhalos_per_forest = mycalloc(totnforests, sizeof(*nhalos_per_forest));
        for(int32_t ifile=firstfile;ifile<=lastfile;ifile++) {
            const int64_t nforests_this_file = totnforests_per_file[ifile];
            char filename[4*MAX_STRING_LEN];
            get_forests_filename_lht_hdf5(filename, 4*MAX_STRING_LEN, ifile, run_params);
            hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
            XRETURN (fd > 0, FILE_NOT_FOUND,
                     "Error: can't open file `%s'\n", filename);

            const int check_size = 1;
            int32_t *buffer = malloc(sizeof(*buffer)*nforests_this_file);
            XRETURN(buffer != NULL, MALLOC_FAILURE, "Error: Could not allocate memory for storing nhalos "\
                                                    "per forest (%"PRId64" forests, with each element of size = %zu bytes)\n",
                                                    nforests_this_file, sizeof(*buffer));
            int status = read_dataset(fd, metadata_names.name_TreeNHalos, -1, (void *) buffer, sizeof(*buffer), check_size);
            if(status < 0) {
                return status;
            }
            for(int64_t i=0;i<nforests_this_file;i++) {
                nhalos_per_forest[i] = buffer[i];
            }
            free(buffer);
            XRETURN( H5Fclose(fd) >= 0, -1, "Error: Could not properly close the hdf5 file for filename = '%s'\n", filename);

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


    int64_t *num_forests_to_process_per_file = calloc((lastfile + 1), sizeof(num_forests_to_process_per_file[0]));/* calloc is required */
    int64_t *start_forestnum_to_process_per_file = malloc((lastfile + 1) * sizeof(start_forestnum_to_process_per_file[0]));
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
                                        // NULL,
                                        num_forests_to_process_per_file, start_forestnum_to_process_per_file,
                                        &start_filenum, &end_filenum);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    struct lhalotree_info *lht = &(forests_info->lht);
    forests_info->nforests_this_task = nforests_this_task;
    lht->nforests = nforests_this_task;

    forests_info->FileNr = malloc(nforests_this_task * sizeof(*(forests_info->FileNr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->FileNr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->FileNr", nforests_this_task,
                                     sizeof(*(forests_info->FileNr)));

    forests_info->original_treenr = malloc(nforests_this_task * sizeof(*(forests_info->original_treenr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->original_treenr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->original_treenr", nforests_this_task,
                                     sizeof(*(forests_info->original_treenr)));

    lht->nhalos_per_forest = NULL;
    /* lht->nhalos_per_forest = mymalloc(nforests_this_task * sizeof(lht->nhalos_per_forest[0])); */
    /* lht->bytes_offset_for_forest = mymalloc(nforests_this_task * sizeof(lht->bytes_offset_for_forest[0])); */
    lht->bytes_offset_for_forest = NULL;
    lht->h5_fd = mymalloc(nforests_this_task * sizeof(lht->h5_fd[0]));
    lht->numfiles = end_filenum - start_filenum + 1;
    lht->open_h5_fds = mymalloc(lht->numfiles * sizeof(lht->open_h5_fds[0]));

    int64_t nforests_so_far = 0;
    /* int *forestnhalos = lht->nhalos_per_forest; */
    for(int filenr=start_filenum;filenr<=end_filenum;filenr++) {
        XRETURN(start_forestnum_to_process_per_file[filenr] >= 0 && start_forestnum_to_process_per_file[filenr] < totnforests_per_file[filenr],
                EXIT_FAILURE,
                "Error: Num forests to process = %"PRId64" for filenr = %d should be in range [0, %"PRId64")\n",
                start_forestnum_to_process_per_file[filenr],
                filenr,
                totnforests_per_file[filenr]);

        XRETURN(num_forests_to_process_per_file[filenr] >= 0 && num_forests_to_process_per_file[filenr] <= totnforests_per_file[filenr],
                EXIT_FAILURE,
                "Error: Num forests to process = %"PRId64" for filenr = %d should be in range [0, %"PRId64")\n",
                num_forests_to_process_per_file[filenr],
                filenr,
                totnforests_per_file[filenr]);

        int file_index = filenr - start_filenum;
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_lht_hdf5(filename, 4*MAX_STRING_LEN, filenr, run_params);
        hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        XRETURN( fd > 0, FILE_NOT_FOUND,"Error: can't open file `%s'\n", filename);

        lht->open_h5_fds[file_index] = fd;/* keep the file open, will be closed at the cleanup stage */

        const int64_t nforests = num_forests_to_process_per_file[filenr];
        for(int64_t i=0;i<nforests;i++) {
            lht->h5_fd[i + nforests_so_far] = fd;

            // Can't guarantee that the `FileNr` variable in the tree file is correct.
            // Hence let's track it explicitly here.
            forests_info->FileNr[i + nforests_so_far] = filenr;

            // We want to track the original tree number from these files.  Since we split multiple
            // files across tasks, we can't guarantee that tree N processed by a certain task is
            // actually the Nth tree in any arbitrary file.
            forests_info->original_treenr[i + nforests_so_far] = i + start_forestnum_to_process_per_file[filenr];
            // We add `start_forestnum...`` because we could start reading from the middle of a
            // forest file.  Hence we would want "Forest 0" processed by that task to be
            // appropriately shifted.
        }

        nforests_so_far += nforests;
    }


    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
    forests_info->frac_volume_processed = 0.0;
    for(int32_t filenr = start_filenum; filenr <= end_filenum; filenr++) {
        forests_info->frac_volume_processed += (double) num_forests_to_process_per_file[filenr] / (double) totnforests_per_file[filenr];
    }
    forests_info->frac_volume_processed /= (double) run_params->NumSimulationTreeFiles;

    free(num_forests_to_process_per_file);
    free(start_forestnum_to_process_per_file);
    free(totnforests_per_file);

    /* Finally setup the multiplication factors necessary to generate
       unique galaxy indices (across all files, all trees and all tasks) for this run*/
    run_params->FileNr_Mulfac = 1000000000000000LL;
    run_params->ForestNr_Mulfac = 1000000000LL;

    return EXIT_SUCCESS;
}


/* MS: 17/9/2019 Assumes a properly allocated variable, with size at least 'nhalos*8' called 'buffer'
   Also assumes a properly allocated variable, 'local_halos' of size 'nhalos' */
#define READ_TREE_PROPERTY(fd, treenr, sage_name, hdf5_name, C_dtype) { \
        snprintf(dataset_name, MAX_STRING_LEN - 1, "Tree%"PRId64"/%s", treenr, #hdf5_name); \
        const int check_size = 1;                                       \
        int macro_status = read_dataset(fd, dataset_name, -1, buffer, sizeof(C_dtype), check_size); \
        if (macro_status != EXIT_SUCCESS) {                                   \
            return -1;                                                  \
        }                                                               \
        C_dtype *macro_x = (C_dtype *) buffer;                          \
        for (int halo_idx = 0; halo_idx < nhalos; ++halo_idx) {         \
            local_halos[halo_idx].sage_name = *macro_x;                 \
            macro_x++;                                                  \
        }                                                               \
    }

/* MS: 17/9/2019 Assumes a properly allocated variable, with size at least 'nhalos*NDIM*8' called 'buffer'
   Also assumes a properly allocated variable, 'local_halos' of size 'nhalos' */
#define READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenr, sage_name, hdf5_name, C_dtype) { \
        snprintf(dataset_name, MAX_STRING_LEN - 1, "Tree%"PRId64"/%s", treenr, #hdf5_name); \
        const int check_size = 1;                                       \
        const int macro_status = read_dataset(fd, dataset_name, -1, buffer, sizeof(C_dtype), check_size); \
        if (macro_status != EXIT_SUCCESS) {                             \
            return -1;                                                  \
        }                                                               \
        C_dtype *macro_x = (C_dtype *) buffer;                          \
        for (int halo_idx = 0; halo_idx < nhalos; ++halo_idx) {         \
            for (int dim = 0; dim < NDIM; ++dim) {                      \
                local_halos[halo_idx].sage_name[dim] = *macro_x;        \
                macro_x++;                                              \
            }                                                           \
        }                                                               \
    }


int64_t load_forest_lht_hdf5(const int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info)
{
    char dataset_name[MAX_STRING_LEN];
    void *buffer; // Buffer to hold the read HDF5 data.

    /* const int64_t nhalos = (int64_t) forests_info->lht.nhalos_per_forest[forestnr];/\* the array itself contains int32_t, since the LHT format*\/ */
    hid_t fd = forests_info->lht.h5_fd[forestnr];

    /* CHECK: HDF5 file pointer is valid */
    if(fd <= 0 ) {
        fprintf(stderr,"Error: File pointer is NULL (i.e., you need to open the file before reading).\n"
                "This error should already have been caught before reaching this line\n");
        return -INVALID_FILE_POINTER;
    }

    const int64_t treenum_in_file = forests_info->original_treenr[forestnr];
    /* fprintf(stderr,"forestnr = %d fd = %d treenum = %d\n", forestnr, (int) fd, treenum_in_file); */


    /* Figure out nhalos by checking the size of the 'Descendant' dataset (could be any other valid field) */
    /* https://stackoverflow.com/questions/15786626/get-the-dimensions-of-a-hdf5-dataset */
    const char field_name[] = "Descendant";
    snprintf(dataset_name, MAX_STRING_LEN - 1, "Tree%"PRId64"/%s", treenum_in_file, field_name);

    int ndims;
    hsize_t *dims;
    int status = (int) read_dataset_shape(fd, dataset_name, &ndims, &dims);
    if(status != EXIT_SUCCESS) {
        const int neg_status = status < 0 ? status:-status;
        return neg_status;
    }

    XRETURN(ndims == 1, -1, "Error: For tree-number = %"PRId64", expected field = '%s' to be 1-D array with ndims == 1. Instead found ndims = %d\n", treenum_in_file, field_name, ndims);
    const int64_t nhalos = dims[0];
    free(dims);

    /* allocate the entire memory space required to store the halos*/
    *halos = mymalloc(sizeof(struct halo_data) * nhalos);
    struct halo_data *local_halos = *halos;

    buffer = malloc(nhalos * NDIM * sizeof(double)); // The largest data-type will be double.
    XRETURN(buffer != NULL, -MALLOC_FAILURE,
            "Error: Could not allocate memory for %"PRId64" halos in the HDF5 buffer. Size requested = %"PRIu64" bytes\n",
            nhalos, nhalos * NDIM * sizeof(double));

    // We now need to read in all the halo fields for this forest.
    // To do so, we read the field into a buffer and then properly slot the field into the Halo struct.

    /* Merger Tree Pointers */
    READ_TREE_PROPERTY(fd, treenum_in_file, Descendant, Descendant, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, FirstProgenitor, FirstProgenitor, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, NextProgenitor, NextProgenitor, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, FirstHaloInFOFgroup, FirstHaloInFOFGroup, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, NextHaloInFOFgroup, NextHaloInFOFGroup, int);

    /* Halo Properties */
    READ_TREE_PROPERTY(fd, treenum_in_file, Len, SubhaloLen, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, M_Mean200, Group_M_Mean200, float);//MS: units 10^10 Msun/h for all Illustris mass fields
    READ_TREE_PROPERTY(fd, treenum_in_file, Mvir, Group_M_Crit200, float);//MS: 16/9/2019 sage uses Mvir but assumes that contains M200c
    READ_TREE_PROPERTY(fd, treenum_in_file, M_TopHat, Group_M_TopHat200, float);
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenum_in_file, Pos, SubhaloPos, float);//needs to be converted from kpc/h -> Mpc/h
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenum_in_file, Vel, SubhaloVel, float);//km/s
    READ_TREE_PROPERTY(fd, treenum_in_file, VelDisp, SubhaloVelDisp, float);//km/s
    READ_TREE_PROPERTY(fd, treenum_in_file, Vmax,  SubhaloVMax, float);//km/s
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, treenum_in_file, Spin, SubhaloSpin, float);//(kpc/h)(km/s) -> convert to (Mpc)*(km/s). Does it need sqrt(3)?
    READ_TREE_PROPERTY(fd, treenum_in_file, MostBoundID, SubhaloIDMostBound, unsigned long long);

    /* File Position Info */
    READ_TREE_PROPERTY(fd, treenum_in_file, SnapNum, SnapNum, int);
    READ_TREE_PROPERTY(fd, treenum_in_file, FileNr, FileNr, int);
    //READ_TREE_PROPERTY(fd, treenum_in_file, SubhaloIndex, SubhaloGrNr, int);//MS: Unsure if this is the right field mapping (another option is SubhaloNumber)
    //READ_TREE_PROPERTY(fd, treenum_in_file, SubHalfMass, SubHalfMass, float);//MS: Unsure what this field captures -> thankfully unused within sage

    free(buffer);

    //MS: 16/9/2019 -- these are the fields present in the Illustris-lhalo-hdf5 file for TNG100-3-Dark
    /* 'Descendant', 'FileNr', 'FirstHaloInFOFGroup', 'FirstProgenitor', 'Group_M_Crit200', 'Group_M_Mean200', 'Group_M_TopHat200', 'NextHaloInFOFGroup', 'NextProgenitor', 'SnapNum', 'SubhaloGrNr', 'SubhaloHalfmassRad', 'SubhaloHalfmassRadType', 'SubhaloIDMostBound', 'SubhaloLen', 'SubhaloLenType', 'SubhaloMassInRadType', 'SubhaloMassType', 'SubhaloNumber', 'SubhaloOffsetType', 'SubhaloPos', 'SubhaloSpin', 'SubhaloVMax', 'SubhaloVel', 'SubhaloVelDisp' */

#ifdef DEBUG_HDF5_READER
    for (int32_t i = 0; i < 20; ++i) {
        printf("halo %d: Descendant %d FirstProg %d x %.4f y %.4f z %.4f\n", i, local_halos[i].Descendant, local_halos[i].FirstProgenitor, local_halos[i].Pos[0], local_halos[i].Pos[1], local_halos[i].Pos[2]);
    }
    ABORT(0);
#endif

    status = convert_units_for_forest(*halos, nhalos);
    if(status != EXIT_SUCCESS) {
        return -1;
    }

    return nhalos;
}

#undef READ_TREE_PROPERTY
#undef READ_TREE_PROPERTY_MULTIPLEDIM

int convert_units_for_forest(struct halo_data *halos, const int64_t nhalos)
{
    if (nhalos <= 0) {
        fprintf(stderr,"Strange!: In function %s> Got nhalos = %"PRId64". Expected to get nhalos > 0\n", __FUNCTION__, nhalos);
        return -1;
    }

    // const float spin_conv_fac = (float) (0.001/hubble);
    const float spin_conv_fac = 0.001f; //As pointed out in https://github.com/sage-home/sage-model/issues/46, scaling with h is not required
    /* Any unit conversions or resetting thing that need to be done */
    for(int64_t i=0;i<nhalos;i++) {
        for(int j=0;j<3;j++) {
            halos[i].Pos[j] *= 0.001f;//convert from kpc/h -> Mpc/h
            halos[i].Spin[j] *= spin_conv_fac;//convert from (kpc/h)*(km/s) -> (Mpc/h)*(km/s)
        }
        halos[i].SubhaloIndex = -1;
        halos[i].SubHalfMass = -1.0f;
    }

    return EXIT_SUCCESS;
}

void cleanup_forests_io_lht_hdf5(struct forest_info *forests_info)
{
    struct lhalotree_info *lht = &(forests_info->lht);
    myfree(lht->h5_fd);
    for(int32_t i=0;i<lht->numfiles;i++) {
        /* could use 'H5close' instead to make sure any open datasets are also
           closed; but that would hide potential bugs in code.
           valgrind should pick those cases up  */
        H5Fclose(lht->open_h5_fds[i]);
    }
    myfree(lht->open_h5_fds);
}
