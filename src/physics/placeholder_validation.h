/**
 * @file placeholder_validation.h
 * @brief Placeholder stubs for physics validation in core-only mode
 *
 * This file provides placeholder stub function declarations for physics validation 
 * functions that aren't defined in the core-only mode. These ensure that validation
 * works correctly when building with core-only features.
 */

#ifndef PLACEHOLDER_VALIDATION_H
#define PLACEHOLDER_VALIDATION_H

struct validation_context;
struct GALAXY;

/* Function declarations for core-only implementation */
int placeholder_validate_galaxy_values(struct validation_context *ctx, 
                                      const struct GALAXY *galaxy,
                                      int index,
                                      const char *component);

int placeholder_validate_galaxy_consistency(struct validation_context *ctx,
                                          const struct GALAXY *galaxy,
                                          int index,
                                          const char *component);

#endif /* PLACEHOLDER_VALIDATION_H */