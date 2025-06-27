#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import os

from random import sample, seed
from scipy import stats

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium_CGMfirst/'
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
infall_ylims = [1e-3, 1e5]    # Reasonable range for infall rates in M_sun/yr

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (18, 12)  # Larger for comprehensive plots
plt.rcParams["figure.dpi"] = 500
plt.rcParams["font.size"] = 12

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

def plot_complete_gas_flows(ax, x_data, infall_to_cgm, infall_to_hot, transfer_cgm_to_hot, xlabel, title, xlims=None, ylims=None):
    """Plot all gas flow components: infall to CGM, direct infall to hot, and CGM->hot transfer"""
    
    print(f'\nPlotting {title}:')
    
    # Calculate total infall rate
    total_infall = infall_to_cgm + infall_to_hot
    
    # Filter out extreme outliers
    max_rate = 1e8  # Reasonable upper limit for infall rates
    
    reasonable_cgm = infall_to_cgm[(infall_to_cgm > 0) & (infall_to_cgm < max_rate)]
    reasonable_hot = infall_to_hot[(infall_to_hot > 0) & (infall_to_hot < max_rate)]
    reasonable_transfer = transfer_cgm_to_hot[(transfer_cgm_to_hot > 0) & (transfer_cgm_to_hot < max_rate)]
    reasonable_total = total_infall[(total_infall > 0) & (total_infall < max_rate)]
    
    print(f'Filtered Direct to CGM: {len(reasonable_cgm)}/{len(infall_to_cgm)} values kept')
    print(f'Filtered Direct to Hot: {len(reasonable_hot)}/{len(infall_to_hot)} values kept')
    print(f'Filtered CGM->Hot Transfer: {len(reasonable_transfer)}/{len(transfer_cgm_to_hot)} values kept')
    print(f'Filtered Total infall: {len(reasonable_total)}/{len(total_infall)} values kept')
    
    # Get corresponding x values for filtered data
    x_cgm = x_data[(infall_to_cgm > 0) & (infall_to_cgm < max_rate)]
    x_hot = x_data[(infall_to_hot > 0) & (infall_to_hot < max_rate)]
    x_transfer = x_data[(transfer_cgm_to_hot > 0) & (transfer_cgm_to_hot < max_rate)]
    x_total = x_data[(total_infall > 0) & (total_infall < max_rate)]
    
    # Calculate medians for each component
    print('Total Infall (CGM + Direct):')
    x_total_bins, total_median = calculate_median_in_bins(x_total, reasonable_total, min_per_bin=1)
    print('Direct Infall to CGM:')
    x_cgm_bins, cgm_median = calculate_median_in_bins(x_cgm, reasonable_cgm, min_per_bin=1)
    print('Direct Infall to Hot:')
    x_hot_bins, hot_median = calculate_median_in_bins(x_hot, reasonable_hot, min_per_bin=1)
    print('CGM->Hot Transfer:')
    x_transfer_bins, transfer_median = calculate_median_in_bins(x_transfer, reasonable_transfer, min_per_bin=1)
    
    # Plot the median lines with distinct colors and styles
    if len(x_total_bins) > 0:
        ax.plot(x_total_bins, total_median, 'black', linewidth=4, label='Total Infall', alpha=0.9, linestyle='-')
        print(f'  Plotted total infall line with {len(x_total_bins)} points')
    
    if len(x_cgm_bins) > 0:
        ax.plot(x_cgm_bins, cgm_median, 'blue', linewidth=3, label='Direct to CGM', alpha=0.8, linestyle='--')
        print(f'  Plotted direct to CGM line with {len(x_cgm_bins)} points')
    
    if len(x_hot_bins) > 0:
        ax.plot(x_hot_bins, hot_median, 'red', linewidth=3, label='Direct to Hot', alpha=0.8, linestyle=':')
        print(f'  Plotted direct to hot line with {len(x_hot_bins)} points')
    
    if len(x_transfer_bins) > 0:
        ax.plot(x_transfer_bins, transfer_median, 'orange', linewidth=3, label='CGM→Hot Transfer', alpha=0.8, linestyle='-.')
        print(f'  Plotted CGM->Hot transfer line with {len(x_transfer_bins)} points')
    
    # Formatting
    ax.set_xlabel(xlabel)
    ax.set_ylabel(r'Flow Rate [M$_{\odot}$ yr$^{-1}$]')
    ax.set_title(title)
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=10)
    
    # Set axis limits if provided
    if xlims is not None:
        ax.set_xlim(xlims)
    
    if ylims is not None:
        ax.set_ylim(ylims)

def plot_cgm_components(ax, x_data, cgm_total, cgm_pristine, cgm_enriched, xlabel, title, xlims=None, ylims=None):
    """Plot CGM gas components: total, pristine, and enriched"""
    
    print(f'\nPlotting {title}:')
    
    # Calculate medians for each component
    print('Total CGM Gas:')
    x_total, total_median = calculate_median_in_bins(x_data, cgm_total)
    print('Pristine CGM Gas:')
    x_pristine, pristine_median = calculate_median_in_bins(x_data, cgm_pristine)
    print('Enriched CGM Gas:')
    x_enriched, enriched_median = calculate_median_in_bins(x_data, cgm_enriched)
    
    # Plot the median lines
    if len(x_total) > 0:
        ax.plot(x_total, total_median, 'darkblue', linewidth=3.5, label='Total CGM', alpha=0.9, linestyle='-')
    
    if len(x_pristine) > 0:
        ax.plot(x_pristine, pristine_median, 'lightblue', linewidth=2.5, label='Pristine CGM', alpha=0.8, linestyle='--')
    
    if len(x_enriched) > 0:
        ax.plot(x_enriched, enriched_median, 'purple', linewidth=2.5, label='Enriched CGM', alpha=0.8, linestyle=':')
    
    # Formatting
    ax.set_xlabel(xlabel)
    ax.set_ylabel(r'CGM Gas Mass [M$_{\odot}$]')
    ax.set_title(title)
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=10)
    
    # Set axis limits if provided
    if xlims is not None:
        ax.set_xlim(xlims)
    
    if ylims is not None:
        ax.set_ylim(ylims)

def plot_flow_fractions(ax, x_data, infall_to_cgm, infall_to_hot, xlabel, title, xlims=None):
    """Plot the fraction of infall going to each pathway"""
    
    print(f'\nPlotting {title}:')
    
    total_infall = infall_to_cgm + infall_to_hot
    valid_mask = total_infall > 0
    
    cgm_fraction = np.zeros_like(infall_to_cgm)
    hot_fraction = np.zeros_like(infall_to_hot)
    
    cgm_fraction[valid_mask] = infall_to_cgm[valid_mask] / total_infall[valid_mask]
    hot_fraction[valid_mask] = infall_to_hot[valid_mask] / total_infall[valid_mask]
    
    # Calculate medians for fractions
    print('CGM Pathway Fraction:')
    x_cgm, cgm_frac_median = calculate_median_in_bins(x_data[valid_mask], cgm_fraction[valid_mask])
    print('Direct Hot Pathway Fraction:')
    x_hot, hot_frac_median = calculate_median_in_bins(x_data[valid_mask], hot_fraction[valid_mask])
    
    # Plot the fraction lines
    if len(x_cgm) > 0:
        ax.plot(x_cgm, cgm_frac_median, 'blue', linewidth=3, label='CGM Pathway Fraction', alpha=0.8)
    
    if len(x_hot) > 0:
        ax.plot(x_hot, hot_frac_median, 'red', linewidth=3, label='Direct Hot Fraction', alpha=0.8)
    
    # Add reference lines
    ax.axhline(y=0.5, color='gray', linestyle='--', alpha=0.5, label='50% Split')
    
    # Formatting
    ax.set_xlabel(xlabel)
    ax.set_ylabel('Fraction of Total Infall')
    ax.set_title(title)
    ax.set_xscale('log')
    ax.set_ylim(0, 1)
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=10)
    
    # Set axis limits if provided
    if xlims is not None:
        ax.set_xlim(xlims)

def create_flow_diagram(ax, galaxy_data):
    """Create a simplified flow diagram showing average gas pathways"""
    
    # Calculate mean flow rates for active galaxies
    active_mask = (galaxy_data['total_infall'] > 0)
    
    if np.sum(active_mask) == 0:
        ax.text(0.5, 0.5, 'No active infall detected', ha='center', va='center', transform=ax.transAxes)
        return
    
    mean_cgm_infall = np.mean(galaxy_data['infall_to_cgm'][active_mask])
    mean_hot_infall = np.mean(galaxy_data['infall_to_hot'][active_mask])
    mean_transfer = np.mean(galaxy_data['transfer_cgm_to_hot'][active_mask])
    
    total_input = mean_cgm_infall + mean_hot_infall
    
    # Create boxes for reservoirs
    ax.add_patch(mpatches.Rectangle((0.1, 0.8), 0.8, 0.15, facecolor='lightgray', alpha=0.5))
    ax.text(0.5, 0.875, 'Cosmic Infall', ha='center', va='center', fontsize=12, weight='bold')
    ax.text(0.5, 0.825, f'{total_input:.1e} M☉/yr', ha='center', va='center', fontsize=10)
    
    ax.add_patch(mpatches.Rectangle((0.05, 0.4), 0.35, 0.25, facecolor='lightblue', alpha=0.5))
    ax.text(0.225, 0.575, 'CGM', ha='center', va='center', fontsize=12, weight='bold')
    ax.text(0.225, 0.525, f'Inflow: {mean_cgm_infall:.1e}', ha='center', va='center', fontsize=9)
    ax.text(0.225, 0.475, f'Transfer: {mean_transfer:.1e}', ha='center', va='center', fontsize=9)
    
    ax.add_patch(mpatches.Rectangle((0.6, 0.4), 0.35, 0.25, facecolor='lightcoral', alpha=0.5))
    ax.text(0.775, 0.575, 'Hot Gas', ha='center', va='center', fontsize=12, weight='bold')
    ax.text(0.775, 0.525, f'Direct: {mean_hot_infall:.1e}', ha='center', va='center', fontsize=9)
    ax.text(0.775, 0.475, f'From CGM: {mean_transfer:.1e}', ha='center', va='center', fontsize=9)
    
    ax.add_patch(mpatches.Rectangle((0.3, 0.05), 0.4, 0.2, facecolor='lightgreen', alpha=0.5))
    ax.text(0.5, 0.15, 'Cold Gas\n(Star Formation)', ha='center', va='center', fontsize=12, weight='bold')
    
    # Draw arrows
    # Cosmic infall to CGM
    ax.annotate('', xy=(0.225, 0.65), xytext=(0.35, 0.8),
                arrowprops=dict(arrowstyle='->', lw=2, color='blue'))
    ax.text(0.25, 0.72, f'{mean_cgm_infall/total_input:.0%}', fontsize=10, color='blue')
    
    # Cosmic infall to Hot
    ax.annotate('', xy=(0.775, 0.65), xytext=(0.65, 0.8),
                arrowprops=dict(arrowstyle='->', lw=2, color='red'))
    ax.text(0.75, 0.72, f'{mean_hot_infall/total_input:.0%}', fontsize=10, color='red')
    
    # CGM to Hot transfer
    ax.annotate('', xy=(0.6, 0.525), xytext=(0.4, 0.525),
                arrowprops=dict(arrowstyle='->', lw=2, color='orange'))
    ax.text(0.5, 0.545, 'Transfer', fontsize=10, color='orange', ha='center')
    
    # Hot to Cold (cooling)
    ax.annotate('', xy=(0.5, 0.25), xytext=(0.775, 0.4),
                arrowprops=dict(arrowstyle='->', lw=2, color='green'))
    ax.text(0.65, 0.32, 'Cooling', fontsize=10, color='green')
    
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_title('Gas Flow Pathways (Average Rates)', fontsize=14, weight='bold')

# ==================================================================

if __name__ == '__main__':

    print('Running enhanced gas flow analysis with all new properties\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    # Read basic galaxy properties
    CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num = Snapshot, param = 'Type')

    # Read CGM components
    try:
        CGMgas = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
        print('Successfully read CGMgas')
    except:
        print('Warning: Could not read CGMgas - setting to zeros')
        CGMgas = np.zeros_like(Mvir)

    try:
        CGMgas_pristine = read_hdf(snap_num = Snapshot, param = 'CGMgas_pristine') * 1.0e10 / Hubble_h
        print('Successfully read CGMgas_pristine')
    except:
        print('Warning: Could not read CGMgas_pristine - setting to zeros')
        CGMgas_pristine = np.zeros_like(Mvir)

    try:
        CGMgas_enriched = read_hdf(snap_num = Snapshot, param = 'CGMgas_enriched') * 1.0e10 / Hubble_h
        print('Successfully read CGMgas_enriched')
    except:
        print('Warning: Could not read CGMgas_enriched - setting to zeros')
        CGMgas_enriched = np.zeros_like(Mvir)

    # Read all flow rates - NO UNIT CONVERSION because they're already in M_sun/yr
    try:
        InfallRate_to_CGM = read_hdf(snap_num = Snapshot, param = 'InfallRate_to_CGM')
        print('Successfully read InfallRate_to_CGM')
    except:
        print('Warning: Could not read InfallRate_to_CGM - setting to zeros')
        InfallRate_to_CGM = np.zeros_like(Mvir)

    try:
        InfallRate_to_Hot = read_hdf(snap_num = Snapshot, param = 'InfallRate_to_Hot')
        print('Successfully read InfallRate_to_Hot')
    except:
        print('Warning: Could not read InfallRate_to_Hot - setting to zeros')
        InfallRate_to_Hot = np.zeros_like(Mvir)

    try:
        TransferRate_CGM_to_Hot = read_hdf(snap_num = Snapshot, param = 'TransferRate_CGM_to_Hot')
        print('Successfully read TransferRate_CGM_to_Hot')
    except:
        print('Warning: Could not read TransferRate_CGM_to_Hot - setting to zeros')
        TransferRate_CGM_to_Hot = np.zeros_like(Mvir)
    
    # Apply sanity filters to remove extreme outliers
    print(f'Before filtering: InfallRate_to_Hot max = {np.max(InfallRate_to_Hot):.3e} M_sun/yr')
    print(f'Before filtering: InfallRate_to_CGM max = {np.max(InfallRate_to_CGM):.3e} M_sun/yr')
    print(f'Before filtering: TransferRate_CGM_to_Hot max = {np.max(TransferRate_CGM_to_Hot):.3e} M_sun/yr')
    
    # Filter out unreasonably high flow rates
    InfallRate_to_Hot = np.where(InfallRate_to_Hot > 1e12, 0.0, InfallRate_to_Hot)
    InfallRate_to_CGM = np.where(InfallRate_to_CGM > 1e10, 0.0, InfallRate_to_CGM)
    TransferRate_CGM_to_Hot = np.where(TransferRate_CGM_to_Hot > 1e10, 0.0, TransferRate_CGM_to_Hot)
    
    print(f'After filtering: InfallRate_to_Hot max = {np.max(InfallRate_to_Hot):.3e} M_sun/yr')
    print(f'After filtering: InfallRate_to_CGM max = {np.max(InfallRate_to_CGM):.3e} M_sun/yr')
    print(f'After filtering: TransferRate_CGM_to_Hot max = {np.max(TransferRate_CGM_to_Hot):.3e} M_sun/yr')

    print(f'Total galaxies loaded: {len(StellarMass)}')
    
    # Separate central and satellite galaxies
    central_mask = (Type == 0) & (StellarMass > 0) & (Mvir > 0)
    satellite_mask = (Type == 1) & (StellarMass > 0)
    
    print(f'Central galaxies: {np.sum(central_mask)}')
    print(f'Satellite galaxies: {np.sum(satellite_mask)}')
    
    # Filter for central galaxies
    stellar_central = StellarMass[central_mask]
    mvir_central = Mvir[central_mask]
    hot_central = HotGas[central_mask]
    cgm_central = CGMgas[central_mask]
    cgm_pristine_central = CGMgas_pristine[central_mask]
    cgm_enriched_central = CGMgas_enriched[central_mask]
    cold_central = ColdGas[central_mask]
    
    # Flow rates for central galaxies
    infall_to_cgm_central = InfallRate_to_CGM[central_mask]
    infall_to_hot_central = InfallRate_to_Hot[central_mask]
    transfer_cgm_to_hot_central = TransferRate_CGM_to_Hot[central_mask]
    total_infall_central = infall_to_cgm_central + infall_to_hot_central
    
    # Check flow rates
    print(f'\nFlow Rate Statistics for Central Galaxies:')
    print(f'Galaxies with CGM infall > 0: {np.sum(infall_to_cgm_central > 0)}')
    print(f'Galaxies with direct hot infall > 0: {np.sum(infall_to_hot_central > 0)}')
    print(f'Galaxies with CGM->Hot transfer > 0: {np.sum(transfer_cgm_to_hot_central > 0)}')
    print(f'Galaxies with total infall > 0: {np.sum(total_infall_central > 0)}')
    
    if np.sum(infall_to_cgm_central > 0) > 0:
        print(f'CGM infall median (>0): {np.median(infall_to_cgm_central[infall_to_cgm_central > 0]):.3e} M_sun/yr')
    if np.sum(infall_to_hot_central > 0) > 0:
        print(f'Direct hot infall median (>0): {np.median(infall_to_hot_central[infall_to_hot_central > 0]):.3e} M_sun/yr')
    if np.sum(transfer_cgm_to_hot_central > 0) > 0:
        print(f'CGM->Hot transfer median (>0): {np.median(transfer_cgm_to_hot_central[transfer_cgm_to_hot_central > 0]):.3e} M_sun/yr')
    
    # Create comprehensive plots
    fig = plt.figure(figsize=(18, 16))
    
    # 1. Complete gas flow rates vs virial mass
    ax1 = plt.subplot(3, 3, 1)
    plot_complete_gas_flows(ax1, mvir_central, infall_to_cgm_central, infall_to_hot_central, transfer_cgm_to_hot_central,
                           r'M$_{vir}$ [M$_{\odot}$]', 'Gas Flow Rates vs Virial Mass', 
                           xlims=mvir_xlims, ylims=infall_ylims)
    
    # 2. Complete gas flow rates vs stellar mass
    ax2 = plt.subplot(3, 3, 2)
    plot_complete_gas_flows(ax2, stellar_central, infall_to_cgm_central, infall_to_hot_central, transfer_cgm_to_hot_central,
                           r'M$_*$ [M$_{\odot}$]', 'Gas Flow Rates vs Stellar Mass', 
                           xlims=stellar_xlims, ylims=infall_ylims)
    
    # 3. Flow fractions vs virial mass
    ax3 = plt.subplot(3, 3, 3)
    plot_flow_fractions(ax3, mvir_central, infall_to_cgm_central, infall_to_hot_central,
                       r'M$_{vir}$ [M$_{\odot}$]', 'Infall Pathway Fractions vs Virial Mass', 
                       xlims=mvir_xlims)
    
    # 4. CGM components vs virial mass
    ax4 = plt.subplot(3, 3, 4)
    plot_cgm_components(ax4, mvir_central, cgm_central, cgm_pristine_central, cgm_enriched_central,
                       r'M$_{vir}$ [M$_{\odot}$]', 'CGM Components vs Virial Mass', 
                       xlims=mvir_xlims, ylims=gas_ylims)
    
    # 5. CGM components vs stellar mass
    ax5 = plt.subplot(3, 3, 5)
    plot_cgm_components(ax5, stellar_central, cgm_central, cgm_pristine_central, cgm_enriched_central,
                       r'M$_*$ [M$_{\odot}$]', 'CGM Components vs Stellar Mass', 
                       xlims=stellar_xlims, ylims=gas_ylims)
    
    # 6. Flow fractions vs stellar mass
    ax6 = plt.subplot(3, 3, 6)
    plot_flow_fractions(ax6, stellar_central, infall_to_cgm_central, infall_to_hot_central,
                       r'M$_*$ [M$_{\odot}$]', 'Infall Pathway Fractions vs Stellar Mass', 
                       xlims=stellar_xlims)
    
    # 7. Gas masses comparison
    ax7 = plt.subplot(3, 3, 7)
    x_hot, hot_median = calculate_median_in_bins(stellar_central, hot_central)
    x_cgm, cgm_median = calculate_median_in_bins(stellar_central, cgm_central)
    x_cold, cold_median = calculate_median_in_bins(stellar_central, cold_central)
    
    if len(x_hot) > 0:
        ax7.plot(x_hot, hot_median, 'r-', linewidth=2.5, label='Hot Gas', alpha=0.8)
    if len(x_cgm) > 0:
        ax7.plot(x_cgm, cgm_median, 'b-', linewidth=2.5, label='CGM Gas', alpha=0.8)
    if len(x_cold) > 0:
        ax7.plot(x_cold, cold_median, 'g-', linewidth=2.5, label='Cold Gas', alpha=0.8)
    
    ax7.set_xlabel(r'M$_*$ [M$_{\odot}$]')
    ax7.set_ylabel(r'Gas Mass [M$_{\odot}$]')
    ax7.set_title('Gas Reservoir Masses')
    ax7.set_xscale('log')
    ax7.set_yscale('log')
    ax7.grid(True, alpha=0.3)
    ax7.legend(fontsize=10)
    if stellar_xlims: ax7.set_xlim(stellar_xlims)
    if gas_ylims: ax7.set_ylim(gas_ylims)
    
    # 8. Flow diagram
    ax8 = plt.subplot(3, 3, 8)
    galaxy_data = {
        'infall_to_cgm': infall_to_cgm_central,
        'infall_to_hot': infall_to_hot_central,
        'transfer_cgm_to_hot': transfer_cgm_to_hot_central,
        'total_infall': total_infall_central
    }
    create_flow_diagram(ax8, galaxy_data)
    
    # 9. CGM pristine vs enriched fraction
    ax9 = plt.subplot(3, 3, 9)
    valid_cgm = cgm_central > 0
    if np.sum(valid_cgm) > 0:
        pristine_frac = np.zeros_like(cgm_central)
        enriched_frac = np.zeros_like(cgm_central)
        pristine_frac[valid_cgm] = cgm_pristine_central[valid_cgm] / cgm_central[valid_cgm]
        enriched_frac[valid_cgm] = cgm_enriched_central[valid_cgm] / cgm_central[valid_cgm]
        
        x_prist, prist_median = calculate_median_in_bins(stellar_central[valid_cgm], pristine_frac[valid_cgm])
        x_enrich, enrich_median = calculate_median_in_bins(stellar_central[valid_cgm], enriched_frac[valid_cgm])
        
        if len(x_prist) > 0:
            ax9.plot(x_prist, prist_median, 'lightblue', linewidth=3, label='Pristine Fraction', alpha=0.8)
        if len(x_enrich) > 0:
            ax9.plot(x_enrich, enrich_median, 'purple', linewidth=3, label='Enriched Fraction', alpha=0.8)
    
    ax9.axhline(y=0.5, color='gray', linestyle='--', alpha=0.5)
    ax9.set_xlabel(r'M$_*$ [M$_{\odot}$]')
    ax9.set_ylabel('CGM Component Fraction')
    ax9.set_title('CGM Pristine vs Enriched Fractions')
    ax9.set_xscale('log')
    ax9.set_ylim(0, 1)
    ax9.grid(True, alpha=0.3)
    ax9.legend(fontsize=10)
    if stellar_xlims: ax9.set_xlim(stellar_xlims)
    
    plt.tight_layout()
    output_filename = f'{OutputDir}complete_gas_flow_analysis{OutputFormat}'
    plt.savefig(output_filename, bbox_inches='tight', dpi=300)
    print(f'Complete gas flow analysis saved as: {output_filename}')
    
    # Enhanced Statistics Summary
    print('\n' + '='*60)
    print('ENHANCED GAS FLOW STATISTICS')
    print('='*60)
    
    active_infall = total_infall_central > 0
    if np.sum(active_infall) > 0:
        print(f'\nACTIVE GALAXIES: {np.sum(active_infall)} out of {len(total_infall_central)}')
        
        # Flow rates
        print(f'\nFLOW RATES (Active Galaxies Only):')
        print(f'Total infall rate median:     {np.median(total_infall_central[active_infall]):.2e} M☉/yr')
        print(f'Direct to CGM rate median:    {np.median(infall_to_cgm_central[active_infall]):.2e} M☉/yr')
        print(f'Direct to Hot rate median:    {np.median(infall_to_hot_central[active_infall]):.2e} M☉/yr')
        print(f'CGM→Hot transfer median:      {np.median(transfer_cgm_to_hot_central[active_infall]):.2e} M☉/yr')
        
        # Pathway fractions
        cgm_pathway_frac = infall_to_cgm_central[active_infall] / total_infall_central[active_infall]
        hot_pathway_frac = infall_to_hot_central[active_infall] / total_infall_central[active_infall]
        
        print(f'\nPATHWAY FRACTIONS:')
        print(f'CGM pathway fraction median:  {np.median(cgm_pathway_frac):.2f} ({np.median(cgm_pathway_frac)*100:.1f}%)')
        print(f'Direct hot fraction median:   {np.median(hot_pathway_frac):.2f} ({np.median(hot_pathway_frac)*100:.1f}%)')
        
        # CGM composition
        active_cgm = (cgm_central > 0) & active_infall
        if np.sum(active_cgm) > 0:
            pristine_frac_active = cgm_pristine_central[active_cgm] / cgm_central[active_cgm]
            enriched_frac_active = cgm_enriched_central[active_cgm] / cgm_central[active_cgm]
            
            print(f'\nCGM COMPOSITION (Galaxies with CGM > 0):')
            print(f'Pristine fraction median:     {np.median(pristine_frac_active):.2f} ({np.median(pristine_frac_active)*100:.1f}%)')
            print(f'Enriched fraction median:     {np.median(enriched_frac_active):.2f} ({np.median(enriched_frac_active)*100:.1f}%)')
        
        # Flow efficiency
        efficiency = total_infall_central[active_infall] / mvir_central[active_infall]
        print(f'\nFLOW EFFICIENCY:')
        print(f'Infall rate / Mvir median:    {np.median(efficiency):.2e} yr⁻¹')
        print(f'Typical halo doubling time:   {1.0/np.median(efficiency)/1e9:.1f} Gyr')
    
    print(f'\nGAS RESERVOIR MASSES:')
    print(f'Hot gas median:               {np.median(hot_central[hot_central > 0]):.2e} M☉')
    print(f'Total CGM median:             {np.median(cgm_central[cgm_central > 0]):.2e} M☉')
    print(f'Pristine CGM median:          {np.median(cgm_pristine_central[cgm_pristine_central > 0]):.2e} M☉')
    print(f'Enriched CGM median:          {np.median(cgm_enriched_central[cgm_enriched_central > 0]):.2e} M☉')
    print(f'Cold gas median:              {np.median(cold_central[cold_central > 0]):.2e} M☉')
    
    print('\n' + '='*60)
    print('SUCCESS: Enhanced gas flow analysis complete!')
    print(f'Generated comprehensive plot: {output_filename}')
    print('='*60)