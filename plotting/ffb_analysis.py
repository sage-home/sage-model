#!/usr/bin/env python
"""
FFB Galaxy Analysis Script
===========================
Analyzes and plots properties of Feedback-Free Burst (FFB) galaxies.

Creates plots for:
1. FFB galaxy evolution (Mvir, StellarMass, SFR, sSFR vs redshift)
2. Mass generation in FFB galaxies
3. FFB mass contribution to z=0 galaxies
4. Metallicity evolution
5. Merger rates of FFB galaxies
6. Bulge-to-total ratio: FFB vs normal galaxies (z=6-8)
7. Merger trees (color-coded by sSFR)
8. FFB progenitor trees (z=0 galaxies with FFB history)
9. BCG-style merger tree (single galaxy, full evolution)
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
from matplotlib.gridspec import GridSpec
import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model
FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read

redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

# Plotting options
OutputDir = './ffb_plots/'
OutputFormat = '.png'
plt.rcParams["figure.figsize"] = (10, 8)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 12

# FFB analysis options
min_stellar_mass = 1e7  # Minimum stellar mass to consider (Msun/h)
num_example_galaxies = 50  # Number of example FFB galaxies to track

# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read parameter from HDF5 file"""
    property = h5.File(DirName + FileName, 'r')
    snap_key = f'Snap_{snap_num}'
    return np.array(property[snap_key][param])


def ensure_output_dir():
    """Create output directory if it doesn't exist"""
    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)
        print(f"Created output directory: {OutputDir}")


def identify_ffb_galaxies():
    """
    Identify FFB galaxies across all snapshots and track their evolution.
    Returns a dictionary with galaxy indices and their properties over time.
    """
    print("\n" + "="*60)
    print("IDENTIFYING FFB GALAXIES")
    print("="*60)
    
    # Pass 1: Identify all galaxies that ever enter FFB regime
    print("Pass 1: Identifying all FFB galaxies across simulation...")
    ffb_galaxy_indices = set()
    
    for snap in range(FirstSnap, LastSnap + 1):
        try:
            ffb_regime = read_hdf(snap_num=snap, param='FFBRegime')
            galaxy_index = read_hdf(snap_num=snap, param='GalaxyIndex')
            
            is_ffb = (ffb_regime == 1)
            if np.any(is_ffb):
                ffb_galaxy_indices.update(galaxy_index[is_ffb])
        except Exception as e:
            # Silent fail for pass 1, will catch in pass 2
            continue
            
    print(f"Found {len(ffb_galaxy_indices)} unique galaxies that experience FFB phase.")
    
    # Pass 2: Track these galaxies
    print("Pass 2: Tracking evolution...")
    
    # Track FFB galaxies by their GalaxyIndex
    ffb_galaxy_data = {}
    
    # Statistics
    ffb_count_per_snap = []
    total_count_per_snap = []
    
    # Loop through snapshots from high to low redshift
    for snap in range(FirstSnap, LastSnap + 1):
        z = redshifts[snap]
        
        try:
            # Read data
            ffb_regime = read_hdf(snap_num=snap, param='FFBRegime')
            galaxy_index = read_hdf(snap_num=snap, param='GalaxyIndex')
            mvir = read_hdf(snap_num=snap, param='Mvir')
            stellar_mass = read_hdf(snap_num=snap, param='StellarMass')
            
            # Calculate total SFR = SfrBulge + SfrDisk
            sfr_bulge = read_hdf(snap_num=snap, param='SfrBulge')
            sfr_disk = read_hdf(snap_num=snap, param='SfrDisk')
            sfr = sfr_bulge + sfr_disk
            
            # Optional fields (may not exist in all runs)
            try:
                cold_gas = read_hdf(snap_num=snap, param='ColdGas')
            except:
                cold_gas = np.zeros_like(stellar_mass)
            
            try:
                metals_stellar = read_hdf(snap_num=snap, param='MetalsStellarMass')
            except:
                try:
                    # Try alternative field name
                    metals_stellar = read_hdf(snap_num=snap, param='StellarMetals')
                except:
                    metals_stellar = np.zeros_like(stellar_mass)
            
            try:
                merge_type = read_hdf(snap_num=snap, param='mergeType')
            except:
                merge_type = np.zeros_like(stellar_mass, dtype=int)
            
            # Identify FFB galaxies (for stats)
            is_ffb = (ffb_regime == 1)
            n_ffb = np.sum(is_ffb)
            n_total = len(ffb_regime)
            
            ffb_count_per_snap.append(n_ffb)
            total_count_per_snap.append(n_total)
            
            if n_ffb > 0:
                print(f"Snap {snap:3d} (z={z:6.2f}): {n_ffb:6d} FFB galaxies out of {n_total:8d} ({100*n_ffb/n_total:.3f}%)")
            
            # Store data for each FFB galaxy (if it is in our tracking list)
            # Use isin to find indices of galaxies we want to track
            is_trackable = np.isin(galaxy_index, list(ffb_galaxy_indices))
            
            for idx in np.where(is_trackable)[0]:
                gal_idx = galaxy_index[idx]
                
                if gal_idx not in ffb_galaxy_data:
                    ffb_galaxy_data[gal_idx] = {
                        'snapshots': [],
                        'redshifts': [],
                        'mvir': [],
                        'stellar_mass': [],
                        'sfr': [],
                        'cold_gas': [],
                        'metals_stellar': [],
                        'ffb_regime': [],
                        'merge_type': [],
                        'first_ffb_snap': snap,
                        'first_ffb_z': z
                    }
                
                data = ffb_galaxy_data[gal_idx]
                data['snapshots'].append(snap)
                data['redshifts'].append(z)
                data['mvir'].append(mvir[idx] * 1e10 / Hubble_h)  # Convert to Msun
                data['stellar_mass'].append(stellar_mass[idx] * 1e10 / Hubble_h)
                data['sfr'].append(sfr[idx])
                data['cold_gas'].append(cold_gas[idx] * 1e10 / Hubble_h)
                data['metals_stellar'].append(metals_stellar[idx])
                data['ffb_regime'].append(ffb_regime[idx])
                data['merge_type'].append(merge_type[idx])
                
        except Exception as e:
            print(f"Warning: Could not read snap {snap}: {e}")
            continue
    
    print(f"\nTotal unique FFB galaxies identified: {len(ffb_galaxy_data)}")
    
    # Convert lists to arrays
    for gal_idx in ffb_galaxy_data:
        for key in ['snapshots', 'redshifts', 'mvir', 'stellar_mass', 'sfr', 
                    'cold_gas', 'metals_stellar', 'ffb_regime', 'merge_type']:
            ffb_galaxy_data[gal_idx][key] = np.array(ffb_galaxy_data[gal_idx][key])
    
    return ffb_galaxy_data, ffb_count_per_snap, total_count_per_snap


def plot_ffb_evolution(ffb_data):
    """
    Plot 1: Evolution of FFB galaxies
    Shows Mvir, StellarMass, SFR, and sSFR vs redshift
    Highlights z=0 survivors as thick green lines and mergers as blue markers
    """
    print("\n" + "="*60)
    print("PLOTTING FFB GALAXY EVOLUTION")
    print("="*60)
    
    fig = plt.figure(figsize=(14, 10))
    gs = GridSpec(2, 2, hspace=0.3, wspace=0.3)
    
    # Identify z=0 survivors
    z0_survivors = set()
    
    # Diagnostic: Check what final redshifts and masses we have
    final_redshifts = []
    final_masses = []
    for gal_idx, data in ffb_data.items():
        final_redshifts.append(data['redshifts'][-1])
        final_masses.append(data['stellar_mass'][-1])
        # Check if galaxy exists at z=0 (last snapshot should be z~0)
        if data['redshifts'][-1] < 1.0 and data['stellar_mass'][-1] > min_stellar_mass:
            z0_survivors.add(gal_idx)
    
    print(f"Found {len(z0_survivors)} FFB galaxies that survive to z=0")
    print(f"  Total FFB galaxies: {len(ffb_data)}")
    print(f"  Final redshift range: {min(final_redshifts):.3f} to {max(final_redshifts):.3f}")
    print(f"  Final mass range: {min(final_masses):.2e} to {max(final_masses):.2e} Msun")
    print(f"  Minimum mass threshold: {min_stellar_mass:.2e} Msun")
    print(f"  Galaxies with z<1.0: {sum(1 for z in final_redshifts if z < 1.0)}")
    print(f"  Galaxies with M*>{min_stellar_mass:.1e}: {sum(1 for m in final_masses if m > min_stellar_mass)}")
    
    # Select example galaxies to highlight
    # Choose galaxies that spend significant time in FFB mode
    ffb_durations = []
    for gal_idx, data in ffb_data.items():
        ffb_time = np.sum(data['ffb_regime'] == 1)
        if data['stellar_mass'][-1] > min_stellar_mass:
            ffb_durations.append((gal_idx, ffb_time))
    
    ffb_durations.sort(key=lambda x: x[1], reverse=True)
    example_galaxies = [x[0] for x in ffb_durations[:num_example_galaxies]]
    
    print(f"Plotting {len(example_galaxies)} example FFB galaxies")
    
    # Plot 1: Mvir vs Redshift
    ax1 = fig.add_subplot(gs[0, 0])
    for i, gal_idx in enumerate(example_galaxies):
        data = ffb_data[gal_idx]
        
        # Only highlight FFB at the snapshots where it actually occurred (high-z)
        # FFBRegime=1 is persistent, so we identify true FFB phase as high-z snapshots
        is_ffb_active = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)  # Only at z>6
        
        # Check for mergers
        is_merger = (data['merge_type'] > 0) & (data['merge_type'] < 4)
        
        # Determine line style based on z=0 survival
        if gal_idx in z0_survivors:
            ax1.plot(data['redshifts'], data['mvir'], 'g-', alpha=0.8, lw=2.5)
        else:
            ax1.plot(data['redshifts'], data['mvir'], 'k-', alpha=0.3, lw=0.5)
        
        # Highlight actual FFB phases at high-z
        if np.any(is_ffb_active):
            ax1.plot(data['redshifts'][is_ffb_active], data['mvir'][is_ffb_active], 
                    'ro', markersize=3, alpha=0.6)
        
        # Highlight mergers
        if np.any(is_merger):
            ax1.plot(data['redshifts'][is_merger], data['mvir'][is_merger],
                    'bo', markersize=5, alpha=0.7)
    
    ax1.set_xlabel('Redshift')
    ax1.set_ylabel(r'$M_{\rm vir}$ [$M_\odot$]')
    ax1.set_yscale('log')
    ax1.set_title('Virial Mass Evolution')
    ax1.grid(True, alpha=0.3)
    
    # Plot 2: Stellar Mass vs Redshift
    ax2 = fig.add_subplot(gs[0, 1])
    for gal_idx in example_galaxies:
        data = ffb_data[gal_idx]
        is_ffb_active = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)
        is_merger = (data['merge_type'] > 0) & (data['merge_type'] < 4)
        valid = data['stellar_mass'] > 0
        
        if np.any(valid):
            if gal_idx in z0_survivors:
                ax2.plot(data['redshifts'][valid], data['stellar_mass'][valid], 
                        'g-', alpha=0.8, lw=2.5)
            else:
                ax2.plot(data['redshifts'][valid], data['stellar_mass'][valid], 
                        'k-', alpha=0.3, lw=0.5)
            
            if np.any(is_ffb_active & valid):
                ax2.plot(data['redshifts'][is_ffb_active & valid], 
                        data['stellar_mass'][is_ffb_active & valid], 
                        'ro', markersize=3, alpha=0.6)
            
            if np.any(is_merger & valid):
                ax2.plot(data['redshifts'][is_merger & valid],
                        data['stellar_mass'][is_merger & valid],
                        'bo', markersize=5, alpha=0.7)
    
    ax2.set_xlabel('Redshift')
    ax2.set_ylabel(r'$M_*$ [$M_\odot$]')
    ax2.set_yscale('log')
    ax2.set_title('Stellar Mass Evolution')
    ax2.grid(True, alpha=0.3)
    
    # Plot 3: SFR vs Redshift
    ax3 = fig.add_subplot(gs[1, 0])
    for gal_idx in example_galaxies:
        data = ffb_data[gal_idx]
        is_ffb_active = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)
        is_merger = (data['merge_type'] > 0) & (data['merge_type'] < 4)
        valid = data['sfr'] > 0
        
        if np.any(valid):
            if gal_idx in z0_survivors:
                ax3.plot(data['redshifts'][valid], data['sfr'][valid], 
                        'g-', alpha=0.8, lw=2.5)
            else:
                ax3.plot(data['redshifts'][valid], data['sfr'][valid], 
                        'k-', alpha=0.3, lw=0.5)
            
            if np.any(is_ffb_active & valid):
                ax3.plot(data['redshifts'][is_ffb_active & valid], 
                        data['sfr'][is_ffb_active & valid], 
                        'ro', markersize=3, alpha=0.6)
            
            if np.any(is_merger & valid):
                ax3.plot(data['redshifts'][is_merger & valid],
                        data['sfr'][is_merger & valid],
                        'bo', markersize=5, alpha=0.7)
    
    ax3.set_xlabel('Redshift')
    ax3.set_ylabel(r'SFR [$M_\odot$ yr$^{-1}$]')
    ax3.set_yscale('log')
    ax3.set_title('Star Formation Rate')
    ax3.grid(True, alpha=0.3)
    
    # Plot 4: sSFR vs Redshift
    ax4 = fig.add_subplot(gs[1, 1])
    for gal_idx in example_galaxies:
        data = ffb_data[gal_idx]
        is_ffb_active = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)
        is_merger = (data['merge_type'] > 0) & (data['merge_type'] < 4)
        valid = (data['sfr'] > 0) & (data['stellar_mass'] > 0)
        
        if np.any(valid):
            ssfr = data['sfr'][valid] / data['stellar_mass'][valid] * 1e9  # Gyr^-1
            
            if gal_idx in z0_survivors:
                ax4.plot(data['redshifts'][valid], ssfr, 'g-', alpha=0.8, lw=2.5)
            else:
                ax4.plot(data['redshifts'][valid], ssfr, 'k-', alpha=0.3, lw=0.5)
            
            if np.any(is_ffb_active & valid):
                ssfr_ffb = data['sfr'][is_ffb_active & valid] / data['stellar_mass'][is_ffb_active & valid] * 1e9
                ax4.plot(data['redshifts'][is_ffb_active & valid], ssfr_ffb, 
                        'ro', markersize=3, alpha=0.6)
            
            if np.any(is_merger & valid):
                ssfr_merger = data['sfr'][is_merger & valid] / data['stellar_mass'][is_merger & valid] * 1e9
                ax4.plot(data['redshifts'][is_merger & valid], ssfr_merger,
                        'bo', markersize=5, alpha=0.7)
    
    ax4.set_xlabel('Redshift')
    ax4.set_ylabel(r'sSFR [Gyr$^{-1}$]')
    ax4.set_yscale('log')
    ax4.set_title('Specific Star Formation Rate')
    ax4.grid(True, alpha=0.3)
    
    # Add legend
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], color='k', alpha=0.5, lw=0.5, label='Galaxy evolution'),
        Line2D([0], [0], color='g', alpha=0.8, lw=2.5, label='z=0 survivors'),
        Line2D([0], [0], marker='o', color='w', markerfacecolor='r', 
               markersize=6, label='FFB phase (z>6)', alpha=0.6),
        Line2D([0], [0], marker='o', color='w', markerfacecolor='b',
               markersize=6, label='Mergers', alpha=0.7)
    ]
    ax4.legend(handles=legend_elements, loc='lower right', fontsize=10)
    
    plt.suptitle(f'FFB Galaxy Evolution ({len(example_galaxies)} example galaxies, {len(z0_survivors)} z=0 survivors)', 
                 fontsize=14, y=0.995)
    
    filename = OutputDir + 'ffb_evolution' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()


def plot_mass_generation(ffb_data, ffb_counts, total_counts):
    """
    Plot 2: Mass generation in FFB galaxies
    Shows cumulative stellar mass formed in FFB mode vs redshift
    """
    print("\n" + "="*60)
    print("PLOTTING MASS GENERATION IN FFB GALAXIES")
    print("="*60)
    
    # Calculate mass generated per snapshot
    mass_in_ffb_per_snap = []
    total_mass_per_snap = []
    mass_formed_in_ffb_per_snap = []
    
    for snap in range(FirstSnap, LastSnap + 1):
        try:
            ffb_regime = read_hdf(snap_num=snap, param='FFBRegime')
            stellar_mass = read_hdf(snap_num=snap, param='StellarMass') * 1e10 / Hubble_h
            
            is_ffb = (ffb_regime == 1)
            
            mass_in_ffb = np.sum(stellar_mass[is_ffb]) if np.any(is_ffb) else 0
            total_mass = np.sum(stellar_mass)
            
            mass_in_ffb_per_snap.append(mass_in_ffb)
            total_mass_per_snap.append(total_mass)
            
            # Estimate mass formed in FFB (crude approximation)
            if snap > 0 and np.any(is_ffb):
                # Mass formed ~ change in stellar mass for FFB galaxies
                prev_stellar_mass = read_hdf(snap_num=snap-1, param='StellarMass') * 1e10 / Hubble_h
                # This is a simplification - proper tracking would require galaxy matching
                mass_formed = np.sum(stellar_mass[is_ffb]) - np.sum(prev_stellar_mass[:len(stellar_mass[is_ffb])])
                mass_formed = max(0, mass_formed)
            else:
                mass_formed = 0
            
            mass_formed_in_ffb_per_snap.append(mass_formed)
            
        except Exception as e:
            mass_in_ffb_per_snap.append(0)
            total_mass_per_snap.append(0)
            mass_formed_in_ffb_per_snap.append(0)
    
    mass_in_ffb_per_snap = np.array(mass_in_ffb_per_snap)
    total_mass_per_snap = np.array(total_mass_per_snap)
    mass_formed_in_ffb_per_snap = np.array(mass_formed_in_ffb_per_snap)
    
    # Get redshifts only for snapshots that were actually read
    z_array = np.array(redshifts[FirstSnap:FirstSnap+len(mass_in_ffb_per_snap)])
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Plot 1: Stellar mass in FFB galaxies vs z
    ax1 = axes[0, 0]
    ax1.plot(z_array, mass_in_ffb_per_snap, 'r-', lw=2, label='In FFB galaxies')
    ax1.plot(z_array, total_mass_per_snap, 'k--', lw=1, alpha=0.5, label='Total')
    ax1.set_xlabel('Redshift')
    ax1.set_ylabel(r'Stellar Mass [$M_\odot$]')
    ax1.set_yscale('log')
    ax1.legend()
    ax1.set_title('Stellar Mass in FFB Galaxies')
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(0, 15)
    
    # Plot 2: Fraction of stellar mass in FFB galaxies
    ax2 = axes[0, 1]
    fraction = np.divide(mass_in_ffb_per_snap, total_mass_per_snap, 
                        where=total_mass_per_snap>0, out=np.zeros_like(mass_in_ffb_per_snap))
    ax2.plot(z_array, fraction * 100, 'b-', lw=2)
    ax2.set_xlabel('Redshift')
    ax2.set_ylabel('Fraction of stellar mass in FFB galaxies [%]')
    ax2.set_title('FFB Stellar Mass Fraction')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(bottom=0)
    ax2.set_xlim(0, 15)
    
    # Plot 3: Number of FFB galaxies vs z
    ax3 = axes[1, 0]
    ffb_counts_array = np.array(ffb_counts[:len(z_array)])
    total_counts_array = np.array(total_counts[:len(z_array)])
    ax3.plot(z_array, ffb_counts_array, 'r-', lw=2, label='FFB galaxies')
    ax3.plot(z_array, total_counts_array, 'k--', lw=1, alpha=0.5, label='Total galaxies')
    ax3.set_xlabel('Redshift')
    ax3.set_ylabel('Number of galaxies')
    ax3.set_yscale('log')
    ax3.legend()
    ax3.set_title('Number of FFB Galaxies')
    ax3.grid(True, alpha=0.3)
    ax3.set_xlim(0, 15)
    
    # Plot 4: FFB fraction
    ax4 = axes[1, 1]
    ffb_fraction = np.divide(ffb_counts_array, total_counts_array, 
                            where=total_counts_array>0, out=np.zeros_like(ffb_counts_array, dtype=float))
    ax4.plot(z_array, ffb_fraction * 100, 'g-', lw=2)
    ax4.set_xlabel('Redshift')
    ax4.set_ylabel('FFB galaxy fraction [%]')
    ax4.set_title('Fraction of Galaxies in FFB Mode')
    ax4.grid(True, alpha=0.3)
    ax4.set_ylim(bottom=0)
    ax4.set_xlim(0, 15)
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_mass_generation' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()
    
    # Print summary statistics
    print(f"\nMass generation summary:")
    print(f"  Peak FFB mass fraction: {np.max(fraction)*100:.2f}% at z={z_array[np.argmax(fraction)]:.2f}")
    print(f"  Peak FFB number fraction: {np.max(ffb_fraction)*100:.2f}% at z={z_array[np.argmax(ffb_fraction)]:.2f}")


def plot_ffb_contribution_z0(ffb_data):
    """
    Plot 3: FFB mass contribution to z=0 galaxies
    Shows what fraction of z=0 stellar mass was formed during FFB phase
    """
    print("\n" + "="*60)
    print("PLOTTING FFB CONTRIBUTION TO Z=0 GALAXIES")
    print("="*60)
    
    # Read z=0 data
    try:
        stellar_mass_z0 = read_hdf(snap_num=LastSnap, param='StellarMass') * 1e10 / Hubble_h
        galaxy_index_z0 = read_hdf(snap_num=LastSnap, param='GalaxyIndex')
    except:
        print("Warning: Could not read z=0 data")
        return
    
    # For each z=0 galaxy, calculate how much mass was formed during FFB
    ffb_mass_fraction = []
    final_masses = []
    
    for i, gal_idx in enumerate(galaxy_index_z0):
        if gal_idx in ffb_data:
            data = ffb_data[gal_idx]
            
            # Find snapshots where galaxy was in FFB mode
            is_ffb = data['ffb_regime'] == 1
            
            if np.any(is_ffb):
                # Estimate mass formed during FFB
                # This is simplified - assumes mass difference during FFB phase
                ffb_snaps = np.where(is_ffb)[0]
                if len(ffb_snaps) > 0:
                    mass_start_ffb = data['stellar_mass'][ffb_snaps[0]] if ffb_snaps[0] > 0 else 0
                    mass_end_ffb = data['stellar_mass'][ffb_snaps[-1]]
                    mass_formed_in_ffb = max(0, mass_end_ffb - mass_start_ffb)
                    
                    final_mass = stellar_mass_z0[i]
                    if final_mass > min_stellar_mass:
                        fraction = mass_formed_in_ffb / final_mass if final_mass > 0 else 0
                        ffb_mass_fraction.append(min(fraction, 1.0))  # Cap at 100%
                        final_masses.append(final_mass)
    
    if len(ffb_mass_fraction) == 0:
        print("Warning: No z=0 galaxies with FFB history found")
        return
    
    ffb_mass_fraction = np.array(ffb_mass_fraction)
    final_masses = np.array(final_masses)
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    
    # Plot 1: Histogram of FFB mass fractions
    ax1 = axes[0]
    ax1.hist(ffb_mass_fraction * 100, bins=30, color='steelblue', alpha=0.7, edgecolor='black')
    ax1.set_xlabel('FFB mass fraction [%]')
    ax1.set_ylabel('Number of z=0 galaxies')
    ax1.set_title('Distribution of FFB Mass Contribution\nat z=0')
    ax1.grid(True, alpha=0.3, axis='y')
    
    # Add statistics
    median_frac = np.median(ffb_mass_fraction) * 100
    mean_frac = np.mean(ffb_mass_fraction) * 100
    ax1.axvline(median_frac, color='red', linestyle='--', lw=2, label=f'Median: {median_frac:.1f}%')
    ax1.axvline(mean_frac, color='orange', linestyle='--', lw=2, label=f'Mean: {mean_frac:.1f}%')
    ax1.legend()
    
    # Plot 2: FFB fraction vs final mass
    ax2 = axes[1]
    ax2.scatter(final_masses, ffb_mass_fraction * 100, alpha=0.5, s=10)
    ax2.set_xlabel(r'$M_*$ at z=0 [$M_\odot$]')
    ax2.set_ylabel('FFB mass fraction [%]')
    ax2.set_xscale('log')
    ax2.set_title('FFB Contribution vs Final Stellar Mass')
    ax2.grid(True, alpha=0.3)
    
    # Add running median
    mass_bins = np.logspace(np.log10(final_masses.min()), np.log10(final_masses.max()), 10)
    bin_centers = []
    median_fracs = []
    for i in range(len(mass_bins)-1):
        mask = (final_masses >= mass_bins[i]) & (final_masses < mass_bins[i+1])
        if np.sum(mask) > 5:
            bin_centers.append(np.sqrt(mass_bins[i] * mass_bins[i+1]))
            median_fracs.append(np.median(ffb_mass_fraction[mask]) * 100)
    
    if len(bin_centers) > 0:
        ax2.plot(bin_centers, median_fracs, 'r-', lw=2, label='Running median')
        ax2.legend()
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_contribution_z0' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()
    
    print(f"\nFFB contribution to z=0 galaxies:")
    print(f"  Galaxies analyzed: {len(ffb_mass_fraction)}")
    print(f"  Mean FFB fraction: {mean_frac:.2f}%")
    print(f"  Median FFB fraction: {median_frac:.2f}%")
    print(f"  Galaxies with >50% FFB mass: {np.sum(ffb_mass_fraction>0.5)} ({100*np.sum(ffb_mass_fraction>0.5)/len(ffb_mass_fraction):.1f}%)")


def plot_metallicity_evolution(ffb_data):
    """
    Plot 4: Metallicity evolution and z=0 metallicity of FFB galaxies
    """
    print("\n" + "="*60)
    print("PLOTTING METALLICITY EVOLUTION")
    print("="*60)
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    
    # Plot 1: Metallicity vs redshift - use ALL galaxies, not just examples
    ax1 = axes[0]
    
    has_metallicity_data = False
    n_galaxies_plotted = 0
    n_points_plotted = 0
    all_metallicities = []
    all_redshifts = []
    all_is_ffb = []
    
    # Collect all data points first
    for gal_idx, data in ffb_data.items():
        if data['stellar_mass'][-1] < min_stellar_mass:
            continue
            
        # Calculate metallicity (Z = metals / stellar_mass, solar = 0.02)
        valid = (data['stellar_mass'] > 0) & (data['metals_stellar'] > 0)
        
        if np.sum(valid) > 0:
            has_metallicity_data = True
            metallicity = np.divide(data['metals_stellar'], data['stellar_mass'], 
                                   where=valid, out=np.zeros_like(data['metals_stellar']))
            metallicity = metallicity / 0.02  # In units of solar
            
            is_ffb_active = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)
            
            # Only collect valid metallicity points
            valid_Z = valid & (metallicity > 0)
            
            if np.sum(valid_Z) >= 1:
                n_galaxies_plotted += 1
                n_points_plotted += np.sum(valid_Z)
                
                # Store all data points
                all_metallicities.extend(metallicity[valid_Z].tolist())
                all_redshifts.extend(data['redshifts'][valid_Z].tolist())
                all_is_ffb.extend(is_ffb_active[valid_Z].tolist())
    
    if not has_metallicity_data or len(all_metallicities) == 0:
        ax1.text(0.5, 0.5, 'No metallicity data available\n(MetalsStellarMass field missing or all zero)', 
                ha='center', va='center', transform=ax1.transAxes, fontsize=12, 
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
        print("Warning: No metallicity data found. Check if MetalsStellarMass field exists in output.")
    else:
        # Convert to arrays
        all_metallicities = np.array(all_metallicities)
        all_redshifts = np.array(all_redshifts)
        all_is_ffb = np.array(all_is_ffb)
        
        print(f"  Plotted {n_galaxies_plotted} galaxies with {n_points_plotted} total data points")
        print(f"  Metallicity range: {all_metallicities.min():.2e} to {all_metallicities.max():.2e} Z/Zsun")
        print(f"  Redshift range: {all_redshifts.min():.2f} to {all_redshifts.max():.2f}")
        
        # Plot non-FFB points first (background)
        non_ffb_mask = ~all_is_ffb
        if np.any(non_ffb_mask):
            ax1.scatter(all_redshifts[non_ffb_mask], all_metallicities[non_ffb_mask], 
                       c='gray', s=10, alpha=0.5, edgecolors='none', label='Normal phase')
        
        # Plot FFB points on top (foreground)
        ffb_mask = all_is_ffb
        if np.any(ffb_mask):
            ax1.scatter(all_redshifts[ffb_mask], all_metallicities[ffb_mask], 
                       c='red', s=20, alpha=0.7, edgecolors='none', label='FFB phase (z>6)')
        
        # Adjust y-limits based on actual data
        if all_metallicities.max() > 0:
            ymin = max(1e-15, all_metallicities.min() * 0.3)
            ymax = min(10, all_metallicities.max() * 3)
            ax1.set_ylim(ymin, ymax)
    
    ax1.set_xlabel('Redshift')
    ax1.set_ylabel(r'Metallicity [$Z/Z_\odot$]')
    ax1.set_yscale('log')
    ax1.set_title('Metallicity Evolution')
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='lower right', fontsize=10)
    
    # Add text annotation about the extremely low metallicities
    if len(all_metallicities) > 0 and all_metallicities.max() < 1e-6:
        ax1.text(0.98, 0.02, f'Extremely metal-poor\n(~10$^{{-11}}$ to 10$^{{-13}}$ Z$_\\odot$)', 
                transform=ax1.transAxes, ha='right', va='bottom',
                fontsize=9, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    # Plot 2: z=0 metallicity distribution for galaxies with FFB history
    ax2 = axes[1]
    z0_metallicities = []
    z0_masses = []
    
    for gal_idx in ffb_data:
        data = ffb_data[gal_idx]
        if len(data['stellar_mass']) > 0 and data['stellar_mass'][-1] > min_stellar_mass:
            final_metals = data['metals_stellar'][-1]
            final_mass = data['stellar_mass'][-1]
            
            if final_mass > 0:
                metallicity = (final_metals / final_mass) / 0.02
                if metallicity > 0:
                    z0_metallicities.append(metallicity)
                    z0_masses.append(final_mass)
    
    if len(z0_metallicities) > 0:
        z0_metallicities = np.array(z0_metallicities)
        z0_masses = np.array(z0_masses)
        
        ax2.scatter(z0_masses, z0_metallicities, alpha=0.5, s=20, c='steelblue')
        ax2.set_xlabel(r'$M_*$ at z=0 [$M_\odot$]')
        ax2.set_ylabel(r'Metallicity at z=0 [$Z/Z_\odot$]')
        ax2.set_xscale('log')
        ax2.set_yscale('log')
        ax2.set_title('z=0 Metallicity vs Stellar Mass\n(Galaxies with FFB history)')
        ax2.grid(True, alpha=0.3)
        
        # Add mass-metallicity relation fit
        log_mass = np.log10(z0_masses)
        log_Z = np.log10(z0_metallicities)
        
        mass_bins = np.logspace(np.log10(z0_masses.min()), np.log10(z0_masses.max()), 8)
        bin_centers = []
        median_Z = []
        
        for i in range(len(mass_bins)-1):
            mask = (z0_masses >= mass_bins[i]) & (z0_masses < mass_bins[i+1])
            if np.sum(mask) > 3:
                bin_centers.append(np.sqrt(mass_bins[i] * mass_bins[i+1]))
                median_Z.append(np.median(z0_metallicities[mask]))
        
        if len(bin_centers) > 0:
            ax2.plot(bin_centers, median_Z, 'r-', lw=2, label='Running median')
            ax2.legend()
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_metallicity' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()


def plot_merger_rates(ffb_data):
    """
    Plot 5: Merger rates of FFB galaxies
    """
    print("\n" + "="*60)
    print("PLOTTING MERGER RATES")
    print("="*60)
    
    # Count mergers during FFB phase vs non-FFB phase
    mergers_during_ffb = []
    mergers_outside_ffb = []
    redshifts_ffb_mergers = []
    redshifts_normal_mergers = []
    
    for gal_idx, data in ffb_data.items():
        # Only consider FFB as active at high-z (z>6)
        is_ffb_active = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)
        merge_type = data['merge_type']
        
        # merge_type: 0=none, 1=minor, 2=major, 3=disk instability, 4=disruption
        is_merger = (merge_type > 0) & (merge_type < 4)
        
        # Mergers during active FFB phase
        ffb_mergers = is_merger & is_ffb_active
        if np.any(ffb_mergers):
            mergers_during_ffb.extend(merge_type[ffb_mergers].tolist())
            redshifts_ffb_mergers.extend(data['redshifts'][ffb_mergers].tolist())
        
        # Mergers outside FFB (for galaxies that experienced FFB at some point)
        normal_mergers = is_merger & ~is_ffb_active
        if np.any(normal_mergers):
            mergers_outside_ffb.extend(merge_type[normal_mergers].tolist())
            redshifts_normal_mergers.extend(data['redshifts'][normal_mergers].tolist())
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Plot 1: Merger type distribution during FFB
    ax1 = axes[0, 0]
    if len(mergers_during_ffb) > 0:
        merge_types, counts = np.unique(mergers_during_ffb, return_counts=True)
        colors = ['skyblue', 'coral', 'lightgreen']
        labels = ['Minor merger', 'Major merger', 'Disk instability']
        
        ax1.bar(range(len(merge_types)), counts, color=[colors[int(mt)-1] for mt in merge_types])
        ax1.set_xticks(range(len(merge_types)))
        ax1.set_xticklabels([labels[int(mt)-1] for mt in merge_types], rotation=15)
        ax1.set_ylabel('Number of events')
        ax1.set_title(f'Mergers During Active FFB Phase (z>6)\n(Total: {len(mergers_during_ffb)})')
        ax1.grid(True, alpha=0.3, axis='y')
    else:
        ax1.text(0.5, 0.5, 'No mergers during FFB', ha='center', va='center', 
                transform=ax1.transAxes, fontsize=14)
        ax1.set_title('Mergers During Active FFB Phase (z>6)')
    
    # Plot 2: Merger type distribution outside FFB
    ax2 = axes[0, 1]
    if len(mergers_outside_ffb) > 0:
        merge_types, counts = np.unique(mergers_outside_ffb, return_counts=True)
        ax2.bar(range(len(merge_types)), counts, color=[colors[int(mt)-1] for mt in merge_types])
        ax2.set_xticks(range(len(merge_types)))
        ax2.set_xticklabels([labels[int(mt)-1] for mt in merge_types], rotation=15)
        ax2.set_ylabel('Number of events')
        ax2.set_title(f'Mergers Outside FFB Phase\n(Total: {len(mergers_outside_ffb)})')
        ax2.grid(True, alpha=0.3, axis='y')
    else:
        ax2.text(0.5, 0.5, 'No mergers outside FFB', ha='center', va='center',
                transform=ax2.transAxes, fontsize=14)
        ax2.set_title('Mergers Outside FFB Phase')
    
    # Plot 3: Merger redshift distribution during FFB
    ax3 = axes[1, 0]
    if len(redshifts_ffb_mergers) > 0:
        ax3.hist(redshifts_ffb_mergers, bins=20, color='coral', alpha=0.7, edgecolor='black')
        ax3.set_xlabel('Redshift')
        ax3.set_ylabel('Number of mergers')
        ax3.set_title('Redshift Distribution of FFB Mergers')
        ax3.grid(True, alpha=0.3, axis='y')
        ax3.invert_xaxis()
    else:
        ax3.text(0.5, 0.5, 'No FFB mergers', ha='center', va='center',
                transform=ax3.transAxes, fontsize=14)
        ax3.set_title('Redshift Distribution of FFB Mergers')
    
    # Plot 4: Comparison of merger rates
    ax4 = axes[1, 1]
    
    # Calculate merger rates per galaxy
    n_galaxies = len(ffb_data)
    rate_ffb = len(mergers_during_ffb) / n_galaxies if n_galaxies > 0 else 0
    rate_normal = len(mergers_outside_ffb) / n_galaxies if n_galaxies > 0 else 0
    
    categories = ['FFB Phase', 'Normal Phase']
    rates = [rate_ffb, rate_normal]
    colors_bar = ['coral', 'steelblue']
    
    ax4.bar(categories, rates, color=colors_bar, alpha=0.7, edgecolor='black')
    ax4.set_ylabel('Mergers per galaxy')
    ax4.set_title('Average Merger Rate Comparison')
    ax4.grid(True, alpha=0.3, axis='y')
    
    # Add values on top of bars
    for i, (cat, rate) in enumerate(zip(categories, rates)):
        ax4.text(i, rate, f'{rate:.3f}', ha='center', va='bottom', fontsize=12, fontweight='bold')
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_mergers' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()
    
    print(f"\nMerger statistics:")
    print(f"  Total galaxies with FFB history: {n_galaxies}")
    print(f"  Mergers during FFB: {len(mergers_during_ffb)}")
    print(f"  Mergers outside FFB: {len(mergers_outside_ffb)}")
    print(f"  Average mergers per galaxy (FFB): {rate_ffb:.3f}")
    print(f"  Average mergers per galaxy (normal): {rate_normal:.3f}")


def plot_bulge_to_total_ratio():
    """
    Plot bulge-to-total ratio as a function of redshift
    Compares FFB galaxies vs normal galaxies in redshift range 6-8
    Bulge mass = MergerBulgeMass + InstabilityBulgeMass
    """
    print("\n" + "="*60)
    print("PLOTTING BULGE-TO-TOTAL RATIO (z=6-8)")
    print("="*60)
    
    # Redshift range to analyze
    z_min, z_max = 6.0, 8.0
    
    # Storage for data
    ffb_redshifts = []
    ffb_ratios = []
    normal_redshifts = []
    normal_ratios = []
    
    # Loop through snapshots in the redshift range
    for snap in range(FirstSnap, LastSnap + 1):
        z = redshifts[snap]
        
        # Only analyze snapshots in the desired redshift range
        if z < z_min or z > z_max:
            continue
        
        try:
            # Read data
            ffb_regime = read_hdf(snap_num=snap, param='FFBRegime')
            stellar_mass = read_hdf(snap_num=snap, param='StellarMass') * 1e10 / Hubble_h
            merger_bulge_mass = read_hdf(snap_num=snap, param='MergerBulgeMass') * 1e10 / Hubble_h
            instability_bulge_mass = read_hdf(snap_num=snap, param='InstabilityBulgeMass') * 1e10 / Hubble_h
            bulge_mass = read_hdf(snap_num=snap, param='BulgeMass') * 1e10 / Hubble_h
            
            # Calculate total bulge mass
            total_bulge_mass = bulge_mass
            
            # Filter: only galaxies above minimum stellar mass with positive bulge and stellar mass
            valid = (stellar_mass > min_stellar_mass) & (stellar_mass > 0) & (total_bulge_mass >= 0)
            
            # Calculate bulge-to-total ratio
            bulge_to_total = np.divide(total_bulge_mass, stellar_mass, 
                                      where=valid, out=np.zeros_like(stellar_mass))
            
            # Separate FFB and normal galaxies
            is_ffb = (ffb_regime == 1) & valid
            is_normal = (ffb_regime == 0) & valid
            
            # Store FFB galaxy data
            if np.any(is_ffb):
                n_ffb = np.sum(is_ffb)
                ffb_redshifts.extend([z] * n_ffb)
                ffb_ratios.extend(bulge_to_total[is_ffb].tolist())
            
            # Store normal galaxy data
            if np.any(is_normal):
                n_normal = np.sum(is_normal)
                normal_redshifts.extend([z] * n_normal)
                normal_ratios.extend(bulge_to_total[is_normal].tolist())
            
            print(f"Snap {snap:3d} (z={z:6.2f}): {np.sum(is_ffb):6d} FFB, {np.sum(is_normal):6d} normal galaxies")
            
        except Exception as e:
            print(f"Warning: Could not read snap {snap}: {e}")
            continue
    
    # Convert to arrays
    ffb_redshifts = np.array(ffb_redshifts)
    ffb_ratios = np.array(ffb_ratios)
    normal_redshifts = np.array(normal_redshifts)
    normal_ratios = np.array(normal_ratios)
    
    print(f"\nTotal data points collected:")
    print(f"  FFB galaxies: {len(ffb_ratios)}")
    print(f"  Normal galaxies: {len(normal_ratios)}")
    
    if len(ffb_ratios) == 0 and len(normal_ratios) == 0:
        print("Warning: No data found in redshift range 6-8")
        return
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Plot scatter points with transparency
    if len(normal_ratios) > 0:
        ax.scatter(normal_redshifts, normal_ratios, c='steelblue', s=5, alpha=0.3, 
                  label=f'Normal galaxies (N={len(normal_ratios)})', edgecolors='none')
    
    if len(ffb_ratios) > 0:
        ax.scatter(ffb_redshifts, ffb_ratios, c='red', s=10, alpha=0.4, 
                  label=f'FFB galaxies (N={len(ffb_ratios)})', edgecolors='none')
    
    # Calculate and plot median trends with percentile bands
    z_bins = np.linspace(z_min, z_max, 10)
    
    # Normal galaxies trend
    if len(normal_ratios) > 0:
        bin_centers_normal = []
        median_normal = []
        mean_normal = []
        p25_normal = []
        p75_normal = []
        
        for i in range(len(z_bins) - 1):
            mask = (normal_redshifts >= z_bins[i]) & (normal_redshifts < z_bins[i+1])
            if np.sum(mask) > 5:  # Require at least 5 galaxies per bin
                bin_centers_normal.append((z_bins[i] + z_bins[i+1]) / 2)
                median_normal.append(np.median(normal_ratios[mask]))
                mean_normal.append(np.mean(normal_ratios[mask]))
                p25_normal.append(np.percentile(normal_ratios[mask], 25))
                p75_normal.append(np.percentile(normal_ratios[mask], 75))
        
        if len(bin_centers_normal) > 0:
            bin_centers_normal = np.array(bin_centers_normal)
            median_normal = np.array(median_normal)
            mean_normal = np.array(mean_normal)
            p25_normal = np.array(p25_normal)
            p75_normal = np.array(p75_normal)
            
            ax.plot(bin_centers_normal, median_normal, 'b-', lw=3, label='Normal median', zorder=5)
            ax.plot(bin_centers_normal, mean_normal, 'b--', lw=2, alpha=0.7, label='Normal mean', zorder=5)
            ax.fill_between(bin_centers_normal, p25_normal, p75_normal, 
                           color='steelblue', alpha=0.3, label='Normal 25-75%', zorder=3)
    
    # FFB galaxies trend
    if len(ffb_ratios) > 0:
        bin_centers_ffb = []
        median_ffb = []
        mean_ffb = []
        p25_ffb = []
        p75_ffb = []
        
        for i in range(len(z_bins) - 1):
            mask = (ffb_redshifts >= z_bins[i]) & (ffb_redshifts < z_bins[i+1])
            if np.sum(mask) > 5:
                bin_centers_ffb.append((z_bins[i] + z_bins[i+1]) / 2)
                median_ffb.append(np.median(ffb_ratios[mask]))
                mean_ffb.append(np.mean(ffb_ratios[mask]))
                p25_ffb.append(np.percentile(ffb_ratios[mask], 25))
                p75_ffb.append(np.percentile(ffb_ratios[mask], 75))
        
        if len(bin_centers_ffb) > 0:
            bin_centers_ffb = np.array(bin_centers_ffb)
            median_ffb = np.array(median_ffb)
            mean_ffb = np.array(mean_ffb)
            p25_ffb = np.array(p25_ffb)
            p75_ffb = np.array(p75_ffb)
            
            ax.plot(bin_centers_ffb, median_ffb, 'r-', lw=3, label='FFB median', zorder=5)
            ax.plot(bin_centers_ffb, mean_ffb, 'r--', lw=2, alpha=0.7, label='FFB mean', zorder=5)
            ax.fill_between(bin_centers_ffb, p25_ffb, p75_ffb, 
                           color='red', alpha=0.3, label='FFB 25-75%', zorder=3)
    
    ax.set_xlabel('Redshift', fontsize=14)
    ax.set_ylabel(r'Bulge-to-Total Ratio ($M_{\rm bulge} / M_*$)', fontsize=14)
    ax.set_title(r'Bulge-to-Total Ratio: FFB vs Normal Galaxies (z=6-8)' + '\n' + 
                 r'$M_{\rm bulge} = M_{\rm bulge,merger} + M_{\rm bulge,instability}$', 
                 fontsize=15)
    ax.set_xlim(z_min, z_max)
    ax.set_ylim(0, 0.2)
    ax.legend(loc='best', fontsize=10, framealpha=0.9)
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_bulge_to_total_ratio' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"\nSaved: {filename}")
    plt.close()
    
    # Print summary statistics
    print(f"\nBulge-to-total ratio statistics:")
    if len(ffb_ratios) > 0:
        print(f"  FFB galaxies:")
        print(f"    Median: {np.median(ffb_ratios):.3f}")
        print(f"    Mean: {np.mean(ffb_ratios):.3f}")
        print(f"    25th-75th percentile: {np.percentile(ffb_ratios, 25):.3f} - {np.percentile(ffb_ratios, 75):.3f}")
    if len(normal_ratios) > 0:
        print(f"  Normal galaxies:")
        print(f"    Median: {np.median(normal_ratios):.3f}")
        print(f"    Mean: {np.mean(normal_ratios):.3f}")
        print(f"    25th-75th percentile: {np.percentile(normal_ratios, 25):.3f} - {np.percentile(normal_ratios, 75):.3f}")


def redshift_to_lookback_time(z, H0=73, OmegaM=0.25, OmegaL=0.75):
    """
    Convert redshift to lookback time in Gyr
    Uses simple cosmology calculator
    """
    from scipy.integrate import quad
    
    def integrand(z_prime):
        return 1.0 / ((1 + z_prime) * np.sqrt(OmegaM * (1 + z_prime)**3 + OmegaL))
    
    # Hubble time in Gyr
    H0_inv = 9.778 / (H0 / 100.0)  # Gyr
    
    if isinstance(z, (list, np.ndarray)):
        times = []
        for zi in z:
            if zi <= 0:
                times.append(0.0)
            else:
                result, _ = quad(integrand, 0, zi)
                times.append(H0_inv * result)
        return np.array(times)
    else:
        if z <= 0:
            return 0.0
        result, _ = quad(integrand, 0, z)
        return H0_inv * result


def plot_merger_tree(ffb_data, num_trees=5):
    """
    Plot merger trees for selected FFB galaxies
    Similar to BCG merger tree plots, color-coded by sSFR
    """
    print("\n" + "="*60)
    print("PLOTTING FFB MERGER TREES")
    print("="*60)
    
    # Select galaxies that survive to z=0 with high mass
    z0_survivors = []
    for gal_idx, data in ffb_data.items():
        if data['redshifts'][-1] < 1.0 and data['stellar_mass'][-1] > min_stellar_mass * 10:
            z0_survivors.append((gal_idx, data['stellar_mass'][-1]))
    
    if len(z0_survivors) == 0:
        print("Warning: No massive z=0 survivors found for merger tree plot")
        return
    
    # Sort by final mass and select most massive
    z0_survivors.sort(key=lambda x: x[1], reverse=True)
    selected_galaxies = [x[0] for x in z0_survivors[:min(num_trees, len(z0_survivors))]]
    
    print(f"Plotting {len(selected_galaxies)} merger trees")
    
    # Create figure
    fig, ax = plt.subplots(figsize=(16, 10))
    
    # Color map for sSFR
    from matplotlib.colors import Normalize
    from matplotlib.cm import ScalarMappable
    
    # Collect all sSFR values to set color scale
    all_ssfr = []
    for gal_idx in selected_galaxies:
        data = ffb_data[gal_idx]
        valid = (data['sfr'] > 0) & (data['stellar_mass'] > 0)
        if np.any(valid):
            ssfr = np.log10(data['sfr'][valid] / data['stellar_mass'][valid] * 1e9)  # log10(Gyr^-1)
            all_ssfr.extend(ssfr.tolist())
    
    if len(all_ssfr) == 0:
        print("Warning: No valid sSFR data for color-coding")
        return
    
    # Set color normalization (similar to B-V range)
    ssfr_min, ssfr_max = np.percentile(all_ssfr, [5, 95])
    norm = Normalize(vmin=ssfr_min, vmax=ssfr_max)
    cmap = plt.cm.coolwarm  # Blue (high sSFR) to Red (low sSFR)
    
    # Plot each merger tree
    x_offset = 0
    x_spacing = 1.5  # Horizontal spacing between trees
    
    for tree_idx, gal_idx in enumerate(selected_galaxies):
        data = ffb_data[gal_idx]
        
        # Calculate lookback times
        lookback_times = redshift_to_lookback_time(data['redshifts'])
        
        # Calculate sSFR
        valid = (data['sfr'] > 0) & (data['stellar_mass'] > 0)
        ssfr = np.zeros_like(data['sfr'])
        ssfr[valid] = data['sfr'][valid] / data['stellar_mass'][valid] * 1e9  # Gyr^-1
        log_ssfr = np.zeros_like(ssfr)
        log_ssfr[valid] = np.log10(ssfr[valid])
        
        # For invalid sSFR (quenched galaxies), use minimum value
        log_ssfr[~valid] = ssfr_min
        
        # Calculate symbol sizes based on stellar mass
        # Size range from 10 to 200
        mass_norm = (np.log10(data['stellar_mass'] + 1) - 7) / 5  # Normalize roughly
        sizes = 10 + 190 * np.clip(mass_norm, 0, 1)
        
        # X positions (all at same x for this tree, with slight jitter at early times for visibility)
        x_pos = np.ones(len(lookback_times)) * (x_offset + tree_idx * x_spacing)
        
        # Add slight random jitter to early times to avoid overlap
        jitter = np.random.normal(0, 0.05, len(lookback_times))
        jitter[lookback_times < 3] = 0  # No jitter for recent times
        x_pos = x_pos + jitter
        
        # Plot connecting lines (progenitor spine)
        ax.plot(x_pos, lookback_times, '-', color='gray', alpha=0.3, lw=1, zorder=1)
        
        # Plot galaxy points colored by sSFR
        scatter = ax.scatter(x_pos, lookback_times, c=log_ssfr, s=sizes, 
                           cmap=cmap, norm=norm, alpha=0.8, edgecolors='black', 
                           linewidths=0.5, zorder=2)
        
        # Highlight mergers with different marker
        is_merger = (data['merge_type'] > 0) & (data['merge_type'] < 4)
        if np.any(is_merger):
            ax.scatter(x_pos[is_merger], lookback_times[is_merger], 
                      s=sizes[is_merger]*1.5, facecolors='none', 
                      edgecolors='blue', linewidths=2, zorder=3, marker='o')
        
        # Highlight FFB phase
        is_ffb = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)
        if np.any(is_ffb):
            ax.scatter(x_pos[is_ffb], lookback_times[is_ffb],
                      s=sizes[is_ffb]*1.3, facecolors='none',
                      edgecolors='gold', linewidths=2, zorder=3, marker='s')
        
        # Add final mass annotation at z=0
        final_mass_label = f"{data['stellar_mass'][-1]:.1e}"
        ax.text(x_pos[0], -0.5, final_mass_label, ha='center', va='top', 
               fontsize=8, rotation=45)
    
    # Add colorbar
    cbar = plt.colorbar(ScalarMappable(norm=norm, cmap=cmap), ax=ax, 
                        label=r'log$_{10}$(sSFR [Gyr$^{-1}$])')
    cbar.ax.tick_params(labelsize=10)
    
    # Formatting
    ax.set_xlabel('Merger Tree Index', fontsize=14)
    ax.set_ylabel('Lookback Time [Gyr]', fontsize=14)
    ax.set_title(f'FFB Galaxy Merger Trees (N={len(selected_galaxies)})\nColor: sSFR | Size: Stellar Mass', 
                 fontsize=14, pad=15)
    ax.invert_yaxis()  # Early times at top
    ax.grid(True, alpha=0.3, axis='y')
    
    # Set x-axis limits and labels
    ax.set_xlim(-0.5, len(selected_galaxies) * x_spacing - 0.5)
    ax.set_xticks([i * x_spacing for i in range(len(selected_galaxies))])
    ax.set_xticklabels([f'Tree {i+1}' for i in range(len(selected_galaxies))])
    
    # Add legend
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], marker='o', color='w', markerfacecolor='gray', 
               markersize=8, label='Galaxy', markeredgecolor='black'),
        Line2D([0], [0], marker='o', color='w', markerfacecolor='none',
               markeredgecolor='blue', markersize=10, markeredgewidth=2, label='Merger'),
        Line2D([0], [0], marker='s', color='w', markerfacecolor='none',
               markeredgecolor='gold', markersize=10, markeredgewidth=2, label='FFB phase (z>6)')
    ]
    ax.legend(handles=legend_elements, loc='lower right', fontsize=11, framealpha=0.9)
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_merger_trees' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()
    
    print(f"Plotted {len(selected_galaxies)} merger trees")
    print(f"sSFR range: {ssfr_min:.2f} to {ssfr_max:.2f} log10(Gyr^-1)")


def identify_z0_galaxies_with_ffb_progenitors():
    """
    Identify all z=0 galaxies and determine which contain FFB progenitors
    Returns dictionary of z=0 galaxy data with FFB history flagged
    """
    print("\n" + "="*60)
    print("IDENTIFYING Z=0 GALAXIES WITH FFB PROGENITORS")
    print("="*60)
    
    # Read z=0 data
    z0_snap = LastSnap
    galaxy_index_z0 = read_hdf(snap_num=z0_snap, param='GalaxyIndex')
    stellar_mass_z0 = read_hdf(snap_num=z0_snap, param='StellarMass') * 1e10 / Hubble_h
    
    # Track all galaxies at z=0 with mass above threshold
    z0_galaxy_data = {}
    
    # Loop through all snapshots and track galaxies that end up at z=0
    for snap in range(FirstSnap, LastSnap + 1):
        z = redshifts[snap]
        
        try:
            galaxy_index = read_hdf(snap_num=snap, param='GalaxyIndex')
            mvir = read_hdf(snap_num=snap, param='Mvir')
            stellar_mass = read_hdf(snap_num=snap, param='StellarMass')
            sfr_bulge = read_hdf(snap_num=snap, param='SfrBulge')
            sfr_disk = read_hdf(snap_num=snap, param='SfrDisk')
            sfr = sfr_bulge + sfr_disk
            ffb_regime = read_hdf(snap_num=snap, param='FFBRegime')
            
            try:
                merge_type = read_hdf(snap_num=snap, param='mergeType')
            except:
                merge_type = np.zeros_like(stellar_mass, dtype=int)
            
            # For each galaxy at this snapshot
            for idx, gal_idx in enumerate(galaxy_index):
                # Check if this galaxy exists at z=0
                if gal_idx in galaxy_index_z0:
                    z0_idx = np.where(galaxy_index_z0 == gal_idx)[0][0]
                    final_mass = stellar_mass_z0[z0_idx]
                    
                    # Only track if final mass is above threshold
                    if final_mass > min_stellar_mass:
                        if gal_idx not in z0_galaxy_data:
                            z0_galaxy_data[gal_idx] = {
                                'snapshots': [],
                                'redshifts': [],
                                'mvir': [],
                                'stellar_mass': [],
                                'sfr': [],
                                'ffb_regime': [],
                                'merge_type': [],
                                'final_mass': final_mass,
                                'had_ffb': False
                            }
                        
                        data = z0_galaxy_data[gal_idx]
                        data['snapshots'].append(snap)
                        data['redshifts'].append(z)
                        data['mvir'].append(mvir[idx] * 1e10 / Hubble_h)
                        data['stellar_mass'].append(stellar_mass[idx] * 1e10 / Hubble_h)
                        data['sfr'].append(sfr[idx])
                        data['ffb_regime'].append(ffb_regime[idx])
                        data['merge_type'].append(merge_type[idx])
                        
                        # Flag if it was ever in FFB mode
                        if ffb_regime[idx] == 1:
                            data['had_ffb'] = True
        except Exception as e:
            print(f"Warning: Could not read snap {snap}: {e}")
            continue
    
    # Convert lists to arrays
    for gal_idx in z0_galaxy_data:
        for key in ['snapshots', 'redshifts', 'mvir', 'stellar_mass', 'sfr', 'ffb_regime', 'merge_type']:
            z0_galaxy_data[gal_idx][key] = np.array(z0_galaxy_data[gal_idx][key])
    
    # Count galaxies with FFB history
    galaxies_with_ffb = sum(1 for data in z0_galaxy_data.values() if data['had_ffb'])
    
    print(f"Total z=0 galaxies tracked: {len(z0_galaxy_data)}")
    print(f"z=0 galaxies with FFB progenitors: {galaxies_with_ffb}")
    
    return z0_galaxy_data


def plot_ffb_progenitor_trees(num_trees=5):
    """
    Plot merger trees for z=0 galaxies that contain FFB progenitors
    Shows full evolution from high-z FFB phase to present day
    """
    print("\n" + "="*60)
    print("PLOTTING FFB PROGENITOR MERGER TREES")
    print("="*60)
    
    # Get z=0 galaxies with FFB history
    z0_data = identify_z0_galaxies_with_ffb_progenitors()
    
    # Filter for galaxies with FFB history
    ffb_progenitor_galaxies = [(gal_idx, data['final_mass']) 
                                for gal_idx, data in z0_data.items() 
                                if data['had_ffb']]
    
    if len(ffb_progenitor_galaxies) == 0:
        print("Warning: No z=0 galaxies with FFB progenitors found")
        return
    
    # Sort by final mass and select most massive
    ffb_progenitor_galaxies.sort(key=lambda x: x[1], reverse=True)
    selected_galaxies = [x[0] for x in ffb_progenitor_galaxies[:min(num_trees, len(ffb_progenitor_galaxies))]]
    
    print(f"Plotting {len(selected_galaxies)} z=0 galaxies with FFB progenitors")
    
    # Create figure
    fig, ax = plt.subplots(figsize=(16, 10))
    
    # Color map for sSFR
    from matplotlib.colors import Normalize
    from matplotlib.cm import ScalarMappable
    
    # Collect all sSFR values to set color scale
    all_ssfr = []
    for gal_idx in selected_galaxies:
        data = z0_data[gal_idx]
        valid = (data['sfr'] > 0) & (data['stellar_mass'] > 0)
        if np.any(valid):
            ssfr = np.log10(data['sfr'][valid] / data['stellar_mass'][valid] * 1e9)
            all_ssfr.extend(ssfr.tolist())
    
    if len(all_ssfr) == 0:
        print("Warning: No valid sSFR data for color-coding")
        return
    
    # Set color normalization
    ssfr_min, ssfr_max = np.percentile(all_ssfr, [5, 95])
    norm = Normalize(vmin=ssfr_min, vmax=ssfr_max)
    cmap = plt.cm.coolwarm
    
    # Plot each merger tree
    x_spacing = 1.5
    
    for tree_idx, gal_idx in enumerate(selected_galaxies):
        data = z0_data[gal_idx]
        
        # Calculate lookback times
        lookback_times = redshift_to_lookback_time(data['redshifts'])
        
        # Calculate sSFR
        valid = (data['sfr'] > 0) & (data['stellar_mass'] > 0)
        ssfr = np.zeros_like(data['sfr'])
        ssfr[valid] = data['sfr'][valid] / data['stellar_mass'][valid] * 1e9
        log_ssfr = np.zeros_like(ssfr)
        log_ssfr[valid] = np.log10(ssfr[valid])
        log_ssfr[~valid] = ssfr_min
        
        # Calculate symbol sizes
        mass_norm = (np.log10(data['stellar_mass'] + 1) - 7) / 5
        sizes = 10 + 190 * np.clip(mass_norm, 0, 1)
        
        # X positions with jitter
        x_pos = np.ones(len(lookback_times)) * (tree_idx * x_spacing)
        jitter = np.random.normal(0, 0.05, len(lookback_times))
        jitter[lookback_times < 3] = 0
        x_pos = x_pos + jitter
        
        # Plot connecting lines
        ax.plot(x_pos, lookback_times, '-', color='gray', alpha=0.3, lw=1, zorder=1)
        
        # Plot galaxy points colored by sSFR
        scatter = ax.scatter(x_pos, lookback_times, c=log_ssfr, s=sizes,
                           cmap=cmap, norm=norm, alpha=0.8, edgecolors='black',
                           linewidths=0.5, zorder=2)
        
        # Highlight mergers
        is_merger = (data['merge_type'] > 0) & (data['merge_type'] < 4)
        if np.any(is_merger):
            ax.scatter(x_pos[is_merger], lookback_times[is_merger],
                      s=sizes[is_merger]*1.5, facecolors='none',
                      edgecolors='blue', linewidths=2, zorder=3, marker='o')
        
        # Highlight FFB progenitor phase
        is_ffb = (data['ffb_regime'] == 1) & (data['redshifts'] >= 6.0)
        if np.any(is_ffb):
            ax.scatter(x_pos[is_ffb], lookback_times[is_ffb],
                      s=sizes[is_ffb]*1.3, facecolors='none',
                      edgecolors='gold', linewidths=2, zorder=3, marker='s')
        
        # Add final mass annotation at z=0
        final_mass_label = f"{data['final_mass']:.1e}"
        ax.text(x_pos[0], -0.5, final_mass_label, ha='center', va='top',
               fontsize=8, rotation=45)
    
    # Add colorbar
    cbar = plt.colorbar(ScalarMappable(norm=norm, cmap=cmap), ax=ax,
                        label=r'log$_{10}$(sSFR [Gyr$^{-1}$])')
    cbar.ax.tick_params(labelsize=10)
    
    # Formatting
    ax.set_xlabel('Galaxy Tree Index', fontsize=14)
    ax.set_ylabel('Lookback Time [Gyr]', fontsize=14)
    ax.set_title(f'z=0 Galaxies with FFB Progenitors (N={len(selected_galaxies)})\nColor: sSFR | Size: Stellar Mass | Gold Squares: FFB Phase',
                 fontsize=14, pad=15)
    ax.invert_yaxis()
    ax.grid(True, alpha=0.3, axis='y')
    
    # Set x-axis
    ax.set_xlim(-0.5, len(selected_galaxies) * x_spacing - 0.5)
    ax.set_xticks([i * x_spacing for i in range(len(selected_galaxies))])
    ax.set_xticklabels([f'Galaxy {i+1}' for i in range(len(selected_galaxies))])
    
    # Add legend
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], marker='o', color='w', markerfacecolor='gray',
               markersize=8, label='Evolution', markeredgecolor='black'),
        Line2D([0], [0], marker='s', color='w', markerfacecolor='none',
               markeredgecolor='gold', markersize=10, markeredgewidth=2, label='FFB progenitor (z>6)'),
        Line2D([0], [0], marker='o', color='w', markerfacecolor='none',
               markeredgecolor='blue', markersize=10, markeredgewidth=2, label='Merger')
    ]
    ax.legend(handles=legend_elements, loc='lower right', fontsize=11, framealpha=0.9)
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_progenitor_trees' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()
    
    print(f"Successfully plotted {len(selected_galaxies)} z=0 galaxies with FFB progenitors")


def plot_bcg_style_merger_tree():
    """
    Plot exact BCG merger tree by reconstructing full merger history from SAGE output.
    Reads all snapshots to link progenitors via GalaxyIndex and mergeIntoID (Tree-Relative).
    """
    print("\n" + "="*60)
    print("PLOTTING EXACT BCG MERGER TREE")
    print("="*60)
    
    # ---------------------------------------------------------
    # Step 1: Identify Target Galaxy (Global BCG at z=0)
    # ---------------------------------------------------------
    print("Identifying target galaxy (Global BCG)...")
    
    # Read z=0 properties
    try:
        g_index = read_hdf(snap_num=LastSnap, param='GalaxyIndex')
        stellar_mass = read_hdf(snap_num=LastSnap, param='StellarMass') * 1e10 / Hubble_h
        
        # Find most massive
        max_idx = np.argmax(stellar_mass)
        target_gal_idx = g_index[max_idx]
        target_mass = stellar_mass[max_idx]
        
        print(f"Target Galaxy Index: {target_gal_idx}")
        print(f"Target Mass: {target_mass:.2e} Msun")
        
    except Exception as e:
        print(f"Error identifying BCG: {e}")
        return
    
    # ---------------------------------------------------------
    # Step 2: Build Global Maps (IndexMap and MergerMap)
    # ---------------------------------------------------------
    print("\nBuilding global merger tree maps (reading all snapshots)...")
    
    # IndexMap: GalaxyIndex -> {SnapNum: (TreeID, LocalID)}
    # Tracks the identity of a galaxy across snapshots
    index_map = {}
    
    # MergerMap: (DestSnap, TreeID, DestLocalID) -> list of (SrcSnap, TreeID, SrcLocalID)
    # Tracks which galaxies merge into a specific galaxy
    merger_map = {}
    
    # DataCache: (Snap, TreeID, LocalID) -> {Mass, sSFR, FFB, etc}
    # Stores properties for plotting
    data_cache = {}
    
    # Read all snapshots
    for snap in range(FirstSnap, LastSnap + 1):
        if snap % 5 == 0:
            print(f"  Processing snapshot {snap}/{LastSnap}...")
            
        try:
            # Read necessary fields
            g_index = read_hdf(snap_num=snap, param='GalaxyIndex')
            
            # Read Tree Index
            try:
                tree_idx = read_hdf(snap_num=snap, param='SAGETreeIndex')
            except:
                print(f"  Warning: SAGETreeIndex missing in snap {snap}")
                continue
                
            # Compute TreeLocalID
            # Robust way for unsorted arrays:
            if len(tree_idx) > 0:
                # 1. Sort to group by TreeID
                sort_idx = np.argsort(tree_idx, kind='stable')
                sorted_tree = tree_idx[sort_idx]
                
                # 2. Find unique trees and counts
                unique_trees, indices, counts = np.unique(sorted_tree, return_index=True, return_counts=True)
                
                # 3. Assign local IDs in sorted order
                local_ids_sorted = np.zeros(len(tree_idx), dtype=np.int32)
                for start, count in zip(indices, counts):
                    local_ids_sorted[start:start+count] = np.arange(count)
                
                # 4. Map back to original order
                local_ids = np.zeros(len(tree_idx), dtype=np.int32)
                local_ids[sort_idx] = local_ids_sorted
            else:
                local_ids = np.array([], dtype=np.int32)
            
            try:
                merge_into_id = read_hdf(snap_num=snap, param='mergeIntoID')
                merge_into_snap = read_hdf(snap_num=snap, param='mergeIntoSnapNum')
                merge_type = read_hdf(snap_num=snap, param='mergeType')
            except:
                merge_into_id = np.zeros_like(tree_idx) - 1
                merge_into_snap = np.zeros_like(tree_idx) - 1
                merge_type = np.zeros_like(tree_idx)
            
            # Properties
            stellar_mass = read_hdf(snap_num=snap, param='StellarMass') * 1e10 / Hubble_h
            mvir = read_hdf(snap_num=snap, param='Mvir') * 1e10 / Hubble_h
            sfr = (read_hdf(snap_num=snap, param='SfrDisk') + read_hdf(snap_num=snap, param='SfrBulge'))
            ffb = read_hdf(snap_num=snap, param='FFBRegime')
            gal_type = read_hdf(snap_num=snap, param='Type') # 0=Central, 1=Satellite
            
            # Populate maps
            # This loop is slow in Python. Optimize?
            # We can skip the loop if we don't need to store everything in IndexMap.
            # But we need IndexMap to trace the main branch.
            
            # Let's just loop. 30k galaxies is fast.
            for i in range(len(g_index)):
                idx = g_index[i]
                tid = tree_idx[i]
                lid = local_ids[i]
                
                key = (snap, tid, lid)
                
                # 1. Update IndexMap
                if idx not in index_map:
                    index_map[idx] = {}
                index_map[idx][snap] = (tid, lid)
                
                # 2. Update DataCache
                if stellar_mass[i] > 0 and sfr[i] > 0:
                    this_ssfr = sfr[i] / stellar_mass[i] * 1e9
                else:
                    this_ssfr = 1e-4
                    
                data_cache[key] = {
                    'mass': stellar_mass[i],
                    'mvir': mvir[i],
                    'ssfr': this_ssfr,
                    'ffb': ffb[i],
                    'type': gal_type[i],
                    'redshift': redshifts[snap]
                }
                
                # 3. Update MergerMap
                if merge_type[i] > 0:
                    dest_snap = merge_into_snap[i]
                    dest_lid = merge_into_id[i]
                    # Mergers happen within same tree
                    dest_tid = tid 
                    
                    if dest_snap >= snap:
                        dest_key = (dest_snap, dest_tid, dest_lid)
                        src_key = (snap, tid, lid)
                        
                        if dest_key not in merger_map:
                            merger_map[dest_key] = []
                        merger_map[dest_key].append(src_key)
                        
        except Exception as e:
            print(f"  Warning: Error reading snap {snap}: {e}")
            import traceback
            traceback.print_exc()
            continue

    print(f"Maps built successfully. Total mergers tracked: {len(merger_map)}")
    
    # Debug Tree 20 mergers
    tree_20_mergers = [k for k in merger_map.keys() if k[1] == 20]
    print(f"Mergers for Tree 20: {len(tree_20_mergers)}")
    if len(tree_20_mergers) > 0:
        print(f"Sample Tree 20 keys: {tree_20_mergers[:5]}")
        # Check if any match the target galaxy ID (which we don't know the LID of yet, but we can guess)
        
    # ---------------------------------------------------------
    # Step 3: Recursive Tree Building
    # ---------------------------------------------------------
    print("\nReconstructing tree structure...")
    
    plot_items = {'lines': [], 'symbols': []}
    
    def get_time(snap):
        return redshift_to_lookback_time(redshifts[snap])
    
    # Reverse Map: (Snap, TreeID, LocalID) -> GalaxyIndex
    print("Building reverse lookup map...")
    reverse_map = {}
    for idx, history in index_map.items():
        for snap, val in history.items():
            reverse_map[(snap, val[0], val[1])] = idx
            
    processed_mergers = set()
    
    def build_tree_recursive(current_idx, x_pos, width, start_snap, depth=0):
        if current_idx not in index_map: return
        
        history = index_map[current_idx]
        snaps = sorted([s for s in history.keys() if s <= start_snap], reverse=True)
        
        if not snaps: return

        points = []
        for snap in snaps:
            tid, lid = history[snap]
            key = (snap, tid, lid)
            if key not in data_cache: continue
            
            props = data_cache[key]
            y = get_time(snap)
            points.append((x_pos, y, props, key))
        
        # Add symbol for the head of the branch (if it's the start of a tree)
        if depth == 0 and len(points) > 0:
            p0 = points[0]
            mass = p0[2]['mass']
            if mass > 1e10:
                size = 20 + (np.log10(mass) - 10) * 50
                marker = 'o' # Central
                color = plt.cm.seismic_r((np.log10(p0[2]['ssfr']) - (-2)) / 3)
                plot_items['symbols'].append((p0[0], p0[1], size, color, marker, 'k'))
        
        # Draw segments
        for i in range(len(points) - 1):
            p1 = points[i]
            p2 = points[i+1]
            
            mass = p2[2]['mass']
            ssfr = p2[2]['ssfr']
            ffb = p2[2]['ffb']
            
            # Color map
            log_ssfr = np.log10(ssfr)
            vmin, vmax = -2, 1
            norm_val = (log_ssfr - vmin) / (vmax - vmin)
            norm_val = np.clip(norm_val, 0, 1)
            color = plt.cm.seismic_r(norm_val)
            
            lw = 1.0 + np.log10(mass/1e8 + 1) * 0.5
            if lw < 0.5: lw = 0.5
            
            plot_items['lines'].append((p1[0], p1[1], p2[0], p2[1], color, lw))
            
            # Symbol
            g_type = p2[2]['type']
            
            if mass > 1e10:
                size = 20 + (np.log10(mass) - 10) * 50
                
                # Marker shape based on Type
                if g_type == 1: # Satellite
                    marker = '^' # Triangle
                else:
                    marker = 'o' # Circle
                    
                edge = 'k'
                if ffb == 1:
                    marker = 's'
                    edge = 'gold'
                    size *= 1.5
                
                plot_items['symbols'].append((p2[0], p2[1], size, color, marker, edge))
            elif ffb == 1:
                plot_items['symbols'].append((p2[0], p2[1], 40, color, 's', 'gold'))

            # Check for mergers into p1 (the descendant at this step)
            # p1 is at Snap N. We look for mergers that end at Snap N.
            # Cast snap to np.int32 to match merger_map keys exactly
            key_raw = p1[3]
            key = (np.int32(key_raw[0]), key_raw[1], key_raw[2])
            
            if key in merger_map:
                mergers = merger_map[key]
                
                merger_infos = []
                for m_snap, m_tid, m_lid in mergers:
                    m_key = (m_snap, m_tid, m_lid)
                    if m_key in data_cache:
                        m_mass = data_cache[m_key]['mass']
                        merger_infos.append((m_snap, m_tid, m_lid, m_mass))
                
                merger_infos.sort(key=lambda x: x[3], reverse=True)
                
                spread = width * 1.5
                
                for m_i, (m_snap, m_tid, m_lid, m_mass) in enumerate(merger_infos):
                    m_key = (m_snap, m_tid, m_lid)
                    if m_key in processed_mergers: continue
                    processed_mergers.add(m_key)
                    
                    if m_key in reverse_map:
                        src_idx = reverse_map[m_key]
                        
                        sign = 1 if m_i % 2 == 0 else -1
                        offset = sign * (spread * (1 + m_i//2))
                        
                        new_x = x_pos + offset
                        new_width = width * 0.5
                        
                        m_y = get_time(m_snap)
                        dest_y = p1[1] # Connect to p1 (descendant)
                        
                        plot_items['lines'].append((new_x, m_y, x_pos, dest_y, 'gray', 0.5))
                        
                        build_tree_recursive(src_idx, new_x, new_width, m_snap, depth+1)

    # ---------------------------------------------------------
    # Step 4: Trace and Select Best Tree
    # ---------------------------------------------------------
    print("Tracing potential trees to select the best visualization...")
    
    # Candidate 1: Mass-selected Target
    print(f"Tracing Candidate 1 (Mass-selected): Index {target_gal_idx}")
    plot_items_1 = {'lines': [], 'symbols': []}
    # Temporarily redirect plot_items
    original_plot_items = plot_items
    plot_items = plot_items_1
    build_tree_recursive(target_gal_idx, 0.0, 4.0, LastSnap)
    count_1 = len(plot_items_1['lines'])
    print(f"  Candidate 1 has {count_1} segments")
    
    # Candidate 2: Central (LID 0) of the same tree
    plot_items_2 = {'lines': [], 'symbols': []}
    count_2 = 0
    idx_lid0 = -1
    
    if target_gal_idx in index_map:
        tid_target = index_map[target_gal_idx][LastSnap][0]
        key_lid0 = (LastSnap, tid_target, 0)
        if key_lid0 in reverse_map:
            idx_lid0 = reverse_map[key_lid0]
            print(f"Tracing Candidate 2 (Central LID 0): Index {idx_lid0}")
            plot_items = plot_items_2
            build_tree_recursive(idx_lid0, 0.0, 4.0, LastSnap)
            count_2 = len(plot_items_2['lines'])
            print(f"  Candidate 2 has {count_2} segments")
    
    # Select best
    plot_items = original_plot_items
    if count_2 > count_1 * 2: # significantly better
        print("Selecting Candidate 2 (Central) due to richer history")
        plot_items['lines'] = plot_items_2['lines']
        plot_items['symbols'] = plot_items_2['symbols']
        final_target_mass = data_cache[key_lid0]['mass'] if idx_lid0 != -1 and key_lid0 in data_cache else 0
    else:
        print("Selecting Candidate 1 (Mass-selected)")
        plot_items['lines'] = plot_items_1['lines']
        plot_items['symbols'] = plot_items_1['symbols']
        final_target_mass = target_mass

    # ---------------------------------------------------------
    # Step 5: Rendering
    # ---------------------------------------------------------
    print(f"Rendering {len(plot_items['lines'])} segments and {len(plot_items['symbols'])} symbols...")
    
    fig, ax = plt.subplots(figsize=(16, 12))
    
    # Plot lines
    for x1, y1, x2, y2, c, lw in plot_items['lines']:
        ax.plot([x1, x2], [y1, y2], color=c, lw=lw, alpha=0.8, solid_capstyle='round')
        
    # Plot symbols
    for x, y, s, c, m, ec in plot_items['symbols']:
        ax.scatter(x, y, s=s, c=[c], marker=m, edgecolors=ec, linewidth=1.5, zorder=10)
        
    # Formatting
    ax.set_ylabel('Lookback Time [Gyr]', fontsize=14, fontweight='bold')
    ax.set_xlabel('', fontsize=14)
    ax.set_title(f'Exact BCG Merger Tree\n$M_{{final}}$ = {final_target_mass:.2e} $M_\\odot$', 
                 fontsize=16, fontweight='bold', pad=20)
    
    ax.invert_yaxis()
    ax.grid(True, alpha=0.1, axis='y')
    
    # Remove x ticks
    ax.set_xticks([])
    
    # Add Colorbar
    from matplotlib.cm import ScalarMappable
    from matplotlib.colors import Normalize
    norm = Normalize(vmin=-2, vmax=1)
    cmap = plt.cm.seismic_r
    cbar = plt.colorbar(ScalarMappable(norm=norm, cmap=cmap), ax=ax, pad=0.02)
    cbar.set_label(r'log$_{10}$(sSFR [Gyr$^{-1}$])', fontsize=12)
    
    # Add Legend
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], marker='o', color='w', markerfacecolor='gray', markersize=10, label='Central (M > 10$^{10}$)'),
        Line2D([0], [0], marker='^', color='w', markerfacecolor='gray', markersize=10, label='Satellite (M > 10$^{10}$)'),
        Line2D([0], [0], marker='s', color='w', markerfacecolor='none', markeredgecolor='gold', markeredgewidth=2, label='FFB Phase'),
        Line2D([0], [0], color='gray', lw=2, label='Progenitor Branch')
    ]
    ax.legend(handles=legend_elements, loc='lower right', fontsize=12)
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_bcg_merger_tree_exact' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()


def main():
    """Main analysis routine"""
    print("\n" + "="*70)
    print(" "*15 + "FFB GALAXY ANALYSIS")
    print("="*70)
    
    # Create output directory
    ensure_output_dir()
    
    # Check if data file exists
    if not os.path.exists(DirName + FileName):
        print(f"\nERROR: Data file not found: {DirName + FileName}")
        print("Please update the DirName and FileName variables in the script.")
        return
    
    # Identify FFB galaxies and collect their evolution
    ffb_data, ffb_counts, total_counts = identify_ffb_galaxies()
    
    if len(ffb_data) == 0:
        print("\nWARNING: No FFB galaxies found!")
        print("This could mean:")
        print("  1. FFB mode is not enabled (FeedbackFreeModeOn = 0)")
        print("  2. No galaxies meet the FFB criteria")
        print("  3. FFBRegime field is not saved in output")
        return
    
    # Generate all plots
    print("\n" + "="*70)
    print("GENERATING PLOTS")
    print("="*70)
    
    plot_ffb_evolution(ffb_data)
    plot_mass_generation(ffb_data, ffb_counts, total_counts)
    plot_ffb_contribution_z0(ffb_data)
    plot_metallicity_evolution(ffb_data)
    plot_merger_rates(ffb_data)
    plot_bulge_to_total_ratio()  # Bulge-to-total ratio: FFB vs normal galaxies (z=6-8)
    plot_merger_tree(ffb_data, num_trees=5)
    plot_ffb_progenitor_trees(num_trees=5)  # z=0 galaxies with FFB progenitors
    plot_bcg_style_merger_tree()  # BCG-style single tree for most massive z=0 galaxy
    
    print("\n" + "="*70)
    print("ANALYSIS COMPLETE!")
    print(f"All plots saved to: {OutputDir}")
    print("="*70 + "\n")


if __name__ == "__main__":
    main()