#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "read_tree_genesis_standard_hdf5.h"
#include "hdf5_read_utils.h"
#include "forest_utils.h"

#include "../core_mymalloc.h"
#include "../core_utils.h"


#include <hdf5.h>

/* Local enum for individual properties that are read in
   from the Genesis hdf5 file*/
enum GalaxyProperty
{
 head_enum = 0,
 tail_enum = 1,
 hosthaloid_enum,
 m200c_enum,
 /* m200b_enum, */
 vmax_enum,
 xc_enum,
 yc_enum,
 zc_enum,
 vxc_enum,
 vyc_enum,
 vzc_enum,
 len_enum,
 mostboundid_enum,
 lx_enum,
 ly_enum,
 lz_enum,
 veldisp_enum,
 num_galaxy_props /* should be the last one */
};

static char galaxy_property_names[num_galaxy_props][MAX_STRING_LEN];
static int fix_flybys_genesis(struct halo_data *halos, const int64_t nhalos_last_snap);
static void get_forest_metadata_filename(const char *forestfilename, const size_t stringlen, char *metadata_filename);


void get_forests_filename_genesis_hdf5(char *filename, const size_t len, const struct params *run_params)
{
    snprintf(filename, len - 1, "%s/%s.%s", run_params->SimulationDir, run_params->TreeName, run_params->TreeExtension);
}


void get_forest_metadata_filename(const char *forestfilename, const size_t stringlen, char *metadata_filename)
{
    memmove(metadata_filename, forestfilename, stringlen);
    metadata_filename[stringlen - 1] = '\0';

    const char searchstring[] = {".hdf5"};
    const size_t searchlen = strlen(searchstring);

    const char replacestring[] = {".foreststats.hdf5"};
    const size_t replacelen = strlen(replacestring);

    const size_t currlen = strnlen(metadata_filename, stringlen);
    const size_t required_len = currlen - searchlen + replacelen;
    if(required_len >= stringlen) {
        fprintf(stderr,"Error: Not enough memory reserved for 'metadata_filename' to correctly hold the file after "
                                                "replacing string -- '%s' with (larger) string '%s'.\n"
                "Please increase the size of 'metadata_filename' to be at least %zu\n",
                searchstring, replacestring, required_len);
        ABORT(INVALID_MEMORY_ACCESS_REQUESTED);
    }

    char *start = strstr(metadata_filename, searchstring);
    strncpy(start, replacestring, replacelen);
    start[replacelen] = '\0';

    fprintf(stderr,"forest_filename   = '%s'\nmetadata_filename = '%s'\n",
            forestfilename, metadata_filename);
    return;
}



/* Externally visible Functions */
int setup_forests_io_genesis_hdf5(struct forest_info *forests_info, const int ThisTask, const int NTasks, struct params *run_params)
{
    if(run_params->FirstFile < 0 || run_params->LastFile < 0 || run_params->LastFile < run_params->FirstFile) {
        fprintf(stderr,"Error: FirstFile = %d and LastFile = %d must both be >=0 *AND* LastFile should be larger than FirstFile.\n"
                "Probably a typo in the parameter-file. Please change to appropriate values...exiting\n",
                run_params->FirstFile, run_params->LastFile);
        return INVALID_OPTION_IN_PARAMS;
    }

    const int firstfile = run_params->FirstFile;
    const int lastfile = run_params->LastFile;
    const int numfiles = lastfile - firstfile + 1;/* This is total number of files to process across all tasks */
    if(numfiles <= 0) {
        fprintf(stderr,"Error: Need at least one file to process. Calculated numfiles = %d (firstfile = %d, lastfile = %d)\n",
                numfiles, run_params->FirstFile, run_params->LastFile);
        return INVALID_OPTION_IN_PARAMS;
    }
    struct genesis_info *gen = &(forests_info->gen);

    char filename[4*MAX_STRING_LEN], metadata_fname[4*MAX_STRING_LEN];

    get_forests_filename_genesis_hdf5(filename, 4*MAX_STRING_LEN, run_params);

    /* Now read in the meta-data info about the forests */
    get_forest_metadata_filename(filename, sizeof(filename), metadata_fname);

    gen->meta_fd = H5Fopen(metadata_fname, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (gen->meta_fd < 0) {
        fprintf(stderr,"Error: On ThisTask = %d can't open file metadata file '%s'\n", ThisTask, metadata_fname);
        return FILE_NOT_FOUND;
    }

#define READ_GENESIS_ATTRIBUTE(hid, dspace, attrname, dst) {            \
        herr_t h5_status = read_attribute (hid, #dspace, #attrname, (void *) &dst, sizeof(dst)); \
        if(h5_status < 0) {                                             \
            return (int) h5_status;                                     \
        }                                                               \
    }

    int64_t check_totnfiles;
    READ_GENESIS_ATTRIBUTE(gen->meta_fd, "Header", "NFiles", check_totnfiles);
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
    if(numfiles < check_totnfiles && ThisTask == 0) {
        fprintf(stdout, "Info: Processing %d files out of a total of %"PRId64" files written out\n",
                numfiles, check_totnfiles);
        fflush(stdout);
    }
    int64_t totnfiles = lastfile + 1;/* Wastes space but makes for easier indexing */

    uint32_t nsnaps;
    READ_GENESIS_ATTRIBUTE(gen->meta_fd, "Header", "NSnaps", nsnaps);
    XRETURN(nsnaps >= 1, INVALID_VALUE_READ_FROM_FILE,
            "Error: Expected total number of snapshots to be at least 1. However, reading in from "
            "metadata file ('%s') shows nsnapshots = %"PRIu32"\n. Exiting...\n",
            metadata_fname, nsnaps);
    gen->maxsnaps = nsnaps;


    int64_t totnforests = 0;
    READ_GENESIS_ATTRIBUTE( gen->meta_fd, "ForestInfo", "NForests", totnforests);
    XRETURN(totnforests >= 1, INVALID_VALUE_READ_FROM_FILE,
            "Error: Expected total number of forests to be at least 1. However, reading in from "
            "metadata file ('%s') shows totnforests = %"PRId64"\n. Exiting...\n",
            metadata_fname, totnforests);
    forests_info->totnforests = totnforests;/*Note: 'totnforests' is assigned into the main structure (forests_info) and
                                             not the genesis structure (named 'gen' here) -- MS 22/11/2019 */

    int64_t maxforestsize;
    READ_GENESIS_ATTRIBUTE(gen->meta_fd, "ForestInfo", "MaxForestSize", maxforestsize);
    XRETURN(maxforestsize >= 1, INVALID_VALUE_READ_FROM_FILE,
            "Error: Expected max. number of halos in any forest to be at least 1. However, reading in from "
            "metadata file ('%s') shows MaxForestSize = %"PRId64"\n. Exiting...\n",
            metadata_fname, maxforestsize);
    gen->maxforestsize = maxforestsize;

    gen->halo_offset_per_snap = calloc(sizeof(gen->halo_offset_per_snap[0]), gen->maxsnaps);/* Stores the halo index offset (i.e., marks the end of
                                                                                               the halos from the previous forest)
                                                                                               to read from at every snapshot */
    XRETURN(gen->halo_offset_per_snap != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold halo offsets at every snapshot (%d items of size %zu bytes)\n",
            gen->maxsnaps, sizeof(gen->halo_offset_per_snap[0]));

    int64_t *totnforests_per_file = calloc(sizeof(*totnforests_per_file), totnfiles);
    XRETURN(totnforests_per_file != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold number of forests per file (%"PRId64" items of size %zu bytes)\n",
            totnfiles, sizeof(*totnforests_per_file));

    /* Now figure out the number of forests per requested file (there might be more
       forest files but we will ignore forests in those files for this particular run)  */
    for(int64_t ifile=firstfile;ifile<totnfiles;ifile++) {
        char fname[5*MAX_STRING_LEN];
        snprintf(fname, sizeof(fname), "%s.%"PRId64, filename, ifile);
        hid_t h5_fd = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
        if(h5_fd < 0) {
            fprintf(stderr,"Error: On ThisTask = %d can't open file forest file '%s'\n", ThisTask, fname);
            return FILE_NOT_FOUND;
        }

        int64_t nforests_this_file;
        READ_GENESIS_ATTRIBUTE(h5_fd, "ForestInfo", "NForests", nforests_this_file);
        XRETURN(nforests_this_file >= 1, INVALID_VALUE_READ_FROM_FILE,
                "Error: Expected the number of forests in this file to be at least 1. However, reading in from "
                "forest file ('%s') shows nforests = %"PRId64"\n. Exiting...\n",
                fname, nforests_this_file);
        totnforests_per_file[ifile] = nforests_this_file;

        XRETURN(H5Fclose(h5_fd) >= 0, HDF5_ERROR,
                "Error: On ThisTask = %d could not close file descriptor for filename = '%s'\n", ThisTask, fname);
    }


    int64_t nforests_this_task, start_forestnum;
    int status = distribute_forests_over_ntasks(totnforests, NTasks, ThisTask, &nforests_this_task, &start_forestnum);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    const int64_t end_forestnum = start_forestnum + nforests_this_task; /* not inclusive, i.e., do not process forestnr == end_forestnum */
    gen->nforests = nforests_this_task;
    gen->start_forestnum = start_forestnum;

    gen->num_forests_to_process_per_file = calloc(totnfiles, sizeof(gen->num_forests_to_process_per_file[0]));/* calloc is required */
    gen->start_forestnum_per_file = calloc(totnfiles, sizeof(gen->start_forestnum_per_file[0]));
    gen->offset_for_global_forestnum = calloc(totnfiles, sizeof(gen->offset_for_global_forestnum[0]));
    gen->halo_offset_per_snap = calloc(gen->maxsnaps, sizeof(gen->halo_offset_per_snap[0]));

    int64_t *num_forests_to_process_per_file = gen->num_forests_to_process_per_file;
    int64_t *start_forestnum_per_file = gen->start_forestnum_per_file;
    int64_t *offset_for_global_forestnum = gen->offset_for_global_forestnum;
    int64_t *halo_offset_per_snap = gen->halo_offset_per_snap;
    if(num_forests_to_process_per_file == NULL || start_forestnum_per_file == NULL ||
       halo_offset_per_snap == NULL || offset_for_global_forestnum == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests that need to be processed per file (on thistask=%d)\n", ThisTask);
        perror(NULL);
        return MALLOC_FAILURE;
    }

    /* show no forests need to be processed by default. THe other variables are initialised to '0' */
    for(int i=0;i<totnfiles;i++) {
        start_forestnum_per_file[i] = -1;
    }

    // Now for each task, we know the starting forest number it needs to start reading from.
    // So let's determine what file and forest number within the file each task needs to start/end reading from.
    int start_filenum = -1, end_filenum = -1;
    int64_t nforests_so_far = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        const int64_t nforests_this_file = totnforests_per_file[filenr];
        const int64_t end_forestnum_this_file = nforests_so_far + nforests_this_file;
        start_forestnum_per_file[filenr] = 0;
        num_forests_to_process_per_file[filenr] = nforests_this_file;
        offset_for_global_forestnum[filenr] = nforests_so_far;

        /* Check if this task should be reading from this file (referred by filenr)
           If the starting forest number (start_forestnum, which is cumulative across all files)
           is located within this file, then the task will need to read from this file.
         */
        if(start_forestnum >= nforests_so_far && start_forestnum < end_forestnum_this_file) {
            start_filenum = filenr;
            start_forestnum_per_file[filenr] = start_forestnum - nforests_so_far;
            num_forests_to_process_per_file[filenr] = nforests_this_file - (start_forestnum - nforests_so_far);
        }

        /* Similar to above, if the end forst number (end_forestnum, again cumulative across all files)
           is located with this file, then the task will need to read from this file.
        */

        if(end_forestnum >= nforests_so_far && end_forestnum <= end_forestnum_this_file) {
            end_filenum = filenr;

            // In the scenario where this task reads ALL forests from a single file, then the number
            // of forests read from this file will be the number of forests assigned to it.
            if(end_filenum == start_filenum) {
                num_forests_to_process_per_file[filenr] = nforests_this_task;
            } else {
                num_forests_to_process_per_file[filenr] = end_forestnum - nforests_so_far;
            }
            /* MS & JS: 07/03/2019 -- Probably okay to break here but might need to complete loop for validation */
        }
        nforests_so_far += nforests_this_file;
    }

    // Make sure we found a file to start/end reading for this task.
    if(start_filenum == -1 || end_filenum == -1 ) {
        fprintf(stderr,"Error: Could not locate start or end file number for the lhalotree binary files\n");
        fprintf(stderr,"Printing debug info\n");
        fprintf(stderr,"ThisTask = %d NTasks = %d totnforests = %"PRId64" start_forestnum = %"PRId64" nforests_this_task = %"PRId64"\n",
                ThisTask, NTasks, totnforests, start_forestnum, nforests_this_task);
        for(int filenr=firstfile;filenr<=lastfile;filenr++) {
            fprintf(stderr,"filenr := %d contains %"PRId64" forests\n",filenr, totnforests_per_file[filenr]);
        }

        return -1;
    }
    free(totnforests_per_file);

    /* So we have the correct files */
    gen->totnfiles = totnfiles;/* the number of files to be processed across all tasks */
    gen->numfiles = end_filenum - start_filenum + 1;/* Number of files to process on this task */
    gen->start_filenum = start_filenum;
    gen->curr_filenum = -1;/* Curr_file_num has to be set to some negative value so that the
                            'gen->halo_offset_per_snap' values are not reset for the first forest. */

    /* We need to track which file each forest is in for two reasons -- i) to actually read from the correct file and ii) to create unique IDs */
    gen->FileNr = calloc(nforests_this_task, sizeof(gen->FileNr[0]));
    XRETURN(gen->FileNr != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold the file numbers for each forest (%"PRId64" items each of size %zu bytes)\n",
            nforests_this_task, sizeof(gen->FileNr[0]));

    /* Really only required for the first file -- since we will likely process from an arbitrary forest number
       We do need these 'file-local' forestnumbers to create the unique IDs */
    gen->forestnum_in_file = calloc(nforests_this_task, sizeof(gen->forestnum_in_file[0]));
    XRETURN(gen->forestnum_in_file != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to hold the true forest number (file-local) within each file (%"PRId64" items each of size %zu bytes)\n",
            nforests_this_task, sizeof(gen->forestnum_in_file[0]));

    for(int64_t iforest=0;iforest<nforests_this_task;iforest++) {
        gen->FileNr[iforest] = -1;
        gen->forestnum_in_file[iforest] = -1;
    }

    /* Now fill up the arrays that are of shape (nforests, ) -- FileNr, forestnum_in_file */
    int32_t curr_filenum = start_filenum;
    int64_t endforestnum_in_currfile = totnforests_per_file[curr_filenum];
    int64_t offset = 0;
    for(int64_t iforest=0;iforest<nforests_this_task;iforest++) {
        if(iforest >= endforestnum_in_currfile) {
            offset = endforestnum_in_currfile;
            curr_filenum++;
            endforestnum_in_currfile += totnforests_per_file[curr_filenum];
        }
        gen->FileNr[iforest] = curr_filenum;
        gen->forestnum_in_file[iforest] = iforest - offset;
    }

    /* Now fill out the halo offsets per snapshot for the first forest */
#define READ_GENESIS_PARTIAL_FORESTINFO(fd, group_name, dataset_name, offset, count, buffer) { \
        hid_t h5_grp = H5Gopen(fd, group_name, H5P_DEFAULT);            \
        XRETURN(h5_grp >= 0, HDF5_ERROR, "Error: Could not open group = '%s'\n", group_name); \
        hid_t h5_dset = H5Dopen2(h5_grp, dataset_name, H5P_DEFAULT);    \
        XRETURN(h5_dset >= 0, HDF5_ERROR, "Error: Could not open dataset = '%s' (within group = '%s')\n", dataset_name, group_name); \
        hid_t h5_space = H5Dget_space(h5_dset);                         \
        XRETURN(h5_space >= 0, HDF5_ERROR, "Error: Could not reserve space for open dataset = '%s' (within group = '%s')\n", dataset_name, group_name); \
        herr_t macro_status = H5Sselect_hyperslab(h5_space, H5S_SELECT_SET, offset, NULL, count, NULL); \
        if(macro_status < 0) {                                          \
            fprintf(stderr,"Error: Failed to select hyperslab for dataset = '%s'.\n" \
                    "The dimensions of the dataset was %d\nThe file ID was %d\n.", \
                    dataset_name, (int32_t) *count, (int32_t) fd);      \
            return HDF5_ERROR;                                          \
        }                                                               \
        hid_t h5_memspace = H5Screate_simple(1, count, NULL);        \
        XRETURN(h5_memspace >= 0, HDF5_ERROR,                        \
                "Error: Failed to select hyperslab for dataset = %s.\n" \
                "The dimensions of the dataset was %d\nThe file ID was %d\n.", \
                dataset_name, (int32_t) *count, (int32_t) fd);          \
        const hid_t h5_dtype = H5Dget_type(h5_dset);                    \
        macro_status = H5Dread(h5_dset, h5_dtype, h5_memspace, h5_space, H5P_DEFAULT, buffer); \
        XRETURN(macro_status >= 0, HDF5_ERROR, "Error: Failed to read array for dataset = '%s'.\n" \
                "The dimensions of the dataset was %d\nThe file ID was %d\n.", \
                dataset_name, (int32_t) *count, (int32_t) fd);          \
        XRETURN(H5Dclose(h5_dset) >= 0, HDF5_ERROR,                     \
                "Error: Could not close dataset = '%s' (within group = '%s')\n", \
                dataset_name, group_name);                              \
        XRETURN(H5Tclose(h5_dtype) >= 0, HDF5_ERROR,                    \
                "Error: Failed to close the datatype for = %s.\n"       \
                "The dimensions of the dataset was %d\nThe file ID was %d\n.", \
                dataset_name, (int32_t) *count, (int32_t) fd);          \
        XRETURN(H5Sclose(h5_memspace) >= 0, HDF5_ERROR,                 \
                "Error: Failed to close the dataspace for = %s.\n"      \
                "The dimensions of the dataset was %d\nThe file ID was %d\n.", \
                dataset_name, (int32_t) *count, (int32_t) fd);          \
        XRETURN(H5Gclose(h5_grp) >= 0, HDF5_ERROR, "Error: Could not close group = '%s'\n", group_name); \
    }

    {
        /* Intentionally written in new scope (separate '{')

           For the first forest on this task, we need to start at some arbitrary index within the snapshot group.
           This index is simply the sum of the number of halos at that snapshot located within all preceeding
           forests (these preceeding forests are processed on other tasks). In this section, we simply assign the cumulative
           sum as the 'offset' to start reading from for the first forest.
        */
        char fname[5*MAX_STRING_LEN];
        snprintf(fname, sizeof(fname), "%s.%d", filename, start_filenum);
        hid_t h5_fd = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
        if(h5_fd < 0) {
            fprintf(stderr,"Error: On ThisTask = %d can't open the first file to process. filename is '%s'\n", run_params->ThisTask, fname);
            return FILE_NOT_FOUND;
        }
        const int64_t offset_for_forestnum = offset_for_global_forestnum[start_filenum];
        for(int isnap=0;isnap<gen->maxsnaps;isnap++) {
            int64_t snap_offset = 0; /* declared previously */
            for(int64_t iforest=0;iforest<start_forestnum_per_file[start_filenum];iforest++) {
                /* Read in the number of halos within each forest at this snapshot */
                int64_t nhalos;
                const hsize_t h5_single_item = 1;
                const hsize_t h5_global_forestnum = (hsize_t) offset_for_forestnum + iforest;
                READ_GENESIS_PARTIAL_FORESTINFO(gen->meta_fd, "ForestInfo", "ForestSizes", &h5_global_forestnum, &h5_single_item, &nhalos);
                snap_offset += nhalos;
            }
            gen->halo_offset_per_snap[isnap] = snap_offset;
        }
        XRETURN(H5Fclose(h5_fd) >= 0, HDF5_ERROR,
                "Error: On ThisTask = %d could not close file descriptor for filename = '%s'\n", ThisTask, fname);
    }


    /* Now malloc the relevant arrays in forests_info->gen */
    gen->h5_fds = calloc(gen->totnfiles, sizeof(gen->h5_fds[0]));/* Allocates enough space to store all '(lastfile + 1)' hdf5 file descriptors
                                                                    (out of these file descriptors, only
                                                                    numfiles := (end_filenum - start_filenum + 1) are actually used
                                                                    The wasted space is small, and the indexing is a lot easier
                                                                    The assumption is that no one will have >= 10k files. -- MS 26/11/2109
                                                                 */
    XRETURN(gen->h5_fds != NULL, MALLOC_FAILURE, "Error: Could not allocate memory to hold %d hdf5 file descriptors (each of size = %zu bytes)\n",
            gen->numfiles, sizeof(gen->h5_fds[0]));
    for(int i=0;i<totnfiles;i++) {
        gen->h5_fds[i] = -1;/* Initialise to enable later checks */
    }


    for(int i=start_filenum;i<=end_filenum;i++) {
        char fname[5*MAX_STRING_LEN];
        snprintf(fname, sizeof(fname), "%s.%d", filename, i);
        gen->h5_fds[i] = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        XRETURN(gen->h5_fds[i] >= 0, FILE_NOT_FOUND,
                "Error: On ThisTask = %d can't open file forest file '%s'\n",
                ThisTask, fname);
    }

    /* Perform some consistency checks from the first file */
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header", "NSnaps", (run_params->nsnapshots));
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Particle_mass", "DarkMatter", (run_params->PartMass));
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Simulation", "Omega_m", (run_params->Omega));
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Simulation", "Omega_Lambda", (run_params->OmegaLambda));
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Simulation", "h_val", (run_params->Hubble_h));
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Simulation", "Period", (run_params->BoxSize));

    double lunit, munit, vunit;

    /* Read in units from the Genesis forests file */
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Units", "Length_unit_to_kpc", lunit);
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Units", "Velocity_unit_to_kms", vunit);
    READ_GENESIS_ATTRIBUTE(gen->h5_fds[start_filenum], "/Header/Units", "Mass_unit_to_solarmass", munit);

    /* convert the units to the appropriate cgs values */
    lunit *= CM_PER_MPC * 1e-3; /* convert from kpc to cm */
    vunit *= 1e5; /* convert to cm/s */
    munit *= SOLAR_MASS;/* convert from 1e10 Msun to gm */

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

    CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Length Unit", lunit, run_params->UnitLength_in_cm, maxdiff, maxreldiff);
    CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Velocity Unit", vunit, run_params->UnitVelocity_in_cm_per_s, maxdiff, maxreldiff);
    CHECK_AND_ABORT_UNITS_VS_PARAM_FILE("Mass Unit", munit, run_params->UnitMass_in_g, maxdiff, maxreldiff);

#undef CHECK_AND_ABORT_UNITS_VS_PARAM_FILE


    int num_props_assigned = 0;
#define ASSIGN_GALAXY_PROPERTY_NAME(propertyname, enumval) {            \
        snprintf(galaxy_property_names[enumval], MAX_STRING_LEN-1, "%s", propertyname); \
        num_props_assigned++;                                           \
    }

    ASSIGN_GALAXY_PROPERTY_NAME("Head", head_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Tail", tail_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("hostHaloID", hosthaloid_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Mass_200crit", m200c_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Xc", xc_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Yc", yc_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Zc", zc_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("VXc", vxc_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("VYc", vyc_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("VZc", vzc_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Lx", lx_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Ly", ly_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Lz", lz_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("sigV", veldisp_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("ID", mostboundid_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("npart", len_enum);
    ASSIGN_GALAXY_PROPERTY_NAME("Vmax", vmax_enum);
    if(num_props_assigned != num_galaxy_props) {
        fprintf(stderr,"Error: Not all Genesis galaxy properties have been assigned properly...exiting\n");
        fprintf(stderr,"Expected to assign = %d galaxy properties but assigned %d properties instead\n", num_galaxy_props, num_props_assigned);
        return EXIT_FAILURE;
    }

#undef ASSIGN_GALAXY_PROPERTY_NAME




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
    free(totnforests_per_file);

    /* Finally setup the multiplication factors necessary to generate
       unique galaxy indices (across all files, all trees and all tasks) for this run*/
    run_params->FileNr_Mulfac = 1000000000000000LL;
    run_params->ForestNr_Mulfac =     1000000000LL;

    return EXIT_SUCCESS;
}



/*
  Fields in the particle data type, stored at each snapshot
  ['Efrac', 'ForestID', 'ForestLevel',
  'Head', 'HeadRank', 'HeadSnap',
  'ID',
  'Lx', 'Ly', 'Lz',
  'Mass_200crit', 'Mass_200mean', 'Mass_FOF', 'Mass_tot',
  'Num_descen', 'Num_progen',
  'RVmax_Lx', 'RVmax_Ly', 'RVmax_Lz',
  'RVmax_sigV', 'R_200crit', 'R_200mean',
  'R_HalfMass', 'R_size', 'Rmax',
  'RootHead', 'RootHeadSnap', 'RootTail', 'RootTailSnap',
  'Structuretype',
  'Tail', 'TailSnap',
  'VXc', 'VYc', 'VZc', 'Vmax',
  'Xc', 'Yc', 'Zc',
  'cNFW',
  'hostHaloID',
  'lambda_B',
  'npart',
  'numSubStruct',
  'sigV']


----------------------------
  From the ASTRO 3D wiki, here is info about the fields.

  This format as several key fields per snapshot:

  Head: A halo ID pointing the immediate descendant of a halo. With temporally unique ids, this id encodes both the snapshot that the descendant is at and the index in the properties array
  HeadSnap: The snapshot of the immediate descendant
  RootHead: Final descendant
  RootHeadSnap: Final descendant snapshot
  Tail: A halo ID pointing to the immediate progenitor
  TailSnap, RootTail, RootTailSnap: similar in operation to HeadSnap, RootHead, RootHeadSnap but for progenitors
  ID: The halo ID
  Num_progen: number of progenitors

  There are also additional fields that are present for Meraxes,

  ForestID: A unique id that groups all descendants of a field halo and any subhalos it may have contained (which can link halos together if one was initially a subhalo of the other).This is computationally intensive. Allows for quick parsing of all halos to identify those that interact across cosmic time.

  To walk the tree, one needs only to move forward/backward in time one just needs to get Head or Tail and access the data given by that ID.
  The temporally unique ID is given by:

  ID = snapshot*1e12 + halo index

----------------------------

 */


int64_t load_forest_genesis_hdf5(int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info, struct params *run_params)
{
    struct genesis_info *gen = &(forests_info->gen);
    int processing_first_forest = 0;
    if(gen->curr_filenum < 0) {
        processing_first_forest = 1;
        gen->curr_filenum = gen->start_filenum;
        if(forestnr != gen->start_forestnum) {
            fprintf(stderr,"Error: On ThisTask = %d looks like we are processing the first forest but forestnr = %"PRId64" "
                    "is not equal to start_forestnum = %"PRId64"\n", run_params->ThisTask, forestnr, gen->start_forestnum);
            return -1;
        }
    }

    const int filenum_for_forest = gen->FileNr[forestnr];

    /* Do the forest offsets have to be reset? Only reset if this is not the first forest
       The offsets for the first forest have been populated at the forest_setup stage  */
    if(processing_first_forest == 0 && gen->curr_filenum != filenum_for_forest) {
        /* This forest is in a new file (but this forest isn't the first forest being processed by this task)*/
        memset(gen->halo_offset_per_snap, 0, sizeof(gen->halo_offset_per_snap[0]) * gen->maxsnaps);
        gen->curr_filenum = filenum_for_forest;
    }
    const int filenum = gen->curr_filenum;

    if (gen->h5_fds[filenum] < 0) {
        fprintf(stderr, "The HDF5 file '%d' (corresponding to '%d'th file on ThisTask) should still be opened when reading the halos in the forest.\n",
                filenum_for_forest, filenum);
        fprintf(stderr, "For forest %"PRId64" we encountered error\n", forestnr);
        H5Eprint(gen->h5_fds[filenum], stderr);
        ABORT(NULL_POINTER_FOUND);
    }

    const int64_t forestnum_across_all_files = forestnr + gen->offset_for_global_forestnum[filenum];

    int64_t nhalos;
    const hsize_t h5_single_item = 1;
    const hsize_t h5_global_forestnum = (hsize_t) forestnum_across_all_files;
    /* Read the number of halos in this forest -> starting at offset 'forestnum_across_all_files'  */
    READ_GENESIS_PARTIAL_FORESTINFO(gen->meta_fd, "ForestInfo", "ForestSizes", &h5_global_forestnum, &h5_single_item, &nhalos);

    int64_t *nhalos_per_snap = calloc(gen->maxsnaps, sizeof(*nhalos_per_snap));
    XRETURN(nhalos_per_snap != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to store the number of halos per snapshot (forestnr = %"PRId64")\n",
            forestnr);

    int32_t *forest_local_offsets = calloc(gen->maxsnaps, sizeof(*forest_local_offsets));
    XRETURN(forest_local_offsets != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory for the storing the forest local offsets that separate each snapshot (forestnr = %"PRId64")\n",
            forestnr);

    int64_t offset=0;
    const int start_snap = gen->min_snapnum;
    const int end_snap = gen->min_snapnum + gen->maxsnaps - 1;//maxsnaps already includes a +1,
    for(int isnap=end_snap;isnap>=start_snap;isnap--) {
        if(offset > INT_MAX) {
            fprintf(stderr,"Error: In function %s> Can not correctly represent %"PRId64" as an offset in the 32-bit variable within "
                    "the LHaloTree struct. \n", __FUNCTION__, offset);
            ABORT(INTEGER_32BIT_TOO_SMALL);
        }
        char dset_name[MAX_STRING_LEN + 1];
        snprintf(dset_name, MAX_STRING_LEN, "ForestInfo/Snaps/Snaps_%d", isnap);
        READ_GENESIS_PARTIAL_FORESTINFO(gen->meta_fd, dset_name, "NumHalosInForest", &h5_global_forestnum, &h5_single_item, &(nhalos_per_snap[isnap]));

        forest_local_offsets[isnap] = offset;
        offset += nhalos_per_snap[isnap];
    }

    /* Check that the number of halos to read in agrees with that derived with the per snapshot one */
    if(offset != nhalos) {
        fprintf(stderr,"Error: On ThisTask = %d while processing task-local-forestnr = %"PRId64" "
                " file-local-forestnr = %"PRId64"and global forestnum = %"PRId64" located in the file = %d\n",
                run_params->ThisTask, forestnr, gen->forestnum_in_file[forestnr], forestnum_across_all_files, filenum);
        fprintf(stderr,"Expected the 'nhalos_per_snap' array to sum up to 'nhalos' but that is not the case\n");
        fprintf(stderr,"Sum(nhalos_per_snap) = %"PRId64" nhalos = %"PRId64"\n", offset, nhalos);
        fprintf(stderr,"Now printing out individual values of the nhalos_per_snap\n");
        for(int isnap=end_snap;isnap>=start_snap;isnap--) {
            fprintf(stderr,"nhalos_per_snap[%03d] = %09"PRId64"\n", isnap, nhalos_per_snap[isnap]);
        }
        fprintf(stderr,"Now printing out the offset need per file\n");
        for(int i=0;i<gen->totnfiles;i++) {
            fprintf(stderr,"gen->offset_for_global_forestnum[%04d] = %09"PRId64"\n", i, gen->offset_for_global_forestnum[i]);
        }

        ABORT(INVALID_VALUE_READ_FROM_FILE);
    }


    *halos = mymalloc(sizeof(struct halo_data) * nhalos);//the malloc failure check is done within mymalloc
    struct halo_data *local_halos = *halos;
    for(int64_t i=0;i<nhalos;i++) {
        local_halos[i].FirstHaloInFOFgroup = -1;
        local_halos[i].NextHaloInFOFgroup = -1;

        local_halos[i].FirstProgenitor = -1;
        local_halos[i].NextProgenitor = -1;
        local_halos[i].Descendant = -1;
    }

    // The max. size of the data to be read in would be "nhalos" * NDIM (== 3 for pos/vel) * sizeof(double)
    char *buffer = calloc(nhalos * NDIM * sizeof(double), sizeof(*buffer));
    if (buffer == NULL) {
        fprintf(stderr, "Could not allocate memory for the HDF5 multiple dimension buffer.\n");
        ABORT(MALLOC_FAILURE);
    }

#define READ_PARTIAL_1D_ARRAY(snap_group, isnap, dataset_enum, offset, count, buffer) { \
        hid_t h5_dset = H5Dopen2(snap_group, galaxy_property_names[dataset_enum], H5P_DEFAULT); \
        XRETURN(h5_dset >= 0, HDF5_ERROR,                               \
                "Error encountered when trying to open up dataset %s at snapshot = %d\n", \
                galaxy_property_names[dataset_enum], isnap);            \
        hid_t h5_fspace = H5Dget_space(h5_dset);                        \
        XRETURN(h5_fspace >= 0, HDF5_ERROR,                             \
                "Error encountered when trying to reserve filespace for dataset %s at snapshot = %d\n", \
                galaxy_property_names[dataset_enum], isnap);            \
        herr_t macro_status = H5Sselect_hyperslab(h5_fspace, H5S_SELECT_SET, offset, NULL, count, NULL); \
        XRETURN(macro_status >= 0, HDF5_ERROR,                          \
                "Error: Failed to select hyperslab for dataset = %s.\n" \
                "The dimensions of the dataset was %d.\n",              \
                galaxy_property_names[dataset_enum], (int32_t) *count); \
        hid_t h5_memspace = H5Screate_simple(1, count, NULL);           \
        XRETURN(h5_memspace >= 0, HDF5_ERROR,                           \
                "Error: Failed to select hyperslab for dataset = %s.\n" \
                "The dimensions of the dataset was %d\n.",              \
                galaxy_property_names[dataset_enum], (int32_t) *count); \
        const hid_t h5_dtype = H5Dget_type(h5_dset);                    \
        XRETURN(h5_dtype >= 0, HDF5_ERROR,                              \
                "Error: Failed to get datatype for dataset = %s.\n"     \
                "The dimensions of the dataset was %d\n.",              \
                galaxy_property_names[dataset_enum], (int32_t) *count); \
        macro_status = H5Dread(h5_dset, h5_dtype, h5_memspace, h5_fspace, H5P_DEFAULT, buffer); \
        XRETURN(macro_status >= 0, FILE_READ_ERROR,                     \
                "Error: Failed to read array for dataset = %s.\n"       \
                "The dimensions of the dataset was %d\n.",              \
                galaxy_property_names[dataset_enum], (int32_t) *count); \
        XRETURN(H5Dclose(h5_dset) >= 0, HDF5_ERROR,                     \
                "Error: Could not close dataset = '%s'.\n"              \
                "The dimensions of the dataset was %d\n.",              \
                galaxy_property_names[dataset_enum], (int32_t) *count); \
        XRETURN(H5Tclose(h5_dtype) >= 0, HDF5_ERROR,                    \
                "Error: Failed to close the datatype for = %s.\n"       \
                "The dimensions of the dataset was %d\n.",              \
                galaxy_property_names[dataset_enum], (int32_t) *count); \
        XRETURN(H5Sclose(h5_memspace) >= 0, HDF5_ERROR,                 \
                "Error: Failed to close the dataspace for = %s.\n"      \
                "The dimensions of the dataset was %d\n.",              \
                galaxy_property_names[dataset_enum], (int32_t) *count); \
    }

#define ASSIGN_BUFFER_TO_SAGE(nhalos, buffer_dtype, sage_name, sage_dtype) { \
        for(hsize_t i=0;i<nhalos[0];i++) {                                 \
            local_halos[i].sage_name = (sage_dtype) ((buffer_dtype *)buffer)[i]; \
        }                                                               \
    }

#define ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos, ndim, buffer_dtype, sage_name, sage_dtype) { \
        for(hsize_t i=0;i<nhalos[0];i++) {                              \
            for(int idim=0;idim<ndim;idim++) {                          \
                buffer_dtype *new_buf = ((buffer_dtype*)buffer) + idim*nhalos[0]; \
                local_halos[i].sage_name[idim] = (sage_dtype) new_buf[i]; \
            }                                                           \
        }                                                               \
    }

#define ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos, buffer_dtype, sage_name, snapnum, is_mergertree_index) { \
        for(hsize_t i=0;i<nhalos[0];i++) {                              \
            const int64_t macro_haloid = ((int64_t *) buffer)[i];       \
            const int64_t macro_snapshot = CONVERT_HALOID_TO_SNAPSHOT(macro_haloid); \
            const int64_t macro_haloindex = CONVERT_HALOID_TO_INDEX(macro_haloid); \
            /*if the halo is pointing to itself and the index is a mergertree index then */ \
            /* then follow the sage convention of setting as '-1'. Note that 'FirstHaloinFOFGroup' */ \
            /* would point to itself correctly (is_mergertree_index should be 'False' for */ \
            /* 'FirstHaloinFOFGroup') */                                \
            if(is_mergertree_index && macro_snapshot == snapnum && (hsize_t) macro_haloindex == i) { \
                local_halos[i].sage_name = -1;                          \
                continue;                                               \
            }                                                           \
            const int64_t macro_forest_local_index = forest_local_offsets[macro_snapshot] + macro_haloindex; \
            if(macro_forest_local_index > INT_MAX) {                    \
                fprintf(stderr,"Error: In function %s> Can not correctly represent %"PRId64" as an offset in the 32-bit variable within " \
                        "the LHaloTree struct. \n", __FUNCTION__, offset); \
                ABORT(INTEGER_32BIT_TOO_SMALL);                         \
            }                                                           \
            local_halos[i].sage_name = (int32_t) macro_forest_local_index; \
        }                                                               \
    }

    int64_t *forest_offsets = gen->halo_offset_per_snap;
    hid_t h5_fd = gen->h5_fds[filenum];
    for(int isnap=end_snap;isnap>=start_snap;isnap--) {
        hsize_t snap_offset[1], nhalos_snap[1];
        snap_offset[0] = (hsize_t) forest_offsets[isnap];
        nhalos_snap[0] = nhalos_per_snap[isnap];
        if(nhalos_snap[0] == 0) continue;

        char snap_group_name[MAX_STRING_LEN];
        snprintf(snap_group_name, MAX_STRING_LEN-1, "Snap_%03d", isnap);
        hid_t h5_grp = H5Gopen(h5_fd, snap_group_name, H5P_DEFAULT);
        XRETURN(h5_grp >= 0, HDF5_ERROR, "Error: Could not open group = `%s` corresponding to snapshot = %d\n",
                snap_group_name, isnap);

        /* Merger Tree Pointers */
        //Descendant, FirstProgenitor, NextProgenitor, FirstHaloInFOFgroup, NextHaloInFOFgroup
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, head_enum, snap_offset, nhalos_snap, buffer);

        /* Can not directly assign since 'Head' contains Descendant haloid which is too large to be contained
           within 32 bits. I will need a separate assignment to break up haloid into a local index + snapshot,
           and then use the forest-local offset for each snapshot */

        //the last parameter is '1' for mergertree indices and 0 otherwise
        ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos_snap, int64_t, Descendant, isnap, 1);

        //Same with 'Tail' -> 'FirstProgenitor'
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, tail_enum, snap_offset, nhalos_snap, buffer);
        //the last parameter is '1' for mergertree indices and 0 otherwise
        ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos_snap, int64_t, FirstProgenitor, isnap, 1);

        //And same with 'hostHaloID' -> FirstHaloinFOFGroup.
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, hosthaloid_enum, snap_offset, nhalos_snap, buffer);
        //the last parameter is '1' for mergertree indices and 0 otherwise
        ASSIGN_BUFFER_WITH_MERGERTREE_TO_SAGE(nhalos_snap, int64_t, FirstHaloInFOFgroup, isnap, 0);

        /* MS 3rd June, 2019: The LHaloTree convention (which sage uses) is that Mvir contains M200c. While this is DEEPLY confusing,
           I am using C 'unions' to reduce the confusion slightly. What will happen here is that 'Mass_200crit' will get
           assigned to the 'M200c' field within the halo struct; but that 'M200c' will also be accessible via 'Mvir'.
        */
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, m200c_enum, snap_offset, nhalos_snap, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_snap, double, M200c, float);//M200c is an alias for Mvir
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, vmax_enum, snap_offset, nhalos_snap, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_snap, double, Vmax, float);

        /* Read in the positions for the halo centre*/
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, xc_enum, snap_offset, nhalos_snap, buffer);
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, yc_enum, snap_offset, nhalos_snap, buffer + nhalos_snap[0]*sizeof(double));
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, zc_enum, snap_offset, nhalos_snap, buffer + 2*nhalos_snap[0]*sizeof(double));
        /* Assign to the Pos array within sage*/
        ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos_snap, NDIM, double, Pos, float);/* Be careful that the last argument is only used
                                                                             at the assignment stage (i.e., for type conversion)
                                                                             Otherwise, the memory areas would be accessed incorrectly
                                                                             and we would get corrupted results -- MS 27/11/2019 */

        /* Read in the halo velocities */
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, vxc_enum, snap_offset, nhalos_snap, buffer);
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, vyc_enum, snap_offset, nhalos_snap, buffer + nhalos_snap[0]*sizeof(double));
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, vzc_enum, snap_offset, nhalos_snap, buffer + 2*nhalos_snap[0]*sizeof(double));
        /* Assign to the appropriate vel array within sage*/
        ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos_snap, NDIM, double, Vel, float);

        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, len_enum,  snap_offset, nhalos_snap, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_snap, int64_t, Len, int32_t);

        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, mostboundid_enum,  snap_offset, nhalos_snap, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_snap, int64_t, MostBoundID, long long);

        /* Read in the angular momentum */
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, lx_enum, snap_offset, nhalos_snap, buffer);
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, ly_enum, snap_offset, nhalos_snap, buffer + nhalos_snap[0]*sizeof(double));
        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, lz_enum, snap_offset, nhalos_snap, buffer + 2*nhalos_snap[0]*sizeof(double));
        /* Assign to the appropriate vel array within sage*/
        ASSIGN_BUFFER_TO_NDIM_SAGE(nhalos_snap, NDIM, double, Spin, float);

        /* READ_PARTIAL_1D_ARRAY(h5_grp, isnap, m200b_enum, snap_offset, nhalos_snap, buffer); */
        /* ASSIGN_BUFFER_TO_SAGE(nhalos_snap, double, M_Mean200, float); */

        READ_PARTIAL_1D_ARRAY(h5_grp, isnap, veldisp_enum, snap_offset, nhalos_snap, buffer);
        ASSIGN_BUFFER_TO_SAGE(nhalos_snap, double, VelDisp, float);

        XRETURN(H5Gclose(h5_grp) >= 0, HDF5_ERROR, "Error: Could not close snapshot group = '%s'\n", snap_group_name);
        /* Done with all the reading for this snapshot */

        const double scale_factor = run_params->scale_factors[isnap];
        const double hubble_h = run_params->Hubble_h;
        for(hsize_t i=0;i<nhalos_snap[0];i++) {
            /* Fill up the remaining properties that are not within the GENESIS dataset */
            local_halos[i].SnapNum = isnap;
            local_halos[i].FileNr = 0;
            local_halos[i].SubhaloIndex = -1;
            local_halos[i].SubHalfMass = -1.0;

            /* Change the conventions across the entire forest to match the SAGE conventions */
            /* convert the masses into 1d10 Msun/h units */
            local_halos[i].M200c *= hubble_h * 1e-10;// M200c is an alias for Mvir
            for(int j=0;j<NDIM;j++) {
                local_halos[i].Pos[j] *= hubble_h / scale_factor;
                local_halos[i].Vel[j] /= scale_factor;
                local_halos[i].Spin[j] *= hubble_h * hubble_h * 1e-10;
            }
        }

        //Done reading all halos belonging to this forest at this snapshot
        local_halos += nhalos_snap[0];
    }
    //Done reading all halos belonging to this forest (across all snapshots)
    local_halos -= nhalos;//rewind the local_halos to the first halo in this forest

    //Populate the NextProg, NexthaloinFofgroup indices. FirstHaloinFOFGroup, Descendant, FirstProgenitor should already be set correctly


    //First populate the NextProg
    for(int64_t i=0;i<nhalos;i++) {
        int32_t desc = local_halos[i].Descendant;
        if(desc == -1) continue;

        int32_t first_prog_of_desc_halo = local_halos[desc].FirstProgenitor;
        if(first_prog_of_desc_halo == -1) {
            //THIS can not happen. FirstProg should have been assigned correctly already
            fprintf(stderr,"Error: FirstProgenitor can not be -1\n");
            ABORT(EXIT_FAILURE);
        }

        //if the first progenitor is this current halo, then nothing to do here
        if(first_prog_of_desc_halo == i) continue;

        //So the current halo is not the first progenitor - so we need to assign this current halo
        int32_t next_prog = first_prog_of_desc_halo;
        while(local_halos[next_prog].NextProgenitor != -1) {
            next_prog = local_halos[next_prog].NextProgenitor;
        }
        if(next_prog < 0 || next_prog >= nhalos || local_halos[next_prog].NextProgenitor != -1) {
            fprintf(stderr,"Error: In function %s> Bug in code logic. next_prog = %d must be within [0, %"PRId64") "
                    "*AND* local_halos[next_prog].NextProgenitor = %d should be '-1'",
                    __FUNCTION__, next_prog, nhalos, local_halos[next_prog].NextProgenitor);
            return -1;
        }
        local_halos[next_prog].NextProgenitor = i;
    }

    //Now populate the NextHaloinFOFGroup
    for(int64_t i=0;i<nhalos;i++) {
        int32_t fofhalo = local_halos[i].FirstHaloInFOFgroup;
        if(fofhalo == -1) {
            //This can not happen. FirstHaloinFOF should already be set correctly
            fprintf(stderr,"Error: FOFhalo can not be -1\n");
            ABORT(EXIT_FAILURE);
        }
        //if the FOFhalo is this current halo, then nothing to do here
        if(fofhalo == i) continue;

        int32_t next_halo = fofhalo;
        while(local_halos[next_halo].NextHaloInFOFgroup != -1) {
            next_halo = local_halos[next_halo].NextHaloInFOFgroup;
        }
        if(next_halo < 0 || next_halo >= nhalos || local_halos[next_halo].NextHaloInFOFgroup != -1) {
            fprintf(stderr,"Error: In function %s> Bug in code logic. next_halo = %d must be within [0, %"PRId64")"
                    " *AND* local_halos[next_halo].NextHaloInFOFgroup=%d should be '-1'",
                    __FUNCTION__, next_halo, nhalos, local_halos[next_halo].NextHaloInFOFgroup);
            return -1;
        }
        local_halos[next_halo].NextHaloInFOFgroup = i;
    }

    const int32_t lastsnap = local_halos[0].SnapNum;
    const int64_t numhalos_last_snap = nhalos_per_snap[lastsnap];;
    int status = fix_flybys_genesis(local_halos, numhalos_last_snap);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    free(forest_local_offsets);

    /* We have loaded in this forest -> now update the offsets so that we can
       correctly read in the next forest from this file. If the next forest
       is in a new file, then there is a condition at the top of this function that
       will reset all the halo_offset values to 0 -- MS 21/11/2019  */
    for(int isnap=end_snap;isnap>=start_snap;isnap--) {
        gen->halo_offset_per_snap[isnap] += nhalos_per_snap[isnap];
    }
    gen->curr_filenum = filenum_for_forest;

    free(nhalos_per_snap);

    return nhalos;
}

#undef READ_PARTIAL_1D_ARRAY
#undef ASSIGN_BUFFER_TO_SAGE
#undef ASSIGN_BUFFER_TO_NDIM_SAGE

void cleanup_forests_io_genesis_hdf5(struct forest_info *forests_info)
{
    struct genesis_info *gen = &(forests_info->gen);

    for(int i=0;i<gen->numfiles;i++) {
        H5Fclose(gen->h5_fds[i]);
    }

    free(gen->halo_offset_per_snap);
    free(gen->h5_fds);
    free(gen->FileNr);
    free(gen->forestnum_in_file);
    free(gen->offset_for_global_forestnum);
    free(gen->num_forests_to_process_per_file);
    free(gen->start_forestnum_per_file);

}

#define CHECK_IF_HALO_IS_FOF(halos, index)  (halos[index].FirstHaloInFOFgroup == index ? 1:0)

int fix_flybys_genesis(struct halo_data *halos, const int64_t nhalos_last_snap)
{
    if(nhalos_last_snap == 0) {
        fprintf(stderr,"Warning: There are no halos at the last snapshot. Therefore nothing to fix for flybys. BUT this should not happen - check code\n");
        return EXIT_SUCCESS;
    }
    if(halos == NULL || nhalos_last_snap < 0) {
        fprintf(stderr,"Error: In function %s> The struct containing halo data (address = %p )can not be NULL *AND* the total number of halos (=%"PRId64") must be > 0\n",
                __FUNCTION__, halos, nhalos_last_snap);
        return EXIT_FAILURE;
    }


    int64_t num_fofs = 0;
    for(int64_t i=0;i<nhalos_last_snap;i++) {
        num_fofs += CHECK_IF_HALO_IS_FOF(halos, i);
    }

    if(num_fofs == 0) {
        fprintf(stderr,"Error: There are no FOF halos at the last snapshot. This is highly unusual and almost certainly a bug (in reading the data)\n");
        return EXIT_FAILURE;
    }

    /* Is there anything to do? If there is only one FOF at z=0, then simply return */
    if(num_fofs == 1) {
        return EXIT_SUCCESS;
    }

    int64_t max_mass_fof_loc = -1;
    float max_mass_fof = -1.0f;
    for(int64_t i=0;i<nhalos_last_snap;i++) {
        if(halos[i].Mvir > max_mass_fof && CHECK_IF_HALO_IS_FOF(halos, i) == 1) {
            max_mass_fof_loc = i;
            max_mass_fof = halos[max_mass_fof_loc].Mvir;
        }
    }

    XASSERT(max_mass_fof_loc < INT_MAX, EXIT_FAILURE,
            "Most massive FOF location=%"PRId64" must be representable within INT_MAX=%d",
            max_mass_fof_loc, INT_MAX);

    int FirstHaloInFOFgroup = (int) max_mass_fof_loc;
    int insertion_point_next_sub = FirstHaloInFOFgroup;
    while(halos[insertion_point_next_sub].NextHaloInFOFgroup != -1) {
        insertion_point_next_sub = halos[insertion_point_next_sub].NextHaloInFOFgroup;
    }

    if(insertion_point_next_sub < 0 || insertion_point_next_sub>= nhalos_last_snap || halos[insertion_point_next_sub].NextHaloInFOFgroup != -1) {
        fprintf(stderr,"bug in code logic in previous while loop at line=%d in file=%s\n", __LINE__, __FILE__);
        return -1;
    }

    for(int64_t i=0;i<nhalos_last_snap;i++) {
        if(i == FirstHaloInFOFgroup) {
            continue;
        }

        //Only need to switch for other FOF halos
        if(CHECK_IF_HALO_IS_FOF(halos, i) == 1) {
            //Show that this halo was switched from being a central
            //just flip the sign. (MostBoundID should not have negative
            //values -> this would signify a flyby)
            halos[i].MostBoundID = -halos[i].MostBoundID;
            halos[insertion_point_next_sub].NextHaloInFOFgroup = i;
            halos[i].FirstHaloInFOFgroup = FirstHaloInFOFgroup;

            //Now figure out where the next FOF halo (if any) would need to be attached
            insertion_point_next_sub = i;
            while(halos[insertion_point_next_sub].NextHaloInFOFgroup != -1) {
                insertion_point_next_sub = halos[insertion_point_next_sub].NextHaloInFOFgroup;
                halos[insertion_point_next_sub].FirstHaloInFOFgroup = FirstHaloInFOFgroup;
            }

            if(insertion_point_next_sub < 0 || insertion_point_next_sub>= nhalos_last_snap || halos[insertion_point_next_sub].NextHaloInFOFgroup != -1) {
                fprintf(stderr,"bug in code logic in previous while loop at line=%d in file=%s\n", __LINE__, __FILE__);
                return -1;
            }
        }
    }

    return EXIT_SUCCESS;
}
#undef CHECK_IF_HALO_IS_FOF
