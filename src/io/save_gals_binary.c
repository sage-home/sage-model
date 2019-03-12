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

void initialize_binary_galaxy_files(const int filenr, const int ntrees, struct save_info *save_info, const struct params *run_params)
{

    // We open up files for each output. We'll store the file IDs of each of these file. 
    save_info->save_fd = malloc(sizeof(int32_t) * run_params->NOUT);

    char buffer[4*MAX_STRING_LEN + 1];
    /* Open all the output files */
    for(int n = 0; n < run_params->NOUT; n++) {
        snprintf(buffer, 4*MAX_STRING_LEN, "%s/%s_z%1.3f_%d", run_params->OutputDir, run_params->FileNameGalaxies,
                 run_params->ZZ[run_params->ListOutputSnaps[n]], filenr);

        /* the last argument sets permissions as "rw-r--r--" (read/write owner, read group, read other)*/
        save_info->save_fd[n] = open(buffer,  O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
        if (save_info->save_fd[n] < 0) {
            fprintf(stderr, "Error: Can't open file `%s'\n", buffer);
            ABORT(FILE_NOT_FOUND);
        }
        
        // write out placeholders for the header data.
        const off_t off = (ntrees + 2) * sizeof(int32_t);
        const off_t status = lseek(save_info->save_fd[n], off, SEEK_SET);
        if(status < 0) {
            fprintf(stderr, "Error: Failed to write out %d elements for header information for file %d. "
                    "Attempted to write %"PRId64" bytes\n", ntrees + 2, n, off);
            perror(NULL);
            ABORT(FILE_WRITE_ERROR);
        }
    }
}
