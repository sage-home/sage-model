#!/usr/bin/env python3
"""
Quick test to verify constraint data is being loaded correctly
"""

import sys
import numpy as np
sys.path.insert(0, '.')

from constraints import SMF_z0, SMF_z05, SMF_z10, SMF_z20, BHMF_z0, BHBM_z0
from analysis import chi2

# Test configuration
snapshot_map = {
    'SMF_z0': 63,
    'SMF_z05': 40,
    'SMF_z10': 38,
    'SMF_z20': 32,
    'BHMF_z0': 63,
    'BHBM_z0': 63
}

sim_params = {
    'sim': 1,
    'boxsize': 62.5,
    'vol_frac': 1.0,
    'h0': 0.73,
    'Omega0': 0.25,
    'age_alist_file': '../input/millennium/trees/millennium.a_list'
}

# Model directory - use a recent PSO run
modeldir = '../output/millennium_pso_multi_20251028_204037/run_1/tracks/DS_output_0/0/'
subvols = [0]

# Test each constraint
constraints_to_test = [
    ('SMF_z0', SMF_z0),
    ('SMF_z05', SMF_z05),
]

for name, ConstraintClass in constraints_to_test:
    print(f"\n{'='*70}")
    print(f"Testing: {name}")
    print('='*70)
    
    # Create constraint instance
    constraint = ConstraintClass(
        snapshot=snapshot_map[name],
        **sim_params
    )
    
    # Get observational data directly
    x_obs_raw, y_obs_raw, err_low_raw, err_up_raw = constraint.get_obs_x_y_err()
    
    print(f"\nRaw observational data from get_obs_x_y_err():")
    print(f"  Number of points: {len(y_obs_raw)}")
    print(f"  x_obs (first 5): {x_obs_raw[:5]}")
    print(f"  y_obs (first 5): {y_obs_raw[:5]}")
    print(f"  y_obs (last 5): {y_obs_raw[-5:]}")
    print(f"  y_obs range: [{np.min(y_obs_raw):.3f}, {np.max(y_obs_raw):.3f}]")
    print(f"  All y_obs same? {np.all(y_obs_raw == y_obs_raw[0])}")
    print(f"  Unique y_obs values: {len(np.unique(y_obs_raw))}")
    
    # Now test get_data (which is what PSO uses)
    try:
        y_obs, y_mod, err = constraint.get_data(modeldir, subvols)
        
        print(f"\nData from get_data() (used by PSO):")
        print(f"  Number of points: {len(y_obs)}")
        print(f"  y_obs (first 5): {y_obs[:5]}")
        print(f"  y_obs (last 5): {y_obs[-5:]}")
        print(f"  y_obs range: [{np.min(y_obs):.3f}, {np.max(y_obs):.3f}]")
        print(f"  All y_obs same? {np.all(y_obs == y_obs[0])}")
        print(f"  Unique y_obs values: {len(np.unique(y_obs))}")
        
        # Calculate chi-squared
        score = chi2(y_obs, y_mod, err)
        print(f"\n  Chi-squared score: {score:.2f}")
        print(f"  Chi-squared per data point: {score/len(y_obs):.2f}")
        
    except Exception as e:
        print(f"\nError in get_data(): {e}")
        import traceback
        traceback.print_exc()

print(f"\n{'='*70}")
print("Test complete!")
print('='*70)
