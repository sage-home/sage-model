#!/usr/bin/env python3
"""
Comprehensive FFB vs No-FFB Bulge-Disk Analysis
================================================

This script performs detailed comparison of bulge-disk properties between
FFB=ON and FFB=OFF runs to understand how the feedback-free burst mode
affects galaxy structure and morphology.

Analyses performed:
1. Bulge-to-Total ratio (B/T) evolution with redshift
2. Disk mass evolution
3. Bulge mass evolution (total, merger, instability components)
4. Bulge formation pathway changes (merger vs instability fractions)
5. Morphological distributions (early-type vs late-type)
6. Bulge size evolution
7. Disk size evolution
8. Mass-size relations

Author: Generated for FFB validation
Date: 2024-11-28
"""

import h5py
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
from pathlib import Path
from scipy.interpolate import interp1d
import sys

# ============================================================================
# CONFIGURATION
# ============================================================================

# Hubble parameter (Millennium)
HUBBLE_H = 0.73

# Snapshot to redshift mapping (approximate for Millennium)
# You can refine this based on your actual snapshot list
def snapshot_to_redshift(snap):
    """Convert snapshot number to approximate redshift"""
    # Millennium: snap 63 = z=0, roughly linear
    return max(0, (63 - snap) * 0.15)

# Mass thresholds for analysis
MIN_STELLAR_MASS = 1e8  # Msun
MIN_BULGE_MASS = 1e7    # Msun

# ============================================================================
# DATA LOADING FUNCTIONS
# ============================================================================

def load_all_snapshots(filepath):
    """
    Load data from all snapshots in HDF5 file
    
    Returns:
        dict: Dictionary with snapshot numbers as keys, each containing galaxy data
    """
    print(f"Loading data from: {filepath}")
    
    data = {}
    
    with h5py.File(filepath, 'r') as f:
        # Get all snapshot keys
        snap_keys = sorted([k for k in f.keys() if k.startswith('Snap_')])
        
        print(f"Found {len(snap_keys)} snapshots")
        
        for snap_key in snap_keys:
            snap_num = int(snap_key.split('_')[1])
            snap_group = f[snap_key]
            
            # Load all relevant fields
            snap_data = {
                'SnapNum': snap_num,
                'Redshift': snapshot_to_redshift(snap_num),
                'Type': snap_group['Type'][:],
                'StellarMass': snap_group['StellarMass'][:] * 1e10 / HUBBLE_H,  # Msun
                'BulgeMass': snap_group['BulgeMass'][:] * 1e10 / HUBBLE_H,  # Msun
                'ColdGas': snap_group['ColdGas'][:] * 1e10 / HUBBLE_H,  # Msun
                'Mvir': snap_group['Mvir'][:] * 1e10 / HUBBLE_H,  # Msun
            }
            
            # Optional fields (may not exist in all versions)
            if 'MergerBulgeMass' in snap_group:
                snap_data['MergerBulgeMass'] = snap_group['MergerBulgeMass'][:] * 1e10 / HUBBLE_H
            else:
                snap_data['MergerBulgeMass'] = np.zeros_like(snap_data['StellarMass'])
            
            if 'InstabilityBulgeMass' in snap_group:
                snap_data['InstabilityBulgeMass'] = snap_group['InstabilityBulgeMass'][:] * 1e10 / HUBBLE_H
            else:
                snap_data['InstabilityBulgeMass'] = np.zeros_like(snap_data['StellarMass'])
            
            if 'FFBRegime' in snap_group:
                snap_data['FFBRegime'] = snap_group['FFBRegime'][:]
            else:
                snap_data['FFBRegime'] = np.zeros_like(snap_data['Type'])
            
            if 'DiskScaleRadius' in snap_group:
                snap_data['DiskScaleRadius'] = snap_group['DiskScaleRadius'][:] * 1e3 / HUBBLE_H  # kpc
            else:
                snap_data['DiskScaleRadius'] = np.zeros_like(snap_data['StellarMass'])
            
            if 'BulgeScaleRadius' in snap_group:
                snap_data['BulgeScaleRadius'] = snap_group['BulgeScaleRadius'][:] * 1e3 / HUBBLE_H  # kpc
            else:
                snap_data['BulgeScaleRadius'] = np.zeros_like(snap_data['StellarMass'])
            
            # Calculate derived quantities
            snap_data['DiskMass'] = snap_data['StellarMass'] - snap_data['BulgeMass']
            snap_data['BulgeToTotal'] = np.zeros_like(snap_data['StellarMass'])
            
            # Calculate B/T only where stellar mass > 0
            stellar_mask = snap_data['StellarMass'] > 0
            snap_data['BulgeToTotal'][stellar_mask] = (
                snap_data['BulgeMass'][stellar_mask] / snap_data['StellarMass'][stellar_mask]
            )
            
            # Classify morphology
            # Early-type: B/T > 0.5, Late-type: B/T < 0.5
            snap_data['Morphology'] = np.where(snap_data['BulgeToTotal'] > 0.5, 
                                               'Early', 'Late')
            
            data[snap_num] = snap_data
            
            if snap_num % 10 == 0:
                print(f"  Loaded Snap_{snap_num:02d} (z={snap_data['Redshift']:.2f}): "
                      f"{len(snap_data['Type'])} galaxies")
    
    return data

# ============================================================================
# ANALYSIS FUNCTIONS
# ============================================================================

def calculate_bt_evolution(data_ffb_on, data_ffb_off):
    """
    Calculate B/T evolution with redshift
    
    Returns:
        dict: Statistics for each snapshot
    """
    print("\nCalculating B/T evolution...")
    
    results = {
        'redshift': [],
        'median_bt_ffb_on': [],
        'q16_bt_ffb_on': [],
        'q84_bt_ffb_on': [],
        'median_bt_ffb_off': [],
        'q16_bt_ffb_off': [],
        'q84_bt_ffb_off': [],
        'median_bt_ffb_galaxies': [],  # Only FFB galaxies from FFB=ON run
        'n_galaxies_on': [],
        'n_galaxies_off': [],
        'n_ffb_galaxies': [],
    }
    
    for snap_num in sorted(data_ffb_on.keys()):
        if snap_num not in data_ffb_off:
            continue
        
        snap_on = data_ffb_on[snap_num]
        snap_off = data_ffb_off[snap_num]
        
        # Select centrals with sufficient stellar mass AND non-zero bulge
        # This avoids artificially low B/T from disk-only galaxies
        mask_on = ((snap_on['Type'] == 0) & 
                   (snap_on['StellarMass'] > MIN_STELLAR_MASS) & 
                   (snap_on['BulgeMass'] > MIN_BULGE_MASS))
        mask_off = ((snap_off['Type'] == 0) & 
                    (snap_off['StellarMass'] > MIN_STELLAR_MASS) & 
                    (snap_off['BulgeMass'] > MIN_BULGE_MASS))
        
        bt_on = snap_on['BulgeToTotal'][mask_on]
        bt_off = snap_off['BulgeToTotal'][mask_off]
        
        # FFB galaxies specifically
        ffb_mask = mask_on & (snap_on['FFBRegime'] == 1)
        bt_ffb = snap_on['BulgeToTotal'][ffb_mask]
        
        if len(bt_on) > 10 and len(bt_off) > 10:
            results['redshift'].append(snap_on['Redshift'])
            
            # FFB=ON statistics
            results['median_bt_ffb_on'].append(np.median(bt_on))
            results['q16_bt_ffb_on'].append(np.percentile(bt_on, 16))
            results['q84_bt_ffb_on'].append(np.percentile(bt_on, 84))
            
            # FFB=OFF statistics
            results['median_bt_ffb_off'].append(np.median(bt_off))
            results['q16_bt_ffb_off'].append(np.percentile(bt_off, 16))
            results['q84_bt_ffb_off'].append(np.percentile(bt_off, 84))
            
            # FFB galaxies only
            if len(bt_ffb) > 0:
                results['median_bt_ffb_galaxies'].append(np.median(bt_ffb))
            else:
                results['median_bt_ffb_galaxies'].append(np.nan)
            
            results['n_galaxies_on'].append(len(bt_on))
            results['n_galaxies_off'].append(len(bt_off))
            results['n_ffb_galaxies'].append(np.sum(ffb_mask))
    
    # Convert to arrays
    for key in results:
        results[key] = np.array(results[key])
    
    return results

def calculate_mass_evolution(data_ffb_on, data_ffb_off):
    """
    Calculate stellar, bulge, and disk mass evolution
    
    Returns:
        dict: Mass evolution statistics
    """
    print("\nCalculating mass evolution...")
    
    results = {
        'redshift': [],
        'stellar_mass_on': [],
        'stellar_mass_off': [],
        'bulge_mass_on': [],
        'bulge_mass_off': [],
        'disk_mass_on': [],
        'disk_mass_off': [],
        'merger_bulge_on': [],
        'merger_bulge_off': [],
        'instability_bulge_on': [],
        'instability_bulge_off': [],
        'stellar_mass_ffb': [],  # FFB galaxies only
        'bulge_mass_ffb': [],
    }
    
    for snap_num in sorted(data_ffb_on.keys()):
        if snap_num not in data_ffb_off:
            continue
        
        snap_on = data_ffb_on[snap_num]
        snap_off = data_ffb_off[snap_num]
        
        # Select centrals with sufficient mass
        mask_on = (snap_on['Type'] == 0) & (snap_on['StellarMass'] > MIN_STELLAR_MASS)
        mask_off = (snap_off['Type'] == 0) & (snap_off['StellarMass'] > MIN_STELLAR_MASS)
        ffb_mask = mask_on & (snap_on['FFBRegime'] == 1)
        
        if np.sum(mask_on) > 10 and np.sum(mask_off) > 10:
            results['redshift'].append(snap_on['Redshift'])
            
            # Median masses
            results['stellar_mass_on'].append(np.median(snap_on['StellarMass'][mask_on]))
            results['stellar_mass_off'].append(np.median(snap_off['StellarMass'][mask_off]))
            results['bulge_mass_on'].append(np.median(snap_on['BulgeMass'][mask_on]))
            results['bulge_mass_off'].append(np.median(snap_off['BulgeMass'][mask_off]))
            results['disk_mass_on'].append(np.median(snap_on['DiskMass'][mask_on]))
            results['disk_mass_off'].append(np.median(snap_off['DiskMass'][mask_off]))
            
            # Bulge components
            results['merger_bulge_on'].append(np.median(snap_on['MergerBulgeMass'][mask_on]))
            results['merger_bulge_off'].append(np.median(snap_off['MergerBulgeMass'][mask_off]))
            results['instability_bulge_on'].append(np.median(snap_on['InstabilityBulgeMass'][mask_on]))
            results['instability_bulge_off'].append(np.median(snap_off['InstabilityBulgeMass'][mask_off]))
            
            # FFB galaxies
            if np.sum(ffb_mask) > 0:
                results['stellar_mass_ffb'].append(np.median(snap_on['StellarMass'][ffb_mask]))
                results['bulge_mass_ffb'].append(np.median(snap_on['BulgeMass'][ffb_mask]))
            else:
                results['stellar_mass_ffb'].append(np.nan)
                results['bulge_mass_ffb'].append(np.nan)
    
    for key in results:
        results[key] = np.array(results[key])
    
    return results

def calculate_morphology_fractions(data_ffb_on, data_ffb_off):
    """
    Calculate fraction of early-type vs late-type galaxies
    
    Returns:
        dict: Morphology fraction evolution
    """
    print("\nCalculating morphology fractions...")
    
    results = {
        'redshift': [],
        'early_frac_on': [],
        'early_frac_off': [],
        'late_frac_on': [],
        'late_frac_off': [],
    }
    
    for snap_num in sorted(data_ffb_on.keys()):
        if snap_num not in data_ffb_off:
            continue
        
        snap_on = data_ffb_on[snap_num]
        snap_off = data_ffb_off[snap_num]
        
        mask_on = (snap_on['Type'] == 0) & (snap_on['StellarMass'] > MIN_STELLAR_MASS)
        mask_off = (snap_off['Type'] == 0) & (snap_off['StellarMass'] > MIN_STELLAR_MASS)
        
        if np.sum(mask_on) > 10 and np.sum(mask_off) > 10:
            early_on = np.sum(snap_on['BulgeToTotal'][mask_on] > 0.5)
            early_off = np.sum(snap_off['BulgeToTotal'][mask_off] > 0.5)
            
            results['redshift'].append(snap_on['Redshift'])
            results['early_frac_on'].append(early_on / np.sum(mask_on))
            results['early_frac_off'].append(early_off / np.sum(mask_off))
            results['late_frac_on'].append(1 - early_on / np.sum(mask_on))
            results['late_frac_off'].append(1 - early_off / np.sum(mask_off))
    
    for key in results:
        results[key] = np.array(results[key])
    
    return results

def calculate_bulge_pathway_fractions(data_ffb_on, data_ffb_off):
    """
    Calculate the fraction of bulge mass formed via mergers vs disk instabilities
    """
    print("Calculating bulge pathway fractions...")
    
    results = {
        'redshift': [],
        'merger_frac_ffb_on': [],
        'instability_frac_ffb_on': [],
        'merger_frac_ffb_off': [],
        'instability_frac_ffb_off': [],
    }
    
    for snap_num in sorted(data_ffb_on.keys()):
        if snap_num not in data_ffb_off:
            continue
        
        snap_on = data_ffb_on[snap_num]
        snap_off = data_ffb_off[snap_num]
        
        # Filter: centrals with M* > 10^9 Msun and M_bulge > 10^7 Msun
        mask_on = ((snap_on['Type'] == 0) & 
                   (snap_on['StellarMass'] > 1e9) & 
                   (snap_on['BulgeMass'] > 1e7))
        mask_off = ((snap_off['Type'] == 0) & 
                    (snap_off['StellarMass'] > 1e9) & 
                    (snap_off['BulgeMass'] > 1e7))
        
        if np.sum(mask_on) > 10 and np.sum(mask_off) > 10:
            # FFB=ON
            total_bulge_on = np.sum(snap_on['BulgeMass'][mask_on])
            merger_bulge_on = np.sum(snap_on['MergerBulgeMass'][mask_on])
            instability_bulge_on = np.sum(snap_on['InstabilityBulgeMass'][mask_on])
            
            # FFB=OFF
            total_bulge_off = np.sum(snap_off['BulgeMass'][mask_off])
            merger_bulge_off = np.sum(snap_off['MergerBulgeMass'][mask_off])
            instability_bulge_off = np.sum(snap_off['InstabilityBulgeMass'][mask_off])
            
            results['redshift'].append(snap_on['Redshift'])
            
            if total_bulge_on > 0:
                results['merger_frac_ffb_on'].append(merger_bulge_on / total_bulge_on)
                results['instability_frac_ffb_on'].append(instability_bulge_on / total_bulge_on)
            else:
                results['merger_frac_ffb_on'].append(0.0)
                results['instability_frac_ffb_on'].append(0.0)
            
            if total_bulge_off > 0:
                results['merger_frac_ffb_off'].append(merger_bulge_off / total_bulge_off)
                results['instability_frac_ffb_off'].append(instability_bulge_off / total_bulge_off)
            else:
                results['merger_frac_ffb_off'].append(0.0)
                results['instability_frac_ffb_off'].append(0.0)
    
    # Convert to arrays
    for key in results:
        results[key] = np.array(results[key])
    
    return results

def calculate_global_bt_evolution(data_ffb_on, data_ffb_off):
    """
    Calculate global bulge-to-total ratio evolution
    (total bulge mass / total stellar mass for all galaxies)
    
    Returns:
        dict: Global B/T evolution for both runs
    """
    print("\nCalculating global B/T evolution...")
    
    results = {
        'redshift': [],
        'global_bt_ffb_on': [],
        'global_bt_ffb_off': [],
    }
    
    for snap_num in sorted(data_ffb_on.keys()):
        if snap_num not in data_ffb_off:
            continue
        
        snap_on = data_ffb_on[snap_num]
        snap_off = data_ffb_off[snap_num]
        
        # Filter for massive galaxies to avoid resolution noise (M* > 10^9 Msun)
        mask_on = snap_on['StellarMass'] > 1e9
        mask_off = snap_off['StellarMass'] > 1e9
        
        if np.sum(mask_on) > 10 and np.sum(mask_off) > 10:
            # Calculate global B/T (total bulge / total stellar)
            total_stellar_on = np.sum(snap_on['StellarMass'][mask_on])
            total_bulge_on = np.sum(snap_on['BulgeMass'][mask_on])
            
            total_stellar_off = np.sum(snap_off['StellarMass'][mask_off])
            total_bulge_off = np.sum(snap_off['BulgeMass'][mask_off])
            
            bt_on = total_bulge_on / total_stellar_on if total_stellar_on > 0 else 0
            bt_off = total_bulge_off / total_stellar_off if total_stellar_off > 0 else 0
            
            results['redshift'].append(snap_on['Redshift'])
            results['global_bt_ffb_on'].append(bt_on)
            results['global_bt_ffb_off'].append(bt_off)
            
            if snap_num % 10 == 0:
                print(f"  z={snap_on['Redshift']:.2f}: B/T(FFB=ON)={bt_on:.4f}, B/T(FFB=OFF)={bt_off:.4f}")
    
    for key in results:
        results[key] = np.array(results[key])
    
    return results

# ============================================================================
# PLOTTING FUNCTIONS
# ============================================================================

def create_global_bt_plot(global_bt_results, output_file):
    """
    Create standalone plot of global bulge-to-total ratio evolution
    Similar to your original plot but comparing FFB=ON vs FFB=OFF
    
    NOTE: This shows the GLOBAL average (total bulge / total stellar mass)
    not the distribution of individual galaxy B/T values
    """
    print("\nCreating global B/T evolution plot...")
    
    fig, ax = plt.subplots(1, 1, figsize=(10, 7))
    
    z = global_bt_results['redshift']
    bt_on = global_bt_results['global_bt_ffb_on']
    bt_off = global_bt_results['global_bt_ffb_off']
    
    # Plot both runs
    ax.plot(z, bt_on, 'o-', color='#D62728', linewidth=2.5, markersize=7,
            label='FFB=ON', markeredgecolor='black', markeredgewidth=0.5)
    ax.plot(z, bt_off, 's-', color='#1F77B4', linewidth=2.5, markersize=7,
            label='FFB=OFF', markeredgecolor='black', markeredgewidth=0.5)
    
    # Calculate and show difference
    diff = bt_on - bt_off
    ax_twin = ax.twinx()
    ax_twin.plot(z, diff * 100, '^-', color='purple', linewidth=1.5, 
                markersize=5, alpha=0.6, label='Difference (%)')
    ax_twin.axhline(0, color='gray', linestyle='--', alpha=0.5)
    ax_twin.set_ylabel('Difference (FFB=ON - FFB=OFF) [%]', fontsize=12, color='purple')
    ax_twin.tick_params(axis='y', labelcolor='purple')
    
    # Formatting
    ax.set_xlabel('Redshift', fontsize=13)
    ax.set_ylabel(r'Global Bulge-to-Total Ratio ($\Sigma M_{\mathrm{bulge}}/\Sigma M_{\mathrm{stars}}$)', 
                  fontsize=13)
    ax.set_title('Global Bulge-to-Total Mass Ratio Evolution\n' + 
                 r'(for galaxies with $M_* > 10^9$ M$_\odot$)',
                 fontsize=14, fontweight='bold', pad=15)
    
    # Set limits
    ax.set_xlim(z.max(), -0.2)  # Invert x-axis (high-z to low-z)
    ax.set_ylim(0, 1.0)
    
    # Grid
    ax.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
    
    # Legends
    ax.legend(loc='upper left', fontsize=11, framealpha=0.9)
    ax_twin.legend(loc='upper right', fontsize=10, framealpha=0.9)
    
    # Add text box with key statistics
    # Find max difference
    max_diff_idx = np.argmax(np.abs(diff))
    max_diff_z = z[max_diff_idx]
    max_diff_val = diff[max_diff_idx] * 100
    
    textstr = f'Maximum difference:\n' \
              f'Δ(B/T) = {max_diff_val:+.2f}%\n' \
              f'at z = {max_diff_z:.2f}'
    
    props = dict(boxstyle='round', facecolor='wheat', alpha=0.8)
    ax.text(0.98, 0.02, textstr, transform=ax.transAxes, fontsize=10,
            verticalalignment='bottom', horizontalalignment='right', bbox=props)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=200, bbox_inches='tight')
    print(f"✓ Saved: {output_file}")
    plt.close()

def create_bt_distribution_plot(data_ffb_on, data_ffb_off, output_file):
    """
    Create plot showing the DISTRIBUTION of B/T ratios as a function of redshift
    Shows median, 16th-84th percentiles, and 5th-95th percentiles
    
    This shows how individual galaxy B/T values are distributed at each z
    """
    print("\nCreating B/T distribution evolution plot...")
    
    results_on = {
        'redshift': [],
        'median': [],
        'p16': [], 'p84': [],
        'p05': [], 'p95': [],
    }
    
    results_off = {
        'redshift': [],
        'median': [],
        'p16': [], 'p84': [],
        'p05': [], 'p95': [],
    }
    
    # Calculate percentiles for each snapshot
    for snap_num in sorted(data_ffb_on.keys()):
        if snap_num not in data_ffb_off:
            continue
        
        snap_on = data_ffb_on[snap_num]
        snap_off = data_ffb_off[snap_num]
        
        # Filter: centrals with M* > 10^9 Msun and M_bulge > 10^7 Msun
        mask_on = ((snap_on['Type'] == 0) & 
                   (snap_on['StellarMass'] > 1e9) & 
                   (snap_on['BulgeMass'] > 1e7))
        mask_off = ((snap_off['Type'] == 0) & 
                    (snap_off['StellarMass'] > 1e9) & 
                    (snap_off['BulgeMass'] > 1e7))
        
        bt_on = snap_on['BulgeToTotal'][mask_on]
        bt_off = snap_off['BulgeToTotal'][mask_off]
        
        if len(bt_on) > 20 and len(bt_off) > 20:  # Need sufficient statistics
            z = snap_on['Redshift']
            
            # FFB=ON percentiles
            results_on['redshift'].append(z)
            results_on['median'].append(np.median(bt_on))
            results_on['p16'].append(np.percentile(bt_on, 16))
            results_on['p84'].append(np.percentile(bt_on, 84))
            results_on['p05'].append(np.percentile(bt_on, 5))
            results_on['p95'].append(np.percentile(bt_on, 95))
            
            # FFB=OFF percentiles
            results_off['redshift'].append(z)
            results_off['median'].append(np.median(bt_off))
            results_off['p16'].append(np.percentile(bt_off, 16))
            results_off['p84'].append(np.percentile(bt_off, 84))
            results_off['p05'].append(np.percentile(bt_off, 5))
            results_off['p95'].append(np.percentile(bt_off, 95))
    
    # Convert to arrays
    for key in results_on:
        results_on[key] = np.array(results_on[key])
        results_off[key] = np.array(results_off[key])
    
    # Create plot
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 10), sharex=True)
    
    z_on = results_on['redshift']
    z_off = results_off['redshift']
    
    # ========================================================================
    # Top panel: FFB=ON vs FFB=OFF median with 16-84 percentile bands
    # ========================================================================
    
    # FFB=ON
    ax1.plot(z_on, results_on['median'], 'o-', color='#D62728', linewidth=2.5, 
             markersize=6, label='FFB=ON (median)', markeredgecolor='black', 
             markeredgewidth=0.5, zorder=3)
    ax1.fill_between(z_on, results_on['p16'], results_on['p84'], 
                     color='#D62728', alpha=0.3, label='FFB=ON (16-84%)', zorder=1)
    
    # FFB=OFF
    ax1.plot(z_off, results_off['median'], 's-', color='#1F77B4', linewidth=2.5,
             markersize=6, label='FFB=OFF (median)', markeredgecolor='black',
             markeredgewidth=0.5, zorder=3)
    ax1.fill_between(z_off, results_off['p16'], results_off['p84'],
                     color='#1F77B4', alpha=0.3, label='FFB=OFF (16-84%)', zorder=1)
    
    ax1.set_ylabel(r'Bulge-to-Total Ratio ($M_{\mathrm{bulge}}/M_{\mathrm{stars}}$)',
                   fontsize=13)
    ax1.set_title(r'Distribution of B/T Ratios vs Redshift' + '\n' +
                  r'(galaxies with $M_* > 10^9$ M$_\odot$ and $M_{\mathrm{bulge}} > 10^7$ M$_\odot$)',
                  fontsize=14, fontweight='bold', pad=15)
    ax1.set_ylim(0, 1.0)
    ax1.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
    ax1.legend(loc='best', fontsize=10, framealpha=0.9, ncol=2)
    
    # ========================================================================
    # Bottom panel: Difference in median B/T
    # ========================================================================
    
    # Need to interpolate to common redshift grid for difference
    from scipy.interpolate import interp1d
    
    if len(z_on) > 3 and len(z_off) > 3:
        # Create common redshift grid
        z_common = np.linspace(max(z_on.min(), z_off.min()), 
                              min(z_on.max(), z_off.max()), 50)
        
        # Interpolate
        interp_on = interp1d(z_on, results_on['median'], kind='linear', 
                            fill_value='extrapolate')
        interp_off = interp1d(z_off, results_off['median'], kind='linear',
                             fill_value='extrapolate')
        
        median_on_interp = interp_on(z_common)
        median_off_interp = interp_off(z_common)
        
        # Calculate difference
        diff = median_on_interp - median_off_interp
        
        ax2.plot(z_common, diff, 'o-', color='purple', linewidth=2, 
                markersize=5, label='Δ(B/T) = B/T(FFB=ON) - B/T(FFB=OFF)')
        ax2.axhline(0, color='gray', linestyle='--', alpha=0.5, linewidth=1.5)
        ax2.fill_between(z_common, 0, diff, where=(diff > 0), 
                        color='red', alpha=0.2, label='FFB increases B/T')
        ax2.fill_between(z_common, 0, diff, where=(diff < 0),
                        color='blue', alpha=0.2, label='FFB decreases B/T')
        
        # Find maximum difference
        max_diff_idx = np.argmax(np.abs(diff))
        max_diff_z = z_common[max_diff_idx]
        max_diff_val = diff[max_diff_idx]
        
        ax2.plot(max_diff_z, max_diff_val, 'r*', markersize=20, 
                markeredgecolor='black', markeredgewidth=1, zorder=5,
                label=f'Max |Δ| = {abs(max_diff_val):.3f} at z={max_diff_z:.2f}')
    
    ax2.set_xlabel('Redshift', fontsize=13)
    ax2.set_ylabel(r'Δ(B/T)', fontsize=13)
    ax2.set_title('Difference in Median B/T Ratio', fontsize=12, fontweight='bold')
    ax2.grid(True, alpha=0.3, linestyle='--', linewidth=0.5)
    ax2.legend(loc='best', fontsize=10, framealpha=0.9)
    
    # Invert x-axis (high-z to low-z)
    ax2.set_xlim(max(z_on.max(), z_off.max()), -0.2)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=200, bbox_inches='tight')
    print(f"✓ Saved: {output_file}")
    plt.close()
    
    return results_on, results_off

def create_comprehensive_plot(bt_results, mass_results, morph_results, pathway_results, output_file):
    """
    Create comprehensive multi-panel plot showing all analyses
    """
    print("\nCreating comprehensive plot...")
    
    fig = plt.figure(figsize=(18, 12))
    gs = GridSpec(3, 3, figure=fig, hspace=0.3, wspace=0.3)
    
    # Color scheme
    color_on = '#D62728'   # Red
    color_off = '#1F77B4'  # Blue
    color_ffb = '#FF7F0E'  # Orange
    
    # ========================================================================
    # Panel 1: B/T Evolution
    # ========================================================================
    ax1 = fig.add_subplot(gs[0, 0])
    
    z = bt_results['redshift']
    ax1.plot(z, bt_results['median_bt_ffb_on'], 'o-', color=color_on, 
             label='FFB=ON', lw=2, markersize=4)
    ax1.fill_between(z, bt_results['q16_bt_ffb_on'], bt_results['q84_bt_ffb_on'],
                     color=color_on, alpha=0.2)
    
    ax1.plot(z, bt_results['median_bt_ffb_off'], 's-', color=color_off,
             label='FFB=OFF', lw=2, markersize=4)
    ax1.fill_between(z, bt_results['q16_bt_ffb_off'], bt_results['q84_bt_ffb_off'],
                     color=color_off, alpha=0.2)
    
    # Plot FFB galaxies specifically (where they exist)
    valid = ~np.isnan(bt_results['median_bt_ffb_galaxies'])
    if np.sum(valid) > 0:
        ax1.plot(z[valid], bt_results['median_bt_ffb_galaxies'][valid], '^-',
                color=color_ffb, label='FFB galaxies only', lw=2, markersize=6,
                markeredgecolor='black', markeredgewidth=0.5)
    
    ax1.set_xlabel('Redshift', fontsize=11)
    ax1.set_ylabel('Bulge-to-Total Ratio (B/T)', fontsize=11)
    ax1.set_title('B/T Evolution', fontsize=12, fontweight='bold')
    ax1.legend(loc='best', fontsize=9)
    ax1.grid(alpha=0.3)
    ax1.set_xlim(z.max(), -0.2)
    ax1.set_ylim(0, 1)
    
    # ========================================================================
    # Panel 2: Stellar Mass Evolution
    # ========================================================================
    ax2 = fig.add_subplot(gs[0, 1])
    
    z = mass_results['redshift']
    ax2.semilogy(z, mass_results['stellar_mass_on'], 'o-', color=color_on,
                label='FFB=ON', lw=2, markersize=4)
    ax2.semilogy(z, mass_results['stellar_mass_off'], 's-', color=color_off,
                label='FFB=OFF', lw=2, markersize=4)
    
    valid = ~np.isnan(mass_results['stellar_mass_ffb'])
    if np.sum(valid) > 0:
        ax2.semilogy(z[valid], mass_results['stellar_mass_ffb'][valid], '^-',
                    color=color_ffb, label='FFB galaxies', lw=2, markersize=6,
                    markeredgecolor='black', markeredgewidth=0.5)
    
    ax2.set_xlabel('Redshift', fontsize=11)
    ax2.set_ylabel('Median M* [M$_\\odot$]', fontsize=11)
    ax2.set_title('Stellar Mass Evolution', fontsize=12, fontweight='bold')
    ax2.legend(loc='best', fontsize=9)
    ax2.grid(alpha=0.3)
    ax2.set_xlim(z.max(), -0.2)
    
    # ========================================================================
    # Panel 3: Mass Ratio Evolution
    # ========================================================================
    ax3 = fig.add_subplot(gs[0, 2])
    
    ratio_stellar = mass_results['stellar_mass_on'] / np.maximum(mass_results['stellar_mass_off'], 1e-10)
    ratio_bulge = mass_results['bulge_mass_on'] / np.maximum(mass_results['bulge_mass_off'], 1e-10)
    ratio_disk = mass_results['disk_mass_on'] / np.maximum(mass_results['disk_mass_off'], 1e-10)
    
    ax3.plot(z, ratio_stellar, 'o-', color='black', label='Stellar', lw=2, markersize=4)
    ax3.plot(z, ratio_bulge, 's-', color='red', label='Bulge', lw=2, markersize=4)
    ax3.plot(z, ratio_disk, '^-', color='blue', label='Disk', lw=2, markersize=4)
    ax3.axhline(1.0, color='gray', ls='--', alpha=0.5, label='No difference')
    ax3.fill_between([z.max(), -0.2], 0.95, 1.05, alpha=0.2, color='gray')
    
    ax3.set_xlabel('Redshift', fontsize=11)
    ax3.set_ylabel('Mass Ratio (FFB=ON / FFB=OFF)', fontsize=11)
    ax3.set_title('Mass Component Ratios', fontsize=12, fontweight='bold')
    ax3.legend(loc='best', fontsize=9)
    ax3.grid(alpha=0.3)
    ax3.set_xlim(z.max(), -0.2)
    ax3.set_ylim(0.8, 1.2)
    
    # ========================================================================
    # Panel 4: Bulge Mass Evolution
    # ========================================================================
    ax4 = fig.add_subplot(gs[1, 0])
    
    ax4.semilogy(z, mass_results['bulge_mass_on'], 'o-', color=color_on,
                label='FFB=ON', lw=2, markersize=4)
    ax4.semilogy(z, mass_results['bulge_mass_off'], 's-', color=color_off,
                label='FFB=OFF', lw=2, markersize=4)
    
    valid = ~np.isnan(mass_results['bulge_mass_ffb'])
    if np.sum(valid) > 0:
        ax4.semilogy(z[valid], mass_results['bulge_mass_ffb'][valid], '^-',
                    color=color_ffb, label='FFB galaxies', lw=2, markersize=6,
                    markeredgecolor='black', markeredgewidth=0.5)
    
    ax4.set_xlabel('Redshift', fontsize=11)
    ax4.set_ylabel('Median M$_{bulge}$ [M$_\\odot$]', fontsize=11)
    ax4.set_title('Bulge Mass Evolution', fontsize=12, fontweight='bold')
    ax4.legend(loc='best', fontsize=9)
    ax4.grid(alpha=0.3)
    ax4.set_xlim(z.max(), -0.2)
    
    # ========================================================================
    # Panel 5: Disk Mass Evolution
    # ========================================================================
    ax5 = fig.add_subplot(gs[1, 1])
    
    ax5.semilogy(z, mass_results['disk_mass_on'], 'o-', color=color_on,
                label='FFB=ON', lw=2, markersize=4)
    ax5.semilogy(z, mass_results['disk_mass_off'], 's-', color=color_off,
                label='FFB=OFF', lw=2, markersize=4)
    
    ax5.set_xlabel('Redshift', fontsize=11)
    ax5.set_ylabel('Median M$_{disk}$ [M$_\\odot$]', fontsize=11)
    ax5.set_title('Disk Mass Evolution', fontsize=12, fontweight='bold')
    ax5.legend(loc='best', fontsize=9)
    ax5.grid(alpha=0.3)
    ax5.set_xlim(z.max(), -0.2)
    
    # ========================================================================
    # Panel 6: Morphology Fractions
    # ========================================================================
    ax6 = fig.add_subplot(gs[1, 2])
    
    z = morph_results['redshift']
    ax6.plot(z, morph_results['early_frac_on'], 'o-', color=color_on,
            label='Early-type (FFB=ON)', lw=2, markersize=4)
    ax6.plot(z, morph_results['early_frac_off'], 's-', color=color_off,
            label='Early-type (FFB=OFF)', lw=2, markersize=4)
    ax6.plot(z, morph_results['late_frac_on'], 'o--', color=color_on,
            label='Late-type (FFB=ON)', lw=2, markersize=4, alpha=0.6)
    ax6.plot(z, morph_results['late_frac_off'], 's--', color=color_off,
            label='Late-type (FFB=OFF)', lw=2, markersize=4, alpha=0.6)
    
    ax6.set_xlabel('Redshift', fontsize=11)
    ax6.set_ylabel('Fraction', fontsize=11)
    ax6.set_title('Morphology Fractions (B/T > 0.5)', fontsize=12, fontweight='bold')
    ax6.legend(loc='best', fontsize=8)
    ax6.grid(alpha=0.3)
    ax6.set_xlim(z.max(), -0.2)
    ax6.set_ylim(0, 1)
    
    # ========================================================================
    # Panel 7: Bulge Formation Pathways
    # ========================================================================
    ax7 = fig.add_subplot(gs[2, 0])
    
    z = pathway_results['redshift']
    ax7.plot(z, pathway_results['merger_frac_ffb_on'], 'o-', color='red',
            label='Merger (FFB=ON)', lw=2, markersize=4)
    ax7.plot(z, pathway_results['merger_frac_ffb_off'], 's', color='red',
            label='Merger (FFB=OFF)', lw=2, markersize=4, alpha=0.5, linestyle='--')
    ax7.plot(z, pathway_results['instability_frac_ffb_on'], 'o-', color='blue',
            label='Instability (FFB=ON)', lw=2, markersize=4)
    ax7.plot(z, pathway_results['instability_frac_ffb_off'], 's', color='blue',
            label='Instability (FFB=OFF)', lw=2, markersize=4, alpha=0.5, linestyle='--')
    
    ax7.set_xlabel('Redshift', fontsize=11)
    ax7.set_ylabel('Bulge Mass Fraction', fontsize=11)
    ax7.set_title('Bulge Formation Pathways', fontsize=12, fontweight='bold')
    ax7.legend(loc='best', fontsize=9)
    ax7.grid(alpha=0.3)
    ax7.set_xlim(z.max(), -0.2)
    ax7.set_ylim(0, 1)
    
    # ========================================================================
    # Panel 8: Bulge Component Evolution
    # ========================================================================
    ax8 = fig.add_subplot(gs[2, 1])
    
    z = mass_results['redshift']
    ax8.semilogy(z, np.maximum(mass_results['merger_bulge_on'], 1e6), 'o-', color='red',
                label='Merger (ON)', lw=2, markersize=4)
    ax8.semilogy(z, np.maximum(mass_results['merger_bulge_off'], 1e6), 's--', color='red',
                label='Merger (OFF)', lw=2, markersize=4, alpha=0.5)
    ax8.semilogy(z, np.maximum(mass_results['instability_bulge_on'], 1e6), '^-', color='blue',
                label='Instability (ON)', lw=2, markersize=4)
    ax8.semilogy(z, np.maximum(mass_results['instability_bulge_off'], 1e6), 'v--', color='blue',
                label='Instability (OFF)', lw=2, markersize=4, alpha=0.5)
    
    ax8.set_xlabel('Redshift', fontsize=11)
    ax8.set_ylabel('Median M$_{bulge}$ [M$_\\odot$]', fontsize=11)
    ax8.set_title('Bulge Components', fontsize=12, fontweight='bold')
    ax8.legend(loc='best', fontsize=9)
    ax8.grid(alpha=0.3)
    ax8.set_xlim(z.max(), -0.2)
    
    # ========================================================================
    # Panel 9: Number of FFB Galaxies
    # ========================================================================
    ax9 = fig.add_subplot(gs[2, 2])
    
    z = bt_results['redshift']
    ax9.semilogy(z, bt_results['n_ffb_galaxies'], 'o-', color=color_ffb,
                lw=2, markersize=5, markeredgecolor='black', markeredgewidth=0.5)
    ax9.fill_between(z, 1, bt_results['n_ffb_galaxies'], color=color_ffb, alpha=0.3)
    
    # Add percentage on secondary axis
    ax9_twin = ax9.twinx()
    pct_ffb = 100 * bt_results['n_ffb_galaxies'] / bt_results['n_galaxies_on']
    ax9_twin.plot(z, pct_ffb, 's-', color='purple', lw=2, markersize=4, alpha=0.7)
    ax9_twin.set_ylabel('% of Galaxies', fontsize=11, color='purple')
    ax9_twin.tick_params(axis='y', labelcolor='purple')
    
    ax9.set_xlabel('Redshift', fontsize=11)
    ax9.set_ylabel('Number of FFB Galaxies', fontsize=11, color=color_ffb)
    ax9.set_title('FFB Galaxy Population', fontsize=12, fontweight='bold')
    ax9.tick_params(axis='y', labelcolor=color_ffb)
    ax9.grid(alpha=0.3)
    ax9.set_xlim(z.max(), -0.2)
    
    # Add overall title
    fig.suptitle('Comprehensive FFB vs No-FFB Bulge-Disk Analysis', 
                 fontsize=16, fontweight='bold', y=0.995)
    
    plt.savefig(output_file, dpi=200, bbox_inches='tight')
    print(f"✓ Saved: {output_file}")

def create_summary_table(bt_results, mass_results, pathway_results):
    """
    Create summary table comparing key quantities at different redshifts
    """
    print("\n" + "="*80)
    print("SUMMARY TABLE: KEY QUANTITIES AT DIFFERENT REDSHIFTS")
    print("="*80)
    
    # Select representative redshifts
    z_targets = [0.0, 1.0, 2.0, 4.0, 6.0, 8.0]
    
    print(f"\n{'Redshift':<10} {'B/T (ON)':<12} {'B/T (OFF)':<12} {'ΔB/T':<10} "
          f"{'M* Ratio':<12} {'M_bulge Ratio':<15} {'Merger Frac (ON)':<18} {'Merger Frac (OFF)':<18}")
    print("-" * 120)
    
    for z_target in z_targets:
        # Find closest redshift
        idx = np.argmin(np.abs(bt_results['redshift'] - z_target))
        z_actual = bt_results['redshift'][idx]
        
        if abs(z_actual - z_target) < 0.5:  # Only show if close enough
            bt_on = bt_results['median_bt_ffb_on'][idx]
            bt_off = bt_results['median_bt_ffb_off'][idx]
            delta_bt = bt_on - bt_off
            
            # Find corresponding index in mass results
            idx_mass = np.argmin(np.abs(mass_results['redshift'] - z_actual))
            mass_ratio = (mass_results['stellar_mass_on'][idx_mass] / 
                         mass_results['stellar_mass_off'][idx_mass])
            bulge_ratio = (mass_results['bulge_mass_on'][idx_mass] /
                          np.maximum(mass_results['bulge_mass_off'][idx_mass], 1e-10))
            
            # Find corresponding index in pathway results
            idx_path = np.argmin(np.abs(pathway_results['redshift'] - z_actual))
            merger_on = pathway_results['merger_frac_ffb_on'][idx_path]
            merger_off = pathway_results['merger_frac_ffb_off'][idx_path]
            
            # Show N/A for values that might be unreliable
            bt_on_str = f"{bt_on:.3f}" if bt_on > 0.01 else "N/A"
            bt_off_str = f"{bt_off:.3f}" if bt_off > 0.01 else "N/A"
            delta_bt_str = f"{delta_bt:+.3f}" if abs(delta_bt) > 0.001 else "N/A"
            
            print(f"{z_actual:<10.2f} {bt_on_str:<12} {bt_off_str:<12} {delta_bt_str:<10} "
                  f"{mass_ratio:<12.3f} {bulge_ratio:<15.3f} {merger_on:<18.3f} {merger_off:<18.3f}")
    
    print("="*80)

# ============================================================================
# MAIN FUNCTION
# ============================================================================

def main():
    """
    Main analysis function
    """
    print("\n" + "="*80)
    print("COMPREHENSIVE FFB BULGE-DISK ANALYSIS")
    print("="*80)
    
    # Get file paths
    if len(sys.argv) >= 3:
        file_ffb_on = Path(sys.argv[1])
        file_ffb_off = Path(sys.argv[2])
    else:
        file_ffb_on = Path("output/millennium/model_0.hdf5")
        file_ffb_off = Path("output/millennium_noffb/model_0.hdf5")
        print(f"\nUsage: {sys.argv[0]} <ffb_on_file> <ffb_off_file>")
        print(f"Using defaults:")
        print(f"  FFB=ON:  {file_ffb_on}")
        print(f"  FFB=OFF: {file_ffb_off}\n")
    
    # Check files exist
    if not file_ffb_on.exists():
        print(f"ERROR: File not found: {file_ffb_on}")
        return
    if not file_ffb_off.exists():
        print(f"ERROR: File not found: {file_ffb_off}")
        return
    
    # Load data
    print("\n" + "-"*80)
    print("STEP 1: LOADING DATA")
    print("-"*80)
    data_ffb_on = load_all_snapshots(file_ffb_on)
    data_ffb_off = load_all_snapshots(file_ffb_off)
    
    # Perform analyses
    print("\n" + "-"*80)
    print("STEP 2: PERFORMING ANALYSES")
    print("-"*80)
    
    bt_results = calculate_bt_evolution(data_ffb_on, data_ffb_off)
    mass_results = calculate_mass_evolution(data_ffb_on, data_ffb_off)
    morph_results = calculate_morphology_fractions(data_ffb_on, data_ffb_off)
    pathway_results = calculate_bulge_pathway_fractions(data_ffb_on, data_ffb_off)
    global_bt_results = calculate_global_bt_evolution(data_ffb_on, data_ffb_off)
    
    # Create plots
    print("\n" + "-"*80)
    print("STEP 3: CREATING VISUALIZATIONS")
    print("-"*80)
    
    output_file = "ffb_comprehensive_bulge_disk_analysis.png"
    create_comprehensive_plot(bt_results, mass_results, morph_results, 
                             pathway_results, output_file)
    
    # Create standalone global B/T plot
    create_global_bt_plot(global_bt_results, "ffb_global_bt_evolution.png")
    
    # Create B/T distribution plot (this is what the user wants!)
    bt_dist_on, bt_dist_off = create_bt_distribution_plot(data_ffb_on, data_ffb_off, 
                                                          "ffb_bt_distribution_evolution.png")
    
    # Print summary table
    create_summary_table(bt_results, mass_results, pathway_results)
    
    # Final summary
    print("\n" + "="*80)
    print("ANALYSIS COMPLETE!")
    print("="*80)
    print(f"\n✓ Analyzed {len(data_ffb_on)} snapshots")
    print(f"✓ Redshift range: z = {bt_results['redshift'].max():.1f} to {bt_results['redshift'].min():.1f}")
    print(f"✓ Total FFB galaxies across all snapshots: {bt_results['n_ffb_galaxies'].sum():.0f}")
    print(f"\nKey findings:")
    
    # Calculate some key statistics
    z0_idx = np.argmin(np.abs(bt_results['redshift']))
    zhigh_idx = np.argmax(bt_results['redshift'])
    
    bt_diff_z0 = bt_results['median_bt_ffb_on'][z0_idx] - bt_results['median_bt_ffb_off'][z0_idx]
    bt_diff_zhigh = (bt_results['median_bt_ffb_on'][zhigh_idx] - 
                     bt_results['median_bt_ffb_off'][zhigh_idx])
    
    print(f"  • B/T difference at z=0: {bt_diff_z0:+.3f}")
    print(f"  • B/T difference at high-z: {bt_diff_zhigh:+.3f}")
    
    # Merger fraction difference
    merger_diff = (pathway_results['merger_frac_ffb_on'] - 
                   pathway_results['merger_frac_ffb_off'])
    max_merger_diff_idx = np.argmax(np.abs(merger_diff))
    print(f"  • Max merger fraction difference: {merger_diff[max_merger_diff_idx]:+.3f} "
          f"at z={pathway_results['redshift'][max_merger_diff_idx]:.1f}")
    
    # Global B/T difference
    global_bt_diff = (global_bt_results['global_bt_ffb_on'] - 
                     global_bt_results['global_bt_ffb_off'])
    max_global_bt_idx = np.argmax(np.abs(global_bt_diff))
    print(f"  • Max global B/T difference: {global_bt_diff[max_global_bt_idx]:+.4f} "
          f"({100*global_bt_diff[max_global_bt_idx]:+.2f}%) "
          f"at z={global_bt_results['redshift'][max_global_bt_idx]:.1f}")
    
    print(f"\n✓ Output saved to: {output_file}")
    print(f"✓ Global B/T plot saved to: ffb_global_bt_evolution.png")
    print(f"✓ B/T distribution plot saved to: ffb_bt_distribution_evolution.png")
    print("="*80 + "\n")

if __name__ == "__main__":
    main()