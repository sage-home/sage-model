#include <math.h>
#include "core_allvars.h"
#include "model_h2_formation.h"
#include "model_misc.h"

// Minimum surface density for efficient H2 formation (Msun/pc^2 in internal units)
static const float MIN_SURFACE_DENSITY = 10.0;
// Minimum pressure threshold for H2 formation (dimensionless)
static const float MIN_PRESSURE_NORM = 1e-3;

// Calculate molecular fraction based on mid-plane pressure and environmental factors
float calculate_H2_fraction(const float surface_density, const float metallicity, 
    const float DiskRadius, const struct params *run_params) 
{
    const float disk_area = M_PI * DiskRadius * DiskRadius;
    if (disk_area <= 0.0 || surface_density <= 0.0) {
        return 0.0;
    }

    // Calculate midplane pressure
    const float P_mid = M_PI * run_params->G * surface_density * surface_density;
    const float P_0_internal = 5.93e-12 / run_params->UnitPressure_in_cgs;
    const float P_norm = P_mid / P_0_internal;

#ifdef VERBOSE
    static int counter = 0;
    if (counter % 500000 == 0) {
        printf("Disk area: %g, Surface density: %g, Metallicity: %g\n", disk_area, surface_density, metallicity);
        printf("Internal pressure: %g, P_norm: %g\n", P_mid, P_norm);
    }
    counter++;
#endif

    if (P_norm < MIN_PRESSURE_NORM) {
        return 0.0;
    }

    // Adjusted molecular fraction formula (reducing HI → H₂ conversion in high-pressure regions)
    float f_H2 = 1.0 / (1.0 + pow(P_norm, -0.92 * run_params->H2FractionExponent));

    // Surface density scaling (unchanged)
    if (surface_density < MIN_SURFACE_DENSITY) {
        f_H2 *= surface_density / MIN_SURFACE_DENSITY;
    }

    // Adjusted metallicity dependence
    if (metallicity > 0.0) {
        if (metallicity < 0.1) {
            f_H2 *= pow(metallicity / 0.1, 0.3);  // Shallower scaling at low Z
        } else {
            f_H2 *= pow(metallicity, 0.2);
        }
    }

    // Apply calibrated normalization factor
    f_H2 *= run_params->H2FractionFactor;

    // Ensure fraction stays within bounds
    if (f_H2 < 0.0) f_H2 = 0.0;
    if (f_H2 > 1.0) f_H2 = 1.0;

#ifdef VERBOSE
    if (counter % 500000 == 0) {
        printf("Final f_H2: %g\n", f_H2);
    }
#endif

    return f_H2;
}


void update_gas_components(struct GALAXY *g, const struct params *run_params)
{
    // Don't update if no cold gas
    if(g->ColdGas <= 0.0) {
        g->H2_gas = 0.0;
        g->HI_gas = 0.0;
#ifdef VERBOSE
        static int counter2 = 0;  // Persistent counter across function calls
        if (counter2 % 500000 == 0) {
            printf("No cold gas: ColdGas=%g, setting H2=0, HI=0\n", g->ColdGas);
        }
        counter2++;
#endif
        return;
    }

    // Calculate surface density using disk scale radius
    const float disk_area = M_PI * g->DiskScaleRadius * g->DiskScaleRadius;
    if(disk_area <= 0.0) {
        g->H2_gas = 0.0;
        g->HI_gas = g->ColdGas;  // All cold gas is atomic if no valid disk
#ifdef VERBOSE
        static int counter3 = 0;  // Persistent counter across function calls
        if (counter3 % 500000 == 0) {
            printf("Invalid disk area: DiskScaleRadius=%g, setting all cold gas to HI\n", 
                   g->DiskScaleRadius);
        }
        counter3++;
#endif
        return;
    }
    
    const float surface_density = g->ColdGas / disk_area;
    
    // Get metallicity relative to solar (Zsun = 0.02)
    float metallicity = 0.0;
    if(g->ColdGas > 0.0) {
        metallicity = g->MetalsColdGas / g->ColdGas / 0.02;  // Normalized to solar metallicity
    }
    
    // Calculate H2 fraction with all environmental effects
    const float f_H2 = calculate_H2_fraction(surface_density, metallicity, 
                                           g->DiskScaleRadius, run_params);
    
    // Update gas components
    g->H2_gas = f_H2 * g->ColdGas;
    if(g->H2_gas > g->ColdGas) {
        g->H2_gas = g->ColdGas;
        g->HI_gas = 0.0;
    } else {
        g->HI_gas = g->ColdGas - g->H2_gas;
    }

#ifdef VERBOSE
    static int counter4 = 0;  // Persistent counter across function calls
    if (counter4 % 500000 == 0) {
        printf("Gas components update:\n");
        printf("  ColdGas=%g\n", g->ColdGas);
        printf("  Surface density=%g\n", surface_density);
        printf("  Metallicity=%g\n", metallicity);
        printf("  f_H2=%g\n", f_H2);
        printf("  H2=%g\n", g->H2_gas);
        printf("  HI=%g\n", g->HI_gas);
    }
    counter4++;
#endif

    // Final sanity checks
    if(g->H2_gas < 0.0) g->H2_gas = 0.0;
    if(g->HI_gas < 0.0) g->HI_gas = 0.0;
    
    // Check mass conservation with small numerical tolerance
    if(g->H2_gas + g->HI_gas > g->ColdGas * 1.001) {  
        float total = g->H2_gas + g->HI_gas;
        float scale = g->ColdGas / total;
        g->H2_gas *= scale;
        g->HI_gas *= scale;
    }
}

void init_gas_components(struct GALAXY *g)
{
    g->H2_gas = 0.0;
    g->HI_gas = 0.0;
}