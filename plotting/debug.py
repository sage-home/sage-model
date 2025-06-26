#!/usr/bin/env python
"""
Quick debug script to check the actual infall values
"""
import h5py as h5
import numpy as np
import os

def read_hdf(dirname, snap_num, param):
    """Read data from one or more SAGE model files"""
    model_files = [f for f in os.listdir(dirname) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    combined_data = None
    for model_file in model_files:
        with h5.File(dirname + model_file, 'r') as f:
            data = np.array(f[snap_num][param])
            if combined_data is None:
                combined_data = data
            else:
                combined_data = np.concatenate((combined_data, data))
    return combined_data

def analyze_infall_values():
    DirName = './output/millennium/'
    Snapshot = 'Snap_63'
    Hubble_h = 0.73
    
    print("=== INFALL VALUE ANALYSIS ===\n")
    
    # Read all data
    Type = read_hdf(DirName, Snapshot, 'Type')
    StellarMass = read_hdf(DirName, Snapshot, 'StellarMass') * 1.0e10 / Hubble_h
    Mvir = read_hdf(DirName, Snapshot, 'Mvir') * 1.0e10 / Hubble_h
    HotGas = read_hdf(DirName, Snapshot, 'HotGas') * 1.0e10 / Hubble_h
    CGMgas = read_hdf(DirName, Snapshot, 'CGMgas') * 1.0e10 / Hubble_h
    
    # Read infall rates (already in M_sun/yr after your fixes)
    infallToCGM = read_hdf(DirName, Snapshot, 'infallToCGM')
    infallToHot = read_hdf(DirName, Snapshot, 'infallToHot')
    InfallMvir = read_hdf(DirName, Snapshot, 'infallMvir') * 1.0e10 / Hubble_h
    
    # Separate by galaxy type
    central_mask = (Type == 0) & (StellarMass > 0) & (Mvir > 0)
    satellite_mask = (Type == 1) & (StellarMass > 0)
    
    print(f"Total galaxies: {len(Type)}")
    print(f"Central galaxies: {np.sum(central_mask)}")
    print(f"Satellite galaxies: {np.sum(satellite_mask)}")
    
    # Analyze central galaxies
    print("\n=== CENTRAL GALAXY ANALYSIS ===")
    
    central_mvir = Mvir[central_mask]
    central_stellar = StellarMass[central_mask]
    central_hot = HotGas[central_mask]
    central_cgm = CGMgas[central_mask]
    central_cgm_infall = infallToCGM[central_mask]
    central_hot_infall = infallToHot[central_mask]
    
    print(f"Central galaxies with CGM infall > 0: {np.sum(central_cgm_infall > 0)}")
    print(f"Central galaxies with Hot infall > 0: {np.sum(central_hot_infall > 0)}")
    
    # Statistics for central galaxies with significant infall
    cgm_active = central_cgm_infall > 0
    hot_active = central_hot_infall > 0
    
    if np.sum(cgm_active) > 0:
        print(f"\nCGM Infall Statistics (for {np.sum(cgm_active)} active galaxies):")
        print(f"  Min: {np.min(central_cgm_infall[cgm_active]):.2e} M_sun/yr")
        print(f"  Median: {np.median(central_cgm_infall[cgm_active]):.2e} M_sun/yr")
        print(f"  Max: {np.max(central_cgm_infall[cgm_active]):.2e} M_sun/yr")
        print(f"  Mean: {np.mean(central_cgm_infall[cgm_active]):.2e} M_sun/yr")
    
    if np.sum(hot_active) > 0:
        print(f"\nHot Gas Infall Statistics (for {np.sum(hot_active)} active galaxies):")
        print(f"  Min: {np.min(central_hot_infall[hot_active]):.2e} M_sun/yr")
        print(f"  Median: {np.median(central_hot_infall[hot_active]):.2e} M_sun/yr")
        print(f"  Max: {np.max(central_hot_infall[hot_active]):.2e} M_sun/yr")
        print(f"  Mean: {np.mean(central_hot_infall[hot_active]):.2e} M_sun/yr")
        
        # Check for extremely high values
        very_high = central_hot_infall > 1e12  # > 1 trillion M_sun/yr
        print(f"  Galaxies with > 10^12 M_sun/yr: {np.sum(very_high)}")
        
        if np.sum(very_high) > 0:
            print(f"  Max outlier: {np.max(central_hot_infall):.2e} M_sun/yr")
            # Find the galaxy with the highest infall rate
            max_idx = np.argmax(central_hot_infall)
            print(f"  Galaxy with max hot infall:")
            print(f"    Mvir: {central_mvir[max_idx]:.2e} M_sun")
            print(f"    Stellar Mass: {central_stellar[max_idx]:.2e} M_sun")
            print(f"    Hot Gas: {central_hot[max_idx]:.2e} M_sun")
            print(f"    CGM Gas: {central_cgm[max_idx]:.2e} M_sun")
            print(f"    Hot Infall Rate: {central_hot_infall[max_idx]:.2e} M_sun/yr")
            print(f"    CGM Infall Rate: {central_cgm_infall[max_idx]:.2e} M_sun/yr")
    
    # Check ratios
    print(f"\n=== PHYSICAL REASONABLENESS CHECKS ===")
    
    # For galaxies with both types of infall
    both_active = cgm_active & hot_active
    if np.sum(both_active) > 0:
        ratio = central_hot_infall[both_active] / central_cgm_infall[both_active]
        print(f"Hot/CGM infall ratio for {np.sum(both_active)} galaxies with both:")
        print(f"  Median ratio: {np.median(ratio):.1f}")
        print(f"  Mean ratio: {np.mean(ratio):.1f}")
    
    # Compare infall rates to galaxy masses
    if np.sum(hot_active) > 0:
        # Infall rate as fraction of current hot gas mass
        hot_frac = central_hot_infall[hot_active] / central_hot[hot_active]
        print(f"\nHot gas infall rate / Hot gas mass:")
        print(f"  Median: {np.median(hot_frac):.2e} /yr")
        print(f"  Mean: {np.mean(hot_frac):.2e} /yr")
        print(f"  This means hot gas would double in {1.0/np.median(hot_frac):.1e} years")
        
        # Infall rate as fraction of virial mass
        mvir_frac = central_hot_infall[hot_active] / central_mvir[hot_active]
        print(f"\nHot gas infall rate / Virial mass:")
        print(f"  Median: {np.median(mvir_frac):.2e} /yr")
        print(f"  This means infall = {100*np.median(mvir_frac):.1f}% of Mvir per year")

if __name__ == '__main__':
    analyze_infall_values()