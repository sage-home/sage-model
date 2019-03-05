#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "read_tree_binary.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"


/* Externally visible Functions */
void get_forests_filename_lht_binary(char *filename, const size_t len, const int filenr, const struct params *run_params)
{
    snprintf(filename, len - 1, "%s/%s.%d%s", run_params->SimulationDir, run_params->TreeName, filenr, run_params->TreeExtension);
}

int setup_forests_io_lht_binary(struct forest_info *forests_info, const int firstfile, const int lastfile,
                                const int ThisTask, const int NTasks, const struct params *run_params)
{
    const int numfiles = lastfile - firstfile + 1;
    if(numfiles <= 0) {
        return -1;
    }
    
    /* wasteful to allocate for lastfile + 1 indices, rather than numfiles; but makes indexing easier */
    int32_t *totnforests_per_file = calloc(lastfile + 1, sizeof(totnforests_per_file[0]));
    if(totnforests_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests in each file\n");
        perror(NULL);
        return MALLOC_FAILURE;
    }
    int64_t totnforests = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_lht_binary(filename, 4*MAX_STRING_LEN, filenr, run_params);
        int fd = open(filename, O_RDONLY);
        if(fd < 0) {
            fprintf(stderr, "Error: can't open file `%s'\n", filename);
            perror(NULL);
            ABORT(FILE_NOT_FOUND);
        }
        mypread(fd, &(totnforests_per_file[filenr]), sizeof(int), 0);
        totnforests += totnforests_per_file[filenr];
        
        close(fd);
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
    
    struct lhalotree_info *lht = &(forests_info->lht);
    forests_info->nforests_this_task = nforests_this_task;
    lht->nforests = nforests_this_task;
    lht->nhalos_per_forest = mymalloc(nforests_this_task * sizeof(lht->nhalos_per_forest[0]));
    lht->bytes_offset_for_forest = mymalloc(nforests_this_task * sizeof(lht->bytes_offset_for_forest[0]));
    lht->fd = mymalloc(nforests_this_task * sizeof(lht->fd[0]));
    
    int64_t *num_forests_to_process_per_file = calloc(lastfile + 1, sizeof(num_forests_to_process_per_file[0]));/* calloc is required */
    int64_t *start_forestnum_to_process_per_file = malloc((lastfile + 1) * sizeof(start_forestnum_to_process_per_file[0]));
    if(num_forests_to_process_per_file == NULL || start_forestnum_to_process_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests that need to be processed per file (on thistask=%d)\n", ThisTask);
        perror(NULL);
        return MALLOC_FAILURE;
    }

    /* show no forests need to be processed by default */
    for(int i=0;i<=lastfile;i++) {
        start_forestnum_to_process_per_file[i] = -1;
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
    lht->numfiles = end_filenum - start_filenum + 1;
    lht->open_fds = mymalloc(lht->numfiles * sizeof(lht->open_fds[0]));

    nforests_so_far = 0;
    int *forestnhalos = lht->nhalos_per_forest;
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
        get_forests_filename_lht_binary(filename, 4*MAX_STRING_LEN, filenr, run_params);
        int fd = open(filename, O_RDONLY);
        if(fd < 0) {
            fprintf(stderr, "Error: can't open file `%s'\n", filename);
            perror(NULL);
            ABORT(FILE_NOT_FOUND);
        }
        lht->open_fds[file_index] = fd;/* keep the file open, will be closed at the cleanup stage */
        
        const int64_t nforests = num_forests_to_process_per_file[filenr];
        const size_t nbytes = totnforests_per_file[filenr] * sizeof(int32_t);
        int32_t *nhalos_per_forest = malloc(nbytes);
        if(nhalos_per_forest == NULL) {
            fprintf(stderr,"Error: Could not allocate memory to read nhalos per forest. Bytes requested = %zu\n", nbytes);
            perror(NULL);
            ABORT(MALLOC_FAILURE);
        }
        mypread(fd, nhalos_per_forest, nbytes, 8); /* the last argument says to start after sizeof(totntrees) + sizeof(totnhalos) */
        memcpy(forestnhalos, &(nhalos_per_forest[start_forestnum_to_process_per_file[filenr]]), nforests * sizeof(forestnhalos[0]));

        /* first compute the byte offset to the halos in start_forestnum */
        size_t byte_offset_to_halos = sizeof(int32_t) + sizeof(int32_t) + nbytes;/* start at the beginning of halo #0 in tree #0 */
        for(int64_t i=0;i<start_forestnum_to_process_per_file[filenr];i++) {
            byte_offset_to_halos += nhalos_per_forest[i]*sizeof(struct halo_data);
        }
        free(nhalos_per_forest);
        
        nforests_so_far = forestnhalos - lht->nhalos_per_forest;
        for(int64_t i=0;i<nforests;i++) {
            lht->bytes_offset_for_forest[i + nforests_so_far] = byte_offset_to_halos;
            lht->fd[i + nforests_so_far] = fd;
            byte_offset_to_halos += forestnhalos[i]*sizeof(struct halo_data);
        }
        forestnhalos += nforests;
    }

    free(num_forests_to_process_per_file);
    free(start_forestnum_to_process_per_file);
    free(totnforests_per_file);

    return EXIT_SUCCESS;
}

int64_t load_forest_lht_binary(const int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info)
{
    const int64_t nhalos = (int64_t) forests_info->lht.nhalos_per_forest[forestnr];/* the array itself contains int32_t, since the LHT format*/
    struct halo_data *local_halos = mymalloc(sizeof(struct halo_data) * nhalos);
    if(local_halos == NULL) {
        fprintf(stderr, "Error: Could not allocate memory for %"PRId64" halos in forestnr = %"PRId64"\n",
                nhalos, forestnr);
        ABORT(MALLOC_FAILURE);
    }
    int fd = forests_info->lht.fd[forestnr];

    /* must have a valid file pointer  */
    if(fd <= 0 ) {
        fprintf(stderr,"Error: File pointer is NULL (i.e., you need to open the file before reading) \n");
        ABORT(INVALID_FILE_POINTER);
    }

    const off_t offset = forests_info->lht.bytes_offset_for_forest[forestnr];
    if(offset < 0) {
        fprintf(stderr,"Error: offset = %"PRId64" must be at least 0. (Can't interpret negative offsets)\n", offset);
        ABORT(FILE_READ_ERROR);/* negative offset would lead to file read error */
    }

    /* file descriptor can be pointing anywhere, does not get modified by this pread */
    mypread(fd, local_halos, sizeof(struct halo_data) * nhalos, offset);

    *halos = local_halos;

    return nhalos;
}


void cleanup_forests_io_lht_binary(struct forest_info *forests_info)
{
    struct lhalotree_info *lht = &(forests_info->lht);
    myfree(lht->nhalos_per_forest);
    myfree(lht->bytes_offset_for_forest);
    myfree(lht->fd);
    
    for(int32_t i=0;i<lht->numfiles;i++) {
        close(lht->open_fds[i]);
    }
    myfree(lht->open_fds);
}




