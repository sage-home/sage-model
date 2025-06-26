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

# Example usage:
# mvir_xlims = [1e10, 1e15]     # Set Mvir x-axis from 10^10 to 10^15 M_sun
# stellar_xlims = [1e8, 1e12]   # Set stellar mass x-axis from 10^8 to 10^12 M_sun  
# gas_ylims = [1e8, 1e13]       # Set gas mass y-axis from 10^8 to 10^13 M_sun

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

def plot_gas_components(ax, x_data, hot_gas, cgm_gas, infall_gas, xlabel, title, xlims=None, ylims=None):
    """Plot the three gas component median lines"""
    
    print(f'\nPlotting {title}:')
    
    # Calculate medians for each component
    print('Hot Gas:')
    x_hot, hot_median = calculate_median_in_bins(x_data, hot_gas)
    print('CGM Gas:')
    x_cgm, cgm_median = calculate_median_in_bins(x_data, cgm_gas)
    print('Infall Gas:')
    x_infall, infall_median = calculate_median_in_bins(x_data, infall_gas, min_per_bin=1)  # Lower threshold for infall
    
    # Plot the median lines
    if len(x_hot) > 0:
        ax.plot(x_hot, hot_median, 'r-', linewidth=2.5, label='Hot Gas', alpha=0.8)
    
    if len(x_cgm) > 0:
        ax.plot(x_cgm, cgm_median, 'b-', linewidth=2.5, label='CGM Gas', alpha=0.8)
    
    if len(x_infall) > 0:
        ax.plot(x_infall, infall_median, 'g-', linewidth=2.5, label='Infall Mvir', alpha=0.8)
        print(f'  Plotted infall line with {len(x_infall)} points')
    else:
        print('  No infall line plotted - no valid data')
    
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
        print(f'  Set x-limits to: {xlims}')
    
    if ylims is not None:
        ax.set_ylim(ylims)
        print(f'  Set y-limits to: {ylims}')


# ==================================================================

if __name__ == '__main__':

    print('Running allresults (local)\n')

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
    BulgeMass = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
    BlackHoleMass = read_hdf(snap_num = Snapshot, param = 'BlackHoleMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
    DiskRadius = read_hdf(snap_num = Snapshot, param = 'DiskRadius') / Hubble_h
    H1Gas = read_hdf(snap_num = Snapshot, param = 'H1_gas') * 1.0e10 / Hubble_h
    H2Gas = read_hdf(snap_num = Snapshot, param = 'H2_gas') * 1.0e10 / Hubble_h
    MetalsColdGas = read_hdf(snap_num = Snapshot, param = 'MetalsColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    EjectedMass = read_hdf(snap_num = Snapshot, param = 'EjectedMass') * 1.0e10 / Hubble_h
    IntraClusterStars = read_hdf(snap_num = Snapshot, param = 'IntraClusterStars') * 1.0e10 / Hubble_h
    InfallMvir = read_hdf(snap_num = Snapshot, param = 'infallMvir') * 1.0e10 / Hubble_h
    outflowrate = read_hdf(snap_num = Snapshot, param = 'OutflowRate') * 1.0e10 / Hubble_h
    cgm = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h

    Vvir = read_hdf(snap_num = Snapshot, param = 'Vvir')
    Vmax = read_hdf(snap_num = Snapshot, param = 'Vmax')
    Rvir = read_hdf(snap_num = Snapshot, param = 'Rvir')
    SfrDisk = read_hdf(snap_num = Snapshot, param = 'SfrDisk')
    SfrBulge = read_hdf(snap_num = Snapshot, param = 'SfrBulge')

    CentralGalaxyIndex = read_hdf(snap_num = Snapshot, param = 'CentralGalaxyIndex')
    Type = read_hdf(snap_num = Snapshot, param = 'Type')
    Posx = read_hdf(snap_num = Snapshot, param = 'Posx')
    Posy = read_hdf(snap_num = Snapshot, param = 'Posy')
    Posz = read_hdf(snap_num = Snapshot, param = 'Posz')

    print(f'Total galaxies loaded: {len(StellarMass)}')
    
    # Check InfallMvir values
    print(f'InfallMvir > 0: {np.sum(InfallMvir > 0)}')
    print(f'InfallMvir for Type 0: {np.sum((InfallMvir > 0) & (Type == 0))}')
    print(f'InfallMvir for Type 1: {np.sum((InfallMvir > 0) & (Type == 1))}')
    print(f'InfallMvir for Type 2: {np.sum((InfallMvir > 0) & (Type == 2))}')
    
    # Filter for valid galaxies (include all types since InfallMvir is for satellites)
    valid_mask = (StellarMass > 0) & (Mvir > 0)  # All galaxy types
    
    print(f'All galaxies for analysis: {np.sum(valid_mask)}')
    
    # Apply filters
    stellar_mass_filtered = StellarMass[valid_mask]
    mvir_filtered = Mvir[valid_mask]
    hot_gas_filtered = HotGas[valid_mask]
    cgm_filtered = cgm[valid_mask]
    infall_filtered = InfallMvir[valid_mask]  # Using InfallMvir as "Infall Gas"
    
    # Create the figure with two subplots
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    # Left plot: vs Mvir
    plot_gas_components(ax1, mvir_filtered, hot_gas_filtered, cgm_filtered, 
                       infall_filtered, r'M$_{vir}$ [M$_{\odot}$]', 
                       'Gas Components vs Virial Mass', xlims=mvir_xlims, ylims=gas_ylims)
    
    # Right plot: vs Stellar Mass
    plot_gas_components(ax2, stellar_mass_filtered, hot_gas_filtered, cgm_filtered, 
                       infall_filtered, r'M$_*$ [M$_{\odot}$]', 
                       'Gas Components vs Stellar Mass', xlims=stellar_xlims, ylims=gas_ylims)
    
    # Adjust layout and save
    plt.tight_layout()
    
    # Save the plot
    output_filename = f'{OutputDir}gas_components_comparison{OutputFormat}'
    plt.savefig(output_filename, bbox_inches='tight', dpi=300)
    print(f'Plot saved as: {output_filename}')
    
    # Display some statistics
    print('\nData Statistics:')
    print(f'Hot Gas median: {np.median(hot_gas_filtered[hot_gas_filtered > 0]):.2e} M_sun')
    print(f'CGM Gas median: {np.median(cgm_filtered[cgm_filtered > 0]):.2e} M_sun')
    print(f'Infall Mvir median: {np.median(infall_filtered[infall_filtered > 0]):.2e} M_sun')
    print(f'Infall Mvir valid values: {np.sum(infall_filtered > 0)} out of {len(infall_filtered)}')