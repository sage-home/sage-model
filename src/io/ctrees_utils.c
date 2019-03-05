#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <inttypes.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>


#include "../core_allvars.h"
#include "../core_utils.h"
#include "../sglib.h"

#include "ctrees_utils.h"
#include "parse_ctrees.h"

int CTREES_UPID_FEATURE = 0;

int64_t find_fof_halo(const int64_t totnhalos, const struct additional_info *info, const int start_loc, const int64_t upid, int verbose, int64_t calldepth);


int64_t read_forests(const char *filename, int64_t **f, int64_t **t)
{
    int64_t ntrees;
    char buffer[MAX_STRING_LEN];
    const char comment = '#';
    
    /* By passing the comment character, getnumlines
       will return the actual number of lines, ignoring
       the first header line. 
    */
    FILE *fp = fopen(filename, "rt");
    if(fp == NULL) {
        printf("Error: can't open file `%s'\n", filename);
        ABORT(FILE_NOT_FOUND);
    }
    ntrees = getnumlines(filename, comment);

    *f = malloc(ntrees * sizeof(int64_t));
    *t = malloc(ntrees * sizeof(int64_t));
    
    int64_t *forests    = *f;
    int64_t *tree_roots = *t;
    
    int64_t ntrees_found = 0;
    while(fgets(buffer, MAX_STRING_LEN, fp) != NULL) {
        if(buffer[0] == comment) {
            continue;
        } else {
            const int nitems_expected = 2;
            XASSERT(ntrees_found < ntrees, -1,
                    "ntrees=%"PRId64" should be less than ntrees_found=%"PRId64"\n", ntrees, ntrees_found);            
            int nitems = sscanf(buffer, "%"SCNd64" %"SCNd64, tree_roots, forests);
            XASSERT(nitems == nitems_expected, -1,
                    "Expected to parse %d long integers but found `%s' in the buffer. nitems = %d \n",
                    nitems_expected, buffer, nitems);
            ntrees_found++;tree_roots++;forests++;
        }
    }
    fclose(fp);
    XASSERT(ntrees == ntrees_found, -1,
            "ntrees=%"PRId64" should be equal to ntrees_found=%"PRId64"\n", ntrees, ntrees_found);
    
    return ntrees;
}    


int64_t read_locations(const char *filename, const int64_t ntrees, struct locations_with_forests *l, struct filenames_and_fd *filenames_and_fd)
{
    char buffer[MAX_STRING_LEN];
    int64_t max_fileid = 0;
    const char comment = '#';
    /* By passing the comment character, getnumlines
       will return the actual number of lines, ignoring
       the first header line. */

    char dirname[MAX_STRING_LEN];
    memset(dirname, '\0', MAX_STRING_LEN);
    memcpy(dirname, filename, strlen(filename));
    for(int i=MAX_STRING_LEN-2;i>=0;i--) {
        if(dirname[i] == '/') {
            dirname[i] = '\0';
            break;
        }
    }
    
    struct filenames_and_fd *files_fd = filenames_and_fd;
    uint32_t numfiles_allocated = 2000;
    files_fd->fd = malloc(numfiles_allocated * sizeof(files_fd->fd[0]));
    XASSERT( files_fd->fd != NULL, MALLOC_FAILURE, "Error: Could not allocate memory of %zu bytes to hold %"PRIu32" file descriptors",
             numfiles_allocated*sizeof(files_fd->fd[0]), numfiles_allocated);
    files_fd->nallocated = numfiles_allocated;
    files_fd->numfiles = 0;
    for(uint32_t i=0;i<numfiles_allocated;i++) {
        files_fd->fd[i] = -1;
    }
    
    struct locations_with_forests *locations = l;

    int64_t ntrees_found = 0;
    FILE *fp = fopen(filename, "r");
    XASSERT( fp != NULL, FILE_NOT_FOUND, "Error: Could not open filename `%s'\n", filename);
    while(fgets(buffer, MAX_STRING_LEN, fp) != NULL) {
        if(buffer[0] == comment) {
            continue;
        } else {
            const int nitems_expected = 4;
            char linebuf[MAX_STRING_LEN];
            XASSERT(ntrees_found < ntrees, EXIT_FAILURE,
                    "ntrees=%"PRId64" should be less than ntrees_found=%"PRId64"\n",
                    ntrees, ntrees_found);            
            int nitems = sscanf(buffer, "%"SCNd64" %d %"SCNd64 "%s",
                                &locations->treeid, &locations->fileid, &locations->offset, linebuf);

            XASSERT(locations->offset >= 0, INVALID_VALUE_READ_FROM_FILE, 
                    "offset=%"PRId64" for ntree =%"PRId64" must be positive.\nFile = `%s'\nbuffer = `%s'\n",
                    locations->offset, ntrees_found, filename, buffer);
            
            const size_t fileid = locations->fileid;
            if(fileid > files_fd->nallocated) {
                numfiles_allocated *= 2;
                int *new_fd = realloc(files_fd->fd, numfiles_allocated*sizeof(files_fd->fd[0]));
                XASSERT(new_fd != NULL, MALLOC_FAILURE, "Error: Could not re-allocate memory of %zu bytes to hold %"PRIu32" file descriptors\n",
                        numfiles_allocated*sizeof(files_fd->fd[0]), numfiles_allocated);

                files_fd->fd = new_fd;
                for(uint32_t i=files_fd->nallocated;i<numfiles_allocated;i++) {
                    files_fd->fd[i] = -1;
                };
                files_fd->nallocated = numfiles_allocated;
            }
            XASSERT(fileid < files_fd->nallocated, EXIT_FAILURE, "Error: Trying to open fileid = %zu but only %d"
                    " files can be opened (need to malloc) \n",
                    fileid, files_fd->nallocated);
            
            /* file has not been opened yet - let's open this file */
            if(files_fd->fd[fileid] < 0) {
                char treefilename[MAX_STRING_LEN];
                snprintf(treefilename, MAX_STRING_LEN-1, "%s/%s", dirname, linebuf); 
                files_fd->fd[fileid] = open(treefilename, O_RDONLY);
                XASSERT( files_fd->fd[fileid] > 0, FILE_NOT_FOUND, "Error: Could not open file `%s'\n", treefilename);
                files_fd->numfiles++;
            }
            
            XASSERT(nitems == nitems_expected, EXIT_FAILURE, "Expected to parse two long integers but found `%s' in the buffer\n", buffer);
            ntrees_found++;
            locations++;
        }
    }
    XASSERT(ntrees == ntrees_found, EXIT_FAILURE, "ntrees=%"PRId64" should be equal to ntrees_found=%"PRId64"\n", ntrees, ntrees_found);
    fclose(fp);

    locations = l;
    for(int64_t i=0;i<ntrees_found;i++){
        if (locations[i].fileid > max_fileid) {
            max_fileid = locations[i].fileid;
        }
    }

    /* number of files is one greater from 0 based indexing of C files */
    XASSERT(max_fileid + 1 == files_fd->numfiles, EXIT_FAILURE,
            "Error: Validation error -- number of files expected from max. of fileids in 'locations.dat' = %"PRId64" but only found %d filenames\n"
            "Perhaps fileids (column 3) in 'locations.dat' are not contiguous?\n",
            max_fileid + 1, files_fd->numfiles);
    const int box_divisions = (int) round(cbrt(files_fd->numfiles));
    const int box_cube = box_divisions * box_divisions * box_divisions;
    XASSERT( box_cube == files_fd->numfiles, EXIT_FAILURE, 
             "box_divisions^3=%d should be equal to nfiles=%"PRId32"\n",
             box_cube, files_fd->numfiles);
    
    return ntrees_found;
}    


void sort_forests_by_treeid(const int64_t ntrees, int64_t *forests, int64_t *treeids)
{

#define MULTIPLE_ARRAY_EXCHANGER(type,a,i,j) {                      \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(int64_t, treeids,i, j);      \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(int64_t, forests, i, j);     \
    }
    
    SGLIB_ARRAY_QUICK_SORT(int64_t, treeids, ntrees, SGLIB_NUMERIC_COMPARATOR , MULTIPLE_ARRAY_EXCHANGER);
#undef MULTIPLE_ARRAY_EXCHANGER

}    

int compare_locations_treeids(const void *l1, const void *l2)
{
    const struct locations_with_forests *aa = (const struct locations_with_forests *) l1;
    const struct locations_with_forests *bb = (const struct locations_with_forests *) l2;
    return (aa->treeid < bb->treeid) ? -1:(aa->treeid==bb->treeid ? 0:1);
}

int compare_locations_fid(const void *l1, const void *l2)
{
    const struct locations_with_forests *aa = (const struct locations_with_forests *) l1;
    const struct locations_with_forests *bb = (const struct locations_with_forests *) l2;
    return (aa->forestid < bb->forestid) ? -1:1;
}

int compare_locations_file_offset(const void *l1, const void *l2)
{
    const struct locations_with_forests *aa = (const struct locations_with_forests *) l1;
    const struct locations_with_forests *bb = (const struct locations_with_forests *) l2;

    const int file_id_cmp = (aa->fileid == bb->fileid) ? 0:((aa->fileid < bb->fileid) ? -1:1);
    if(file_id_cmp == 0) {
        /* trees are in same file -> sort by offset */
        return (aa->offset < bb->offset) ? -1:1;
    } else {
        return file_id_cmp;
    }
        
    return 0;
}

int compare_locations_fid_file_offset(const void *l1, const void *l2)
{
    const struct locations_with_forests *aa = (const struct locations_with_forests *) l1;
    const struct locations_with_forests *bb = (const struct locations_with_forests *) l2;

    if(aa->forestid != bb->forestid) {
        return (aa->forestid < bb->forestid) ? -1:1;
    } else {
        /* The trees are in the same forest. Check filename */
        const int file_id_cmp = (aa->fileid == bb->fileid) ? 0:((aa->fileid < bb->fileid) ? -1:1);
        if(file_id_cmp == 0) {
            /* trees are in same file -> sort by offset */
            return (aa->offset < bb->offset) ? -1:1;
        } else {
            return file_id_cmp;
        }
    }
        
    return 0;/* un-reachable */
}

void sort_locations_on_treeroot(const int64_t ntrees, struct locations_with_forests *locations)
{
    qsort(locations, ntrees, sizeof(*locations), compare_locations_treeids);
}    

void sort_locations_file_offset(const int64_t ntrees, struct locations_with_forests *locations)
{
    qsort(locations, ntrees, sizeof(*locations), compare_locations_file_offset);
}

void sort_locations_on_fid(const int64_t ntrees, struct locations_with_forests *locations)
{
    qsort(locations, ntrees, sizeof(*locations), compare_locations_fid);
}    

void sort_locations_on_fid_file_offset(const int64_t ntrees, struct locations_with_forests *locations)
{
    qsort(locations, ntrees, sizeof(*locations), compare_locations_fid_file_offset);
}    


int assign_forest_ids(const int64_t ntrees, struct locations_with_forests *locations, int64_t *forests, int64_t *treeids)
{
    /* Sort forests by tree roots -> necessary for assigning forest ids */
    sort_forests_by_treeid(ntrees, forests, treeids);
    sort_locations_on_treeroot(ntrees, locations);    
    
    /* forests and treeids are sorted together, on treeids */
    /* locations is sorted on tree roots */
    for(int64_t i=0;i<ntrees;i++) {
        XASSERT(treeids[i] == locations[i].treeid, EXIT_FAILURE, 
                "tree roots[%"PRId64"] = %"PRId64" does not equal tree roots in locations = %"PRId64"\n",
                i, treeids[i], locations[i].treeid);
        locations[i].forestid = forests[i];
    }

    return EXIT_SUCCESS;
}    


int fix_flybys(const int64_t totnhalos, struct halo_data *forest, struct additional_info *info, int verbose)
{
#define ID_COMPARATOR(x, y)         ((x.id > y.id ? 1:(x.id < y.id ? -1:0)))
#define SCALE_ID_COMPARATOR(x,y)    ((x.scale > y.scale ? -1:(x.scale < y.scale ? 1:ID_COMPARATOR(x, y))) )
#define MULTIPLE_ARRAY_EXCHANGER(type,a,i,j) {                          \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(struct halo_data, forest,i,j); \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(struct additional_info, info, i, j) \
            }
    SGLIB_ARRAY_HEAP_SORT(struct additional_info, info, totnhalos, SCALE_ID_COMPARATOR, MULTIPLE_ARRAY_EXCHANGER);

#undef ID_COMPARATOR
#undef SCALE_ID_COMPARATOR
#undef MULTIPLE_ARRAY_EXCHANGER    

    double max_scale = info[0].scale;
    int64_t last_halo_with_max_scale = 1;
    int64_t num_fofs_last_scale = info[0].pid == -1 ? 1:0;
    for(int64_t i=1;i<totnhalos;i++) {
        if(info[i].scale < max_scale) {
            break;
        }
        num_fofs_last_scale += (info[i].pid == -1) ? 1:0;
        last_halo_with_max_scale = i;
    }
    if(num_fofs_last_scale == 0) {
        fprintf(stderr,"ERROR: NO FOFs at max scale = %lf Will crash - here's some info that might help debug\n", max_scale);
        fprintf(stderr,"Last scale halo id (likely tree root id ) = %"PRId64" at a = %lf\n",info[0].id, info[0].scale);
        fprintf(stderr,"########################################################\n");
        fprintf(stderr,"# snap     id      pid      upid    mass     scale      \n");
        fprintf(stderr,"########################################################\n");
        for(int64_t i=0;i<=last_halo_with_max_scale;i++) {
            fprintf(stderr,"%d  %10"PRId64"  %10"PRId64" %10"PRId64" %12.6e  %20.8e\n",
                    forest[i].SnapNum, info[i].id, info[i].pid, info[i].upid, forest[i].Mvir, info[i].scale);
        }
        fprintf(stderr,"All halos now:\n\n");
        for(int64_t i=0;i<totnhalos;i++) {
            fprintf(stderr,"%d  %10"PRId64"  %10"PRId64" %10"PRId64" %12.6e %20.8e\n",
                    forest[i].SnapNum, info[i].id, info[i].pid, info[i].upid, forest[i].Mvir, info[i].scale);
        }
        return -1;
    }
    
    /* Is there anything to do? If there is only one FOF at z=0, then simply return */
    if(num_fofs_last_scale == 1) {
        return EXIT_SUCCESS;
    }
    
    int64_t max_mass_fof_loc = -1;
    float max_mass_fof = -1.0f;
    int64_t fof_id = -1;
    for(int64_t i=0;i<=last_halo_with_max_scale;i++) {
        if(forest[i].Mvir > max_mass_fof && info[i].pid == -1) {
            max_mass_fof_loc = i;
            max_mass_fof = forest[max_mass_fof_loc].Mvir;
            fof_id = info[max_mass_fof_loc].id;
        }
    }

    XASSERT(fof_id != -1, EXIT_FAILURE, 
            "There must be at least one FOF halo.");
    XASSERT(max_mass_fof_loc < INT_MAX, EXIT_FAILURE, 
            "Most massive FOF location=%"PRId64" must be representable within INT_MAX=%d",
            max_mass_fof_loc, INT_MAX);

    int FirstHaloInFOFgroup = (int) max_mass_fof_loc;
    for(int64_t i=0;i<=last_halo_with_max_scale;i++) {
        if(i == FirstHaloInFOFgroup) {
            continue;
        }
        if(info[i].pid == -1) {
            //Show that this halo was switched from being a central
            //just flip the sign. (MostBoundID should not have negative
            //values -> this would signify a flyby)
            forest[i].MostBoundID = -forest[i].MostBoundID;
            info[i].pid = fof_id;
            if(verbose == 1) {
                fprintf(stderr,"id = %"PRId64" changed pid = -1 to pid = %"PRId64" for i=%"PRId64" FirstHaloInFOFgroup =%d last_halo_max_scale=%"PRId64"\n",
                        info[i].id, fof_id, i, FirstHaloInFOFgroup,last_halo_with_max_scale);
            }
        }
        info[i].upid = fof_id;
    }

    return EXIT_SUCCESS;
}


int fix_upid(const int64_t totnhalos, struct halo_data *forest, struct additional_info *info, int *interrupted, const int verbose)
{

    int max_snapnum = -1;

    /*First sort everything on ID */
#define ID_COMPARATOR(x, y)         ((x.id > y.id ? 1:(x.id < y.id ? -1:0)))
#define SCALE_ID_COMPARATOR(x,y)    ((x.scale > y.scale ? -1:(x.scale < y.scale ? 1:ID_COMPARATOR(x, y))) )
#define MULTIPLE_ARRAY_EXCHANGER(type,a,i,j) {                          \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(struct halo_data, forest,i,j); \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(struct additional_info, info, i, j) \
            }
    SGLIB_ARRAY_HEAP_SORT(struct additional_info, info, totnhalos, SCALE_ID_COMPARATOR, MULTIPLE_ARRAY_EXCHANGER);

    /* Change upid to id, so we can sort the fof's and subs to be contiguous */
    //I am paranoid -> so I am going to set all FOF upid's first and then
    //use the upid again. Two loops are required but that relaxes any assumptions
    //about ordering of fof/subhalos. 
    for(int64_t i=0;i<totnhalos;i++) {
        info[i].upid = (info[i].pid == -1) ? info[i].id:info[i].upid;
        if(forest[i].SnapNum > max_snapnum) {
            max_snapnum = forest[i].SnapNum;
        }
    }

    for(int64_t i=0;i<totnhalos;i++) {
        if(info[i].pid == -1) {
            continue;
        }
        
        /* Only (sub)subhalos should reach here */
        /*Check if upid points to host halo with pid == -1*/
        int64_t upid = info[i].upid;
        int64_t calldepth=0;
        if(verbose) {
            fprintf(stderr,"CALLING FIND FOF HALO with i = %"PRId64" id = %"PRId64" upid = %"PRId64"\n", i, info[i].id, upid);
        }
        int64_t loc = find_fof_halo(totnhalos, info, i, upid, verbose, calldepth);
        while(loc < 0 || loc >= totnhalos) {
            fprintf(stderr, "looping to locate fof halo for i = %"PRId64" id = %"PRId64" upid = %"PRId64" loc=%"PRId64"\n",
                    i, info[i].id, upid, loc);
            int64_t track_id = upid;
            int found = 0;
            for(int64_t j=0;j<totnhalos;j++) {
                if(info[j].id == track_id) {
                    found = 1;
                    fprintf(stderr,"found track_id = %"PRId64" pid = %"PRId64" upid = %"PRId64"\n", track_id, info[j].pid, info[j].upid);
                    if(info[j].pid == -1) {
                        loc = j;
                        break;
                    } else {
                        track_id = info[j].pid;
                    }
                    break;
                }
            }
            XASSERT( found == 1, EXIT_FAILURE,
                     "Error: Could not locate FOF halo for halo with id = %"PRId64" and upid = %"PRId64"\n",
                     info[i].id, upid);
        }
        if(verbose) {
            fprintf(stderr,"found FOF halo for halnum = %"PRId64". loc = %"PRId64" id = %"PRId64" upid = %"PRId64"\n",
                    i, loc, info[loc].id, info[loc].upid);
        }
        XASSERT(loc >=0 && loc < totnhalos, EXIT_FAILURE, 
                "could not locate fof halo for i = %"PRId64" id = %"PRId64" upid = %"PRId64" loc=%"PRId64"\n",
                i, info[i].id, upid, loc);
        const int64_t new_upid = info[loc].id;
        if(new_upid != upid && CTREES_UPID_FEATURE == 0) {
            fprintf(stderr, ANSI_COLOR_RED "Fixing upid for i=%"PRId64" original upid =%"PRId64" new fof upid = %"PRId64 ANSI_COLOR_RESET"\n",
                    i, upid, new_upid);
            CTREES_UPID_FEATURE = 1;
            *interrupted = 1;
        }
        if(verbose) {
            fprintf(stderr,"setting upid/pid for halonum = %"PRId64" to %"PRId64". previously: pid = %"PRId64" upid = %"PRId64". id = %"PRId64"\n",
                    i, new_upid, info[i].pid, info[i].upid, info[i].id); 
        }
        info[i].upid = new_upid;
        info[i].pid  = new_upid;
    }
#undef ID_COMPARATOR
#undef SCALE_ID_COMPARATOR
#undef MULTIPLE_ARRAY_EXCHANGER    

    return max_snapnum;
    
}

void assign_mergertree_indices(const int64_t totnhalos, struct halo_data *forest, struct additional_info *info, const int max_snapnum)
{
    const int nsnapshots = max_snapnum + 1;
    double *scales = malloc(nsnapshots * sizeof(*scales));
    XASSERT(scales != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to store the scale-factors for each snapshot (nsnapshots = %d)\n", nsnapshots);
    for(int i=0;i<nsnapshots;i++) {
        scales[i] = DBL_MAX;
    }
    int64_t *start_scale = malloc(nsnapshots * sizeof(*start_scale));
    XASSERT(start_scale != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to store the starting scale-factors for each snapshot (nsnapshots = %d)\n", nsnapshots);
    int64_t *end_scale = malloc(nsnapshots * sizeof(*end_scale));
    XASSERT(end_scale != NULL, MALLOC_FAILURE,
            "Error: Could not allocate memory to store the ending scale-factors for each snapshot (nsnapshots = %d)\n", nsnapshots);
    for(int i=0;i<nsnapshots;i++) {
        start_scale[i] = -1;
    }
    
    /* Sort the trees based on scale, upid, and pid */
    /* Descending sort on scale, and then ascending sort on upid.
       The pid sort is so that the FOF halo comes before the (sub-)subhalos.
       The last id sort is such that the ordering of (sub-)subhalos
       is unique (stable sort, since id's are unique) 
     */
#define ID_COMPARATOR(x, y)         ((x.id > y.id ? 1:(x.id < y.id ? -1: 0)))    
#define PID_COMPARATOR(x, y)         ((x.pid > y.pid ? 1:(x.pid < y.pid ? -1:ID_COMPARATOR(x,y))))
#define UPID_COMPARATOR(x, y)         ((x.upid > y.upid ? 1:(x.upid < y.upid ? -1:PID_COMPARATOR(x,y))))
/* Note, the negated order in the scale comparison . this ensures descending sort */    
#define SCALE_UPID_COMPARATOR(x,y)    ((x.scale > y.scale ? -1:(x.scale < y.scale ? 1:UPID_COMPARATOR(x, y))) )
#define MULTIPLE_ARRAY_EXCHANGER(type,a,i,j) {                          \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(struct halo_data, forest,i,j); \
        SGLIB_ARRAY_ELEMENTS_EXCHANGER(struct additional_info, info, i, j) \
            }
    

    /* I am using heap sort rather than qsort because the forest is already somewhat sorted. For sorted data,
       qsort behaves closer to O(N^2). 
     */
    SGLIB_ARRAY_HEAP_SORT(struct additional_info, info, totnhalos, SCALE_UPID_COMPARATOR, MULTIPLE_ARRAY_EXCHANGER);
    
    /* Fix subs of subs first */
    int64_t FirstHaloInFOFgroup=-1;
    int64_t fof_id=-1;
    for(int64_t i=0;i<totnhalos;i++) {
        /* fprintf(stderr,ANSI_COLOR_RED"FirstHaloInFOFgroup = %"PRId64 ANSI_COLOR_RESET"\n",FirstHaloInFOFgroup); */
        const int snapnum = forest[i].SnapNum;
        XASSERT(snapnum >= 0 && snapnum < nsnapshots, EXIT_FAILURE, 
                "snapnum = %d is outside range [0, %d)\n",
                snapnum, nsnapshots);
        scales[snapnum] = info[i].scale;
        end_scale[snapnum] = i;
        if(start_scale[snapnum] == -1) {
            start_scale[snapnum] = i;
        }
        
        if(info[i].pid == -1) {
            XASSERT(i < INT_MAX, EXIT_FAILURE, 
                    "Assigning to integer i = %"PRId64" is more than %d\n",
                    i, INT_MAX);
            forest[i].FirstHaloInFOFgroup = (int) i;
            forest[i].NextHaloInFOFgroup = -1;
            FirstHaloInFOFgroup = i;
            fof_id = info[i].id;
            continue;
        } else { 
            if(FirstHaloInFOFgroup == -1) {
                fprintf(stderr,"About to crash\n");
                for(int64_t k=0;k<totnhalos;k++) {
                    fprintf(stderr,"%03d %12.5lf %10"PRId64" %10"PRId64" %10"PRId64" %12.4g\n",
                            forest[k].SnapNum, info[k].scale, info[k].upid, info[k].pid, info[k].id, forest[k].Mvir);
                }
            }

            XASSERT(FirstHaloInFOFgroup != -1, EXIT_FAILURE, 
                    "Processing subhalos i=%"PRId64" but have not encountered FOF yet..bug\n"
                    "id = %"PRId64" pid = %"PRId64" upid = %"PRId64" snapnum = %d\n",
                    i,info[i].id, info[i].pid, info[i].upid, forest[i].SnapNum);
            
            if(info[i].upid == fof_id) {
                XASSERT(FirstHaloInFOFgroup < INT_MAX, EXIT_FAILURE, 
                        "Assigning FirstHaloInFOFgroup = %"PRId64". Must be less than %d\n",
                        FirstHaloInFOFgroup, INT_MAX);
                forest[i].FirstHaloInFOFgroup = FirstHaloInFOFgroup;
            } else {
                /* Should not reach here..I have already sorted the forest such that the FOF appears before the subs */
                fprintf(stderr,"ERROR: the sort did not place the FOF before the subs. BUG IN CTREES OR IN SORT\n");
                for(int64_t k=0;k<totnhalos;k++) {
                    fprintf(stderr,"%03d %12.5lf %10"PRId64" %10"PRId64" %10"PRId64" %12.4g\n",
                            forest[k].SnapNum, info[k].scale, info[k].upid, info[k].pid, info[k].id, forest[k].Mvir);
                }

                fprintf(stderr,"i = %"PRId64" id = %"PRId64" pid = %"PRId64" fof_id = %"PRId64" upid = %"PRId64" FirstHaloInFOFgroup = %"PRId64"\n",
                        i, info[i].id, info[i].pid, fof_id, info[i].upid, FirstHaloInFOFgroup); 
                exit(EXIT_FAILURE);
            }
            int64_t insertion_point = FirstHaloInFOFgroup;
            while(forest[insertion_point].NextHaloInFOFgroup != -1) {
                const int32_t nexthalo = forest[insertion_point].NextHaloInFOFgroup;
                XASSERT(nexthalo >=0 && nexthalo < totnhalos, EXIT_FAILURE, 
                        "Inserting next halo in FOF group into invalid index. nexthalo = %d totnhalos = %"PRId64"\n",
                        nexthalo, totnhalos);

                insertion_point = nexthalo;
            }
            XASSERT(i < INT_MAX, EXIT_FAILURE, 
                    "Assigning FirstHaloInFOFgroup = %"PRId64". Must be less than %d\n",
                    i, INT_MAX);

            forest[insertion_point].NextHaloInFOFgroup = i;
        }
    }

    /* Now figure out merger tree pointers. Need to set descendant, firstprogenitor and nextprogenitor. */
    for(int64_t i=0;i<totnhalos;i++) {
        if(info[i].descid == -1) {
            forest[i].Descendant = -1;
            continue;
        }

        int desc_snapnum = nsnapshots-1;
        const double desc_scale = info[i].desc_scale;
        const int64_t descid = info[i].descid;
        const double max_epsilon_scale = 1.0e-4;
        while((desc_snapnum >= 0) &&
              (fabs(scales[desc_snapnum] - desc_scale) > max_epsilon_scale) ) {
            desc_snapnum--;
        }
        XASSERT(desc_snapnum >= 0 && desc_snapnum < nsnapshots && (fabs(scales[desc_snapnum] - desc_scale) <= 1e-4),
                EXIT_FAILURE, 
                "Could not locate desc_snapnum. desc_snapnum = %d nsnapshots = %d \n",
                desc_snapnum, nsnapshots);

        /*start_scale and end_scale are inclusive. Hence the stopping condition is "<=" rather than simply "<" */
        int64_t desc_loc = start_scale[desc_snapnum];
        while(desc_loc >= start_scale[desc_snapnum] && desc_loc <= end_scale[desc_snapnum] && info[desc_loc].id != descid) {
            desc_loc++;
        }
        XASSERT(desc_loc >= start_scale[desc_snapnum] && desc_loc <= end_scale[desc_snapnum],
                EXIT_FAILURE, 
                "Desc loc = %"PRId64" for snapnum = %d is outside range [%"PRId64", %"PRId64"]\n",
                desc_loc, desc_snapnum, start_scale[desc_snapnum], end_scale[desc_snapnum]);
        XASSERT(info[desc_loc].id == descid,
                EXIT_FAILURE, 
                "Should have found descendant id = %"PRId64" but info[%"PRId64"]=%"PRId64" instead \n",
                descid, desc_loc, info[desc_loc].id);

        XASSERT(desc_loc < INT_MAX, EXIT_FAILURE, 
                "desc_loc = %"PRId64" must be less than INT_MAX = %d\n",
                desc_loc, INT_MAX);
        
        forest[i].Descendant = desc_loc;
        
        //Now assign first progenitor + next progenitor 
        if(forest[desc_loc].FirstProgenitor == -1) {
            forest[desc_loc].FirstProgenitor = i;
            forest[i].NextProgenitor = -1;

        } else {
            /* The descendant halo already has progenitors. Figure out the correct
               order -- should this halo be FirstProgenitor?
               Not necessary but ensure nextprog are ordered by mass.
            */
            const int first_prog = forest[desc_loc].FirstProgenitor;
            XASSERT(first_prog >= 0 && first_prog < totnhalos, EXIT_FAILURE, 
                    "first_prog=%d must lie within [0, %"PRId64"\n",
                    first_prog, totnhalos);
            if(forest[first_prog].Mvir < forest[i].Mvir) {
                XASSERT(i < INT_MAX, EXIT_FAILURE, 
                        "Assigning Nextprogenitor = %"PRId64" to an int will result in garbage. INT_MAX = %d\n",
                        i, INT_MAX);
                forest[desc_loc].FirstProgenitor = i;
                forest[i].NextProgenitor = first_prog;
            } else {
                int64_t insertion_point = first_prog;
                while(forest[insertion_point].NextProgenitor != -1) {
                    const int64_t next_prog = forest[insertion_point].NextProgenitor;
                    XASSERT(next_prog >=0 && next_prog < totnhalos, EXIT_FAILURE, 
                            "Inserting next progenitor into invalid index. insertion_point = %"PRId64" totnhalos = %"PRId64"\n",
                            next_prog, totnhalos);
                    
                    /* if(forest[next_prog].Mvir < forest[i].Mvir) { */
                    /*     forest[i].NextProgenitor = next_prog; */
                    /*     break; */
                    /* } */
                    insertion_point = next_prog;
                }
                XASSERT(i < INT_MAX, EXIT_FAILURE, 
                        "Assigning Nextprogenitor = %"PRId64" to an int will result in garbage. INT_MAX = %d\n",
                        i, INT_MAX);
                forest[insertion_point].NextProgenitor = i;
            }
        }
    }

    free(scales);
    free(start_scale);
    free(end_scale);
    
#undef MULTIPLE_ARRAY_EXCHANGER
#undef SCALE_UPID_COMPARATOR
#undef UPID_COMPARATOR
#undef PID_COMPARATOR    
#undef ID_COMPARATOR
    
    return;
}


int64_t find_fof_halo(const int64_t totnhalos, const struct additional_info *info, const int start_loc, const int64_t upid, int verbose, int64_t calldepth)
{
    XASSERT(totnhalos < INT_MAX, EXIT_FAILURE, 
            "Totnhalos must be less than %d. Otherwise indexing with int (start_loc) will break\n", INT_MAX);
    int64_t loc = -1;
    if(info[start_loc].pid == -1) {
        return start_loc;/* should never be called to find the FOF of a FOF halo -> but whatever*/
    }

    const int64_t max_recursion_depth = 30, recursion_depth_for_verbose = 3;
    if(calldepth >= recursion_depth_for_verbose) {
        verbose = 1;
    }
    XASSERT(calldepth <= max_recursion_depth, EXIT_FAILURE,
            "%s has been recursively called %"PRId64" times already. Likely caught in infinite loop..exiting\n",
            __FUNCTION__, calldepth);

    while(start_loc >= 0 && start_loc < totnhalos && info[start_loc].pid != -1) {
        if(verbose == 1) {
            fprintf(stderr,"start_loc = %d id = %"PRId64" pid = %"PRId64"\n", start_loc, info[start_loc].id, info[start_loc].pid);
            fprintf(stderr,"scale = %lf pid = %"PRId64" upid = %"PRId64"\n",
                    info[start_loc].scale, info[start_loc].pid, info[start_loc].upid);
        }
        if(upid > info[start_loc].id) {
            for(int64_t k=start_loc+1;k<totnhalos;k++) {
                if(info[k].id == upid) {
                    loc = k;
                    break;
                }
            }
        } else {
            for(int64_t k=start_loc-1;k>=0;k--) {
                if(info[k].id == upid) {
                    loc = k;
                    break;
                }
            }
        }

        if( loc < 0 || loc >= totnhalos) {
            return -1;
        }
        if(info[loc].pid == -1) {
            return loc;
        } else {
            if(verbose == 1) {
                fprintf(stderr,"calling find_fof_halo again with loc =%"PRId64" (int) loc = %d start_loc was =%d\n",loc, (int) loc, start_loc);
                fprintf(stderr,"scale = %lf id = %"PRId64" pid = %"PRId64" upid = %"PRId64" calldepth=%"PRId64"\n",
                        info[loc].scale, info[loc].id, info[loc].pid, info[loc].upid,calldepth);
            }
            calldepth++;
            return find_fof_halo(totnhalos, info, (int) loc, info[loc].upid, verbose, calldepth);
        }
    }

    return -1;
}


