#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import pandas as pd
import os
from collections import defaultdict

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
# DirName = './output/miniUchuu/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'
# Snapshot = 'Snap_49'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5        # h-1 Mpc
# Hubble_h = 0.6774        # Hubble parameter
# BoxSize = 400.0         # h-1 Mpc
VolumeFraction = 1.0  # Fraction of the full volume output by the model

# CSV export options
OutputCSVDir = DirName + 'csv_exports/'
OutputCSVFile = f'galaxy_properties_{Snapshot}.csv'

StellarMassMin = 1.0e9  # M_sun (minimum stellar mass)


# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        property = h5.File(DirName + model_file, 'r')
        data = np.array(property[snap_num][param])
        
        if combined_data is None:
            combined_data = data
        else:
            combined_data = np.concatenate((combined_data, data))
            
    return combined_data


# ==================================================================

if __name__ == '__main__':

    print(f'Exporting {Snapshot} galaxy properties to CSV\n')

    # Create output directory if it doesn't exist
    if not os.path.exists(OutputCSVDir): 
        os.makedirs(OutputCSVDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    # Read all galaxy properties
    print('Reading galaxy properties...')
    
    # Basic halo properties
    CentralMvir = read_hdf(snap_num=Snapshot, param='CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    Rvir = read_hdf(snap_num=Snapshot, param='Rvir')
    Vvir = read_hdf(snap_num=Snapshot, param='Vvir')
    Vmax = read_hdf(snap_num=Snapshot, param='Vmax')
    NDM = read_hdf(snap_num=Snapshot, param='Len')
    
    # Stellar properties
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    # BulgeMass = read_hdf(snap_num=Snapshot, param='BulgeMass') * 1.0e10 / Hubble_h
    # DiskRadius = read_hdf(snap_num=Snapshot, param='DiskRadius') / Hubble_h
    
    # # Gas properties
    # ColdGas = read_hdf(snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
    # HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    # H1Gas = read_hdf(snap_num=Snapshot, param='HI_gas') * 1.0e10 / Hubble_h
    # H2Gas = read_hdf(snap_num=Snapshot, param='H2_gas') * 1.0e10 / Hubble_h
    # cgm = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    
    # # Metal properties
    # MetalsColdGas = read_hdf(snap_num=Snapshot, param='MetalsColdGas') * 1.0e10 / Hubble_h
    # MetalsCGMgas = read_hdf(snap_num=Snapshot, param='MetalsCGMgas') * 1.0e10 / Hubble_h
    # metalshotgas = read_hdf(snap_num=Snapshot, param='MetalsHotGas') * 1.0e10 / Hubble_h
    
    # Star formation properties
    SfrDisk = read_hdf(snap_num=Snapshot, param='SfrDisk')
    SfrBulge = read_hdf(snap_num=Snapshot, param='SfrBulge')
    
    # Black hole properties
    # BlackHoleMass = read_hdf(snap_num=Snapshot, param='BlackHoleMass') * 1.0e10 / Hubble_h
    
    # Other properties
    # IntraClusterStars = read_hdf(snap_num=Snapshot, param='IntraClusterStars') * 1.0e10 / Hubble_h
    InfallMvir = read_hdf(snap_num=Snapshot, param='infallMvir') * 1.0e10 / Hubble_h
    # outflowrate = read_hdf(snap_num=Snapshot, param='OutflowRate') * 1.0e10 / Hubble_h
    # massload = read_hdf(snap_num=Snapshot, param='MassLoading')
    # Cooling = read_hdf(snap_num=Snapshot, param='Cooling')
    
    # Galaxy classification
    CentralGalaxyIndex = read_hdf(snap_num=Snapshot, param='CentralGalaxyIndex')
    Type = read_hdf(snap_num=Snapshot, param='Type')
    GalaxyID = read_hdf(snap_num=Snapshot, param='GalaxyIndex')
    
    # Spatial coordinates
    Posx = read_hdf(snap_num=Snapshot, param='Posx')
    Posy = read_hdf(snap_num=Snapshot, param='Posy')
    Posz = read_hdf(snap_num=Snapshot, param='Posz')

    Velx = read_hdf(snap_num=Snapshot, param='Velx')
    Vely = read_hdf(snap_num=Snapshot, param='Vely')
    Velz = read_hdf(snap_num=Snapshot, param='Velz')

    print(f'Read properties for {len(StellarMass)} galaxies')

    # Apply stellar mass cut
    print('Applying stellar mass cut...')
    mass_mask = StellarMass >= StellarMassMin
    print(f'Stellar mass >= {StellarMassMin:.1e} M_sun: {mass_mask.sum():,} galaxies pass')
    print(f'Final sample after cuts: {mass_mask.sum():,} galaxies ({100*mass_mask.sum()/len(StellarMass):.1f}%)')
    
    # Apply mask to all arrays
    if not np.all(mass_mask):
        print('Applying mass cuts to all arrays...')
        CentralMvir = CentralMvir[mass_mask]
        Mvir = Mvir[mass_mask]
        Rvir = Rvir[mass_mask]
        Vvir = Vvir[mass_mask]
        Vmax = Vmax[mass_mask]
        NDM = NDM[mass_mask]
        
        StellarMass = StellarMass[mass_mask]
        # BulgeMass = BulgeMass[mass_mask]
        # DiskRadius = DiskRadius[mass_mask]
        
        # ColdGas = ColdGas[mass_mask]
        # HotGas = HotGas[mass_mask]
        # H1Gas = H1Gas[mass_mask]
        # H2Gas = H2Gas[mass_mask]
        # cgm = cgm[mass_mask]
        
        # MetalsColdGas = MetalsColdGas[mass_mask]
        # MetalsCGMgas = MetalsCGMgas[mass_mask]
        # metalshotgas = metalshotgas[mass_mask]
        
        SfrDisk = SfrDisk[mass_mask]
        SfrBulge = SfrBulge[mass_mask]
        
        # BlackHoleMass = BlackHoleMass[mass_mask]
        # IntraClusterStars = IntraClusterStars[mass_mask]
        InfallMvir = InfallMvir[mass_mask]
        # outflowrate = outflowrate[mass_mask]
        # massload = massload[mass_mask]
        # Cooling = Cooling[mass_mask]
        
        CentralGalaxyIndex = CentralGalaxyIndex[mass_mask]
        Type = Type[mass_mask]
        GalaxyID = GalaxyID[mass_mask]
        
        Posx = Posx[mass_mask]
        Posy = Posy[mass_mask]
        Posz = Posz[mass_mask]
        Velx = Velx[mass_mask]
        Vely = Vely[mass_mask]
        Velz = Velz[mass_mask]

    # Calculate derived quantities
    print('Calculating derived quantities...')
    
    # Total star formation rate
    SFR_total = SfrDisk + SfrBulge
    
    # Specific star formation rate (avoiding division by zero)
    sSFR = np.full_like(SFR_total, -99.0)  # Default value for galaxies with zero stellar mass
    stellar_mass_mask = StellarMass > 0
    sSFR[stellar_mass_mask] = np.log10(SFR_total[stellar_mass_mask] / StellarMass[stellar_mass_mask])
    
    # # Total baryonic mass (stars + cold gas with helium correction)
    # BaryonicMass = StellarMass + 1.4 * ColdGas
    
    # # Gas fractions
    # gas_fraction = np.zeros_like(ColdGas)
    # total_baryons_mask = (StellarMass + ColdGas) > 0
    # gas_fraction[total_baryons_mask] = ColdGas[total_baryons_mask] / (StellarMass[total_baryons_mask] + ColdGas[total_baryons_mask])
    
    # # H2 fraction
    # h2_fraction = np.zeros_like(H2Gas)
    # total_hi_h2_mask = (H1Gas + H2Gas) > 0
    # h2_fraction[total_hi_h2_mask] = H2Gas[total_hi_h2_mask] / (H1Gas[total_hi_h2_mask] + H2Gas[total_hi_h2_mask])
    
    # Bulge fraction
    # bulge_fraction = np.zeros_like(BulgeMass)
    stellar_mass_nonzero = StellarMass > 0
    # bulge_fraction[stellar_mass_nonzero] = BulgeMass[stellar_mass_nonzero] / StellarMass[stellar_mass_nonzero]
    
    # Cold gas metallicity (12 + log10[O/H])
    # metallicity_cold = np.full_like(MetalsColdGas, -99.0)
    # cold_gas_mask = (MetalsColdGas > 0) & (ColdGas > 0)
    # metallicity_cold[cold_gas_mask] = np.log10((MetalsColdGas[cold_gas_mask] / ColdGas[cold_gas_mask]) / 0.02) + 9.0
    
    # # CGM metallicity
    # metallicity_cgm = np.full_like(MetalsCGMgas, -99.0)
    # cgm_mask = (MetalsCGMgas > 0) & (cgm > 0)
    # metallicity_cgm[cgm_mask] = np.log10((MetalsCGMgas[cgm_mask] / cgm[cgm_mask]) / 0.02) + 9.0
    
    # Hot gas metallicity
    # metallicity_hot = np.full_like(metalshotgas, -99.0)
    # hot_gas_mask = (metalshotgas > 0) & (HotGas > 0)
    # metallicity_hot[hot_gas_mask] = np.log10((metalshotgas[hot_gas_mask] / HotGas[hot_gas_mask]) / 0.02) + 9.0
    
    # Virial temperature (approximate)
    virial_temp = 35.9 * Vvir**2  # in Kelvin
    
    # Surface densities (for galaxies with positive disk radius)
    # sigma_H2 = np.zeros_like(H2Gas)
    sigma_SFR = np.zeros_like(SFR_total)
    # disk_area_mask = DiskRadius > 0
    
    # if np.any(disk_area_mask):
    #     disk_radius_pc = DiskRadius[disk_area_mask] * 1.0e6 / Hubble_h  # Convert to pc
    #     disk_area = 2 * np.pi * disk_radius_pc**2
    #     sigma_H2[disk_area_mask] = H2Gas[disk_area_mask] / disk_area
    #     sigma_SFR[disk_area_mask] = SFR_total[disk_area_mask] / disk_area * 1.0e6  # Convert to per kpc^2

    # Create DataFrame with all properties
    print('Creating DataFrame...')
    
    galaxy_data = {
        # Basic identifiers and classification
        'GalaxyID': np.arange(len(StellarMass)),
        'Type': Type,
        'CentralGalaxyIndex': CentralGalaxyIndex,
        
        # Spatial coordinates
        'Pos_x': Posx,
        'Pos_y': Posy,  
        'Pos_z': Posz,

        'Vel_x': Velx,
        'Vel_y': Vely,
        'Vel_z': Velz,

        # Halo properties
        'Mvir': Mvir,
        'CentralMvir': CentralMvir,
        'InfallMvir': InfallMvir,
        'Rvir': Rvir,
        'Vvir': Vvir,
        'Vmax': Vmax,
        'Virial_Temperature': virial_temp,
        'NDM_particles': NDM,
        
        # Stellar properties
        'StellarMass': StellarMass,
        # 'BulgeMass': BulgeMass,
        # 'BulgeFraction': bulge_fraction,
        # 'DiskRadius': DiskRadius,
        # 'BlackHoleMass': BlackHoleMass,
        # 'IntraClusterStars': IntraClusterStars,
        
        # Gas properties
        # 'ColdGas': ColdGas,
        # 'HotGas': HotGas,
        # 'H1Gas': H1Gas,
        # 'H2Gas': H2Gas,
        # 'CGMgas': cgm,
        # 'BaryonicMass': BaryonicMass,
        # 'GasFraction': gas_fraction,
        # 'H2Fraction': h2_fraction,
        
        # Star formation
        'SFR_Disk': SfrDisk,
        'SFR_Bulge': SfrBulge,
        'SFR_Total': SFR_total,
        'sSFR': sSFR,
        
        # Metallicity
        # 'Metallicity_ColdGas': metallicity_cold,
        # 'Metallicity_CGM': metallicity_cgm,
        # 'Metallicity_HotGas': metallicity_hot,
        # 'MetalMass_ColdGas': MetalsColdGas,
        # 'MetalMass_CGM': MetalsCGMgas,
        # 'MetalMass_HotGas': metalshotgas,
        
        # Feedback and outflows
        # 'OutflowRate': outflowrate,
        # 'MassLoading': massload,
        # 'Cooling': Cooling,
        
        # # Surface densities
        # 'SurfaceDensity_H2': sigma_H2,
        'SurfaceDensity_SFR': sigma_SFR,
    }
    
    df = pd.DataFrame(galaxy_data)
    
    # Add some useful flags
    df['StarForming'] = df['sSFR'] > -11.0  # Star-forming vs quiescent threshold
    df['Central'] = df['Type'] == 0
    df['Satellite'] = df['Type'] == 1
    df['Orphan'] = df['Type'] == 2
    
    # Sort by descending stellar mass
    df = df.sort_values('StellarMass', ascending=False).reset_index(drop=True)
    
    print(f'DataFrame created with {len(df)} galaxies and {len(df.columns)} properties')
    
    # Save to CSV
    output_path = os.path.join(OutputCSVDir, OutputCSVFile)
    print(f'Saving to {output_path}...')
    
    df.to_csv(output_path, index=False, float_format='%.6e')
    
    print(f'Successfully exported galaxy properties to {output_path}')
    print(f'File size: {os.path.getsize(output_path) / (1024**2):.2f} MB')
    
    # Print summary statistics
    print('\n' + '='*60)
    print('SUMMARY STATISTICS')
    print('='*60)
    
    print(f'Total galaxies: {len(df):,}')
    print(f'Central galaxies: {df["Central"].sum():,}')
    print(f'Satellite galaxies: {df["Satellite"].sum():,}')
    print(f'Orphan galaxies: {df["Orphan"].sum():,}')
    print(f'Star-forming galaxies: {df["StarForming"].sum():,}')
    print(f'Quiescent galaxies: {(~df["StarForming"]).sum():,}')
    
    print(f'\nStellar mass range: {df["StellarMass"].min():.2e} - {df["StellarMass"].max():.2e} M_sun')
    print(f'Halo mass range: {df["Mvir"].min():.2e} - {df["Mvir"].max():.2e} M_sun')
    print(f'SFR range: {df["SFR_Total"].min():.2e} - {df["SFR_Total"].max():.2e} M_sun/yr')
    
    # Show first few rows
    print('\n' + '='*60)
    print('FIRST 5 GALAXIES (by stellar mass)')
    print('='*60)
    
    display_cols = ['GalaxyID', 'CentralGalaxyIndex', 'Type', 'Mvir', 'SFR_Total', 'Central', 'Pos_x', 'Pos_y', 'Pos_z', 'Vel_x', 'Vel_y', 'Vel_z']
    print(df[display_cols].head().to_string(index=False, float_format='%.3e'))
    
    print(f'\nCSV export completed successfully!')
    print(f'Data exported to: {output_path}')