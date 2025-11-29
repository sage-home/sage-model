#!/usr/bin/env python3
"""
Quick diagnostic: Check FFBRegime field across all snapshots
This will tell us WHERE the FFBRegime values are being lost
"""

import h5py
import numpy as np
from pathlib import Path

def check_ffb_across_snapshots(filepath):
    """Check FFBRegime in all snapshots"""
    print(f"\n{'='*80}")
    print(f"CHECKING FFBRegime ACROSS ALL SNAPSHOTS")
    print(f"File: {filepath}")
    print(f"{'='*80}\n")
    
    with h5py.File(filepath, 'r') as f:
        # Get all snapshot keys
        snap_keys = sorted([k for k in f.keys() if k.startswith('Snap_')])
        
        print(f"Found {len(snap_keys)} snapshots\n")
        print(f"{'Snapshot':<12} {'z (approx)':<12} {'N_gal':<10} {'N_FFB':<10} {'%_FFB':<10}")
        print(f"{'-'*60}")
        
        for snap_key in snap_keys:
            snap_num = int(snap_key.split('_')[1])
            snap_group = f[snap_key]
            
            # Check if FFBRegime field exists
            if 'FFBRegime' not in snap_group:
                print(f"{snap_key:<12} {'N/A':<12} {'N/A':<10} {'FIELD MISSING!':>30}")
                continue
            
            ffb_regime = snap_group['FFBRegime'][:]
            n_gal = len(ffb_regime)
            n_ffb = np.sum(ffb_regime == 1)
            pct_ffb = 100.0 * n_ffb / n_gal if n_gal > 0 else 0.0
            
            # Estimate redshift (rough)
            # Assuming snapshot 63 ~ z=0, and each snapshot is ~0.2 in z
            z_approx = max(0, (63 - snap_num) * 0.15)
            
            # Highlight high-z snapshots where FFB should appear
            marker = " <-- HIGH-Z" if z_approx > 5.0 else ""
            marker = " <-- FFB ACTIVE!" if n_ffb > 0 else marker
            
            print(f"{snap_key:<12} {z_approx:<12.2f} {n_gal:<10} {n_ffb:<10} {pct_ffb:<10.2f}{marker}")
        
        print(f"\n{'='*80}\n")

def check_ffb_statistics(filepath):
    """Print detailed statistics for FFB galaxies"""
    print(f"DETAILED FFB STATISTICS")
    print(f"{'='*80}\n")
    
    with h5py.File(filepath, 'r') as f:
        snap_keys = sorted([k for k in f.keys() if k.startswith('Snap_')])
        
        total_ffb_galaxies = 0
        total_galaxies = 0
        
        for snap_key in snap_keys:
            snap_group = f[snap_key]
            if 'FFBRegime' not in snap_group:
                continue
            
            ffb_regime = snap_group['FFBRegime'][:]
            n_ffb = np.sum(ffb_regime == 1)
            
            if n_ffb > 0:
                snap_num = int(snap_key.split('_')[1])
                z_approx = max(0, (63 - snap_num) * 0.15)
                
                # Get additional info
                stellar_mass = snap_group['StellarMass'][:] if 'StellarMass' in snap_group else None
                mvir = snap_group['Mvir'][:] if 'Mvir' in snap_group else None
                
                ffb_mask = ffb_regime == 1
                
                print(f"\n{snap_key} (z ~ {z_approx:.2f}): {n_ffb} FFB galaxies")
                
                if stellar_mass is not None:
                    ffb_mstar = stellar_mass[ffb_mask] * 1e10  # Convert to Msun
                    print(f"  Stellar masses: {np.median(ffb_mstar[ffb_mstar > 0]):.2e} Msun (median)")
                
                if mvir is not None:
                    ffb_mvir = mvir[ffb_mask] * 1e10
                    print(f"  Halo masses: {np.median(ffb_mvir[ffb_mvir > 0]):.2e} Msun (median)")
                
                total_ffb_galaxies += n_ffb
            
            total_galaxies += len(ffb_regime)
        
        print(f"\n{'-'*80}")
        print(f"TOTAL: {total_ffb_galaxies} FFB galaxies found across all snapshots")
        print(f"       {total_galaxies} total galaxies across all snapshots")
        print(f"       {100.0 * total_ffb_galaxies / total_galaxies:.3f}% FFB")
        print(f"{'='*80}\n")

def main():
    print("\n" + "="*80)
    print("FFB REGIME DIAGNOSTIC ACROSS SNAPSHOTS")
    print("="*80)
    
    # Get file path from user or use default
    import sys
    if len(sys.argv) > 1:
        filepath = Path(sys.argv[1])
    else:
        filepath = Path("output/millennium/model_0.hdf5")
        print(f"\nUsage: {sys.argv[0]} <path_to_hdf5_file>")
        print(f"Using default: {filepath}\n")
    
    if not filepath.exists():
        print(f"ERROR: File not found: {filepath}")
        print("\nPlease provide path to SAGE HDF5 output file")
        return
    
    # Check across all snapshots
    check_ffb_across_snapshots(filepath)
    
    # Get detailed statistics
    check_ffb_statistics(filepath)
    
    print("\nINTERPRETATION:")
    print("-" * 80)
    print("✓ If FFB galaxies appear at z>6: Bug is fixed, FFB is working!")
    print("✓ If N_FFB decreases at low-z: Expected (mergers, quenching)")
    print("✗ If N_FFB = 0 everywhere: FFBRegime not being assigned properly")
    print("✗ If N_FFB > 0 at high-z but 0 at z=0: FFBRegime not preserved through mergers")
    print("✗ If 'FIELD MISSING': FFBRegime not in HDF5 output fields")
    print("=" * 80 + "\n")

if __name__ == "__main__":
    main()