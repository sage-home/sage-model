#!/usr/bin/env python3
"""
Analyze CGM vs Hot-ICM regime transition using real SAGE data
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import matplotlib.patches as mpatches
import os

# ========================== USER OPTIONS ==========================

# File details (MODIFY THESE TO MATCH YOUR SETUP)
DirName = './output/millennium/'
OutputDir = DirName + 'plots/'
OutputFormat = '.png'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'  # z=0 for Millennium

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc

# Snapshot redshift (for z-dependent threshold)
# For Millennium Snap_63 is z=0, adjust if using different snapshot
snapshot_redshift = 0.0

# ==================================================================

def read_hdf(snap_num, param):
    """Read HDF5 data from SAGE output"""
    try:
        with h5.File(DirName + FileName, 'r') as f:
            return np.array(f[snap_num][param])
    except Exception as e:
        print(f"Error reading {param}: {e}")
        return None

def calc_virial_temp(Vvir):
    """Calculate virial temperature from circular velocity"""
    return 35.9 * Vvir**2

def calc_threshold_temp(z):
    """Calculate the regime threshold temperature at redshift z"""
    Tvir_threshold_z0 = 8.0e5
    z_scaling = (1.0 + z)**0.35
    return Tvir_threshold_z0 * z_scaling

def calc_critical_vvir(z):
    """Calculate the critical Vvir at the regime transition"""
    Tvir_threshold = calc_threshold_temp(z)
    return np.sqrt(Tvir_threshold / 35.9)

def calc_approx_mvir(Vvir, z=0):
    """Rough approximation of virial mass"""
    H_z = 1.0 * (1 + z)**1.5
    return 1.5e-4 * Vvir**3 / H_z

def calc_sharp_regime(Tvir, Tvir_threshold):
    """Sharp transition (current implementation)"""
    return np.where(Tvir < Tvir_threshold, 0, 1)

def calc_smooth_regime_weight(Tvir, Tvir_threshold, transition_width=0.5):
    """Smooth transition (commented out in code)"""
    log_ratio = np.log10(Tvir / Tvir_threshold)
    smooth_weight = 0.5 * (1.0 + np.tanh(log_ratio / transition_width))
    return smooth_weight

# ==================================================================

if __name__ == '__main__':
    
    print("="*70)
    print("REGIME TRANSITION ANALYSIS WITH REAL SAGE DATA")
    print("="*70)
    
    # Check if file exists
    if not os.path.exists(DirName + FileName):
        print(f"\nERROR: File not found: {DirName + FileName}")
        print("Please update DirName and FileName in the script to match your setup.\n")
        exit(1)
    
    print(f"\nReading data from: {DirName + FileName}")
    print(f"Snapshot: {Snapshot} (z={snapshot_redshift})\n")
    
    # Read galaxy properties
    print("Reading galaxy properties...")
    Vvir = read_hdf(Snapshot, 'Vvir')
    Mvir = read_hdf(Snapshot, 'Mvir') * 1.0e10 / Hubble_h  # Convert to Msun
    StellarMass = read_hdf(Snapshot, 'StellarMass') * 1.0e10 / Hubble_h
    Regime = read_hdf(Snapshot, 'Regime')
    
    print(f"Total galaxies read: {len(Vvir):,}\n")
    
    # Calculate virial temperature
    Tvir = calc_virial_temp(Vvir)
    
    # Calculate threshold
    Tvir_threshold = calc_threshold_temp(snapshot_redshift)
    Vvir_threshold = calc_critical_vvir(snapshot_redshift)
    
    # ================================================================
    # CREATE THE EXACT SAME FIGURE AS ORIGINAL
    # ================================================================
    
    fig = plt.figure(figsize=(18, 12))
    
    # ================================================================
    # Panel 1: Tvir vs Vvir with regime boundaries at different redshifts
    # (Uses real data overlaid on theory)
    # ================================================================
    ax1 = plt.subplot(2, 3, 1)
    
    # Sample data for plotting
    sample_size = min(10000, len(Vvir))
    sample_idx = np.random.choice(len(Vvir), sample_size, replace=False)
    
    Vvir_range = np.logspace(1.5, 2.8, 300)
    redshifts = [0.0, 0.5, 1.0, 2.0, 4.0]
    colors = plt.cm.viridis(np.linspace(0, 1, len(redshifts)))
    
    for z, color in zip(redshifts, colors):
        Tvir_theory = calc_virial_temp(Vvir_range)
        ax1.loglog(Vvir_range, Tvir_theory, 'k-', alpha=0.3, linewidth=1)
        
        Vvir_crit = calc_critical_vvir(z)
        Tvir_threshold_z = calc_threshold_temp(z)
        ax1.plot(Vvir_crit, Tvir_threshold_z, 'o', color=color, markersize=10,
                 label=f'z={z:.1f}: Vvir={Vvir_crit:.1f} km/s')
        ax1.axhline(Tvir_threshold_z, color=color, linestyle='--', alpha=0.5, linewidth=1)
    
    # Overlay real data points
    cgm_mask = (Regime == 0)
    hot_mask = (Regime == 1)
    ax1.scatter(Vvir[sample_idx][cgm_mask[sample_idx]], Tvir[sample_idx][cgm_mask[sample_idx]],
                c='blue', s=0.5, alpha=0.2, rasterized=True)
    ax1.scatter(Vvir[sample_idx][hot_mask[sample_idx]], Tvir[sample_idx][hot_mask[sample_idx]],
                c='red', s=0.5, alpha=0.2, rasterized=True)
    
    ax1.axhline(8e5, color='red', linestyle=':', linewidth=2, alpha=0.7, 
                label='z=0 threshold (8×10⁵ K)')
    ax1.fill_between([30, 1000], [1e4, 1e4], [8e5, 8e5], 
                      alpha=0.1, color='blue', label='CGM Regime')
    ax1.fill_between([30, 1000], [8e5, 8e5], [1e8, 1e8], 
                      alpha=0.1, color='red', label='Hot-ICM Regime')
    
    ax1.set_xlabel('Virial Velocity (km/s)', fontsize=12)
    ax1.set_ylabel('Virial Temperature (K)', fontsize=12)
    ax1.set_title('Regime Boundaries at Different Redshifts', fontsize=13, fontweight='bold')
    ax1.legend(loc='upper left', fontsize=9)
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(30, 600)
    ax1.set_ylim(1e4, 1e8)
    
    # ================================================================
    # Panel 2: Critical Vvir vs Redshift (pure theory)
    # ================================================================
    ax2 = plt.subplot(2, 3, 2)
    
    z_range = np.linspace(0, 6, 100)
    Vvir_crit_array = calc_critical_vvir(z_range)
    
    ax2.plot(z_range, Vvir_crit_array, 'b-', linewidth=2.5)
    ax2.fill_between(z_range, 0, Vvir_crit_array, alpha=0.2, color='blue', 
                      label='CGM-dominated')
    ax2.fill_between(z_range, Vvir_crit_array, 400, alpha=0.2, color='red',
                      label='Hot-ICM-dominated')
    
    # Mark characteristic values
    for z_mark in [0, 1, 2, 4]:
        Vvir_mark = calc_critical_vvir(z_mark)
        ax2.plot(z_mark, Vvir_mark, 'ko', markersize=8)
        ax2.text(z_mark + 0.1, Vvir_mark + 5, f'z={z_mark}\n{Vvir_mark:.0f} km/s',
                 fontsize=9, verticalalignment='bottom')
    
    ax2.set_xlabel('Redshift', fontsize=12)
    ax2.set_ylabel('Critical Vvir (km/s)', fontsize=12)
    ax2.set_title('Regime Transition Velocity vs Redshift', fontsize=13, fontweight='bold')
    ax2.legend(loc='upper right', fontsize=10)
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(0, 6)
    ax2.set_ylim(100, 350)
    
    # ================================================================
    # Panel 3: Sharp vs Smooth Transition (theory)
    # ================================================================
    ax3 = plt.subplot(2, 3, 3)
    
    z_test = 0.0
    Tvir_test = np.logspace(5, 6.5, 300)
    Tvir_threshold_test = calc_threshold_temp(z_test)
    
    # Sharp transition (current)
    regime_sharp = calc_sharp_regime(Tvir_test, Tvir_threshold_test)
    
    # Smooth transitions
    regime_smooth_05 = calc_smooth_regime_weight(Tvir_test, Tvir_threshold_test, 0.5)
    regime_smooth_02 = calc_smooth_regime_weight(Tvir_test, Tvir_threshold_test, 0.2)
    
    ax3.semilogx(Tvir_test, regime_sharp, 'k-', linewidth=2.5, 
                 label='Sharp (current)', drawstyle='steps-post')
    ax3.semilogx(Tvir_test, regime_smooth_05, 'b--', linewidth=2,
                 label='Smooth (width=0.5 dex)')
    ax3.semilogx(Tvir_test, regime_smooth_02, 'r:', linewidth=2,
                 label='Smooth (width=0.2 dex)')
    
    ax3.axvline(Tvir_threshold_test, color='gray', linestyle='--', alpha=0.7,
                label=f'Threshold ({Tvir_threshold_test:.1e} K)')
    ax3.axhline(0.5, color='gray', linestyle=':', alpha=0.5)
    
    ax3.set_xlabel('Virial Temperature (K)', fontsize=12)
    ax3.set_ylabel('Regime Weight (0=CGM, 1=Hot-ICM)', fontsize=12)
    ax3.set_title('Sharp vs Smooth Transition Functions', fontsize=13, fontweight='bold')
    ax3.legend(loc='right', fontsize=10)
    ax3.grid(True, alpha=0.3)
    ax3.set_xlim(1e5, 3e6)
    ax3.set_ylim(-0.1, 1.1)
    
    # ================================================================
    # Panel 4: Halo Mass vs Vvir (uses REAL data!)
    # ================================================================
    ax4 = plt.subplot(2, 3, 4)
    
    # Bin the data
    vvir_bins = np.linspace(50, 500, 30)
    vvir_centers = (vvir_bins[:-1] + vvir_bins[1:]) / 2
    
    # Calculate median Mvir in each bin
    mvir_medians = []
    mvir_p16 = []
    mvir_p84 = []
    
    for i in range(len(vvir_bins)-1):
        mask = (Vvir >= vvir_bins[i]) & (Vvir < vvir_bins[i+1]) & (Mvir > 0)
        if mask.sum() > 10:
            mvir_medians.append(np.median(Mvir[mask]))
            mvir_p16.append(np.percentile(Mvir[mask], 16))
            mvir_p84.append(np.percentile(Mvir[mask], 84))
        else:
            mvir_medians.append(np.nan)
            mvir_p16.append(np.nan)
            mvir_p84.append(np.nan)
    
    mvir_medians = np.array(mvir_medians)
    mvir_p16 = np.array(mvir_p16)
    mvir_p84 = np.array(mvir_p84)
    
    # Plot real data
    valid = ~np.isnan(mvir_medians)
    ax4.semilogy(vvir_centers[valid], mvir_medians[valid], 'ko-', linewidth=2, 
                 markersize=4, label=f'Real data (z={snapshot_redshift})')
    ax4.fill_between(vvir_centers[valid], mvir_p16[valid], mvir_p84[valid],
                      alpha=0.3, color='gray', label='16-84 percentile')
    
    # Overlay theory lines
    Vvir_range_mass = np.linspace(50, 500, 100)
    for z, color in zip([0.0, 1.0, 2.0], ['blue', 'green', 'red']):
        Mvir_approx = calc_approx_mvir(Vvir_range_mass, z)
        ax4.semilogy(Vvir_range_mass, Mvir_approx, '--', color=color, linewidth=1,
                     alpha=0.5, label=f'Theory z={z:.1f}')
        
        Vvir_crit = calc_critical_vvir(z)
        Mvir_crit = calc_approx_mvir(Vvir_crit, z)
        ax4.plot(Vvir_crit, Mvir_crit, 'o', color=color, markersize=8)
        ax4.axvline(Vvir_crit, color=color, linestyle=':', alpha=0.3)
    
    ax4.set_xlabel('Virial Velocity (km/s)', fontsize=12)
    ax4.set_ylabel('Mvir (M☉)', fontsize=12)
    ax4.set_title('Halo Mass at Regime Transition', fontsize=13, fontweight='bold')
    ax4.legend(loc='upper left', fontsize=9)
    ax4.grid(True, alpha=0.3)
    ax4.set_xlim(50, 500)
    ax4.set_ylim(1e10, 1e15)
    
    # Add reference lines
    ax4.axhline(1e11, color='gray', linestyle=':', alpha=0.5)
    ax4.axhline(1e12, color='gray', linestyle=':', alpha=0.5)
    ax4.axhline(1e13, color='gray', linestyle=':', alpha=0.5)
    
    # ================================================================
    # Panel 5: Temperature Evolution with Redshift (theory)
    # ================================================================
    ax5 = plt.subplot(2, 3, 5)
    
    z_range_temp = np.linspace(0, 4, 100)
    Tvir_threshold_evolution = calc_threshold_temp(z_range_temp)
    
    ax5.semilogy(z_range_temp, Tvir_threshold_evolution, 'r-', linewidth=3,
                 label='Threshold: 8×10⁵(1+z)^0.35 K')
    ax5.axhline(8e5, color='gray', linestyle='--', alpha=0.5, label='z=0 baseline')
    
    ax5.fill_between(z_range_temp, 8e5, Tvir_threshold_evolution,
                      alpha=0.2, color='orange')
    
    ax5.set_xlabel('Redshift', fontsize=12)
    ax5.set_ylabel('Threshold Temperature (K)', fontsize=12)
    ax5.set_title('Redshift Evolution of Regime Boundary', fontsize=13, fontweight='bold')
    ax5.legend(loc='upper left', fontsize=10)
    ax5.grid(True, alpha=0.3)
    ax5.set_xlim(0, 4)
    ax5.set_ylim(7e5, 1.5e6)
    
    # Add text annotation
    evolution_factor = calc_threshold_temp(4) / calc_threshold_temp(0)
    ax5.text(0.5, 0.95, f'z=0→4: {evolution_factor:.2f}× increase\n(35% power law)',
             transform=ax5.transAxes, fontsize=10, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    # ================================================================
    # Panel 6: Distribution context (REAL DATA!)
    # ================================================================
    ax6 = plt.subplot(2, 3, 6)
    
    # Use all data for histogram
    Vvir_crit_sample = calc_critical_vvir(snapshot_redshift)
    
    # Separate by regime
    cgm_mask = (Regime == 0) & (Vvir > 30) & (Vvir < 500)
    hot_mask = (Regime == 1) & (Vvir > 30) & (Vvir < 500)
    
    ax6.hist(Vvir[cgm_mask], bins=50, alpha=0.6, color='blue', 
             label=f'CGM Regime (n={cgm_mask.sum():,})', density=True)
    ax6.hist(Vvir[hot_mask], bins=50, alpha=0.6, color='red',
             label=f'Hot-ICM Regime (n={hot_mask.sum():,})', density=True)
    
    ax6.axvline(Vvir_crit_sample, color='black', linestyle='--', linewidth=2,
                label=f'Transition: {Vvir_crit_sample:.1f} km/s')
    
    ax6.set_xlabel('Virial Velocity (km/s)', fontsize=12)
    ax6.set_ylabel('Normalized Frequency', fontsize=12)
    ax6.set_title(f'Real Galaxy Distribution (z={snapshot_redshift:.1f})', fontsize=13, fontweight='bold')
    ax6.legend(loc='upper right', fontsize=10)
    ax6.grid(True, alpha=0.3, axis='y')
    ax6.set_xlim(30, 500)
    
    # Add percentage annotation
    total_valid = cgm_mask.sum() + hot_mask.sum()
    pct_cgm = 100 * cgm_mask.sum() / total_valid if total_valid > 0 else 0
    ax6.text(0.05, 0.95, f'{pct_cgm:.1f}% in CGM regime\n{100-pct_cgm:.1f}% in Hot-ICM regime',
             transform=ax6.transAxes, fontsize=10, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))
    
    plt.tight_layout()

    outputFile = OutputDir + 'regime_transition_analysis' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()
    
    print("\n" + "="*70)
    print("SUCCESS: Figure saved to regime_transition_analysis.png")
    print("="*70 + "\n")
    
    # ================================================================
    # Print diagnostic information
    # ================================================================
    print("\n" + "="*70)
    print("REGIME TRANSITION ANALYSIS")
    print("="*70)
    
    print("\n1. THRESHOLD FORMULA:")
    print("   Tvir_threshold = 8.0×10⁵ × (1+z)^0.35 K")
    print("   Vvir_critical = sqrt(Tvir_threshold / 35.9) km/s")
    
    print("\n2. CRITICAL VALUES AT KEY REDSHIFTS:")
    print("   " + "-"*60)
    print(f"   {'z':<8} {'Tvir_threshold':<18} {'Vvir_critical':<18} {'~Mvir (M☉)'}")
    print("   " + "-"*60)
    for z in [0.0, 0.5, 1.0, 2.0, 4.0, 6.0]:
        Tvir_th = calc_threshold_temp(z)
        Vvir_cr = calc_critical_vvir(z)
        Mvir_approx = calc_approx_mvir(Vvir_cr, z)
        print(f"   {z:<8.1f} {Tvir_th:<18.2e} {Vvir_cr:<18.1f} {Mvir_approx*1e10:<18.2e}")
    
    print("\n3. REAL DATA STATISTICS:")
    print(f"   Total galaxies: {len(Vvir):,}")
    print(f"   CGM Regime (0): {(Regime==0).sum():,} ({100*(Regime==0).sum()/len(Regime):.1f}%)")
    print(f"   Hot Regime (1): {(Regime==1).sum():,} ({100*(Regime==1).sum()/len(Regime):.1f}%)")
    
    print("\n4. VVIR STATISTICS BY REGIME:")
    print(f"   CGM: median={np.median(Vvir[Regime==0]):.1f} km/s, mean={np.mean(Vvir[Regime==0]):.1f} km/s")
    print(f"   Hot: median={np.median(Vvir[Regime==1]):.1f} km/s, mean={np.mean(Vvir[Regime==1]):.1f} km/s")
    
    print("\n5. MVIR STATISTICS BY REGIME:")
    print(f"   CGM: median={np.median(Mvir[Regime==0]):.2e} M☉")
    print(f"   Hot: median={np.median(Mvir[Regime==1]):.2e} M☉")
    
    print("\n" + "="*70)
    print("Analysis complete!")
    print("="*70)