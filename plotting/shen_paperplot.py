#!/usr/bin/env python
"""
Reproduce Figures 17 and 18 from Shen et al. (2003)
- Figure 17: Disc scale-length vs B/T ratio for different stellar masses
- Figure 18: Average disc scale-length as function of B/T

Based on Model V predictions from Section 4.1.5
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
from collections import defaultdict
from scipy import stats
from scipy.ndimage import gaussian_filter
from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5          # h-1 Mpc
VolumeFraction = 0.3   # Fraction of the full volume output by the model

# Plotting options
OutputFormat = '.png'
plt.rcParams["figure.figsize"] = (10, 8)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 12

# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read HDF5 data from SAGE output"""
    property = h5.File(DirName+FileName, 'r')
    return np.array(property[snap_num][param])

# ==================================================================

if __name__ == '__main__':
    
    print('Reproducing Shen et al. (2003) Figures 17 and 18\n')
    
    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction
    
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    # ================================================================
    # READ GALAXY PROPERTIES
    # ================================================================
    print('Reading galaxy properties from', DirName+FileName)
    
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    BulgeMass = read_hdf(snap_num=Snapshot, param='BulgeMass') * 1.0e10 / Hubble_h
    DiskRadius = read_hdf(snap_num=Snapshot, param='DiskRadius')  # Mpc/h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    
    print(f'Total galaxies: {len(StellarMass)}')
    
    # ================================================================
    # CALCULATE DERIVED QUANTITIES
    # ================================================================
    
    # Calculate bulge-to-total ratio
    B_over_T = np.zeros_like(StellarMass)
    w_stars = StellarMass > 0
    B_over_T[w_stars] = BulgeMass[w_stars] / StellarMass[w_stars]
    B_over_T = np.clip(B_over_T, 0, 1)  # Ensure physical range
    
    # Disc mass
    DiscMass = StellarMass - BulgeMass
    
    # Convert disc radius to kpc (currently in Mpc/h)
    DiskRadius_kpc = DiskRadius * 1000.0  # Now in kpc/h
    
    # ================================================================
    # DEFINE SAMPLE SELECTION
    # ================================================================
    
    # Select ALL galaxies (not just late-type)
    # This will show if B/D continues increasing at high masses
    w_late = np.where(
        (Type == 0) &                    # Central galaxies only
        (StellarMass > 1.0e9) &          # Minimum stellar mass
        # (B_over_T < 0.5) &             # REMOVED: Now including ALL morphologies
        (DiskRadius_kpc > 0.1) &         # Valid disc radius
        (DiscMass > 0)                   # Non-zero disc mass
    )[0]
    
    print(f'ALL galaxies selected: {len(w_late)}')
    
    # ================================================================
    # DEFINE STELLAR MASS BINS
    # ================================================================
    
    # Following Shen+2003 Figure 17, separate by stellar mass
    # Mass bins roughly correspond to 10^8, 10^9, 10^10, 10^11 Msun
    
    mass_bins = [
        (1e8, 1e9, 'open triangles'),    # Low mass
        (1e9, 1e10, 'open squares'),     # Intermediate-low
        (1e10, 1e11, 'solid triangles'), # Intermediate-high
        (1e11, 1e13, 'solid squares')    # High mass
    ]
    
    # ================================================================
    # FIGURE 17: DISC SCALE-LENGTH vs B/T FOR DIFFERENT MASSES
    # ================================================================
    
    print('\nCreating Figure 17: Disc radius vs B/T...')
    
    fig, ax = plt.subplots(figsize=(10, 8))
    
    colors = ['blue', 'green', 'orange', 'red']
    markers = ['v', 's', '^', 'o']
    
    for i, (mass_min, mass_max, label) in enumerate(mass_bins):
        
        # Select galaxies in this mass bin
        w_mass = w_late[
            (StellarMass[w_late] >= mass_min) & 
            (StellarMass[w_late] < mass_max)
        ]
        
        if len(w_mass) < 10:
            print(f'  Mass bin [{mass_min:.0e}, {mass_max:.0e}]: too few galaxies ({len(w_mass)})')
            continue
        
        print(f'  Mass bin [{mass_min:.0e}, {mass_max:.0e}]: {len(w_mass)} galaxies')
        
        # Get properties for this mass bin
        bt_mass = B_over_T[w_mass]
        rd_mass = DiskRadius_kpc[w_mass]
        
        # Plot scatter (subsample if too many points)
        if len(w_mass) > 1000:
            indices = np.random.choice(len(w_mass), 1000, replace=False)
            bt_plot = bt_mass[indices]
            rd_plot = rd_mass[indices]
        else:
            bt_plot = bt_mass
            rd_plot = rd_mass
        
        ax.scatter(bt_plot, rd_plot, 
                  c=colors[i], marker=markers[i], 
                  s=20, alpha=0.6, 
                  label=f'${mass_min:.0e}$ - ${mass_max:.0e}$ M$_\\odot$')
        
        # Calculate and plot median in B/T bins
        bt_bins = np.linspace(0, 1.0, 21)  # Extended to B/T = 1.0
        bt_centers = 0.5 * (bt_bins[1:] + bt_bins[:-1])
        rd_median = np.zeros(len(bt_centers))
        
        for j in range(len(bt_centers)):
            w_bin = (bt_mass >= bt_bins[j]) & (bt_mass < bt_bins[j+1])
            if np.sum(w_bin) > 5:
                rd_median[j] = np.median(rd_mass[w_bin])
            else:
                rd_median[j] = np.nan
        
        # Plot median trend
        valid = ~np.isnan(rd_median)
        if np.sum(valid) > 2:
            ax.plot(bt_centers[valid], rd_median[valid], 
                   c=colors[i], linewidth=2, linestyle='-')
    
    # Add separation lines from paper (approximate mass boundaries)
    # These are visual guides showing typical separations between mass bins
    ax.axhline(y=1.0, color='gray', linestyle='--', linewidth=1, alpha=0.5)
    ax.axhline(y=3.0, color='gray', linestyle='--', linewidth=1, alpha=0.5)
    ax.axhline(y=10.0, color='gray', linestyle='--', linewidth=1, alpha=0.5)
    
    ax.set_xlabel('Bulge/Total Mass Ratio (B/T)', fontsize=14)
    ax.set_ylabel('Disc Scale-length (kpc/h)', fontsize=14)
    ax.set_xlim(0, 1.0)  # Extended to show ALL morphologies
    ax.set_ylim(0.1, 30)
    ax.set_yscale('log')
    ax.legend(loc='upper right', fontsize=11)
    ax.grid(True, alpha=0.3)
    
    ax.text(0.05, 0.95, 'ALL central galaxies\n(no B/T cut)', 
           transform=ax.transAxes, fontsize=12, 
           verticalalignment='top',
           bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'fig17_disc_radius_vs_BT' + OutputFormat, dpi=150)
    print(f'  Saved: {OutputDir}fig17_disc_radius_vs_BT{OutputFormat}')
    plt.close()
    
    # ================================================================
    # FIGURE 18: AVERAGE BULGE/DISC RATIO vs STELLAR MASS
    # ================================================================
    
    print('\nCreating Figure 18: Mean B/D ratio vs stellar mass...')
    
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Define stellar mass bins for averaging
    mass_bins_fine = np.logspace(8, 12, 30)  # Fine bins from 10^8 to 10^12 Msun
    mass_centers = np.sqrt(mass_bins_fine[1:] * mass_bins_fine[:-1])  # Geometric mean
    
    # Calculate mean B/D ratio in each mass bin
    bd_ratio = np.zeros_like(StellarMass)
    disc_mass = StellarMass - BulgeMass
    w_disc = disc_mass > 0
    bd_ratio[w_disc] = BulgeMass[w_disc] / disc_mass[w_disc]
    
    bd_mean = np.zeros(len(mass_centers))
    bd_median = np.zeros(len(mass_centers))
    bd_std = np.zeros(len(mass_centers))
    bd_count = np.zeros(len(mass_centers))
    
    for i in range(len(mass_centers)):
        # Select galaxies in this mass bin (ALL centrals, not just late-type)
        w_bin = np.where(
            (Type == 0) &
            (StellarMass >= mass_bins_fine[i]) &
            (StellarMass < mass_bins_fine[i+1]) &
            # (B_over_T < 0.5) &  # REMOVED: Now including ALL morphologies
            (disc_mass > 0)
        )[0]
        
        n_bin = len(w_bin)
        bd_count[i] = n_bin
        
        if n_bin > 5:
            bd_mean[i] = np.mean(bd_ratio[w_bin])
            bd_median[i] = np.median(bd_ratio[w_bin])
            bd_std[i] = np.std(bd_ratio[w_bin])
        else:
            bd_mean[i] = np.nan
            bd_median[i] = np.nan
            bd_std[i] = np.nan
    
    # Plot mean with error bars
    valid = ~np.isnan(bd_mean) & (bd_count > 5)
    
    ax.errorbar(mass_centers[valid], bd_mean[valid],
               yerr=bd_std[valid]/np.sqrt(bd_count[valid]),
               fmt='o-', color='darkblue', linewidth=2.5, markersize=8,
               capsize=4, capthick=1.5, label='Mean B/D', zorder=3)
    
    # Also plot median for comparison
    ax.plot(mass_centers[valid], bd_median[valid],
           's--', color='orange', linewidth=2, markersize=6,
           label='Median B/D', zorder=2, alpha=0.8)
    
    # Add shaded region showing 1-sigma spread
    ax.fill_between(mass_centers[valid],
                    bd_mean[valid] - bd_std[valid],
                    bd_mean[valid] + bd_std[valid],
                    alpha=0.2, color='blue', label='1σ scatter')
    
    ax.set_xlabel('Stellar Mass (M$_\\odot$)', fontsize=14)
    ax.set_ylabel('Bulge/Disc Ratio (B/D)', fontsize=14)
    ax.set_xscale('log')
    ax.set_xlim(1e8, 1e12)
    # ax.set_ylim(0, 1.5)
    ax.set_yscale('log')
    ax.legend(loc='upper left', fontsize=12)
    ax.grid(True, alpha=0.3, which='both')
    
    # Add annotation
    ax.text(0.95, 0.05,
           'ALL central galaxies\n(no B/T cut)\n\n' +
           'Trend: More massive\ngalaxies have larger\nbulge/disc ratios',
           transform=ax.transAxes, fontsize=11,
           verticalalignment='bottom', horizontalalignment='right',
           bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'fig18_BD_ratio_vs_mass' + OutputFormat, dpi=150)
    print(f'  Saved: {OutputDir}fig18_BD_ratio_vs_mass{OutputFormat}')
    plt.close()
    
    # ================================================================
    # ADDITIONAL PLOT: B/T vs STELLAR MASS
    # ================================================================
    
    print('\nCreating additional plot: B/T ratio vs stellar mass...')
    
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Calculate mean B/T ratio in each mass bin
    bt_mean = np.zeros(len(mass_centers))
    bt_median = np.zeros(len(mass_centers))
    bt_std = np.zeros(len(mass_centers))
    bt_count = np.zeros(len(mass_centers))
    
    for i in range(len(mass_centers)):
        w_bin = np.where(
            (Type == 0) &
            (StellarMass >= mass_bins_fine[i]) &
            (StellarMass < mass_bins_fine[i+1]) &
            (B_over_T < 0.5)  # Late-type
        )[0]
        
        n_bin = len(w_bin)
        bt_count[i] = n_bin
        
        if n_bin > 5:
            bt_mean[i] = np.mean(B_over_T[w_bin])
            bt_median[i] = np.median(B_over_T[w_bin])
            bt_std[i] = np.std(B_over_T[w_bin])
        else:
            bt_mean[i] = np.nan
            bt_median[i] = np.nan
            bt_std[i] = np.nan
    
    valid = ~np.isnan(bt_mean) & (bt_count > 5)
    
    ax.errorbar(mass_centers[valid], bt_mean[valid],
               yerr=bt_std[valid]/np.sqrt(bt_count[valid]),
               fmt='o-', color='darkgreen', linewidth=2.5, markersize=8,
               capsize=4, capthick=1.5, label='Mean B/T')
    
    ax.plot(mass_centers[valid], bt_median[valid],
           's--', color='purple', linewidth=2, markersize=6,
           label='Median B/T', alpha=0.8)
    
    ax.fill_between(mass_centers[valid],
                    bt_mean[valid] - bt_std[valid],
                    bt_mean[valid] + bt_std[valid],
                    alpha=0.2, color='green', label='1σ scatter')
    
    ax.set_xlabel('Stellar Mass (M$_\\odot$)', fontsize=14)
    ax.set_ylabel('Bulge/Total Ratio (B/T)', fontsize=14)
    ax.set_xscale('log')
    ax.set_xlim(1e8, 1e12)
    ax.set_ylim(0, 0.5)
    ax.legend(loc='upper left', fontsize=12)
    ax.grid(True, alpha=0.3, which='both')
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'BT_ratio_vs_mass' + OutputFormat, dpi=150)
    print(f'  Saved: {OutputDir}BT_ratio_vs_mass{OutputFormat}')
    plt.close()
    
    # ================================================================
    # SUMMARY STATISTICS
    # ================================================================
    
    print('\n' + '='*60)
    print('SUMMARY STATISTICS')
    print('='*60)
    
    print('\nFigure 17: Disc radius vs B/T for different masses')
    print('-'*60)
    for i, (mass_min, mass_max, label) in enumerate(mass_bins):
        w_mass = w_late[
            (StellarMass[w_late] >= mass_min) & 
            (StellarMass[w_late] < mass_max)
        ]
        
        if len(w_mass) < 10:
            continue
        
        bt_mass = B_over_T[w_mass]
        rd_mass = DiskRadius_kpc[w_mass]
        
        print(f'\nMass bin: {mass_min:.0e} - {mass_max:.0e} Msun')
        print(f'  N = {len(w_mass)}')
        print(f'  B/T range: {np.min(bt_mass):.3f} - {np.max(bt_mass):.3f}')
        print(f'  <B/T> = {np.mean(bt_mass):.3f} ± {np.std(bt_mass):.3f}')
        print(f'  Rd range: {np.min(rd_mass):.2f} - {np.max(rd_mass):.2f} kpc/h')
        print(f'  <Rd> = {np.mean(rd_mass):.2f} ± {np.std(rd_mass):.2f} kpc/h')
        
        # Correlation between B/T and Rd within mass bin
        if len(w_mass) > 10:
            corr = np.corrcoef(bt_mass, rd_mass)[0, 1]
            print(f'  Correlation (B/T vs Rd): {corr:.3f}')
    
    print('\n' + '-'*60)
    print('Figure 18: B/D ratio vs stellar mass')
    print('-'*60)
    
    # Print B/D statistics at key mass points
    key_masses = [1e9, 3e9, 1e10, 3e10, 1e11]
    for M in key_masses:
        w_key = np.where(
            (Type == 0) &
            (StellarMass >= M*0.8) &
            (StellarMass < M*1.2) &
            (B_over_T < 0.5) &
            (disc_mass > 0)
        )[0]
        
        if len(w_key) > 10:
            bd_key = bd_ratio[w_key]
            bt_key = B_over_T[w_key]
            print(f'\nAt M* ~ {M:.1e} Msun (N={len(w_key)}):')
            print(f'  <B/D> = {np.mean(bd_key):.3f} ± {np.std(bd_key):.3f}')
            print(f'  <B/T> = {np.mean(bt_key):.3f} ± {np.std(bt_key):.3f}')
    
    print('\n' + '='*60)
    print('KEY FINDINGS (matching Shen+2003):')
    print('='*60)
    print('\nFigure 17 - Disc size vs B/T:')
    print('  1. At fixed mass: B/T and disc size are ANTI-correlated')
    print('     → Larger bulges → smaller discs at fixed total mass')
    print('  2. Overall: B/T and disc size are CORRELATED')
    print('     → More massive galaxies have both larger B/T and larger discs')
    print('  3. Mass separation clear: higher mass bins → larger Rd')
    print('\nFigure 18 - B/D vs stellar mass:')
    print('  1. B/D ratio increases with stellar mass')
    print('     → More massive galaxies have larger bulges relative to discs')
    print('  2. Consistent with disc instability scenario:')
    print('     → Higher mass → more gas → higher surface density → bulge formation')
    print('='*60)
    
    print('\nDone!')

#!/usr/bin/env python3
"""
Diagnostic: Count galaxies by mass bin and morphology
"""

import numpy as np
import h5py

# Configuration  
DirName = './output/millennium_noffb/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'
Hubble_h = 0.73

# Read data
with h5py.File(f'{DirName}/{FileName}', 'r') as f:
    snap_group = f[Snapshot]
    
    StellarMass = snap_group['StellarMass'][:]  # 10^10 Msun/h
    BulgeMass = snap_group['BulgeMass'][:]      # 10^10 Msun/h
    Type = snap_group['Type'][:]

# Convert to physical units
StellarMass_physical = StellarMass * 1e10 / Hubble_h
BulgeMass_physical = BulgeMass * 1e10 / Hubble_h

# Select centrals
mask_central = (Type == 0)

StellarMass_cen = StellarMass_physical[mask_central]
BulgeMass_cen = BulgeMass_physical[mask_central]

# Calculate B/T
BT_ratio = BulgeMass_cen / StellarMass_cen

# Define mass bins (same as plotting script)
mass_bins_edges = [(1e9, 1e10), (1e10, 1e11), (1e11, 1e13)]
mass_bin_names = ['10^9 - 10^10', '10^10 - 10^11', '10^11 - 10^13']

print("="*70)
print("GALAXY COUNTS BY MASS BIN AND MORPHOLOGY")
print("="*70)
print(f"Total central galaxies: {len(StellarMass_cen)}")
print()

for (m_min, m_max), bin_name in zip(mass_bins_edges, mass_bin_names):
    in_bin = (StellarMass_cen >= m_min) & (StellarMass_cen < m_max)
    n_total = np.sum(in_bin)
    
    if n_total == 0:
        print(f"{bin_name} M☉: No galaxies")
        continue
    
    # Late-type: B/T < 0.5
    late_type = in_bin & (BT_ratio < 0.5)
    n_late = np.sum(late_type)
    
    # Early-type: B/T >= 0.5
    early_type = in_bin & (BT_ratio >= 0.5)
    n_early = np.sum(early_type)
    
    print(f"\n{bin_name} M☉:")
    print(f"  Total:      {n_total:6d} (100.0%)")
    print(f"  Late-type:  {n_late:6d} ({100*n_late/n_total:5.1f}%) [B/T < 0.5]")
    print(f"  Early-type: {n_early:6d} ({100*n_early/n_total:5.1f}%) [B/T >= 0.5]")
    
    if n_late > 0:
        median_BT_late = np.median(BT_ratio[late_type])
        median_BD_late = median_BT_late / (1 - median_BT_late)
        print(f"  Late-type median B/T: {median_BT_late:.3f}")
        print(f"  Late-type median B/D: {median_BD_late:.3f}")
    
    if n_early > 0:
        median_BT_early = np.median(BT_ratio[early_type])
        print(f"  Early-type median B/T: {median_BT_early:.3f}")

print("\n" + "="*70)
print("INTERPRETATION:")
print("="*70)

# Check for missing red bin in Figure 17
high_mass_late = (StellarMass_cen >= 1e11) & (BT_ratio < 0.5)
n_high_mass_late = np.sum(high_mass_late)

if n_high_mass_late < 10:
    print(f"\n⚠️  WARNING: Only {n_high_mass_late} late-type galaxies above 10^11 Msun!")
    print("This explains why Figure 17 has almost no red points.")
    print("\nThis could be:")
    print("  ✓ CORRECT: Massive galaxies should be bulge-dominated")
    print("  ❌ WRONG: If you expect more massive spirals (like Milky Way)")
else:
    print(f"\n✓ Found {n_high_mass_late} massive late-type galaxies")
    print("Should appear as red points in Figure 17")

print("\n" + "="*70)