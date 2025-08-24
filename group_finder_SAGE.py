#!/usr/bin/env python3
"""
SAGE 2.0 Group Evolution Tracker - Complete Version
Complete script for analyzing galaxy groups from SAGE 2.0 simulation data with evolution tracking.

This version includes:
1. All original SAGE group finding functionality
2. NEW: Multi-snapshot group evolution tracking
3. NEW: Group merger/splitting detection
4. NEW: Evolution visualization and analysis
5. Full backward compatibility with original script
6. FIXED: Proper cosmological time calculation using actual SAGE redshift values
7. SIMPLIFIED: Unified snapshot arguments with flexible plotting options

Unit Conversions Applied:
- Mass properties (StellarMass, Mvir, ColdGas, etc.): * 1.0e10 / Hubble_h → M_sun
- Distance properties (Rvir, DiskRadius): / Hubble_h → Mpc or kpc
- Other properties retain native units

Cosmological Time Calculation:
- Uses actual SAGE redshift values for snapshots 0-63
- Snapshot 63 = z=0.000 (present day)
- Snapshot 0 = z=127.000 (early universe)
- Proper cosmological integration for lookback times

Plotting Features:
- Spatial distribution plots with snapshot numbers in filenames
- Group properties plots with snapshot numbers in filenames
- Can create plots for multiple snapshots independently
- Performance optimized for large simulation boxes

Usage:
    # Single snapshot analysis
    python sage_group_evolution.py --snapshots 63 --input_file sage_output.hdf5
    
    # Multiple snapshots with evolution tracking
    python sage_group_evolution.py --snapshots 63 50 40 30 --input_file sage_output.hdf5
    
    # Evolution analysis + plots for specific snapshots
    python sage_group_evolution.py --snapshots 63 50 40 30 --plot_snapshots 63 50 --input_file sage_output.hdf5
    
    # Single snapshot with multiple plot outputs
    python sage_group_evolution.py --snapshots 63 --plot_snapshots 63 60 55 --input_file sage_output.hdf5
    
Example usage:
    # Basic single snapshot run
    python sage_group_evolution.py --snapshots 63 --input_file output/model.hdf5
    
    # Track groups from z=0 (snap 63) back to z~1 (snap 40)
    python sage_group_evolution.py --snapshots 63 55 50 45 40 --input_file output/model.hdf5
    
    # Evolution tracking with plots for multiple snapshots
    python sage_group_evolution.py --snapshots 63 50 40 --plot_snapshots 63 50 --input_file output/model.hdf5
    
    # Custom evolution tracking parameters
    python sage_group_evolution.py --snapshots 63 50 40 --track_overlap_fraction 0.3 --track_max_distance 2.0 --input_file output/model.hdf5
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import argparse
import h5py
import sys
from pathlib import Path
from scipy.spatial.distance import cdist
from scipy.integrate import quad

# Progress bar import
try:
    from tqdm import tqdm
    TQDM_AVAILABLE = True
except ImportError:
    print("Warning: tqdm not available. Install with 'pip install tqdm' for progress bars.")
    TQDM_AVAILABLE = False
    # Create a dummy tqdm that just returns the iterator
    def tqdm(iterable, **kwargs):
        return iterable

# ============================================================================
# NEW: COSMOLOGICAL TIME CALCULATIONS
# ============================================================================

def snapshot_to_redshift(snapshot, snap_z0=63):
    """
    Convert SAGE snapshot number to redshift using actual SAGE redshift values.
    
    Parameters:
    snapshot: int, snapshot number (0-63)
    snap_z0: int, snapshot number corresponding to z=0 (should be 63)
    
    Returns:
    redshift: float
    """
    # Actual redshift values for SAGE snapshots 0-63
    sage_redshifts = [
        127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
        9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
        2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
        0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
        0.116, 0.089, 0.064, 0.041, 0.020, 0.000
    ]
    
    # Check if snapshot is valid
    if snapshot < 0 or snapshot >= len(sage_redshifts):
        print(f"Warning: Snapshot {snapshot} out of range (0-{len(sage_redshifts)-1})")
        # Return a reasonable fallback
        if snapshot < 0:
            return sage_redshifts[0]  # Highest redshift
        else:
            return sage_redshifts[-1]  # z=0
    
    return sage_redshifts[snapshot]

def redshift_to_lookback_time(z, H0=70.0, Om0=0.27, OL0=0.73):
    """
    Convert redshift to lookback time using cosmological parameters.
    
    Parameters:
    z: redshift
    H0: Hubble constant in km/s/Mpc
    Om0: matter density parameter
    OL0: dark energy density parameter
    
    Returns:
    lookback_time: float, in Gyr
    """
    # Speed of light in km/s
    c = 299792.458
    
    # Hubble time in Gyr
    t_H = (c / H0) / (3.0857e16 / 3.15576e16)  # Convert to Gyr
    
    def integrand(z_prime):
        """Integrand for lookback time calculation."""
        return 1.0 / ((1 + z_prime) * np.sqrt(Om0 * (1 + z_prime)**3 + OL0))
    
    if z <= 0:
        return 0.0
    
    try:
        integral, _ = quad(integrand, 0, z)
        lookback_time = t_H * integral
        return lookback_time
    except:
        # Fallback approximation for small z
        return t_H * z / np.sqrt(Om0 * (1 + z)**3 + OL0)

def snapshot_to_lookback_time(snapshot, cosmo_params=None):
    """
    Convert snapshot number to lookback time.
    
    Parameters:
    snapshot: int, snapshot number
    cosmo_params: dict with cosmological parameters
    
    Returns:
    lookback_time: float, in Gyr
    """
    if cosmo_params is None:
        cosmo_params = {'H0': 70.0, 'Omega_m': 0.27, 'Omega_Lambda': 0.73}
    
    # Convert snapshot to redshift using actual SAGE values
    z = snapshot_to_redshift(snapshot)
    
    # Convert redshift to lookback time
    lookback_time = redshift_to_lookback_time(z, 
                                            H0=cosmo_params['H0'],
                                            Om0=cosmo_params['Omega_m'], 
                                            OL0=cosmo_params['Omega_Lambda'])
    
    return lookback_time

def get_time_axis_for_snapshots(snapshots, cosmo_params=None):
    """
    Get proper time axis for a list of snapshots.
    
    Parameters:
    snapshots: list of snapshot numbers
    cosmo_params: dict with cosmological parameters
    
    Returns:
    lookback_times: list of lookback times in Gyr
    redshifts: list of redshifts
    """
    if cosmo_params is None:
        cosmo_params = {'H0': 70.0, 'Omega_m': 0.27, 'Omega_Lambda': 0.73}
    
    lookback_times = []
    redshifts = []
    
    for snapshot in snapshots:
        z = snapshot_to_redshift(snapshot)
        t_lookback = redshift_to_lookback_time(z,
                                              H0=cosmo_params['H0'],
                                              Om0=cosmo_params['Omega_m'], 
                                              OL0=cosmo_params['Omega_Lambda'])
        lookback_times.append(t_lookback)
        redshifts.append(z)
    
    return lookback_times, redshifts

def print_snapshot_redshift_mapping(snapshots=None):
    """
    Print the snapshot to redshift mapping for verification.
    """
    if snapshots is None:
        # Show a representative sample
        snapshots = [0, 10, 20, 30, 40, 50, 60, 63]
    
    print("Snapshot → Redshift mapping:")
    for snap in snapshots:
        z = snapshot_to_redshift(snap)
        print(f"  Snapshot {snap:2d}: z = {z:.3f}")
    print()

# ============================================================================
# CORE GROUP FINDING FUNCTIONS (ORIGINAL)
# ============================================================================

def disp_gap(data):
    """
    Compute dispersion estimate using gaps between sorted data points.
    """
    N = len(data)
    if N < 2:
        return np.nan
    data_sort = np.sort(data)
    # compute gaps and weights
    gaps = data_sort[1:] - data_sort[:-1]
    weights = np.arange(1, N) * np.arange(N - 1, 0, -1)
    # Gapper estimate of dispersion
    sigma_gap = np.sqrt(np.pi) / (N * (N - 1)) * np.sum(weights * gaps)
    sigma = np.sqrt((N / (N - 1)) * sigma_gap**2)
    return sigma

def calculate_3d_distance(pos1, pos2):
    """
    Calculate 3D euclidean distance between two positions.
    """
    return np.sqrt(np.sum((pos1 - pos2)**2, axis=1))

def calculate_velocity_from_3d(vel):
    """
    Calculate velocity magnitude from 3D velocity components.
    """
    return np.sqrt(np.sum(vel**2, axis=1))

def S2HM(M_stellar, A=39.788, M1=10.611, beta=0.926, gamma=-0.609):
    """
    Stellar-to-halo mass relation (Moster et al. 2010 style)
    """
    log_M_h = A + beta * M_stellar + gamma * M_stellar**2 + M1
    return log_M_h

def S3HM(M_stellar_3, A=46.944, M1=10.483, beta=0.249, gamma=-0.601):
    """
    3-galaxy stellar-to-halo mass relation
    """
    log_M_h = A + beta * M_stellar_3 + gamma * M_stellar_3**2 + M1
    return log_M_h

def SAGE_Group_Catalog(data, cosmo_params=None, group_id_col='CentralGalaxyIndex', 
                       stellar_mass_min_msun=1e8, stellar_mass_max_msun=1e12, max_group_size=1000,
                       separate_groups_clusters=False, max_group_members=50, max_group_velocity_dispersion=600,
                       show_progress=True):
    """
    Generate group catalog from SAGE 2.0 data with option to separate groups from clusters.
    
    Parameters:
    data: DataFrame with SAGE galaxy data
    cosmo_params: dict with cosmological parameters
    group_id_col: Column name for group identification  
    stellar_mass_min_msun: Minimum stellar mass in M_sun units (used when group_id_col != 'SAGEHaloIndex')
    stellar_mass_max_msun: Maximum stellar mass in M_sun units (used when group_id_col != 'SAGEHaloIndex')
    max_group_size: Maximum allowed group size (filters out unrealistic large groups)
    separate_groups_clusters: If True, separate groups from clusters based on physical criteria
    max_group_members: Maximum members for galaxy groups (vs clusters)
    max_group_velocity_dispersion: Maximum velocity dispersion for groups (km/s)
    show_progress: If True, show progress bar during group processing
    """
    if cosmo_params is None:
        cosmo_params = {'H0': 70.0, 'Omega_m': 0.27, 'Omega_Lambda': 0.73, 'h': 0.7}
    
    h = cosmo_params['h']
    
    # MODIFIED: Check if we're using SAGEHaloIndex for halo-based filtering
    use_halo_mass_filtering = (group_id_col == 'SAGEHaloIndex')
    
    if use_halo_mass_filtering:
        # Use halo mass filtering for SAGEHaloIndex
        mass_min_physical = 1.0e10  # M_sun
        mass_max_physical = 1.0e15  # M_sun
        mass_field = 'Mvir'
        mass_type = 'halo'
        print(f"Using halo mass cuts (SAGEHaloIndex mode): {mass_min_physical:.1e} - {mass_max_physical:.1e} M_sun")
    else:
        # Use stellar mass filtering for other group ID fields
        mass_min_physical = stellar_mass_min_msun
        mass_max_physical = stellar_mass_max_msun
        mass_field = 'StellarMass'
        mass_type = 'stellar'
        print(f"Using stellar mass cuts: {mass_min_physical:.1e} - {mass_max_physical:.1e} M_sun")
    
    # Check if the mass field exists in the data
    if mass_field not in data.columns:
        print(f"Warning: {mass_field} not found in data columns!")
        print(f"Available mass-related columns: {[col for col in data.columns if 'mass' in col.lower() or 'mvir' in col.lower()]}")
        # Fallback to stellar mass if halo mass not available
        if use_halo_mass_filtering and 'StellarMass' in data.columns:
            print("Falling back to stellar mass filtering...")
            mass_field = 'StellarMass'
            mass_min_physical = stellar_mass_min_msun
            mass_max_physical = stellar_mass_max_msun
            mass_type = 'stellar (fallback)'
        else:
            print("Cannot proceed without appropriate mass field!")
            return pd.DataFrame(), pd.DataFrame()
    
    original_count = len(data)
    data_filtered = data[
        (data[mass_field] >= mass_min_physical) & 
        (data[mass_field] <= mass_max_physical)
    ].copy()
    print(f"Kept {len(data_filtered)}/{original_count} galaxies after {mass_type} mass cuts")
    
    # Check group size distribution before processing
    group_sizes = data_filtered.groupby(group_id_col).size()
    print(f"Group size statistics:")
    print(f"  Mean: {group_sizes.mean():.1f}")
    print(f"  Median: {group_sizes.median():.1f}") 
    print(f"  Max: {group_sizes.max()}")
    print(f"  Groups > {max_group_size}: {(group_sizes > max_group_size).sum()}")
    
    if group_sizes.max() > max_group_size:
        print(f"Warning: Some groups are very large (max={group_sizes.max()})")
        print(f"Consider using a different group_id_field or check data integrity")
        # Optionally filter out very large groups
        large_groups = group_sizes[group_sizes > max_group_size].index
        print(f"Filtering out {len(large_groups)} groups with >{max_group_size} members")
        data_filtered = data_filtered[~data_filtered[group_id_col].isin(large_groups)]
    
    group_catalog = []
    
    # Get total number of groups for progress bar
    unique_groups = data_filtered[group_id_col].nunique()
    print(f"Processing {unique_groups} groups...")

    # VELOCITY DISPERSION FIX: Force use of 3D velocity calculation for group velocity dispersion
    # This prevents using individual galaxy VelDisp values (internal stellar motion) 
    # for group velocity dispersion (galaxy-to-galaxy motion)
    # data_filtered = data_filtered.drop('VelDisp', axis=1, errors='ignore')
    
    # Set up progress bar
    group_iterator = data_filtered.groupby(group_id_col)
    if show_progress and TQDM_AVAILABLE:
        group_iterator = tqdm(group_iterator, 
                             total=unique_groups,
                             desc="Processing groups", 
                             unit="groups",
                             ncols=100,
                             ascii=True)
    elif show_progress:
        print("Processing groups (install tqdm for progress bar)...")
    
    processed_groups = 0
    for group_id, group in group_iterator:
        if len(group) < 2:  # Skip single-galaxy "groups"
            continue
            
        processed_groups += 1
        
        # Print progress every 100 groups if tqdm not available
        if not TQDM_AVAILABLE and show_progress and processed_groups % 100 == 0:
            print(f"  Processed {processed_groups}/{unique_groups} groups...")
            
        # Find central galaxy using CentralGalaxyIndex
        # The CentralGalaxyIndex should point to the GalaxyIndex of the central galaxy
        if 'CentralGalaxyIndex' in group.columns and 'GalaxyIndex' in group.columns:
            central_galaxy_idx = group['CentralGalaxyIndex'].iloc[0]  # Should be same for all galaxies in group
            central_candidates = group[group['GalaxyIndex'] == central_galaxy_idx]
            
            if not central_candidates.empty:
                central_galaxy = central_candidates.iloc[0]
            else:
                # Fallback: use Type == 0 or most massive
                type_0_candidates = group[group['Type'] == 0] if 'Type' in group.columns else pd.DataFrame()
                if not type_0_candidates.empty:
                    central_galaxy = type_0_candidates.loc[type_0_candidates['StellarMass'].idxmax()]
                else:
                    central_galaxy = group.loc[group['StellarMass'].idxmax()]
        else:
            # Fallback to original method if CentralGalaxyIndex not available
            central_candidates = group[group['Type'] == 0] if 'Type' in group.columns else pd.DataFrame()
            if not central_candidates.empty:
                central_galaxy = central_candidates.loc[central_candidates['StellarMass'].idxmax()]
            else:
                central_galaxy = group.loc[group['StellarMass'].idxmax()]
        
        # Calculate group center using 3D coordinates
        pos_median = np.array([group['Posx'].median(), 
                              group['Posy'].median(), 
                              group['Posz'].median()])
        
        vel_median = np.array([group['Velx'].median(),
                              group['Vely'].median(), 
                              group['Velz'].median()])
        
        # Use snapshot number as proxy for redshift 
        z_obs_median = group['SnapNum'].median() / 100.0 if 'SnapNum' in group.columns else 0.0
        z_cos_median = z_obs_median
        
        # Get halo properties from central galaxy
        M_200 = central_galaxy.get('Mvir', 0.0)
        R_200 = central_galaxy.get('Rvir', 0.0)
        
        # Calculate velocity dispersion FROM 3D GALAXY VELOCITIES (not individual VelDisp)
        # This gives the velocity dispersion of galaxies within the group, not stellar motion within galaxies
        vel_3d = calculate_velocity_from_3d(group[['Velx', 'Vely', 'Velz']].values)
        vdisp = np.std(vel_3d)
        vdisp_gap = disp_gap(vel_3d)
        
        # Calculate 3D separations from group center
        positions = group[['Posx', 'Posy', 'Posz']].values
        center_pos = np.tile(pos_median, (len(group), 1))
        group_seps = calculate_3d_distance(positions, center_pos)
        
        radius = np.max(group_seps)
        
        # Calculate stellar masses at different thresholds (always use stellar mass for this)
        with np.errstate(divide='ignore', invalid='ignore', over='ignore'):
            # Handle stellar mass units - masses are now in physical M_sun units
            stellar_mass_values = group['StellarMass'].values
            
            # Filter out invalid stellar masses and apply safety limits for stellar mass calculations
            # Note: we use stellar mass limits here regardless of the filtering method
            valid_mass_mask = ((stellar_mass_values >= stellar_mass_min_msun) & 
                              (stellar_mass_values <= stellar_mass_max_msun) & 
                              np.isfinite(stellar_mass_values) &
                              (stellar_mass_values > 0))
            
            if valid_mass_mask.any():
                valid_stellar_masses = stellar_mass_values[valid_mass_mask]
                
                # Calculate total stellar mass in log10(M_sun) units for output
                total_stellar_mass_linear = np.sum(valid_stellar_masses)
                if total_stellar_mass_linear > 0:
                    Sig_M = np.log10(total_stellar_mass_linear)
                else:
                    Sig_M = np.nan
            else:
                Sig_M = np.nan
            
            # Stellar mass above different thresholds (in log10(M_sun) units)
            thresholds = [11, 10.5, 10, 9.5, 9]  # log10(M_sun)
            sig_masses = {}
            for threshold in thresholds:
                # Convert threshold from log to linear for comparison
                linear_threshold = 10**threshold
                mask = (stellar_mass_values > linear_threshold) & valid_mass_mask
                if mask.any():
                    masses_above_thresh = stellar_mass_values[mask]
                    total_mass_above_thresh = np.sum(masses_above_thresh)
                    
                    if total_mass_above_thresh > 0:
                        sig_mass = np.log10(total_mass_above_thresh)
                    else:
                        sig_mass = np.nan
                    
                    sig_masses[f'Sig_M{threshold}'.replace('.', '')] = sig_mass if np.isfinite(sig_mass) else np.nan
                else:
                    sig_masses[f'Sig_M{threshold}'.replace('.', '')] = np.nan
        
        # Central galaxy stellar mass (convert to log10(M_sun) units for output)
        if (stellar_mass_min_msun <= central_galaxy['StellarMass'] <= stellar_mass_max_msun and 
            np.isfinite(central_galaxy['StellarMass']) and central_galaxy['StellarMass'] > 0):
            M_Cen = np.log10(central_galaxy['StellarMass'])
        else:
            M_Cen = np.nan
        
        # Top 3 and 5 most massive galaxies (only use valid masses)
        valid_group = group[valid_mass_mask] if valid_mass_mask.any() else pd.DataFrame()
        
        if len(valid_group) >= 3:
            top_3 = valid_group.nlargest(3, 'StellarMass')
            top3_total_mass = np.sum(top_3['StellarMass'].values)
            if top3_total_mass > 0:
                M_Cen3 = np.log10(top3_total_mass)
            else:
                M_Cen3 = np.nan
        else:
            M_Cen3 = np.nan
            
        if len(valid_group) >= 5:
            top_5 = valid_group.nlargest(5, 'StellarMass')
            top5_total_mass = np.sum(top_5['StellarMass'].values)
            if top5_total_mass > 0:
                M_Cen5 = np.log10(top5_total_mass)
            else:
                M_Cen5 = np.nan
        else:
            M_Cen5 = np.nan
        
        # Dynamical mass estimates using velocity dispersion
        if not np.isnan(vdisp_gap) and vdisp_gap > 0 and radius > 0:
            # Convert units appropriately for dynamical mass estimate
            # M = 5/3 * sigma^2 * R / G (in appropriate units)
            radius_kpc = radius * 1000  # Convert Mpc to kpc if needed
            M_200_Disp = np.log10(5/3 * vdisp_gap**2 * radius_kpc / 4.3)  # Rough dynamical mass
        else:
            M_200_Disp = np.nan
        
        # Alternative dynamical mass using typical scaling
        if not np.isnan(vdisp_gap) and vdisp_gap > 0:
            M_200_Disp_Other = np.log10(1.2e15 * (vdisp_gap / 1000)**3 * 
                                       (1 / np.sqrt(0.7 + 0.3 * (1 + z_obs_median)**3)) / h)
        else:
            M_200_Disp_Other = np.nan
        
        # Velocity dispersion correction (placeholder)
        M_200_Disp_Correction = M_200_Disp  
        
        # Stellar-to-halo mass relations
        if not np.isnan(M_Cen):
            M_200_CM = S2HM(M_Cen)
        else:
            M_200_CM = np.nan
            
        if not np.isnan(M_Cen3):
            M_200_C3M = S3HM(M_Cen3)
        else:
            M_200_C3M = np.nan
        
        # Central galaxy ID
        CID = int(central_galaxy.get('GalaxyIndex', central_galaxy.name))
        
        group_entry = {
            'group_id': group_id,
            'Nm': len(group),
            'x_center': pos_median[0],
            'y_center': pos_median[1], 
            'z_center': pos_median[2],
            'vx_center': vel_median[0],
            'vy_center': vel_median[1],
            'vz_center': vel_median[2],
            'z_obs': z_obs_median,
            'z_cos': z_cos_median,
            'vdisp': vdisp,
            'vdisp_gap': vdisp_gap,
            'radius': radius,
            'M_200': M_200,
            'Mhalo_VT': M_200_Disp,
            'M_200_Disp_Other': M_200_Disp_Other,
            'Mhalo_CVT': M_200_Disp_Correction,
            'Mhalo_PM': M_200_CM,
            'Mhalo_P3M': M_200_C3M,
            'R_200': R_200,
            'Sig_M': Sig_M,
            'M_Cen': M_Cen,
            'M_Cen3': M_Cen3,
            'M_Cen5': M_Cen5,
            'CID': CID
        }
        
        # Add threshold stellar masses
        group_entry.update(sig_masses)
        
        group_catalog.append(group_entry)
    
    # Final progress message if tqdm not available
    if not TQDM_AVAILABLE and show_progress:
        print(f"  Completed processing {processed_groups} groups")
    
    group_catalog = pd.DataFrame(group_catalog)
    
    # NEW: Separate groups from clusters if requested
    if separate_groups_clusters and len(group_catalog) > 0:
        groups_only, clusters_only = filter_groups_from_clusters(
            group_catalog, 
            max_members=max_group_members,
            max_vdisp=max_group_velocity_dispersion
        )
        print(f"Using {len(groups_only)} galaxy groups for analysis (excluded {len(clusters_only)} clusters)")
        final_catalog = groups_only
    else:
        final_catalog = group_catalog
    
    # Merge group catalog columns back into the original filtered data
    if len(final_catalog) > 0:
        merge_cols = ['group_id', 'vdisp_gap', 'radius', 'Mhalo_P3M', 'Mhalo_VT', 'Mhalo_CVT']
        # Only merge columns that exist in the group catalog
        merge_cols = [col for col in merge_cols if col in final_catalog.columns]
        data_merged = data_filtered.merge(final_catalog[merge_cols], left_on=group_id_col, right_on='group_id', how='left')
    else:
        print("No groups found to merge")
        data_merged = data_filtered.copy()
    
    return data_merged, final_catalog


def filter_groups_from_clusters(group_catalog, max_members=50, max_vdisp=600):
    """
    Separate galaxy groups from clusters based on physical criteria.
    
    Parameters:
    group_catalog: DataFrame with group properties
    max_members: Maximum number of members for galaxy groups
    max_vdisp: Maximum velocity dispersion for galaxy groups (km/s)
    
    Returns:
    groups_only: DataFrame with galaxy groups only
    clusters_only: DataFrame with galaxy clusters only
    """
    print(f"\nSeparating groups from clusters:")
    print(f"  Criteria: ≤{max_members} members AND ≤{max_vdisp} km/s velocity dispersion")
    
    original_count = len(group_catalog)
    
    # Apply group vs cluster cuts
    # Groups: small number of members AND low velocity dispersion
    # Clusters: large number of members OR high velocity dispersion
    group_mask = (
        (group_catalog['Nm'] <= max_members) & 
        (group_catalog['vdisp_gap'] <= max_vdisp) &
        (group_catalog['vdisp_gap'].notna())  # Ensure we have valid velocity dispersion
    )
    
    groups_only = group_catalog[group_mask].copy()
    clusters_only = group_catalog[~group_mask].copy()
    
    print(f"  Original systems: {original_count}")
    print(f"  Galaxy groups: {len(groups_only)} (≤{max_members} members AND ≤{max_vdisp} km/s)")
    print(f"  Galaxy clusters: {len(clusters_only)} (>{max_members} members OR >{max_vdisp} km/s)")
    
    if len(clusters_only) > 0:
        print(f"  Cluster sizes: {clusters_only['Nm'].min()}-{clusters_only['Nm'].max()} members")
        if 'vdisp_gap' in clusters_only.columns:
            cluster_vdisp = clusters_only['vdisp_gap'].dropna()
            if len(cluster_vdisp) > 0:
                print(f"  Cluster velocity dispersions: {cluster_vdisp.min():.0f}-{cluster_vdisp.max():.0f} km/s")
    
    return groups_only, clusters_only

# ============================================================================
# NEW: GROUP EVOLUTION TRACKING FUNCTIONS
# ============================================================================

def track_groups_across_snapshots(snapshot_data, snapshots, track_overlap_fraction=0.3, 
                                 track_max_distance=2.0, show_progress=True):
    """
    Track galaxy groups across multiple snapshots.
    
    Parameters:
    snapshot_data: dict of {snapshot: (data_merged, group_catalog)} 
    snapshots: list of snapshot numbers (should be in descending order - high to low redshift)
    track_overlap_fraction: minimum fraction of galaxies that must overlap to consider groups related
    track_max_distance: maximum distance (Mpc) between group centers to consider them related
    show_progress: show progress during tracking
    
    Returns:
    group_evolution: dict with group evolution data
    evolution_summary: DataFrame with evolution statistics
    """
    
    print(f"\n" + "="*60)
    print("TRACKING GROUP EVOLUTION ACROSS SNAPSHOTS")
    print("="*60)
    
    # Sort snapshots in descending order (highest first = lowest redshift = most recent)
    snapshots_sorted = sorted(snapshots, reverse=True)
    print(f"Tracking {len(snapshots_sorted)} snapshots: {snapshots_sorted}")
    print(f"Direction: z=low (snap {snapshots_sorted[0]}) → z=high (snap {snapshots_sorted[-1]})")
    
    # Initialize tracking data structures
    group_evolution = {}
    next_tracking_id = 0
    
    # Start with groups from the highest snapshot (lowest redshift)
    start_snapshot = snapshots_sorted[0]
    start_data, start_groups = snapshot_data[start_snapshot]
    
    # Filter groups by minimum size for tracking
    trackable_groups = start_groups[start_groups['Nm'] >= 3].copy()
    print(f"\nStarting with {len(trackable_groups)} trackable groups from snapshot {start_snapshot}")
    
    # Initialize tracking IDs for starting groups
    for _, group in trackable_groups.iterrows():
        group_id = group['group_id']
        tracking_id = f"track_{next_tracking_id:04d}"
        next_tracking_id += 1
        
        group_evolution[tracking_id] = {
            'tracking_id': tracking_id,
            'start_snapshot': start_snapshot,
            'snapshots': [start_snapshot],
            'group_ids': [group_id],
            'group_data': [group.to_dict()],
            'galaxy_lists': [get_galaxy_list_for_group(start_data, group_id)],
            'status': 'active'
        }
    
    print(f"Initialized {len(group_evolution)} tracking chains")
    
    # Track through subsequent snapshots
    for i in range(1, len(snapshots_sorted)):
        current_snapshot = snapshots_sorted[i]
        prev_snapshot = snapshots_sorted[i-1]
        
        print(f"\nTracking snapshot {prev_snapshot} → {current_snapshot}")
        
        if current_snapshot not in snapshot_data:
            print(f"Warning: No data for snapshot {current_snapshot}, skipping")
            continue
            
        current_data, current_groups = snapshot_data[current_snapshot]
        current_trackable = current_groups[current_groups['Nm'] >= 3].copy()
        
        # Track each active group from previous snapshot
        active_tracks = {tid: track for tid, track in group_evolution.items() 
                        if track['status'] == 'active' and track['snapshots'][-1] == prev_snapshot}
        
        matched_current_groups = set()
        
        if show_progress and TQDM_AVAILABLE:
            track_iterator = tqdm(active_tracks.items(), 
                                desc=f"Tracking to snap {current_snapshot}",
                                unit="groups")
        else:
            track_iterator = active_tracks.items()
        
        for tracking_id, track in track_iterator:
            # Get the last known state of this group
            last_group_data = track['group_data'][-1]
            last_galaxy_list = track['galaxy_lists'][-1]
            
            # Find best match in current snapshot
            best_match = find_best_group_match(
                last_group_data, last_galaxy_list, 
                current_trackable, current_data,
                track_overlap_fraction, track_max_distance
            )
            
            if best_match is not None:
                match_group_id, match_score, match_type = best_match
                
                # Mark this current group as matched
                matched_current_groups.add(match_group_id)
                
                # Update tracking chain
                matched_group_data = current_trackable[current_trackable['group_id'] == match_group_id].iloc[0]
                matched_galaxy_list = get_galaxy_list_for_group(current_data, match_group_id)
                
                track['snapshots'].append(current_snapshot)
                track['group_ids'].append(match_group_id)
                track['group_data'].append(matched_group_data.to_dict())
                track['galaxy_lists'].append(matched_galaxy_list)
                track.setdefault('match_scores', []).append(match_score)
                track.setdefault('match_types', []).append(match_type)
            else:
                # No match found - group disappeared
                track['status'] = 'disappeared'
                track['end_snapshot'] = prev_snapshot
        
        # Handle unmatched groups in current snapshot (new groups or mergers)
        unmatched_groups = current_trackable[~current_trackable['group_id'].isin(matched_current_groups)]
        if len(unmatched_groups) > 0:
            print(f"  Found {len(unmatched_groups)} new/unmatched groups in snapshot {current_snapshot}")
            
            # These could be newly formed groups or results of complex mergers
            for _, group in unmatched_groups.iterrows():
                group_id = group['group_id']
                tracking_id = f"track_{next_tracking_id:04d}"
                next_tracking_id += 1
                
                group_evolution[tracking_id] = {
                    'tracking_id': tracking_id,
                    'start_snapshot': current_snapshot,
                    'snapshots': [current_snapshot],
                    'group_ids': [group_id],
                    'group_data': [group.to_dict()],
                    'galaxy_lists': [get_galaxy_list_for_group(current_data, group_id)],
                    'status': 'active',
                    'formation_type': 'appeared'
                }
        
        # Update status summary
        active_count = sum(1 for track in group_evolution.values() if track['status'] == 'active')
        disappeared_count = sum(1 for track in group_evolution.values() if track['status'] == 'disappeared')
        print(f"  Active tracks: {active_count}, Disappeared: {disappeared_count}")
    
    # Finalize tracking
    for track in group_evolution.values():
        if track['status'] == 'active':
            track['end_snapshot'] = track['snapshots'][-1]
    
    # Create evolution summary
    evolution_summary = create_evolution_summary(group_evolution, snapshots_sorted)
    
    print(f"\nEvolution tracking complete!")
    print(f"Total tracking chains: {len(group_evolution)}")
    print(f"Complete chains (all snapshots): {len([t for t in group_evolution.values() if len(t['snapshots']) == len(snapshots_sorted)])}")
    
    return group_evolution, evolution_summary

def get_galaxy_list_for_group(data, group_id, group_id_col='group_id'):
    """
    Get list of galaxy indices for a given group.
    """
    if group_id_col in data.columns:
        group_galaxies = data[data[group_id_col] == group_id]
        if 'GalaxyIndex' in group_galaxies.columns:
            return group_galaxies['GalaxyIndex'].tolist()
        else:
            return group_galaxies.index.tolist()
    return []

def find_best_group_match(prev_group_data, prev_galaxy_list, current_groups, current_data,
                         overlap_fraction=0.3, max_distance=2.0):
    """
    Find the best matching group in the current snapshot for a group from the previous snapshot.
    
    Returns:
    tuple: (group_id, match_score, match_type) or None if no good match
    """
    
    if len(current_groups) == 0:
        return None
    
    best_match = None
    best_score = 0
    best_type = None
    
    prev_pos = np.array([prev_group_data['x_center'], prev_group_data['y_center'], prev_group_data['z_center']])
    
    for _, current_group in current_groups.iterrows():
        current_group_id = current_group['group_id']
        current_galaxy_list = get_galaxy_list_for_group(current_data, current_group_id)
        current_pos = np.array([current_group['x_center'], current_group['y_center'], current_group['z_center']])
        
        # Distance check
        distance = np.linalg.norm(current_pos - prev_pos)
        if distance > max_distance:
            continue
        
        # Galaxy overlap check
        if len(prev_galaxy_list) == 0 or len(current_galaxy_list) == 0:
            continue
            
        overlap = len(set(prev_galaxy_list) & set(current_galaxy_list))
        overlap_frac = overlap / min(len(prev_galaxy_list), len(current_galaxy_list))
        
        if overlap_frac < overlap_fraction:
            continue
        
        # Calculate combined score (overlap + inverse distance)
        distance_score = max(0, 1 - distance / max_distance)
        combined_score = 0.7 * overlap_frac + 0.3 * distance_score
        
        if combined_score > best_score:
            best_score = combined_score
            best_match = current_group_id
            
            # Determine match type
            if overlap_frac > 0.7:
                best_type = 'strong_overlap'
            elif overlap_frac > 0.4:
                best_type = 'medium_overlap'
            else:
                best_type = 'weak_overlap'
    
    if best_match is not None:
        return (best_match, best_score, best_type)
    return None

def create_evolution_summary(group_evolution, snapshots):
    """
    Create a summary DataFrame of group evolution statistics.
    """
    summary_data = []
    
    for tracking_id, track in group_evolution.items():
        # Basic info
        row = {
            'tracking_id': tracking_id,
            'start_snapshot': track['start_snapshot'],
            'end_snapshot': track.get('end_snapshot', track['snapshots'][-1]),
            'n_snapshots_tracked': len(track['snapshots']),
            'status': track['status'],
            'formation_type': track.get('formation_type', 'initial')
        }
        
        # Evolution metrics
        if len(track['group_data']) > 1:
            start_data = track['group_data'][0]
            end_data = track['group_data'][-1]
            
            # Mass evolution
            start_mass = start_data.get('Sig_M', np.nan)
            end_mass = end_data.get('Sig_M', np.nan)
            if not np.isnan(start_mass) and not np.isnan(end_mass):
                row['mass_evolution'] = end_mass - start_mass
                row['mass_growth_rate'] = (end_mass - start_mass) / len(track['snapshots'])
            
            # Size evolution
            start_members = start_data.get('Nm', 0)
            end_members = end_data.get('Nm', 0)
            row['member_evolution'] = end_members - start_members
            row['member_growth_rate'] = (end_members - start_members) / len(track['snapshots'])
            
            # Velocity dispersion evolution
            start_vdisp = start_data.get('vdisp_gap', np.nan)
            end_vdisp = end_data.get('vdisp_gap', np.nan)
            if not np.isnan(start_vdisp) and not np.isnan(end_vdisp):
                row['vdisp_evolution'] = end_vdisp - start_vdisp
        
        # Match quality (if available)
        if 'match_scores' in track:
            row['avg_match_score'] = np.mean(track['match_scores'])
            row['min_match_score'] = np.min(track['match_scores'])
        
        summary_data.append(row)
    
    return pd.DataFrame(summary_data)

# ============================================================================
# MULTI-SNAPSHOT DATA LOADING
# ============================================================================

def load_multiple_snapshots(filename, snapshots, **kwargs):
    """
    Load data from multiple snapshots and run group finding on each.
    
    Parameters:
    filename: path to SAGE HDF5 file
    snapshots: list of snapshot numbers
    **kwargs: additional arguments passed to SAGE_Group_Catalog
    
    Returns:
    snapshot_data: dict of {snapshot: (data_merged, group_catalog)}
    """
    snapshot_data = {}
    
    print(f"Loading {len(snapshots)} snapshots: {snapshots}")
    
    for snapshot in snapshots:
        print(f"\n" + "-"*40)
        print(f"Loading snapshot {snapshot}")
        print("-"*40)
        
        # Load data for this snapshot
        sage_data = load_sage_hdf5(filename, snapshot)
        
        if len(sage_data) == 0:
            print(f"Warning: No data loaded for snapshot {snapshot}")
            continue
        
        # Run group finding
        data_merged, group_catalog = SAGE_Group_Catalog(sage_data, **kwargs)
        
        # Store results
        snapshot_data[snapshot] = (data_merged, group_catalog)
        
        print(f"Snapshot {snapshot}: {len(sage_data)} galaxies, {len(group_catalog)} groups")
    
    return snapshot_data

# ============================================================================
# DATA LOADING FUNCTIONS - FIXED VERSION (ORIGINAL)
# ============================================================================

def explore_hdf5_structure(filename, snapshot=63):
    """
    Explore the structure of SAGE HDF5 file to understand the data layout.
    """
    print(f"Exploring structure of {filename}")
    
    with h5py.File(filename, 'r') as f:
        print("Root level datasets:")
        for key in f.keys():
            print(f"  {key}")
        
        # Check a specific snapshot
        snap_key = f'Snap_{snapshot}'
        if snap_key in f:
            print(f"\nFields in {snap_key}:")
            snap_data = f[snap_key]
            for field in snap_data.keys():
                shape = snap_data[field].shape
                dtype = snap_data[field].dtype
                print(f"  {field}: shape={shape}, dtype={dtype}")
                
                # Show some sample values for key fields
                if field in ['Posx', 'StellarMass', 'Type', 'SAGEHaloIndex']:
                    sample_data = snap_data[field][:5]
                    print(f"    Sample values: {sample_data}")
        else:
            print(f"\nSnapshot {snapshot} not found")

def analyze_group_id_fields(data):
    """
    Analyze potential group ID fields to help choose the best one.
    """
    print("\nAnalyzing potential group ID fields:")
    print("="*50)
    
    potential_fields = [col for col in data.columns 
                       if 'halo' in col.lower() or 'index' in col.lower() 
                       or 'central' in col.lower() or 'id' in col.lower()]
    
    for field in potential_fields:
        unique_vals = data[field].nunique()
        group_sizes = data.groupby(field).size()
        
        print(f"\n{field}:")
        print(f"  Unique values: {unique_vals}")
        print(f"  Group sizes - Mean: {group_sizes.mean():.1f}, Median: {group_sizes.median():.1f}")
        print(f"  Group sizes - Min: {group_sizes.min()}, Max: {group_sizes.max()}")
        print(f"  Groups with >100 members: {(group_sizes > 100).sum()}")
        print(f"  Groups with >1000 members: {(group_sizes > 1000).sum()}")
        
        # Show a few sample group sizes
        print(f"  Sample group sizes: {sorted(group_sizes.values)[:10]}...")
    
    return potential_fields

def load_sage_hdf5(filename, snapshot=None):
    """
    Load SAGE HDF5 output data - FIXED VERSION.
    
    This version properly loads individual position/velocity fields and
    maps the correct group ID fields, plus all SAGE galactic properties.
    """
    print(f"Loading SAGE data from {filename}")
    
    try:
        with h5py.File(filename, 'r') as f:
            # If snapshot is specified, load from that snapshot
            if snapshot is not None:
                snap_key = f'Snap_{snapshot}'
                if snap_key not in f:
                    print(f"Snapshot {snapshot} not found. Available snapshots:")
                    snap_keys = [k for k in f.keys() if k.startswith('Snap_')]
                    for sk in sorted(snap_keys):
                        print(f"  {sk}")
                    return create_sample_data()
                
                print(f"\nLoading data from {snap_key}")
                snap_data = f[snap_key]
                
                # Get cosmological parameters for unit conversion
                # Default values - these should ideally come from the simulation
                Hubble_h = 0.7  # Default value, should be read from file if available
                
                # Load galaxy data from snapshot
                galaxy_data = {}
                
                # Complete field mapping - load all SAGE fields with proper names
                expected_fields = {
                    # Position and velocity
                    'Posx': 'Posx',
                    'Posy': 'Posy', 
                    'Posz': 'Posz',
                    'Velx': 'Velx',
                    'Vely': 'Vely',
                    'Velz': 'Velz',
                    
                    # Mass properties (will be converted to M_sun units)
                    'StellarMass': 'StellarMass',
                    'Mvir': 'Mvir',
                    'CentralMvir': 'CentralMvir',
                    'BulgeMass': 'BulgeMass',
                    'BlackHoleMass': 'BlackHoleMass',
                    'ColdGas': 'ColdGas',
                    'HI_gas': 'HI_gas',  # H1Gas
                    'H2_gas': 'H2_gas',  # H2Gas
                    'MetalsColdGas': 'MetalsColdGas',
                    'HotGas': 'HotGas',
                    'IntraClusterStars': 'IntraClusterStars',
                    'infallMvir': 'infallMvir',  # InfallMvir
                    'OutflowRate': 'OutflowRate',
                    'CGMgas': 'CGMgas',
                    
                    # Distance properties (will be converted to proper units)
                    'Rvir': 'Rvir',
                    'DiskRadius': 'DiskRadius',
                    
                    # Dimensionless or other units
                    'Type': 'Type',
                    'SnapNum': 'SnapNum',
                    'VelDisp': 'VelDisp',
                    'MassLoading': 'MassLoading',  # Mass loading factor
                    'Cooling': 'Cooling',
                    'Vvir': 'Vvir',
                    'Vmax': 'Vmax',
                    'SfrDisk': 'SfrDisk',
                    'SfrBulge': 'SfrBulge',
                    
                    # Index fields
                    'SAGEHaloIndex': 'SAGEHaloIndex',  # Key group ID field
                    'GalaxyIndex': 'GalaxyIndex',
                    'CentralGalaxyIndex': 'CentralGalaxyIndex'
                }
                
                # Define which fields need unit conversion
                mass_fields = {
                    'StellarMass', 'Mvir', 'CentralMvir', 'BulgeMass', 'BlackHoleMass',
                    'ColdGas', 'HI_gas', 'H2_gas', 'MetalsColdGas', 'HotGas',
                    'IntraClusterStars', 'infallMvir', 'OutflowRate', 'CGMgas'
                }  # These get * 1.0e10 / Hubble_h
                
                distance_fields = {
                    'DiskRadius', 'Rvir'
                }  # These get / Hubble_h
                
                n_galaxies = None
                
                # Load fields directly with unit conversion
                for our_name, sage_name in expected_fields.items():
                    if sage_name in snap_data:
                        raw_data = snap_data[sage_name][:]
                        
                        # Apply unit conversions
                        if our_name in mass_fields:
                            # Convert from SAGE internal units to M_sun
                            galaxy_data[our_name] = raw_data * 1.0e10 / Hubble_h
                            unit_str = "M_sun"
                        elif our_name in distance_fields:
                            # Convert distance units
                            galaxy_data[our_name] = raw_data / Hubble_h
                            unit_str = "Mpc" if our_name == 'Rvir' else "kpc"
                        else:
                            # No unit conversion needed
                            galaxy_data[our_name] = raw_data
                            unit_str = "native"
                        
                        if n_galaxies is None:
                            n_galaxies = len(galaxy_data[our_name])
                        
                        print(f"  Loaded {our_name} from {sage_name}: {galaxy_data[our_name].shape} [{unit_str}]")
                        
                        # Show some stats for key fields
                        if our_name in ['Posx', 'StellarMass', 'SAGEHaloIndex', 'Mvir']:
                            data_vals = galaxy_data[our_name]
                            if our_name == 'SAGEHaloIndex':
                                valid_vals = data_vals
                            else:
                                valid_vals = data_vals[data_vals != 0]
                            
                            if len(valid_vals) > 0:
                                print(f"    Range: [{np.min(valid_vals):.3e}, {np.max(valid_vals):.3e}]")
                    else:
                        print(f"  Warning: {sage_name} not found in snapshot data")
                        if n_galaxies is not None:
                            # Create dummy data for missing fields with appropriate defaults
                            if our_name == 'VelDisp':
                                galaxy_data[our_name] = np.zeros(n_galaxies)
                            elif our_name in ['GalaxyIndex', 'CentralGalaxyIndex']:
                                galaxy_data[our_name] = np.arange(n_galaxies)
                            elif our_name in mass_fields:
                                galaxy_data[our_name] = np.zeros(n_galaxies)  # Zero mass for missing fields
                            elif our_name in distance_fields:
                                galaxy_data[our_name] = np.zeros(n_galaxies)  # Zero distance for missing fields
                            else:
                                galaxy_data[our_name] = np.zeros(n_galaxies)  # Default zeros
                
                # Handle missing group ID - this is critical
                if 'SAGEHaloIndex' not in galaxy_data:
                    print("  ERROR: SAGEHaloIndex not found - this is the key group identifier!")
                    print("  Available fields that might work as group ID:")
                    for field in snap_data.keys():
                        if 'halo' in field.lower() or 'index' in field.lower() or 'central' in field.lower():
                            print(f"    {field}: shape={snap_data[field].shape}")
                    
                    # Try alternative fields
                    if 'CentralGalaxyIndex' in snap_data:
                        print("  Using CentralGalaxyIndex as group identifier")
                        galaxy_data['SAGEHaloIndex'] = snap_data['CentralGalaxyIndex'][:]
                    else:
                        print("  Creating dummy group data - results will not be meaningful!")
                        galaxy_data['SAGEHaloIndex'] = np.arange(n_galaxies) // 50  # ~50 gals per group
                
            else:
                print("No specific snapshot requested. Please specify a snapshot number.")
                return create_sample_data()
            
    except Exception as e:
        print(f"Error loading HDF5 file: {e}")
        print("Creating sample data for demonstration...")
        return create_sample_data()
    
    df = pd.DataFrame(galaxy_data)
    print(f"\nLoaded {len(df)} galaxies from snapshot {snapshot}")
    
    # Print data summary
    if len(df) > 0:
        print(f"Data summary:")
        if 'Posx' in df.columns:
            valid_pos = df['Posx'][df['Posx'] != 0]
            if len(valid_pos) > 0:
                print(f"  Position range: x=[{valid_pos.min():.1f}, {valid_pos.max():.1f}] Mpc/h")
            else:
                print(f"  Warning: All position values are zero!")
                
        if 'StellarMass' in df.columns:
            valid_mass = df['StellarMass'][(df['StellarMass'] > 0) & (df['StellarMass'] < 1e15)]
            if len(valid_mass) > 0:
                print(f"  Stellar mass range: [{valid_mass.min():.1e}, {valid_mass.max():.1e}] M_sun")
                
        if 'Type' in df.columns:
            type_counts = df['Type'].value_counts()
            print(f"  Galaxy types: {dict(type_counts)}")
            
        if 'SAGEHaloIndex' in df.columns:
            n_groups = df['SAGEHaloIndex'].nunique()
            print(f"  Number of unique groups: {n_groups}")
            
        # Show summary of other key SAGE properties
        mass_props = ['Mvir', 'ColdGas', 'HotGas', 'BlackHoleMass']
        for prop in mass_props:
            if prop in df.columns:
                valid_vals = df[prop][(df[prop] > 0) & (df[prop] < 1e16)]
                if len(valid_vals) > 0:
                    print(f"  {prop} range: [{valid_vals.min():.1e}, {valid_vals.max():.1e}] M_sun")
    
    return df

def create_sample_data(n_galaxies=10000, n_groups=1000):
    """
    Create sample SAGE-like data for demonstration purposes.
    Includes all SAGE galactic properties with realistic values.
    """
    print(f"Creating sample data with {n_galaxies} galaxies in {n_groups} groups")
    
    np.random.seed(42)  # For reproducibility
    
    # Generate group IDs
    group_ids = np.random.randint(0, n_groups, n_galaxies)
    
    # Generate positions (comoving coordinates in Mpc/h) 
    positions = np.random.uniform(-100, 100, (n_galaxies, 3))
    
    # Generate velocities (km/s)
    velocities = np.random.normal(0, 200, (n_galaxies, 3))
    
    # Generate stellar masses (M_sun) - already in physical units
    stellar_masses = 10**(np.random.normal(10, 1.5, n_galaxies))  # M_sun
    
    # Generate halo masses (M_sun) - correlated with stellar mass
    log_stellar = np.log10(stellar_masses)
    log_halo = log_stellar + np.random.normal(2, 0.3, n_galaxies)  # Stellar-halo mass relation
    halo_masses = 10**log_halo
    
    # Generate other mass components as fractions of stellar mass
    bulge_masses = stellar_masses * np.random.uniform(0.1, 0.5, n_galaxies)
    black_hole_masses = stellar_masses * np.random.lognormal(-6, 1, n_galaxies)  # ~10^-6 M_BH/M_*
    cold_gas = stellar_masses * np.random.lognormal(-0.3, 0.8, n_galaxies)  # Cold gas
    hi_gas = cold_gas * np.random.uniform(0.3, 0.8, n_galaxies)  # HI fraction of cold gas
    h2_gas = cold_gas - hi_gas  # H2 is remainder
    metals_cold_gas = cold_gas * np.random.uniform(0.01, 0.03, n_galaxies)  # ~2% metals
    hot_gas = halo_masses * np.random.uniform(0.05, 0.15, n_galaxies)  # Hot gas fraction
    intracluster_stars = stellar_masses * np.random.uniform(0, 0.1, n_galaxies)
    cgm_gas = hot_gas * np.random.uniform(0.1, 0.5, n_galaxies)
    
    # Generate distances (physical units)
    rvir = (3 * halo_masses / (4 * np.pi * 200 * 2.78e11))**(1/3)  # Virial radius in Mpc
    disk_radius = rvir * np.random.uniform(0.01, 0.05, n_galaxies) * 1000  # Disk radius in kpc
    
    # Generate other properties
    types = np.random.choice([0, 1], n_galaxies, p=[0.3, 0.7])  # 0=central, 1=satellite
    snapnums = np.full(n_galaxies, 63)  # z=0 snapshot
    vvir = np.sqrt(6.67e-11 * halo_masses * 1.989e30 / (rvir * 3.086e22)) / 1000  # km/s
    vmax = vvir * np.random.uniform(0.8, 1.2, n_galaxies)
    
    # Star formation rates (M_sun/yr)
    sfr_disk = stellar_masses * np.random.lognormal(-10, 1, n_galaxies)  # SFR
    sfr_bulge = sfr_disk * np.random.uniform(0, 0.3, n_galaxies)  # Bulge SFR fraction
    
    # Outflow and cooling properties
    outflow_rate = sfr_disk * np.random.uniform(0.1, 5, n_galaxies)  # Outflow rate
    mass_loading = outflow_rate / np.maximum(sfr_disk, 1e-10)  # Mass loading factor
    cooling = hot_gas * np.random.uniform(0.01, 0.1, n_galaxies)  # Cooling rate
    
    data = pd.DataFrame({
        # Index fields
        'SAGEHaloIndex': group_ids,
        'GalaxyIndex': np.arange(n_galaxies),
        'CentralGalaxyIndex': group_ids,
        'Type': types,
        'SnapNum': snapnums,
        
        # Position and velocity
        'Posx': positions[:, 0],
        'Posy': positions[:, 1],
        'Posz': positions[:, 2],
        'Velx': velocities[:, 0],
        'Vely': velocities[:, 1],
        'Velz': velocities[:, 2],
        'VelDisp': np.random.lognormal(5, 0.5, n_galaxies),  # km/s
        
        # Mass properties (all in M_sun)
        'StellarMass': stellar_masses,
        'Mvir': halo_masses,
        'CentralMvir': halo_masses,  # Same as Mvir for centrals
        'BulgeMass': bulge_masses,
        'BlackHoleMass': black_hole_masses,
        'ColdGas': cold_gas,
        'HI_gas': hi_gas,
        'H2_gas': h2_gas,
        'MetalsColdGas': metals_cold_gas,
        'HotGas': hot_gas,
        'IntraClusterStars': intracluster_stars,
        'infallMvir': halo_masses,  # Infall mass same as current for centrals
        'OutflowRate': outflow_rate,
        'CGMgas': cgm_gas,
        
        # Distance properties (physical units)
        'Rvir': rvir,  # Mpc
        'DiskRadius': disk_radius,  # kpc
        
        # Other properties
        'Vvir': vvir,  # km/s
        'Vmax': vmax,  # km/s
        'SfrDisk': sfr_disk,  # M_sun/yr
        'SfrBulge': sfr_bulge,  # M_sun/yr
        'MassLoading': mass_loading,  # dimensionless
        'Cooling': cooling  # M_sun/yr
    })
    
    return data

# ============================================================================
# EVOLUTION VISUALIZATION FUNCTIONS - FIXED VERSION
# ============================================================================

def plot_group_evolution(group_evolution, evolution_summary, output_dir='./'):
    """
    Create plots showing group evolution across snapshots.
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    print("Creating group evolution plots...")
    
    # Filter for groups that were tracked across multiple snapshots
    multi_snapshot_tracks = {tid: track for tid, track in group_evolution.items() 
                           if len(track['snapshots']) > 1}
    
    if len(multi_snapshot_tracks) == 0:
        print("No groups tracked across multiple snapshots")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    
    # Plot 1: Evolution duration distribution
    axes[0, 0].hist(evolution_summary['n_snapshots_tracked'], bins=20, alpha=0.7, 
                   color='#440154', edgecolor='black')
    axes[0, 0].set_xlabel('Number of Snapshots Tracked')
    axes[0, 0].set_ylabel('Number of Groups')
    axes[0, 0].set_title('Group Evolution Duration')
    
    # Plot 2: Mass evolution
    if 'mass_evolution' in evolution_summary.columns:
        valid_mass = evolution_summary['mass_evolution'].dropna()
        if len(valid_mass) > 0:
            axes[0, 1].hist(valid_mass, bins=20, alpha=0.7, 
                           color='#31688e', edgecolor='black')
            axes[0, 1].set_xlabel('Mass Evolution [log10(M_sun)]')
            axes[0, 1].set_ylabel('Number of Groups')
            axes[0, 1].set_title('Stellar Mass Evolution')
            axes[0, 1].axvline(0, color='red', linestyle='--', alpha=0.7)
        else:
            axes[0, 1].text(0.5, 0.5, 'No mass evolution data', 
                          transform=axes[0, 1].transAxes, ha='center', va='center')
    
    # Plot 3: Member evolution
    if 'member_evolution' in evolution_summary.columns:
        valid_members = evolution_summary['member_evolution'].dropna()
        if len(valid_members) > 0:
            axes[1, 0].hist(valid_members, bins=20, alpha=0.7, 
                           color='#fde725', edgecolor='black')
            axes[1, 0].set_xlabel('Member Evolution (ΔN)')
            axes[1, 0].set_ylabel('Number of Groups')
            axes[1, 0].set_title('Group Size Evolution')
            axes[1, 0].axvline(0, color='red', linestyle='--', alpha=0.7)
        else:
            axes[1, 0].text(0.5, 0.5, 'No member evolution data', 
                          transform=axes[1, 0].transAxes, ha='center', va='center')
    
    # Plot 4: Example evolution tracks
    axes[1, 1].set_title('Example Evolution Tracks (Group Size)')
    
    # Show a few example tracks
    example_tracks = list(multi_snapshot_tracks.items())[:10]  # Show up to 10 tracks
    
    colors = plt.cm.Set3(np.linspace(0, 1, len(example_tracks)))
    
    for i, (tracking_id, track) in enumerate(example_tracks):
        snapshots = track['snapshots']
        group_sizes = [data['Nm'] for data in track['group_data']]
        
        axes[1, 1].plot(snapshots, group_sizes, 'o-', color=colors[i], 
                       alpha=0.7, linewidth=2, markersize=4, 
                       label=tracking_id if len(example_tracks) <= 5 else None)
    
    axes[1, 1].set_xlabel('Snapshot Number')
    axes[1, 1].set_ylabel('Number of Members')
    axes[1, 1].grid(True, alpha=0.3)
    
    if len(example_tracks) <= 5:
        axes[1, 1].legend(fontsize=8)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'group_evolution_analysis.png', dpi=150, bbox_inches='tight')
    print(f"Saved evolution analysis plot to {output_dir / 'group_evolution_analysis.png'}")
    plt.close()
    
    # Create individual evolution tracks plot for most interesting groups
    plot_individual_evolution_tracks(multi_snapshot_tracks, output_dir)

def plot_individual_evolution_tracks(multi_snapshot_tracks, output_dir):
    """
    Plot detailed evolution tracks for the most interesting groups.
    """
    
    # Select most interesting tracks (longest duration, significant evolution)
    interesting_tracks = []
    
    for tracking_id, track in multi_snapshot_tracks.items():
        if len(track['snapshots']) >= 3:  # At least 3 snapshots
            # Calculate some evolution metrics
            start_data = track['group_data'][0]
            end_data = track['group_data'][-1]
            
            member_change = abs(end_data['Nm'] - start_data['Nm'])
            
            # Score based on duration and change
            score = len(track['snapshots']) * (1 + member_change / 10)
            interesting_tracks.append((tracking_id, track, score))
    
    # Sort by score and take top 6
    interesting_tracks.sort(key=lambda x: x[2], reverse=True)
    top_tracks = interesting_tracks[:6]
    
    if len(top_tracks) == 0:
        print("No interesting evolution tracks to plot")
        return
    
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    for i, (tracking_id, track, score) in enumerate(top_tracks):
        ax = axes[i]
        
        snapshots = track['snapshots']
        
        # Extract evolution data
        group_sizes = [data['Nm'] for data in track['group_data']]
        masses = [data.get('Sig_M', np.nan) for data in track['group_data']]
        vdisps = [data.get('vdisp_gap', np.nan) for data in track['group_data']]
        
        # Create subplot with dual y-axis
        ax2 = ax.twinx()
        
        # Plot group size
        line1 = ax.plot(snapshots, group_sizes, 'o-', color='blue', linewidth=2, 
                       markersize=6, label='Members')
        ax.set_ylabel('Number of Members', color='blue')
        ax.tick_params(axis='y', labelcolor='blue')
        
        # Plot mass if available
        if not all(np.isnan(masses)):
            line2 = ax2.plot(snapshots, masses, 's-', color='red', linewidth=2, 
                            markersize=6, label='Stellar Mass')
            ax2.set_ylabel('Log10(Stellar Mass [M_sun])', color='red')
            ax2.tick_params(axis='y', labelcolor='red')
        
        ax.set_xlabel('Snapshot Number')
        ax.set_title(f'{tracking_id}\n{len(snapshots)} snapshots, Score: {score:.1f}')
        ax.grid(True, alpha=0.3)
        
        # Add text with evolution summary
        start_size = group_sizes[0]
        end_size = group_sizes[-1]
        size_change = end_size - start_size
        
        summary_text = f"Δ Members: {int(size_change):+d}"
        if not all(np.isnan(masses)):
            mass_change = masses[-1] - masses[0]
            summary_text += f"\nΔ Mass: {mass_change:+.2f}"
        
        ax.text(0.05, 0.95, summary_text, transform=ax.transAxes, 
               fontsize=8, verticalalignment='top',
               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
    
    # Hide unused subplots
    for i in range(len(top_tracks), len(axes)):
        axes[i].set_visible(False)
    
    plt.tight_layout()
    plt.savefig(output_dir / 'individual_evolution_tracks.png', dpi=150, bbox_inches='tight')
    print(f"Saved individual evolution tracks to {output_dir / 'individual_evolution_tracks.png'}")
    plt.close()

def plot_group_property_evolution(group_evolution, snapshots, output_dir='./', snapshot_data=None):
    """
    Create a grid plot showing galactic property evolution for fully tracked groups.
    FIXED VERSION: Uses proper cosmological time calculation.
    
    Parameters:
    group_evolution: dict with group evolution data
    snapshots: list of snapshot numbers
    output_dir: output directory for plots
    snapshot_data: dict of {snapshot: (data_merged, group_catalog)} - optional for enhanced galaxy property calculation
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    print("Creating galactic property evolution plots...")
    
    # Filter for complete chains (tracked through all snapshots)
    complete_tracks = {tid: track for tid, track in group_evolution.items() 
                      if len(track['snapshots']) == len(snapshots)}
    
    if len(complete_tracks) == 0:
        print("No complete evolution tracks found - cannot create property evolution plot")
        return
    
    print(f"Plotting evolution for {len(complete_tracks)} complete tracks")
    
    # FIXED: Use proper cosmological time calculation
    cosmo_params = {'H0': 70.0, 'Omega_m': 0.27, 'Omega_Lambda': 0.73}
    lookback_times, redshifts = get_time_axis_for_snapshots(snapshots, cosmo_params)
    
    print(f"Snapshot to time mapping:")
    for snap, t_lookback, z in zip(snapshots, lookback_times, redshifts):
        print(f"  Snapshot {snap}: z={z:.2f}, lookback time={t_lookback:.2f} Gyr")
    
    # Set up the subplot grid (2x3)
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    # Properties to plot - using group-level properties where available, 
    # and we'll calculate galaxy-summed properties separately
    properties = [
        {'name': 'Halo Mass', 'field': 'M_200', 'unit': 'M_sun', 'log': True, 'source': 'group'},
        {'name': 'Total Stellar Mass', 'field': 'Sig_M', 'unit': 'M_sun', 'log': True, 'is_log': True, 'source': 'group'},
        {'name': 'Hot Gas Mass', 'field': 'HotGas', 'unit': 'M_sun', 'log': True, 'source': 'galaxy_sum'},
        {'name': 'Cold Gas Mass', 'field': 'ColdGas', 'unit': 'M_sun', 'log': True, 'source': 'galaxy_sum'},
        {'name': 'H2 Gas Mass', 'field': 'H2_gas', 'unit': 'M_sun', 'log': True, 'source': 'galaxy_sum'},
        {'name': 'sSFR', 'field': 'sSFR', 'unit': 'yr^-1', 'log': True, 'source': 'galaxy_calc'}
    ]
    
    # Colors for different groups
    colors = plt.cm.Set3(np.linspace(0, 1, min(len(complete_tracks), 20)))
    
    for prop_idx, prop in enumerate(properties):
        ax = axes[prop_idx]
        
        # Extract data for this property across all complete tracks
        all_track_data = []
        
        for track_idx, (tracking_id, track) in enumerate(complete_tracks.items()):
            # Get property values for each snapshot
            property_values = []
            track_lookback_times = []
            
            for i, snapshot in enumerate(track['snapshots']):
                group_data = track['group_data'][i]
                galaxy_list = track['galaxy_lists'][i]
                
                # Get the lookback time for this snapshot
                if snapshot in snapshots:
                    snap_index = snapshots.index(snapshot)
                    t_lookback = lookback_times[snap_index]
                else:
                    # Fallback calculation for snapshots not in main list
                    t_lookback = snapshot_to_lookback_time(snapshot, cosmo_params)
                
                # Calculate property value based on source
                if prop['source'] == 'group':
                    # Use group-level property
                    value = group_data.get(prop['field'], np.nan)
                    
                    if prop.get('is_log', False) and not np.isnan(value):
                        # Property is already in log10 units, convert to linear
                        if value > -10:  # Reasonable log value
                            property_values.append(10**value)
                        else:
                            property_values.append(np.nan)
                    elif not np.isnan(value) and (value > 0 or not prop['log']):
                        property_values.append(value)
                    else:
                        property_values.append(np.nan)
                        
                elif prop['source'] == 'galaxy_sum' and snapshot_data is not None:
                    # Sum property from all galaxies in the group
                    if snapshot in snapshot_data:
                        snap_data, _ = snapshot_data[snapshot]
                        group_id = track['group_ids'][i]
                        
                        # Get galaxies in this group
                        group_galaxies = snap_data[snap_data['group_id'] == group_id] if 'group_id' in snap_data.columns else pd.DataFrame()
                        
                        if len(group_galaxies) > 0 and prop['field'] in group_galaxies.columns:
                            # Sum the property across all galaxies in the group
                            galaxy_values = group_galaxies[prop['field']].values
                            # Filter out invalid values
                            valid_values = galaxy_values[(galaxy_values > 0) & np.isfinite(galaxy_values)]
                            if len(valid_values) > 0:
                                total_value = np.sum(valid_values)
                                property_values.append(total_value)
                            else:
                                property_values.append(np.nan)
                        else:
                            property_values.append(np.nan)
                    else:
                        property_values.append(np.nan)
                        
                elif prop['source'] == 'galaxy_calc' and snapshot_data is not None:
                    # Calculate sSFR from individual galaxies
                    if snapshot in snapshot_data:
                        snap_data, _ = snapshot_data[snapshot]
                        group_id = track['group_ids'][i]
                        
                        # Get galaxies in this group
                        group_galaxies = snap_data[snap_data['group_id'] == group_id] if 'group_id' in snap_data.columns else pd.DataFrame()
                        
                        if len(group_galaxies) > 0:
                            # Calculate total SFR and stellar mass
                            sfr_disk = group_galaxies.get('SfrDisk', pd.Series(dtype=float)).fillna(0)
                            sfr_bulge = group_galaxies.get('SfrBulge', pd.Series(dtype=float)).fillna(0)
                            stellar_mass = group_galaxies.get('StellarMass', pd.Series(dtype=float)).fillna(0)
                            
                            total_sfr = sfr_disk.sum() + sfr_bulge.sum()
                            total_stellar_mass = stellar_mass.sum()
                            
                            if total_stellar_mass > 0 and total_sfr > 0:
                                ssfr = total_sfr / total_stellar_mass  # yr^-1
                                property_values.append(ssfr)
                            else:
                                property_values.append(np.nan)
                        else:
                            property_values.append(np.nan)
                    else:
                        property_values.append(np.nan)
                else:
                    # Fallback for missing data or unsupported source
                    property_values.append(np.nan)
                
                track_lookback_times.append(t_lookback)
            
            # Only plot if we have valid data
            valid_data = [(t, v) for t, v in zip(track_lookback_times, property_values) 
                         if not np.isnan(v)]
            
            if prop['log']:
                # For log plots, also require positive values
                valid_data = [(t, v) for t, v in valid_data if v > 0]
            
            if len(valid_data) >= 2:  # Need at least 2 points
                times, values = zip(*valid_data)
                
                # Plot individual track (thin line)
                color = colors[track_idx % len(colors)]
                ax.plot(times, values, '-', color=color, alpha=0.6, linewidth=1.5,
                       label=tracking_id if len(complete_tracks) <= 5 else None)
                
                # Store for median calculation
                all_track_data.append((times, values))
        
        # Calculate and plot median evolution
        if len(all_track_data) > 0:
            # Create a common time grid for interpolation
            all_times = sorted(set(t for times, values in all_track_data for t in times))
            
            if len(all_times) > 1:
                # Interpolate all tracks to common time grid
                interpolated_values = []
                
                for times, values in all_track_data:
                    if len(times) >= 2:
                        # Interpolate this track to common time grid
                        interp_values = np.interp(all_times, times, values, 
                                                left=np.nan, right=np.nan)
                        interpolated_values.append(interp_values)
                
                if len(interpolated_values) > 0:
                    # Calculate median at each time point
                    interpolated_array = np.array(interpolated_values)
                    median_values = np.nanmedian(interpolated_array, axis=0)
                    
                    # Only plot where we have valid medians
                    valid_median_mask = ~np.isnan(median_values)
                    if np.any(valid_median_mask):
                        median_times = np.array(all_times)[valid_median_mask]
                        median_vals = median_values[valid_median_mask]
                        
                        # Plot median line (thick black line)
                        ax.plot(median_times, median_vals, 'k-', linewidth=3, 
                               label=f'Median ({len(all_track_data)} groups)', zorder=10)
        
        # Set scale and labels
        if prop['log']:
            ax.set_yscale('log')
        
        ax.set_xlabel('Lookback Time [Gyr]')
        ylabel = f"{prop['name']} [{prop['unit']}]"
        ax.set_ylabel(ylabel)
        ax.set_title(f"{prop['name']} Evolution")
        ax.grid(True, alpha=0.3)
        
        # Keep natural axis order: present (low lookback time) on left, early universe (high lookback time) on right
        
        # Add legend for median line (keep it simple) - only on first subplot
        if len(all_track_data) > 0 and prop_idx == 0:
            # Only show median in legend
            handles, labels = ax.get_legend_handles_labels()
            median_handles = [h for h, l in zip(handles, labels) if 'Median' in l]
            median_labels = [l for l in labels if 'Median' in l]
            if median_handles:
                ax.legend(median_handles, median_labels, loc='best', fontsize=8)
        
        # Add text showing number of tracks and time range - only on first subplot
        if prop_idx == 0:
            time_range = max(lookback_times) - min(lookback_times)
            ax.text(0.05, 0.95, f'N = {len(all_track_data)}\nΔt = {time_range:.1f} Gyr', 
                   transform=ax.transAxes, fontsize=10, 
                   bbox=dict(boxstyle='round', facecolor='white', alpha=0.8),
                   verticalalignment='top')
    
    # Overall title with corrected time information
    min_z = min(redshifts)
    max_z = max(redshifts)
    min_t = min(lookback_times)
    max_t = max(lookback_times)
    
    fig.suptitle(f'Group Property Evolution - Complete Tracks\n'
                f'{len(complete_tracks)} groups: {min_t:.1f}→{max_t:.1f} Gyr lookback (z={min_z:.2f}→{max_z:.2f})', 
                fontsize=16)
    
    plt.tight_layout()
    plt.subplots_adjust(top=0.90)
    
    # Save the plot
    output_file = output_dir / 'galactic_property_evolution.png'
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"Saved galactic property evolution plot to {output_file}")
    plt.close()
    
    # Print summary
    print(f"\nProperty Evolution Summary:")
    print(f"Complete tracks plotted: {len(complete_tracks)}")
    print(f"Snapshots: {snapshots}")
    print(f"Time evolution: {min_t:.1f} → {max_t:.1f} Gyr lookback")
    print(f"Redshift evolution: z={min_z:.2f} → z={max_z:.2f}")
    print(f"Time span: {time_range:.1f} Gyr")
    print(f"Properties: {[p['name'] for p in properties]}")
    
    if snapshot_data is not None:
        print(f"Enhanced galaxy properties calculated using snapshot data")
    else:
        print(f"Limited to group-level properties only (no snapshot_data provided)")

def save_evolution_results(group_evolution, evolution_summary, output_dir='./'):
    """
    Save group evolution results to files.
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Save evolution summary
    evolution_summary.to_csv(output_dir / 'group_evolution_summary.csv', index=False)
    print(f"Saved evolution summary to {output_dir / 'group_evolution_summary.csv'}")
    
    # Save detailed evolution tracks
    detailed_tracks = []
    
    for tracking_id, track in group_evolution.items():
        for i, snapshot in enumerate(track['snapshots']):
            row = {
                'tracking_id': tracking_id,
                'snapshot': snapshot,
                'sequence_number': i,
                'group_id': track['group_ids'][i],
                'status': track['status']
            }
            
            # Add group properties
            group_data = track['group_data'][i]
            for key, value in group_data.items():
                if key not in row:  # Avoid duplicates
                    row[key] = value
            
            # Add match info if available
            if 'match_scores' in track and i > 0:  # No match score for first entry
                row['match_score'] = track['match_scores'][i-1]
                row['match_type'] = track['match_types'][i-1]
            
            # Add cosmological time information
            cosmo_params = {'H0': 70.0, 'Omega_m': 0.27, 'Omega_Lambda': 0.73}
            row['redshift'] = snapshot_to_redshift(snapshot)
            row['lookback_time_gyr'] = snapshot_to_lookback_time(snapshot, cosmo_params)
            
            detailed_tracks.append(row)
    
    detailed_df = pd.DataFrame(detailed_tracks)
    detailed_df.to_csv(output_dir / 'group_evolution_detailed.csv', index=False)
    print(f"Saved detailed evolution tracks to {output_dir / 'group_evolution_detailed.csv'}")
    
    return detailed_df

# ============================================================================
# ANALYSIS AND PLOTTING FUNCTIONS (ORIGINAL)
# ============================================================================

def plot_spatial_distribution_centrals_only(data_merged, group_catalog, output_dir='./', snapshot=None, 
                                          dilute_factor=0.3, original_data=None, 
                                          max_background_points=50000):
    """
    Plot the spatial distribution showing only central galaxies with marker size proportional to group members.
    
    Parameters:
    data_merged: DataFrame with galaxy data including group information (filtered)
    group_catalog: DataFrame with group catalog
    output_dir: Output directory for plots
    snapshot: Snapshot number for filename (optional)
    dilute_factor: Fraction of background galaxies to plot (for performance)
    original_data: Original unfiltered galaxy data for background plot
    max_background_points: Maximum number of background points to plot
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    print(f'Plotting central galaxies spatial distribution for snapshot {snapshot if snapshot is not None else "unknown"}')
    
    # Use original_data for background if provided, otherwise use data_merged
    if original_data is not None:
        background_source = original_data
        print(f"Using {len(original_data)} galaxies from original dataset for background")
    else:
        background_source = data_merged
        print(f"Using {len(data_merged)} filtered galaxies for background")
    
    # Get box size from data range
    x_min, x_max = background_source['Posx'].min(), background_source['Posx'].max()
    y_min, y_max = background_source['Posy'].min(), background_source['Posy'].max()
    z_min, z_max = background_source['Posz'].min(), background_source['Posz'].max()
    
    # Filter for valid galaxies in background data (for black points)
    valid_background_gals = background_source[
        (background_source['Posx'] > 0) & 
        (background_source['Posy'] > 0) & 
        (background_source['Posz'] > 0)
    ].copy()
    
    # Background galaxies sampling
    target_background = min(int(len(valid_background_gals) * dilute_factor), max_background_points)
    if len(valid_background_gals) > target_background:
        background_sample = valid_background_gals.sample(n=target_background, random_state=42)
    else:
        background_sample = valid_background_gals
    
    # Get groups with at least 3 members for highlighting
    valid_groups = group_catalog[group_catalog['Nm'] >= 3].copy()
    print(f"Plotting {len(valid_groups)} groups with ≥3 members")
    
    # Extract central galaxy positions for each group
    central_galaxies_data = []
    
    for _, group_info in valid_groups.iterrows():
        group_id = group_info['group_id']
        
        # Find galaxies in this group
        group_galaxies = data_merged[data_merged['group_id'] == group_id]
        
        if len(group_galaxies) == 0:
            continue
            
        # Find the central galaxy (Type == 0, or use CentralGalaxyIndex, or most massive)
        central_galaxy = None
        
        # Method 1: Try Type == 0 (central galaxies)
        if 'Type' in group_galaxies.columns:
            central_candidates = group_galaxies[group_galaxies['Type'] == 0]
            if len(central_candidates) > 0:
                central_galaxy = central_candidates.iloc[0]
        
        # Method 2: If no Type==0, use CentralGalaxyIndex matching
        if central_galaxy is None and 'CentralGalaxyIndex' in group_galaxies.columns and 'GalaxyIndex' in group_galaxies.columns:
            central_galaxy_idx = group_galaxies['CentralGalaxyIndex'].iloc[0]
            central_candidates = group_galaxies[group_galaxies['GalaxyIndex'] == central_galaxy_idx]
            if len(central_candidates) > 0:
                central_galaxy = central_candidates.iloc[0]
        
        # Method 3: Fallback to most massive galaxy
        if central_galaxy is None:
            central_galaxy = group_galaxies.loc[group_galaxies['StellarMass'].idxmax()]
        
        # Store central galaxy info with group properties
        central_data = {
            'group_id': group_id,
            'Posx': central_galaxy['Posx'],
            'Posy': central_galaxy['Posy'],
            'Posz': central_galaxy['Posz'],
            'Nm': group_info['Nm'],  # Number of members
            'StellarMass': central_galaxy['StellarMass'],
            'group_total_mass': group_info.get('Sig_M', np.nan),  # Total group stellar mass
            'vdisp_gap': group_info.get('vdisp_gap', np.nan)
        }
        central_galaxies_data.append(central_data)
    
    # Convert to DataFrame
    central_galaxies_df = pd.DataFrame(central_galaxies_data)
    
    if len(central_galaxies_df) == 0:
        print("No central galaxies found to plot")
        return
    
    print(f"Found {len(central_galaxies_df)} central galaxies to plot")
    
    # Set up color mapping based on group size
    group_sizes = central_galaxies_df['Nm'].values
    size_norm = plt.Normalize(vmin=group_sizes.min(), vmax=group_sizes.max())
    plasma_colors = plt.cm.plasma(size_norm(group_sizes))
    
    # Set up marker sizes proportional to group size (with reasonable bounds)
    min_marker_size = 50   # Minimum size for small groups
    max_marker_size = 500  # Maximum size for large groups
    size_range = group_sizes.max() - group_sizes.min()
    
    if size_range > 0:
        marker_sizes = min_marker_size + (max_marker_size - min_marker_size) * (group_sizes - group_sizes.min()) / size_range
    else:
        marker_sizes = np.full(len(group_sizes), min_marker_size)
    
    print(f"Group sizes range: {group_sizes.min()}-{group_sizes.max()} members")
    print(f"Marker sizes range: {marker_sizes.min():.1f}-{marker_sizes.max():.1f} points")
    
    # Create the plot
    fig = plt.figure(figsize=(15, 12))
    
    # Plot 1: X-Y projection
    ax1 = plt.subplot(221)
    # Background galaxies (small, transparent)
    plt.scatter(background_sample['Posx'], background_sample['Posy'], 
               marker='o', s=0.3, c='black', alpha=0.5, label='Background')

    # Central galaxies (sized by group membership, colored by group size)
    scatter1 = plt.scatter(central_galaxies_df['Posx'], central_galaxies_df['Posy'], 
                          s=marker_sizes, c=group_sizes, cmap='plasma', alpha=0.8, 
                          edgecolors='black', linewidth=0.5, label='Group Centers', rasterized=True)
    
    plt.xlim(x_min, x_max)
    plt.ylim(y_min, y_max)
    plt.xlabel('X [Mpc/h]')
    plt.ylabel('Y [Mpc/h]')
    plt.title('X-Y Projection')
    
    # Plot 2: X-Z projection  
    ax2 = plt.subplot(222)
    plt.scatter(background_sample['Posx'], background_sample['Posz'], 
               marker='o', s=0.3, c='black', alpha=0.5)
    
    scatter2 = plt.scatter(central_galaxies_df['Posx'], central_galaxies_df['Posz'], 
                          s=marker_sizes, c=group_sizes, cmap='plasma', alpha=0.8, 
                          edgecolors='black', linewidth=0.5, rasterized=True)
    
    plt.xlim(x_min, x_max)
    plt.ylim(z_min, z_max)
    plt.xlabel('X [Mpc/h]')
    plt.ylabel('Z [Mpc/h]')
    plt.title('X-Z Projection')
    
    # Plot 3: Y-Z projection
    ax3 = plt.subplot(223)
    plt.scatter(background_sample['Posy'], background_sample['Posz'], 
               marker='o', s=0.3, c='black', alpha=0.5)
    
    scatter3 = plt.scatter(central_galaxies_df['Posy'], central_galaxies_df['Posz'], 
                          s=marker_sizes, c=group_sizes, cmap='plasma', alpha=0.8, 
                          edgecolors='black', linewidth=0.5, rasterized=True)
    
    plt.xlim(y_min, y_max)
    plt.ylim(z_min, z_max)
    plt.xlabel('Y [Mpc/h]')
    plt.ylabel('Z [Mpc/h]')
    plt.title('Y-Z Projection')

    # Plot 4: Group properties scatter plot
    ax4 = plt.subplot(224)
    if 'group_total_mass' in central_galaxies_df.columns and central_galaxies_df['group_total_mass'].notna().any():
        valid_mass = central_galaxies_df[central_galaxies_df['group_total_mass'].notna()]
        if len(valid_mass) > 0:
            if 'vdisp_gap' in valid_mass.columns and valid_mass['vdisp_gap'].notna().any():
                # Color by velocity dispersion
                scatter4 = plt.scatter(valid_mass['Nm'], valid_mass['group_total_mass'], 
                                     c=valid_mass['vdisp_gap'], cmap='plasma', alpha=0.8, 
                                     s=60, edgecolors='black', linewidth=0.5)
                cbar4 = plt.colorbar(scatter4, ax=ax4)
                cbar4.set_label('Velocity Dispersion [km/s]', fontsize=10)
            else:
                # Color by group size
                scatter4 = plt.scatter(valid_mass['Nm'], valid_mass['group_total_mass'], 
                                     c=valid_mass['Nm'], cmap='plasma', alpha=0.8, vmin=3, vmax=20,
                                     s=60, edgecolors='black', linewidth=0.5)
                cbar4 = plt.colorbar(scatter4, ax=ax4)
                cbar4.set_label('Group Size (N members)', fontsize=10)
            
            plt.xlabel('Group Size (N members)')
            plt.ylabel('Log10(Total Stellar Mass [M_sun])')
            plt.title('Group Mass vs Size')
        else:
            plt.text(0.5, 0.5, 'No valid group mass data', 
                    transform=ax4.transAxes, ha='center', va='center')
    else:
        plt.text(0.5, 0.5, 'No group mass data available', 
                transform=ax4.transAxes, ha='center', va='center')
    
    # Add colorbar for group sizes in spatial plots
    cbar_ax = fig.add_axes([0.15, 0.46, 0.7, 0.02])  # [left, bottom, width, height]
    cbar_spatial = fig.colorbar(scatter1, ax=[ax1, ax2, ax3], cax=cbar_ax,
                               location='top', shrink=0.8, pad=0.1)
    cbar_spatial.set_label('Group Size (N members)', fontsize=12)
    
    # Add marker size legend
    # Create a few example sizes to show the scaling
    example_sizes = [group_sizes.min(), np.median(group_sizes), group_sizes.max()]
    example_marker_sizes = min_marker_size + (max_marker_size - min_marker_size) * (np.array(example_sizes) - group_sizes.min()) / size_range if size_range > 0 else [min_marker_size] * 3
    
    # Add overall title with snapshot info
    if snapshot is not None:
        redshift = snapshot_to_redshift(snapshot)
        fig.suptitle(f'Spatial Distribution - Snapshot {snapshot} (z={redshift:.3f})', fontsize=16, y=0.98)
    
    plt.tight_layout()
    plt.subplots_adjust(top=0.92, bottom=0.05, hspace=0.35)
    
    # Save the plot with snapshot number in filename
    if snapshot is not None:
        output_file = output_dir / f'spatial_distribution_snap_{snapshot}.png'
    else:
        output_file = output_dir / 'central_galaxies_distribution.png'
    
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f'Saved central galaxies distribution plot to {output_file}')
    plt.close()
    
    # Print summary
    print(f"\nCentral Galaxies Distribution Summary (Snapshot {snapshot}):")
    print(f"Total background galaxies plotted: {len(background_sample):,}")
    print(f"Central galaxies plotted: {len(central_galaxies_df):,}")
    print(f"Group sizes: {group_sizes.min()}-{group_sizes.max()} members")
    print(f"Marker sizes: {marker_sizes.min():.1f}-{marker_sizes.max():.1f} points")
    print(f"Box size: {max(x_max, y_max, z_max):.1f} Mpc/h")
    
    # Return the central galaxies data for further analysis if needed
    return central_galaxies_df

def plot_xz_projection_centrals_only(data_merged, group_catalog, output_dir='./', snapshot=None, 
                                    dilute_factor=0.3, original_data=None, 
                                    max_background_points=50000):
    """
    Plot dedicated X-Z projection showing only central galaxies with marker size proportional to group members.
    
    Parameters:
    data_merged: DataFrame with galaxy data including group information (filtered)
    group_catalog: DataFrame with group catalog
    output_dir: Output directory for plots
    snapshot: Snapshot number for filename (optional)
    dilute_factor: Fraction of background galaxies to plot (for performance)
    original_data: Original unfiltered galaxy data for background plot
    max_background_points: Maximum number of background points to plot
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    print(f'Creating dedicated X-Z projection for snapshot {snapshot if snapshot is not None else "unknown"}')
    
    # Use original_data for background if provided, otherwise use data_merged
    if original_data is not None:
        background_source = original_data
    else:
        background_source = data_merged
    
    # Get box size from data range
    x_min, x_max = background_source['Posx'].min(), background_source['Posx'].max()
    z_min, z_max = background_source['Posz'].min(), background_source['Posz'].max()
    
    # Filter for valid galaxies in background data (for gray points)
    valid_background_gals = background_source[
        (background_source['Posx'] > 0) & 
        (background_source['Posy'] > 0) & 
        (background_source['Posz'] > 0)
    ].copy()
    
    # Background galaxies sampling
    target_background = min(int(len(valid_background_gals) * dilute_factor), max_background_points)
    if len(valid_background_gals) > target_background:
        background_sample = valid_background_gals.sample(n=target_background, random_state=42)
    else:
        background_sample = valid_background_gals
    
    # Get groups with at least 3 members for highlighting
    valid_groups = group_catalog[group_catalog['Nm'] >= 3].copy()
    
    # Extract central galaxy positions for each group (same logic as before)
    central_galaxies_data = []
    
    for _, group_info in valid_groups.iterrows():
        group_id = group_info['group_id']
        
        # Find galaxies in this group
        group_galaxies = data_merged[data_merged['group_id'] == group_id]
        
        if len(group_galaxies) == 0:
            continue
            
        # Find the central galaxy (same logic as in main function)
        central_galaxy = None
        
        # Method 1: Try Type == 0 (central galaxies)
        if 'Type' in group_galaxies.columns:
            central_candidates = group_galaxies[group_galaxies['Type'] == 0]
            if len(central_candidates) > 0:
                central_galaxy = central_candidates.iloc[0]
        
        # Method 2: If no Type==0, use CentralGalaxyIndex matching
        if central_galaxy is None and 'CentralGalaxyIndex' in group_galaxies.columns and 'GalaxyIndex' in group_galaxies.columns:
            central_galaxy_idx = group_galaxies['CentralGalaxyIndex'].iloc[0]
            central_candidates = group_galaxies[group_galaxies['GalaxyIndex'] == central_galaxy_idx]
            if len(central_candidates) > 0:
                central_galaxy = central_candidates.iloc[0]
        
        # Method 3: Fallback to most massive galaxy
        if central_galaxy is None:
            central_galaxy = group_galaxies.loc[group_galaxies['StellarMass'].idxmax()]
        
        # Store central galaxy info with group properties
        central_data = {
            'group_id': group_id,
            'Posx': central_galaxy['Posx'],
            'Posz': central_galaxy['Posz'],
            'Nm': group_info['Nm'],  # Number of members
            'StellarMass': central_galaxy['StellarMass'],
            'group_total_mass': group_info.get('Sig_M', np.nan),  # Total group stellar mass
            'vdisp_gap': group_info.get('vdisp_gap', np.nan)
        }
        central_galaxies_data.append(central_data)
    
    # Convert to DataFrame
    central_galaxies_df = pd.DataFrame(central_galaxies_data)
    
    if len(central_galaxies_df) == 0:
        print("No central galaxies found for X-Z projection")
        return
    
    # Set up color mapping and marker sizes (same as main function)
    group_sizes = central_galaxies_df['Nm'].values
    size_norm = plt.Normalize(vmin=group_sizes.min(), vmax=group_sizes.max())
    
    
    # Set up marker sizes proportional to group size
    min_marker_size = 50   # Slightly larger for single plot
    max_marker_size = 2500  # Slightly larger for single plot
    size_range = group_sizes.max() - group_sizes.min()
    
    if size_range > 0:
        marker_sizes = min_marker_size + (max_marker_size - min_marker_size) * (group_sizes - group_sizes.min()) / size_range
    else:
        marker_sizes = np.full(len(group_sizes), min_marker_size)
    
    # Create the dedicated X-Z projection plot
    fig, ax = plt.subplots(1, 1, figsize=(12, 10))  # Larger single plot
    
    # Background galaxies (small, transparent gray)
    ax.scatter(background_sample['Posx'], background_sample['Posz'], 
               marker='o', s=0.5, c='black', alpha=0.5, 
               label=f'Background ({len(background_sample):,} galaxies)')

    # Central galaxies (sized by group membership, colored by group size)
    scatter = ax.scatter(central_galaxies_df['Posx'], central_galaxies_df['Posz'], 
                        s=marker_sizes, c=group_sizes, cmap='plasma', alpha=0.8, 
                        edgecolors='black', linewidth=0.5, vmin=3, vmax=20,
                        label=f'Group Centers ({len(central_galaxies_df)} groups)', rasterized=True)
    
    # Set limits and labels
    ax.set_xlim(x_min, x_max)
    ax.set_ylim(z_min, z_max)
    ax.set_xlabel('X [Mpc/h]', fontsize=14)
    ax.set_ylabel('Z [Mpc/h]', fontsize=14)
    
    # Add grid
    ax.grid(True, alpha=0.3)
    
    # Add colorbar for group sizes
    cbar = plt.colorbar(scatter, ax=ax, shrink=0.8, pad=0.02)
    cbar.set_label('Group Size (N members)', fontsize=12)
    cbar.ax.tick_params(labelsize=11)
    
    # Add marker size legend - create a few example sizes
    if size_range > 0:
        example_sizes = [group_sizes.min(), np.median(group_sizes), group_sizes.max()]
        example_marker_sizes = min_marker_size + (max_marker_size - min_marker_size) * (np.array(example_sizes) - group_sizes.min()) / size_range
        
        # Create legend elements for marker sizes
        from matplotlib.lines import Line2D
        legend_elements = []
        for size_val, marker_size in zip(example_sizes, example_marker_sizes):
            legend_elements.append(Line2D([0], [0], marker='o', color='w', 
                                        markerfacecolor='gray', markersize=np.sqrt(marker_size/10), 
                                        label=f'{int(size_val)} members'))
    
    # Add title with snapshot info
    if snapshot is not None:
        redshift = snapshot_to_redshift(snapshot)
        title = f'X-Z Projection - Group Positions\n'
        title += f'Snapshot {snapshot} (z={redshift:.3f})'
        ax.set_title(title, fontsize=16, pad=20)
    else:
        ax.set_title('X-Z Projection - Group Positions\n', 
                    fontsize=16, pad=20)
    
    # Adjust layout
    plt.tight_layout()
    
    # Save the plot with specific filename
    if snapshot is not None:
        output_file = output_dir / f'xz_projection_centrals_snap_{snapshot}.png'
    else:
        output_file = output_dir / 'xz_projection_centrals.png'
    
    plt.savefig(output_file, dpi=200, bbox_inches='tight')  # Higher DPI for single plot
    print(f'Saved X-Z projection plot to {output_file}')
    plt.close()
    
    # Print summary
    print(f"X-Z Projection Summary (Snapshot {snapshot}):")
    print(f"  Central galaxies plotted: {len(central_galaxies_df):,}")
    print(f"  Background galaxies: {len(background_sample):,}")
    print(f"  Group sizes: {group_sizes.min()}-{group_sizes.max()} members")
    print(f"  X range: {x_min:.1f} to {x_max:.1f} Mpc/h")
    print(f"  Z range: {z_min:.1f} to {z_max:.1f} Mpc/h")
    
    return central_galaxies_df

def plot_group_properties(group_catalog, output_dir='./', snapshot=None):
    """
    Create plots of group properties.
    
    Parameters:
    group_catalog: DataFrame with group properties
    output_dir: Output directory for plots
    snapshot: Snapshot number for filename (optional)
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Filter out groups with too few members or invalid data
    valid_groups = group_catalog[group_catalog['Nm'] >= 3].copy()
    
    # Further filter for valid velocity dispersion data
    if 'vdisp_gap' in valid_groups.columns:
        valid_groups = valid_groups[(valid_groups['vdisp_gap'].notna()) & 
                                   (valid_groups['vdisp_gap'] > 0)]
    
    print(f"Plotting properties for {len(valid_groups)} valid groups (Snapshot {snapshot})")
    
    if len(valid_groups) == 0:
        print("No valid groups for plotting")
        return
    
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    
    # Group richness distribution
    axes[0, 0].hist(valid_groups['Nm'], bins=min(50, len(valid_groups)//2 + 1), 
                   alpha=0.7, edgecolor='black', color='#440154')  # Dark plasma color
    axes[0, 0].set_xlabel('Number of Group Members')
    axes[0, 0].set_ylabel('Number of Groups')
    axes[0, 0].set_title('Group Richness Distribution')
    if valid_groups['Nm'].max() > 10:
        axes[0, 0].set_yscale('log')
    
    # Velocity dispersion distribution
    if 'vdisp_gap' in valid_groups.columns and valid_groups['vdisp_gap'].notna().any():
        vdisp_data = valid_groups['vdisp_gap'].dropna()
        if len(vdisp_data) > 0:
            axes[0, 1].hist(vdisp_data, bins=min(50, len(vdisp_data)//2 + 1), 
                           alpha=0.7, edgecolor='black', color='#31688e')  # Mid plasma color
            axes[0, 1].set_xlabel('Velocity Dispersion (km/s)')
            axes[0, 1].set_ylabel('Number of Groups')
            axes[0, 1].set_title('Velocity Dispersion Distribution')
        else:
            axes[0, 1].text(0.5, 0.5, 'No valid velocity\ndispersion data', 
                          transform=axes[0, 1].transAxes, ha='center', va='center')
    else:
        axes[0, 1].text(0.5, 0.5, 'No velocity dispersion\ndata available', 
                      transform=axes[0, 1].transAxes, ha='center', va='center')
    
    # Mass-richness relation
    if 'Sig_M' in valid_groups.columns and valid_groups['Sig_M'].notna().any():
        mass_valid = valid_groups[valid_groups['Sig_M'].notna()]
        if len(mass_valid) > 0:
            # Color by group size using plasma
            scatter = axes[1, 0].scatter(mass_valid['Nm'], mass_valid['Sig_M'], 
                                       c=mass_valid['Nm'], cmap='plasma', 
                                       alpha=0.7, s=30, edgecolors='black', linewidth=0.5)
            axes[1, 0].set_xlabel('Number of Group Members')
            axes[1, 0].set_ylabel('Log10(Total Stellar Mass [M_sun])')
            axes[1, 0].set_title('Mass-Richness Relation')
            cbar1 = plt.colorbar(scatter, ax=axes[1, 0])
            cbar1.set_label('Group Size')
        else:
            axes[1, 0].text(0.5, 0.5, 'No valid mass data', 
                          transform=axes[1, 0].transAxes, ha='center', va='center')
    else:
        axes[1, 0].text(0.5, 0.5, 'No mass data available', 
                      transform=axes[1, 0].transAxes, ha='center', va='center')
    
    # Velocity dispersion vs stellar mass
    if ('Sig_M' in valid_groups.columns and 'vdisp_gap' in valid_groups.columns and 
        valid_groups['Sig_M'].notna().any() and valid_groups['vdisp_gap'].notna().any()):
        both_valid = valid_groups[(valid_groups['Sig_M'].notna()) & 
                                 (valid_groups['vdisp_gap'].notna())]
        if len(both_valid) > 0:
            # Color by velocity dispersion using plasma
            scatter = axes[1, 1].scatter(both_valid['Sig_M'], both_valid['vdisp_gap'], 
                                       c=both_valid['vdisp_gap'], cmap='plasma',
                                       alpha=0.7, s=30, edgecolors='black', linewidth=0.5)
            axes[1, 1].set_xlabel('Log10(Total Stellar Mass [M_sun])')
            axes[1, 1].set_ylabel('Velocity Dispersion (km/s)')
            axes[1, 1].set_title('Velocity Dispersion vs Stellar Mass')
            cbar2 = plt.colorbar(scatter, ax=axes[1, 1])
            cbar2.set_label('Velocity Dispersion [km/s]')
        else:
            axes[1, 1].text(0.5, 0.5, 'No data with both\nmass and velocity dispersion', 
                          transform=axes[1, 1].transAxes, ha='center', va='center')
    else:
        axes[1, 1].text(0.5, 0.5, 'Insufficient data for\nmass-velocity relation', 
                      transform=axes[1, 1].transAxes, ha='center', va='center')
    
    # Add overall title with snapshot info
    if snapshot is not None:
        redshift = snapshot_to_redshift(snapshot)
        fig.suptitle(f'Group Properties - Snapshot {snapshot} (z={redshift:.3f})', fontsize=16)
    
    plt.tight_layout()
    plt.subplots_adjust(top=0.92)  # Make room for title
    
    # Save with snapshot number in filename
    if snapshot is not None:
        output_file = output_dir / f'group_properties_snap_{snapshot}.png'
    else:
        output_file = output_dir / 'group_properties.png'
    
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"Saved plot to {output_file}")
    plt.close()
    
    # Print some statistics
    print(f"\nGroup Statistics (Snapshot {snapshot}):")
    print(f"Total groups: {len(valid_groups)}")
    print(f"Mean group richness: {valid_groups['Nm'].mean():.1f}")
    
    if 'vdisp_gap' in valid_groups.columns:
        vdisp_data = valid_groups['vdisp_gap'].dropna()
        if len(vdisp_data) > 0:
            print(f"Mean velocity dispersion: {vdisp_data.mean():.1f} ± {vdisp_data.std():.1f} km/s")
    
    if 'Sig_M' in valid_groups.columns:
        mass_data = valid_groups['Sig_M'].dropna()
        if len(mass_data) > 0:
            print(f"Mean total stellar mass: {mass_data.mean():.2f} log10(M_sun)")

def save_results(data_merged, group_catalog, output_dir='./'):
    """
    Save results to CSV files, including all SAGE galactic properties.
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Save group catalog
    group_catalog.to_csv(output_dir / 'sage_group_catalog_fixed.csv', index=False)
    print(f"Saved group catalog to {output_dir / 'sage_group_catalog_fixed.csv'}")
    
    # Save individual galaxy data with all SAGE properties
    galaxy_output = data_merged.copy()
    
    # Ensure all SAGE properties are included in the galaxy output
    sage_properties = [
        'GalaxyIndex', 'CentralGalaxyIndex', 'SAGEHaloIndex', 'Type', 'SnapNum',
        'Posx', 'Posy', 'Posz', 'Velx', 'Vely', 'Velz', 'VelDisp',
        'StellarMass', 'Mvir', 'CentralMvir', 'BulgeMass', 'BlackHoleMass',
        'ColdGas', 'HI_gas', 'H2_gas', 'MetalsColdGas', 'HotGas', 
        'IntraClusterStars', 'infallMvir', 'OutflowRate', 'CGMgas',
        'Rvir', 'DiskRadius', 'Vvir', 'Vmax', 'SfrDisk', 'SfrBulge',
        'MassLoading', 'Cooling'
    ]
    
    # Add group-derived properties if they exist
    group_properties = ['group_id', 'vdisp_gap', 'radius', 'Mhalo_P3M', 'Mhalo_VT', 'Mhalo_CVT']
    
    # Select columns that exist in the data
    available_sage_props = [col for col in sage_properties if col in galaxy_output.columns]
    available_group_props = [col for col in group_properties if col in galaxy_output.columns]
    
    output_columns = available_sage_props + available_group_props
    galaxy_subset = galaxy_output[output_columns]
    
    # Save with descriptive filename
    galaxy_output_file = output_dir / 'sage_galaxies_with_groups.csv'
    galaxy_subset.to_csv(galaxy_output_file, index=False)
    print(f"Saved galaxy data with all SAGE properties to {galaxy_output_file}")
    
    # Print summary of what was saved
    print(f"\nSaved data summary:")
    print(f"  Group catalog: {len(group_catalog)} groups")
    print(f"  Galaxy data: {len(galaxy_subset)} galaxies")
    print(f"  SAGE properties included: {len(available_sage_props)}")
    print(f"  Group properties included: {len(available_group_props)}")
    
    # List the SAGE properties that were saved
    print(f"  SAGE properties saved: {', '.join(available_sage_props)}")
    if available_group_props:
        print(f"  Group properties saved: {', '.join(available_group_props)}")
    
    # Check for any missing expected properties
    missing_sage_props = [col for col in sage_properties if col not in galaxy_output.columns]
    if missing_sage_props:
        print(f"  Missing SAGE properties: {', '.join(missing_sage_props)}")
        print(f"    (These were not found in the input data)")
        
    return galaxy_output_file

# ============================================================================
# MAIN FUNCTION - EXTENDED VERSION
# ============================================================================

def main():
    """
    Main function to run the SAGE 2.0 group finding analysis with evolution tracking.
    """
    parser = argparse.ArgumentParser(description='SAGE 2.0 Group Evolution Tracker - Complete Version')
    
    # Basic arguments
    parser.add_argument('--input_file', type=str, default=None,
                       help='Path to SAGE HDF5 output file')
    parser.add_argument('--output_dir', type=str, default='./',
                       help='Output directory for results')
    
    # Snapshot arguments - SIMPLIFIED
    parser.add_argument('--snapshots', type=int, nargs='+', default=None,
                       help='Snapshots to analyze (e.g., --snapshots 63 50 40 30). Single snapshot for basic analysis, multiple for evolution tracking.')
    parser.add_argument('--plot_snapshots', type=int, nargs='+', default=None,
                       help='Snapshots for spatial/properties plots (e.g., --plot_snapshots 63 50). If not specified, uses latest snapshot from --snapshots.')
    
    # Evolution tracking parameters
    parser.add_argument('--track_overlap_fraction', type=float, default=0.3,
                       help='Minimum galaxy overlap fraction for group tracking (default: 0.3)')
    parser.add_argument('--track_max_distance', type=float, default=2.0,
                       help='Maximum distance (Mpc) for group tracking (default: 2.0)')
    parser.add_argument('--evolution_only', action='store_true',
                       help='Only perform evolution analysis, skip single-snapshot plots')
    
    # Original arguments (maintained for compatibility)
    parser.add_argument('--min_group_size', type=int, default=3,
                       help='Minimum number of galaxies per group')
    parser.add_argument('--explore', action='store_true',
                       help='Just explore the HDF5 file structure and exit')
    parser.add_argument('--sample_data', action='store_true',
                       help='Use sample data instead of loading file')
    parser.add_argument('--group_id_field', type=str, default='CentralGalaxyIndex',
                       help='Field to use for group identification')
    parser.add_argument('--stellar_mass_min', type=float, default=1e8,
                       help='Minimum stellar mass in M_sun units')
    parser.add_argument('--stellar_mass_max', type=float, default=1e12,
                       help='Maximum stellar mass in M_sun units')
    parser.add_argument('--max_group_size', type=int, default=1000,
                       help='Maximum allowed group size')
    parser.add_argument('--analyze_groups', action='store_true',
                       help='Analyze different group ID fields and exit')
    parser.add_argument('--dilute_factor', type=float, default=0.3,
                       help='Fraction of background galaxies to plot in spatial distribution (0.1-1.0)')
    parser.add_argument('--max_background_points', type=int, default=50000,
                       help='Maximum background points for spatial plots (default: 50,000)')
    parser.add_argument('--max_group_points', type=int, default=100000,
                       help='Maximum group member points for spatial plots (default: 100,000)')
    parser.add_argument('--max_group_members', type=int, default=50,
                       help='Maximum members for galaxy groups (vs clusters)')
    parser.add_argument('--max_group_velocity_dispersion', type=float, default=600,
                       help='Maximum velocity dispersion for groups (km/s)')
    parser.add_argument('--separate_groups_clusters', action='store_true',
                       help='Apply cuts to separate groups from clusters')
    parser.add_argument('--no_progress', action='store_true',
                       help='Disable progress bar during group processing')
    
    args = parser.parse_args()
    
    print("="*60)
    print("SAGE 2.0 GROUP EVOLUTION TRACKER - COMPLETE VERSION")
    print("="*60)
    
    # Check if tqdm is available
    if not TQDM_AVAILABLE and not args.no_progress:
        print("Note: Install tqdm for progress bars: pip install tqdm")
    
    # If just exploring, do that and exit
    if args.explore and args.input_file and Path(args.input_file).exists():
        explore_hdf5_structure(args.input_file, args.snapshots[0] if args.snapshots else 63)
        return
    
    # Determine snapshots to analyze
    if args.snapshots is not None:
        snapshots = args.snapshots
    else:
        # Default single snapshot
        snapshots = [63]
    
    # Determine snapshots for plotting
    if args.plot_snapshots is not None:
        plot_snapshots = args.plot_snapshots
    else:
        # Default to latest snapshot from analysis
        plot_snapshots = [max(snapshots)]
    
    # Determine analysis mode
    multi_snapshot_mode = len(snapshots) > 1
    
    print(f"Analysis mode: {'Multi-snapshot evolution' if multi_snapshot_mode else 'Single snapshot'}")
    print(f"Analysis snapshots: {snapshots}")
    print(f"Plot snapshots: {plot_snapshots}")
    
    # Show redshift mapping for multi-snapshot mode
    if multi_snapshot_mode:
        print_snapshot_redshift_mapping(snapshots)
    
    # Set up parameters for group finding
    group_params = {
        'group_id_col': args.group_id_field,
        'stellar_mass_min_msun': args.stellar_mass_min,
        'stellar_mass_max_msun': args.stellar_mass_max,
        'max_group_size': args.max_group_size,
        'separate_groups_clusters': args.separate_groups_clusters,
        'max_group_members': args.max_group_members,
        'max_group_velocity_dispersion': args.max_group_velocity_dispersion,
        'show_progress': not args.no_progress,
        'cosmo_params': {'H0': 70.0, 'Omega_m': 0.27, 'Omega_Lambda': 0.73, 'h': 0.7}
    }
    
    if multi_snapshot_mode:
        # Multi-snapshot evolution analysis
        
        if args.sample_data:
            print("Multi-snapshot mode not supported with sample data. Use --input_file instead.")
            return
        
        if args.input_file is None or not Path(args.input_file).exists():
            print("Multi-snapshot mode requires a valid input file.")
            return
        
        # Load data from all snapshots
        print(f"\n" + "="*50)
        print("LOADING MULTI-SNAPSHOT DATA")
        print("="*50)
        
        snapshot_data = load_multiple_snapshots(args.input_file, snapshots, **group_params)
        
        if len(snapshot_data) < 2:
            print("Need at least 2 snapshots for evolution tracking.")
            return
        
        # Track group evolution
        group_evolution, evolution_summary = track_groups_across_snapshots(
            snapshot_data, snapshots,
            track_overlap_fraction=args.track_overlap_fraction,
            track_max_distance=args.track_max_distance,
            show_progress=not args.no_progress
        )
        
        # Create evolution plots
        print(f"\n" + "="*50)
        print("CREATING EVOLUTION VISUALIZATIONS")
        print("="*50)
        plot_group_evolution(group_evolution, evolution_summary, args.output_dir)
        
        # Create galactic property evolution plot
        plot_group_property_evolution(group_evolution, snapshots, output_dir=args.output_dir, snapshot_data=snapshot_data)
        
        # Save evolution results
        print(f"\nSaving evolution results...")
        save_evolution_results(group_evolution, evolution_summary, args.output_dir)
        
        # Print evolution summary
        print(f"\n" + "="*60)
        print("EVOLUTION ANALYSIS SUMMARY")
        print("="*60)
        print(f"Snapshots analyzed: {len(snapshots)}")
        print(f"Total tracking chains: {len(group_evolution)}")
        
        complete_chains = len([t for t in group_evolution.values() if len(t['snapshots']) == len(snapshots)])
        print(f"Complete evolution chains: {complete_chains}")
        
        if len(evolution_summary) > 0:
            if 'member_evolution' in evolution_summary.columns:
                growing_groups = len(evolution_summary[evolution_summary['member_evolution'] > 0])
                shrinking_groups = len(evolution_summary[evolution_summary['member_evolution'] < 0])
                stable_groups = len(evolution_summary[evolution_summary['member_evolution'] == 0])
                print(f"Growing groups: {growing_groups}")
                print(f"Shrinking groups: {shrinking_groups}")
                print(f"Stable groups: {stable_groups}")
            
            if 'mass_evolution' in evolution_summary.columns:
                mass_growing = len(evolution_summary[evolution_summary['mass_evolution'] > 0])
                mass_losing = len(evolution_summary[evolution_summary['mass_evolution'] < 0])
                print(f"Mass growing groups: {mass_growing}")
                print(f"Mass losing groups: {mass_losing}")
        
        disappeared_groups = len([t for t in group_evolution.values() if t['status'] == 'disappeared'])
        appeared_groups = len([t for t in group_evolution.values() if t.get('formation_type') == 'appeared'])
        print(f"Groups that disappeared: {disappeared_groups}")
        print(f"Groups that appeared: {appeared_groups}")
        
        # Create snapshot-specific plots if requested
        if not args.evolution_only:
            print(f"\n" + "="*50)
            print("CREATING SNAPSHOT-SPECIFIC PLOTS")
            print("="*50)
            
            for plot_snap in plot_snapshots:
                if plot_snap in snapshot_data:
                    print(f"\nCreating plots for snapshot {plot_snap}...")
                    data_merged, group_catalog = snapshot_data[plot_snap]
                    
                    # Use original plotting functions with snapshot number
                    valid_groups = group_catalog[group_catalog['Nm'] >= args.min_group_size]
                    if len(valid_groups) > 0:
                        plot_group_properties(valid_groups, args.output_dir, plot_snap)
                        
                        # Load original data for spatial plot background
                        original_data = load_sage_hdf5(args.input_file, plot_snap)
                        central_galaxies_df = plot_spatial_distribution_centrals_only(
                                        data_merged, group_catalog, args.output_dir, plot_snap,
                                        args.dilute_factor, original_data, args.max_background_points)
                        plot_xz_projection_centrals_only(
                                        data_merged, group_catalog, args.output_dir, plot_snap,
                                        args.dilute_factor, original_data, args.max_background_points)
                
                        
                        # Save single-snapshot results for this snapshot
                        save_results(data_merged, group_catalog, args.output_dir)
                    else:
                        print(f"No valid groups found for snapshot {plot_snap}")
                else:
                    print(f"Warning: Snapshot {plot_snap} not found in analysis data")
        
        print(f"\nResults saved to: {Path(args.output_dir).absolute()}")
        print(f"Evolution summary: group_evolution_summary.csv")
        print(f"Detailed tracks: group_evolution_detailed.csv")
        print(f"Evolution plots: group_evolution_analysis.png, individual_evolution_tracks.png")
        print(f"Property evolution: galactic_property_evolution.png")
        if not args.evolution_only:
            print(f"Snapshot plots: spatial_distribution_snap_X.png, group_properties_snap_X.png")
        
    else:
        # Single snapshot analysis (original functionality)
        print(f"\n" + "="*50)
        print("RUNNING SINGLE-SNAPSHOT ANALYSIS")
        print("="*50)
        
        single_snapshot = snapshots[0]
        
        # Load data
        if args.sample_data or args.input_file is None:
            sage_data = create_sample_data()
        else:
            if not Path(args.input_file).exists():
                print(f"Input file {args.input_file} not found. Using sample data instead.")
                sage_data = create_sample_data()
            else:
                sage_data = load_sage_hdf5(args.input_file, single_snapshot)
        
        print(f"\nData loaded: {len(sage_data)} galaxies")
        print(f"Columns: {list(sage_data.columns)}")
        
        # Analyze group ID fields if requested
        if args.analyze_groups:
            potential_fields = analyze_group_id_fields(sage_data)
            print(f"\nRecommendation: Try different group_id_field values:")
            for field in potential_fields:
                print(f"  --group_id_field {field}")
            return
        
        # Verify group ID field exists
        if args.group_id_field not in sage_data.columns:
            print(f"\nERROR: Group ID field '{args.group_id_field}' not found in data!")
            print("Available columns that might work:")
            for col in sage_data.columns:
                if 'halo' in col.lower() or 'index' in col.lower() or 'central' in col.lower():
                    print(f"  {col}")
            return
        
        # Run group finding analysis
        print(f"\n" + "="*50)
        print("Running Group Finding Analysis")
        print("="*50)
        
        data_merged, group_catalog = SAGE_Group_Catalog(sage_data, **group_params)
        
        print(f"\nGroup analysis complete!")
        print(f"Found {len(group_catalog)} groups")
        
        # Save separate catalogs if groups/clusters were separated
        if args.separate_groups_clusters and len(group_catalog) > 0:
            # The group_catalog now contains only groups (clusters were filtered out)
            print(f"Galaxy groups catalog saved (clusters excluded)")
            
            # Save the groups-only catalog
            group_catalog.to_csv(Path(args.output_dir) / 'sage_galaxy_groups_only.csv', index=False)
            print(f"Saved galaxy groups catalog to {Path(args.output_dir) / 'sage_galaxy_groups_only.csv'}")
        
        print(f"Groups with ≥{args.min_group_size} members: {len(group_catalog[group_catalog['Nm'] >= args.min_group_size])}")
        
        # Filter groups by minimum size
        valid_groups = group_catalog[group_catalog['Nm'] >= args.min_group_size]
        
        if len(valid_groups) > 0:
            # Create plots for specified snapshots
            print(f"\nCreating plots...")
            
            for plot_snap in plot_snapshots:
                if plot_snap == single_snapshot:
                    # Use the data we already have
                    print(f"Creating plots for snapshot {plot_snap}...")
                    plot_group_properties(valid_groups, args.output_dir, plot_snap)
                    central_galaxies_df = plot_spatial_distribution_centrals_only(
                                    data_merged, group_catalog, args.output_dir, plot_snap,
                                    args.dilute_factor, sage_data, args.max_background_points)
                    plot_xz_projection_centrals_only(  # ← CHANGE TO THIS
                                data_merged, group_catalog, args.output_dir, plot_snap,
                                args.dilute_factor, sage_data, args.max_background_points)
                    save_results(data_merged, group_catalog, args.output_dir)
                else:
                    # Load data for different snapshot
                    print(f"Loading and creating plots for snapshot {plot_snap}...")
                    if args.input_file and Path(args.input_file).exists():
                        other_sage_data = load_sage_hdf5(args.input_file, plot_snap)
                        other_data_merged, other_group_catalog = SAGE_Group_Catalog(other_sage_data, **group_params)
                        other_valid_groups = other_group_catalog[other_group_catalog['Nm'] >= args.min_group_size]
                        
                        if len(other_valid_groups) > 0:
                            plot_group_properties(other_valid_groups, args.output_dir, plot_snap)
                            central_galaxies_df = plot_spatial_distribution_centrals_only(
                                            other_data_merged, other_group_catalog, args.output_dir, plot_snap,
                                            args.dilute_factor, other_sage_data, args.max_background_points)
                            plot_xz_projection_centrals_only(
                                        data_merged, group_catalog, args.output_dir, plot_snap,
                                        args.dilute_factor, original_data, args.max_background_points)
                        else:
                            print(f"No valid groups found for snapshot {plot_snap}")
                    else:
                        print(f"Cannot load snapshot {plot_snap} - no input file specified")
            
            # Save results
            print(f"\nSaving results...")
            galaxy_output_file = save_results(data_merged, group_catalog, args.output_dir)
            
            # Print summary statistics
            print("\n" + "="*50)
            print("SUMMARY")
            print("="*50)
            print(f"Total galaxies analyzed: {len(sage_data):,}")
            print(f"Total groups found: {len(group_catalog):,}")
            print(f"Groups with ≥{args.min_group_size} members: {len(valid_groups):,}")
            print(f"Largest group size: {group_catalog['Nm'].max()}")
            if len(valid_groups) > 0 and 'vdisp_gap' in valid_groups.columns:
                valid_vdisp = valid_groups['vdisp_gap'].dropna()
                if len(valid_vdisp) > 0:
                    print(f"Mean velocity dispersion: {valid_vdisp.mean():.1f} ± {valid_vdisp.std():.1f} km/s")
            
            # Show summary of SAGE properties included
            sage_mass_props = [col for col in data_merged.columns if col in 
                              ['StellarMass', 'Mvir', 'ColdGas', 'HotGas', 'BlackHoleMass', 'BulgeMass']]
            if sage_mass_props:
                print(f"SAGE mass properties included: {', '.join(sage_mass_props)}")
                
            sage_sfr_props = [col for col in data_merged.columns if col in ['SfrDisk', 'SfrBulge']]
            if sage_sfr_props:
                print(f"SAGE SFR properties included: {', '.join(sage_sfr_props)}")
                
            sage_gas_props = [col for col in data_merged.columns if col in 
                             ['HI_gas', 'H2_gas', 'CGMgas', 'MetalsColdGas']]
            if sage_gas_props:
                print(f"SAGE gas properties included: {', '.join(sage_gas_props)}")
            
            print(f"Results saved to: {Path(args.output_dir).absolute()}")
            print(f"Main galaxy catalog: {galaxy_output_file.name}")
            print(f"Plots created for snapshots: {plot_snapshots}")
            
            # Print group vs cluster summary if separation was applied
            if args.separate_groups_clusters:
                print(f"\nGroup/Cluster Separation Applied:")
                print(f"  Max group members: {args.max_group_members}")
                print(f"  Max group velocity dispersion: {args.max_group_velocity_dispersion} km/s")
                print(f"  Results contain galaxy groups only (clusters excluded)")
        else:
            print("No valid groups found with the specified criteria.")
    
    print(f"\n" + "="*60)
    print("ANALYSIS COMPLETE")
    print("="*60)

if __name__ == "__main__":
    main()