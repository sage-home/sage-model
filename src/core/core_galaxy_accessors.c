#include "core_galaxy_accessors.h"
#include "core_properties.h"

/* Core-only property accessors implementation */

/* Position accessor implementations */
float galaxy_get_pos_x(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Pos_ELEM(galaxy, 0);
}

float galaxy_get_pos_y(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Pos_ELEM(galaxy, 1);
}

float galaxy_get_pos_z(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Pos_ELEM(galaxy, 2);
}

/* Velocity accessor implementations */
float galaxy_get_vel_x(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Vel_ELEM(galaxy, 0);
}

float galaxy_get_vel_y(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Vel_ELEM(galaxy, 1);
}

float galaxy_get_vel_z(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Vel_ELEM(galaxy, 2);
}

/* Core identification & merger properties */
int32_t galaxy_get_snapshot_number(const struct GALAXY *galaxy) {
    return GALAXY_PROP_SnapNum(galaxy);
}

int32_t galaxy_get_type(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Type(galaxy);
}

int32_t galaxy_get_halo_nr(const struct GALAXY *galaxy) {
    return GALAXY_PROP_HaloNr(galaxy);
}

int32_t galaxy_get_central_gal(const struct GALAXY *galaxy) {
    return GALAXY_PROP_CentralGal(galaxy);
}

long long galaxy_get_most_bound_id(const struct GALAXY *galaxy) {
    return GALAXY_PROP_MostBoundID(galaxy);
}

uint64_t galaxy_get_galaxy_index(const struct GALAXY *galaxy) {
    return GALAXY_PROP_GalaxyIndex(galaxy);
}


/* Core halo properties */
float galaxy_get_mvir(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Mvir(galaxy);
}

float galaxy_get_rvir(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Rvir(galaxy);
}

float galaxy_get_vvir(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Vvir(galaxy);
}

float galaxy_get_vmax(const struct GALAXY *galaxy) {
    return GALAXY_PROP_Vmax(galaxy);
}

/* Setter functions for core properties */
void galaxy_set_pos_x(struct GALAXY *galaxy, float value) {
    GALAXY_PROP_Pos_ELEM(galaxy, 0) = value;
}

void galaxy_set_pos_y(struct GALAXY *galaxy, float value) {
    GALAXY_PROP_Pos_ELEM(galaxy, 1) = value;
}

void galaxy_set_pos_z(struct GALAXY *galaxy, float value) {
    GALAXY_PROP_Pos_ELEM(galaxy, 2) = value;
}

void galaxy_set_vel_x(struct GALAXY *galaxy, float value) {
    GALAXY_PROP_Vel_ELEM(galaxy, 0) = value;
}

void galaxy_set_vel_y(struct GALAXY *galaxy, float value) {
    GALAXY_PROP_Vel_ELEM(galaxy, 1) = value;
}

void galaxy_set_vel_z(struct GALAXY *galaxy, float value) {
    GALAXY_PROP_Vel_ELEM(galaxy, 2) = value;
}

void galaxy_set_snapshot_number(struct GALAXY *galaxy, int32_t value) {
    GALAXY_PROP_SnapNum(galaxy) = value;
}

void galaxy_set_type(struct GALAXY *galaxy, int32_t value) {
    GALAXY_PROP_Type(galaxy) = value;
}

