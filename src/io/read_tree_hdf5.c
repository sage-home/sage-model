#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>

#include "read_tree_hdf5.h"
#include "../core_mymalloc.h"


/* Local Structs */
struct METADATA_NAMES 
{
  char name_NTrees[MAX_STRING_LEN];
  char name_totNHalos[MAX_STRING_LEN];
  char name_TreeNHalos[MAX_STRING_LEN];
}; 

/* Local Proto-Types */
static int32_t fill_metadata_names(struct METADATA_NAMES *metadata_names, enum Valid_TreeTypes my_TreeType);
static int32_t read_attribute_int(hid_t my_hdf5_file, const char *groupname, const char *attr_name, int *attribute);
static int32_t read_dataset(hid_t fd, const char *dataset_name, const hid_t datatype, void *buffer);

void get_forests_filename_lht_hdf5(char *filename, const size_t len, const int filenr)
{
    snprintf(filename, len - 1, "%s/%s.%d%s", run_params.SimulationDir, run_params.TreeName, filenr, run_params.TreeExtension);
}


int setup_forests_io_lht_hdf5(struct forest_info *forests_info, const int firstfile, const int lastfile,
                              const int ThisTask, const int NTasks)
{

    const int numfiles = lastfile - firstfile + 1;
    if(numfiles <= 0) {
        return -1;
    }
    
    /* wasteful to allocate for lastfile + 1 indices, rather than numfiles; but makes indexing easier */
    int32_t *totnforests_per_file = malloc((lastfile + 1) * sizeof(totnforests_per_file[0]));
    if(totnforests_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests in each file\n");
        perror(NULL);
        return MALLOC_FAILURE;
    }
    struct METADATA_NAMES metadata_names;
    int64_t totnforests = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_lht_hdf5(filename, 4*MAX_STRING_LEN, filenr);
        hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        if(fd < 0) {
            fprintf(stderr, "Error: can't open file `%s'\n", filename);
            perror(NULL);
            ABORT(FILE_NOT_FOUND);
        }
        int status = fill_metadata_names(&metadata_names, run_params.TreeType);
        if (status != EXIT_SUCCESS) {
            ABORT(status);
        }

        int32_t nforests;
        status = read_attribute_int(fd, "/Header", metadata_names.name_NTrees, &nforests);
        if (status != EXIT_SUCCESS) {
            fprintf(stderr, "Error while processing file %s\n", filename);
            fprintf(stderr, "Error code is %d\n", status);
            ABORT(status);
        }
        totnforests_per_file[filenr] = nforests;
        totnforests += nforests;
        
        H5Fclose(fd);
    }
    forests_info->totnforests = totnforests;

    const int64_t nforests_per_cpu = (int64_t) (totnforests/NTasks);
    const int64_t rem_nforests = totnforests % NTasks;
    int64_t nforests_this_task = nforests_per_cpu;
    if(ThisTask < rem_nforests) {
        nforests_this_task++;
    }

    int64_t start_forestnum = nforests_per_cpu * ThisTask;
    /* Add in the remainder forests that also need to be processed
       equivalent to the loop

       for(int task=0;task<ThisTask;task++) {
           if(task < rem_nforests) start_forestnum++;
       }

     */
    start_forestnum += ThisTask <= rem_nforests ? ThisTask:rem_nforests; /* assumes that "0<= ThisTask < NTasks" */
    const int64_t end_forestnum = start_forestnum + nforests_this_task; /* not inclusive, i.e., do not process forestnr == end_forestnum */
    
    int64_t *num_forests_to_process_per_file = calloc((lastfile + 1), sizeof(num_forests_to_process_per_file[0]));/* calloc is required */
    int64_t *start_forestnum_to_process_per_file = malloc((lastfile + 1) * sizeof(start_forestnum_to_process_per_file[0]));
    if(num_forests_to_process_per_file == NULL || start_forestnum_to_process_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests that need to be processed per file (on thistask=%d)\n", ThisTask);
        perror(NULL);
        return MALLOC_FAILURE;
    }

    /* show no forests need to be processed by default */
    for(int i=0;i<=lastfile;i++) {
        start_forestnum_to_process_per_file[i] = -1;
        num_forests_to_process_per_file[i] = 0;
    }
    
    int start_filenum = -1, end_filenum = -1;
    int64_t nforests_so_far = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        const int32_t nforests_this_file = totnforests_per_file[filenr];
        const int64_t end_forestnum_this_file = nforests_so_far + nforests_this_file;
        start_forestnum_to_process_per_file[filenr] = 0;
        num_forests_to_process_per_file[filenr] = nforests_this_file;

        if(start_forestnum >= nforests_so_far && start_forestnum < end_forestnum_this_file) {
            start_filenum = filenr;
            start_forestnum_to_process_per_file[filenr] = start_forestnum - nforests_so_far;
            num_forests_to_process_per_file[filenr] = nforests_this_file - (start_forestnum - nforests_so_far);
        }

        
        if(end_forestnum >= nforests_so_far && end_forestnum <= end_forestnum_this_file) {
            end_filenum = filenr;
            num_forests_to_process_per_file[filenr] = end_forestnum - nforests_so_far;/* does this need a + 1? */
        }
        nforests_so_far += nforests_this_file;
    }

    if(start_filenum == -1 || end_filenum == -1 ) {
        fprintf(stderr,"Error: Could not locate start or end file number for the lhalotree binary files\n");
        fprintf(stderr,"Printing debug info\n");
        fprintf(stderr,"ThisTask = %d NTasks = %d totnforests = %"PRId64" start_forestnum = %"PRId64" nforests_this_task = %"PRId64"\n",
                ThisTask, NTasks, totnforests, start_forestnum, nforests_this_task);
        for(int filenr=firstfile;filenr<=lastfile;filenr++) {
            fprintf(stderr,"filenr := %d contains %d forests\n",filenr, totnforests_per_file[filenr]);
        }
                
        return -1;
    }

    struct lhalotree_info *lht = &(forests_info->lht);
    forests_info->nforests_this_task = nforests_this_task;
    lht->nforests = nforests_this_task;
    /* lht->nhalos_per_forest = mymalloc(nforests_this_task * sizeof(lht->nhalos_per_forest[0])); */
    lht->bytes_offset_for_forest = mymalloc(nforests_this_task * sizeof(lht->bytes_offset_for_forest[0]));
    lht->h5_fd = mymalloc(nforests_this_task * sizeof(lht->h5_fd[0]));
    lht->numfiles = end_filenum - start_filenum + 1;
    lht->open_h5_fds = mymalloc(lht->numfiles * sizeof(lht->open_h5_fds[0]));

    nforests_so_far = 0;
    /* int *forestnhalos = lht->nhalos_per_forest; */
    for(int filenr=start_filenum;filenr<=end_filenum;filenr++) {
        XASSERT(start_forestnum_to_process_per_file[filenr] >= 0 && start_forestnum_to_process_per_file[filenr] < totnforests_per_file[filenr],
                EXIT_FAILURE,
                "Error: Num forests to process = %"PRId64" for filenr = %d should be in range [0, %d)\n",
                start_forestnum_to_process_per_file[filenr],
                filenr,
                totnforests_per_file[filenr]);

        XASSERT(num_forests_to_process_per_file[filenr] >= 0 && num_forests_to_process_per_file[filenr] <= totnforests_per_file[filenr],
                EXIT_FAILURE,
                "Error: Num forests to process = %"PRId64" for filenr = %d should be in range [0, %d)\n",
                num_forests_to_process_per_file[filenr],
                filenr,
                totnforests_per_file[filenr]);

        int file_index = filenr - start_filenum;
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_lht_hdf5(filename, 4*MAX_STRING_LEN, filenr);
        hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        if(fd < 0) {
            fprintf(stderr, "Error: can't open file `%s'\n", filename);
            perror(NULL);
            ABORT(FILE_NOT_FOUND);
        }
        lht->open_h5_fds[file_index] = fd;/* keep the file open, will be closed at the cleanup stage */
        
        const int64_t nforests = num_forests_to_process_per_file[filenr];
        /* const size_t nbytes = totnforests_per_file[filenr] * sizeof(int32_t); */
        /* int32_t *nhalos_per_forest = malloc(nbytes); */
        /* if(nhalos_per_forest == NULL) { */
        /*     fprintf(stderr,"Error: Could not allocate memory to read nhalos per forest. Bytes requested = %zu\n", nbytes); */
        /*     perror(NULL); */
        /*     ABORT(MALLOC_FAILURE); */
        /* } */
        /* mypread(fd, nhalos_per_forest, nbytes, 8); /\* the last argument says to start after sizeof(totntrees) + sizeof(totnhalos) *\/ */
        /* memcpy(forestnhalos, &(nhalos_per_forest[start_forestnum_to_process_per_file[filenr]]), nforests * sizeof(forestnhalos[0])); */

        /* free(nhalos_per_forest); */
        
        /* nforests_so_far = forestnhalos - lht->nhalos_per_forest; */
        for(int64_t i=0;i<nforests;i++) {
            /* lht->bytes_offset_for_forest[i + nforests_so_far] = byte_offset_to_halos; */
            lht->h5_fd[i + nforests_so_far] = fd;
            /* byte_offset_to_halos += forestnhalos[i]*sizeof(struct halo_data); */
        }
        nforests_so_far += nforests;
    }

    free(num_forests_to_process_per_file);
    free(start_forestnum_to_process_per_file);
    free(totnforests_per_file);

    return EXIT_SUCCESS;

    /* status = read_attribute_int(forests_info->hdf5_fp, "/Header", metadata_names.name_totNHalos, &totNHalos); */
    /* if (status != EXIT_SUCCESS) {  */
    /*     fprintf(stderr, "Error while processing file %s\n", filename); */
    /*     fprintf(stderr, "Error code is %d\n", status); */
    /*     ABORT(0); */
    /* } */
    
    /* forests_info->totnhalos_per_forest = mycalloc(nforests, sizeof(int));  */
    /* status = read_attribute_int(forests_info->hdf5_fp, "/Header", metadata_names.name_TreeNHalos, forests_info->totnhalos_per_forest); */
    /* if (status != EXIT_SUCCESS) { */
    /*     fprintf(stderr, "Error while processing file %s\n", filename); */
    /*     fprintf(stderr, "Error code is %d\n", status); */
    /*     ABORT(0); */
    /* } */
}

#define READ_TREE_PROPERTY(fd, sage_name, hdf5_name, h5_dtype, C_dtype) \
    {                                                                   \
        snprintf(dataset_name, MAX_STRING_LEN - 1, "tree_%03d/%s", forestnr, #hdf5_name); \
        status = read_dataset(fd, dataset_name, h5_dtype, buffer);      \
        if (status != EXIT_SUCCESS) {                                   \
            ABORT(status);                                              \
        }                                                               \
        for (int halo_idx = 0; halo_idx < nhalos; ++halo_idx) {         \
            local_halos[halo_idx].sage_name = ((C_dtype*)buffer)[halo_idx]; \
        }                                                               \
    }

#define READ_TREE_PROPERTY_MULTIPLEDIM(fd, sage_name, hdf5_name, h5_dtype, C_dtype) \
    {                                                                   \
        snprintf(dataset_name, MAX_STRING_LEN - 1, "tree_%03d/%s", forestnr, #hdf5_name); \
        status = read_dataset(fd, dataset_name, h5_dtype, buffer_multipledim); \
        if (status != EXIT_SUCCESS) {                                   \
            ABORT(status);                                              \
        }                                                               \
        for (int halo_idx = 0; halo_idx < nhalos; ++halo_idx) {         \
            for (int dim = 0; dim < NDIM; ++dim) {                      \
                local_halos[halo_idx].sage_name[dim] = ((C_dtype*)buffer_multipledim)[halo_idx * NDIM + dim]; \
            }                                                           \
        }                                                               \
    }       
        

int64_t load_forest_hdf5(const int32_t forestnr, struct halo_data **halos, struct forest_info *forests_info)
{
    char dataset_name[MAX_STRING_LEN];
    int32_t status;

    double *buffer; // Buffer to hold the read HDF5 data.  
    // The largest data-type will be double. 

    double *buffer_multipledim; // However also need a buffer three times as large to hold data such as position/velocity.

    /* const int64_t nhalos = (int64_t) forests_info->lht.nhalos_per_forest[forestnr];/\* the array itself contains int32_t, since the LHT format*\/ */
    hid_t fd = forests_info->lht.h5_fd[forestnr];

    /* Figure out nhalos by checking the size of the 'Descendant' dataset */
    /* https://stackoverflow.com/questions/15786626/get-the-dimensions-of-a-hdf5-dataset */
    const char field_name[] = "Descendant";
    snprintf(dataset_name, MAX_STRING_LEN - 1, "tree_%03d/%s", forestnr, field_name);
    hid_t dspace = H5Dget_space(dataset_name);
    const int ndims = H5Sget_simple_extent_ndims(dspace);
    XASSERT(ndims == 0, -1,
            "Error: Expected field = '%s' to be 1-D array\n",
            field_name);
    hsize_t dims[ndims];
    H5Sget_simple_extent_dims(dspace, dims, NULL);
    const int64_t nhalos = dims[0];

    /* CHECK: HDF5 file pointer is valid */
    /* if(fd <= 0 ) { */
    /*     fprintf(stderr,"Error: File pointer is NULL (i.e., you need to open the file before reading) \n"); */
    /*     ABORT(INVALID_FILE_POINTER); */
    /* } */

    buffer = malloc(nhalos * sizeof(buffer[0]));
    if (buffer == NULL) {
        fprintf(stderr, "Error:Could not allocate memory for %"PRId64" halos in the HDF5 buffer.\n", nhalos);
        ABORT(MALLOC_FAILURE);
    }

    buffer_multipledim = malloc(nhalos * NDIM * sizeof(buffer_multipledim[0]));
    if (buffer_multipledim == NULL) {
        fprintf(stderr, "Could not allocate memory for %"PRId64" halos in the HDF5 multiple dimension buffer.\n", nhalos);
        ABORT(MALLOC_FAILURE);
    }

    // We now need to read in all the halo fields for this forest.
    // To do so, we read the field into a buffer and then properly slot the field into the Halo struct.

    /* Merger Tree Pointers */ 
    READ_TREE_PROPERTY(fd, Descendant, Descendant, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, FirstProgenitor, FirstProgenitor, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, NextProgenitor, NextProgenitor, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, FirstHaloInFOFgroup, FirstHaloInFOFgroup, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, NextHaloInFOFgroup, NextHaloInFOFgroup, H5T_NATIVE_INT, int);

    /* Halo Properties */ 
    READ_TREE_PROPERTY(fd, Len, Len, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, M_Mean200, M_mean200, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY(fd, Mvir, Mvir, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY(fd, M_TopHat, M_TopHat, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, Pos, Pos, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, Vel, Vel, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY(fd, VelDisp, VelDisp, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY(fd, Vmax, Vmax, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY_MULTIPLEDIM(fd, Spin, Spin, H5T_NATIVE_FLOAT, float);
    READ_TREE_PROPERTY(fd, MostBoundID, MostBoundID, H5T_NATIVE_LLONG, long long);

    /* File Position Info */
    READ_TREE_PROPERTY(fd, SnapNum, SnapNum, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, FileNr, Filenr, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, SubhaloIndex, SubHaloIndex, H5T_NATIVE_INT, int);
    READ_TREE_PROPERTY(fd, SubHalfMass, SubHalfMass, H5T_NATIVE_INT, int);

    free(buffer);
    free(buffer_multipledim);

#ifdef DEBUG_HDF5_READER 
    for (int32_t i = 0; i < 20; ++i) {
        printf("halo %d: Descendant %d FirstProg %d x %.4f y %.4f z %.4f\n", i, local_halos[i].Descendant, local_halos[i].FirstProgenitor, local_halos[i].Pos[0], local_halos[i].Pos[1], local_halos[i].Pos[2]); 
    }
    ABORT(0);
#endif 

    return nhalos;
}

#undef READ_TREE_PROPERTY
#undef READ_TREE_PROPERTY_MULTIPLEDIM

    
void cleanup_forests_io_lht_hdf5(struct forest_info *forests_info)
{
    struct lhalotree_info *lht = &(forests_info->lht);
    myfree(lht->nhalos_per_forest);
    myfree(lht->h5_fd);
    for(int32_t i=0;i<lht->numfiles;i++) {
        /* could use 'H5close' instead to make sure any open datasets are also
           closed; but that would hide potential bugs in code.
           valgrind should pick those cases up  */
        H5Fclose(lht->open_h5_fds[i]);
    }
    myfree(lht->open_h5_fds);
}


/* Local Functions */
static int32_t fill_metadata_names(struct METADATA_NAMES *metadata_names, enum Valid_TreeTypes my_TreeType)
{
    switch (my_TreeType) {
    case genesis_lhalo_hdf5: 
        
        snprintf(metadata_names->name_NTrees, MAX_STRING_LEN - 1, "NTrees"); // Total number of forests within the file.
        snprintf(metadata_names->name_totNHalos, MAX_STRING_LEN - 1, "totNHalos"); // Total number of halos within the file.
        snprintf(metadata_names->name_TreeNHalos, MAX_STRING_LEN - 1, "TreeNHalos"); // Number of halos per forest within the file.
        return EXIT_SUCCESS;
        
    case lhalo_binary: 
        fprintf(stderr, "If the file is binary then this function should never be called.  Something's gone wrong...");
        return EXIT_FAILURE;
        
    default:
        fprintf(stderr, "Your tree type has not been included in the switch statement for ``%s`` in file ``%s``.\n", __FUNCTION__, __FILE__);
        fprintf(stderr, "Please add it there.\n");
        ABORT(EXIT_FAILURE);
        
    }

    return EXIT_FAILURE;
}

static int32_t read_attribute_int(hid_t fd, const char *group_name, const char *attr_name, int *attribute)
{
    hid_t attr_id = H5Aopen_by_name(fd, group_name, attr_name, H5P_DEFAULT, H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "Could not open the attribute %s in group %s\n", attr_name, group_name);
        return attr_id;
    }

    int status = H5Aread(attr_id, H5T_NATIVE_INT, attribute);
    if (status < 0) {
        fprintf(stderr, "Could not read the attribute %s in group %s\n", attr_name, group_name);
        return status;
    }
    
    status = H5Aclose(attr_id);
    if (status < 0) {
        fprintf(stderr, "Error when closing the attribute %s in group %s.\n", attr_name, group_name); 
        H5Eprint(status, stderr);
        return status;
    }
   
    return EXIT_SUCCESS; 
}

static int32_t read_dataset(hid_t fd, const char *dataset_name, const hid_t datatype, void *buffer)
{
    hid_t dataset_id;

    dataset_id = H5Dopen2(fd, dataset_name, H5P_DEFAULT);
    if (dataset_id < 0) {
        fprintf(stderr, "Error encountered when trying to open up dataset %s\n", dataset_name); 
        H5Eprint(dataset_id, stderr);
        ABORT(FILE_READ_ERROR);
    }

    H5Dread(dataset_id, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
    H5Dclose(dataset_id);
  
    return EXIT_SUCCESS;
}
