#include "tree_fof.h"
#include "tree_galaxies.h"
#include "tree_traversal.h"
#include "tree_physics.h"
#include "core_logging.h"

bool is_fof_ready(int fof_root, TreeContext* ctx) {
    // Check all halos in FOF have processed their progenitors
    int current = fof_root;
    
    while (current >= 0) {
        // Check all progenitors of this halo
        int prog = ctx->halos[current].FirstProgenitor;
        while (prog >= 0) {
            if (!ctx->halo_done[prog]) {
                return false;  // Not ready yet
            }
            prog = ctx->halos[prog].NextProgenitor;
        }
        
        current = ctx->halos[current].NextHaloInFOFgroup;
    }
    
    return true;
}

int process_tree_fof_group(int fof_root, TreeContext* ctx) {
    LOG_DEBUG("Processing FOF group %d", fof_root);
    
    // First, ensure all FOF members' progenitors are processed
    int current = fof_root;
    while (current >= 0) {
        int prog = ctx->halos[current].FirstProgenitor;
        while (prog >= 0) {
            if (!ctx->halo_done[prog]) {
                int status = process_tree_recursive(prog, ctx);
                if (status != EXIT_SUCCESS) return status;
            }
            prog = ctx->halos[prog].NextProgenitor;
        }
        current = ctx->halos[current].NextHaloInFOFgroup;
    }
    
    // Now collect galaxies for all halos in FOF
    current = fof_root;
    while (current >= 0) {
        int status = collect_halo_galaxies(current, ctx);
        if (status != EXIT_SUCCESS) return status;
        
        status = inherit_galaxies_with_orphans(current, ctx);
        if (status != EXIT_SUCCESS) return status;
        
        current = ctx->halos[current].NextHaloInFOFgroup;
    }
    
    // Mark FOF as processed
    ctx->fof_done[fof_root] = true;
    
    // Apply physics to the collected FOF galaxies
    return apply_physics_to_fof(fof_root, ctx);
}