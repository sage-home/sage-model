#!/usr/bin/env python3
"""
SAGE 2.0 Group Finding Algorithm - FIXED VERSION
Complete script for analyzing galaxy groups from SAGE 2.0 simulation data.

Usage:
    python sage_group_finder_fixed.py --input_file sage_output.hdf5 --snapshot 63
    
This fixed version addresses:
1. Proper loading of individual position/velocity fields 
2. Correct group ID field mapping using CentralGalaxyIndex by default
3. Simplified field mapping logic
4. Better handling of stellar mass units (now uses standard M_sun units)
5. Automatic identification of central galaxies using CentralGalaxyIndex

Example usage:
    # Basic run with default mass range (1e8 - 1e12 M_sun)
    python sage_group_finder_fixed.py --input_file output/model.hdf5 --snapshot 63
    
    # Custom mass range
    python sage_group_finder_fixed.py --input_file output/model.hdf5 --snapshot 63 --stellar_mass_min 5e8 --stellar_mass_max 5e11
    
    # Use different group ID field
    python sage_group_finder_fixed.py --input_file output/model.hdf5 --snapshot 63 --group_id_field SAGEHaloIndex
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import argparse
import h5py
import sys
from pathlib import Path

# ============================================================================
# CORE GROUP FINDING FUNCTIONS
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
                       stellar_mass_min_msun=1e8, stellar_mass_max_msun=1e12, max_group_size=1000):
    """
    Generate group catalog from SAGE 2.0 data.
    
    Parameters:
    data: DataFrame with SAGE galaxy data
    cosmo_params: dict with cosmological parameters
    group_id_col: Column name for group identification  
    stellar_mass_min_msun: Minimum stellar mass in M_sun units
    stellar_mass_max_msun: Maximum stellar mass in M_sun units
    max_group_size: Maximum allowed group size (to filter out unrealistic large groups)
    """
    if cosmo_params is None:
        cosmo_params = {'H0': 70.0, 'Omega_m': 0.27, 'Omega_Lambda': 0.73, 'h': 0.7}
    
    h = cosmo_params['h']
    
    # Convert stellar mass limits from M_sun to SAGE units (log10(M*/1e10 M_sun))
    stellar_mass_min = np.log10(stellar_mass_min_msun) - 10
    stellar_mass_max = np.log10(stellar_mass_max_msun) - 10
    
    print(f"Applying stellar mass cuts: {stellar_mass_min_msun:.1e} - {stellar_mass_max_msun:.1e} M_sun")
    print(f"  (SAGE units: [{stellar_mass_min:.1f}, {stellar_mass_max:.1f}])")
    
    original_count = len(data)
    data_filtered = data[
        (data['StellarMass'] >= stellar_mass_min) & 
        (data['StellarMass'] <= stellar_mass_max)
    ].copy()
    print(f"Kept {len(data_filtered)}/{original_count} galaxies after stellar mass cuts")
    
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
    
    print(f"Processing {data_filtered[group_id_col].nunique()} groups...")
    
    for group_id, group in data_filtered.groupby(group_id_col):
        if len(group) < 2:  # Skip single-galaxy "groups"
            continue
            
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
        
        # Calculate velocity dispersion
        if 'VelDisp' in group.columns and group['VelDisp'].notna().any():
            # Use SAGE's built-in velocity dispersion if available
            vdisp_raw = group['VelDisp'].dropna()
            if len(vdisp_raw) > 0:
                vdisp = np.std(vdisp_raw)
                vdisp_gap = disp_gap(vdisp_raw)
            else:
                vdisp = np.nan
                vdisp_gap = np.nan
        else:
            # Calculate from 3D velocities
            vel_3d = calculate_velocity_from_3d(group[['Velx', 'Vely', 'Velz']].values)
            vdisp = np.std(vel_3d)
            vdisp_gap = disp_gap(vel_3d)
        
        # Calculate 3D separations from group center
        positions = group[['Posx', 'Posy', 'Posz']].values
        center_pos = np.tile(pos_median, (len(group), 1))
        group_seps = calculate_3d_distance(positions, center_pos)
        
        radius = np.max(group_seps)
        
        # Calculate stellar masses at different thresholds
        with np.errstate(divide='ignore', invalid='ignore', over='ignore'):
            # Handle stellar mass units - SAGE outputs in log10(M*/1e10 M_sun) units
            stellar_mass_values = group['StellarMass'].values
            
            # Filter out invalid stellar masses and apply safety limits
            valid_mass_mask = ((stellar_mass_values > stellar_mass_min) & 
                              (stellar_mass_values < stellar_mass_max) & 
                              np.isfinite(stellar_mass_values))
            
            if valid_mass_mask.any():
                valid_stellar_masses = stellar_mass_values[valid_mass_mask]
                
                # Be careful with unit conversion to avoid overflow
                # SAGE: log10(M*/1e10 M_sun) -> Standard: log10(M*/M_sun) 
                # So we add 10 to convert: log10(M*/1e10 M_sun) -> log10(M*/M_sun)
                stellar_masses_standard = valid_stellar_masses + 10  # Now in log10(M_sun)
                
                # Calculate total stellar mass using log-space arithmetic to avoid overflow
                if len(stellar_masses_standard) == 1:
                    Sig_M = stellar_masses_standard[0]
                else:
                    # Use logsumexp equivalent: log(sum(10^x)) = max(x) + log(sum(10^(x-max(x))))
                    max_mass = np.max(stellar_masses_standard)
                    if max_mass > 20:  # Safety check - reject unrealistic masses
                        Sig_M = np.nan
                    else:
                        sum_term = np.sum(10**(stellar_masses_standard - max_mass))
                        Sig_M = max_mass + np.log10(sum_term)
            else:
                Sig_M = np.nan
            
            # Stellar mass above different thresholds (in standard log10(M_sun) units)
            thresholds = [11, 10.5, 10, 9.5, 9]  # log10(M_sun)
            sig_masses = {}
            for threshold in thresholds:
                # Convert threshold to SAGE units for comparison
                sage_threshold = threshold - 10
                mask = (stellar_mass_values > sage_threshold) & valid_mass_mask
                if mask.any():
                    masses_above_thresh = stellar_mass_values[mask] + 10  # Convert to log10(M_sun)
                    
                    if len(masses_above_thresh) == 1:
                        sig_mass = masses_above_thresh[0]
                    else:
                        max_mass = np.max(masses_above_thresh)
                        if max_mass > 20:  # Safety check
                            sig_mass = np.nan
                        else:
                            sum_term = np.sum(10**(masses_above_thresh - max_mass))
                            sig_mass = max_mass + np.log10(sum_term)
                    
                    sig_masses[f'Sig_M{threshold}'.replace('.', '')] = sig_mass if np.isfinite(sig_mass) else np.nan
                else:
                    sig_masses[f'Sig_M{threshold}'.replace('.', '')] = np.nan
        
        # Central galaxy stellar mass (convert to standard log10(M_sun) units)
        if (stellar_mass_min <= central_galaxy['StellarMass'] <= stellar_mass_max and 
            np.isfinite(central_galaxy['StellarMass'])):
            M_Cen = central_galaxy['StellarMass'] + 10  # Convert to log10(M_sun)
        else:
            M_Cen = np.nan
        
        # Top 3 and 5 most massive galaxies (only use valid masses)
        valid_group = group[valid_mass_mask] if valid_mass_mask.any() else pd.DataFrame()
        
        if len(valid_group) >= 3:
            top_3 = valid_group.nlargest(3, 'StellarMass')
            top3_masses = top_3['StellarMass'].values + 10  # Convert to log10(M_sun)
            max_mass = np.max(top3_masses)
            if max_mass <= 20:  # Safety check
                sum_term = np.sum(10**(top3_masses - max_mass))
                M_Cen3 = max_mass + np.log10(sum_term)
            else:
                M_Cen3 = np.nan
        else:
            M_Cen3 = np.nan
            
        if len(valid_group) >= 5:
            top_5 = valid_group.nlargest(5, 'StellarMass')
            top5_masses = top_5['StellarMass'].values + 10  # Convert to log10(M_sun)
            max_mass = np.max(top5_masses)
            if max_mass <= 20:  # Safety check
                sum_term = np.sum(10**(top5_masses - max_mass))
                M_Cen5 = max_mass + np.log10(sum_term)
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
    
    group_catalog = pd.DataFrame(group_catalog)
    
    # Merge group catalog columns back into the original filtered data
    if len(group_catalog) > 0:
        merge_cols = ['group_id', 'vdisp_gap', 'radius', 'Mhalo_P3M', 'Mhalo_VT', 'Mhalo_CVT']
        # Only merge columns that exist in the group catalog
        merge_cols = [col for col in merge_cols if col in group_catalog.columns]
        data_merged = data_filtered.merge(group_catalog[merge_cols], left_on=group_id_col, right_on='group_id', how='left')
    else:
        print("No groups found to merge")
        data_merged = data_filtered.copy()
    
    return data_merged, group_catalog

# ============================================================================
# DATA LOADING FUNCTIONS - FIXED VERSION
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
    maps the correct group ID fields.
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
                
                # Load galaxy data from snapshot
                galaxy_data = {}
                
                # Direct field mapping - load fields as they exist in SAGE output
                expected_fields = {
                    'Posx': 'Posx',
                    'Posy': 'Posy', 
                    'Posz': 'Posz',
                    'Velx': 'Velx',
                    'Vely': 'Vely',
                    'Velz': 'Velz',
                    'StellarMass': 'StellarMass',
                    'Mvir': 'Mvir',
                    'Rvir': 'Rvir',
                    'Type': 'Type',
                    'SnapNum': 'SnapNum',
                    'VelDisp': 'VelDisp',
                    'SAGEHaloIndex': 'SAGEHaloIndex',  # This is the key group ID field
                    'GalaxyIndex': 'GalaxyIndex',
                    'CentralGalaxyIndex': 'CentralGalaxyIndex'
                }
                
                n_galaxies = None
                
                # Load fields directly 
                for our_name, sage_name in expected_fields.items():
                    if sage_name in snap_data:
                        galaxy_data[our_name] = snap_data[sage_name][:]
                        if n_galaxies is None:
                            n_galaxies = len(galaxy_data[our_name])
                        print(f"  Loaded {our_name} from {sage_name}: {galaxy_data[our_name].shape}")
                        
                        # Show some stats for key fields
                        if our_name in ['Posx', 'StellarMass', 'SAGEHaloIndex']:
                            data_vals = galaxy_data[our_name]
                            valid_vals = data_vals[data_vals != 0] if our_name != 'SAGEHaloIndex' else data_vals
                            if len(valid_vals) > 0:
                                print(f"    Range: [{np.min(valid_vals):.3f}, {np.max(valid_vals):.3f}]")
                    else:
                        print(f"  Warning: {sage_name} not found in snapshot data")
                        if n_galaxies is not None:
                            # Create dummy data for missing fields
                            if our_name == 'VelDisp':
                                galaxy_data[our_name] = np.zeros(n_galaxies)
                            elif our_name in ['GalaxyIndex', 'CentralGalaxyIndex']:
                                galaxy_data[our_name] = np.arange(n_galaxies)
                
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
            valid_mass = df['StellarMass'][(df['StellarMass'] > -10) & (df['StellarMass'] < 20)]
            if len(valid_mass) > 0:
                print(f"  Stellar mass range: [{valid_mass.min():.1f}, {valid_mass.max():.1f}] log10(M_sun/1e10)")
                
        if 'Type' in df.columns:
            type_counts = df['Type'].value_counts()
            print(f"  Galaxy types: {dict(type_counts)}")
            
        if 'SAGEHaloIndex' in df.columns:
            n_groups = df['SAGEHaloIndex'].nunique()
            print(f"  Number of unique groups: {n_groups}")
    
    return df

def create_sample_data(n_galaxies=10000, n_groups=1000):
    """
    Create sample SAGE-like data for demonstration purposes.
    """
    print(f"Creating sample data with {n_galaxies} galaxies in {n_groups} groups")
    
    np.random.seed(42)  # For reproducibility
    
    # Generate group IDs
    group_ids = np.random.randint(0, n_groups, n_galaxies)
    
    # Generate positions (comoving coordinates in Mpc/h)
    positions = np.random.uniform(-100, 100, (n_galaxies, 3))
    
    # Generate velocities (km/s)
    velocities = np.random.normal(0, 200, (n_galaxies, 3))
    
    # Generate stellar masses (log10 M_sun/1e10) - SAGE units
    stellar_masses = np.random.normal(0, 1.5, n_galaxies)  # SAGE units
    
    # Generate halo masses (log10 M_sun)  
    halo_masses = stellar_masses + 10 + np.random.normal(2, 0.3, n_galaxies)
    
    # Generate types (0=central, 1=satellite)
    types = np.random.choice([0, 1], n_galaxies, p=[0.3, 0.7])
    
    # Generate snapshot numbers
    snapnums = np.full(n_galaxies, 63)  # z=0 snapshot
    
    data = pd.DataFrame({
        'SAGEHaloIndex': group_ids,  # Use correct field name
        'Type': types,
        'Posx': positions[:, 0],
        'Posy': positions[:, 1],
        'Posz': positions[:, 2],
        'Velx': velocities[:, 0],
        'Vely': velocities[:, 1],
        'Velz': velocities[:, 2],
        'StellarMass': stellar_masses,
        'Mvir': halo_masses,
        'Rvir': np.random.lognormal(0, 0.3, n_galaxies),
        'SnapNum': snapnums,
        'GalaxyIndex': np.arange(n_galaxies),
        'CentralGalaxyIndex': group_ids,
        'VelDisp': np.random.lognormal(5, 0.5, n_galaxies)  # km/s
    })
    
    return data

# ============================================================================
# ANALYSIS AND PLOTTING FUNCTIONS
# ============================================================================

def plot_spatial_distribution(data_merged, group_catalog, output_dir='./', dilute_factor=0.3):
    """
    Plot the spatial distribution of galaxies with groups highlighted.
    
    Parameters:
    data_merged: DataFrame with galaxy data including group information
    group_catalog: DataFrame with group catalog
    output_dir: Output directory for plots
    dilute_factor: Fraction of background galaxies to plot (for performance)
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    print('Plotting the spatial distribution of galaxies with groups highlighted')
    
    # Get box size from data range
    box_size = max(data_merged['Posx'].max(), data_merged['Posy'].max(), data_merged['Posz'].max())

    x_min, x_max = data_merged['Posx'].min(), data_merged['Posx'].max()
    y_min, y_max = data_merged['Posy'].min(), data_merged['Posy'].max()
    z_min, z_max = data_merged['Posz'].min(), data_merged['Posz'].max()
    
    # Filter for valid galaxies
    valid_gals = data_merged[
        (data_merged['StellarMass'] > -2.0) &  # SAGE units
        (data_merged['Posx'] > 0) & 
        (data_merged['Posy'] > 0) & 
        (data_merged['Posz'] > 0)
    ].copy()
    
    # Separate galaxies in groups vs background
    galaxies_in_groups = valid_gals[valid_gals['group_id'].notna()].copy()
    background_galaxies = valid_gals[valid_gals['group_id'].isna()].copy()
    
    # Dilute background for performance if needed
    if len(background_galaxies) > int(len(valid_gals) * dilute_factor):
        background_sample = background_galaxies.sample(n=int(len(valid_gals) * dilute_factor), random_state=42)
    else:
        background_sample = background_galaxies
    
    print(f"Plotting {len(background_sample)} background galaxies and {len(galaxies_in_groups)} group members")
    
    # Create the plot
    fig = plt.figure(figsize=(15, 12))
    
    # Get groups with at least 3 members for highlighting
    valid_groups = group_catalog[group_catalog['Nm'] >= 3].copy()
    
    # Create a mapping of group_id to group size and assign colors based on group size
    if len(valid_groups) > 0:
        # Sort groups by size for consistent coloring
        valid_groups = valid_groups.sort_values('Nm')
        
        # Use plasma colormap for groups based on group size
        group_sizes = valid_groups['Nm'].values
        size_norm = plt.Normalize(vmin=group_sizes.min(), vmax=group_sizes.max())
        plasma_colors = plt.cm.plasma(size_norm(group_sizes))
        
        # Create mapping from group_id to color and size
        group_id_to_color = dict(zip(valid_groups['group_id'], plasma_colors))
        group_id_to_size = dict(zip(valid_groups['group_id'], valid_groups['Nm']))
        
        # Calculate marker sizes proportional to group size (with reasonable bounds)
        min_marker_size = 3
        max_marker_size = 20
        size_range = group_sizes.max() - group_sizes.min()
        if size_range > 0:
            marker_sizes = min_marker_size + (max_marker_size - min_marker_size) * (group_sizes - group_sizes.min()) / size_range
        else:
            marker_sizes = np.full(len(group_sizes), min_marker_size)
        group_id_to_marker_size = dict(zip(valid_groups['group_id'], marker_sizes))
    else:
        group_id_to_color = {}
        group_id_to_size = {}
        group_id_to_marker_size = {}
    
    # Plot 1: X-Y projection
    ax1 = plt.subplot(221)
    # Background galaxies (small, transparent)
    plt.scatter(background_sample['Posx'], background_sample['Posy'], 
               marker='o', s=0.5, c='black', alpha=0.4, label='Background')

    # Group galaxies (size proportional to group size, colored by group size using plasma)
    for group_id, color in group_id_to_color.items():
        group_gals = galaxies_in_groups[galaxies_in_groups['group_id'] == group_id]
        if len(group_gals) > 0:
            marker_size = group_id_to_marker_size[group_id]
            group_size = group_id_to_size[group_id]
            plt.scatter(group_gals['Posx'], group_gals['Posy'], 
                       marker='o', s=marker_size, c=[color], alpha=0.8, 
                       label=f'N={group_size}' if len(group_id_to_color) <= 10 else None)
    
    plt.xlim(x_min, x_max)
    plt.ylim(y_min, y_max)
    plt.xlabel('X [Mpc/h]')
    plt.ylabel('Y [Mpc/h]')
    plt.title('X-Y Projection')
    if len(group_id_to_color) <= 10:
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8, title='Group Size')
    
    # Plot 2: X-Z projection  
    ax2 = plt.subplot(222)
    plt.scatter(background_sample['Posx'], background_sample['Posz'], 
               marker='o', s=0.5, c='black', alpha=0.4)
    
    for group_id, color in group_id_to_color.items():
        group_gals = galaxies_in_groups[galaxies_in_groups['group_id'] == group_id]
        if len(group_gals) > 0:
            marker_size = group_id_to_marker_size[group_id]
            plt.scatter(group_gals['Posx'], group_gals['Posz'], 
                       marker='o', s=marker_size, c=[color], alpha=0.8)
    
    plt.xlim(x_min, x_max)
    plt.ylim(z_min, z_max)
    plt.xlabel('X [Mpc/h]')
    plt.ylabel('Z [Mpc/h]')
    plt.title('X-Z Projection')
    
    # Plot 3: Y-Z projection
    ax3 = plt.subplot(223)
    plt.scatter(background_sample['Posy'], background_sample['Posz'], 
               marker='o', s=0.5, c='black', alpha=0.4)
    
    for group_id, color in group_id_to_color.items():
        group_gals = galaxies_in_groups[galaxies_in_groups['group_id'] == group_id]
        if len(group_gals) > 0:
            marker_size = group_id_to_marker_size[group_id]
            plt.scatter(group_gals['Posy'], group_gals['Posz'], 
                       marker='o', s=marker_size, c=[color], alpha=0.8)
    
    plt.xlim(y_min, y_max)
    plt.ylim(z_min, z_max)
    plt.xlabel('Y [Mpc/h]')
    plt.ylabel('Z [Mpc/h]')
    plt.title('Y-Z Projection')
    
    # Plot 4: Group size vs stellar mass (colored by velocity dispersion using plasma)
    ax4 = plt.subplot(224)
    if len(valid_groups) > 0 and 'Sig_M' in valid_groups.columns:
        valid_mass_groups = valid_groups[valid_groups['Sig_M'].notna()]
        if len(valid_mass_groups) > 0:
            if 'vdisp_gap' in valid_mass_groups.columns and valid_mass_groups['vdisp_gap'].notna().any():
                scatter = plt.scatter(valid_mass_groups['Nm'], valid_mass_groups['Sig_M'], 
                                    c=valid_mass_groups['vdisp_gap'], 
                                    cmap='plasma', alpha=0.8, s=40, edgecolors='black', linewidth=0.5)
                cbar = plt.colorbar(scatter)
                cbar.set_label('Velocity Dispersion [km/s]')
            else:
                # Color by group size if no velocity dispersion
                scatter = plt.scatter(valid_mass_groups['Nm'], valid_mass_groups['Sig_M'], 
                                    c=valid_mass_groups['Nm'], 
                                    cmap='plasma', alpha=0.8, s=40, edgecolors='black', linewidth=0.5)
                cbar = plt.colorbar(scatter)
                cbar.set_label('Group Size (N members)')
            
            plt.xlabel('Group Size (N members)')
            plt.ylabel('Log10(Total Stellar Mass [M_sun])')
            plt.title('Group Properties')
        else:
            plt.text(0.5, 0.5, 'No valid group mass data', 
                    transform=ax4.transAxes, ha='center', va='center')
    else:
        plt.text(0.5, 0.5, 'No group data available', 
                transform=ax4.transAxes, ha='center', va='center')
    
    # Add colorbar for group sizes in spatial plots if we have groups
    if len(valid_groups) > 0:
        cbar_ax = fig.add_axes([0.15, 0.48, 0.7, 0.02])  # [left, bottom, width, height]
        # Create a dummy scatter plot for the colorbar
        dummy_scatter = ax1.scatter([], [], c=[], cmap='plasma', 
                                   vmin=group_sizes.min(), vmax=group_sizes.max())
        cbar_spatial = fig.colorbar(dummy_scatter, ax=[ax1, ax2, ax3], cax=cbar_ax,
                                   location='top', shrink=0.8, pad=0.1)
        cbar_spatial.set_label('Group Size (N members)', fontsize=10)
    
    plt.tight_layout()
    # Adjust layout to make room for the central colorbar
    plt.subplots_adjust(top=0.95, bottom=0.05, hspace=0.35)
    
    # Save the plot
    output_file = output_dir / 'spatial_distribution_with_groups.png'
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f'Saved spatial distribution plot to {output_file}')
    plt.close()
    
    # Print summary
    print(f"\nSpatial Distribution Summary:")
    print(f"Total galaxies plotted: {len(valid_gals):,}")
    print(f"Background galaxies: {len(background_sample):,}")
    print(f"Galaxies in groups (≥3 members): {len(galaxies_in_groups):,}")
    print(f"Groups highlighted: {len(valid_groups)}")
    print(f"Box size: {box_size:.1f} Mpc/h")
    if len(valid_groups) > 0:
        print(f"Group sizes: {group_sizes.min()}-{group_sizes.max()} members")
        print(f"Marker sizes: {min_marker_size}-{max_marker_size} points")

def plot_group_properties(group_catalog, output_dir='./'):
    """
    Create plots of group properties.
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Filter out groups with too few members or invalid data
    valid_groups = group_catalog[group_catalog['Nm'] >= 3].copy()
    
    # Further filter for valid velocity dispersion data
    if 'vdisp_gap' in valid_groups.columns:
        valid_groups = valid_groups[(valid_groups['vdisp_gap'].notna()) & 
                                   (valid_groups['vdisp_gap'] > 0)]
    
    print(f"Plotting properties for {len(valid_groups)} valid groups")
    
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
    
    plt.tight_layout()
    plt.savefig(output_dir / 'group_properties.png', dpi=150, bbox_inches='tight')
    print(f"Saved plot to {output_dir / 'group_properties.png'}")
    
    # Print some statistics
    print(f"\nGroup Statistics:")
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
    Save results to CSV files.
    """
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True)
    
    # Save group catalog
    group_catalog.to_csv(output_dir / 'sage_group_catalog_fixed.csv', index=False)
    print(f"Saved group catalog to {output_dir / 'sage_group_catalog_fixed.csv'}")

# ============================================================================
# MAIN FUNCTION
# ============================================================================

def main():
    """
    Main function to run the SAGE 2.0 group finding analysis.
    """
    parser = argparse.ArgumentParser(description='SAGE 2.0 Group Finding Analysis - FIXED VERSION')
    parser.add_argument('--input_file', type=str, default=None,
                       help='Path to SAGE HDF5 output file')
    parser.add_argument('--snapshot', type=int, default=63,
                       help='Specific snapshot to analyze')
    parser.add_argument('--output_dir', type=str, default='./',
                       help='Output directory for results')
    parser.add_argument('--min_group_size', type=int, default=3,
                       help='Minimum number of galaxies per group')
    parser.add_argument('--explore', action='store_true',
                       help='Just explore the HDF5 file structure and exit')
    parser.add_argument('--sample_data', action='store_true',
                       help='Use sample data instead of loading file')
    parser.add_argument('--group_id_field', type=str, default='CentralGalaxyIndex',
                       help='Field to use for group identification')
    parser.add_argument('--stellar_mass_min', type=float, default=1e8,
                       help='Minimum stellar mass in M_sun units (e.g., 1e8)')
    parser.add_argument('--stellar_mass_max', type=float, default=1e12,
                       help='Maximum stellar mass in M_sun units (e.g., 1e12)')
    parser.add_argument('--max_group_size', type=int, default=1000,
                       help='Maximum allowed group size (filters out unrealistic large groups)')
    parser.add_argument('--analyze_groups', action='store_true',
                       help='Analyze different group ID fields and exit')
    parser.add_argument('--dilute_factor', type=float, default=0.3,
                       help='Fraction of background galaxies to plot in spatial distribution (0.1-1.0)')
    
    args = parser.parse_args()
    
    print("="*60)
    print("SAGE 2.0 Group Finding Analysis - FIXED VERSION")
    print("="*60)
    
    # If just exploring, do that and exit
    if args.explore and args.input_file and Path(args.input_file).exists():
        explore_hdf5_structure(args.input_file, args.snapshot)
        return
    
    # Load data
    if args.sample_data or args.input_file is None:
        print("Using sample data for demonstration...")
        sage_data = create_sample_data()
    else:
        if not Path(args.input_file).exists():
            print(f"Input file {args.input_file} not found. Using sample data instead.")
            sage_data = create_sample_data()
        else:
            sage_data = load_sage_hdf5(args.input_file, args.snapshot)
    
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
    
    # Set up cosmological parameters
    cosmo_params = {
        'H0': 70.0,          # km/s/Mpc
        'Omega_m': 0.27,     # Matter density
        'Omega_Lambda': 0.73, # Dark energy density
        'h': 0.7             # Little h
    }
    
    # Run group finding analysis
    print("\n" + "="*50)
    print("Running Group Finding Analysis")
    print("="*50)
    
    data_merged, group_catalog = SAGE_Group_Catalog(
        sage_data, 
        cosmo_params=cosmo_params,
        group_id_col=args.group_id_field,
        stellar_mass_min_msun=args.stellar_mass_min,
        stellar_mass_max_msun=args.stellar_mass_max,
        max_group_size=args.max_group_size
    )
    
    print(f"\nGroup analysis complete!")
    print(f"Found {len(group_catalog)} groups")
    print(f"Groups with ≥{args.min_group_size} members: {len(group_catalog[group_catalog['Nm'] >= args.min_group_size])}")
    
    # Filter groups by minimum size
    valid_groups = group_catalog[group_catalog['Nm'] >= args.min_group_size]
    
    if len(valid_groups) > 0:
        # Create plots
        print(f"\nCreating plots...")
        plot_group_properties(valid_groups, args.output_dir)
        
        # Create spatial distribution plot
        plot_spatial_distribution(data_merged, group_catalog, args.output_dir, args.dilute_factor)
        
        # Save results
        print(f"\nSaving results...")
        save_results(data_merged, group_catalog, args.output_dir)
        
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
        print(f"Results saved to: {Path(args.output_dir).absolute()}")
    else:
        print(f"\n{'='*50}")
        print("ISSUES FOUND")
        print("="*50)
        if len(group_catalog) == 0:
            print("No groups were detected. This might be because:")
            print("- The group ID field doesn't contain valid group information")
            print("- Position/velocity data is not loading correctly")
        else:
            print(f"Found {len(group_catalog)} groups, but none have ≥{args.min_group_size} members")
            print(f"Group sizes: min={group_catalog['Nm'].min()}, max={group_catalog['Nm'].max()}, mean={group_catalog['Nm'].mean():.1f}")
    
    print("Run with --explore to examine your HDF5 file structure")
    print("Run with --analyze_groups to see group ID field options")

if __name__ == "__main__":
    main()