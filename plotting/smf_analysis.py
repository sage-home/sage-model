#!/usr/bin/env python3
"""
SAGE Stellar Mass Function Analysis - Redshift Binned SMF Grid
============================================================

This script uses your existing SAGE data reading approach and adds redshift binning
for creating SMF grids, now with integrated observational data from the ECSV file
and Baldry et al. 2008 data for the lowest redshift bin.

Author: Analysis script for SAGE semi-analytic model
"""

import numpy as np
import matplotlib.pyplot as plt
import h5py as h5
import glob
import os
from scipy import stats
import pandas as pd
from random import seed

# Configuration parameters - UPDATE THESE TO MATCH YOUR SETUP
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
ObsDataFile = 'SMF_data_points.ecsv'  # Path to observational data file

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model

# IMF setting - set to 1 if using Chabrier IMF, 0 otherwise
whichimf = 1  # Add this variable to control IMF correction

# Snapshot to redshift mapping - UPDATE THIS TO MATCH YOUR SIMULATION
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

OutputFormat = '.png'
plt.rcParams["figure.figsize"] = (15, 12)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 12


def get_baldry_2008_data():
    """
    Return Baldry et al. 2008 stellar mass function data
    
    Returns:
    --------
    tuple : (log_mass, phi_upper, phi_lower) arrays for plotting
    """
    # Baldry+ 2008 modified data used for the MCMC fitting
    Baldry = np.array([
        [7.05, 1.3531e-01, 6.0741e-02],
        [7.15, 1.3474e-01, 6.0109e-02],
        [7.25, 2.0971e-01, 7.7965e-02],
        [7.35, 1.7161e-01, 3.1841e-02],
        [7.45, 2.1648e-01, 5.7832e-02],
        [7.55, 2.1645e-01, 3.9988e-02],
        [7.65, 2.0837e-01, 4.8713e-02],
        [7.75, 2.0402e-01, 7.0061e-02],
        [7.85, 1.5536e-01, 3.9182e-02],
        [7.95, 1.5232e-01, 2.6824e-02],
        [8.05, 1.5067e-01, 4.8824e-02],
        [8.15, 1.3032e-01, 2.1892e-02],
        [8.25, 1.2545e-01, 3.5526e-02],
        [8.35, 9.8472e-02, 2.7181e-02],
        [8.45, 8.7194e-02, 2.8345e-02],
        [8.55, 7.0758e-02, 2.0808e-02],
        [8.65, 5.8190e-02, 1.3359e-02],
        [8.75, 5.6057e-02, 1.3512e-02],
        [8.85, 5.1380e-02, 1.2815e-02],
        [8.95, 4.4206e-02, 9.6866e-03],
        [9.05, 4.1149e-02, 1.0169e-02],
        [9.15, 3.4959e-02, 6.7898e-03],
        [9.25, 3.3111e-02, 8.3704e-03],
        [9.35, 3.0138e-02, 4.7741e-03],
        [9.45, 2.6692e-02, 5.5029e-03],
        [9.55, 2.4656e-02, 4.4359e-03],
        [9.65, 2.2885e-02, 3.7915e-03],
        [9.75, 2.1849e-02, 3.9812e-03],
        [9.85, 2.0383e-02, 3.2930e-03],
        [9.95, 1.9929e-02, 2.9370e-03],
        [10.05, 1.8865e-02, 2.4624e-03],
        [10.15, 1.8136e-02, 2.5208e-03],
        [10.25, 1.7657e-02, 2.4217e-03],
        [10.35, 1.6616e-02, 2.2784e-03],
        [10.45, 1.6114e-02, 2.1783e-03],
        [10.55, 1.4366e-02, 1.8819e-03],
        [10.65, 1.2588e-02, 1.8249e-03],
        [10.75, 1.1372e-02, 1.4436e-03],
        [10.85, 9.1213e-03, 1.5816e-03],
        [10.95, 6.1125e-03, 9.6735e-04],
        [11.05, 4.3923e-03, 9.6254e-04],
        [11.15, 2.5463e-03, 5.0038e-04],
        [11.25, 1.4298e-03, 4.2816e-04],
        [11.35, 6.4867e-04, 1.6439e-04],
        [11.45, 2.8294e-04, 9.9799e-05],
        [11.55, 1.0617e-04, 4.9085e-05],
        [11.65, 3.2702e-05, 2.4546e-05],
        [11.75, 1.2571e-05, 1.2571e-05],
        [11.85, 8.4589e-06, 8.4589e-06],
        [11.95, 7.4764e-06, 7.4764e-06],
    ], dtype=np.float32)

    # Convert mass to log scale and apply IMF correction
    Baldry_xval = np.log10(10 ** Baldry[:, 0] / Hubble_h / Hubble_h)
    if whichimf == 1:  
        Baldry_xval = Baldry_xval - 0.26  # convert back to Chabrier IMF
    
    # Convert phi values and apply volume correction
    Baldry_yvalU = (Baldry[:, 1] + Baldry[:, 2]) * Hubble_h * Hubble_h * Hubble_h
    Baldry_yvalL = (Baldry[:, 1] - Baldry[:, 2]) * Hubble_h * Hubble_h * Hubble_h
    Baldry_yval = Baldry[:, 1] * Hubble_h * Hubble_h * Hubble_h
    
    return Baldry_xval, Baldry_yval, Baldry_yvalU, Baldry_yvalL


def load_observational_data(filename=None):
    """
    Load observational stellar mass function data from ECSV file
    
    Returns:
    --------
    dict : Dictionary with redshift bins as keys and (masses, phi, phi_err) as values
    """
    if filename is None:
        filename = ObsDataFile
    
    if not os.path.exists(filename):
        print(f"Warning: Observational data file {filename} not found!")
        return {}
    
    obs_data = {}
    
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Skip header lines (start with #)
        data_lines = [line.strip() for line in lines if not line.startswith('#') and line.strip()]
        
        # Parse header
        header = data_lines[0].split()
        
        # Parse data
        for line in data_lines[1:]:
            # Handle quoted redshift strings
            if '"' in line:
                parts = line.split('"')
                redshift_bin = parts[1]
                data_parts = parts[2].strip().split()
                
                M_star = float(data_parts[0])
                Phi = float(data_parts[1])
                dPhi = float(data_parts[2])
                
                if redshift_bin not in obs_data:
                    obs_data[redshift_bin] = {'M_star': [], 'Phi': [], 'dPhi': []}
                
                obs_data[redshift_bin]['M_star'].append(M_star)
                obs_data[redshift_bin]['Phi'].append(Phi)
                obs_data[redshift_bin]['dPhi'].append(dPhi)
        
        # Convert lists to numpy arrays
        for redshift_bin in obs_data:
            obs_data[redshift_bin]['M_star'] = np.array(obs_data[redshift_bin]['M_star'])
            obs_data[redshift_bin]['Phi'] = np.array(obs_data[redshift_bin]['Phi'])
            obs_data[redshift_bin]['dPhi'] = np.array(obs_data[redshift_bin]['dPhi'])
        
        print(f"Loaded observational data for {len(obs_data)} redshift bins")
        for redshift_bin in sorted(obs_data.keys()):
            n_points = len(obs_data[redshift_bin]['M_star'])
            print(f"  {redshift_bin}: {n_points} data points")
        
        return obs_data
        
    except Exception as e:
        print(f"Error loading observational data: {e}")
        return {}


def match_redshift_bin_to_obs(z_low, z_high, obs_data):
    """
    Find the best matching observational redshift bin for given z_low, z_high
    
    Parameters:
    -----------
    z_low, z_high : float
        Redshift bin boundaries
    obs_data : dict
        Observational data dictionary
    
    Returns:
    --------
    str or None : Best matching observational redshift bin key
    """
    z_center = (z_low + z_high) / 2
    
    best_match = None
    best_overlap = 0
    
    for obs_bin in obs_data.keys():
        # Parse observational bin
        if '<' in obs_bin and '>' not in obs_bin:
            # Format like "0.2 < z < 0.5"
            parts = obs_bin.split('<')
            if len(parts) == 3:
                try:
                    obs_z_low = float(parts[0].strip())
                    obs_z_high = float(parts[2].strip())
                    
                    # Calculate overlap
                    overlap_low = max(z_low, obs_z_low)
                    overlap_high = min(z_high, obs_z_high)
                    overlap = max(0, overlap_high - overlap_low)
                    
                    if overlap > best_overlap:
                        best_overlap = overlap
                        best_match = obs_bin
                except:
                    continue
    
    return best_match


def read_hdf(filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files - YOUR EXISTING FUNCTION"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        try:
            with h5.File(DirName + model_file, 'r') as property_file:
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


def get_available_snapshots():
    """Get list of available snapshots from the data files"""
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    
    if not model_files:
        return []
    
    # Check first file for available snapshots
    try:
        with h5.File(DirName + model_files[0], 'r') as f:
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
        print(f"Error reading snapshot list: {e}")
        return []


def create_redshift_bins(available_snapshots):
    """
    Create redshift bins matching observational studies
    
    Returns:
    --------
    list : List of (z_low, z_high, z_center, snapshots_in_bin) tuples
    """
    # Define redshift bin edges to match observational figure
    # Combine first two bins and make them cover a reasonable range
    redshift_bins = [
        (0.0, 0.5),    # z ~ 0.25 (combined low-z bin for Baldry data)
        (0.5, 0.8),    # z ~ 0.65  
        (0.8, 1.1),    # z ~ 0.95
        (1.1, 1.5),    # z ~ 1.3
        (1.5, 2.0),    # z ~ 1.75
        (2.0, 2.5),    # z ~ 2.25
        (2.5, 3.0),    # z ~ 2.75
        (3.0, 3.5),    # z ~ 3.25
        (3.5, 4.5),    # z ~ 4.0
        (4.5, 5.5),    # z ~ 5.0
        (5.5, 6.5),    # z ~ 6.0
        (6.5, 7.5),    # z ~ 7.0
        (7.5, 8.5),    # z ~ 8.0
        (8.5, 10.0),   # z ~ 9.25
        (10.0, 12.0),  # z ~ 11.0
    ]
    
    bins_with_data = []
    
    for z_low, z_high in redshift_bins:
        z_center = (z_low + z_high) / 2
        snapshots_in_bin = []
        
        # Find all snapshots within this redshift range
        for snap in available_snapshots:
            z_snap = get_redshift_from_snapshot(snap)
            if z_low <= z_snap < z_high:
                snapshots_in_bin.append(snap)
        
        if snapshots_in_bin:
            bins_with_data.append((z_low, z_high, z_center, snapshots_in_bin))
    
    return bins_with_data


def calculate_smf(stellar_masses, volume=None):
    """Calculate stellar mass function using the exact same approach as the standalone script"""
    
    # Convert to log mass first (exactly like standalone script)
    w = np.where(stellar_masses > 0.0)[0]
    mass = np.log10(stellar_masses[w])
    
    # Use exact same binning as standalone script
    binwidth = 0.1  # mass function histogram bin width
    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    
    # Calculate histogram in log space (exactly like standalone)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins
    
    # Calculate volume (same as standalone script)
    if volume is None:
        volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction
    
    # Calculate phi (exact same normalization as standalone script)
    phi = counts / (volume * binwidth)
    
    # Poisson errors (same as standalone)
    phi_err = np.sqrt(counts) / (volume * binwidth)
    
    # Handle zeros - set to upper limit (same as standalone)
    zero_mask = counts == 0
    phi_err[zero_mask] = 1.0 / (volume * binwidth)
    
    # Return bin centers in log space (for plotting) and phi values
    return xaxeshisto, phi, phi_err


def is_lowest_redshift_bin(z_low, z_high):
    """
    Check if this is the lowest redshift bin where we want to add Baldry data
    
    Parameters:
    -----------
    z_low, z_high : float
        Redshift bin boundaries
        
    Returns:
    --------
    bool : True if this is the lowest redshift bin
    """
    return z_low <= 0.1 and z_high >= 0.4  # Covers the combined low-z range (0.0 < z < 0.5)


def plot_smf_redshift_grid(galaxy_types='all', mass_range=(7, 12), 
                          save_path=None, show_observations=True, 
                          sage_color='red', sage_label='SAGE'):
    """
    Create a grid plot of stellar mass functions for redshift bins
    """
    # Load observational data
    obs_data = {}
    if show_observations:
        obs_data = load_observational_data()
        if not obs_data:
            print("Warning: No observational data loaded. Proceeding without observations.")
            show_observations = False
    
    # Get available snapshots
    available_snapshots = get_available_snapshots()
    
    if not available_snapshots:
        raise ValueError("No snapshots found in the data files")
    
    print(f"Found {len(available_snapshots)} available snapshots")
    print(f"Snapshot range: {min(available_snapshots)} to {max(available_snapshots)}")
    
    # Create redshift bins
    redshift_bins = create_redshift_bins(available_snapshots)
    
    if not redshift_bins:
        raise ValueError("No redshift bins with data found")
    
    print(f"Created {len(redshift_bins)} redshift bins with data")
    
    # Set up the plot grid (5 columns x 3 rows to match typical SMF studies)
    n_bins = len(redshift_bins)
    n_cols = 3
    n_rows = 5
    
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(20, 32), 
                           sharex=True, sharey=True)
    
    # Flatten axes for easier indexing
    if n_rows == 1:
        axes = axes.reshape(1, -1)
    axes_flat = axes.flatten()
    
    # Define mass bins and volume (same as standalone script)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction
    
    for i, (z_low, z_high, z_center, snapshots) in enumerate(redshift_bins):
        if i >= len(axes_flat):
            break
            
        ax = axes_flat[i]
        
        print(f"Processing redshift bin {z_low:.1f} < z < {z_high:.1f} with {len(snapshots)} snapshots")
        
        # Add observational data first (so it appears behind SAGE data)
        obs_added = False
        if show_observations:
            obs_added = add_observational_data_with_baldry(ax, z_low, z_high, obs_data)
        
        # Add single snapshot closest to bin center for comparison
        try:
            # For the lowest redshift bin, use z=0 snapshot for better Baldry comparison
            if is_lowest_redshift_bin(z_low, z_high):
                # Use Snap_63 (z=0) for the lowest bin to match Baldry data
                closest_snap = 63
                z_snap = 0.0
            else:
                # Find snapshot closest to redshift bin center for other bins
                z_center_actual = (z_low + z_high) / 2
                closest_snap = None
                min_diff = float('inf')
                
                for snap in snapshots:
                    z_snap_test = get_redshift_from_snapshot(snap)
                    diff = abs(z_snap_test - z_center_actual)
                    if diff < min_diff:
                        min_diff = diff
                        closest_snap = snap
                
                z_snap = get_redshift_from_snapshot(closest_snap)
            
            if closest_snap is not None:
                snap_str = f'Snap_{closest_snap}'
                
                # Read data for the closest snapshot
                snap_stellar_mass = read_hdf(snap_num=snap_str, param='StellarMass')
                snap_galaxy_type = read_hdf(snap_num=snap_str, param='Type')
                
                if snap_stellar_mass is not None and snap_galaxy_type is not None:
                    snap_stellar_mass = snap_stellar_mass * 1.0e10 / Hubble_h
                    
                    # Filter by galaxy type
                    if galaxy_types == 'central':
                        mask_type = (snap_galaxy_type == 0)
                    elif galaxy_types == 'satellite':
                        mask_type = (snap_galaxy_type == 1)
                    elif galaxy_types == 'orphan':
                        mask_type = (snap_galaxy_type == 2)
                    else:  # 'all'
                        mask_type = np.ones(len(snap_stellar_mass), dtype=bool)
                    
                    # Apply filters
                    mask_combined = (snap_stellar_mass > 0) & mask_type
                    snap_filtered_masses = snap_stellar_mass[mask_combined]
                    
                        
        except Exception as e:
            print(f"Warning: Could not add single snapshot comparison: {e}")
        
        # Find the snapshot with redshift just above the lower bound of this bin
        # This gives us the most representative single snapshot for comparison
        target_z = z_low  # Use the low end of the redshift range
        best_snap = None
        min_diff = float('inf')
        
        for snap in snapshots:
            z_snap = get_redshift_from_snapshot(snap)
            # Only consider snapshots with redshift >= target_z
            if z_snap >= target_z:
                diff = z_snap - target_z
                if diff < min_diff:
                    min_diff = diff
                    best_snap = snap
        
        # If no snapshot found above target_z, use the one closest to target_z
        if best_snap is None:
            for snap in snapshots:
                z_snap = get_redshift_from_snapshot(snap)
                diff = abs(z_snap - target_z)
                if diff < min_diff:
                    min_diff = diff
                    best_snap = snap
        
        if best_snap is not None:
            try:
                snap_str = f'Snap_{best_snap}'
                z_best = get_redshift_from_snapshot(best_snap)
                
                # Read data for the selected snapshot
                stellar_mass = read_hdf(snap_num=snap_str, param='StellarMass')
                galaxy_type = read_hdf(snap_num=snap_str, param='Type')
                
                if stellar_mass is not None and galaxy_type is not None:
                    # Convert to solar masses
                    stellar_mass = stellar_mass * 1.0e10 / Hubble_h
                    
                    # Apply galaxy type filter
                    if galaxy_types == 'central':
                        mask = (galaxy_type == 0)
                    elif galaxy_types == 'satellite':
                        mask = (galaxy_type == 1)
                    elif galaxy_types == 'orphan':
                        mask = (galaxy_type == 2)
                    else:  # 'all'
                        mask = np.ones(len(stellar_mass), dtype=bool)
                    
                    # Only keep galaxies with positive stellar mass and correct type
                    combined_mask = (stellar_mass > 0) & mask
                    filtered_masses = stellar_mass[combined_mask]
                    
                    print(f"  Using Snap_{best_snap} (z={z_best:.2f}) for z_target={target_z:.2f}: {len(filtered_masses)} galaxies")
                else:
                    filtered_masses = np.array([])
                    
            except Exception as e:
                print(f"Warning: Could not read Snap_{best_snap} data: {e}")
                filtered_masses = np.array([])
        else:
            print(f"Warning: No suitable snapshot found for target z={target_z:.2f}")
            filtered_masses = np.array([])
        
        if len(filtered_masses) == 0:
            ax.text(0.5, 0.5, 'No data', transform=ax.transAxes, 
                   ha='center', va='center')
            continue
        
        # Calculate SMF using single snapshot (no normalization needed)
        xaxeshisto, phi, phi_err = calculate_smf(filtered_masses, volume=volume)
        
        # Plot SAGE data with snapshot redshift in label
        mask_plot = phi > 0
        if np.any(mask_plot):
            phi_log = np.log10(phi[mask_plot])
            phi_err_log = phi_err[mask_plot] / (phi[mask_plot] * np.log(10))
            
            # Create label with snapshot redshift
            sage_label_with_z = f'{sage_label} (z={z_best:.2f})'
            
            ax.errorbar(xaxeshisto[mask_plot], phi_log, 
                       yerr=phi_err_log,
                       fmt='o-', color=sage_color, markersize=4, linewidth=2,
                       label=sage_label_with_z, alpha=0.8)
        
        # Add upper limits for zero counts
        mask_upper = phi == 0
        if np.any(mask_upper):
            phi_upper = np.log10(phi_err[mask_upper])
            ax.plot(xaxeshisto[mask_upper], phi_upper, 'v', 
                   color=sage_color, markersize=4, alpha=0.6)
        
        # Formatting
        ax.set_title(f'{z_low:.1f} < z < {z_high:.1f}', fontsize=12)
        ax.grid(True, alpha=0.3)
        ax.set_xlim(mass_range)
        ax.set_ylim(-6, -1)
        
        # Add legend: full legend to first panel, simple SAGE legend to all others
        if i == 0:
            # First panel gets full legend with all available data
            ax.legend(fontsize=8, loc='lower left', frameon=False)
        else:
            # All other panels get a simple legend showing just SAGE with redshift
            # Create a custom legend with just the SAGE line
            from matplotlib.lines import Line2D
            custom_legend = [Line2D([0], [0], color=sage_color, linewidth=2, 
                                  label=f'{sage_label} (z={z_best:.2f})')]
            ax.legend(handles=custom_legend, fontsize=8, loc='lower left', frameon=False)
    
    # Remove empty subplots
    for i in range(n_bins, len(axes_flat)):
        fig.delaxes(axes_flat[i])
    
    # Set common labels
    for i in range(n_bins):
        row = i // n_cols
        col = i % n_cols
        
        # Bottom row gets x-labels
        if row == n_rows - 1 or i >= n_bins - n_cols:
            axes_flat[i].set_xlabel(r'$\log_{10}(M_*/M_{\odot})$', fontsize=14)
        
        # Left column gets y-labels
        if col == 0:
            axes_flat[i].set_ylabel(r'$\log_{10}(\phi/\mathrm{Mpc}^{-3}\,\mathrm{dex}^{-1})$', fontsize=14)
    
    plt.tight_layout()
    
    # Add overall title
    galaxy_type_str = galaxy_types if isinstance(galaxy_types, str) else 'selected'
    obs_str = " vs Observations" if show_observations else ""
    
    if save_path:
        plt.savefig(save_path, dpi=300, bbox_inches='tight')
        print(f"Saved plot to {save_path}")
    
    #plt.show()
    
    return fig, axes


def add_observational_data_with_baldry(ax, z_low, z_high, obs_data):
    """
    Add observational data to the plot, including Baldry 2008 data for lowest redshift bin
    
    Parameters:
    -----------
    ax : matplotlib axis
        Plot axis
    z_low, z_high : float
        Redshift bin boundaries  
    obs_data : dict
        Observational data dictionary
        
    Returns:
    --------
    bool : True if observational data was added, False otherwise
    """
    obs_added = False
    
    # Add Baldry 2008 data for the lowest redshift bin
    if is_lowest_redshift_bin(z_low, z_high):
        try:
            Baldry_xval, Baldry_yval, Baldry_yvalU, Baldry_yvalL = get_baldry_2008_data()
            
            # Convert to log space for plotting
            log_phi_upper = np.log10(Baldry_yvalU)
            log_phi_lower = np.log10(Baldry_yvalL)
            log_phi_center = np.log10(Baldry_yval)
            
            # Plot filled region
            ax.fill_between(Baldry_xval, log_phi_upper, log_phi_lower, 
                           facecolor='purple', alpha=0.25, label='Baldry et al. 2008 (z~0.1)')
            
            # Plot center line for legend
            ax.plot(Baldry_xval, log_phi_center, color='purple', alpha=0.7, linewidth=1)
            
            obs_added = True
            print(f"  Added Baldry 2008 data to combined low-z bin {z_low:.1f} < z < {z_high:.1f}")
            
        except Exception as e:
            print(f"Warning: Could not add Baldry 2008 data: {e}")
    
    # Add other observational data if available
    obs_bin = match_redshift_bin_to_obs(z_low, z_high, obs_data)
    
    if obs_bin is not None and obs_bin in obs_data:
        # Get observational data for this bin
        obs_masses = obs_data[obs_bin]['M_star']
        obs_phi = obs_data[obs_bin]['Phi']
        obs_phi_err = obs_data[obs_bin]['dPhi']
        
        # Filter out zero or negative values for log plotting
        mask = (obs_phi > 0) & (obs_masses > 0)
        
        if np.any(mask):
            obs_masses_plot = obs_masses[mask]
            obs_phi_plot = obs_phi[mask]
            obs_phi_err_plot = obs_phi_err[mask]
            
            # Convert to log space
            log_phi = np.log10(obs_phi_plot)
            log_phi_err = obs_phi_err_plot / (obs_phi_plot * np.log(10))
            
            # Plot observational data
            ax.errorbar(obs_masses_plot, log_phi, 
                       yerr=log_phi_err,
                       fmt='s-', color='blue', markersize=3, linewidth=1.5,
                       label='COSMOS Web', alpha=0.8, capsize=2)
            
            obs_added = True
    
    return obs_added


def create_simple_smf_plot(snap_list=None):
    """
    Create a simple SMF plot for one or more snapshots
    """
    available_snapshots = get_available_snapshots()
    
    if snap_list is None:
        # Use a few representative snapshots
        if len(available_snapshots) > 5:
            step = len(available_snapshots) // 5
            snap_list = available_snapshots[::step]
        else:
            snap_list = available_snapshots
    
    plt.figure(figsize=(10, 8))
    
    colors = plt.cm.viridis(np.linspace(0, 1, len(snap_list)))
    
    for i, snap in enumerate(snap_list):
        snap_str = f'Snap_{snap}'
        
        try:
            stellar_mass = read_hdf(snap_num=snap_str, param='StellarMass')
            if stellar_mass is None:
                continue
                
            # Convert to solar masses
            stellar_mass = stellar_mass * 1.0e10 / Hubble_h
            
            # Only positive masses
            stellar_mass = stellar_mass[stellar_mass > 0]
            
            z = get_redshift_from_snapshot(snap)
            
            # Calculate SMF
            bin_centers, phi, phi_err = calculate_smf(stellar_mass)
            
            # Plot
            mask = phi > 0
            if np.any(mask):
                plt.errorbar(np.log10(bin_centers[mask]), np.log10(phi[mask]),
                            yerr=phi_err[mask]/(phi[mask]*np.log(10)),
                            fmt='o-', color=colors[i], label=f'z={z:.1f}', alpha=0.8)
                            
        except Exception as e:
            print(f"Warning: Could not process snapshot {snap}: {e}")
            continue
    
    plt.xlabel(r'$\log_{10}(M_*/M_{\odot})$')
    plt.ylabel(r'$\log_{10}(\phi/\mathrm{Mpc}^{-3}\,\mathrm{dex}^{-1})$')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    #plt.show()


# Example usage and main execution
if __name__ == "__main__":
    print("SAGE SMF Analysis with Redshift Bins and Observational Data")
    print("=" * 60)
    
    # Check if data directory exists
    if not os.path.exists(DirName):
        print(f"Error: Directory {DirName} does not exist!")
        print("Please update the DirName variable to point to your SAGE output directory.")
        exit(1)
    
    # Check for model files
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    
    if not model_files:
        print(f"Error: No model files found in {DirName}")
        print("Please check that SAGE has run successfully and produced HDF5 output.")
        exit(1)
    
    print(f"Found {len(model_files)} model files in {DirName}")
    
    try:
        # First create a simple SMF plot to test
        print("\nCreating simple SMF plot...")
        create_simple_smf_plot()
        
        print("\nCreating SMF redshift grid with observational data...")
        # Create the main SMF grid plot
        fig1, axes1 = plot_smf_redshift_grid(
            galaxy_types='all',
            mass_range=(8, 12),
            save_path='sage_smf_redshift_grid_all' + OutputFormat,
            sage_color='red',
            sage_label='SAGE'
        )
        
        # Create separate plots for central and satellite galaxies
        print("Creating central galaxies SMF grid...")
        fig2, axes2 = plot_smf_redshift_grid(
            galaxy_types='central',
            mass_range=(8, 12),
            save_path='sage_smf_redshift_grid_central' + OutputFormat,
            sage_color='darkred',
            sage_label='SAGE Central'
        )
        
        print("Creating satellite galaxies SMF grid...")
        fig3, axes3 = plot_smf_redshift_grid(
            galaxy_types='satellite',
            mass_range=(8, 12),
            save_path='sage_smf_redshift_grid_satellite' + OutputFormat,
            sage_color='orange',
            sage_label='SAGE Satellite'
        )
        
        print("SMF redshift grid analysis complete!")
        print("Generated plots:")
        print("1. sage_smf_redshift_grid_all.png - All galaxies vs observations (with Baldry 2008)")
        print("2. sage_smf_redshift_grid_central.png - Central galaxies vs observations (with Baldry 2008)")
        print("3. sage_smf_redshift_grid_satellite.png - Satellite galaxies vs observations (with Baldry 2008)")
        
    except Exception as e:
        print(f"Error during analysis: {e}")
        import traceback
        traceback.print_exc()