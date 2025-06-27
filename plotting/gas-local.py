#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os

from random import sample, seed
from scipy import stats

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0  # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7500       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

# X-axis limits (set to None for automatic limits)
mvir_xlims = None      # e.g., [1e10, 1e15] for Mvir plot
stellar_xlims = [1e8, 1e12]   # e.g., [1e8, 1e12] for Stellar Mass plot

# Y-axis limits (set to None for automatic limits)  
gas_ylims = None       # e.g., [1e8, 1e13] for gas mass
infall_ylims = [1e-1, 1e5]    # Reasonable range for infall rates in M_sun/yr

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (16, 6)  # Wider for side-by-side plots
plt.rcParams["figure.dpi"] = 500
plt.rcParams["font.size"] = 14

# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        property = h5.File(DirName + model_file, 'r')
        data = np.array(property[snap_num][param])
        
        if combined_data is None:
            combined_data = data
        else:
            combined_data = np.concatenate((combined_data, data))
            
    return combined_data

def calculate_median_in_bins(x_data, y_data, nbins=20, x_range=None, min_per_bin=3):
    """Calculate median values in logarithmic bins"""
    
    # Remove invalid data
    valid = (x_data > 0) & (y_data > 0) & np.isfinite(x_data) & np.isfinite(y_data)
    x_clean = x_data[valid]
    y_clean = y_data[valid]
    
    print(f'  Valid data points: {len(x_clean)} out of {len(x_data)}')
    
    if len(x_clean) == 0:
        return np.array([]), np.array([])
    
    # Set x range if not provided
    if x_range is None:
        x_min, x_max = np.log10(np.min(x_clean)), np.log10(np.max(x_clean))
    else:
        x_min, x_max = np.log10(x_range[0]), np.log10(x_range[1])
    
    # Create logarithmic bins
    log_x_bins = np.linspace(x_min, x_max, nbins + 1)
    x_bins = 10**log_x_bins
    
    # Calculate medians in each bin
    bin_centers = []
    medians = []
    
    for i in range(nbins):
        mask = (x_clean >= x_bins[i]) & (x_clean < x_bins[i+1])
        
        if np.sum(mask) >= min_per_bin:  # Configurable minimum per bin
            bin_centers.append(np.sqrt(x_bins[i] * x_bins[i+1]))  # Geometric mean
            medians.append(np.median(y_clean[mask]))
    
    print(f'  Bins with data: {len(bin_centers)} out of {nbins}')
    return np.array(bin_centers), np.array(medians)

def plot_infall_rates_enhanced(ax, x_data, infall_cgm, infall_hot, xlabel, title, xlims=None, ylims=None):
    """Plot the infall rate components AND total for central galaxies"""
    
    print(f'\nPlotting {title}:')
    
    # Calculate total infall rate
    total_infall = infall_cgm + infall_hot
    
    # Filter out extreme outliers that might be due to bugs
    reasonable_cgm = infall_cgm[(infall_cgm > 0) & (infall_cgm < 1e8)]
    reasonable_hot = infall_hot[(infall_hot > 0) & (infall_hot < 1e8)]
    reasonable_total = total_infall[(total_infall > 0) & (total_infall < 1e8)]
    
    print(f'Filtered CGM infall: {len(reasonable_cgm)}/{len(infall_cgm)} values kept')
    print(f'Filtered Hot infall: {len(reasonable_hot)}/{len(infall_hot)} values kept')
    print(f'Filtered Total infall: {len(reasonable_total)}/{len(total_infall)} values kept')
    
    # Get corresponding x values for filtered data
    x_cgm_filtered = x_data[(infall_cgm > 0) & (infall_cgm < 1e8)]
    x_hot_filtered = x_data[(infall_hot > 0) & (infall_hot < 1e8)]
    x_total_filtered = x_data[(total_infall > 0) & (total_infall < 1e8)]
    
    # Calculate medians for each component
    print('Total Infall:')
    x_total, total_median = calculate_median_in_bins(x_total_filtered, reasonable_total, min_per_bin=1)
    print('Infall to CGM:')
    x_cgm, cgm_median = calculate_median_in_bins(x_cgm_filtered, reasonable_cgm, min_per_bin=1)
    print('Infall to Hot:')
    x_hot, hot_median = calculate_median_in_bins(x_hot_filtered, reasonable_hot, min_per_bin=1)
    
    # Plot the median lines with distinct colors and styles
    if len(x_total) > 0:
        ax.plot(x_total, total_median, 'black', linewidth=3.5, label='Total Infall', alpha=0.9, linestyle='-')
        print(f'  Plotted total infall line with {len(x_total)} points')
    
    if len(x_hot) > 0:
        ax.plot(x_hot, hot_median, 'red', linewidth=2.5, label='Infall to Hot', alpha=0.8, linestyle=':')
        print(f'  Plotted infall to Hot line with {len(x_hot)} points')
    
    if len(x_cgm) > 0:
        ax.plot(x_cgm, cgm_median, 'blue', linewidth=2.5, label='Infall to CGM', alpha=0.8, linestyle='--')
        print(f'  Plotted infall to CGM line with {len(x_cgm)} points')
    
    # Formatting
    ax.set_xlabel(xlabel)
    ax.set_ylabel(r'Infall Rate [M$_{\odot}$ yr$^{-1}$]')
    ax.set_title(title)
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    # Set axis limits if provided
    if xlims is not None:
        ax.set_xlim(xlims)
    
    if ylims is not None:
        ax.set_ylim(ylims)

def plot_gas_masses_with_virial(ax, x_data, hot_gas, cgm_gas, cold_gas, mvir_data, xlabel, title, xlims=None, ylims=None):
    """Plot gas masses AND virial mass for comparison"""
    
    print(f'\nPlotting {title}:')
    
    # Calculate medians for each component
    print('Virial Mass:')
    x_mvir, mvir_median = calculate_median_in_bins(x_data, mvir_data)
    print('Hot Gas:')
    x_hot, hot_median = calculate_median_in_bins(x_data, hot_gas)
    print('Cold Gas:')
    x_cold, cold_median = calculate_median_in_bins(x_data, cold_gas)
    print('CGM Gas:')
    x_cgm, cgm_median = calculate_median_in_bins(x_data, cgm_gas)
    
    # Plot the median lines
    if len(x_mvir) > 0:
        ax.plot(x_mvir, mvir_median, 'purple', linewidth=3.5, label='Virial Mass', alpha=0.9, linestyle='-')
    
    if len(x_hot) > 0:
        ax.plot(x_hot, hot_median, 'r-', linewidth=2.5, label='Hot Gas', alpha=0.8)
    
    if len(x_cold) > 0:
        ax.plot(x_cold, cold_median, 'g-', linewidth=2.5, label='Cold Gas', alpha=0.8)
    
    if len(x_cgm) > 0:
        ax.plot(x_cgm, cgm_median, 'b-', linewidth=2.5, label='CGM Gas', alpha=0.8)
    
    # Formatting
    ax.set_xlabel(xlabel)
    ax.set_ylabel(r'Mass [M$_{\odot}$]')
    ax.set_title(title)
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    # Set axis limits if provided
    if xlims is not None:
        ax.set_xlim(xlims)
    
    if ylims is not None:
        ax.set_ylim(ylims)

def plot_satellite_properties(ax, stellar_mass, infall_mvir, xlabel, title, xlims=None, ylims=None):
    """Plot satellite galaxy properties"""
    
    print(f'\nPlotting {title}:')
    
    print('Satellite Infall Mvir:')
    x_mvir, mvir_median = calculate_median_in_bins(stellar_mass, infall_mvir, min_per_bin=1)
    
    if len(x_mvir) > 0:
        ax.plot(x_mvir, mvir_median, 'orange', linewidth=2.5, label='Satellite Infall Mvir', alpha=0.8, linestyle='-')
        print(f'  Plotted satellite infall Mvir line with {len(x_mvir)} points')
    else:
        print('  No satellite infall Mvir line plotted - no valid data')
    
    # Formatting
    ax.set_xlabel(xlabel)
    ax.set_ylabel(r'Halo Mass at Infall [M$_{\odot}$]')
    ax.set_title(title)
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    # Set axis limits if provided
    if xlims is not None:
        ax.set_xlim(xlims)
    
    if ylims is not None:
        ax.set_ylim(ylims)

# ==================================================================

if __name__ == '__main__':

    print('Running enhanced analysis with total infall rates\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    # Read all the data
    CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    CGMgas = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
    InfallMvir = read_hdf(snap_num = Snapshot, param = 'infallMvir') * 1.0e10 / Hubble_h

    # Read infall rates - NO UNIT CONVERSION because they're already in M_sun/yr from C code
    try:
        infallToCGM = read_hdf(snap_num = Snapshot, param = 'CGMInflowRate')  # Already in M_sun/yr
        print('Successfully read infallToCGM')
    except:
        print('Warning: Could not read infallToCGM - setting to zeros')
        infallToCGM = np.zeros_like(Mvir)

    try:
        infallToHot = read_hdf(snap_num = Snapshot, param = 'HotGasInflowRate')  # Already in M_sun/yr
        print('Successfully read infallToHot')
    except:
        print('Warning: Could not read infallToHot - setting to zeros')
        infallToHot = np.zeros_like(Mvir)
    
    # # Apply sanity filter to remove extreme outliers that might be due to bugs
    # print(f'Before filtering: infallToHot max = {np.max(infallToHot):.3e} M_sun/yr')
    
    # Filter out unreasonably high infall rates (> 10^12 M_sun/yr)
    # infallToHot = np.where(infallToHot > 1e12, 0.0, infallToHot)
    # infallToCGM = np.where(infallToCGM > 1e10, 0.0, infallToCGM)  # More conservative for CGM
    
    # print(f'After filtering: infallToHot max = {np.max(infallToHot):.3e} M_sun/yr')

    # Type = read_hdf(snap_num = Snapshot, param = 'Type')

    # print(f'Total galaxies loaded: {len(StellarMass)}')
    
    # # Separate central and satellite galaxies
    # central_mask = (Type == 0) & (StellarMass > 0) & (Mvir > 0)
    # satellite_mask = (Type == 1) & (StellarMass > 0) & (InfallMvir > 0)
    
    # print(f'Central galaxies: {np.sum(central_mask)}')
    # print(f'Satellite galaxies: {np.sum(satellite_mask)}')
    
    # # Check values for central galaxies
    # central_cgm_infall = infallToCGM[central_mask]
    # central_hot_infall = infallToHot[central_mask]
    # total_infall_central = central_cgm_infall + central_hot_infall
    
    # print(f'Central galaxies with CGM infall > 0: {np.sum(central_cgm_infall > 0)}')
    # print(f'Central galaxies with Hot infall > 0: {np.sum(central_hot_infall > 0)}')
    # print(f'Central galaxies with Total infall > 0: {np.sum(total_infall_central > 0)}')
    
    # if np.sum(central_cgm_infall > 0) > 0:
    #     print(f'CGM infall median (>0): {np.median(central_cgm_infall[central_cgm_infall > 0]):.3e} M_sun/yr')
    # if np.sum(central_hot_infall > 0) > 0:
    #     print(f'Hot infall median (>0): {np.median(central_hot_infall[central_hot_infall > 0]):.3e} M_sun/yr')
    # if np.sum(total_infall_central > 0) > 0:
    #     print(f'Total infall median (>0): {np.median(total_infall_central[total_infall_central > 0]):.3e} M_sun/yr')
    
    # # Filter for central galaxies
    # stellar_central = StellarMass[central_mask]
    # mvir_central = Mvir[central_mask]
    # hot_central = HotGas[central_mask]
    # cgm_central = CGMgas[central_mask]
    # cold_central = ColdGas[central_mask]
    
    # # Filter for satellite galaxies
    # stellar_satellite = StellarMass[satellite_mask]
    # infall_mvir_satellite = InfallMvir[satellite_mask]
    
    # # Figure 1: Enhanced Gas Masses with Virial Mass
    # fig1, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    # plot_gas_masses_with_virial(ax1, mvir_central, hot_central, cgm_central, cold_central, mvir_central,
    #                            r'M$_{vir}$ [M$_{\odot}$]', 'Masses vs Virial Mass (Centrals)', 
    #                            xlims=mvir_xlims, ylims=gas_ylims)
    
    # plot_gas_masses_with_virial(ax2, stellar_central, hot_central, cgm_central, cold_central, mvir_central,
    #                            r'M$_*$ [M$_{\odot}$]', 'Masses vs Stellar Mass (Centrals)', 
    #                            xlims=stellar_xlims, ylims=gas_ylims)
    
    # plt.tight_layout()
    # output_filename1 = f'{OutputDir}enhanced_gas_masses_central{OutputFormat}'
    # plt.savefig(output_filename1, bbox_inches='tight', dpi=300)
    # print(f'Enhanced gas masses plot saved as: {output_filename1}')
    
    # # Figure 2: Enhanced Infall Rates for Central Galaxies
    # fig2, (ax3, ax4) = plt.subplots(1, 2, figsize=(16, 6))
    
    # plot_infall_rates_enhanced(ax3, mvir_central, central_cgm_infall, central_hot_infall,
    #                           r'M$_{vir}$ [M$_{\odot}$]', 'Infall Rates vs Virial Mass (Centrals)', 
    #                           xlims=mvir_xlims, ylims=infall_ylims)
    
    # plot_infall_rates_enhanced(ax4, stellar_central, central_cgm_infall, central_hot_infall,
    #                           r'M$_*$ [M$_{\odot}$]', 'Infall Rates vs Stellar Mass (Centrals)', 
    #                           xlims=stellar_xlims, ylims=infall_ylims)
    
    # plt.tight_layout()
    # output_filename2 = f'{OutputDir}enhanced_infall_rates_central{OutputFormat}'
    # plt.savefig(output_filename2, bbox_inches='tight', dpi=300)
    # print(f'Enhanced infall rates plot saved as: {output_filename2}')
    
    # # Figure 3: Satellite Properties
    # fig3, (ax5, ax6) = plt.subplots(1, 2, figsize=(16, 6))
    
    # plot_satellite_properties(ax5, stellar_satellite, infall_mvir_satellite,
    #                          r'M$_*$ [M$_{\odot}$]', 'Satellite Infall Mass vs Stellar Mass', 
    #                          xlims=stellar_xlims)
    
    # # Second subplot: Compare satellite infall masses with central galaxy current masses
    # if len(stellar_satellite) > 0 and len(mvir_central) > 0:
    #     # Plot satellite infall masses
    #     x_sat, sat_median = calculate_median_in_bins(stellar_satellite, infall_mvir_satellite, min_per_bin=3)
    #     if len(x_sat) > 0:
    #         ax6.plot(x_sat, sat_median, 'orange', linewidth=2.5, label='Satellite Infall Mvir', alpha=0.8)
        
    #     # Plot central galaxy current masses for comparison
    #     x_cent, cent_median = calculate_median_in_bins(stellar_central, mvir_central, min_per_bin=3)
    #     if len(x_cent) > 0:
    #         ax6.plot(x_cent, cent_median, 'purple', linewidth=2.5, label='Central Current Mvir', alpha=0.8)
    
    # ax6.set_xlabel(r'M$_*$ [M$_{\odot}$]')
    # ax6.set_ylabel(r'Halo Mass [M$_{\odot}$]')
    # ax6.set_title('Satellite vs Central Halo Masses')
    # ax6.set_xscale('log')
    # ax6.set_yscale('log')
    # ax6.grid(True, alpha=0.3)
    # ax6.legend()
    # if stellar_xlims: ax6.set_xlim(stellar_xlims)
    
    # plt.tight_layout()
    # output_filename3 = f'{OutputDir}enhanced_satellite_properties{OutputFormat}'
    # plt.savefig(output_filename3, bbox_inches='tight', dpi=300)
    # print(f'Enhanced satellite properties plot saved as: {output_filename3}')
    
    # # Display enhanced statistics
    # print('\n=== ENHANCED STATISTICS ===')
    
    # print('\nCENTRAL GALAXY STATISTICS:')
    # print(f'Hot Gas median: {np.median(hot_central[hot_central > 0]):.2e} M_sun')
    # print(f'CGM Gas median: {np.median(cgm_central[cgm_central > 0]):.2e} M_sun')
    # print(f'Cold Gas median: {np.median(cold_central[cold_central > 0]):.2e} M_sun')
    # print(f'Virial Mass median: {np.median(mvir_central[mvir_central > 0]):.2e} M_sun')
    
    # active_infall = total_infall_central > 0
    # if np.sum(active_infall) > 0:
    #     print(f'\nINFALL STATISTICS (Active galaxies: {np.sum(active_infall)}):')
    #     print(f'Total infall rate median: {np.median(total_infall_central[active_infall]):.2e} M_sun/yr')
    #     print(f'CGM infall rate median: {np.median(central_cgm_infall[active_infall]):.2e} M_sun/yr')
    #     print(f'Hot infall rate median: {np.median(central_hot_infall[active_infall]):.2e} M_sun/yr')
        
    #     # Calculate fractions
    #     cgm_fraction = central_cgm_infall[active_infall] / total_infall_central[active_infall]
    #     hot_fraction = central_hot_infall[active_infall] / total_infall_central[active_infall]
        
    #     print(f'CGM fraction median: {np.median(cgm_fraction):.2f}')
    #     print(f'Hot fraction median: {np.median(hot_fraction):.2f}')
        
    #     # Infall efficiency
    #     efficiency = total_infall_central[active_infall] / mvir_central[active_infall]
    #     print(f'Infall efficiency median: {np.median(efficiency):.2e} yr^-1')
    #     print(f'Typical halo doubling time: {1.0/np.median(efficiency)/1e9:.1f} Gyr')
    
    # print('\nSATELLITE GALAXY STATISTICS:')
    # print(f'Satellite count: {len(stellar_satellite)}')
    # print(f'Infall Mvir median: {np.median(infall_mvir_satellite):.2e} M_sun')
    # print(f'Current stellar mass median: {np.median(stellar_satellite):.2e} M_sun')
    
    # # Mass ratio analysis
    # if len(stellar_satellite) > 0:
    #     mass_ratio = infall_mvir_satellite / stellar_satellite
    #     print(f'Infall halo/stellar mass ratio median: {np.median(mass_ratio):.1f}')
    
    # print('\n=== SUCCESS: Enhanced analysis complete! ===')
    # print('Generated plots:')
    # print(f'  1. {output_filename1} - Gas masses + virial mass')
    # print(f'  2. {output_filename2} - Infall rates + total infall')  
    # print(f'  3. {output_filename3} - Satellite properties + comparisons')

    print(infallToHot, infallToCGM, Mvir, StellarMass, HotGas, CGMgas, ColdGas)