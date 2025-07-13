#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
from scipy.interpolate import interp1d
from random import sample, seed
import pandas as pd
from functools import lru_cache
from concurrent.futures import ThreadPoolExecutor, as_completed
import time

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium_dekel_h2/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0  # Fraction of the full volume output by the model
CGMsnaps = [63, 50, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF

# Target halo masses (in M_sun, not log10)
TARGET_MASSES = [1e10, 1e12, 1e14]
TARGET_MASS_TOLERANCE = 0.2  # log10 tolerance for finding halos

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7500       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (8.34,6.25)
plt.rcParams["figure.dpi"] = 500
plt.rcParams["font.size"] = 14

FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

CGMsnaps = [63, 50, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF

# ==================================================================

class OptimizedDataReader:
    """Optimized data reader for large HDF5 files with caching and batch reading"""
    
    def __init__(self, dir_name):
        self.dir_name = dir_name
        self.model_files = [f for f in os.listdir(dir_name) 
                           if f.startswith('model_') and f.endswith('.hdf5')]
        self.model_files.sort()
        self._cache = {}
        self._file_handles = {}
        print(f'Found {len(self.model_files)} model files')
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close_all_files()
    
    def close_all_files(self):
        """Close all open file handles"""
        for handle in self._file_handles.values():
            handle.close()
        self._file_handles.clear()
    
    def _get_file_handle(self, model_file):
        """Get cached file handle or open new one"""
        if model_file not in self._file_handles:
            self._file_handles[model_file] = h5.File(self.dir_name + model_file, 'r')
        return self._file_handles[model_file]
    
    @lru_cache(maxsize=128)
    def read_single_param(self, snap_num, param, model_file):
        """Read single parameter from single file with caching"""
        cache_key = (snap_num, param, model_file)
        if cache_key in self._cache:
            return self._cache[cache_key]
        
        try:
            file_handle = self._get_file_handle(model_file)
            data = np.array(file_handle[snap_num][param])
            # Only cache if data is reasonably small
            if data.nbytes < 100 * 1024 * 1024:  # 100MB threshold
                self._cache[cache_key] = data
            return data
        except Exception as e:
            print(f"Error reading {param} from {model_file}: {e}")
            return np.array([])
    
    def read_hdf_batch(self, snap_num, params):
        """Read multiple parameters at once for efficiency"""
        print(f"Reading {len(params)} parameters for {snap_num}")
        start_time = time.time()
        
        results = {}
        
        # Use parallel reading for multiple files
        with ThreadPoolExecutor(max_workers=min(4, len(self.model_files))) as executor:
            # Submit tasks for each file
            future_to_file = {}
            for model_file in self.model_files:
                future = executor.submit(self._read_file_params, model_file, snap_num, params)
                future_to_file[future] = model_file
            
            # Collect results
            file_results = {}
            for future in as_completed(future_to_file):
                model_file = future_to_file[future]
                try:
                    file_results[model_file] = future.result()
                except Exception as e:
                    print(f"Error reading {model_file}: {e}")
                    file_results[model_file] = {param: np.array([]) for param in params}
        
        # Combine results from all files
        for param in params:
            combined_data = None
            for model_file in self.model_files:
                if param in file_results[model_file]:
                    data = file_results[model_file][param]
                    if combined_data is None:
                        combined_data = data
                    else:
                        combined_data = np.concatenate((combined_data, data))
            
            results[param] = combined_data if combined_data is not None else np.array([])
        
        elapsed = time.time() - start_time
        print(f"Batch read completed in {elapsed:.2f} seconds")
        return results
    
    def _read_file_params(self, model_file, snap_num, params):
        """Read multiple parameters from a single file"""
        file_handle = self._get_file_handle(model_file)
        results = {}
        
        try:
            snapshot_group = file_handle[snap_num]
            for param in params:
                if param in snapshot_group:
                    results[param] = np.array(snapshot_group[param])
                else:
                    print(f"Warning: Parameter {param} not found in {model_file}")
                    results[param] = np.array([])
        except Exception as e:
            print(f"Error reading from {model_file}: {e}")
            results = {param: np.array([]) for param in params}
        
        return results
    
    def read_hdf(self, snap_num=None, param=None):
        """Legacy interface for single parameter reading"""
        combined_data = None
        
        for model_file in self.model_files:
            data = self.read_single_param(snap_num, param, model_file)
            if combined_data is None:
                combined_data = data
            else:
                combined_data = np.concatenate((combined_data, data))
                
        return combined_data

def load_all_snapshots_optimized(data_reader, params):
    """
    Optimized loading of all snapshots for multiple parameters
    Uses memory-efficient chunked loading and parallel processing
    """
    print(f'Loading {len(params)} parameters for snapshots {FirstSnap} to {LastSnap}')
    start_time = time.time()
    
    # Initialize data structures
    data_full = {param: [None] * (LastSnap - FirstSnap + 1) for param in params}
    
    # Process snapshots in chunks to manage memory
    chunk_size = 8  # Process 8 snapshots at a time
    
    for chunk_start in range(FirstSnap, LastSnap + 1, chunk_size):
        chunk_end = min(chunk_start + chunk_size, LastSnap + 1)
        chunk_snaps = list(range(chunk_start, chunk_end))
        
        print(f'Processing snapshot chunk {chunk_start} to {chunk_end-1}')
        
        # Use threading for I/O bound operations
        with ThreadPoolExecutor(max_workers=4) as executor:
            future_to_snap = {}
            for snap in chunk_snaps:
                snapshot_name = f'Snap_{snap}'
                future = executor.submit(data_reader.read_hdf_batch, snapshot_name, params)
                future_to_snap[future] = snap
            
            # Collect results
            for future in as_completed(future_to_snap):
                snap = future_to_snap[future]
                try:
                    snap_data = future.result()
                    for param in params:
                        data_full[param][snap] = snap_data[param] * 1.0e10 / Hubble_h
                except Exception as e:
                    print(f"Error loading snapshot {snap}: {e}")
                    for param in params:
                        data_full[param][snap] = np.array([])
    
    elapsed = time.time() - start_time
    print(f'Loaded all snapshots in {elapsed:.2f} seconds')
    
    return data_full

# ==================================================================

def plot_cgm_mass_histogram(data_reader):
    """
    Create CGM mass histogram divided by halo mass bins
    """
    print('Creating CGM mass histogram by halo mass bins')
    
    # Read data using batch reader
    params = ['CGMgas', 'Mvir']
    data = data_reader.read_hdf_batch(Snapshot, params)
    
    cgm = data['CGMgas'] * 1.0e10 / Hubble_h
    mvir = data['Mvir'] * 1.0e10 / Hubble_h
    
    # Filter out invalid data
    valid_mask = (cgm > 0) & (mvir > 0)
    cgm_valid = cgm[valid_mask]
    mvir_valid = mvir[valid_mask]
    
    # Calculate volume (following your reference function format)
    BoxSize_corrected = 62.5  # Use the actual BoxSize from millennium
    box_fraction = 1.0
    volume = (BoxSize_corrected / Hubble_h)**3.0 * box_fraction
    
    # Histogram parameters for CGM mass
    mi = 7.5
    ma = 12.75
    binwidth = 0.25
    NB = int((ma - mi) / binwidth)
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (9.5, 12.0),    # Low mass halos
        (12.0, 13.5),    # Intermediate mass halos
        (13.5, 15.0)     # High mass halos
    ]
    
    subset_colors = ['red', 'green', 'blue']
    legend_labels = [
        r'$10^{9.5} < M_{\mathrm{halo}} < 10^{12}$',
        r'$10^{12} < M_{\mathrm{halo}} < 10^{13.5}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 8))
    
    custom_lines = []
    
    # Pre-compute log values once
    log_mvir = np.log10(mvir_valid)
    log_cgm = np.log10(cgm_valid)
    
    # Create subsets based on halo mass and plot histograms
    for i, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, subset_colors, legend_labels)):
        # Create mask for this halo mass bin
        if i == len(halo_mass_bins) - 1:  # Last bin: no upper limit
            mask = log_mvir >= mass_min
        else:
            mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
        
        # Get CGM masses for this halo mass bin
        cgm_subset = log_cgm[mask]
        
        if len(cgm_subset) > 0:
            # Create histogram
            (counts, binedges) = np.histogram(cgm_subset, range=(mi, ma), bins=NB)
            xaxeshisto = binedges[:-1] + 0.5 * binwidth
            
            # Plot line and fill
            line, = ax.plot(xaxeshisto, counts / volume / binwidth, 
                           color=color, label=label, alpha=0.8, linewidth=2)
            ax.fill_between(xaxeshisto, 0, counts / volume / binwidth, 
                           color=color, alpha=0.2)
            custom_lines.append(line)
    
    # Plot overall histogram (all CGM masses)
    (counts_all, binedges_all) = np.histogram(log_cgm, range=(mi, ma), bins=NB)
    xaxeshisto_all = binedges_all[:-1] + 0.5 * binwidth
    line_all, = ax.plot(xaxeshisto_all, counts_all / volume / binwidth, 
                       color='black', label='Overall', linewidth=3)
    
    # Set plot properties
    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
   
    # Set axis limits
    ax.set_xlim(mi, ma)
    ax.set_ylim(1e-6, 1e-1)
    
    # Create legend
    handles = custom_lines + [line_all]
    ax.legend(handles=handles, loc='upper right', frameon=False, fontsize='medium')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    outputFile = OutputDir + '23.CGM_MassFunction_HaloBins' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass histogram to {outputFile}')
    plt.close()

# ==================================================================

def plot_cgm_mass_histogram_grid(data_reader):
    """
    Create CGM mass histogram grid divided by halo mass bins across redshifts
    """
    print('Creating CGM mass histogram grid by halo mass bins across redshifts')
    
    # Load all required data at once
    params = ['CGMgas', 'Mvir', 'StellarMass']
    data_full = load_all_snapshots_optimized(data_reader, params)
    
    # Calculate volume
    volume = (BoxSize / Hubble_h)**3.0 * VolumeFraction
    
    # Histogram parameters for CGM mass
    mi = 7.5
    ma = 12.75
    binwidth = 0.25
    NB = int((ma - mi) / binwidth)
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (9.5, 12.0),    # Low mass halos
        (12.0, 13.5),    # Intermediate mass halos
        (13.5, 15.0)     # High mass halos
    ]
    
    subset_colors = ['red', 'green', 'blue']
    legend_labels = [
        r'$10^{9.5} < M_{\mathrm{halo}} < 10^{12}$',
        r'$10^{12} < M_{\mathrm{halo}} < 10^{13.5}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Determine grid size based on number of snapshots
    n_snaps = len(CGMsnaps)
    if n_snaps <= 4:
        rows, cols = 2, 2
    elif n_snaps <= 6:
        rows, cols = 2, 3
    elif n_snaps <= 9:
        rows, cols = 3, 3
    else:
        rows, cols = 4, 3
    
    # Create subplot grid
    fig, axes = plt.subplots(rows, cols, figsize=(8*cols, 6*rows))
    if n_snaps == 1:
        axes = [axes]  # Make it a list for consistency
    else:
        axes = axes.flatten()
    
    for plot_idx, snap in enumerate(CGMsnaps):
        ax = axes[plot_idx]
        z = redshifts[snap]
        
        # Get data for this snapshot
        cgm_snap = data_full['CGMgas'][snap]
        mvir_snap = data_full['Mvir'][snap]
        stellar_snap = data_full['StellarMass'][snap]
        
        # Filter out invalid data
        valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
        cgm_valid = cgm_snap[valid_mask]
        mvir_valid = mvir_snap[valid_mask]
        
        # Pre-compute log values
        log_mvir = np.log10(mvir_valid)
        log_cgm = np.log10(cgm_valid)
        
        custom_lines = []
        
        # Create subsets based on halo mass and plot histograms
        for i, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, subset_colors, legend_labels)):
            # Create mask for this halo mass bin
            if i == len(halo_mass_bins) - 1:  # Last bin: no upper limit
                mask = log_mvir >= mass_min
            else:
                mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            # Get CGM masses for this halo mass bin
            cgm_subset = log_cgm[mask]
            
            if len(cgm_subset) > 0:
                # Create histogram
                (counts, binedges) = np.histogram(cgm_subset, range=(mi, ma), bins=NB)
                xaxeshisto = binedges[:-1] + 0.5 * binwidth
                
                # Calculate y-values and take log10
                y_values = counts / volume / binwidth
                # Add small value to avoid log(0) and take log10
                y_values_log = np.log10(y_values + 1e-10)
                
                # Plot line and fill
                line, = ax.plot(xaxeshisto, y_values_log, 
                               color=color, label=label, alpha=0.8, linewidth=2)
                ax.fill_between(xaxeshisto, np.full_like(xaxeshisto, -10), y_values_log, 
                               color=color, alpha=0.2)
                custom_lines.append(line)
        
        # Plot overall histogram (all CGM masses)
        (counts_all, binedges_all) = np.histogram(log_cgm, range=(mi, ma), bins=NB)
        xaxeshisto_all = binedges_all[:-1] + 0.5 * binwidth
        y_values_all = counts_all / volume / binwidth
        y_values_all_log = np.log10(y_values_all + 1e-10)
        line_all, = ax.plot(xaxeshisto_all, y_values_all_log, 
                           color='black', label='Overall', linewidth=3)
        
        # Set plot properties (removed set_yscale('log'))
        ax.set_ylabel(r'$\log_{10}[\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})]$')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
        ax.set_title(f'z = {z:.1f}')
        
        # Set axis limits (now in log10 space)
        ax.set_xlim(mi, ma)
        ax.set_ylim(-6, 0)  # log10(1e-6) = -6, log10(1e-0) = 0
        
        # Add legend only to the first plot
        if plot_idx == 0:
            handles = custom_lines + [line_all]
            ax.legend(handles=handles, loc='upper right', frameon=False, fontsize='small')
        
        # Set minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        
        # Print some statistics for this redshift
        print(f'\nRedshift z = {z:.1f} (Snap {snap}):')
        print(f'Total number of galaxies: {len(cgm_valid)}')
        for i, ((mass_min, mass_max), label) in enumerate(zip(halo_mass_bins, legend_labels)):
            if i == len(halo_mass_bins) - 1:
                mask = log_mvir >= mass_min
            else:
                mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            print(f'  {label}: {np.sum(mask)} galaxies')
    
    # Hide unused subplots
    for plot_idx in range(len(CGMsnaps), rows * cols):
        if plot_idx < len(axes):
            axes[plot_idx].axis('off')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    outputFile = OutputDir + '24.CGM_MassFunction_HaloBins_Grid' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'\nSaved CGM mass histogram grid to {outputFile}')
    plt.close()

# ==================================================================

def plot_halo_mass_histogram_grid(data_reader):
    """
    Create Halo mass histogram grid divided by gas component types across redshifts
    Subsets: Hot Gas, CGM mass, and IntraCluster Stars
    """
    print('Creating Halo mass histogram grid by gas component types across redshifts')
    
    # Load all required data at once
    params = ['CGMgas', 'Mvir', 'StellarMass', 'HotGas']
    
    # Try to read IntraClusterStars - handle if it doesn't exist
    try:
        test_data = data_reader.read_hdf_batch(Snapshot, ['IntraClusterStars'])
        if len(test_data['IntraClusterStars']) > 0:
            params.append('IntraClusterStars')
        else:
            params.append('ICM')  # Try alternative name
    except:
        print("Warning: IntraClusterStars/ICM parameter not found, using zeros")
    
    data_full = load_all_snapshots_optimized(data_reader, params)
    
    # Handle missing IntraClusterStars
    if 'IntraClusterStars' not in data_full and 'ICM' not in data_full:
        for snap in range(FirstSnap, LastSnap+1):
            data_full['IntraClusterStars'] = [np.zeros_like(data_full['Mvir'][snap]) 
                                            for snap in range(FirstSnap, LastSnap+1)]
    elif 'ICM' in data_full:
        data_full['IntraClusterStars'] = data_full['ICM']
    
    # Calculate volume
    volume = (BoxSize / Hubble_h)**3.0 * VolumeFraction
    
    # Histogram parameters for Halo mass
    mi = 10.0
    ma = 15.0
    binwidth = 0.25
    NB = int((ma - mi) / binwidth)
    
    subset_colors = ['red', 'green', 'blue']
    legend_labels = [
        r'CGM Gas',
        r'IntraCluster Stars',
        r'Stellar Mass'
    ]
    
    # Determine grid size based on number of snapshots
    n_snaps = len(CGMsnaps)
    if n_snaps <= 4:
        rows, cols = 2, 2
    elif n_snaps <= 6:
        rows, cols = 2, 3
    elif n_snaps <= 9:
        rows, cols = 3, 3
    else:
        rows, cols = 4, 3
    
    # Create subplot grid
    fig, axes = plt.subplots(rows, cols, figsize=(8*cols, 6*rows))
    if n_snaps == 1:
        axes = [axes]  # Make it a list for consistency
    else:
        axes = axes.flatten()
    
    for plot_idx, snap in enumerate(CGMsnaps):
        ax = axes[plot_idx]
        z = redshifts[snap]
        
        # Get data for this snapshot
        halo_mass = data_full['Mvir'][snap]
        cgm_gas = data_full['CGMgas'][snap]
        ic_stars = data_full['IntraClusterStars'][snap]
        stellar_mass = data_full['StellarMass'][snap]
        
        # Filter out invalid data
        valid_mask = (halo_mass > 0) & (stellar_mass > 0)
        halo_mass_valid = halo_mass[valid_mask]
        cgm_gas_valid = cgm_gas[valid_mask]
        ic_stars_valid = ic_stars[valid_mask]
        stellar_mass_valid = stellar_mass[valid_mask]
        
        custom_lines = []
        
        # Create subsets based on component mass thresholds and plot histograms
        component_data = [cgm_gas_valid, ic_stars_valid, stellar_mass_valid]

        for i, (component_mass, color, label) in enumerate(zip(component_data, subset_colors, legend_labels)):
            # Create mask for galaxies where this component has significant mass
            # Use a threshold: component mass > 1e8 solar masses
            mass_threshold = 1e8
            mask = component_mass > mass_threshold
            
            # Get halo masses for galaxies with significant mass in this component
            halo_subset = halo_mass_valid[mask]
            
            if len(halo_subset) > 0:
                # Create histogram
                (counts, binedges) = np.histogram(np.log10(halo_subset), range=(mi, ma), bins=NB)
                xaxeshisto = binedges[:-1] + 0.5 * binwidth
                
                # Plot line and fill
                line, = ax.plot(xaxeshisto, counts / volume / binwidth, 
                               color=color, label=label, alpha=0.8, linewidth=2)
                ax.fill_between(xaxeshisto, 0, counts / volume / binwidth, 
                               color=color, alpha=0.2)
                custom_lines.append(line)
        
        # Plot overall histogram (all halo masses)
        (counts_all, binedges_all) = np.histogram(np.log10(halo_mass_valid), range=(mi, ma), bins=NB)
        xaxeshisto_all = binedges_all[:-1] + 0.5 * binwidth
        line_all, = ax.plot(xaxeshisto_all, counts_all / volume / binwidth, 
                           color='black', label='Overall', linewidth=3)
        
        # Set plot properties
        #ax.set_yscale('log')
        ax.set_ylabel(r'$\log_{10}\ \phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
        ax.set_title(f'Halo Mass Function by Components, z = {z:.1f}')
        
        # Set axis limits
        ax.set_xlim(mi, ma)
        ax.set_ylim(1e-6, 1e-0)
        
        # Add grid for better readability
        ax.grid(True, alpha=0.3)
        
        # Add legend only to the first plot
        if plot_idx == 0:
            handles = custom_lines + [line_all]
            ax.legend(handles=handles, loc='upper right', frameon=False, fontsize='small')
        
        # Set minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        
        # Print some statistics for this redshift
        print(f'\nRedshift z = {z:.1f} (Snap {snap}):')
        print(f'Total number of galaxies: {len(halo_mass_valid)}')
        for i, (component_mass, label) in enumerate(zip(component_data, legend_labels)):
            mass_threshold = 1e8
            mask = component_mass > mass_threshold
            print(f'  {label} (M > 1e8): {np.sum(mask)} galaxies')
    
    # Hide unused subplots
    for plot_idx in range(len(CGMsnaps), rows * cols):
        if plot_idx < len(axes):
            axes[plot_idx].axis('off')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    outputFile = OutputDir + '25.Halo_MassFunction_ComponentBins_Grid' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'\nSaved Halo mass histogram grid to {outputFile}')
    plt.close()

# ==================================================================

def plot_component_mass_function_evolution(data_reader):
    """
    Create evolution plot showing Hot Gas, CGM, and IntraCluster Stars mass functions
    """
    print('Creating component mass function evolution plot')
    
    # Load all required data at once
    params = ['CGMgas', 'Mvir', 'StellarMass', 'HotGas']
    data_full = load_all_snapshots_optimized(data_reader, params)
    
    # Calculate volume
    volume = (BoxSize / Hubble_h)**3.0 * VolumeFraction
    
    # Create three subplots: one for each component
    fig, axes = plt.subplots(1, 3, figsize=(24, 6))
    
    component_data = [data_full['HotGas'], data_full['CGMgas']]
    component_names = ['Hot Gas', 'CGM Gas']
    
    # Colors for different redshifts
    colors = ['black', 'blue', 'green', 'red', 'orange', 'purple']
    
    for comp_idx, (component_full, comp_name) in enumerate(zip(component_data, component_names)):
        ax = axes[comp_idx]
        
        # Plot mass function for each redshift
        for i, snap in enumerate(CGMsnaps[:6]):  # Limit to 6 redshifts
            z = redshifts[snap]
            color = colors[i % len(colors)]
            
            # Filter data
            stellar_snap = data_full['StellarMass'][snap]
            component_snap = component_full[snap]
            
            w = np.where((stellar_snap > 0.0) & (component_snap > 0.0))[0]
            mass = np.log10(component_snap[w])
            
            if len(mass) > 0:
                binwidth = 0.1
                mi = 7.0
                ma = 14.0
                NB = int((ma - mi) / binwidth)
                (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
                xaxeshisto = binedges[:-1] + 0.5 * binwidth
                
                label = f'z = {z:.1f}' if i == 0 else f'{z:.1f}'
                ax.plot(xaxeshisto, counts / volume / binwidth, color=color, 
                       linewidth=2, label=label)
        
        ax.set_yscale('log')
        ax.set_xlim(7.0, 12.2)
        ax.set_ylim(1.0e-6, 1.0e-0)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(f'$\log_{{10}} M_{{{comp_name.replace(" ", "")}}}$ $(M_{{\odot}})$')
        ax.set_title(f'{comp_name} Mass Function Evolution')
        ax.grid(True, alpha=0.3)
        
        if comp_idx == 0:
            leg = ax.legend(loc='lower left', numpoints=1, labelspacing=0.1)
            leg.draw_frame(False)
            for t in leg.get_texts():
                t.set_fontsize('small')
    
    # Third subplot for combined comparison at z=0
    ax = axes[2]
    snap = CGMsnaps[0]  # z=0
    z = redshifts[snap]
    
    for comp_idx, (component_full, comp_name, color) in enumerate(zip(
            [data_full['HotGas'], data_full['CGMgas']], 
            ['Hot Gas', 'CGM Gas'],
            ['red', 'blue'])):
        
        stellar_snap = data_full['StellarMass'][snap]
        component_snap = component_full[snap]
        
        w = np.where((stellar_snap > 0.0) & (component_snap > 0.0))[0]
        mass = np.log10(component_snap[w])
        
        if len(mass) > 0:
            binwidth = 0.1
            mi = 7.0
            ma = 14.0
            NB = int((ma - mi) / binwidth)
            (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
            xaxeshisto = binedges[:-1] + 0.5 * binwidth
            
            ax.plot(xaxeshisto, counts / volume / binwidth, color=color, 
                   linewidth=3, label=comp_name)
    
    ax.set_yscale('log')
    ax.set_xlim(7.0, 12.2)
    ax.set_ylim(1.0e-6, 1.0e-0)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{component}}\ (M_{\odot})$')
    ax.set_title(f'Component Comparison at z = {z:.1f}')
    ax.grid(True, alpha=0.3)
    
    leg = ax.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('small')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '26.Component_MassFunction_Evolution' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved component mass function evolution to {outputFile}')
    plt.close()

def plot_cgm_mass_evolution(data_reader):
    """
    Plot CGM mass evolution over redshift for different halo mass bins
    Shows how median CGM mass changes with time for 3 different halo mass ranges
    Uses specific redshifts: 0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5
    Bottom x-axis: Age of Universe (increasing left to right)
    Top x-axis: Redshift (decreasing left to right)
    """
    print('Creating CGM mass evolution plot for different halo mass bins')
    
    # Import cosmology for age calculations
    try:
        from astropy.cosmology import Planck18
        cosmo = Planck18
    except ImportError:
        print("Warning: astropy not available, using approximate age calculation")
        cosmo = None
    
    # Load all required data at once
    params = ['CGMgas', 'Mvir', 'StellarMass']
    data_full = load_all_snapshots_optimized(data_reader, params)
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (11.5, 12.5),    # Low mass halos
        (12.5, 13.5),    # Intermediate mass halos  
        (13.5, 16.5)     # High mass halos
    ]
    
    colors = ['red', 'green', 'blue']
    labels = [
        r'$10^{11.5} < M_{\mathrm{halo}} < 10^{12.5}$ M$_{\odot}$',
        r'$10^{12.5} < M_{\mathrm{halo}} < 10^{13.5}$ M$_{\odot}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Function to calculate age of Universe (approximate if astropy not available)
    def redshift_to_age(z):
        if cosmo is not None:
            return cosmo.age(z).value  # Returns age in Gyr
        else:
            # Approximate formula for Planck cosmology
            H0 = 67.4  # km/s/Mpc
            Omega_m = 0.315
            Omega_lambda = 0.685
            
            # Convert H0 to 1/Gyr
            H0_Gyr = H0 / 977.8  # Conversion factor
            
            # Approximate age calculation
            def integrand(z_prime):
                E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                return 1.0 / ((1 + z_prime) * E_z)
            
            # Numerical integration (simple trapezoidal rule)
            z_vals = np.linspace(z, 30, 1000)
            y_vals = [integrand(z_val) for z_val in z_vals]
            age = np.trapz(y_vals, z_vals) / H0_Gyr
            return age
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 4))
    
    # Define desired redshifts
    desired_redshifts = [0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5]
    
    # Find snapshots closest to desired redshifts
    selected_snapshots = []
    actual_redshifts = []
    
    for target_z in desired_redshifts:
        # Find the snapshot with redshift closest to target_z
        differences = [abs(z - target_z) for z in redshifts]
        closest_snap = differences.index(min(differences))
        selected_snapshots.append(closest_snap)
        actual_redshifts.append(redshifts[closest_snap])
        print(f'Target z={target_z:.2f} -> Snap {closest_snap}, actual z={redshifts[closest_snap]:.3f}')
    
    # For each halo mass bin, track CGM mass evolution
    for bin_idx, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, colors, labels)):
        redshift_values = []
        age_values = []
        median_cgm_masses = []
        std_cgm_masses = []
        
        # Loop through selected snapshots only
        for i, snap in enumerate(selected_snapshots):
            z = actual_redshifts[i]  # Use the actual redshift for this snapshot
            
            # Get data for this snapshot
            cgm_snap = data_full['CGMgas'][snap]
            mvir_snap = data_full['Mvir'][snap]
            stellar_snap = data_full['StellarMass'][snap]
            
            # Filter valid data
            valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
            cgm_valid = cgm_snap[valid_mask]
            mvir_valid = mvir_snap[valid_mask]
            
            # Filter by halo mass bin
            log_mvir = np.log10(mvir_valid)
            mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            if np.sum(mass_mask) > 1:  # Require at least 2 galaxies for statistics
                cgm_in_bin = cgm_valid[mass_mask]
                median_cgm = np.median(cgm_in_bin)
                std_cgm = np.std(np.log10(cgm_in_bin))
                
                # Calculate age of Universe at this redshift
                age = redshift_to_age(z)
                
                redshift_values.append(z)
                age_values.append(age)
                median_cgm_masses.append(median_cgm)
                std_cgm_masses.append(std_cgm)
                
                print(f'z={z:.3f}, age={age:.2f} Gyr, Halo bin {bin_idx+1}: {np.sum(mass_mask)} galaxies, median CGM = {median_cgm:.2e} M_sun')
        
        # Plot evolution for this halo mass bin using age on x-axis
        if len(age_values) > 0:
            age_values = np.array(age_values)
            median_cgm_masses = np.array(median_cgm_masses)
            
            # Sort by age to ensure proper line plotting
            sort_idx = np.argsort(age_values)
            age_values = age_values[sort_idx]
            median_cgm_masses = median_cgm_masses[sort_idx]
            
            ax.plot(age_values, np.log10(median_cgm_masses), 
                   color=color, label=label, linewidth=3)
            
            # Add shaded error region
            log_median_cgm = np.log10(median_cgm_masses)
            upper_bound = log_median_cgm + std_cgm_masses
            lower_bound = log_median_cgm - std_cgm_masses

            ax.fill_between(age_values, lower_bound, upper_bound, 
                        color=color, alpha=0.1, edgecolor='none')
    
    # Set up bottom x-axis (age)
    ax.set_xlabel('Age of Universe (Gyr)', fontsize=14)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$', fontsize=14)
    
    # Create top x-axis for redshift
    ax2 = ax.twiny()
    
    # Set up the relationship between age and redshift for the top axis
    z_ticks = np.array([0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5])
    age_ticks = np.array([redshift_to_age(z) for z in z_ticks])
    
    # Set the secondary axis limits and ticks
    ax2.set_xlim(ax.get_xlim())  # Match the primary axis limits
    ax2.set_xticks(age_ticks)
    ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
    ax2.set_xlabel('Redshift', fontsize=14)
    
    # Create legend
    leg = ax.legend(loc='lower left', frameon=False, fontsize=12)
    
    # Set reasonable axis limits
    ax.set_ylim(9.0, 11.5)
    ax.set_xlim(min(age_ticks), 14)  # Age of Universe from 0 to 13 Gyr
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '27.CGM_Mass_Evolution_HaloBins' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass evolution plot to {outputFile}')
    plt.close()

def plot_cgm_mass_evolution2(data_reader):
    """
    Plot CGM mass evolution over redshift for different halo mass bins
    Shows how median CGM mass changes with time for 3 different halo mass ranges
    Includes scatter plot of individual galaxies behind the median lines
    """
    print('Creating CGM mass evolution plot for different halo mass bins with scatter')
    
    # Import cosmology for age calculations
    try:
        from astropy.cosmology import Planck18
        cosmo = Planck18
    except ImportError:
        print("Warning: astropy not available, using approximate age calculation")
        cosmo = None
    
    # Load all required data at once
    params = ['CGMgas', 'Mvir', 'StellarMass']
    data_full = load_all_snapshots_optimized(data_reader, params)
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (11.5, 12.5),    # Low mass halos
        (12.5, 13.5),    # Intermediate mass halos  
        (13.5, 16.5)     # High mass halos
    ]
    
    colors = ['red', 'green', 'blue']
    labels = [
        r'$10^{11.5} < M_{\mathrm{halo}} < 10^{12.5}$ M$_{\odot}$',
        r'$10^{12.5} < M_{\mathrm{halo}} < 10^{13.5}$ M$_{\odot}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Function to calculate age of Universe
    def redshift_to_age(z):
        if cosmo is not None:
            return cosmo.age(z).value
        else:
            H0 = 67.4
            Omega_m = 0.315
            Omega_lambda = 0.685
            H0_Gyr = H0 / 977.8
            
            def integrand(z_prime):
                E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                return 1.0 / ((1 + z_prime) * E_z)
            
            z_vals = np.linspace(z, 30, 1000)
            y_vals = [integrand(z_val) for z_val in z_vals]
            age = np.trapz(y_vals, z_vals) / H0_Gyr
            return age
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 4))
    
    # Define desired redshifts for median line calculation
    desired_redshifts = [0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5]
    
    # Find snapshots closest to desired redshifts for median lines
    selected_snapshots = []
    actual_redshifts = []
    
    for target_z in desired_redshifts:
        differences = [abs(z - target_z) for z in redshifts]
        closest_snap = differences.index(min(differences))
        selected_snapshots.append(closest_snap)
        actual_redshifts.append(redshifts[closest_snap])
    
    # For scatter plot, use more snapshots for continuity
    scatter_snapshots = list(range(FirstSnap, LastSnap+1, 2))
    
    # Target number of points per mass bin
    target_points_per_bin = 750
    
    # Calculate typical time spacing for jitter
    age_spacings = []
    for i in range(len(scatter_snapshots)-1):
        age1 = redshift_to_age(redshifts[scatter_snapshots[i]])
        age2 = redshift_to_age(redshifts[scatter_snapshots[i+1]])
        age_spacings.append(abs(age2 - age1))
    typical_spacing = np.median(age_spacings) if age_spacings else 0.1
    jitter_range = typical_spacing * 0.9
    
    # Collect scatter data for each mass bin
    scatter_data = {bin_idx: {'ages': [], 'cgm_masses': []} for bin_idx in range(len(halo_mass_bins))}
    
    for bin_idx, (mass_min, mass_max) in enumerate(halo_mass_bins):
        all_ages = []
        all_cgm_masses = []
        
        for snap in scatter_snapshots:
            z = redshifts[snap]
            base_age = redshift_to_age(z)
            
            # Get data for this snapshot
            cgm_snap = data_full['CGMgas'][snap]
            mvir_snap = data_full['Mvir'][snap]
            stellar_snap = data_full['StellarMass'][snap]
            
            # Filter valid data
            valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
            cgm_valid = cgm_snap[valid_mask]
            mvir_valid = mvir_snap[valid_mask]
            
            # Filter by halo mass bin
            log_mvir = np.log10(mvir_valid)
            mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            if np.sum(mass_mask) > 0:
                cgm_in_bin = cgm_valid[mass_mask]
                
                # Sample a few points from each snapshot
                max_points_per_snap = max(1, target_points_per_bin // len(scatter_snapshots))
                if len(cgm_in_bin) > max_points_per_snap:
                    sample_indices = np.random.choice(len(cgm_in_bin), max_points_per_snap, replace=False)
                    cgm_sampled = cgm_in_bin[sample_indices]
                else:
                    cgm_sampled = cgm_in_bin
                
                # Add temporal jitter
                n_points = len(cgm_sampled)
                jittered_ages = base_age + np.random.uniform(-jitter_range/2, jitter_range/2, n_points)
                
                all_ages.extend(jittered_ages)
                all_cgm_masses.extend(cgm_sampled)
        
        scatter_data[bin_idx]['ages'] = np.array(all_ages)
        scatter_data[bin_idx]['cgm_masses'] = np.array(all_cgm_masses)
    
    # Plot scatter points first
    for bin_idx, (color, label) in enumerate(zip(colors, labels)):
        if len(scatter_data[bin_idx]['ages']) > 0:
            ax.scatter(scatter_data[bin_idx]['ages'], 
                      np.log10(scatter_data[bin_idx]['cgm_masses']),
                      c=color, alpha=0.1, s=5, rasterized=True)
    
    # Plot median lines
    for bin_idx, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, colors, labels)):
        age_values = []
        median_cgm_masses = []
        
        for i, snap in enumerate(selected_snapshots):
            z = actual_redshifts[i]
            
            # Get data for this snapshot
            cgm_snap = data_full['CGMgas'][snap]
            mvir_snap = data_full['Mvir'][snap]
            stellar_snap = data_full['StellarMass'][snap]
            
            # Filter valid data
            valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
            cgm_valid = cgm_snap[valid_mask]
            mvir_valid = mvir_snap[valid_mask]
            
            # Filter by halo mass bin
            log_mvir = np.log10(mvir_valid)
            mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            if np.sum(mass_mask) > 1:
                cgm_in_bin = cgm_valid[mass_mask]
                median_cgm = np.median(cgm_in_bin)
                age = redshift_to_age(z)
                
                age_values.append(age)
                median_cgm_masses.append(median_cgm)
        
        if len(age_values) > 0:
            age_values = np.array(age_values)
            median_cgm_masses = np.array(median_cgm_masses)
            
            sort_idx = np.argsort(age_values)
            age_values = age_values[sort_idx]
            median_cgm_masses = median_cgm_masses[sort_idx]
            
            ax.plot(age_values, np.log10(median_cgm_masses), 
                   color=color, label=label, linewidth=3, zorder=10)
    
    # Set up axes
    ax.set_xlabel('Age of Universe (Gyr)', fontsize=14)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$', fontsize=14)
    
    # Create top x-axis for redshift
    ax2 = ax.twiny()
    z_ticks = np.array([0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5])
    age_ticks = np.array([redshift_to_age(z) for z in z_ticks])
    
    ax2.set_xlim(ax.get_xlim())
    ax2.set_xticks(age_ticks)
    ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
    ax2.set_xlabel('Redshift', fontsize=14)
    
    # Create legend
    leg = ax.legend(loc='lower left', frameon=False, fontsize=12)
    
    # Set axis limits
    ax.set_ylim(9.0, 11.5)
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '27.CGM_Mass_Evolution_HaloBins_Scatter' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass evolution plot with scatter to {outputFile}')
    plt.close()

def plot_cgm_mass_evolution_central_satellite(data_reader):
    """
    Plot CGM mass evolution over redshift for different halo mass bins separated by centrals and satellites
    """
    print('Creating CGM mass evolution plot for centrals vs satellites in different halo mass bins')
    
    # Import cosmology for age calculations
    try:
        from astropy.cosmology import Planck18
        cosmo = Planck18
    except ImportError:
        print("Warning: astropy not available, using approximate age calculation")
        cosmo = None
    
    # Load all required data at once
    params = ['CGMgas', 'Mvir', 'StellarMass', 'Type']
    data_full = load_all_snapshots_optimized(data_reader, params)
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (11.5, 12.5),    # Low mass halos
        (12.5, 13.5),    # Intermediate mass halos  
        (13.5, 14.5)     # High mass halos
    ]
    
    # Define galaxy types
    galaxy_types = [0, 1]  # Central, Satellite
    type_names = ['Central', 'Satellite']
    
    # Define colors and line styles for each combination
    colors = ['red', 'green', 'blue']  # One for each mass bin
    linestyles = ['-', '--']  # Solid for centrals, dashed for satellites
    
    # Create labels for each combination
    mass_bin_labels = [
        r'$10^{11.5} < M_{\mathrm{halo}} < 10^{12.5}$ M$_{\odot}$',
        r'$10^{12.5} < M_{\mathrm{halo}} < 10^{13.5}$ M$_{\odot}$',
        r'$10^{13.5} < M_{\mathrm{halo}} < 10^{14.5}$ M$_{\odot}$'
    ]
    
    # Function to calculate age of Universe
    def redshift_to_age(z):
        if cosmo is not None:
            return cosmo.age(z).value
        else:
            H0 = 67.4
            Omega_m = 0.315
            Omega_lambda = 0.685
            H0_Gyr = H0 / 977.8
            
            def integrand(z_prime):
                E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                return 1.0 / ((1 + z_prime) * E_z)
            
            z_vals = np.linspace(z, 30, 1000)
            y_vals = [integrand(z_val) for z_val in z_vals]
            age = np.trapz(y_vals, z_vals) / H0_Gyr
            return age
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Define desired redshifts
    desired_redshifts = [0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5]
    
    # Find snapshots closest to desired redshifts
    selected_snapshots = []
    actual_redshifts = []
    
    for target_z in desired_redshifts:
        differences = [abs(z - target_z) for z in redshifts]
        closest_snap = differences.index(min(differences))
        selected_snapshots.append(closest_snap)
        actual_redshifts.append(redshifts[closest_snap])
    
    # For each halo mass bin and galaxy type combination, track CGM mass evolution
    for mass_bin_idx, (mass_min, mass_max) in enumerate(halo_mass_bins):
        for type_idx, galaxy_type in enumerate(galaxy_types):
            age_values = []
            median_cgm_masses = []
            
            # Create label for this combination
            type_name = type_names[type_idx]
            mass_label = mass_bin_labels[mass_bin_idx]
            label = f'{type_name} - {mass_label}'
            
            color = colors[mass_bin_idx]
            linestyle = linestyles[type_idx]
            
            # Loop through selected snapshots only
            for i, snap in enumerate(selected_snapshots):
                z = actual_redshifts[i]
                
                # Get data for this snapshot
                cgm_snap = data_full['CGMgas'][snap]
                mvir_snap = data_full['Mvir'][snap]
                stellar_snap = data_full['StellarMass'][snap]
                type_snap = data_full['Type'][snap]
                
                # Filter valid data
                valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
                cgm_valid = cgm_snap[valid_mask]
                mvir_valid = mvir_snap[valid_mask]
                type_valid = type_snap[valid_mask]
                
                # Filter by halo mass bin
                log_mvir = np.log10(mvir_valid)
                mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
                
                # Apply mass mask to all arrays
                cgm_mass_filtered = cgm_valid[mass_mask]
                type_mass_filtered = type_valid[mass_mask]
                
                # Filter by galaxy type within this mass bin
                type_mask = (type_mass_filtered == galaxy_type)
                
                if np.sum(type_mask) > 5:  # Require at least 5 galaxies for statistics
                    cgm_final = cgm_mass_filtered[type_mask]
                    median_cgm = np.median(cgm_final)
                    
                    # Calculate age of Universe at this redshift
                    age = redshift_to_age(z)
                    
                    age_values.append(age)
                    median_cgm_masses.append(median_cgm)
            
            # Plot evolution for this combination using age on x-axis
            if len(age_values) > 0:
                age_values = np.array(age_values)
                median_cgm_masses = np.array(median_cgm_masses)
                
                # Sort by age to ensure proper line plotting
                sort_idx = np.argsort(age_values)
                age_values = age_values[sort_idx]
                median_cgm_masses = median_cgm_masses[sort_idx]
                
                # Plot the main line
                ax.plot(age_values, np.log10(median_cgm_masses), 
                       color=color, label=label, linewidth=3, linestyle=linestyle)
    
    # Set up bottom x-axis (age)
    ax.set_xlabel('Age of Universe (Gyr)', fontsize=14)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$', fontsize=14)
    
    # Create top x-axis for redshift
    ax2 = ax.twiny()
    z_ticks = np.array([0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5])
    age_ticks = np.array([redshift_to_age(z) for z in z_ticks])
    
    ax2.set_xlim(ax.get_xlim())
    ax2.set_xticks(age_ticks)
    ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
    ax2.set_xlabel('Redshift', fontsize=14)
    
    # Create legend
    leg = ax.legend(loc='lower left', frameon=False, fontsize=10)
    
    # Set reasonable axis limits
    ax.set_ylim(9.0, 11.0)
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '27.CGM_Mass_Evolution_CentralSatellite' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass evolution plot (central vs satellite) to {outputFile}')
    plt.close()

# ==================================================================

def bin_average(x, y, num_bins=10):
    """
    Calculate the average of array x for y value bins.
    
    Parameters:
    x (array-like): Values to be averaged
    y (array-like): Values to determine bins
    num_bins (int): Number of bins to create
    
    Returns:
    bin_centers (array): Centers of each bin
    bin_averages (array): Average x value for each bin
    bin_counts (array): Number of values in each bin
    """
    # Create bins based on y values
    y_min, y_max = np.min(y), np.max(y)
    bins = np.linspace(y_min, y_max, num_bins + 1)
    
    # Initialize arrays to store results
    bin_averages = np.zeros(num_bins)
    bin_counts = np.zeros(num_bins)
    bin_centers = (bins[:-1] + bins[1:]) / 2
    
    # Calculate averages for each bin
    for i in range(num_bins):
        # Find indices of y values that fall into this bin
        mask = (y >= bins[i]) & (y < bins[i+1])
        
        # If last bin, include the upper boundary
        if i == num_bins - 1:
            mask = (y >= bins[i]) & (y <= bins[i+1])
            
        # Get x values that correspond to y values in this bin
        x_in_bin = x[mask]
        
        # Calculate average if there are values in this bin
        if len(x_in_bin) > 0:
            bin_averages[i] = np.mean(x_in_bin)
            bin_counts[i] = len(x_in_bin)
        else:
            bin_averages[i] = np.nan  # Set NaN for empty bins
    
    return bin_centers, bin_averages, bin_counts

# ==================================================================

if __name__ == '__main__':

    print('Running optimized analysis\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)

    # Use the optimized data reader
    with OptimizedDataReader(DirName) as data_reader:
        
        print('Reading galaxy properties from', DirName)
        
        # Read data using batch reader for the main analysis
        main_params = ['CentralMvir', 'Mvir', 'StellarMass', 'BulgeMass', 'HotGas', 
                      'Vvir', 'Vmax', 'Rvir', 'SfrDisk', 'SfrBulge', 'CGMgas',
                      'ColdGas', 'MetalsColdGas', 'MetalsCGMgas', 'Type']
        
        print('Loading main parameters for current snapshot...')
        start_time = time.time()
        main_data = data_reader.read_hdf_batch(Snapshot, main_params)
        
        # Apply unit conversions
        CentralMvir = main_data['CentralMvir'] * 1.0e10 / Hubble_h
        Mvir = main_data['Mvir'] * 1.0e10 / Hubble_h
        StellarMass = main_data['StellarMass'] * 1.0e10 / Hubble_h
        BulgeMass = main_data['BulgeMass'] * 1.0e10 / Hubble_h
        HotGas = main_data['HotGas'] * 1.0e10 / Hubble_h
        Vvir = main_data['Vvir']
        Vmax = main_data['Vmax']
        Rvir = main_data['Rvir']
        SfrDisk = main_data['SfrDisk']
        SfrBulge = main_data['SfrBulge']
        cgm = main_data['CGMgas'] * 1.0e10 / Hubble_h
        ColdGas = main_data['ColdGas'] * 1.0e10 / Hubble_h
        MetalsColdGas = main_data['MetalsColdGas'] * 1.0e10 / Hubble_h
        MetalsCGMgas = main_data['MetalsCGMgas'] * 1.0e10 / Hubble_h
        Type = main_data['Type']
        
        elapsed = time.time() - start_time
        print(f'Main data loaded in {elapsed:.2f} seconds')
        
        # Run optimized plotting functions
        print('\nGenerating plots...')
        
        #plot_cgm_mass_histogram(data_reader)
        plot_cgm_mass_histogram_grid(data_reader)
        #plot_halo_mass_histogram_grid(data_reader)
        #plot_component_mass_function_evolution(data_reader)
        #plot_cgm_mass_evolution(data_reader)
        #plot_cgm_mass_evolution2(data_reader)
        #plot_cgm_mass_evolution_central_satellite(data_reader)

        # -------------------------------------------------------

        print('Plotting CGM outflow diagnostics')

        plt.figure(figsize=(10, 8))  # New figure

        # First subplot: Vmax vs Stellar Mass
        ax1 = plt.subplot(111)  # Top plot

        w = np.where((Vmax > 0))[0]
        if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

        mass = np.log10(Mvir[w])
        cgm_subset = np.log10(cgm[w])

        # Color points by halo mass
        halo_mass = np.log10(StellarMass[w])
        sc = plt.scatter(mass, cgm_subset, c=halo_mass, cmap='viridis', s=5, alpha=0.7)
        cbar = plt.colorbar(sc)
        cbar.set_label(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')

        plt.ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
        plt.xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        plt.axis([9.5, 14.0, 7.0, 10.5])
        plt.title('CGM vs Mvir')

        plt.tight_layout()

        outputFile = OutputDir + '19.CGMMass' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print('Saved file to', outputFile, '\n')
        plt.close()

        # -------------------------------------------------------

        print('Plotting CGM Mass Functions')

        plt.figure()  # New figure
        ax = plt.subplot(111)  # 1 plot on the figure

        binwidth = 0.1  # mass function histogram bin width

        # calculate all
        filter = np.where((Mvir > 0.0))[0]
        if(len(filter) > dilute): filter = sample(list(range(len(filter))), dilute)

        mass = np.log10(cgm[filter])

        mi = 7
        ma = 10
        NB = 25
        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

        plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', lw=2, label='Model - CGM')

        plt.yscale('log')
        plt.axis([7.0, 10.2, 1.0e-6, 1.0e-1])
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

        plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
        plt.xlabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')  # and the x-axis labels

        leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
            t.set_fontsize('medium')

        outputFile = OutputDir + '20.CGMMassFunction' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print('Saved to', outputFile, '\n')
        plt.close()

        # Calculate sSFR and CGM fraction analysis

        # Filter out invalid values using metallicity criteria similar to your example
        w = np.where((Type == 0) & (ColdGas / (StellarMass + ColdGas) > 0.1) & (StellarMass > 1.0e8))[0]
        if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

        mvir = Mvir[w]
        HotGas_filtered = HotGas[w]
        cgm_filtered = cgm[w]
        StellarMass_w = StellarMass[w]
        ColdGas_w = ColdGas[w]
        MetalsCGMgas_w = MetalsCGMgas[w]
        
        # Calculate CGM metallicity (12 + log(O/H)) using direct CGM metallicity
        # MetalsCGMgas is the total metal mass in CGM gas
        # Solar metallicity is 0.02
        Z = np.log10((MetalsCGMgas_w / cgm_filtered) / 0.02) + 9.0
        
        # sSFR = (np.log10((SfrDisk + SfrBulge) / StellarMass_w))

        # Calculate CGM mass fraction
        f_CGM = (cgm_filtered / mvir)
        f_CGM_normalized = f_CGM / 0.17  # Normalized by cosmic baryon fraction
        combined_gas = (cgm_filtered + HotGas_filtered) / mvir
        f_combined_gas = combined_gas / 0.17
        
        log_mvir = np.log10(mvir)
        # sSFR_valid = sSFR
        f_combined_gas_valid = f_combined_gas

        # Create a DataFrame with the provided variables
        df = pd.DataFrame({
            'log_mvir': log_mvir,
            # 'sSFR': sSFR_valid,
            'f_combined_gas': f_combined_gas_valid,
            'metallicity': Z
        })

        # Sort by log_mvir
        df = df.sort_values('log_mvir')

        # Calculate running median (adjust window size as needed)
        window_size = 101  # This should be an odd number
        # df['sSFR_median'] = df['sSFR'].rolling(window=window_size, center=True).median()

        # # Handle NaN values at the edges due to rolling window
        # if df['sSFR_median'].notna().any():
        #     first_valid = df['sSFR_median'].first_valid_index()
        #     last_valid = df['sSFR_median'].last_valid_index()
        #     if first_valid is not None and last_valid is not None:
        #         df.loc[:first_valid, 'sSFR_median'] = df.loc[first_valid, 'sSFR_median']
        #         df.loc[last_valid:, 'sSFR_median'] = df.loc[last_valid, 'sSFR_median']

        # # Alternative: use interpolation to handle NaN values
        # mask = df['sSFR_median'].notna()
        # if mask.sum() > 1:  # Make sure we have at least 2 points for interpolation
        #     f_interp = interp1d(
        #         df.loc[mask, 'log_mvir'], 
        #         df.loc[mask, 'sSFR_median'],
        #         bounds_error=False,
        #         fill_value=(df.loc[mask, 'sSFR_median'].iloc[0], df.loc[mask, 'sSFR_median'].iloc[-1])
        #     )
        #     df['sSFR_median'] = f_interp(df['log_mvir'])

        # # Calculate residuals
        # df['sSFR_residual'] = df['sSFR'] - df['sSFR_median']

        # CALCULATE RUNNING MEDIAN OF f_combined_gas VS LOG_MVIR
        df['f_combined_gas_median'] = df['f_combined_gas'].rolling(window=window_size, center=True).median()

        # Handle NaN values for f_combined_gas_median
        mask_gas = df['f_combined_gas_median'].notna()
        if mask_gas.sum() > 1:
            f_gas_interp = interp1d(
                df.loc[mask_gas, 'log_mvir'], 
                df.loc[mask_gas, 'f_combined_gas_median'],
                bounds_error=False,
                fill_value=(df.loc[mask_gas, 'f_combined_gas_median'].iloc[0], 
                        df.loc[mask_gas, 'f_combined_gas_median'].iloc[-1])
            )
            df['f_combined_gas_median'] = f_gas_interp(df['log_mvir'])

        # Create the plot
        plt.figure(figsize=(8, 6))

        # Color scatter points by metallicity
        sc = plt.scatter(np.log10(mvir), f_combined_gas, c=Z, cmap='viridis', s=5, vmin=7.5, vmax=9.5)

        cbar = plt.colorbar(sc)
        cbar.set_label(r'$12 + \log(\mathrm{O/H})$ (Metallicity)')

        plt.plot(df['log_mvir'], df['f_combined_gas_median'], 'k-', linewidth=2)

        plt.axis([9.75, 14.5, 0, 1.0])

        # Customize plot
        plt.xlabel(r'$\log_{10}(M_{vir}) [M_{\odot}]$')
        plt.ylabel(r'$f_{\mathrm{CGM}}/{\Omega_b}$')

        leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
            t.set_fontsize('medium')

        outputFile = OutputDir + '21.CGMMassFraction' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print('Saved to', outputFile, '\n')
        plt.close()

        # -------------------------------------------------------

        # Sample data for final plot
        cgm_fraction = np.log10(cgm_filtered / (0.17 * mvir))  # Normalized by cosmic baryon fraction
        hotgas_fraction = np.log10(HotGas_filtered / (0.17 * mvir))  # Normalized by cosmic baryon fraction

        valid_indices = np.isfinite(cgm_fraction) & np.isfinite(np.log10(mvir)) & np.isfinite(np.log10(HotGas_filtered))
        cgm_fraction_filtered = cgm_fraction[valid_indices]
        mvir_filtered_final = mvir[valid_indices]
        hotgas_fraction_filtered = hotgas_fraction[valid_indices]

        combined_gas_final = cgm_filtered + HotGas_filtered
        combined_gas_fraction = np.log10(combined_gas_final / (0.17 * mvir))

        # Filter the combined fraction with the same criteria
        combined_gas_fraction_filtered = combined_gas_fraction[valid_indices]

        # Calculate bin averages for the combined fraction
        bin_centers_combined, bin_averages_combined, bin_counts_combined = bin_average(
            combined_gas_fraction_filtered, np.log10(mvir_filtered_final), num_bins=20)

        # Calculate bin averages
        bin_centers, bin_averages, bin_counts = bin_average(cgm_fraction_filtered, np.log10(mvir_filtered_final), num_bins=20)
        bin_centers_hgas, bin_averages_hgas, bin_counts_hgas = bin_average(hotgas_fraction_filtered, np.log10(mvir_filtered_final), num_bins=20)
        
        # Plot results
        plt.figure(figsize=(10, 6))
        plt.scatter(np.log10(mvir_filtered_final), cgm_fraction_filtered, alpha=0.1, c='b', s=0.5)
        plt.scatter(np.log10(mvir_filtered_final), hotgas_fraction_filtered, alpha=0.1, c='r', s=0.5)

        plt.plot(bin_centers, bin_averages, '--', linewidth=1, c='b', label='CGM')
        plt.plot(bin_centers_hgas, bin_averages_hgas, '--', linewidth=1, c='r', label='Hot Gas')
        plt.plot(bin_centers_combined, bin_averages_combined, '-', linewidth=2, c='k', label='Combined Gas')

        # Customize plot
        plt.axis([10, 14.5, -2.5, 0.0])
        plt.xlabel(r'$\log_{10}(M_{vir}) [M_{\odot}]$')
        plt.ylabel(r'$\log_{10}M_{CGM}\ /\ f_b\ M_h$')

        leg = plt.legend(loc='lower right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
            t.set_fontsize('medium')

        outputFile = OutputDir + '22.CGMMassFraction' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print('Saved to', outputFile, '\n')
        plt.close()

    print('\nOptimized analysis completed!')

    def plot_cgm_composition_analysis(data_reader):
        """
        Plot CGM composition: pristine vs enriched gas fractions
        """
        print('Creating CGM composition analysis plots')
        
        # Load required data
        params = ['CGMgas', 'CGMgas_pristine', 'CGMgas_enriched', 'Mvir', 'StellarMass']
        data = data_reader.read_hdf_batch(Snapshot, params)
        
        # Apply unit conversions
        cgm_total = data['CGMgas'] * 1.0e10 / Hubble_h
        cgm_pristine = data['CGMgas_pristine'] * 1.0e10 / Hubble_h
        cgm_enriched = data['CGMgas_enriched'] * 1.0e10 / Hubble_h
        mvir = data['Mvir'] * 1.0e10 / Hubble_h
        stellar_mass = data['StellarMass'] * 1.0e10 / Hubble_h
        
        print(f'Initial data loaded - CGM total range: {cgm_total.min():.2e} to {cgm_total.max():.2e}')
        print(f'CGM pristine range: {cgm_pristine.min():.2e} to {cgm_pristine.max():.2e}')
        print(f'CGM enriched range: {cgm_enriched.min():.2e} to {cgm_enriched.max():.2e}')
        
        # Enhanced filtering for valid data - use more stringent thresholds
        min_cgm_mass = 1e7  # Minimum CGM mass threshold (solar masses)
        valid_mask = (
            (cgm_total >= min_cgm_mass) & 
            (mvir > 0) & 
            (stellar_mass > 0) & 
            np.isfinite(cgm_total) & 
            np.isfinite(cgm_pristine) & 
            np.isfinite(cgm_enriched) &
            np.isfinite(mvir) &
            np.isfinite(stellar_mass)
        )
        
        print(f'Valid galaxies after filtering: {np.sum(valid_mask)} out of {len(cgm_total)}')
        
        cgm_total_valid = cgm_total[valid_mask]
        cgm_pristine_valid = cgm_pristine[valid_mask]
        cgm_enriched_valid = cgm_enriched[valid_mask]
        mvir_valid = mvir[valid_mask]
        stellar_mass_valid = stellar_mass[valid_mask]
        
        # Calculate fractions with additional safety checks
        pristine_fraction = np.zeros_like(cgm_total_valid)
        enriched_fraction = np.zeros_like(cgm_total_valid)
        
        # Only calculate fractions where cgm_total is above threshold
        safe_division_mask = cgm_total_valid > min_cgm_mass
        
        pristine_fraction[safe_division_mask] = cgm_pristine_valid[safe_division_mask] / cgm_total_valid[safe_division_mask]
        enriched_fraction[safe_division_mask] = cgm_enriched_valid[safe_division_mask] / cgm_total_valid[safe_division_mask]
        
        # Additional check for finite values and reasonable ranges
        finite_mask = (
            np.isfinite(pristine_fraction) & 
            np.isfinite(enriched_fraction) &
            (pristine_fraction >= 0) & 
            (pristine_fraction <= 1) &
            (enriched_fraction >= 0) & 
            (enriched_fraction <= 1)
        )
        
        print(f'Galaxies with valid fractions: {np.sum(finite_mask)} out of {len(pristine_fraction)}')
        
        # Apply the finite mask to all arrays
        pristine_fraction = pristine_fraction[finite_mask]
        enriched_fraction = enriched_fraction[finite_mask]
        cgm_total_final = cgm_total_valid[finite_mask]
        cgm_pristine_final = cgm_pristine_valid[finite_mask]
        cgm_enriched_final = cgm_enriched_valid[finite_mask]
        mvir_final = mvir_valid[finite_mask]
        stellar_mass_final = stellar_mass_valid[finite_mask]
        
        if len(pristine_fraction) == 0:
            print("Warning: No valid data found for CGM composition analysis!")
            return
        
        print(f'Pristine fraction range: {pristine_fraction.min():.3f} to {pristine_fraction.max():.3f}')
        print(f'Enriched fraction range: {enriched_fraction.min():.3f} to {enriched_fraction.max():.3f}')
        
        # Create subplot figure
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        
        # Plot 1: Pristine fraction vs Halo Mass
        ax = axes[0, 0]
        if len(mvir_final) > dilute:
            indices = sample(list(range(len(mvir_final))), dilute)
        else:
            indices = list(range(len(mvir_final)))
        
        scatter = ax.scatter(np.log10(mvir_final[indices]), pristine_fraction[indices], 
                            c=np.log10(stellar_mass_final[indices]), cmap='viridis', s=10, alpha=0.7)
        
        # Add running median
        bin_centers, bin_averages, _ = bin_average(pristine_fraction, np.log10(mvir_final), num_bins=15)
        valid_bins = np.isfinite(bin_averages)
        if np.sum(valid_bins) > 0:
            ax.plot(bin_centers[valid_bins], bin_averages[valid_bins], 'r-', linewidth=3, label='Running Median')
        
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        ax.set_ylabel(r'$f_{\mathrm{pristine}} = M_{\mathrm{CGM,pristine}} / M_{\mathrm{CGM,total}}$')
        ax.set_title('CGM Pristine Fraction vs Halo Mass')
        ax.set_ylim(0, 1)
        ax.legend()
        
        cbar = plt.colorbar(scatter, ax=ax)
        cbar.set_label(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')
        
        # Plot 2: CGM Mass vs Halo Mass (colored by pristine fraction)
        ax = axes[0, 1]
        scatter = ax.scatter(np.log10(mvir_final[indices]), np.log10(cgm_total_final[indices]), 
                            c=pristine_fraction[indices], cmap='RdYlBu', s=10, alpha=0.7, vmin=0, vmax=1)
        
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
        ax.set_title('CGM Mass vs Halo Mass (colored by pristine fraction)')
        
        cbar = plt.colorbar(scatter, ax=ax)
        cbar.set_label(r'$f_{\mathrm{pristine}}$')
        
        # Plot 3: Pristine vs Enriched Mass
        ax = axes[1, 0]
        
        # Filter for galaxies with both pristine and enriched components > 0
        both_components_mask = (cgm_pristine_final > 0) & (cgm_enriched_final > 0)
        
        if np.sum(both_components_mask) > 0:
            pristine_subset = cgm_pristine_final[both_components_mask]
            enriched_subset = cgm_enriched_final[both_components_mask]
            mvir_subset = mvir_final[both_components_mask]
            
            if len(pristine_subset) > dilute:
                plot_indices = sample(list(range(len(pristine_subset))), dilute)
            else:
                plot_indices = list(range(len(pristine_subset)))
            
            scatter = ax.scatter(np.log10(pristine_subset[plot_indices]), np.log10(enriched_subset[plot_indices]), 
                        c=np.log10(mvir_subset[plot_indices]), cmap='plasma', s=10, alpha=0.7)
            
            # Add 1:1 line
            min_mass = min(np.log10(pristine_subset).min(), np.log10(enriched_subset).min())
            max_mass = max(np.log10(pristine_subset).max(), np.log10(enriched_subset).max())
            ax.plot([min_mass, max_mass], [min_mass, max_mass], 'k--', alpha=0.5, label='1:1 line')
            
            cbar = plt.colorbar(scatter, ax=ax)
            cbar.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        else:
            ax.text(0.5, 0.5, 'No galaxies with both\ncomponents > 0', ha='center', va='center', transform=ax.transAxes)
        
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{CGM,pristine}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM,enriched}}\ (M_{\odot})$')
        ax.set_title('Pristine vs Enriched CGM Mass')
        ax.legend()
        
        # Plot 4: Pristine fraction histogram
        ax = axes[1, 1]
        
        # Create histogram with finite values only
        if len(pristine_fraction) > 0:
            # Remove any remaining edge cases
            histogram_data = pristine_fraction[np.isfinite(pristine_fraction) & (pristine_fraction >= 0) & (pristine_fraction <= 1)]
            
            if len(histogram_data) > 0:
                ax.hist(histogram_data, bins=50, alpha=0.7, color='blue', edgecolor='black')
                median_val = np.median(histogram_data)
                ax.axvline(median_val, color='red', linestyle='--', linewidth=2, 
                        label=f'Median = {median_val:.2f}')
                
                # Add statistics text
                ax.text(0.05, 0.95, f'N = {len(histogram_data)}', transform=ax.transAxes, 
                    bbox=dict(boxstyle="round", facecolor='white', alpha=0.8))
            else:
                ax.text(0.5, 0.5, 'No valid data\nfor histogram', ha='center', va='center', transform=ax.transAxes)
        else:
            ax.text(0.5, 0.5, 'No valid data\nfor histogram', ha='center', va='center', transform=ax.transAxes)
        
        ax.set_xlabel(r'$f_{\mathrm{pristine}}$')
        ax.set_ylabel('Number of Galaxies')
        ax.set_title('Distribution of CGM Pristine Fractions')
        ax.legend()
        
        plt.tight_layout()
        
        # Save the plot
        OutputDir = DirName + 'plots/'
        outputFile = OutputDir + '28.CGM_Composition_Analysis' + OutputFormat
        plt.savefig(outputFile, dpi=500, bbox_inches='tight')
        print(f'Saved CGM composition analysis to {outputFile}')
        plt.close()
        
        # Print summary statistics
        print(f'\n=== CGM Composition Summary ===')
        print(f'Total galaxies analyzed: {len(pristine_fraction)}')
        if len(pristine_fraction) > 0:
            print(f'Median pristine fraction: {np.median(pristine_fraction):.3f}')
            print(f'Mean pristine fraction: {np.mean(pristine_fraction):.3f}')
            print(f'Std pristine fraction: {np.std(pristine_fraction):.3f}')
            
            # Count galaxies by dominant component
            pristine_dominated = np.sum(pristine_fraction > 0.5)
            enriched_dominated = np.sum(pristine_fraction < 0.5)
            balanced = np.sum(np.abs(pristine_fraction - 0.5) < 0.1)
            
            print(f'Pristine-dominated galaxies (f_pris > 0.5): {pristine_dominated} ({pristine_dominated/len(pristine_fraction)*100:.1f}%)')
            print(f'Enriched-dominated galaxies (f_pris < 0.5): {enriched_dominated} ({enriched_dominated/len(pristine_fraction)*100:.1f}%)')
            print(f'Balanced galaxies (0.4 < f_pris < 0.6): {balanced} ({balanced/len(pristine_fraction)*100:.1f}%)')
        print('================================\n')



    def plot_gas_flow_rates_analysis(data_reader):
        """
        Plot gas flow rates: infall, transfer, and reincorporation rates
        """
        print('Creating gas flow rates analysis plots')
        
        # Load required data
        params = ['CGMgas', 'HotGas', 'Mvir', 'StellarMass', 'Vvir',
                'InfallRate_to_CGM', 'InfallRate_to_Hot', 
                'TransferRate_CGM_to_Hot']
        
        data = data_reader.read_hdf_batch(Snapshot, params)
        
        # Apply unit conversions
        cgm_gas = data['CGMgas'] * 1.0e10 / Hubble_h
        hot_gas = data['HotGas'] * 1.0e10 / Hubble_h
        mvir = data['Mvir'] * 1.0e10 / Hubble_h
        stellar_mass = data['StellarMass'] * 1.0e10 / Hubble_h
        vvir = data['Vvir']
        
        infall_to_cgm = data['InfallRate_to_CGM'] * 1.0e10 / Hubble_h
        infall_to_hot = data['InfallRate_to_Hot'] * 1.0e10 / Hubble_h
        transfer_cgm_to_hot = data['TransferRate_CGM_to_Hot'] * 1.0e10 / Hubble_h
        
        # Filter valid data
        valid_mask = (mvir > 0) & (stellar_mass > 0)
        
        # Create subplot figure
        fig, axes = plt.subplots(2, 2, figsize=(20, 12))
        
        # Plot 1: Infall rates vs Halo Mass
        ax = axes[0, 0]
        valid_infall = valid_mask & (infall_to_cgm > 0)
        
        if np.sum(valid_infall) > dilute:
            indices = sample(list(np.where(valid_infall)[0]), dilute)
        else:
            indices = np.where(valid_infall)[0]
        
        scatter = ax.scatter(np.log10(mvir[indices]), np.log10(infall_to_cgm[indices]), 
                            c=np.log10(vvir[indices]), cmap='plasma', s=10, alpha=0.7)
        
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\log_{10}$ Infall Rate to CGM $(M_{\odot}/\mathrm{timestep})$')
        ax.set_title('Infall Rate to CGM vs Halo Mass')
        
        cbar = plt.colorbar(scatter, ax=ax)
        cbar.set_label(r'$\log_{10} V_{\mathrm{vir}}$ (km/s)')
        
        # Plot 2: Transfer rates vs CGM Mass
        ax = axes[0, 1]
        valid_transfer = valid_mask & (transfer_cgm_to_hot > 0) & (cgm_gas > 0)
        
        if np.sum(valid_transfer) > dilute:
            indices = sample(list(np.where(valid_transfer)[0]), dilute)
        else:
            indices = np.where(valid_transfer)[0]
        
        scatter = ax.scatter(np.log10(cgm_gas[indices]), np.log10(transfer_cgm_to_hot[indices]), 
                            c=np.log10(mvir[indices]), cmap='viridis', s=10, alpha=0.7)
        
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\log_{10}$ Transfer Rate CGM→Hot $(M_{\odot}/\mathrm{timestep})$')
        ax.set_title('CGM to Hot Transfer Rate vs CGM Mass')
        
        cbar = plt.colorbar(scatter, ax=ax)
        cbar.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        
        # Plot 3: Transfer Efficiency (Transfer Rate / CGM Mass)
        ax = axes[1, 0]
        transfer_efficiency = transfer_cgm_to_hot / cgm_gas
        valid_efficiency = valid_mask & (transfer_efficiency > 0) & np.isfinite(transfer_efficiency)
        
        if np.sum(valid_efficiency) > dilute:
            indices = sample(list(np.where(valid_efficiency)[0]), dilute)
        else:
            indices = np.where(valid_efficiency)[0]
        
        scatter = ax.scatter(np.log10(mvir[indices]), np.log10(transfer_efficiency[indices]), 
                            c=np.log10(vvir[indices]), cmap='coolwarm', s=10, alpha=0.7)
        
        # Add running median
        bin_centers, bin_averages, _ = bin_average(np.log10(transfer_efficiency[valid_efficiency]), 
                                                np.log10(mvir[valid_efficiency]), num_bins=15)
        ax.plot(bin_centers, bin_averages, 'k-', linewidth=3, label='Running Median')
        
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\log_{10}$ Transfer Efficiency (1/timestep)')
        ax.set_title('CGM Transfer Efficiency vs Halo Mass')
        ax.legend()
        
        cbar = plt.colorbar(scatter, ax=ax)
        cbar.set_label(r'$\log_{10} V_{\mathrm{vir}}$ (km/s)')
        
        # Plot 6: CGM Residence Time
        ax = axes[1, 1]
        cgm_residence_time = cgm_gas / (transfer_cgm_to_hot + 1e-10)  # Add small value to avoid division by zero
        valid_residence = valid_mask & (cgm_residence_time > 0) & np.isfinite(cgm_residence_time) & (cgm_gas > 0)
        
        if np.sum(valid_residence) > dilute:
            indices = sample(list(np.where(valid_residence)[0]), dilute)
        else:
            indices = np.where(valid_residence)[0]
        
        scatter = ax.scatter(np.log10(mvir[indices]), np.log10(cgm_residence_time[indices]), 
                            c=np.log10(cgm_gas[indices]), cmap='viridis', s=10, alpha=0.7)
        
        # Add running median
        bin_centers, bin_averages, _ = bin_average(np.log10(cgm_residence_time[valid_residence]), 
                                                np.log10(mvir[valid_residence]), num_bins=15)
        ax.plot(bin_centers, bin_averages, 'r-', linewidth=3, label='Running Median')
        
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\log_{10}$ CGM Residence Time (timesteps)')
        ax.set_title('CGM Residence Time vs Halo Mass')
        ax.legend()
        
        cbar = plt.colorbar(scatter, ax=ax)
        cbar.set_label(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
        
        plt.tight_layout()
        
        # Save the plot
        OutputDir = DirName + 'plots/'
        outputFile = OutputDir + '29.Gas_Flow_Rates_Analysis' + OutputFormat
        plt.savefig(outputFile, dpi=500, bbox_inches='tight')
        print(f'Saved gas flow rates analysis to {outputFile}')
        plt.close()

    def plot_cgm_composition_evolution(data_reader):
        """
        Plot evolution of CGM composition (pristine fraction) across redshift
        """
        print('Creating CGM composition evolution plot across redshift')
        
        # Load all required data at once
        params = ['CGMgas', 'CGMgas_pristine', 'CGMgas_enriched', 'Mvir', 'StellarMass']
        data_full = load_all_snapshots_optimized(data_reader, params)
        
        # Define halo mass bins (in log10 solar masses)
        halo_mass_bins = [
            (11.5, 12.5),    # Low mass halos
            (12.5, 13.5),    # Intermediate mass halos  
            (13.5, 15.0)     # High mass halos
        ]
        
        colors = ['red', 'green', 'blue']
        labels = [
            r'$10^{11.5} < M_{\mathrm{halo}} < 10^{12.5}$ M$_{\odot}$',
            r'$10^{12.5} < M_{\mathrm{halo}} < 10^{13.5}$ M$_{\odot}$',
            r'$10^{13.5} < M_{\mathrm{halo}}$'
        ]
        
        # Create figure
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        
        # Plot 1: Pristine fraction evolution
        ax = axes[0, 0]
        
        for bin_idx, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, colors, labels)):
            redshift_values = []
            median_pristine_fractions = []
            
            for snap in CGMsnaps:
                z = redshifts[snap]
                
                # Get data for this snapshot
                cgm_total = data_full['CGMgas'][snap]
                cgm_pristine = data_full['CGMgas_pristine'][snap]
                mvir_snap = data_full['Mvir'][snap]
                stellar_snap = data_full['StellarMass'][snap]
                
                # Filter valid data
                valid_mask = (cgm_total > 0) & (mvir_snap > 0) & (stellar_snap > 0)
                cgm_total_valid = cgm_total[valid_mask]
                cgm_pristine_valid = cgm_pristine[valid_mask]
                mvir_valid = mvir_snap[valid_mask]
                
                # Filter by halo mass bin
                log_mvir = np.log10(mvir_valid)
                mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
                
                if np.sum(mass_mask) > 10:  # Require at least 10 galaxies
                    pristine_fraction = cgm_pristine_valid[mass_mask] / cgm_total_valid[mass_mask]
                    median_pristine = np.median(pristine_fraction)
                    
                    redshift_values.append(z)
                    median_pristine_fractions.append(median_pristine)
            
            if len(redshift_values) > 0:
                ax.plot(redshift_values, median_pristine_fractions, 
                    color=color, label=label, linewidth=3, marker='o')
        
        ax.set_xlabel('Redshift')
        ax.set_ylabel('Median CGM Pristine Fraction')
        ax.set_title('CGM Pristine Fraction Evolution')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        # Plot 2: Total CGM mass evolution  
        ax = axes[0, 1]
        
        for bin_idx, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, colors, labels)):
            redshift_values = []
            median_cgm_masses = []
            
            for snap in CGMsnaps:
                z = redshifts[snap]
                
                cgm_total = data_full['CGMgas'][snap]
                mvir_snap = data_full['Mvir'][snap]
                stellar_snap = data_full['StellarMass'][snap]
                
                valid_mask = (cgm_total > 0) & (mvir_snap > 0) & (stellar_snap > 0)
                cgm_total_valid = cgm_total[valid_mask]
                mvir_valid = mvir_snap[valid_mask]
                
                log_mvir = np.log10(mvir_valid)
                mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
                
                if np.sum(mass_mask) > 10:
                    median_cgm = np.median(cgm_total_valid[mass_mask])
                    
                    redshift_values.append(z)
                    median_cgm_masses.append(median_cgm)
            
            if len(redshift_values) > 0:
                ax.plot(redshift_values, np.log10(median_cgm_masses), 
                    color=color, label=label, linewidth=3, marker='s')
        
        ax.set_xlabel('Redshift')
        ax.set_ylabel(r'$\log_{10}$ Median CGM Mass $(M_{\odot})$')
        ax.set_title('CGM Mass Evolution')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        # Plot 3: Pristine/Enriched ratio evolution
        ax = axes[1, 0]
        
        all_redshifts = []
        all_pristine_ratios = []
        
        for snap in CGMsnaps:
            z = redshifts[snap]
            
            cgm_pristine = data_full['CGMgas_pristine'][snap]
            cgm_enriched = data_full['CGMgas_enriched'][snap]
            mvir_snap = data_full['Mvir'][snap]
            stellar_snap = data_full['StellarMass'][snap]
            
            valid_mask = (cgm_pristine > 0) & (cgm_enriched > 0) & (mvir_snap > 0) & (stellar_snap > 0)
            
            if np.sum(valid_mask) > 100:
                pristine_enriched_ratio = cgm_pristine[valid_mask] / cgm_enriched[valid_mask]
                median_ratio = np.median(pristine_enriched_ratio)
                
                all_redshifts.append(z)
                all_pristine_ratios.append(median_ratio)
        
        if len(all_redshifts) > 0:
            ax.plot(all_redshifts, all_pristine_ratios, 'k-', linewidth=3, marker='D')
            ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5, label='Equal masses')
        
        ax.set_xlabel('Redshift')
        ax.set_ylabel('Median Pristine/Enriched Mass Ratio')
        ax.set_title('CGM Pristine/Enriched Ratio Evolution')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        # Plot 4: CGM fraction of total baryons evolution
        ax = axes[1, 1]
        
        for bin_idx, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, colors, labels)):
            redshift_values = []
            median_cgm_fractions = []
            
            for snap in CGMsnaps:
                z = redshifts[snap]
                
                cgm_total = data_full['CGMgas'][snap]
                mvir_snap = data_full['Mvir'][snap]
                stellar_snap = data_full['StellarMass'][snap]
                
                valid_mask = (cgm_total > 0) & (mvir_snap > 0) & (stellar_snap > 0)
                cgm_total_valid = cgm_total[valid_mask]
                mvir_valid = mvir_snap[valid_mask]
                
                log_mvir = np.log10(mvir_valid)
                mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
                
                if np.sum(mass_mask) > 10:
                    cgm_fraction = cgm_total_valid[mass_mask] / (0.17 * mvir_valid[mass_mask])
                    median_fraction = np.median(cgm_fraction)
                    
                    redshift_values.append(z)
                    median_cgm_fractions.append(median_fraction)
            
            if len(redshift_values) > 0:
                ax.plot(redshift_values, median_cgm_fractions, 
                    color=color, label=label, linewidth=3, marker='^')
        
        ax.set_xlabel('Redshift')
        ax.set_ylabel(r'Median $f_{\mathrm{CGM}} = M_{\mathrm{CGM}} / (0.17 \times M_{\mathrm{vir}})$')
        ax.set_title('CGM Baryon Fraction Evolution')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # Save the plot
        OutputDir = DirName + 'plots/'
        outputFile = OutputDir + '30.CGM_Composition_Evolution' + OutputFormat
        plt.savefig(outputFile, dpi=500, bbox_inches='tight')
        print(f'Saved CGM composition evolution to {outputFile}')
        plt.close()


    def find_halos_near_target_masses(base_dir, snap_num, target_masses, tolerance=0.2, Hubble_h=0.73):
        """
        Find halos close to target masses in a given snapshot.
        
        Parameters:
        -----------
        base_dir : str
            Directory where model files are stored
        snap_num : int
            Snapshot number to search in
        target_masses : list
            List of target masses in M_sun (not log10)
        tolerance : float
            Log10 tolerance for mass matching
        Hubble_h : float
            Hubble parameter
        
        Returns:
        --------
        list
            List of (galaxy_id, halo_mass, target_mass) tuples for selected halos
        """
        
        snapshot_name = f'Snap_{snap_num}'
        model_files = [f for f in os.listdir(base_dir) if f.startswith('model_') and f.endswith('.hdf5')]
        model_files.sort()
        
        print(f"Searching for halos near target masses in snapshot {snap_num}...")
        
        # Store all halos with their masses
        all_halos = []
        
        for model_file in model_files:
            try:
                with h5.File(os.path.join(base_dir, model_file), 'r') as f:
                    if snapshot_name not in f:
                        continue
                    
                    # Get masses and convert to physical units
                    mvir = np.array(f[snapshot_name]['Mvir']) * 1.0e10 / Hubble_h
                    galaxy_ids = np.array(f[snapshot_name]['GalaxyIndex'])
                    
                    # Store all halos above minimum mass
                    min_mass = min(target_masses) * 0.1  # Use lower threshold for initial filtering
                    valid_indices = np.where(mvir > min_mass)[0]
                    
                    for idx in valid_indices:
                        all_halos.append({
                            'galaxy_id': galaxy_ids[idx],
                            'mass': mvir[idx],
                            'log10_mass': np.log10(mvir[idx]),
                            'file': model_file,
                            'index': idx
                        })
                        
            except Exception as e:
                print(f"Error processing file {model_file}: {e}")
        
        # Find best matches for each target mass
        selected_halos = []
        
        for target_mass in target_masses:
            log10_target = np.log10(target_mass)
            best_halo = None
            best_diff = float('inf')
            
            for halo in all_halos:
                diff = abs(halo['log10_mass'] - log10_target)
                if diff < tolerance and diff < best_diff:
                    best_diff = diff
                    best_halo = halo
            
            if best_halo is not None:
                selected_halos.append((
                    best_halo['galaxy_id'], 
                    best_halo['mass'], 
                    target_mass
                ))
                print(f"  Target: {np.log10(target_mass):.1f}, Found: GalaxyIndex {best_halo['galaxy_id']}, "
                    f"Mass: {best_halo['log10_mass']:.3f} log10 M_sun")
            else:
                print(f"  Warning: No halo found near target mass {np.log10(target_mass):.1f}")
        
        return selected_halos

    def track_specific_galaxy_cgm(base_dir, galaxy_id, start_snap, end_snap, Hubble_h=0.73):
        """
        Track CGM properties of a specific galaxy across snapshots.
        
        Parameters:
        -----------
        base_dir : str
            Directory where model files are stored
        galaxy_id : int
            GalaxyIndex to track
        start_snap : int
            Starting snapshot number
        end_snap : int  
            Ending snapshot number
        Hubble_h : float
            Hubble parameter
            
        Returns:
        --------
        dict
            Dictionary containing tracked CGM properties
        """
        
        tracked_data = {
            'snapnum': [],
            'redshift': [],
            'galaxy_id': [],
            'log10_mvir': [],
            'log10_stellar_mass': [],
            'log10_cgm_mass': [],
            'log10_hot_gas': [],
            'log10_cold_gas': [],
            'type': []
        }
        
        # Determine direction of tracking
        step = -1 if end_snap < start_snap else 1
        snaps_range = range(start_snap, end_snap + step, step)
        
        # Track the galaxy across snapshots
        for snap in snaps_range:
            snapshot_name = f'Snap_{snap}'
            model_files = [f for f in os.listdir(base_dir) if f.startswith('model_') and f.endswith('.hdf5')]
            model_files.sort()
            
            found = False
            
            # Search for the galaxy by GalaxyIndex in all files for this snapshot
            for model_file in model_files:
                try:
                    with h5.File(os.path.join(base_dir, model_file), 'r') as f:
                        if snapshot_name not in f:
                            continue
                            
                        # Get all GalaxyIndexs in this file
                        galaxy_ids = np.array(f[snapshot_name]['GalaxyIndex'])
                        
                        # Find the index of our target galaxy
                        indices = np.where(galaxy_ids == galaxy_id)[0]
                        
                        if len(indices) > 0:
                            idx = indices[0]
                            
                            # Extract CGM and related properties
                            mvir = f[snapshot_name]['Mvir'][idx] * 1.0e10 / Hubble_h
                            stellar_mass = f[snapshot_name]['StellarMass'][idx] * 1.0e10 / Hubble_h
                            
                            # CGM mass - check if available
                            if 'CGMgas' in f[snapshot_name]:
                                cgm_mass = f[snapshot_name]['CGMgas'][idx] * 1.0e10 / Hubble_h
                            else:
                                cgm_mass = 0.0
                            
                            # Hot gas
                            if 'HotGas' in f[snapshot_name]:
                                hot_gas = f[snapshot_name]['HotGas'][idx] * 1.0e10 / Hubble_h
                            else:
                                hot_gas = 0.0
                            
                            # Cold gas
                            if 'ColdGas' in f[snapshot_name]:
                                cold_gas = f[snapshot_name]['ColdGas'][idx] * 1.0e10 / Hubble_h
                            else:
                                cold_gas = 0.0
                            
                            # Galaxy type
                            if 'Type' in f[snapshot_name]:
                                gal_type = f[snapshot_name]['Type'][idx]
                            else:
                                gal_type = 0
                            
                            # Store data
                            tracked_data['snapnum'].append(snap)
                            tracked_data['redshift'].append(redshifts[snap])
                            tracked_data['galaxy_id'].append(galaxy_id)
                            tracked_data['log10_mvir'].append(np.log10(mvir) if mvir > 0 else -999)
                            tracked_data['log10_stellar_mass'].append(np.log10(stellar_mass) if stellar_mass > 0 else -999)
                            tracked_data['log10_cgm_mass'].append(np.log10(cgm_mass) if cgm_mass > 0 else -999)
                            tracked_data['log10_hot_gas'].append(np.log10(hot_gas) if hot_gas > 0 else -999)
                            tracked_data['log10_cold_gas'].append(np.log10(cold_gas) if cold_gas > 0 else -999)
                            tracked_data['type'].append(gal_type)
                            
                            found = True
                            break
                            
                except Exception as e:
                    print(f"Error processing snapshot {snap} in file {model_file}: {e}")
            
            if not found:
                print(f"Warning: Could not find galaxy {galaxy_id} in snapshot {snap}")
        
        return tracked_data

    def plot_cgm_evolution_specific_halos(base_dir, start_snap=63, end_snap=0, target_masses=None, Hubble_h=0.73):
        """
        Plot CGM mass evolution for specific tracked halos.
        
        Parameters:
        -----------
        base_dir : str
            Directory where model files are stored
        start_snap : int
            Starting snapshot (usually z=0)
        end_snap : int
            Ending snapshot (higher redshift)
        target_masses : list
            Target masses in M_sun
        Hubble_h : float
            Hubble parameter
        """
        
        if target_masses is None:
            target_masses = TARGET_MASSES
        
        # Import cosmology for age calculations
        try:
            from astropy.cosmology import Planck18
            cosmo = Planck18
        except ImportError:
            print("Warning: astropy not available, using approximate age calculation")
            cosmo = None
        
        # Function to calculate age of Universe
        def redshift_to_age(z):
            if cosmo is not None:
                return cosmo.age(z).value
            else:
                # Approximate formula for Planck cosmology
                H0 = 67.4
                Omega_m = 0.315
                Omega_lambda = 0.685
                H0_Gyr = H0 / 977.8
                
                def integrand(z_prime):
                    E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                    return 1.0 / ((1 + z_prime) * E_z)
                
                z_vals = np.linspace(z, 30, 1000)
                y_vals = [integrand(z_val) for z_val in z_vals]
                age = np.trapz(y_vals, z_vals) / H0_Gyr
                return age
        
        print('Finding halos near target masses...')
        selected_halos = find_halos_near_target_masses(
            base_dir, start_snap, target_masses, TARGET_MASS_TOLERANCE, Hubble_h)
        
        if not selected_halos:
            print("No suitable halos found!")
            return
        
        # Define colors and labels
        colors = ['red', 'green', 'blue']
        labels = [
            r'$M_{\mathrm{target}} = 10^{10}$ M$_{\odot}$',
            r'$M_{\mathrm{target}} = 10^{12}$ M$_{\odot}$',
            r'$M_{\mathrm{target}} = 10^{14}$ M$_{\odot}$'
        ]
        
        # Create figure
        fig, ax = plt.subplots(figsize=(12, 8))
        
        # Track each selected halo
        for i, (galaxy_id, actual_mass, target_mass) in enumerate(selected_halos):
            print(f'\nTracking galaxy {galaxy_id} (target mass: {np.log10(target_mass):.1f})...')
            
            tracked_data = track_specific_galaxy_cgm(
                base_dir, galaxy_id, start_snap, end_snap, Hubble_h)
            
            if len(tracked_data['snapnum']) == 0:
                print(f"No data found for galaxy {galaxy_id}")
                continue
            
            # Convert to arrays and filter valid CGM data
            redshift_vals = np.array(tracked_data['redshift'])
            cgm_masses = np.array(tracked_data['log10_cgm_mass'])
            
            # Filter out invalid CGM masses
            valid_mask = cgm_masses > -900
            if np.sum(valid_mask) == 0:
                print(f"No valid CGM data for galaxy {galaxy_id}")
                continue
            
            redshift_valid = redshift_vals[valid_mask]
            cgm_masses_valid = cgm_masses[valid_mask]
            
            # Calculate ages
            age_values = np.array([redshift_to_age(z) for z in redshift_valid])
            
            # Sort by age
            sort_idx = np.argsort(age_values)
            age_values = age_values[sort_idx]
            cgm_masses_valid = cgm_masses_valid[sort_idx]
            redshift_valid = redshift_valid[sort_idx]
            
            # Plot CGM evolution
            color = colors[i % len(colors)]
            actual_mass_label = f'Actual: {np.log10(actual_mass):.2f}'
            full_label = f'{labels[i]}\n({actual_mass_label})'
            
            ax.plot(age_values, cgm_masses_valid, 
                color=color, label=full_label, linewidth=3, marker='o', markersize=5)
            
            # Print some statistics
            print(f"  Successfully tracked across {len(age_values)} snapshots")
            print(f"  CGM mass range: {cgm_masses_valid.min():.2f} to {cgm_masses_valid.max():.2f} log10 M_sun")
            print(f"  Redshift range: {redshift_valid.max():.2f} to {redshift_valid.min():.2f}")
        
        # Set up bottom x-axis (age)
        ax.set_xlabel('Age of Universe (Gyr)', fontsize=14)
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$', fontsize=14)
        
        # Create top x-axis for redshift
        ax2 = ax.twiny()
        
        # Set up the relationship between age and redshift for the top axis
        z_ticks = np.array([0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5])
        age_ticks = np.array([redshift_to_age(z) for z in z_ticks])
        
        # Set the secondary axis limits and ticks
        ax2.set_xlim(ax.get_xlim())
        ax2.set_xticks(age_ticks)
        ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
        ax2.set_xlabel('Redshift', fontsize=14)
        
        # Create legend
        leg = ax.legend(loc='lower left', frameon=False, fontsize=10)
        
        # Set reasonable axis limits
        ax.set_ylim(6.0, 14.5)
        ax.set_xlim(min(age_ticks), 14)
        
        # Add grid
        ax.grid(True, alpha=0.3)
        
        plt.title('CGM Mass Evolution for Specific Tracked Halos', fontsize=16)
        plt.tight_layout()
        
        # Save the plot
        OutputDir = base_dir + 'plots/'
        if not os.path.exists(OutputDir):
            os.makedirs(OutputDir)
        
        outputFile = OutputDir + '31.CGM_Evolution_Specific_Halos' + OutputFormat
        plt.savefig(outputFile, dpi=500, bbox_inches='tight')
        print(f'\nSaved CGM evolution plot for specific halos to {outputFile}')
        plt.close()
        
        return selected_halos

    def plot_cgm_evolution_with_scatter_specific_halos(base_dir, start_snap=63, end_snap=0, target_masses=None, Hubble_h=0.73):
        """
        Plot CGM mass evolution for specific tracked halos with additional gas components.
        """
        
        if target_masses is None:
            target_masses = TARGET_MASSES
        
        # Import cosmology for age calculations
        try:
            from astropy.cosmology import Planck18
            cosmo = Planck18
        except ImportError:
            print("Warning: astropy not available, using approximate age calculation")
            cosmo = None
        
        def redshift_to_age(z):
            if cosmo is not None:
                return cosmo.age(z).value
            else:
                H0 = 67.4
                Omega_m = 0.315
                Omega_lambda = 0.685
                H0_Gyr = H0 / 977.8
                
                def integrand(z_prime):
                    E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                    return 1.0 / ((1 + z_prime) * E_z)
                
                z_vals = np.linspace(z, 30, 1000)
                y_vals = [integrand(z_val) for z_val in z_vals]
                age = np.trapz(y_vals, z_vals) / H0_Gyr
                return age
        
        print('Finding halos near target masses...')
        selected_halos = find_halos_near_target_masses(
            base_dir, start_snap, target_masses, TARGET_MASS_TOLERANCE, Hubble_h)
        
        if not selected_halos:
            print("No suitable halos found!")
            return
        
        # Create figure with subplots
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        
        # Define colors and labels
        colors = ['red', 'green', 'blue']
        labels = [
            r'$M_{\mathrm{target}} = 10^{10}$ M$_{\odot}$',
            r'$M_{\mathrm{target}} = 10^{12}$ M$_{\odot}$',
            r'$M_{\mathrm{target}} = 10^{14}$ M$_{\odot}$'
        ]
        
        # Track each selected halo
        all_tracked_data = []
        for i, (galaxy_id, actual_mass, target_mass) in enumerate(selected_halos):
            print(f'\nTracking galaxy {galaxy_id} (target mass: {np.log10(target_mass):.1f})...')
            
            tracked_data = track_specific_galaxy_cgm(
                base_dir, galaxy_id, start_snap, end_snap, Hubble_h)
            
            if len(tracked_data['snapnum']) > 0:
                tracked_data['color'] = colors[i % len(colors)]
                tracked_data['label'] = labels[i]
                tracked_data['actual_mass'] = actual_mass
                tracked_data['target_mass'] = target_mass
                all_tracked_data.append(tracked_data)
        
        # Plot 1: CGM Mass Evolution
        ax = axes[0, 0]
        for i, tracked_data in enumerate(all_tracked_data):
            redshift_vals = np.array(tracked_data['redshift'])
            cgm_masses = np.array(tracked_data['log10_cgm_mass'])
            
            valid_mask = cgm_masses > -900
            if np.sum(valid_mask) > 0:
                redshift_valid = redshift_vals[valid_mask]
                cgm_masses_valid = cgm_masses[valid_mask]
                age_values = np.array([redshift_to_age(z) for z in redshift_valid])
                
                sort_idx = np.argsort(age_values)
                age_values = age_values[sort_idx]
                cgm_masses_valid = cgm_masses_valid[sort_idx]
                
                actual_mass_label = f'Actual: {np.log10(tracked_data["actual_mass"]):.2f}'
                full_label = f'{tracked_data["label"]}\n({actual_mass_label})'
                
                ax.plot(age_values, cgm_masses_valid, 
                    color=tracked_data['color'], label=full_label, 
                    linewidth=3, marker='o', markersize=4)
        
        ax.set_xlabel('Age of Universe (Gyr)')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
        ax.set_title('CGM Mass Evolution')
        ax.legend(fontsize=9)
        ax.grid(True, alpha=0.3)
        ax.set_ylim(6.0, 14.5)
        
        # Plot 2: Hot Gas Evolution
        ax = axes[0, 1]
        for tracked_data in all_tracked_data:
            redshift_vals = np.array(tracked_data['redshift'])
            hot_gas_masses = np.array(tracked_data['log10_hot_gas'])
            
            valid_mask = hot_gas_masses > -900
            if np.sum(valid_mask) > 0:
                redshift_valid = redshift_vals[valid_mask]
                hot_gas_valid = hot_gas_masses[valid_mask]
                age_values = np.array([redshift_to_age(z) for z in redshift_valid])
                
                sort_idx = np.argsort(age_values)
                age_values = age_values[sort_idx]
                hot_gas_valid = hot_gas_valid[sort_idx]
                
                ax.plot(age_values, hot_gas_valid, 
                    color=tracked_data['color'], linewidth=3, marker='s', markersize=4)
        
        ax.set_xlabel('Age of Universe (Gyr)')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{Hot}}\ (M_{\odot})$')
        ax.set_title('Hot Gas Mass Evolution')
        ax.grid(True, alpha=0.3)
        ax.set_ylim(6.0, 14.5)
        
        # Plot 3: Halo Mass Evolution
        ax = axes[1, 0]
        for tracked_data in all_tracked_data:
            redshift_vals = np.array(tracked_data['redshift'])
            halo_masses = np.array(tracked_data['log10_mvir'])
            
            valid_mask = halo_masses > -900
            if np.sum(valid_mask) > 0:
                redshift_valid = redshift_vals[valid_mask]
                halo_masses_valid = halo_masses[valid_mask]
                age_values = np.array([redshift_to_age(z) for z in redshift_valid])
                
                sort_idx = np.argsort(age_values)
                age_values = age_values[sort_idx]
                halo_masses_valid = halo_masses_valid[sort_idx]
                
                ax.plot(age_values, halo_masses_valid, 
                    color=tracked_data['color'], linewidth=3, marker='^', markersize=4)
        
        ax.set_xlabel('Age of Universe (Gyr)')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
        ax.set_title('Halo Mass Evolution')
        ax.grid(True, alpha=0.3)
        
        # Plot 4: CGM Fraction Evolution
        ax = axes[1, 1]
        for tracked_data in all_tracked_data:
            redshift_vals = np.array(tracked_data['redshift'])
            cgm_masses = np.array(tracked_data['log10_cgm_mass'])
            halo_masses = np.array(tracked_data['log10_mvir'])
            
            valid_mask = (cgm_masses > -900) & (halo_masses > -900)
            if np.sum(valid_mask) > 0:
                redshift_valid = redshift_vals[valid_mask]
                cgm_masses_valid = cgm_masses[valid_mask]
                halo_masses_valid = halo_masses[valid_mask]
                
                # Calculate CGM fraction (normalized by cosmic baryon fraction)
                cgm_fraction = cgm_masses_valid - np.log10(0.17) - halo_masses_valid
                
                age_values = np.array([redshift_to_age(z) for z in redshift_valid])
                
                sort_idx = np.argsort(age_values)
                age_values = age_values[sort_idx]
                cgm_fraction = cgm_fraction[sort_idx]
                
                ax.plot(age_values, cgm_fraction, 
                    color=tracked_data['color'], linewidth=3, marker='d', markersize=4)
        
        ax.set_xlabel('Age of Universe (Gyr)')
        ax.set_ylabel(r'$\log_{10}(M_{\mathrm{CGM}} / (0.17 \times M_{\mathrm{vir}}))$')
        ax.set_title('CGM Baryon Fraction Evolution')
        ax.grid(True, alpha=0.3)
        ax.axhline(y=0, color='gray', linestyle='--', alpha=0.5)
        
        plt.tight_layout()
        
        # Save the plot
        OutputDir = base_dir + 'plots/'
        if not os.path.exists(OutputDir):
            os.makedirs(OutputDir)
        
        outputFile = OutputDir + '32.CGM_Evolution_Specific_Halos_MultiPanel' + OutputFormat
        plt.savefig(outputFile, dpi=500, bbox_inches='tight')
        print(f'\nSaved multi-panel CGM evolution plot to {outputFile}')
        plt.close()
        
        return selected_halos

    print('\n=== Creating CGM Tracking Analysis Plots ===')
    plot_cgm_composition_analysis(data_reader)
    plot_gas_flow_rates_analysis(data_reader)
    plot_cgm_composition_evolution(data_reader)

    base_dir = DirName
    start_snap = 63  # z=0
    end_snap = 0     # High redshift
    target_masses = [1e10, 1e12, 1e14]  # M_sun
    
    # Create basic evolution plot
    selected_halos = plot_cgm_evolution_specific_halos(
        base_dir, start_snap, end_snap, target_masses, Hubble_h)
    
    # Create multi-panel plot with additional gas components
    plot_cgm_evolution_with_scatter_specific_halos(
        base_dir, start_snap, end_snap, target_masses, Hubble_h)
    
    # Print summary
    if selected_halos:
        print('\n=== Summary of Tracked Halos ===')
        for i, (galaxy_id, actual_mass, target_mass) in enumerate(selected_halos):
            print(f'Halo {i+1}: GalaxyIndex {galaxy_id}')
            print(f'  Target mass: {np.log10(target_mass):.1f} log10 M_sun')
            print(f'  Actual mass: {np.log10(actual_mass):.3f} log10 M_sun')
            print(f'  Difference: {abs(np.log10(actual_mass) - np.log10(target_mass)):.3f} dex')
    
    print('\nCGM evolution analysis for specific halos completed!')