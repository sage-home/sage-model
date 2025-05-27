#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_disk_instability.h"
#include "model_misc.h"
#include "model_mergers.h"
#include "model_h2_formation.h"

void check_disk_instability(const int p, const int centralgal, const int halonr, const double time, const double dt, const int step,
                            struct GALAXY *galaxies, struct params *run_params)
{
    // Here we calculate the stability of the stellar and gaseous disk as discussed in Mo, Mao & White (1998).
    // For unstable stars and gas, we transfer the required amount to the bulge to make the disk stable again

    // Update H2 and HI gas components before calculations
    update_gas_components(&galaxies[p], run_params);

    // Disk mass has to be > 0.0
    const double diskmass = galaxies[p].ColdGas + (galaxies[p].StellarMass - galaxies[p].BulgeMass);
    if(diskmass > 0.0) {
        // calculate critical disk mass
        double Mcrit = galaxies[p].Vmax * galaxies[p].Vmax * (3.0 * galaxies[p].DiskScaleRadius) / run_params->G;
        if(Mcrit > diskmass) {
            Mcrit = diskmass;
        }

        // MODIFIED: Calculate unstable gas based on total cold gas instead of H2
        const double gas_fraction = galaxies[p].ColdGas / diskmass;
        const double unstable_gas = gas_fraction * (diskmass - Mcrit);
        const double star_fraction = 1.0 - gas_fraction;
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
        }

        // burst excess gas and feed black hole
        if(unstable_gas > 0.0) {

            // MODIFIED: Calculate fraction based on total cold gas instead of H2
            const double unstable_gas_fraction = unstable_gas / galaxies[p].ColdGas;
            if(run_params->AGNrecipeOn > 0) {
                grow_black_hole(p, unstable_gas_fraction, galaxies, run_params);
            }

            collisional_starburst_recipe(unstable_gas_fraction, p, centralgal, time, dt, halonr, 1, step, galaxies, run_params);
        }
    }
}