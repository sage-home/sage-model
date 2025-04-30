#pragma once

#include "../../core/core_allvars.h"

// Stubs for feedback-related accessors (to be implemented fully in a future phase)

static inline double galaxy_get_outflow_rate(const struct GALAXY *galaxy) {
    return galaxy->OutflowRate;
}

static inline void galaxy_set_outflow_rate(struct GALAXY *galaxy, double value) {
    galaxy->OutflowRate = value;
}
