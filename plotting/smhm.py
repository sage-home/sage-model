#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os

from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
DirName_old = './output/millennium_vanilla/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details - NEW MODEL (SAGE 2.0)
Hubble_h = 0.73
BoxSize = 62.5          # Mpc/h
VolumeFraction = 1.0

# Simulation details - OLD MODEL (SAGE C16) - ADD THESE PARAMETERS
BoxSize_old = 62.5      # Mpc/h - Change this to your old model's box size
VolumeFraction_old = 1.0  # Change this to your old model's volume fraction
Hubble_h_old = 0.73     # Change this if old model uses different Hubble parameter

# Plotting options
whichimf = 1
dilute = 7500  # Not used for filtering anymore - only for scatter plots if needed
sSFRcut = -11.0
bulge_ratio_cut = 0.1  # Legacy parameter - now using SAGE 2.0/C16 criteria for ETG/LTG

OutputFormat = '.pdf'
# plt.rcParams["figure.figsize"] = (10,8)
# plt.rcParams["figure.dpi"] = 500
# plt.rcParams["font.size"] = 14
plt.style.use('/Users/mbradley/Documents/cohare_palatino_sty.mplstyle')

# ==================================================================

def read_hdf_smart_sampling(dirname, filename=None, snap_num=None, param=None):
    """Read data with smart sampling strategy that preserves rare massive haloes"""
    if not os.path.exists(dirname):
        print(f"Directory {dirname} does not exist")
        return None
        
    model_files = [f for f in os.listdir(dirname) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    if not model_files:
        print(f"No model files found in {dirname}")
        return None
    
    combined_data = None
    
    for model_file in model_files:
        try:
            property = h5.File(dirname + model_file, 'r')
            data = np.array(property[snap_num][param])
            
            if combined_data is None:
                combined_data = data
            else:
                combined_data = np.concatenate((combined_data, data))
        except Exception as e:
            print(f"Error reading {model_file}: {e}")
            
    return combined_data

def smart_sampling_strategy(mvir, stellar_mass, galaxy_type, target_sample_size=None):
    """
    Filter for all galaxies with positive stellar mass (centrals + satellites)
    No sampling - use ALL galaxies for better statistics
    """
    # Filter for all galaxies with positive stellar mass (centrals + satellites)
    mass_mask = stellar_mass > 0
    all_indices = np.where(mass_mask)[0]
    
    central_count = np.sum((galaxy_type == 0) & mass_mask)
    satellite_count = np.sum((galaxy_type == 1) & mass_mask)
    
    print(f"Total galaxies with M* > 0: {len(all_indices)}")
    print(f"  - Centrals: {central_count}")
    print(f"  - Satellites: {satellite_count}")
    print(f"Mass range: {np.log10(mvir[all_indices].min()):.2f} to {np.log10(mvir[all_indices].max()):.2f}")
    
    return all_indices

def smart_sampling_strategy_sf_passive(mvir, stellar_mass, galaxy_type, ssfr, ssfr_cut, target_sample_size=None):
    """
    Filter and separate star forming and passive galaxies (centrals + satellites)
    No sampling - use ALL galaxies for better statistics
    """
    # Filter for all galaxies with positive stellar mass (centrals + satellites)
    mass_mask = stellar_mass > 0
    all_indices = np.where(mass_mask)[0]
    all_ssfr = ssfr[mass_mask]
    all_types = galaxy_type[mass_mask]
    
    # Separate into star forming and passive
    sf_mask = all_ssfr > ssfr_cut
    passive_mask = all_ssfr <= ssfr_cut
    
    # Star forming galaxies
    sf_indices = all_indices[sf_mask]
    sf_types = all_types[sf_mask]
    
    # Passive galaxies
    passive_indices = all_indices[passive_mask]
    passive_types = all_types[passive_mask]
    
    print(f"Star forming galaxies: {len(sf_indices)}")
    print(f"  - Centrals: {np.sum(sf_types == 0)}, Satellites: {np.sum(sf_types == 1)}")
    print(f"Passive galaxies: {len(passive_indices)}")
    print(f"  - Centrals: {np.sum(passive_types == 0)}, Satellites: {np.sum(passive_types == 1)}")
    print(f"Total galaxies: {len(all_indices)}")
    
    return sf_indices, passive_indices

def smart_sampling_strategy_centrals_only(mvir, stellar_mass, galaxy_type, target_sample_size=None):
    """
    Filter for CENTRAL galaxies only (Type == 0) for proper SMHM relation
    NOW WITH PROPER MASS FILTERING
    """
    # Filter for central galaxies with positive stellar mass AND positive virial mass
    central_mask = (galaxy_type == 0) & (stellar_mass > 0) & (mvir > 0)
    central_indices = np.where(central_mask)[0]
    
    central_count = len(central_indices)
    satellite_count = np.sum((galaxy_type == 1) & (stellar_mass > 0))
    excluded_zero_mvir = np.sum((galaxy_type == 0) & (stellar_mass > 0) & (mvir <= 0))
    
    print(f"Central galaxies with M* > 0 AND Mvir > 0: {central_count}")
    print(f"Satellite galaxies excluded: {satellite_count}")
    print(f"Central galaxies excluded due to Mvir <= 0: {excluded_zero_mvir}")
    
    if len(central_indices) > 0:
        print(f"Mass range (centrals): {np.log10(mvir[central_indices].min()):.2f} to {np.log10(mvir[central_indices].max()):.2f}")
    else:
        print("No valid central galaxies found!")
    
    return central_indices

def smart_sampling_strategy_etg_ltg(mvir, stellar_mass, galaxy_type, bulge_mass, cold_gas, target_sample_size=None):
    """
    Filter and separate ETGs and LTGs based on SAGE 2.0 and C16 criteria
    
    For LTGs (disk-dominated/spiral galaxies):
    - Centrals only (Type == 0)
    - Positive total baryonic mass (StellarMass + ColdGas > 0.0)
    - Bulge-to-stellar mass ratio between 0.1 and 0.5
    
    For ETGs (early-type galaxies):
    - All galaxies with B/T > 0.5
    """
    # Calculate bulge-to-stellar mass ratio
    bulge_ratio = np.where(stellar_mass > 0, bulge_mass / stellar_mass, 0.0)
    
    # LTGs (Late Type Galaxies - disk dominated/spiral): SAGE 2.0 and C16 criteria
    ltg_mask = ((galaxy_type == 0) & 
                (stellar_mass + cold_gas > 0.0) & 
                (bulge_ratio > 0.1) & 
                (bulge_ratio < 0.5))
    ltg_indices = np.where(ltg_mask)[0]
    
    # ETGs (Early Type Galaxies - bulge dominated): Traditional criterion
    # Include both centrals and satellites with high bulge fraction
    etg_mask = (stellar_mass > 0) & (bulge_ratio > 0.5)
    etg_indices = np.where(etg_mask)[0]
    
    # Get galaxy types for reporting
    ltg_types = galaxy_type[ltg_indices] if len(ltg_indices) > 0 else np.array([])
    etg_types = galaxy_type[etg_indices] if len(etg_indices) > 0 else np.array([])
    
    print(f"LTGs (SAGE 2.0/C16 criteria - centrals only, 0.1 < B/T < 0.5): {len(ltg_indices)} galaxies")
    if len(ltg_indices) > 0:
        print(f"  - All centrals by definition: {np.sum(ltg_types == 0)}")
        ltg_bulge_ratios = bulge_ratio[ltg_indices]
        print(f"  - B/T range: {ltg_bulge_ratios.min():.3f} to {ltg_bulge_ratios.max():.3f}")
    
    print(f"ETGs (B/T > 0.5): {len(etg_indices)} galaxies")
    if len(etg_indices) > 0:
        print(f"  - Centrals: {np.sum(etg_types == 0)}, Satellites: {np.sum(etg_types == 1)}")
        etg_bulge_ratios = bulge_ratio[etg_indices]
        print(f"  - B/T range: {etg_bulge_ratios.min():.3f} to {etg_bulge_ratios.max():.3f}")
    
    print(f"Total classified galaxies: {len(ltg_indices) + len(etg_indices)}")
    
    return etg_indices, ltg_indices

def calculate_bulge_ratio(bulge_mass, stellar_mass):
    """
    Calculate bulge-to-total stellar mass ratio
    """
    # Avoid division by zero
    bulge_ratio = np.where(stellar_mass > 0, bulge_mass / stellar_mass, 0.0)
    return bulge_ratio

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

def calculate_median_std(log_mvir_data, log_stellar_data, bin_edges, label_prefix=""):
    """Calculate median and standard error in bins"""
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
    medians = []
    std_errors = []  # Changed from std_devs to std_errors
    valid_bins = []
    
    for i in range(len(bin_edges) - 1):
        bin_mask = (log_mvir_data >= bin_edges[i]) & (log_mvir_data < bin_edges[i+1])
        bin_stellar = log_stellar_data[bin_mask]
        
        min_galaxies = 5
        
        if len(bin_stellar) >= min_galaxies:
            median_val = np.median(bin_stellar)
            std_val = np.std(bin_stellar)
            
            # Use standard error of the mean instead of full standard deviation
            if len(bin_stellar) > 1:
                std_error = std_val / np.sqrt(len(bin_stellar))
            else:
                std_error = 0.3  # Default uncertainty for single galaxy bins
            
            # Set minimum error bar to avoid overly small error bars
            if std_error < 0.05:
                std_error = 0.05
            
            medians.append(median_val)
            std_errors.append(std_error)
            valid_bins.append(bin_centers[i])
            
        print(f"{label_prefix}Bin {bin_edges[i]:.1f}-{bin_edges[i+1]:.1f}: {len(bin_stellar)} galaxies")
    
    return np.array(valid_bins), np.array(medians), np.array(std_errors)

def calculate_median_iqr(log_mvir_data, log_stellar_data, bin_edges, label_prefix=""):
    """Calculate median and inter-quartile range in bins - shows data spread rather than uncertainty"""
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
    medians = []
    iqr_errors = []
    valid_bins = []
    
    for i in range(len(bin_edges) - 1):
        bin_mask = (log_mvir_data >= bin_edges[i]) & (log_mvir_data < bin_edges[i+1])
        bin_stellar = log_stellar_data[bin_mask]
        
        min_galaxies = 5  # Need at least 3 for meaningful quartiles
        
        if len(bin_stellar) >= min_galaxies:
            median_val = np.median(bin_stellar)
            q25 = np.percentile(bin_stellar, 25)
            q75 = np.percentile(bin_stellar, 75)
            iqr_error = (q75 - q25) / 2  # Half the IQR for symmetric error bars
            
            # Set minimum error bar
            if iqr_error < 0.05:
                iqr_error = 0.05
            
            medians.append(median_val)
            iqr_errors.append(iqr_error)
            valid_bins.append(bin_centers[i])
            
        print(f"{label_prefix}Bin {bin_edges[i]:.1f}-{bin_edges[i+1]:.1f}: {len(bin_stellar)} galaxies")
    
    return np.array(valid_bins), np.array(medians), np.array(iqr_errors)

def smhm_plot_fixed(compare_old_model=True):
    """Plot the stellar mass-halo mass relation with fixed sampling"""
    plt.figure(figsize=(10, 8))

    # Load SAGE 2.0 data
    print("Loading SAGE 2.0 data...")
    StellarMass = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    Mvir = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h  
    Type = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='Type')
    
    if StellarMass is None or Mvir is None or Type is None:
        print("Failed to load SAGE 2.0 data")
        return
    
    print(f"Loaded {len(StellarMass)} galaxies")
    print(f"Mvir range: {np.min(Mvir[Mvir>0]):.2e} to {np.max(Mvir):.2e} M☉")
    print(f"Max log(Mvir): {np.log10(np.max(Mvir)):.2f}")
    
    # Apply filtering (no sampling - use all galaxies: centrals + satellites)
    filter_indices = smart_sampling_strategy_centrals_only(Mvir, StellarMass, Type)
    
    # Get the filtered data for SAGE 2.0
    log_mvir = np.log10(Mvir[filter_indices])
    log_stellar = np.log10(StellarMass[filter_indices])

    # Plot SAGE 2.0 scatter
    # plt.scatter(log_mvir, log_stellar, s=1, alpha=0.3, color='lightblue')  # Removed label

    # Load and plot older SAGE model if requested
    log_mvir_old = None
    log_stellar_old = None
    
    if compare_old_model and os.path.exists(DirName_old):
        try:
            print(f'Loading older SAGE model from {DirName_old}')
            print(f'Using old model parameters: BoxSize={BoxSize_old} Mpc/h, VolumeFraction={VolumeFraction_old}, h={Hubble_h_old}')
            
            StellarMass_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h_old
            Mvir_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h_old
            Type_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='Type')
            
            if StellarMass_old is not None and Mvir_old is not None and Type_old is not None:
                # Apply filtering to older model too (no sampling)
                filter_old = smart_sampling_strategy_centrals_only(Mvir_old, StellarMass_old, Type_old)

                log_mvir_old = np.log10(Mvir_old[filter_old])
                log_stellar_old = np.log10(StellarMass_old[filter_old])
                
                # plt.scatter(log_mvir_old, log_stellar_old, s=1, alpha=0.2, color='lightcoral')  # Removed label
                
                print(f"Loaded older SAGE data: {len(filter_old)} galaxies")
            else:
                print("Failed to load older SAGE data arrays")
                
        except Exception as e:
            print(f"Could not load older SAGE data: {e}")
    elif compare_old_model:
        print(f"Older SAGE directory {DirName_old} not found")

    # Load observational data (store handles for legend)
    obs_handles = []
    obs_labels = []
    
    try:
        obs_data = np.loadtxt('./data/Romeo20_SMHM.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        log_ms_obs = log_mh_obs + log_ms_mh_obs
        handle = plt.scatter(log_mh_obs, log_ms_obs, s=50, alpha=0.8, 
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
        plt.scatter(log_mh_obs, log_ms_obs, s=50, alpha=0.8, color='white', 
                   marker='*', edgecolors='orange', linewidth=0.5)  # Same style as Romeo+20, no separate legend
    except FileNotFoundError:
        print("Romeo20_SMHM_ETGs.dat file not found")

    try:
        obs_data = np.loadtxt('./data/SatKinsAndClusters_Kravtsov18.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        handle = plt.scatter(log_mh_obs, log_ms_mh_obs, s=50, alpha=0.8,
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
        handle, = plt.plot(log_mh_moster, log_ms_moster, 
                   linestyle='-.', linewidth=1.5, color='blue')
        obs_handles.append(handle)
        obs_labels.append('Moster+13')
    except Exception as e:
        print(f"Error loading Moster13 data: {e}")

    # Add Behroozi+13 model (z=0 for Snap_63)
    behroozi_handle, = plt.plot([], [], 'r', linestyle='dashdot', linewidth=1.5)  # Dummy for legend
    plot_behroozi13(plt.gca(), z=0.0, labels=False)  # Plot without label
    obs_handles.append(behroozi_handle)
    obs_labels.append('Behroozi+13')

    # Calculate median and standard deviation in bins (extended range)
    bin_edges = np.linspace(10.0, 15.0, 17)  # Extended to cover massive haloes
    
    # Store handles for SAGE models
    sage_handles = []
    sage_labels = []

    print(f"Filtered data stats:")
    print(f"  Number of centrals: {len(filter_indices)}")
    print(f"  Mvir range: {Mvir[filter_indices].min():.2e} to {Mvir[filter_indices].max():.2e}")
    print(f"  Log10(Mvir) range: {np.log10(Mvir[filter_indices]).min():.2f} to {np.log10(Mvir[filter_indices]).max():.2f}")
    print(f"  Stellar mass range: {StellarMass[filter_indices].min():.2e} to {StellarMass[filter_indices].max():.2e}")

    # Check for any potential issues
    invalid_mvir = np.sum(Mvir[filter_indices] <= 0)
    invalid_stellar = np.sum(StellarMass[filter_indices] <= 0)
    if invalid_mvir > 0:
        print(f"WARNING: {invalid_mvir} galaxies with Mvir <= 0")
    if invalid_stellar > 0:
        print(f"WARNING: {invalid_stellar} galaxies with stellar mass <= 0")
    
    # SAGE 2.0 statistics (all galaxies)
    valid_bins, medians, std_errors = calculate_median_std(log_mvir, log_stellar, bin_edges, "SAGE 2.0 (all) ")
    
    # # Plot SAGE 2.0 median with error bars
    sage2_handle = plt.errorbar(valid_bins, medians, yerr=std_errors, 
                fmt='k-', linewidth=2, capsize=3, capthick=1.5, zorder=10)
    sage_handles.append(sage2_handle)
    sage_labels.append('SAGE 2.0')
    
    # Older SAGE statistics and plotting (if data available)
    if log_mvir_old is not None:
        valid_bins_old, medians_old, std_errors_old = calculate_median_std(
            log_mvir_old, log_stellar_old, bin_edges, "SAGE C16 (all) ")
        
        sage_old_handle, = plt.plot(valid_bins_old, medians_old, 
                    linestyle='--', color='darkred', linewidth=2, zorder=9)
        sage_handles.append(sage_old_handle)
        sage_labels.append('SAGE C16')

    # Add labels and title
    plt.xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$')
    plt.ylabel(r'$\log_{10}(M_*/M_\odot)$')
    plt.xlim(10.0, 15.0)  # Extended range to show massive haloes
    plt.ylim(8.0, 12.0)
    
    # Create two separate legends
    # Upper left: SAGE models
    if sage_handles:
        legend1 = plt.legend(sage_handles, sage_labels, loc='upper left', fontsize=12, frameon=False)
        plt.gca().add_artist(legend1)  # Add first legend to axes
    
    # Lower right: Observations and theory
    if obs_handles:
        legend2 = plt.legend(obs_handles, obs_labels, loc='lower right', fontsize=12, frameon=False)
    
    # Add text showing the volume differences if different
    if compare_old_model and os.path.exists(DirName_old):
        volume_new = (BoxSize/Hubble_h)**3.0 * VolumeFraction
        volume_old = (BoxSize_old/Hubble_h_old)**3.0 * VolumeFraction_old
        if abs(volume_new - volume_old) > 0.01:  # Only show if significantly different
            plt.text(0.05, 0.85, f'New: ({BoxSize:.1f}/h)³×{VolumeFraction:.1f}\nOld: ({BoxSize_old:.1f}/h)³×{VolumeFraction_old:.1f}', 
                     transform=plt.gca().transAxes, fontsize=9, 
                     bbox=dict(boxstyle="round,pad=0.3", facecolor="white", alpha=0.8))
    
    # Save the plot
    plt.tight_layout()
    plt.savefig(OutputDir + 'smhm_relation_fixed' + OutputFormat)
    plt.close()

def smhm_plot_etg_ltg(compare_old_model=True):
    """Plot the stellar mass-halo mass relation separated by ETGs and LTGs"""
    plt.figure(figsize=(10, 8))

    # Load SAGE 2.0 data
    print("\nLoading SAGE 2.0 data for ETG/LTG analysis...")
    StellarMass = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    Mvir = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h  
    Type = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='Type')
    
    # Load bulge mass and cold gas for SAGE 2.0/C16 criteria
    BulgeMass = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='BulgeMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
    
    if any(x is None for x in [StellarMass, Mvir, Type, BulgeMass, ColdGas]):
        print("Failed to load SAGE 2.0 data")
        return
    
    print(f"Loaded {len(StellarMass)} galaxies")
    
    # Apply filtering for ETGs and LTGs using SAGE 2.0/C16 criteria
    etg_indices, ltg_indices = smart_sampling_strategy_etg_ltg(
        Mvir, StellarMass, Type, BulgeMass, ColdGas)
    
    # Get the filtered data for SAGE 2.0 ETGs
    if len(etg_indices) > 0:
        log_mvir_etg = np.log10(Mvir[etg_indices])
        log_stellar_etg = np.log10(StellarMass[etg_indices])
    else:
        log_mvir_etg = np.array([])
        log_stellar_etg = np.array([])
    
    # Get the filtered data for SAGE 2.0 LTGs
    if len(ltg_indices) > 0:
        log_mvir_ltg = np.log10(Mvir[ltg_indices])
        log_stellar_ltg = np.log10(StellarMass[ltg_indices])
    else:
        log_mvir_ltg = np.array([])
        log_stellar_ltg = np.array([])

    # Load and process older SAGE model if requested
    log_mvir_etg_old = None
    log_stellar_etg_old = None
    log_mvir_ltg_old = None
    log_stellar_ltg_old = None
    
    if compare_old_model and os.path.exists(DirName_old):
        try:
            print(f'Loading older SAGE model from {DirName_old}')
            print(f'Using old model parameters: BoxSize={BoxSize_old} Mpc/h, VolumeFraction={VolumeFraction_old}, h={Hubble_h_old}')
            
            StellarMass_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h_old
            Mvir_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h_old
            Type_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='Type')
            BulgeMass_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='BulgeMass') * 1.0e10 / Hubble_h_old
            ColdGas_old = read_hdf_smart_sampling(DirName_old, snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h_old
            
            if all(x is not None for x in [StellarMass_old, Mvir_old, Type_old, BulgeMass_old, ColdGas_old]):
                # Apply filtering to older model using SAGE 2.0/C16 criteria
                etg_indices_old, ltg_indices_old = smart_sampling_strategy_etg_ltg(
                    Mvir_old, StellarMass_old, Type_old, BulgeMass_old, ColdGas_old)
                
                if len(etg_indices_old) > 0:
                    log_mvir_etg_old = np.log10(Mvir_old[etg_indices_old])
                    log_stellar_etg_old = np.log10(StellarMass_old[etg_indices_old])
                
                if len(ltg_indices_old) > 0:
                    log_mvir_ltg_old = np.log10(Mvir_old[ltg_indices_old])
                    log_stellar_ltg_old = np.log10(StellarMass_old[ltg_indices_old])
                
                print(f"Loaded older SAGE data with ETG/LTG separation")
            else:
                print("Failed to load older SAGE data arrays")
                
        except Exception as e:
            print(f"Could not load older SAGE data: {e}")
    elif compare_old_model:
        print(f"Older SAGE directory {DirName_old} not found")

    # Load observational data for ETGs and LTGs
    obs_handles = []
    obs_labels = []
    
    # Load Correa+19 ETGs
    try:
        try:
            correa_etg_data = np.loadtxt('./data/ETGs_Correa19.dat', comments='#')
        except FileNotFoundError:
            correa_etg_data = np.loadtxt('./ETGs_Correa19.dat', comments='#')
        
        log_mh_correa_etg = correa_etg_data[:, 0]  # log10(M200)
        log_ms_correa_etg = correa_etg_data[:, 1]  # log10(Mstar)
        handle = plt.scatter(log_mh_correa_etg, log_ms_correa_etg, s=50, alpha=0.8, 
                   color='red', marker='s', edgecolors='darkred', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Correa+19 ETGs')
        print(f"Loaded Correa+19 ETGs: {len(log_mh_correa_etg)} galaxies")
    except FileNotFoundError:
        print("ETGs_Correa19.dat file not found in ./data/ or current directory")
    
    # Load Correa+19 LTGs
    try:
        try:
            correa_ltg_data = np.loadtxt('./data/LTGs_Correa19.dat', comments='#')
        except FileNotFoundError:
            correa_ltg_data = np.loadtxt('./LTGs_Correa19.dat', comments='#')
        
        log_mh_correa_ltg = correa_ltg_data[:, 0]  # log10(M200)
        log_ms_correa_ltg = correa_ltg_data[:, 1]  # log10(Mstar)
        handle = plt.scatter(log_mh_correa_ltg, log_ms_correa_ltg, s=50, alpha=0.8, 
                   color='blue', marker='s', edgecolors='darkblue', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Correa+19 LTGs')
        print(f"Loaded Correa+19 LTGs: {len(log_mh_correa_ltg)} galaxies")
    except FileNotFoundError:
        print("LTGs_Correa19.dat file not found in ./data/ or current directory")
    
    # Load Kravtsov+18 ETGs
    try:
        try:
            kravtsov_etg_data = np.loadtxt('./data/ETGs_Kravtsov18.dat', comments='#')
        except FileNotFoundError:
            kravtsov_etg_data = np.loadtxt('./ETGs_Kravtsov18.dat', comments='#')
        
        log_mh_kravtsov_etg = kravtsov_etg_data[:, 0]  # log10(Mhalo)
        log_ms_kravtsov_etg = kravtsov_etg_data[:, 1]  # log10(Mcen)
        handle = plt.scatter(log_mh_kravtsov_etg, log_ms_kravtsov_etg, s=60, alpha=0.8, 
                   color='red', marker='^', edgecolors='darkred', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Kravtsov+18 ETGs')
        print(f"Loaded Kravtsov+18 ETGs: {len(log_mh_kravtsov_etg)} galaxies")
    except FileNotFoundError:
        print("ETGs_Kravtsov18.dat file not found in ./data/ or current directory")
    
    # Load Kravtsov+18 LTGs
    try:
        try:
            kravtsov_ltg_data = np.loadtxt('./data/LTGs_Kravtsov18.dat', comments='#')
        except FileNotFoundError:
            kravtsov_ltg_data = np.loadtxt('./LTGs_Kravtsov18.dat', comments='#')
        
        log_mh_kravtsov_ltg = kravtsov_ltg_data[:, 0]  # log10(Mhalo)
        log_ms_kravtsov_ltg = kravtsov_ltg_data[:, 1]  # log10(Mcen)
        handle = plt.scatter(log_mh_kravtsov_ltg, log_ms_kravtsov_ltg, s=60, alpha=0.8, 
                   color='blue', marker='^', edgecolors='darkblue', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Kravtsov+18 LTGs')
        print(f"Loaded Kravtsov+18 LTGs: {len(log_mh_kravtsov_ltg)} galaxies")
    except FileNotFoundError:
        print("LTGs_Kravtsov18.dat file not found in ./data/ or current directory")

    # Calculate median and standard deviation in bins
    bin_edges = np.linspace(10.0, 16.0, 15)
    
    # Store handles for legend
    handles = []
    labels = []
    
    # Plot SAGE 2.0 ETGs
    if len(log_mvir_etg) > 0:
        valid_bins_etg, medians_etg, std_errors_etg = calculate_median_std(
            log_mvir_etg, log_stellar_etg, bin_edges, "SAGE 2.0 ETGs (all) ")
        
        if len(valid_bins_etg) > 0:
            handle_etg = plt.errorbar(valid_bins_etg, medians_etg, yerr=std_errors_etg, 
                        fmt='r-', linewidth=2, capsize=3, capthick=1.5, zorder=10, color='red')
            handles.append(handle_etg)
            labels.append('SAGE 2.0 ETGs')
    
    # Plot SAGE 2.0 LTGs
    if len(log_mvir_ltg) > 0:
        valid_bins_ltg, medians_ltg, std_errors_ltg = calculate_median_std(
            log_mvir_ltg, log_stellar_ltg, bin_edges, "SAGE 2.0 LTGs (all) ")
        
        if len(valid_bins_ltg) > 0:
            handle_ltg = plt.errorbar(valid_bins_ltg, medians_ltg, yerr=std_errors_ltg, 
                        fmt='b-', linewidth=2, capsize=3, capthick=1.5, zorder=10, color='blue')
            handles.append(handle_ltg)
            labels.append('SAGE 2.0 LTGs')
    
    # Plot older SAGE ETGs
    if log_mvir_etg_old is not None and len(log_mvir_etg_old) > 0:
        valid_bins_etg_old, medians_etg_old, std_errors_etg_old = calculate_median_std(
            log_mvir_etg_old, log_stellar_etg_old, bin_edges, "SAGE C16 ETGs (all) ")
        
        if len(valid_bins_etg_old) > 0:
            handle_etg_old, = plt.plot(valid_bins_etg_old, medians_etg_old, 
                        linestyle='--', color='lightcoral', linewidth=2, zorder=9)
            handles.append(handle_etg_old)
            labels.append('SAGE C16 ETGs')
    
    # Plot older SAGE LTGs
    if log_mvir_ltg_old is not None and len(log_mvir_ltg_old) > 0:
        valid_bins_ltg_old, medians_ltg_old, std_errors_ltg_old = calculate_median_std(
            log_mvir_ltg_old, log_stellar_ltg_old, bin_edges, "SAGE C16 LTGs (all) ")
        
        if len(valid_bins_ltg_old) > 0:
            handle_ltg_old, = plt.plot(valid_bins_ltg_old, medians_ltg_old, 
                        linestyle='--', color='lightblue', linewidth=2, zorder=9)
            handles.append(handle_ltg_old)
            labels.append('SAGE C16 LTGs')

    # Add labels and title
    plt.xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$')
    plt.ylabel(r'$\log_{10}(M_*/M_\odot)$')
    plt.xlim(10.0, 15.0)
    plt.ylim(8.0, 12.0)

    # Create two separate legends
    # Upper left: SAGE models
    if handles:
        legend1 = plt.legend(handles, labels, loc='upper left', fontsize=12, frameon=False)
        plt.gca().add_artist(legend1)  # Add first legend to axes
    
    # Lower right: Observations
    if obs_handles:
        legend2 = plt.legend(obs_handles, obs_labels, loc='lower right', fontsize=10, frameon=False)
    
    # Add text showing the volume differences if different
    if compare_old_model and os.path.exists(DirName_old):
        volume_new = (BoxSize/Hubble_h)**3.0 * VolumeFraction
        volume_old = (BoxSize_old/Hubble_h_old)**3.0 * VolumeFraction_old
        if abs(volume_new - volume_old) > 0.01:  # Only show if significantly different
            plt.text(0.05, 0.85, f'New: ({BoxSize:.1f}/h)³×{VolumeFraction:.1f}\nOld: ({BoxSize_old:.1f}/h)³×{VolumeFraction_old:.1f}', 
                     transform=plt.gca().transAxes, fontsize=9, 
                     bbox=dict(boxstyle="round,pad=0.3", facecolor="white", alpha=0.8))
    
    # Save the plot
    plt.tight_layout()
    plt.savefig(OutputDir + 'smhm_relation_etg_ltg' + OutputFormat, bbox_inches='tight')
    plt.close()

def smhm_plot_sf_passive_redshift_grid(compare_old_model=True):
    """Plot a 2x2 grid of SMHM relations at different redshifts"""
    
    # Define redshifts and their corresponding snapshots
    redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                 0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    # Target redshifts and find closest snapshots
    target_redshifts = [1.0, 2.0, 3.0, 4.0]
    target_snapshots = []
    actual_redshifts = []
    
    for target_z in target_redshifts:
        # Find closest redshift in the list
        closest_idx = np.argmin(np.abs(np.array(redshifts) - target_z))
        target_snapshots.append(closest_idx)
        actual_redshifts.append(redshifts[closest_idx])
        print(f"Target z={target_z:.1f} -> Snap_{closest_idx} (z={redshifts[closest_idx]:.3f})")
    
    # Create 2x2 subplot grid
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    axes = axes.flatten()
    
    # NOTE: No longer sampling - using all galaxies for better statistics
    
    for i, (snap_idx, actual_z) in enumerate(zip(target_snapshots, actual_redshifts)):
        ax = axes[i]
        snap_name = f'Snap_{snap_idx}'
        
        print(f"\nProcessing {snap_name} (z={actual_z:.3f})...")
        
        # Load SAGE 2.0 data for this snapshot
        StellarMass = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='StellarMass') * 1.0e10 / Hubble_h
        Mvir = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='Mvir') * 1.0e10 / Hubble_h  
        Type = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='Type')
        SfrDisk = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='SfrDisk')
        SfrBulge = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='SfrBulge')
        
        if any(x is None for x in [StellarMass, Mvir, Type, SfrDisk, SfrBulge]):
            print(f"Failed to load SAGE 2.0 data for {snap_name}")
            ax.text(0.5, 0.5, f'No data for z={actual_z:.1f}', 
                   transform=ax.transAxes, ha='center', va='center', fontsize=14)
            continue
        
        # Calculate sSFR
        stellar_mass_nonzero = np.where(StellarMass > 0, StellarMass, 1e-10)
        sSFR = np.log10((SfrDisk + SfrBulge) / stellar_mass_nonzero)
        
        # Apply filtering (no sampling)
        sf_indices, passive_indices = smart_sampling_strategy_sf_passive(
            Mvir, StellarMass, Type, sSFR, sSFRcut)
        
        # Get SF data
        if len(sf_indices) > 0:
            log_mvir_sf = np.log10(Mvir[sf_indices])
            log_stellar_sf = np.log10(StellarMass[sf_indices])
        else:
            log_mvir_sf = np.array([])
            log_stellar_sf = np.array([])
        
        # Get passive data
        if len(passive_indices) > 0:
            log_mvir_passive = np.log10(Mvir[passive_indices])
            log_stellar_passive = np.log10(StellarMass[passive_indices])
        else:
            log_mvir_passive = np.array([])
            log_stellar_passive = np.array([])
        
        # Load older SAGE model if requested
        log_mvir_sf_old = None
        log_stellar_sf_old = None
        log_mvir_passive_old = None
        log_stellar_passive_old = None
        
        if compare_old_model and os.path.exists(DirName_old):
            try:
                StellarMass_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='StellarMass') * 1.0e10 / Hubble_h_old
                Mvir_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='Mvir') * 1.0e10 / Hubble_h_old
                Type_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='Type')
                SfrDisk_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='SfrDisk')
                SfrBulge_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='SfrBulge')
                
                if all(x is not None for x in [StellarMass_old, Mvir_old, Type_old, SfrDisk_old, SfrBulge_old]):
                    stellar_mass_nonzero_old = np.where(StellarMass_old > 0, StellarMass_old, 1e-10)
                    sSFR_old = np.log10((SfrDisk_old + SfrBulge_old) / stellar_mass_nonzero_old)
                    
                    sf_indices_old, passive_indices_old = smart_sampling_strategy_sf_passive(
                        Mvir_old, StellarMass_old, Type_old, sSFR_old, sSFRcut)
                    
                    if len(sf_indices_old) > 0:
                        log_mvir_sf_old = np.log10(Mvir_old[sf_indices_old])
                        log_stellar_sf_old = np.log10(StellarMass_old[sf_indices_old])
                    
                    if len(passive_indices_old) > 0:
                        log_mvir_passive_old = np.log10(Mvir_old[passive_indices_old])
                        log_stellar_passive_old = np.log10(StellarMass_old[passive_indices_old])
                        
            except Exception as e:
                print(f"Could not load older SAGE data for {snap_name}: {e}")
        
        # Calculate medians in bins (smaller range for higher redshifts due to fewer massive haloes)
        if actual_z >= 3.0:
            bin_edges = np.linspace(10.0, 15.0, 15)  # Reduced range for high-z
        else:
            bin_edges = np.linspace(10.0, 15.0, 15)
        
        # Plot SAGE 2.0 star forming galaxies
        if len(log_mvir_sf) > 0:
            valid_bins_sf, medians_sf, std_errors_sf = calculate_median_std(
                log_mvir_sf, log_stellar_sf, bin_edges, f"z={actual_z:.1f} SAGE 2.0 SF ")
            
            if len(valid_bins_sf) > 0:
                ax.errorbar(valid_bins_sf, medians_sf, yerr=std_errors_sf, 
                           fmt='b-', linewidth=2, capsize=2, capthick=1, zorder=10, color='blue')
        
        # Plot SAGE 2.0 passive galaxies
        if len(log_mvir_passive) > 0:
            valid_bins_passive, medians_passive, std_errors_passive = calculate_median_std(
                log_mvir_passive, log_stellar_passive, bin_edges, f"z={actual_z:.1f} SAGE 2.0 Passive ")
            
            if len(valid_bins_passive) > 0:
                ax.errorbar(valid_bins_passive, medians_passive, yerr=std_errors_passive, 
                           fmt='r-', linewidth=2, capsize=2, capthick=1, zorder=10, color='red')
        
        # Plot older SAGE star forming galaxies
        if log_mvir_sf_old is not None and len(log_mvir_sf_old) > 0:
            valid_bins_sf_old, medians_sf_old, std_errors_sf_old = calculate_median_std(
                log_mvir_sf_old, log_stellar_sf_old, bin_edges, f"z={actual_z:.1f} SAGE C16 SF ")
            
            if len(valid_bins_sf_old) > 0:
                ax.plot(valid_bins_sf_old, medians_sf_old, 
                       linestyle='--', color='lightblue', linewidth=1.5, zorder=9)
        
        # Plot older SAGE passive galaxies
        if log_mvir_passive_old is not None and len(log_mvir_passive_old) > 0:
            valid_bins_passive_old, medians_passive_old, std_errors_passive_old = calculate_median_std(
                log_mvir_passive_old, log_stellar_passive_old, bin_edges, f"z={actual_z:.1f} SAGE C16 Passive ")
            
            if len(valid_bins_passive_old) > 0:
                ax.plot(valid_bins_passive_old, medians_passive_old, 
                       linestyle='--', color='lightcoral', linewidth=1.5, zorder=9)
        
        # Subplot formatting
        ax.set_xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$')
        ax.set_ylabel(r'$\log_{10}(M_*/M_\odot)$')
        if actual_z >= 3.0:
            ax.set_xlim(10.0, 15.0)
            ax.set_ylim(8.0, 12.0)
        else:
            ax.set_xlim(10.0, 15.0)
            ax.set_ylim(8.0, 12.0)
        ax.set_title(f'z = {actual_z:.1f}', fontsize=14)
    
    # Add overall title
    fig.suptitle(f'SMHM Relation: Star Forming vs Passive Galaxies (sSFR cut = {sSFRcut})', fontsize=16)
    
    # Create a single legend for the entire figure
    handles = []
    labels = []
    
    # Add dummy handles for legend
    handles.append(plt.Line2D([0], [0], color='blue', linewidth=2, label='SAGE 2.0 Star Forming'))
    handles.append(plt.Line2D([0], [0], color='red', linewidth=2, label='SAGE 2.0 Passive'))
    if compare_old_model:
        handles.append(plt.Line2D([0], [0], color='lightblue', linewidth=1.5, linestyle='--', label='SAGE C16 Star Forming'))
        handles.append(plt.Line2D([0], [0], color='lightcoral', linewidth=1.5, linestyle='--', label='SAGE C16 Passive'))
    
    # Place legend outside the plot area
    fig.legend(handles=handles, loc='center', bbox_to_anchor=(0.5, 0.02), ncol=4, fontsize=12, frameon=False)
    
    # Adjust layout to make room for legend
    plt.tight_layout()
    plt.subplots_adjust(bottom=0.12)
    
    # Save the plot
    plt.savefig(OutputDir + 'smhm_relation_sf_passive_redshift_grid' + OutputFormat, bbox_inches='tight')
    plt.close()

def sfms_plot_three_populations_redshift_grid(compare_old_model=True):
    """Plot a 2x2 grid of star forming main sequence at different redshifts with three populations"""
    
    # Define redshifts and their corresponding snapshots (same as existing function)
    redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                 0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    # Target redshifts and find closest snapshots
    target_redshifts = [1.0, 2.0, 3.0, 4.0]
    target_snapshots = []
    actual_redshifts = []
    
    for target_z in target_redshifts:
        closest_idx = np.argmin(np.abs(np.array(redshifts) - target_z))
        target_snapshots.append(closest_idx)
        actual_redshifts.append(redshifts[closest_idx])
        print(f"Target z={target_z:.1f} -> Snap_{closest_idx} (z={redshifts[closest_idx]:.3f})")
    
    # Create 2x2 subplot grid
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    axes = axes.flatten()
    
    # NOTE: No longer sampling - using all galaxies for better statistics
    
    for i, (snap_idx, actual_z) in enumerate(zip(target_snapshots, actual_redshifts)):
        ax = axes[i]
        snap_name = f'Snap_{snap_idx}'
        
        print(f"\nProcessing {snap_name} (z={actual_z:.3f}) for SFMS...")
        
        # Load SAGE 2.0 data for this snapshot
        StellarMass = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='StellarMass') * 1.0e10 / Hubble_h
        Mvir = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='Mvir') * 1.0e10 / Hubble_h  
        Type = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='Type')
        SfrDisk = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='SfrDisk')
        SfrBulge = read_hdf_smart_sampling(DirName, snap_num=snap_name, param='SfrBulge')
        
        if any(x is None for x in [StellarMass, Mvir, Type, SfrDisk, SfrBulge]):
            print(f"Failed to load SAGE 2.0 data for {snap_name}")
            ax.text(0.5, 0.5, f'No data for z={actual_z:.1f}', 
                   transform=ax.transAxes, ha='center', va='center', fontsize=14)
            continue
        
        # Filter for galaxies with positive stellar mass (include all types: centrals + satellites)
        mass_mask = StellarMass > 0
        stellar_mass_all = StellarMass[mass_mask]
        sfr_disk_all = SfrDisk[mass_mask]
        sfr_bulge_all = SfrBulge[mass_mask]
        all_indices = np.where(mass_mask)[0]
        
        # Calculate total SFR and sSFR
        sfr_total = sfr_disk_all + sfr_bulge_all
        
        # Use ALL galaxies - no SFR threshold filtering
        stellar_mass_det = stellar_mass_all
        sfr_total_det = sfr_total
        det_indices = all_indices
        
        # For sSFR calculation, avoid log(0) by setting minimum SFR
        sfr_for_ssfr = np.where(sfr_total_det > 0, sfr_total_det, 1e-10)
        ssfr_det = np.log10(sfr_for_ssfr / stellar_mass_det)
        
        # Define classification based on sSFR (adjust these cuts as needed)
        ssfr_sf_cut = -10.5      # Above this: star forming
        ssfr_quiescent_cut = -11.5  # Below this: quiescent
        # Between these two: green valley
        
        sf_mask = ssfr_det > ssfr_sf_cut
        quiescent_mask = ssfr_det < ssfr_quiescent_cut
        green_valley_mask = (ssfr_det >= ssfr_quiescent_cut) & (ssfr_det <= ssfr_sf_cut)
        
        print(f"  Star forming: {np.sum(sf_mask)} galaxies (centrals + satellites, all SFR)")
        print(f"  Green valley: {np.sum(green_valley_mask)} galaxies (centrals + satellites, all SFR)") 
        print(f"  Quiescent: {np.sum(quiescent_mask)} galaxies (centrals + satellites, all SFR)")
        
        # Get indices for each population
        sf_plot_indices = np.where(sf_mask)[0]
        gv_plot_indices = np.where(green_valley_mask)[0]
        q_plot_indices = np.where(quiescent_mask)[0]
        
        # Apply dilution to scatter plots using global dilute variable
        if len(sf_plot_indices) > dilute:
            sf_sample_indices = sample(list(sf_plot_indices), dilute)
            print(f"  Star forming: diluted from {len(sf_plot_indices)} to {len(sf_sample_indices)} points")
        else:
            sf_sample_indices = sf_plot_indices
            print(f"  Star forming: using all {len(sf_sample_indices)} points (no dilution needed)")
            
        if len(gv_plot_indices) > dilute:
            gv_sample_indices = sample(list(gv_plot_indices), dilute)
            print(f"  Green valley: diluted from {len(gv_plot_indices)} to {len(gv_sample_indices)} points")
        else:
            gv_sample_indices = gv_plot_indices
            print(f"  Green valley: using all {len(gv_sample_indices)} points (no dilution needed)")
            
        if len(q_plot_indices) > dilute:
            q_sample_indices = sample(list(q_plot_indices), dilute)
            print(f"  Quiescent: diluted from {len(q_plot_indices)} to {len(q_sample_indices)} points")
        else:
            q_sample_indices = q_plot_indices
            print(f"  Quiescent: using all {len(q_sample_indices)} points (no dilution needed)")
        
        # Prepare data for plotting (handle zero SFR by using minimum value)
        if len(sf_sample_indices) > 0:
            log_stellar_sf = np.log10(stellar_mass_det[sf_sample_indices])
            log_sfr_sf = np.log10(np.where(sfr_total_det[sf_sample_indices] > 0, sfr_total_det[sf_sample_indices], 1e-10))
            ax.scatter(log_stellar_sf, log_sfr_sf, s=1.5, alpha=0.6, color='blue', 
                      label='Star Forming' if i == 0 else '', rasterized=True)
        
        if len(gv_sample_indices) > 0:
            log_stellar_gv = np.log10(stellar_mass_det[gv_sample_indices])
            log_sfr_gv = np.log10(np.where(sfr_total_det[gv_sample_indices] > 0, sfr_total_det[gv_sample_indices], 1e-10))
            ax.scatter(log_stellar_gv, log_sfr_gv, s=1.5, alpha=0.7, color='green', 
                      label='Green Valley' if i == 0 else '', rasterized=True)
        
        if len(q_sample_indices) > 0:
            log_stellar_q = np.log10(stellar_mass_det[q_sample_indices])
            log_sfr_q = np.log10(np.where(sfr_total_det[q_sample_indices] > 0, sfr_total_det[q_sample_indices], 1e-10))
            ax.scatter(log_stellar_q, log_sfr_q, s=1.5, alpha=0.7, color='red', 
                      label='Quiescent' if i == 0 else '', rasterized=True)
        
        # Plot observational star forming main sequence relation
        # Based on Speagle+14, Whitaker+12, and other observations
        mass_range = np.linspace(8.0, 12.0, 100)
        
        # Redshift-dependent SFMS relation (corrected normalizations)
        if actual_z <= 1.0:
            # z~0 relation (Whitaker+12/Brinchmann+04 type)
            sfms_relation = 0.76 * mass_range - 7.6
        elif actual_z <= 2.0:
            # z~1-2 relation (Whitaker+12)
            sfms_relation = 0.80 * mass_range - 7.8
        elif actual_z <= 3.0:
            # z~2-3 relation
            sfms_relation = 0.84 * mass_range - 8.0
        else:
            # z>3 relation (higher normalization)
            sfms_relation = 0.88 * mass_range - 8.2
        
        ax.plot(mass_range, sfms_relation, 'k--', linewidth=2, alpha=0.8, 
               label='SFMS' if i == 0 else '')
        
        # Also plot ±0.3 dex scatter around the main sequence
        ax.fill_between(mass_range, sfms_relation - 0.3, sfms_relation + 0.3, 
                       color='gray', alpha=0.2)
        
        # Load and plot older SAGE model if requested
        if compare_old_model and os.path.exists(DirName_old):
            try:
                StellarMass_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='StellarMass') * 1.0e10 / Hubble_h_old
                Type_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='Type')
                SfrDisk_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='SfrDisk')
                SfrBulge_old = read_hdf_smart_sampling(DirName_old, snap_num=snap_name, param='SfrBulge')
                
                if all(x is not None for x in [StellarMass_old, Type_old, SfrDisk_old, SfrBulge_old]):
                    # Process older model data (include all galaxy types: centrals + satellites)
                    mass_mask_old = StellarMass_old > 0
                    stellar_mass_old = StellarMass_old[mass_mask_old]
                    sfr_total_old = (SfrDisk_old + SfrBulge_old)[mass_mask_old]
                    
                    # Use ALL galaxies - no SFR threshold filtering
                    if len(stellar_mass_old) > 100:  # Only plot if we have enough data
                        # Apply dilution to older model data as well
                        if len(stellar_mass_old) > dilute:
                            old_sample_indices = sample(list(range(len(stellar_mass_old))), dilute)
                            stellar_mass_old_sample = stellar_mass_old[old_sample_indices]
                            sfr_total_old_sample = sfr_total_old[old_sample_indices]
                            print(f"  SAGE C16: diluted from {len(stellar_mass_old)} to {len(stellar_mass_old_sample)} points")
                        else:
                            stellar_mass_old_sample = stellar_mass_old
                            sfr_total_old_sample = sfr_total_old
                            print(f"  SAGE C16: using all {len(stellar_mass_old_sample)} points (no dilution needed)")
                            
                        ax.scatter(np.log10(stellar_mass_old_sample), np.log10(np.where(sfr_total_old_sample > 0, sfr_total_old_sample, 1e-10)), 
                                 s=0.8, alpha=0.3, color='orange', 
                                 label='SAGE C16' if i == 0 else '', rasterized=True)
                        
            except Exception as e:
                print(f"Could not load older SAGE data for {snap_name}: {e}")
        
        # Subplot formatting
        ax.set_xlabel(r'$\log_{10}(M_*/M_\odot)$')
        ax.set_ylabel(r'$\log_{10}({\rm SFR}/M_\odot \, {\rm yr}^{-1})$')
        ax.set_xlim(8.0, 12.0)
        ax.set_ylim(-10.0, 2.5)  # Extended to include very low SFR values
        ax.set_title(f'z = {actual_z:.1f}', fontsize=14)
        
        # Add legend only to first subplot
        if i == 0:
            ax.legend(loc='lower right', fontsize=9, frameon=False, markerscale=3)
    
    # Adjust layout
    plt.tight_layout()
    
    # Save the plot
    plt.savefig(OutputDir + 'sfms_three_populations_redshift_grid' + OutputFormat, bbox_inches='tight', dpi=300)
    plt.close()

def sfms_plot_three_populations_single_redshift(snapshot='Snap_63', compare_old_model=True):
    """Plot star forming main sequence for a single redshift with three populations"""
    
    print(f"\nCreating SFMS plot for {snapshot}...")
    
    plt.figure(figsize=(12, 9))
    
    # Load SAGE 2.0 data
    StellarMass = read_hdf_smart_sampling(DirName, snap_num=snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    Mvir = read_hdf_smart_sampling(DirName, snap_num=snapshot, param='Mvir') * 1.0e10 / Hubble_h  
    Type = read_hdf_smart_sampling(DirName, snap_num=snapshot, param='Type')
    SfrDisk = read_hdf_smart_sampling(DirName, snap_num=snapshot, param='SfrDisk')
    SfrBulge = read_hdf_smart_sampling(DirName, snap_num=snapshot, param='SfrBulge')
    
    if any(x is None for x in [StellarMass, Mvir, Type, SfrDisk, SfrBulge]):
        print(f"Failed to load SAGE 2.0 data for {snapshot}")
        return
    
    # Filter for galaxies with positive stellar mass (include all types: centrals + satellites)
    mass_mask = StellarMass > 0
    stellar_mass_all = StellarMass[mass_mask]
    sfr_disk_all = SfrDisk[mass_mask]
    sfr_bulge_all = SfrBulge[mass_mask]
    
    # Calculate total SFR
    sfr_total = sfr_disk_all + sfr_bulge_all
    
    # Use ALL galaxies - no SFR threshold filtering
    stellar_mass_det = stellar_mass_all
    sfr_total_det = sfr_total
    
    # For sSFR calculation, avoid log(0) by setting minimum SFR
    sfr_for_ssfr = np.where(sfr_total_det > 0, sfr_total_det, 1e-10)
    ssfr_det = np.log10(sfr_for_ssfr / stellar_mass_det)
    
    # Define classification
    ssfr_sf_cut = -10.5
    ssfr_quiescent_cut = -11.5
    
    sf_mask = ssfr_det > ssfr_sf_cut
    quiescent_mask = ssfr_det < ssfr_quiescent_cut
    green_valley_mask = (ssfr_det >= ssfr_quiescent_cut) & (ssfr_det <= ssfr_sf_cut)
    
    print(f"Star forming: {np.sum(sf_mask)} galaxies (centrals + satellites, all SFR)")
    print(f"Green valley: {np.sum(green_valley_mask)} galaxies (centrals + satellites, all SFR)")
    print(f"Quiescent: {np.sum(quiescent_mask)} galaxies (centrals + satellites, all SFR)")
    
    # Get indices for each population and apply dilution using global dilute variable
    sf_indices = np.where(sf_mask)[0]
    gv_indices = np.where(green_valley_mask)[0]
    q_indices = np.where(quiescent_mask)[0]
    
    # Apply dilution to scatter plots using global dilute variable
    if len(sf_indices) > dilute:
        sf_sample_indices = sample(list(sf_indices), dilute)
        print(f"Star forming: diluted from {len(sf_indices)} to {len(sf_sample_indices)} points")
    else:
        sf_sample_indices = sf_indices
        print(f"Star forming: using all {len(sf_sample_indices)} points (no dilution needed)")
        
    if len(gv_indices) > dilute:
        gv_sample_indices = sample(list(gv_indices), dilute)
        print(f"Green valley: diluted from {len(gv_indices)} to {len(gv_sample_indices)} points")
    else:
        gv_sample_indices = gv_indices
        print(f"Green valley: using all {len(gv_sample_indices)} points (no dilution needed)")
        
    if len(q_indices) > dilute:
        q_sample_indices = sample(list(q_indices), dilute)
        print(f"Quiescent: diluted from {len(q_indices)} to {len(q_sample_indices)} points")
    else:
        q_sample_indices = q_indices
        print(f"Quiescent: using all {len(q_sample_indices)} points (no dilution needed)")
    
    # Plot each population (handle zero SFR by using minimum value)
    if len(sf_sample_indices) > 0:
        plt.scatter(np.log10(stellar_mass_det[sf_sample_indices]), 
                   np.log10(np.where(sfr_total_det[sf_sample_indices] > 0, sfr_total_det[sf_sample_indices], 1e-10)), 
                   s=2, alpha=0.6, color='blue', label=f'Star Forming', rasterized=True)
    
    if len(gv_sample_indices) > 0:
        plt.scatter(np.log10(stellar_mass_det[gv_sample_indices]), 
                   np.log10(np.where(sfr_total_det[gv_sample_indices] > 0, sfr_total_det[gv_sample_indices], 1e-10)), 
                   s=2, alpha=0.7, color='green', label=f'Green Valley', rasterized=True)
    
    if len(q_sample_indices) > 0:
        plt.scatter(np.log10(stellar_mass_det[q_sample_indices]), 
                   np.log10(np.where(sfr_total_det[q_sample_indices] > 0, sfr_total_det[q_sample_indices], 1e-10)), 
                   s=2, alpha=0.7, color='red', label=f'Quiescent', rasterized=True)
    
    # Plot observational SFMS (z=0 for Snap_63)
    mass_range = np.linspace(8.0, 12.0, 100)
    sfms_relation = 0.76 * mass_range - 7.6  # z~0 relation (Whitaker+12/Brinchmann+04)
    plt.plot(mass_range, sfms_relation, 'k--', linewidth=2, alpha=0.8, label='Expected SFMS')
    plt.fill_between(mass_range, sfms_relation - 0.3, sfms_relation + 0.3, 
                    color='gray', alpha=0.2, label='SFMS ±0.3 dex')
    
    plt.xlabel(r'$\log_{10}(M_*/M_\odot)$')
    plt.ylabel(r'$\log_{10}({\rm SFR}/M_\odot \, {\rm yr}^{-1})$')
    plt.xlim(8.0, 12.0)
    plt.ylim(-10.0, 2.5)  # Extended to include very low SFR values
    plt.legend(loc='upper left', fontsize=10, frameon=False, markerscale=3)
    
    plt.tight_layout()
    plt.savefig(OutputDir + f'sfms_three_populations_{snapshot}' + OutputFormat, bbox_inches='tight', dpi=300)
    plt.close()

def simple_smhm_plot(log_mvir, log_stellar, title="SMHM Relation", 
                      save_name="smhm_simple", output_dir="./", 
                      galaxies_per_bin=200, use_rolling=True,
                      scatter_alpha=0.3, scatter_size=1, scatter_color='lightblue'):
    """
    Simple, clean SMHM relation plot with scatter and smooth median line
    
    Parameters:
    -----------
    log_mvir : array
        log10(virial mass) values
    log_stellar : array
        log10(stellar mass) values
    title : str
        Plot title
    save_name : str
        Filename for saving (without extension)
    output_dir : str
        Directory to save plot
    galaxies_per_bin : int
        Target number of galaxies per bin (adaptive binning)
    use_rolling : bool
        Use rolling median for smoother line (recommended)
    """
    
    plt.figure(figsize=(10, 8))
    
    # Remove any invalid values
    valid_mask = np.isfinite(log_mvir) & np.isfinite(log_stellar)
    log_mvir_clean = log_mvir[valid_mask]
    log_stellar_clean = log_stellar[valid_mask]
    
    print(f"Plotting {len(log_mvir_clean)} valid galaxies")
    print(f"Mvir range: {log_mvir_clean.min():.2f} to {log_mvir_clean.max():.2f}")
    print(f"M* range: {log_stellar_clean.min():.2f} to {log_stellar_clean.max():.2f}")
    
    # Plot scatter
    plt.scatter(log_mvir_clean, log_stellar_clean, 
               s=scatter_size, alpha=scatter_alpha, color=scatter_color, 
               rasterized=True, label='Galaxies')
    
    # Sort data by halo mass for smooth calculations
    sort_idx = np.argsort(log_mvir_clean)
    mvir_sorted = log_mvir_clean[sort_idx]
    stellar_sorted = log_stellar_clean[sort_idx]
    
    if use_rolling:
        # Rolling median for smooth line
        window_size = galaxies_per_bin
        step_size = window_size // 4  # 75% overlap for smoothness
        
        rolling_mvir = []
        rolling_median = []
        rolling_std = []
        
        for i in range(0, len(mvir_sorted) - window_size, step_size):
            window_mvir = mvir_sorted[i:i+window_size]
            window_stellar = stellar_sorted[i:i+window_size]
            
            rolling_mvir.append(np.median(window_mvir))
            rolling_median.append(np.median(window_stellar))
            rolling_std.append(np.std(window_stellar))
        
        rolling_mvir = np.array(rolling_mvir)
        rolling_median = np.array(rolling_median)
        rolling_std = np.array(rolling_std)
        
        # Plot smooth median line
        plt.plot(rolling_mvir, rolling_median, 'k-', linewidth=3, 
                label='Median', zorder=10)
        
        # Optional: light error envelope (1-sigma scatter)
        plt.fill_between(rolling_mvir, 
                        rolling_median - rolling_std/3,  
                        rolling_median + rolling_std/3, 
                        alpha=0.15, color='black', zorder=5, label='±σ/3')
        
        print(f"Rolling median: {len(rolling_mvir)} points")
        
    else:
        # Adaptive binning with equal number of galaxies per bin
        n_bins = len(log_mvir_clean) // galaxies_per_bin
        
        bin_mvir = []
        bin_median = []
        bin_std = []
        
        for i in range(n_bins):
            start_idx = i * galaxies_per_bin
            end_idx = min((i + 1) * galaxies_per_bin, len(mvir_sorted))
            
            bin_mvir_vals = mvir_sorted[start_idx:end_idx]
            bin_stellar_vals = stellar_sorted[start_idx:end_idx]
            
            bin_mvir.append(np.median(bin_mvir_vals))
            bin_median.append(np.median(bin_stellar_vals))
            bin_std.append(np.std(bin_stellar_vals))
        
        bin_mvir = np.array(bin_mvir)
        bin_median = np.array(bin_median)
        bin_std = np.array(bin_std)
        
        # Plot median with error bars (uncertainty on median, not scatter)
        plt.errorbar(bin_mvir, bin_median, yerr=bin_std/np.sqrt(galaxies_per_bin),
                    fmt='o-', color='black', linewidth=2, 
                    markersize=4, capsize=3, capthick=1,
                    label=f'Median ±σ/√N', zorder=10)
        
        print(f"Adaptive binning: {n_bins} bins with ~{galaxies_per_bin} galaxies each")
    
    # Formatting
    plt.xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$', fontsize=14)
    plt.ylabel(r'$\log_{10}(M_*/M_\odot)$', fontsize=14)
    plt.title(title, fontsize=16)
    
    # Set reasonable limits
    data_min = log_mvir_clean.min()
    data_max = log_mvir_clean.max()
    plt.xlim(data_min - 0.1, data_max + 0.1)
    y_min = max(8.0, log_stellar_clean.min() - 0.5)
    y_max = min(12.5, log_stellar_clean.max() + 0.5)
    plt.ylim(y_min, y_max)
    
    # Legend
    plt.legend(loc='upper left', fontsize=12, frameon=False)
    
    # Grid for easier reading
    plt.grid(True, alpha=0.3, linestyle=':')
    
    # Tight layout and save
    plt.tight_layout()
    
    # Save
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    save_path = os.path.join(output_dir, f"{save_name}.pdf")
    plt.savefig(save_path, dpi=300, bbox_inches='tight')
    print(f"Saved: {save_path}")

# ==================================================================

if __name__ == '__main__':

    print('Running SAGE plotting with ALL galaxies (centrals + satellites, no sampling)\n')
    print(f'New model parameters: BoxSize={BoxSize} Mpc/h, VolumeFraction={VolumeFraction}, h={Hubble_h}')
    print(f'Old model parameters: BoxSize={BoxSize_old} Mpc/h, VolumeFraction={VolumeFraction_old}, h={Hubble_h_old}')

    seed(2222)
    np.random.seed(2222)
    
    # Calculate volumes for both models
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction
    volume_old = (BoxSize_old/Hubble_h_old)**3.0 * VolumeFraction_old
    
    print(f'New model volume: {volume:.2f} (Mpc)³')
    print(f'Old model volume: {volume_old:.2f} (Mpc)³')
    if abs(volume - volume_old) > 0.01:
        print(f'Volume ratio (new/old): {volume/volume_old:.3f}')

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    StellarMass = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    Mvir = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h  
    Type = read_hdf_smart_sampling(DirName, snap_num=Snapshot, param='Type')

    # Filter for centrals with positive masses
    central_mask = (Type == 0) & (StellarMass > 0) & (Mvir > 0)
    log_mvir = np.log10(Mvir[central_mask])
    log_stellar = np.log10(StellarMass[central_mask])

    # Plot with smooth median line
    simple_smhm_plot(log_mvir, log_stellar, 
                    title="SAGE 2.0 SMHM Relation", 
                    save_name="sage2_smhm_smooth",
                    output_dir=OutputDir,
                    galaxies_per_bin=150,  # Fewer = more detail, more = smoother
                    use_rolling=True)      # Smooth rolling median


    # Run the fixed plotting function
    smhm_plot_fixed(compare_old_model=True)
    print(f"\nFixed plot saved as: {OutputDir}smhm_relation_fixed{OutputFormat}")
    
    # Run the ETG/LTG separation plotting function
    smhm_plot_etg_ltg(compare_old_model=True)
    print(f"ETG/LTG plot saved as: {OutputDir}smhm_relation_etg_ltg{OutputFormat}")
    
    # Run the redshift grid plotting function
    smhm_plot_sf_passive_redshift_grid(compare_old_model=True)
    print(f"Redshift grid plot saved as: {OutputDir}smhm_relation_sf_passive_redshift_grid{OutputFormat}")
    
    # NEW: Run the SFMS three populations plotting function (redshift grid)
    sfms_plot_three_populations_redshift_grid(compare_old_model=True)
    print(f"SFMS redshift grid plot saved as: {OutputDir}sfms_three_populations_redshift_grid{OutputFormat}")
    
    # NEW: Run the SFMS three populations plotting function (single redshift)
    sfms_plot_three_populations_single_redshift(snapshot=Snapshot, compare_old_model=True)
    print(f"SFMS single redshift plot saved as: {OutputDir}sfms_three_populations_{Snapshot}{OutputFormat}")
    
    print("Done.")