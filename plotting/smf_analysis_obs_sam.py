#!/usr/bin/env python3
"""
SAGE Stellar Mass Function Analysis - Redshift Binned SMF Grid with Multiple Models
==================================================================================

This script uses your existing SAGE data reading approach and adds redshift binning
for creating SMF grids, now with integrated observational data from the ECSV file
and Baldry et al. 2008 data for the lowest redshift bin.

MODIFIED: Now supports comparing multiple SAGE model runs/directories with different box sizes.
FIXED: Resolved duplicate SHARK entries and missing z=1.17 data issues.
FIXED: Legend now only appears when new data is plotted for the first time.
ADDED: Support for SMFvals CSV data with error bounds (plotted as symbols only).

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
# Define multiple directories and their properties
MODEL_CONFIGS = [
    {
        'name': 'SAGE 2.0',           # Display name for legend
        'dir': './output/millennium/',  # Directory path
        'color': 'red',              # Color for plotting
        'linestyle': '-',            # Line style
        'linewidth': 3,              # Thick line for SAGE 2.0
        'alpha': 0.8,                # Transparency
        'boxsize': 62.5,             # Box size in h^-1 Mpc for this model
        'volume_fraction': 1.0       # Fraction of the full volume output by the model
    },
    {
        'name': 'Sage Vanilla',           # Display name for legend
        'dir': './output/millennium_vanilla/',  # Second directory path
        'color': 'black',             # Different color
        'linestyle': '-',            # Thin solid line style
        'linewidth': 2,              # Thin line for Vanilla SAGE
        'alpha': 0.8,                # Transparency
        'boxsize': 62.5,             # Box size in h^-1 Mpc for this model
        'volume_fraction': 1.0       # Fraction of the full volume output by the model
    }
]

# List of observational CSV files, one per redshift
# Note: SMFvals data will be plotted as symbols only (no lines), using squares by default
OBSERVATIONAL_FILES = [
    {'file': './data/SHARK_smf_z0.csv', 'z': 0.0, 'color': 'orange', 'label': 'SHARK', 'linestyle': ':', 'linewidth': 1, 'type': 'shark'},
    {'file': './data/SHARK_smf_z05.csv', 'z': 0.5, 'color': 'orange', 'label': 'SHARK', 'linestyle': ':', 'linewidth': 1, 'type': 'shark'},
    {'file': './data/SHARK_smf_z1.csv', 'z': 1.0, 'color': 'orange', 'label': 'SHARK', 'linestyle': ':', 'linewidth': 1, 'type': 'shark'},
    {'file': './data/SHARK_smf_z2.csv', 'z': 2.0, 'color': 'orange', 'label': 'SHARK', 'linestyle': ':', 'linewidth': 1, 'type': 'shark'},
    {'file': './data/SHARK_smf_z3.csv', 'z': 3.0, 'color': 'orange', 'label': 'SHARK', 'linestyle': ':', 'linewidth': 1, 'type': 'shark'},
    {'file': './data/SHARK_smf_z4.csv', 'z': 4.0, 'color': 'orange', 'label': 'SHARK', 'linestyle': ':', 'linewidth': 1, 'type': 'shark'},
    # Add your new SMFvals files here (plotted as symbols only)
    {'file': './data/Thorne21/SMFvals_z2.csv', 'z': 2.0, 'color': 'grey', 'label': 'Thorne+21', 'marker': 's', 'markersize': 4, 'type': 'smfvals'},
    {'file': './data/Thorne21/SMFvals_z2.4.csv', 'z': 2.4, 'color': 'grey', 'label': 'Thorne+21', 'marker': 's', 'markersize': 4, 'type': 'smfvals'},
    {'file': './data/Thorne21/SMFvals_z3.csv', 'z': 3.0, 'color': 'grey', 'label': 'Thorne+21', 'marker': 's', 'markersize': 4, 'type': 'smfvals'},
    {'file': './data/Thorne21/SMFvals_z3.5.csv', 'z': 3.5, 'color': 'grey', 'label': 'Thorne+21', 'marker': 's', 'markersize': 4, 'type': 'smfvals'},
    {'file': './data/Thorne21/SMFvals_z4.csv', 'z': 4.0, 'color': 'grey', 'label': 'Thorne+21', 'marker': 's', 'markersize': 4, 'type': 'smfvals'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_1.5z2.0_total.txt', 'z': 1.75, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_2.0z2.5_total.txt', 'z': 2.25, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_2.5z3.0_total.txt', 'z': 2.75, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_3.0z3.5_total.txt', 'z': 3.25, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_3.5z4.5_total.txt', 'z': 4.0, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_4.5z5.5_total.txt', 'z': 5.0, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_5.5z6.5_total.txt', 'z': 6.0, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
    {'file': './data/COSMOS2020/SMF_Farmer_v2.1_6.5z7.5_total.txt', 'z': 7.0, 'color': 'grey', 'label': 'Weaver+23', 'marker': 'D', 'markersize': 5, 'type': 'farmer'},
]

FileName = 'model_0.hdf5'
OutputDir = './output/millennium/plots/'
ObsDataFile = './data/SMF_data_points.ecsv'  # Path to observational data file
MuzzinDataFile = './data/SMF_Muzzin2013.dat'  # Path to Muzzin 2013 data file
SantiniDataFile = './data/SMF_Santini2012.dat'  # Path to Santini 2012 data file
WrightDataFile = './data/Wright18_CombinedSMF.dat'  # Path to Wright 2018 data file


# Simulation details
Hubble_h = 0.73        # Hubble parameter
# BoxSize removed - now model-specific
# VolumeFraction removed - now model-specific

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


def get_model_volume(model_config):
    """
    Calculate volume for a specific model configuration
    
    Parameters:
    -----------
    model_config : dict
        Model configuration dictionary containing 'boxsize' and 'volume_fraction'
        
    Returns:
    --------
    float : Volume in (Mpc/h)^3
    """
    boxsize = model_config['boxsize']
    volume_fraction = model_config['volume_fraction']
    volume = (boxsize/Hubble_h)**3.0 * volume_fraction
    return volume

def load_obs_from_file(file_path, file_type='shark'):
    """
    Loads observational SMF data from CSV files.
    Supports different formats:
    - 'shark': format with log_mass, phi (2 columns)
    - 'smfvals': format with logMstar, phi, phi_16, phi_84 (4 columns)
    - 'farmer': format with logMstar, bin_width, phi, phi_lower, phi_upper (5 columns)
    """
    print(f"Attempting to load observational data from: {file_path}")
    
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"File not found: {file_path}")
    
    try:
        # Try multiple delimiters and skip different numbers of header rows
        data = None
        for delimiter in [',', '\t', ' ']:
            for skip_header in [0, 1, 2]:
                try:
                    data = np.genfromtxt(file_path, delimiter=delimiter, skip_header=skip_header)
                    if data.ndim == 2 and data.shape[1] >= 2:
                        print(f"  Successfully loaded with delimiter='{delimiter}', skip_header={skip_header}")
                        print(f"  Data shape: {data.shape}")
                        break
                except:
                    continue
            if data is not None and data.ndim == 2:
                break
        
        if data is None or data.ndim != 2:
            raise ValueError("Could not parse file with any delimiter/header combination")
        
        if file_type == 'farmer' and data.shape[1] >= 5:
            # Farmer format: logMstar, bin_width, phi, phi_lower, phi_upper
            x = data[:, 0]  # logMstar
            bin_width = data[:, 1]  # bin_width (usually 0.25)
            y = data[:, 2]  # phi (central value)
            y_lower = data[:, 3]  # phi_lower (lower bound)
            y_upper = data[:, 4]  # phi_upper (upper bound)
            
            print(f"  Farmer format detected - 5 columns")
            print(f"  Raw data - x range: {np.min(x):.3f} to {np.max(x):.3f}")
            print(f"  Raw data - phi range: {np.min(y):.3e} to {np.max(y):.3e}")
            print(f"  Bin width: {np.unique(bin_width)} dex")
            
            # Check for finite values
            mask = np.isfinite(x) & np.isfinite(y) & np.isfinite(y_lower) & np.isfinite(y_upper)
            print(f"  Valid points: {np.sum(mask)} / {len(x)}")
            
            if np.sum(mask) == 0:
                raise ValueError("No valid (finite) data points found")
            
            return x[mask], y[mask], y_lower[mask], y_upper[mask]
        
        elif file_type == 'smfvals' and data.shape[1] >= 4:
            # SMFvals format: logMstar, phi, phi_16, phi_84
            x = data[:, 0]  # logMstar
            y = data[:, 1]  # phi
            y_lower = data[:, 2]  # phi_16 (lower bound)
            y_upper = data[:, 3]  # phi_84 (upper bound)
            
            print(f"  SMFvals format detected - 4 columns")
            print(f"  Raw data - x range: {np.min(x):.3f} to {np.max(x):.3f}")
            print(f"  Raw data - phi range: {np.min(y):.3e} to {np.max(y):.3e}")
            
            # Check for finite values
            mask = np.isfinite(x) & np.isfinite(y) & np.isfinite(y_lower) & np.isfinite(y_upper)
            print(f"  Valid points: {np.sum(mask)} / {len(x)}")
            
            if np.sum(mask) == 0:
                raise ValueError("No valid (finite) data points found")
            
            return x[mask], y[mask], y_lower[mask], y_upper[mask]
            
        else:
            # SHARK format: log_mass, phi (2 columns)
            x = data[:, 0]
            y = data[:, 1]
            
            print(f"  SHARK format detected - 2 columns")
            print(f"  Raw data - x range: {np.min(x):.3f} to {np.max(x):.3f}")
            print(f"  Raw data - y range: {np.min(y):.3e} to {np.max(y):.3e}")
            
            # Check for finite values
            mask = np.isfinite(x) & np.isfinite(y)
            print(f"  Valid points: {np.sum(mask)} / {len(x)}")
            
            if np.sum(mask) == 0:
                raise ValueError("No valid (finite) data points found")
            
            return x[mask], y[mask]
        
    except Exception as e:
        print(f"  Error loading file: {e}")
        raise

# Pre-load observational data into dictionary keyed by redshift
obs_data_by_z = {}
print("Loading observational CSV files...")

for obs in OBSERVATIONAL_FILES:
    print(f"\nProcessing {obs['file']}...")
    try:
        file_type = obs.get('type', 'shark')
        
        if file_type == 'farmer':
            x, y, y_lower, y_upper = load_obs_from_file(obs['file'], file_type='farmer')
            obs_data_by_z[obs['z']] = {
                'x': x,
                'y': y,
                'y_lower': y_lower,
                'y_upper': y_upper,
                'label': obs['label'],
                'color': obs['color'],
                'marker': obs.get('marker', 'D'),
                'markersize': obs.get('markersize', 5),
                'type': file_type
            }
        elif file_type == 'smfvals':
            x, y, y_lower, y_upper = load_obs_from_file(obs['file'], file_type='smfvals')
            obs_data_by_z[obs['z']] = {
                'x': x,
                'y': y,
                'y_lower': y_lower,
                'y_upper': y_upper,
                'label': obs['label'],
                'color': obs['color'],
                'marker': obs.get('marker', 's'),
                'markersize': obs.get('markersize', 6),
                'type': file_type
            }
        else:
            x, y = load_obs_from_file(obs['file'], file_type='shark')
            obs_data_by_z[obs['z']] = {
                'x': x,
                'y': y,
                'label': obs['label'],
                'color': obs['color'],
                'type': file_type
            }
        
        print(f"✓ Loaded {obs['label']} with {len(x)} data points at z={obs['z']}")
        print(f"  Final data - mass range: {np.min(x):.2f} to {np.max(x):.2f}")
        print(f"  Final data - phi range: {np.min(y):.2e} to {np.max(y):.2e}")
        
    except Exception as e:
        print(f"✗ Warning: Could not load observational file {obs['file']}: {e}")

print(f"\nTotal observational datasets loaded: {len(obs_data_by_z)}")
for z, info in obs_data_by_z.items():
    print(f"  z={z}: {len(info['x'])} points, label='{info['label']}'")

def load_muzzin_2013_data(filename=None):
    """
    Load Muzzin et al. 2013 stellar mass function data
    
    Returns:
    --------
    dict : Dictionary with redshift bin strings as keys and data as values
    """
    if filename is None:
        filename = MuzzinDataFile
    
    if not os.path.exists(filename):
        print(f"Warning: Muzzin 2013 data file {filename} not found!")
        return {}
    
    muzzin_data = {}
    
    try:
        print(f"Loading Muzzin et al. 2013 data from {filename}")
        
        # Read the file, skipping comment lines
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Filter out comment lines and empty lines
        data_lines = []
        for line in lines:
            line = line.strip()
            if line and not line.startswith('#'):
                data_lines.append(line)
        
        # Parse each data line
        for line in data_lines:
            parts = line.split()
            if len(parts) < 5:  # Need at least z_low, z_high, M_star, E_Mstar, logPhi
                continue
                
            try:
                z_low = float(parts[0])
                z_high = float(parts[1])
                M_star = float(parts[2])
                E_Mstar = float(parts[3])
                logPhi = float(parts[4])
                
                # Skip invalid data points
                if logPhi == -99 or logPhi < -10:
                    continue
                
                # Create redshift bin string
                z_bin = f"{z_low} < z < {z_high}"
                
                # Initialize bin if not exists
                if z_bin not in muzzin_data:
                    muzzin_data[z_bin] = {
                        'M_star': [],
                        'logPhi': [],
                        'z_center': (z_low + z_high) / 2
                    }
                
                muzzin_data[z_bin]['M_star'].append(M_star)
                muzzin_data[z_bin]['logPhi'].append(logPhi)
                
            except (ValueError, IndexError):
                continue
        
        # Convert lists to numpy arrays and calculate phi values
        for z_bin in muzzin_data:
            muzzin_data[z_bin]['M_star'] = np.array(muzzin_data[z_bin]['M_star'])
            muzzin_data[z_bin]['logPhi'] = np.array(muzzin_data[z_bin]['logPhi'])
            
            # Convert from log(phi) to phi and apply cosmology correction
            # Muzzin uses H0=70, Omega_lambda=0.7, Omega_matter=0.3
            # Need to convert to our cosmology (H0=73)
            h_muzzin = 0.7
            h_ours = Hubble_h
            
            # Convert phi values (volume correction)
            phi_muzzin = 10**muzzin_data[z_bin]['logPhi']
            phi_corrected = phi_muzzin * (h_muzzin/h_ours)**3
            
            # Apply IMF correction if needed
            M_star_corrected = muzzin_data[z_bin]['M_star'].copy()
            if whichimf == 1:
                # Muzzin assumes Kroupa IMF, convert to Chabrier
                # Muzzin note: M(Kroupa) = M(Chabrier) + 0.04 dex
                M_star_corrected = M_star_corrected - 0.04
            
            muzzin_data[z_bin]['M_star'] = M_star_corrected
            muzzin_data[z_bin]['Phi'] = phi_corrected
            muzzin_data[z_bin]['logPhi'] = np.log10(phi_corrected)
        
        print(f"Loaded Muzzin 2013 data for {len(muzzin_data)} redshift bins:")
        for z_bin in sorted(muzzin_data.keys()):
            n_points = len(muzzin_data[z_bin]['M_star'])
            z_center = muzzin_data[z_bin]['z_center']
            print(f"  {z_bin} (z_center={z_center:.2f}): {n_points} data points")
        
        return muzzin_data
        
    except Exception as e:
        print(f"Error loading Muzzin 2013 data: {e}")
        import traceback
        traceback.print_exc()
        return {}

def load_santini_2012_data(filename=None):
    """
    Load Santini et al. 2012 stellar mass function data
    
    Returns:
    --------
    dict : Dictionary with redshift bin strings as keys and data as values
    """
    if filename is None:
        filename = SantiniDataFile
    
    if not os.path.exists(filename):
        print(f"Warning: Santini 2012 data file {filename} not found!")
        return {}
    
    santini_data = {}
    
    try:
        print(f"Loading Santini et al. 2012 data from {filename}")
        
        # Read the file, skipping comment lines
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Filter out comment lines and empty lines
        data_lines = []
        for line in lines:
            line = line.strip()
            if line and not line.startswith('#'):
                data_lines.append(line)
        
        # Parse each data line
        for line in data_lines:
            parts = line.split()
            if len(parts) < 7:  # Need z_low, z_high, lg_mass, lg_phi, error_hi, error_lo, z_low2, z_high2
                continue
                
            try:
                z_low = float(parts[0])
                z_high = float(parts[1])
                lg_mass = float(parts[2])
                lg_phi = float(parts[3])
                error_hi = float(parts[4])
                error_lo = float(parts[5])
                # parts[6] and [7] are duplicate z_low and z_high values
                
                # Debug print for first few lines
                if len(santini_data) < 3:
                    print(f"  Debug: z_low={z_low}, z_high={z_high}, lg_mass={lg_mass}, lg_phi={lg_phi}")
                
                # Skip invalid data points
                if lg_phi < -10 or not np.isfinite(lg_phi):
                    continue
                
                # Create redshift bin string
                z_bin = f"{z_low} < z < {z_high}"
                
                # Initialize bin if not exists
                if z_bin not in santini_data:
                    santini_data[z_bin] = {
                        'M_star': [],
                        'logPhi': [],
                        'error_hi': [],
                        'error_lo': [],
                        'z_center': (z_low + z_high) / 2
                    }
                
                # Apply cosmology correction (Santini uses H0=70, we use H0=73)
                h_santini = 0.7
                h_ours = Hubble_h
                
                # Convert phi values (volume correction)
                phi_santini = 10**lg_phi
                phi_corrected = phi_santini * (h_santini/h_ours)**3
                lg_phi_corrected = np.log10(phi_corrected)
                
                # Apply IMF correction if needed
                # Santini uses Salpeter IMF, masses for Chabrier are lower by -0.24 dex
                M_star_corrected = lg_mass
                if whichimf == 1:
                    # Convert from Salpeter to Chabrier
                    M_star_corrected = lg_mass - 0.24
                
                santini_data[z_bin]['M_star'].append(M_star_corrected)
                santini_data[z_bin]['logPhi'].append(lg_phi_corrected)
                santini_data[z_bin]['error_hi'].append(error_hi)
                santini_data[z_bin]['error_lo'].append(error_lo)
                
            except (ValueError, IndexError):
                continue
        
        # Convert lists to numpy arrays and calculate phi values
        for z_bin in santini_data:
            santini_data[z_bin]['M_star'] = np.array(santini_data[z_bin]['M_star'])
            santini_data[z_bin]['logPhi'] = np.array(santini_data[z_bin]['logPhi'])
            santini_data[z_bin]['error_hi'] = np.array(santini_data[z_bin]['error_hi'])
            santini_data[z_bin]['error_lo'] = np.array(santini_data[z_bin]['error_lo'])
            santini_data[z_bin]['Phi'] = 10**santini_data[z_bin]['logPhi']
        
        print(f"Loaded Santini 2012 data for {len(santini_data)} redshift bins:")
        for z_bin in sorted(santini_data.keys()):
            n_points = len(santini_data[z_bin]['M_star'])
            z_center = santini_data[z_bin]['z_center']
            print(f"  {z_bin} (z_center={z_center:.2f}): {n_points} data points")
        
        return santini_data
        
    except Exception as e:
        print(f"Error loading Santini 2012 data: {e}")
        import traceback
        traceback.print_exc()
        return {}

def load_wright_2018_data(filename=None):
    """
    Load Wright et al. 2018 stellar mass function data
    
    Returns:
    --------
    dict : Dictionary with redshift values as keys and data as values
    """
    if filename is None:
        filename = WrightDataFile
    
    if not os.path.exists(filename):
        print(f"Warning: Wright 2018 data file {filename} not found!")
        return {}
    
    wright_data = {}
    
    try:
        print(f"Loading Wright et al. 2018 data from {filename}")
        
        # Read the file, skipping comment lines
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Filter out comment lines and empty lines
        data_lines = []
        for line in lines:
            line = line.strip()
            if line and not line.startswith('#'):
                data_lines.append(line)
        
        # Parse each data line
        for line in data_lines:
            parts = line.split()
            if len(parts) < 6:  # Need med_redshift, x, log_y, dlog_yu, dlog_yd, ycv
                continue
                
            try:
                med_redshift = float(parts[0])
                stellar_mass = float(parts[1])
                log_y = float(parts[2])
                dlog_yu = float(parts[3])
                dlog_yd = float(parts[4])
                ycv = float(parts[5])
                
                # Skip invalid data points
                if log_y < -10 or not np.isfinite(log_y):
                    continue
                
                # Apply bin size correction to convert from 0.25 dex bins to 1 dex^-1
                # log_y is in log10(Mpc^-3) with 0.25 dex bins
                # To get Mpc^-3 dex^-1, add -log10(0.25) = +0.602
                log_y_corrected = log_y + np.log10(1.0/0.25)  # Add log10(4) = 0.602
                
                # Initialize redshift bin if not exists
                if med_redshift not in wright_data:
                    wright_data[med_redshift] = {
                        'M_star': [],
                        'logPhi': [],
                        'error_hi': [],
                        'error_lo': []
                    }
                
                # Apply cosmology correction (Wright uses H0=70, we use H0=73)
                h_wright = 0.7
                h_ours = Hubble_h
                
                # Convert phi values (volume correction)
                phi_wright = 10**log_y_corrected
                phi_corrected = phi_wright * (h_wright/h_ours)**3
                log_phi_corrected = np.log10(phi_corrected)
                
                # Apply IMF correction if needed
                # Wright uses Chabrier IMF, same as our setup
                M_star_corrected = stellar_mass
                if whichimf == 1:
                    # No correction needed - Wright already uses Chabrier
                    pass
                
                wright_data[med_redshift]['M_star'].append(M_star_corrected)
                wright_data[med_redshift]['logPhi'].append(log_phi_corrected)
                wright_data[med_redshift]['error_hi'].append(dlog_yu)
                wright_data[med_redshift]['error_lo'].append(dlog_yd)
                
            except (ValueError, IndexError):
                continue
        
        # Convert lists to numpy arrays and calculate phi values
        for redshift in wright_data:
            wright_data[redshift]['M_star'] = np.array(wright_data[redshift]['M_star'])
            wright_data[redshift]['logPhi'] = np.array(wright_data[redshift]['logPhi'])
            wright_data[redshift]['error_hi'] = np.array(wright_data[redshift]['error_hi'])
            wright_data[redshift]['error_lo'] = np.array(wright_data[redshift]['error_lo'])
            wright_data[redshift]['Phi'] = 10**wright_data[redshift]['logPhi']
        
        print(f"Loaded Wright 2018 data for {len(wright_data)} redshift bins:")
        for redshift in sorted(wright_data.keys()):
            n_points = len(wright_data[redshift]['M_star'])
            print(f"  z={redshift}: {n_points} data points")
        
        return wright_data
        
    except Exception as e:
        print(f"Error loading Wright 2018 data: {e}")
        import traceback
        traceback.print_exc()
        return {}
    
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


def find_closest_redshift_in_range(target_z, z_low, z_high, tolerance=0.3):
    """
    Find if target redshift is close enough to the range to be included
    
    Parameters:
    -----------
    target_z : float
        Target redshift from CSV data
    z_low, z_high : float
        Redshift bin boundaries
    tolerance : float
        How far outside the range we allow
        
    Returns:
    --------
    bool : True if redshift should be included in this bin
    """
    # Check if exactly in range
    if z_low <= target_z <= z_high:
        return True
    
    # Check if close to range boundaries
    if abs(target_z - z_low) <= tolerance or abs(target_z - z_high) <= tolerance:
        return True
    
    # Check if target is close to center of range
    z_center = (z_low + z_high) / 2
    if abs(target_z - z_center) <= tolerance:
        return True
        
    return False


def read_hdf(directory, filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files - MODIFIED to accept directory parameter"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        try:
            with h5.File(os.path.join(directory, model_file), 'r') as property_file:
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


def get_available_snapshots(directory):
    """Get list of available snapshots from the data files - MODIFIED to accept directory parameter"""
    model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    
    if not model_files:
        return []
    
    # Check first file for available snapshots
    try:
        with h5.File(os.path.join(directory, model_files[0]), 'r') as f:
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
        print(f"Error reading snapshot list from {directory}: {e}")
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


def calculate_smf(stellar_masses, volume):
    """
    Calculate stellar mass function using the exact same approach as the standalone script
    
    Parameters:
    -----------
    stellar_masses : array
        Array of stellar masses
    volume : float
        Volume for normalization (model-specific)
    """
    
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
    
    # Calculate phi (exact same normalization as standalone script)
    phi = counts / (volume * binwidth)
    
    # Poisson errors (same as standalone)
    phi_err = np.sqrt(counts) / (volume * binwidth)
    
    # Handle zeros - set to upper limit (same as standalone)
    zero_mask = counts == 0
    phi_err[zero_mask] = 1.0 / (volume * binwidth)
    
    # Return bin centers in log space (for plotting) and phi values
    return xaxeshisto, phi, phi_err

def add_observational_data_with_baldry(ax, z_low, z_high, obs_data, muzzin_data, santini_data, wright_data, obs_datasets_in_legend):
    """
    Add observational data to the plot, including Baldry 2008 data for lowest redshift bin
    and Muzzin 2013 data for appropriate redshift bins
    
    MODIFIED: Only add labels for dataset TYPES that haven't appeared in any previous legend
    Each dataset type (COSMOS Web, SHARK, Muzzin+2013, etc.) only gets labeled once in the entire figure
    
    Parameters:
    -----------
    ax : matplotlib axis
        Plot axis
    z_low, z_high : float
        Redshift bin boundaries  
    obs_data : dict
        Observational data dictionary from ECSV file
    muzzin_data : dict
        Muzzin 2013 data dictionary
    santini_data : dict
        Santini 2012 data dictionary
    wright_data : dict
        Wright 2018 data dictionary
    obs_datasets_in_legend : dict
        Tracking dictionary for which dataset TYPES have been added to legends globally
        
    Returns:
    --------
    dict : Dictionary containing information about what observational data was added
    """
    obs_added_info = {
        'baldry': False,
        'muzzin': None,
        'santini': None,
        'shark': None,
        'cosmos': None,
        'wright': None,
        'smfvals': None,  # Add this line
        'farmer': None    # Add this line
    }
    
    # Add Baldry 2008 data for the lowest redshift bin
    if is_lowest_redshift_bin(z_low, z_high):
        try:
            Baldry_xval, Baldry_yval, Baldry_yvalU, Baldry_yvalL = get_baldry_2008_data()
            
            # Convert to log space for plotting
            log_phi_upper = np.log10(Baldry_yvalU)
            log_phi_lower = np.log10(Baldry_yvalL)
            log_phi_center = np.log10(Baldry_yval)
            
            # Only add label if this is the first time Baldry appears anywhere in the figure
            baldry_label = 'Baldry et al. 2008 (z~0.1)' if not obs_datasets_in_legend['baldry'] else None
            
            # Plot filled region
            ax.fill_between(Baldry_xval, log_phi_upper, log_phi_lower, 
                           facecolor='purple', alpha=0.25, label=baldry_label)
            
            # Plot center line for legend
            ax.plot(Baldry_xval, log_phi_center, color='purple', alpha=0.7, linewidth=1)
            
            obs_added_info['baldry'] = True
            if baldry_label is not None:
                obs_datasets_in_legend['baldry'] = True
            print(f"  Added Baldry 2008 data to combined low-z bin {z_low:.1f} < z < {z_high:.1f}")
            
        except Exception as e:
            print(f"Warning: Could not add Baldry 2008 data: {e}")
    
    # Add Muzzin 2013 data - find best matching redshift bin
    print(f"  Checking Muzzin 2013 data for bin {z_low:.1f} < z < {z_high:.1f}")
    
    muzzin_bin = match_redshift_bin_to_obs(z_low, z_high, muzzin_data)
    
    if muzzin_bin is not None and muzzin_bin in muzzin_data:
        try:
            muzzin_masses = muzzin_data[muzzin_bin]['M_star']
            muzzin_phi = muzzin_data[muzzin_bin]['Phi']
            muzzin_logphi = muzzin_data[muzzin_bin]['logPhi']
            
            # Filter out zero or negative values for log plotting
            mask = (muzzin_phi > 0) & (muzzin_masses > 0)
            
            if np.any(mask):
                muzzin_masses_plot = muzzin_masses[mask]
                muzzin_logphi_plot = muzzin_logphi[mask]
                
                # Only add label if Muzzin dataset type hasn't been labeled anywhere in the figure yet
                muzzin_label = 'Muzzin+2013' if not obs_datasets_in_legend['muzzin'] else None
                
                # Plot Muzzin 2013 data - POINTS ONLY, NO LINES
                ax.plot(muzzin_masses_plot, muzzin_logphi_plot, 
                       's', color='grey', markersize=6, 
                       label=muzzin_label, alpha=0.8)
                
                obs_added_info['muzzin'] = muzzin_bin
                if muzzin_label is not None:
                    obs_datasets_in_legend['muzzin'] = True
                print(f"  ✓ Added Muzzin 2013 data for bin {muzzin_bin}")
                print(f"    -> {np.sum(mask)} data points plotted")
                
        except Exception as e:
            print(f"Warning: Could not add Muzzin 2013 data: {e}")
            import traceback
            traceback.print_exc()
    else:
        print(f"    No Muzzin 2013 data matches bin {z_low:.1f} < z < {z_high:.1f}")

    # Add Santini 2012 data - find best matching redshift bin
    print(f"  Checking Santini 2012 data for bin {z_low:.1f} < z < {z_high:.1f}")
    
    santini_bin = match_redshift_bin_to_obs(z_low, z_high, santini_data)
    
    if santini_bin is not None and santini_bin in santini_data:
        try:
            santini_masses = santini_data[santini_bin]['M_star']
            santini_phi = santini_data[santini_bin]['Phi']
            santini_logphi = santini_data[santini_bin]['logPhi']
            santini_error_hi = santini_data[santini_bin]['error_hi']
            santini_error_lo = santini_data[santini_bin]['error_lo']
            
            # Filter out zero or negative values for log plotting
            mask = (santini_phi > 0) & (santini_masses > 0)
            
            if np.any(mask):
                santini_masses_plot = santini_masses[mask]
                santini_logphi_plot = santini_logphi[mask]
                santini_error_hi_plot = santini_error_hi[mask]
                santini_error_lo_plot = santini_error_lo[mask]
                
                # Only add label if Santini dataset type hasn't been labeled anywhere in the figure yet
                santini_label = 'Santini+2012' if not obs_datasets_in_legend['santini'] else None
                
                # Plot Santini 2012 data - POINTS WITH ERROR BARS, NO LINES
                ax.errorbar(santini_masses_plot, santini_logphi_plot, 
                           yerr=[santini_error_lo_plot, santini_error_hi_plot],
                           fmt='^', color='grey', markersize=5, 
                           label=santini_label, alpha=0.8, capsize=2)
                
                obs_added_info['santini'] = santini_bin
                if santini_label is not None:
                    obs_datasets_in_legend['santini'] = True
                print(f"  ✓ Added Santini 2012 data for bin {santini_bin}")
                print(f"    -> {np.sum(mask)} data points plotted")
                
        except Exception as e:
            print(f"Warning: Could not add Santini 2012 data: {e}")
            import traceback
            traceback.print_exc()
    else:
        print(f"    No Santini 2012 data matches bin {z_low:.1f} < z < {z_high:.1f}")
    

    # Add CSV observational data (SHARK files) - BEST MATCH ONLY
    print(f"  Checking SHARK data for bin {z_low:.1f} < z < {z_high:.1f}")
    print(f"  Available SHARK redshifts: {list(obs_data_by_z.keys())}")
    
    # Find the best matching SHARK redshift for this bin
    z_center = (z_low + z_high) / 2
    best_shark_z = None
    best_distance = float('inf')
    
    # Look for SHARK data only
    shark_data = {z: data for z, data in obs_data_by_z.items() if data.get('type') == 'shark'}
    
    for obs_z in shark_data.keys():
        if find_closest_redshift_in_range(obs_z, z_low, z_high):
            # Calculate distance to bin center
            distance = abs(obs_z - z_center)
            if distance < best_distance:
                best_distance = distance
                best_shark_z = obs_z
    
    # Plot only the best matching SHARK data
    if best_shark_z is not None:
        obs_info = obs_data_by_z[best_shark_z]
        print(f"    Selected best match: SHARK z={best_shark_z} (distance to center: {best_distance:.3f})")
        
        try:
            obs_masses = obs_info['x']
            obs_phi = obs_info['y']  # SHARK data already in log space
            
            print(f"      -> Data loaded: {len(obs_masses)} points")
            print(f"      -> Mass range: {np.min(obs_masses):.2f} to {np.max(obs_masses):.2f}")
            print(f"      -> Phi range: {np.min(obs_phi):.2f} to {np.max(obs_phi):.2f}")
            
            # NO FILTERING - plot all points as is, NO LOG10 conversion (already in log space)
            print(f"      -> Plotting ALL {len(obs_masses)} points (no filtering, no log conversion)")
            
            # Only add label if SHARK dataset type hasn't been labeled anywhere in the figure yet
            shark_label = f"{obs_info['label']}" if not obs_datasets_in_legend['shark'] else None
            
            # Plot observational data - SHARK as DASHED LINES ONLY
            ax.plot(obs_masses, obs_phi, 
                   linestyle=obs_info.get('linestyle', '--'), color=obs_info['color'], 
                   linewidth=obs_info.get('linewidth', 2),
                   label=shark_label, alpha=0.8)
            
            obs_added_info['shark'] = best_shark_z
            if shark_label is not None:
                obs_datasets_in_legend['shark'] = True
            print(f"  ✓ Successfully added {obs_info['label']} (z={best_shark_z}) to bin {z_low:.1f} < z < {z_high:.1f}")
                
        except Exception as e:
            print(f"Warning: Could not add {obs_info['label']} data: {e}")
            import traceback
            traceback.print_exc()
    else:
        print(f"    No SHARK data matches bin {z_low:.1f} < z < {z_high:.1f}")

    # Add SMFvals data - find best matching redshift
    print(f"  Checking SMFvals data for bin {z_low:.1f} < z < {z_high:.1f}")
    
    # Find the best matching SMFvals redshift for this bin
    z_center = (z_low + z_high) / 2
    best_smfvals_z = None
    best_distance = float('inf')
    
    # Look for SMFvals data
    smfvals_data = {z: data for z, data in obs_data_by_z.items() if data.get('type') == 'smfvals'}
    
    for obs_z in smfvals_data.keys():
        if find_closest_redshift_in_range(obs_z, z_low, z_high):
            distance = abs(obs_z - z_center)
            if distance < best_distance:
                best_distance = distance
                best_smfvals_z = obs_z
    
    # Plot SMFvals data if found
    if best_smfvals_z is not None:
        obs_info = obs_data_by_z[best_smfvals_z]
        print(f"    Selected best match: SMFvals z={best_smfvals_z} (distance to center: {best_distance:.3f})")
        
        try:
            obs_masses = obs_info['x']
            obs_phi = obs_info['y']
            
            # Check if we have error bounds
            if 'y_lower' in obs_info and 'y_upper' in obs_info:
                obs_phi_lower = obs_info['y_lower']
                obs_phi_upper = obs_info['y_upper']
                
                # Convert to log space for plotting
                mask = (obs_phi > 0) & (obs_phi_lower > 0) & (obs_phi_upper > 0)
                if np.any(mask):
                    obs_masses_plot = obs_masses[mask]
                    obs_phi_log = np.log10(obs_phi[mask])
                    obs_phi_lower_log = np.log10(obs_phi_lower[mask])
                    obs_phi_upper_log = np.log10(obs_phi_upper[mask])
                    
                    # Calculate VERTICAL error bars in log space
                    # phi_16 and phi_84 are 16th and 84th percentiles of phi (y-axis uncertainty)
                    yerr_lower = obs_phi_log - obs_phi_lower_log  # Lower error bar length
                    yerr_upper = obs_phi_upper_log - obs_phi_log  # Upper error bar length
                    
                    print(f"      -> Plotting with VERTICAL error bars")
                    print(f"      -> Central phi: {np.mean(obs_phi_log):.3f}")
                    print(f"      -> Average lower error: {np.mean(yerr_lower):.3f}")
                    print(f"      -> Average upper error: {np.mean(yerr_upper):.3f}")
                    
                    # Only add label if SMFvals dataset type hasn't been labeled anywhere in the figure yet
                    smfvals_label = f"{obs_info['label']}" if not obs_datasets_in_legend['smfvals'] else None
                    
                    # Get marker style from config
                    marker_style = obs_info.get('marker', 's')  # default to squares
                    marker_size = obs_info.get('markersize', 6)
                    
                    # Plot with VERTICAL error bars (yerr parameter) - SYMBOLS ONLY, NO LINES
                    ax.errorbar(obs_masses_plot, obs_phi_log, 
                               yerr=[yerr_lower, yerr_upper],  # VERTICAL error bars for phi uncertainty
                               xerr=None,  # NO horizontal error bars
                               fmt=marker_style, color=obs_info['color'], markersize=marker_size,
                               label=smfvals_label, alpha=0.8, capsize=3, linewidth=0,
                               elinewidth=1.5)  # Make error bar lines a bit thicker
                    
                    print(f"  ✓ Successfully plotted {obs_info['label']} with error bars (z={best_smfvals_z})")
            else:
                # No error bounds, plot as simple symbols
                mask = obs_phi > 0
                if np.any(mask):
                    obs_masses_plot = obs_masses[mask]
                    obs_phi_log = np.log10(obs_phi[mask])
                    
                    # Only add label if SMFvals dataset type hasn't been labeled anywhere in the figure yet
                    smfvals_label = f"{obs_info['label']}" if not obs_datasets_in_legend['smfvals'] else None
                    
                    # Get marker style from config
                    marker_style = obs_info.get('marker', 'o')  # default to circles
                    marker_size = obs_info.get('markersize', 6)
                    
                    # Plot as symbols only - NO LINES
                    ax.plot(obs_masses_plot, obs_phi_log, 
                           marker_style, color=obs_info['color'], 
                           label=smfvals_label, alpha=0.8, markersize=marker_size)
                    
                    print(f"  ✓ Successfully plotted {obs_info['label']} (z={best_smfvals_z})")
            
            obs_added_info['smfvals'] = best_smfvals_z
            if smfvals_label is not None:
                obs_datasets_in_legend['smfvals'] = True
                
        except Exception as e:
            print(f"Warning: Could not add {obs_info['label']} data: {e}")
            import traceback
            traceback.print_exc()
    else:
        print(f"    No SMFvals data matches bin {z_low:.1f} < z < {z_high:.1f}")

    # Add Farmer data - find best matching redshift
    print(f"  Checking Farmer data for bin {z_low:.1f} < z < {z_high:.1f}")
    
    # Find the best matching Farmer redshift for this bin
    z_center = (z_low + z_high) / 2
    best_farmer_z = None
    best_distance = float('inf')
    
    # Look for Farmer data
    farmer_data = {z: data for z, data in obs_data_by_z.items() if data.get('type') == 'farmer'}
    
    for obs_z in farmer_data.keys():
        if find_closest_redshift_in_range(obs_z, z_low, z_high):
            distance = abs(obs_z - z_center)
            if distance < best_distance:
                best_distance = distance
                best_farmer_z = obs_z
    
    # Plot Farmer data if found
    if best_farmer_z is not None:
        obs_info = obs_data_by_z[best_farmer_z]
        print(f"    Selected best match: Farmer z={best_farmer_z} (distance to center: {best_distance:.3f})")
        
        try:
            obs_masses = obs_info['x']
            obs_phi = obs_info['y']
            
            # Check if we have error bounds
            if 'y_lower' in obs_info and 'y_upper' in obs_info:
                obs_phi_lower = obs_info['y_lower']
                obs_phi_upper = obs_info['y_upper']
                
                # Convert to log space for plotting
                mask = (obs_phi > 0) & (obs_phi_lower > 0) & (obs_phi_upper > 0)
                if np.any(mask):
                    obs_masses_plot = obs_masses[mask]
                    obs_phi_log = np.log10(obs_phi[mask])
                    obs_phi_lower_log = np.log10(obs_phi_lower[mask])
                    obs_phi_upper_log = np.log10(obs_phi_upper[mask])
                    
                    # Calculate VERTICAL error bars in log space
                    # Farmer phi_lower and phi_upper are absolute bounds, not percentiles
                    yerr_lower = obs_phi_log - obs_phi_lower_log  # Lower error bar length
                    yerr_upper = obs_phi_upper_log - obs_phi_log  # Upper error bar length
                    
                    print(f"      -> Plotting with VERTICAL error bars")
                    print(f"      -> Central phi: {np.mean(obs_phi_log):.3f}")
                    print(f"      -> Average lower error: {np.mean(yerr_lower):.3f}")
                    print(f"      -> Average upper error: {np.mean(yerr_upper):.3f}")
                    
                    # Only add label if Farmer dataset type hasn't been labeled anywhere in the figure yet
                    farmer_label = f"{obs_info['label']}" if not obs_datasets_in_legend['farmer'] else None
                    
                    # Get marker style from config
                    marker_style = obs_info.get('marker', 'D')  # default to diamonds
                    marker_size = obs_info.get('markersize', 5)
                    
                    # Plot with VERTICAL error bars (yerr parameter) - SYMBOLS ONLY, NO LINES
                    ax.errorbar(obs_masses_plot, obs_phi_log, 
                               yerr=[yerr_lower, yerr_upper],  # VERTICAL error bars for phi uncertainty
                               xerr=None,  # NO horizontal error bars
                               fmt=marker_style, color=obs_info['color'], markersize=marker_size,
                               label=farmer_label, alpha=0.8, capsize=3, linewidth=0,
                               elinewidth=1.5)  # Make error bar lines a bit thicker
                    
                    print(f"  ✓ Successfully plotted {obs_info['label']} with error bars (z={best_farmer_z})")
            else:
                # No error bounds, plot as simple symbols
                mask = obs_phi > 0
                if np.any(mask):
                    obs_masses_plot = obs_masses[mask]
                    obs_phi_log = np.log10(obs_phi[mask])
                    
                    # Only add label if Farmer dataset type hasn't been labeled anywhere in the figure yet
                    farmer_label = f"{obs_info['label']}" if not obs_datasets_in_legend['farmer'] else None
                    
                    # Get marker style from config
                    marker_style = obs_info.get('marker', 'D')  # default to diamonds
                    marker_size = obs_info.get('markersize', 5)
                    
                    # Plot as symbols only - NO LINES
                    ax.plot(obs_masses_plot, obs_phi_log, 
                           marker_style, color=obs_info['color'], 
                           label=farmer_label, alpha=0.8, markersize=marker_size)
                    
                    print(f"  ✓ Successfully plotted {obs_info['label']} (z={best_farmer_z})")
            
            obs_added_info['farmer'] = best_farmer_z
            if farmer_label is not None:
                obs_datasets_in_legend['farmer'] = True
                
        except Exception as e:
            print(f"Warning: Could not add {obs_info['label']} data: {e}")
            import traceback
            traceback.print_exc()
    else:
        print(f"    No Farmer data matches bin {z_low:.1f} < z < {z_high:.1f}")
    
    # Add other observational data from ECSV file if available
    obs_bin = match_redshift_bin_to_obs(z_low, z_high, obs_data)
    
    if obs_bin is not None and obs_bin in obs_data:
        # Get observational data for this bin
        obs_masses = obs_data[obs_bin]['M_star']
        obs_phi = obs_data[obs_bin]['Phi']
        obs_phi_err = obs_data[obs_bin]['dPhi']
        
        # Filter out zero or negative values for log plotting (still needed for ECSV data)
        mask = (obs_phi > 0) & (obs_masses > 0)
        
        if np.any(mask):
            obs_masses_plot = obs_masses[mask]
            obs_phi_plot = obs_phi[mask]
            obs_phi_err_plot = obs_phi_err[mask]
            
            # Convert to log space
            log_phi = np.log10(obs_phi_plot)
            log_phi_err = obs_phi_err_plot / (obs_phi_plot * np.log(10))
            
            # Only add label if COSMOS dataset type hasn't been labeled anywhere in the figure yet
            cosmos_label = 'COSMOS Web' if not obs_datasets_in_legend['cosmos'] else None
            
            # Plot observational data - POINTS ONLY, NO LINES
            ax.errorbar(obs_masses_plot, log_phi, 
                       yerr=log_phi_err,
                       fmt='o', color='grey', markersize=4,
                       label=cosmos_label, alpha=0.8, capsize=2)
            
            obs_added_info['cosmos'] = obs_bin
            if cosmos_label is not None:
                obs_datasets_in_legend['cosmos'] = True
            print(f"  Added COSMOS Web data for bin {obs_bin}")
    
    # Add Wright 2018 data - find best matching redshift
    print(f"  Checking Wright 2018 data for bin {z_low:.1f} < z < {z_high:.1f}")
    
    # Find the best matching Wright redshift for this bin
    z_center = (z_low + z_high) / 2
    best_wright_z = None
    best_distance = float('inf')
    
    for wright_z in wright_data.keys():
        if z_low <= wright_z <= z_high:
            # Calculate distance to bin center
            distance = abs(wright_z - z_center)
            if distance < best_distance:
                best_distance = distance
                best_wright_z = wright_z
    
    # If no exact match, find closest within tolerance
    if best_wright_z is None:
        tolerance = 0.3
        for wright_z in wright_data.keys():
            distance = abs(wright_z - z_center)
            if distance <= tolerance and distance < best_distance:
                best_distance = distance
                best_wright_z = wright_z
    
    # Plot Wright 2018 data if found
    if best_wright_z is not None:
        try:
            wright_masses = wright_data[best_wright_z]['M_star']
            wright_phi = wright_data[best_wright_z]['Phi']
            wright_logphi = wright_data[best_wright_z]['logPhi']
            wright_error_hi = wright_data[best_wright_z]['error_hi']
            wright_error_lo = wright_data[best_wright_z]['error_lo']
            
            # Filter out zero or negative values for log plotting
            mask = (wright_phi > 0) & (wright_masses > 0)
            
            if np.any(mask):
                wright_masses_plot = wright_masses[mask]
                wright_logphi_plot = wright_logphi[mask]
                wright_error_hi_plot = wright_error_hi[mask]
                wright_error_lo_plot = wright_error_lo[mask]
                
                # Only add label if Wright dataset type hasn't been labeled anywhere in the figure yet
                wright_label = 'Wright+2018' if not obs_datasets_in_legend['wright'] else None
                
                # Plot Wright 2018 data - DIAMONDS WITH ERROR BARS
                ax.errorbar(wright_masses_plot, wright_logphi_plot, 
                           yerr=[wright_error_lo_plot, wright_error_hi_plot],
                           fmt='D', color='grey', markersize=4, 
                           label=wright_label, alpha=0.8, capsize=2)
                
                obs_added_info['wright'] = best_wright_z
                if wright_label is not None:
                    obs_datasets_in_legend['wright'] = True
                print(f"  ✓ Added Wright 2018 data for z={best_wright_z}")
                print(f"    -> {np.sum(mask)} data points plotted")
                
        except Exception as e:
            print(f"Warning: Could not add Wright 2018 data: {e}")
            import traceback
            traceback.print_exc()
    else:
        print(f"    No Wright 2018 data matches bin {z_low:.1f} < z < {z_high:.1f}")
    
    return obs_added_info


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
                          save_path=None, show_observations=True):
    """
    Create a grid plot of stellar mass functions for redshift bins - ENHANCED with all observational datasets
    and FIXED legend handling to only show new data
    """
    # Load observational data
    obs_data = {}
    muzzin_data = {}
    santini_data = {}
    wright_data = {}
    
    # Track which observational dataset TYPES have appeared in legends globally
    # MODIFIED: Simple boolean tracking - each dataset type only gets labeled once in entire figure
    obs_datasets_in_legend = {
        'baldry': False,
        'muzzin': False,
        'santini': False,
        'shark': False,
        'cosmos': False,
        'wright': False,
        'smfvals': False,  # Add this new dataset type
        'farmer': False   # Add this new dataset type
    }
    
    # Track which models have appeared in legends globally
    models_in_legend = set()
    
    if show_observations:
        obs_data = load_observational_data()
        muzzin_data = load_muzzin_2013_data()
        santini_data = load_santini_2012_data()
        wright_data = load_wright_2018_data()
        
        if not obs_data and not muzzin_data and not santini_data and not wright_data:
            print("Warning: No observational data loaded. Proceeding without observations.")
            show_observations = False
        else:
            print(f"Loaded {len(obs_data)} ECSV bins, {len(muzzin_data)} Muzzin 2013 bins, and {len(santini_data)} Santini 2012 bins, and {len(wright_data)} Wright 2018 bins")

    # Check that all model directories exist and get available snapshots
    model_snapshots = {}
    model_configs_valid = []
    for model_config in MODEL_CONFIGS:
        directory = model_config['dir']
        model_name = model_config['name']
        
        if not os.path.exists(directory):
            print(f"Error: Directory {directory} does not exist!")
            continue
            
        available_snapshots = get_available_snapshots(directory)
        if not available_snapshots:
            print(f"Warning: No snapshots found in {directory}")
            continue
            
        model_snapshots[model_name] = available_snapshots
        model_configs_valid.append(model_config)
        
        # Print model info including box size
        boxsize = model_config['boxsize']
        volume_fraction = model_config['volume_fraction']
        volume = get_model_volume(model_config)
        print(f"Found {len(available_snapshots)} snapshots in {directory} for {model_name}")
        print(f"  Box size: {boxsize} h^-1 Mpc, Volume fraction: {volume_fraction}, Total volume: {volume:.2e} (Mpc/h)^3")
    
    if not model_snapshots:
        raise ValueError("No valid model directories with snapshots found")
    
    # Use snapshots from first valid model to create redshift bins
    first_model_snapshots = list(model_snapshots.values())[0]
    redshift_bins = create_redshift_bins(first_model_snapshots)
    
    if not redshift_bins:
        raise ValueError("No redshift bins with data found")
    
    print(f"Created {len(redshift_bins)} redshift bins with data")
    
    # Set up the plot grid (3 columns x 5 rows to match typical SMF studies)
    n_bins = len(redshift_bins)
    n_cols = 3
    n_rows = 5
    
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(20, 32), 
                           sharex=True, sharey=True)
    
    # Flatten axes for easier indexing
    if n_rows == 1:
        axes = axes.reshape(1, -1)
    axes_flat = axes.flatten()
    
    for i, (z_low, z_high, z_center, snapshots) in enumerate(redshift_bins):
        if i >= len(axes_flat):
            break
            
        ax = axes_flat[i]
        
        print(f"Processing redshift bin {z_low:.1f} < z < {z_high:.1f} with {len(snapshots)} snapshots")
        
        # Track what observational data gets plotted in this panel
        obs_added_info = {'baldry': False, 'muzzin': None, 'santini': None, 'shark': None, 'cosmos': None, 'wright': None, 'smfvals': None, 'farmer': None}
        
        # Add observational data first (so it appears behind SAGE data)
        if show_observations:
            obs_added_info = add_observational_data_with_baldry(ax, z_low, z_high, obs_data, muzzin_data, santini_data, wright_data, obs_datasets_in_legend)
        
        # Process each model
        model_redshifts_used = {}  # Track which redshift each model used in this panel
        
        for model_idx, model_config in enumerate(model_configs_valid):
            model_name = model_config['name']
            directory = model_config['dir']
            color = model_config['color']
            linestyle = model_config['linestyle']
            linewidth = model_config.get('linewidth', 2)
            alpha = model_config['alpha']
            
            # Get model-specific volume
            volume = get_model_volume(model_config)
            
            # Skip if this model doesn't have data
            if model_name not in model_snapshots:
                continue
            
            try:
                # Find the best snapshot for this redshift bin from this model
                available_snaps = model_snapshots[model_name]
                target_z = z_low
                best_snap = None
                min_diff = float('inf')
                
                # Find snapshots in this bin for this model
                snapshots_in_bin = []
                for snap in available_snaps:
                    z_snap = get_redshift_from_snapshot(snap)
                    if z_low <= z_snap < z_high:
                        snapshots_in_bin.append(snap)
                
                if not snapshots_in_bin:
                    print(f"  No snapshots found for {model_name} in this redshift bin")
                    continue
                
                # Use the same logic as original script to pick the best snapshot
                if is_lowest_redshift_bin(z_low, z_high):
                    # Use z=0 snapshot if available
                    if 63 in snapshots_in_bin:
                        best_snap = 63
                    else:
                        best_snap = snapshots_in_bin[0]  # Use first available
                else:
                    # Find snapshot with redshift just above the lower bound
                    for snap in snapshots_in_bin:
                        z_snap = get_redshift_from_snapshot(snap)
                        if z_snap >= target_z:
                            diff = z_snap - target_z
                            if diff < min_diff:
                                min_diff = diff
                                best_snap = snap
                    
                    # If no snapshot found above target_z, use closest
                    if best_snap is None:
                        for snap in snapshots_in_bin:
                            z_snap = get_redshift_from_snapshot(snap)
                            diff = abs(z_snap - target_z)
                            if diff < min_diff:
                                min_diff = diff
                                best_snap = snap
                
                if best_snap is None:
                    continue
                
                snap_str = f'Snap_{best_snap}'
                z_best = get_redshift_from_snapshot(best_snap)
                model_redshifts_used[model_name] = z_best

                # Read data for the selected snapshot
                stellar_mass = read_hdf(directory, snap_num=snap_str, param='StellarMass')
                galaxy_type = read_hdf(directory, snap_num=snap_str, param='Type')
                
                if stellar_mass is None or galaxy_type is None:
                    print(f"  Could not read data for {model_name} Snap_{best_snap}")
                    continue
                
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
                
                print(f"  {model_name}: Using Snap_{best_snap} (z={z_best:.2f}): {len(filtered_masses)} galaxies, Volume={volume:.2e} (Mpc/h)^3")
                
                if len(filtered_masses) == 0:
                    continue
                
                # Calculate SMF with model-specific volume
                xaxeshisto, phi, phi_err = calculate_smf(filtered_masses, volume=volume)
                
                # Plot SAGE data
                mask_plot = phi > 0
                if np.any(mask_plot):
                    phi_log = np.log10(phi[mask_plot])
                    phi_err_log = phi_err[mask_plot] / (phi[mask_plot] * np.log(10))
                    
                    # MODIFIED: Only create label for models that haven't appeared in any previous legend
                    model_label = None
                    if model_name not in models_in_legend:
                        model_label = f'{model_name} (z={z_best:.2f})'
                        models_in_legend.add(model_name)
                    
                    # SAGE models: LINES ONLY, NO POINTS
                    ax.plot(xaxeshisto[mask_plot], phi_log, 
                           color=color, linestyle=linestyle, linewidth=linewidth,
                           label=model_label, alpha=alpha)
                
                # Add upper limits for zero counts
                mask_upper = phi == 0
                if np.any(mask_upper):
                    phi_upper = np.log10(phi_err[mask_upper])
                    ax.plot(xaxeshisto[mask_upper], phi_upper, 'v', 
                           color=color, markersize=4, alpha=alpha*0.7)
                
            except Exception as e:
                print(f"Warning: Could not process {model_name} for this redshift bin: {e}")
                continue
        
        # Formatting
        ax.set_title(f'{z_low:.1f} < z < {z_high:.1f}', fontsize=14)
        ax.set_xlim(mass_range)
        ax.set_ylim(-6, -1)
        
        # MODIFIED LEGEND HANDLING: Only show legend if there are new items to show
        legend_handles, legend_labels = ax.get_legend_handles_labels()
        
        if legend_handles:  # Only add legend if there are items with labels
            # Choose legend location based on panel position
            row = i // n_cols
            is_last_row = (row == n_rows - 1) or (i >= n_bins - n_cols)
            legend_location = 'upper right' if is_last_row else 'lower left'
            
            ax.legend(fontsize=12, loc=legend_location, frameon=False)
            print(f"  Panel {i}: Added legend with {len(legend_handles)} items")
        else:
            print(f"  Panel {i}: No legend items - skipping legend")
    
    # Remove empty subplots
    for i in range(n_bins, len(axes_flat)):
        fig.delaxes(axes_flat[i])
    
    # Set common labels
    for i in range(n_bins):
        row = i // n_cols
        col = i % n_cols
        
        # Bottom row gets x-labels
        if row == n_rows - 1 or i >= n_bins - n_cols:
            axes_flat[i].set_xlabel(r'$\log_{10}(M_*/M_{\odot})$', fontsize=16)
        
        # Left column gets y-labels
        if col == 0:
            axes_flat[i].set_ylabel(r'$\log_{10}(\phi/\mathrm{Mpc}^{-3}\,\mathrm{dex}^{-1})$', fontsize=16)
    
    plt.tight_layout()
    
    # Add overall title
    galaxy_type_str = galaxy_types if isinstance(galaxy_types, str) else 'selected'
    obs_str = " vs Observations (Baldry+2008, Muzzin+2013, Santini+2012, Wright+2018, SMFvals, Farmer v2.1)" if show_observations else ""
    
    if save_path:
        plt.savefig(save_path, dpi=300, bbox_inches='tight')
        print(f"Saved plot to {save_path}")
    
    return fig, axes


def create_simple_smf_plot(snap_list=None):
    """
    Create a simple SMF plot for one or more snapshots - MODIFIED for multiple models with different box sizes
    """
    plt.figure(figsize=(12, 8))
    
    # Process each model
    for model_idx, model_config in enumerate(MODEL_CONFIGS):
        model_name = model_config['name']
        directory = model_config['dir']
        color = model_config['color']
        linestyle = model_config['linestyle']
        alpha = model_config['alpha']
        
        if not os.path.exists(directory):
            print(f"Warning: Directory {directory} does not exist, skipping {model_name}")
            continue
        
        # Get model-specific volume
        volume = get_model_volume(model_config)
        
        available_snapshots = get_available_snapshots(directory)
        
        if not available_snapshots:
            print(f"Warning: No snapshots found in {directory}")
            continue
        
        if snap_list is None:
            # Use a few representative snapshots
            if len(available_snapshots) > 5:
                step = len(available_snapshots) // 5
                model_snap_list = available_snapshots[::step]
            else:
                model_snap_list = available_snapshots
        else:
            # Use only snapshots that exist in this model
            model_snap_list = [s for s in snap_list if s in available_snapshots]
        
        # Generate colors for different redshifts within this model
        n_snaps = len(model_snap_list)
        if n_snaps > 1:
            alphas = np.linspace(0.4, 1.0, n_snaps)
        else:
            alphas = [alpha]
        
        print(f"Processing {model_name} with volume = {volume:.2e} (Mpc/h)^3")
        
        for i, snap in enumerate(model_snap_list):
            snap_str = f'Snap_{snap}'
            
            try:
                stellar_mass = read_hdf(directory, snap_num=snap_str, param='StellarMass')
                if stellar_mass is None:
                    continue
                    
                # Convert to solar masses
                stellar_mass = stellar_mass * 1.0e10 / Hubble_h
                
                # Only positive masses
                stellar_mass = stellar_mass[stellar_mass > 0]
                
                z = get_redshift_from_snapshot(snap)
                
                # Calculate SMF with model-specific volume
                bin_centers, phi, phi_err = calculate_smf(stellar_mass, volume=volume)
                
                # Plot
                mask = phi > 0
                if np.any(mask):
                    # Create unique label for first snapshot of each model
                    if i == 0:
                        label = f'{model_name} (z={z:.1f})'
                    else:
                        label = f'z={z:.1f}'
                    
                    plt.errorbar(bin_centers[mask], np.log10(phi[mask]),
                                yerr=phi_err[mask]/(phi[mask]*np.log(10)),
                                fmt='o-', color=color, linestyle=linestyle,
                                alpha=alphas[i], label=label, markersize=4)
                                
            except Exception as e:
                print(f"Warning: Could not process {model_name} snapshot {snap}: {e}")
                continue
    
    plt.xlabel(r'$\log_{10}(M_*/M_{\odot})$')
    plt.ylabel(r'$\log_{10}(\phi/\mathrm{Mpc}^{-3}\,\mathrm{dex}^{-1})$')
    plt.legend(fontsize=16, frameon=False)
    plt.tight_layout()


# Example usage and main execution
if __name__ == "__main__":
    print("SAGE SMF Analysis with Multiple Models and Different Box Sizes - Redshift Binned SMF Grid")
    print("=" * 80)
    
    # Check if data directories exist
    valid_models = []
    for model_config in MODEL_CONFIGS:
        directory = model_config['dir']
        model_name = model_config['name']
        boxsize = model_config.get('boxsize', 'Unknown')
        volume_fraction = model_config.get('volume_fraction', 'Unknown')
        
        if not os.path.exists(directory):
            print(f"Warning: Directory {directory} for {model_name} does not exist!")
            continue
        
        # Check for model files
        model_files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
        
        if not model_files:
            print(f"Warning: No model files found in {directory} for {model_name}")
            continue
        
        # Calculate volume for this model
        volume = get_model_volume(model_config)
        
        print(f"✓ Found {len(model_files)} model files in {directory} for {model_name}")
        print(f"  Box size: {boxsize} h^-1 Mpc, Volume fraction: {volume_fraction}")
        print(f"  Total volume: {volume:.2e} (Mpc/h)^3")
        valid_models.append(model_config)
    
    if not valid_models:
        print("Error: No valid model directories found!")
        print("Please update the MODEL_CONFIGS variable to point to your SAGE output directories.")
        print("Make sure to include 'boxsize' and 'volume_fraction' for each model.")
        exit(1)
    
    # Update the global MODEL_CONFIGS to only include valid models
    MODEL_CONFIGS[:] = valid_models
    
    try:
        # First create a simple SMF plot to test
        print("\nCreating simple SMF comparison plot...")
        create_simple_smf_plot()
        
        print("\nCreating SMF redshift grid with multiple models and observational data...")
        # Create the main SMF grid plot
        fig1, axes1 = plot_smf_redshift_grid(
            galaxy_types='all',
            mass_range=(8, 12),
            save_path=OutputDir +'sage_smf_redshift_grid_all' + OutputFormat
        )
        
        # Create separate plots for central and satellite galaxies
        print("Creating central galaxies SMF grid...")
        fig2, axes2 = plot_smf_redshift_grid(
            galaxy_types='central',
            mass_range=(8, 12),
            save_path=OutputDir +'sage_smf_redshift_grid_central' + OutputFormat
        )
        
        print("Creating satellite galaxies SMF grid...")
        fig3, axes3 = plot_smf_redshift_grid(
            galaxy_types='satellite',
            mass_range=(8, 12),
            save_path=OutputDir +'sage_smf_redshift_grid_satellite' + OutputFormat
        )
        
        print("Multi-model SMF redshift grid analysis complete!")
        print("Generated plots:")
        print("1. test_observational_data.png - Test plot of all observational data")
        print("2. sage_smf_redshift_grid_all.png - All galaxies vs observations (multiple models)")
        print("3. sage_smf_redshift_grid_central.png - Central galaxies vs observations (multiple models)")
        print("4. sage_smf_redshift_grid_satellite.png - Satellite galaxies vs observations (multiple models)")
        print("\nObservational datasets included:")
        print("- Baldry et al. 2008 (z~0.1)")
        print("- Muzzin et al. 2013 (various redshift bins)")
        print("- Santini et al. 2012 (various redshift bins)")
        print("- Wright et al. 2018 (z=0.5, 1.0, 2.0, 3.0, 4.0)")
        print("- SHARK model comparisons")
        print("- COSMOS Web data")
        print("- SMFvals data (plotted as dark magenta squares with error bars)")
        print("- Farmer v2.1 data (plotted as dark red diamonds with error bars)")
        
        # Print model comparison summary
        print("\nModel Configuration Summary:")
        for i, model_config in enumerate(MODEL_CONFIGS):
            volume = get_model_volume(model_config)
            print(f"  {i+1}. {model_config['name']}: {model_config['dir']}")
            print(f"     Color: {model_config['color']}, Style: {model_config['linestyle']}")
            print(f"     Box size: {model_config['boxsize']} h^-1 Mpc, Volume: {volume:.2e} (Mpc/h)^3")
        
        print("\nDebugging observational data:")
        print("Available dataset redshifts and corresponding bins:")
        for obs_z in obs_data_by_z.keys():
            data_type = obs_data_by_z[obs_z].get('type', 'unknown')
            label = obs_data_by_z[obs_z].get('label', 'unknown')
            print(f"  {label} ({data_type}) z={obs_z}")
            # Check which bin this would fall into
            redshift_bins = create_redshift_bins(get_available_snapshots(MODEL_CONFIGS[0]['dir']))
            for z_low, z_high, z_center, _ in redshift_bins:
                if find_closest_redshift_in_range(obs_z, z_low, z_high):
                    print(f"    -> Matches bin {z_low:.1f} < z < {z_high:.1f}")
        
    except Exception as e:
        print(f"Error during analysis: {e}")
        import traceback
        traceback.print_exc()