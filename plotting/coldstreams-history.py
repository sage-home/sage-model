#!/usr/bin/env python

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
import csv
import pandas as pd
from matplotlib.colors import LogNorm
from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model

FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

# Target redshifts for analysis
target_redshifts = [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.5, 2.0, 2.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.25, 6.5, 6.75, 7.0, 7.25, 7.5, 7.75, 8.0, 8.25, 8.5, 8.75, 9.0, 10.0, 11.0, 12.0]

# Plotting options
OutputFormat = '.pdf'
# plt.rcParams["figure.figsize"] = (12, 8)
# plt.rcParams["figure.dpi"] = 150
# plt.rcParams["font.size"] = 12
plt.style.use('/Users/mbradley/Documents/cohare_palatino_sty.mplstyle')

# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files"""
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    combined_data = None
    
    for model_file in model_files:
        property = h5.File(DirName + model_file, 'r')
        data = np.array(property[snap_num][param])
        
        if combined_data is None:
            combined_data = data
        else:
            combined_data = np.concatenate((combined_data, data))
            
    return combined_data

def find_closest_snapshot(target_z, redshift_list):
    """Find the snapshot closest to the target redshift"""
    differences = [abs(z - target_z) for z in redshift_list]
    return differences.index(min(differences))

def load_galaxy_data(snap):
    """Load all galaxy properties for a given snapshot"""
    Snapshot = 'Snap_'+str(snap)
    
    data = {}
    data['StellarMass'] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    data['SfrDisk'] = read_hdf(snap_num=Snapshot, param='SfrDisk')
    data['SfrBulge'] = read_hdf(snap_num=Snapshot, param='SfrBulge')
    data['ColdGas'] = read_hdf(snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
    data['HotGas'] = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    data['Mvir'] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    data['Type'] = read_hdf(snap_num=Snapshot, param='Type')
    data['H1Gas'] = read_hdf(snap_num=Snapshot, param='HI_gas') * 1.0e10 / Hubble_h
    data['H2Gas'] = read_hdf(snap_num=Snapshot, param='H2_gas') * 1.0e10 / Hubble_h
    
    # Try to read Dekel & Birnboim specific fields (adjust field names as needed)
    try:
        data['CriticalMassDB06'] = read_hdf(snap_num=Snapshot, param='CriticalMassDB06') * 1.0e10 / Hubble_h
        data['MvirToMcritRatio'] = read_hdf(snap_num=Snapshot, param='MvirToMcritRatio')
        data['ColdInflowMass'] = read_hdf(snap_num=Snapshot, param='ColdInflowMass') * 1.0e10 / Hubble_h
        data['HotInflowMass'] = read_hdf(snap_num=Snapshot, param='HotInflowMass') * 1.0e10 / Hubble_h
    except:
        # If these fields don't exist, calculate approximations
        print(f"Warning: Dekel & Birnboim fields not found for snap {snap}, using approximations")
        data['CriticalMassDB06'] = np.ones_like(data['Mvir']) * 1e12  # Placeholder
        data['MvirToMcritRatio'] = data['Mvir'] / data['CriticalMassDB06']
        data['ColdInflowMass'] = data['ColdGas'] * 0.1  # Placeholder
        data['HotInflowMass'] = data['HotGas'] * 0.1   # Placeholder
    
    return data

def plot_regime_diagram_combined(all_data, actual_redshifts, output_dir):
    """Plot 1: Multi-redshift regime diagram showing cold streams vs shock heated galaxies"""
    plt.figure(figsize=(12, 10))
    
    # Color maps for different redshifts - blues for cold, reds for hot
    blue_colors = plt.cm.Blues(np.linspace(0.4, 1.0, len(actual_redshifts)))
    red_colors = plt.cm.Reds(np.linspace(0.4, 1.0, len(actual_redshifts)))
    
    # Plot the 1:1 line (Mvir = Mcrit)
    mass_range = np.logspace(10, 15, 100)
    plt.loglog(mass_range, mass_range, 'k--', linewidth=2, label='Mvir = Mcrit')
    
    # Plot each redshift separately
    for i, (data, z) in enumerate(zip(all_data, actual_redshifts)):
        # Filter for central galaxies with reasonable masses
        centrals = (data['Type'] == 0) & (data['StellarMass'] > 1e8) & (data['Mvir'] > 0)
        
        # Check if we have the actual fields or placeholders
        mcrit_unique = len(np.unique(data['CriticalMassDB06']))
        ratio_unique = len(np.unique(data['MvirToMcritRatio']))
        
        print(f"z={z:.1f}: {np.sum(centrals)} central galaxies")
        print(f"z={z:.1f}: CriticalMassDB06 unique values: {mcrit_unique}")
        print(f"z={z:.1f}: MvirToMcritRatio unique values: {ratio_unique}")
        
        # Skip if we have placeholder data (all values the same)
        if mcrit_unique <= 1 or ratio_unique <= 1:
            print(f"z={z:.1f}: Skipping due to placeholder data")
            continue
            
        # Scatter plot colored by regime
        cold_regime = centrals & (data['MvirToMcritRatio'] < 1.0)
        hot_regime = centrals & (data['MvirToMcritRatio'] >= 1.0)
        
        print(f"z={z:.1f}: {np.sum(cold_regime)} cold, {np.sum(hot_regime)} hot galaxies")
        
        # Sample data if too many points (for better visualization)
        max_points = 1000
        
        # Cold regime (blue)
        if np.sum(cold_regime) > 0:
            mcrit_cold = data['CriticalMassDB06'][cold_regime]
            mvir_cold = data['Mvir'][cold_regime]
            
            # Don't sample if we have few points - show them all
            if len(mcrit_cold) > max_points:
                indices = np.random.choice(len(mcrit_cold), max_points, replace=False)
                mcrit_cold = mcrit_cold[indices]
                mvir_cold = mvir_cold[indices]
            
            plt.scatter(mcrit_cold, mvir_cold, c=blue_colors[i], alpha=0.8, s=30, 
                       label=f'Cold z={z:.1f} (n={np.sum(cold_regime)})', zorder=10)
            print(f"z={z:.1f}: Plotted {len(mcrit_cold)} cold points")
        
        # Hot regime (red)  
        if np.sum(hot_regime) > 0:
            mcrit_hot = data['CriticalMassDB06'][hot_regime]
            mvir_hot = data['Mvir'][hot_regime]
            
            if len(mcrit_hot) > max_points:
                indices = np.random.choice(len(mcrit_hot), max_points, replace=False)
                mcrit_hot = mcrit_hot[indices]
                mvir_hot = mvir_hot[indices]
            
            plt.scatter(mcrit_hot, mvir_hot, c=red_colors[i], alpha=0.4, s=20, 
                       label=f'Hot z={z:.1f} (n={np.sum(hot_regime)})', zorder=5)
            print(f"z={z:.1f}: Plotted {len(mcrit_hot)} hot points")
    
    plt.xlabel('Critical Mass Mcrit [M☉]')
    plt.ylabel('Virial Mass Mvir [M☉]')
    plt.title(f'Dekel & Birnboim Regime Diagram (z={actual_redshifts[0]:.1f}-{actual_redshifts[-1]:.1f})')
    plt.grid(True, alpha=0.3)
    
    # Put legend at bottom outside plot
    plt.legend(bbox_to_anchor=(0.5, -0.1), loc='upper center', ncol=5)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/dekel_regime_diagram_combined{OutputFormat}', bbox_inches='tight')
    plt.close()

def plot_cold_inflow_fraction_combined(all_data, actual_redshifts, output_dir):
    """Plot 2: Multi-redshift cold vs hot inflow fractions"""
    plt.figure(figsize=(12, 10))
    
    # Bin by halo mass
    mass_bins = np.logspace(10, 14, 20)
    mass_centers = (mass_bins[1:] * mass_bins[:-1])**0.5
    
    # Color map for different redshifts
    colors = plt.cm.viridis(np.linspace(0, 1, len(actual_redshifts)))
    
    # Plot each redshift separately
    for i, (data, z) in enumerate(zip(all_data, actual_redshifts)):
        centrals = (data['Type'] == 0) & (data['StellarMass'] > 1e8) & (data['Mvir'] > 0)
        total_inflow = data['ColdInflowMass'] + data['HotInflowMass']
        
        cold_fraction = np.zeros_like(data['ColdInflowMass'])
        mask = total_inflow > 0
        cold_fraction[mask] = data['ColdInflowMass'][mask] / total_inflow[mask]
        
        has_inflow = centrals & (total_inflow > 1e8)
        
        mvir_data = data['Mvir'][has_inflow]
        cold_frac_data = cold_fraction[has_inflow]
        
        if len(mvir_data) > 50:
            # Calculate median for each bin
            median_cold_frac = []
            for j in range(len(mass_bins)-1):
                in_bin = (mvir_data >= mass_bins[j]) & (mvir_data < mass_bins[j+1])
                if np.sum(in_bin) > 5:
                    median_cold_frac.append(np.median(cold_frac_data[in_bin]))
                else:
                    median_cold_frac.append(np.nan)
            
            # Remove NaN values
            valid = ~np.isnan(median_cold_frac)
            if np.sum(valid) > 0:
                median_cold_frac = np.array(median_cold_frac)[valid]
                mass_centers_valid = mass_centers[valid]
                
                # Plot median line for this redshift
                plt.plot(mass_centers_valid, median_cold_frac, color=colors[i], 
                        linewidth=2, label=f'z={z:.1f}')
    
    plt.axhline(y=0.5, color='k', linestyle='--', alpha=0.5, label='50% cold')
    plt.xlabel('Virial Mass [M☉]')
    plt.ylabel('Cold Inflow Fraction')
    plt.title(f'Cold Stream Efficiency vs Halo Mass (z={actual_redshifts[0]:.1f}-{actual_redshifts[-1]:.1f})')
    plt.xscale('log')
    plt.ylim(0, 1)
    plt.grid(True, alpha=0.3)
    
    # Put legend at bottom outside plot
    plt.legend(bbox_to_anchor=(0.5, -0.1), loc='upper center', ncol=5)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/cold_inflow_fraction_combined{OutputFormat}', bbox_inches='tight')
    plt.close()

def plot_sf_vs_stellar_mass_combined(all_data, actual_redshifts, output_dir):
    """Plot 3a: Multi-redshift star formation vs stellar mass by infall mode"""
    plt.figure(figsize=(12, 10))
    
    # Color map for different redshifts
    colors = plt.cm.viridis(np.linspace(0, 1, len(actual_redshifts)))
    
    # Bin by stellar mass
    mass_bins = np.linspace(9, 12, 12)
    mass_centers = (mass_bins[1:] + mass_bins[:-1]) / 2
    
    # Plot both cold and hot regimes for each redshift
    for i, (data, z) in enumerate(zip(all_data, actual_redshifts)):
        SFR = data['SfrDisk'] + data['SfrBulge']
        sSFR = np.log10(SFR / data['StellarMass'] + 1e-12)
        
        centrals = (data['Type'] == 0) & (data['StellarMass'] > 1e9) & (SFR > 1e-3)
        cold_regime = centrals & (data['MvirToMcritRatio'] < 1.0)
        hot_regime = centrals & (data['MvirToMcritRatio'] >= 1.0)
        
        # Cold regime
        stellar_mass_cold = np.log10(data['StellarMass'][cold_regime])
        sSFR_cold = sSFR[cold_regime]
        
        if len(stellar_mass_cold) > 10:
            cold_median = []
            for j in range(len(mass_bins)-1):
                in_bin = (stellar_mass_cold >= mass_bins[j]) & (stellar_mass_cold < mass_bins[j+1])
                if np.sum(in_bin) > 2:
                    cold_median.append(np.median(sSFR_cold[in_bin]))
                else:
                    cold_median.append(np.nan)
            
            valid = ~np.isnan(cold_median)
            if np.sum(valid) > 0:
                cold_median = np.array(cold_median)[valid]
                mass_centers_valid = mass_centers[valid]
                
                plt.plot(mass_centers_valid, cold_median, color=colors[i], 
                        linewidth=2, linestyle='-', label=f'Cold z={z:.1f}')
        
        # Hot regime
        stellar_mass_hot = np.log10(data['StellarMass'][hot_regime])
        sSFR_hot = sSFR[hot_regime]
        
        if len(stellar_mass_hot) > 10:
            hot_median = []
            for j in range(len(mass_bins)-1):
                in_bin = (stellar_mass_hot >= mass_bins[j]) & (stellar_mass_hot < mass_bins[j+1])
                if np.sum(in_bin) > 2:
                    hot_median.append(np.median(sSFR_hot[in_bin]))
                else:
                    hot_median.append(np.nan)
            
            valid = ~np.isnan(hot_median)
            if np.sum(valid) > 0:
                hot_median = np.array(hot_median)[valid]
                mass_centers_valid = mass_centers[valid]
                
                plt.plot(mass_centers_valid, hot_median, color=colors[i], 
                        linewidth=2, linestyle='--', label=f'Hot z={z:.1f}')
    
    plt.xlabel('log Stellar Mass [M☉]')
    plt.ylabel('log sSFR [yr⁻¹]')
    plt.title(f'Star Formation vs Infall Mode (z={actual_redshifts[0]:.1f}-{actual_redshifts[-1]:.1f})')
    plt.grid(True, alpha=0.3)
    
    # Put legend at bottom outside plot
    plt.legend(bbox_to_anchor=(0.5, -0.1), loc='upper center', ncol=6)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/sf_vs_stellar_mass_combined{OutputFormat}', bbox_inches='tight')
    plt.close()

def plot_gas_fraction_vs_halo_mass_combined(all_data, actual_redshifts, output_dir):
    """Plot 3b: Multi-redshift gas fraction vs halo mass by infall mode"""
    plt.figure(figsize=(12, 10))
    
    # Color map for different redshifts
    colors = plt.cm.viridis(np.linspace(0, 1, len(actual_redshifts)))
    
    # Bin by halo mass
    mvir_bins = np.linspace(10, 15, 12)
    mvir_centers = (mvir_bins[1:] + mvir_bins[:-1]) / 2
    
    # Plot both cold and hot regimes for each redshift
    for i, (data, z) in enumerate(zip(all_data, actual_redshifts)):
        centrals = (data['Type'] == 0) & (data['StellarMass'] > 1e9)
        cold_regime = centrals & (data['MvirToMcritRatio'] < 1.0)
        hot_regime = centrals & (data['MvirToMcritRatio'] >= 1.0)
        
        cold_gas_frac = data['ColdGas'] / (data['StellarMass'] + data['ColdGas'] + 1e-12)
        
        # Cold regime
        mvir_cold = np.log10(data['Mvir'][cold_regime])
        cold_gas_cold = cold_gas_frac[cold_regime]
        
        if len(mvir_cold) > 10:
            cold_gas_median = []
            for j in range(len(mvir_bins)-1):
                in_bin = (mvir_cold >= mvir_bins[j]) & (mvir_cold < mvir_bins[j+1])
                if np.sum(in_bin) > 2:
                    cold_gas_median.append(np.median(cold_gas_cold[in_bin]))
                else:
                    cold_gas_median.append(np.nan)
            
            valid = ~np.isnan(cold_gas_median)
            if np.sum(valid) > 0:
                cold_gas_median = np.array(cold_gas_median)[valid]
                mvir_centers_valid = mvir_centers[valid]
                
                plt.plot(mvir_centers_valid, cold_gas_median, color=colors[i], 
                        linewidth=2, linestyle='-', label=f'Cold z={z:.1f}')
        
        # Hot regime
        mvir_hot = np.log10(data['Mvir'][hot_regime])
        cold_gas_hot = cold_gas_frac[hot_regime]
        
        if len(mvir_hot) > 10:
            hot_gas_median = []
            for j in range(len(mvir_bins)-1):
                in_bin = (mvir_hot >= mvir_bins[j]) & (mvir_hot < mvir_bins[j+1])
                if np.sum(in_bin) > 2:
                    hot_gas_median.append(np.median(cold_gas_hot[in_bin]))
                else:
                    hot_gas_median.append(np.nan)
            
            valid = ~np.isnan(hot_gas_median)
            if np.sum(valid) > 0:
                hot_gas_median = np.array(hot_gas_median)[valid]
                mvir_centers_valid = mvir_centers[valid]
                
                plt.plot(mvir_centers_valid, hot_gas_median, color=colors[i], 
                        linewidth=2, linestyle='--', label=f'Hot z={z:.1f}')
    
    plt.xlabel('log Virial Mass [M☉]')
    plt.ylabel('Cold Gas Fraction')
    plt.title(f'Gas Content vs Infall Mode (z={actual_redshifts[0]:.1f}-{actual_redshifts[-1]:.1f})')
    plt.grid(True, alpha=0.3)
    
    # Put legend at bottom outside plot
    plt.legend(bbox_to_anchor=(0.5, -0.1), loc='upper center', ncol=6)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/gas_fraction_vs_halo_mass_combined{OutputFormat}', bbox_inches='tight')
    plt.close()

def plot_h2_fraction_combined(all_data, actual_redshifts, output_dir):
    """Plot 4: Multi-redshift molecular vs atomic gas in different regimes"""
    plt.figure(figsize=(12, 10))
    
    # Bin by stellar mass
    mass_bins = np.linspace(8, 12, 15)
    mass_centers = (mass_bins[1:] + mass_bins[:-1]) / 2
    
    # Color map for different redshifts
    colors = plt.cm.viridis(np.linspace(0, 1, len(actual_redshifts)))
    
    # Plot both cold and hot regimes for each redshift
    for i, (data, z) in enumerate(zip(all_data, actual_redshifts)):
        h2_fraction = data['H2Gas'] / (data['H1Gas'] + data['H2Gas'] + 1e-12)
        centrals = (data['Type'] == 0)
        has_gas = centrals & (data['ColdGas'] > 1e8) & (data['StellarMass'] > 1e8)
        
        cold_regime = has_gas & (data['MvirToMcritRatio'] < 1.0)
        hot_regime = has_gas & (data['MvirToMcritRatio'] >= 1.0)
        
        print(f"z={z:.1f}: {len(data['StellarMass'][cold_regime])} cold, {len(data['StellarMass'][hot_regime])} hot galaxies for H2 fraction")
        
        # Cold regime
        stellar_mass_cold = np.log10(data['StellarMass'][cold_regime])
        h2_frac_cold = h2_fraction[cold_regime]
        
        if len(stellar_mass_cold) > 10:
            cold_median = []
            for j in range(len(mass_bins)-1):
                in_bin = (stellar_mass_cold >= mass_bins[j]) & (stellar_mass_cold < mass_bins[j+1])
                if np.sum(in_bin) > 2:
                    cold_median.append(np.median(h2_frac_cold[in_bin]))
                else:
                    cold_median.append(np.nan)
            
            valid = ~np.isnan(cold_median)
            if np.sum(valid) > 0:
                cold_median = np.array(cold_median)[valid]
                mass_centers_valid = mass_centers[valid]
                
                plt.plot(mass_centers_valid, cold_median, color=colors[i], 
                        linewidth=2, linestyle='-', label=f'Cold z={z:.1f}')
                print(f"z={z:.1f}: Plotted {len(mass_centers_valid)} cold points")
        
        # Hot regime
        stellar_mass_hot = np.log10(data['StellarMass'][hot_regime])
        h2_frac_hot = h2_fraction[hot_regime]
        
        if len(stellar_mass_hot) > 10:
            hot_median = []
            for j in range(len(mass_bins)-1):
                in_bin = (stellar_mass_hot >= mass_bins[j]) & (stellar_mass_hot < mass_bins[j+1])
                if np.sum(in_bin) > 2:
                    hot_median.append(np.median(h2_frac_hot[in_bin]))
                else:
                    hot_median.append(np.nan)
            
            valid = ~np.isnan(hot_median)
            if np.sum(valid) > 0:
                hot_median = np.array(hot_median)[valid]
                mass_centers_valid = mass_centers[valid]
                
                plt.plot(mass_centers_valid, hot_median, color=colors[i], 
                        linewidth=2, linestyle='--', label=f'Hot z={z:.1f}')
                print(f"z={z:.1f}: Plotted {len(mass_centers_valid)} hot points")
    
    plt.xlabel('log Stellar Mass [M☉]')
    plt.ylabel('H₂ Fraction')
    plt.title(f'Molecular Gas vs Infall Mode (z={actual_redshifts[0]:.1f}-{actual_redshifts[-1]:.1f})')
    plt.grid(True, alpha=0.3)
    plt.ylim(0, 1)
    
    # Put legend at bottom outside plot
    plt.legend(bbox_to_anchor=(0.5, -0.1), loc='upper center', ncol=6)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/h2_fraction_by_regime_combined{OutputFormat}', bbox_inches='tight')
    plt.close()

def plot_mass_ratio_distribution_combined(all_data, actual_redshifts, output_dir):
    """Plot 5: Multi-redshift distribution of Mvir/Mcrit ratios"""
    plt.figure(figsize=(12, 10))
    
    # Color map for different redshifts
    colors = plt.cm.viridis(np.linspace(0, 1, len(actual_redshifts)))
    
    # Plot each redshift separately
    for i, (data, z) in enumerate(zip(all_data, actual_redshifts)):
        centrals = (data['Type'] == 0)
        valid_ratios = centrals & (data['MvirToMcritRatio'] > 0)
        
        if np.sum(valid_ratios) > 50:
            ratios = data['MvirToMcritRatio'][valid_ratios]
            log_ratios = np.log10(ratios)
            
            # Create histogram
            bins = np.linspace(-2, 2, 50)
            hist, bin_edges = np.histogram(log_ratios, bins=bins, density=True)
            bin_centers = (bin_edges[1:] + bin_edges[:-1]) / 2
            
            # Plot histogram as line
            plt.plot(bin_centers, hist, color=colors[i], linewidth=2, 
                    label=f'z={z:.1f}')
            
            # Print statistics
            cold_frac = np.sum(ratios < 1.0) / len(ratios)
            print(f"z={z:.1f}: Fraction in cold stream regime: {cold_frac:.3f}")
            print(f"z={z:.1f}: Median Mvir/Mcrit ratio: {np.median(ratios):.3f}")
    
    plt.axvline(x=0, color='k', linestyle='--', linewidth=2, label='Mvir = Mcrit')
    plt.xlabel('log(Mvir/Mcrit)')
    plt.ylabel('Probability Density')
    plt.title(f'Distribution of Mass Ratios (z={actual_redshifts[0]:.1f}-{actual_redshifts[-1]:.1f})')
    plt.grid(True, alpha=0.3)
    
    # Put legend at bottom outside plot
    plt.legend(bbox_to_anchor=(0.5, -0.1), loc='upper center', ncol=5)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/mass_ratio_distribution_combined{OutputFormat}', bbox_inches='tight')
    plt.close()

def create_evolution_plots(all_data, target_redshifts, output_dir):
    """Create plots showing evolution with redshift"""
    
    # 1. Evolution of cold stream fraction
    plt.figure(figsize=(10, 6))
    
    cold_fractions = []
    redshift_values = []
    
    for i, z in enumerate(target_redshifts):
        data = all_data[i]
        centrals = (data['Type'] == 0) & (data['Mvir'] > 1e11)
        
        if np.sum(centrals) > 0:
            cold_frac = np.sum(data['MvirToMcritRatio'][centrals] < 1.0) / np.sum(centrals)
            cold_fractions.append(cold_frac)
            redshift_values.append(z)
    
    plt.plot(redshift_values, cold_fractions, 'o-', linewidth=2, markersize=8)
    plt.xlabel('Redshift')
    plt.ylabel('Fraction in Cold Stream Regime')
    plt.title('Evolution of Cold Stream Fraction')
    plt.grid(True, alpha=0.3)
    plt.savefig(f'{output_dir}/cold_stream_evolution{OutputFormat}', bbox_inches='tight')
    plt.close()
    
    # 2. Evolution of characteristic masses
    plt.figure(figsize=(10, 6))
    
    for i, z in enumerate(target_redshifts):
        data = all_data[i]
        centrals = (data['Type'] == 0) & (data['Mvir'] > 1e10)
        
        if np.sum(centrals) > 10:
            # Plot median Mcrit vs redshift
            median_mcrit = np.median(data['CriticalMassDB06'][centrals])
            plt.scatter(z, median_mcrit, s=50, alpha=0.8, label=f'z={z:.1f}' if i < 5 else "")
    
    plt.xlabel('Redshift')
    plt.ylabel('Median Critical Mass [M☉]')
    plt.title('Evolution of Critical Mass Scale')
    plt.yscale('log')
    plt.grid(True, alpha=0.3)
    plt.savefig(f'{output_dir}/mcrit_evolution{OutputFormat}', bbox_inches='tight')
    plt.close()

# ==================================================================

if __name__ == '__main__':
    
    print('Running Multi-Redshift Dekel & Birnboim Analysis\n')
    
    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction
    
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    # Find snapshots closest to target redshifts
    target_snaps = []
    actual_redshifts = []
    
    for target_z in target_redshifts:
        snap = find_closest_snapshot(target_z, redshifts)
        target_snaps.append(snap)
        actual_redshifts.append(redshifts[snap])
        print(f"Target z={target_z:.1f} -> Snap {snap} (z={redshifts[snap]:.3f})")
    
    # Load data for all target redshifts
    print("\nLoading galaxy data...")
    all_data = []
    
    for i, snap in enumerate(target_snaps):
        print(f"Loading snapshot {snap} (z={actual_redshifts[i]:.3f})...")
        data = load_galaxy_data(snap)
        all_data.append(data)
    
    # Generate combined plots
    print("\nGenerating combined plots...")
    
    # Create combined plots for all redshifts
    plot_regime_diagram_combined(all_data, actual_redshifts, OutputDir)
    plot_cold_inflow_fraction_combined(all_data, actual_redshifts, OutputDir)
    plot_sf_vs_stellar_mass_combined(all_data, actual_redshifts, OutputDir)
    plot_gas_fraction_vs_halo_mass_combined(all_data, actual_redshifts, OutputDir)
    plot_h2_fraction_combined(all_data, actual_redshifts, OutputDir)
    plot_mass_ratio_distribution_combined(all_data, actual_redshifts, OutputDir)
    
    # Create evolution plots
    print("Creating evolution plots...")
    create_evolution_plots(all_data, actual_redshifts, OutputDir)
    
    print(f"\nAll plots saved to {OutputDir}")
    print("Generated plot types:")
    print("- dekel_regime_diagram_combined.pdf")
    print("- cold_inflow_fraction_combined.pdf") 
    print("- sf_vs_stellar_mass_combined.pdf")
    print("- gas_fraction_vs_halo_mass_combined.pdf")
    print("- h2_fraction_by_regime_combined.pdf")
    print("- mass_ratio_distribution_combined.pdf")
    print("- cold_stream_evolution.pdf")
    print("- mcrit_evolution.pdf")