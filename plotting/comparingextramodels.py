import numpy as np
import matplotlib.pyplot as plt
from scipy.optimize import minimize
from itertools import combinations
import pandas as pd

# All the mechanism functions from before
def empirical_boost_function(z):
    """Your empirical boost function"""
    z_boost = np.where(z <= 2.0, z, 
              np.where(z <= 4.0, 2.0,
                      2.0 * np.exp(-(z-4.0)/1.5)))
    
    extra_boost = np.where((z > 4.5) & (z <= 7.5), (z - 4.5) * 0.5,
                  np.where(z > 7.5, (7.5 - 4.5) * 0.5, 0.0))
    
    return z_boost + extra_boost

def metallicity_evolution(z):
    Z0 = 1.0
    z_char = 2.5
    return Z0 * np.exp(-z / z_char)

def gas_fraction_evolution(z):
    f0 = 0.1
    alpha = 1.2
    return f0 * np.power(1.0 + z, alpha)

def metallicity_dependent_cooling(z):
    Z_solar = metallicity_evolution(z)
    return np.power(np.fmax(Z_solar, 0.01), -0.5)

def gas_fraction_momentum_coupling(z):
    fgas = gas_fraction_evolution(z)
    return np.power(fgas / 0.1, 0.5)

def cosmic_ray_confinement(z):
    confinement_factor = 1.0 / (1.0 + 0.3 * z)
    base_cr_boost = 1.0 + z * 0.4
    return base_cr_boost * confinement_factor

def reionization_feedback_suppression(z):
    z_reion = 8.0
    uv_background_strength = np.exp(-(z - z_reion)**2 / 4.0)
    suppression = 1.0 - 0.4 * uv_background_strength
    base_boost = 1.0 + z * 0.2
    return base_boost * suppression

def turbulent_dissipation_evolution(z):
    z_peak = 2.5
    width = 2.0
    return 1.0 + 1.5 * np.exp(-((z - z_peak) / width)**2)

def multiphase_ism_evolution(z):
    clumpiness = 1.0 + 0.8 * z * np.exp(-z / 3.0)
    optimal_clump = 2.0
    return 1.0 + 0.6 * np.exp(-((clumpiness - optimal_clump) / 1.0)**2)

def cosmic_ray_feedback(z):
    Z_solar = metallicity_evolution(z)
    return np.power(1.0 + z, 0.3) * np.power(Z_solar + 0.1, -0.2)

def density_dependent_feedback(z):
    return 1.0 + 0.5 * np.power(1.0 + z, 0.7) * np.exp(-z/3.0)

def turbulent_ism_structure(z):
    return np.power(1.0 + z, 0.25) * np.exp(-z/4.0)

# Dictionary of all mechanisms
MECHANISMS = {
    'metallicity_cooling': metallicity_dependent_cooling,
    'gas_fraction_coupling': gas_fraction_momentum_coupling,
    'cr_confinement': cosmic_ray_confinement,
    'reionization_suppress': reionization_feedback_suppression,
    'turbulent_dissipation': turbulent_dissipation_evolution,
    'multiphase_ism': multiphase_ism_evolution,
    'cosmic_ray_basic': cosmic_ray_feedback,
    'density_dependent': density_dependent_feedback,
    'turbulent_basic': turbulent_ism_structure,
}

def combined_mechanism(z, weights, mechanism_names):
    """Combine multiple mechanisms with given weights"""
    result = np.zeros_like(z)
    for i, name in enumerate(mechanism_names):
        if i < len(weights):  # Safety check
            result += weights[i] * MECHANISMS[name](z)
    return result

def objective_function(weights, z_array, target, mechanism_names):
    """Objective function to minimize (RMS difference)"""
    predicted = combined_mechanism(z_array, weights, mechanism_names)
    rms_diff = np.sqrt(np.mean((target - predicted)**2))
    return rms_diff

def optimize_combination(mechanism_names, z_array, target):
    """Optimize weights for a given combination of mechanisms"""
    n_mechanisms = len(mechanism_names)
    
    # Initial guess: equal weights
    initial_weights = np.ones(n_mechanisms) * 0.5
    
    # Bounds: allow both positive and negative contributions
    bounds = [(-2.0, 2.0) for _ in range(n_mechanisms)]
    
    # Optimize
    result = minimize(objective_function, initial_weights, 
                     args=(z_array, target, mechanism_names),
                     bounds=bounds, method='L-BFGS-B')
    
    return result.x, result.fun

def systematic_search():
    """Systematically test different combinations"""
    redshifts = np.linspace(0, 8, 200)
    target = empirical_boost_function(redshifts)
    
    mechanism_names = list(MECHANISMS.keys())
    results = []
    
    print("Systematic search for best mechanism combinations...")
    print("="*60)
    
    # Test individual mechanisms
    print("INDIVIDUAL MECHANISMS:")
    for name in mechanism_names:
        weights, rms = optimize_combination([name], redshifts, target)
        results.append({
            'combination': name,
            'mechanisms': [name],
            'weights': weights,
            'rms': rms,
            'n_mechanisms': 1
        })
        print(f"{name:25s}: RMS = {rms:.3f}, Weight = {weights[0]:.3f}")
    
    print("\nPAIRWISE COMBINATIONS:")
    # Test pairs
    for pair in combinations(mechanism_names, 2):
        weights, rms = optimize_combination(list(pair), redshifts, target)
        results.append({
            'combination': ' + '.join(pair),
            'mechanisms': list(pair),
            'weights': weights,
            'rms': rms,
            'n_mechanisms': 2
        })
        print(f"{' + '.join(pair):40s}: RMS = {rms:.3f}")
    
    print("\nTRIPLE COMBINATIONS (best pairs only):")
    # Test top combinations with 3 mechanisms
    sorted_pairs = sorted([r for r in results if r['n_mechanisms'] == 2], 
                         key=lambda x: x['rms'])[:5]  # Top 5 pairs
    
    for pair_result in sorted_pairs:
        for third_mechanism in mechanism_names:
            if third_mechanism not in pair_result['mechanisms']:
                triple = pair_result['mechanisms'] + [third_mechanism]
                weights, rms = optimize_combination(triple, redshifts, target)
                results.append({
                    'combination': ' + '.join(triple),
                    'mechanisms': triple,
                    'weights': weights,
                    'rms': rms,
                    'n_mechanisms': 3
                })
                print(f"{' + '.join(triple):50s}: RMS = {rms:.3f}")
    
    return results, redshifts, target

def plot_best_combinations(results, redshifts, target, top_n=5):
    """Plot the best combinations"""
    # Sort by RMS
    sorted_results = sorted(results, key=lambda x: x['rms'])
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
    
    # Plot 1: Best combinations vs empirical
    ax1.plot(redshifts, target, 'k-', linewidth=3, label='Your Empirical Function')
    
    colors = ['red', 'blue', 'green', 'orange', 'purple', 'brown', 'pink', 'gray', 'olive', 'cyan']
    for i, result in enumerate(sorted_results[:top_n]):
        predicted = combined_mechanism(redshifts, result['weights'], result['mechanisms'])
        ax1.plot(redshifts, predicted, color=colors[i], linewidth=2, alpha=0.8,
                label=f"{result['combination'][:30]}... (RMS={result['rms']:.3f})")
    
    ax1.set_xlabel('Redshift')
    ax1.set_ylabel('Feedback Boost Factor')
    ax1.set_title(f'Top {top_n} Best Physical Mechanism Combinations')
    ax1.grid(True, alpha=0.3)
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax1.set_ylim(0, 4)
    
    # Plot 2: Best single combination details
    best = sorted_results[0]
    predicted_best = combined_mechanism(redshifts, best['weights'], best['mechanisms'])
    
    ax2.plot(redshifts, target, 'k-', linewidth=3, label='Your Empirical Function')
    ax2.plot(redshifts, predicted_best, 'r-', linewidth=2, label=f'Best Fit (RMS={best["rms"]:.3f})')
    ax2.fill_between(redshifts, target*0.95, target*1.05, alpha=0.2, color='black', label='±5% range')
    
    # Show individual components
    for i, mechanism in enumerate(best['mechanisms']):
        component = best['weights'][i] * MECHANISMS[mechanism](redshifts)
        ax2.plot(redshifts, component, '--', alpha=0.6, 
                label=f'{mechanism} (w={best["weights"][i]:.2f})')
    
    ax2.set_xlabel('Redshift')
    ax2.set_ylabel('Feedback Boost Factor')
    ax2.set_title('Best Combination Breakdown')
    ax2.grid(True, alpha=0.3)
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax2.set_ylim(0, 4)
    
    plt.tight_layout()
    plt.show()
    # plt.savefig('best_combinations_plot.png', dpi=300)
    
    return sorted_results[0]

def create_implementation_code(best_result):
    """Create C code for the best combination"""
    print("\n" + "="*60)
    print("IMPLEMENTATION CODE FOR SAGE:")
    print("="*60)
    
    print("// Optimized physical mechanism combination")
    print("double calculate_physical_feedback_boost(double z, struct GALAXY *galaxies, struct params *run_params) {")
    print("    double boost = 0.0;")
    print("    double Z_solar, fgas, mechanism_value;")
    print("")
    
    for i, mechanism in enumerate(best_result['mechanisms']):
        weight = best_result['weights'][i]
        print(f"    // {mechanism} (weight = {weight:.3f})")
        
        if mechanism == 'metallicity_cooling':
            print("    Z_solar = galaxies[p].MetalsColdGas / galaxies[p].ColdGas / 0.02;")
            print("    Z_solar = fmax(Z_solar, 0.01);")
            print(f"    mechanism_value = pow(Z_solar, -0.5);")
            print(f"    boost += {weight:.3f} * mechanism_value;")
            
        elif mechanism == 'gas_fraction_coupling':
            print("    fgas = galaxies[p].ColdGas / (galaxies[p].StellarMass + galaxies[p].ColdGas);")
            print(f"    mechanism_value = pow(fgas / 0.1, 0.5);")
            print(f"    boost += {weight:.3f} * mechanism_value;")
            
        elif mechanism == 'cr_confinement':
            print("    mechanism_value = (1.0 + z * 0.4) / (1.0 + 0.3 * z);")
            print(f"    boost += {weight:.3f} * mechanism_value;")
            
        elif mechanism == 'reionization_suppress':
            print("    mechanism_value = (1.0 + z * 0.2) * (1.0 - 0.4 * exp(-pow(z - 8.0, 2) / 4.0));")
            print(f"    boost += {weight:.3f} * mechanism_value;")
            
        elif mechanism == 'turbulent_dissipation':
            print("    mechanism_value = 1.0 + 1.5 * exp(-pow((z - 2.5) / 2.0, 2));")
            print(f"    boost += {weight:.3f} * mechanism_value;")
            
        elif mechanism == 'multiphase_ism':
            print("    double clumpiness = 1.0 + 0.8 * z * exp(-z / 3.0);")
            print("    mechanism_value = 1.0 + 0.6 * exp(-pow((clumpiness - 2.0) / 1.0, 2));")
            print(f"    boost += {weight:.3f} * mechanism_value;")
            
        print("")
    
    print("    return boost;")
    print("}")
    print("")
    print("// In your main feedback code:")
    print("double physical_boost = calculate_physical_feedback_boost(z, galaxies, run_params);")
    print("reheated_mass *= (1.0 + suppression_factor * physical_boost);")

# Run the analysis
if __name__ == "__main__":
    # Perform systematic search
    results, redshifts, target = systematic_search()
    
    # Plot best combinations
    best_result = plot_best_combinations(results, redshifts, target, top_n=8)
    
    # Print summary
    print("\n" + "="*60)
    print("BEST COMBINATION SUMMARY:")
    print("="*60)
    print(f"Best RMS: {best_result['rms']:.4f}")
    print(f"Mechanisms: {', '.join(best_result['mechanisms'])}")
    print("Weights:")
    for i, mechanism in enumerate(best_result['mechanisms']):
        print(f"  {mechanism:25s}: {best_result['weights'][i]:6.3f}")
    
    # Show all results in a table
    print("\n" + "="*60)
    print("ALL RESULTS (sorted by RMS):")
    print("="*60)
    df = pd.DataFrame(results)
    df_sorted = df.sort_values('rms')
    for idx, row in df_sorted.head(15).iterrows():
        print(f"RMS: {row['rms']:.3f} | {row['combination']}")
    
    # Generate implementation code
    create_implementation_code(best_result)
    
    # Show physics interpretation
    print("\n" + "="*60)
    print("PHYSICS INTERPRETATION:")
    print("="*60)
    mechanism_physics = {
        'metallicity_cooling': 'Lower metallicity at high-z reduces radiative cooling efficiency',
        'gas_fraction_coupling': 'Higher gas fractions improve momentum coupling with ISM',
        'cr_confinement': 'Cosmic rays become less confined at high-z, reducing efficiency',
        'reionization_suppress': 'UV background heating reduces SN feedback contrast',
        'turbulent_dissipation': 'Optimal turbulent coupling at intermediate redshift',
        'multiphase_ism': 'ISM clumpiness evolution affects momentum transfer efficiency',
        'cosmic_ray_basic': 'Basic cosmic ray production scales with star formation',
        'density_dependent': 'Cosmic density evolution affects local environment',
        'turbulent_basic': 'Basic turbulent energy evolution with cosmic time'
    }
    
    print("Your optimized combination captures:")
    for mechanism in best_result['mechanisms']:
        weight = best_result['weights'][best_result['mechanisms'].index(mechanism)]
        sign = "enhances" if weight > 0 else "suppresses"
        print(f"  • {mechanism_physics.get(mechanism, 'Unknown mechanism')} ({sign} feedback)")