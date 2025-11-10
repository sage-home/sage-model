#!/usr/bin/env python

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
FileName = 'model_0.hdf5'

# Simulation details
Hubble_h = 0.73
BoxSize = 62.5
VolumeFraction = 1.0
FirstSnap = 0
LastSnap = 63
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 
             11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 
             4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 2.831, 2.619, 2.422, 2.239, 
             2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 
             0.242, 0.208, 0.175, 0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

OutputFormat = '.png'

# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    property = h5.File(DirName+FileName, 'r')
    return np.array(property[snap_num][param])

# ==================================================================

if __name__ == '__main__':
    
    print('Creating regime evolution plot\n')
    
    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction
    
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    # Read galaxy properties for all snapshots
    print('Reading galaxy properties...\n')
    
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    TypeFull = [0]*(LastSnap-FirstSnap+1)
    RegimeFull = [0]*(LastSnap-FirstSnap+1)
    GalaxyIndexFull = [0]*(LastSnap-FirstSnap+1)
    
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        TypeFull[snap] = read_hdf(snap_num=Snapshot, param='Type')
        RegimeFull[snap] = read_hdf(snap_num=Snapshot, param='Regime')
        GalaxyIndexFull[snap] = read_hdf(snap_num=Snapshot, param='GalaxyIndex')
    
    # Find example central galaxies in each regime (at z=0, snap=63)
    z0_snap = 63
    centrals_z0 = TypeFull[z0_snap] == 0
    
    # DIAGNOSTICS: Print halo mass statistics
    print(f"=== HALO MASS DIAGNOSTICS (z=0) ===")
    print(f"Box size: {BoxSize} Mpc/h")
    print(f"Volume: {volume:.2e} (Mpc)^3")
    print(f"Total galaxies: {len(HaloMassFull[z0_snap])}")
    print(f"Central galaxies: {np.sum(centrals_z0)}")
    central_masses = HaloMassFull[z0_snap][centrals_z0]
    print(f"Central halo mass range: {central_masses.min():.2e} - {central_masses.max():.2e} Msun")
    print(f"Number of centrals > 1e11 Msun: {np.sum(central_masses > 1e11)}")
    print(f"Number of centrals > 1e12 Msun: {np.sum(central_masses > 1e12)}")
    print(f"Number of centrals > 1e13 Msun: {np.sum(central_masses > 1e13)}")
    print()
    
    massive_z0 = HaloMassFull[z0_snap] > 1e10  # Only consider massive galaxies
    
    # CGM regime galaxy (Regime == 0)
    cgm_regime_mask = (centrals_z0) & (RegimeFull[z0_snap] == 0) & (massive_z0)
    cgm_indices = np.where(cgm_regime_mask)[0]
    
    # Hot regime galaxy (Regime == 1)
    hot_regime_mask = (centrals_z0) & (RegimeFull[z0_snap] == 1) & (massive_z0)
    hot_indices = np.where(hot_regime_mask)[0]
    
    print(f"CGM regime centrals (>1e11 Msun): {len(cgm_indices)}")
    print(f"Hot regime centrals (>1e11 Msun): {len(hot_indices)}")
    print()
    
    if len(cgm_indices) > 0 and len(hot_indices) > 0:
        # Pick three galaxies from each regime
        n_galaxies = min(7, len(cgm_indices), len(hot_indices))
        
        # Select evenly spaced galaxies from sorted arrays
        cgm_selected = [cgm_indices[i * len(cgm_indices) // (n_galaxies + 1)] for i in range(1, n_galaxies + 1)]
        hot_selected = [hot_indices[i * len(hot_indices) // (n_galaxies + 1)] for i in range(1, n_galaxies + 1)]
        
        cgm_galaxy_indices = [GalaxyIndexFull[z0_snap][idx] for idx in cgm_selected]
        hot_galaxy_indices = [GalaxyIndexFull[z0_snap][idx] for idx in hot_selected]
        
        print(f"Found {n_galaxies} CGM-regime galaxies:")
        for i, idx in enumerate(cgm_selected):
            print(f"  Galaxy {i+1}: M_halo = {HaloMassFull[z0_snap][idx]:.2e} Msun")
        
        print(f"\nFound {n_galaxies} Hot-regime galaxies:")
        for i, idx in enumerate(hot_selected):
            print(f"  Galaxy {i+1}: M_halo = {HaloMassFull[z0_snap][idx]:.2e} Msun")
        print()
        
        # Track these galaxies through time
        cgm_histories = [{'z': [], 'mass': [], 'regime': []} for _ in range(n_galaxies)]
        hot_histories = [{'z': [], 'mass': [], 'regime': []} for _ in range(n_galaxies)]
        
        for snap in range(FirstSnap, LastSnap+1):
            # Track CGM galaxies
            for i, gal_idx in enumerate(cgm_galaxy_indices):
                match = np.where(GalaxyIndexFull[snap] == gal_idx)[0]
                if len(match) > 0:
                    idx = match[0]
                    if TypeFull[snap][idx] == 0:  # Still central
                        cgm_histories[i]['z'].append(redshifts[snap])
                        cgm_histories[i]['mass'].append(HaloMassFull[snap][idx])
                        cgm_histories[i]['regime'].append(RegimeFull[snap][idx])
            
            # Track Hot galaxies
            for i, gal_idx in enumerate(hot_galaxy_indices):
                match = np.where(GalaxyIndexFull[snap] == gal_idx)[0]
                if len(match) > 0:
                    idx = match[0]
                    if TypeFull[snap][idx] == 0:  # Still central
                        hot_histories[i]['z'].append(redshifts[snap])
                        hot_histories[i]['mass'].append(HaloMassFull[snap][idx])
                        hot_histories[i]['regime'].append(RegimeFull[snap][idx])
        
        # Create the plot
        fig, ax = plt.subplots(figsize=(8.34, 6.25), dpi=96)
        
        # Set up axes to match original
        ax.set_xlim(0, 5)
        ax.set_ylim(1e9, 1e14)
        ax.set_yscale('log')
        ax.set_xlabel('Redshift', fontsize=16)
        ax.set_ylabel(r'$\mathrm{M_{vir}}$ [$\mathrm{M}_{\odot}$]', fontsize=16)
        
        # # Draw the boundary lines (just visual, not calculated)
        # # Upper diagonal dashed line (from ~(0, 10^13.5) to ~(5, 10^10))
        # z_diag1 = np.array([0, 5])
        # mass_diag1 = np.array([1e13, 1e10])
        # ax.plot(z_diag1, mass_diag1, 'k--', linewidth=1.5, zorder=1)
        
        # # Lower diagonal dashed line labeled "PS M_* 1σ" (from ~(0.5, 10^9.5) to ~(5, 10^9))
        # z_diag2 = np.array([0.5, 5])
        # mass_diag2 = np.array([1e9, 1e9])
        # ax.plot(z_diag2, mass_diag2, 'k--', linewidth=1.5, zorder=1)
        # ax.text(3.5, 1.5e9, 'PS M$_*$ 1$\\sigma$', fontsize=12)
        
        # # Label for 2σ line
        # ax.text(4.5, 1.5e10, '2$\\sigma$', fontsize=12)
        
        # Horizontal black line at ~10^12 M_sun (shock boundary)
        ax.axhline(y=6e11, color='black', linewidth=2.5, zorder=10)
        
        # Diagonal black line (from ~(1.5, 10^11) to ~(3, 10^14))
        z_mag = np.array([1.4, 3.4])
        mass_mag = np.array([6e11, 1e14])
        ax.plot(z_mag, mass_mag, color='black', linewidth=3, zorder=10)
        
        # Add text labels for regions
        ax.text(0.7, 2e12, 'hot', fontsize=18, color='red', fontweight='bold', zorder=11)
        ax.text(0.7, 3e11, 'cold', fontsize=18, color='blue', fontweight='bold', zorder=11)
        ax.text(3.2, 9e12, 'cold', fontsize=18, color='blue', fontweight='bold', zorder=11)
        ax.text(3.2, 5e12, 'in hot', fontsize=16, color='orange', fontweight='bold', zorder=11)
        ax.text(3.2, 7e11, 'shock', fontsize=16, color='black', fontweight='bold', zorder=11)
        
        # Map regime values to color names
        def get_region_name(regime_val):
            """Convert regime integer to color region name"""
            if regime_val == 0:
                return 'cold'  # CGM/cold mode
            elif regime_val == 1:
                return 'hot'   # Hot mode
            elif regime_val == 2:
                return 'cold_in_hot'  # Cold flows in hot halo
            else:
                return 'unknown'
        
        # Create colormaps for evolution
        from matplotlib.cm import Blues, Oranges, Reds
        
        # Plot the galaxy evolution tracks with region-based coloring
        print("\n=== Checking for regime transitions ===")
        for i, hist in enumerate(cgm_histories + hot_histories):
            if len(hist['z']) == 0:
                continue
            
            galaxy_type = "CGM" if i < len(cgm_histories) else "Hot"
            transitions = []
            prev_region = None
            
            # Plot segments with appropriate colors based on actual regime from simulation
            for j in range(len(hist['z']) - 1):
                z_start, z_end = hist['z'][j], hist['z'][j+1]
                mass_start, mass_end = hist['mass'][j], hist['mass'][j+1]
                regime_end = hist['regime'][j+1]
                
                # Use the actual regime from the simulation at the endpoint
                region = get_region_name(regime_end)
                
                # Track transitions
                if prev_region is not None and prev_region != region:
                    transitions.append((z_end, mass_end, prev_region, region, regime_end))
                prev_region = region
                
                # Map redshift to color intensity (higher z = darker/more saturated)
                z_mid = (z_start + z_end) / 2.0
                z_norm = z_mid / 5.0  # Normalize to [0, 1] range
                
                if region == 'cold':
                    color = Blues(0.3 + 0.7 * z_norm)  # Range from light to dark blue
                elif region == 'cold_in_hot':
                    color = Oranges(0.3 + 0.7 * z_norm)  # Range from light to dark orange
                elif region == 'hot':
                    color = Reds(0.3 + 0.7 * z_norm)  # Range from light to dark red
                else:
                    color = 'gray'  # Unknown regime
                
                ax.plot([z_start, z_end], [mass_start, mass_end], 'o-', 
                       color=color, linewidth=2, markersize=4, alpha=0.8, zorder=5)
            
            # Report transitions
            if transitions:
                print(f"\n{galaxy_type} Galaxy {i % len(cgm_histories) + 1}:")
                for z, m, old_reg, new_reg, regime_val in transitions:
                    print(f"  z={z:.2f}, M={m:.2e}: {old_reg} → {new_reg} (Regime={regime_val})")
        
        # Add legend with color patches
        from matplotlib.patches import Patch
        legend_elements = [
            Patch(facecolor='blue', label='Cold regime'),
            Patch(facecolor='orange', label='Cold-in-hot regime'),
            Patch(facecolor='red', label='Hot regime')
        ]
        # ax.legend(handles=legend_elements, loc='lower left', fontsize=12, framealpha=0.9)
        
        # ax.legend(loc='lower left', fontsize=12, framealpha=0.9)
        ax.grid(False)
        
        plt.tight_layout()
        plt.savefig(OutputDir + 'regime_evolution_plot' + OutputFormat, dpi=150)
        print(f'Saved plot to {OutputDir}regime_evolution_plot{OutputFormat}')
        plt.close()
        
        # Print some statistics about the tracked galaxies
        print("\n=== CGM-regime galaxies evolution ===")
        for i, hist in enumerate(cgm_histories):
            if len(hist['z']) > 0:
                print(f"Galaxy {i+1}:")
                print(f"  Redshift range: {min(hist['z']):.2f} - {max(hist['z']):.2f}")
                print(f"  Mass range: {min(hist['mass']):.2e} - {max(hist['mass']):.2e} Msun")
        
        print("\n=== Hot-regime galaxies evolution ===")
        for i, hist in enumerate(hot_histories):
            if len(hist['z']) > 0:
                print(f"Galaxy {i+1}:")
                print(f"  Redshift range: {min(hist['z']):.2f} - {max(hist['z']):.2f}")
                print(f"  Mass range: {min(hist['mass']):.2e} - {max(hist['mass']):.2e} Msun")
        
    else:
        print("Could not find suitable galaxies in both regimes!")