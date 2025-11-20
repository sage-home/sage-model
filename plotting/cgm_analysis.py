#!/usr/bin/env python
"""
CGM Analysis and Plotting Script for SAGE
Generates figures comparing SAGE CGM predictions
Modified to separate CGM and HotGas plots

Figures produced:
1a. CGM Mass vs Halo Mass (CGM only)
1b. Hot Gas Mass vs Halo Mass (Hot only)
2. Baryon Budget Across Mass (combined)
3a. CGM Precipitation Criterion (t_cool/t_ff)
3b. Hot Gas Precipitation Criterion (t_cool/t_ff)
4a. CGM Metallicity vs Stellar Mass
4b. Hot Gas Metallicity vs Stellar Mass
5a. CGM Evolution with Time
5b. Hot Gas Evolution with Time
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import LogNorm, Normalize
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
FirstSnap = 0
LastSnap = 63
BaryonFrac = 0.17  # Match the value in your parameter file

redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 
             11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 
             4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 2.831, 2.619, 2.422, 2.239, 
             2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 
             0.242, 0.208, 0.175, 0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

# Plotting options
dilute = 5000  # Number of galaxies for scatter plots
OutputFormat = '.png'

# Plot styling
plt.rcParams["figure.figsize"] = (10, 8)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 12
plt.rcParams["font.family"] = "serif"

# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read parameter from HDF5 file"""
    property = h5.File(DirName + FileName, 'r')
    return np.array(property[snap_num][param])


def get_cosmic_baryon_fraction():
    """Return cosmic baryon fraction"""
    return BaryonFrac


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


def plot_figure1a_cgm_vs_halo_mass(snap=63):
    """
    Figure 1a: CGM Mass vs Halo Mass (CGM only)
    """
    print("Creating Figure 1a: CGM Mass vs Halo Mass...")
    
    Snapshot = f'Snap_{snap}'
    z = redshifts[snap]
    
    # Read data
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    
    # Select centrals with significant gas
    mask = (Type == 0) & (Mvir > 1e10) & (CGMgas > 1e8)
    
    if np.sum(mask) > dilute:
        indices = sample(range(np.sum(mask)), dilute)
        mask_indices = np.where(mask)[0][indices]
    else:
        mask_indices = np.where(mask)[0]
    
    Mvir_plot = Mvir[mask_indices]
    CGMgas_plot = CGMgas[mask_indices]
    
    # Calculate median relations
    mass_bins = np.logspace(10, 15, 25)
    
    # Create figure
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # ========== Main Panel: CGM Mass vs Halo Mass ==========
    # Plot individual galaxies
    ax1.scatter(Mvir_plot, CGMgas_plot, 
                c='cornflowerblue', alpha=0.3, s=10, 
                label='CGM gas', rasterized=True)
    
    # Add median relation
    x_c, med, low, upp = calculate_median_relation(
        np.log10(Mvir_plot), 
        np.log10(CGMgas_plot), 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax1.plot(10**x_c[valid], 10**med[valid], color='blue', 
            lw=2.5, label='Median', zorder=5)
    ax1.fill_between(10**x_c[valid], 10**low[valid], 10**upp[valid], 
                    color='blue', alpha=0.2, zorder=4)
    
    # Add M_shock line
    M_shock = 6e11
    ax1.axvline(M_shock, color='black', ls='--', lw=2, 
               label=r'$M_{\rm shock} = 6 \times 10^{11}$ M$_\odot$', zorder=6)
    
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=14)
    ax1.set_ylabel(r'$M_{\rm CGM}$ [M$_\odot$]', fontsize=14)
    ax1.set_xlim(1e10, 1e15)
    ax1.set_ylim(1e8, 1e13)
    ax1.legend(loc='upper left', fontsize=10, framealpha=0.9)
    ax1.grid(alpha=0.3, ls=':')
    ax1.text(0.95, 0.05, f'z = {z:.1f}', transform=ax1.transAxes,
            ha='right', va='bottom', fontsize=12,
            bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    # ========== Right Panel: CGM Fraction ==========
    f_cgm = CGMgas_plot / (Mvir_plot * get_cosmic_baryon_fraction())
    
    ax2.scatter(Mvir_plot, f_cgm, 
               c='cornflowerblue', alpha=0.3, s=10, rasterized=True)
    
    # Add median
    x_c, med, low, upp = calculate_median_relation(
        np.log10(Mvir_plot), 
        np.log10(f_cgm), 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax2.plot(10**x_c[valid], 10**med[valid], color='blue', 
            lw=2.5, label='Median', zorder=5)
    ax2.fill_between(10**x_c[valid], 10**low[valid], 10**upp[valid], 
                    color='blue', alpha=0.3, zorder=4)
    
    ax2.axhline(1.0, color='gray', ls=':', lw=1.5, 
               label='Cosmic fraction', zorder=3)
    ax2.axvline(M_shock, color='black', ls='--', lw=2, zorder=6)
    
    ax2.set_xscale('log')
    ax2.set_yscale('log')
    ax2.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=14)
    ax2.set_ylabel(r'$M_{\rm CGM} / (f_b M_{\rm vir})$', fontsize=14)
    ax2.set_xlim(1e10, 1e15)
    ax2.set_ylim(0.01, 3)
    ax2.legend(loc='best', fontsize=10)
    ax2.grid(alpha=0.3, ls=':')
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig1a_cgm_vs_halo_mass{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig1a_cgm_vs_halo_mass{OutputFormat}')


def plot_figure1b_hot_vs_halo_mass(snap=63):
    """
    Figure 1b: Hot Gas Mass vs Halo Mass (Hot only)
    """
    print("Creating Figure 1b: Hot Gas Mass vs Halo Mass...")
    
    Snapshot = f'Snap_{snap}'
    z = redshifts[snap]
    
    # Read data
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    
    # Select centrals with significant gas
    mask = (Type == 0) & (Mvir > 1e10) & (HotGas > 1e8)
    
    if np.sum(mask) > dilute:
        indices = sample(range(np.sum(mask)), dilute)
        mask_indices = np.where(mask)[0][indices]
    else:
        mask_indices = np.where(mask)[0]
    
    Mvir_plot = Mvir[mask_indices]
    HotGas_plot = HotGas[mask_indices]
    
    # Calculate median relations
    mass_bins = np.logspace(10, 15, 25)
    
    # Create figure
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # ========== Main Panel: Hot Gas Mass vs Halo Mass ==========
    # Plot individual galaxies
    ax1.scatter(Mvir_plot, HotGas_plot, 
                c='firebrick', alpha=0.3, s=10, 
                label='Hot gas', rasterized=True)
    
    # Add median relation
    x_c, med, low, upp = calculate_median_relation(
        np.log10(Mvir_plot), 
        np.log10(HotGas_plot), 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax1.plot(10**x_c[valid], 10**med[valid], color='darkred', 
            lw=2.5, label='Median', zorder=5)
    ax1.fill_between(10**x_c[valid], 10**low[valid], 10**upp[valid], 
                    color='darkred', alpha=0.2, zorder=4)
    
    # Add M_shock line
    M_shock = 6e11
    ax1.axvline(M_shock, color='black', ls='--', lw=2, 
               label=r'$M_{\rm shock} = 6 \times 10^{11}$ M$_\odot$', zorder=6)
    
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=14)
    ax1.set_ylabel(r'$M_{\rm hot}$ [M$_\odot$]', fontsize=14)
    ax1.set_xlim(1e10, 1e15)
    ax1.set_ylim(1e8, 1e13)
    ax1.legend(loc='upper left', fontsize=10, framealpha=0.9)
    ax1.grid(alpha=0.3, ls=':')
    ax1.text(0.95, 0.05, f'z = {z:.1f}', transform=ax1.transAxes,
            ha='right', va='bottom', fontsize=12,
            bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    # ========== Right Panel: Hot Gas Fraction ==========
    f_hot = HotGas_plot / (Mvir_plot * get_cosmic_baryon_fraction())
    
    ax2.scatter(Mvir_plot, f_hot, 
               c='firebrick', alpha=0.3, s=10, rasterized=True)
    
    # Add median
    x_c, med, low, upp = calculate_median_relation(
        np.log10(Mvir_plot), 
        np.log10(f_hot), 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax2.plot(10**x_c[valid], 10**med[valid], color='darkred', 
            lw=2.5, label='Median', zorder=5)
    ax2.fill_between(10**x_c[valid], 10**low[valid], 10**upp[valid], 
                    color='darkred', alpha=0.3, zorder=4)
    
    ax2.axhline(1.0, color='gray', ls=':', lw=1.5, 
               label='Cosmic fraction', zorder=3)
    ax2.axvline(M_shock, color='black', ls='--', lw=2, zorder=6)
    
    ax2.set_xscale('log')
    ax2.set_yscale('log')
    ax2.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=14)
    ax2.set_ylabel(r'$M_{\rm hot} / (f_b M_{\rm vir})$', fontsize=14)
    ax2.set_xlim(1e10, 1e15)
    ax2.set_ylim(0.01, 3)
    ax2.legend(loc='best', fontsize=10)
    ax2.grid(alpha=0.3, ls=':')
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig1b_hot_vs_halo_mass{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig1b_hot_vs_halo_mass{OutputFormat}')


def plot_figure2_baryon_budget(snap=63):
    """
    Figure 2: Baryon Budget Across Mass (Combined)
    Stacked area plot showing where baryons reside
    """
    print("Creating Figure 2: Baryon Budget Across Mass...")
    
    Snapshot = f'Snap_{snap}'
    z = redshifts[snap]
    
    # Read data
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
    CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    EjectedMass = read_hdf(snap_num=Snapshot, param='EjectedMass') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    
    # Select centrals
    mask = (Type == 0) & (Mvir > 1e10)
    
    # Calculate baryon fractions
    f_baryon = get_cosmic_baryon_fraction()
    TotalBaryons = Mvir * f_baryon
    
    # Bin by halo mass
    mass_bins = np.logspace(10, 14.5, 20)
    x_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
    
    # Initialize arrays
    f_stars = np.zeros(len(x_centers))
    f_cold = np.zeros(len(x_centers))
    f_cgm = np.zeros(len(x_centers))
    f_hot = np.zeros(len(x_centers))
    f_ejected = np.zeros(len(x_centers))
    f_missing = np.zeros(len(x_centers))
    
    for i, (m_low, m_high) in enumerate(zip(mass_bins[:-1], mass_bins[1:])):
        bin_mask = mask & (Mvir >= m_low) & (Mvir < m_high)
        if np.sum(bin_mask) > 10:
            f_stars[i] = np.median(StellarMass[bin_mask] / TotalBaryons[bin_mask])
            f_cold[i] = np.median(ColdGas[bin_mask] / TotalBaryons[bin_mask])
            f_cgm[i] = np.median(CGMgas[bin_mask] / TotalBaryons[bin_mask])
            f_hot[i] = np.median(HotGas[bin_mask] / TotalBaryons[bin_mask])
            f_ejected[i] = np.median(EjectedMass[bin_mask] / TotalBaryons[bin_mask])
            
            # Missing baryons
            accounted = f_stars[i] + f_cold[i] + f_cgm[i] + f_hot[i] + f_ejected[i]
            f_missing[i] = np.maximum(0, 1.0 - accounted)
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 7))
    
    # Stacked area plot
    ax.fill_between(x_centers, 0, f_stars, 
                    color='gold', alpha=0.8, label='Stars')
    ax.fill_between(x_centers, f_stars, f_stars + f_cold, 
                    color='deepskyblue', alpha=0.8, label='Cold Gas')
    ax.fill_between(x_centers, f_stars + f_cold, 
                    f_stars + f_cold + f_cgm, 
                    color='cornflowerblue', alpha=0.8, label='CGM')
    ax.fill_between(x_centers, f_stars + f_cold + f_cgm, 
                    f_stars + f_cold + f_cgm + f_hot, 
                    color='firebrick', alpha=0.8, label='Hot Gas')
    ax.fill_between(x_centers, f_stars + f_cold + f_cgm + f_hot, 
                    f_stars + f_cold + f_cgm + f_hot + f_ejected, 
                    color='purple', alpha=0.8, label='Ejected')
    ax.fill_between(x_centers, 
                    f_stars + f_cold + f_cgm + f_hot + f_ejected, 
                    1.0, 
                    color='lightgray', alpha=0.6, label='Missing')
    
    ax.set_xscale('log')
    ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=14)
    ax.set_ylabel(r'Baryon Fraction', fontsize=14)
    ax.set_xlim(1e10, 3e14)
    ax.set_ylim(0, 1.05)
    ax.legend(loc='upper right', fontsize=11, framealpha=0.9)
    ax.grid(alpha=0.3, ls=':', axis='y')
    
    # Add vertical line at M_shock
    M_shock = 6e11
    ax.axvline(M_shock, color='black', ls='--', lw=1.5, alpha=0.5)
    ax.text(M_shock*1.3, 0.95, r'$M_{\rm shock}$', 
           fontsize=11, rotation=0, ha='left', va='top')
    
    ax.text(0.05, 0.95, f'z = {z:.1f}', transform=ax.transAxes,
           ha='left', va='top', fontsize=12,
           bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig2_baryon_budget{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig2_baryon_budget{OutputFormat}')


def plot_figure3a_cgm_precipitation(snap=63):
    """
    Figure 3a: CGM Precipitation Criterion (t_cool/t_ff)
    Shows thermal stability vs halo mass for CGM
    """
    print("Creating Figure 3a: CGM Precipitation Criterion...")
    
    Snapshot = f'Snap_{snap}'
    z = redshifts[snap]
    
    # Read data
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    tcool_tff = read_hdf(snap_num=Snapshot, param='tcool_over_tff')
    
    # Select centrals with valid data
    mask = (Type == 0) & (Mvir > 1e10) & (tcool_tff > 0) & np.isfinite(tcool_tff)
    
    if np.sum(mask) > dilute:
        indices = sample(range(np.sum(mask)), dilute)
        mask_indices = np.where(mask)[0][indices]
    else:
        mask_indices = np.where(mask)[0]
    
    Mvir_plot = Mvir[mask_indices]
    tcool_tff_plot = tcool_tff[mask_indices]
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Color by tcool/tff value
    scatter = ax.scatter(Mvir_plot, tcool_tff_plot, 
                        c=tcool_tff_plot, cmap='coolwarm_r',
                        norm=LogNorm(vmin=1, vmax=100),
                        s=20, alpha=0.5, edgecolors='none',
                        rasterized=True)
    
    # Add median relation
    mass_bins = np.logspace(10, 14, 20)
    x_c, med, low, upp = calculate_median_relation(
        np.log10(Mvir_plot), 
        np.log10(tcool_tff_plot), 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax.plot(10**x_c[valid], 10**med[valid], color='black', 
           lw=3, label='Median', zorder=5)
    ax.fill_between(10**x_c[valid], 10**low[valid], 10**upp[valid], 
                   color='gray', alpha=0.3, zorder=4)
    
    # Add precipitation threshold
    ax.axhline(10, color='green', ls='--', lw=2.5, 
              label=r'Precipitation threshold ($t_{\rm cool}/t_{\rm ff} = 10$)',
              zorder=6)
    
    # Shade unstable region
    ax.fill_between([1e10, 1e15], 0, 10, 
                   color='green', alpha=0.1, 
                   label='Thermally unstable (precipitation)')
    
    # Add M_shock line
    M_shock = 6e11
    ax.axvline(M_shock, color='black', ls=':', lw=2, alpha=0.5, zorder=3)
    
    # Colorbar
    cbar = plt.colorbar(scatter, ax=ax, pad=0.02)
    cbar.set_label(r'$t_{\rm cool}/t_{\rm ff}$', fontsize=12)
    
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=14)
    ax.set_ylabel(r'$t_{\rm cool}/t_{\rm ff}$ (CGM)', fontsize=14)
    ax.set_xlim(1e10, 1e15)
    ax.set_ylim(0.5, 100)
    ax.legend(loc='upper left', fontsize=10, framealpha=0.9)
    ax.grid(alpha=0.3, ls=':')
    
    ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
           ha='right', va='bottom', fontsize=12,
           bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig3a_cgm_precipitation{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig3a_cgm_precipitation{OutputFormat}')


def plot_figure3b_hot_precipitation(snap=63):
    """
    Figure 3b: Hot Gas Precipitation Criterion (t_cool/t_ff)
    Shows thermal stability vs halo mass for hot gas
    Note: This assumes tcool_tff is computed for hot gas
    """
    print("Creating Figure 3b: Hot Gas Precipitation Criterion...")
    
    Snapshot = f'Snap_{snap}'
    z = redshifts[snap]
    
    # Read data
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    tcool_tff = read_hdf(snap_num=Snapshot, param='tcool_over_tff')
    
    # Select centrals with valid data
    mask = (Type == 0) & (Mvir > 1e10) & (tcool_tff > 0) & np.isfinite(tcool_tff)
    
    if np.sum(mask) > dilute:
        indices = sample(range(np.sum(mask)), dilute)
        mask_indices = np.where(mask)[0][indices]
    else:
        mask_indices = np.where(mask)[0]
    
    Mvir_plot = Mvir[mask_indices]
    tcool_tff_plot = tcool_tff[mask_indices]
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Color by tcool/tff value
    scatter = ax.scatter(Mvir_plot, tcool_tff_plot, 
                        c=tcool_tff_plot, cmap='coolwarm_r',
                        norm=LogNorm(vmin=1, vmax=100),
                        s=20, alpha=0.5, edgecolors='none',
                        rasterized=True)
    
    # Add median relation
    mass_bins = np.logspace(10, 14, 20)
    x_c, med, low, upp = calculate_median_relation(
        np.log10(Mvir_plot), 
        np.log10(tcool_tff_plot), 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax.plot(10**x_c[valid], 10**med[valid], color='black', 
           lw=3, label='Median', zorder=5)
    ax.fill_between(10**x_c[valid], 10**low[valid], 10**upp[valid], 
                   color='gray', alpha=0.3, zorder=4)
    
    # Add precipitation threshold
    ax.axhline(10, color='green', ls='--', lw=2.5, 
              label=r'Precipitation threshold ($t_{\rm cool}/t_{\rm ff} = 10$)',
              zorder=6)
    
    # Shade unstable region
    ax.fill_between([1e10, 1e15], 0, 10, 
                   color='green', alpha=0.1, 
                   label='Thermally unstable (precipitation)')
    
    # Add M_shock line
    M_shock = 6e11
    ax.axvline(M_shock, color='black', ls=':', lw=2, alpha=0.5, zorder=3)
    
    # Colorbar
    cbar = plt.colorbar(scatter, ax=ax, pad=0.02)
    cbar.set_label(r'$t_{\rm cool}/t_{\rm ff}$', fontsize=12)
    
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=14)
    ax.set_ylabel(r'$t_{\rm cool}/t_{\rm ff}$ (Hot gas)', fontsize=14)
    ax.set_xlim(1e10, 1e15)
    ax.set_ylim(0.5, 100)
    ax.legend(loc='upper left', fontsize=10, framealpha=0.9)
    ax.grid(alpha=0.3, ls=':')
    
    ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
           ha='right', va='bottom', fontsize=12,
           bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig3b_hot_precipitation{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig3b_hot_precipitation{OutputFormat}')


def plot_figure4a_cgm_metallicity(snap=63):
    """
    Figure 4a: CGM Metallicity vs Stellar Mass
    """
    print("Creating Figure 4a: CGM Metallicity vs Stellar Mass...")
    
    Snapshot = f'Snap_{snap}'
    z = redshifts[snap]
    
    # Read data
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    MetalsCGM = read_hdf(snap_num=Snapshot, param='MetalsCGMgas') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    
    # Calculate CGM metallicity
    Z_CGM = np.zeros_like(CGMgas)
    mask_gas = CGMgas > 0
    Z_CGM[mask_gas] = MetalsCGM[mask_gas] / CGMgas[mask_gas]
    
    # Convert to solar units (Z_sun = 0.02 by mass)
    Z_sun = 0.02
    log_Z_CGM = np.log10(Z_CGM / Z_sun + 1e-10)  # Add small offset for log
    
    # Select centrals with gas
    mask = (Type == 0) & (StellarMass > 1e9) & (CGMgas > 1e8) & (Z_CGM > 0)
    
    if np.sum(mask) > dilute:
        indices = sample(range(np.sum(mask)), dilute)
        mask_indices = np.where(mask)[0][indices]
    else:
        mask_indices = np.where(mask)[0]
    
    StellarMass_plot = StellarMass[mask_indices]
    log_Z_CGM_plot = log_Z_CGM[mask_indices]
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 8))
    
    ax.scatter(StellarMass_plot, log_Z_CGM_plot, 
              c='cornflowerblue', alpha=0.3, s=15, 
              label='CGM', rasterized=True)
    
    # Add median relation
    mass_bins = np.logspace(9, 12, 20)
    x_c, med, low, upp = calculate_median_relation(
        np.log10(StellarMass_plot), 
        log_Z_CGM_plot, 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax.plot(10**x_c[valid], med[valid], color='blue', 
           lw=3, label='Median', zorder=5)
    ax.fill_between(10**x_c[valid], low[valid], upp[valid], 
                   color='blue', alpha=0.3, zorder=4)
    
    # Add solar metallicity line
    ax.axhline(0, color='gray', ls=':', lw=1.5, 
              label='Solar', alpha=0.7)
    
    ax.set_xscale('log')
    ax.set_xlabel(r'$M_*$ [M$_\odot$]', fontsize=14)
    ax.set_ylabel(r'$\log_{10}(Z_{\rm CGM}/Z_\odot)$', fontsize=14)
    ax.set_xlim(1e9, 5e11)
    ax.set_ylim(-2.5, 0.5)
    ax.legend(loc='lower right', fontsize=10, framealpha=0.9)
    ax.grid(alpha=0.3, ls=':')
    
    ax.text(0.05, 0.95, f'z = {z:.1f}', transform=ax.transAxes,
           ha='left', va='top', fontsize=12,
           bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig4a_cgm_metallicity{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig4a_cgm_metallicity{OutputFormat}')


def plot_figure4b_hot_metallicity(snap=63):
    """
    Figure 4b: Hot Gas Metallicity vs Stellar Mass
    """
    print("Creating Figure 4b: Hot Gas Metallicity vs Stellar Mass...")
    
    Snapshot = f'Snap_{snap}'
    z = redshifts[snap]
    
    # Read data
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    MetalsHot = read_hdf(snap_num=Snapshot, param='MetalsHotGas') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    
    # Calculate Hot gas metallicity
    Z_Hot = np.zeros_like(HotGas)
    mask_gas = HotGas > 0
    Z_Hot[mask_gas] = MetalsHot[mask_gas] / HotGas[mask_gas]
    
    # Convert to solar units (Z_sun = 0.02 by mass)
    Z_sun = 0.02
    log_Z_Hot = np.log10(Z_Hot / Z_sun + 1e-10)  # Add small offset for log
    
    # Select centrals with gas
    mask = (Type == 0) & (StellarMass > 1e9) & (HotGas > 1e8) & (Z_Hot > 0)
    
    if np.sum(mask) > dilute:
        indices = sample(range(np.sum(mask)), dilute)
        mask_indices = np.where(mask)[0][indices]
    else:
        mask_indices = np.where(mask)[0]
    
    StellarMass_plot = StellarMass[mask_indices]
    log_Z_Hot_plot = log_Z_Hot[mask_indices]
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 8))
    
    ax.scatter(StellarMass_plot, log_Z_Hot_plot, 
              c='firebrick', alpha=0.3, s=15, 
              label='Hot gas', rasterized=True)
    
    # Add median relation
    mass_bins = np.logspace(9, 12, 20)
    x_c, med, low, upp = calculate_median_relation(
        np.log10(StellarMass_plot), 
        log_Z_Hot_plot, 
        np.log10(mass_bins)
    )
    valid = ~np.isnan(med)
    ax.plot(10**x_c[valid], med[valid], color='darkred', 
           lw=3, label='Median', zorder=5)
    ax.fill_between(10**x_c[valid], low[valid], upp[valid], 
                   color='darkred', alpha=0.3, zorder=4)
    
    # Add solar metallicity line
    ax.axhline(0, color='gray', ls=':', lw=1.5, 
              label='Solar', alpha=0.7)
    
    ax.set_xscale('log')
    ax.set_xlabel(r'$M_*$ [M$_\odot$]', fontsize=14)
    ax.set_ylabel(r'$\log_{10}(Z_{\rm hot}/Z_\odot)$', fontsize=14)
    ax.set_xlim(1e9, 5e11)
    ax.set_ylim(-2.5, 0.5)
    ax.legend(loc='lower right', fontsize=10, framealpha=0.9)
    ax.grid(alpha=0.3, ls=':')
    
    ax.text(0.05, 0.95, f'z = {z:.1f}', transform=ax.transAxes,
           ha='left', va='top', fontsize=12,
           bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig4b_hot_metallicity{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig4b_hot_metallicity{OutputFormat}')


def plot_figure5a_cgm_evolution():
    """
    Figure 5a: CGM Mass Fraction Evolution
    Shows how CGM evolves with time for different mass bins
    """
    print("Creating Figure 5a: CGM Evolution with Time...")
    
    # Define mass bins
    mass_bins = [
        (1e10, 1e11, 'Low mass\n$(10^{10}-10^{11}$ M$_\\odot)$', 'blue'),
        (1e11, 1e12, 'Milky Way\n$(10^{11}-10^{12}$ M$_\\odot)$', 'green'),
        (1e12, 1e13, 'Massive\n$(10^{12}-10^{13}$ M$_\\odot)$', 'red')
    ]
    
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    
    for ax, (M_low, M_high, label, color) in zip(axes, mass_bins):
        
        # Arrays to store evolution
        times = []
        redshifts_plot = []
        f_cgm_med = []
        f_cgm_low = []
        f_cgm_high = []
        
        # Loop through snapshots
        for snap in range(20, LastSnap+1, 3):  # Every 3rd snapshot to reduce clutter
            Snapshot = f'Snap_{snap}'
            z = redshifts[snap]
            
            try:
                Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
                CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
                Type = read_hdf(snap_num=Snapshot, param='Type')
                
                # Select mass bin
                mask = (Type == 0) & (Mvir >= M_low) & (Mvir < M_high)
                
                if np.sum(mask) > 20:
                    f_baryon = get_cosmic_baryon_fraction()
                    f_cgm = CGMgas[mask] / (Mvir[mask] * f_baryon)
                    
                    # Calculate time (Gyr) - approximate
                    t = 13.8 * (1 - 1/(1+z)**1.5)  # Rough approximation
                    
                    times.append(t)
                    redshifts_plot.append(z)
                    f_cgm_med.append(np.median(f_cgm))
                    f_cgm_low.append(np.percentile(f_cgm, 16))
                    f_cgm_high.append(np.percentile(f_cgm, 84))
            except:
                continue
        
        times = np.array(times)
        redshifts_plot = np.array(redshifts_plot)
        f_cgm_med = np.array(f_cgm_med)
        f_cgm_low = np.array(f_cgm_low)
        f_cgm_high = np.array(f_cgm_high)
        
        # Plot CGM fraction
        ax.plot(times, f_cgm_med, color=color, lw=2.5, 
               label='CGM', zorder=5)
        ax.fill_between(times, f_cgm_low, f_cgm_high, 
                       color=color, alpha=0.3, zorder=4)
        
        # Formatting
        ax.set_xlabel('Time [Gyr]', fontsize=12)
        if ax == axes[0]:
            ax.set_ylabel(r'$f_{\rm CGM} = M_{\rm CGM}/(f_b M_{\rm vir})$', fontsize=12)
        ax.set_xlim(0, 13.8)
        ax.set_ylim(0, 1.2)
        ax.axhline(1.0, color='gray', ls=':', lw=1, alpha=0.7)
        ax.grid(alpha=0.3, ls=':')
        ax.legend(loc='best', fontsize=10)
        ax.set_title(label, fontsize=11)
        
        # Add redshift axis on top
        ax2 = ax.twiny()
        z_ticks = [5, 3, 2, 1, 0.5, 0]
        t_ticks = [13.8 * (1 - 1/(1+z)**1.5) for z in z_ticks]
        ax2.set_xticks(t_ticks)
        ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
        ax2.set_xlim(0, 13.8)
        if ax == axes[1]:
            ax2.set_xlabel('Redshift', fontsize=12)
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig5a_cgm_evolution{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig5a_cgm_evolution{OutputFormat}')


def plot_figure5b_hot_evolution():
    """
    Figure 5b: Hot Gas Mass Fraction Evolution
    Shows how hot gas evolves with time for different mass bins
    """
    print("Creating Figure 5b: Hot Gas Evolution with Time...")
    
    # Define mass bins
    mass_bins = [
        (1e10, 1e11, 'Low mass\n$(10^{10}-10^{11}$ M$_\\odot)$', 'blue'),
        (1e11, 1e12, 'Milky Way\n$(10^{11}-10^{12}$ M$_\\odot)$', 'green'),
        (1e12, 1e13, 'Massive\n$(10^{12}-10^{13}$ M$_\\odot)$', 'red')
    ]
    
    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    
    for ax, (M_low, M_high, label, color) in zip(axes, mass_bins):
        
        # Arrays to store evolution
        times = []
        redshifts_plot = []
        f_hot_med = []
        f_hot_low = []
        f_hot_high = []
        
        # Loop through snapshots
        for snap in range(20, LastSnap+1, 3):  # Every 3rd snapshot to reduce clutter
            Snapshot = f'Snap_{snap}'
            z = redshifts[snap]
            
            try:
                Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
                HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
                Type = read_hdf(snap_num=Snapshot, param='Type')
                
                # Select mass bin
                mask = (Type == 0) & (Mvir >= M_low) & (Mvir < M_high)
                
                if np.sum(mask) > 20:
                    f_baryon = get_cosmic_baryon_fraction()
                    f_hot = HotGas[mask] / (Mvir[mask] * f_baryon)
                    
                    # Calculate time (Gyr) - approximate
                    t = 13.8 * (1 - 1/(1+z)**1.5)  # Rough approximation
                    
                    times.append(t)
                    redshifts_plot.append(z)
                    f_hot_med.append(np.median(f_hot))
                    f_hot_low.append(np.percentile(f_hot, 16))
                    f_hot_high.append(np.percentile(f_hot, 84))
            except:
                continue
        
        times = np.array(times)
        redshifts_plot = np.array(redshifts_plot)
        f_hot_med = np.array(f_hot_med)
        f_hot_low = np.array(f_hot_low)
        f_hot_high = np.array(f_hot_high)
        
        # Plot hot gas fraction
        ax.plot(times, f_hot_med, color=color, lw=2.5, 
               label='Hot gas', zorder=5)
        ax.fill_between(times, f_hot_low, f_hot_high, 
                       color=color, alpha=0.3, zorder=4)
        
        # Formatting
        ax.set_xlabel('Time [Gyr]', fontsize=12)
        if ax == axes[0]:
            ax.set_ylabel(r'$f_{\rm hot} = M_{\rm hot}/(f_b M_{\rm vir})$', fontsize=12)
        ax.set_xlim(0, 13.8)
        ax.set_ylim(0, 1.2)
        ax.axhline(1.0, color='gray', ls=':', lw=1, alpha=0.7)
        ax.grid(alpha=0.3, ls=':')
        ax.legend(loc='best', fontsize=10)
        ax.set_title(label, fontsize=11)
        
        # Add redshift axis on top
        ax2 = ax.twiny()
        z_ticks = [5, 3, 2, 1, 0.5, 0]
        t_ticks = [13.8 * (1 - 1/(1+z)**1.5) for z in z_ticks]
        ax2.set_xticks(t_ticks)
        ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
        ax2.set_xlim(0, 13.8)
        if ax == axes[1]:
            ax2.set_xlabel('Redshift', fontsize=12)
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/fig5b_hot_evolution{OutputFormat}', 
                dpi=300, bbox_inches='tight')
    plt.close()
    print("  Saved to", f'{DirName}plots/fig5b_hot_evolution{OutputFormat}')


def main():
    """Main execution function"""
    
    print("="*60)
    print("CGM Analysis and Plotting for SAGE (Modified)")
    print("="*60)
    
    # Create output directory
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)
        print(f"Created output directory: {OutputDir}")
    
    print(f"\nReading from: {DirName}{FileName}")
    print(f"Output to: {OutputDir}\n")
    
    # Set random seed for reproducibility
    seed(42)
    
    # Generate all figures
    try:
        plot_figure1a_cgm_vs_halo_mass(snap=63)
    except Exception as e:
        print(f"  Error creating Figure 1a: {e}")
    
    try:
        plot_figure1b_hot_vs_halo_mass(snap=63)
    except Exception as e:
        print(f"  Error creating Figure 1b: {e}")
    
    try:
        plot_figure2_baryon_budget(snap=63)
    except Exception as e:
        print(f"  Error creating Figure 2: {e}")
    
    try:
        plot_figure3a_cgm_precipitation(snap=63)
    except Exception as e:
        print(f"  Error creating Figure 3a: {e}")
    
    try:
        plot_figure3b_hot_precipitation(snap=63)
    except Exception as e:
        print(f"  Error creating Figure 3b: {e}")
    
    try:
        plot_figure4a_cgm_metallicity(snap=63)
    except Exception as e:
        print(f"  Error creating Figure 4a: {e}")
    
    try:
        plot_figure4b_hot_metallicity(snap=63)
    except Exception as e:
        print(f"  Error creating Figure 4b: {e}")
    
    try:
        plot_figure5a_cgm_evolution()
    except Exception as e:
        print(f"  Error creating Figure 5a: {e}")
    
    try:
        plot_figure5b_hot_evolution()
    except Exception as e:
        print(f"  Error creating Figure 5b: {e}")
    
    print("\n" + "="*60)
    print("All figures completed!")
    print("="*60)


if __name__ == '__main__':
    main()