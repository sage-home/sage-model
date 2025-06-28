#ifndef TREE_TRAVERSAL_H
#define TREE_TRAVERSAL_H

#include "tree_context.h"

// Optional callback for tracking traversal order
typedef void (*traversal_callback_t)(int halo_nr, void* user_data);

// Main tree processing
int process_tree_recursive(int halo_nr, TreeContext* ctx);

// Tree processing with tracking callback for testing
int process_tree_recursive_with_tracking(int halo_nr, TreeContext* ctx, 
                                       traversal_callback_t callback, void* user_data);

// Entry point for forest
int process_forest_trees(TreeContext* ctx);

#endif