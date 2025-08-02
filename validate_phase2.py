#!/usr/bin/env python3
"""
Phase 2 Validation Script
Validates that scientific calculations are working correctly after Phase 2 enhancements
"""

import h5py
import numpy as np

def validate_phase2():
    """Validate Phase 2 scientific calculations"""
    print("=== Phase 2 Scientific Calculation Validation ===")
    
    try:
        with h5py.File('./output/millennium/model_z0.000.h5', 'r') as f:
            deltaMvir = f['galaxies']['deltaMvir'][:]
            infallMvir = f['galaxies']['infallMvir'][:]
            infallVvir = f['galaxies']['infallVvir'][:]
            infallVmax = f['galaxies']['infallVmax'][:]
            galaxy_type = f['galaxies']['Type'][:]
            
            total_galaxies = len(deltaMvir)
            print(f"Total galaxies analyzed: {total_galaxies}")
            
            # Validate deltaMvir calculations
            nonzero_delta = np.count_nonzero(deltaMvir)
            print(f"deltaMvir non-zero count: {nonzero_delta}/{total_galaxies} ({100*nonzero_delta/total_galaxies:.1f}%)")
            
            # Validate infall properties
            nonzero_infall_mvir = np.count_nonzero(infallMvir)
            nonzero_infall_vvir = np.count_nonzero(infallVvir)
            nonzero_infall_vmax = np.count_nonzero(infallVmax)
            
            print(f"infallMvir non-zero count: {nonzero_infall_mvir}/{total_galaxies} ({100*nonzero_infall_mvir/total_galaxies:.1f}%)")
            print(f"infallVvir non-zero count: {nonzero_infall_vvir}/{total_galaxies} ({100*nonzero_infall_vvir/total_galaxies:.1f}%)")
            print(f"infallVmax non-zero count: {nonzero_infall_vmax}/{total_galaxies} ({100*nonzero_infall_vmax/total_galaxies:.1f}%)")
            
            # Sample values
            if nonzero_delta > 0:
                delta_samples = deltaMvir[deltaMvir != 0][:5]
                print(f"deltaMvir sample values: {delta_samples}")
            
            if nonzero_infall_mvir > 0:
                infall_samples = infallMvir[infallMvir != 0][:5]
                print(f"infallMvir sample values: {infall_samples}")
            
            # Validate galaxy types
            type_counts = np.bincount(galaxy_type.astype(int))
            print(f"Galaxy type distribution:")
            print(f"  Type 0 (Central): {type_counts[0] if len(type_counts) > 0 else 0}")
            print(f"  Type 1 (Satellite): {type_counts[1] if len(type_counts) > 1 else 0}")
            print(f"  Type 2 (Orphan): {type_counts[2] if len(type_counts) > 2 else 0}")
            
            # Check for scientific validity
            if nonzero_delta == 0:
                print("❌ FAILURE: No deltaMvir values calculated!")
                return False
            
            if nonzero_infall_mvir == 0:
                print("❌ FAILURE: No infallMvir values calculated!")
                return False
                
            print("✅ SUCCESS: Phase 2 scientific calculations working correctly")
            return True
            
    except FileNotFoundError:
        print(f"❌ ERROR: Output file not found. Run SAGE first.")
        return False
    except Exception as e:
        print(f"❌ ERROR: {e}")
        return False

if __name__ == "__main__":
    validate_phase2()