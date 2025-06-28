#ifndef SAGE_TREE_MODE_H
#define SAGE_TREE_MODE_H

#include "core_allvars.h"

// Main entry point for tree-based forest processing
int sage_process_forest_tree_mode(int64_t forestnr, 
                                 struct save_info* save_info,
                                 struct forest_info* forest_info,
                                 struct params* run_params);

#endif