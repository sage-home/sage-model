#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <hdf5.h>

#include "read_tree_gadget4_hdf5.h"
#include "hdf5_read_utils.h"
#include "../core_mymalloc.h"
#include "forest_utils.h"


/* Local Proto-Types */
static void get_forests_filename_gadget4_hdf5(char *filename, const size_t len, const int filenr, const struct params *run_params);

int load_tree_table_gadget4_hdf5(const int firstfile, const int lastfile, const int64_t *totnforests_per_file, 
                                 const struct params *run_params, const int ThisTask, 
                                 int64_t *nhalos_per_forest);

void get_forests_filename_gadget4_hdf5(char *filename, const size_t len, const int filenr,  const struct params *run_params)
{
    snprintf(filename, len - 1, "%s/%s.%d%s", run_params->SimulationDir, run_params->TreeName, filenr, run_params->TreeExtension);
}


int setup_forests_io_gadget4_hdf5(struct forest_info *forests_info,
                                  const int ThisTask, const int NTasks, struct params *run_params)
{
    const int firstfile = run_params->FirstFile;
    const int lastfile = run_params->LastFile;
    const int numfiles = lastfile - firstfile + 1;
    if(numfiles <= 0) {
        return -1;
    }

    /* We can not determine the halo offset to start reading from within 'firstfile' unless we have access to *all* previous files */
    if(firstfile != 0) {
        fprintf(stderr, "Error: Since the Gadget4 mergertrees can be split across files, we *have* to begin processing at the 0'th file\n");
        fprintf(stderr,"If you are confident that the first tree located within 'firstfile' = %d begins at 0 halo offset (i.e., there "
                       "are no previous trees whose halos are contained within 'firstfile', then you can comment out this error at your own risk)\n",
                       firstfile);
        return -1;
    }

    /* wasteful to allocate for lastfile + 1 indices, rather than numfiles; but makes indexing easier */
    int64_t *totnforests_per_file = calloc(lastfile + 1, sizeof(totnforests_per_file[0]));
    if(totnforests_per_file == NULL) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests in each file\n");
        perror(NULL);
        return MALLOC_FAILURE;
    }
    struct gadget4_info *g4 = &(forests_info->gadget4);

    struct HDF5_METADATA_NAMES metadata_names;
    int status = fill_hdf5_metadata_names(&metadata_names, run_params->TreeType);
    if (status != EXIT_SUCCESS) {
        return -1;
    }

    int64_t totnforests = 0, sanity_check_totnforests;
    /* Extra step for Gadget4 hdf5 format since the forests can span multiple files 
       -> need to calculate (for each forest) how many files the forest is spread across, 
          and the start and end halo number within each file. Most forests will likely be within
          one file, but this design helps simplify the code in the `load_halos` function MS 13th June, 2023 */

    int64_t *nhalos_per_file = malloc( (lastfile + 1)*sizeof(*nhalos_per_file) );
    CHECK_POINTER_AND_RETURN_ON_NULL(nhalos_per_file, "Failed to allocate %d elements of size %zu for nhalos per file\n", 
                                     (lastfile + 1),
                                     sizeof(*nhalos_per_file));

    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_gadget4_hdf5(filename, 4*MAX_STRING_LEN, filenr, run_params);
        hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        XRETURN (fd > 0, FILE_NOT_FOUND,
                 "Error: can't open file `%s'\n", filename);

#define READ_G4_ATTRIBUTE(hid, groupname, attrname, dst) {           \
            herr_t h5_status = read_attribute(hid, groupname, attrname, (void *) &dst, sizeof(dst)); \
            if(h5_status < 0) {                                         \
                return (int) h5_status;                                 \
            }                                                           \
        }

        if(filenr == firstfile) {
            //Check if the number of simulation output files have been set correctly
            int32_t numsimulationfiles;
            READ_G4_ATTRIBUTE(fd, "/Header", metadata_names.name_NumSimulationTreeFiles, numsimulationfiles);

            if(numsimulationfiles != run_params->NumSimulationTreeFiles) {
                fprintf(stderr,"Error: Parameter file mentions total number of simulation output files = %d but the "
                        "hdf5 field `%s' says %d tree files\n",
                        run_params->NumSimulationTreeFiles, metadata_names.name_NumSimulationTreeFiles, numsimulationfiles);
                fprintf(stderr,"May be the value in the parameter file needs to be updated?\n");
                return -1;
            }

            // Read totnforests over across all files to sanity check the value at the end
            READ_G4_ATTRIBUTE(fd, "/Header", "Ntrees_Total", sanity_check_totnforests);
            if(sanity_check_totnforests <= 0) {
                fprintf(stderr,"Error: Total number of trees = %"PRId64" should be >=1\n", sanity_check_totnforests);
                return -1;
            }

            READ_G4_ATTRIBUTE(fd, "/Header", "Nhalos_Total", forests_info->totnhalos);
            if(forests_info->totnhalos <= 0) {
                fprintf(stderr,"Error: Total number of halos = %"PRId64" should be >=1\n", forests_info->totnhalos);
                return -1;
            }

            // Read and check boxsize, Omega0, OmegaLambda, Hubble
            #define SANITY_CHECK_FLOAT64_ATTRIBUTE(hid, groupname, attrname, dst, expected_value, tolerance) { \
                READ_G4_ATTRIBUTE(hid, groupname, attrname, dst);                                              \
                XRETURN(fabs(dst - expected_value) < tolerance, -1,                                            \
                        "Error: Expected value for '" #attrname "' = %g "                                      \
                        "but found %g instead in hdf5 file\n", expected_value, dst);                           \
            }
            double tmp;
            const double tolerance = 1e-6;
            SANITY_CHECK_FLOAT64_ATTRIBUTE(fd, "/Parameters", "BoxSize", tmp, run_params->BoxSize, tolerance);
            SANITY_CHECK_FLOAT64_ATTRIBUTE(fd, "/Parameters", "Omega0", tmp, run_params->Omega, tolerance);
            SANITY_CHECK_FLOAT64_ATTRIBUTE(fd, "/Parameters", "OmegaLambda", tmp, run_params->OmegaLambda, tolerance);
            SANITY_CHECK_FLOAT64_ATTRIBUTE(fd, "/Parameters", "HubbleParam", tmp, run_params->Hubble_h, tolerance);
            #undef SANITY_CHECK_FLOAT64_ATTRIBUTE

        }

        uint64_t nforests_thisfile;
        READ_G4_ATTRIBUTE(fd, "/Header", metadata_names.name_NTrees, nforests_thisfile);
#ifdef DEBUG_PRINT        
        fprintf(stderr,"[On ThisTask = %d] filenr = %d nforests_thisfile attribute = %"PRIu64"\n", ThisTask, filenr, nforests_thisfile);
#endif
        totnforests_per_file[filenr] = nforests_thisfile;
        totnforests += nforests_thisfile;

        int64_t nhalos_thisfile;
        READ_G4_ATTRIBUTE(fd, "/Header", metadata_names.name_totNHalos, nhalos_thisfile);
        nhalos_per_file[filenr] = nhalos_thisfile;

        XRETURN( H5Fclose(fd) >= 0, -1, "Error: Could not close hdf5 file `%s`\n", filename);
    }
    if(run_params->NumSimulationTreeFiles == (lastfile - firstfile + 1)) {
        XRETURN( sanity_check_totnforests == totnforests, -1, "Error: Total number of trees = %" PRId64" "
                "read in from firstfile = %d should match the number of forests summed across all files = %"PRId64"\n",
                sanity_check_totnforests, firstfile, totnforests);
    }
    forests_info->totnforests = totnforests;/* forests_info->totnhalos has already been set */

    // const int need_nhalos_per_forest = run_params->ForestDistributionScheme == uniform_in_forests ? 0:1;
    int64_t *nhalos_per_forest = malloc(totnforests * sizeof(*nhalos_per_forest));  
    CHECK_POINTER_AND_RETURN_ON_NULL(nhalos_per_forest,
                                    "Failed to allocate %"PRId64" elements of size %zu for nhalos_per_forest", totnforests,
                                    sizeof(*(nhalos_per_forest)));

    status = load_tree_table_gadget4_hdf5(firstfile, lastfile, totnforests_per_file, run_params, ThisTask, nhalos_per_forest);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    int64_t nforests_this_task, start_forestnum;
    status = distribute_weighted_forests_over_ntasks(totnforests, nhalos_per_forest,
                                                    run_params->ForestDistributionScheme, run_params->Exponent_Forest_Dist_Scheme,
                                                    NTasks, ThisTask, &nforests_this_task, &start_forestnum);
    if(status != EXIT_SUCCESS) {
        return status;
    }

    const int64_t end_forestnum = start_forestnum + nforests_this_task - 1; /* inclusive, i.e., DO process forestnr == end_forestnum */
#ifdef DEBUG_PRINT    
    fprintf(stderr,"Thistask = %d start_forestnum = %"PRId64" end_forestnum = %"PRId64"\n", ThisTask, start_forestnum, end_forestnum);
#endif
    forests_info->nforests_this_task = nforests_this_task;
    g4->nforests = nforests_this_task;
    int64_t nhalos_this_task = 0;
    for(int64_t i=start_forestnum;i<=end_forestnum;i++) {
        nhalos_this_task += nhalos_per_forest[i];
    }
    forests_info->nhalos_this_task = nhalos_this_task;

    int64_t end_halonum_for_end_forestnum = 0;
    for(int64_t i=0;i<=end_forestnum;i++) {
        end_halonum_for_end_forestnum += nhalos_per_forest[i];
    }
    end_halonum_for_end_forestnum--;/* inclusive - that's why we need the -1. */


    int64_t *num_forests_to_process_per_file = calloc((lastfile + 1), sizeof(num_forests_to_process_per_file[0]));/* calloc is required */
    int64_t *start_forestnum_to_process_per_file = malloc((lastfile + 1) * sizeof(start_forestnum_to_process_per_file[0]));
    if(num_forests_to_process_per_file == NULL || start_forestnum_to_process_per_file == NULL) {
    // if(num_forests_to_process_per_file == NULL ) {
        fprintf(stderr,"Error: Could not allocate memory to store the number of forests that need to be processed per file (on thistask=%d)\n", ThisTask);
        perror(NULL);
        return MALLOC_FAILURE;
    }

    int *end_filenum_for_last_forest_in_file = malloc((lastfile + 1) * sizeof(*end_filenum_for_last_forest_in_file));
    CHECK_POINTER_AND_RETURN_ON_NULL(end_filenum_for_last_forest_in_file, "Error: Could not allocate memory to store the end filenum for the last forest in file\n");

    int64_t *end_forestnum_for_last_forest_in_file = malloc((lastfile + 1) * sizeof(end_forestnum_for_last_forest_in_file));
    CHECK_POINTER_AND_RETURN_ON_NULL(end_forestnum_for_last_forest_in_file, "Error: Could not allocate memory to store the end filenum for the last forest in file\n");

    int64_t *end_halonum_in_file = malloc((lastfile + 1) * sizeof(*end_halonum_in_file));   
    CHECK_POINTER_AND_RETURN_ON_NULL(end_halonum_in_file, "Error: Could not allocate memory to store the cumulative number of halos encountered  in files\n");

    /* contains the file-local offset in units of nhalos (not bytes) for the beginning of the *first* 'new' forest 
        within the file. Typically, would be 0 when the first forest begins as the start of the file; however, since
        a previous tree may have been split into this file, the offset needs to be adjusted accordingly (i.e., if the 
        previous tree has, say 126 halos within this file, then the offset for the first new tree will be '126' (0-based indexing for C))
        - MS 6th Jul, 2023
    */

    int start_filenum=-1, end_filenum=-1;
    int64_t nhalos_so_far = 0, nforests_so_far = 0;
    for(int filenr=firstfile;filenr<=lastfile;filenr++) {
        end_halonum_in_file[filenr] = nhalos_so_far + nhalos_per_file[filenr] - 1;/* storing the last halo number (cumulative across all previous files)*/
        end_forestnum_for_last_forest_in_file[filenr] = nforests_so_far + totnforests_per_file[filenr];/* storing the last forest number */
        if(totnforests_per_file[filenr] > 0) end_forestnum_for_last_forest_in_file[filenr]--; /* Only do the decrement if at least one new forest was found
                                                                                                in this file. Otherwise, preserve the forestnumber from the 
                                                                                                last file where a *new* forest was present */

        start_forestnum_to_process_per_file[filenr] = 0;
        num_forests_to_process_per_file[filenr] = totnforests_per_file[filenr];

        /* We can use forest number to get 'start_filenum'; however, since the 'end_forestnum' tree might be split across many files,
        we have to use 'halonum' to detect the final file that needs to be opened for completing all the halos in 'end_halonum' */
        if(start_forestnum >= nforests_so_far && start_forestnum <= end_forestnum_for_last_forest_in_file[filenr]) {
            start_filenum = filenr;
            start_forestnum_to_process_per_file[filenr] = start_forestnum - nforests_so_far;
            num_forests_to_process_per_file[filenr] = totnforests_per_file[filenr] - (start_forestnum - nforests_so_far);
        }

        if(end_halonum_for_end_forestnum >= nhalos_so_far && end_halonum_for_end_forestnum <= end_halonum_in_file[filenr]) {
            end_filenum = filenr;
            num_forests_to_process_per_file[filenr] = end_forestnum - (start_forestnum_to_process_per_file[filenr] + nforests_so_far);
        }

        nhalos_so_far += nhalos_per_file[filenr];
        nforests_so_far += totnforests_per_file[filenr];
#ifdef DEBUG_PRINT                
        if(ThisTask == 0) {
            fprintf(stderr,"end_forestnum_for_last_forest_in_file[%03d] = %08"PRId64" nhalos_so_far = %10"PRId64" nhalos_per_forest = %10"PRId64" end_halonum_in_file = %10"PRId64"\n", 
                            filenr, end_forestnum_for_last_forest_in_file[filenr], nhalos_so_far, nhalos_per_forest[nforests_so_far-1], end_halonum_in_file[filenr]);

        }
#endif        
    }
    XRETURN(start_filenum != -1 && end_filenum != -1, -1, 
            "Error: Could not locate start_filenum ()= %d) and/or end_filenum (=%d)\n", start_filenum, end_filenum);

    g4->numfiles = end_filenum - start_filenum + 1;
    g4->open_h5_fds = mymalloc(g4->numfiles * sizeof(g4->open_h5_fds[0]));
    XRETURN(g4->open_h5_fds != NULL, MALLOC_FAILURE, "Error: Could not allocate memory of size %zu bytes"
                                                     " to hold open HDF5 file pointers for %d files\n", 
                                                     g4->numfiles * sizeof(g4->open_h5_fds[0]), g4->numfiles);

    /* Now we can open the needed files for later reading in 'load_forest_gadget4_hdf5'*/
    for(int filenr=start_filenum;filenr<=end_filenum;filenr++) {
        char filename[4*MAX_STRING_LEN];
        get_forests_filename_gadget4_hdf5(filename, 4*MAX_STRING_LEN, filenr, run_params);
        hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        XRETURN (fd > 0, FILE_NOT_FOUND,
                 "Error: can't open file `%s'\n", filename);
        g4->open_h5_fds[filenr - start_filenum] = fd;
    }

    int64_t *nhalo_offset_first_forest_in_file = calloc( (lastfile + 1), sizeof(*nhalo_offset_first_forest_in_file) );
    CHECK_POINTER_AND_RETURN_ON_NULL(nhalo_offset_first_forest_in_file, "Failed to allocate %d elements of size %zu for nhalo_offset_first_forest_in_file\n", 
                                     (lastfile + 1),
                                     sizeof(*nhalo_offset_first_forest_in_file));

    g4->num_files_per_forest = mymalloc(nforests_this_task * sizeof(g4->num_files_per_forest[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(g4->num_files_per_forest,
                                     "Failed to allocate %"PRId64" elements of size %zu for gadget4->num_files_per_forest", nforests_this_task,
                                     sizeof(*(g4->num_files_per_forest)));

    forests_info->FileNr = mymalloc(nforests_this_task * sizeof(*(forests_info->FileNr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->FileNr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->FileNr", nforests_this_task,
                                     sizeof(*(forests_info->FileNr)));

    forests_info->original_treenr = mymalloc(nforests_this_task * sizeof(*(forests_info->original_treenr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->original_treenr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->original_treenr", nforests_this_task,
                                     sizeof(*(forests_info->original_treenr)));

    int filenr = firstfile;
    XRETURN(firstfile == 0, -1, "Error: 'firstfile' = %d must be 0 for Gadget4 mergertree type", firstfile);
    nhalo_offset_first_forest_in_file[filenr] = 0;/* This assumes firstfile == 0*/
    nhalos_so_far = 0;

    int64_t nhalos_left_this_file = nhalos_per_file[filenr];
    int64_t file_nhalo_offset = 0;
    int64_t file_nhalo_offset_for_start_forestnum = 0;
    int64_t file_local_treenum = 0;
    for(int64_t i=0;i<totnforests;i++) {
        int numfiles_this_forest = 1;
        int start_filenr = filenr;
        if(i == start_forestnum) {
            file_nhalo_offset_for_start_forestnum = file_nhalo_offset;
#ifdef DEBUG_PRINT                           
            fprintf(stderr,"CALCULATING NHALO OFFSET FOR START_FORESTNUM (ThisTask = %d) start_filenr = %d file_nhalo_offset_for_start_forestnum = %"PRId64" numfiles_per_forest[%"PRId64"] = %d\n", 
                            ThisTask, start_filenr, file_nhalo_offset_for_start_forestnum, i, numfiles_this_forest);
#endif                            
        }

        if(i == end_forestnum_for_last_forest_in_file[filenr]) {
            int64_t nhalos_left_this_forest = nhalos_per_forest[i];
            while(nhalos_left_this_forest > nhalos_left_this_file) {
#ifdef DEBUG_PRINT                
                fprintf(stderr,"(BEFORE) Entering initial while loop: i = %"PRId64" filenr = %d numfiles = %d nhalos_left_this_forest = %"PRId64" nhalos_left_this_file = %"PRId64"\n", 
                        i, filenr, numfiles_this_forest, nhalos_left_this_forest, nhalos_left_this_file);
#endif

                nhalos_left_this_forest -= nhalos_left_this_file;
                numfiles_this_forest++;
                filenr++;

            
                XRETURN( filenr <= lastfile, -1, 
                        "(ThisTask = %d) filenr = %d should have been <= lastfile = %d iforest = %"PRId64" nforests_this_task = %"PRId64" nhalos = %"PRId64"\n", 
                        ThisTask, filenr, lastfile, i, nforests_this_task, nhalos_per_forest[i]);
                nhalos_left_this_file = nhalos_per_file[filenr];
                file_nhalo_offset = 0;
                file_local_treenum = 0;
#ifdef DEBUG_PRINT                
                fprintf(stderr,"(AFTER) Entering initial while loop: i = %"PRId64" filenr = %d numfiles = %d nhalos_left_this_forest = %"PRId64" nhalos_left_this_file = %"PRId64"\n", 
                        i, filenr, numfiles_this_forest, nhalos_left_this_forest, nhalos_left_this_file);
#endif                        
            }

            XRETURN(filenr <= lastfile && nhalos_left_this_forest <= nhalos_left_this_file, -1, "Error: WHYYY");
            if(nhalos_left_this_file == nhalos_left_this_forest) {
                end_filenum_for_last_forest_in_file[start_filenr] = filenr;                   
                nhalos_left_this_file = 0;
                filenr++;
                if(filenr <= lastfile) nhalos_left_this_file = nhalos_per_file[filenr]; /* filenr will be == (lastfile + 1)  after the increment when processing the final forest */
                file_nhalo_offset = 0;
                file_local_treenum = 0;
            } else {
                end_filenum_for_last_forest_in_file[start_filenr] = filenr;                   
                nhalos_left_this_file -= nhalos_left_this_forest;
                file_nhalo_offset += nhalos_left_this_forest;
                nhalo_offset_first_forest_in_file[filenr] = file_nhalo_offset;
                file_local_treenum++;
            }
        } else {
            nhalos_left_this_file -= nhalos_per_forest[i];
            file_nhalo_offset += nhalos_per_forest[i];
            file_local_treenum++;
        }

        if(i >= start_forestnum && i <= end_forestnum) {
            g4->num_files_per_forest[i - start_forestnum]  = numfiles_this_forest;
            forests_info->FileNr[i - start_forestnum] = start_filenr;
            forests_info->original_treenr[i - start_forestnum] = file_local_treenum;
            // fprintf(stderr,"(ThisTask = %d) i = %"PRId64" start_filenr = %d numfiles = %d\n", ThisTask, i, start_filenr, numfiles_this_forest);
        }

        nhalos_so_far += nhalos_per_forest[i];
    }
    free(end_filenum_for_last_forest_in_file);
    free(end_halonum_in_file);
    free(end_forestnum_for_last_forest_in_file);



    g4->nhalos_per_forest = mymalloc(nforests_this_task * sizeof(g4->nhalos_per_forest[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(g4->nhalos_per_forest,
                                     "Failed to allocate %"PRId64" elements of size %zu for gadget4->nhalos_per_forest", nforests_this_task,
                                     sizeof(*(g4->nhalos_per_forest)));

    XRETURN(sizeof(*g4->nhalos_per_forest) == sizeof(*nhalos_per_forest), -1, 
            "Error: Mis-match in element size of the nhalos per forest array. Each element in source requires = %zd bytes"
            ", while each element in the destination requires %zd bytes. Perhaps change the defintion of `nhalos_per_forest` "
            "structure member within the 'struct gadget4_info' defined in 'core_allvars.h'\n", 
            sizeof(*g4->nhalos_per_forest), sizeof(*nhalos_per_forest));
    memcpy(g4->nhalos_per_forest, &nhalos_per_forest[start_forestnum], nforests_this_task * sizeof(*(g4->nhalos_per_forest)));
    free(nhalos_per_forest);
    nhalos_per_forest = g4->nhalos_per_forest;


    g4->nhalos_per_file_per_forest = mymalloc(nforests_this_task * sizeof(g4->nhalos_per_file_per_forest[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(g4->nhalos_per_file_per_forest,
                                     "Failed to allocate %"PRId64" elements of size %zu for gadget4->nhalos_per_file_per_forest", nforests_this_task,
                                     sizeof(*(g4->nhalos_per_file_per_forest)));

    g4->offset_in_nhalos_first_file_for_forests = mymalloc(nforests_this_task * sizeof(g4->offset_in_nhalos_first_file_for_forests[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(g4->offset_in_nhalos_first_file_for_forests,
                                     "Failed to allocate %"PRId64" elements of size %zu for g4->offset_in_nhalos_first_file_for_forests", nforests_this_task,
                                     sizeof(*(g4->offset_in_nhalos_first_file_for_forests)));

    g4->start_h5_fd_index = mymalloc(nforests_this_task * sizeof(g4->start_h5_fd_index[0]));
    CHECK_POINTER_AND_RETURN_ON_NULL(g4->start_h5_fd_index,
                                     "Failed to allocate %"PRId64" elements of size %zu for g4->start_h5_fd_index", nforests_this_task,
                                     sizeof(*(g4->start_h5_fd_index)));

    /* Extra step for Gadget4 hdf5 format since the forests can span multiple files 
       -> need to calculate (for each forest) how many files the forest is spread across, 
          and the start and end halo number within each file. Most forests will likely be within
          one file, but this design helps simplify the code in the `load_halos` function MS 13th June, 2023 */

    /* In case 'start_filenum' != 0, we have to skip to where the halos begin in the
    first forest in the file (i.e., after where the previous forest that started at an earlier 
    file finishes). MS: 29th June, 2023 */   
    filenr = start_filenum;                                    
    file_nhalo_offset = file_nhalo_offset_for_start_forestnum;
    nhalos_left_this_file = nhalos_per_file[start_filenum] - file_nhalo_offset_for_start_forestnum;

    for(int64_t iforest=0;iforest<nforests_this_task;iforest++) {
        g4->nhalos_per_file_per_forest[iforest] = calloc( g4->num_files_per_forest[iforest], sizeof(*(g4->nhalos_per_file_per_forest[iforest])));
        CHECK_POINTER_AND_RETURN_ON_NULL(g4->nhalos_per_file_per_forest[iforest],
                                        "Failed to allocate %d elements of size %zu for gadget4->nhalos_per_file_per_forest[%"PRId64"]", 
                                        g4->num_files_per_forest[iforest],
                                        sizeof(*(g4->nhalos_per_file_per_forest[iforest])), 
                                        iforest);

        g4->start_h5_fd_index[iforest] = filenr - start_filenum;
        g4->offset_in_nhalos_first_file_for_forests[iforest] = file_nhalo_offset;

        int64_t nhalos_left_this_forest = g4->nhalos_per_forest[iforest];
        int64_t nhalos_assigned = 0;
        int forest_numfiles_index = 0;
        while(nhalos_left_this_forest > nhalos_left_this_file) {
            XRETURN(forest_numfiles_index < g4->num_files_per_forest[iforest], -1, 
                    "Error (on ThisTask=%d): forest_numfiles_index = %d should be less than g4->num_files_per_forest[%"PRId64"] = %d\n",
                    ThisTask, forest_numfiles_index, iforest, g4->num_files_per_forest[iforest]);
            g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index] = nhalos_left_this_file;
            nhalos_assigned += nhalos_left_this_file;
#ifdef DEBUG_PRINT                
            fprintf(stderr,"(BEFORE) filenr = %d iforest = %"PRId64" nhalos_left_this_forest "
                           " = %"PRId64" nhalos_left_this_file = %"PRId64" nhalos_per_file_per_forest = %d nhalos_assigned = %"PRId64" forest_numfiles_index = %d\n",
                           filenr, iforest, nhalos_left_this_forest, nhalos_left_this_file, 
                           g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index], nhalos_assigned, forest_numfiles_index);
#endif                           

            nhalos_left_this_forest -= nhalos_left_this_file;
            forest_numfiles_index++;
            filenr++;
            
            XRETURN( filenr <= end_filenum, -1, 
                    "(ThisTask = %d) filenr = %d should have been <= end_filenum = %d iforest = %"PRId64" nforests_this_task = %"PRId64" nhalos = %"PRId64
                    " nhalos_left_this_forest = %"PRId64" \n", 
                    ThisTask, filenr, end_filenum, iforest, nforests_this_task, nhalos_per_forest[iforest], nhalos_left_this_forest);
            file_nhalo_offset = 0;
            if(filenr <= end_filenum) {
                nhalos_left_this_file = nhalos_per_file[filenr];
#ifdef DEBUG_PRINT                
                fprintf(stderr,"(AFTER) filenr = %d iforest = %"PRId64" nhalos_left_this_forest "
                            " = %"PRId64" nhalos_left_this_file = %"PRId64" nhalos_per_file_per_forest = %d nhalos_assigned = %"PRId64" forest_numfiles_index = %d\n",
                            filenr, iforest, nhalos_left_this_forest, nhalos_left_this_file, 
                            g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index-1], nhalos_assigned, forest_numfiles_index);
#endif                            
            }

        }


        /* Two things need to be cross-checked - 
            i) whether the realloc accounts for the final file that contains the last of the halos for a forest that spans mulltiple files
            ii) whether the indexing with filenr is consistent with the memory allocation but also how the filenr is used in the other io loading
            functions (i.e., index with potentially [filenr - start_filenum - 1] rather than [filenr - 1]
            MS: 15th June, 2023
        */
        XRETURN(forest_numfiles_index < g4->num_files_per_forest[iforest], -1, 
                "Error (on ThisTask=%d): forest_numfiles_index = %d should be less than g4->num_files_per_forest[%"PRId64"] = %d\n",
                ThisTask, forest_numfiles_index, iforest, g4->num_files_per_forest[iforest]);

        nhalos_assigned += nhalos_left_this_forest;

        if (nhalos_left_this_forest < nhalos_left_this_file){
            XRETURN(forest_numfiles_index < g4->num_files_per_forest[iforest], -1, 
                    "Error (on ThisTask=%d): forest_numfiles_index = %d should be less than g4->num_files_per_forest[%"PRId64"] = %d\n",
                    ThisTask, forest_numfiles_index, iforest, g4->num_files_per_forest[iforest]);
            XRETURN(g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index] == 0, -1, "Error: Code mnight be over-writing existing data");

            g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index] = nhalos_left_this_forest;
            nhalos_left_this_file -= nhalos_left_this_forest;

            file_nhalo_offset  += nhalos_left_this_forest;
        } else if(nhalos_left_this_forest == nhalos_left_this_file) {
            g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index] = nhalos_left_this_forest;
#ifdef DEBUG_PRINT            
            fprintf(stderr,"EXACTLY EQUAL iforest = %"PRId64" filenr = %d forest_numfiles_index = %d g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index] = %d\n\n",
                           iforest, filenr, forest_numfiles_index, g4->nhalos_per_file_per_forest[iforest][forest_numfiles_index]);
#endif                           
            filenr++;
            file_nhalo_offset = 0;
            if(filenr <= end_filenum) nhalos_left_this_file = nhalos_per_file[filenr];
        } 
        else {
            fprintf(stderr,"Error: Code logic should not have reached here\n");
            return -1;
        }
        XRETURN( nhalos_assigned == g4->nhalos_per_forest[iforest], -1, 
                 "Error: nhalos_assigned = %"PRId64"  should be *exactly* equal to nhalos_per_forest[%"PRId64"] = %"PRId64"\n", 
                 nhalos_assigned, iforest, g4->nhalos_per_forest[iforest]);
    }
    free(nhalos_per_file);
    free(nhalo_offset_first_forest_in_file);

    for(int64_t i=0;i<nforests_this_task;i++) {
        const int64_t expected_nhalos = g4->nhalos_per_forest[i];
        int64_t check_nhalos = 0;
        for(int j=0;j<g4->num_files_per_forest[i];j++) {
            check_nhalos += g4->nhalos_per_file_per_forest[i][j];
        }
        XRETURN(check_nhalos == expected_nhalos, -1, 
                "Error: For forestnum = %10"PRId64" expected to find %10"PRId64" halos but instead found %10"PRId64" halos (numfiles = %d)\n",
                i + start_forestnum, expected_nhalos, check_nhalos, g4->num_files_per_forest[i]);
    }


    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
    forests_info->frac_volume_processed = 0.0;
    for(int32_t ifile = start_filenum; ifile <= end_filenum; ifile++) {
        if(totnforests_per_file[ifile] > 0) {
            forests_info->frac_volume_processed += (double) num_forests_to_process_per_file[ifile] / (double) totnforests_per_file[ifile];
        }
    }
    forests_info->frac_volume_processed /= (double) run_params->NumSimulationTreeFiles;

    free(num_forests_to_process_per_file);
    free(start_forestnum_to_process_per_file);
    free(totnforests_per_file);

    /* Finally setup the multiplication factors necessary to generate
       unique galaxy indices (across all files, all trees and all tasks) for this run*/
    run_params->FileNr_Mulfac = 1000000000000000LL;
    run_params->ForestNr_Mulfac = 1000000000LL;

    fprintf(stderr,"[On ThisTask = %d] start_forestnum = %"PRId64" end_forestnum = %"PRId64" start_filenum = %d end_filenum = %d "
                    "file_nhalo_offset_for_start_forestnum = %"PRId64" end_halonum = %"PRId64"\n", 
                    ThisTask, start_forestnum, end_forestnum, start_filenum, end_filenum, 
                    file_nhalo_offset_for_start_forestnum, end_halonum_for_end_forestnum);

    return EXIT_SUCCESS;


}


#define READ_TREE_PROPERTY(fd, offset, sage_name, dataset_name, count, C_dtype) { \
        const long long ndim = 1;                                                  \
        READ_PARTIAL_DATASET(fd, "TreeHalos", dataset_name, ndim, offset, count, buffer);\
        C_dtype *macro_x = (C_dtype *) buffer;                          \
        for (hsize_t halo_idx = 0; halo_idx < count[0]; halo_idx++){       \
            local_halos[halo_idx].sage_name = *macro_x;                 \
            macro_x++;                                                  \
        }                                                               \
    }

#define READ_TREE_PROPERTY_MULTIPLEDIM(fd, offset, sage_name, dataset_name, ndim, count, C_dtype) { \
        const long long dims = 2;                                                  \
        READ_PARTIAL_DATASET(fd, "TreeHalos", dataset_name, dims, offset, count, buffer);\
        C_dtype *macro_x = (C_dtype *) buffer;                          \
        for (hsize_t halo_idx = 0; halo_idx < count[0]; halo_idx++){       \
            for (int dim = 0; dim < NDIM; dim++) {                      \
                local_halos[halo_idx].sage_name[dim] = *macro_x;        \
                macro_x++;                                              \
            }                                                           \
        }                                                               \
    }



int64_t load_forest_gadget4_hdf5(const int64_t forestnr, struct halo_data **halos, struct forest_info *forests_info)
{

    /* Since the Gadget4 mergertree allows trees to be split across multiple files, we need to loop over the files */
    const struct gadget4_info *g4 = &(forests_info->gadget4);
    // const int64_t treenum_in_file = forests_info->original_treenr[forestnr];
    const int64_t nhalos = g4->nhalos_per_forest[forestnr];

    /* allocate the entire memory space required to store the halos*/
    *halos = mymalloc(sizeof(struct halo_data) * nhalos);
    XRETURN(*halos != NULL, -MALLOC_FAILURE,
            "Error: Could not allocate memory for %"PRId64" halos. Size requested = %"PRIu64" bytes\n",
            nhalos, nhalos * sizeof(struct halo_data));

    struct halo_data *local_halos = *halos;

    // char dataset_name[MAX_STRING_LEN];
    void *buffer; // Buffer to hold the read HDF5 data.
    buffer = malloc(nhalos * NDIM * sizeof(double)); // The largest data-type will be double.
    XRETURN(buffer != NULL, -MALLOC_FAILURE,
            "Error: Could not allocate memory for %"PRId64" halos in the HDF5 buffer. Size requested = %"PRIu64" bytes\n",
            nhalos, nhalos * NDIM * sizeof(double));

    const int32_t numfiles_this_forest = g4->num_files_per_forest[forestnr];
    for(int32_t ifile=0; ifile<numfiles_this_forest;ifile++) {
        const int32_t fd_index = g4->start_h5_fd_index[forestnr] + ifile;
        XRETURN(fd_index < g4->numfiles, -1, "Error: Index for HDF5 file pointer = %d should be between [0, %d)\n",
                fd_index, g4->numfiles);
        const hid_t fd = g4->open_h5_fds[fd_index];
        /* CHECK: HDF5 file pointer is valid */
        XRETURN( fd > 0, -INVALID_FILE_POINTER, "Error: File pointer is NULL (i.e., you need to open the file before reading).\n"
                "This error should already have been caught before reaching this line\n");


        hsize_t nhalo_offset[1] = {0};
        if(ifile == 0) {
            nhalo_offset[0] = g4->offset_in_nhalos_first_file_for_forests[forestnr];
        }
        // fprintf(stderr,"forestnr = %"PRId64" ifile = %d fd_index = %d (numfiles_this_task=%d) nhalos = %"PRId64" offset = %lld\n",
        //        forestnr, ifile, fd_index, g4->numfiles, nhalos, nhalo_offset[0]);

        const hsize_t numhalos_this_file[1] = {g4->nhalos_per_file_per_forest[forestnr][ifile]};
        // fprintf(stderr,"forestnr = %"PRId64" fd = %ld (numfiles = %d) treenum = %"PRId64" nhalos = %"PRId64" nhalos_this_file = %llu\n", 
        //                forestnr, (long) fd, numfiles_this_forest, treenum_in_file, nhalos, numhalos_this_file[0]);

        /* Merger Tree Pointers */
        READ_TREE_PROPERTY(fd, nhalo_offset, Descendant, "TreeDescendant", numhalos_this_file, int);
        READ_TREE_PROPERTY(fd, nhalo_offset, FirstProgenitor, "TreeFirstProgenitor", numhalos_this_file, int);
        READ_TREE_PROPERTY(fd, nhalo_offset, NextProgenitor, "TreeNextProgenitor", numhalos_this_file, int);
        READ_TREE_PROPERTY(fd, nhalo_offset, FirstHaloInFOFgroup, "TreeFirstHaloInFOFgroup", numhalos_this_file, int);
        READ_TREE_PROPERTY(fd, nhalo_offset, NextHaloInFOFgroup, "TreeNextHaloInFOFgroup", numhalos_this_file, int);

        /* Halo Properties */
        const hsize_t multi_dim_offset[] = {nhalo_offset[0], 0};
        const hsize_t multi_dim_count[] = {numhalos_this_file[0], 3};
        READ_TREE_PROPERTY(fd, nhalo_offset, Len, "SubhaloLen", numhalos_this_file, int);
        // READ_TREE_PROPERTY(fd, nhalo_offset, M_Mean200, Group_M_Mean200, numhalos_this_file, float);//MS: units 10^10 Msun/h for all Illustris mass fields
        // READ_TREE_PROPERTY(fd, nhalo_offset, M_Mean200, "SubhaloMass", numhalos_this_file, float);//MS: units 10^10 Msun/h for all Illustris mass fields
        READ_TREE_PROPERTY(fd, nhalo_offset, Mvir, "Group_M_Crit200", numhalos_this_file, float);//MS: 16/9/2019 sage uses Mvir but assumes that contains M200c
        // READ_TREE_PROPERTY(fd, nhalo_offset, M_TopHat, Group_M_TopHat200, numhalos_this_file, float);
        READ_TREE_PROPERTY_MULTIPLEDIM(fd, multi_dim_offset, Pos, "SubhaloPos", 3, multi_dim_count, float);//needs to be converted from kpc/h -> Mpc/h
        READ_TREE_PROPERTY_MULTIPLEDIM(fd, multi_dim_offset, Vel, "SubhaloVel", 3, multi_dim_count, float);//km/s
        READ_TREE_PROPERTY(fd, nhalo_offset, VelDisp, "SubhaloVelDisp", numhalos_this_file, float);//km/s
        READ_TREE_PROPERTY(fd, nhalo_offset, Vmax, "SubhaloVmax", numhalos_this_file, float);//km/s
        READ_TREE_PROPERTY_MULTIPLEDIM(fd, multi_dim_offset, Spin, "SubhaloSpin", 3, multi_dim_count, float);//(kpc/h)(km/s) -> convert to (Mpc)*(km/s). Does it need sqrt(3)?
        READ_TREE_PROPERTY(fd, nhalo_offset, MostBoundID, "SubhaloIDMostbound", numhalos_this_file, unsigned int);

        /* File Position Info */
        READ_TREE_PROPERTY(fd, nhalo_offset, SnapNum, "SnapNum", numhalos_this_file, int);
        // READ_TREE_PROPERTY(fd, nhalo_offset, FileNr, FileNr, numhalos_this_file, int);
        READ_TREE_PROPERTY(fd, nhalo_offset, SubhaloIndex, "SubhaloNr", numhalos_this_file, int);//MS: Unsure if this is the right field mapping (another option is SubhaloNumber)
        //READ_TREE_PROPERTY(fd, nhalo_offset, SubHalfMass, SubHalfMass, numhalos_this_file, float);//MS: Unsure what this field captures -> thankfully unused within sage

        local_halos += numhalos_this_file[0];

    #ifdef DEBUG_HDF5_READER
        for (int32_t i = 0; i < 20; ++i) {
            printf("halo %d: Descendant %d FirstProg %d x %.4f y %.4f z %.4f\n", i, local_halos[i].Descendant, local_halos[i].FirstProgenitor, local_halos[i].Pos[0], local_halos[i].Pos[1], local_halos[i].Pos[2]);
        }
        ABORT(0);
    #endif
    }
    local_halos -= nhalos;

    free(buffer);

    /* Since the Gadget4 mergertree is by far the most complicated among all the supported formats,
        we have this extra validation step within the load routine. MS 29/07/2023 */
    for(int64_t i=0;i<nhalos;i++) {
        XRETURN(local_halos[i].FirstProgenitor == -1 || (local_halos[i].FirstProgenitor >= 0 && local_halos[i].FirstProgenitor < nhalos), -1, 
        "Error: forestnr = %"PRId64" (with nhalos = %"PRId64") for i=%"PRId64" firstprog = %d\n", forestnr, nhalos, i, local_halos[i].FirstProgenitor);
        XRETURN(local_halos[i].Descendant == -1 || (local_halos[i].Descendant >= 0 && local_halos[i].Descendant < nhalos), -1, 
        "Error: forestnr = %"PRId64" (with nhalos = %"PRId64") for i=%"PRId64" firstprog = %d\n", forestnr, nhalos, i, local_halos[i].Descendant);
        XRETURN(local_halos[i].NextProgenitor == -1 || (local_halos[i].NextProgenitor >= 0 && local_halos[i].NextProgenitor < nhalos), -1, 
        "Error: forestnr = %"PRId64" (with nhalos = %"PRId64") for i=%"PRId64" firstprog = %d\n", forestnr, nhalos, i, local_halos[i].NextProgenitor);


        XRETURN(local_halos[i].FirstHaloInFOFgroup >= 0 && local_halos[i].FirstHaloInFOFgroup < nhalos, -1, 
        "Error: forestnr = %"PRId64" (with nhalos = %"PRId64") for i=%"PRId64" firstprog = %d\n", 
        forestnr, nhalos, i, local_halos[i].FirstHaloInFOFgroup);
        XRETURN(local_halos[i].NextHaloInFOFgroup == -1 || (local_halos[i].NextHaloInFOFgroup >= 0 && local_halos[i].NextHaloInFOFgroup < nhalos), -1, 
        "Error: forestnr = %"PRId64" (with nhalos = %"PRId64") for i=%"PRId64" firstprog = %d\n", forestnr, nhalos, i, local_halos[i].NextHaloInFOFgroup);

        // for(int k=0;k<3;k++) {
        //     XRETURN(local_halos[i].Pos[k] >= 0 && local_halos[i].Pos[k] <= forests_info->BoxSize, -1, 
        //     "Error: forestnr = %"PRId64" (with nhalos = %"PRId64" offset = %"PRId64") for i=%"PRId64" pos[%d] = %g original_treenr = %"PRId64" filenr = %d\n", 
        //     forestnr, nhalos, g4->offset_in_nhalos_first_file_for_forests[forestnr], 
        //     i, k, local_halos[i].Pos[k], forests_info->original_treenr[forestnr], forests_info->FileNr[forestnr]);
        // }
    }

    return nhalos;
}

#undef READ_TREE_PROPERTY
#undef READ_TREE_PROPERTY_MULTIPLEDIM


void cleanup_forests_io_gadget4_hdf5(struct forest_info *forests_info)
{
    struct gadget4_info *g4 = &(forests_info->gadget4);
    for(int32_t i=0;i<g4->numfiles;i++) {
        /* could use 'H5close' instead to make sure any open datasets are also
           closed; but that would hide potential bugs in code.
           valgrind should pick those cases up  */
        H5Fclose(g4->open_h5_fds[i]);
    }
    myfree(g4->open_h5_fds);
    myfree(g4->start_h5_fd_index);
    myfree(g4->nhalos_per_forest);
    for(int64_t iforest=0;iforest<g4->nforests;iforest++) {
        free(g4->nhalos_per_file_per_forest[iforest]);
    }
    myfree(g4->nhalos_per_file_per_forest);
    myfree(g4->offset_in_nhalos_first_file_for_forests);
    myfree(g4->num_files_per_forest);
}

int load_tree_table_gadget4_hdf5(const int firstfile, const int lastfile, const int64_t *totnforests_per_file, 
                                 const struct params *run_params, const int ThisTask, 
                                 int64_t *nhalos_per_forest)
{
    /* The treetable for nhalos per forest is written as 32-bit integers; 
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
        get_forests_filename_gadget4_hdf5(filename, 4*MAX_STRING_LEN, ifile, run_params);
        hid_t fd = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
        XRETURN( fd > 0, FILE_NOT_FOUND,"Error: can't open file `%s'\n", filename);

        /* If we are here, then there *must* be at least one *new* tree that starts in this file 
            -> the dataset TreeTable *must* exist. (The TreeTable dataset does not exist in files
            where there are no new trees beginning - only halos from trees that have begun in a previous
            file and potentially continue to the exact end of this file or beyond this file. - MS 6th June, 2023)
        */

        char dataset_name[MAX_STRING_LEN];
        snprintf(dataset_name, MAX_STRING_LEN, "TreeTable/Length");
        const int check_size = 1;
        herr_t h5_status = read_dataset(fd, dataset_name, 0, (void *) buffer, sizeof(*buffer), check_size);
        if(h5_status < 0) {
            return -1;
        }

        //Now assign the (32-bit integers) from buffer to the 64-bit nhalos_per_forest
        for(int64_t j=0;j<nforests_this_file;j++) {
            nhalos_per_forest[j] = (int64_t) buffer[j];
        }
        nhalos_per_forest += nforests_this_file;
        XRETURN ( H5Fclose(fd) >= 0, -1, "Error: Could not properly close the hdf5 file for filename = '%s'\n", filename);
    }
    free(buffer);
    return EXIT_SUCCESS;
}
