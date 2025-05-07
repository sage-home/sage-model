#include <stdio.h>
#include <stdlib.h>
#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/physics/cooling_module.h"
#include "../src/physics/infall_module.h"

// Mock functions for testing property equivalence
// These are simplified versions just for validation tests

// Logging mock
void log_message(int level, const char *format, ...) {
    // Just a stub implementation for testing
    (void)level;  // Unused parameter
    (void)format; // Unused parameter
}

// Memory management mocks
int allocate_galaxy_properties(struct GALAXY *g, const struct params *params) {
    galaxy_properties_t *props = malloc(sizeof(galaxy_properties_t));
    if (!props) return -1;
    
    memset(props, 0, sizeof(galaxy_properties_t));
    g->properties = props;
    return 0;
}

void free_galaxy_properties(struct GALAXY *g) {
    if (g && g->properties) {
        free(g->properties);
        g->properties = NULL;
    }
}

int copy_galaxy_properties(struct GALAXY *dest, const struct GALAXY *src, const struct params *params) {
    if (!dest || !src || !src->properties) return -1;
    
    // Allocate if needed
    if (!dest->properties) {
        int result = allocate_galaxy_properties(dest, params);
        if (result != 0) return result;
    }
    
    // Copy all fields
    memcpy(dest->properties, src->properties, sizeof(galaxy_properties_t));
    return 0;
}

// Cooling module mocks
double cooling_recipe(const int gal, const double dt, struct GALAXY *galaxies, 
                     const struct cooling_params_view *cooling_params) {
    // Simple mock that returns a fixed value and updates a property
    GALAXY_PROP_Cooling(&galaxies[gal]) += 0.1;
    return 0.1;
}

void cool_gas_onto_galaxy(const int centralgal, const double coolingGas, struct GALAXY *galaxies) {
    // Simple mock that moves gas between reservoirs
    double metallicity = 0.1; // Fixed test value
    
    if (coolingGas > 0.0) {
        GALAXY_PROP_ColdGas(&galaxies[centralgal]) += coolingGas;
        GALAXY_PROP_MetalsColdGas(&galaxies[centralgal]) += metallicity * coolingGas;
        GALAXY_PROP_HotGas(&galaxies[centralgal]) -= coolingGas;
        GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]) -= metallicity * coolingGas;
    }
}

// Infall module mocks
double infall_recipe(const int centralgal, const int ngal, const double Zcurr, 
                    struct GALAXY *galaxies, const struct params *run_params) {
    // Simple mock that returns a fixed value
    return 0.2;
}

void strip_from_satellite(const int centralgal, const int gal, const double Zcurr, 
                         struct GALAXY *galaxies, const struct params *run_params) {
    // Simple mock that strips gas
    if (GALAXY_PROP_HotGas(&galaxies[gal]) > 0.0) {
        double strippedGas = 0.05;
        double metallicity = 0.1;
        
        GALAXY_PROP_HotGas(&galaxies[gal]) -= strippedGas;
        GALAXY_PROP_MetalsHotGas(&galaxies[gal]) -= metallicity * strippedGas;
        
        GALAXY_PROP_HotGas(&galaxies[centralgal]) += strippedGas;
        GALAXY_PROP_MetalsHotGas(&galaxies[centralgal]) += metallicity * strippedGas;
    }
}

void add_infall_to_hot(const int gal, double infallingGas, struct GALAXY *galaxies) {
    // Simple mock that adds gas to hot component
    if (infallingGas > 0.0) {
        GALAXY_PROP_HotGas(&galaxies[gal]) += infallingGas;
    }
}

// Extension system mocks (if needed)
void *galaxy_extension_get_data(const struct GALAXY *galaxy, int prop_id) {
    // Just a stub
    (void)galaxy;
    (void)prop_id;
    return NULL;
}

int galaxy_extension_register(galaxy_property_t *property) {
    // Just a stub
    (void)property;
    return 0;
}
