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

# Define multiple models to compare
MODELS = [
    {
        'name': 'SAGE (latest)',
        'dir': './output/millennium/',
        'box_size': 62.5,
        'volume_fraction': 1.0,
        'hubble_h': 0.73,
        'color': 'black',
        'linestyle': '-',
        'linewidth': 2,
        'zorder': 10
    },
    {
        'name': 'SAGE C16',
        'dir': './output/millennium_vanilla_vanilla/',
        'box_size': 62.5,
        'volume_fraction': 1.0,
        'hubble_h': 0.73,
        'color': 'blue',
        'linestyle': '--',
        'linewidth': 2,
        'zorder': 9
    },
    {
        'name': 'evilSAGE',
        'dir': '/Users/mbradley/Documents/PhD/SAGE_BROKEN/sage-model/output/millennium/',
        'box_size': 62.5,
        'volume_fraction': 1.0,
        'hubble_h': 0.73,
        'color': 'purple',
        'linestyle': ':',
        'linewidth': 2,
        'zorder': 8
    }
]

# Primary model is the first one in the list
PRIMARY_MODEL = MODELS[0]
Hubble_h = PRIMARY_MODEL['hubble_h']
BoxSize = PRIMARY_MODEL['box_size']
VolumeFraction = PRIMARY_MODEL['volume_fraction']

# File and snapshot details
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Plotting options
whichimf = 1
dilute = 7500
sSFRcut = -11.0
bulge_ratio_cut = 0.1

OutputFormat = '.pdf'
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

def load_model_data(model_config, snapshot, param):
    """Load data for a specific model configuration"""
    if not os.path.exists(model_config['dir']):
        print(f"Directory {model_config['dir']} does not exist")
        return None
    
    data = read_hdf_smart_sampling(model_config['dir'], snap_num=snapshot, param=param)
    if data is not None and param in ['StellarMass', 'Mvir', 'BulgeMass', 'ColdGas']:
        data = data * 1.0e10 / model_config['hubble_h']
    
    return data

def smart_sampling_strategy(mvir, stellar_mass, galaxy_type, target_sample_size=None):
    """
    Filter for all galaxies with positive stellar mass (centrals + satellites)
    No sampling - use ALL galaxies for better statistics
    """
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
    mass_mask = stellar_mass > 0
    all_indices = np.where(mass_mask)[0]
    all_ssfr = ssfr[mass_mask]
    all_types = galaxy_type[mass_mask]
    
    sf_mask = all_ssfr > ssfr_cut
    passive_mask = all_ssfr <= ssfr_cut
    
    sf_indices = all_indices[sf_mask]
    sf_types = all_types[sf_mask]
    
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
    Filter and separate ETGs and LTGs based on SAGE25 and C16 criteria
    """
    bulge_ratio = np.where(stellar_mass > 0, bulge_mass / stellar_mass, 0.0)
    
    ltg_mask = ((galaxy_type == 0) & 
                (stellar_mass + cold_gas > 0.0) & 
                (bulge_ratio > 0.1) & 
                (bulge_ratio < 0.5))
    ltg_indices = np.where(ltg_mask)[0]
    
    etg_mask = (stellar_mass > 0) & (bulge_ratio > 0.5)
    etg_indices = np.where(etg_mask)[0]
    
    ltg_types = galaxy_type[ltg_indices] if len(ltg_indices) > 0 else np.array([])
    etg_types = galaxy_type[etg_indices] if len(etg_indices) > 0 else np.array([])
    
    print(f"LTGs (SAGE25/C16 criteria - centrals only, 0.1 < B/T < 0.5): {len(ltg_indices)} galaxies")
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
    """Calculate bulge-to-total stellar mass ratio"""
    bulge_ratio = np.where(stellar_mass > 0, bulge_mass / stellar_mass, 0.0)
    return bulge_ratio

def plot_behroozi13(ax, z, labels=True, label='Behroozi+13'):
    """Plot Behroozi+13 stellar mass-halo mass relation"""
    xmf = np.linspace(10.0, 15.0, 100)
    
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
    std_errors = []
    valid_bins = []
    
    for i in range(len(bin_edges) - 1):
        bin_mask = (log_mvir_data >= bin_edges[i]) & (log_mvir_data < bin_edges[i+1])
        bin_stellar = log_stellar_data[bin_mask]
        
        min_galaxies = 5
        
        if len(bin_stellar) >= min_galaxies:
            median_val = np.median(bin_stellar)
            std_val = np.std(bin_stellar)
            
            if len(bin_stellar) > 1:
                std_error = std_val / np.sqrt(len(bin_stellar))
            else:
                std_error = 0.3
            
            if std_error < 0.05:
                std_error = 0.05
            
            medians.append(median_val)
            std_errors.append(std_error)
            valid_bins.append(bin_centers[i])
            
        print(f"{label_prefix}Bin {bin_edges[i]:.1f}-{bin_edges[i+1]:.1f}: {len(bin_stellar)} galaxies")
    
    return np.array(valid_bins), np.array(medians), np.array(std_errors)

def calculate_median_iqr(log_mvir_data, log_stellar_data, bin_edges, label_prefix=""):
    """Calculate median and inter-quartile range in bins"""
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
    medians = []
    iqr_errors = []
    valid_bins = []
    
    for i in range(len(bin_edges) - 1):
        bin_mask = (log_mvir_data >= bin_edges[i]) & (log_mvir_data < bin_edges[i+1])
        bin_stellar = log_stellar_data[bin_mask]
        
        min_galaxies = 5
        
        if len(bin_stellar) >= min_galaxies:
            median_val = np.median(bin_stellar)
            q25 = np.percentile(bin_stellar, 25)
            q75 = np.percentile(bin_stellar, 75)
            iqr_error = (q75 - q25) / 2
            
            if iqr_error < 0.05:
                iqr_error = 0.05
            
            medians.append(median_val)
            iqr_errors.append(iqr_error)
            valid_bins.append(bin_centers[i])
            
        print(f"{label_prefix}Bin {bin_edges[i]:.1f}-{bin_edges[i+1]:.1f}: {len(bin_stellar)} galaxies")
    
    return np.array(valid_bins), np.array(medians), np.array(iqr_errors)

def plot_observations():
    """Plot observational data and return handles for legend"""
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
                   marker='*', edgecolors='orange', linewidth=0.5)
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

    behroozi_handle, = plt.plot([], [], 'r', linestyle='dashdot', linewidth=1.5)
    plot_behroozi13(plt.gca(), z=0.0, labels=False)
    obs_handles.append(behroozi_handle)
    obs_labels.append('Behroozi+13')
    
    return obs_handles, obs_labels

def smhm_plot_fixed(compare_old_model=True):
    """Plot the stellar mass-halo mass relation with fixed sampling"""
    plt.figure(figsize=(10, 8))

    print("Loading SAGE25 data...")
    StellarMass = load_model_data(PRIMARY_MODEL, Snapshot, 'StellarMass')
    Mvir = load_model_data(PRIMARY_MODEL, Snapshot, 'Mvir')
    Type = load_model_data(PRIMARY_MODEL, Snapshot, 'Type')
    
    if StellarMass is None or Mvir is None or Type is None:
        print("Failed to load SAGE25 data")
        return
    
    print(f"Loaded {len(StellarMass)} galaxies")
    
    filter_indices = smart_sampling_strategy_centrals_only(Mvir, StellarMass, Type)
    log_mvir = np.log10(Mvir[filter_indices])
    log_stellar = np.log10(StellarMass[filter_indices])

    sage_handles = []
    sage_labels = []
    
    # Plot primary model
    bin_edges = np.linspace(10.0, 15.0, 17)
    valid_bins, medians, std_errors = calculate_median_std(log_mvir, log_stellar, bin_edges, "SAGE25 (all) ")
    
    sage_handle = plt.errorbar(valid_bins, medians, yerr=std_errors, 
                fmt='k-', linewidth=2, capsize=3, capthick=1.5, zorder=10)
    sage_handles.append(sage_handle)
    sage_labels.append('SAGE25')
    
    # Plot other models if compare_old_model is True
    if compare_old_model:
        for model in MODELS[1:]:
            if not os.path.exists(model['dir']):
                continue
                
            try:
                print(f"Loading {model['name']} from {model['dir']}")
                StellarMass_old = load_model_data(model, Snapshot, 'StellarMass')
                Mvir_old = load_model_data(model, Snapshot, 'Mvir')
                Type_old = load_model_data(model, Snapshot, 'Type')
                
                if all(x is not None for x in [StellarMass_old, Mvir_old, Type_old]):
                    filter_old = smart_sampling_strategy_centrals_only(Mvir_old, StellarMass_old, Type_old)
                    log_mvir_old = np.log10(Mvir_old[filter_old])
                    log_stellar_old = np.log10(StellarMass_old[filter_old])
                    
                    valid_bins_old, medians_old, std_errors_old = calculate_median_std(
                        log_mvir_old, log_stellar_old, bin_edges, f"{model['name']} (all) ")
                    
                    sage_old_handle, = plt.plot(valid_bins_old, medians_old, 
                                linestyle=model['linestyle'], color=model['color'], 
                                linewidth=model['linewidth'], zorder=model['zorder'])
                    sage_handles.append(sage_old_handle)
                    sage_labels.append(model['name'])
            except Exception as e:
                print(f"Could not load {model['name']} data: {e}")

    obs_handles, obs_labels = plot_observations()

    plt.xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$')
    plt.ylabel(r'$\log_{10}(M_*/M_\odot)$')
    plt.xlim(10.0, 15.0)
    plt.ylim(8.0, 12.0)
    
    if sage_handles:
        legend1 = plt.legend(sage_handles, sage_labels, loc='upper left', fontsize=12, frameon=False)
        plt.gca().add_artist(legend1)
    
    if obs_handles:
        legend2 = plt.legend(obs_handles, obs_labels, loc='lower right', fontsize=12, frameon=False)
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'smhm_relation_fixed' + OutputFormat)
    plt.close()

def smhm_plot_etg_ltg(compare_old_model=True):
    """Plot the stellar mass-halo mass relation separated by ETGs and LTGs"""
    plt.figure(figsize=(10, 8))

    print("\nLoading SAGE25 data for ETG/LTG analysis...")
    StellarMass = load_model_data(PRIMARY_MODEL, Snapshot, 'StellarMass')
    Mvir = load_model_data(PRIMARY_MODEL, Snapshot, 'Mvir')
    Type = load_model_data(PRIMARY_MODEL, Snapshot, 'Type')
    BulgeMass = load_model_data(PRIMARY_MODEL, Snapshot, 'BulgeMass')
    ColdGas = load_model_data(PRIMARY_MODEL, Snapshot, 'ColdGas')
    
    if any(x is None for x in [StellarMass, Mvir, Type, BulgeMass, ColdGas]):
        print("Failed to load SAGE25 data")
        return
    
    print(f"Loaded {len(StellarMass)} galaxies")
    
    etg_indices, ltg_indices = smart_sampling_strategy_etg_ltg(
        Mvir, StellarMass, Type, BulgeMass, ColdGas)
    
    handles = []
    labels = []
    bin_edges = np.linspace(10.0, 16.0, 15)
    
    # Plot SAGE25 ETGs
    if len(etg_indices) > 0:
        log_mvir_etg = np.log10(Mvir[etg_indices])
        log_stellar_etg = np.log10(StellarMass[etg_indices])
        valid_bins_etg, medians_etg, std_errors_etg = calculate_median_std(
            log_mvir_etg, log_stellar_etg, bin_edges, "SAGE25 ETGs (all) ")
        
        if len(valid_bins_etg) > 0:
            handle_etg = plt.errorbar(valid_bins_etg, medians_etg, yerr=std_errors_etg, 
                        fmt='r-', linewidth=2, capsize=3, capthick=1.5, zorder=10, color='red')
            handles.append(handle_etg)
            labels.append('SAGE25 ETGs')
    
    # Plot SAGE25 LTGs
    if len(ltg_indices) > 0:
        log_mvir_ltg = np.log10(Mvir[ltg_indices])
        log_stellar_ltg = np.log10(StellarMass[ltg_indices])
        valid_bins_ltg, medians_ltg, std_errors_ltg = calculate_median_std(
            log_mvir_ltg, log_stellar_ltg, bin_edges, "SAGE25 LTGs (all) ")
        
        if len(valid_bins_ltg) > 0:
            handle_ltg = plt.errorbar(valid_bins_ltg, medians_ltg, yerr=std_errors_ltg, 
                        fmt='b-', linewidth=2, capsize=3, capthick=1.5, zorder=10, color='blue')
            handles.append(handle_ltg)
            labels.append('SAGE25 LTGs')
    
    # Plot other models
    if compare_old_model:
        for model in MODELS[1:]:
            if not os.path.exists(model['dir']):
                continue
                
            try:
                print(f'Loading {model["name"]} from {model["dir"]}')
                StellarMass_old = load_model_data(model, Snapshot, 'StellarMass')
                Mvir_old = load_model_data(model, Snapshot, 'Mvir')
                Type_old = load_model_data(model, Snapshot, 'Type')
                BulgeMass_old = load_model_data(model, Snapshot, 'BulgeMass')
                ColdGas_old = load_model_data(model, Snapshot, 'ColdGas')
                
                if all(x is not None for x in [StellarMass_old, Mvir_old, Type_old, BulgeMass_old, ColdGas_old]):
                    etg_indices_old, ltg_indices_old = smart_sampling_strategy_etg_ltg(
                        Mvir_old, StellarMass_old, Type_old, BulgeMass_old, ColdGas_old)
                    
                    if len(etg_indices_old) > 0:
                        log_mvir_etg_old = np.log10(Mvir_old[etg_indices_old])
                        log_stellar_etg_old = np.log10(StellarMass_old[etg_indices_old])
                        valid_bins_etg_old, medians_etg_old, std_errors_etg_old = calculate_median_std(
                            log_mvir_etg_old, log_stellar_etg_old, bin_edges, f"{model['name']} ETGs (all) ")
                        
                        if len(valid_bins_etg_old) > 0:
                            handle_etg_old, = plt.plot(valid_bins_etg_old, medians_etg_old, 
                                        linestyle=model['linestyle'], color='lightcoral', 
                                        linewidth=model['linewidth'], zorder=model['zorder'])
                            handles.append(handle_etg_old)
                            labels.append(f"{model['name']} ETGs")
                    
                    if len(ltg_indices_old) > 0:
                        log_mvir_ltg_old = np.log10(Mvir_old[ltg_indices_old])
                        log_stellar_ltg_old = np.log10(StellarMass_old[ltg_indices_old])
                        valid_bins_ltg_old, medians_ltg_old, std_errors_ltg_old = calculate_median_std(
                            log_mvir_ltg_old, log_stellar_ltg_old, bin_edges, f"{model['name']} LTGs (all) ")
                        
                        if len(valid_bins_ltg_old) > 0:
                            handle_ltg_old, = plt.plot(valid_bins_ltg_old, medians_ltg_old, 
                                        linestyle=model['linestyle'], color='lightblue', 
                                        linewidth=model['linewidth'], zorder=model['zorder'])
                            handles.append(handle_ltg_old)
                            labels.append(f"{model['name']} LTGs")
                            
            except Exception as e:
                print(f"Could not load {model['name']} data: {e}")

    # Load observational data
    obs_handles = []
    obs_labels = []
    
    try:
        correa_etg_data = np.loadtxt('./data/ETGs_Correa19.dat', comments='#')
        log_mh_correa_etg = correa_etg_data[:, 0]
        log_ms_correa_etg = correa_etg_data[:, 1]
        handle = plt.scatter(log_mh_correa_etg, log_ms_correa_etg, s=50, alpha=0.8, 
                   color='red', marker='s', edgecolors='darkred', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Correa+19 ETGs')
    except FileNotFoundError:
        pass
    
    try:
        correa_ltg_data = np.loadtxt('./data/LTGs_Correa19.dat', comments='#')
        log_mh_correa_ltg = correa_ltg_data[:, 0]
        log_ms_correa_ltg = correa_ltg_data[:, 1]
        handle = plt.scatter(log_mh_correa_ltg, log_ms_correa_ltg, s=50, alpha=0.8, 
                   color='blue', marker='s', edgecolors='darkblue', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Correa+19 LTGs')
    except FileNotFoundError:
        pass
    
    try:
        kravtsov_etg_data = np.loadtxt('./data/ETGs_Kravtsov18.dat', comments='#')
        log_mh_kravtsov_etg = kravtsov_etg_data[:, 0]
        log_ms_kravtsov_etg = kravtsov_etg_data[:, 1]
        handle = plt.scatter(log_mh_kravtsov_etg, log_ms_kravtsov_etg, s=60, alpha=0.8, 
                   color='red', marker='^', edgecolors='darkred', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Kravtsov+18 ETGs')
    except FileNotFoundError:
        pass
    
    try:
        kravtsov_ltg_data = np.loadtxt('./data/LTGs_Kravtsov18.dat', comments='#')
        log_mh_kravtsov_ltg = kravtsov_ltg_data[:, 0]
        log_ms_kravtsov_ltg = kravtsov_ltg_data[:, 1]
        handle = plt.scatter(log_mh_kravtsov_ltg, log_ms_kravtsov_ltg, s=60, alpha=0.8, 
                   color='blue', marker='^', edgecolors='darkblue', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Kravtsov+18 LTGs')
    except FileNotFoundError:
        pass

    plt.xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$')
    plt.ylabel(r'$\log_{10}(M_*/M_\odot)$')
    plt.xlim(10.0, 15.0)
    plt.ylim(8.0, 12.0)

    if handles:
        legend1 = plt.legend(handles, labels, loc='upper left', fontsize=12, frameon=False)
        plt.gca().add_artist(legend1)
    
    if obs_handles:
        legend2 = plt.legend(obs_handles, obs_labels, loc='lower right', fontsize=10, frameon=False)
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'smhm_relation_etg_ltg' + OutputFormat, bbox_inches='tight')
    plt.close()

def smhm_plot_sf_passive_redshift_grid(compare_old_model=True):
    """Plot a 2x2 grid of SMHM relations at different redshifts"""
    
    redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                 0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    target_redshifts = [1.0, 2.0, 3.0, 4.0]
    target_snapshots = []
    actual_redshifts = []
    
    for target_z in target_redshifts:
        closest_idx = np.argmin(np.abs(np.array(redshifts) - target_z))
        target_snapshots.append(closest_idx)
        actual_redshifts.append(redshifts[closest_idx])
        print(f"Target z={target_z:.1f} -> Snap_{closest_idx} (z={redshifts[closest_idx]:.3f})")
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    axes = axes.flatten()
    
    for i, (snap_idx, actual_z) in enumerate(zip(target_snapshots, actual_redshifts)):
        ax = axes[i]
        snap_name = f'Snap_{snap_idx}'
        
        print(f"\nProcessing {snap_name} (z={actual_z:.3f})...")
        
        # Load primary model
        StellarMass = load_model_data(PRIMARY_MODEL, snap_name, 'StellarMass')
        Mvir = load_model_data(PRIMARY_MODEL, snap_name, 'Mvir')
        Type = load_model_data(PRIMARY_MODEL, snap_name, 'Type')
        SfrDisk = load_model_data(PRIMARY_MODEL, snap_name, 'SfrDisk')
        SfrBulge = load_model_data(PRIMARY_MODEL, snap_name, 'SfrBulge')
        
        if any(x is None for x in [StellarMass, Mvir, Type, SfrDisk, SfrBulge]):
            ax.text(0.5, 0.5, f'No data for z={actual_z:.1f}', 
                   transform=ax.transAxes, ha='center', va='center', fontsize=14)
            continue
        
        stellar_mass_nonzero = np.where(StellarMass > 0, StellarMass, 1e-10)
        sSFR = np.log10((SfrDisk + SfrBulge) / stellar_mass_nonzero)
        
        sf_indices, passive_indices = smart_sampling_strategy_sf_passive(
            Mvir, StellarMass, Type, sSFR, sSFRcut)
        
        bin_edges = np.linspace(10.0, 15.0, 15)
        
        # Plot primary model
        if len(sf_indices) > 0:
            log_mvir_sf = np.log10(Mvir[sf_indices])
            log_stellar_sf = np.log10(StellarMass[sf_indices])
            valid_bins_sf, medians_sf, std_errors_sf = calculate_median_std(
                log_mvir_sf, log_stellar_sf, bin_edges, f"z={actual_z:.1f} SAGE25 SF ")
            
            if len(valid_bins_sf) > 0:
                ax.errorbar(valid_bins_sf, medians_sf, yerr=std_errors_sf, 
                           fmt='b-', linewidth=2, capsize=2, capthick=1, zorder=10, color='blue')
        
        if len(passive_indices) > 0:
            log_mvir_passive = np.log10(Mvir[passive_indices])
            log_stellar_passive = np.log10(StellarMass[passive_indices])
            valid_bins_passive, medians_passive, std_errors_passive = calculate_median_std(
                log_mvir_passive, log_stellar_passive, bin_edges, f"z={actual_z:.1f} SAGE25 Passive ")
            
            if len(valid_bins_passive) > 0:
                ax.errorbar(valid_bins_passive, medians_passive, yerr=std_errors_passive, 
                           fmt='r-', linewidth=2, capsize=2, capthick=1, zorder=10, color='red')
        
        # Plot other models
        if compare_old_model:
            for model in MODELS[1:]:
                if not os.path.exists(model['dir']):
                    continue
                try:
                    StellarMass_old = load_model_data(model, snap_name, 'StellarMass')
                    Mvir_old = load_model_data(model, snap_name, 'Mvir')
                    Type_old = load_model_data(model, snap_name, 'Type')
                    SfrDisk_old = load_model_data(model, snap_name, 'SfrDisk')
                    SfrBulge_old = load_model_data(model, snap_name, 'SfrBulge')
                    
                    if all(x is not None for x in [StellarMass_old, Mvir_old, Type_old, SfrDisk_old, SfrBulge_old]):
                        stellar_mass_nonzero_old = np.where(StellarMass_old > 0, StellarMass_old, 1e-10)
                        sSFR_old = np.log10((SfrDisk_old + SfrBulge_old) / stellar_mass_nonzero_old)
                        
                        sf_indices_old, passive_indices_old = smart_sampling_strategy_sf_passive(
                            Mvir_old, StellarMass_old, Type_old, sSFR_old, sSFRcut)
                        
                        if len(sf_indices_old) > 0:
                            log_mvir_sf_old = np.log10(Mvir_old[sf_indices_old])
                            log_stellar_sf_old = np.log10(StellarMass_old[sf_indices_old])
                            valid_bins_sf_old, medians_sf_old, _ = calculate_median_std(
                                log_mvir_sf_old, log_stellar_sf_old, bin_edges, f"z={actual_z:.1f} {model['name']} SF ")
                            
                            if len(valid_bins_sf_old) > 0:
                                ax.plot(valid_bins_sf_old, medians_sf_old, 
                                       linestyle=model['linestyle'], color='lightblue', 
                                       linewidth=1.5, zorder=9)
                        
                        if len(passive_indices_old) > 0:
                            log_mvir_passive_old = np.log10(Mvir_old[passive_indices_old])
                            log_stellar_passive_old = np.log10(StellarMass_old[passive_indices_old])
                            valid_bins_passive_old, medians_passive_old, _ = calculate_median_std(
                                log_mvir_passive_old, log_stellar_passive_old, bin_edges, f"z={actual_z:.1f} {model['name']} Passive ")
                            
                            if len(valid_bins_passive_old) > 0:
                                ax.plot(valid_bins_passive_old, medians_passive_old, 
                                       linestyle=model['linestyle'], color='lightcoral', 
                                       linewidth=1.5, zorder=9)
                except Exception as e:
                    print(f"Could not load {model['name']} data for {snap_name}: {e}")
        
        ax.set_xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$')
        ax.set_ylabel(r'$\log_{10}(M_*/M_\odot)$')
        ax.set_xlim(10.0, 15.0)
        ax.set_ylim(8.0, 12.0)
        ax.set_title(f'z = {actual_z:.1f}', fontsize=14)
    
    fig.suptitle(f'SMHM Relation: Star Forming vs Passive Galaxies (sSFR cut = {sSFRcut})', fontsize=16)
    
    handles = []
    handles.append(plt.Line2D([0], [0], color='blue', linewidth=2, label='SAGE25 Star Forming'))
    handles.append(plt.Line2D([0], [0], color='red', linewidth=2, label='SAGE25 Passive'))
    if compare_old_model:
        handles.append(plt.Line2D([0], [0], color='lightblue', linewidth=1.5, linestyle='--', label='Other Models SF'))
        handles.append(plt.Line2D([0], [0], color='lightcoral', linewidth=1.5, linestyle='--', label='Other Models Passive'))
    
    fig.legend(handles=handles, loc='center', bbox_to_anchor=(0.5, 0.02), ncol=4, fontsize=12, frameon=False)
    
    plt.tight_layout()
    plt.subplots_adjust(bottom=0.12)
    
    plt.savefig(OutputDir + 'smhm_relation_sf_passive_redshift_grid' + OutputFormat, bbox_inches='tight')
    plt.close()

def sfms_plot_three_populations_redshift_grid(compare_old_model=True):
    """Plot a 2x2 grid of star forming main sequence at different redshifts with three populations"""
    
    redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                 0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    target_redshifts = [1.0, 2.0, 3.0, 4.0]
    target_snapshots = []
    actual_redshifts = []
    
    for target_z in target_redshifts:
        closest_idx = np.argmin(np.abs(np.array(redshifts) - target_z))
        target_snapshots.append(closest_idx)
        actual_redshifts.append(redshifts[closest_idx])
        print(f"Target z={target_z:.1f} -> Snap_{closest_idx} (z={redshifts[closest_idx]:.3f})")
    
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    axes = axes.flatten()
    
    for i, (snap_idx, actual_z) in enumerate(zip(target_snapshots, actual_redshifts)):
        ax = axes[i]
        snap_name = f'Snap_{snap_idx}'
        
        print(f"\nProcessing {snap_name} (z={actual_z:.3f}) for SFMS...")
        
        StellarMass = load_model_data(PRIMARY_MODEL, snap_name, 'StellarMass')
        Mvir = load_model_data(PRIMARY_MODEL, snap_name, 'Mvir')
        Type = load_model_data(PRIMARY_MODEL, snap_name, 'Type')
        SfrDisk = load_model_data(PRIMARY_MODEL, snap_name, 'SfrDisk')
        SfrBulge = load_model_data(PRIMARY_MODEL, snap_name, 'SfrBulge')
        
        if any(x is None for x in [StellarMass, Mvir, Type, SfrDisk, SfrBulge]):
            ax.text(0.5, 0.5, f'No data for z={actual_z:.1f}', 
                   transform=ax.transAxes, ha='center', va='center', fontsize=14)
            continue
        
        mass_mask = StellarMass > 0
        stellar_mass_all = StellarMass[mass_mask]
        sfr_disk_all = SfrDisk[mass_mask]
        sfr_bulge_all = SfrBulge[mass_mask]
        all_indices = np.where(mass_mask)[0]
        
        sfr_total = sfr_disk_all + sfr_bulge_all
        stellar_mass_det = stellar_mass_all
        sfr_total_det = sfr_total
        det_indices = all_indices
        
        sfr_for_ssfr = np.where(sfr_total_det > 0, sfr_total_det, 1e-10)
        ssfr_det = np.log10(sfr_for_ssfr / stellar_mass_det)
        
        ssfr_sf_cut = -10.5
        ssfr_quiescent_cut = -11.5
        
        sf_mask = ssfr_det > ssfr_sf_cut
        quiescent_mask = ssfr_det < ssfr_quiescent_cut
        green_valley_mask = (ssfr_det >= ssfr_quiescent_cut) & (ssfr_det <= ssfr_sf_cut)
        
        sf_plot_indices = np.where(sf_mask)[0]
        gv_plot_indices = np.where(green_valley_mask)[0]
        q_plot_indices = np.where(quiescent_mask)[0]
        
        if len(sf_plot_indices) > dilute:
            sf_sample_indices = sample(list(sf_plot_indices), dilute)
        else:
            sf_sample_indices = sf_plot_indices
            
        if len(gv_plot_indices) > dilute:
            gv_sample_indices = sample(list(gv_plot_indices), dilute)
        else:
            gv_sample_indices = gv_plot_indices
            
        if len(q_plot_indices) > dilute:
            q_sample_indices = sample(list(q_plot_indices), dilute)
        else:
            q_sample_indices = q_plot_indices
        
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
        
        mass_range = np.linspace(8.0, 12.0, 100)
        
        if actual_z <= 1.0:
            sfms_relation = 0.76 * mass_range - 7.6
        elif actual_z <= 2.0:
            sfms_relation = 0.80 * mass_range - 7.8
        elif actual_z <= 3.0:
            sfms_relation = 0.84 * mass_range - 8.0
        else:
            sfms_relation = 0.88 * mass_range - 8.2
        
        ax.plot(mass_range, sfms_relation, 'k--', linewidth=2, alpha=0.8, 
               label='SFMS' if i == 0 else '')
        ax.fill_between(mass_range, sfms_relation - 0.3, sfms_relation + 0.3, 
                       color='gray', alpha=0.2)
        
        if compare_old_model:
            for model in MODELS[1:]:
                if not os.path.exists(model['dir']):
                    continue
                try:
                    StellarMass_old = load_model_data(model, snap_name, 'StellarMass')
                    Type_old = load_model_data(model, snap_name, 'Type')
                    SfrDisk_old = load_model_data(model, snap_name, 'SfrDisk')
                    SfrBulge_old = load_model_data(model, snap_name, 'SfrBulge')
                    
                    if all(x is not None for x in [StellarMass_old, Type_old, SfrDisk_old, SfrBulge_old]):
                        mass_mask_old = StellarMass_old > 0
                        stellar_mass_old = StellarMass_old[mass_mask_old]
                        sfr_total_old = (SfrDisk_old + SfrBulge_old)[mass_mask_old]
                        
                        if len(stellar_mass_old) > 100:
                            if len(stellar_mass_old) > dilute:
                                old_sample_indices = sample(list(range(len(stellar_mass_old))), dilute)
                                stellar_mass_old_sample = stellar_mass_old[old_sample_indices]
                                sfr_total_old_sample = sfr_total_old[old_sample_indices]
                            else:
                                stellar_mass_old_sample = stellar_mass_old
                                sfr_total_old_sample = sfr_total_old
                                
                            ax.scatter(np.log10(stellar_mass_old_sample), 
                                     np.log10(np.where(sfr_total_old_sample > 0, sfr_total_old_sample, 1e-10)), 
                                     s=0.8, alpha=0.3, color='orange', 
                                     label=f'{model["name"]}' if i == 0 else '', rasterized=True)
                except Exception as e:
                    print(f"Could not load {model['name']} data for {snap_name}: {e}")
        
        ax.set_xlabel(r'$\log_{10}(M_*/M_\odot)$')
        ax.set_ylabel(r'$\log_{10}({\rm SFR}/M_\odot \, {\rm yr}^{-1})$')
        ax.set_xlim(8.0, 12.0)
        ax.set_ylim(-10.0, 2.5)
        ax.set_title(f'z = {actual_z:.1f}', fontsize=14)
        
        if i == 0:
            ax.legend(loc='lower right', fontsize=9, frameon=False, markerscale=3)
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'sfms_three_populations_redshift_grid' + OutputFormat, bbox_inches='tight', dpi=300)
    plt.close()

def sfms_plot_three_populations_single_redshift(snapshot='Snap_63', compare_old_model=True):
    """Plot star forming main sequence for a single redshift with three populations"""
    
    print(f"\nCreating SFMS plot for {snapshot}...")
    
    plt.figure(figsize=(12, 9))
    
    StellarMass = load_model_data(PRIMARY_MODEL, snapshot, 'StellarMass')
    Mvir = load_model_data(PRIMARY_MODEL, snapshot, 'Mvir')
    Type = load_model_data(PRIMARY_MODEL, snapshot, 'Type')
    SfrDisk = load_model_data(PRIMARY_MODEL, snapshot, 'SfrDisk')
    SfrBulge = load_model_data(PRIMARY_MODEL, snapshot, 'SfrBulge')
    
    if any(x is None for x in [StellarMass, Mvir, Type, SfrDisk, SfrBulge]):
        print(f"Failed to load SAGE25 data for {snapshot}")
        return
    
    mass_mask = StellarMass > 0
    stellar_mass_all = StellarMass[mass_mask]
    sfr_disk_all = SfrDisk[mass_mask]
    sfr_bulge_all = SfrBulge[mass_mask]
    
    sfr_total = sfr_disk_all + sfr_bulge_all
    stellar_mass_det = stellar_mass_all
    sfr_total_det = sfr_total
    
    sfr_for_ssfr = np.where(sfr_total_det > 0, sfr_total_det, 1e-10)
    ssfr_det = np.log10(sfr_for_ssfr / stellar_mass_det)
    
    ssfr_sf_cut = -10.5
    ssfr_quiescent_cut = -11.5
    
    sf_mask = ssfr_det > ssfr_sf_cut
    quiescent_mask = ssfr_det < ssfr_quiescent_cut
    green_valley_mask = (ssfr_det >= ssfr_quiescent_cut) & (ssfr_det <= ssfr_sf_cut)
    
    sf_indices = np.where(sf_mask)[0]
    gv_indices = np.where(green_valley_mask)[0]
    q_indices = np.where(quiescent_mask)[0]
    
    if len(sf_indices) > dilute:
        sf_sample_indices = sample(list(sf_indices), dilute)
    else:
        sf_sample_indices = sf_indices
        
    if len(gv_indices) > dilute:
        gv_sample_indices = sample(list(gv_indices), dilute)
    else:
        gv_sample_indices = gv_indices
        
    if len(q_indices) > dilute:
        q_sample_indices = sample(list(q_indices), dilute)
    else:
        q_sample_indices = q_indices
    
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
    
    mass_range = np.linspace(8.0, 12.0, 100)
    sfms_relation = 0.76 * mass_range - 7.6
    plt.plot(mass_range, sfms_relation, 'k--', linewidth=2, alpha=0.8, label='Expected SFMS')
    plt.fill_between(mass_range, sfms_relation - 0.3, sfms_relation + 0.3, 
                    color='gray', alpha=0.2, label='SFMS ±0.3 dex')
    
    plt.xlabel(r'$\log_{10}(M_*/M_\odot)$')
    plt.ylabel(r'$\log_{10}({\rm SFR}/M_\odot \, {\rm yr}^{-1})$')
    plt.xlim(8.0, 12.0)
    plt.ylim(-10.0, 2.5)
    plt.legend(loc='upper left', fontsize=10, frameon=False, markerscale=3)
    
    plt.tight_layout()
    plt.savefig(OutputDir + f'sfms_three_populations_{snapshot}' + OutputFormat, bbox_inches='tight', dpi=300)
    plt.close()

def simple_smhm_plot(log_mvir, log_stellar, title="SMHM Relation", 
                      save_name="smhm_simple", output_dir="./", 
                      galaxies_per_bin=200, use_rolling=True,
                      scatter_alpha=0.3, scatter_size=1, scatter_color='lightblue'):
    """Simple, clean SMHM relation plot with scatter and smooth median line"""
    
    plt.figure(figsize=(10, 8))
    
    valid_mask = np.isfinite(log_mvir) & np.isfinite(log_stellar)
    log_mvir_clean = log_mvir[valid_mask]
    log_stellar_clean = log_stellar[valid_mask]
    
    print(f"Plotting {len(log_mvir_clean)} valid galaxies")
    
    plt.scatter(log_mvir_clean, log_stellar_clean, 
               s=scatter_size, alpha=scatter_alpha, color=scatter_color, 
               rasterized=True, label='Galaxies')
    
    sort_idx = np.argsort(log_mvir_clean)
    mvir_sorted = log_mvir_clean[sort_idx]
    stellar_sorted = log_stellar_clean[sort_idx]
    
    if use_rolling:
        window_size = galaxies_per_bin
        step_size = window_size // 4
        
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
        
        plt.plot(rolling_mvir, rolling_median, 'k-', linewidth=3, 
                label='Median', zorder=10)
        
        plt.fill_between(rolling_mvir, 
                        rolling_median - rolling_std/3,  
                        rolling_median + rolling_std/3, 
                        alpha=0.15, color='black', zorder=5, label='±σ/3')
    
    plt.xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$', fontsize=14)
    plt.ylabel(r'$\log_{10}(M_*/M_\odot)$', fontsize=14)
    plt.title(title, fontsize=16)
    
    data_min = log_mvir_clean.min()
    data_max = log_mvir_clean.max()
    plt.xlim(data_min - 0.1, data_max + 0.1)
    y_min = max(8.0, log_stellar_clean.min() - 0.5)
    y_max = min(12.5, log_stellar_clean.max() + 0.5)
    plt.ylim(y_min, y_max)
    
    plt.legend(loc='upper left', fontsize=12, frameon=False)
    plt.grid(True, alpha=0.3, linestyle=':')
    plt.tight_layout()
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    save_path = os.path.join(output_dir, f"{save_name}.pdf")
    plt.savefig(save_path, dpi=300, bbox_inches='tight')
    print(f"Saved: {save_path}")
    plt.close()

# ==================================================================

if __name__ == '__main__':
    print('Running SAGE plotting with multiple models\n')
    
    for i, model in enumerate(MODELS):
        print(f"{i+1}. {model['name']}: {model['dir']}")
        volume = (model['box_size']/model['hubble_h'])**3.0 * model['volume_fraction']
        print(f"   Volume: {volume:.2f} (Mpc)³")
    
    seed(2222)
    np.random.seed(2222)
    
    OutputDir = PRIMARY_MODEL['dir'] + 'plots/'
    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)
    
    print('\n' + '='*60)
    print('Creating plots...')
    print('='*60)
    
    # Load primary model data for simple plot
    StellarMass = load_model_data(PRIMARY_MODEL, Snapshot, 'StellarMass')
    Mvir = load_model_data(PRIMARY_MODEL, Snapshot, 'Mvir')
    Type = load_model_data(PRIMARY_MODEL, Snapshot, 'Type')

    if all(x is not None for x in [StellarMass, Mvir, Type]):
        central_mask = (Type == 0) & (StellarMass > 0) & (Mvir > 0)
        log_mvir = np.log10(Mvir[central_mask])
        log_stellar = np.log10(StellarMass[central_mask])

        simple_smhm_plot(log_mvir, log_stellar, 
                        title=f"{PRIMARY_MODEL['name']} SMHM Relation", 
                        save_name=f"{PRIMARY_MODEL['name'].lower().replace(' ', '_')}_smhm_smooth",
                        output_dir=OutputDir,
                        galaxies_per_bin=150,
                        use_rolling=True)

    # Run all plotting functions
    smhm_plot_fixed(compare_old_model=True)
    print(f"\nFixed plot saved as: {OutputDir}smhm_relation_fixed{OutputFormat}")
    
    smhm_plot_etg_ltg(compare_old_model=True)
    print(f"ETG/LTG plot saved as: {OutputDir}smhm_relation_etg_ltg{OutputFormat}")
    
    smhm_plot_sf_passive_redshift_grid(compare_old_model=True)
    print(f"Redshift grid plot saved as: {OutputDir}smhm_relation_sf_passive_redshift_grid{OutputFormat}")
    
    sfms_plot_three_populations_redshift_grid(compare_old_model=True)
    print(f"SFMS redshift grid plot saved as: {OutputDir}sfms_three_populations_redshift_grid{OutputFormat}")
    
    sfms_plot_three_populations_single_redshift(snapshot=Snapshot, compare_old_model=True)
    print(f"SFMS single redshift plot saved as: {OutputDir}sfms_three_populations_{Snapshot}{OutputFormat}")
    
    print("\nDone.")