#include "tree_traversal.h"
#include "tree_fof.h"
#include "core_logging.h"

int process_tree_recursive(int halo_nr, TreeContext* ctx) {
    return process_tree_recursive_with_tracking(halo_nr, ctx, NULL, NULL);
}

int process_tree_recursive_with_tracking(int halo_nr, TreeContext* ctx, 
                                       traversal_callback_t callback, void* user_data) {
    struct halo_data* halo = &ctx->halos[halo_nr];
    
    // Already processed?
    if (ctx->halo_done[halo_nr]) {
        return EXIT_SUCCESS;
    }
    
    // STEP 1: Process all progenitors first (depth-first)
    int prog = halo->FirstProgenitor;
    while (prog >= 0) {
        int status = process_tree_recursive_with_tracking(prog, ctx, callback, user_data);
        if (status != EXIT_SUCCESS) return status;
        
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    // Mark as done AFTER processing progenitors
    ctx->halo_done[halo_nr] = true;
    
    // Track this halo (after processing progenitors - correct order)
    if (callback) {
        callback(halo_nr, user_data);
    }
    
    // STEP 2: Check if we need to process FOF group
    int fof_root = halo->FirstHaloInFOFgroup;
    if (halo_nr == fof_root && !ctx->fof_done[fof_root]) {
        if (is_fof_ready(fof_root, ctx)) {
            int status = process_tree_fof_group(fof_root, ctx);
            if (status != EXIT_SUCCESS) return status;
        }
    }
    
    return EXIT_SUCCESS;
}

int process_forest_trees(TreeContext* ctx) {
    // Find roots (halos with no descendants)
    for (int64_t i = 0; i < ctx->nhalos; i++) {
        if (ctx->halos[i].Descendant == -1) {
            // LOG_INFO("Processing tree from root halo %ld", i);
            int status = process_tree_recursive(i, ctx);
            if (status != EXIT_SUCCESS) return status;
        }
    }
    
    // Process any disconnected sub-trees (like legacy SAGE)
    for (int64_t i = 0; i < ctx->nhalos; i++) {
        if (!ctx->halo_done[i]) {
            // LOG_INFO("Processing disconnected sub-tree from halo %ld", i);
            int status = process_tree_recursive(i, ctx);
            if (status != EXIT_SUCCESS) return status;
        }
    }
    
    return EXIT_SUCCESS;
}