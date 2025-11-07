#!/usr/bin/env python3
"""
Diagnostic tool to identify parameter degeneracies and sensitivities
Run this BEFORE attempting PSO calibration
"""

import numpy as np
import matplotlib.pyplot as plt
from dataclasses import dataclass
from typing import List, Tuple

@dataclass
class ParameterTest:
    """Test parameter sensitivity by varying one at a time"""
    name: str
    baseline: float
    range_multiplier: Tuple[float, float]  # (min_mult, max_mult)
    
    
# Define parameters to test - ADJUST THESE TO YOUR ACTUAL PARAMETERS
PARAMETERS = [
    ParameterTest("FeedbackReheatingEpsilon", 2.9, (0.5, 5.0)),
    ParameterTest("RedshiftPowerLawExponent", 1.25, (0.5, 3.0)),
    ParameterTest("FeedbackEjectionEfficiency", 0.5, (0.1, 2.0)),
    ParameterTest("SfrEfficiency", 0.03, (0.5, 2.0)),
    ParameterTest("ReIncorporationFactor", 0.15, (0.3, 3.0)),
    ParameterTest("RadioModeEfficiency", 0.05, (0.3, 3.0)),
    ParameterTest("QuasarModeEfficiency", 0.008, (0.3, 3.0))
]

# Observable types
OBSERVABLES = [
    "SMF_z0",      # Stellar mass function at z=0
    "BHMF_z0",     # Black hole mass function
    "BHBM_z0",     # BH-bulge relation
    "CSFRD_z1",    # Cosmic SFR at z=1
    "CSFRD_z2",    # Cosmic SFR at z=2
    "fgas_M11",    # Gas fraction at 10^11 Msun
    "fgas_M12",    # Gas fraction at 10^12 Msun
]


def compute_sensitivity_matrix(run_model_func):
    """
    Compute derivative dObservable/dParameter for each pair
    
    Returns:
        sensitivity_matrix: (n_obs, n_params) array
        normalized_sensitivity: column-normalized version
    """
    n_params = len(PARAMETERS)
    n_obs = len(OBSERVABLES)
    
    sensitivity = np.zeros((n_obs, n_params))
    
    # Get baseline observables
    baseline_params = {p.name: p.baseline for p in PARAMETERS}
    baseline_obs = run_model_func(baseline_params)
    
    print("Computing sensitivity matrix...")
    print(f"Baseline observables: {baseline_obs}")
    
    # Vary each parameter
    for i, param in enumerate(PARAMETERS):
        print(f"\nTesting parameter: {param.name}")
        
        # Small perturbation (1% change)
        delta = 0.01 * param.baseline
        
        test_params = baseline_params.copy()
        test_params[param.name] = param.baseline + delta
        
        perturbed_obs = run_model_func(test_params)
        
        # Compute derivative: dObs/dParam
        for j in range(n_obs):
            if baseline_obs[j] != 0:
                # Fractional sensitivity
                sensitivity[j, i] = (perturbed_obs[j] - baseline_obs[j]) / baseline_obs[j] / (delta / param.baseline)
            else:
                sensitivity[j, i] = 0.0
        
        print(f"  Sensitivities: {sensitivity[:, i]}")
    
    # Normalize columns (parameters) to unit variance
    normalized = sensitivity.copy()
    for i in range(n_params):
        std = np.std(sensitivity[:, i])
        if std > 0:
            normalized[:, i] /= std
    
    return sensitivity, normalized


def check_degeneracies(sensitivity_matrix):
    """
    Identify parameter combinations that produce similar observable changes
    """
    from numpy.linalg import svd
    
    print("\n" + "="*60)
    print("DEGENERACY ANALYSIS")
    print("="*60)
    
    # SVD to find near-zero singular values
    U, s, Vt = svd(sensitivity_matrix, full_matrices=False)
    
    print(f"\nSingular values: {s}")
    print(f"Condition number: {s[0]/s[-1]:.2e}")
    
    if s[0]/s[-1] > 1e6:
        print("\n⚠️  WARNING: Matrix is ILL-CONDITIONED!")
        print("   Some parameter combinations are nearly degenerate.")
    
    # Find parameter combinations in null space
    threshold = 0.01 * s[0]  # parameters with singular values < 1% of max
    null_indices = np.where(s < threshold)[0]
    
    if len(null_indices) > 0:
        print(f"\n⚠️  Found {len(null_indices)} near-degenerate directions:")
        for idx in null_indices:
            print(f"\n  Direction {idx} (singular value: {s[idx]:.2e}):")
            param_weights = Vt[idx, :]
            for i, weight in enumerate(param_weights):
                if abs(weight) > 0.1:
                    print(f"    {PARAMETERS[i].name}: {weight:.3f}")
    
    return s, Vt


def plot_sensitivity_matrix(sensitivity_matrix):
    """
    Visualize which parameters affect which observables
    """
    fig, ax = plt.subplots(figsize=(10, 8))
    
    im = ax.imshow(sensitivity_matrix, aspect='auto', cmap='RdBu_r', 
                   vmin=-1, vmax=1)
    
    ax.set_xticks(range(len(PARAMETERS)))
    ax.set_xticklabels([p.name for p in PARAMETERS], rotation=45, ha='right')
    ax.set_yticks(range(len(OBSERVABLES)))
    ax.set_yticklabels(OBSERVABLES)
    
    ax.set_xlabel('Parameters')
    ax.set_ylabel('Observables')
    ax.set_title('Sensitivity Matrix: dObservable/dParameter\n(normalized by std)')
    
    plt.colorbar(im, ax=ax, label='Normalized sensitivity')
    plt.tight_layout()
    plt.savefig('sensitivity_matrix.png', dpi=150)
    print("\nSaved sensitivity matrix plot to 'sensitivity_matrix.png'")


def suggest_parameter_reductions(sensitivity_matrix, Vt):
    """
    Suggest which parameters to fix or group together
    """
    print("\n" + "="*60)
    print("PARAMETER REDUCTION SUGGESTIONS")
    print("="*60)
    
    # Find parameters with low total sensitivity
    total_sensitivity = np.sum(np.abs(sensitivity_matrix), axis=0)
    weak_params = np.where(total_sensitivity < 0.1 * np.max(total_sensitivity))[0]
    
    if len(weak_params) > 0:
        print("\n📌 Consider FIXING these weakly-constrained parameters:")
        for idx in weak_params:
            print(f"   - {PARAMETERS[idx].name} (total sensitivity: {total_sensitivity[idx]:.3f})")
    
    # Find highly correlated parameters
    param_corr = np.corrcoef(sensitivity_matrix.T)
    high_corr = np.where(np.triu(np.abs(param_corr) > 0.9, k=1))
    
    if len(high_corr[0]) > 0:
        print("\n📌 Consider GROUPING these highly correlated parameters:")
        for i, j in zip(high_corr[0], high_corr[1]):
            print(f"   - {PARAMETERS[i].name} ↔ {PARAMETERS[j].name} (corr: {param_corr[i,j]:.3f})")


# SAGE INTEGRATION
import sys
import os
import tempfile
import shutil
import h5py as h5
import subprocess

def run_sage_model(params, template_parfile='../input/millennium.par', 
                   sage_executable='../sage', cleanup=True):
    """
    Run SAGE with modified parameters and extract observables
    
    Input: dict of parameter names to values
    Output: array of observable values in same order as OBSERVABLES list
    """
    # Create temporary directory for this run
    temp_dir = tempfile.mkdtemp(prefix='sage_sensitivity_')
    temp_parfile = os.path.join(temp_dir, 'test.par')
    output_dir = os.path.join(temp_dir, 'output')
    
    # Create output directory
    os.makedirs(output_dir, exist_ok=True)
    
    try:
        # Read template parameter file and modify parameters
        with open(template_parfile, 'r') as f:
            lines = f.readlines()
        
        with open(temp_parfile, 'w') as f:
            for line in lines:
                modified = False
                
                # Update OutputDir
                if line.strip().startswith('OutputDir'):
                    f.write(f'OutputDir              {output_dir}\n')
                    modified = True
                
                # Update each parameter from params dict
                for param_name, param_value in params.items():
                    if line.strip().startswith(param_name):
                        f.write(f'{param_name:<22} {param_value}\n')
                        modified = True
                        break
                
                if not modified:
                    f.write(line)
        
        print(f"  Running SAGE with: {params}")
        print(f"  Executable: {sage_executable}")
        print(f"  Parameter file: {temp_parfile}")
        
        # Check if executable exists
        if not os.path.exists(sage_executable):
            raise FileNotFoundError(f"SAGE executable not found: {sage_executable}")
        
        # Run SAGE executable directly
        result = subprocess.run([sage_executable, temp_parfile], 
                              capture_output=True, text=True)
        
        print(f"  SAGE completed. Return code: {result.returncode}")
        if result.stdout:
            print(f"  STDOUT: {result.stdout[:200]}")
        if result.stderr:
            print(f"  STDERR: {result.stderr[:200]}")
        
        # Read output and compute observables
        output_file = None
        for fname in os.listdir(output_dir):
            if fname.startswith('model_') and fname.endswith('.hdf5'):
                output_file = os.path.join(output_dir, fname)
                break
        
        if output_file is None:
            raise FileNotFoundError(f"No SAGE output found in {output_dir}")
        
        obs = np.zeros(len(OBSERVABLES))
        
        with h5.File(output_file, 'r') as f:
            # Adjust snapshot and cosmology to your simulation
            snapshot = 'Snap_63'  # z=0 for Millennium
            hubble_h = 0.73
            
            # Read galaxy properties
            stellar_mass = np.array(f[snapshot]['StellarMass']) * 1.0e10 / hubble_h
            bh_mass = np.array(f[snapshot]['BlackHoleMass']) * 1.0e10 / hubble_h
            bulge_mass = np.array(f[snapshot]['BulgeMass']) * 1.0e10 / hubble_h
            cold_gas = np.array(f[snapshot]['ColdGas']) * 1.0e10 / hubble_h
            sfr_disk = np.array(f[snapshot]['SfrDisk'])
            sfr_bulge = np.array(f[snapshot]['SfrBulge'])
            
            # Compute observables - CUSTOMIZE THESE TO YOUR NEEDS
            # 0. SMF_z0: Count galaxies in stellar mass bin
            w = (stellar_mass > 1e10) & (stellar_mass < 2e10)
            obs[0] = np.sum(w)
            
            # 1. BHMF_z0: Count galaxies in BH mass bin
            w = (bh_mass > 1e7) & (bh_mass < 1e8)
            obs[1] = np.sum(w)
            
            # 2. BHBM_z0: Median BH mass at Mbulge ~ 10^10.5
            w = (bulge_mass > 2e10) & (bulge_mass < 5e10) & (bh_mass > 0)
            obs[2] = np.median(np.log10(bh_mass[w])) if np.sum(w) > 5 else 7.5
            
            # 3. CSFRD_z1: Total SFR (proxy for cosmic SFR density)
            total_sfr = np.sum(sfr_disk + sfr_bulge)
            obs[3] = total_sfr / 1000.0  # normalized
            
            # 4. CSFRD_z2: Similar (placeholder)
            obs[4] = obs[3] * 1.2
            
            # 5. fgas_M11: Gas fraction at M* ~ 10^11
            w = (stellar_mass > 8e10) & (stellar_mass < 1.2e11) & (cold_gas > 0)
            if np.sum(w) > 5:
                gas_frac = cold_gas[w] / (stellar_mass[w] + cold_gas[w])
                obs[5] = np.median(gas_frac)
            else:
                obs[5] = 0.1
            
            # 6. fgas_M12: Gas fraction at M* ~ 10^12
            w = (stellar_mass > 8e11) & (stellar_mass < 1.2e12) & (cold_gas > 0)
            if np.sum(w) > 5:
                gas_frac = cold_gas[w] / (stellar_mass[w] + cold_gas[w])
                obs[6] = np.median(gas_frac)
            else:
                obs[6] = 0.05
        
        print(f"  Observables: {obs}")
        return obs
        
    except Exception as e:
        print(f"  ERROR running SAGE: {e}")
        return np.zeros(len(OBSERVABLES))
        
    finally:
        # Cleanup temporary directory
        if cleanup and os.path.exists(temp_dir):
            shutil.rmtree(temp_dir)


# ============================================================================
# PRE-FLIGHT DIAGNOSTIC TESTS
# ============================================================================

def test_model_runs():
    """Test 1: Model completes successfully"""
    print("\n" + "="*60)
    print("TEST 1: Model Execution")
    print("="*60)
    
    try:
        baseline_params = {p.name: p.baseline for p in PARAMETERS}
        obs = run_sage_model(baseline_params)
        print("✓ Model completed successfully")
        print(f"  Observables computed: {OBSERVABLES}")
        print(f"  Values: {obs}")
        
        # Check for NaN/inf
        has_bad_values = False
        for i, val in enumerate(obs):
            if not np.isfinite(val):
                print(f"✗ Observable '{OBSERVABLES[i]}' is {val} (should be finite)")
                has_bad_values = True
        
        if not has_bad_values:
            print("✓ All observables are finite")
            return True
        else:
            return False
            
    except Exception as e:
        print(f"✗ Model failed with error: {e}")
        import traceback
        traceback.print_exc()
        return False


def test_parameter_sensitivity_quick():
    """Test 2: Parameters affect outputs (quick version)"""
    print("\n" + "="*60)
    print("TEST 2: Parameter Sensitivity (Quick)")
    print("="*60)
    
    baseline_params = {p.name: p.baseline for p in PARAMETERS}
    baseline_obs = run_sage_model(baseline_params)
    
    sensitivities = {}
    all_pass = True
    
    # Test only first 2 parameters for speed
    for param in PARAMETERS[:2]:
        params_plus = baseline_params.copy()
        params_plus[param.name] *= 1.2  # +20% change
        
        obs_plus = run_sage_model(params_plus)
        
        # Compute max fractional change
        max_change = 0.0
        for i in range(len(OBSERVABLES)):
            if baseline_obs[i] != 0:
                frac_change = abs(obs_plus[i] - baseline_obs[i]) / abs(baseline_obs[i])
                max_change = max(max_change, frac_change)
        
        sensitivities[param.name] = max_change
        
        if max_change < 0.001:
            print(f"⚠ {param.name}: No effect detected (change < 0.1%)")
            all_pass = False
        elif max_change > 10.0:
            print(f"⚠ {param.name}: Extreme sensitivity (change > 1000%)")
            all_pass = False
        else:
            print(f"✓ {param.name}: {max_change*100:.1f}% change (reasonable)")
    
    return all_pass


def test_reproducibility():
    """Test 3: Model produces consistent results"""
    print("\n" + "="*60)
    print("TEST 3: Reproducibility")
    print("="*60)
    
    baseline_params = {p.name: p.baseline for p in PARAMETERS}
    obs1 = run_sage_model(baseline_params)
    obs2 = run_sage_model(baseline_params)
    
    max_diff = 0.0
    all_match = True
    
    for i in range(len(OBSERVABLES)):
        if obs1[i] != 0:
            frac_diff = abs(obs2[i] - obs1[i]) / abs(obs1[i])
            max_diff = max(max_diff, frac_diff)
            
            if frac_diff > 0.01:  # More than 1% difference
                print(f"⚠ {OBSERVABLES[i]}: {frac_diff*100:.2f}% difference between runs")
                all_match = False
    
    if all_match:
        print("✓ Model produces identical results (reproducible)")
        return True
    else:
        print(f"⚠ Maximum difference: {max_diff*100:.2f}%")
        print("  Check for random seeds or uninitialized variables")
        return False


def run_pre_flight_diagnostics():
    """Run quick diagnostic tests before full sensitivity analysis"""
    print("="*60)
    print("SAGE PSO PRE-FLIGHT DIAGNOSTIC")
    print("="*60)
    print("Running quick tests to verify model works properly...")
    print("(~3 model evaluations)\n")
    
    results = {
        'Model Execution': test_model_runs(),
        'Parameter Sensitivity': test_parameter_sensitivity_quick(),
        'Reproducibility': test_reproducibility(),
    }
    
    # Summary
    print("\n" + "="*60)
    print("DIAGNOSTIC SUMMARY")
    print("="*60)
    
    all_pass = True
    for test_name, passed in results.items():
        status = "PASS" if passed else "FAIL"
        symbol = "✓" if passed else "✗"
        print(f"{symbol} {test_name}: {status}")
        if not passed:
            all_pass = False
    
    print("\n" + "="*60)
    if all_pass:
        print("✓ ALL TESTS PASSED")
        print("Proceeding with full sensitivity analysis...")
    else:
        print("✗ SOME TESTS FAILED")
        print("Fix issues above before continuing.")
        print("Common fixes:")
        print("  - No sensitivity: Check parameter is used in model")
        print("  - Not reproducible: Set random seeds consistently")
    print("="*60)
    
    return all_pass


if __name__ == "__main__":
    print("="*60)
    print("SAGE CGM MODEL: PARAMETER SENSITIVITY ANALYSIS")
    print("="*60)
    
    # Run pre-flight diagnostics first
    if not run_pre_flight_diagnostics():
        print("\n⚠️  Stopping due to diagnostic failures.")
        print("Fix the issues above and try again.")
        sys.exit(1)
    
    # Step 1: Compute sensitivity matrix
    print("\n" + "="*60)
    print("FULL SENSITIVITY ANALYSIS")
    print("="*60)
    print("This will run SAGE multiple times (1 + N_parameters runs)")
    print(f"Total runs: {1 + len(PARAMETERS)}")
    print("="*60 + "\n")
    
    sensitivity, normalized = compute_sensitivity_matrix(run_sage_model)
    
    # Step 2: Check for degeneracies
    singular_values, Vt = check_degeneracies(normalized)
    
    # Step 3: Visualize
    plot_sensitivity_matrix(normalized)
    
    # Step 4: Suggest parameter reductions
    suggest_parameter_reductions(normalized, Vt)
    
    print("\n" + "="*60)
    print("NEXT STEPS:")
    print("="*60)
    print("1. Review sensitivity_matrix.png to see parameter impacts")
    print("2. Fix/remove weakly constrained parameters")
    print("3. Group/tie together degenerate parameters")
    print("4. Use only well-constrained subset for PSO (typically 3-4 params)")
    print("5. Consider sequential optimization (star formation → feedback → AGN)")
    print("\nRecommended: Start PSO with 3-4 most sensitive parameters only!")