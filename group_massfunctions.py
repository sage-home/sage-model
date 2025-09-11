#!/usr/bin/env python3
"""
Group Mass Function Evolution Plotter - FINAL COMPLETE VERSION

Creates mass function evolution plots for galaxy groups, similar to stellar mass function plots.
Generates multiple figures:
1. Group Halo Mass Function Evolution vs redshift (with 1-sigma shading)
2. Group Total Stellar Mass Function Evolution vs redshift (with 1-sigma shading)
3. Group SFR vs Stellar Mass Relation Evolution vs redshift
4. Group Stellar Mass vs Halo Mass Relation Evolution vs redshift

Features:
- Proper y-axis limits that match actual data range
- 1-sigma error shading for both mass functions
- Duplicate group removal within redshift bins
- Dynamic axis scaling
- Plasma colormap evolution

Usage:
    python plot_group_mass_functions.py --input group_aggregated_properties_evolution.csv --volume 100.0 --hubble_h 0.7
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import argparse
from pathlib import Path
import warnings
warnings.filterwarnings('ignore')

def find_snapshots_in_z_range(z_min, z_max, data):
    """Find data points within redshift range, removing duplicates per group"""
    z_range_data = data[(data['redshift'] >= z_min) & (data['redshift'] <= z_max)]
    
    # IMPORTANT: Remove duplicates - only keep one entry per group per redshift bin
    # This prevents double-counting groups that appear in multiple snapshots within the same z bin
    unique_groups = z_range_data.drop_duplicates(subset=['tracking_id'], keep='first')
    
    print(f"    Redshift bin {z_min}-{z_max}: {len(z_range_data)} entries -> {len(unique_groups)} unique groups")
    
    return unique_groups

def calculate_group_mass_function_with_errors(data_snapshots, volume, mass_bins):
    """
    Calculate group mass function with error estimates from multiple snapshots.
    
    Parameters:
    data_snapshots: list of arrays, each representing masses from one snapshot
    volume: simulation volume in (Mpc/h)^3
    mass_bins: mass bin edges for histogram
    
    Returns:
    mass_centers: bin centers
    phi_mean: mean number density [Mpc^-3 dex^-1]
    phi_err: 1-sigma error on phi
    """
    if len(data_snapshots) == 0:
        return np.array([]), np.array([]), np.array([])
    
    bin_width = mass_bins[1] - mass_bins[0]  # dex
    mass_centers = mass_bins[:-1] + bin_width/2
    
    # Calculate phi for each snapshot separately
    phi_snapshots = []
    
    for snapshot_data in data_snapshots:
        if len(snapshot_data) == 0:
            continue
            
        counts, bin_edges = np.histogram(snapshot_data, bins=mass_bins)
        phi_snap = counts.astype(float) / (volume * bin_width)
        phi_snapshots.append(phi_snap)
    
    if len(phi_snapshots) == 0:
        return mass_centers, np.zeros(len(mass_centers)), np.zeros(len(mass_centers))
    
    # Convert to numpy array for easier manipulation
    phi_snapshots = np.array(phi_snapshots)
    
    # Calculate mean and 1-sigma error (standard error of the mean)
    phi_mean = np.mean(phi_snapshots, axis=0)
    if len(phi_snapshots) > 1:
        phi_err = np.std(phi_snapshots, axis=0) / np.sqrt(len(phi_snapshots))
    else:
        # Poisson error approximation for single snapshot
        phi_err = np.sqrt(phi_mean * volume * bin_width) / (volume * bin_width)
    
    print(f"      Mass function with errors:")
    print(f"        {len(phi_snapshots)} snapshots processed")
    print(f"        Bin width: {bin_width:.3f} dex")
    print(f"        Volume: {volume:.1f} (Mpc/h)^3")
    print(f"        Max phi mean: {phi_mean.max():.3e}")
    print(f"        Max phi error: {phi_err.max():.3e}")
    
    return mass_centers, phi_mean, phi_err

def plot_group_halo_mass_function_evolution(data, volume, hubble_h=0.7, output_dir='./'):
    """
    Plot group halo mass function evolution with redshift.
    
    Parameters:
    data: DataFrame with group evolution data
    volume: simulation volume in (Mpc/h)^3
    hubble_h: Hubble parameter
    output_dir: output directory
    """
    print('Plotting group halo mass function evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Define redshift bins to match the stellar mass function figure
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    # Define colormap - from dark to light/orange (plasma)
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    # Check what halo mass columns are available
    halo_columns = [col for col in data.columns if 'mvir' in col.lower() or 'm_200' in col]
    print(f"Available halo mass columns: {halo_columns}")
    
    if len(halo_columns) == 0:
        print(f"ERROR: No halo mass column found in the data!")
        print(f"Available columns: {sorted(data.columns)}")
        print(f"\nTo fix this, you need to add halo mass to your aggregation function.")
        print(f"Add this to create_group_aggregated_evolution():")
        print(f"  # Add central galaxy halo mass")
        print(f"  if 'Mvir' in group_galaxies.columns:")
        print(f"      central_candidates = group_galaxies[group_galaxies['Type'] == 0]")
        print(f"      if len(central_candidates) > 0:")
        print(f"          central_mvir = central_candidates['Mvir'].iloc[0]")
        print(f"      else:")
        print(f"          central_mvir = group_galaxies.loc[group_galaxies['StellarMass'].idxmax(), 'Mvir']")
        print(f"      row['central_Mvir'] = central_mvir")
        print(f"\nSkipping halo mass function plot.")
        plt.close()
        return

    # Initialize array for dynamic y-axis limits
    all_phi_values = []
    
    # Calculate group halo mass function for each redshift bin
    for i, (z_min, z_max) in enumerate(z_bins):
        # Find groups in this redshift range
        z_data = find_snapshots_in_z_range(z_min, z_max, data)
        
        if len(z_data) == 0:
            continue

        # Get halo masses - check which column is available
        halo_mass_found = False
        if 'central_Mvir' in z_data.columns:
            halo_masses = z_data['central_Mvir'].dropna()
            mass_column_name = 'central_Mvir'
            halo_mass_found = True
        elif 'total_Mvir' in z_data.columns:
            halo_masses = z_data['total_Mvir'].dropna()
            mass_column_name = 'total_Mvir'
            halo_mass_found = True
        elif 'Mvir' in z_data.columns:
            halo_masses = z_data['Mvir'].dropna()
            mass_column_name = 'Mvir'
            halo_mass_found = True
        elif 'M_200' in z_data.columns:
            halo_masses = z_data['M_200'].dropna()
            mass_column_name = 'M_200'
            halo_mass_found = True
        else:
            continue
        
        if not halo_mass_found or len(halo_masses) == 0:
            continue
        
        print(f"  Redshift bin {z_min}-{z_max}: {len(halo_masses)} groups with valid {mass_column_name}")
        
        # Convert to log10 if not already
        if halo_masses.max() > 100:  # Assume linear masses if max > 100
            log_halo_masses = np.log10(halo_masses)
            print(f"    Linear masses detected, range: {halo_masses.min():.2e} - {halo_masses.max():.2e}")
            print(f"    Log masses range: {log_halo_masses.min():.2f} - {log_halo_masses.max():.2f}")
        else:  # Assume already in log
            log_halo_masses = halo_masses
            print(f"    Log masses detected, range: {log_halo_masses.min():.2f} - {log_halo_masses.max():.2f}")
        
        # Use different mass bins for highest redshift bins
        if i >= len(z_bins) - 4:  # Last 4 redshift bins
            mass_bins = np.arange(10.0, 15.5, 0.1)  # Lower mass limit for high-z
        else:
            mass_bins = np.arange(9.5, 15.5, 0.1)   # Full range for low-z
        
        # For error calculation, split data by snapshot if available
        if 'snapshot' in z_data.columns:
            # Group by snapshot within this redshift bin
            snapshot_data_list = []
            for snapshot, snap_group in z_data.groupby('snapshot'):
                snap_masses = snap_group[mass_column_name].dropna()
                if len(snap_masses) > 0:
                    if snap_masses.max() > 100:
                        snap_log_masses = np.log10(snap_masses)
                    else:
                        snap_log_masses = snap_masses
                    snapshot_data_list.append(snap_log_masses.values)
            
            # Calculate mass function with errors
            mass_centers, phi_mean, phi_err = calculate_group_mass_function_with_errors(
                snapshot_data_list, volume, mass_bins)
        else:
            # No snapshot info available, treat as single snapshot
            mass_centers, phi_mean, phi_err = calculate_group_mass_function_with_errors(
                [log_halo_masses.values], volume, mass_bins)
        
        # Store all phi values for y-axis limits
        valid_phi = phi_mean[phi_mean > 0]
        all_phi_values.extend(valid_phi)
        
        # Only plot where we have data
        valid = phi_mean > 0
        if not np.any(valid):
            print(f"    No valid phi values > 0")
            continue
        
        print(f"    Plotting {np.sum(valid)} valid points with error bars")
            
        # Plot the main curve
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], phi_mean[valid], 
                color=colors[i], linewidth=2, label=label)
        
        # Add 1-sigma shading
        ax.fill_between(mass_centers[valid], 
                       np.maximum(phi_mean[valid] - phi_err[valid], 1e-10), 
                       (phi_mean[valid] + phi_err[valid]),
                       color=colors[i], alpha=0.3)

    # Set log scale and limits
    ax.set_yscale('log')
    ax.set_xlim(10.0, 15.0)
    
    # Dynamically set y-axis limits based on actual data
    if len(all_phi_values) > 0:
        phi_min = min(all_phi_values) * 0.5  # Only slightly below actual minimum
        phi_max = max(all_phi_values) * 2.0  # Only slightly above actual maximum
        ax.set_ylim(phi_min, phi_max)
        print(f"Set halo mass function y-axis limits: {phi_min:.2e} to {phi_max:.2e}")
    else:
        ax.set_ylim(1e-8, 1e-2)  # Default limits
        print("Using default y-axis limits for halo mass function")

    # Labels and formatting
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{halo}} [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\phi_{\mathrm{group}}$ [Mpc$^{-3}$ dex$^{-1}$]', fontsize=14)

    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    # Create legend
    leg = ax.legend(loc='lower left', fontsize=10, frameon=False)
    for text in leg.get_texts():
        text.set_fontsize(10)

    # Title
    ax.set_title('Group Halo Mass Function Evolution', fontsize=16, fontweight='bold')

    plt.tight_layout()
    
    # Save the plot
    output_path = Path(output_dir)
    output_file = output_path / 'group_halo_mass_function_evolution.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f'Saved halo mass function plot to {output_file}')
    
    # Also save PDF
    output_file_pdf = output_path / 'group_halo_mass_function_evolution.pdf'
    plt.savefig(output_file_pdf, bbox_inches='tight')
    print(f'Saved PDF to {output_file_pdf}')
    
    plt.close()

def plot_group_stellar_mass_function_evolution(data, volume, hubble_h=0.7, output_dir='./'):
    """
    Plot group total stellar mass function evolution with redshift.
    
    Parameters:
    data: DataFrame with group evolution data
    volume: simulation volume in (Mpc/h)^3
    hubble_h: Hubble parameter
    output_dir: output directory
    """
    print('Plotting group total stellar mass function evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Use same redshift bins and colors
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    # Initialize array for dynamic y-axis limits
    all_phi_values = []
    
    # Calculate group stellar mass function for each redshift bin
    for i, (z_min, z_max) in enumerate(z_bins):
        # Find groups in this redshift range (NO DUPLICATES)
        z_data = find_snapshots_in_z_range(z_min, z_max, data)
        
        if len(z_data) == 0:
            continue

        # Get total stellar masses
        if 'total_StellarMass' in z_data.columns:
            stellar_masses = z_data['total_StellarMass'].dropna()
            mass_column_name = 'total_StellarMass'
        elif 'Sig_M' in z_data.columns:
            # Sig_M is already in log10, need to convert to linear first
            sig_m_data = z_data['Sig_M'].dropna()
            stellar_masses = 10**sig_m_data
            mass_column_name = 'Sig_M (converted from log)'
        else:
            print(f"  Warning: No total stellar mass column found for redshift bin {z_min}-{z_max}")
            continue
        
        print(f"  Redshift bin {z_min}-{z_max}: {len(stellar_masses)} groups with valid {mass_column_name}")
        
        if len(stellar_masses) == 0:
            print(f"    No valid stellar masses in this bin")
            continue
        
        # Convert to log10 if not already
        if stellar_masses.max() > 100:  # Assume linear masses if max > 100
            log_stellar_masses = np.log10(stellar_masses)
            print(f"    Linear masses detected, range: {stellar_masses.min():.2e} - {stellar_masses.max():.2e}")
            print(f"    Log masses range: {log_stellar_masses.min():.2f} - {log_stellar_masses.max():.2f}")
        else:  # Assume already in log
            log_stellar_masses = stellar_masses
            print(f"    Log masses detected, range: {log_stellar_masses.min():.2f} - {log_stellar_masses.max():.2f}")
        
        # Use different mass bins for highest redshift bins (similar to galaxy SMF)
        if i >= len(z_bins) - 4:  # Last 4 redshift bins
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.5, 12.5, 0.1)
        
        # For error calculation, split data by snapshot if available
        if 'snapshot' in z_data.columns:
            # Group by snapshot within this redshift bin
            snapshot_data_list = []
            for snapshot, snap_group in z_data.groupby('snapshot'):
                snap_masses = snap_group[mass_column_name].dropna()
                if len(snap_masses) > 0:
                    if snap_masses.max() > 100:
                        snap_log_masses = np.log10(snap_masses)
                    else:
                        snap_log_masses = snap_masses
                    snapshot_data_list.append(snap_log_masses.values)
            
            # Calculate mass function with errors
            mass_centers, phi_mean, phi_err = calculate_group_mass_function_with_errors(
                snapshot_data_list, volume, mass_bins)
        else:
            # No snapshot info available, treat as single snapshot
            mass_centers, phi_mean, phi_err = calculate_group_mass_function_with_errors(
                [log_stellar_masses.values], volume, mass_bins)
        
        # Store all phi values for y-axis limits
        valid_phi = phi_mean[phi_mean > 0]
        all_phi_values.extend(valid_phi)
        
        # Only plot where we have data
        valid = phi_mean > 0
        if not np.any(valid):
            print(f"    No valid phi values > 0")
            continue
        
        print(f"    Plotting {np.sum(valid)} valid points with error bars")
            
        # Plot the main curve
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], phi_mean[valid], 
                color=colors[i], linewidth=2, label=label)
        
        # Add 1-sigma shading
        ax.fill_between(mass_centers[valid], 
                       np.maximum(phi_mean[valid] - phi_err[valid], 1e-10), 
                       (phi_mean[valid] + phi_err[valid]),
                       color=colors[i], alpha=0.3)

    # Set log scale and limits
    ax.set_yscale('log')
    ax.set_xlim(8.0, 12.5)
    
    # Dynamically set y-axis limits based on actual data
    if len(all_phi_values) > 0:
        phi_min = min(all_phi_values) * 0.5  # Only slightly below actual minimum
        phi_max = max(all_phi_values) * 2.0  # Only slightly above actual maximum
        ax.set_ylim(phi_min, phi_max)
        print(f"Set stellar mass function y-axis limits: {phi_min:.2e} to {phi_max:.2e}")
    else:
        ax.set_ylim(1e-8, 1e-2)  # Default limits
        print("Using default y-axis limits for stellar mass function")

    # Labels and formatting
    ax.set_xlabel(r'$\log_{10} M_{*,\mathrm{group}} [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\phi_{\mathrm{group}}$ [Mpc$^{-3}$ dex$^{-1}$]', fontsize=14)

    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    # Create legend
    leg = ax.legend(loc='lower left', fontsize=10, frameon=False)
    for text in leg.get_texts():
        text.set_fontsize(10)

    # Title
    ax.set_title('Group Total Stellar Mass Function Evolution', fontsize=16, fontweight='bold')

    plt.tight_layout()
    
    # Save the plot
    output_path = Path(output_dir)
    output_file = output_path / 'group_stellar_mass_function_evolution.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f'Saved stellar mass function plot to {output_file}')
    
    # Also save PDF
    output_file_pdf = output_path / 'group_stellar_mass_function_evolution.pdf'
    plt.savefig(output_file_pdf, bbox_inches='tight')
    print(f'Saved PDF to {output_file_pdf}')
    
    plt.close()

def plot_group_sfr_stellar_mass_relation_evolution(data, output_dir='./'):
    """
    Plot group SFR vs stellar mass relation evolution with redshift.
    Similar to the SFR-stellar mass relation for individual galaxies.
    """
    print('Plotting group SFR vs stellar mass relation evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Use same redshift bins and colors
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    for i, (z_min, z_max) in enumerate(z_bins):
        # Find groups in this redshift range (NO DUPLICATES)
        z_data = find_snapshots_in_z_range(z_min, z_max, data)
        
        if len(z_data) == 0:
            continue

        # Get stellar masses and SFRs
        if 'total_StellarMass' not in z_data.columns or 'total_SFR' not in z_data.columns:
            continue
            
        # Filter for groups with positive stellar mass and SFR
        valid_groups = z_data[(z_data['total_StellarMass'] > 0) & (z_data['total_SFR'] > 0)]
        
        if len(valid_groups) == 0:
            continue
        
        # Convert masses to log10 if needed
        if valid_groups['total_StellarMass'].max() > 100:
            log_stellar_masses = np.log10(valid_groups['total_StellarMass'])
        else:
            log_stellar_masses = valid_groups['total_StellarMass']
            
        log_sfrs = np.log10(valid_groups['total_SFR'])
        
        # Use same mass bins as stellar mass function
        if i >= len(z_bins) - 4:
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.5, 12.5, 0.1)
            
        mass_centers = mass_bins[:-1] + 0.05
        
        # Bin by stellar mass and calculate mean SFR
        sfr_bin_means = np.zeros(len(mass_centers))
        sfr_bin_means.fill(np.nan)
        sfr_bin_stds = np.zeros(len(mass_centers))
        sfr_bin_stds.fill(np.nan)
        
        for j in range(len(mass_centers)):
            mask = (log_stellar_masses >= mass_bins[j]) & (log_stellar_masses < mass_bins[j+1])
            if np.sum(mask) > 2:  # Need at least 3 groups per bin
                sfr_bin_means[j] = np.mean(log_sfrs.values[mask])
                sfr_bin_stds[j] = np.std(log_sfrs.values[mask])
        
        # Plot only where we have valid data
        valid = ~np.isnan(sfr_bin_means)
        if not np.any(valid):
            continue
            
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], sfr_bin_means[valid], 
                color=colors[i], linewidth=2, label=label)
        
        # Add error shading if we have standard deviations
        valid_with_std = valid & ~np.isnan(sfr_bin_stds)
        if np.any(valid_with_std):
            ax.fill_between(mass_centers[valid_with_std], 
                           sfr_bin_means[valid_with_std] - sfr_bin_stds[valid_with_std], 
                           sfr_bin_means[valid_with_std] + sfr_bin_stds[valid_with_std],
                           color=colors[i], alpha=0.3)

    # Set limits and labels
    ax.set_xlim(8.0, 12.2)
    ax.set_ylim(-1, 3)
    ax.set_xlabel(r'$\log_{10} M_{*,\mathrm{group}} [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\log_{10}\mathrm{SFR}_{\mathrm{group}}\ [M_\odot/yr]$', fontsize=14)
    
    # Minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    
    # Legend
    leg = ax.legend(loc='lower right', fontsize=10, frameon=False, ncol=3)
    for text in leg.get_texts():
        text.set_fontsize(10)
    
    # Title
    ax.set_title('Group SFR vs Stellar Mass Relation Evolution', fontsize=16, fontweight='bold')
    
    plt.tight_layout()
    
    # Save
    output_path = Path(output_dir)
    output_file = output_path / 'group_sfr_stellar_mass_evolution.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f'Saved SFR-stellar mass relation plot to {output_file}')
    
    output_file_pdf = output_path / 'group_sfr_stellar_mass_evolution.pdf'
    plt.savefig(output_file_pdf, bbox_inches='tight')
    print(f'Saved PDF to {output_file_pdf}')
    
    plt.close()

def plot_group_stellar_mass_halo_mass_relation_evolution(data, output_dir='./'):
    """
    Plot group stellar mass vs halo mass relation evolution with redshift.
    Shows the relationship between group total stellar mass (x-axis) and group halo mass (y-axis).
    """
    print('Plotting group stellar mass vs halo mass relation evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Use same redshift bins and colors
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    # Check what columns are available
    halo_columns = [col for col in data.columns if 'mvir' in col.lower() or 'm_200' in col]
    stellar_columns = [col for col in data.columns if 'stellar' in col.lower()]
    
    print(f"Available halo mass columns: {halo_columns}")
    print(f"Available stellar mass columns: {stellar_columns}")

    for i, (z_min, z_max) in enumerate(z_bins):
        # Find groups in this redshift range (NO DUPLICATES)
        z_data = find_snapshots_in_z_range(z_min, z_max, data)
        
        if len(z_data) == 0:
            continue

        # Get stellar masses - check available columns
        stellar_mass_found = False
        stellar_masses = None
        stellar_column_name = None
        
        if 'total_StellarMass' in z_data.columns:
            stellar_masses = z_data['total_StellarMass'].dropna()
            stellar_column_name = 'total_StellarMass'
            stellar_mass_found = True
        elif 'Sig_M' in z_data.columns:
            # Sig_M is already in log10, convert to linear first
            sig_m_data = z_data['Sig_M'].dropna()
            stellar_masses = 10**sig_m_data
            stellar_column_name = 'Sig_M (converted from log)'
            stellar_mass_found = True

        # Get halo masses - check available columns
        halo_mass_found = False
        halo_masses = None
        halo_column_name = None
        
        if 'central_Mvir' in z_data.columns:
            halo_masses = z_data['central_Mvir'].dropna()
            halo_column_name = 'central_Mvir'
            halo_mass_found = True
        elif 'total_Mvir' in z_data.columns:
            halo_masses = z_data['total_Mvir'].dropna()
            halo_column_name = 'total_Mvir'
            halo_mass_found = True
        elif 'Mvir' in z_data.columns:
            halo_masses = z_data['Mvir'].dropna()
            halo_column_name = 'Mvir'
            halo_mass_found = True
        elif 'M_200' in z_data.columns:
            halo_masses = z_data['M_200'].dropna()
            halo_column_name = 'M_200'
            halo_mass_found = True

        if not stellar_mass_found or not halo_mass_found:
            print(f"  Warning: Missing required columns for redshift bin {z_min}-{z_max}")
            print(f"    Stellar mass found: {stellar_mass_found}")
            print(f"    Halo mass found: {halo_mass_found}")
            continue

        # Filter for groups with both valid stellar and halo masses
        if stellar_column_name == 'total_StellarMass':
            valid_groups = z_data[(z_data[stellar_column_name] > 0) & 
                                 (z_data[halo_column_name] > 0)].copy()
        else:  # Sig_M case
            valid_groups = z_data[(~z_data['Sig_M'].isna()) & 
                                 (z_data[halo_column_name] > 0)].copy()
        
        if len(valid_groups) == 0:
            print(f"  No valid groups with both stellar and halo masses for redshift bin {z_min}-{z_max}")
            continue
        
        print(f"  Redshift bin {z_min}-{z_max}: {len(valid_groups)} groups with valid {stellar_column_name} and {halo_column_name}")
        
        # Convert stellar masses to log10 if needed
        if stellar_column_name == 'total_StellarMass':
            if valid_groups[stellar_column_name].max() > 100:
                log_stellar_masses = np.log10(valid_groups[stellar_column_name])
            else:
                log_stellar_masses = valid_groups[stellar_column_name]
        else:  # Sig_M case - already in log
            log_stellar_masses = valid_groups['Sig_M']
            
        # Convert halo masses to log10 if needed  
        if valid_groups[halo_column_name].max() > 100:
            log_halo_masses = np.log10(valid_groups[halo_column_name])
        else:
            log_halo_masses = valid_groups[halo_column_name]
        
        print(f"    Stellar mass range: {log_stellar_masses.min():.2f} - {log_stellar_masses.max():.2f}")
        print(f"    Halo mass range: {log_halo_masses.min():.2f} - {log_halo_masses.max():.2f}")
        
        # Use same stellar mass bins as stellar mass function
        if i >= len(z_bins) - 4:  # Last 4 redshift bins
            stellar_mass_bins = np.arange(8.0, 12.5, 0.2)  # Wider bins for high-z
        else:
            stellar_mass_bins = np.arange(7.5, 12.5, 0.2)  # Wider bins for better statistics
            
        mass_centers = stellar_mass_bins[:-1] + 0.1
        
        # Bin by stellar mass and calculate mean halo mass
        halo_mass_bin_means = np.zeros(len(mass_centers))
        halo_mass_bin_means.fill(np.nan)
        halo_mass_bin_stds = np.zeros(len(mass_centers))
        halo_mass_bin_stds.fill(np.nan)
        
        for j in range(len(mass_centers)):
            mask = (log_stellar_masses >= stellar_mass_bins[j]) & (log_stellar_masses < stellar_mass_bins[j+1])
            if np.sum(mask) > 2:  # Need at least 3 groups per bin
                halo_mass_bin_means[j] = np.mean(log_halo_masses.values[mask])
                halo_mass_bin_stds[j] = np.std(log_halo_masses.values[mask])
        
        # Plot only where we have valid data
        valid = ~np.isnan(halo_mass_bin_means)
        if not np.any(valid):
            print(f"    No valid binned data for plotting")
            continue
        
        print(f"    Plotting {np.sum(valid)} valid stellar mass bins")
            
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], halo_mass_bin_means[valid], 
                color=colors[i], linewidth=2, label=label)
        
        # Add error shading if we have standard deviations
        valid_with_std = valid & ~np.isnan(halo_mass_bin_stds)
        if np.any(valid_with_std):
            ax.fill_between(mass_centers[valid_with_std], 
                           halo_mass_bin_means[valid_with_std] - halo_mass_bin_stds[valid_with_std], 
                           halo_mass_bin_means[valid_with_std] + halo_mass_bin_stds[valid_with_std],
                           color=colors[i], alpha=0.3)

    # Set limits and labels
    ax.set_xlim(8.7, 13.2)
    ax.set_ylim(10.5, 15.0)
    ax.set_xlabel(r'$\log_{10} M_{*,\mathrm{group}} [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{halo}} [M_\odot]$', fontsize=14)
    
    # Minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.1))
    
    # Legend
    leg = ax.legend(loc='lower right', fontsize=10, frameon=False, ncol=3)
    for text in leg.get_texts():
        text.set_fontsize(10)
    
    # Title
    ax.set_title('Group Stellar Mass vs Halo Mass Relation Evolution', fontsize=16, fontweight='bold')
    
    plt.tight_layout()
    
    # Save
    output_path = Path(output_dir)
    output_file = output_path / 'group_stellar_mass_halo_mass_evolution.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f'Saved stellar mass-halo mass relation plot to {output_file}')
    
    output_file_pdf = output_path / 'group_stellar_mass_halo_mass_evolution.pdf'
    plt.savefig(output_file_pdf, bbox_inches='tight')
    print(f'Saved PDF to {output_file_pdf}')
    
    plt.close()

def main():
    parser = argparse.ArgumentParser(description='Plot group mass function evolution')
    
    parser.add_argument('--input', type=str, required=True,
                       help='Path to group_aggregated_properties_evolution.csv')
    parser.add_argument('--volume', type=float, required=True,
                       help='Simulation volume in (Mpc/h)^3')
    parser.add_argument('--hubble_h', type=float, default=0.7,
                       help='Hubble parameter h')
    parser.add_argument('--volume_fraction', type=float, default=1.0,
                       help='Volume fraction of simulation used')
    parser.add_argument('--output_dir', type=str, default='./',
                       help='Output directory for plots')
    parser.add_argument('--plot_type', type=str, choices=['all', 'halo', 'stellar', 'sfr', 'smhm'],
                       default='all', help='Which plots to create')
    
    args = parser.parse_args()
    
    # Check if input file exists
    if not Path(args.input).exists():
        print(f"Error: Input file {args.input} not found!")
        return
    
    print("="*60)
    print("GROUP MASS FUNCTION EVOLUTION PLOTTER - FINAL COMPLETE VERSION")
    print("="*60)
    print(f"Input file: {args.input}")
    print(f"Volume: {args.volume} (Mpc/h)^3")
    print(f"Hubble h: {args.hubble_h}")
    print(f"Volume fraction: {args.volume_fraction}")
    print(f"Output directory: {args.output_dir}")
    
    # Load data
    print("\nLoading group evolution data...")
    data = pd.read_csv(args.input)
    print(f"Loaded {len(data)} data points for {data['tracking_id'].nunique()} groups")
    print(f"Redshift range: {data['redshift'].min():.3f} - {data['redshift'].max():.3f}")
    
    # Adjust volume for volume fraction
    effective_volume = args.volume * args.volume_fraction
    print(f"Effective volume: {effective_volume} (Mpc/h)^3")
    
    # Create output directory
    output_path = Path(args.output_dir)
    output_path.mkdir(exist_ok=True)
    
    # Create plots based on user selection
    if args.plot_type in ['all', 'halo']:
        print("\nCreating group halo mass function evolution plot...")
        plot_group_halo_mass_function_evolution(data, effective_volume, args.hubble_h, args.output_dir)
    
    if args.plot_type in ['all', 'stellar']:
        print("\nCreating group stellar mass function evolution plot...")
        plot_group_stellar_mass_function_evolution(data, effective_volume, args.hubble_h, args.output_dir)
    
    if args.plot_type in ['all', 'sfr']:
        print("\nCreating group SFR-stellar mass relation evolution plot...")
        plot_group_sfr_stellar_mass_relation_evolution(data, args.output_dir)
    
    if args.plot_type in ['all', 'smhm']:
        print("\nCreating group stellar mass-halo mass relation evolution plot...")
        plot_group_stellar_mass_halo_mass_relation_evolution(data, args.output_dir)
    
    print("\n" + "="*60)
    print("PLOTTING COMPLETE")
    print("="*60)

if __name__ == "__main__":
    main()