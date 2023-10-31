#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "read_tree_lhalo_binary.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"
#include "forest_utils.h"


static void get_forests_filename_lht_binary(char *filename, const size_t len, const int filenr, const struct params *run_params);
static int load_tree_table_lht_binary(const int firstfile, const int lastfile, const int64_t *totnforests_per_file,
                                      const struct params *run_params, const int ThisTask,
                                      int64_t *nhalos_per_forest);

void get_forests_filename_lht_binary(char *filename, const size_t len, const int filenr, const struct params *run_params)
{
    snprintf(filename, len - 1, "%s/%s.%d%s", run_params->SimulationDir, run_params->TreeName, filenr, run_params->TreeExtension);
}

/* Externally visible Functions */
int setup_forests_io_lht_binary(struct forest_info *forests_info,
                                const int ThisTask, const int NTasks, struct params *run_params)
{
    const int firstfile = run_params->FirstFile;
    const int lastfile = run_params->LastFile;

    const int numfiles = lastfile - firstfile + 1;
    if(numfiles <= 0) {
        return -1;
    }

    /* wasteful to allocate for lastfile + 1 indices, rather than numfiles; but makes indexing easier */
    int64_t *totnforests_per_file = calloc(lastfile + 1, sizeof(totnforests_per_file[0]));
    if(totnforests_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests in each file\n");
        perror(NULL);
        return MALLOC_FAILURE;
    }

    // First go through each file and determine the total number of forests across all files.
    int64_t totnforests = 0, totnhalos = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_lht_binary(filename, 4*MAX_STRING_LEN, filenr, run_params);
        int fd = open(filename, O_RDONLY);
        if(fd < 0) {
            fprintf(stderr, "Error: can't open file `%s'\n", filename);
            perror(NULL);
            return FILE_NOT_FOUND;
        }
        int tmp;
        mypread(fd, &tmp, sizeof(tmp), 0);
        totnforests_per_file[filenr] = tmp;
        totnforests += totnforests_per_file[filenr];

        mypread(fd, &tmp, sizeof(tmp), 4);
        totnhalos += tmp;
        close(fd);
    }
    forests_info->totnforests = totnforests;
    forests_info->totnhalos = totnhalos;

    const int need_nhalos_per_forest = run_params->ForestDistributionScheme == uniform_in_forests ? 0:1;
    int64_t *nhalos_per_forest = NULL;
    if(need_nhalos_per_forest) {
        nhalos_per_forest = mycalloc(totnforests, sizeof(*nhalos_per_forest));
        int status = load_tree_table_lht_binary(firstfile, lastfile, totnforests_per_file, run_params, ThisTask, nhalos_per_forest);
        if(status != EXIT_SUCCESS) {
            return status;
        }
    }

    int64_t nforests_this_task, start_forestnum;
    int status = distribute_weighted_forests_over_ntasks(totnforests, nhalos_per_forest,
                                                         run_params->ForestDistributionScheme, run_params->Exponent_Forest_Dist_Scheme,
                                                         NTasks, ThisTask, &nforests_this_task, &start_forestnum);
    if(status != EXIT_SUCCESS) {
        return status;
    }
    if(need_nhalos_per_forest) {
        myfree(nhalos_per_forest);
    }

    const int64_t end_forestnum = start_forestnum + nforests_this_task; /* not inclusive, i.e., do not process forestnr == end_forestnum */

    // Now that we know the number of trees being processed by each task, let's set up and malloc the structs.
    struct lhalotree_info *lht = &(forests_info->lht);
    forests_info->nforests_this_task = nforests_this_task;

    forests_info->FileNr = malloc(nforests_this_task * sizeof(*(forests_info->FileNr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->FileNr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->FileNr", nforests_this_task,
                                     sizeof(*(forests_info->FileNr)));

    forests_info->original_treenr = malloc(nforests_this_task * sizeof(*(forests_info->original_treenr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->original_treenr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->original_treenr", nforests_this_task,
                                     sizeof(*(forests_info->original_treenr)));

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


    lht->numfiles = end_filenum - start_filenum + 1;
    lht->open_fds = mymalloc(lht->numfiles * sizeof(lht->open_fds[0]));

    int64_t *forestnhalos = lht->nhalos_per_forest;
    int64_t nforests_so_far = 0;
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
        get_forests_filename_lht_binary(filename, 4*MAX_STRING_LEN, filenr, run_params);
        int fd = open(filename, O_RDONLY);
        XRETURN(fd > 0, FILE_NOT_FOUND,
                "Error: can't open file `%s'\n", filename);
        lht->open_fds[file_index] = fd;/* keep the file open, will be closed at the cleanup stage */

        const int64_t nforests_to_process_this_file = num_forests_to_process_per_file[filenr];
        const size_t nbytes = totnforests_per_file[filenr] * sizeof(int32_t);
        int32_t *buffer = malloc(nbytes);
        XRETURN(buffer != NULL, MALLOC_FAILURE,
                "Error: Could not allocate memory to read nhalos per forest. Bytes requested = %zu\n", nbytes);

        mypread(fd, buffer, nbytes, 8); /* the last argument says to start after sizeof(totntrees) + sizeof(totnhalos) */
        buffer += start_forestnum_to_process_per_file[filenr];
        for(int k=0;k<nforests_to_process_this_file;k++) {
            forestnhalos[k] = buffer[k];
        }
        buffer -= start_forestnum_to_process_per_file[filenr];

        /* first compute the byte offset to the halos in start_forestnum */
        size_t byte_offset_to_halos = sizeof(int32_t) + sizeof(int32_t) + nbytes;/* start at the beginning of halo #0 in tree #0 */
        for(int64_t i=0;i<start_forestnum_to_process_per_file[filenr];i++) {
            byte_offset_to_halos += buffer[i]*sizeof(struct halo_data);
        }
        free(buffer);

        nforests_so_far = forestnhalos - lht->nhalos_per_forest;
        if(filenr == start_filenum) {
            XRETURN(nforests_so_far == 0, EXIT_FAILURE,
                    "For the first iteration total forests already processed should be identically zero. Instead we got = %"PRId64"\n",
                    nforests_so_far);
        }

        for(int64_t i=0; i<nforests_to_process_this_file; i++) {
            lht->bytes_offset_for_forest[i + nforests_so_far] = byte_offset_to_halos;
            XRETURN(i + nforests_so_far < lht->nforests, EXIT_FAILURE,
                    "ThisTask = %d Assigning to index = %"PRId64" but only space of %"PRId64" forest fds\n", ThisTask, i + nforests_so_far, lht->nforests);
            lht->fd[i + nforests_so_far] = fd;
            byte_offset_to_halos += forestnhalos[i]*sizeof(struct halo_data);

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

        forestnhalos += nforests_to_process_this_file;
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

int64_t load_forest_lht_binary(const int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info)
{
    const int64_t nhalos = (int64_t) forests_info->lht.nhalos_per_forest[forestnr];/* the array itself contains int32_t, since the LHT format*/
    struct halo_data *local_halos = mymalloc(sizeof(struct halo_data) * nhalos);
    XRETURN(local_halos != NULL, -MALLOC_FAILURE,
            "Error: Could not allocate memory for %"PRId64" halos in forestnr = %"PRId64"\n",
            nhalos, forestnr);

    if(forestnr >= forests_info->lht.nforests) {
        fprintf(stderr,"Error: Attempting to access forest = %"PRId64" but memory is allocated for only %"PRId64"\n"
                "Perhaps, the starting forest offset was not accounted for?\n", forestnr, forests_info->lht.nforests);
        return -INVALID_MEMORY_ACCESS_REQUESTED;
    }

    int fd = forests_info->lht.fd[forestnr];

    /* must have a valid file pointer  */
    if(fd <= 0 ) {
        fprintf(stderr,"Error: File pointer is NULL (i.e., you need to open the file before reading) \n");
        return -INVALID_FILE_POINTER;
    }

    const off_t offset = forests_info->lht.bytes_offset_for_forest[forestnr];
    if(offset < 0) {
        fprintf(stderr,"Error: offset = %"PRId64" must be at least 0. (Can't interpret negative offsets)\n", offset);
        return -FILE_READ_ERROR;/* negative offset would lead to file read error */
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

int load_tree_table_lht_binary(const int firstfile, const int lastfile, const int64_t *totnforests_per_file,
                                 const struct params *run_params, const int ThisTask,
                                 int64_t *nhalos_per_forest)
{
    /* The nhalos_per_forest array is written as 32-bit integers;
        however, we really use 64-bit integers. So, we have to split up the work and read
        in the 32-bit integers into a temporary buffer and then assign
        to the parameter *nhalos_per_forest.  */
    int64_t max_nforests_per_file = 0;
    int32_t *buffer = NULL;
    for(int32_t ifile=firstfile;ifile<=lastfile;ifile++) {
        int64_t nforests_this_file = totnforests_per_file[ifile];
        if (nforests_this_file > max_nforests_per_file) max_nforests_per_file = nforests_this_file;
    }
    buffer = malloc(max_nforests_per_file * sizeof(*buffer));
    CHECK_POINTER_AND_RETURN_ON_NULL(buffer,
                                    "Failed to allocate %"PRId64" buffer to hold (32-bit integers, size = %zu) nhalos_per_forest\n",
                                    max_nforests_per_file,
                                    sizeof(*buffer));

    for(int32_t ifile=firstfile;ifile<=lastfile;ifile++) {
        int64_t nforests_this_file = totnforests_per_file[ifile];
        if(nforests_this_file == 0) {
            if(ThisTask == 0 && ifile==firstfile) {
                fprintf(stderr, "WARNING: The first file = %d does not contain any halos from a *new* tree (i.e., "
                                "the first file *only* contains halos belonging to a tree that starts in a previous file\n", ifile);
            }
            continue;
        }
        if(nforests_this_file > max_nforests_per_file) {
            fprintf(stderr,"Error: The number of forests in this file = %"PRId64" exceeds the max. number of expected forests = %"PRId64"\n",
                           nforests_this_file, max_nforests_per_file);
            return -1;
        }

        char filename[4*MAX_STRING_LEN];
        get_forests_filename_lht_binary(filename, 4*MAX_STRING_LEN, ifile, run_params);
        int fd = open(filename, O_RDONLY);
        XRETURN( fd > 0, FILE_NOT_FOUND,"Error: can't open file `%s'\n", filename);

        //the last argument is the offset for nhalos_per_forest -> the first 4 bytes
        //are for totnforests, and the next 4 bytes are for totnhalos, after that
        //the nhalos_per_forest[totnforests] starts.
        mypread(fd, buffer, sizeof(*buffer)*nforests_this_file, 8);
        for(int64_t i=0;i<nforests_this_file;i++) {
            nhalos_per_forest[i] = (int64_t) buffer[i];
        }
        nhalos_per_forest += nforests_this_file;
        XRETURN ( close(fd) >= 0, -1, "Error: Could not properly close the binary file for filename = '%s'\n", filename);
    }
    free(buffer);
    return EXIT_SUCCESS;
}