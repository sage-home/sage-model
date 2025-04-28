#include "core_galaxy_accessors.h"
#include "../physics/standard_physics_properties.h"
#include "core_logging.h"

// Default to direct access initially
int use_extension_properties = 0;

// (All accessors are currently inlined in the header for performance and clarity.)
// Add LOG_DEBUG/LOG_ERROR here if you later need runtime diagnostics for accessor switching.
