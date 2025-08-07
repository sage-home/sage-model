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
            galaxies[p].MetalsBulgeMass += metallicity * unstable_stars;

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
