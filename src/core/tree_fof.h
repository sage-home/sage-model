#ifndef TREE_FOF_H
#define TREE_FOF_H

#include "tree_context.h"

// Process entire FOF group after all members are ready
int process_tree_fof_group(int fof_root, TreeContext* ctx);

// Check if all FOF members have processed progenitors
bool is_fof_ready(int fof_root, TreeContext* ctx);

#endif