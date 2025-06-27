#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
import gc  # For garbage collection
import time  # For timing measurements

from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium_CGMfirst/'
DirName_c16 = './output/millennium_vanilla/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73
BoxSize = 62.5
VolumeFraction = 1.0

# Plotting options
whichimf = 1
dilute = 7500  # Not used for filtering anymore - only for scatter plots if needed

# Performance options
ENABLE_FILTERING = True  # Filter out galaxies with zero stellar mass early
MIN_STELLAR_MASS = 0.0001  # Minimum stellar mass to include (in file units, before 1e10/h scaling)
MEMORY_EFFICIENT = False  # Use memory-efficient operations

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (10,8)
plt.rcParams["figure.dpi"] = 500
plt.rcParams["font.size"] = 14

# ==================================================================

def read_hdf_single(filepath, snap_num=None, param=None):
    """Read data from a single SAGE model file"""
    with h5.File(filepath, 'r') as h5file:
        data = h5file[snap_num][param][:]  # Use slice notation for better performance
    return data

def read_hdf_multiple(directory, snap_num=None, param=None):
    """Read and combine data from multiple SAGE model files efficiently"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    if not model_files:
        raise ValueError(f"No model files found in {directory}")
    
    # First pass: determine total size for efficient memory allocation
    total_size = 0
    for model_file in model_files:
        filepath = os.path.join(directory, model_file)
        with h5.File(filepath, 'r') as h5file:
            total_size += h5file[snap_num][param].shape[0]
    
    # Pre-allocate array for better performance
    with h5.File(os.path.join(directory, model_files[0]), 'r') as h5file:
        dtype = h5file[snap_num][param].dtype
    
    combined_data = np.empty(total_size, dtype=dtype)
    
    # Second pass: read data into pre-allocated array
    offset = 0
    for model_file in model_files:
        filepath = os.path.join(directory, model_file)
        with h5.File(filepath, 'r') as h5file:
            data = h5file[snap_num][param][:]
            combined_data[offset:offset + len(data)] = data
            offset += len(data)
    
    return combined_data

def read_multiple_params(directory, snap_num, params, apply_filter=False, min_stellar_mass=None):
    """Read multiple parameters from all model files efficiently in one pass"""
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    if not model_files:
        raise ValueError(f"No model files found in {directory}")
    
    print(f"Processing {len(model_files)} model files...")
    
    if not apply_filter:
        # Original behavior - pre-allocate full arrays
        return _read_multiple_params_preallocated(directory, model_files, snap_num, params)
    else:
        # Memory-efficient with filtering
        return _read_multiple_params_filtered(directory, model_files, snap_num, params, min_stellar_mass)

def _read_multiple_params_preallocated(directory, model_files, snap_num, params):
    """Pre-allocate arrays and read all data (original behavior)"""
    # First pass: determine total size and data types
    total_size = 0
    dtypes = {}
    for model_file in model_files:
        filepath = os.path.join(directory, model_file)
        with h5.File(filepath, 'r') as h5file:
            if total_size == 0:  # First file
                for param in params:
                    dtypes[param] = h5file[snap_num][param].dtype
            total_size += h5file[snap_num][params[0]].shape[0]  # Assume all params have same size
    
    # Pre-allocate arrays
    data_arrays = {}
    for param in params:
        data_arrays[param] = np.empty(total_size, dtype=dtypes[param])
    
    # Second pass: read all data
    offset = 0
    for i, model_file in enumerate(model_files):
        if i % 10 == 0:
            print(f"  Processing file {i+1}/{len(model_files)}: {model_file}")
        filepath = os.path.join(directory, model_file)
        with h5.File(filepath, 'r') as h5file:
            for param in params:
                data = h5file[snap_num][param][:]
                data_arrays[param][offset:offset + len(data)] = data
            offset += len(data)
    
    return data_arrays

def _read_multiple_params_filtered(directory, model_files, snap_num, params, min_stellar_mass):
    """Read data with filtering to reduce memory usage"""
    all_data = {param: [] for param in params}
    total_galaxies = 0
    filtered_galaxies = 0
    
    for i, model_file in enumerate(model_files):
        if i % 10 == 0:
            print(f"  Processing file {i+1}/{len(model_files)}: {model_file}")
        
        filepath = os.path.join(directory, model_file)
        with h5.File(filepath, 'r') as h5file:
            # Read stellar mass and type for filtering
            stellar_mass = h5file[snap_num]['StellarMass'][:]
            galaxy_type = h5file[snap_num]['Type'][:]
            total_galaxies += len(stellar_mass)
            
            # Create filter mask (stellar mass AND Type == 0)
            if min_stellar_mass is not None:
                mask = (stellar_mass >= min_stellar_mass) & (galaxy_type == 0)
            else:
                mask = (stellar_mass > 0) & (galaxy_type == 0)  # Filter out zero stellar mass AND require Type == 0
            
            filtered_count = np.sum(mask)
            filtered_galaxies += filtered_count
            
            # Print debug info for first file
            if i == 0:
                print(f"    Debug: stellar mass range: {stellar_mass.min():.2e} to {stellar_mass.max():.2e}")
                print(f"    Debug: Type distribution: {np.bincount(galaxy_type.astype(int))}")
                print(f"    Debug: {filtered_count}/{len(stellar_mass)} galaxies pass filter (Type==0 + stellar mass)")
            
            # Only read and store data that passes the filter
            if np.any(mask):
                for param in params:
                    data = h5file[snap_num][param][:]
                    all_data[param].append(data[mask])
        
        # Periodic garbage collection for memory management
        if i % 50 == 0:
            gc.collect()
    
    print(f"  Total galaxies processed: {total_galaxies}")
    print(f"  Galaxies passing filter: {filtered_galaxies}")
    
    # Concatenate all filtered data
    final_data = {}
    for param in params:
        if all_data[param]:
            final_data[param] = np.concatenate(all_data[param])
        else:
            final_data[param] = np.array([])
    
    return final_data

def calculate_median_relation(Mvir, StellarMass, n_bins=15, min_galaxies_per_bin=10):
    """Calculate median stellar mass as a function of halo mass"""
    # Filter out zero/negative values
    mask = (Mvir > 0) & (StellarMass > 0)
    if not np.any(mask):
        return np.array([]), np.array([])
    
    Mvir_clean = Mvir[mask]
    StellarMass_clean = StellarMass[mask]
    
    # Create logarithmic bins for halo mass
    log_mvir_min = np.log10(Mvir_clean.min())
    log_mvir_max = np.log10(Mvir_clean.max())
    log_mvir_bins = np.linspace(log_mvir_min, log_mvir_max, n_bins + 1)
    
    median_mvir = []
    median_stellar_mass = []
    
    for i in range(n_bins):
        # Find galaxies in this halo mass bin
        bin_mask = (np.log10(Mvir_clean) >= log_mvir_bins[i]) & (np.log10(Mvir_clean) < log_mvir_bins[i+1])
        
        if np.sum(bin_mask) >= min_galaxies_per_bin:
            mvir_bin = Mvir_clean[bin_mask]
            stellar_mass_bin = StellarMass_clean[bin_mask]
            
            # Calculate medians
            median_mvir.append(np.median(mvir_bin))
            median_stellar_mass.append(np.median(stellar_mass_bin))
    
    return np.array(median_mvir), np.array(median_stellar_mass)

def plot_behroozi13(ax, z, labels=True, label='Behroozi+13'):
    """Plot Behroozi+13 stellar mass-halo mass relation"""
    # Define halo mass range
    xmf = np.linspace(10.0, 15.0, 100)  # log10(Mvir/Msun)
    
    a = 1.0/(1.0+z)
    nu = np.exp(-4*a*a)
    log_epsilon = -1.777 + (-0.006*(a-1)) * nu
    M1 = 11.514 + (-1.793 * (a-1) - 0.251 * z) * nu
    alpha = -1.412 + 0.731 * nu * (a-1)
    delta = 3.508 + (2.608*(a-1)-0.043 * z) * nu
    gamma = 0.316 + (1.319*(a-1)+0.279 * z) * nu
    Min = xmf - M1
    fx = -np.log10(np.power(10, alpha*Min) + 1.0) + delta * np.power(np.log10(1+np.exp(Min)), gamma) / (1+np.exp(np.power(10, -Min)))
    f = -0.3 + delta * np.power(np.log10(2.0), gamma) / (1+np.exp(1))
    m = log_epsilon + M1 + fx - f
    
    if not labels:
        ax.plot(xmf, m, 'r', linestyle='dashdot', linewidth=1.5)
    else:
        ax.plot(xmf, m, 'r', linestyle='dashdot', linewidth=1.5, label=label)

def plot_smhm(Mvir, StellarMass, Mvir_c16, StellarMass_c16, OutputDir, OutputFormat):
    """Plot Stellar Mass vs Halo Mass and other properties"""
    
    # Check if we have any data to plot
    if len(Mvir) == 0 and len(Mvir_c16) == 0:
        print("Warning: No data to plot! Both datasets are empty.")
        return
    
    # Create a scatter plot of Stellar Mass vs Halo Mass
    plt.figure()
    
    # Plot SAGE data if available
    sage_median_mvir = np.array([])
    sage_median_stellar = np.array([])
    if len(Mvir) > 0:
        # Filter out zero/negative values for log plotting
        sage_mask = (Mvir > 0) & (StellarMass > 0)
        if np.any(sage_mask):
            # plt.scatter(Mvir[sage_mask], StellarMass[sage_mask], s=1, alpha=0.3, c='darkblue', label='SAGE')
            # print(f"Plotted {np.sum(sage_mask)} SAGE galaxies")
            
            # Calculate median relation for SAGE
            sage_median_mvir, sage_median_stellar = calculate_median_relation(Mvir, StellarMass)
            if len(sage_median_mvir) > 0:
                plt.plot(sage_median_mvir, sage_median_stellar, '-', color='blue', linewidth=3, 
                        label='SAGE median', zorder=10)
                print(f"Added SAGE median line with {len(sage_median_mvir)} points")
        else:
            print("Warning: No valid SAGE data for plotting (all zero/negative values)")
    else:
        print("Warning: No SAGE data to plot")
    
    # Plot C16 data if available
    c16_median_mvir = np.array([])
    c16_median_stellar = np.array([])
    if len(Mvir_c16) > 0:
        # Filter out zero/negative values for log plotting
        c16_mask = (Mvir_c16 > 0) & (StellarMass_c16 > 0)
        if np.any(c16_mask):
            # plt.scatter(Mvir_c16[c16_mask], StellarMass_c16[c16_mask], s=1, alpha=0.3, c='darkred', label='C16')
            # print(f"Plotted {np.sum(c16_mask)} C16 galaxies")
            
            # Calculate median relation for C16
            c16_median_mvir, c16_median_stellar = calculate_median_relation(Mvir_c16, StellarMass_c16)
            if len(c16_median_mvir) > 0:
                plt.plot(c16_median_mvir, c16_median_stellar, '-', color='red', linewidth=3, 
                        label='C16 median', zorder=10)
                print(f"Added C16 median line with {len(c16_median_mvir)} points")
        else:
            print("Warning: No valid C16 data for plotting (all zero/negative values)")
    else:
        print("Warning: No C16 data to plot")
    
    # Load observational data (store handles for legend)
    obs_handles = []
    obs_labels = []
    
    try:
        obs_data = np.loadtxt('./data/Romeo20_SMHM.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        log_ms_obs = log_mh_obs + log_ms_mh_obs
        # Convert from log space to linear space for plotting
        mh_obs = 10**log_mh_obs
        ms_obs = 10**log_ms_obs
        handle = plt.scatter(mh_obs, ms_obs, s=50, alpha=0.8, 
                   color='white', marker='*', edgecolors='orange', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Romeo+20')
    except FileNotFoundError:
        print("Romeo20_SMHM.dat file not found")

    try:
        obs_data = np.loadtxt('./data/Romeo20_SMHM_ETGs.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        log_ms_obs = log_mh_obs + log_ms_mh_obs
        # Convert from log space to linear space for plotting
        mh_obs = 10**log_mh_obs
        ms_obs = 10**log_ms_obs
        plt.scatter(mh_obs, ms_obs, s=50, alpha=0.8, color='white', 
                   marker='*', edgecolors='orange', linewidth=0.5)  # Same style as Romeo+20, no separate legend
    except FileNotFoundError:
        print("Romeo20_SMHM_ETGs.dat file not found")

    try:
        obs_data = np.loadtxt('./data/SatKinsAndClusters_Kravtsov18.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        # Convert from log space to linear space for plotting
        mh_obs = 10**log_mh_obs
        ms_mh_obs = 10**log_ms_mh_obs
        handle = plt.scatter(mh_obs, ms_mh_obs, s=50, alpha=0.8,
                   color='purple', marker='s', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Kravtsov+18')
    except FileNotFoundError:
        print("SatKinsAndClusters_Kravtsov18.dat file not found")

    try:
        # Moster data loading 
        try:
            moster_data = np.loadtxt('./optim/data/Moster_2013.csv', delimiter=None)
            if moster_data.ndim == 1:
                moster_data = moster_data.reshape(-1, 2)
        except:
            with open('./optim/Moster_2013.csv', 'r') as f:
                lines = f.readlines()
            all_values = []
            for line in lines:
                if not line.strip() or line.startswith('#'):
                    continue
                values = [float(x) for x in line.strip().split() if x.strip()]
                all_values.extend(values)
            moster_data = np.array(all_values).reshape(-1, 2)
        
        log_mh_moster = moster_data[:, 0]
        log_ms_moster = moster_data[:, 1]
        # Convert from log space to linear space for plotting
        mh_moster = 10**log_mh_moster
        ms_moster = 10**log_ms_moster
        handle, = plt.plot(mh_moster, ms_moster, 
                   linestyle='-.', linewidth=1.5, color='blue')
        obs_handles.append(handle)
        obs_labels.append('Moster+13')
    except Exception as e:
        print(f"Error loading Moster13 data: {e}")
    
    # Add Behroozi+13 model (z=0 for Snap_63)
    # Calculate Behroozi+13 relation manually to ensure proper coordinate conversion
    xmf = np.linspace(10.0, 15.0, 100)  # log10(Mvir/Msun)
    z = 0.0  # For Snap_63
    a = 1.0/(1.0+z)
    nu = np.exp(-4*a*a)
    log_epsilon = -1.777 + (-0.006*(a-1)) * nu
    M1 = 11.514 + (-1.793 * (a-1) - 0.251 * z) * nu
    alpha = -1.412 + 0.731 * nu * (a-1)
    delta = 3.508 + (2.608*(a-1)-0.043 * z) * nu
    gamma = 0.316 + (1.319*(a-1)+0.279 * z) * nu
    Min = xmf - M1
    fx = -np.log10(np.power(10, alpha*Min) + 1.0) + delta * np.power(np.log10(1+np.exp(Min)), gamma) / (1+np.exp(np.power(10, -Min)))
    f = -0.3 + delta * np.power(np.log10(2.0), gamma) / (1+np.exp(1))
    log_ms_behroozi = log_epsilon + M1 + fx - f
    
    # Convert to linear space for plotting
    mvir_behroozi = 10**xmf
    ms_behroozi = 10**log_ms_behroozi
    
    behroozi_handle, = plt.plot(mvir_behroozi, ms_behroozi, 'r', linestyle='dashdot', linewidth=1.5, label='Behroozi+13')
    obs_handles.append(behroozi_handle)
    obs_labels.append('Behroozi+13')
    print("Added Behroozi+13 model curve")
    
    # Set scales and limits only if we have data
    if len(Mvir) > 0 or len(Mvir_c16) > 0:
        plt.xscale('log')
        plt.yscale('log')
        plt.ylim(1e8, 1e12)
        plt.xlim(1e10, 1e15)
        plt.xlabel(r'$M_{\rm vir} \; [M_\odot]$')
        plt.ylabel(r'$M_{\rm star} \; [M_\odot]$')
        
        # Create combined legend with both model and observational data
        if obs_handles:
            plt.legend(loc='upper left', frameon=False, fontsize=12)
        else:
            plt.legend(loc='upper left', frameon=False, fontsize=12)
    else:
        plt.text(0.5, 0.5, 'No data to plot', transform=plt.gca().transAxes, 
                ha='center', va='center', fontsize=16)
        plt.xlabel('No data')
        plt.ylabel('No data')
    
    # Save the plot
    output_file = os.path.join(OutputDir, 'smhm' + OutputFormat)
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()


# ==================================================================

if __name__ == '__main__':

    print('Running allresults (local) - Optimized Version\n')
    start_time = time.time()

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    # Read multiple parameters efficiently in one pass
    print('Reading Mvir, StellarMass, and Type data...')
    if ENABLE_FILTERING:
        print(f'Applying filter: StellarMass >= {MIN_STELLAR_MASS:.1e} (file units) AND Type == 0')
    
    data = read_multiple_params(DirName, snap_num=Snapshot, params=['Mvir', 'StellarMass', 'Type'], 
                              apply_filter=ENABLE_FILTERING, min_stellar_mass=MIN_STELLAR_MASS)
    
    print(f'Loaded {len(data["Mvir"])} galaxies after filtering')
    
    # If no data after filtering, try without filtering
    if len(data["Mvir"]) == 0 and ENABLE_FILTERING:
        print("Warning: No galaxies pass the filter. Trying without filtering...")
        data = read_multiple_params(DirName, snap_num=Snapshot, params=['Mvir', 'StellarMass', 'Type'], 
                                  apply_filter=False, min_stellar_mass=None)
        print(f'Loaded {len(data["Mvir"])} galaxies without filtering')
    
    # Apply unit conversion
    Mvir = data['Mvir'] * 1.0e10 / Hubble_h
    StellarMass = data['StellarMass'] * 1.0e10 / Hubble_h
    Type = data['Type']
    
    # Apply Type == 0 filter if not already applied during file reading
    if not ENABLE_FILTERING or len(data["Mvir"]) > 0:
        type_mask = Type == 0
        print(f'SAGE: {np.sum(type_mask)}/{len(Type)} galaxies have Type == 0')
        
        if np.any(type_mask):
            Mvir = Mvir[type_mask]
            StellarMass = StellarMass[type_mask] 
            Type = Type[type_mask]
        else:
            print("Warning: No SAGE galaxies have Type == 0!")
            Mvir = np.array([])
            StellarMass = np.array([])
            Type = np.array([])
    
    # Print actual ranges after unit conversion for debugging
    if len(StellarMass) > 0:
        non_zero_mask = StellarMass > 0
        if np.any(non_zero_mask):
            print(f'SAGE stellar mass range after scaling: {StellarMass[non_zero_mask].min():.2e} to {StellarMass[non_zero_mask].max():.2e} M_sun')
            print(f'SAGE: {np.sum(non_zero_mask)}/{len(StellarMass)} galaxies have non-zero stellar mass')
    
    # Clear intermediate data to save memory
    del data
    if MEMORY_EFFICIENT:
        gc.collect()

    print('Reading galaxy properties from', DirName_c16)
    model_files_c16 = [f for f in os.listdir(DirName_c16) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files_c16)} model files')

    # Read C16 data - if single file, use single file reader, otherwise use multiple
    if len(model_files_c16) == 1:
        print('Reading C16 data from single file...')
        filepath_c16 = os.path.join(DirName_c16, FileName)
        
        if ENABLE_FILTERING:
            # For single file, we need to implement filtering differently
            with h5.File(filepath_c16, 'r') as h5file:
                stellar_mass_raw = h5file[Snapshot]['StellarMass'][:]
                galaxy_type_raw = h5file[Snapshot]['Type'][:]
                print(f'C16 stellar mass range: {stellar_mass_raw.min():.2e} to {stellar_mass_raw.max():.2e} (file units)')
                print(f'C16 Type distribution: {np.bincount(galaxy_type_raw.astype(int))}')
                
                mask = (stellar_mass_raw >= MIN_STELLAR_MASS) & (galaxy_type_raw == 0)
                print(f'C16: {np.sum(mask)}/{len(stellar_mass_raw)} galaxies pass filter (Type==0 + stellar mass)')
                
                if np.any(mask):
                    Mvir_c16 = h5file[Snapshot]['Mvir'][:][mask] * 1.0e10 / Hubble_h
                    StellarMass_c16 = stellar_mass_raw[mask] * 1.0e10 / Hubble_h
                    Type_c16 = galaxy_type_raw[mask]
                else:
                    # If no data passes filter, try without stellar mass filter but keep Type == 0
                    print("Warning: No C16 galaxies pass the full filter. Trying with Type==0 only...")
                    type_only_mask = galaxy_type_raw == 0
                    if np.any(type_only_mask):
                        Mvir_c16 = h5file[Snapshot]['Mvir'][:][type_only_mask] * 1.0e10 / Hubble_h
                        StellarMass_c16 = stellar_mass_raw[type_only_mask] * 1.0e10 / Hubble_h
                        Type_c16 = galaxy_type_raw[type_only_mask]
                        print(f'C16: {np.sum(type_only_mask)} galaxies with Type==0 loaded')
                    else:
                        print("Warning: No C16 galaxies have Type==0!")
                        Mvir_c16 = np.array([])
                        StellarMass_c16 = np.array([])
                        Type_c16 = np.array([])
                
                # Print C16 ranges after unit conversion
                if len(StellarMass_c16) > 0:
                    non_zero_mask_c16 = StellarMass_c16 > 0
                    if np.any(non_zero_mask_c16):
                        print(f'C16 stellar mass range after scaling: {StellarMass_c16[non_zero_mask_c16].min():.2e} to {StellarMass_c16[non_zero_mask_c16].max():.2e} M_sun')
                        print(f'C16: {np.sum(non_zero_mask_c16)}/{len(StellarMass_c16)} galaxies have non-zero stellar mass')
        else:
            # No filtering enabled, but we still need Type == 0
            Mvir_c16 = read_hdf_single(filepath_c16, snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
            StellarMass_c16 = read_hdf_single(filepath_c16, snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
            Type_c16 = read_hdf_single(filepath_c16, snap_num=Snapshot, param='Type')
            
            # Apply Type == 0 filter
            type_mask_c16 = Type_c16 == 0
            print(f'C16: {np.sum(type_mask_c16)}/{len(Type_c16)} galaxies have Type == 0')
            
            if np.any(type_mask_c16):
                Mvir_c16 = Mvir_c16[type_mask_c16]
                StellarMass_c16 = StellarMass_c16[type_mask_c16]
                Type_c16 = Type_c16[type_mask_c16]
            else:
                print("Warning: No C16 galaxies have Type == 0!")
                Mvir_c16 = np.array([])
                StellarMass_c16 = np.array([])
                Type_c16 = np.array([])
    else:
        print('Reading C16 data from multiple files...')
        data_c16 = read_multiple_params(DirName_c16, snap_num=Snapshot, params=['Mvir', 'StellarMass', 'Type'],
                                      apply_filter=ENABLE_FILTERING, min_stellar_mass=MIN_STELLAR_MASS)
        
        print(f'Loaded {len(data_c16["Mvir"])} C16 galaxies after filtering')
        
        Mvir_c16 = data_c16['Mvir'] * 1.0e10 / Hubble_h
        StellarMass_c16 = data_c16['StellarMass'] * 1.0e10 / Hubble_h
        Type_c16 = data_c16['Type']
        
        # Apply Type == 0 filter if not already applied during file reading
        if not ENABLE_FILTERING or len(data_c16["Mvir"]) > 0:
            type_mask_c16 = Type_c16 == 0
            print(f'C16: {np.sum(type_mask_c16)}/{len(Type_c16)} galaxies have Type == 0')
            
            if np.any(type_mask_c16):
                Mvir_c16 = Mvir_c16[type_mask_c16]
                StellarMass_c16 = StellarMass_c16[type_mask_c16]
                Type_c16 = Type_c16[type_mask_c16]
            else:
                print("Warning: No C16 galaxies have Type == 0!")
                Mvir_c16 = np.array([])
                StellarMass_c16 = np.array([])
                Type_c16 = np.array([])
        
        # Clear intermediate data
        del data_c16
        if MEMORY_EFFICIENT:
            gc.collect()

    plot_smhm(Mvir, StellarMass, Mvir_c16, StellarMass_c16, OutputDir, OutputFormat)
    
    end_time = time.time()
    total_time = end_time - start_time
    
    print(f'\nProcessing completed in {total_time:.2f} seconds')
    print(f'Final dataset sizes:')
    print(f'  SAGE: {len(Mvir)} galaxies')
    print(f'  C16:  {len(Mvir_c16)} galaxies')
    
    if ENABLE_FILTERING:
        print(f'Data filtering was enabled (min stellar mass: {MIN_STELLAR_MASS:.1e} file units = {MIN_STELLAR_MASS * 1.0e10 / Hubble_h:.1e} M_sun)')
    
    if len(Mvir) == 0 and len(Mvir_c16) == 0:
        print('WARNING: No data was loaded! Consider:')
        print(f'  1. Lowering MIN_STELLAR_MASS threshold (currently {MIN_STELLAR_MASS:.1e} file units)')
        print('  2. Setting ENABLE_FILTERING = False')
        print('  3. Checking if data files contain the expected parameters')
        print('  Note: Data appears to be in units of 1e10 M_sun/h, with max values ~40-50 in file units')
    
    print('Plot saved to:', os.path.join(OutputDir, 'smhm' + OutputFormat))
