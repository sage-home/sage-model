#!/usr/bin/env python3
"""
SAGE sSFR vs Stellar Mass Analysis - Grid Plot for Multiple Redshifts
====================================================================

This script creates a 2x3 grid showing sSFR vs stellar mass for 6 redshifts:
z = 0.0, 0.5, 1.0, 2.0, 3.0, 4.0

Visualization approach:
- SAGE 2.0: Blue/red scatter points + solid median lines  
- SAGE (C16): Black dashed median lines only (no scatter)

Based on the existing SAGE data reading approach, adapted for sSFR analysis.
Supports comparing multiple SAGE model runs/directories with different box sizes.

Features:
- Scatter points shown only for SAGE 2.0 (blue for star-forming, red for quiescent)
- Median lines for both models: SAGE 2.0 (solid) and SAGE (C16) (dashed black)
- Multiple model comparison with clean visualization
- Simplified legend showing just model names with line styles

Author: Analysis script for SAGE semi-analytic model
"""

import numpy as np
import matplotlib.pyplot as plt
import h5py as h5
import glob
import os
from scipy import stats
import pandas as pd
from random import seed, sample

# Configuration parameters - UPDATE THESE TO MATCH YOUR SETUP
# Define multiple directories and their properties
MODEL_CONFIGS = [
    {
        'name': 'SAGE 2.0',           # Display name for legend
        'dir': './output/millennium/',  # Directory path
        'sf_color': '#0072B2',       # Color for star-forming population (blue)
        'q_color': '#D55E00',        # Color for quiescent population (red)
        'linestyle': '-',            # Line style
        'linewidth': 3,              # Thick line for SAGE 2.0
        'alpha': 0.8,                # Transparency
        'boxsize': 62.5,             # Box size in h^-1 Mpc for this model
        'volume_fraction': 1.0       # Fraction of the full volume output by the model
    },
    {
        'name': 'SAGE C16',           # Display name for legend
        'dir': './output/millennium_vanilla/',  # Second directory path
        'sf_color': 'black',         # Color for star-forming population (black)
        'q_color': 'black',          # Color for quiescent population (black)
        'linestyle': '--',           # Dashed line style
        'linewidth': 2,              # Thin line for Vanilla SAGE
        'alpha': 0.8,                # Transparency
        'boxsize': 62.5,             # Box size in h^-1 Mpc for this model
        'volume_fraction': 1.0       # Fraction of the full volume output by the model
    }
]

# Target redshifts for the 2x3 grid
TARGET_REDSHIFTS = [0.0, 0.5, 1.0, 2.0, 3.0, 4.0]

# Analysis parameters
sSFRcut = -11.0  # Log sSFR cut between star-forming and quiescent galaxies (yr^-1)
dilute = 7000   # Maximum number of points to plot (for performance)
mass_min = 0.01  # Minimum stellar mass threshold (10^10 M_sun)

FileName = 'model_0.hdf5'
OutputDir = './output/millennium/plots/'

# Simulation details
Hubble_h = 0.73        # Hubble parameter

# Snapshot to redshift mapping - UPDATE THIS TO MATCH YOUR SIMULATION
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (18, 12)
plt.rcParams["figure.dpi"] = 300
plt.rcParams["font.size"] = 14


def get_model_volume(model_config):
    """
    Calculate volume for a specific model configuration
    
    Parameters:
    -----------
    model_config : dict
        Model configuration dictionary containing 'boxsize' and 'volume_fraction'
        
    Returns:
    --------
    float : Volume in (Mpc/h)^3
    """
    boxsize = model_config['boxsize']
    volume_fraction = model_config['volume_fraction']
    volume = (boxsize/Hubble_h)**3.0 * volume_fraction
    return volume


def read_hdf(directory, filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        try:
            with h5.File(os.path.join(directory, model_file), 'r') as property_file:
                data = np.array(property_file[snap_num][param])
                
                if combined_data is None:
                    combined_data = data
                else:
                    combined_data = np.concatenate((combined_data, data))
        except Exception as e:
            print(f"Warning: Could not read {param} from {model_file} for {snap_num}: {e}")
            continue
            
    return combined_data


def get_redshift_from_snapshot(snap_num):
    """Convert snapshot number to redshift using the provided redshift list"""
    if isinstance(snap_num, str):
        # Extract number from string like 'Snap_042'
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
    
    # Check first file for available snapshots
    try:
        with h5.File(os.path.join(directory, model_files[0]), 'r') as f:
            snap_keys = [key for key in f.keys() if 'snap' in key.lower()]
            
            # Extract snapshot numbers
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
    """
    Find the snapshot closest to the target redshift
    
    Parameters:
    -----------
    target_z : float
        Target redshift
    available_snapshots : list
        List of available snapshot numbers
    tolerance : float
        Maximum allowed difference in redshift
        
    Returns:
    --------
    int or None : Best matching snapshot number
    """
    best_snap = None
    best_diff = float('inf')
    
    for snap in available_snapshots:
        z_snap = get_redshift_from_snapshot(snap)
        diff = abs(z_snap - target_z)
        
        if diff < best_diff and diff <= tolerance:
            best_diff = diff
            best_snap = snap
    
    return best_snap


def calculate_ssfr_statistics(mass, ssfr, ssfr_cut):
    """
    Calculate median sSFR and scatter for star-forming and quiescent populations
    
    Parameters:
    -----------
    mass : array
        Log stellar mass
    ssfr : array  
        Log sSFR
    ssfr_cut : float
        sSFR threshold for classification
        
    Returns:
    --------
    dict : Dictionary with statistics for both populations
    """
    # Separate into star-forming and quiescent
    sf_mask = ssfr > ssfr_cut
    q_mask = ssfr <= ssfr_cut
    
    mass_sf = mass[sf_mask]
    ssfr_sf = ssfr[sf_mask]
    mass_q = mass[q_mask]
    ssfr_q = ssfr[q_mask]
    
    # Define mass bins
    mass_bins = np.arange(8.0, 12.0, 0.2)  # 0.2 dex bins
    mass_centers = mass_bins[:-1] + 0.1  # Center of each bin
    
    # Calculate statistics for star-forming population
    median_sf = []
    p16_sf = []
    p84_sf = []
    
    for i in range(len(mass_bins)-1):
        mask = (mass_sf >= mass_bins[i]) & (mass_sf < mass_bins[i+1])
        if np.sum(mask) > 5:  # Need at least 5 galaxies
            values = ssfr_sf[mask]
            median_sf.append(np.median(values))
            p16_sf.append(np.percentile(values, 16))
            p84_sf.append(np.percentile(values, 84))
        else:
            median_sf.append(np.nan)
            p16_sf.append(np.nan)
            p84_sf.append(np.nan)
    
    # Calculate statistics for quiescent population  
    median_q = []
    p16_q = []
    p84_q = []
    
    for i in range(len(mass_bins)-1):
        mask = (mass_q >= mass_bins[i]) & (mass_q < mass_bins[i+1])
        if np.sum(mask) > 5:  # Need at least 5 galaxies
            values = ssfr_q[mask]
            median_q.append(np.median(values))
            p16_q.append(np.percentile(values, 16))
            p84_q.append(np.percentile(values, 84))
        else:
            median_q.append(np.nan)
            p16_q.append(np.nan)
            p84_q.append(np.nan)
    
    # Convert to arrays and remove NaN values
    median_sf = np.array(median_sf)
    p16_sf = np.array(p16_sf)
    p84_sf = np.array(p84_sf)
    median_q = np.array(median_q)
    p16_q = np.array(p16_q)
    p84_q = np.array(p84_q)
    
    # Remove NaN entries
    valid_sf = ~np.isnan(median_sf)
    valid_q = ~np.isnan(median_q)
    
    return {
        'mass_centers': mass_centers,
        'sf': {
            'mass': mass_sf,
            'ssfr': ssfr_sf,
            'median': median_sf,
            'p16': p16_sf,
            'p84': p84_sf,
            'valid': valid_sf
        },
        'q': {
            'mass': mass_q,
            'ssfr': ssfr_q,
            'median': median_q,
            'p16': p16_q,
            'p84': p84_q,
            'valid': valid_q
        }
    }


def plot_ssfr_panel(ax, mass, ssfr, ssfr_cut, model_config, z_actual, dilute_factor=None):
    """
    Plot sSFR vs stellar mass for a single panel
    
    Parameters:
    -----------
    ax : matplotlib axis
        Plot axis
    mass : array
        Log stellar mass
    ssfr : array
        Log sSFR  
    ssfr_cut : float
        sSFR threshold for classification
    model_config : dict
        Model configuration
    z_actual : float
        Actual redshift used
    dilute_factor : int or None
        Number of points to randomly sample for plotting
        
    Note: Scatter points are only plotted for SAGE 2.0, 
          median lines are plotted for all models
    """
    
    # Dilute data if requested
    if dilute_factor is not None and len(mass) > dilute_factor:
        indices = sample(list(range(len(mass))), dilute_factor)
        mass_plot = mass[indices]
        ssfr_plot = ssfr[indices]
    else:
        mass_plot = mass
        ssfr_plot = ssfr
    
    # Calculate statistics for median lines (for all models)
    stats = calculate_ssfr_statistics(mass, ssfr, ssfr_cut)
    
    # Separate into populations for plotting scatter (only for SAGE 2.0)
    sf_mask = ssfr_plot > ssfr_cut
    q_mask = ssfr_plot <= ssfr_cut
    
    # Plot scatter points only for SAGE 2.0
    if model_config['name'] == 'SAGE 2.0':
        if np.any(sf_mask):
            ax.scatter(mass_plot[sf_mask], ssfr_plot[sf_mask], 
                      marker='o', s=1, c=model_config['sf_color'], alpha=0.3)
        
        if np.any(q_mask):
            ax.scatter(mass_plot[q_mask], ssfr_plot[q_mask], 
                      marker='o', s=1, c=model_config['q_color'], alpha=0.3)
    
    # Plot median lines for all models
    mass_centers = stats['mass_centers']
    
    # Star-forming median line
    valid_sf = stats['sf']['valid']
    if np.sum(valid_sf) > 0:
        ax.plot(mass_centers[valid_sf], stats['sf']['median'][valid_sf], 
               color=model_config['sf_color'], linestyle=model_config['linestyle'], 
               linewidth=model_config['linewidth'], alpha=model_config['alpha'], 
               zorder=3)
    
    # Quiescent median line
    valid_q = stats['q']['valid']
    if np.sum(valid_q) > 0:
        ax.plot(mass_centers[valid_q], stats['q']['median'][valid_q], 
               color=model_config['q_color'], linestyle=model_config['linestyle'], 
               linewidth=model_config['linewidth'], alpha=model_config['alpha'], 
               zorder=3)
    
    # Formatting
    ax.set_xlim(8.0, 12.0)
    ax.set_ylim(-16.0, -8.0)
    
    # Set title with actual redshift used
    ax.set_title(f'z = {z_actual:.1f}', fontsize=16)
    
    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))


def create_ssfr_grid(galaxy_types='all', save_path=None):
    """
    Create a 2x3 grid plot of sSFR vs stellar mass for multiple redshifts
    
    Parameters:
    -----------
    galaxy_types : str
        Type of galaxies to include ('all', 'central', 'satellite', 'orphan')
    save_path : str or None
        Path to save the figure
    """
    
    # Check that all model directories exist
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
        
        # Print model info
        boxsize = model_config['boxsize']
        volume_fraction = model_config['volume_fraction']
        volume = get_model_volume(model_config)
        print(f"Found {len(available_snapshots)} snapshots in {directory} for {model_name}")
        print(f"  Box size: {boxsize} h^-1 Mpc, Volume fraction: {volume_fraction}, Total volume: {volume:.2e} (Mpc/h)^3")
    
    if not model_configs_valid:
        raise ValueError("No valid model directories with snapshots found")
    
    # Create 2x3 grid
    fig, axes = plt.subplots(2, 3, figsize=(18, 12), sharex=False, sharey=True)
    axes_flat = axes.flatten()
    
    for i, target_z in enumerate(TARGET_REDSHIFTS):
        ax = axes_flat[i]
        
        print(f"Processing panel {i+1}: target z = {target_z}")
        
        # Process each model
        for model_idx, model_config in enumerate(model_configs_valid):
            model_name = model_config['name']
            directory = model_config['dir']
            
            # Get available snapshots for this model
            available_snapshots = get_available_snapshots(directory)
            
            # Find closest snapshot to target redshift
            best_snap = find_closest_snapshot(target_z, available_snapshots)
            
            if best_snap is None:
                print(f"  No suitable snapshot found for {model_name} at z={target_z}")
                continue
            
            z_actual = get_redshift_from_snapshot(best_snap)
            snap_str = f'Snap_{best_snap}'
            
            print(f"  {model_name}: Using Snap_{best_snap} (z={z_actual:.2f})")
            
            try:
                # Read data
                stellar_mass = read_hdf(directory, snap_num=snap_str, param='StellarMass')
                sfr_disk = read_hdf(directory, snap_num=snap_str, param='SfrDisk')
                sfr_bulge = read_hdf(directory, snap_num=snap_str, param='SfrBulge')
                galaxy_type = read_hdf(directory, snap_num=snap_str, param='Type')
                
                if stellar_mass is None or sfr_disk is None or sfr_bulge is None:
                    print(f"    Could not read required data for {model_name}")
                    continue
                
                # Convert to solar masses and sum SFR components
                stellar_mass = stellar_mass * 1.0e10 / Hubble_h
                total_sfr = sfr_disk + sfr_bulge
                
                # Apply galaxy type filter
                if galaxy_types == 'central':
                    type_mask = (galaxy_type == 0)
                elif galaxy_types == 'satellite':
                    type_mask = (galaxy_type == 1)
                elif galaxy_types == 'orphan':
                    type_mask = (galaxy_type == 2)
                else:  # 'all'
                    type_mask = np.ones(len(stellar_mass), dtype=bool)
                
                # Filter for positive stellar mass and galaxy type
                mass_mask = stellar_mass > mass_min
                combined_mask = mass_mask & type_mask
                
                stellar_mass_filtered = stellar_mass[combined_mask]
                total_sfr_filtered = total_sfr[combined_mask]
                
                if len(stellar_mass_filtered) == 0:
                    print(f"    No galaxies remain after filtering for {model_name}")
                    continue
                
                # Calculate sSFR
                # Add small value to avoid log(0)
                ssfr = total_sfr_filtered / stellar_mass_filtered
                ssfr = np.where(ssfr > 0, ssfr, 1e-16)  # Set minimum value
                
                # Convert to log space
                log_mass = np.log10(stellar_mass_filtered)
                log_ssfr = np.log10(ssfr)
                
                print(f"    -> {len(log_mass)} galaxies after filtering")
                print(f"    -> Mass range: {np.min(log_mass):.2f} to {np.max(log_mass):.2f}")
                print(f"    -> sSFR range: {np.min(log_ssfr):.2f} to {np.max(log_ssfr):.2f}")
                
                # Plot this model's data
                plot_ssfr_panel(ax, log_mass, log_ssfr, sSFRcut, model_config, 
                               z_actual, dilute_factor=dilute)
                
            except Exception as e:
                print(f"    Error processing {model_name}: {e}")
                continue
    
    # Set common labels
    for i in range(6):
        row = i // 3
        col = i % 3
        
        # Bottom row gets x-labels
        if row == 1:
            axes_flat[i].set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=16)
        
        # Left column gets y-labels
        if col == 0:
            axes_flat[i].set_ylabel(r'$\log_{10}\ s\mathrm{SFR}\ (\mathrm{M_{\odot}\ yr^{-1}})$', fontsize=16)
    
    # Add simple model legend to the first panel
    ax = axes_flat[0]
    
    # Simple model legend - just showing the model names with their line styles
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
        plt.savefig(save_path, dpi=300, bbox_inches='tight')
        print(f"Saved plot to {save_path}")
    
    return fig, axes


def create_individual_ssfr_plot(target_z=0.0, galaxy_types='all', save_path=None):
    """
    Create a single sSFR vs stellar mass plot for a specific redshift
    
    Parameters:
    -----------
    target_z : float
        Target redshift
    galaxy_types : str
        Type of galaxies to include
    save_path : str or None
        Path to save the figure
    """
    
    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)
    
    models_plotted = []
    
    for model_config in MODEL_CONFIGS:
        model_name = model_config['name']
        directory = model_config['dir']
        
        if not os.path.exists(directory):
            continue
        
        available_snapshots = get_available_snapshots(directory)
        best_snap = find_closest_snapshot(target_z, available_snapshots)
        
        if best_snap is None:
            continue
        
        z_actual = get_redshift_from_snapshot(best_snap)
        snap_str = f'Snap_{best_snap}'
        
        try:
            # Read and process data (same as in grid function)
            stellar_mass = read_hdf(directory, snap_num=snap_str, param='StellarMass')
            sfr_disk = read_hdf(directory, snap_num=snap_str, param='SfrDisk')
            sfr_bulge = read_hdf(directory, snap_num=snap_str, param='SfrBulge')
            galaxy_type = read_hdf(directory, snap_num=snap_str, param='Type')
            
            if stellar_mass is None or sfr_disk is None or sfr_bulge is None:
                continue
            
            stellar_mass = stellar_mass * 1.0e10 / Hubble_h
            total_sfr = sfr_disk + sfr_bulge
            
            # Apply filters
            if galaxy_types == 'central':
                type_mask = (galaxy_type == 0)
            elif galaxy_types == 'satellite':
                type_mask = (galaxy_type == 1)
            elif galaxy_types == 'orphan':
                type_mask = (galaxy_type == 2)
            else:
                type_mask = np.ones(len(stellar_mass), dtype=bool)
            
            mass_mask = stellar_mass > mass_min
            combined_mask = mass_mask & type_mask
            
            stellar_mass_filtered = stellar_mass[combined_mask]
            total_sfr_filtered = total_sfr[combined_mask]
            
            if len(stellar_mass_filtered) == 0:
                continue
            
            # Calculate sSFR
            ssfr = total_sfr_filtered / stellar_mass_filtered
            ssfr = np.where(ssfr > 0, ssfr, 1e-16)
            
            log_mass = np.log10(stellar_mass_filtered)
            log_ssfr = np.log10(ssfr)
            
            # Plot
            plot_ssfr_panel(ax, log_mass, log_ssfr, sSFRcut, model_config, z_actual, dilute)
            models_plotted.append(model_config)
            
        except Exception as e:
            print(f"Error processing {model_name}: {e}")
            continue
    
    # Add simple model legend
    if models_plotted:
        model_handles = []
        for model_config in models_plotted:
            handle = plt.Line2D([0], [0], color=model_config['sf_color'], 
                               linestyle=model_config['linestyle'],
                               linewidth=model_config['linewidth'],
                               label=model_config['name'])
            model_handles.append(handle)
        ax.legend(handles=model_handles, loc='upper right', fontsize=12, frameon=False)
    
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=16)
    ax.set_ylabel(r'$\log_{10}\ s\mathrm{SFR}\ (\mathrm{yr^{-1}})$', fontsize=16)
    ax.set_title(f'sSFR vs Stellar Mass (z ≈ {target_z})', fontsize=18)
    
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=300, bbox_inches='tight')
        print(f"Saved plot to {save_path}")
    
    return plt.gcf(), ax


# Example usage and main execution
if __name__ == "__main__":
    print("SAGE sSFR vs Stellar Mass Analysis")
    print("2x3 Grid for redshifts: z = 0.0, 0.5, 1.0, 2.0, 3.0, 4.0")
    print("Visualization: SAGE 2.0 scatter + medians vs SAGE (C16) medians only")
    print("=" * 60)
    
    # Check if data directories exist
    valid_models = []
    for model_config in MODEL_CONFIGS:
        directory = model_config['dir']
        model_name = model_config['name']
        
        if not os.path.exists(directory):
            print(f"Warning: Directory {directory} for {model_name} does not exist!")
            continue
        
        # Check for model files
        model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
        
        if not model_files:
            print(f"Warning: No model files found in {directory} for {model_name}")
            continue
        
        volume = get_model_volume(model_config)
        print(f"✓ Found {len(model_files)} model files for {model_name}")
        print(f"  Volume: {volume:.2e} (Mpc/h)^3")
        valid_models.append(model_config)
    
    if not valid_models:
        print("Error: No valid model directories found!")
        exit(1)
    
    # Update the global MODEL_CONFIGS to only include valid models
    MODEL_CONFIGS[:] = valid_models
    
    try:
        print(f"\nAnalysis parameters:")
        print(f"  sSFR cut: {sSFRcut} yr^-1 (log)")
        print(f"  Minimum stellar mass: {mass_min} × 10^10 M_sun")
        print(f"  Dilution factor: {dilute} points max per panel")
        
        print("\nCreating sSFR vs stellar mass grid (all galaxies)...")
        fig1, axes1 = create_ssfr_grid(
            galaxy_types='all',
            save_path=OutputDir + 'sage_ssfr_grid_all' + OutputFormat
        )
        
        print("\nCreating sSFR vs stellar mass grid (central galaxies only)...")
        fig2, axes2 = create_ssfr_grid(
            galaxy_types='central',
            save_path=OutputDir + 'sage_ssfr_grid_central' + OutputFormat
        )
        
        print("\nCreating individual sSFR plot for z=0...")
        fig3, ax3 = create_individual_ssfr_plot(
            target_z=0.0,
            galaxy_types='all',
            save_path=OutputDir + 'sage_ssfr_z0' + OutputFormat
        )
        
        print("\nsSFR analysis complete!")
        print("Generated plots:")
        print("1. sage_ssfr_grid_all.pdf - All galaxies (2x3 grid)")
        print("   • SAGE 2.0: Blue/red scatter + solid median lines")
        print("   • SAGE (C16): Black dashed median lines only")
        print("2. sage_ssfr_grid_central.pdf - Central galaxies only (2x3 grid)")  
        print("   • Same visualization approach as above")
        print("3. sage_ssfr_z0.pdf - Single plot at z=0")
        print("   • Same visualization approach as above")
        
        print("\nModel configuration summary:")
        for i, model_config in enumerate(MODEL_CONFIGS):
            volume = get_model_volume(model_config)
            print(f"  {i+1}. {model_config['name']}: {model_config['dir']}")
            print(f"     SF Color: {model_config['sf_color']}, Q Color: {model_config['q_color']}, Style: {model_config['linestyle']}")
            print(f"     Volume: {volume:.2e} (Mpc/h)^3")
        
        print(f"\nRedshift mapping for {len(TARGET_REDSHIFTS)} panels:")
        for i, target_z in enumerate(TARGET_REDSHIFTS):
            row = i // 3 + 1
            col = i % 3 + 1
            print(f"  Panel ({row},{col}): z = {target_z}")
        
    except Exception as e:
        print(f"Error during analysis: {e}")
        import traceback
        traceback.print_exc()