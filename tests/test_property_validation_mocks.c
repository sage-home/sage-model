#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/core/core_allvars.h"
#include "../src/core/core_properties.h"
#include "../src/core/core_property_utils.h"

/**
 * @file test_property_validation_mocks.c
 * @brief Mock functions for property access validation tests
 */

// Define property not found constant
#define PROP_NOT_FOUND ((property_id_t)-1)

// Logging mock
void log_message(int level, const char *format, ...) {
    // Just a stub implementation for testing
    (void)level;  // Unused parameter
    (void)format; // Unused parameter
}

// Memory management mocks
int allocate_galaxy_properties(struct GALAXY *g, const struct params *params) {
    (void)params; // Suppress unused parameter warning
    
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

// Property system mocks - simplified for testing
property_id_t get_property_id(const char *name) {
    // This is a simplified mock that doesn't actually look up properties
    // In a real implementation, this would search a property registry
    
    // For testing, return valid IDs for a few common properties
    if (strcmp(name, "HotGas") == 0) return 1;
    if (strcmp(name, "ColdGas") == 0) return 2;
    if (strcmp(name, "StellarMass") == 0) return 3;
    if (strcmp(name, "MetalsHotGas") == 0) return 4;
    if (strcmp(name, "Mvir") == 0) return 5;
    
    return PROP_NOT_FOUND;
}

property_id_t get_cached_property_id(const char *name) {
    return get_property_id(name);
}

float get_float_property(const struct GALAXY *galaxy, property_id_t prop_id, float default_value) {
    if (!galaxy || !galaxy->properties) return default_value;
    
    // This is a simplified mock that just accesses a few known properties
    // In a real implementation, this would use a property registry and typed accessors
    
    switch (prop_id) {
        case 1: // HotGas
            return galaxy->properties->HotGas;
        case 2: // ColdGas
            return galaxy->properties->ColdGas;
        case 3: // StellarMass
            return galaxy->properties->StellarMass;
        case 4: // MetalsHotGas
            return galaxy->properties->MetalsHotGas;
        default:
            return default_value;
    }
}

int set_float_property(struct GALAXY *galaxy, property_id_t prop_id, float value) {
    if (!galaxy || !galaxy->properties) return -1;
    
    // This is a simplified mock that just accesses a few known properties
    
    switch (prop_id) {
        case 1: // HotGas
            galaxy->properties->HotGas = value;
            break;
        case 2: // ColdGas
            galaxy->properties->ColdGas = value;
            break;
        case 3: // StellarMass
            galaxy->properties->StellarMass = value;
            break;
        case 4: // MetalsHotGas
            galaxy->properties->MetalsHotGas = value;
            break;
        default:
            return -1;
    }
    
    return 0;
}

double get_double_property(const struct GALAXY *galaxy, property_id_t prop_id, double default_value) {
    // Similar to get_float_property but returns a double
    if (!galaxy || !galaxy->properties) return default_value;
    
    switch (prop_id) {
        case 5: // Mvir
            return galaxy->properties->Mvir;
        default:
            return default_value;
    }
}

int set_double_property(struct GALAXY *galaxy, property_id_t prop_id, double value) {
    // Similar to set_float_property but for double properties
    if (!galaxy || !galaxy->properties) return -1;
    
    switch (prop_id) {
        case 5: // Mvir
            galaxy->properties->Mvir = value;
            return 0;
        default:
            return -1;
    }
}

bool has_property(const struct GALAXY *galaxy, property_id_t prop_id) {
    if (!galaxy || !galaxy->properties) return false;
    
    // This is a simplified mock that just checks a few known properties
    switch (prop_id) {
        case 1: // HotGas
        case 2: // ColdGas
        case 3: // StellarMass
        case 4: // MetalsHotGas
        case 5: // Mvir
            return true;
        default:
            return false;
    }
}
