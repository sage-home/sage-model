#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "core_allvars.h"

#include "model_misc.h"
#include "model_h2_formation.h"    // Add this line

void init_galaxy(const int p, const int halonr, int *galaxycounter, const struct halo_data *halos,
                 struct GALAXY *galaxies, const struct params *run_params)
{

	XASSERT(halonr == halos[halonr].FirstHaloInFOFgroup, -1,
            "Error: halonr = %d should be equal to the FirsthaloInFOFgroup = %d\n",
            halonr, halos[halonr].FirstHaloInFOFgroup);

    galaxies[p].Type = 0;

    galaxies[p].GalaxyNr = *galaxycounter;
    (*galaxycounter)++;

    galaxies[p].HaloNr = halonr;
    galaxies[p].MostBoundID = halos[halonr].MostBoundID;
    galaxies[p].SnapNum = halos[halonr].SnapNum - 1;

    galaxies[p].mergeType = 0;
    galaxies[p].mergeIntoID = -1;
    galaxies[p].mergeIntoSnapNum = -1;
    galaxies[p].dT = -1.0;

    for(int j = 0; j < 3; j++) {
        galaxies[p].Pos[j] = halos[halonr].Pos[j];
        galaxies[p].Vel[j] = halos[halonr].Vel[j];
    }

    galaxies[p].Len = halos[halonr].Len;
    galaxies[p].Vmax = halos[halonr].Vmax;
    galaxies[p].Vvir = get_virial_velocity(halonr, halos, run_params);
    galaxies[p].Mvir = get_virial_mass(halonr, halos, run_params);
    galaxies[p].Rvir = get_virial_radius(halonr, halos, run_params);

    galaxies[p].deltaMvir = 0.0;

    galaxies[p].ColdGas = 0.0;
    galaxies[p].H2_gas = 0.0;  // Initialize H2 gas
    galaxies[p].HI_gas = 0.0;  // Initialize HI gas
    galaxies[p].StellarMass = 0.0;
    galaxies[p].BulgeMass = 0.0;
    galaxies[p].HotGas = 0.0;
    galaxies[p].BlackHoleMass = 0.0;
    galaxies[p].ICS = 0.0;
    galaxies[p].CGMgas = 0.0;

    // Initialize H2 fractions if using that model
    if (run_params->SFprescription >= 1) {
        init_gas_components(&galaxies[p]);
    }


    galaxies[p].MetalsColdGas = 0.0;
    galaxies[p].MetalsStellarMass = 0.0;
    galaxies[p].MetalsBulgeMass = 0.0;
    galaxies[p].MetalsHotGas = 0.0;
    galaxies[p].MetalsICS = 0.0;
    galaxies[p].MetalsCGMgas = 0.0;

    for(int step = 0; step < STEPS; step++) {
        galaxies[p].SfrDisk[step] = 0.0;
        galaxies[p].SfrBulge[step] = 0.0;
        galaxies[p].SfrDiskColdGas[step] = 0.0;
        galaxies[p].SfrDiskColdGasMetals[step] = 0.0;
        galaxies[p].SfrBulgeColdGas[step] = 0.0;
        galaxies[p].SfrBulgeColdGasMetals[step] = 0.0;
    }

    galaxies[p].DiskScaleRadius = get_disk_radius(halonr, p, halos, galaxies);
    galaxies[p].MergTime = 999.9f;
    galaxies[p].Cooling = 0.0;
    galaxies[p].Heating = 0.0;
    galaxies[p].r_heat = 0.0;
    galaxies[p].QuasarModeBHaccretionMass = 0.0;
    galaxies[p].TimeOfLastMajorMerger = -1.0;
    galaxies[p].TimeOfLastMinorMerger = -1.0;
    galaxies[p].OutflowRate = 0.0;
	  galaxies[p].TotalSatelliteBaryons = 0.0;

	// infall properties
    galaxies[p].infallMvir = -1.0;
    galaxies[p].infallVvir = -1.0;
    galaxies[p].infallVmax = -1.0;

  // Initialize flow rate tracking
    galaxies[p].CGMgas_pristine = 0.0;  // NEW: Add this line
    galaxies[p].CGMgas_enriched = 0.0;  // NEW: Add this line
    galaxies[p].InfallRate_to_CGM = 0.0;
    galaxies[p].InfallRate_to_Hot = 0.0;
    galaxies[p].TransferRate_CGM_to_Hot = 0.0;

    galaxies[p].MassLoadingFactor = 0.0;  // NEW: Add this line

}

// Add this function to model_misc.c or create a new file
void print_gas_flow_summary(const int gal, struct GALAXY *galaxies, const double dt, const double redshift)
{
    static int print_counter = 0;
    print_counter++;
    
    // Only print for every 9,000,000th galaxy
    if (print_counter % 50000 != 0) {
        return;
    }
    
    printf("=== Gas Flow Summary for Galaxy %d at z=%.2f ===\n", gal, redshift);
    printf("Gas Reservoirs:\n");
    printf("  Cold gas:    %.4f\n", galaxies[gal].ColdGas);
    printf("  Hot gas:     %.4f\n", galaxies[gal].HotGas);
    printf("  CGM total:   %.4f (Pristine: %.4f, Enriched: %.4f)\n", 
           galaxies[gal].CGMgas, galaxies[gal].CGMgas_pristine, galaxies[gal].CGMgas_enriched);
    
    printf("\nFlow Rates (this timestep, dt=%.4f):\n", dt);
    printf("  Infall to CGM:      %.6f\n", galaxies[gal].InfallRate_to_CGM);
    printf("  Infall to Hot:      %.6f\n", galaxies[gal].InfallRate_to_Hot);
    printf("  CGM -> Hot:         %.6f\n", galaxies[gal].TransferRate_CGM_to_Hot);
    printf("  Total outflow:      %.6f\n", galaxies[gal].OutflowRate);
    
    // Calculate flow ratios
    double total_inflow = galaxies[gal].InfallRate_to_CGM + galaxies[gal].InfallRate_to_Hot;
    if(total_inflow > 0.0) {
        printf("\nFlow Fractions:\n");
        printf("  CGM pathway:    %.1f%%\n", 100.0 * galaxies[gal].InfallRate_to_CGM / total_inflow);
        printf("  Direct pathway: %.1f%%\n", 100.0 * galaxies[gal].InfallRate_to_Hot / total_inflow);
    }
    printf("================================================\n\n");
}


double get_disk_radius(const int halonr, const int p, const struct halo_data *halos, const struct GALAXY *galaxies)
{
	if(galaxies[p].Vvir > 0.0 && galaxies[p].Rvir > 0.0) {
		// See Mo, Shude & White (1998) eq12, and using a Bullock style lambda.
		double SpinMagnitude = sqrt(halos[halonr].Spin[0] * halos[halonr].Spin[0] +
                                    halos[halonr].Spin[1] * halos[halonr].Spin[1] + halos[halonr].Spin[2] * halos[halonr].Spin[2]);

		double SpinParameter = SpinMagnitude / ( 1.414 * galaxies[p].Vvir * galaxies[p].Rvir);
		return (SpinParameter / 1.414 ) * galaxies[p].Rvir;
        /* return SpinMagnitude * 0.5 / galaxies[p].Vvir; /\* should be equivalent to previous call *\/ */
	} else {
		return 0.1 * galaxies[p].Rvir;
    }
}



double get_metallicity(const double gas, const double metals)
{
  double metallicity = 0.0;

  if(gas > 0.0 && metals > 0.0) {
      metallicity = metals / gas;
      metallicity = metallicity >= 1.0 ? 1.0:metallicity;
  }

  return metallicity;
}



double dmax(const double x, const double y)
{
    return (x > y) ? x:y;
}



double get_virial_mass(const int halonr, const struct halo_data *halos, const struct params *run_params)
{
  if(halonr == halos[halonr].FirstHaloInFOFgroup && halos[halonr].Mvir >= 0.0)
    return halos[halonr].Mvir;   /* take spherical overdensity mass estimate */
  else
    return halos[halonr].Len * run_params->PartMass;
}



double get_virial_velocity(const int halonr, const struct halo_data *halos, const struct params *run_params)
{
	double Rvir;

	Rvir = get_virial_radius(halonr, halos, run_params);

    if(Rvir > 0.0)
		return sqrt(run_params->G * get_virial_mass(halonr, halos, run_params) / Rvir);
	else
		return 0.0;
}


double get_virial_radius(const int halonr, const struct halo_data *halos, const struct params *run_params)
{
  // return halos[halonr].Rvir;  // Used for Bolshoi
  const int snapnum = halos[halonr].SnapNum;
  const double zplus1 = 1.0 + run_params->ZZ[snapnum];
  const double hubble_of_z_sq =
      run_params->Hubble * run_params->Hubble *(run_params->Omega * zplus1 * zplus1 * zplus1 + (1.0 - run_params->Omega - run_params->OmegaLambda) * zplus1 * zplus1 +
                                              run_params->OmegaLambda);

  const double rhocrit = 3.0 * hubble_of_z_sq / (8.0 * M_PI * run_params->G);
  const double fac = 1.0 / (200.0 * 4.0 * M_PI / 3.0 * rhocrit);

  return cbrt(get_virial_mass(halonr, halos, run_params) * fac);
}

// Global counter for logging frequency control
static int param_calculation_counter = 0;

double test_redshift_param(double param_z0, double alpha, double z) {
  double result = param_z0 * pow(1.0 + z, alpha);
  
  // Only log every 10000 calculations
  //if (param_calculation_counter % 500000 == 0) {
      //printf("TEST: param_z0=%f, alpha=%f, z=%f, result=%f\n", 
            // param_z0, alpha, z, result);
  //}
  param_calculation_counter++;
  
  return result;
}

double get_redshift_dependent_parameter(double param_z0, double alpha, double redshift)
{

  test_redshift_param(param_z0, alpha, redshift);

  return param_z0 * pow(1.0 + redshift, alpha);
}

