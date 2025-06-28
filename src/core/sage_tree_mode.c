#include "sage_tree_mode.h"
#include "tree_context.h"
#include "tree_traversal.h"
#include "tree_output.h"
#include "core_io_tree.h"
#include "core_mymalloc.h"
#include "core_logging.h"

// External function from core_io_tree.c
extern int64_t load_forest(struct params *run_params, int64_t forestnr, 
                          struct halo_data **Halo, struct forest_info *forest_info);

// Memory management functions
extern void begin_tree_memory_scope(void);
extern void end_tree_memory_scope(void);

int sage_process_forest_tree_mode(int64_t forestnr, 
                                 struct save_info* save_info,
                                 struct forest_info* forest_info,
                                 struct params* run_params) {
    
    if (!save_info || !forest_info || !run_params) {
        LOG_ERROR("Invalid parameters to sage_process_forest_tree_mode");
        return EXIT_FAILURE;
    }
    
    LOG_INFO("Processing forest %ld using tree-based mode", forestnr);
    
    // Begin tree-scoped memory management
    begin_tree_memory_scope();
    
    // Load forest halos
    struct halo_data* Halo = NULL;
    const int64_t nhalos = load_forest(run_params, forestnr, &Halo, forest_info);
    if (nhalos < 0) {
        LOG_ERROR("Failed to load forest %ld: %ld", forestnr, nhalos);
        end_tree_memory_scope();
        return nhalos;
    }
    
    if (nhalos == 0) {
        LOG_INFO("Forest %ld has no halos - skipping", forestnr);
        if (Halo) myfree(Halo);
        end_tree_memory_scope();
        return EXIT_SUCCESS;
    }
    
    LOG_DEBUG("Loaded %ld halos for forest %ld", nhalos, forestnr);
    
    // Create tree processing context
    TreeContext* ctx = tree_context_create(Halo, nhalos, run_params);
    if (!ctx) {
        LOG_ERROR("Failed to create tree context for forest %ld", forestnr);
        myfree(Halo);
        end_tree_memory_scope();
        return EXIT_FAILURE;
    }
    
    LOG_DEBUG("Created tree context for forest %ld", forestnr);
    
    // Process all trees in the forest using depth-first traversal
    int status = process_forest_trees(ctx);
    
    if (status == EXIT_SUCCESS) {
        LOG_DEBUG("Successfully processed all trees in forest %ld", forestnr);
        
        // Report processing statistics
        tree_context_report_stats(ctx);
        
        // Output galaxies organized by snapshot
        status = output_tree_galaxies(ctx, forestnr, save_info, forest_info);
        
        if (status == EXIT_SUCCESS) {
            LOG_INFO("Successfully completed tree-based processing for forest %ld", forestnr);
        } else {
            LOG_ERROR("Failed to output galaxies for forest %ld", forestnr);
        }
    } else {
        LOG_ERROR("Failed to process trees in forest %ld", forestnr);
    }
    
    // Cleanup
    tree_context_destroy(&ctx);
    myfree(Halo);
    end_tree_memory_scope();
    
    LOG_DEBUG("Cleaned up tree processing for forest %ld", forestnr);
    
    return status;
}