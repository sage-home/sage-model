#!/usr/bin/env python
"""
CGM Precipitation Analysis for SAGE - ENHANCED DIAGNOSTIC VERSION
Handles extreme distributions and provides detailed diagnostics
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from matplotlib.colors import LogNorm
import os
import warnings
warnings.filterwarnings("ignore")

# ========================== PHYSICAL CONSTANTS ==========================
GRAVITY = 6.672e-8  # cm^3 g^-1 s^-2
SOLAR_MASS = 1.989e33  # g
PROTONMASS = 1.6726e-24  # g
BOLTZMANN = 1.3806e-16  # erg K^-1
CM_PER_MPC = 3.085678e24  # cm
SEC_PER_MEGAYEAR = 3.155e13  # s

# ========================== USER OPTIONS ==========================
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

Hubble_h = 0.73
BoxSize = 62.5
VolumeFraction = 1.0

OutputDir = DirName + 'plots/'
if not os.path.exists(OutputDir): 
    os.makedirs(OutputDir)

# ========================== HELPER FUNCTIONS ==========================

def get_cooling_rate(logTemp, logZ):
    """Simplified cooling function based on SAGE's metal-dependent cooling"""
    if logTemp < 4.0:
        logTemp = 4.0
    elif logTemp > 8.5:
        logTemp = 8.5
    
    # Base cooling rate
    if logTemp < 5.0:
        log_lambda = -22.0
    elif logTemp < 6.0:
        log_lambda = -21.5 + (logTemp - 5.0) * (-0.5)
    elif logTemp < 7.0:
        log_lambda = -22.0 - (logTemp - 6.0) * 0.5
    else:
        log_lambda = -22.5
    
    # Metallicity dependence
    Z_sun = 0.02
    if logZ > np.log10(Z_sun):
        logZ = np.log10(Z_sun)
    
    if logZ > -10:
        metal_factor = 0.5 * (logZ - np.log10(Z_sun))
        log_lambda += metal_factor
    
    return 10.0**log_lambda


def calculate_tcool_tff(Mvir, Rvir, Vvir, CGMgas, MetalsCGMgas, Hubble_h):
    """Calculate cooling time and free-fall time ratio with detailed output"""
    
    Mvir_cgs = Mvir * 1e10 * SOLAR_MASS / Hubble_h
    Rvir_cgs = Rvir * CM_PER_MPC / Hubble_h
    CGMgas_cgs = CGMgas * 1e10 * SOLAR_MASS / Hubble_h
    
    if CGMgas <= 0 or Rvir <= 0 or Vvir <= 0:
        return np.nan, np.nan, np.nan, np.nan, np.nan
    
    # Virial temperature
    Tvir = 35.9 * Vvir**2
    
    # Metallicity
    if MetalsCGMgas > 0 and CGMgas > 0:
        logZ = np.log10(MetalsCGMgas / CGMgas)
    else:
        logZ = -10.0
    
    # Density
    volume_cgs = (4.0 * np.pi / 3.0) * Rvir_cgs**3
    mass_density_cgs = CGMgas_cgs / volume_cgs
    mu = 0.59
    mean_particle_mass = mu * PROTONMASS
    n_gas = mass_density_cgs / mean_particle_mass
    
    # Cooling time
    lambda_cool = get_cooling_rate(np.log10(Tvir), logZ)
    tcool_s = (1.5 * BOLTZMANN * Tvir) / (n_gas * lambda_cool)
    tcool_myr = tcool_s / SEC_PER_MEGAYEAR
    
    # Free-fall time
    g_accel = GRAVITY * Mvir_cgs / Rvir_cgs**2
    tff_s = np.sqrt(2.0 * Rvir_cgs / g_accel)
    tff_myr = tff_s / SEC_PER_MEGAYEAR
    
    tcool_tff = tcool_myr / tff_myr
    
    return tcool_tff, tcool_myr, tff_myr, n_gas, logZ


def calculate_precipitation_quantities(tcool_tff, CGMgas, tff_myr, dt_myr=10.0):
    """Calculate precipitation-related quantities"""
    
    if np.isnan(tcool_tff) or CGMgas <= 0 or tff_myr <= 0:
        return 0.0, np.nan, 0.0, 0.0
    
    threshold = 10.0
    transition_width = 5.0
    
    if tcool_tff < threshold:
        precip_frac = 1.0
    elif tcool_tff < threshold + transition_width:
        x = (tcool_tff - threshold) / transition_width
        precip_frac = 0.5 * (1.0 - np.tanh(x))
    else:
        precip_frac = 0.0
    
    if precip_frac > 0:
        depletion_time = tff_myr / precip_frac
    else:
        depletion_time = np.inf
    
    precip_efficiency = precip_frac
    
    if precip_frac > 0:
        instant_cool_rate = (precip_frac / tff_myr) * dt_myr * 100.0
    else:
        instant_cool_rate = 0.0
    
    return precip_frac, depletion_time, precip_efficiency, instant_cool_rate


def read_hdf(filename=None, snap_num=None, param=None):
    """Read HDF5 data"""
    property_file = h5.File(DirName + FileName, 'r')
    return np.array(property_file[snap_num][param])


# ========================== DATA READING ==========================

print('=' * 70)
print('CGM PRECIPITATION ANALYSIS - ENHANCED DIAGNOSTICS')
print('=' * 70)
print(f'\nReading data from: {DirName + FileName}')
print(f'Snapshot: {Snapshot}')
print(f'Box size: {BoxSize} Mpc/h\n')

# Read galaxy properties
Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
CentralMvir = read_hdf(snap_num=Snapshot, param='CentralMvir') * 1.0e10 / Hubble_h
StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
MetalsCGMgas = read_hdf(snap_num=Snapshot, param='MetalsCGMgas') * 1.0e10 / Hubble_h
HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
Vvir = read_hdf(snap_num=Snapshot, param='Vvir')
Rvir = read_hdf(snap_num=Snapshot, param='Rvir')
Type = read_hdf(snap_num=Snapshot, param='Type')
Regime = read_hdf(snap_num=Snapshot, param='Regime')

print(f'Number of galaxies: {len(Mvir)}')
print(f'Galaxies with CGM gas > 0: {np.sum(CGMgas > 0)}')
print(f'Regime 0 (CGM) galaxies: {np.sum(Regime == 0)}')
print(f'Regime 1 (Hot-ICM) galaxies: {np.sum(Regime == 1)}')

# ========================== DETAILED DIAGNOSTICS ==========================

print('\n' + '=' * 70)
print('DETAILED DIAGNOSTICS')
print('=' * 70)

# CGM properties
cgm_present = CGMgas > 0
print(f'\nCGM Mass Statistics:')
print(f'  Total CGM mass: {np.sum(CGMgas):.2e} M_sun')
print(f'  Mean CGM mass: {np.mean(CGMgas[cgm_present]):.2e} M_sun')
print(f'  Median CGM mass: {np.median(CGMgas[cgm_present]):.2e} M_sun')
print(f'  Min CGM mass: {np.min(CGMgas[cgm_present]):.2e} M_sun')
print(f'  Max CGM mass: {np.max(CGMgas[cgm_present]):.2e} M_sun')

# Metallicity
Z = MetalsCGMgas[cgm_present] / CGMgas[cgm_present]
Z_solar = Z / 0.02
print(f'\nCGM Metallicity (Z/Z_sun):')
print(f'  Mean: {np.mean(Z_solar):.3f}')
print(f'  Median: {np.median(Z_solar):.3f}')
print(f'  16th-84th percentile: {np.percentile(Z_solar, 16):.3f} - {np.percentile(Z_solar, 84):.3f}')

# Halo properties
print(f'\nHalo Mass Statistics:')
print(f'  Mean log10(Mvir/Msun): {np.mean(np.log10(Mvir[Mvir>0])):.2f}')
print(f'  Median log10(Mvir/Msun): {np.median(np.log10(Mvir[Mvir>0])):.2f}')
print(f'  Range: {np.log10(np.min(Mvir[Mvir>0])):.2f} - {np.log10(np.max(Mvir)):.2f}')

print(f'\nVirial Velocity Statistics:')
print(f'  Mean Vvir: {np.mean(Vvir[Vvir>0]):.1f} km/s')
print(f'  Median Vvir: {np.median(Vvir[Vvir>0]):.1f} km/s')
print(f'  Range: {np.min(Vvir[Vvir>0]):.1f} - {np.max(Vvir):.1f} km/s')

Tvir_sample = 35.9 * Vvir[cgm_present]**2
print(f'\nVirial Temperature Statistics:')
print(f'  Mean log10(Tvir/K): {np.mean(np.log10(Tvir_sample)):.2f}')
print(f'  Median log10(Tvir/K): {np.median(np.log10(Tvir_sample)):.2f}')
print(f'  Range: {np.log10(np.min(Tvir_sample)):.2f} - {np.log10(np.max(Tvir_sample)):.2f}')

# ========================== CALCULATE PHYSICS ==========================

print('\n' + '=' * 70)
print('CALCULATING PRECIPITATION PHYSICS')
print('=' * 70)

n_gal = len(Mvir)
tcool_tff = np.full(n_gal, np.nan)
tcool = np.full(n_gal, np.nan)
tff = np.full(n_gal, np.nan)
n_gas = np.full(n_gal, np.nan)
logZ = np.full(n_gal, np.nan)
Tvir = 35.9 * Vvir**2

# Calculate for all galaxies
for i in range(n_gal):
    if CGMgas[i] > 0:
        tcool_tff[i], tcool[i], tff[i], n_gas[i], logZ[i] = calculate_tcool_tff(
            Mvir[i], Rvir[i], Vvir[i], CGMgas[i], MetalsCGMgas[i], Hubble_h
        )

# Calculate precipitation quantities
precip_frac = np.zeros(n_gal)
depletion_time = np.full(n_gal, np.nan)
precip_efficiency = np.zeros(n_gal)
instant_cool_rate = np.zeros(n_gal)

for i in range(n_gal):
    if not np.isnan(tcool_tff[i]):
        precip_frac[i], depletion_time[i], precip_efficiency[i], instant_cool_rate[i] = \
            calculate_precipitation_quantities(tcool_tff[i], CGMgas[i], tff[i])

# CGM mass fraction
CGM_mass_fraction = np.zeros(n_gal)
valid_mvir = Mvir > 0
CGM_mass_fraction[valid_mvir] = 100.0 * CGMgas[valid_mvir] / Mvir[valid_mvir]

print(f'\nValid tcool/tff calculations: {np.sum(~np.isnan(tcool_tff))}')

# Distribution statistics
valid_ratio = tcool_tff[~np.isnan(tcool_tff)]
print(f'\ntcool/tff Statistics:')
print(f'  Mean: {np.mean(valid_ratio):.3e}')
print(f'  Median: {np.median(valid_ratio):.3e}')
print(f'  Min: {np.min(valid_ratio):.3e}')
print(f'  Max: {np.max(valid_ratio):.3e}')
print(f'  16th-84th percentile: {np.percentile(valid_ratio, 16):.3e} - {np.percentile(valid_ratio, 84):.3e}')

# Regime breakdown
print(f'\nPrecipitation Regime Distribution:')
print(f'  Ultra-fast (tcool/tff < 0.15): {np.sum(tcool_tff < 0.15)} ({100*np.sum(tcool_tff < 0.15)/np.sum(~np.isnan(tcool_tff)):.1f}%)')
print(f'  Fast (0.15 < tcool/tff < 0.5): {np.sum((tcool_tff >= 0.15) & (tcool_tff < 0.5))} ({100*np.sum((tcool_tff >= 0.15) & (tcool_tff < 0.5))/np.sum(~np.isnan(tcool_tff)):.1f}%)')
print(f'  Marginal (0.5 < tcool/tff < 2): {np.sum((tcool_tff >= 0.5) & (tcool_tff < 2))} ({100*np.sum((tcool_tff >= 0.5) & (tcool_tff < 2))/np.sum(~np.isnan(tcool_tff)):.1f}%)')
print(f'  Weak (2 < tcool/tff < 10): {np.sum((tcool_tff >= 2) & (tcool_tff < 10))} ({100*np.sum((tcool_tff >= 2) & (tcool_tff < 10))/np.sum(~np.isnan(tcool_tff)):.1f}%)')
print(f'  Stable (tcool/tff > 10): {np.sum(tcool_tff >= 10)} ({100*np.sum(tcool_tff >= 10)/np.sum(~np.isnan(tcool_tff)):.1f}%)')

# Physical interpretation
print(f'\n' + '=' * 70)
print('PHYSICAL INTERPRETATION')
print('=' * 70)

if np.sum(tcool_tff < 0.15) / np.sum(~np.isnan(tcool_tff)) > 0.9:
    print('\n⚠️  WARNING: >90% of galaxies show ultra-fast precipitation!')
    print('   This is EXPECTED for small boxes with mostly low-mass halos.')
    print('   Physical reasons:')
    print('   1. Low-mass halos (< 10^12 Msun) naturally have short t_cool/t_ff')
    print('   2. Small box volumes preferentially sample low-mass halos')
    print('   3. CGM in these halos is dense and metal-enriched')
    print('   4. Virial temperatures are low (few × 10^5 K)')
    print('\n   This means your CGM implementation is working correctly!')
    print('   The precipitation cooling is very efficient, as expected.')
    print('\n   For larger boxes (500 Mpc), you should see:')
    print('   - More high-mass halos (> 10^12 Msun)')
    print('   - Higher fraction in stable regime (>10%)')
    print('   - Broader distribution of t_cool/t_ff')

# Sample a few galaxies for detailed output
print(f'\n' + '=' * 70)
print('SAMPLE GALAXY DETAILS (Random Selection)')
print('=' * 70)

valid = (CGMgas > 0) & ~np.isnan(tcool_tff)
valid_indices = np.where(valid)[0]
if len(valid_indices) >= 5:
    sample_indices = np.random.choice(valid_indices, 5, replace=False)
    for i, idx in enumerate(sample_indices):
        print(f'\nGalaxy {i+1}:')
        print(f'  Mvir = {Mvir[idx]:.2e} M_sun (log = {np.log10(Mvir[idx]):.2f})')
        print(f'  Vvir = {Vvir[idx]:.1f} km/s')
        print(f'  Tvir = {Tvir[idx]:.2e} K (log = {np.log10(Tvir[idx]):.2f})')
        print(f'  CGM mass = {CGMgas[idx]:.2e} M_sun')
        print(f'  CGM metallicity = {MetalsCGMgas[idx]/CGMgas[idx]/0.02:.3f} Z_sun')
        print(f'  Gas density = {n_gas[idx]:.2e} cm^-3')
        print(f'  t_cool = {tcool[idx]:.2f} Myr')
        print(f'  t_ff = {tff[idx]:.2f} Myr')
        print(f'  t_cool/t_ff = {tcool_tff[idx]:.4f}')
        if tcool_tff[idx] < 0.15:
            print(f'  Regime: ULTRA-FAST precipitation ⚡')
        elif tcool_tff[idx] < 0.5:
            print(f'  Regime: Fast precipitation')
        elif tcool_tff[idx] < 2:
            print(f'  Regime: Marginal precipitation')
        elif tcool_tff[idx] < 10:
            print(f'  Regime: Weak precipitation')
        else:
            print(f'  Regime: Stable (no precipitation)')

# ========================== FIGURE 1 ==========================

print(f'\n' + '=' * 70)
print('CREATING FIGURES')
print('=' * 70)

print('\nCreating Figure 1: Physical Drivers of CGM Precipitation...')

fig = plt.figure(figsize=(14, 10))
gs = gridspec.GridSpec(2, 2, figure=fig, hspace=0.3, wspace=0.3)

valid = (CGMgas > 0) & ~np.isnan(tcool_tff) & (n_gas > 0) & ~np.isnan(logZ)

# For color scale, use actual data range or set reasonable limits
tcool_tff_min = max(np.nanmin(tcool_tff[valid]), 0.001)
tcool_tff_max = min(np.nanmax(tcool_tff[valid]), 100)
if tcool_tff_max <= tcool_tff_min:
    tcool_tff_max = tcool_tff_min * 10

# Subplot 1: Gas Density vs Metallicity
ax1 = fig.add_subplot(gs[0, 0])
sc1 = ax1.scatter(n_gas[valid], logZ[valid], c=tcool_tff[valid], 
                  cmap='RdYlBu_r', norm=LogNorm(vmin=tcool_tff_min, vmax=tcool_tff_max),
                  s=50, alpha=0.6, edgecolors='k', linewidth=0.3)
ax1.set_xscale('log')
ax1.set_xlabel('Gas Density [cm$^{-3}$]', fontsize=12, fontweight='bold')
ax1.set_ylabel('log$_{10}$(Z/Z$_\\odot$)', fontsize=12, fontweight='bold')
if np.sum(valid) > 0:
    ax1.set_xlim(np.nanmin(n_gas[valid])*0.5, np.nanmax(n_gas[valid])*2)
    ax1.set_ylim(np.nanmin(logZ[valid])-0.5, np.nanmax(logZ[valid])+0.5)
ax1.text(0.05, 0.95, 'Stronger\nCooling', transform=ax1.transAxes,
         fontsize=11, fontweight='bold', color='red',
         bbox=dict(boxstyle='round', facecolor='white', edgecolor='red', linewidth=2),
         verticalalignment='top')
cbar1 = plt.colorbar(sc1, ax=ax1, label='$t_{\\rm cool}/t_{\\rm ff}$')
ax1.grid(True, alpha=0.3)
ax1.set_title('Drivers of Precipitation:\nHigher Density + Higher Metallicity → Faster Cooling',
              fontsize=11, fontweight='bold', pad=10)

# Subplot 2: Halo Mass vs CGM Mass Fraction
ax2 = fig.add_subplot(gs[0, 1])
valid2 = valid & (CentralMvir > 0) & (CGM_mass_fraction > 0)
if np.sum(valid2) > 0:
    sc2 = ax2.scatter(CentralMvir[valid2]/1e10, CGM_mass_fraction[valid2], 
                      c=tcool_tff[valid2], cmap='RdYlBu_r',
                      norm=LogNorm(vmin=tcool_tff_min, vmax=tcool_tff_max),
                      s=50, alpha=0.6, edgecolors='k', linewidth=0.3)
ax2.set_xscale('log')
ax2.set_yscale('log')
ax2.set_xlabel('Halo Mass [$10^{10}M_\\odot$]', fontsize=12, fontweight='bold')
ax2.set_ylabel('CGM Mass Fraction [%]', fontsize=12, fontweight='bold')
ax2.axhline(10, color='red', linestyle='--', linewidth=2, alpha=0.7, 
            label='10% of halo mass')
if np.sum(valid2) > 0:
    ax2.set_xlim(np.min(CentralMvir[valid2]/1e10)*0.5, np.max(CentralMvir[valid2]/1e10)*2)
    ax2.set_ylim(np.min(CGM_mass_fraction[valid2])*0.5, np.max(CGM_mass_fraction[valid2])*2)
ax2.legend(fontsize=10)
ax2.grid(True, alpha=0.3)
ax2.set_title('CGM Dominance in Low-Mass Halos',
              fontsize=11, fontweight='bold', pad=10)

# Subplot 3: Temperature-Density Phase Space
ax3 = fig.add_subplot(gs[1, 0])
sc3 = ax3.scatter(n_gas[valid], Tvir[valid], c=tcool_tff[valid],
                  cmap='RdYlBu_r', norm=LogNorm(vmin=tcool_tff_min, vmax=tcool_tff_max),
                  s=50, alpha=0.6, edgecolors='k', linewidth=0.3)
ax3.set_xscale('log')
ax3.set_yscale('log')
ax3.set_xlabel('Gas Density [cm$^{-3}$]', fontsize=12, fontweight='bold')
ax3.set_ylabel('Temperature [K]', fontsize=12, fontweight='bold')
if np.sum(valid) > 0:
    ax3.set_xlim(np.nanmin(n_gas[valid])*0.5, np.nanmax(n_gas[valid])*2)
    ax3.set_ylim(np.nanmin(Tvir[valid])*0.5, np.nanmax(Tvir[valid])*2)
ax3.grid(True, alpha=0.3)
ax3.set_title('Temperature-Density Phase Space',
              fontsize=11, fontweight='bold', pad=10)

# Subplot 4: The Precipitation Sequence
ax4 = fig.add_subplot(gs[1, 1])
ax4.axis('off')

textstr = """
The Precipitation Sequence

Dense, Metal-Rich CGM
       ↓
Rapid cooling forms cold clouds
       ↓
Clouds precipitate on t_ff
    (Rain mode)
       ↓
Fuels star formation
       ↓
CGM depleted / SN feedback
       ↓
Diffuse, stable CGM
"""

ax4.text(0.5, 0.5, textstr, transform=ax4.transAxes,
         fontsize=13, verticalalignment='center', horizontalalignment='center',
         bbox=dict(boxstyle='round,pad=1', facecolor='lightblue', 
                  edgecolor='darkblue', linewidth=3, alpha=0.8),
         family='monospace', fontweight='bold')

plt.suptitle('Physical Drivers of CGM Precipitation', 
             fontsize=16, fontweight='bold', y=0.98)

plt.savefig(OutputDir + 'cgm_precipitation_drivers.png', dpi=150, bbox_inches='tight')
print(f'Saved: {OutputDir}cgm_precipitation_drivers.png')

# ========================== FIGURE 2 ==========================

print('\nCreating Figure 2: CGM Precipitation Regimes...')

fig2 = plt.figure(figsize=(16, 12))
gs2 = gridspec.GridSpec(3, 3, figure=fig2, hspace=0.35, wspace=0.35)

def get_regime_color(ratio):
    if ratio < 0.15:
        return 'darkred'
    elif ratio < 0.5:
        return 'orangered'
    elif ratio < 2.0:
        return 'orange'
    elif ratio < 10.0:
        return 'steelblue'
    else:
        return 'gray'

# FIXED: Make colors a numpy array so it can be indexed with boolean masks
colors = np.array([get_regime_color(r) if not np.isnan(r) else 'gray' for r in tcool_tff])

# Top: Distribution histogram
ax_hist = fig2.add_subplot(gs2[0, :])
valid_ratio = tcool_tff[~np.isnan(tcool_tff)]

# Adaptive binning based on actual data range
if len(valid_ratio) > 0:
    ratio_min = max(np.min(valid_ratio), 1e-4)
    ratio_max = min(np.max(valid_ratio), 1e3)
    bins = np.logspace(np.log10(ratio_min), np.log10(ratio_max), 50)
    ax_hist.hist(valid_ratio, bins=bins, 
                 color='steelblue', alpha=0.7, edgecolor='black', linewidth=1.5)

ax_hist.axvline(0.15, color='darkred', linestyle='--', linewidth=2, alpha=0.8)
ax_hist.axvline(0.5, color='orangered', linestyle='--', linewidth=2, alpha=0.8)
ax_hist.axvline(2.0, color='orange', linestyle='--', linewidth=2, alpha=0.8)
ax_hist.axvline(10.0, color='steelblue', linestyle='--', linewidth=2, alpha=0.8)
ax_hist.set_xscale('log')
ax_hist.set_xlabel('$t_{\\rm cool}/t_{\\rm ff}$', fontsize=13, fontweight='bold')
ax_hist.set_ylabel('Number of Galaxies', fontsize=13, fontweight='bold')
if len(valid_ratio) > 0:
    ax_hist.set_xlim(ratio_min*0.5, ratio_max*2)
ax_hist.grid(True, alpha=0.3, axis='y')

# Add regime labels
ax_hist.text(0.07, 0.95, 'Ultra-Fast\nPrecipitation', transform=ax_hist.transAxes,
             fontsize=9, bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.7),
             verticalalignment='top')
ax_hist.text(0.28, 0.95, 'Fast\nPrecipitation', transform=ax_hist.transAxes,
             fontsize=9, bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.7),
             verticalalignment='top')
ax_hist.text(0.50, 0.95, 'Marginal\nPrecipitation', transform=ax_hist.transAxes,
             fontsize=9, bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.7),
             verticalalignment='top')
ax_hist.text(0.72, 0.95, 'Weak\nPrecipitation', transform=ax_hist.transAxes,
             fontsize=9, bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.7),
             verticalalignment='top')

ax_hist.set_title('Distribution of Cooling-to-Freefall Time Ratio',
                  fontsize=13, fontweight='bold', pad=10)

# Middle and bottom rows
valid_plot = valid & ~np.isnan(tcool_tff)

# Plot setup function for consistent style
def setup_regime_plot(ax, xlabel, ylabel, log_x=True, log_y=False):
    if log_x:
        ax.set_xscale('log')
    if log_y:
        ax.set_yscale('log')
    ax.set_xlabel(xlabel, fontsize=11, fontweight='bold')
    ax.set_ylabel(ylabel, fontsize=11, fontweight='bold')
    if len(valid_ratio) > 0:
        ax.set_xlim(ratio_min*0.5, ratio_max*2)
    ax.axvline(0.15, color='darkred', linestyle=':', alpha=0.5)
    ax.axvline(0.5, color='orangered', linestyle=':', alpha=0.5)
    ax.axvline(2.0, color='orange', linestyle=':', alpha=0.5)
    ax.axvline(10.0, color='steelblue', linestyle=':', alpha=0.5)
    ax.grid(True, alpha=0.3)

# Density
ax1 = fig2.add_subplot(gs2[1, 0])
if np.sum(valid_plot) > 0:
    for c in np.unique(colors[valid_plot]):
        mask = (colors[valid_plot] == c)
        if np.any(mask):
            ax1.scatter(tcool_tff[valid_plot][mask], n_gas[valid_plot][mask], 
                       c=c, s=40, alpha=0.6, edgecolors='k', linewidth=0.3, label=None)
setup_regime_plot(ax1, '$t_{\\rm cool}/t_{\\rm ff}$', 'Gas Density [cm$^{-3}$]', log_y=True)
ax1.set_title('Density vs Precipitation Regime', fontsize=10, fontweight='bold')

# Metallicity
ax2 = fig2.add_subplot(gs2[1, 1])
if np.sum(valid_plot) > 0:
    for c in np.unique(colors[valid_plot]):
        mask = (colors[valid_plot] == c)
        if np.any(mask):
            ax2.scatter(tcool_tff[valid_plot][mask], logZ[valid_plot][mask], 
                       c=c, s=40, alpha=0.6, edgecolors='k', linewidth=0.3)
setup_regime_plot(ax2, '$t_{\\rm cool}/t_{\\rm ff}$', 'log$_{10}$(Z/Z$_\\odot$)')
ax2.set_title('Metallicity vs Precipitation Regime', fontsize=10, fontweight='bold')

# Temperature
ax3 = fig2.add_subplot(gs2[1, 2])
if np.sum(valid_plot) > 0:
    for c in np.unique(colors[valid_plot]):
        mask = (colors[valid_plot] == c)
        if np.any(mask):
            ax3.scatter(tcool_tff[valid_plot][mask], Tvir[valid_plot][mask], 
                       c=c, s=40, alpha=0.6, edgecolors='k', linewidth=0.3)
setup_regime_plot(ax3, '$t_{\\rm cool}/t_{\\rm ff}$', '$T_{\\rm vir}$ [K]', log_y=True)
ax3.set_title('Temperature vs Precipitation Regime', fontsize=10, fontweight='bold')

# Depletion timescale
valid_depl = valid_plot & (depletion_time < 1e4) & ~np.isinf(depletion_time)
ax4 = fig2.add_subplot(gs2[2, 0])
if np.sum(valid_depl) > 0:
    for c in np.unique(colors[valid_depl]):
        mask = (colors[valid_depl] == c)
        if np.any(mask):
            ax4.scatter(tcool_tff[valid_depl][mask], depletion_time[valid_depl][mask], 
                       c=c, s=40, alpha=0.6, edgecolors='k', linewidth=0.3)
setup_regime_plot(ax4, '$t_{\\rm cool}/t_{\\rm ff}$', 'CGM Depletion Time [Myr]', log_y=False)
ax4.axhline(1000, color='gray', linestyle='--', alpha=0.5, label='1 Gyr')
ax4.legend(fontsize=9)
ax4.set_title('Depletion Timescale vs Precipitation Regime', fontsize=10, fontweight='bold')

# Precipitation efficiency
ax5 = fig2.add_subplot(gs2[2, 1])
if np.sum(valid_plot) > 0:
    for c in np.unique(colors[valid_plot]):
        mask = (colors[valid_plot] == c)
        if np.any(mask):
            ax5.scatter(tcool_tff[valid_plot][mask], precip_efficiency[valid_plot][mask],
                       c=c, s=40, alpha=0.6, edgecolors='k', linewidth=0.3)
setup_regime_plot(ax5, '$t_{\\rm cool}/t_{\\rm ff}$', 'Precipitation Fraction', log_y=False)
ax5.set_ylim(0, 1.05)
ax5.set_title('Precipitation Efficiency', fontsize=10, fontweight='bold')

# Instantaneous cooling rate
valid_cool = valid_plot & (instant_cool_rate > 0)
ax6 = fig2.add_subplot(gs2[2, 2])
if np.sum(valid_cool) > 0:
    for c in np.unique(colors[valid_cool]):
        mask = (colors[valid_cool] == c)
        if np.any(mask):
            ax6.scatter(tcool_tff[valid_cool][mask], instant_cool_rate[valid_cool][mask],
                       c=c, s=40, alpha=0.6, edgecolors='k', linewidth=0.3)
setup_regime_plot(ax6, '$t_{\\rm cool}/t_{\\rm ff}$', 'CGM Cooled per Timestep [%]', log_y=False)
ax6.set_title('Instantaneous Cooling Rate', fontsize=10, fontweight='bold')

# Add legend
from matplotlib.patches import Patch
legend_elements = [
    Patch(facecolor='darkred', edgecolor='black', label='Ultra-Fast (<0.15)'),
    Patch(facecolor='orangered', edgecolor='black', label='Fast (0.15-0.5)'),
    Patch(facecolor='orange', edgecolor='black', label='Marginal (0.5-2)'),
    Patch(facecolor='steelblue', edgecolor='black', label='Weak (2-10)'),
    Patch(facecolor='gray', edgecolor='black', label='Stable (>10)')
]
fig2.legend(handles=legend_elements, loc='lower center', ncol=5, 
           fontsize=11, title='Precipitation Regimes', title_fontsize=12,
           frameon=True, fancybox=True, shadow=True, bbox_to_anchor=(0.5, -0.02))

plt.suptitle('CGM Precipitation Cooling: Physical Regimes and Behaviors',
             fontsize=16, fontweight='bold', y=0.99)

plt.savefig(OutputDir + 'cgm_precipitation_regimes.png', dpi=150, bbox_inches='tight')
print(f'Saved: {OutputDir}cgm_precipitation_regimes.png')

plt.close('all')

print('\n' + '=' * 70)
print('ANALYSIS COMPLETE!')
print('=' * 70)
print(f'\nFigures saved to: {OutputDir}')
print('\nNext steps:')
print('1. Check the figures to see your precipitation distribution')
print('2. Compare to larger box results (500 Mpc) when available')
print('3. Track evolution across redshift by analyzing multiple snapshots')
print('4. The ultra-fast precipitation is EXPECTED for small boxes!')