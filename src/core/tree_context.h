#ifndef TREE_CONTEXT_H
#define TREE_CONTEXT_H

#include "core_allvars.h"
#include "galaxy_array.h"

typedef struct TreeContext {
    // Core data
    struct halo_data* halos;
    int64_t nhalos;
    struct params* run_params;
    
    // Modern galaxy management
    GalaxyArray* working_galaxies;    // Temporary processing
    GalaxyArray* output_galaxies;     // Final output
    
    // Processing flags (clean version of legacy approach)
    bool* halo_done;                  // Halo has been processed
    bool* fof_done;                   // FOF group has been evolved
    
    // Galaxy-halo mapping
    int* halo_galaxy_count;           // Number of galaxies per halo
    int* halo_first_galaxy;           // Index of first galaxy
    
    // State
    int32_t galaxy_counter;           // Global galaxy ID
    
    // Diagnostics
    int total_orphans;
    int total_gaps_spanned;
    int max_gap_length;
    
} TreeContext;

// Lifecycle
TreeContext* tree_context_create(struct halo_data* halos, int64_t nhalos, 
                                struct params* run_params);
void tree_context_destroy(TreeContext** ctx);

// Statistics
void tree_context_report_stats(const TreeContext* ctx);

#endif