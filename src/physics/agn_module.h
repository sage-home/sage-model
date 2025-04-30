#pragma once

#include "../../core/core_allvars.h"

// Stubs for AGN-related accessors (to be implemented fully in a future phase)

static inline double galaxy_get_quasar_accretion(const struct GALAXY *galaxy) {
    return galaxy->QuasarModeBHaccretionMass;
}

static inline void galaxy_set_quasar_accretion(struct GALAXY *galaxy, double value) {
    galaxy->QuasarModeBHaccretionMass = value;
}
