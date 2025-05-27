// model_lowmass_suppression.c
#include <math.h>
#include "core_allvars.h"
#include "model_lowmass_suppression.h"

double calculate_lowmass_suppression(const int gal, const double redshift, struct GALAXY *galaxies, const struct params *run_params)
{
    // If feature is disabled, return 1.0 (no suppression)
    if (run_params->LowMassHighzSuppressionOn != 1) {
        return 1.0;
    }
    
    // No effect below redshift 2
    if (redshift <= 1.0) {
        return 1.0;
    }
    
    // Get critical threshold mass from parameters
    const double critical_mass = run_params->SuppressionMassThreshold;
    
    // No effect for galaxies above the threshold
    if (galaxies[gal].Mvir > critical_mass) {
        return 1.0;  // No suppression
    }
    
    // Calculate mass ratio (ranges from 0 to 1)
    double mass_ratio = galaxies[gal].Mvir / critical_mass;
    
    // Apply power law with configurable exponent for mass dependence
    double base_suppression = pow(mass_ratio, run_params->SuppressionMassExponent);
    
    // Apply redshift scaling - stronger effect at higher redshift
    double redshift_factor = 1.0;
    if (redshift > 1.0) {
        // Use configurable exponent for redshift dependence
        // Default is 1.0 (linear scaling with z-2)
        redshift_factor = pow(0.5, (redshift - 2.0) * run_params->SuppressionRedshiftExp);
    }
    
    // Combine factors
    double total_suppression = base_suppression * redshift_factor;
    
    // Ensure minimum value
    if (total_suppression < 0.01) {
        total_suppression = 0.01;  // Floor at 1% 
    }
    
    return total_suppression;
}