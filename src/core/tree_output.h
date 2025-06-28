#ifndef TREE_OUTPUT_H
#define TREE_OUTPUT_H

#include "tree_context.h"

// Output galaxies organized by snapshot
int output_tree_galaxies(TreeContext* ctx, int64_t forestnr,
                        struct save_info* save_info,
                        struct forest_info* forest_info);

#endif