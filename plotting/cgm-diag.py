#!/usr/bin/env python

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
from collections import defaultdict
from scipy import stats
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
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 1000       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

OutputFormat = '.png'
plt.rcParams["figure.figsize"] = (8.34,6.25)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 14


# ==================================================================

def read_hdf(filename = None, snap_num = None, param = None):

    property = h5.File(DirName+FileName,'r')
    return np.array(property[snap_num][param])


# ==================================================================

seed(2222)
volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

OutputDir = DirName + 'plots/'
if not os.path.exists(OutputDir): os.makedirs(OutputDir)

# Read galaxy properties
print('Reading galaxy properties from', DirName+FileName)

CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
BulgeMass = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
BlackHoleMass = read_hdf(snap_num = Snapshot, param = 'BlackHoleMass') * 1.0e10 / Hubble_h
ColdGas = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
MetalsColdGas = read_hdf(snap_num = Snapshot, param = 'MetalsColdGas') * 1.0e10 / Hubble_h
MetalsEjectedMass = read_hdf(snap_num = Snapshot, param = 'MetalsEjectedMass') * 1.0e10 / Hubble_h
HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
MetalsHotGas = read_hdf(snap_num = Snapshot, param = 'MetalsHotGas') * 1.0e10 / Hubble_h
EjectedMass = read_hdf(snap_num = Snapshot, param = 'EjectedMass') * 1.0e10 / Hubble_h
CGMgas = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
MetalsCGMgas = read_hdf(snap_num = Snapshot, param = 'MetalsCGMgas') * 1.0e10 / Hubble_h
IntraClusterStars = read_hdf(snap_num = Snapshot, param = 'IntraClusterStars') * 1.0e10 / Hubble_h
DiskRadius = read_hdf(snap_num = Snapshot, param = 'DiskRadius')
H2gas = read_hdf(snap_num = Snapshot, param = 'H2gas') * 1.0e10 / Hubble_h

Vvir = read_hdf(snap_num = Snapshot, param = 'Vvir')
Vmax = read_hdf(snap_num = Snapshot, param = 'Vmax')
Rvir = read_hdf(snap_num = Snapshot, param = 'Rvir')

SfrDisk = read_hdf(snap_num = Snapshot, param = 'SfrDisk')
SfrBulge = read_hdf(snap_num = Snapshot, param = 'SfrBulge')

CentralGalaxyIndex = read_hdf(snap_num = Snapshot, param = 'CentralGalaxyIndex')
Type = read_hdf(snap_num = Snapshot, param = 'Type')

Posx = read_hdf(snap_num = Snapshot, param = 'Posx')
Posy = read_hdf(snap_num = Snapshot, param = 'Posy')
Posz = read_hdf(snap_num = Snapshot, param = 'Posz')

OutflowRate = read_hdf(snap_num = Snapshot, param = 'OutflowRate')
MassLoading = read_hdf(snap_num = Snapshot, param = 'MassLoading')

Tvir = 35.9 * (Vvir)**2  # in Kelvin
Tmax = 2.5e5  # K, corresponds to Vvir ~52.7 km/s
Regime = read_hdf(snap_num = Snapshot, param = 'Regime')
tcool = read_hdf(snap_num = Snapshot, param = 'tcool')
tff = read_hdf(snap_num = Snapshot, param = 'tff')
tcool_over_tff = read_hdf(snap_num = Snapshot, param = 'tcool_over_tff')
tdeplete = read_hdf(snap_num = Snapshot, param = 'tdeplete')

unit_time_in_s = 3.08568e+24 / 100000
sec_per_year = 3.155e+7
solar_mass_in_g = 1.989e+33
cm_per_mpc = 3.085678e+24

# Check the constants - these seem to be causing overflow
print(f'Checking constants:')
print(f'solar_mass_in_g: {solar_mass_in_g:.3e}')
print(f'cm_per_mpc: {cm_per_mpc:.3e}')

# Test a typical value to see if it overflows
test_cgm = 1.0  # 1 solar mass
test_rvir = 0.1  # 0.1 Mpc/h
print(f'Test: 1 solar mass * solar_mass_in_g = {test_cgm * solar_mass_in_g:.3e}')
print(f'Test: 0.1 Mpc/h * cm_per_mpc / Hubble_h = {test_rvir * cm_per_mpc / Hubble_h:.3e}')

tcool = tcool * unit_time_in_s / (1e6 * sec_per_year)
tff = tff * unit_time_in_s / (1e6 * sec_per_year)
tdeplete = tdeplete * unit_time_in_s / (1e6 * sec_per_year)

# Calculate precipitation factor (McCourt et al. 2012)
precipitation_threshold = 10.0  # McCourt et al. 2012
transition_width = 2.0  # Smooth transition over factor ~2

precipitation_fraction = np.zeros_like(tcool_over_tff)

# Case 1: tcool_over_tff < precipitation_threshold
mask1 = tcool_over_tff < precipitation_threshold
instability_factor = precipitation_threshold / tcool_over_tff[mask1]
instability_factor = np.minimum(instability_factor, 3.0)  # Cap at 3x
precipitation_fraction[mask1] = np.tanh(instability_factor / 2.0)  # Smooth scaling

# Debug unit conversions - work with raw units first
print(f'Sample Rvir values (raw): {Rvir[:5]}')
print(f'Sample CGMgas values (raw): {CGMgas[:5]}')
print(f'Max CGMgas value: {np.max(CGMgas):.2e}')
print(f'Min CGMgas value: {np.min(CGMgas):.2e}')
print(f'Number of galaxies with CGMgas > 0: {np.sum(CGMgas > 0)}')

# Let's also check other gas components for comparison
print(f'Sample ColdGas values: {ColdGas[:5]}')
print(f'Sample HotGas values: {HotGas[:5]}')
print(f'Sample EjectedMass values: {EjectedMass[:5]}')

# Check typical ratios
nonzero_mask = CGMgas > 0
if np.sum(nonzero_mask) > 0:
    print(f'Median CGMgas/ColdGas ratio: {np.median(CGMgas[nonzero_mask]/ColdGas[nonzero_mask]):.2e}')
    print(f'Median CGMgas/HotGas ratio: {np.median(CGMgas[nonzero_mask]/HotGas[nonzero_mask]):.2e}')

# Work with CGM mass in solar masses (no conversion yet)
print(f'Median CGM mass in CGM-regime galaxies: {np.median(CGMgas[Regime==0]):.2e} solar masses')

# Work with Rvir in Mpc/h (no conversion yet) 
print(f'Median Rvir in CGM-regime galaxies: {np.median(Rvir[Regime==0]):.2e} Mpc/h')

# Calculate volume in (Mpc/h)^3 - this is the FULL halo volume
volume_cgm_mpc = (4.0 * np.pi / 3.0) * (Rvir**3)
print(f'Median CGM volume in CGM-regime galaxies: {np.median(volume_cgm_mpc[Regime==0]):.2e} (Mpc/h)^3')

# Maybe we should use a different radius? CGM typically extends from ~0.1*Rvir to Rvir
# Let's try calculating density assuming CGM occupies the volume from 0.1*Rvir to Rvir
volume_cgm_shell = (4.0 * np.pi / 3.0) * (Rvir**3 - (0.1*Rvir)**3)
print(f'CGM shell volume (0.1*Rvir to Rvir): {np.median(volume_cgm_shell[Regime==0]):.2e} (Mpc/h)^3')

# Calculate density in solar masses per (Mpc/h)^3
mask_nonzero = (CGMgas > 0) & (Rvir > 0)
density_cgm_raw = np.zeros_like(CGMgas)
density_cgm_raw[mask_nonzero] = CGMgas[mask_nonzero] / volume_cgm_mpc[mask_nonzero]

# Also try with CGM shell volume
density_cgm_shell = np.zeros_like(CGMgas)
density_cgm_shell[mask_nonzero] = CGMgas[mask_nonzero] / volume_cgm_shell[mask_nonzero]

# Convert to physical units (g/cm^3) first, then to number density (cm^-3)
conversion_factor = solar_mass_in_g / (cm_per_mpc / Hubble_h)**3
density_physical_mass = density_cgm_raw * conversion_factor
density_physical_shell_mass = density_cgm_shell * conversion_factor

# Convert mass density to number density (assuming hydrogen)
proton_mass = 1.67e-24  # g
density_physical = density_physical_mass / proton_mass  # particles/cm^3
density_physical_shell = density_physical_shell_mass / proton_mass  # particles/cm^3

finite_density_mask = mask_nonzero & (Regime == 0) & np.isfinite(density_cgm_raw)
if np.sum(finite_density_mask) > 0:
    print(f'Median CGM density (full halo) in CGM-regime galaxies: {np.median(density_cgm_raw[finite_density_mask]):.2e} solar masses/(Mpc/h)^3')
    print(f'Median CGM density (shell 0.1-1 Rvir) in CGM-regime galaxies: {np.median(density_cgm_shell[finite_density_mask]):.2e} solar masses/(Mpc/h)^3')
    print(f'Number of galaxies with finite CGM density: {np.sum(finite_density_mask)}')
    
    print(f'Conversion factor: {conversion_factor:.2e}')
    print(f'Median CGM mass density (full halo): {np.median(density_physical_mass[finite_density_mask]):.2e} g/cm^3')
    print(f'Median CGM mass density (shell): {np.median(density_physical_shell_mass[finite_density_mask]):.2e} g/cm^3')
    
    print(f'Median CGM number density (full halo): {np.median(density_physical[finite_density_mask]):.2e} particles/cm^3')
    print(f'Median CGM number density (shell): {np.median(density_physical_shell[finite_density_mask]):.2e} particles/cm^3')
else:
    print('No finite, non-zero CGM density values in CGM-regime galaxies')

# Case 2: transition regime
mask2 = (tcool_over_tff >= precipitation_threshold) & (tcool_over_tff < precipitation_threshold + transition_width)
x = (tcool_over_tff[mask2] - precipitation_threshold) / transition_width
precipitation_fraction[mask2] = 0.5 * (1.0 - np.tanh(x))

# Case 3: tcool_over_tff >= precipitation_threshold + transition_width
# precipitation_fraction remains 0.0 (already initialized)

print(f'Median cooling time in CGM-regime galaxies: {np.median(tcool[Regime==0]):.2e} Myr')
print(f'Median free-fall time in CGM-regime galaxies: {np.median(tff[Regime==0]):.2e} Myr')
print(f'Median cooling time over free-fall time in CGM-regime galaxies: {np.median(tcool_over_tff[Regime==0]):.2e}')
print(f'Median depletion time in CGM-regime galaxies: {np.median(tdeplete[Regime==0]):.2e} Myr')
print(f'Median precipitation fraction in CGM-regime galaxies: {np.median(precipitation_fraction[Regime==0]):.3f}')

print(f'Sample of cooling times: {sample(list(tcool[Regime==0]), 10)}')
print(f'Sample of free-fall times: {sample(list(tff[Regime==0]), 10)}')
print(f'Sample of tcool/tff: {sample(list(tcool_over_tff[Regime==0]), 10)}')
print(f'Sample of depletion times: {sample(list(tdeplete[Regime==0]), 10)}')
print(f'Sample of precipitation fractions: {sample(list(precipitation_fraction[Regime==0]), 10)}')
if np.sum(finite_density_mask) > 0:
    print(f'Sample of CGM densities: {sample(list(density_cgm_raw[finite_density_mask]), min(10, len(density_cgm_raw[finite_density_mask])))}')
else:
    print('Sample of CGM densities: No finite densities available')

# ========================== PLOTTING ==========================

# Create the multi-panel figure
fig = plt.figure(figsize=(18, 10))

# Define precipitation regimes based on tcool/tff
ultra_fast_mask = tcool_over_tff < 0.15
fast_mask = (tcool_over_tff >= 0.15) & (tcool_over_tff < 0.5)
marginal_mask = (tcool_over_tff >= 0.5) & (tcool_over_tff < 2.0)
weak_mask = (tcool_over_tff >= 2.0) & (tcool_over_tff < 10.0)
stable_mask = tcool_over_tff >= 10.0

# Colors for each regime
colors = {
    'ultra_fast': '#ff6b6b',    # Red
    'fast': '#ffa500',          # Orange  
    'marginal': '#ffd700',      # Gold
    'weak': '#87ceeb',          # Light blue
    'stable': '#808080'         # Gray
}

# Only plot CGM-regime galaxies with finite values (for scatter plots)
cgm_mask = (Regime == 0) & mask_nonzero & np.isfinite(density_physical) & (density_physical > 0)

# For histogram, use ALL CGM-regime galaxies
cgm_hist_mask = (Regime == 0)

# Debug: Check how many galaxies we have in each regime
print(f'Total galaxies: {len(Regime)}')
print(f'Regime values: {np.unique(Regime, return_counts=True)}')
print(f'CGM-regime galaxies (Regime=0): {np.sum(Regime == 0)}')
print(f'Galaxies with CGMgas > 0: {np.sum(CGMgas > 0)}')
print(f'CGM-regime galaxies with CGMgas > 0: {np.sum((Regime == 0) & (CGMgas > 0))}')

# Panel 1: Histogram of tcool/tff distribution (spanning all 3 columns)
ax1 = plt.subplot2grid((3, 3), (0, 0), colspan=3)
bins = np.logspace(-2, 2.5, 30)
hist_data = tcool_over_tff[cgm_hist_mask]
n, bins_out, patches = ax1.hist(hist_data, bins=bins, alpha=1.0, color='lightblue', edgecolor='black', linewidth=0.5)

print(f'Histogram showing {len(hist_data)} CGM-regime galaxies (Regime=0)')

# Keep histogram bars light blue with black outlines (don't recolor them)

# Add vertical lines for regime boundaries (grey dashed lines)
ax1.axvline(0.15, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax1.axvline(0.5, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax1.axvline(2.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax1.axvline(10.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax1.axvline(10.0, color='lightblue', linestyle='--', alpha=0.7)

ax1.set_xscale('log')
ax1.set_xlim(0.01, 100)
ax1.set_xlabel('$t_{cool}/t_{ff}$')
ax1.set_ylabel('Number of Galaxies')
ax1.set_title('CGM Precipitation Cooling: Physical Regimes and Behaviors\n\nDistribution of Cooling-to-Freefall Time Ratio')
ax1.grid(True, alpha=0.3)

# Add regime labels
ax1.text(0.05, 0.9, 'Ultra-Fast\nPrecipitation', transform=ax1.transAxes, fontsize=10, 
         bbox=dict(boxstyle="round,pad=0.3", facecolor=colors['ultra_fast'], alpha=0.7))
ax1.text(0.2, 0.9, 'Fast\nPrecipitation', transform=ax1.transAxes, fontsize=10,
         bbox=dict(boxstyle="round,pad=0.3", facecolor=colors['fast'], alpha=0.7))
ax1.text(0.4, 0.9, 'Marginal\nPrecipitation', transform=ax1.transAxes, fontsize=10,
         bbox=dict(boxstyle="round,pad=0.3", facecolor=colors['marginal'], alpha=0.7))
ax1.text(0.6, 0.9, 'Weak\nPrecipitation', transform=ax1.transAxes, fontsize=10,
         bbox=dict(boxstyle="round,pad=0.3", facecolor=colors['weak'], alpha=0.7))
ax1.text(0.8, 0.9, 'Stable', transform=ax1.transAxes, fontsize=10,
         bbox=dict(boxstyle="round,pad=0.3", facecolor=colors['stable'], alpha=0.7))

# Panel 2: Density vs Precipitation Regime  
ax2 = plt.subplot2grid((3, 3), (1, 0))

# Dilute the data for scatter plots - randomly sample 7000 points from cgm_mask
np.random.seed(2222)  # For reproducibility
if np.sum(cgm_mask) > dilute:
    dilute_indices = np.random.choice(np.where(cgm_mask)[0], dilute, replace=False)
    dilute_mask = np.zeros_like(cgm_mask, dtype=bool)
    dilute_mask[dilute_indices] = True
else:
    dilute_mask = cgm_mask

print(f'Using {np.sum(dilute_mask)} galaxies for scatter plots (diluted from {np.sum(cgm_mask)})')

scatter_masks = [
    (ultra_fast_mask & dilute_mask, colors['ultra_fast'], 'Ultra-Fast'),
    (fast_mask & dilute_mask, colors['fast'], 'Fast'),
    (marginal_mask & dilute_mask, colors['marginal'], 'Marginal'),
    (weak_mask & dilute_mask, colors['weak'], 'Weak'),
    (stable_mask & dilute_mask, colors['stable'], 'Stable')
]

for mask, color, label in scatter_masks:
    if np.sum(mask) > 0:
        ax2.scatter(tcool_over_tff[mask], density_physical[mask], 
                   c=color, alpha=0.6, s=50, marker='o', edgecolor='black', label=label)

# Add regime boundary lines
ax2.axvline(0.15, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax2.axvline(0.5, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax2.axvline(2.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax2.axvline(10.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)

ax2.set_xscale('log')
ax2.set_yscale('log')
ax2.set_xlim(0.01, 100)
ax2.set_ylim(1e-8, 1e-2)
ax2.set_xlabel('$t_{cool}/t_{ff}$')
ax2.set_ylabel('Number Density (cm$^{-3}$)')
ax2.set_title('Number Density')
ax2.grid(True, alpha=0.3)

# Panel 3: Metallicity vs Precipitation Regime
ax3 = plt.subplot2grid((3, 3), (1, 1))
# Calculate metallicity in 12 + log10(O/H) scale (assuming solar metallicity = 0.02)
metallicity = np.log10((MetalsCGMgas / CGMgas) / 0.02) + 9.0
metallicity[~np.isfinite(metallicity)] = 0.0  # Set invalid values to very low

for mask, color, label in scatter_masks:
    if np.sum(mask) > 0:
        valid_met = mask & (MetalsCGMgas > 0) & (CGMgas > 0)
        if np.sum(valid_met) > 0:
            ax3.scatter(tcool_over_tff[valid_met], metallicity[valid_met], 
                       c=color, alpha=0.6, s=50, marker='o', edgecolor='black')

# Add regime boundary lines
ax3.axvline(0.15, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax3.axvline(0.5, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax3.axvline(2.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax3.axvline(10.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)

ax3.set_xscale('log')
ax3.set_xlim(0.01, 100)
ax3.set_ylim(5, 9)
ax3.set_xlabel('$t_{cool}/t_{ff}$')
ax3.set_ylabel('12 + log$_{10}$(O/H)')
ax3.set_title('Metallicity')
ax3.grid(True, alpha=0.3)

# Panel 4: Temperature vs Precipitation Regime
ax4 = plt.subplot2grid((3, 3), (1, 2))
for mask, color, label in scatter_masks:
    if np.sum(mask) > 0:
        ax4.scatter(tcool_over_tff[mask], Tvir[mask], 
                   c=color, alpha=0.6, s=50, marker='o', edgecolor='black')

# Add regime boundary lines
ax4.axvline(0.15, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax4.axvline(0.5, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax4.axvline(2.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax4.axvline(10.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)

ax4.set_xscale('log')
ax4.set_yscale('log')
ax4.set_xlim(0.01, 100)
ax4.set_ylim(1e4, 1e7)
ax4.set_xlabel('$t_{cool}/t_{ff}$')
ax4.set_ylabel('T$_{vir}$ (K)')
ax4.set_title('Temperature')
ax4.grid(True, alpha=0.3)

# Panel 5: Depletion Timescale vs Precipitation Regime
ax5 = plt.subplot2grid((3, 3), (2, 0))

print(f'\nDepletion time diagnostics:')
print(f'Total galaxies: {len(tdeplete)}')
print(f'Galaxies with tdeplete > 0: {np.sum(tdeplete > 0)}')
print(f'CGM-regime galaxies with tdeplete > 0: {np.sum((Regime==0) & (tdeplete > 0))}')
print(f'Diluted galaxies: {np.sum(dilute_mask)}')

for mask, color, label in scatter_masks:
    # Only filter out zero/negative values - keep all positive values including large ones
    valid_in_regime = mask & (tdeplete > 0)
    print(f'{label} regime: {np.sum(valid_in_regime)} galaxies in plot')
    if np.sum(valid_in_regime) > 0:
        ax5.scatter(tcool_over_tff[valid_in_regime], tdeplete[valid_in_regime], 
                   c=color, alpha=0.6, s=50, marker='o', edgecolor='black')

ax5.axhline(1000, color='red', linestyle='--', alpha=0.7, label='1 Gyr')
# Add regime boundary lines
ax5.axvline(0.15, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax5.axvline(0.5, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax5.axvline(2.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax5.axvline(10.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)

ax5.set_xscale('log')
ax5.set_yscale('log')
ax5.set_xlim(0.01, 100)
ax5.set_ylim(1, 1e8)  # Extend upper limit to show very long depletion times
ax5.set_xlabel('$t_{cool}/t_{ff}$')
ax5.set_ylabel('CGM Depletion Time (Myr)')
ax5.set_title('Depletion Timescale')
ax5.grid(True, alpha=0.3)

# Panel 6: Precipitation Efficiency
ax6 = plt.subplot2grid((3, 3), (2, 1))
for mask, color, label in scatter_masks:
    if np.sum(mask) > 0:
        ax6.scatter(tcool_over_tff[mask], precipitation_fraction[mask], 
                   c=color, alpha=0.6, s=50, marker='o', edgecolor='black')

# Add regime boundary lines
ax6.axvline(0.15, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax6.axvline(0.5, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax6.axvline(2.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)
ax6.axvline(10.0, color='grey', linestyle='--', alpha=0.7, linewidth=1)

ax6.set_xscale('log')
ax6.set_xlim(0.01, 100)
ax6.set_ylim(0, 1)
ax6.set_xlabel('$t_{cool}/t_{ff}$')
ax6.set_ylabel('Precipitation Fraction')
ax6.set_title('Precipitation Efficiency')
ax6.grid(True, alpha=0.3)

# Panel 7: Use this space for the legend instead of empty placeholder
ax7 = plt.subplot2grid((3, 3), (2, 2))
ax7.axis('off')  # Turn off axis

# Add legend in the third row, third column
legend_handles = [
    plt.Line2D([0], [0], marker='o', color='w', markerfacecolor=colors['ultra_fast'], 
               markersize=10, markeredgecolor='black', label='Ultra-Fast (<0.15)'),
    plt.Line2D([0], [0], marker='o', color='w', markerfacecolor=colors['fast'], 
               markersize=10, markeredgecolor='black', label='Fast (0.15-0.5)'),
    plt.Line2D([0], [0], marker='o', color='w', markerfacecolor=colors['marginal'], 
               markersize=10, markeredgecolor='black', label='Marginal (0.5-2)'),
    plt.Line2D([0], [0], marker='o', color='w', markerfacecolor=colors['weak'], 
               markersize=10, markeredgecolor='black', label='Weak (2-10)'),
    plt.Line2D([0], [0], marker='o', color='w', markerfacecolor=colors['stable'], 
               markersize=10, markeredgecolor='black', label='Stable (>10)')
]

ax7.legend(handles=legend_handles, loc='center', fontsize=11, 
          title='Precipitation Regimes', title_fontsize=12, frameon=True)

plt.tight_layout()
plt.savefig(OutputDir + 'cgm_precipitation_analysis' + OutputFormat, dpi=150, bbox_inches='tight')
# plt.show()

print(f'Plot saved to {OutputDir}cgm_precipitation_analysis{OutputFormat}')