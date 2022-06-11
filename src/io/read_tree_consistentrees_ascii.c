#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/resource.h>
#include <limits.h>
#include <unistd.h>

#include "read_tree_consistentrees_ascii.h"
#include "../core_allvars.h"
#include "../core_mymalloc.h"
#include "../core_utils.h"
#include "../sglib.h"

#include "ctrees_utils.h"
#include "parse_ctrees.h"

void convert_ctrees_conventions_to_lht(struct halo_data *halos, struct additional_info *info, const int64_t nhalos,
                                       const int32_t snap_offset, const double part_mass, const int64_t forest_offset);

void get_forests_filename_ctr_ascii(char *filename, const size_t len, const struct params *run_params)
{
    /* this prints the first filename (tree_0_0_0.dat) */
    snprintf(filename, len-1, "%s/%s", run_params->SimulationDir, run_params->TreeName);
}

/* Externally visible Functions */
int setup_forests_io_ctrees(struct forest_info *forests_info, const int ThisTask, const int NTasks, struct params *run_params)
{
    struct rlimit rlp;
    getrlimit(RLIMIT_NOFILE, &rlp);
    rlp.rlim_cur = rlp.rlim_max;
    setrlimit(RLIMIT_NOFILE, &rlp);

    char locations_file[2*MAX_STRING_LEN], forests_file[2*MAX_STRING_LEN];
    snprintf(locations_file, 2*MAX_STRING_LEN-1, "%s/locations.dat", run_params->SimulationDir);
    snprintf(forests_file, 2*MAX_STRING_LEN-1, "%s/forests.list", run_params->SimulationDir);

    int64_t *treeids, *forestids;
    const int64_t totntrees = read_forests(forests_file, &forestids, &treeids);
    if(totntrees < 0) {
        return totntrees;
    }
    struct locations_with_forests *locations = calloc(totntrees, sizeof(locations[0]));
    XRETURN(locations != NULL, MALLOC_FAILURE, "Error: Could not allocate memory for storing locations details of %"PRId64" trees, each of size = %zu bytes\n",
            totntrees, sizeof(locations[0]));

    struct filenames_and_fd files_fd;
    int64_t nread = read_locations(locations_file, totntrees, locations, &files_fd);/* read_locations returns the number of trees read, but we already know it */
    if(nread != totntrees) {
        fprintf(stderr,"Number of trees read from the locations file ('%s') = %"PRId64" does not equal the number of trees read from the "
                "forests file (='%s') %"PRId64"...exiting\n",
                locations_file, nread, forests_file, totntrees);
        return EXIT_FAILURE;
    }

    int status = assign_forest_ids(totntrees, locations, forestids, treeids);
    if(status != EXIT_SUCCESS) {
        return status;
    }
    /* forestids are now within the locations variable */
    free(treeids);free(forestids);

    /* now sort by forestid, fileid, and file offset
       and then count the number of trees per forest */
    sort_locations_on_fid_file_offset(totntrees, locations);

    int64_t totnforests = 0;
    int64_t prev_forestid = -1;
    for(int64_t i=0;i<totntrees;i++) {
        if(locations[i].forestid != prev_forestid) {
            totnforests++;
            prev_forestid = locations[i].forestid;
        }
    }
    XRETURN(totnforests < INT_MAX,
            INTEGER_32BIT_TOO_SMALL,
            "Error: totnforests = %"PRId64" can not be represented by a 32 bit integer (max = %d)\n",
            totnforests, INT_MAX);

    struct ctrees_info *ctr = &(forests_info->ctr);

    forests_info->totnforests = totnforests;
    const int64_t nforests_per_cpu = (int64_t) (totnforests/NTasks);
    const int64_t rem_nforests = totnforests % NTasks;
    int64_t nforests_this_task = nforests_per_cpu;
    if(ThisTask < rem_nforests) {
        nforests_this_task++;
    }
    forests_info->nforests_this_task = nforests_this_task;

    int64_t start_forestnum = nforests_per_cpu * ThisTask;
    /* Add in the remainder forests that also need to be processed
       equivalent to the loop
       for(int task=0;task<ThisTask;task++) {
           if(task < rem_nforests) start_forestnum++;
       }

     */
    start_forestnum += ThisTask <= rem_nforests ? ThisTask:rem_nforests; /* assumes that "0<= ThisTask < NTasks" */
    const int64_t end_forestnum = start_forestnum + nforests_this_task; /* not inclusive, i.e., do not process foresnr == end_forestnum */

    int64_t ntrees_this_task = 0;
    int64_t start_treenum = -1;
    prev_forestid = -1;
    int64_t iforest = -1;
    for(int64_t i=0;i<totntrees;i++) {
        if(locations[i].forestid != prev_forestid) {
            iforest++;
            prev_forestid = locations[i].forestid;
        }
        if(iforest < start_forestnum) continue;

        if(iforest == start_forestnum && start_treenum < 0) {
            start_treenum = i;
        }
        if(iforest >= end_forestnum) break;

        ntrees_this_task++;
    }
    XRETURN(start_treenum >= 0 && start_treenum < totntrees,
            EXIT_FAILURE,
            "Error: start_treenum = %"PRId64" must be in range [0, %"PRId64")\n",
            start_treenum, totntrees);
    XRETURN(ntrees_this_task >= 0 && ntrees_this_task <= totntrees,
            EXIT_FAILURE,
            "Error: start_treenum = %"PRId64" must be in range [0, %"PRId64"]\n",
            start_treenum, totntrees);

    ctr->nforests = nforests_this_task;
    ctr->ntrees_per_forest = mymalloc(nforests_this_task * sizeof(ctr->ntrees_per_forest[0]));


    XRETURN(ctr->ntrees_per_forest != NULL, MALLOC_FAILURE, "Error: Could not allocate memory to store the number of trees per forest\n"
            "nforests_this_task = %"PRId64". Total number of bytes = %"PRIu64"\n",nforests_this_task, nforests_this_task*sizeof(ctr->ntrees_per_forest[0]));
    ctr->start_treenum_per_forest = mymalloc(nforests_this_task * sizeof(ctr->start_treenum_per_forest[0]));
    XRETURN(ctr->start_treenum_per_forest != NULL, MALLOC_FAILURE, "Error: Could not allocate memory to store the starting tree number per forest\n"
            "nforests_this_task = %"PRId64". Total number of bytes = %"PRIu64"\n", nforests_this_task, nforests_this_task*sizeof(ctr->start_treenum_per_forest[0]));
    ctr->tree_offsets = mymalloc(ntrees_this_task * sizeof(ctr->tree_offsets[0]));
    XRETURN(ctr->tree_offsets != NULL, MALLOC_FAILURE, "Error: Could not allocate memory to store the file offset (in bytes) per tree\n"
            "ntrees_this_task = %"PRId64". Total number of bytes = %"PRIu64"\n",ntrees_this_task, ntrees_this_task*sizeof(ctr->tree_offsets[0]));

    ctr->tree_fd = mymalloc(ntrees_this_task * sizeof(ctr->tree_fd[0]));
    XRETURN(ctr->tree_fd != NULL, MALLOC_FAILURE, "Error: Could not allocate memory to store the file descriptor per tree\n"
            "ntrees_this_task = %"PRId64". Total number of bytes = %"PRIu64"\n",ntrees_this_task, ntrees_this_task*sizeof(ctr->tree_fd[0]));

    forests_info->FileNr = malloc(nforests_this_task * sizeof(*(forests_info->FileNr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->FileNr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->FileNr", nforests_this_task,
                                     sizeof(*(forests_info->FileNr)));

    forests_info->original_treenr = malloc(nforests_this_task * sizeof(*(forests_info->original_treenr)));
    CHECK_POINTER_AND_RETURN_ON_NULL(forests_info->original_treenr,
                                     "Failed to allocate %"PRId64" elements of size %zu for forests_info->original_treenr", nforests_this_task,
                                     sizeof(*(forests_info->original_treenr)));

    iforest = -1;
    prev_forestid = -1;
    int first_tree = 0;
    const int64_t end_treenum = start_treenum + ntrees_this_task;

    // We assume that each of the input tree files span the same volume. Hence by summing the
    // number of trees processed by each task from each file, we can determine the
    // fraction of the simulation volume that this task processes.  We weight this summation by the
    // number of trees in each file because some files may have more/less trees whilst still spanning the
    // same volume (e.g., a void would contain few trees whilst a dense knot would contain many).
    forests_info->frac_volume_processed = 0.0;
    for(int64_t i=start_treenum;i<end_treenum;i++) {
        if(locations[i].forestid != prev_forestid) {
            iforest++;
            prev_forestid = locations[i].forestid;
            first_tree = 1;
        }
        const int64_t treeindex = i - start_treenum;

        if(first_tree == 1) {
            /* first tree in the forest */
            ctr->ntrees_per_forest[iforest] = 1;
            ctr->start_treenum_per_forest[iforest] = treeindex;
            first_tree = 0;

            /* MS: The 'filenr' variable is not unique at the forest level
             i.e., the trees from the same forest could belong to different files.
             The choice we make here is to pick the filenr corresponding to the
             first tree in the forest */
            forests_info->FileNr[iforest] = locations[i].fileid;
            /* forests_info->original_treenr[iforest] = locations[i].forestid;/\* MS: Stores the forestID as assigned by CTrees *\/ */
            forests_info->original_treenr[iforest] = start_forestnum + iforest;/* The forestID is too big and can not be used to
                                                                                  generate the unique GalaxyIndices directly. Hence resorting
                                                                                  to a forest index across all files */
        } else {
            /* still the same forest -> increment the number
               of trees this forest has */
            ctr->ntrees_per_forest[iforest]++;
        }

        /* fd contains ntrees elements; as does tree_offsets
           When we are reading a forest, we will need to load
           individual trees, where the trees themselves could be
           coming from different files (each tree is always fully
           contained in one file, but different trees from the
           same forest might be in different files) - MS: 27/7/2018
         */
        const int64_t fileid = locations[i].fileid;
        ctr->tree_fd[treeindex] = files_fd.fd[fileid];
        ctr->tree_offsets[treeindex] = locations[i].offset;

        /* MS: 23/9/2019 each tree from a given file is inversely weighted by the total
           number of trees in that file */
        forests_info->frac_volume_processed += 1.0/(double) files_fd.numtrees_per_file[fileid];
    }
    XRETURN(iforest == nforests_this_task-1, EXIT_FAILURE,
            "Error: Should have recovered the exact same value of forests. iforest = %"PRId64" should equal nforests =%"PRId64" - 1 \n",
            iforest, nforests_this_task);
    free(locations);

    /*MS: 23/9/2019 Fix up the normalisation to make the volume in [0.0, 1.0] (the previous step adds 1.0 per file
      -> the sum should be NumSimulationTreeFiles) */
    forests_info->frac_volume_processed /= (double) run_params->NumSimulationTreeFiles;
    if(forests_info->frac_volume_processed > 1.0) {
        fprintf(stderr,"Warning: Fraction of simulation volume  was > 1.0, *clamping* that to 1.0. (fraction - 1.0) = %g\n", forests_info->frac_volume_processed - 1.0);
        forests_info->frac_volume_processed = 1.0;
    }

    ctr->numfiles = files_fd.numfiles;
    ctr->open_fds = mymalloc(ctr->numfiles * sizeof(ctr->open_fds[0]));
    XRETURN(ctr->open_fds != NULL, MALLOC_FAILURE, "Error: Could not allocate memory to store the file descriptor per file\n"
            "numfiles = %d. Total number of bytes = %zu\n",ctr->numfiles, ctr->numfiles*sizeof(ctr->open_fds[0]));

    for(int i=0;i<ctr->numfiles;i++) {
        ctr->open_fds[i] = files_fd.fd[i];
    }

    free(files_fd.numtrees_per_file);
    free(files_fd.fd);

    /* Now need to parse the header to figure out which columns go where ... */
    ctr->column_info = mymalloc(1 * sizeof(struct ctrees_column_to_ptr));
    XRETURN(ctr->column_info != NULL, EXIT_FAILURE,
            "Error: Could not allocate memory to store the column_info struct of size = %zu bytes\n",
            sizeof(struct ctrees_column_to_ptr));
    char column_names[][PARSE_CTREES_MAX_COLNAME_LEN] = {"scale", "id", "desc_scale", "desc_id",
                                                         "pid", "upid",
                                                         "mvir", "vrms",
                                                         "vmax",
                                                         "x", "y", "z",
                                                         "vx", "vy", "vz",
                                                         "Jx", "Jy", "Jz",
                                                         "snap_num", "snap_idx",/* older versions use snap_num -> only one will be found! */
                                                         "M200b", "M200c"};

    enum parse_numeric_types dest_field_types[] = {F64, I64, F64, I64,
                                                   I64, I64,
                                                   F32, F32,
                                                   F32,
                                                   F32, F32, F32,
                                                   F32, F32, F32,
                                                   F32, F32, F32,
                                                   I32, I32,
                                                   F32, F32};
    int64_t base_ptr_idx[] = {1, 1, 1, 1,
                              1, 1,
                              0, 0,
                              0,
                              0, 0, 0,
                              0, 0, 0,
                              0, 0, 0,
                              0, 0,
                              0, 0};

    size_t dest_offset_to_element[] = {offsetof(struct additional_info, scale),
                                       offsetof(struct additional_info, id),
                                       offsetof(struct additional_info, desc_scale),
                                       offsetof(struct additional_info, descid),
                                       offsetof(struct additional_info, pid),
                                       offsetof(struct additional_info, upid),
                                       offsetof(struct halo_data, Mvir),
                                       offsetof(struct halo_data, VelDisp),
                                       offsetof(struct halo_data, Vmax),
                                       offsetof(struct halo_data, Pos[0]),
                                       offsetof(struct halo_data, Pos[1]),
                                       offsetof(struct halo_data, Pos[2]),
                                       offsetof(struct halo_data, Vel[0]),
                                       offsetof(struct halo_data, Vel[1]),
                                       offsetof(struct halo_data, Vel[2]),
                                       offsetof(struct halo_data, Spin[0]),
                                       offsetof(struct halo_data, Spin[1]),
                                       offsetof(struct halo_data, Spin[2]),
                                       /* only one of 'snap_num' or 'snap_idx' will be found -> assign to SnapNum within struct halo_data*/
                                       offsetof(struct halo_data, SnapNum), offsetof(struct halo_data, SnapNum),
                                       offsetof(struct halo_data, M_Mean200),
                                       offsetof(struct halo_data, M_TopHat)};

    const int nwanted = sizeof(column_names)/sizeof(column_names[0]);
    const int nwanted_types = sizeof(dest_field_types)/sizeof(dest_field_types[0]);
    const int nwanted_idx = sizeof(base_ptr_idx)/sizeof(base_ptr_idx[0]);
    const int nwanted_offs = sizeof(dest_offset_to_element)/sizeof(dest_offset_to_element[0]);
    XRETURN(nwanted == nwanted_types, EXIT_FAILURE,
            "nwanted = %d should be equal to ntypes = %d\n",
            nwanted, nwanted_types);
    XRETURN(nwanted_idx == nwanted_offs, EXIT_FAILURE,
            "nwanted_idx = %d should be equal to nwanted_offs = %d\n",
            nwanted_idx, nwanted_offs);
    XRETURN(nwanted == nwanted_offs, EXIT_FAILURE,
            "nwanted = %d should be equal to nwanted_offs = %d\n",
            nwanted, nwanted_offs);

    char filename[2*MAX_STRING_LEN + 1];
    get_forests_filename_ctr_ascii(filename, sizeof(filename), run_params);
    status = parse_header_ctrees(column_names, dest_field_types, base_ptr_idx, dest_offset_to_element,
                                 nwanted, filename, (struct ctrees_column_to_ptr *) ctr->column_info);
    if(status != EXIT_SUCCESS) {
        return status;
    }


    /* Finally setup the multiplication factors necessary to generate
       unique galaxy indices (across all files, all trees and all tasks) for this run*/
    run_params->FileNr_Mulfac = 0;
    run_params->ForestNr_Mulfac = 1000000000LL;/*MS: The ID needs to fit in 64 bits -> ID must be <  2^64 ~ 1e19.*/

    return EXIT_SUCCESS;
}

int64_t load_forest_ctrees(const int32_t forestnr, struct halo_data **halos, struct forest_info *forests_info, struct params *run_params)
{
    struct ctrees_info *ctr = &(forests_info->ctr);
    const int64_t ntrees = ctr->ntrees_per_forest[forestnr];
    const int64_t start_treenum = ctr->start_treenum_per_forest[forestnr];

    const int64_t default_nhalos_per_tree = 1000;/* allocate for a 100k halos per tree by default */
    int64_t nhalos_allocated = default_nhalos_per_tree * ntrees;

    *halos = mymalloc(nhalos_allocated * sizeof(struct halo_data));
    XRETURN( *halos != NULL, -MALLOC_FAILURE, "Error: Could not allocate memory to store halos\n"
             "ntrees = %"PRId64" nhalos_allocated = %"PRId64". Total number of bytes = %"PRIu64"\n",
             ntrees, nhalos_allocated, nhalos_allocated*sizeof(struct halo_data));

    struct additional_info *info = mymalloc(nhalos_allocated * sizeof(struct additional_info));
    XRETURN( info != NULL, -MALLOC_FAILURE, "Error: Could not allocate memory to store additional info per halo\n"
             "ntrees = %"PRId64" nhalos_allocated = %"PRId64". Total number of bytes = %"PRIu64"\n",
             ntrees, nhalos_allocated, nhalos_allocated*sizeof(info[0]));

    struct base_ptr_info base_info;
    base_info.num_base_ptrs = 2;
    base_info.base_ptrs[0] = (void **) halos;
    base_info.base_element_size[0] = sizeof(struct halo_data);

    base_info.base_ptrs[1] = (void **) &info;
    base_info.base_element_size[1] = sizeof(struct additional_info);

    base_info.N = 0;
    base_info.nallocated = nhalos_allocated;

    struct ctrees_column_to_ptr *column_info = ctr->column_info;
    /* fprintf(stderr,"Reading in forestnr = %d ntrees = %"PRId64"\n", forestnr, ntrees); */
    for(int64_t i=0;i<ntrees;i++) {
        const int64_t treenum = i + start_treenum;
        int fd = ctr->tree_fd[treenum];
        off_t offset = ctr->tree_offsets[treenum];
        const int64_t prev_N = base_info.N;
        /* fprintf(stderr,"Loading treenum=%"PRId64" (offset = %"PRId64") in forestnr = %d. Nhalos so far = %"PRId64"\n", i, (int64_t) offset, forestnr, prev_N); */
        /* fprintf(stderr,"*halos = %p , info = %p\n", *halos, info); */
        /* fprintf(stderr,"*(base[0]) = %p , *(base[1]) = %p\n", *(base_info.base_ptrs[0]), *(base_info.base_ptrs[1])); */
        int status = read_single_tree_ctrees(fd, offset, column_info, &base_info);
        if(status != EXIT_SUCCESS) {
            return -EXIT_FAILURE;
        }
        struct halo_data *local_halos = *halos;
        const int64_t nhalos = base_info.N - prev_N;
        const int32_t snap_offset = 0;/* need to figure out how to set this correctly (do not think there is an automatic way to do so): MS 03/08/2018 */
        convert_ctrees_conventions_to_lht(&(local_halos[prev_N]), info, nhalos, snap_offset, run_params->PartMass, prev_N);
    }
    const int64_t totnhalos = base_info.N;
    const int64_t nallocated = base_info.nallocated;
    /* fprintf(stderr,"Reading in forestnr = %d ...done. totnhalos = %"PRId64"\n", forestnr, totnhalos); */
    /* forests_info->totnhalos_per_forest[forestnr] = (int32_t) totnhalos; */

    XRETURN(totnhalos  <= nallocated, -1,"Error: Total number of halos loaded = %"PRId64" must be less than the number of halos "
            "allocated = %"PRId64"\n", base_info.N, nallocated);

    /* release any additional memory that may have been allocated */
    *halos = myrealloc(*halos, totnhalos * sizeof(struct halo_data));
    XRETURN( *halos != NULL, -1, "Bug: This should not have happened -- a 'realloc' call to reduce the amount of memory failed\n"
             "Trying to reduce from %"PRIu64" bytes to %"PRIu64" bytes\n",
             nhalos_allocated*sizeof(struct halo_data), totnhalos * sizeof(struct halo_data));

    info = myrealloc(info, totnhalos * sizeof(struct additional_info));
    XRETURN( info != NULL, -1, "Bug: This should not have happened -- a 'realloc' call (for 'struct additional_info')"
             "to reduce the amount of memory failed\nTrying to reduce from %"PRIu64" bytes to %"PRIu64" bytes\n",
             nhalos_allocated * sizeof(struct additional_info), totnhalos * sizeof(struct additional_info));

    /* all halos belonging to this forest have now been loaded up */
    int verbose = 0;
    struct halo_data *forest_halos = *halos;

    /* Fix flybys -> multiple roots at z=0 must be joined such that only one root remains */
    int status = fix_flybys(totnhalos, forest_halos, info, verbose);
    if(status != EXIT_SUCCESS) {
        /* Not successful but the exit status from this routine has to be negative */
        const int neg_status = status < 0 ? status:-status;
        return neg_status;
    }


    /* Entire tree is loaded in. Fix upid's (i.e., only keep 1-level halo hierarchy: FOF->subhalo */
    const int max_snapnum = fix_upid(totnhalos, forest_halos, info, verbose);
    if(max_snapnum < 0) {
        return -EXIT_FAILURE;
    }

    /* Now the entire tree is loaded in. Assign the mergertree indices */
    status = assign_mergertree_indices(totnhalos, forest_halos, info, max_snapnum);
    if(status != EXIT_SUCCESS) {
        /* Not successful but the exit status from this routine has to be negative */
        const int neg_status = status < 0 ? status:-status;
        return neg_status;
    }

    /* Now we can free the additional_info struct */
    myfree(info);

    return totnhalos;
}

void convert_ctrees_conventions_to_lht(struct halo_data *halos, struct additional_info *info, const int64_t nhalos,
                                       const int32_t snap_offset, const double part_mass, const int64_t forest_offset)
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
        halos->SubhaloIndex = (int) (forest_offset + nhalos);
        halos->SubHalfMass  = -1.0f;

        /* Carry the Rockstar/Ctrees generated haloID through */
        halos->MostBoundID  = info->id;

        /* All the mergertree indices */
        halos->Descendant = -1;
        halos->FirstProgenitor = -1;
        halos->NextProgenitor = -1;
        halos->FirstHaloInFOFgroup = -1;
        halos->NextHaloInFOFgroup = -1;

        /* Convert the snapshot index output by Consistent Trees
           into the snapshot number as reported by the simulation */
        halos->SnapNum += snap_offset;
        halos++;info++;
    }
}

void cleanup_forests_io_ctrees(struct forest_info *forests_info)
{
    struct ctrees_info *ctr = &(forests_info->ctr);
    myfree(ctr->ntrees_per_forest);
    myfree(ctr->start_treenum_per_forest);
    myfree(ctr->tree_offsets);
    myfree(ctr->tree_fd);
    myfree(ctr->column_info);
    for(int64_t i=0;i<ctr->numfiles;i++) {
        close(ctr->open_fds[i]);
    }
    myfree(ctr->open_fds);
}
