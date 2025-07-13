#!/usr/bin/env python3
"""
Debug Kennicutt-Schmidt Relation Plots
=====================================
Creates two clean figures with debug output and multiple observational comparisons
"""

import numpy as np
import matplotlib.pyplot as plt
import h5py as h5
import glob
import os
from scipy import stats
import pandas as pd
from random import seed, sample
import gc

# Set random seed for reproducible results
np.random.seed(42)
seed(42)

# Configuration parameters - UPDATED COLORS
MODEL_CONFIGS = [
    {
        'name': 'SAGE 2.0',
        'dir': './output/millennium/',
        'color': '#2E8B57',  # Sea Green
        'linestyle': '-',
        'linewidth': 3,
        'alpha': 0.3,  # More transparent points
        'marker': 'o',
        'markersize': 6,
        'boxsize': 62.5,
        'volume_fraction': 1.0
    },
    {
        'name': 'SAGE (C16)',
        'dir': './output/millennium_vanilla/',
        'color': '#B22222',  # Fire Brick
        'linestyle': '--',
        'linewidth': 2,
        'alpha': 0.3,  # More transparent points
        'marker': 's',
        'markersize': 5,
        'boxsize': 62.5,
        'volume_fraction': 1.0
    }
]

# Analysis parameters
dilute = 50000   
mass_min = 1e8   
sfr_min = 1e-3   
sSFRcut = -11.0  

# RADIUS CHOICE - EASY TO CHANGE
RADIUS_CHOICE = 'effective'  # Options: 'scale', 'effective', 'half_scale'

# SAGE-specific unit conversions
Hubble_h = 0.73
SFR_UNIT_CONVERSION = 1.0e10 / Hubble_h
MYR_TO_YR = 1.0e6

FileName = 'model_0.hdf5'
OutputDir = './output/millennium/plots/'

# Snapshot to redshift mapping
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (10, 7)
plt.rcParams["figure.dpi"] = 300
plt.rcParams["font.size"] = 14


def read_hdf_for_ks_relation_fixed(directory, snap_num=None, galaxy_types='all'):
    """Fixed data reading for K-S relation analysis"""
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    print(f"    Reading {len(model_files)} files for K-S analysis...")
    
    # Initialize data containers
    all_stellar_mass = []
    all_sfr_disk = []
    all_sfr_bulge = []
    all_cold_gas = []
    all_h2_gas = []
    all_hi_gas = []
    all_disk_radius = []
    all_galaxy_type = []
    
    total_galaxies = 0
    files_processed = 0
    has_h2_data = False
    
    for file_idx, model_file in enumerate(model_files):
        try:
            filepath = os.path.join(directory, model_file)
            
            with h5.File(filepath, 'r') as f:
                if snap_num not in f:
                    continue
                
                stellar_mass = np.array(f[snap_num]['StellarMass'])
                sfr_disk = np.array(f[snap_num]['SfrDisk'])
                sfr_bulge = np.array(f[snap_num]['SfrBulge'])
                cold_gas = np.array(f[snap_num]['ColdGas'])
                disk_radius = np.array(f[snap_num]['DiskRadius'])
                galaxy_type = np.array(f[snap_num]['Type'])
                
                try:
                    h2_gas = np.array(f[snap_num]['H2_gas'])
                    hi_gas = np.array(f[snap_num]['HI_gas'])
                    has_h2_data = True
                except KeyError:
                    h2_gas = np.zeros_like(cold_gas)
                    hi_gas = cold_gas.copy()
                
                stellar_mass_solar = stellar_mass * 1.0e10 / Hubble_h
                sfr_disk_solar = sfr_disk * SFR_UNIT_CONVERSION / MYR_TO_YR
                sfr_bulge_solar = sfr_bulge * SFR_UNIT_CONVERSION / MYR_TO_YR
                sfr_total = sfr_disk_solar + sfr_bulge_solar
                
                mass_mask = stellar_mass_solar > mass_min
                sfr_mask = sfr_total > sfr_min
                radius_mask = disk_radius > 1e-5
                gas_mask = cold_gas > 1e-5
                
                if galaxy_types == 'central':
                    type_mask = (galaxy_type == 0)
                elif galaxy_types == 'satellite':
                    type_mask = (galaxy_type == 1)
                elif galaxy_types == 'orphan':
                    type_mask = (galaxy_type == 2)
                else:
                    type_mask = np.ones(len(stellar_mass), dtype=bool)
                
                combined_mask = mass_mask & sfr_mask & radius_mask & gas_mask & type_mask
                n_valid = np.sum(combined_mask)
                
                if n_valid == 0:
                    continue
                
                all_stellar_mass.append(stellar_mass_solar[combined_mask])
                all_sfr_disk.append(sfr_disk_solar[combined_mask])
                all_sfr_bulge.append(sfr_bulge_solar[combined_mask])
                all_cold_gas.append(cold_gas[combined_mask])
                all_h2_gas.append(h2_gas[combined_mask])
                all_hi_gas.append(hi_gas[combined_mask])
                all_disk_radius.append(disk_radius[combined_mask])
                all_galaxy_type.append(galaxy_type[combined_mask])
                
                total_galaxies += n_valid
                files_processed += 1
                    
        except Exception as e:
            print(f"      Warning: Could not read {model_file}: {e}")
            continue
    
    if total_galaxies == 0:
        print(f"    No valid galaxies found!")
        return None
    
    print(f"    Loaded {total_galaxies} galaxies from {files_processed} files")
    
    result = {
        'stellar_mass': np.concatenate(all_stellar_mass),
        'sfr_disk': np.concatenate(all_sfr_disk),
        'sfr_bulge': np.concatenate(all_sfr_bulge),
        'cold_gas': np.concatenate(all_cold_gas),
        'h2_gas': np.concatenate(all_h2_gas),
        'hi_gas': np.concatenate(all_hi_gas),
        'disk_radius': np.concatenate(all_disk_radius),
        'galaxy_type': np.concatenate(all_galaxy_type),
        'has_h2_data': has_h2_data
    }
    
    del all_stellar_mass, all_sfr_disk, all_sfr_bulge, all_cold_gas
    del all_h2_gas, all_hi_gas, all_disk_radius, all_galaxy_type
    gc.collect()
    
    return result


def calculate_ks_surface_densities_fixed(data, radius_choice='scale', ssfr_cut=-11.0):
    """Fixed K-S surface density calculation with debug output"""
    rd = data['disk_radius']
    
    print(f"    Using radius choice: {radius_choice}")
    print(f"    Disk scale radius range: {np.min(rd):.3f} to {np.max(rd):.3f} Mpc/h")
    
    if radius_choice == 'effective':
        r_calc = 1.678 * rd  # Effective radius Re ≈ 1.678 * scale radius
        print(f"    Using effective radius: Re = 1.678 * Rd")
    elif radius_choice == 'scale':
        r_calc = rd  # Scale radius directly
        print(f"    Using scale radius: Rd")
    elif radius_choice == 'half_scale':
        r_calc = 0.5 * rd  # Half scale radius
        print(f"    Using half scale radius: 0.5 * Rd")
    else:
        r_calc = rd
    
    disk_area_mpc2 = np.pi * r_calc**2
    disk_area_pc2 = disk_area_mpc2 * (1e6 / Hubble_h)**2
    
    print(f"    Radius used range: {np.min(r_calc):.3f} to {np.max(r_calc):.3f} Mpc/h")
    print(f"    Radius used range: {np.min(r_calc*1e3/Hubble_h):.1f} to {np.max(r_calc*1e3/Hubble_h):.1f} kpc")
    
    sfr_total = data['sfr_disk'] + data['sfr_bulge']
    stellar_mass = data['stellar_mass']
    
    print(f"    SFR range: {np.min(sfr_total):.2e} to {np.max(sfr_total):.2e} M☉/yr")
    print(f"    Stellar mass range: {np.min(stellar_mass):.2e} to {np.max(stellar_mass):.2e} M☉")
    
    stellar_mass_safe = np.maximum(stellar_mass, 1e6)
    ssfr = sfr_total / stellar_mass_safe
    log_ssfr = np.log10(ssfr + 1e-15)
    
    sf_mask = log_ssfr > ssfr_cut
    
    print(f"    Star-forming galaxies: {np.sum(sf_mask)}/{len(sf_mask)} ({100*np.sum(sf_mask)/len(sf_mask):.1f}%)")
    
    if np.sum(sf_mask) == 0:
        print(f"    No star-forming galaxies found!")
        return None
    
    sf_data = {}
    for key in data.keys():
        if isinstance(data[key], np.ndarray) and len(data[key]) == len(sf_mask):
            sf_data[key] = data[key][sf_mask]
        else:
            sf_data[key] = data[key]
    
    rd_sf = sf_data['disk_radius']
    if radius_choice == 'effective':
        r_calc_sf = 1.678 * rd_sf
    elif radius_choice == 'scale':
        r_calc_sf = rd_sf
    elif radius_choice == 'half_scale':
        r_calc_sf = 0.5 * rd_sf
    else:
        r_calc_sf = rd_sf
    
    disk_area_sf = np.pi * r_calc_sf**2 * (1e6 / Hubble_h)**2
    
    sfr_total_sf = sf_data['sfr_disk'] + sf_data['sfr_bulge']
    sigma_sfr = sfr_total_sf / disk_area_sf
    
    mass_conversion = 1.0e10 / Hubble_h
    cold_gas_msun = sf_data['cold_gas'] * mass_conversion
    sigma_gas_total = cold_gas_msun / disk_area_sf
    
    sigma_gas_h2 = None
    if sf_data['has_h2_data'] and np.any(sf_data['h2_gas'] > 0):
        h2_gas_msun = sf_data['h2_gas'] * mass_conversion
        sigma_gas_h2 = h2_gas_msun / disk_area_sf
    
    sigma_gas_hi = None
    if sf_data['has_h2_data'] and np.any(sf_data['hi_gas'] > 0):
        hi_gas_msun = sf_data['hi_gas'] * mass_conversion
        sigma_gas_hi = hi_gas_msun / disk_area_sf
    
    print(f"    SURFACE DENSITIES CALCULATED:")
    print(f"      Σ_SFR range: {np.min(sigma_sfr):.2e} to {np.max(sigma_sfr):.2e} M☉/yr/pc²")
    print(f"      Σ_SFR median: {np.median(sigma_sfr):.2e} M☉/yr/pc²")
    print(f"      Σ_gas median: {np.median(sigma_gas_total):.2e} M☉/pc²")
    
    return {
        'sigma_sfr': sigma_sfr,
        'sigma_gas_total': sigma_gas_total,
        'sigma_gas_h2': sigma_gas_h2,
        'sigma_gas_hi': sigma_gas_hi,
        'stellar_mass': sf_data['stellar_mass'],
        'disk_area_pc2': disk_area_sf,
        'radius_used_kpc': r_calc_sf * 1e3 / Hubble_h,
        'has_h2_data': sf_data['has_h2_data'],
        'log_ssfr': log_ssfr[sf_mask],
        'n_sf_galaxies': np.sum(sf_mask),
        'n_total_galaxies': len(sf_mask)
    }


def fit_ks_relation_improved(sigma_gas, sigma_sfr, gas_type='total'):
    """Improved K-S relation fitting with better filtering"""
    gas_mask = (sigma_gas > 1.0)
    sfr_mask = (sigma_sfr > 1e-5)
    finite_mask = np.isfinite(sigma_gas) & np.isfinite(sigma_sfr)
    
    valid_mask = gas_mask & sfr_mask & finite_mask
    
    if np.sum(valid_mask) < 10:
        return None
    
    log_sigma_gas = np.log10(sigma_gas[valid_mask])
    log_sigma_sfr = np.log10(sigma_sfr[valid_mask])
    
    slope, intercept, r_value, p_value, std_err = stats.linregress(log_sigma_gas, log_sigma_sfr)
    
    predicted = slope * log_sigma_gas + intercept
    residuals = log_sigma_sfr - predicted
    scatter = np.std(residuals)
    
    print(f"    K-S FIT RESULTS:")
    print(f"      Slope: {slope:.2f} ± {std_err:.2f}")
    print(f"      Correlation: r = {r_value:.3f}")
    print(f"      Scatter: {scatter:.2f} dex")
    print(f"      N points: {np.sum(valid_mask)}")
    
    return {
        'slope': slope,
        'intercept': intercept,
        'r_value': r_value,
        'p_value': p_value,
        'std_err': std_err,
        'scatter': scatter,
        'n_points': len(log_sigma_gas),
        'log_sigma_gas': log_sigma_gas,
        'log_sigma_sfr': log_sigma_sfr
    }


def plot_observational_relations(ax, gas_type='total'):
    """Add multiple observational K-S relations to the plot"""
    
    if gas_type == 'h2':
        gas_range = np.logspace(0, 2.5, 100)
        
        # Bigiel et al. (2008) - resolved regions in nearby galaxies
        # Σ_SFR = 1.6e-3 × (Σ_H2)^1.0
        ks_bigiel = np.log10(1.6e-3) + 1.0 * np.log10(gas_range)
        ax.plot(np.log10(gas_range), ks_bigiel, 'k:', linewidth=2.5, alpha=0.8, 
               label='Bigiel+ (2008) - resolved', zorder=2)
        
        # Schruba et al. (2011) - different normalization
        # Σ_SFR = 2.1e-3 × (Σ_H2)^1.0
        ks_schruba = np.log10(2.1e-3) + 1.0 * np.log10(gas_range)
        ax.plot(np.log10(gas_range), ks_schruba, 'k--', linewidth=2, alpha=0.6, 
               label='Schruba+ (2011)', zorder=2)
               
        # Leroy et al. (2013) - whole galaxy integrated
        # Σ_SFR = 1.4e-3 × (Σ_H2)^1.1
        ks_leroy = np.log10(1.4e-3) + 1.1 * np.log10(gas_range)
        ax.plot(np.log10(gas_range), ks_leroy, 'k-', linewidth=2, alpha=0.7, 
               label='Leroy+ (2013) - galaxies', zorder=2)
               
        # Saintonge et al. (2011) - COLD GASS survey
        # Σ_SFR = 1.0e-3 × (Σ_H2)^0.96
        ks_saintonge = np.log10(1.0e-3) + 0.96 * np.log10(gas_range)
        ax.plot(np.log10(gas_range), ks_saintonge, 'k-.', linewidth=1.5, alpha=0.5, 
               label='Saintonge+ (2011)', zorder=2)
    
    elif gas_type == 'total':
        gas_range = np.logspace(-0.5, 2.5, 100)
        
        # Kennicutt (1998) - Total gas, whole galaxies
        # Σ_SFR = 2.5e-4 × (Σ_gas)^1.4
        ks_kennicutt = np.log10(2.5e-4) + 1.4 * np.log10(gas_range)
        ax.plot(np.log10(gas_range), ks_kennicutt, 'k:', linewidth=2.5, alpha=0.8, 
               label='Kennicutt (1998) - total gas', zorder=2)
        
        # Schmidt (1959) - original
        # Σ_SFR = 3.2e-4 × (Σ_gas)^1.5
        ks_schmidt = np.log10(3.2e-4) + 1.5 * np.log10(gas_range)
        ax.plot(np.log10(gas_range), ks_schmidt, 'k--', linewidth=2, alpha=0.6, 
               label='Schmidt (1959)', zorder=2)
    
    elif gas_type == 'hi':
        # HI typically shows very weak or no correlation with SFR
        ax.text(0.5, 0.1, 'HI-SFR: Weak/No Correlation\n(Bigiel et al. 2008)', 
               transform=ax.transAxes, fontsize=12, 
               bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.7),
               horizontalalignment='center')


def get_redshift_from_snapshot(snap_num):
    """Convert snapshot number to redshift"""
    if isinstance(snap_num, str):
        snap_num = int(snap_num.split('_')[-1])
    
    if 0 <= snap_num < len(redshifts):
        return redshifts[snap_num]
    else:
        raise ValueError(f"Snapshot {snap_num} out of range for redshift list")


def get_available_snapshots(directory):
    """Get list of available snapshots from the data files"""
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    
    if not model_files:
        return []
    
    try:
        with h5.File(os.path.join(directory, model_files[0]), 'r') as f:
            snap_keys = [key for key in f.keys() if 'snap' in key.lower()]
            
            snapshots = []
            for key in snap_keys:
                try:
                    if key.startswith('Snap_'):
                        snap_num = int(key.split('_')[-1])
                    else:
                        snap_num = int(key)
                    snapshots.append(snap_num)
                except ValueError:
                    continue
            
            return sorted(snapshots)
            
    except Exception as e:
        print(f"Error reading snapshot list from {directory}: {e}")
        return []


def find_closest_snapshot(target_z, available_snapshots, tolerance=0.2):
    """Find the snapshot closest to the target redshift"""
    best_snap = None
    best_diff = float('inf')
    
    for snap in available_snapshots:
        z_snap = get_redshift_from_snapshot(snap)
        diff = abs(z_snap - target_z)
        
        if diff < best_diff and diff <= tolerance:
            best_diff = diff
            best_snap = snap
    
    return best_snap


def create_debug_ks_plots(galaxy_types='all', save_path=None):
    """Create debug K-S relation plots with detailed output"""
    
    print("=" * 70)
    print("DEBUG K-S RELATION ANALYSIS")
    print("=" * 70)
    print(f"Radius choice: {RADIUS_CHOICE}")
    print(f"sSFR cut: {sSFRcut}")
    print(f"Mass min: {mass_min:.0e} M☉")
    print(f"SFR min: {sfr_min:.0e} M☉/yr")
    print("=" * 70)
    
    # Check directories
    model_configs_valid = []
    for model_config in MODEL_CONFIGS:
        directory = model_config['dir']
        
        if not os.path.exists(directory):
            print(f"Error: Directory {directory} does not exist!")
            continue
        
        available_snapshots = get_available_snapshots(directory)
        if not available_snapshots:
            print(f"Warning: No snapshots found in {directory}")
            continue
        
        model_configs_valid.append(model_config)
    
    if not model_configs_valid:
        raise ValueError("No valid model directories with snapshots found")
    
    # Target redshift
    target_z = 0.0
    
    # Create plots for each gas type
    gas_types = [('h2', 'Molecular'), ('hi', 'Atomic')]
    
    for gas_type, gas_name in gas_types:
        print(f"\n" + "=" * 50)
        print(f"ANALYZING {gas_name.upper()} GAS AT z = {target_z}")
        print("=" * 50)
        
        # Create figure
        fig, ax = plt.subplots(1, 1, figsize=(10, 7))
        
        plot_data = []
        
        for model_config in model_configs_valid:
            model_name = model_config['name']
            directory = model_config['dir']
            
            print(f"\n--- PROCESSING {model_name} ---")
            
            # Find best snapshot
            available_snapshots = get_available_snapshots(directory)
            best_snap = find_closest_snapshot(target_z, available_snapshots)
            
            if best_snap is None:
                print(f"  No suitable snapshot found for {model_name} at z={target_z}")
                continue
            
            z_actual = get_redshift_from_snapshot(best_snap)
            snap_str = f'Snap_{best_snap}'
            
            print(f"  Using {snap_str} (z={z_actual:.2f})")
            
            try:
                # Load data
                data = read_hdf_for_ks_relation_fixed(
                    directory, 
                    snap_num=snap_str, 
                    galaxy_types=galaxy_types
                )
                
                if data is None:
                    print(f"    No data loaded for {model_name}")
                    continue
                
                # Calculate surface densities
                ks_data = calculate_ks_surface_densities_fixed(
                    data, 
                    radius_choice=RADIUS_CHOICE,
                    ssfr_cut=sSFRcut
                )
                
                if ks_data is None:
                    print(f"    No star-forming galaxies for {model_name}")
                    continue
                
                # Get the right gas type and add debug info
                if gas_type == 'h2':
                    sigma_gas = ks_data['sigma_gas_h2']
                    gas_label = r'$\log_{10} \Sigma_{\mathrm{H_2}}\ (M_{\odot}\ \mathrm{pc^{-2}})$'
                elif gas_type == 'hi':
                    sigma_gas = ks_data['sigma_gas_hi']
                    gas_label = r'$\log_{10} \Sigma_{\mathrm{HI}}\ (M_{\odot}\ \mathrm{pc^{-2}})$'
                
                # DEBUG: Print detailed information
                print(f"    DEBUG {model_name} {gas_type.upper()}:")
                if sigma_gas is not None:
                    print(f"      Median SFR surface density: {np.median(ks_data['sigma_sfr']):.2e} M☉/yr/pc²")
                    print(f"      Median {gas_type.upper()} surface density: {np.median(sigma_gas):.2e} M☉/pc²")
                    print(f"      Median radius used: {np.median(ks_data['radius_used_kpc']):.1f} kpc")
                    print(f"      N star-forming galaxies: {len(sigma_gas)}")
                    
                    # Check some individual galaxies for sanity
                    print(f"      INDIVIDUAL GALAXY EXAMPLES:")
                    sf_indices = np.arange(len(data['sfr_disk']))
                    sample_indices = np.random.choice(sf_indices, min(3, len(sf_indices)), replace=False)
                    for i, idx in enumerate(sample_indices):
                        sfr_total = (data['sfr_disk'] + data['sfr_bulge'])[idx]
                        if gas_type == 'h2':
                            gas_mass = data['h2_gas'][idx] * 1e10 / Hubble_h  # Convert to M☉
                        else:
                            gas_mass = data['hi_gas'][idx] * 1e10 / Hubble_h  # Convert to M☉
                        radius_kpc = data['disk_radius'][idx] * 1e3 / Hubble_h  # Convert to kpc
                        stellar_mass = data['stellar_mass'][idx]
                        
                        print(f"        Galaxy {i+1}: SFR={sfr_total:.3f} M☉/yr, "
                              f"{gas_type.upper()}={gas_mass:.1e} M☉, "
                              f"R={radius_kpc:.1f} kpc, M*={stellar_mass:.1e} M☉")
                else:
                    print(f"    No {gas_type} data available for {model_name}")
                    continue
                
                sigma_sfr = ks_data['sigma_sfr']
                
                # Filter data for plotting
                gas_mask = (sigma_gas > 1.0) & np.isfinite(sigma_gas)
                sfr_mask = (sigma_sfr > 1e-5) & np.isfinite(sigma_sfr)
                valid_mask = gas_mask & sfr_mask
                
                if np.sum(valid_mask) < 10:
                    print(f"    Too few valid points for {gas_type} gas plotting")
                    continue
                
                log_sigma_gas = np.log10(sigma_gas[valid_mask])
                log_sigma_sfr = np.log10(sigma_sfr[valid_mask])
                
                print(f"      log Σ_SFR range: {np.min(log_sigma_sfr):.2f} to {np.max(log_sigma_sfr):.2f}")
                print(f"      log Σ_{gas_type} range: {np.min(log_sigma_gas):.2f} to {np.max(log_sigma_gas):.2f}")
                
                # Apply dilution for plotting performance
                if len(log_sigma_gas) > dilute:
                    indices = np.random.choice(len(log_sigma_gas), size=dilute, replace=False)
                    plot_gas = log_sigma_gas[indices]
                    plot_sfr = log_sigma_sfr[indices]
                    print(f"      Diluted to {dilute} points for plotting")
                else:
                    plot_gas = log_sigma_gas
                    plot_sfr = log_sigma_sfr
                
                # Plot scatter points with increased transparency
                ax.scatter(plot_gas, plot_sfr, 
                          marker=model_config['marker'], 
                          s=model_config['markersize']**2, 
                          c=model_config['color'], 
                          alpha=model_config['alpha'], 
                          rasterized=True, 
                          zorder=1, 
                          label=f"{model_config['name']}")
                
                # Fit K-S relation
                fit_result = fit_ks_relation_improved(sigma_gas, sigma_sfr, gas_type)
                
                if fit_result is not None:
                    # Plot fit line
                    gas_range = np.linspace(np.min(log_sigma_gas), np.max(log_sigma_gas), 100)
                    fit_line = fit_result['slope'] * gas_range + fit_result['intercept']
                    
                    ax.plot(gas_range, fit_line, 
                           color=model_config['color'], 
                           linestyle=model_config['linestyle'], 
                           linewidth=model_config['linewidth'], 
                           alpha=1.0,  # Keep fit lines fully opaque
                           zorder=3)
                
                # Clean up
                del data, ks_data
                gc.collect()
                
            except Exception as e:
                print(f"    Error processing {model_name}: {e}")
                import traceback
                traceback.print_exc()
                continue
        
        # Add observational relations
        plot_observational_relations(ax, gas_type)
        
        # Set labels and formatting
        ax.set_xlabel(gas_label, fontsize=14)
        ax.set_ylabel(r'$\log_{10} \Sigma_{\mathrm{SFR}}\ (M_{\odot}\ \mathrm{yr^{-1}}\ \mathrm{pc^{-2}})$', fontsize=14)
        ax.set_title(f'{gas_name} Gas K-S Relation (z = 0, {RADIUS_CHOICE} radius)', fontsize=16)
        
        # Set axis limits - wider range to see everything
        ax.set_xlim(-1, 3)    
        ax.set_ylim(-6, -1)
        
        # Add grid for easier reading
        ax.grid(True, alpha=0.3)
        
        # Add legend - try upper left if bottom right is crowded
        ax.legend(loc='upper left', fontsize=11, frameon=True, fancybox=True, shadow=True)
        
        plt.tight_layout()
        
        if save_path:
            gas_save_path = save_path.replace('.pdf', f'_{gas_type}_debug_{RADIUS_CHOICE}.pdf')
            print(f"\nSaving debug {gas_type} gas K-S plot to {gas_save_path}")
            plt.savefig(gas_save_path, dpi=300, bbox_inches='tight')
        
        plt.show()


# Main execution
if __name__ == "__main__":
    print("DEBUG Kennicutt-Schmidt Relation Analysis")
    print("=" * 70)
    print("🔍 This version includes:")
    print("  • Detailed debug output for all calculations")
    print("  • Multiple observational comparisons")
    print("  • Individual galaxy examples")
    print("  • Surface density diagnostics")
    print("  • Easy radius choice modification")
    print("=" * 70)
    
    try:
        # Ensure output directory exists
        os.makedirs(OutputDir, exist_ok=True)
        
        create_debug_ks_plots(
            galaxy_types='all',
            save_path=OutputDir + 'sage_kennicutt_schmidt_debug' + OutputFormat
        )
        
        print("\n" + "=" * 70)
        print("DEBUG K-S relation analysis complete!")
        print("=" * 70)
        
    except Exception as e:
        print(f"Error during analysis: {e}")
        import traceback
        traceback.print_exc()