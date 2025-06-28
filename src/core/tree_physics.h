#ifndef TREE_PHYSICS_H
#define TREE_PHYSICS_H

#include "tree_context.h"

/**
 * @file tree_physics.h
 * @brief Physics pipeline integration for tree-based processing
 * 
 * This module integrates the existing physics pipeline system with tree-based
 * galaxy evolution. It collects galaxies from FOF groups and applies physics
 * using the same modular pipeline system used in snapshot-based processing.
 */

/**
 * @brief Apply physics to FOF group using existing pipeline
 * 
 * Collects all galaxies from a FOF group after inheritance, applies the 
 * physics pipeline using evolve_galaxies(), and adds results to output.
 * 
 * @param fof_root Root halo index for the FOF group
 * @param ctx Tree processing context
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error
 */
int apply_physics_to_fof(int fof_root, TreeContext* ctx);

#endif