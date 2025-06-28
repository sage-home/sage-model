# SAGE Modern Tree-Based Processing: Implementation Plan

**Document Version**: 3.0  
**Date**: July 1, 2025  
**Author**: SAGE Architecture Team  
**Status**: Implementation Ready

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Critical Motivation: Scientific Accuracy](#2-critical-motivation-scientific-accuracy)
3. [Current State Analysis](#3-current-state-analysis)
4. [Target Architecture Design](#4-target-architecture-design)
5. [Implementation Strategy](#5-implementation-strategy)
6. [Phase 1: Tree Infrastructure Foundation](#6-phase-1-tree-infrastructure-foundation)
7. [Phase 2: Galaxy Collection and Inheritance](#7-phase-2-galaxy-collection-and-inheritance)
8. [Phase 3: FOF Group Processing](#8-phase-3-fof-group-processing)
9. [Phase 4: Physics Pipeline Integration](#9-phase-4-physics-pipeline-integration)
10. [Phase 5: Output System Integration](#10-phase-5-output-system-integration)
11. [Phase 6: Validation and Optimization](#11-phase-6-validation-and-optimization)
12. [Risk Management](#12-risk-management)
13. [Success Metrics](#13-success-metrics)

---

## 1. Executive Summary

This document presents a comprehensive implementation plan to transition SAGE from snapshot-based processing to modern tree-based processing. The primary driver is **scientific accuracy**: the current snapshot-based approach loses galaxies when halos temporarily disappear from merger trees, violating mass conservation and producing incorrect results.

### Key Scientific Issues Addressed
- **Missing Orphans**: Galaxies lost when host halos are disrupted between snapshots
- **Snapshot Gaps**: Galaxies lost when halos temporarily disappear for multiple snapshots
- **Mass Conservation**: Total galaxy mass must be preserved throughout evolution

### Implementation Approach
- **Modern Architecture**: Preserve all improvements (GalaxyArray, property system, pipeline)
- **Proven Algorithm**: Follow Legacy SAGE's tree traversal for scientific correctness
- **Clean Implementation**: Use modern coding practices, not legacy patterns
- **Incremental Development**: Six phases with stable checkpoints

### Timeline
- **Total Duration**: 6 weeks
- **Risk Level**: Low (proven algorithm, careful implementation)

---

## 2. Critical Motivation: Scientific Accuracy

### 2.1 The Fundamental Problem

Snapshot-based processing creates **scientific inaccuracies** due to the nature of merger trees:

```
Problem 1: Disrupted Halos
Snapshot N:   Halo A (contains Galaxy X) → Halo B
Snapshot N+1: Halo B exists, but Halo A is disrupted
Result:       Galaxy X is LOST (violates mass conservation)

Problem 2: Temporary Gaps
Snapshot N:   Halo A (contains Galaxy Y)
Snapshot N+1: Halo A missing from tree (numerical issues)
Snapshot N+2: Halo A reappears
Result:       Galaxy Y is LOST during the gap
```

### 2.2 Why Tree-Based Processing Solves This

Tree-based (depth-first) processing naturally handles these cases:

```
Tree Processing:
1. Start at z=0 (root)
2. Recursively process ALL progenitors
3. Galaxy inherits through the tree structure
4. Gaps are irrelevant - we follow the connections
5. Orphans created naturally when progenitor has no descendant
```

**Key Insight**: The merger tree already encodes the complete history. By following it depth-first, we never lose galaxies regardless of gaps or disruptions.

### 2.3 Scientific Requirements

1. **Mass Conservation**: No galaxies lost due to processing artifacts
2. **Orphan Handling**: Correct identification when host halos disappear
3. **Gap Tolerance**: Process correctly regardless of snapshot coverage
4. **Physical Consistency**: Same physics results as proven Legacy SAGE

---

## 3. Current State Analysis

### 3.1 Architectural Strengths to Preserve

**Memory Management**:
- `GalaxyArray`: Safe, bounds-checked dynamic arrays
- Property system: All data access through `GALAXY_PROP_*` macros
- Deep copying: Safe galaxy duplication with `deep_copy_galaxy()`
- Memory pools: Tree-scoped allocation with proper cleanup

**Physics Pipeline**:
- Four-phase execution: HALO → GALAXY → POST → FINAL
- Modular physics: Modules plug into pipeline phases
- Clean separation: Core infrastructure knows nothing about physics

**Modern Patterns**:
- Comprehensive error handling with context
- Structured logging with levels
- Clean interfaces and separation of concerns
- No global state or static variables

### 3.2 What Must Change

**Processing Flow**:
```c
// Current: Snapshot-based with complex tracking
for (snapshot = 0; snapshot < max; snapshot++) {
    process_fof_groups_at_snapshot();
    track_processed_flags();  // Complex!
    identify_orphans();       // Forward-looking
}

// Target: Tree-based with natural flow
for (each tree root) {
    recursively_process_tree();
    // Orphans handled naturally
    // Gaps irrelevant
}
```

### 3.3 Legacy Code as Scientific Reference

We examine Legacy SAGE for:
- ✅ Correct tree traversal order
- ✅ FOF group processing logic
- ✅ Galaxy initialization patterns
- ✅ Orphan identification

We do NOT adopt:
- ❌ Raw malloc/realloc patterns
- ❌ Global variables
- ❌ Monolithic functions
- ❌ Poor error handling

---

## 4. Target Architecture Design

### 4.1 Core Design Principles

1. **Scientific Correctness First**: Match Legacy SAGE's proven results
2. **Modern Implementation**: Use all architectural improvements
3. **Natural Data Flow**: Algorithm matches data structure
4. **Minimal Disruption**: Reuse existing components where possible

### 4.2 Processing Architecture

```
Tree-Based Processing Flow:
1. Load merger tree (unchanged)
2. Initialize tree context (modern memory management)
3. Process each tree recursively:
   a. Depth-first traversal
   b. Natural galaxy inheritance
   c. FOF group assembly
   d. Physics application via pipeline
4. Output by snapshot (unchanged format)
```

### 4.3 Key Data Structures

```c
// Modern tree processing context
typedef struct TreeContext {
    // Core data
    struct halo_data* halos;
    int64_t nhalos;
    struct params* run_params;
    
    // Galaxy management (modern approach)
    GalaxyArray* working_galaxies;    // Galaxies being processed
    GalaxyArray* output_galaxies;     // Final galaxies for output
    
    // FOF tracking (inspired by legacy but cleaner)
    bool* halo_done_flag;             // Halo processed
    bool* fof_done_flag;              // FOF group processed
    
    // State
    int32_t galaxy_counter;
    
    // Statistics
    int orphans_created;
    int gaps_handled;
} TreeContext;
```

### 4.4 Critical Algorithm Flow

```c
// Correct tree processing order (based on Legacy SAGE)
process_tree(root_halo) {
    // 1. Process this halo's progenitors first
    process_progenitors(halo);
    
    // 2. Check if FOF group is ready
    if (is_fof_root(halo) && !fof_processed(halo)) {
        // 3. Process ALL halos in FOF group
        process_fof_group_progenitors(fof_root);
        
        // 4. Collect galaxies for entire FOF
        collect_fof_galaxies(fof_root);
        
        // 5. Evolve FOF galaxies together
        evolve_fof_galaxies(fof_root);
    }
}
```

---

## 5. Implementation Strategy

### 5.1 Phased Approach

| Phase | Week | Description | Deliverable |
|-------|------|-------------|-------------|
| 1 | 1 | Tree infrastructure foundation | Basic traversal working |
| 2 | 1-2 | Galaxy collection and inheritance | Correct galaxy accumulation |
| 3 | 2-3 | FOF group processing | Proper FOF handling |
| 4 | 3-4 | Physics pipeline integration | Physics correctly applied |
| 5 | 4-5 | Output system integration | HDF5 files match legacy |
| 6 | 5-6 | Validation and optimization | Production ready |

### 5.2 Development Principles

1. **Incremental**: Each phase produces runnable code
2. **Testable**: Unit tests for each component
3. **Comparable**: Output can be compared to snapshot mode
4. **Reversible**: Easy to switch between modes

### 5.3 File Organization

```
src/core/
├── tree_context.h/c          # Modern context management
├── tree_traversal.h/c        # Recursive tree processing
├── tree_galaxies.h/c         # Galaxy inheritance logic
├── tree_fof.h/c              # FOF group handling
├── tree_physics.h/c          # Pipeline integration
├── tree_output.h/c           # Output organization
└── sage_tree_mode.h/c        # Main entry point
```

---

## 6. Phase 1: Tree Infrastructure Foundation - COMPLETED

**Duration**: Week 1  
**Goal**: Establish modern tree processing infrastructure

### 6.1 Tree Context Management

**File**: `src/core/tree_context.h`
```c
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
```

**File**: `src/core/tree_context.c`
```c
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
```

### 6.2 Tree Traversal Logic

**File**: `src/core/tree_traversal.h`
```c
#ifndef TREE_TRAVERSAL_H
#define TREE_TRAVERSAL_H

#include "tree_context.h"

// Main tree processing
int process_tree_recursive(int halo_nr, TreeContext* ctx);

// Entry point for forest
int process_forest_trees(TreeContext* ctx);

#endif
```

**File**: `src/core/tree_traversal.c`
```c
#include "tree_traversal.h"
#include "core_logging.h"

int process_tree_recursive(int halo_nr, TreeContext* ctx) {
    struct halo_data* halo = &ctx->halos[halo_nr];
    
    // Already processed?
    if (ctx->halo_done[halo_nr]) {
        return EXIT_SUCCESS;
    }
    
    // Mark as done
    ctx->halo_done[halo_nr] = true;
    
    // STEP 1: Process all progenitors first (depth-first)
    int prog = halo->FirstProgenitor;
    while (prog >= 0) {
        int status = process_tree_recursive(prog, ctx);
        if (status != EXIT_SUCCESS) return status;
        
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    // STEP 2: Check if we need to process FOF group
    int fof_root = halo->FirstHaloInFOFgroup;
    if (halo_nr == fof_root && !ctx->fof_done[fof_root]) {
        // This will be implemented in Phase 3
        LOG_DEBUG("Ready to process FOF group %d", fof_root);
        ctx->fof_done[fof_root] = true;
    }
    
    return EXIT_SUCCESS;
}

int process_forest_trees(TreeContext* ctx) {
    // Find roots (halos with no descendants)
    int roots_found = 0;
    
    for (int64_t i = 0; i < ctx->nhalos; i++) {
        if (ctx->halos[i].Descendant == -1) {
            LOG_INFO("Processing tree from root halo %ld", i);
            int status = process_tree_recursive(i, ctx);
            if (status != EXIT_SUCCESS) return status;
            roots_found++;
        }
    }
    
    // Process any disconnected sub-trees (like legacy SAGE)
    for (int64_t i = 0; i < ctx->nhalos; i++) {
        if (!ctx->halo_done[i]) {
            LOG_INFO("Processing disconnected sub-tree from halo %ld", i);
            int status = process_tree_recursive(i, ctx);
            if (status != EXIT_SUCCESS) return status;
        }
    }
    
    LOG_INFO("Processed %d root trees", roots_found);
    return EXIT_SUCCESS;
}
```

### 6.3 Phase 1 Tests

**File**: `tests/test_tree_infrastructure.c`
```c
void test_tree_traversal_order(void) {
    printf("Testing tree traversal order...\n");
    
    // Create simple tree
    struct halo_data halos[4];
    halos[0].Descendant = -1;      // Root
    halos[0].FirstProgenitor = 1;
    halos[1].Descendant = 0;
    halos[1].FirstProgenitor = 2;
    halos[1].NextProgenitor = 3;
    halos[2].Descendant = 1;       // Leaf
    halos[2].FirstProgenitor = -1;
    halos[3].Descendant = 1;       // Leaf
    halos[3].FirstProgenitor = -1;
    
    // Process
    TreeContext* ctx = tree_context_create(halos, 4, &params);
    assert(process_forest_trees(ctx) == EXIT_SUCCESS);
    
    // Verify all processed
    for (int i = 0; i < 4; i++) {
        assert(ctx->halo_done[i] == true);
    }
    
    tree_context_destroy(&ctx);
    printf("✓ Tree traversal test passed\n");
}
```

### 6.4 Phase 1 Checkpoint
```bash
make clean && make
make test_tree_infrastructure && ./test_tree_infrastructure
./sage ./input/millennium.par  # Should still work
```


---

## 7. Phase 2: Galaxy Collection and Inheritance - COMPLETED

**Duration**: Week 1-2  
**Goal**: Implement galaxy inheritance that handles gaps and orphans naturally

### 7.1 Galaxy Inheritance Logic

**File**: `src/core/tree_galaxies.h`
```c
#ifndef TREE_GALAXIES_H
#define TREE_GALAXIES_H

#include "tree_context.h"

// Process galaxies for a single halo
int collect_halo_galaxies(int halo_nr, TreeContext* ctx);

// Check for gaps in the tree
int measure_tree_gap(int descendant_snap, int progenitor_snap);

#endif
```

**File**: `src/core/tree_galaxies.c`
```c
#include "tree_galaxies.h"
#include "core_build_model.h"
#include "core_logging.h"

// Import from core_build_model.c
extern void init_galaxy(const int p, const int halonr, int *galaxycounter, 
                       struct halo_data *halos, struct GALAXY *galaxies, 
                       struct params *run_params);

int measure_tree_gap(int descendant_snap, int progenitor_snap) {
    // Gaps occur when snapshots are not consecutive
    int gap = descendant_snap - progenitor_snap - 1;
    return (gap > 0) ? gap : 0;
}

int collect_halo_galaxies(int halo_nr, TreeContext* ctx) {
    struct halo_data* halo = &ctx->halos[halo_nr];
    
    // Count galaxies from all progenitors
    int total_prog_galaxies = 0;
    int prog = halo->FirstProgenitor;
    
    while (prog >= 0) {
        total_prog_galaxies += ctx->halo_galaxy_count[prog];
        
        // Check for gaps
        int gap = measure_tree_gap(halo->SnapNum, ctx->halos[prog].SnapNum);
        if (gap > 0) {
            ctx->total_gaps_spanned++;
            if (gap > ctx->max_gap_length) {
                ctx->max_gap_length = gap;
            }
            LOG_DEBUG("Spanning gap of %d snapshots: %d -> %d", 
                     gap, ctx->halos[prog].SnapNum, halo->SnapNum);
        }
        
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    if (total_prog_galaxies == 0 && halo->FirstProgenitor == -1) {
        // No progenitors - create primordial galaxy
        if (halo_nr == halo->FirstHaloInFOFgroup) {
            struct GALAXY new_galaxy;
            memset(&new_galaxy, 0, sizeof(struct GALAXY));
            
            init_galaxy(0, halo_nr, &ctx->galaxy_counter, ctx->halos, 
                       &new_galaxy, ctx->run_params);
            
            // Store in working array
            int idx = galaxy_array_append(ctx->working_galaxies, &new_galaxy, 
                                        ctx->run_params);
            
            // Update mapping
            ctx->halo_first_galaxy[halo_nr] = idx;
            ctx->halo_galaxy_count[halo_nr] = 1;
            
            free_galaxy_properties(&new_galaxy);
            
            LOG_DEBUG("Created primordial galaxy for halo %d", halo_nr);
        }
    }
    
    return EXIT_SUCCESS;
}
```

### 7.2 Orphan Handling

**Key Innovation**: Orphans are created naturally when a progenitor halo has galaxies but isn't the "first occupied" progenitor. This handles disrupted halos automatically.

```c
// Add to tree_galaxies.c
int inherit_galaxies_with_orphans(int halo_nr, TreeContext* ctx) {
    struct halo_data* halo = &ctx->halos[halo_nr];
    
    // Find most massive progenitor with galaxies (first_occupied)
    int first_occupied = -1;
    int max_len = 0;
    
    int prog = halo->FirstProgenitor;
    while (prog >= 0) {
        if (ctx->halo_galaxy_count[prog] > 0) {
            if (ctx->halos[prog].Len > max_len) {
                max_len = ctx->halos[prog].Len;
                first_occupied = prog;
            }
        }
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    if (first_occupied == -1) {
        return EXIT_SUCCESS;  // No galaxies to inherit
    }
    
    // Process all progenitors
    prog = halo->FirstProgenitor;
    while (prog >= 0) {
        if (ctx->halo_galaxy_count[prog] > 0) {
            // Get galaxies from this progenitor
            int start_idx = ctx->halo_first_galaxy[prog];
            
            for (int i = 0; i < ctx->halo_galaxy_count[prog]; i++) {
                struct GALAXY* src = galaxy_array_get(ctx->working_galaxies, 
                                                     start_idx + i);
                
                struct GALAXY inherited;
                deep_copy_galaxy(&inherited, src, ctx->run_params);
                
                if (prog == first_occupied) {
                    // Main branch - update properties
                    update_galaxy_for_new_halo(&inherited, halo_nr, ctx);
                } else {
                    // Other branches - create orphans
                    GALAXY_PROP_Type(&inherited) = 2;  // Orphan
                    GALAXY_PROP_Mvir(&inherited) = 0.0;
                    ctx->total_orphans++;
                    LOG_DEBUG("Created orphan from disrupted halo %d", prog);
                }
                
                // Add to working array
                galaxy_array_append(ctx->working_galaxies, &inherited, 
                                  ctx->run_params);
                free_galaxy_properties(&inherited);
            }
        }
        prog = ctx->halos[prog].NextProgenitor;
    }
    
    return EXIT_SUCCESS;
}
```

### 7.3 Phase 2 Tests

Read the testing documents, integrate into the Makefile, write tests to the highest coding standards.

```c
void test_orphan_creation(void) {
    printf("Testing orphan creation...\n");
    
    // Setup: Halo A has two progenitors, smaller one is disrupted
    // This naturally creates an orphan
    
    // ... test implementation ...
    
    assert(ctx->total_orphans == 1);
    printf("✓ Orphan creation test passed\n");
}

void test_gap_handling(void) {
    printf("Testing gap handling...\n");
    
    // Setup: Halo exists at snap 10 and snap 15 (gap of 4)
    // Verify galaxy survives the gap
    
    // ... test implementation ...
    
    assert(ctx->total_gaps_spanned == 1);
    assert(ctx->max_gap_length == 4);
    printf("✓ Gap handling test passed\n");
}
```

### 7.4 Phase 2 Checkpoint
```bash
make clean && make test_orphan_creation && ./tests/test_orphan_creation
make clean && make test_gap_handling && ./tests/test_gap_handling
make clean && make && ./sage ./input/millennium.par  # Should still work
make clean && make unit_tests  # Should still work
```

---

## 8. Phase 3: FOF Group Processing - COMPLETED

**Duration**: Week 2-3  
**Goal**: Correctly handle FOF groups following Legacy SAGE logic

### 8.1 FOF Processing Logic

**File**: `src/core/tree_fof.h`
```c
#ifndef TREE_FOF_H
#define TREE_FOF_H

#include "tree_context.h"

// Process entire FOF group after all members are ready
int process_fof_group(int fof_root, TreeContext* ctx);

// Check if all FOF members have processed progenitors
bool is_fof_ready(int fof_root, TreeContext* ctx);

#endif
```

**File**: `src/core/tree_fof.c`
```c
#include "tree_fof.h"
#include "tree_galaxies.h"
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

int process_fof_group(int fof_root, TreeContext* ctx) {
    LOG_DEBUG("Processing FOF group %d", fof_root);
    
    // First, ensure all FOF members' progenitors are processed
    int current = fof_root;
    while (current >= 0) {
        int prog = ctx->halos[current].FirstProgenitor;
        while (prog >= 0) {
            if (!ctx->halo_done[prog]) {
                process_tree_recursive(prog, ctx);
            }
            prog = ctx->halos[prog].NextProgenitor;
        }
        current = ctx->halos[current].NextHaloInFOFgroup;
    }
    
    // Now collect galaxies for all halos in FOF
    current = fof_root;
    while (current >= 0) {
        collect_halo_galaxies(current, ctx);
        inherit_galaxies_with_orphans(current, ctx);
        current = ctx->halos[current].NextHaloInFOFgroup;
    }
    
    // Mark FOF as processed
    ctx->fof_done[fof_root] = true;
    
    // Physics will be applied in Phase 4
    
    return EXIT_SUCCESS;
}
```

### 8.2 Update Tree Traversal

```c
// Update tree_traversal.c process_tree_recursive():
// Replace placeholder Step 2 with:

    // STEP 2: Check if we need to process FOF group
    int fof_root = halo->FirstHaloInFOFgroup;
    if (halo_nr == fof_root && !ctx->fof_done[fof_root]) {
        if (is_fof_ready(fof_root, ctx)) {
            int status = process_fof_group(fof_root, ctx);
            if (status != EXIT_SUCCESS) return status;
        }
    }
```

### 8.3 Phase 3 Checkpoint
```bash
make clean && make && ./sage ./input/millennium.par  # Should still work
make clean && make unit_tests  # Should still work
```

---

## 9. Phase 4: Physics Pipeline Integration - COMPLETED

**Duration**: Week 3-4  
**Goal**: Apply physics using the existing pipeline system

### 9.1 Physics Integration

**File**: `src/core/tree_physics.h`
```c
#ifndef TREE_PHYSICS_H
#define TREE_PHYSICS_H

#include "tree_context.h"

// Apply physics to FOF group using existing pipeline
int apply_physics_to_fof(int fof_root, TreeContext* ctx);

#endif
```

**File**: `src/core/tree_physics.c`
```c
#include "tree_physics.h"
#include "core_build_model.h"

// Reuse existing evolve_galaxies with minimal adaptation
extern int evolve_galaxies(const int fof_root_halonr, 
                          GalaxyArray* temp_fof_galaxies, 
                          int *numgals, 
                          struct halo_data *halos, 
                          struct halo_aux_data *haloaux,
                          GalaxyArray *galaxies_this_snap, 
                          struct params *run_params);

int apply_physics_to_fof(int fof_root, TreeContext* ctx) {
    // Collect FOF galaxies into temporary array
    GalaxyArray* fof_galaxies = galaxy_array_new();
    
    int current = fof_root;
    while (current >= 0) {
        int start = ctx->halo_first_galaxy[current];
        int count = ctx->halo_galaxy_count[current];
        
        for (int i = 0; i < count; i++) {
            struct GALAXY* gal = galaxy_array_get(ctx->working_galaxies, 
                                                start + i);
            galaxy_array_append(fof_galaxies, gal, ctx->run_params);
        }
        
        current = ctx->halos[current].NextHaloInFOFgroup;
    }
    
    // Apply physics
    int numgals = 0;
    struct halo_aux_data* temp_aux = mycalloc(ctx->nhalos, 
                                            sizeof(struct halo_aux_data));
    
    int status = evolve_galaxies(fof_root, fof_galaxies, &numgals,
                               ctx->halos, temp_aux, 
                               ctx->output_galaxies,  // Direct to output!
                               ctx->run_params);
    
    // Cleanup
    galaxy_array_free(&fof_galaxies);
    myfree(temp_aux);
    
    return status;
}
```

### 9.2 Integration Point

```c
// Update tree_fof.c process_fof_group() to add at end:
    
    // Apply physics to the collected FOF galaxies
    return apply_physics_to_fof(fof_root, ctx);
```

### 9.3 Phase 4 Checkpoint
```bash
make clean && make && ./sage ./input/millennium.par  # Should still work
make clean && make unit_tests  # Should still work
```

---

## 10. Phase 5: Output System Integration

**Duration**: Week 4-5  
**Goal**: Output galaxies maintaining snapshot organization

### 10.1 Output Organization

**File**: `src/core/tree_output.h`
```c
#ifndef TREE_OUTPUT_H
#define TREE_OUTPUT_H

#include "tree_context.h"

// Output galaxies organized by snapshot
int output_tree_galaxies(TreeContext* ctx, int64_t forestnr,
                        struct save_info* save_info,
                        struct forest_info* forest_info);

#endif
```

**File**: `src/core/tree_output.c`
```c
#include "tree_output.h"
#include "core_save.h"

int output_tree_galaxies(TreeContext* ctx, int64_t forestnr,
                        struct save_info* save_info,
                        struct forest_info* forest_info) {
    
    // Group galaxies by snapshot
    int ngal_total = galaxy_array_get_count(ctx->output_galaxies);
    
    for (int snap = 0; snap < ctx->run_params->simulation.SimMaxSnaps; snap++) {
        // Count galaxies at this snapshot
        int snap_count = 0;
        for (int i = 0; i < ngal_total; i++) {
            struct GALAXY* gal = galaxy_array_get(ctx->output_galaxies, i);
            if (GALAXY_PROP_SnapNum(gal) == snap) {
                snap_count++;
            }
        }
        
        if (snap_count == 0) continue;
        
        // Allocate and collect
        struct GALAXY* snap_galaxies = mymalloc(snap_count * sizeof(struct GALAXY));
        struct halo_aux_data* haloaux = mycalloc(ctx->nhalos, 
                                               sizeof(struct halo_aux_data));
        
        int idx = 0;
        for (int i = 0; i < ngal_total; i++) {
            struct GALAXY* gal = galaxy_array_get(ctx->output_galaxies, i);
            if (GALAXY_PROP_SnapNum(gal) == snap) {
                deep_copy_galaxy(&snap_galaxies[idx], gal, ctx->run_params);
                
                // Update haloaux
                int halo_nr = GALAXY_PROP_HaloNr(&snap_galaxies[idx]);
                if (haloaux[halo_nr].NGalaxies == 0) {
                    haloaux[halo_nr].FirstGalaxy = idx;
                }
                haloaux[halo_nr].NGalaxies++;
                
                idx++;
            }
        }
        
        // Save using existing infrastructure
        save_galaxies(forestnr, snap_count, ctx->halos, forest_info,
                     haloaux, snap_galaxies, save_info, ctx->run_params);
        
        // Cleanup
        for (int i = 0; i < snap_count; i++) {
            free_galaxy_properties(&snap_galaxies[i]);
        }
        myfree(snap_galaxies);
        myfree(haloaux);
    }
    
    return EXIT_SUCCESS;
}
```

### 10.2 Main Entry Point

**File**: `src/core/sage_tree_mode.h`
```c
int sage_process_forest_tree_mode(int64_t forestnr, 
                                 struct save_info* save_info,
                                 struct forest_info* forest_info,
                                 struct params* run_params);
```

**File**: `src/core/sage_tree_mode.c`
```c
int sage_process_forest_tree_mode(int64_t forestnr, 
                                 struct save_info* save_info,
                                 struct forest_info* forest_info,
                                 struct params* run_params) {
    
    begin_tree_memory_scope();
    
    // Load forest
    struct halo_data* Halo = NULL;
    const int64_t nhalos = load_forest(run_params, forestnr, &Halo, forest_info);
    if (nhalos < 0) {
        end_tree_memory_scope();
        return nhalos;
    }
    
    // Create context
    TreeContext* ctx = tree_context_create(Halo, nhalos, run_params);
    if (!ctx) {
        myfree(Halo);
        end_tree_memory_scope();
        return EXIT_FAILURE;
    }
    
    // Process all trees
    int status = process_forest_trees(ctx);
    
    if (status == EXIT_SUCCESS) {
        // Report statistics
        tree_context_report_stats(ctx);
        
        // Output galaxies
        status = output_tree_galaxies(ctx, forestnr, save_info, forest_info);
    }
    
    // Cleanup
    tree_context_destroy(&ctx);
    myfree(Halo);
    end_tree_memory_scope();
    
    return status;
}
```

### 10.3 Integration with sage.c

```c
// Add parameter for processing mode
enum ProcessingMode {
    PROCESSING_MODE_SNAPSHOT,
    PROCESSING_MODE_TREE
};

// In sage_per_forest():
if (run_params->ProcessingMode == PROCESSING_MODE_TREE) {
    return sage_process_forest_tree_mode(forestnr, save_info, 
                                       forest_info, run_params);
}
// ... existing snapshot code ...
```

### 10.4 Phase 5 Checkpoint
```bash
make clean && make && ./sage ./input/millennium.par  # Should still work
make clean && make unit_tests  # Should still work
```

---

## 11. Phase 6: Validation and Optimization

**Duration**: Week 5-6  
**Goal**: Ensure scientific accuracy and optimize performance

### 11.1 Scientific Validation

**Critical Tests**:

1. **Mass Conservation**
```python
def test_mass_conservation(snapshot_file, tree_file):
    """Verify no mass is lost between modes"""
    snap_mass = calculate_total_mass(snapshot_file)
    tree_mass = calculate_total_mass(tree_file)
    
    assert abs(snap_mass - tree_mass) / snap_mass < 1e-6
    print(f"✓ Mass conservation: {snap_mass:.3e} = {tree_mass:.3e}")
```

2. **Orphan Validation**
```python
def test_orphan_handling(tree_file):
    """Verify orphans are correctly identified"""
    orphans = count_type_2_galaxies(tree_file)
    
    # Should have orphans from disrupted halos
    assert orphans > 0
    print(f"✓ Found {orphans} orphan galaxies")
```

3. **Gap Tolerance**
```python
def test_gap_handling(tree_file_with_gaps, tree_file_no_gaps):
    """Verify gaps don't affect results"""
    props1 = extract_galaxy_properties(tree_file_with_gaps)
    props2 = extract_galaxy_properties(tree_file_no_gaps)
    
    assert_properties_match(props1, props2, tolerance=1e-5)
    print("✓ Gap handling verified")
```

### 11.2 Phase 6 Checkpoint
```bash
make clean && make test_mass_conservation && ./tests/test_mass_conservation
make clean && make test_orphan_handling && ./tests/test_orphan_handling
make clean && make test_gap_handling && ./tests/test_gap_handling
make clean && make && ./sage ./input/millennium.par  # Should still work
make clean && make unit_tests  # Should still work
```

### 11.3 Performance Optimization

**Key Optimizations**:

1. **Memory Pooling**
```c
// Pre-allocate galaxy pool based on halo count
int estimated_galaxies = (int)(ctx->nhalos * 1.5);
galaxy_array_reserve(ctx->working_galaxies, estimated_galaxies);
```

2. **Batch Processing**
```c
// Process galaxies in batches for better cache usage
const int BATCH_SIZE = 1000;
for (int i = 0; i < ngal; i += BATCH_SIZE) {
    int batch_end = MIN(i + BATCH_SIZE, ngal);
    process_galaxy_batch(i, batch_end);
}
```

3. **Lazy Allocation**
```c
// Only allocate properties when needed
if (galaxy_needs_physics(galaxy)) {
    ensure_galaxy_properties(galaxy, run_params);
}
```

### 11.4 Final Validation Checklist

- [ ] Tree mode produces identical galaxy counts per snapshot
- [ ] All galaxy properties match within tolerance
- [ ] Orphans correctly identified and tracked
- [ ] Gaps handled without galaxy loss
- [ ] Memory usage acceptable (< 1.5x snapshot mode)
- [ ] Performance acceptable (< 1.2x snapshot mode)
- [ ] No memory leaks (valgrind clean)

Run final validation checks using new tree-based processing
```bash
make clean && make && ./sage ./input/millennium.par  # Should still work
make clean && make unit_tests  # Should still work
```

---

## 12. Risk Management

### 12.1 Technical Risks and Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Scientific differences | High | Medium | Extensive validation suite |
| Memory explosion | Medium | Low | Pooling and monitoring |
| Performance regression | Medium | Medium | Profile early and often |
| Integration complexity | Medium | Low | Incremental development |

### 12.2 Rollback Plan

1. **Dual Mode**: Both processing modes available via parameter
2. **Feature Flag**: Can disable tree mode in production
3. **Validation Period**: Run both modes in parallel
4. **Quick Switch**: Single parameter change to revert

### 12.3 Testing Strategy

```bash
# Regression test suite
./run_tree_tests.sh --compare-with-snapshot

# Performance tests
./benchmark_tree_mode.sh --profile

# Memory tests
valgrind --leak-check=full ./sage input/millennium_tree.par
```

---

## 13. Success Metrics

### 13.1 Scientific Success ✓
- **Mass Conservation**: 100% of galaxies preserved
- **Orphan Handling**: All disrupted halos create orphans
- **Gap Tolerance**: Results independent of snapshot coverage
- **Legacy Matching**: Results match Legacy SAGE within tolerance

### 13.2 Performance Success ✓
- **Memory Usage**: < 1.5x snapshot mode
- **Runtime**: < 1.2x snapshot mode
- **Scalability**: Handles billion-particle simulations
- **Efficiency**: Better cache usage than snapshot mode

### 13.3 Code Quality Success ✓
- **Modern Patterns**: Uses GalaxyArray, property system
- **Clean Architecture**: Clear separation of concerns
- **Error Handling**: Comprehensive with context
- **Maintainability**: Well-documented and tested

### 13.4 User Success ✓
- **Easy Migration**: Single parameter to switch modes
- **Backward Compatible**: Same output format
- **Documented**: Clear user guide
- **Validated**: Extensive test results published

---

## Conclusion

This implementation plan addresses the critical scientific issues with snapshot-based processing while preserving all architectural improvements from the Modern SAGE refactoring. By following the proven tree-based algorithm with modern implementation practices, we achieve:

1. **Scientific Accuracy**: No galaxies lost to gaps or disruptions
2. **Clean Implementation**: Modern memory management and error handling
3. **Maintainability**: Clear architecture with separated concerns
4. **Performance**: Efficient processing with bounded memory usage

The phased approach ensures stable development with validation at each step, minimizing risk while delivering a scientifically accurate and technically excellent solution.

**Next Steps**:
1. Review and approve plan
2. Set up development branch
3. Begin Phase 1 implementation
4. Schedule weekly progress reviews

---

*"The best of both worlds: proven science with modern engineering"*