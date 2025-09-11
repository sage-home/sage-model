#!/usr/bin/env python3
"""
Complete SAGE Group Finder Script
Uses actual SAGE output fields and sums properties for groups
Includes minimum group size filtering
"""

import numpy as np
import pandas as pd
import h5py as h5
import os
from astropy.coordinates import SkyCoord
import astropy.units as u
import warnings
warnings.filterwarnings('ignore')

# Configuration (matching the SAGE setup)
DirName = './output/millennium/'
Snapshot = 'Snap_63'
Hubble_h = 0.73
BoxSize = 62.5  # h-1 Mpc
OutputFile = 'sage_group_catalog.csv'

# Group selection criteria
MIN_GROUP_SIZE = 3  # Minimum number of galaxies to be considered a group
                    # Common values: 1 (all), 2 (pairs+), 3 (small groups+), 5 (real groups+)
REDSHIFT = 0.0      # Redshift for snapshot 63 (z=0 as mentioned)

def read_hdf(snap_num=None, param=None):
    """Read data from all SAGE model files and combine"""
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    combined_data = None
    
    for model_file in model_files:
        try:
            property_file = h5.File(os.path.join(DirName, model_file), 'r')
            data = np.array(property_file[snap_num][param])
            
            if combined_data is None:
                combined_data = data
            else:
                combined_data = np.concatenate((combined_data, data))
                
            property_file.close()
        except KeyError:
            print(f"Warning: Parameter '{param}' not found in {model_file}")
            property_file.close()
            continue
            
    return combined_data

def disp_gap(data):
    """Calculate velocity dispersion using the gapper method"""
    N = len(data)
    if N < 2:
        return np.nan
    data_sort = np.sort(data)
    gaps = data_sort[1:] - data_sort[:-1]
    weights = np.arange(1, N) * np.arange(N - 1, 0, -1)
    sigma_gap = np.sqrt(np.pi) / (N * (N - 1)) * np.sum(weights * gaps)
    sigma = np.sqrt((N / (N - 1)) * sigma_gap**2)
    return sigma

def load_sage_data():
    """Load SAGE galaxy data using actual output fields"""
    
    print('Loading SAGE galaxy properties...')
    
    # Read essential galaxy properties from SAGE
    Type = read_hdf(snap_num=Snapshot, param='Type')
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    Vvir = read_hdf(snap_num=Snapshot, param='Vvir')
    Rvir = read_hdf(snap_num=Snapshot, param='Rvir')
    
    # Position and velocity
    Posx = read_hdf(snap_num=Snapshot, param='Posx')
    Posy = read_hdf(snap_num=Snapshot, param='Posy')
    Posz = read_hdf(snap_num=Snapshot, param='Posz')
    
    # Try to read velocity components
    try:
        Velx = read_hdf(snap_num=Snapshot, param='Velx')
        Vely = read_hdf(snap_num=Snapshot, param='Vely')
        Velz = read_hdf(snap_num=Snapshot, param='Velz')
    except:
        # If individual components don't exist, try velocity array
        try:
            Vel = read_hdf(snap_num=Snapshot, param='Vel')
            if len(Vel.shape) > 1:
                Velx, Vely, Velz = Vel[:, 0], Vel[:, 1], Vel[:, 2]
            else:
                # Use Vvir as proxy
                print("Using Vvir as velocity proxy")
                Velx = Vvir
                Vely = np.zeros_like(Vvir)
                Velz = np.zeros_like(Vvir)
        except:
            print("Using Vvir as velocity proxy")
            Velx = Vvir
            Vely = np.zeros_like(Vvir)
            Velz = np.zeros_like(Vvir)
    
    # Central galaxy information for grouping
    CentralGalaxyIndex = read_hdf(snap_num=Snapshot, param='CentralGalaxyIndex')
    GalaxyIndex = read_hdf(snap_num=Snapshot, param='GalaxyIndex')
    
    if CentralGalaxyIndex is None:
        print("Error: CentralGalaxyIndex not found in SAGE output")
        return None
    if GalaxyIndex is None:
        print("Error: GalaxyIndex not found in SAGE output")
        return None
    
    # Optional properties that might exist
    try:
        BulgeMass = read_hdf(snap_num=Snapshot, param='BulgeMass') * 1.0e10 / Hubble_h
    except:
        BulgeMass = np.zeros_like(StellarMass)
        
    try:
        BlackHoleMass = read_hdf(snap_num=Snapshot, param='BlackHoleMass') * 1.0e10 / Hubble_h
    except:
        BlackHoleMass = np.zeros_like(StellarMass)
        
    try:
        CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    except:
        CGMgas = np.zeros_like(StellarMass)
    
    # Calculate total velocity magnitude
    Vel_total = np.sqrt(Velx**2 + Vely**2 + Velz**2)
    
    # Convert positions to RA/Dec (simplified)
    ra = (Posx / BoxSize) * 360.0
    dec = (Posy / BoxSize - 0.5) * 180.0
    
    # Filter out galaxies with very low stellar mass and invalid types
    mass_cut = 1e8  # Minimum stellar mass in solar masses
    valid = (StellarMass > mass_cut) & (Type >= 0) & np.isfinite(StellarMass)
    
    print(f'Total galaxies: {len(StellarMass)}')
    print(f'Galaxies above mass cut ({mass_cut:.0e} Msun): {np.sum(valid)}')
    
    # Apply filters and create data dictionary
    data_dict = {
        'Galaxy_ID': GalaxyIndex[valid],
        'Central_Galaxy_ID': CentralGalaxyIndex[valid],
        'RA': ra[valid],
        'Dec': dec[valid],
        'Redshift': np.full(np.sum(valid), REDSHIFT),
        'StellarMass': StellarMass[valid],
        'ColdGas': ColdGas[valid],
        'HotGas': HotGas[valid],
        'BulgeMass': BulgeMass[valid],
        'BlackHoleMass': BlackHoleMass[valid],
        'CGMgas': CGMgas[valid],
        'Mvir': Mvir[valid],
        'Vvir': Vvir[valid],
        'Rvir': Rvir[valid],
        'Velx': Velx[valid],
        'Vely': Vely[valid],
        'Velz': Velz[valid],
        'Vel_total': Vel_total[valid],
        'Type': Type[valid],
        'Posx': Posx[valid],
        'Posy': Posy[valid],
        'Posz': Posz[valid]
    }
    
    # Create group IDs based on central galaxy
    central_ids = data_dict['Central_Galaxy_ID']
    unique_centrals = np.unique(central_ids)
    group_id_map = {central: i for i, central in enumerate(unique_centrals)}
    data_dict['group_id'] = np.array([group_id_map[cid] for cid in central_ids])
    
    print(f'Number of groups: {len(unique_centrals)}')
    
    return pd.DataFrame(data_dict)

def simple_group_catalog(data):
    """Create a simple group catalog by summing SAGE properties"""
    group_catalog = []
    
    print(f'Building simple group catalog (minimum {MIN_GROUP_SIZE} galaxies per group)...')
    
    total_groups = 0
    accepted_groups = 0
    
    for group_id, group in data.groupby('group_id'):
        total_groups += 1
        
        # Apply minimum group size filter
        Nm = len(group)
        if Nm < MIN_GROUP_SIZE:
            continue  # Skip groups smaller than minimum size
            
        accepted_groups += 1
        ra_median = group['RA'].median()
        dec_median = group['Dec'].median()
        z_median = group['Redshift'].median()
        
        # Find central galaxy (Type=0 usually indicates central)
        centrals = group[group['Type'] == 0]
        if len(centrals) > 0:
            central_galaxy = centrals.iloc[0]
            CID = central_galaxy['Galaxy_ID']
        else:
            # Use most massive galaxy as central
            central_galaxy = group.loc[group['StellarMass'].idxmax()]
            CID = central_galaxy['Galaxy_ID']
        
        # Sum up galaxy properties for the group
        total_stellar_mass = group['StellarMass'].sum()
        total_cold_gas = group['ColdGas'].sum()
        total_hot_gas = group['HotGas'].sum()
        total_bulge_mass = group['BulgeMass'].sum()
        total_bh_mass = group['BlackHoleMass'].sum()
        total_cgm_gas = group['CGMgas'].sum()
        
        # Central galaxy properties
        central_mvir = central_galaxy['Mvir']
        central_vvir = central_galaxy['Vvir']
        central_rvir = central_galaxy['Rvir']
        
        # Velocity statistics
        if Nm > 1:
            vdisp_x = group['Velx'].std()
            vdisp_y = group['Vely'].std()
            vdisp_z = group['Velz'].std()
            vdisp_total = group['Vel_total'].std()
            vdisp_gap_x = disp_gap(group['Velx'])
            vdisp_gap_y = disp_gap(group['Vely'])
            vdisp_gap_z = disp_gap(group['Velz'])
        else:
            vdisp_x = vdisp_y = vdisp_z = vdisp_total = 0.0
            vdisp_gap_x = vdisp_gap_y = vdisp_gap_z = 0.0
        
        # Calculate group radius (maximum distance from group center)
        try:
            c1 = SkyCoord(ra=group['RA'].values * u.degree, 
                         dec=group['Dec'].values * u.degree, frame='icrs')
            c2 = SkyCoord(ra=ra_median * u.degree, dec=dec_median * u.degree, frame='icrs')
            separations = c1.separation(c2).to(u.arcmin).value
            radius_arcmin = np.max(separations) if len(separations) > 0 else 0.0
        except:
            radius_arcmin = 0.0
            
        # Physical separation in Mpc (rough approximation at z=0)
        radius_mpc = radius_arcmin / 60.0 * np.pi / 180.0 * 1000.0  # Very rough conversion
        
        group_catalog.append({
            'group_id': group_id,
            'Nm': Nm,
            'RA': ra_median,
            'Dec': dec_median,
            'Redshift': z_median,
            'CID': int(CID),
            
            # Summed galaxy properties
            'Total_StellarMass': total_stellar_mass,
            'Total_ColdGas': total_cold_gas,
            'Total_HotGas': total_hot_gas,
            'Total_BulgeMass': total_bulge_mass,
            'Total_BlackHoleMass': total_bh_mass,
            'Total_CGMgas': total_cgm_gas,
            
            # Central galaxy properties
            'Central_Mvir': central_mvir,
            'Central_Vvir': central_vvir,
            'Central_Rvir': central_rvir,
            
            # Group kinematics
            'Velocity_Disp_x': vdisp_x,
            'Velocity_Disp_y': vdisp_y,
            'Velocity_Disp_z': vdisp_z,
            'Velocity_Disp_total': vdisp_total,
            'Velocity_Disp_Gap_x': vdisp_gap_x,
            'Velocity_Disp_Gap_y': vdisp_gap_y,
            'Velocity_Disp_Gap_z': vdisp_gap_z,
            
            # Group size
            'Radius_arcmin': radius_arcmin,
            'Radius_Mpc': radius_mpc
        })
    
    print(f'Processed {total_groups} total groups, accepted {accepted_groups} groups with >={MIN_GROUP_SIZE} galaxies')
    print(f'Filtered out {total_groups - accepted_groups} groups ({100*(total_groups - accepted_groups)/total_groups:.1f}%)')
    
    return pd.DataFrame(group_catalog)

def main():
    """Main function to run the simple group finder"""
    
    print('SAGE Simple Group Finder')
    print('========================')
    print(f'Configuration:')
    print(f'  Data directory: {DirName}')
    print(f'  Snapshot: {Snapshot}')
    print(f'  Minimum group size: {MIN_GROUP_SIZE} galaxies')
    print(f'  Output file: {OutputFile}')
    print()
    
    # Check if data directory exists
    if not os.path.exists(DirName):
        print(f'Error: Data directory {DirName} not found!')
        print('Please update the DirName variable to point to your SAGE output directory.')
        return
    
    # Load SAGE data
    try:
        galaxy_data = load_sage_data()
        if galaxy_data is None:
            print('Error: Failed to load galaxy data')
            return
        print(f'Loaded {len(galaxy_data)} galaxies')
    except Exception as e:
        print(f'Error loading SAGE data: {e}')
        return
    
    # Build simple group catalog
    try:
        group_catalog = simple_group_catalog(galaxy_data)
        print(f'Created catalog with {len(group_catalog)} groups')
    except Exception as e:
        print(f'Error building group catalog: {e}')
        return
    
    # Save to CSV
    try:
        group_catalog.to_csv(OutputFile, index=False)
        print(f'Group catalog saved to {OutputFile}')
        
        # Print some basic statistics
        print(f'\nGroup Catalog Statistics (MIN_GROUP_SIZE = {MIN_GROUP_SIZE}):')
        print(f'Total groups: {len(group_catalog)}')
        if len(group_catalog) > 0:
            print(f'Mean group size: {group_catalog["Nm"].mean():.2f}')
            print(f'Median group size: {group_catalog["Nm"].median():.2f}')
            print(f'Largest group: {group_catalog["Nm"].max()} galaxies')
            print(f'Groups with >5 members: {(group_catalog["Nm"] > 5).sum()}')
            print(f'Groups with >10 members: {(group_catalog["Nm"] > 10).sum()}')
            
            # Mass statistics
            print(f'\nMass Statistics (log10 solar masses):')
            print(f'Mean total stellar mass: {np.log10(group_catalog["Total_StellarMass"]).mean():.2f}')
            print(f'Mean central halo mass: {np.log10(group_catalog["Central_Mvir"]).mean():.2f}')
        else:
            print('No groups found meeting the minimum size criteria!')
        
    except Exception as e:
        print(f'Error saving catalog: {e}')
        return
    
    print('\nSimple group finder completed successfully!')

if __name__ == '__main__':
    main()