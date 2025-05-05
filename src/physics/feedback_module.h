#pragma once

#include "../core/core_allvars.h"
// No longer need to define accessors here, they are in core_galaxy_accessors.h

// Add any specific declarations needed *only* for the feedback module itself,
// or for users of the feedback module *interface* (if different from base_module).
// For now, this file might be quite minimal or even empty if all necessary
// declarations are handled elsewhere (like core_galaxy_accessors.h for the
// standard property accessors).

// If the feedback module had its own specific interface structure extending
// base_module, it would be defined here. For example:
/*
struct feedback_module_interface {
    struct base_module base;
    // Feedback-specific function pointers, if any
    // e.g., void (*calculate_ejection_fraction)(...);
};
*/

// If the feedback module needs specific data structures passed around,
// they could be defined here.