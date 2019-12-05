#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    struct locations_with_forests {
        int64_t forestid;
        int64_t treeid;
        int64_t offset;/* byte offset in the file where the tree data begin (i.e., the next line after "#tree TREE_ROOT_ID\n" */
        int32_t fileid;
        int32_t unused;/* unused but here for alignment */
    };

    struct filenames_and_fd {
        int *fd;/* file descriptor for each file output by CTrees*/
        int32_t numfiles;/* total number of unique `tree_*_*_*.dat` files */
        uint32_t nallocated;/* number of elements allocated for the first `int *fd` field */
        uint64_t *numtrees_per_file;/* number of trees present in each of the `tree_*_*_*.dat` files */
    };

    struct additional_info{
        int64_t id;
        int64_t pid;
        int64_t upid;
        double desc_scale;
        int64_t descid;
        double scale;
    };

    /* externally exposed functions */
    extern int64_t read_forests(const char *filename, int64_t **forestids, int64_t **tree_rootids);
    extern int64_t read_locations(const char *filename, const int64_t ntrees, struct locations_with_forests *l, struct filenames_and_fd *filenames_and_fd);
    extern int assign_forest_ids(const int64_t ntrees, struct locations_with_forests *locations, int64_t *forests, int64_t *tree_roots);
    extern void sort_locations_on_fid_file_offset(const int64_t ntrees, struct locations_with_forests *locations);
    extern int fix_flybys(const int64_t totnhalos, struct halo_data *forest, struct additional_info *info, int verbose);
    extern int fix_upid(const int64_t totnhalos, struct halo_data *forest, struct additional_info *info, const int verbose);
    extern int assign_mergertree_indices(const int64_t totnhalos, struct halo_data *forest, struct additional_info *info, const int max_snapnum);

#ifdef __cplusplus
}
#endif
