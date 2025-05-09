/**
 * @file placeholder_validation.c
 * @brief Placeholder stub implementations for physics validation in core-only mode
 *
 * This file provides empty implementations of physics validation functions
 * for use in core-only builds. These stubs always return success (0).
 */

#include <stdio.h>
#include <stdlib.h>
#include "../core/core_allvars.h"
#include "placeholder_validation.h"
#include "placeholder_model_misc.h"

/**
 * @brief Placeholder for galaxy value validation
 * 
 * Empty implementation that always returns success for core-only builds
 */
int placeholder_validate_galaxy_values(struct validation_context *ctx, 
                                      const struct GALAXY *galaxy,
                                      int index,
                                      const char *component) 
{
    /* Mark unused parameters */
    (void)ctx;
    (void)galaxy;
    (void)index;
    (void)component;
    
    /* In core-only mode, we don't validate physics properties */
    return 0;
}

/**
 * @brief Placeholder for galaxy consistency validation
 * 
 * Empty implementation that always returns success for core-only builds
 */
int placeholder_validate_galaxy_consistency(struct validation_context *ctx,
                                          const struct GALAXY *galaxy,
                                          int index,
                                          const char *component)
{
    /* Mark unused parameters */
    (void)ctx;
    (void)galaxy;
    (void)index;
    (void)component;
    
    /* In core-only mode, we don't validate physics consistency */
    return 0;
}

/* Additional placeholder functions required by core_build_model.c */
void init_galaxy(struct GALAXY *g, int p, int snap) {
    /* Mark unused parameter */
    (void)p;
    
    /* Basic initialization - just set zeros for physics fields */
    g->Type = 0;
    g->SnapNum = snap;
}

double estimate_merging_time(int halonr, struct halo_data *halos, const struct params *run_params) {
    /* Mark unused parameters */
    (void)halonr;
    (void)halos;
    (void)run_params;
    
    /* Return a placeholder value for merger time */
    return 999.9;
}

void deal_with_galaxy_merger(int p, int merger_centralgal, int centralgal, double time, 
                           int ngal, struct GALAXY *galaxy, struct params *run_params) {
    /* Mark unused parameters */
    (void)p;
    (void)merger_centralgal;
    (void)centralgal;
    (void)time;
    (void)ngal;
    (void)galaxy;
    (void)run_params;
    
    /* Empty implementation for core-only mode */
}

void disrupt_satellite_to_ICS(int gal, int centralgal, int ngal, struct GALAXY *galaxy) {
    /* Mark unused parameters */
    (void)gal;
    (void)centralgal;
    (void)ngal;
    (void)galaxy;
    
    /* Empty implementation for core-only mode */
}

void *cooling_module_create(void) {
    /* Return NULL for a non-functional placeholder */
    return NULL;
}

void *infall_module_create(void) {
    /* Return NULL for a non-functional placeholder */
    return NULL;
}
