#!/usr/bin/env python

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
from collections import defaultdict
from scipy import stats
from scipy.ndimage import gaussian_filter
from random import sample, seed
import fsps

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5        # h-1 Mpc
VolumeFraction = 1.0  # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 75000       # Number of galaxies to plot in scatter plots
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

if __name__ == '__main__':

    print('Running allresults (local)\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    # Read galaxy properties
    print('Reading galaxy properties from', DirName+FileName)

    CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    MetalsStellarMass = read_hdf(snap_num = Snapshot, param = 'MetalsStellarMass') * 1.0e10 / Hubble_h
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
    BulgeRadius = read_hdf(snap_num = Snapshot, param = 'BulgeScaleRadius')
    MergerBulgeRadius = read_hdf(snap_num = Snapshot, param = 'MergerBulgeRadius')
    InstabilityBulgeRadius = read_hdf(snap_num = Snapshot, param = 'InstabilityBulgeRadius')
    MergerBulgeMass = read_hdf(snap_num = Snapshot, param = 'MergerBulgeMass') * 1.0e10 / Hubble_h
    InstabilityBulgeMass = read_hdf(snap_num = Snapshot, param = 'InstabilityBulgeMass') * 1.0e10 / Hubble_h

    print("Bulge Scale Radius sample:")
    print(BulgeRadius)

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


    w = np.where(StellarMass > 1.0e10)[0]
    print('Number of galaxies read:', len(StellarMass))
    print('Galaxies more massive than 10^10 h-1 Msun:', len(w), '\n')

    Cooling = read_hdf(snap_num = Snapshot, param = 'Cooling')

    Tvir = 35.9 * (Vvir)**2  # in Kelvin
    Tmax = 2.5e5  # K, corresponds to Vvir ~52.7 km/s

    Regime = read_hdf(snap_num = Snapshot, param = 'Regime')

# -------------------------------------------------------

    print('Plotting Bulge Size vs Bulge Mass')

    plt.figure()
    w = np.where((BulgeMass > 0.0) & (BulgeRadius > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_bulge_mass = np.log10(BulgeMass[w])
    log10_bulge_radius = np.log10(BulgeRadius[w] / 0.001)  # Convert to kpc
    bulge_fraction = BulgeMass[w] / StellarMass[w]

    # Color by bulge fraction
    sc = plt.scatter(log10_bulge_mass, log10_bulge_radius, c=bulge_fraction, 
                    cmap='RdYlBu_r', s=5, alpha=0.6, vmin=0, vmax=1)
    plt.colorbar(sc, label=r'$f_{\mathrm{bulge}} = M_{\mathrm{bulge}}/M_{\mathrm{stars}}$')

    # Add the theoretical mass-size relation
    # R_e = 3.5 kpc * (M_bulge / 10^11 Msun)^0.55 (Shen+2003, offset for bulges per Gadotti 2009)
    M_bulge_range = np.logspace(8, 12, 100)
    R_bulge_theory = 3.5 * (M_bulge_range / 1e11)**0.55
    plt.plot(np.log10(M_bulge_range), np.log10(R_bulge_theory), 
            'k--', linewidth=2, label=r'$R_e = 3.5(M/10^{11})^{0.55}$ kpc', zorder=10)

    plt.xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    plt.ylabel(r'$R_{\mathrm{bulge}}\ (\mathrm{kpc})$')
    plt.xlim(8, 12)
    plt.ylim(-0.5, 2.0)
    plt.yticks([0, 1, 2], [r'$10^{0}$', r'$10^{1}$', r'$10^{2}$'])
    plt.legend(loc='upper left', frameon=False)


    outputFile = OutputDir + '26.bulge_size_mass_relation' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Bulge vs Disk Size')

    plt.figure()
    w = np.where((BulgeMass > 0.0) & (BulgeRadius > 0.0) & (DiskRadius > 0.0) & 
                (StellarMass > 1e8))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_disk_radius = np.log10(DiskRadius[w] / 0.001)  # Convert to kpc
    log10_bulge_radius = np.log10(BulgeRadius[w] / 0.001)  # Convert to kpc
    log10_stellar_mass = np.log10(StellarMass[w])

    # Color by total stellar mass
    sc = plt.scatter(log10_disk_radius, log10_bulge_radius, c=log10_stellar_mass,
                    cmap='plasma', s=5, alpha=0.6, vmin=8, vmax=12)
    plt.colorbar(sc, label=r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')

    # Add 1:1 line
    plt.plot([-1, 3], [-1, 3], 'k:', linewidth=1, alpha=0.5, label='1:1')

    # Add typical ratio line (bulge ~ 0.1 * disk)
    disk_range = np.linspace(-1, 3, 100)
    plt.plot(disk_range, disk_range + np.log10(0.1), 'r--', 
            linewidth=2, label=r'$R_{\mathrm{bulge}} = 0.1 R_{\mathrm{disk}}$', alpha=0.7)

    plt.xlabel(r'$R_{\mathrm{disk}}\ (\mathrm{kpc})$')
    plt.ylabel(r'$R_{\mathrm{bulge}}\ (\mathrm{kpc})$')
    plt.xlim(-0.5, 2.5)
    plt.ylim(-1.5, 2.0)
    plt.xticks([0, 1, 2], [r'$10^{0}$', r'$10^{1}$', r'$10^{2}$'])
    plt.yticks([-1, 0, 1, 2], [r'$10^{-1}$', r'$10^{0}$', r'$10^{1}$', r'$10^{2}$'])
    plt.legend(loc='upper left', frameon=False)


    outputFile = OutputDir + '27.bulge_vs_disk_size' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()
    # -------------------------------------------------------

    print('Plotting Mass Components vs Stellar Mass')

    plt.figure()
    w = np.where(StellarMass > 1e9)[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    disk_mass = StellarMass[w] - BulgeMass[w]
    # Ensure positive values for log
    disk_mass[disk_mass <= 0] = 1e-10
    merger_bulge_mass = MergerBulgeMass[w]
    merger_bulge_mass[merger_bulge_mass <= 0] = 1e-10
    instability_bulge_mass = InstabilityBulgeMass[w]
    instability_bulge_mass[instability_bulge_mass <= 0] = 1e-10

    plt.scatter(log10_stellar_mass, np.log10(disk_mass), c='dodgerblue', s=10, alpha=0.8, label='Disk Mass', marker='s', edgecolors='b')
    plt.scatter(log10_stellar_mass, np.log10(merger_bulge_mass), c='r', s=10, alpha=0.6, label='Merger Bulge Mass', marker='s', edgecolors='firebrick')
    plt.scatter(log10_stellar_mass, np.log10(instability_bulge_mass), c='greenyellow', s=10, alpha=0.15, label='Instability Bulge Mass', marker='s')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{component}}\ (M_{\odot})$')
    plt.xlim(9, 12)
    plt.ylim(7, 12)
    plt.legend(loc='upper left', frameon=False)


    outputFile = OutputDir + '28.mass_components_vs_stellar_mass' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Mass Ratios vs Stellar Mass')

    plt.figure()
    w = np.where(StellarMass > 1e9)[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    disk_ratio = (StellarMass[w] - BulgeMass[w]) / StellarMass[w]
    merger_ratio = MergerBulgeMass[w] / StellarMass[w]
    instability_ratio = InstabilityBulgeMass[w] / StellarMass[w]

    plt.scatter(log10_stellar_mass, disk_ratio, c='dodgerblue', s=10, alpha=0.8, label='Disk Fraction', marker='s', edgecolors='b')
    plt.scatter(log10_stellar_mass, merger_ratio, c='r', s=10, alpha=0.6, label='Merger Bulge Fraction', marker='s', edgecolors='firebrick')
    plt.scatter(log10_stellar_mass, instability_ratio, c='greenyellow', s=10, alpha=0.15, label='Instability Bulge Fraction', marker='s')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'Mass Fraction')
    plt.xlim(9, 12)
    plt.ylim(0, 1.05)
    # plt.legend(loc='center left', frameon=False)


    outputFile = OutputDir + '29.mass_ratios_vs_stellar_mass' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Mass Components vs Stellar Mass (Simplified)')

    plt.figure()
    w = np.where(StellarMass > 1e9)[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    disk_mass = StellarMass[w] - BulgeMass[w]
    # Ensure positive values for log
    disk_mass[disk_mass <= 0] = 1e-10
    bulge_mass = BulgeMass[w]
    bulge_mass[bulge_mass <= 0] = 1e-10

    plt.scatter(log10_stellar_mass, np.log10(disk_mass), c='dodgerblue', s=10, alpha=0.8, label='Disk Mass', marker='s', edgecolors='b')
    plt.scatter(log10_stellar_mass, np.log10(bulge_mass), c='r', s=10, alpha=0.6, label='Bulge Mass', marker='s', edgecolors='firebrick')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{component}}\ (M_{\odot})$')
    plt.xlim(9, 12)
    plt.ylim(7, 12)
    plt.legend(loc='upper left', frameon=False)


    outputFile = OutputDir + '28b.mass_components_vs_stellar_mass_simple' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Mass Ratios vs Stellar Mass (Simplified)')

    plt.figure()
    w = np.where(StellarMass > 1e9)[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    disk_ratio = (StellarMass[w] - BulgeMass[w]) / StellarMass[w]
    bulge_ratio = BulgeMass[w] / StellarMass[w]

    plt.scatter(log10_stellar_mass, disk_ratio, c='dodgerblue', s=10, alpha=0.8, label='Disk Fraction', marker='s', edgecolors='b')
    plt.scatter(log10_stellar_mass, bulge_ratio, c='r', s=10, alpha=0.6, label='Bulge Fraction', marker='s', edgecolors='firebrick')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'Mass Fraction')
    plt.xlim(9, 12)
    plt.ylim(0, 1.05)
    plt.legend(loc='upper right', frameon=False)


    outputFile = OutputDir + '29b.mass_ratios_vs_stellar_mass_simple' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Disk Scale Radius vs Disk Mass')

    plt.figure()
    w = np.where((StellarMass > 1e9) & (DiskRadius > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    disk_mass = StellarMass[w] - BulgeMass[w]
    w_pos = np.where(disk_mass > 0)[0]
    disk_mass = disk_mass[w_pos]
    disk_radius = DiskRadius[w][w_pos]

    log10_disk_mass = np.log10(disk_mass)
    disk_diameter_kpc = disk_radius / Hubble_h / 0.001  # Convert to kpc diameter
    log10_disk_diameter = np.log10(disk_diameter_kpc)

    # Create hexbin plot
    hb = plt.hexbin(log10_disk_mass, log10_disk_diameter, 
                    gridsize=100, 
                    cmap='Blues_r', 
                    mincnt=1,
                    edgecolors='face',
                    linewidths=0.2)
    plt.colorbar(hb, label='Number of Galaxies')

    plt.xlabel(r'$\log_{10} M_{\mathrm{disk}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$') 
    plt.xlim(9.0, 12.0)
    plt.ylim(-2, 2.0)  # 10^0 to 10^2 kpc
    plt.yticks([-2, -1, 0, 1, 2], [r'$10^{-2}$', r'$10^{-1}$', r'$10^{0}$', r'$10^{1}$', r'$10^{2}$'])

    outputFile = OutputDir + '30.disk_mass_vs_disk_radius' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Bulge Radius vs Bulge Mass')

    plt.figure()
    w = np.where((StellarMass > 1e9) & (BulgeMass > 0.0) & (BulgeRadius > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    bulge_mass = BulgeMass[w]
    bulge_radius = BulgeRadius[w]

    log10_bulge_mass = np.log10(bulge_mass)
    bulge_radius_kpc = bulge_radius / Hubble_h / 0.001  # Convert to kpc
    log10_bulge_radius = np.log10(bulge_radius_kpc)

    # Create hexbin plot
    hb = plt.hexbin(log10_bulge_mass, log10_bulge_radius, 
                    gridsize=100, 
                    cmap='Blues_r', 
                    mincnt=1,
                    edgecolors='face',
                    linewidths=0.2)
    plt.colorbar(hb, label='Number of Galaxies')

    plt.xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} R_{\mathrm{bulge}}\ (\mathrm{kpc})$') 
    plt.xlim(8.0, 12.0)
    plt.ylim(-2, 2.0)  # 10^-2 to 10^2 kpc
    plt.yticks([-2, -1, 0, 1, 2], [r'$10^{-2}$', r'$10^{-1}$', r'$10^{0}$', r'$10^{1}$', r'$10^{2}$'])

    outputFile = OutputDir + '30b.bulge_mass_vs_bulge_radius' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Combined Disk and Bulge Mass-Radius Relations')

    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Prepare disk data
    w_disk = np.where((StellarMass > 1e9) & (DiskRadius > 0.0))[0]
    if(len(w_disk) > dilute): w_disk = sample(list(w_disk), dilute)
    
    disk_mass = StellarMass[w_disk] - BulgeMass[w_disk]
    w_pos = np.where(disk_mass > 0)[0]
    disk_mass = disk_mass[w_pos]
    disk_radius = DiskRadius[w_disk][w_pos]
    
    log10_disk_mass = np.log10(disk_mass)
    disk_radius_kpc = disk_radius / Hubble_h / 0.001  # Convert to kpc
    log10_disk_radius = np.log10(disk_radius_kpc)
    
    # Prepare bulge data
    w_bulge = np.where((BulgeMass > 0.0) & (BulgeRadius > 0.0))[0]
    if(len(w_bulge) > dilute): w_bulge = sample(list(w_bulge), dilute)
    
    bulge_mass = BulgeMass[w_bulge]
    bulge_radius = BulgeRadius[w_bulge]
    
    log10_bulge_mass = np.log10(bulge_mass)
    bulge_radius_kpc = bulge_radius / Hubble_h / 0.001  # Convert to kpc
    log10_bulge_radius = np.log10(bulge_radius_kpc)
    
    # Plot disks first (background)
    hb_disk = ax.hexbin(log10_disk_mass, log10_disk_radius, 
                        gridsize=100, 
                        cmap='Blues_r', 
                        mincnt=1,
                        edgecolors='none',
                        label='Disks')
    
    # Plot bulges on top
    hb_bulge = ax.hexbin(log10_bulge_mass, log10_bulge_radius, 
                         gridsize=100, 
                         cmap='Reds_r', 
                         mincnt=1,
                         edgecolors='none',
                         label='Bulges')
    
    # Add colorbars
    # cbar_disk = plt.colorbar(hb_disk, ax=ax, pad=0.12, label='Disk Count')
    # cbar_bulge = plt.colorbar(hb_bulge, ax=ax, label='Bulge Count')
    
    # ax.set_xlabel(r'$\log_{10} M_{\mathrm{component}}\ (M_{\odot})$')
    # ax.set_ylabel(r'$\log_{10} R_{\mathrm{component}}\ (\mathrm{kpc})$')
    ax.set_xlim(9.0, 12.0)
    ax.set_ylim(-2, 2.0)
    ax.set_yticks([-2, -1, 0, 1, 2])
    ax.set_yticklabels([r'$10^{-2}$', r'$10^{-1}$', r'$10^{0}$', r'$10^{1}$', r'$10^{2}$'])
    
    # Add legend
    from matplotlib.patches import Patch
    legend_elements = [Patch(facecolor='blue', alpha=0.6, label='Disks'),
                       Patch(facecolor='red', alpha=0.6, label='Bulges')]
    ax.legend(handles=legend_elements, loc='upper left', frameon=True)
    
    outputFile = OutputDir + '30c.combined_disk_bulge_mass_radius' + OutputFormat
    plt.savefig(outputFile, bbox_inches='tight')
    print('Saved file to', outputFile, '\\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Cold gas vs Disk Diameter')

    plt.figure()
    w = np.where((StellarMass > 1e9) & (DiskRadius > 0.0) & (ColdGas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    cold_gas_mass = ColdGas[w] - BulgeMass[w]
    disk_radius = DiskRadius[w]

    log10_cold_gas = np.log10(cold_gas_mass)
    disk_diameter_kpc = 2 * disk_radius / Hubble_h / 0.001  # Convert to kpc diameter
    log10_disk_diameter = np.log10(disk_diameter_kpc)

    # Create hexbin plot
    hb = plt.hexbin(log10_disk_diameter, log10_cold_gas, 
                    gridsize=100, 
                    cmap='Blues_r', 
                    mincnt=1,
                    edgecolors='face',
                    linewidths=0.2)
    plt.colorbar(hb, label='Number of Galaxies')

    plt.xlabel(r'$\log_{10} D_{\mathrm{disk}}\ (\mathrm{kpc})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{cold\ gas}}\ (M_{\odot})$')
    plt.xlim(0, 2.0)  # 10^0 to 10^2 kpc
    plt.ylim(8.0, 11.5)

    outputFile = OutputDir + '31.cold_gas_vs_disk_diameter' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Half-Mass Radius vs Stellar Mass (Sc-type disk-dominated galaxies)')

    plt.figure()
    
    # Calculate disk fraction
    disk_mass = StellarMass - BulgeMass
    disk_fraction = disk_mass / StellarMass
    
    # Filter for Sc-type galaxies: disk >= 80% of stellar mass
    w = np.where((StellarMass > 1e9) & (BulgeRadius > 0.0) & (DiskRadius > 0.0) & (disk_fraction >= 0.8))[0]

    # Calculate half-mass radius as mass-weighted average
    disk_mass = StellarMass[w] - BulgeMass[w]
    bulge_mass = BulgeMass[w]
    total_mass = StellarMass[w]
    
    # Mass-weighted radius
#     half_mass_radius = (disk_mass * DiskRadius[w] + bulge_mass * BulgeRadius[w]) / total_mass
    half_mass_radius = 1.68 * DiskRadius[w]
    
    # Filter positive values
    w_pos = np.where(half_mass_radius > 0)[0]
    half_mass_radius = half_mass_radius[w_pos]
    stellar_mass = StellarMass[w][w_pos]

    log10_stellar_mass = np.log10(stellar_mass)
    log10_half_mass_radius = np.log10(half_mass_radius / 0.001)  # Convert to kpc

    # Create hexbin plot
    hb = plt.hexbin(log10_stellar_mass, log10_half_mass_radius, 
                    gridsize=100, 
                    cmap='Blues_r', 
                    mincnt=1,
                    edgecolors='face',
                    linewidths=0.2)
    plt.colorbar(hb, label='Number of Galaxies')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$R_{1/2}\ (\mathrm{kpc})$')
    plt.xlim(9.5, 11.5)
    plt.ylim(-1, 1)  # log10 space: -1 = 0.1 kpc, 0 = 1 kpc, 1 = 10 kpc
    
    # Set y-tick labels to show exponential notation
    plt.yticks([-1, 0, 1], [r'$10^{-1}$', r'$10^{0}$', r'$10^{1}$'])

    outputFile = OutputDir + '32.half_mass_radius_vs_stellar_mass' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Bulge Formation Timescales (Figure 10)')

    # Read TimeOfLastMajorMerger if not loaded
    try:
        TimeOfLastMajorMerger
    except NameError:
        TimeOfLastMajorMerger = read_hdf(snap_num = Snapshot, param = 'TimeOfLastMajorMerger')

    # Filter for ALL central galaxies at z=0
    w_central = np.where(Type == 0)[0]
    print(f"Total central galaxies: {len(w_central)}")

    # Calculate age of universe at z=0
    age_universe = 13.8  # Gyr

    # Get time info for all centrals
    time_last_major_merger_all = TimeOfLastMajorMerger[w_central]
    time_since_major_merger_all = age_universe - time_last_major_merger_all
    never_merged_all = time_last_major_merger_all < 0.1
    time_since_major_merger_all[never_merged_all] = age_universe

    # Strategy: Oversample galaxies that had major mergers
    # Take ALL galaxies that had major mergers, plus random sample of never-merged
    w_merged = w_central[~never_merged_all]
    w_never_merged = w_central[never_merged_all]

    print(f"Galaxies that had major mergers: {len(w_merged)}")
    print(f"Galaxies that never merged: {len(w_never_merged)}")

    # Create subsample: all merged + sample of never-merged
    n_sample_never = min(10000, len(w_never_merged))  # Take up to 10k never-merged
    if len(w_never_merged) > n_sample_never:
        np.random.seed(42)
        w_never_merged_sample = np.random.choice(w_never_merged, size=n_sample_never, replace=False)
    else:
        w_never_merged_sample = w_never_merged

    # Combine
    w_sample = np.concatenate([w_merged, w_never_merged_sample])
    print(f"Total subsample: {len(w_sample)} (oversampled merged galaxies)")

    # Get properties
    stellar_mass = StellarMass[w_sample]
    bulge_mass = BulgeMass[w_sample]
    disk_mass = stellar_mass - bulge_mass
    merger_bulge_mass = MergerBulgeMass[w_sample]
    instability_bulge_mass = InstabilityBulgeMass[w_sample]
    merger_bulge_radius = MergerBulgeRadius[w_sample]
    instability_bulge_radius = InstabilityBulgeRadius[w_sample]
    disk_radius = DiskRadius[w_sample]
    time_last_major_merger = TimeOfLastMajorMerger[w_sample]

    # Calculate time since last major merger
    time_since_major_merger = age_universe - time_last_major_merger
    never_merged = time_last_major_merger < 0.1
    time_since_major_merger[never_merged] = age_universe

    print(f"In subsample - never merged: {np.sum(never_merged)} ({100*np.sum(never_merged)/len(w_sample):.1f}%)")

    # Filter for galaxies WITH bulges
    has_bulge = bulge_mass > 0
    print(f"Galaxies with bulges in subsample: {np.sum(has_bulge)} ({100*np.sum(has_bulge)/len(w_sample):.1f}%)")

    # Calculate disk fraction
    disk_fraction = np.zeros_like(stellar_mass)
    mask_stellar = stellar_mass > 0
    disk_fraction[mask_stellar] = disk_mass[mask_stellar] / stellar_mass[mask_stellar]

    # Calculate bulge half-mass radius
    bulge_half_mass_radius = np.zeros(len(w_sample))
    bulge_half_mass_radius[has_bulge] = (
        (merger_bulge_mass[has_bulge] * merger_bulge_radius[has_bulge] + 
        instability_bulge_mass[has_bulge] * instability_bulge_radius[has_bulge]) /
        bulge_mass[has_bulge]
    )

    # Calculate galaxy half-mass radius
    galaxy_half_mass_radius = np.zeros(len(w_sample))
    galaxy_half_mass_radius[mask_stellar] = (
        disk_mass[mask_stellar] * disk_radius[mask_stellar] +
        merger_bulge_mass[mask_stellar] * merger_bulge_radius[mask_stellar] +
        instability_bulge_mass[mask_stellar] * instability_bulge_radius[mask_stellar]
    ) / stellar_mass[mask_stellar]

    # Convert radii to kpc
    bulge_half_mass_radius_kpc = bulge_half_mass_radius / Hubble_h / 0.001
    galaxy_half_mass_radius_kpc = galaxy_half_mass_radius / Hubble_h / 0.001

    # Classify bulge types
    bulge_ratio = np.zeros(len(w_sample))
    bulge_ratio[has_bulge] = instability_bulge_mass[has_bulge] / bulge_mass[has_bulge]

    merger_driven = has_bulge & (bulge_ratio < 0.1)
    instability_driven = has_bulge & (bulge_ratio > 0.9)
    mixed_bulge = has_bulge & (bulge_ratio >= 0.1) & (bulge_ratio <= 0.9)

    print(f"Merger-driven: {np.sum(merger_driven)}, Instability-driven: {np.sum(instability_driven)}, Mixed: {np.sum(mixed_bulge)}")

    # Create figure
    fig = plt.figure(figsize=(14, 12))

    # ===== TOP LEFT: Bulge mass vs time since major merger =====
    ax1 = plt.subplot(2, 2, 1)

    # Separate into merged and never-merged
    mask_inst_merged = instability_driven & ~never_merged & (instability_bulge_mass > 0)
    mask_inst_never = instability_driven & never_merged & (instability_bulge_mass > 0)
    mask_merger_merged = merger_driven & ~never_merged & (merger_bulge_mass > 0)
    mask_mixed_merged = mixed_bulge & ~never_merged & (bulge_mass > 0)

    # Plot never-merged as thin vertical strip at t=13.8
    if np.sum(mask_inst_never) > 0:
        # Add small random jitter to x to spread out points
        x_jitter = time_since_major_merger[mask_inst_never] + np.random.normal(0, 0.05, np.sum(mask_inst_never))
        ax1.scatter(x_jitter, np.log10(instability_bulge_mass[mask_inst_never]),
                c='lightblue', s=0.5, alpha=0.2, rasterized=True, zorder=1)

    # Plot merged galaxies
    if np.sum(mask_merger_merged) > 0:
        ax1.scatter(time_since_major_merger[mask_merger_merged],
                np.log10(merger_bulge_mass[mask_merger_merged]),
                c='orange', s=15, alpha=0.7, label='Merger-driven',
                rasterized=True, zorder=3, edgecolors='darkorange', linewidths=0.3)

    if np.sum(mask_inst_merged) > 0:
        ax1.scatter(time_since_major_merger[mask_inst_merged],
                np.log10(instability_bulge_mass[mask_inst_merged]),
                c='blue', s=15, alpha=0.7, label='Instability-driven',
                rasterized=True, zorder=2, edgecolors='darkblue', linewidths=0.3)

    if np.sum(mask_mixed_merged) > 0:
        ax1.scatter(time_since_major_merger[mask_mixed_merged],
                np.log10(bulge_mass[mask_mixed_merged]),
                c='magenta', s=10, alpha=0.5, label='Mixed',
                rasterized=True, zorder=2, edgecolors='none')

    ax1.set_xlabel('Time since last major merger [Gyr]', fontsize=13)
    ax1.set_ylabel(r'$\log_{10}(M_{\rm bulge}/M_\odot)$', fontsize=13)
    ax1.set_xlim(0, 14)
    ax1.set_ylim(8, 12.5)
    ax1.grid(True, alpha=0.3)
    ax1.legend(loc='upper left', fontsize=10, frameon=True, markerscale=1.5)

    # ===== TOP RIGHT: Disk fraction vs stellar mass =====
    ax2 = plt.subplot(2, 2, 2)

    # Only plot galaxies with bulges
    if np.sum(merger_driven) > 0:
        ax2.scatter(np.log10(stellar_mass[merger_driven]), disk_fraction[merger_driven],
                c='orange', s=10, alpha=0.6, label='Merger-driven',
                rasterized=True, edgecolors='none')
    if np.sum(instability_driven) > 0:
        ax2.scatter(np.log10(stellar_mass[instability_driven]), disk_fraction[instability_driven],
                c='blue', s=10, alpha=0.6, label='Instability-driven',
                rasterized=True, edgecolors='none')
    if np.sum(mixed_bulge) > 0:
        ax2.scatter(np.log10(stellar_mass[mixed_bulge]), disk_fraction[mixed_bulge],
                c='magenta', s=8, alpha=0.5, label='Mixed',
                rasterized=True, edgecolors='none')

    ax2.set_xlabel(r'$\log_{10}(M_{\rm stars}/M_\odot)$', fontsize=13)
    ax2.set_ylabel('Disk fraction (mass)', fontsize=13)
    ax2.set_xlim(9, 12.5)
    ax2.set_ylim(0, 1.05)
    ax2.grid(True, alpha=0.3)
    ax2.legend(loc='center left', fontsize=10, frameon=True, markerscale=2)

    # ===== BOTTOM LEFT: Bulge radius vs time since major merger =====
    ax3 = plt.subplot(2, 2, 3)

    mask_inst_rad_merged = instability_driven & (bulge_half_mass_radius_kpc > 0.01) & ~never_merged
    mask_inst_rad_never = instability_driven & (bulge_half_mass_radius_kpc > 0.01) & never_merged
    mask_merger_rad = merger_driven & (bulge_half_mass_radius_kpc > 0.01) & ~never_merged
    mask_mixed_rad = mixed_bulge & (bulge_half_mass_radius_kpc > 0.01) & ~never_merged

    # Never-merged strip
    if np.sum(mask_inst_rad_never) > 0:
        x_jitter = time_since_major_merger[mask_inst_rad_never] + np.random.normal(0, 0.05, np.sum(mask_inst_rad_never))
        ax3.scatter(x_jitter, bulge_half_mass_radius_kpc[mask_inst_rad_never],
                c='lightblue', s=0.5, alpha=0.2, rasterized=True, zorder=1)

    if np.sum(mask_merger_rad) > 0:
        ax3.scatter(time_since_major_merger[mask_merger_rad],
                bulge_half_mass_radius_kpc[mask_merger_rad],
                c='orange', s=15, alpha=0.7, label='Merger-driven',
                rasterized=True, zorder=3, edgecolors='darkorange', linewidths=0.3)

    if np.sum(mask_inst_rad_merged) > 0:
        ax3.scatter(time_since_major_merger[mask_inst_rad_merged],
                bulge_half_mass_radius_kpc[mask_inst_rad_merged],
                c='blue', s=15, alpha=0.7, label='Instability-driven',
                rasterized=True, zorder=2, edgecolors='darkblue', linewidths=0.3)

    if np.sum(mask_mixed_rad) > 0:
        ax3.scatter(time_since_major_merger[mask_mixed_rad],
                bulge_half_mass_radius_kpc[mask_mixed_rad],
                c='magenta', s=10, alpha=0.5, label='Mixed',
                rasterized=True, zorder=2, edgecolors='none')

    ax3.set_xlabel('Time since last major merger [Gyr]', fontsize=13)
    ax3.set_ylabel(r'Bulge half-mass radius [kpc]', fontsize=13)
    ax3.set_xlim(0, 14)
    ax3.set_ylim(0, 20)
    ax3.grid(True, alpha=0.3)
    ax3.legend(loc='upper right', fontsize=10, frameon=True, markerscale=1.5)

    # ===== BOTTOM RIGHT: Galaxy radius vs time since major merger =====
    ax4 = plt.subplot(2, 2, 4)

    mask_inst_gal_merged = instability_driven & (galaxy_half_mass_radius_kpc > 0.01) & ~never_merged
    mask_inst_gal_never = instability_driven & (galaxy_half_mass_radius_kpc > 0.01) & never_merged
    mask_merger_gal = merger_driven & (galaxy_half_mass_radius_kpc > 0.01) & ~never_merged
    mask_mixed_gal = mixed_bulge & (galaxy_half_mass_radius_kpc > 0.01) & ~never_merged

    # Never-merged strip
    if np.sum(mask_inst_gal_never) > 0:
        x_jitter = time_since_major_merger[mask_inst_gal_never] + np.random.normal(0, 0.05, np.sum(mask_inst_gal_never))
        ax4.scatter(x_jitter, galaxy_half_mass_radius_kpc[mask_inst_gal_never],
                c='lightblue', s=0.5, alpha=0.2, rasterized=True, zorder=1)

    if np.sum(mask_merger_gal) > 0:
        ax4.scatter(time_since_major_merger[mask_merger_gal],
                galaxy_half_mass_radius_kpc[mask_merger_gal],
                c='orange', s=15, alpha=0.7, label='Merger-driven',
                rasterized=True, zorder=3, edgecolors='darkorange', linewidths=0.3)

    if np.sum(mask_inst_gal_merged) > 0:
        ax4.scatter(time_since_major_merger[mask_inst_gal_merged],
                galaxy_half_mass_radius_kpc[mask_inst_gal_merged],
                c='blue', s=15, alpha=0.7, label='Instability-driven',
                rasterized=True, zorder=2, edgecolors='darkblue', linewidths=0.3)

    if np.sum(mask_mixed_gal) > 0:
        ax4.scatter(time_since_major_merger[mask_mixed_gal],
                galaxy_half_mass_radius_kpc[mask_mixed_gal],
                c='magenta', s=10, alpha=0.5, label='Mixed',
                rasterized=True, zorder=2, edgecolors='none')

    ax4.set_xlabel('Time since last major merger [Gyr]', fontsize=13)
    ax4.set_ylabel(r'Total galaxy half-mass radius [kpc]', fontsize=13)
    ax4.set_xlim(0, 14)
    ax4.set_ylim(0, 20)
    ax4.grid(True, alpha=0.3)

    plt.tight_layout()

    outputFile = OutputDir + '34.bulge_formation_timescales' + OutputFormat
    plt.savefig(outputFile, dpi=150)
    print(f'Saved file to {outputFile}\n')
    plt.close()

    # =======================================================

    print('Plotting Color-Magnitude Diagram by Bulge Type (Figure 11)')

    # Filter for central galaxies with bulges and reasonable stellar mass
    w_central_bulge = np.where((Type == 0) & (BulgeMass > 0) & (StellarMass > 1e9))[0]

    print(f"Central galaxies with bulges and M* > 10^9: {len(w_central_bulge)}")

    # Subsample to manageable size
    n_sample = 50000
    if len(w_central_bulge) > n_sample:
        np.random.seed(42)
        w_sample = np.random.choice(w_central_bulge, size=n_sample, replace=False)
    else:
        w_sample = w_central_bulge

    print(f"Subsample size: {len(w_sample)}")

    # Get galaxy properties
    stellar_mass = StellarMass[w_sample]
    bulge_mass = BulgeMass[w_sample]
    disk_mass = stellar_mass - bulge_mass
    merger_bulge_mass = MergerBulgeMass[w_sample]
    instability_bulge_mass = InstabilityBulgeMass[w_sample]
    sfr_disk = SfrDisk[w_sample]
    sfr_bulge = SfrBulge[w_sample]
    metals_stellar = MetalsStellarMass[w_sample]

    # Calculate total SFR
    sfr_total = sfr_disk + sfr_bulge

    # Calculate specific SFR
    ssfr = np.zeros_like(sfr_total)
    mask_stellar = stellar_mass > 0
    ssfr[mask_stellar] = sfr_total[mask_stellar] / stellar_mass[mask_stellar]

    # Calculate disk fraction
    disk_fraction = np.zeros_like(stellar_mass)
    disk_fraction[mask_stellar] = disk_mass[mask_stellar] / stellar_mass[mask_stellar]

    # Calculate metallicity
    metallicity = metals_stellar / (stellar_mass + 1e-10)
    metallicity = np.clip(metallicity, 0.001, 0.05)

    # Classify bulge types
    bulge_ratio = np.zeros(len(w_sample))
    has_bulge = bulge_mass > 0
    bulge_ratio[has_bulge] = instability_bulge_mass[has_bulge] / bulge_mass[has_bulge]

    merger_driven = bulge_ratio < 0.1
    instability_driven = bulge_ratio > 0.9

    print(f"Merger-driven: {np.sum(merger_driven)} ({100*np.sum(merger_driven)/len(w_sample):.1f}%)")
    print(f"Instability-driven: {np.sum(instability_driven)} ({100*np.sum(instability_driven)/len(w_sample):.1f}%)")

    # ===== COMPUTE PHOTOMETRY =====
    USE_FSPS = False  # Set to True to use FSPS (slow but accurate), False for fast empirical
    
    if USE_FSPS:
        print("\nInitializing FSPS for photometry calculations...")
        import fsps
        
        # Initialize FSPS stellar population
        sp = fsps.StellarPopulation(compute_vega_mags=False, zcontinuous=1, 
                                     sfh=1, imf_type=1)  # Chabrier IMF, tau model
        
        def fsps_photometry(stellar_mass, sfr, metallicity):
            """Compute SDSS u and r band photometry using FSPS"""
            n_gal = len(stellar_mass)
            u_minus_r = np.zeros(n_gal)
            M_r = np.zeros(n_gal)
            age_universe = 13.8  # Gyr at z=0
            
            for i in range(n_gal):
                if i % 500 == 0:
                    print(f"  Computing photometry for galaxy {i}/{n_gal}")
                
                ssfr = sfr[i] / (stellar_mass[i] + 1e-10)
                log_ssfr = np.log10(ssfr + 1e-13)
                
                # Estimate tau from sSFR
                if log_ssfr > -10.5:
                    tau = np.clip(1.0 / (10**log_ssfr), 0.1, 100)
                else:
                    tau = 0.1
                
                # Set metallicity
                Z_solar = 0.02
                Z = np.clip(metallicity[i], 0.0001, 0.05)
                logzsol = np.log10(Z / Z_solar)
                
                try:
                    sp.params['logzsol'] = logzsol
                    sp.params['tau'] = tau
                    sp.params['tage'] = age_universe
                    
                    mags = sp.get_mags(tage=age_universe, bands=['sdss_u', 'sdss_r'])
                    current_mass = sp.stellar_mass
                    mass_scale = stellar_mass[i] / current_mass
                    
                    M_u = mags[0] - 2.5 * np.log10(mass_scale)
                    M_r[i] = mags[1] - 2.5 * np.log10(mass_scale)
                    u_minus_r[i] = M_u - M_r[i]
                except:
                    # Fallback to empirical
                    log_stellar_mass = np.log10(stellar_mass[i] + 1e-5)
                    u_minus_r[i] = np.clip(2.3 - 0.85 * (log_ssfr + 10.5), 0.0, 3.5)
                    M_r[i] = -20.44 - 2.5 * (log_stellar_mass - 10.0)
            
            return u_minus_r, M_r
        
        u_minus_r, M_r = fsps_photometry(stellar_mass, sfr_total, metallicity)
    
    else:
        print("\nUsing fast empirical photometry...")
        
        def empirical_photometry(stellar_mass, sfr, metallicity):
            """Fast vectorized empirical photometry"""
            # Calculate specific SFR
            ssfr = sfr / (stellar_mass + 1e-10)
            log_ssfr = np.log10(ssfr + 1e-13)
            log_stellar_mass = np.log10(stellar_mass + 1e-5)
            
            # u-r color from sSFR (Salim et al. 2007)
            u_minus_r = 2.3 - 0.85 * (log_ssfr + 10.5)
            
            # Metallicity dependence
            Z_solar = 0.02
            metallicity_safe = np.clip(metallicity, 0.001, 0.05)
            u_minus_r += 0.2 * np.log10(metallicity_safe / Z_solar)
            u_minus_r = np.clip(u_minus_r, 0.0, 3.5)
            
            # r-band magnitude from stellar mass
            M_r = -20.44 - 2.5 * (log_stellar_mass - 10.0)
            
            # sSFR correction (younger = brighter)
            ssfr_correction = 0.4 * np.tanh(log_ssfr + 10.5)
            M_r += ssfr_correction
            
            # Metallicity dependence
            M_r -= 0.1 * np.log10(metallicity_safe / Z_solar)
            
            return u_minus_r, M_r
        
        u_minus_r, M_r = empirical_photometry(stellar_mass, sfr_total, metallicity)


    print(f"\nu-r range: {u_minus_r.min():.2f} to {u_minus_r.max():.2f}")
    print(f"M_r range: {M_r.min():.2f} to {M_r.max():.2f}")

    # Calculate log values for SFR and sSFR
    log_sfr = np.log10(sfr_total + 1e-4)
    log_ssfr = np.log10(ssfr + 1e-13)

    print(f"log(SFR) range: {log_sfr.min():.2f} to {log_sfr.max():.2f}")
    print(f"log(sSFR) range: {log_ssfr.min():.2f} to {log_ssfr.max():.2f}")

    # ===== CREATE FIGURE =====
    fig, axes = plt.subplots(3, 2, figsize=(14, 16))

    # Classify galaxy types based on disk fraction
    # Early types and S0: disk_fraction < 0.4 (bulge-dominated)
    # Spirals Sa to Sc: 0.4 <= disk_fraction < 0.8 (intermediate)
    # Sc to irregulars: disk_fraction >= 0.8 (disk-dominated)
    galaxy_type_early = disk_fraction < 0.4  # Squares
    galaxy_type_spiral = (disk_fraction >= 0.4) & (disk_fraction < 0.8)  # Triangles
    galaxy_type_late = disk_fraction >= 0.8  # Hexagons
    
    # Properties to plot with CORRECT colormaps and CORRECT thresholds
    properties = [
        (disk_fraction, 'Disk fraction', 0, 1, None, ['OrRd_r', 'Blues']),
        (log_sfr, r'$\log_{10}$(SFR [M$_\odot$ yr$^{-1}$])', -2, 1.5, -2, ['coolwarm_r', 'coolwarm_r']),
        (log_ssfr, r'$\log_{10}$(sSFR [yr$^{-1}$])', -12, -8.5, -11, ['seismic_r', 'seismic_r'])
    ]

    # Bulge types (columns)
    bulge_types = [
        (merger_driven, 'Merger-driven bulges'),
        (instability_driven, 'Instability-driven bulges')
    ]

    # Observational data from Drory & Fisher 2007
    classical_data = {
        'ur': np.array([2.2, 2.3, 2.4, 2.5, 2.6, 2.3, 2.4, 2.5, 2.6, 2.7, 2.1, 2.3]),
        'Mr': np.array([-19.5, -20.0, -20.5, -21.0, -21.5, -19.8, -20.3, -20.8, -21.3, -21.8, -19.2, -19.7]),
        'types': ['square', 'square', 'square', 'square', 'square', 
                'triangle', 'triangle', 'triangle', 'triangle', 'triangle',
                'square', 'square']
    }

    pseudo_data = {
        'ur': np.array([1.3, 1.5, 1.7, 1.9, 2.1, 1.4, 1.6, 1.8, 2.0, 2.2, 1.2, 1.4, 1.6, 1.8]),
        'Mr': np.array([-18.5, -19.0, -19.5, -20.0, -20.5, -18.8, -19.3, -19.8, -20.3, -20.8,
                        -18.2, -18.7, -19.2, -19.7]),
        'types': ['triangle', 'triangle', 'triangle', 'triangle', 'triangle',
                'hexagon', 'hexagon', 'hexagon', 'hexagon', 'hexagon',
                'triangle', 'triangle', 'hexagon', 'hexagon']
    }

    marker_map = {'square': 's', 'triangle': '^', 'hexagon': 'h'}

    for row, (prop_data, prop_label, vmin, vmax, threshold, cmaps) in enumerate(properties):
        for col, (bulge_mask, bulge_label) in enumerate(bulge_types):
            ax = axes[row, col]
            
            # Select colormap for this panel
            cmap = cmaps[col]
            
            # Filter for this bulge type
            mask = bulge_mask
            n_in_cat = np.sum(mask)
            
            if n_in_cat == 0:
                ax.text(0.5, 0.5, 'No galaxies in this category',
                    transform=ax.transAxes, ha='center', va='center')
                continue
            
            print(f"\nRow {row}, Col {col} ({bulge_label}): {n_in_cat} galaxies")
            
            # Get data for this subset - SWAPPED AXES!
            x = M_r[mask]  # M_r on x-axis
            y = u_minus_r[mask]  # u-r on y-axis
            c = prop_data[mask]
            
            # Separate active and quiescent for SFR plots
            if threshold is not None:
                active_mask = c > threshold
                quiescent_mask = ~active_mask
                
                print(f"  Active: {np.sum(active_mask)}, Quiescent: {np.sum(quiescent_mask)}")
                print(f"  Threshold: {threshold}")
                
                # Plot quiescent as uniform black crosses
                if np.sum(quiescent_mask) > 0:
                    ax.scatter(x[quiescent_mask], y[quiescent_mask],
                            c='black', s=35, alpha=0.9, marker='x',
                            rasterized=True, linewidths=0.5, zorder=1)
                
                # Plot active galaxies with different symbols based on galaxy type
                if np.sum(active_mask) > 0:
                    from matplotlib import cm
                    import matplotlib.colors as mcolors
                    
                    # Get galaxy type masks for active galaxies
                    active_indices = np.where(mask)[0][active_mask]
                    
                    # Early types and S0 (bulge-dominated): squares
                    early_active = galaxy_type_early[active_indices]
                    if np.sum(early_active) > 0:
                        norm = mcolors.Normalize(vmin=vmin, vmax=vmax)
                        cmap_obj = cm.get_cmap(cmap)
                        edge_colors = cmap_obj(norm(c[active_mask][early_active]))
                        
                        ax.scatter(x[active_mask][early_active], 
                                 y[active_mask][early_active],
                                 c=c[active_mask][early_active], s=35, alpha=0.9, marker='s',
                                 vmin=vmin, vmax=vmax, cmap=cmap,
                                 rasterized=True, edgecolors=edge_colors, linewidths=0.5, zorder=2)
                    
                    # Spirals Sa to Sc (intermediate): triangles
                    spiral_active = galaxy_type_spiral[active_indices]
                    if np.sum(spiral_active) > 0:
                        norm = mcolors.Normalize(vmin=vmin, vmax=vmax)
                        cmap_obj = cm.get_cmap(cmap)
                        edge_colors = cmap_obj(norm(c[active_mask][spiral_active]))
                        
                        ax.scatter(x[active_mask][spiral_active], 
                                 y[active_mask][spiral_active],
                                 c=c[active_mask][spiral_active], s=35, alpha=0.9, marker='^',
                                 vmin=vmin, vmax=vmax, cmap=cmap,
                                 rasterized=True, edgecolors=edge_colors, linewidths=0.5, zorder=2)
                    
                    # Sc to irregulars (disk-dominated): hexagons
                    late_active = galaxy_type_late[active_indices]
                    if np.sum(late_active) > 0:
                        norm = mcolors.Normalize(vmin=vmin, vmax=vmax)
                        cmap_obj = cm.get_cmap(cmap)
                        edge_colors = cmap_obj(norm(c[active_mask][late_active]))
                        
                        scatter = ax.scatter(x[active_mask][late_active], 
                                 y[active_mask][late_active],
                                 c=c[active_mask][late_active], s=35, alpha=0.9, marker='h',
                                 vmin=vmin, vmax=vmax, cmap=cmap,
                                 rasterized=True, edgecolors=edge_colors, linewidths=0.5, zorder=2)
                    
                    # Add colorbar (using the last scatter plot)
                    cbar = plt.colorbar(scatter, ax=ax)
                    cbar.set_label(prop_label, fontsize=10)
            else:
                # Plot all with colors (disk fraction)
                from matplotlib import cm
                import matplotlib.colors as mcolors
                
                norm = mcolors.Normalize(vmin=vmin, vmax=vmax)
                cmap_obj = cm.get_cmap(cmap)
                edge_colors = cmap_obj(norm(c))
                
                scatter = ax.scatter(x, y, c=c, s=35, alpha=0.9,
                                vmin=vmin, vmax=vmax,
                                cmap=cmap, rasterized=True,
                                edgecolors=edge_colors, linewidths=0.5, zorder=2)
                
                cbar = plt.colorbar(scatter, ax=ax)
                cbar.set_label(prop_label, fontsize=10)
            
            # # Add Drory & Fisher 2007 data on top row only
            # if row == 0:
            #     if col == 0:  # Merger-driven (classical bulges)
            #         for i, (ur, mr, mtype) in enumerate(zip(classical_data['ur'], 
            #                                                 classical_data['Mr'], 
            #                                                 classical_data['types'])):
            #             marker = marker_map[mtype]
            #             ax.scatter(mr, ur, c='limegreen', marker=marker, s=120,  # SWAPPED!
            #                     edgecolors='darkgreen', linewidths=2,
            #                     alpha=0.9, zorder=10)
                    
            #         ax.scatter([], [], c='limegreen', marker='s', s=120,
            #                 edgecolors='darkgreen', linewidths=2,
            #                 label='Classical (D&F 2007)', alpha=0.9)
                    
            #     else:  # Instability-driven (pseudo bulges)
            #         for i, (ur, mr, mtype) in enumerate(zip(pseudo_data['ur'],
            #                                                 pseudo_data['Mr'],
            #                                                 pseudo_data['types'])):
            #             marker = marker_map[mtype]
            #             ax.scatter(mr, ur, c='limegreen', marker=marker, s=120,  # SWAPPED!
            #                     edgecolors='darkgreen', linewidths=2,
            #                     alpha=0.9, zorder=10)
                    
            #         ax.scatter([], [], c='limegreen', marker='^', s=120,
            #                 edgecolors='darkgreen', linewidths=2,
            #                 label='Pseudo (D&F 2007)', alpha=0.9)
            
            # Labels - SWAPPED!
            ax.set_xlabel('$M_r$', fontsize=12)
            ax.set_ylabel('u - r', fontsize=12)
            ax.set_xlim(-24, -18)  # M_r range
            ax.set_ylim(0.0, 3.0)  # u-r range
            ax.invert_xaxis()  # Invert x-axis (brighter to the right)
            
            # Title on top row only
            if row == 0:
                ax.set_title(bulge_label, fontsize=13, fontweight='bold')
                ax.legend(loc='upper left', fontsize=9, frameon=True)
            
            ax.grid(True, alpha=0.3)

    plt.tight_layout()

    outputFile = OutputDir + '35.color_magnitude_by_bulge_type' + OutputFormat
    plt.savefig(outputFile, dpi=150)
    print(f'\nSaved file to {outputFile}\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Bulge Type Distribution (Figure 8) - Final Corrected Version')

    # Filter for galaxies with bulges
    w = np.where((BulgeMass > 0))[0]

    total_bulge_mass = BulgeMass[w]
    merger_bulge_mass = MergerBulgeMass[w]
    instability_bulge_mass = InstabilityBulgeMass[w]

    # Calculate the ratio Mi/(Mi + Mm)
    bulge_ratio = instability_bulge_mass / total_bulge_mass

    # Define bulge mass bins - REVERSED ORDER (massive at top)
    mass_bins = [
        (11.0, 12.5, r'$11.0 < \log(M_{\rm bulge}/M_\odot) < 12.5$'),
        (10.5, 11.0, r'$10.5 < \log(M_{\rm bulge}/M_\odot) < 11.0$'),
        (10.0, 10.5, r'$10.0 < \log(M_{\rm bulge}/M_\odot) < 10.5$'),
        (9.0, 10.0, r'$9.0 < \log(M_{\rm bulge}/M_\odot) < 10.0$'),
        (8.0, 9.0, r'$8.0 < \log(M_{\rm bulge}/M_\odot) < 9.0$')
    ]

    # Create figure - SINGLE COLUMN
    fig, axes = plt.subplots(5, 1, figsize=(6, 15))

    for idx, (mass_min, mass_max, title) in enumerate(mass_bins):
        ax = axes[idx]
        
        # Select bulges in this mass range
        log_mass = np.log10(total_bulge_mass)
        mass_mask = (log_mass >= mass_min) & (log_mass < mass_max)
        ratios_in_bin = bulge_ratio[mass_mask]
        
        n_galaxies = len(ratios_in_bin)
        print(f"\nBulge mass bin {mass_min}-{mass_max}: {n_galaxies} galaxies")
        
        # Create histogram with 10 bins
        n_bins = 10
        counts, bin_edges = np.histogram(ratios_in_bin, bins=n_bins, range=(0, 1))
        bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
        bin_width = bin_edges[1] - bin_edges[0]
        
        # Calculate density: normalize by total AND by bin width
        if n_galaxies > 0:
            density = counts / (n_galaxies * bin_width)
        else:
            density = np.zeros_like(counts, dtype=float)
        
        print(f"  Count distribution: {counts}")
        print(f"  Density range: {density[density>0].min():.4f} to {density.max():.4f}")
        
        # CRITICAL: Only plot bins with non-zero counts
        # For zero bins, we'll use a floor value for the y-axis but not actually draw them
        nonzero_mask = counts > 0
        
        # Plot only non-zero bins
        ax.bar(bin_centers[nonzero_mask], density[nonzero_mask],
            width=bin_width * 0.95, 
            color='steelblue',
            edgecolor='navy',
            alpha=0.8,
            linewidth=0.5)
        
        # Set log scale
        ax.set_yscale('log')
        
        # Formatting
        ax.set_xlabel(r'$M_i/(M_i + M_m)$', fontsize=12)
        ax.set_ylabel(r'Log(N)', fontsize=12)
        ax.set_xlim(-0.05, 1.05)
        
        # SAME Y-AXIS for ALL panels
        # Use a range that captures the data well
        ax.set_ylim(1e-2, 2e1)  # Adjusted to better show your data range
        
        # Add title at top of panel
        ax.text(0.5, 0.95, title,
                transform=ax.transAxes, ha='center', va='top', fontsize=11,
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        
        # Statistics
        if n_galaxies > 0:
            pure_merger = np.sum(ratios_in_bin < 0.1)
            pure_instability = np.sum(ratios_in_bin > 0.9)
            mixed = n_galaxies - pure_merger - pure_instability
            
            stats_text = f'N = {n_galaxies}\n'
            stats_text += f'Merger: {100*pure_merger/n_galaxies:.0f}%\n'
            stats_text += f'Instability: {100*pure_instability/n_galaxies:.0f}%'
            
            ax.text(0.98, 0.02, stats_text,
                    transform=ax.transAxes, ha='right', va='bottom', fontsize=9,
                    bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.9),
                    family='monospace')
            
            # Print to console
            print(f"  Pure merger (<0.1): {pure_merger} ({100*pure_merger/n_galaxies:.1f}%)")
            print(f"  Pure instability (>0.9): {pure_instability} ({100*pure_instability/n_galaxies:.1f}%)")
            print(f"  Mixed (0.1-0.9): {mixed} ({100*mixed/n_galaxies:.1f}%)")
        
        # Grid
        ax.grid(True, alpha=0.3, which='major', linestyle='-', linewidth=0.5)
        ax.grid(True, alpha=0.15, which='minor', linestyle=':', linewidth=0.3)

    plt.tight_layout()

    outputFile = OutputDir + '33.bulge_type_distribution' + OutputFormat
    plt.savefig(outputFile, dpi=150)
    print(f'\nSaved file to {outputFile}\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Bulge to Total Ratio Distribution vs Redshift')

    redshifts = []
    bt_median = []
    bt_16 = []
    bt_84 = []

    # Loop over all snapshots
    for snap in range(64):
        snap_str = f'Snap_{snap}'
        try:
            # 1. Get Redshift
            f = h5.File(DirName+FileName, 'r')
            if snap_str not in f:
                f.close()
                continue
            z = f[snap_str].attrs['redshift']
            f.close()

            # 2. Read Arrays (Read TYPE here to ensure length matches)
            Type_snap = read_hdf(snap_num=snap_str, param='Type')
            StellarMass_snap = read_hdf(snap_num=snap_str, param='StellarMass') * 1.0e10 / Hubble_h
            
            # Use BulgeMass directly (Best Practice)
            BulgeMass_snap = read_hdf(snap_num=snap_str, param='BulgeMass') * 1.0e10 / Hubble_h

            # 3. Filter: Mass > 10.5 AND Centrals (Type 0)
            # Isolating centrals removes "stripped" satellites, giving a cleaner physics signal
            w = np.where((StellarMass_snap > 10**10.5) & (Type_snap == 0))[0]
            
            # 4. Safety Check: Only calculate if we have enough galaxies
            # This removes the jagged noise at high-z
            if len(w) > 1:
                stellar_mass = StellarMass_snap[w]
                bulge_mass = BulgeMass_snap[w]
                
                # Calculate Ratios
                ratios = bulge_mass / np.maximum(stellar_mass, 1e-10)
                ratios = np.clip(ratios, 0.0, 1.0) 
                
                # Calculate Stats
                p16, p50, p84 = np.percentile(ratios, [16, 50, 84])
                
                redshifts.append(z)
                bt_median.append(p50)
                bt_16.append(p16)
                bt_84.append(p84)
                
                print(f"Snap {snap} (z={z:.2f}): N={len(w)} | Median B/T={p50:.3f}")
            else:
                print(f"Snap {snap} (z={z:.2f}): Not enough massive centrals (N={len(w)})")

        except Exception as e:
            print(f"Skipping snapshot {snap}: {e}")
            continue

    # Sort by redshift (high to low)
    redshifts = np.array(redshifts)
    bt_median = np.array(bt_median)
    bt_16 = np.array(bt_16)
    bt_84 = np.array(bt_84)
    
    sort_idx = np.argsort(redshifts)
    redshifts = redshifts[sort_idx]
    bt_median = bt_median[sort_idx]
    bt_16 = bt_16[sort_idx]
    bt_84 = bt_84[sort_idx]

    plt.figure()
    
    # Plot median
    plt.plot(redshifts, bt_median, 'k-', linewidth=2, label='Median')
    
    # Plot scatter
    plt.fill_between(redshifts, bt_16, bt_84, color='gray', alpha=0.3, label='16th-84th Percentile')
    
    plt.xlabel('Redshift')
    plt.ylabel(r'Bulge-to-Total Ratio ($M_{\mathrm{bulge}}/M_{\mathrm{stars}}$)')
    plt.xlim(0, 6)  # Adjust as needed
    plt.ylim(0, 1.0)
    plt.grid(True, alpha=0.3)
    plt.legend(loc='upper left', frameon=False)
    
    # Invert x-axis to show time evolution (high z to low z)
    plt.gca().invert_xaxis()

    outputFile = OutputDir + '35.bulge_to_total_evolution' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

print('Plotting Merger vs Instability Bulge Mass-Size Relations (Diagnostic)')

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

# ===== LEFT: Merger Bulge Radius vs Merger Bulge Mass =====
w_merger = np.where((MergerBulgeMass > 0) & (MergerBulgeRadius > 0))[0]
if len(w_merger) > dilute: w_merger = sample(list(w_merger), dilute)

merger_mass = MergerBulgeMass[w_merger]
merger_radius = MergerBulgeRadius[w_merger]

log10_merger_mass = np.log10(merger_mass)
merger_radius_kpc = merger_radius / Hubble_h / 0.001  # Convert to kpc
log10_merger_radius = np.log10(merger_radius_kpc)

# Color by total stellar mass to see galaxy context
stellar_mass_merger = StellarMass[w_merger]
log10_stellar_mass_merger = np.log10(stellar_mass_merger)

sc1 = ax1.scatter(log10_merger_mass, log10_merger_radius, 
                   c=log10_stellar_mass_merger,
                   cmap='plasma', s=15, alpha=0.6, vmin=8, vmax=12)
cbar1 = plt.colorbar(sc1, ax=ax1)
cbar1.set_label(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=12)

# Add Shen+2003 equation 33: log(R/kpc) = 0.56 log(M/Msun) - 5.54
M_range = np.logspace(8, 12, 100)
log_R_shen = 0.56 * np.log10(M_range) - 5.54
ax1.plot(np.log10(M_range), log_R_shen, 'k--', linewidth=2, 
         label='Shen+2003 (eq. 33)', zorder=10)

ax1.set_xlabel(r'$\log_{10} M_{\mathrm{merger\ bulge}}\ (M_{\odot})$', fontsize=13)
ax1.set_ylabel(r'$\log_{10} R_{\mathrm{merger\ bulge}}\ (\mathrm{kpc})$', fontsize=13)
ax1.set_xlim(8.0, 12.0)
ax1.set_ylim(-2, 2.0)
ax1.set_title('Merger-Driven Bulges (Classical)', fontsize=14, fontweight='bold')
ax1.legend(loc='upper left', frameon=True)
ax1.grid(True, alpha=0.3)

print(f"Merger bulge sample: {len(w_merger)} galaxies")
print(f"  Mass range: 10^{log10_merger_mass.min():.1f} to 10^{log10_merger_mass.max():.1f} Msun")
print(f"  Radius range: {merger_radius_kpc.min():.2f} to {merger_radius_kpc.max():.1f} kpc")

# ===== RIGHT: Instability Bulge Radius vs Instability Bulge Mass =====
w_instability = np.where((InstabilityBulgeMass > 0) & (InstabilityBulgeRadius > 0))[0]
if len(w_instability) > dilute: w_instability = sample(list(w_instability), dilute)

instability_mass = InstabilityBulgeMass[w_instability]
instability_radius = InstabilityBulgeRadius[w_instability]

log10_instability_mass = np.log10(instability_mass)
instability_radius_kpc = instability_radius / Hubble_h / 0.001  # Convert to kpc
log10_instability_radius = np.log10(instability_radius_kpc)

# Color by disk radius to show disc connection
disk_radius_inst = DiskRadius[w_instability]
disk_radius_kpc_inst = disk_radius_inst / Hubble_h / 0.001
log10_disk_radius_inst = np.log10(disk_radius_kpc_inst)

sc2 = ax2.scatter(log10_instability_mass, log10_instability_radius,
                   c=log10_disk_radius_inst,
                   cmap='viridis', s=15, alpha=0.6, vmin=-1, vmax=2)
cbar2 = plt.colorbar(sc2, ax=ax2)
cbar2.set_label(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$', fontsize=12)

# Add disc-scaling reference line: R_instability = 0.1 * R_disc_typical
# For a typical disc of M=10^10, R~5 kpc, so R_inst ~ 0.5 kpc
# This is a GUIDE showing typical scaling, not a fit
M_range_inst = np.logspace(8, 11, 100)
R_typical_inst = 0.5 * (M_range_inst / 1e10)**0.2  # Shallow slope
ax2.plot(np.log10(M_range_inst), np.log10(R_typical_inst), 'r--', 
         linewidth=2, label=r'Disc-scaling (shallow)', zorder=10)

# Also show the Shen relation for comparison
ax2.plot(np.log10(M_range), log_R_shen, 'k:', linewidth=1.5, 
         label='Shen+2003 (for reference)', alpha=0.5, zorder=9)

ax2.set_xlabel(r'$\log_{10} M_{\mathrm{instability\ bulge}}\ (M_{\odot})$', fontsize=13)
ax2.set_ylabel(r'$\log_{10} R_{\mathrm{instability\ bulge}}\ (\mathrm{kpc})$', fontsize=13)
ax2.set_xlim(8.0, 12.0)
ax2.set_ylim(-2, 2.0)
ax2.set_title('Instability-Driven Bulges (Pseudo)', fontsize=14, fontweight='bold')
ax2.legend(loc='upper left', frameon=True)
ax2.grid(True, alpha=0.3)

print(f"Instability bulge sample: {len(w_instability)} galaxies")
print(f"  Mass range: 10^{log10_instability_mass.min():.1f} to 10^{log10_instability_mass.max():.1f} Msun")
print(f"  Radius range: {instability_radius_kpc.min():.2f} to {instability_radius_kpc.max():.1f} kpc")

plt.tight_layout()

outputFile = OutputDir + '36.bulge_type_mass_size_diagnostic' + OutputFormat
plt.savefig(outputFile, dpi=150)
print(f'Saved file to {outputFile}\n')
plt.close()

# -------------------------------------------------------

print('Plotting Bulge Radius vs Disk Radius by Bulge Type (Diagnostic)')

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

# ===== LEFT: Merger Bulges =====
w_merger_disk = np.where((MergerBulgeMass > 0) & (MergerBulgeRadius > 0) & 
                         (DiskRadius > 0) & (StellarMass > 1e9))[0]
if len(w_merger_disk) > dilute: w_merger_disk = sample(list(w_merger_disk), dilute)

merger_bulge_r = MergerBulgeRadius[w_merger_disk] / Hubble_h / 0.001  # kpc
disk_r_merger = DiskRadius[w_merger_disk] / Hubble_h / 0.001  # kpc
merger_mass_frac = MergerBulgeMass[w_merger_disk] / StellarMass[w_merger_disk]

sc1 = ax1.scatter(np.log10(disk_r_merger), np.log10(merger_bulge_r),
                   c=merger_mass_frac, cmap='Reds', s=15, alpha=0.6, 
                   vmin=0, vmax=1)
cbar1 = plt.colorbar(sc1, ax=ax1)
cbar1.set_label(r'$M_{\mathrm{merger}}/M_{\mathrm{stars}}$', fontsize=12)

# Add reference lines
disk_range = np.linspace(-1, 2.5, 100)
ax1.plot(disk_range, disk_range, 'k:', linewidth=1, alpha=0.5, label='1:1')
ax1.plot(disk_range, disk_range + np.log10(0.1), 'b--', linewidth=1.5, 
         label=r'$R_{\rm bulge} = 0.1 R_{\rm disk}$', alpha=0.7)

ax1.set_xlabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$', fontsize=13)
ax1.set_ylabel(r'$\log_{10} R_{\mathrm{merger\ bulge}}\ (\mathrm{kpc})$', fontsize=13)
ax1.set_xlim(-0.5, 2.5)
ax1.set_ylim(-1.5, 2.0)
ax1.set_title('Merger-Driven Bulges vs Disk', fontsize=14, fontweight='bold')
ax1.legend(loc='upper left', frameon=True)
ax1.grid(True, alpha=0.3)

print(f"Merger bulges with disks: {len(w_merger_disk)} galaxies")

# ===== RIGHT: Instability Bulges =====
w_inst_disk = np.where((InstabilityBulgeMass > 0) & (InstabilityBulgeRadius > 0) & 
                        (DiskRadius > 0) & (StellarMass > 1e9))[0]
if len(w_inst_disk) > dilute: w_inst_disk = sample(list(w_inst_disk), dilute)

inst_bulge_r = InstabilityBulgeRadius[w_inst_disk] / Hubble_h / 0.001  # kpc
disk_r_inst = DiskRadius[w_inst_disk] / Hubble_h / 0.001  # kpc
inst_mass_frac = InstabilityBulgeMass[w_inst_disk] / StellarMass[w_inst_disk]

sc2 = ax2.scatter(np.log10(disk_r_inst), np.log10(inst_bulge_r),
                   c=inst_mass_frac, cmap='Blues', s=15, alpha=0.6,
                   vmin=0, vmax=1)
cbar2 = plt.colorbar(sc2, ax=ax2)
cbar2.set_label(r'$M_{\mathrm{instability}}/M_{\mathrm{stars}}$', fontsize=12)

# Add reference lines
ax2.plot(disk_range, disk_range, 'k:', linewidth=1, alpha=0.5, label='1:1')
ax2.plot(disk_range, disk_range + np.log10(0.1), 'r--', linewidth=2, 
         label=r'$R_{\rm bulge} = 0.1 R_{\rm disk}$', alpha=0.7)

ax2.set_xlabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$', fontsize=13)
ax2.set_ylabel(r'$\log_{10} R_{\mathrm{instability\ bulge}}\ (\mathrm{kpc})$', fontsize=13)
ax2.set_xlim(-0.5, 2.5)
ax2.set_ylim(-1.5, 2.0)
ax2.set_title('Instability-Driven Bulges vs Disk', fontsize=14, fontweight='bold')
ax2.legend(loc='upper left', frameon=True)
ax2.grid(True, alpha=0.3)

print(f"Instability bulges with disks: {len(w_inst_disk)} galaxies")
print(f"  Typical R_bulge/R_disk = {np.median(inst_bulge_r/disk_r_inst):.2f}")

plt.tight_layout()

outputFile = OutputDir + '37.bulge_disk_radius_by_type' + OutputFormat
plt.savefig(outputFile, dpi=150)
print(f'Saved file to {outputFile}\n')
plt.close()

# -------------------------------------------------------

print('Plotting Combined Bulge Mass-Size with Type Separation (Diagnostic)')

fig = plt.figure(figsize=(12, 10))

# Filter for galaxies with bulges
w_has_bulge = np.where((BulgeMass > 0) & (BulgeRadius > 0))[0]

# Classify bulge types
bulge_ratio = InstabilityBulgeMass[w_has_bulge] / BulgeMass[w_has_bulge]
merger_dominated = bulge_ratio < 0.1
instability_dominated = bulge_ratio > 0.9
mixed = (bulge_ratio >= 0.1) & (bulge_ratio <= 0.9)

print(f"\nTotal galaxies with bulges: {len(w_has_bulge)}")
print(f"  Merger-dominated (ratio<0.1): {np.sum(merger_dominated)} ({100*np.sum(merger_dominated)/len(w_has_bulge):.1f}%)")
print(f"  Instability-dominated (ratio>0.9): {np.sum(instability_dominated)} ({100*np.sum(instability_dominated)/len(w_has_bulge):.1f}%)")
print(f"  Mixed (0.1≤ratio≤0.9): {np.sum(mixed)} ({100*np.sum(mixed)/len(w_has_bulge):.1f}%)")

# Subsample if needed
if np.sum(merger_dominated) > dilute:
    merger_sample = np.random.choice(np.where(merger_dominated)[0], dilute, replace=False)
else:
    merger_sample = np.where(merger_dominated)[0]

if np.sum(instability_dominated) > dilute:
    inst_sample = np.random.choice(np.where(instability_dominated)[0], dilute, replace=False)
else:
    inst_sample = np.where(instability_dominated)[0]

if np.sum(mixed) > dilute//2:
    mixed_sample = np.random.choice(np.where(mixed)[0], dilute//2, replace=False)
else:
    mixed_sample = np.where(mixed)[0]

# Get data for each type
merger_bulge_mass = BulgeMass[w_has_bulge][merger_sample]
merger_bulge_radius = BulgeRadius[w_has_bulge][merger_sample] / Hubble_h / 0.001  # kpc

inst_bulge_mass = BulgeMass[w_has_bulge][inst_sample]
inst_bulge_radius = BulgeRadius[w_has_bulge][inst_sample] / Hubble_h / 0.001  # kpc

mixed_bulge_mass = BulgeMass[w_has_bulge][mixed_sample]
mixed_bulge_radius = BulgeRadius[w_has_bulge][mixed_sample] / Hubble_h / 0.001  # kpc

# Plot each type
plt.scatter(np.log10(merger_bulge_mass), np.log10(merger_bulge_radius),
            c='orangered', s=20, alpha=0.6, label='Merger-driven', 
            edgecolors='darkred', linewidths=0.3)

plt.scatter(np.log10(inst_bulge_mass), np.log10(inst_bulge_radius),
            c='dodgerblue', s=20, alpha=0.6, label='Instability-driven',
            edgecolors='darkblue', linewidths=0.3)

plt.scatter(np.log10(mixed_bulge_mass), np.log10(mixed_bulge_radius),
            c='mediumorchid', s=15, alpha=0.4, label='Mixed',
            edgecolors='purple', linewidths=0.2)

# Add theoretical relations
M_range = np.logspace(8, 12, 100)

# Shen+2003 equation 33 (steep for classical bulges)
log_R_shen = 0.56 * np.log10(M_range) - 5.54
plt.plot(np.log10(M_range), log_R_shen, 'k--', linewidth=2, 
         label='Shen+2003 (classical)', zorder=10)

# Shallow pseudo-bulge relation (approximate)
log_R_pseudo = 0.25 * np.log10(M_range) - 2.5
plt.plot(np.log10(M_range), log_R_pseudo, 'g--', linewidth=2,
         label='Pseudo-bulge (shallow)', alpha=0.7, zorder=10)

plt.xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$', fontsize=14)
plt.ylabel(r'$\log_{10} R_{\mathrm{bulge}}\ (\mathrm{kpc})$', fontsize=14)
plt.xlim(8.0, 12.0)
plt.ylim(-2, 2.0)
plt.title('Bulge Mass-Size by Formation Type', fontsize=15, fontweight='bold')
plt.legend(loc='upper left', frameon=True, fontsize=11)
plt.grid(True, alpha=0.3)

outputFile = OutputDir + '38.combined_bulge_mass_size_by_type' + OutputFormat
plt.savefig(outputFile, dpi=150)
print(f'Saved file to {outputFile}\n')
plt.close()

print('\n=== Diagnostic Plots Complete ===\n')