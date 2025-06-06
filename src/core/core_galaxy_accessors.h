#pragma once

/**
 * @file core_galaxy_accessors.h
 * @brief Core-only galaxy property accessor functions
 * 
 * This file contains accessor functions for core properties only.
 * Physics properties should be accessed through the generic property system
 * in core_property_utils.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "core_allvars.h"

/* Core-only property accessors - no physics properties allowed here */

/* Position accessors */
extern float galaxy_get_pos_x(const struct GALAXY *galaxy);
extern float galaxy_get_pos_y(const struct GALAXY *galaxy);
extern float galaxy_get_pos_z(const struct GALAXY *galaxy);

/* Velocity accessors */
extern float galaxy_get_vel_x(const struct GALAXY *galaxy);
extern float galaxy_get_vel_y(const struct GALAXY *galaxy);
extern float galaxy_get_vel_z(const struct GALAXY *galaxy);

/* Core identification & merger properties */
extern int32_t galaxy_get_snapshot_number(const struct GALAXY *galaxy);
extern int32_t galaxy_get_type(const struct GALAXY *galaxy);
extern int32_t galaxy_get_halo_nr(const struct GALAXY *galaxy);
extern int32_t galaxy_get_central_gal(const struct GALAXY *galaxy);
extern long long galaxy_get_most_bound_id(const struct GALAXY *galaxy);
extern uint64_t galaxy_get_galaxy_index(const struct GALAXY *galaxy);

/* Core halo properties */
extern float galaxy_get_mvir(const struct GALAXY *galaxy);
extern float galaxy_get_rvir(const struct GALAXY *galaxy);
extern float galaxy_get_vvir(const struct GALAXY *galaxy);
extern float galaxy_get_vmax(const struct GALAXY *galaxy);

/* Setter functions for core properties */
extern void galaxy_set_pos_x(struct GALAXY *galaxy, float value);
extern void galaxy_set_pos_y(struct GALAXY *galaxy, float value);
extern void galaxy_set_pos_z(struct GALAXY *galaxy, float value);

extern void galaxy_set_vel_x(struct GALAXY *galaxy, float value);
extern void galaxy_set_vel_y(struct GALAXY *galaxy, float value);
extern void galaxy_set_vel_z(struct GALAXY *galaxy, float value);

extern void galaxy_set_snapshot_number(struct GALAXY *galaxy, int32_t value);
extern void galaxy_set_type(struct GALAXY *galaxy, int32_t value);

#ifdef __cplusplus
}
#endif