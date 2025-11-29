#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_disk_instability.h"
#include "model_misc.h"
#include "model_mergers.h"

void check_disk_instability(const int p, const int centralgal, const int halonr, const double time, const double dt, const int step,
                            struct GALAXY *galaxies, struct params *run_params)
{
    // Here we calculate the stability of the stellar and gaseous disk as discussed in Mo, Mao & White (1998).
    // For unstable stars and gas, we transfer the required ammount to the bulge to make the disk stable again

    // Disk mass has to be > 0.0
    const double diskmass = galaxies[p].ColdGas + (galaxies[p].StellarMass - galaxies[p].BulgeMass);
    if(diskmass > 0.0) {
        // calculate critical disk mass
        double Mcrit = galaxies[p].Vmax * galaxies[p].Vmax * (3.0 * galaxies[p].DiskScaleRadius) / run_params->G;
        if(Mcrit > diskmass) {
            Mcrit = diskmass;
        }

        // use disk mass here
        const double gas_fraction   = galaxies[p].ColdGas / diskmass;
        const double unstable_gas   = gas_fraction * (diskmass - Mcrit);
        const double star_fraction  = 1.0 - gas_fraction;
        const double unstable_stars = star_fraction * (diskmass - Mcrit);

        // add excess stars to the bulge
        if(unstable_stars > 0.0) {
            // Use disk metallicity here
            const double metallicity = get_metallicity(galaxies[p].StellarMass - galaxies[p].BulgeMass, galaxies[p].MetalsStellarMass - galaxies[p].MetalsBulgeMass);

            galaxies[p].BulgeMass += unstable_stars;
            galaxies[p].InstabilityBulgeMass += unstable_stars;  // Track origin of bulge mass
            galaxies[p].MetalsBulgeMass += metallicity * unstable_stars;
            
            // UPDATE: Tonini incremental radius evolution (equation 15)
            update_instability_bulge_radius(p, unstable_stars, galaxies, run_params);

            // Need to fix this. Excluded for now.
            // galaxies[p].mergeType = 3;  // mark as disk instability partial mass transfer
            // galaxies[p].mergeIntoID = NumGals + p - 1;

#ifdef VERBOSE
            if((galaxies[p].BulgeMass >  1.0001 * galaxies[p].StellarMass)  || (galaxies[p].MetalsBulgeMass >  1.0001 * galaxies[p].MetalsStellarMass)) {
                /* fprintf(stderr, "\nInstability: Mbulge > Mtot (stars or metals)\n"); */
                /* run_params->interrupted = 1; */
                //ABORT(EXIT_FAILURE);
            }
#endif

        }

        // CRITICAL: Recalculate disc radius after mass transfer
        // The disc is now less massive, so its scale radius should shrink
        // Use conservation of angular momentum: smaller mass → smaller radius
        if(unstable_stars > 0.0 || unstable_gas > 0.0) {
            // Disc mass after instability
            const double new_diskmass = galaxies[p].ColdGas + (galaxies[p].StellarMass - galaxies[p].BulgeMass);
            
            if(new_diskmass > 0.0 && diskmass > 0.0) {
                // Simple scaling: R_new = R_old × (M_new / M_old)
                // This conserves specific angular momentum per unit mass
                const double mass_ratio = new_diskmass / diskmass;
                galaxies[p].DiskScaleRadius *= mass_ratio;
                
                // Safety check: don't let disc radius go to zero or become huge
                if(galaxies[p].DiskScaleRadius < 0.01 * galaxies[p].Rvir) {
                    galaxies[p].DiskScaleRadius = 0.01 * galaxies[p].Rvir;
                }
                if(galaxies[p].DiskScaleRadius > galaxies[p].Rvir) {
                    galaxies[p].DiskScaleRadius = galaxies[p].Rvir;
                }
            } else {
                // Disc has been completely consumed by bulge
                galaxies[p].DiskScaleRadius = 0.0;
            }
        
        }

        // burst excess gas and feed black hole (really need a dedicated model for bursts and BH growth here)
        if(unstable_gas > 0.0) {
#ifdef VERBOSE
            if(unstable_gas > 1.0001 * galaxies[p].ColdGas ) {
                fprintf(stdout, "unstable_gas > galaxies[p].ColdGas\t%e\t%e\n", unstable_gas, galaxies[p].ColdGas);
                run_params->interrupted = 1;
                // ABORT(EXIT_FAILURE);
            }
#endif

            const double unstable_gas_fraction = unstable_gas / galaxies[p].ColdGas;
            if(run_params->AGNrecipeOn > 0) {
                grow_black_hole(p, unstable_gas_fraction, galaxies, run_params);
            }

            collisional_starburst_recipe(unstable_gas_fraction, p, centralgal, time, dt, halonr, 1, step, galaxies, run_params);
        }
    }
}
