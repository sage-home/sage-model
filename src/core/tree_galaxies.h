#ifndef TREE_GALAXIES_H
#define TREE_GALAXIES_H

#include "tree_context.h"

// Process galaxies for a single halo
int collect_halo_galaxies(int halo_nr, TreeContext* ctx);

// Inherit galaxies from progenitors with orphan handling
int inherit_galaxies_with_orphans(int halo_nr, TreeContext* ctx);

// Check for gaps in the tree
int measure_tree_gap(int descendant_snap, int progenitor_snap);

// Update galaxy properties for new halo assignment
void update_galaxy_for_new_halo(struct GALAXY* galaxy, int halo_nr, TreeContext* ctx);

#endif