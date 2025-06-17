#!/usr/bin/env python3
"""
SAGE sSFR vs Stellar Mass Analysis - FIXED VERSION
==================================================

Fixed issues:
- Better data filtering to preserve more galaxies
- Increased point visibility (size and alpha)
- Added random seed for reproducible results
- Improved debug output
- Fixed potential log calculation issues

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

# Configuration parameters - UPDATE THESE TO MATCH YOUR SETUP
MODEL_CONFIGS = [
    {
        'name': 'SAGE 2.0',
        'dir': './output/millennium/',
        'sf_color': '#0072B2',       # Blue for star-forming
        'q_color': '#D55E00',        # Red/orange for quiescent
        'linestyle': '-',
        'linewidth': 3,
        'alpha': 1.0,
        'boxsize': 500,
        'volume_fraction': 1.0
    },
    {
        'name': 'SAGE (C16)',
        'dir': './output/millennium_vanilla/',
        'sf_color': 'black',
        'q_color': 'black',
        'linestyle': '--',
        'linewidth': 2,
        'alpha': 0.8,
        'boxsize': 500,
        'volume_fraction': 1.0
    }
]

# Target redshifts for the 2x3 grid
TARGET_REDSHIFTS = [0.0, 0.5, 1.0, 2.0, 3.0, 4.0]

# Analysis parameters
sSFRcut = -11.0  # Log sSFR cut between star-forming and quiescent galaxies (yr^-1)
dilute = 10000   # Maximum number of points to plot
mass_min = 0.01  # Minimum stellar mass threshold (10^10 M_sun)

# OPTIMIZATION PARAMETERS
CHUNK_SIZE = 100000
MAX_MEMORY_GB = 4.0
USE_FLOAT32 = True

FileName = 'model_0.hdf5'
OutputDir = './output/millennium/plots/'
Hubble_h = 0.73

# Snapshot to redshift mapping
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (18, 12)
plt.rcParams["figure.dpi"] = 300
plt.rcParams["font.size"] = 14


def read_hdf_optimized(directory, snap_num=None, galaxy_types='all'):
    """Optimized data reading with better filtering"""
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    print(f"    Reading {len(model_files)} files...")
    
    # Initialize data containers
    all_stellar_mass = []
    all_sfr_total = []
    all_galaxy_type = []
    
    total_galaxies = 0
    files_processed = 0
    
    for file_idx, model_file in enumerate(model_files):
        try:
            filepath = os.path.join(directory, model_file)
            
            with h5.File(filepath, 'r') as f:
                # Check if snapshot exists
                if snap_num not in f:
                    continue
                
                # Read only required fields
                stellar_mass = np.array(f[snap_num]['StellarMass'])
                sfr_disk = np.array(f[snap_num]['SfrDisk'])
                sfr_bulge = np.array(f[snap_num]['SfrBulge'])
                galaxy_type = np.array(f[snap_num]['Type'])
                
                # Convert to memory-efficient types
                if USE_FLOAT32:
                    stellar_mass = stellar_mass.astype(np.float32)
                    sfr_disk = sfr_disk.astype(np.float32)
                    sfr_bulge = sfr_bulge.astype(np.float32)
                
                # LESS AGGRESSIVE FILTERING: only remove galaxies with zero stellar mass
                stellar_mass_solar = stellar_mass * 1.0e10 / Hubble_h
                mass_mask = stellar_mass_solar > mass_min
                
                if np.sum(mass_mask) == 0:
                    continue
                
                # Apply galaxy type filter
                if galaxy_types == 'central':
                    type_mask = (galaxy_type == 0)
                elif galaxy_types == 'satellite':
                    type_mask = (galaxy_type == 1)
                elif galaxy_types == 'orphan':
                    type_mask = (galaxy_type == 2)
                else:  # 'all'
                    type_mask = np.ones(len(stellar_mass), dtype=bool)
                
                # Combine filters
                combined_mask = mass_mask & type_mask
                n_valid = np.sum(combined_mask)
                
                if n_valid == 0:
                    continue
                
                # Apply filters
                stellar_mass_filtered = stellar_mass_solar[combined_mask]
                sfr_total = (sfr_disk + sfr_bulge)[combined_mask]
                galaxy_type_filtered = galaxy_type[combined_mask]
                
                # Store data
                all_stellar_mass.append(stellar_mass_filtered)
                all_sfr_total.append(sfr_total)
                all_galaxy_type.append(galaxy_type_filtered)
                
                total_galaxies += n_valid
                files_processed += 1
                
                # Progress feedback
                if (file_idx + 1) % 10 == 0:
                    print(f"      Processed {file_idx + 1}/{len(model_files)} files, {total_galaxies} galaxies so far")
                    
        except Exception as e:
            print(f"      Warning: Could not read {model_file}: {e}")
            continue
    
    if total_galaxies == 0:
        print(f"    No valid galaxies found!")
        return None
    
    print(f"    Loaded {total_galaxies} galaxies from {files_processed} files")
    
    # Combine all data
    combined_stellar_mass = np.concatenate(all_stellar_mass)
    combined_sfr_total = np.concatenate(all_sfr_total)
    combined_galaxy_type = np.concatenate(all_galaxy_type)
    
    # Clean up intermediate data
    del all_stellar_mass, all_sfr_total, all_galaxy_type
    gc.collect()
    
    return {
        'stellar_mass': combined_stellar_mass,
        'sfr_total': combined_sfr_total,
        'galaxy_type': combined_galaxy_type
    }

def darken_color(color, factor=0.7):
    """Darken a color by multiplying RGB values by a factor"""
    import matplotlib.colors as mcolors
    if isinstance(color, str):
        rgb = mcolors.to_rgb(color)
    else:
        rgb = color
    return tuple(c * factor for c in rgb)

def plot_ssfr_panel_fixed(ax, mass, ssfr, ssfr_cut, model_config, z_actual, dilute_factor=None):
    """
    FIXED plotting function with better visibility and handling
    """
    print(f"    Processing {model_config['name']}: {len(mass)} galaxies")
    
    # More lenient SFR filtering - only remove exactly zero SFR
    positive_sfr_mask = ssfr > 0.0
    
    if np.sum(positive_sfr_mask) == 0:
        print(f"    Warning: No galaxies with positive SFR for {model_config['name']}")
        return
    
    # Apply filter
    mass_filtered = mass[positive_sfr_mask]
    sfr_filtered = ssfr[positive_sfr_mask]
    
    # Add small value to very low SFR to avoid log issues
    sfr_filtered = np.maximum(sfr_filtered, 1e-15)
    
    print(f"    After SFR filter: {len(mass_filtered)} galaxies remain")
    
    # Convert to log space
    log_mass = np.log10(mass_filtered)
    
    # Calculate sSFR
    ssfr_values = sfr_filtered / mass_filtered
    log_ssfr = np.log10(ssfr_values)
    
    # Debug: Check data ranges
    print(f"    Mass range: {np.min(log_mass):.2f} to {np.max(log_mass):.2f}")
    print(f"    sSFR range: {np.min(log_ssfr):.2f} to {np.max(log_ssfr):.2f}")
    
    sf_count = np.sum(log_ssfr > ssfr_cut)
    q_count = np.sum(log_ssfr <= ssfr_cut)
    print(f"    Star-forming: {sf_count}, Quiescent: {q_count}")
    
    # Calculate statistics using FULL dataset
    stats = calculate_ssfr_statistics_optimized(log_mass, log_ssfr, ssfr_cut)
    
    # Plot scatter points only for SAGE 2.0
    if model_config['name'] == 'SAGE 2.0':
        # Apply dilution for plotting performance
        if dilute_factor and len(log_mass) > dilute_factor:
            indices = np.random.choice(len(log_mass), size=dilute_factor, replace=False)
            mass_plot = log_mass[indices]
            ssfr_plot = log_ssfr[indices]
            print(f"    Plotting {dilute_factor:,} points (diluted from {len(log_mass):,})")
        else:
            mass_plot = log_mass
            ssfr_plot = log_ssfr
            print(f"    Plotting all {len(log_mass):,} points")
        
        sf_mask = ssfr_plot > ssfr_cut
        q_mask = ssfr_plot <= ssfr_cut
        
        print(f"    Plotting {np.sum(sf_mask)} SF points, {np.sum(q_mask)} quiescent points")
        
        # INCREASED point size and alpha for better visibility
        if np.any(sf_mask):
            ax.scatter(mass_plot[sf_mask], ssfr_plot[sf_mask], 
                      marker='o', s=1, c=model_config['sf_color'], 
                      alpha=0.25, rasterized=False, zorder=1)
        
        if np.any(q_mask):
            ax.scatter(mass_plot[q_mask], ssfr_plot[q_mask], 
                      marker='o', s=1, c=model_config['q_color'], 
                      alpha=0.25, rasterized=False, zorder=1)
    
    # Plot median lines for all models
    mass_centers = stats['mass_centers']
    
    # Star-forming median line
    valid_sf = stats['sf']['valid']
    if np.sum(valid_sf) > 0:
        print(f"    Plotting SF median: {np.sum(valid_sf)} points, color={model_config['sf_color']}")
        ax.plot(mass_centers[valid_sf], stats['sf']['median'][valid_sf], 
               color=darken_color(model_config['sf_color'], 0.85), linestyle=model_config['linestyle'], 
               linewidth=model_config['linewidth'], alpha=model_config['alpha'], 
               zorder=3)
    
    # Quiescent median line
    valid_q = stats['q']['valid']
    if np.sum(valid_q) > 0:
        print(f"    Plotting Q median: {np.sum(valid_q)} points, color={model_config['q_color']}")
        ax.plot(mass_centers[valid_q], stats['q']['median'][valid_q], 
               color=darken_color(model_config['q_color'], 0.85), linestyle=model_config['linestyle'], 
               linewidth=model_config['linewidth'], alpha=model_config['alpha'], 
               zorder=3)
    
    # Formatting
    ax.set_xlim(8.0, 12.0)
    ax.set_ylim(-16.0, -8.0)
    ax.set_title(f'z = {z_actual:.1f}', fontsize=16)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))


def calculate_ssfr_statistics_optimized(mass, ssfr, ssfr_cut):
    """Optimized sSFR statistics calculation"""
    mass = np.asarray(mass)
    ssfr = np.asarray(ssfr)
    
    if USE_FLOAT32:
        mass = mass.astype(np.float32)
        ssfr = ssfr.astype(np.float32)
    
    # Define mass bins
    mass_bins = np.arange(8.0, 12.0, 0.2, dtype=np.float32)
    mass_centers = mass_bins[:-1] + 0.1
    
    # Separate populations
    sf_mask = ssfr > ssfr_cut
    q_mask = ssfr <= ssfr_cut
    
    def calculate_population_stats(pop_mass, pop_ssfr):
        if len(pop_mass) == 0:
            return np.full(len(mass_centers), np.nan), np.full(len(mass_centers), np.nan), np.full(len(mass_centers), np.nan)
        
        medians = []
        p16s = []
        p84s = []
        
        for i in range(len(mass_bins)-1):
            mask = (pop_mass >= mass_bins[i]) & (pop_mass < mass_bins[i+1])
            n_in_bin = np.sum(mask)
            
            if n_in_bin > 5:
                values = pop_ssfr[mask]
                medians.append(np.median(values))
                p16s.append(np.percentile(values, 16))
                p84s.append(np.percentile(values, 84))
            else:
                medians.append(np.nan)
                p16s.append(np.nan)
                p84s.append(np.nan)
        
        return np.array(medians), np.array(p16s), np.array(p84s)
    
    # Calculate for each population
    sf_median, sf_p16, sf_p84 = calculate_population_stats(mass[sf_mask], ssfr[sf_mask])
    q_median, q_p16, q_p84 = calculate_population_stats(mass[q_mask], ssfr[q_mask])
    
    return {
        'mass_centers': mass_centers,
        'sf': {
            'mass': mass[sf_mask],
            'ssfr': ssfr[sf_mask],
            'median': sf_median,
            'p16': sf_p16,
            'p84': sf_p84,
            'valid': ~np.isnan(sf_median)
        },
        'q': {
            'mass': mass[q_mask],
            'ssfr': ssfr[q_mask],
            'median': q_median,
            'p16': q_p16,
            'p84': q_p84,
            'valid': ~np.isnan(q_median)
        }
    }


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


def create_ssfr_grid_fixed(galaxy_types='all', save_path=None):
    """Create FIXED 2x3 grid plot with better visibility"""
    print("Starting FIXED sSFR analysis...")
    
    # Check directories
    model_configs_valid = []
    for model_config in MODEL_CONFIGS:
        directory = model_config['dir']
        model_name = model_config['name']
        
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
    
    # Create 2x3 grid
    fig, axes = plt.subplots(2, 3, figsize=(18, 12), sharex=True, sharey=True)
    axes_flat = axes.flatten()
    
    for i, target_z in enumerate(TARGET_REDSHIFTS):
        ax = axes_flat[i]
        print(f"\n=== PANEL {i+1}: target z = {target_z} ===")
        
        for model_config in model_configs_valid:
            model_name = model_config['name']
            directory = model_config['dir']
            
            print(f"\nProcessing {model_name}...")
            
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
                data = read_hdf_optimized(
                    directory, 
                    snap_num=snap_str, 
                    galaxy_types=galaxy_types
                )
                
                if data is None:
                    print(f"    No data loaded for {model_name}")
                    continue
                
                # Plot using FIXED function
                plot_ssfr_panel_fixed(
                    ax, 
                    data['stellar_mass'], 
                    data['sfr_total'], 
                    sSFRcut, 
                    model_config, 
                    z_actual, 
                    dilute_factor=dilute
                )
                
                # Clean up
                del data
                gc.collect()
                
            except Exception as e:
                print(f"    Error processing {model_name}: {e}")
                import traceback
                traceback.print_exc()
                continue
    
    # Set labels and legend
    for i in range(6):
        row = i // 3
        col = i % 3
        
        if row == 1:
            axes_flat[i].set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=16)
        
        if col == 0:
            axes_flat[i].set_ylabel(r'$\log_{10}\ s\mathrm{SFR}\ (\mathrm{yr^{-1}})$', fontsize=16)
    
    # Create legend
    ax = axes_flat[0]
    model_handles = []
    for model_config in model_configs_valid:
        handle = plt.Line2D([0], [0], color=model_config['sf_color'], 
                           linestyle=model_config['linestyle'],
                           linewidth=model_config['linewidth'],
                           label=model_config['name'])
        model_handles.append(handle)
    
    if model_handles:
        ax.legend(handles=model_handles, loc='upper right', fontsize=12, frameon=False)
    
    plt.tight_layout()
    
    if save_path:
        print(f"\nSaving plot to {save_path}")
        plt.savefig(save_path, dpi=300, bbox_inches='tight')
        print(f"Saved plot to {save_path}")
    
    return fig, axes


# Main execution
if __name__ == "__main__":
    print("SAGE sSFR vs Stellar Mass Analysis - FIXED VERSION")
    print("=" * 60)
    
    try:
        print("Creating FIXED sSFR vs stellar mass grid (all galaxies)...")
        fig1, axes1 = create_ssfr_grid_fixed(
            galaxy_types='all',
            save_path=OutputDir + 'sage_ssfr_grid_all_FIXED' + OutputFormat
        )
        
        print("\nFIXED sSFR analysis complete!")
        
    except Exception as e:
        print(f"Error during analysis: {e}")
        import traceback
        traceback.print_exc()