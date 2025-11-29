#!/usr/bin/env python

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
from scipy.ndimage import gaussian_filter
from matplotlib.colors import LinearSegmentedColormap
import os
import argparse
import pandas as pd

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
SnapshotNum = 7  # Snapshot number

# Simulation details
Hubble_h = 0.6774
BoxSize = 400  # h-1 Mpc (full simulation box)
VolumeFraction = 1.0   # Fraction of the full volume output by the model

# Density plot options
slice_thickness = 10  # Mpc/h thickness of the slice (Thick slice for max particle density)
grid_resolution = 15048  # Resolution of the 2D grid (High detail)
smoothing_sigma = 20.0  # Gaussian smoothing kernel size (Increased to blend dots into filaments)
mass_threshold = 0.0  # No threshold - use all galaxies to trace the web

OutputDir = DirName + 'plots/'
OutputFormat = '.png'

# ==================================================================

# Create Uchuu-inspired colormap
uchuu_colors = [
    "#0a3d4d",  # dark teal (voids)
    "#0f4d5e",  # deep teal
    "#15616d",  # medium teal
    "#1b7a8a",  # teal-cyan
    "#2a8f9f",  # bright teal
    "#4a9f8e",  # teal-green
    "#6aaa7a",  # yellow-green transition
    "#8ab566",  # olive-yellow
    "#b5a84f",  # golden
    "#d49a3a",  # orange-gold
    "#e88c2a",  # bright orange (filaments)
    "#f5a623",  # golden-orange (brightest nodes)
]
uchuu_cmap = LinearSegmentedColormap.from_list("uchuu", uchuu_colors, N=256)
# Millennium Simulation snapshot to redshift mapping
# MILLENNIUM_REDSHIFTS = {
#     63: 0.00, 62: 0.04, 61: 0.08, 60: 0.12, 59: 0.16,
#     58: 0.20, 57: 0.24, 56: 0.28, 55: 0.32, 54: 0.37,
#     53: 0.41, 52: 0.46, 51: 0.50, 50: 0.55, 49: 0.60,
#     48: 0.65, 47: 0.70, 46: 0.76, 45: 0.81, 44: 0.87,
#     43: 0.93, 42: 0.99, 41: 1.05, 40: 1.12, 39: 1.19,
#     38: 1.25, 37: 1.33, 36: 1.40, 35: 1.48, 34: 1.56,
#     33: 1.64, 32: 1.73, 31: 1.82, 30: 1.91, 29: 2.07,
#     28: 2.07, 27: 2.18, 26: 2.30, 25: 2.43, 24: 2.57,
#     23: 2.71, 22: 2.86, 21: 3.02, 20: 3.21, 19: 3.44,
#     18: 3.68, 17: 3.95, 16: 4.24, 15: 4.56, 14: 4.93,
#     13: 5.33, 12: 5.78, 11: 6.20, 10: 6.49, 9: 7.24,
#     8: 8.07, 7: 8.99, 6: 10.07, 5: 11.32, 4: 12.79,
#     3: 14.52, 2: 16.58, 1: 19.05, 0: 22.02
# }

MILLENNIUM_REDSHIFTS = {
    0: 13.9334, 1: 12.67409, 2: 11.50797, 3: 10.44649, 4: 9.480752,
    5: 8.58543, 6: 7.77447, 7: 7.032387, 8: 6.344409, 9: 5.721695,
    10: 5.153127, 11: 4.629078, 12: 4.26715, 13: 3.929071, 14: 3.610462,
    15: 3.314082, 16: 3.128427, 17: 2.951226, 18: 2.77809, 19: 2.616166,
    20: 2.458114, 21: 2.309724, 22: 2.16592, 23: 2.027963, 24: 1.8962,
    25: 1.770958, 26: 1.65124, 27: 1.535928, 28: 1.426272, 29: 1.321656,
    30: 1.220303, 31: 1.124166, 32: 1.031983, 33: 0.9441787, 34: 0.8597281,
    35: 0.779046, 36: 0.7020205, 37: 0.6282588, 38: 0.5575475, 39: 0.4899777,
    40: 0.4253644, 41: 0.3640053, 42: 0.3047063, 43: 0.2483865, 44: 0.1939743,
    45: 0.1425568, 46: 0.09296665, 47: 0.0455745, 48: 0.02265383, 49: 0.0001130128
}

def get_redshift(snapshot_num):
    """Get redshift for a given snapshot number"""
    if snapshot_num in MILLENNIUM_REDSHIFTS:
        return MILLENNIUM_REDSHIFTS[snapshot_num]
    else:
        print(f"Warning: Snapshot {snapshot_num} not in standard Millennium list")
        return None

# ==================================================================

def read_hdf(snap_num = None, param = None):
    property = h5.File(DirName+FileName,'r')
    snapshot_key = f'Snap_{snap_num}'
    return np.array(property[snapshot_key][param])

# ==================================================================

def create_density_plot(positions, masses, boxsize, resolution, thickness, smoothing):
    """
    Create a 2D density map from 3D particle positions
    """
    
    # Filter particles within the slice
    z_min, z_max = boxsize/2 - thickness/2, boxsize/2 + thickness/2
    in_slice = (positions[:, 2] >= z_min) & (positions[:, 2] <= z_max)
    
    pos_slice = positions[in_slice]
    mass_slice = masses[in_slice]
    
    print(f'Particles in slice: {len(mass_slice)} of {len(masses)}')
    print(f'Slice thickness: {thickness} Mpc/h (z = {z_min:.1f} to {z_max:.1f})')
    
    if len(mass_slice) == 0:
        print("ERROR: No particles in slice!")
        return None, None, None
    
    # Create 2D histogram weighted by mass
    H, xedges, yedges = np.histogram2d(
        pos_slice[:, 0], 
        pos_slice[:, 1],
        bins=resolution,
        range=[[0, boxsize], [0, boxsize]],
        weights=mass_slice
    )
    
    # Apply Gaussian smoothing
    H_smooth = gaussian_filter(H, sigma=smoothing)
    
    print(f'Density range: {H_smooth.min():.2e} to {H_smooth.max():.2e}')
    print(f'Non-zero pixels: {np.sum(H_smooth > 0)} of {resolution*resolution}')
    
    return H_smooth, xedges, yedges

# ==================================================================

if __name__ == '__main__':
    
    print('Creating cosmic web density plot\n')
    
    # Get redshift for this snapshot
    redshift = get_redshift(SnapshotNum)
    if redshift is not None:
        print(f'Snapshot {SnapshotNum} corresponds to redshift z = {redshift:.2f}\n')
    
    # Since positions span full box, use full BoxSize
    print(f'Box size: {BoxSize} Mpc/h')
    print(f'Volume fraction: {VolumeFraction}')
    print(f'Note: Positions span full box, so using full BoxSize for plot\n')
    
    # Parse command line arguments
    parser = argparse.ArgumentParser(description='Create cosmic web density plot.')
    parser.add_argument('--csv', type=str, help='Path to CSV file with columns: Mass, x, y, z')
    args = parser.parse_args()

    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    if args.csv:
        print(f'Reading galaxy properties from CSV: {args.csv}')
        try:
            df = pd.read_csv(args.csv)
            
            # Normalize column names to handle case sensitivity and variations
            # Map common variations to standard names
            column_map = {
                'M': 'Mass', 'mass': 'Mass',
                'X': 'x', 'Y': 'y', 'Z': 'z'
            }
            df.rename(columns=column_map, inplace=True)
            
            # Check for required columns
            required_cols = ['Mass', 'x', 'y', 'z']
            missing_cols = [col for col in required_cols if col not in df.columns]
            
            if missing_cols:
                print(f"ERROR: CSV is missing columns: {', '.join(missing_cols)}")
                print(f"Found columns: {', '.join(df.columns)}")
                print("Expected columns: Mass (or M), x (or X), y (or Y), z (or Z)")
                exit(1)
            
            StellarMass = df['Mass'].values
            Posx = df['x'].values
            Posy = df['y'].values
            Posz = df['z'].values
            
            # If the CSV filename is used, we might want to update the output filename or title
            # For now, we keep the SnapshotNum based naming or could append CSV name
            print(f"Loaded {len(df)} rows from CSV.")
            
        except Exception as e:
            print(f"Error reading CSV file: {e}")
            exit(1)
            
    else:
        # Read galaxy properties from HDF5
        print('Reading galaxy properties from', DirName+FileName)
        
        StellarMass = read_hdf(snap_num=SnapshotNum, param='Mvir') * 1.0e10 / Hubble_h
        Posx = read_hdf(snap_num=SnapshotNum, param='Posx')
        Posy = read_hdf(snap_num=SnapshotNum, param='Posy')
        Posz = read_hdf(snap_num=SnapshotNum, param='Posz')
    
    print(f'Total galaxies read: {len(StellarMass)}')
    
    # Filter by mass threshold
    mask = StellarMass > mass_threshold
    positions = np.column_stack([Posx[mask], Posy[mask], Posz[mask]])
    masses = StellarMass[mask]
    
    print(f'Using {len(masses)} galaxies above mass threshold ({mass_threshold:.1e} Msun/h)')
    print(f'Position ranges: X=[{Posx.min():.1f}, {Posx.max():.1f}], '
          f'Y=[{Posy.min():.1f}, {Posy.max():.1f}], '
          f'Z=[{Posz.min():.1f}, {Posz.max():.1f}]\n')
    
    # Create density map using full box size since positions span it
    density_map, xedges, yedges = create_density_plot(
        positions, masses, BoxSize, grid_resolution, 
        slice_thickness, smoothing_sigma
    )
    
    if density_map is None:
        print("Failed to create density map")
        exit(1)
    
    # Plotting
    fig, ax = plt.subplots(figsize=(12, 12))
    
    # Use log scale for better visualization
    # Add small value to avoid log(0)
    density_map_log = np.log10(density_map + 1e-1)
    
    # Create the plot with colors similar to the reference
    # Try different colormaps: 'viridis', 'plasma', 'inferno', 'turbo', 'hot', 'YlOrRd'
    im = ax.imshow(
        density_map_log.T,
        origin='lower',
        extent=[0, BoxSize, 0, BoxSize],
        cmap=uchuu_cmap,
        aspect='auto',
        interpolation='gaussian',  # bicubic for even smoother gradients
        vmin=np.percentile(density_map_log[density_map_log > -10], 5),  # Focus on the web structure
        vmax=np.percentile(density_map_log, 99.5)  # Preserve bright nodes
    )
    
    ax.set_xlabel('X [Mpc/h]', fontsize=14)
    ax.set_ylabel('Y [Mpc/h]', fontsize=14)
    
    # Include redshift in title
    if redshift is not None:
        ax.set_title(f'Galaxy Density Distribution (Snapshot {SnapshotNum}, z = {redshift:.2f})', fontsize=16)
    else:
        ax.set_title(f'Galaxy Density Distribution (Snapshot {SnapshotNum})', fontsize=16)
    
    # Add colorbar
    cbar = plt.colorbar(im, ax=ax, label='log10(Stellar Mass Density)', fraction=0.046, pad=0.04)
    
    plt.tight_layout()
    
    # Include snapshot number in filename
    output_filename = f'density_map_snap{SnapshotNum}' + OutputFormat
    plt.savefig(OutputDir + output_filename, dpi=300, bbox_inches='tight')
    print(f'\nSaved density map to {OutputDir}{output_filename}')
    plt.close()