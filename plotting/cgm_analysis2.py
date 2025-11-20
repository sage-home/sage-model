#!/usr/bin/env python
"""
CGM Analysis Script for SAGE - Modified
Generates comprehensive CGM section figures with CGM and Hot gas separated

Figures produced:
1a. CGM Mass vs Halo Mass (CGMgas only, z=0 and z=2)
1b. Hot Gas Mass vs Halo Mass (HotGas only, z=0 and z=2)
2. CGM+Hot Mass Function (combined, multiple redshifts)
3a. CGM Metallicity (CGMgas only, z=0 and z=2)
3b. Hot Gas Metallicity (HotGas only, z=0 and z=2)
4a. CGM-Halo Mass Relation evolution (CGMgas only, multiple redshifts)
4b. Hot-Halo Mass Relation evolution (HotGas only, multiple redshifts)
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import LogNorm, Normalize, to_rgba
from matplotlib.patches import Rectangle
import os
from scipy import stats, interpolate
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
BaryonFrac = 0.17  # Match your parameter file

FirstSnap = 0
LastSnap = 63

redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 
             11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 
             4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 2.831, 2.619, 2.422, 2.239, 
             2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 
             0.242, 0.208, 0.175, 0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

# Plotting options
dilute = 30000
OutputFormat = '.png'

# Plot styling
plt.rcParams["figure.figsize"] = (12, 10)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 12
plt.rcParams["font.family"] = "serif"

# Colors
COLOR_CGM = 'cornflowerblue'
COLOR_HOT = 'firebrick'

# ==================================================================

def read_hdf(snap_num=None, param=None):
    """Read parameter from HDF5 file"""
    property = h5.File(DirName + FileName, 'r')
    return np.array(property[snap_num][param])


def calculate_median_relation(x, y, x_bins, percentiles=[16, 50, 84]):
    """Calculate median and percentiles in bins"""
    x_centers = (x_bins[:-1] + x_bins[1:]) / 2
    medians = np.zeros(len(x_centers))
    lower = np.zeros(len(x_centers))
    upper = np.zeros(len(x_centers))
    
    for i, (x_low, x_high) in enumerate(zip(x_bins[:-1], x_bins[1:])):
        mask = (x >= x_low) & (x < x_high) & (y > 0)
        if np.sum(mask) > 10:
            values = y[mask]
            medians[i] = np.percentile(values, percentiles[1])
            lower[i] = np.percentile(values, percentiles[0])
            upper[i] = np.percentile(values, percentiles[2])
        else:
            medians[i] = np.nan
            lower[i] = np.nan
            upper[i] = np.nan
    
    return x_centers, medians, lower, upper


def calculate_number_density(masses, mass_bins, volume):
    """
    Calculate number density (dN/dlog10M) in bins
    
    Parameters:
    -----------
    masses : array
        Mass values in M_sun
    mass_bins : array
        Bin edges for masses
    volume : float
        Survey volume in (Mpc/h)^3
        
    Returns:
    --------
    mass_centers, number_density, poisson_error
    """
    hist, _ = np.histogram(np.log10(masses), bins=np.log10(mass_bins))
    
    # dN/dlog10M
    dlog10M = np.diff(np.log10(mass_bins))
    number_density = hist / (volume * dlog10M)
    
    # Poisson error
    poisson_error = np.sqrt(hist) / (volume * dlog10M)
    
    mass_centers = 10**((np.log10(mass_bins[:-1]) + np.log10(mass_bins[1:])) / 2)
    
    return mass_centers, number_density, poisson_error


def plot_fig1a_cgm_mass(snaps=[63, 32]):
    """
    Figure 1a: CGM Mass vs Halo Mass (CGMgas only)
    4 panels (2x2):
    Top row (z=0): M_CGM vs M_vir | f_CGM vs M_vir
    Bottom row (z=2): M_CGM vs M_vir | f_CGM vs M_vir
    """
    print("\nCreating Figure 1a: CGM Mass and Fraction vs Halo Mass...")

    import pandas as pd
    csv_data = pd.read_csv('../../M_and_M.csv')  # or wherever your CSV is
    M2006 = 10**csv_data['M2006'].values  # Convert from log to linear
    M_CGM1 = 10**csv_data['M*CGM1'].values  # Convert from log to linear
    M_CGM2 = 10**csv_data['M*CGM2'].values  # Convert from log to linear
    M_CGM3 = 10**csv_data['M*CGM3'].values  # Convert from log to linear
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    
    for row_idx, snap in enumerate(snaps):
        # ========== LEFT PANEL: M_CGM vs M_vir ==========
        ax = axes[row_idx, 0]
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        Type = read_hdf(snap_num=Snapshot, param='Type')
        
        # Select centrals
        mask = (Type == 0) & (Mvir > 1e10) & (CGMgas > 1e7)
        
        # Create contours
        if np.sum(mask) > 100:
            x_data = np.log10(Mvir[mask])
            y_data = np.log10(CGMgas[mask])
            
            H, xedges, yedges = np.histogram2d(x_data, y_data, bins=50, 
                                                range=[[10, 15], [7, 13]])
            H = H.T
            X, Y = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels = np.percentile(H[H > 0], [39, 86, 99])
            levels = np.unique(levels)
            if len(levels) > 1:
                n_regions = len(levels) - 1
                alphas = np.linspace(0.15, 0.45, n_regions)
                rgb = to_rgba(COLOR_CGM)[:3]
                colors = [(*rgb, alpha) for alpha in alphas]
                ax.contour(10**X, 10**Y, H, levels=levels,
                          colors=COLOR_CGM, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X, 10**Y, H, levels=levels, colors=colors)
        
        # Add median line with error bars
        mass_bins = np.logspace(10, 15, 25)
        
        if np.sum(mask) > 50:
            x_c, med, low, upp = calculate_median_relation(
                np.log10(Mvir[mask]),
                np.log10(CGMgas[mask]),
                np.log10(mass_bins)
            )
            valid = ~np.isnan(med)
            ax.plot(10**x_c[valid], 10**med[valid], color='black',
                   lw=2.5, label='Median', zorder=5)
            # Add error bars with caps
            ax.errorbar(10**x_c[valid], 10**med[valid], 
                       yerr=[10**med[valid] - 10**low[valid], 10**upp[valid] - 10**med[valid]],
                       fmt='none', ecolor='black', capsize=3, capthick=1.5, alpha=0.7, zorder=4)
            
        if row_idx == 0:
            ax.scatter(M2006, M_CGM2, s=30, color='purple', 
                      alpha=0.6, edgecolors='black', linewidths=0.5,
                      label='Marvelous Metals', zorder=8)
        
        # Add M_shock
        M_shock = 6e11
        ax.axvline(M_shock, color='black', ls='--', lw=2,
                  label=r'$M_{\rm shock}$', zorder=6)
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        if row_idx == 1:
            ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=13)
        ax.set_ylabel(r'$M_{\rm CGM}$ [M$_\odot$]', fontsize=13)
        ax.set_xlim(1e10, 1e13)
        ax.set_ylim(1e7, 1e12)
        
        # Update legend position for z=0 to accommodate observations
        if row_idx == 0:
            ax.legend(loc='upper left', fontsize=8, framealpha=0.9, ncol=2)
        else:
            ax.legend(loc='upper left', fontsize=9, framealpha=0.9)
            
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
               ha='right', va='bottom', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        
        # ========== RIGHT PANEL: f_CGM vs M_vir ==========
        ax = axes[row_idx, 1]
        
        # Calculate f_CGM = M_CGM / (f_b * M_vir)
        f_CGM = np.zeros_like(CGMgas)
        mask_mvir = Mvir > 0
        f_CGM[mask_mvir] = CGMgas[mask_mvir] / (BaryonFrac * Mvir[mask_mvir])
        
        # Select data
        mask_f = (Type == 0) & (Mvir > 1e10) & (f_CGM > 1e-4)
        
        # Create contours
        if np.sum(mask_f) > 100:
            x_data_f = np.log10(Mvir[mask_f])
            y_data_f = np.log10(f_CGM[mask_f])
            
            H_f, xedges_f, yedges_f = np.histogram2d(x_data_f, y_data_f, bins=50,
                                                      range=[[10, 15], [-3, 0.5]])
            H_f = H_f.T
            X_f, Y_f = np.meshgrid(xedges_f[:-1], yedges_f[:-1])
            
            levels_f = np.percentile(H_f[H_f > 0], [39, 86, 99])
            levels_f = np.unique(levels_f)
            if len(levels_f) > 1:
                n_regions = len(levels_f) - 1
                alphas = np.linspace(0.15, 0.45, n_regions)
                rgb = to_rgba(COLOR_CGM)[:3]
                colors = [(*rgb, alpha) for alpha in alphas]
                ax.contour(10**X_f, 10**Y_f, H_f, levels=levels_f,
                          colors=COLOR_CGM, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_f, 10**Y_f, H_f, levels=levels_f, colors=colors)
        
        # Add median line with error bars
        if np.sum(mask_f) > 50:
            x_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            medians = np.zeros(len(x_centers))
            lower_percentile = np.zeros(len(x_centers))
            upper_percentile = np.zeros(len(x_centers))
            
            for i, (m_low, m_high) in enumerate(zip(mass_bins[:-1], mass_bins[1:])):
                mask_bin = mask_f & (Mvir >= m_low) & (Mvir < m_high)
                if np.sum(mask_bin) > 10:
                    values = f_CGM[mask_bin]
                    medians[i] = np.percentile(values, 50)
                    lower_percentile[i] = np.percentile(values, 16)
                    upper_percentile[i] = np.percentile(values, 84)
                else:
                    medians[i] = np.nan
                    lower_percentile[i] = np.nan
                    upper_percentile[i] = np.nan
            
            valid = ~np.isnan(medians) & (medians > 0)
            ax.plot(x_centers[valid], medians[valid], color='black',
                   lw=2.5, label='Median', zorder=5)
            # Add error bars with caps
            ax.errorbar(x_centers[valid], medians[valid],
                       yerr=[medians[valid] - lower_percentile[valid], 
                             upper_percentile[valid] - medians[valid]],
                       fmt='none', ecolor='black', capsize=3, capthick=1.5, alpha=0.7, zorder=4)
        
        # Add FIRE data points
        fire_log_mvir = np.array([9.88, 10.14, 10.58, 10.60, 10.57, 10.85, 10.99, 
                                  11.12, 11.15, 11.17, 11.25, 11.38, 11.83, 11.84, 
                                  11.90, 11.95, 12.01, 12.10, 12.12, 12.18, 10.41])
        fire_f_cgm = np.array([0.06, 0.10, 0.24, 0.19, 0.16, 0.14, 0.12, 
                               0.23, 0.22, 0.27, 0.14, 0.25, 0.35, 0.32, 
                               0.23, 0.20, 0.29, 0.24, 0.29, 0.35, 0.03])
        fire_mvir = 10**fire_log_mvir

        # FIRE data for z=2
        fire_log_mvir_z2 = np.array([9.50, 9.57, 10.02, 10.11, 10.19, 10.32, 10.53, 
                                    10.64, 10.65, 10.67, 10.73, 10.95, 11.06, 11.15, 
                                    11.25, 11.36, 11.46, 11.66, 11.68, 11.73])
        fire_f_cgm_z2 = np.array([0.43, 0.09, 0.16, 0.28, 0.22, 0.30, 0.50, 
                                0.27, 0.28, 0.24, 0.48, 0.39, 0.50, 0.38, 
                                0.21, 0.36, 0.34, 0.31, 0.41, 0.50])
        fire_mvir_z2 = 10**fire_log_mvir_z2
        
        if row_idx == 0:
            ax.scatter(fire_mvir, fire_f_cgm, s=80, marker='*', 
                    color='darkred', linewidths=1.5,
                    label='FIRE', zorder=7, alpha=0.9)
            
        if row_idx == 1:
            ax.scatter(fire_mvir_z2, fire_f_cgm_z2, s=80, marker='*', 
                color='darkred', linewidths=1.5,
                label='FIRE', zorder=7, alpha=0.9)
        
        # Add M_shock
        ax.axvline(M_shock, color='black', ls='--', lw=2, zorder=6)
        
        # Add reference line
        ax.axhline(1.0, color='gray', ls=':', lw=1.5, alpha=0.7, label=r'$f_{\rm CGM} = 1$')
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        if row_idx == 1:
            ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=13)
        ax.set_ylabel(r'$f_{\rm CGM} = M_{\rm CGM} / (f_b M_{\rm vir})$', fontsize=13)
        ax.set_xlim(1e10, 1e13)
        ax.set_ylim(1e-3, 3)
        ax.legend(loc='upper right', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
               ha='right', va='bottom', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig1a_cgm_mass_and_fraction{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig1a_cgm_mass_and_fraction{OutputFormat}")


def plot_fig1b_hot_mass(snaps=[63, 32]):
    """
    Figure 1b: Hot Gas Mass vs Halo Mass (HotGas only)
    4 panels (2x2):
    Top row (z=0): M_hot vs M_vir | f_hot vs M_vir
    Bottom row (z=2): M_hot vs M_vir | f_hot vs M_vir
    """
    print("\nCreating Figure 1b: Hot Gas Mass and Fraction vs Halo Mass...")

    Gibble_halo = np.array([12.19,12.33,12.29,12.21,12.16,12.04,11.98,11.94])
    Gible_halo = 10**Gibble_halo
    Gibble_CGM = np.array([10.05,11.20,11.12,11.11,10.90,10.49,10.70,10.25])
    Gible_CGM = 10**Gibble_CGM
    
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    
    for row_idx, snap in enumerate(snaps):
        # ========== LEFT PANEL: M_hot vs M_vir ==========
        ax = axes[row_idx, 0]
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        Type = read_hdf(snap_num=Snapshot, param='Type')
        
        # Select centrals
        mask = (Type == 0) & (Mvir > 1e10) & (HotGas > 1e8)
        
        # Create contours
        if np.sum(mask) > 100:
            x_data = np.log10(Mvir[mask])
            y_data = np.log10(HotGas[mask])
            
            H, xedges, yedges = np.histogram2d(x_data, y_data, bins=50, 
                                                range=[[10, 15], [8, 14]])
            H = H.T
            X, Y = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels = np.percentile(H[H > 0], [39, 86, 99])
            levels = np.unique(levels)
            if len(levels) > 1:
                n_regions = len(levels) - 1
                alphas = np.linspace(0.15, 0.45, n_regions)
                rgb = to_rgba(COLOR_HOT)[:3]
                colors = [(*rgb, alpha) for alpha in alphas]
                ax.contour(10**X, 10**Y, H, levels=levels,
                          colors=COLOR_HOT, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X, 10**Y, H, levels=levels, colors=colors)
        
        # Add median line with error bars
        mass_bins = np.logspace(10, 15, 25)
        
        if np.sum(mask) > 50:
            x_c, med, low, upp = calculate_median_relation(
                np.log10(Mvir[mask]),
                np.log10(HotGas[mask]),
                np.log10(mass_bins)
            )
            valid = ~np.isnan(med)
            ax.plot(10**x_c[valid], 10**med[valid], color='black',
                   lw=2.5, label='Median', zorder=5)
            # Add error bars with caps
            ax.errorbar(10**x_c[valid], 10**med[valid], 
                       yerr=[10**med[valid] - 10**low[valid], 10**upp[valid] - 10**med[valid]],
                       fmt='none', ecolor='black', capsize=3, capthick=1.5, alpha=0.7, zorder=4)
            
        if row_idx == 0:
            ax.scatter(Gible_halo, Gible_CGM, s=100, color='darkgreen', 
                      alpha=0.7, edgecolors='black', linewidths=0.5,
                      label='GIBLE', zorder=8)
            
        # ========== ADD REAL OBSERVATIONAL CONSTRAINTS (z=0 only) ==========
        if row_idx == 0:  # z=0 panel only
            M_halo_obs = 1e12  # All observations are at ~10^12 Msun
            
            # 1. Werk et al. 2014 - lower limit (cool gas only)
            ax.scatter(M_halo_obs, 10**10.81, marker='^', s=150, 
                      color='red', edgecolors='black', linewidths=1.5,
                      label='Werk+14 (>)', zorder=10)
            ax.plot([M_halo_obs, M_halo_obs], [10**10.81, 10**11.5], 
                   color='red', ls='--', lw=1.5, alpha=0.5, zorder=9)
            
            # 2. Faerman et al. 2020 - range for MW
            ax.errorbar(M_halo_obs * 1.1, 10**10.74,  # midpoint of range
                       yerr=[[10**10.74 - 10**10.48], [10**11.0 - 10**10.74]],
                       fmt='o', markersize=10, color='blue', 
                       markeredgecolor='black', markeredgewidth=1.5,
                       ecolor='blue', capsize=5, capthick=2,
                       label='Faerman+20 (MW)', zorder=10)
            
            # 3. Stocke et al. 2013 - single estimate
            ax.scatter(M_halo_obs * 0.9, 10**10.5, marker='s', s=120,
                      color='green', edgecolors='black', linewidths=1.5,
                      label='Stocke+13', zorder=10)
            
            # 4. Bregman et al. 2018 - hot gas range (model dependent)
            ax.errorbar(M_halo_obs * 1.2, 10**10.2,  # midpoint
                       yerr=[[10**10.2 - 10**9.7], [10**10.7 - 10**10.2]],
                       fmt='d', markersize=10, color='orange',
                       markeredgecolor='black', markeredgewidth=1.5,
                       ecolor='orange', capsize=5, capthick=2,
                       label='Bregman+18 (hot)', zorder=10)
        
        # Add M_shock
        M_shock = 6e11
        ax.axvline(M_shock, color='black', ls='--', lw=2,
                  label=r'$M_{\rm shock}$', zorder=6)
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        if row_idx == 1:
            ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=13)
        ax.set_ylabel(r'$M_{\rm hot}$ [M$_\odot$]', fontsize=13)
        ax.set_xlim(1e11, 1e15)
        ax.set_ylim(1e8, 1e14)
        ax.legend(loc='upper left', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
               ha='right', va='bottom', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        
        # ========== RIGHT PANEL: f_hot vs M_vir ==========

        EAGLE_halo = np.array([11.06, 11.10, 11.16, 11.19, 11.22, 11.26, 11.31, 11.36, 11.39, 11.44, 
              11.48, 11.53, 11.60, 11.66, 11.71, 11.79, 11.87, 11.94, 12.02, 12.07, 
              12.13, 12.22, 12.31, 12.38, 12.41, 12.45, 12.52, 12.59, 12.69, 12.78, 
              12.87, 12.99, 13.03, 13.09, 13.19, 13.30, 13.37, 13.47, 13.59, 13.68, 
              13.79, 13.83, 13.88, 13.91, 13.94, 13.98, 14.02, 14.06, 14.11, 14.15, 
              14.20, 14.23, 14.27, 14.30])

        EAGLE_CGM = np.array([-2.87, -2.83, -2.76, -2.68, -2.61, -2.54, -2.48, -2.42, -2.35, -2.28, 
                    -2.18, -2.11, -2.02, -1.95, -1.88, -1.81, -1.71, -1.64, -1.60, -1.57, 
                    -1.55, -1.49, -1.48, -1.46, -1.45, -1.44, -1.37, -1.33, -1.29, -1.24, 
                    -1.21, -1.14, -1.12, -1.08, -1.03, -1.01, -0.99, -0.99, -0.99, -1.02, 
                    -1.09, -1.12, -1.14, -1.21, -1.28, -1.34, -1.44, -1.54, -1.62, -1.74, 
                    -1.81, -1.89, -1.97, -2.04])
        ax = axes[row_idx, 1]
        
        # Calculate f_hot = M_hot / (f_b * M_vir)
        f_hot = np.zeros_like(HotGas)
        mask_mvir = Mvir > 0
        f_hot[mask_mvir] = HotGas[mask_mvir] / (BaryonFrac * Mvir[mask_mvir])
        
        # Select data
        mask_f = (Type == 0) & (Mvir > 1e10) & (f_hot > 1e-4)
        
        # Create contours
        if np.sum(mask_f) > 100:
            x_data_f = np.log10(Mvir[mask_f])
            y_data_f = np.log10(f_hot[mask_f])
            
            H_f, xedges_f, yedges_f = np.histogram2d(x_data_f, y_data_f, bins=50,
                                                      range=[[10, 15], [-3, 0.5]])
            H_f = H_f.T
            X_f, Y_f = np.meshgrid(xedges_f[:-1], yedges_f[:-1])
            
            levels_f = np.percentile(H_f[H_f > 0], [39, 86, 99])
            levels_f = np.unique(levels_f)
            if len(levels_f) > 1:
                n_regions = len(levels_f) - 1
                alphas = np.linspace(0.15, 0.45, n_regions)
                rgb = to_rgba(COLOR_HOT)[:3]
                colors = [(*rgb, alpha) for alpha in alphas]
                ax.contour(10**X_f, 10**Y_f, H_f, levels=levels_f,
                          colors=COLOR_HOT, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_f, 10**Y_f, H_f, levels=levels_f, colors=colors)
        
        # Add median line with error bars
        if np.sum(mask_f) > 50:
            x_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            medians = np.zeros(len(x_centers))
            lower_percentile = np.zeros(len(x_centers))
            upper_percentile = np.zeros(len(x_centers))
            
            for i, (m_low, m_high) in enumerate(zip(mass_bins[:-1], mass_bins[1:])):
                mask_bin = mask_f & (Mvir >= m_low) & (Mvir < m_high)
                if np.sum(mask_bin) > 10:
                    values = f_hot[mask_bin]
                    medians[i] = np.percentile(values, 50)
                    lower_percentile[i] = np.percentile(values, 16)
                    upper_percentile[i] = np.percentile(values, 84)
                else:
                    medians[i] = np.nan
                    lower_percentile[i] = np.nan
                    upper_percentile[i] = np.nan
            
            valid = ~np.isnan(medians) & (medians > 0)
            ax.plot(x_centers[valid], medians[valid], color='black',
                   lw=2.5, label='Median', zorder=5)
            # Add error bars with caps
            ax.errorbar(x_centers[valid], medians[valid],
                       yerr=[medians[valid] - lower_percentile[valid], 
                             upper_percentile[valid] - medians[valid]],
                       fmt='none', ecolor='black', capsize=3, capthick=1.5, alpha=0.7, zorder=4)
            
        if row_idx == 0:
            ax.plot(10**EAGLE_halo, 10**EAGLE_CGM, 'o', 
                      color='darkred', markeredgecolor='black', markeredgewidth=1.5,
                      label='EAGLE', zorder=7, alpha=0.9)
        
        # Add M_shock
        ax.axvline(M_shock, color='black', ls='--', lw=2, zorder=6)
        
        # Add reference line
        ax.axhline(1.0, color='gray', ls=':', lw=1.5, alpha=0.7, label=r'$f_{\rm hot} = 1$')
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        if row_idx == 1:
            ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=13)
        ax.set_ylabel(r'$f_{\rm hot} = M_{\rm hot} / (f_b M_{\rm vir})$', fontsize=13)
        ax.set_xlim(1e11, 1e15)
        ax.set_ylim(1e-2, 3)
        ax.legend(loc='upper right', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
               ha='right', va='bottom', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig1b_hot_mass_and_fraction{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig1b_hot_mass_and_fraction{OutputFormat}")


def plot_fig3a_cgm_metallicity(snaps=[63, 32]):
    """
    Figure 3a: CGM Metallicity (CGMgas only)
    Two panels: z=0 and z=2
    """
    print("\nCreating Figure 3a: CGM Metallicity...")
    
    # Observational data (log10 M* vs log10(Z_CGM/Z_sun))
    obs_logM = np.array([9.78, 9.73, 10.01, 10.02, 10.08, 10.35, 10.35, 10.26, 
                         10.27, 10.15, 10.29, 10.22, 10.39, 10.52, 10.47, 10.58, 
                         10.60, 10.76, 10.86, 10.80])
    obs_logZ = np.array([-0.02, -1.11, -0.58, -0.31, -0.34, -0.06, -0.38, -0.78,
                         -1.13, -1.99, -1.70, -2.59, -1.98, -1.91, -1.44, -1.33,
                         -1.33, -1.49, -1.57, -0.37])
    
    # Observational data set 2 (log10 M* vs log10(Z_CGM/Z_sun))
    obs_logM2 = np.array([10.12, 10.10, 10.78, 10.86, 11.16, 11.16, 11.49, 11.37, 11.35, 11.37,
                          11.45, 11.41, 11.41, 11.65, 12.15, 12.16, 12.06, 12.05, 12.01, 11.92,
                          11.71, 11.71, 11.74, 11.68, 11.64, 11.61, 11.67, 11.69, 11.73, 11.73,
                          11.67, 11.65, 11.62, 11.56, 11.65, 11.59, 11.76, 11.93, 12.01, 11.90,
                          11.87, 11.93, 12.04, 12.00, 12.05, 12.05, 12.13, 12.16, 12.24, 12.37,
                          12.52, 12.52, 12.41, 12.41, 12.37, 12.29, 12.29, 12.28, 12.37, 12.41,
                          12.41, 12.41, 12.51, 12.51, 12.52, 12.41, 12.41, 12.41, 12.40, 12.40,
                          12.37, 12.36, 12.24, 12.28, 12.25, 12.23, 12.16, 12.14, 12.00, 12.01,
                          11.94, 11.90, 11.58, 11.65, 11.70, 11.71, 11.73, 11.72, 11.83, 11.82,
                          11.87, 11.93, 11.84, 11.75, 11.72, 11.68, 11.83, 11.87, 11.91, 11.91,
                          11.95, 11.99, 12.00, 12.04, 12.09, 12.07, 12.16, 12.16, 12.14, 12.13,
                          12.13, 12.14, 12.07, 12.05, 12.00, 12.02, 12.00, 12.00, 11.98, 11.90])
    obs_logZ2 = np.array([-1.67, -0.48, -0.59, -0.72, -0.43, 0.31, 0.72, 0.52, 0.40, -0.44,
                          -1.23, -1.36, -1.72, -2.60, -2.64, -2.41, -2.28, -2.14, -2.02, -1.99,
                          -1.96, -1.79, -1.56, -1.35, -1.28, -1.00, -1.07, -0.98, -0.98, -1.10,
                          -0.89, -0.74, -0.23, -0.23, 0.50, 0.36, 0.85, 0.46, 0.50, -1.55,
                          -1.79, -1.79, -1.58, -1.25, -1.19, -1.10, -1.11, -1.30, -1.74, -1.85,
                          -2.07, -1.36, -1.40, -1.20, -1.07, -1.09, -0.72, -0.57, -0.65, -0.66,
                          -0.86, -0.99, -0.46, -0.15, 0.15, 0.13, -0.38, -0.22, -0.15, -0.02,
                          0.02, 0.09, 0.08, -0.02, -0.27, -0.35, -0.40, -0.62, -0.61, -0.70,
                          -0.59, -0.43, 0.01, 0.10, 0.20, 0.15, 0.04, -0.08, 0.03, 0.15,
                          0.14, 0.10, -0.28, -0.34, -0.45, -0.56, -0.36, -0.21, -0.14, -0.23,
                          -0.33, -0.35, -0.43, -0.32, -0.27, -0.45, -0.26, -0.18, -0.10, 0.04,
                          0.16, 0.21, 0.07, -0.03, -0.17, 0.06, 0.08, 0.19, 0.30, 0.37])
    
    # ========== NEW: COS-Halos data ==========
    # Load COS-Halos merged data
    import pandas as pd
    try:
        cos_halos = pd.read_csv('../../cos_halos_complete.csv')
        
        # Filter for galaxies with both stellar mass and metallicity data
        has_data = cos_halos['Z_H_2'].notna() & cos_halos['log_M_star'].notna()
        cos_halos_clean = cos_halos[has_data].copy()
        
        # Extract stellar masses (convert from log to linear for plotting)
        cos_M_star = 10**cos_halos_clean['log_M_star'].values
        
        # Extract metallicities (Z_H_2 is the median, already in log form)
        cos_Z_median = cos_halos_clean['Z_H_2'].values
        
        # Calculate error bars (68% confidence interval)
        cos_Z_lower = cos_halos_clean['Z_H_1'].values
        cos_Z_upper = cos_halos_clean['Z_H_3'].values
        cos_Z_err_low = cos_Z_median - cos_Z_lower
        cos_Z_err_high = cos_Z_upper - cos_Z_median
        
        print(f"  Loaded {len(cos_halos_clean)} COS-Halos galaxies with metallicity data")
    except FileNotFoundError:
        print("  Warning: cos_halos_complete.csv not found. Skipping COS-Halos data.")
        cos_halos_clean = None
    # ========================================
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    
    for idx, (snap, ax) in enumerate(zip(snaps, axes)):
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        MetalsCGM = read_hdf(snap_num=Snapshot, param='MetalsCGMgas') * 1.0e10 / Hubble_h
        Type = read_hdf(snap_num=Snapshot, param='Type')
        
        # Calculate metallicity
        Z_CGM = np.zeros_like(CGMgas)
        mask_gas = CGMgas > 0
        Z_CGM[mask_gas] = MetalsCGM[mask_gas] / CGMgas[mask_gas]
        
        Z_sun = 0.02
        log_Z_CGM = np.log10(Z_CGM / Z_sun + 1e-10)
        
        # Select centrals
        mask = (Type == 0) & (StellarMass > 1e8) & (CGMgas > 1e7) & (Z_CGM > 0)
        
        # Create contours
        if np.sum(mask) > 100:
            x_data = np.log10(StellarMass[mask])
            y_data = log_Z_CGM[mask]
            
            H, xedges, yedges = np.histogram2d(x_data, y_data, bins=50,
                                                range=[[8, 11.7], [-2.5, 0.5]])
            H = H.T
            X, Y = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels = np.percentile(H[H > 0], [39, 86, 99])
            levels = np.unique(levels)
            if len(levels) > 1:
                n_regions = len(levels) - 1
                alphas = np.linspace(0.15, 0.45, n_regions)
                rgb = to_rgba(COLOR_CGM)[:3]
                colors = [(*rgb, alpha) for alpha in alphas]
                ax.contour(10**X, Y, H, levels=levels,
                          colors=COLOR_CGM, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X, Y, H, levels=levels, colors=colors)
        
        # Add median line with error bars
        mass_bins = np.logspace(8, 12, 20)
        
        if np.sum(mask) > 50:
            x_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            medians = np.zeros(len(x_centers))
            lower_percentile = np.zeros(len(x_centers))
            upper_percentile = np.zeros(len(x_centers))
            
            for i, (m_low, m_high) in enumerate(zip(mass_bins[:-1], mass_bins[1:])):
                mask_bin = mask & (StellarMass >= m_low) & (StellarMass < m_high)
                if np.sum(mask_bin) > 10:
                    values = log_Z_CGM[mask_bin]
                    medians[i] = np.percentile(values, 50)
                    lower_percentile[i] = np.percentile(values, 16)
                    upper_percentile[i] = np.percentile(values, 84)
                else:
                    medians[i] = np.nan
                    lower_percentile[i] = np.nan
                    upper_percentile[i] = np.nan
            
            valid = ~np.isnan(medians)
            ax.plot(x_centers[valid], medians[valid], color='black',
                   lw=2.5, label='Median', zorder=5)
            # Add error bars with caps
            ax.errorbar(x_centers[valid], medians[valid],
                       yerr=[medians[valid] - lower_percentile[valid], 
                             upper_percentile[valid] - medians[valid]],
                       fmt='none', ecolor='black', capsize=3, capthick=1.5, alpha=0.7, zorder=4)
        
        if idx == 0:  # Only for z=0
            # Add observational data
            ax.scatter(10**obs_logM, obs_logZ, c='darkred', marker='o', s=80, 
                    edgecolors='black', linewidths=1.5, alpha=0.8,
                    label='Kacprzak et al., 2019', zorder=6)
            
            ax.scatter(10**obs_logM2, obs_logZ2, c='darkred', marker='s', s=80, 
                      edgecolors='black', linewidths=1.5, alpha=0.8,
                      label='Sameer et al., 2024', zorder=6)
            
            # ========== NEW: Add COS-Halos data ==========
            if cos_halos_clean is not None:
                ax.errorbar(cos_M_star, cos_Z_median,
                           yerr=[cos_Z_err_low, cos_Z_err_high],
                           fmt='D', markersize=7, color='darkred',
                           ecolor='black', capsize=3, capthick=1.5,
                           markeredgecolor='black', markeredgewidth=1.5,
                           alpha=0.8, label='COS-Halos (z~0.2)', zorder=6)
            # =============================================
        
        # Solar line
        ax.axhline(0, color='gray', ls=':', lw=1.5, alpha=0.7, label='Solar')
        
        # Formatting
        ax.set_xscale('log')
        ax.set_xlabel(r'$M_*$ [M$_\odot$]', fontsize=13)
        if idx == 0:
            ax.set_ylabel(r'$\log_{10}(Z_{\rm CGM}/Z_\odot)$', fontsize=13)
        ax.set_xlim(1e8, 5e11)
        ax.set_ylim(-2.5, 0.5)
        ax.legend(loc='lower right', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.05, 0.95, f'z = {z:.1f}', transform=ax.transAxes,
               ha='left', va='top', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig3a_cgm_metallicity{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig3a_cgm_metallicity{OutputFormat}")


def plot_fig3b_hot_metallicity(snaps=[63, 32]):
    """
    Figure 3b: Hot Gas Metallicity (HotGas only)
    Two panels: z=0 and z=2
    """
    print("\nCreating Figure 3b: Hot Gas Metallicity...")
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    # Observational data set 2 (log10 M* vs log10(Z_CGM/Z_sun))
    obs_logM2 = np.array([10.12, 10.10, 10.78, 10.86, 11.16, 11.16, 11.49, 11.37, 11.35, 11.37,
                          11.45, 11.41, 11.41, 11.65, 12.15, 12.16, 12.06, 12.05, 12.01, 11.92,
                          11.71, 11.71, 11.74, 11.68, 11.64, 11.61, 11.67, 11.69, 11.73, 11.73,
                          11.67, 11.65, 11.62, 11.56, 11.65, 11.59, 11.76, 11.93, 12.01, 11.90,
                          11.87, 11.93, 12.04, 12.00, 12.05, 12.05, 12.13, 12.16, 12.24, 12.37,
                          12.52, 12.52, 12.41, 12.41, 12.37, 12.29, 12.29, 12.28, 12.37, 12.41,
                          12.41, 12.41, 12.51, 12.51, 12.52, 12.41, 12.41, 12.41, 12.40, 12.40,
                          12.37, 12.36, 12.24, 12.28, 12.25, 12.23, 12.16, 12.14, 12.00, 12.01,
                          11.94, 11.90, 11.58, 11.65, 11.70, 11.71, 11.73, 11.72, 11.83, 11.82,
                          11.87, 11.93, 11.84, 11.75, 11.72, 11.68, 11.83, 11.87, 11.91, 11.91,
                          11.95, 11.99, 12.00, 12.04, 12.09, 12.07, 12.16, 12.16, 12.14, 12.13,
                          12.13, 12.14, 12.07, 12.05, 12.00, 12.02, 12.00, 12.00, 11.98, 11.90])
    obs_logZ2 = np.array([-1.67, -0.48, -0.59, -0.72, -0.43, 0.31, 0.72, 0.52, 0.40, -0.44,
                          -1.23, -1.36, -1.72, -2.60, -2.64, -2.41, -2.28, -2.14, -2.02, -1.99,
                          -1.96, -1.79, -1.56, -1.35, -1.28, -1.00, -1.07, -0.98, -0.98, -1.10,
                          -0.89, -0.74, -0.23, -0.23, 0.50, 0.36, 0.85, 0.46, 0.50, -1.55,
                          -1.79, -1.79, -1.58, -1.25, -1.19, -1.10, -1.11, -1.30, -1.74, -1.85,
                          -2.07, -1.36, -1.40, -1.20, -1.07, -1.09, -0.72, -0.57, -0.65, -0.66,
                          -0.86, -0.99, -0.46, -0.15, 0.15, 0.13, -0.38, -0.22, -0.15, -0.02,
                          0.02, 0.09, 0.08, -0.02, -0.27, -0.35, -0.40, -0.62, -0.61, -0.70,
                          -0.59, -0.43, 0.01, 0.10, 0.20, 0.15, 0.04, -0.08, 0.03, 0.15,
                          0.14, 0.10, -0.28, -0.34, -0.45, -0.56, -0.36, -0.21, -0.14, -0.23,
                          -0.33, -0.35, -0.43, -0.32, -0.27, -0.45, -0.26, -0.18, -0.10, 0.04,
                          0.16, 0.21, 0.07, -0.03, -0.17, 0.06, 0.08, 0.19, 0.30, 0.37])
    
    for idx, (snap, ax) in enumerate(zip(snaps, axes)):
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        MetalsHot = read_hdf(snap_num=Snapshot, param='MetalsHotGas') * 1.0e10 / Hubble_h
        Type = read_hdf(snap_num=Snapshot, param='Type')
        
        # Calculate metallicity
        Z_hot = np.zeros_like(HotGas)
        mask_gas = HotGas > 0
        Z_hot[mask_gas] = MetalsHot[mask_gas] / HotGas[mask_gas]
        
        Z_sun = 0.02
        log_Z_hot = np.log10(Z_hot / Z_sun + 1e-10)
        
        # Select centrals
        mask = (Type == 0) & (StellarMass > 1e9) & (HotGas > 1e8) & (Z_hot > 0)
        
        # Create contours
        if np.sum(mask) > 100:
            x_data = np.log10(StellarMass[mask])
            y_data = log_Z_hot[mask]
            
            H, xedges, yedges = np.histogram2d(x_data, y_data, bins=50,
                                                range=[[9, 11.7], [-2.5, 0.5]])
            H = H.T
            X, Y = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels = np.percentile(H[H > 0], [39, 86, 99])
            levels = np.unique(levels)
            if len(levels) > 1:
                n_regions = len(levels) - 1
                alphas = np.linspace(0.15, 0.45, n_regions)
                rgb = to_rgba(COLOR_HOT)[:3]
                colors = [(*rgb, alpha) for alpha in alphas]
                ax.contour(10**X, Y, H, levels=levels,
                          colors=COLOR_HOT, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X, Y, H, levels=levels, colors=colors)
        
        # Add median line with error bars
        mass_bins = np.logspace(9, 12, 20)
        
        if np.sum(mask) > 50:
            x_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            medians = np.zeros(len(x_centers))
            lower_percentile = np.zeros(len(x_centers))
            upper_percentile = np.zeros(len(x_centers))
            
            for i, (m_low, m_high) in enumerate(zip(mass_bins[:-1], mass_bins[1:])):
                mask_bin = mask & (StellarMass >= m_low) & (StellarMass < m_high)
                if np.sum(mask_bin) > 10:
                    values = log_Z_hot[mask_bin]
                    medians[i] = np.percentile(values, 50)
                    lower_percentile[i] = np.percentile(values, 16)
                    upper_percentile[i] = np.percentile(values, 84)
                else:
                    medians[i] = np.nan
                    lower_percentile[i] = np.nan
                    upper_percentile[i] = np.nan
            
            valid = ~np.isnan(medians)
            ax.plot(x_centers[valid], medians[valid], color='black',
                   lw=2.5, label='Median', zorder=5)
            # Add error bars with caps
            ax.errorbar(x_centers[valid], medians[valid],
                       yerr=[medians[valid] - lower_percentile[valid], 
                             upper_percentile[valid] - medians[valid]],
                       fmt='none', ecolor='black', capsize=3, capthick=1.5, alpha=0.7, zorder=4)
        
        # Solar line
        ax.axhline(0, color='gray', ls=':', lw=1.5, alpha=0.7, label='Solar')

        if idx == 0:  # Only for z=0
            # Add observational data
            ax.scatter(10**obs_logM2, obs_logZ2, c='darkred', marker='s', s=80, 
                      edgecolors='black', linewidths=1.5, alpha=0.8,
                      label='Sameer et al., 2024', zorder=6)
        
        # Formatting
        ax.set_xscale('log')
        ax.set_xlabel(r'$M_*$ [M$_\odot$]', fontsize=13)
        if idx == 0:
            ax.set_ylabel(r'$\log_{10}(Z_{\rm hot}/Z_\odot)$', fontsize=13)
        ax.set_xlim(1e9, 5e11)
        ax.set_ylim(-2.5, 0.5)
        ax.legend(loc='lower right', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.05, 0.95, f'z = {z:.1f}', transform=ax.transAxes,
               ha='left', va='top', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig3b_hot_metallicity{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig3b_hot_metallicity{OutputFormat}")

def main():
    """Main execution function"""
    
    print("="*70)
    print("CGM SECTION ANALYSIS FOR SAGE (Modified)")
    print("="*70)
    
    # Create output directory
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)
        print(f"Created output directory: {OutputDir}")
    
    print(f"\nReading from: {DirName}{FileName}")
    print(f"Output to: {OutputDir}\n")
    
    # Set random seed for reproducibility
    seed(42)
    
    # Generate all CGM section figures
    try:
        plot_fig1a_cgm_mass(snaps=[63, 32])  # z=0, z=2
    except Exception as e:
        print(f"  Error creating Figure 1a: {e}")
    
    try:
        plot_fig1b_hot_mass(snaps=[63, 32])  # z=0, z=2
    except Exception as e:
        print(f"  Error creating Figure 1b: {e}")
    
    try:
        plot_fig3a_cgm_metallicity(snaps=[63, 32])  # z=0, z=2
    except Exception as e:
        print(f"  Error creating Figure 3a: {e}")
    
    try:
        plot_fig3b_hot_metallicity(snaps=[63, 32])  # z=0, z=2
    except Exception as e:
        print(f"  Error creating Figure 3b: {e}")
    
    print("\n" + "="*70)
    print("CGM SECTION FIGURES COMPLETED!")
    print("="*70)
    print("\nFigures generated:")
    print("  1a. cgm_fig1a_cgm_mass_and_fraction.png - CGM Mass (2 redshifts)")
    print("  1b. cgm_fig1b_hot_mass_and_fraction.png - Hot Gas Mass (2 redshifts)")
    print("  2. cgm_fig2_mass_function.png - Combined Mass Function (4 redshifts)")
    print("  3a. cgm_fig3a_cgm_metallicity.png - CGM Metallicity (2 redshifts)")
    print("  3b. cgm_fig3b_hot_metallicity.png - Hot Gas Metallicity (2 redshifts)")
    print("  4a. cgm_fig4a_cgm_evolution.png - CGM Evolution (4 redshifts)")
    print("  4b. cgm_fig4b_hot_evolution.png - Hot Gas Evolution (4 redshifts)")
    
    # Print galaxy statistics at z=0
    print("\n" + "="*70)
    print("GALAXY STATISTICS AT z=0 (Snap 63)")
    print("="*70)
    
    snap = 63
    Snapshot = f'Snap_{snap}'
    
    # Read data
    Type = read_hdf(snap_num=Snapshot, param='Type')
    CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    
    # Calculate counts
    centrals = (Type == 0)
    n_total = np.sum(centrals)
    
    # Thresholds
    threshold = 1e8  # M_sun
    
    # Galaxies with CGM gas
    with_cgm = centrals & (CGMgas > threshold)
    n_with_cgm = np.sum(with_cgm)
    
    # Galaxies with Hot gas
    with_hot = centrals & (HotGas > threshold)
    n_with_hot = np.sum(with_hot)
    
    # Galaxies with both
    with_both = centrals & (CGMgas > threshold) & (HotGas > threshold)
    n_with_both = np.sum(with_both)
    
    print(f"\nTotal central galaxies:           {n_total:,}")
    print(f"With CGM gas > 10^8:              {n_with_cgm:,} ({100*n_with_cgm/n_total:.1f}%)")
    print(f"With Hot gas > 10^8:              {n_with_hot:,} ({100*n_with_hot/n_total:.1f}%)")
    print(f"With both CGM and Hot > 10^8:     {n_with_both:,} ({100*n_with_both/n_total:.1f}%)")
    print("="*70 + "\n")


if __name__ == '__main__':
    main()