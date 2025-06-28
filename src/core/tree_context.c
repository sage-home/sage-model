#include "tree_context.h"
#include "core_mymalloc.h"
#include "core_logging.h"

TreeContext* tree_context_create(struct halo_data* halos, int64_t nhalos, 
                                struct params* run_params) {
    TreeContext* ctx = mycalloc(1, sizeof(TreeContext));
    if (!ctx) {
        LOG_ERROR("Failed to allocate tree context");
        return NULL;
    }
    
    ctx->halos = halos;
    ctx->nhalos = nhalos;
    ctx->run_params = run_params;
    ctx->galaxy_counter = 0;
    
    // Modern galaxy storage
    ctx->working_galaxies = galaxy_array_new();
    ctx->output_galaxies = galaxy_array_new();
    
    if (!ctx->working_galaxies || !ctx->output_galaxies) {
        tree_context_destroy(&ctx);
        return NULL;
    }
    
    // Processing flags
    ctx->halo_done = mycalloc(nhalos, sizeof(bool));
    ctx->fof_done = mycalloc(nhalos, sizeof(bool));
    
    // Galaxy mapping
    ctx->halo_galaxy_count = mycalloc(nhalos, sizeof(int));
    ctx->halo_first_galaxy = mymalloc(nhalos * sizeof(int));
    
    if (!ctx->halo_done || !ctx->fof_done || !ctx->halo_galaxy_count || !ctx->halo_first_galaxy) {
        tree_context_destroy(&ctx);
        return NULL;
    }
    
    // Initialize mapping
    for (int64_t i = 0; i < nhalos; i++) {
        ctx->halo_first_galaxy[i] = -1;  // No galaxies yet
    }
    
    return ctx;
}

void tree_context_destroy(TreeContext** ctx_ptr) {
    if (!ctx_ptr || !*ctx_ptr) return;
    
    TreeContext* ctx = *ctx_ptr;
    
    // Free modern galaxy arrays
    if (ctx->working_galaxies) galaxy_array_free(&ctx->working_galaxies);
    if (ctx->output_galaxies) galaxy_array_free(&ctx->output_galaxies);
    
    // Free arrays
    if (ctx->halo_done) myfree(ctx->halo_done);
    if (ctx->fof_done) myfree(ctx->fof_done);
    if (ctx->halo_galaxy_count) myfree(ctx->halo_galaxy_count);
    if (ctx->halo_first_galaxy) myfree(ctx->halo_first_galaxy);
    
    myfree(ctx);
    *ctx_ptr = NULL;
}

void tree_context_report_stats(const TreeContext* ctx) {
    LOG_INFO("Tree Processing Statistics:");
    LOG_INFO("  Total galaxies created: %d", galaxy_array_get_count(ctx->output_galaxies));
    LOG_INFO("  Orphans handled: %d", ctx->total_orphans);
    LOG_INFO("  Gaps spanned: %d (max length: %d)", 
             ctx->total_gaps_spanned, ctx->max_gap_length);
}