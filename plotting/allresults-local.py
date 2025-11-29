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

# --------------------------------------------------------

    print('Plotting the stellar mass function')

    def load_li_white_2009(filepath, hubble_h=0.73, whichimf=1):
        """Load Li & White 2009 z~0 SMF data with FIXED error handling"""
        try:
            data = np.genfromtxt(filepath, comments='#')
            
            # Column 1: log stellar mass in Msun/h^2
            log_mass = data[:, 0] + 2.0 * np.log10(hubble_h)
            
            # Column 2: log(phi)
            log_phi = data[:, 1]
            
            # Columns 3, 4: error_lower, error_upper (in log space)
            err_lower = np.abs(data[:, 2])  # Ensure positive
            err_upper = np.abs(data[:, 3])  # Ensure positive
            
            # Convert to linear phi
            phi = 10**log_phi
            phi_upper = 10**(log_phi + err_upper)
            phi_lower = 10**(log_phi - err_lower)
            
            # Calculate error bar magnitudes (always positive)
            yerr_lower = np.abs(phi - phi_lower)
            yerr_upper = np.abs(phi_upper - phi)
            
            # Filter valid points
            mask = np.isfinite(log_mass) & np.isfinite(phi) & (phi > 0)
            mask &= np.isfinite(yerr_lower) & np.isfinite(yerr_upper)
            
            return log_mass[mask], phi[mask], yerr_lower[mask], yerr_upper[mask]
            
        except Exception as e:
            print(f"Error loading Li & White 2009 data: {e}")
            import traceback
            traceback.print_exc()
            return None, None, None, None


    def load_muzzin_2013(filepath, z_low=0.2, z_high=0.5, hubble_h=0.73, whichimf=1):
        """Load Muzzin et al. 2013 SMF data with FIXED error handling"""
        try:
            data = np.genfromtxt(filepath, comments='#')
            
            # Filter for redshift bin
            mask_z = (data[:, 0] == z_low) & (data[:, 1] == z_high)
            
            if not np.any(mask_z):
                print(f"No Muzzin+13 data for z={z_low}-{z_high}")
                return None, None, None, None
            
            stellar_mass = data[mask_z, 2]
            log_phi = data[mask_z, 4]
            err_upper = np.abs(data[mask_z, 5])  # Ensure positive
            err_lower = np.abs(data[mask_z, 6])  # Ensure positive
            
            # Filter out -99 (no data)
            mask_valid = (log_phi > -90)
            
            stellar_mass = stellar_mass[mask_valid]
            log_phi = log_phi[mask_valid]
            err_upper = err_upper[mask_valid]
            err_lower = err_lower[mask_valid]
            
            # Cosmology correction
            h_muzzin = 0.7
            h_ours = hubble_h
            
            # Convert phi
            phi = 10**log_phi * (h_muzzin / h_ours)**3
            phi_upper = 10**(log_phi + err_upper) * (h_muzzin / h_ours)**3
            phi_lower = 10**(log_phi - err_lower) * (h_muzzin / h_ours)**3
            
            # Calculate error bar magnitudes
            yerr_lower = np.abs(phi - phi_lower)
            yerr_upper = np.abs(phi_upper - phi)
            
            # IMF correction
            if whichimf == 1:
                stellar_mass = stellar_mass - 0.04
            
            return stellar_mass, phi, yerr_lower, yerr_upper
            
        except Exception as e:
            print(f"Error loading Muzzin+13 data: {e}")
            import traceback
            traceback.print_exc()
            return None, None, None, None


    def load_santini_2012(filepath, z_low=0.6, z_high=1.0, hubble_h=0.73, whichimf=1):
        """Load Santini et al. 2012 SMF data with FIXED error handling"""
        try:
            data = np.genfromtxt(filepath, comments='#')
            
            # Filter for redshift bin
            mask_z = (data[:, 0] == z_low) & (data[:, 1] == z_high)
            
            if not np.any(mask_z):
                print(f"No Santini+12 data for z={z_low}-{z_high}")
                return None, None, None, None
            
            log_mass = data[mask_z, 2]
            log_phi = data[mask_z, 3]
            err_hi = np.abs(data[mask_z, 4])  # Ensure positive
            err_lo = np.abs(data[mask_z, 5])  # Ensure positive
            
            # Cosmology correction
            h_santini = 0.7
            h_ours = hubble_h
            
            # Convert phi
            phi = 10**log_phi * (h_santini / h_ours)**3
            phi_upper = 10**(log_phi + err_hi) * (h_santini / h_ours)**3
            phi_lower = 10**(log_phi - err_lo) * (h_santini / h_ours)**3
            
            # Calculate error bar magnitudes
            yerr_lower = np.abs(phi - phi_lower)
            yerr_upper = np.abs(phi_upper - phi)
            
            # IMF correction
            if whichimf == 1:
                log_mass = log_mass - 0.24
            
            return log_mass, phi, yerr_lower, yerr_upper
            
        except Exception as e:
            print(f"Error loading Santini+12 data: {e}")
            import traceback
            traceback.print_exc()
            return None, None, None, None


    def load_wright_2018(filepath, target_z=0.5):
        """Load Wright et al. 2018 SMF data with FIXED error handling"""
        try:
            data = np.genfromtxt(filepath, comments='#')
            mask = data[:, 0] == target_z
            
            if not np.any(mask):
                print(f"No Wright+18 data for z={target_z}")
                return None, None, None, None
            
            stellar_mass = data[mask, 1]
            log_phi = data[mask, 2]
            err_upper = np.abs(data[mask, 3])  # Ensure positive
            err_lower = np.abs(data[mask, 4])  # Ensure positive
            
            # Convert from 0.25 dex bins to per dex
            log_phi_per_dex = log_phi + np.log10(4.0)
            phi = 10**log_phi_per_dex
            phi_upper = 10**(log_phi_per_dex + err_upper)
            phi_lower = 10**(log_phi_per_dex - err_lower)
            
            # Calculate error bar magnitudes
            yerr_lower = np.abs(phi - phi_lower)
            yerr_upper = np.abs(phi_upper - phi)
            
            return stellar_mass, phi, yerr_lower, yerr_upper
            
        except Exception as e:
            print(f"Error loading Wright+18 data: {e}")
            import traceback
            traceback.print_exc()
            return None, None, None, None


    def load_shark_z0(filepath):
        """Load SHARK z=0 SMF data"""
        try:
            data = []
            with open(filepath, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line.startswith('﻿'):  # Remove BOM
                        line = line[1:]
                    values = line.split(',')
                    if len(values) >= 2:
                        mass = float(values[0])
                        phi = float(values[1])
                        data.append([mass, phi])
            data = np.array(data)
            x = data[:, 0]
            y = 10**data[:, 1]  # Convert from log to linear
            mask = np.isfinite(x) & np.isfinite(y) & (y > 0)
            return x[mask], y[mask]
        except Exception as e:
            print(f"Error loading SHARK data: {e}")
            import traceback
            traceback.print_exc()
            return None, None

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width

    # calculate all
    w = np.where(StellarMass > 0.0)[0]
    mass = np.log10(StellarMass[w])
    sSFR = np.log10( (SfrDisk[w] + SfrBulge[w]) / StellarMass[w] )

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins
    
    # additionally calculate red
    w = np.where(sSFR < sSFRcut)[0]
    massRED = mass[w]
    (countsRED, binedges) = np.histogram(massRED, range=(mi, ma), bins=NB)

    # additionally calculate blue
    w = np.where(sSFR > sSFRcut)[0]
    massBLU = mass[w]
    (countsBLU, binedges) = np.histogram(massBLU, range=(mi, ma), bins=NB)

    # Baldry+ 2008 modified data used for the MCMC fitting
    Baldry = np.array([
        [7.05, 1.3531e-01, 6.0741e-02],
        [7.15, 1.3474e-01, 6.0109e-02],
        [7.25, 2.0971e-01, 7.7965e-02],
        [7.35, 1.7161e-01, 3.1841e-02],
        [7.45, 2.1648e-01, 5.7832e-02],
        [7.55, 2.1645e-01, 3.9988e-02],
        [7.65, 2.0837e-01, 4.8713e-02],
        [7.75, 2.0402e-01, 7.0061e-02],
        [7.85, 1.5536e-01, 3.9182e-02],
        [7.95, 1.5232e-01, 2.6824e-02],
        [8.05, 1.5067e-01, 4.8824e-02],
        [8.15, 1.3032e-01, 2.1892e-02],
        [8.25, 1.2545e-01, 3.5526e-02],
        [8.35, 9.8472e-02, 2.7181e-02],
        [8.45, 8.7194e-02, 2.8345e-02],
        [8.55, 7.0758e-02, 2.0808e-02],
        [8.65, 5.8190e-02, 1.3359e-02],
        [8.75, 5.6057e-02, 1.3512e-02],
        [8.85, 5.1380e-02, 1.2815e-02],
        [8.95, 4.4206e-02, 9.6866e-03],
        [9.05, 4.1149e-02, 1.0169e-02],
        [9.15, 3.4959e-02, 6.7898e-03],
        [9.25, 3.3111e-02, 8.3704e-03],
        [9.35, 3.0138e-02, 4.7741e-03],
        [9.45, 2.6692e-02, 5.5029e-03],
        [9.55, 2.4656e-02, 4.4359e-03],
        [9.65, 2.2885e-02, 3.7915e-03],
        [9.75, 2.1849e-02, 3.9812e-03],
        [9.85, 2.0383e-02, 3.2930e-03],
        [9.95, 1.9929e-02, 2.9370e-03],
        [10.05, 1.8865e-02, 2.4624e-03],
        [10.15, 1.8136e-02, 2.5208e-03],
        [10.25, 1.7657e-02, 2.4217e-03],
        [10.35, 1.6616e-02, 2.2784e-03],
        [10.45, 1.6114e-02, 2.1783e-03],
        [10.55, 1.4366e-02, 1.8819e-03],
        [10.65, 1.2588e-02, 1.8249e-03],
        [10.75, 1.1372e-02, 1.4436e-03],
        [10.85, 9.1213e-03, 1.5816e-03],
        [10.95, 6.1125e-03, 9.6735e-04],
        [11.05, 4.3923e-03, 9.6254e-04],
        [11.15, 2.5463e-03, 5.0038e-04],
        [11.25, 1.4298e-03, 4.2816e-04],
        [11.35, 6.4867e-04, 1.6439e-04],
        [11.45, 2.8294e-04, 9.9799e-05],
        [11.55, 1.0617e-04, 4.9085e-05],
        [11.65, 3.2702e-05, 2.4546e-05],
        [11.75, 1.2571e-05, 1.2571e-05],
        [11.85, 8.4589e-06, 8.4589e-06],
        [11.95, 7.4764e-06, 7.4764e-06],
        ], dtype=np.float32)

    Baldry_xval = np.log10(10 ** Baldry[:, 0]  /Hubble_h/Hubble_h)
    if(whichimf == 1):  Baldry_xval = Baldry_xval - 0.26  # convert back to Chabrier IMF
    Baldry_yvalU = (Baldry[:, 1]+Baldry[:, 2]) * Hubble_h*Hubble_h*Hubble_h
    Baldry_yvalL = (Baldry[:, 1]-Baldry[:, 2]) * Hubble_h*Hubble_h*Hubble_h
    plt.fill_between(Baldry_xval, Baldry_yvalU, Baldry_yvalL, 
        facecolor='purple', alpha=0.25)
    
    # This next line is just to get the shaded region to appear correctly in the legend
    plt.plot(xaxeshisto, counts / volume / binwidth, label='Baldry et al. 2008', color='purple', alpha=0.3)

    # 2. Muzzin 2013
    muz_x, muz_y, muz_err_lower, muz_err_upper = load_muzzin_2013(
        './data/SMF_Muzzin2013.dat', z_low=0.2, z_high=0.5,
        hubble_h=Hubble_h, whichimf=whichimf)

    if muz_x is not None:
        plt.errorbar(muz_x, muz_y, yerr=[muz_err_lower, muz_err_upper],
                    fmt='o', color='grey', markersize=5, 
                    label='Muzzin+13 (0.2<z<0.5)', alpha=0.7,
                    capsize=3, elinewidth=1.5, zorder=2)
        print(f'  ✓ Muzzin+13: {len(muz_x)} points')

    # 3. Santini 2012
    san_x, san_y, san_err_lower, san_err_upper = load_santini_2012(
        './data/SMF_Santini2012.dat', z_low=0.6, z_high=1.0,
        hubble_h=Hubble_h, whichimf=whichimf)

    if san_x is not None:
        plt.errorbar(san_x, san_y, yerr=[san_err_lower, san_err_upper],
                    fmt='^', color='grey', markersize=5, 
                    label='Santini+12 (0.6<z<1.0)', alpha=0.7,
                    capsize=3, elinewidth=1.5, zorder=2)
        print(f'  ✓ Santini+12: {len(san_x)} points')

    # 4. SHARK z=0
    shark_x, shark_y = load_shark_z0('./data/SHARK_smf_z0.csv')
    if shark_x is not None:
        plt.plot(shark_x, shark_y, ':', color='orange', linewidth=2.5, 
                label='SHARK', alpha=0.9, zorder=3)
        print(f'  ✓ SHARK z=0: {len(shark_x)} points')

    # 5. Wright+18
    wri_x, wri_y, wri_err_lower, wri_err_upper = load_wright_2018(
        './data/Wright18_CombinedSMF.dat', target_z=0.5)

    if wri_x is not None:
        plt.errorbar(wri_x, wri_y, yerr=[wri_err_lower, wri_err_upper],
                    fmt='D', color='grey', markersize=5, 
                    label='Wright+18 (z=0.5)', alpha=0.7,
                    capsize=3, elinewidth=1.5, zorder=2)
        print(f'  ✓ Wright+18: {len(wri_x)} points')

        
    # Overplot the model histograms
    plt.plot(xaxeshisto, counts    / volume / binwidth, 'k-', label='SAGE26')
    # plt.plot(xaxeshisto, countsRED / volume / binwidth, 'r:', lw=2, label='Model - Red')
    # plt.plot(xaxeshisto, countsBLU / volume / binwidth, 'b:', lw=2, label='Model - Blue')

    plt.yscale('log')
    plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '1.StellarMassFunction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the baryonic mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width
  
    # calculate BMF
    w = np.where(StellarMass + ColdGas > 0.0)[0]
    mass = np.log10( (StellarMass[w] + ColdGas[w]) )

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    centrals = np.where(Type[w] == 0)[0]
    satellites = np.where(Type[w] == 1)[0]

    centrals_mass = mass[centrals]
    satellites_mass = mass[satellites]

    mi = np.floor(min(centrals_mass)) - 2
    ma = np.floor(max(centrals_mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts_centrals, binedges_centrals) = np.histogram(centrals_mass, range=(mi, ma), bins=NB)
    xaxeshisto_centrals = binedges_centrals[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    mi = np.floor(min(satellites_mass)) - 2
    ma = np.floor(max(satellites_mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts_satellites, binedges_satellites) = np.histogram(satellites_mass, range=(mi, ma), bins=NB)
    xaxeshisto_satellites = binedges_satellites[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    # Bell et al. 2003 BMF (h=1.0 converted to h=0.73)
    M = np.arange(7.0, 13.0, 0.01)
    Mstar = np.log10(5.3*1.0e10 /Hubble_h/Hubble_h)
    alpha = -1.21
    phistar = 0.0108 *Hubble_h*Hubble_h*Hubble_h
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)
    
    if(whichimf == 0):
        # converted diet Salpeter IMF to Salpeter IMF
        plt.plot(np.log10(10.0**M /0.7), yval, 'b-', lw=2.0, label='Bell et al. 2003')  # Plot the SMF
    elif(whichimf == 1):
        # converted diet Salpeter IMF to Salpeter IMF, then to Chabrier IMF
        plt.plot(np.log10(10.0**M /0.7 /1.8), yval, 'g--', lw=1.5, label='Bell et al. 2003')  # Plot the SMF

    # Overplot the model histograms
    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model')
    plt.plot(xaxeshisto_centrals, counts_centrals / volume / binwidth, 'b:', lw=2, label='Model - Centrals')
    plt.plot(xaxeshisto_satellites, counts_satellites / volume / binwidth, 'g--', lw=1.5, label='Model - Satellites')

    plt.yscale('log')
    plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
    
    outputFile = OutputDir + '2.BaryonicMassFunction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the cold gas mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width

    # calculate all
    w = np.where(ColdGas > 0.0)[0]
    mass = np.log10(ColdGas[w])
    H2mass = np.log10(H2gas[w])
    H1mass = np.log10(ColdGas[w] - H2gas[w])
    sSFR = (SfrDisk[w] + SfrBulge[w]) / StellarMass[w]

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)

    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    (counts_h2, binedges_h2) = np.histogram(H2mass, range=(mi, ma), bins=NB)
    xaxeshisto_h2 = binedges_h2[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    (counts_h1, binedges_h1) = np.histogram(H1mass, range=(mi, ma), bins=NB)
    xaxeshisto_h1 = binedges_h1[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    # additionally calculate red
    w = np.where(sSFR < sSFRcut)[0]
    massRED = mass[w]
    (countsRED, binedges) = np.histogram(massRED, range=(mi, ma), bins=NB)

    # additionally calculate blue
    w = np.where(sSFR > sSFRcut)[0]
    massBLU = mass[w]
    (countsBLU, binedges) = np.histogram(massBLU, range=(mi, ma), bins=NB)

    # Baldry+ 2008 modified data used for the MCMC fitting
    Zwaan = np.array([[6.933,   -0.333],
        [7.057,   -0.490],
        [7.209,   -0.698],
        [7.365,   -0.667],
        [7.528,   -0.823],
        [7.647,   -0.958],
        [7.809,   -0.917],
        [7.971,   -0.948],
        [8.112,   -0.927],
        [8.263,   -0.917],
        [8.404,   -1.062],
        [8.566,   -1.177],
        [8.707,   -1.177],
        [8.853,   -1.312],
        [9.010,   -1.344],
        [9.161,   -1.448],
        [9.302,   -1.604],
        [9.448,   -1.792],
        [9.599,   -2.021],
        [9.740,   -2.406],
        [9.897,   -2.615],
        [10.053,  -3.031],
        [10.178,  -3.677],
        [10.335,  -4.448],
        [10.492,  -5.083]        ], dtype=np.float32)
    
    ObrRaw = np.array([
        [7.300,   -1.104],
        [7.576,   -1.302],
        [7.847,   -1.250],
        [8.133,   -1.240],
        [8.409,   -1.344],
        [8.691,   -1.479],
        [8.956,   -1.792],
        [9.231,   -2.271],
        [9.507,   -3.198],
        [9.788,   -5.062 ]        ], dtype=np.float32)
    ObrCold = np.array([
        [8.009,   -1.042],
        [8.215,   -1.156],
        [8.409,   -0.990],
        [8.604,   -1.156],
        [8.799,   -1.208],
        [9.020,   -1.333],
        [9.194,   -1.385],
        [9.404,   -1.552],
        [9.599,   -1.677],
        [9.788,   -1.812],
        [9.999,   -2.312],
        [10.172,  -2.656],
        [10.362,  -3.500],
        [10.551,  -3.635],
        [10.740,  -5.010]        ], dtype=np.float32)
    
    ObrCold_xval = np.log10(10**(ObrCold[:, 0])  /Hubble_h/Hubble_h)
    ObrCold_yval = (10**(ObrCold[:, 1]) * Hubble_h*Hubble_h*Hubble_h)
    Zwaan_xval = np.log10(10**(Zwaan[:, 0]) /Hubble_h/Hubble_h)
    Zwaan_yval = (10**(Zwaan[:, 1]) * Hubble_h*Hubble_h*Hubble_h)
    ObrRaw_xval = np.log10(10**(ObrRaw[:, 0])  /Hubble_h/Hubble_h)
    ObrRaw_yval = (10**(ObrRaw[:, 1]) * Hubble_h*Hubble_h*Hubble_h)

    plt.plot(ObrCold_xval, ObrCold_yval, color='black', lw = 7, alpha=0.25, label='Obr. & Raw. 2009 (Cold Gas)')
    plt.plot(Zwaan_xval, Zwaan_yval, color='cyan', lw = 7, alpha=0.25, label='Zwaan et al. 2005 (HI)')
    plt.plot(ObrRaw_xval, ObrRaw_yval, color='magenta', lw = 7, alpha=0.25, label='Obr. & Raw. 2009 (H2)')

    plt.plot(xaxeshisto_h2, counts_h2 / volume / binwidth, 'magenta', linestyle='-', label='Model - H2 Gas')
    plt.plot(xaxeshisto_h1, counts_h1 / volume / binwidth, 'cyan', linestyle='-', label='Model - HI Gas')
    
    # Overplot the model histograms
    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model - Cold Gas')

    plt.yscale('log')
    plt.axis([8.0, 11.5, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{X}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '3.GasMassFunction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the baryonic TF relationship')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (StellarMass + ColdGas > 0.0) & 
      (BulgeMass / StellarMass > 0.1) & (BulgeMass / StellarMass < 0.5))[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    mass = np.log10( (StellarMass[w] + ColdGas[w]) )
    vel = np.log10(Vmax[w])
                
    plt.scatter(vel, mass, marker='x', s=50, c='k', alpha=0.3, label='Model Sb/c galaxies')
            
    # overplot Stark, McGaugh & Swatters 2009 (assumes h=0.75? ... what IMF?)
    w = np.arange(0.5, 10.0, 0.5)
    TF = 3.94*w + 1.79
    TF_upper = TF + 0.26
    TF_lower = TF - 0.26

    # plt.plot(w, TF, 'b-', alpha=0.5, label='Stark, McGaugh & Swatters 2009')
    plt.fill_between(w, TF_lower, TF_upper, color='blue', alpha=0.2)

        
    plt.ylabel(r'$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$')  # Set the y...
    plt.xlabel(r'$\log_{10}V_{max}\ (km/s)$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([1.4, 2.9, 7.5, 12.0])
        
    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '4.BaryonicTullyFisher' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the specific sSFR')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where(StellarMass > 0.01)[0]
    if(len(w) > dilute): w = sample(list(w), dilute)
    mass = np.log10(StellarMass[w])
    sSFR = np.log10( (SfrDisk[w] + SfrBulge[w]) / StellarMass[w] )
    plt.scatter(mass, sSFR, marker='o', s=1, c='k', alpha=0.5, label='Model galaxies')

    # overplot dividing line between SF and passive
    w = np.arange(7.0, 13.0, 1.0)
    plt.plot(w, w/w*sSFRcut, 'b:', lw=2.0)

    plt.ylabel(r'$\log_{10}\ s\mathrm{SFR}\ (\mathrm{yr^{-1}})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.axis([8.0, 12.0, -16.0, -8.0])

    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '5.SpecificStarFormationRate' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the gas fractions')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (StellarMass + ColdGas > 0.0) & 
      (BulgeMass / StellarMass > 0.1) & (BulgeMass / StellarMass < 0.5))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)
    
    mass = np.log10(StellarMass[w])
    fraction = ColdGas[w] / (StellarMass[w] + ColdGas[w])

    plt.scatter(mass, fraction, marker='o', s=1, c='k', alpha=0.5, label='Model Sb/c galaxies')
        
    plt.ylabel(r'$\mathrm{Cold\ Mass\ /\ (Cold+Stellar\ Mass)}$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([8.0, 12.0, 0.0, 1.0])
        
    leg = plt.legend(loc='upper right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '6.GasFraction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the metallicities')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (ColdGas / (StellarMass + ColdGas) > 0.1) & (StellarMass > 1.0e8))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)
    
    mass = np.log10(StellarMass[w])
    Z = np.log10((MetalsColdGas[w] / ColdGas[w]) / 0.02) + 9.0
    
    plt.scatter(mass, Z, marker='o', s=1, c='k', alpha=0.5, label='Model galaxies')
        
    # overplot Tremonti et al. 2003 (h=0.7)
    w = np.arange(7.0, 11.5, 0.1)
    Zobs = -1.492 + 1.847*w - 0.08026*w*w
    if(whichimf == 0):
        # Conversion from Kroupa IMF to Slapeter IMF
        # plt.plot(np.log10((10**w *1.5)), Zobs, 'b-', lw=2.0, label='Tremonti et al. 2003')
        plt.fill_between(np.log10((10**w *1.5)), Zobs+0.1, Zobs-0.1, color='blue', alpha=0.2)
    elif(whichimf == 1):
        # Conversion from Kroupa IMF to Slapeter IMF to Chabrier IMF
        # plt.plot(np.log10((10**w *1.5 /1.8)), Zobs, 'b-', lw=2.0, label='Tremonti et al. 2003')
        plt.fill_between(np.log10((10**w *1.5 /1.8)), Zobs+0.1, Zobs-0.1, color='blue', alpha=0.2)
        
    plt.ylabel(r'$12\ +\ \log_{10}[\mathrm{O/H}]$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([8.0, 12.0, 8.0, 9.5])
        
    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '7.Metallicity' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the black hole-bulge relationship')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((BulgeMass > 1.0e8) & (BlackHoleMass > 01.0e6))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    bh = np.log10(BlackHoleMass[w])
    bulge = np.log10(BulgeMass[w])
                
    plt.scatter(bulge, bh, marker='o', s=1, c='k', alpha=0.5, label='Model galaxies')
            
    # overplot Haring & Rix 2004
    w = 10. ** np.arange(20)
    BHdata = 10. ** (8.2 + 1.12 * np.log10(w / 1.0e11))
    plt.plot(np.log10(w), np.log10(BHdata), 'b-', label="Haring \& Rix 2004")

    # Observational points
    M_BH_obs = (0.7/Hubble_h)**2*1e8*np.array([39, 11, 0.45, 25, 24, 0.044, 1.4, 0.73, 9.0, 58, 0.10, 8.3, 0.39, 0.42, 0.084, 0.66, 0.73, 15, 4.7, 0.083, 0.14, 0.15, 0.4, 0.12, 1.7, 0.024, 8.8, 0.14, 2.0, 0.073, 0.77, 4.0, 0.17, 0.34, 2.4, 0.058, 3.1, 1.3, 2.0, 97, 8.1, 1.8, 0.65, 0.39, 5.0, 3.3, 4.5, 0.075, 0.68, 1.2, 0.13, 4.7, 0.59, 6.4, 0.79, 3.9, 47, 1.8, 0.06, 0.016, 210, 0.014, 7.4, 1.6, 6.8, 2.6, 11, 37, 5.9, 0.31, 0.10, 3.7, 0.55, 13, 0.11])
    M_BH_hi = (0.7/Hubble_h)**2*1e8*np.array([4, 2, 0.17, 7, 10, 0.044, 0.9, 0.0, 0.9, 3.5, 0.10, 2.7, 0.26, 0.04, 0.003, 0.03, 0.69, 2, 0.6, 0.004, 0.02, 0.09, 0.04, 0.005, 0.2, 0.024, 10, 0.1, 0.5, 0.015, 0.04, 1.0, 0.01, 0.02, 0.3, 0.008, 1.4, 0.5, 1.1, 30, 2.0, 0.6, 0.07, 0.01, 1.0, 0.9, 2.3, 0.002, 0.13, 0.4, 0.08, 0.5, 0.03, 0.4, 0.38, 0.4, 10, 0.2, 0.014, 0.004, 160, 0.014, 4.7, 0.3, 0.7, 0.4, 1, 18, 2.0, 0.004, 0.001, 2.6, 0.26, 5, 0.005])
    M_BH_lo = (0.7/Hubble_h)**2*1e8*np.array([5, 2, 0.10, 7, 10, 0.022, 0.3, 0.0, 0.8, 3.5, 0.05, 1.3, 0.09, 0.04, 0.003, 0.03, 0.35, 2, 0.6, 0.004, 0.13, 0.1, 0.05, 0.005, 0.2, 0.012, 2.7, 0.06, 0.5, 0.015, 0.06, 1.0, 0.02, 0.02, 0.3, 0.008, 0.6, 0.5, 0.6, 26, 1.9, 0.3, 0.07, 0.01, 1.0, 2.5, 1.5, 0.002, 0.13, 0.9, 0.08, 0.5, 0.09, 0.4, 0.33, 0.4, 10, 0.1, 0.014, 0.004, 160, 0.007, 3.0, 0.4, 0.7, 1.5, 1, 11, 2.0, 0.004, 0.001, 1.5, 0.19, 4, 0.005])
    M_sph_obs = (0.7/Hubble_h)**2*1e10*np.array([69, 37, 1.4, 55, 27, 2.4, 0.46, 1.0, 19, 23, 0.61, 4.6, 11, 1.9, 4.5, 1.4, 0.66, 4.7, 26, 2.0, 0.39, 0.35, 0.30, 3.5, 6.7, 0.88, 1.9, 0.93, 1.24, 0.86, 2.0, 5.4, 1.2, 4.9, 2.0, 0.66, 5.1, 2.6, 3.2, 100, 1.4, 0.88, 1.3, 0.56, 29, 6.1, 0.65, 3.3, 2.0, 6.9, 1.4, 7.7, 0.9, 3.9, 1.8, 8.4, 27, 6.0, 0.43, 1.0, 122, 0.30, 29, 11, 20, 2.8, 24, 78, 96, 3.6, 2.6, 55, 1.4, 64, 1.2])
    M_sph_hi = (0.7/Hubble_h)**2*1e10*np.array([59, 32, 2.0, 80, 23, 3.5, 0.68, 1.5, 16, 19, 0.89, 6.6, 9, 2.7, 6.6, 2.1, 0.91, 6.9, 22, 2.9, 0.57, 0.52, 0.45, 5.1, 5.7, 1.28, 2.7, 1.37, 1.8, 1.26, 1.7, 4.7, 1.7, 7.1, 2.9, 0.97, 7.4, 3.8, 2.7, 86, 2.1, 1.30, 1.9, 0.82, 25, 5.2, 0.96, 4.9, 3.0, 5.9, 1.2, 6.6, 1.3, 5.7, 2.7, 7.2, 23, 5.2, 0.64, 1.5, 105, 0.45, 25, 10, 17, 2.4, 20, 67, 83, 5.2, 3.8, 48, 2.0, 55, 1.8])
    M_sph_lo = (0.7/Hubble_h)**2*1e10*np.array([32, 17, 0.8, 33, 12, 1.4, 0.28, 0.6, 9, 10, 0.39, 2.7, 5, 1.1, 2.7, 0.8, 0.40, 2.8, 12, 1.2, 0.23, 0.21, 0.18, 2.1, 3.1, 0.52, 1.1, 0.56, 0.7, 0.51, 0.9, 2.5, 0.7, 2.9, 1.2, 0.40, 3.0, 1.5, 1.5, 46, 0.9, 0.53, 0.8, 0.34, 13, 2.8, 0.39, 2.0, 1.2, 3.2, 0.6, 3.6, 0.5, 2.3, 1.1, 3.9, 12, 2.8, 0.26, 0.6, 57, 0.18, 13, 5, 9, 1.3, 11, 36, 44, 2.1, 1.5, 26, 0.8, 30, 0.7])
    core = np.array([1,1,0,1,1,0,0,0,1,1,0,1,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0,1,0,1,0,0,1,0,0,0,1,0,1,0,1,0,1,1,1,0,0,1,0,1,0])
    yerr2, yerr1 = np.log10((M_BH_obs+M_BH_hi)/M_BH_obs), -np.log10((M_BH_obs-M_BH_lo)/M_BH_obs)
    xerr2, xerr1 = np.log10((M_sph_obs+M_sph_hi)/M_sph_obs), -np.log10((M_sph_obs-M_sph_lo)/M_sph_obs)
    plt.errorbar(np.log10(M_sph_obs[core==0]), np.log10(M_BH_obs[core==0]), yerr=[yerr1[core==0],yerr2[core==0]], xerr=[xerr1[core==0],xerr2[core==0]], color='purple', alpha=0.3, label=r'S13 core', ls='none', lw=2, ms=0)
    plt.errorbar(np.log10(M_sph_obs[core==1]), np.log10(M_BH_obs[core==1]), yerr=[yerr1[core==1],yerr2[core==1]], xerr=[xerr1[core==1],xerr2[core==1]], color='c', alpha=0.3, label=r'S13 Sersic', ls='none', lw=2, ms=0)
    
    plt.ylabel(r'$\log\ M_{\mathrm{BH}}\ (M_{\odot})$')  # Set the y...
    plt.xlabel(r'$\log\ M_{\mathrm{bulge}}\ (M_{\odot})$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([8.0, 12.0, 6.0, 10.0])
        
    leg = plt.legend(loc='upper left')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '8.BlackHoleBulgeRelationship' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the quiescent fraction vs stellar mass')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    groupscale = 12.5
    
    w = np.where(StellarMass > 0.0)[0]
    stars = np.log10(StellarMass[w])
    halo = np.log10(CentralMvir[w])
    galtype = Type[w]
    sSFR = (SfrDisk[w] + SfrBulge[w]) / StellarMass[w]

    MinRange = 9.5
    MaxRange = 12.0
    Interval = 0.1
    Nbins = int((MaxRange-MinRange)/Interval)
    Range = np.arange(MinRange, MaxRange, Interval)
    
    Mass = []
    Fraction = []
    CentralFraction = []
    SatelliteFraction = []
    SatelliteFractionLo = []
    SatelliteFractionHi = []

    for i in range(Nbins-1):
        
        w = np.where((stars >= Range[i]) & (stars < Range[i+1]))[0]
        if len(w) > 0:
            wQ = np.where((stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut))[0]
            Fraction.append(1.0*len(wQ) / len(w))
        else:
            Fraction.append(0.0)
        
        w = np.where((galtype == 0) & (stars >= Range[i]) & (stars < Range[i+1]))[0]
        if len(w) > 0:
            wQ = np.where((galtype == 0) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut))[0]
            CentralFraction.append(1.0*len(wQ) / len(w))
        else:
            CentralFraction.append(0.0)
        
        w = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]))[0]
        if len(w) > 0:
            wQ = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut))[0]
            SatelliteFraction.append(1.0*len(wQ) / len(w))
            wQ = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut) & (halo < groupscale))[0]
            SatelliteFractionLo.append(1.0*len(wQ) / len(w))
            wQ = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut) & (halo > groupscale))[0]
            SatelliteFractionHi.append(1.0*len(wQ) / len(w))                
        else:
            SatelliteFraction.append(0.0)
            SatelliteFractionLo.append(0.0)
            SatelliteFractionHi.append(0.0)
            
        Mass.append((Range[i] + Range[i+1]) / 2.0)                
    
    Mass = np.array(Mass)
    Fraction = np.array(Fraction)
    CentralFraction = np.array(CentralFraction)
    SatelliteFraction = np.array(SatelliteFraction)
    SatelliteFractionLo = np.array(SatelliteFractionLo)
    SatelliteFractionHi = np.array(SatelliteFractionHi)
    
    w = np.where(Fraction > 0)[0]
    plt.plot(Mass[w], Fraction[w], label='All')

    w = np.where(CentralFraction > 0)[0]
    plt.plot(Mass[w], CentralFraction[w], color='Blue', label='Centrals')

    w = np.where(SatelliteFraction > 0)[0]
    plt.plot(Mass[w], SatelliteFraction[w], color='Red', label='Satellites')

    w = np.where(SatelliteFractionLo > 0)[0]
    plt.plot(Mass[w], SatelliteFractionLo[w], 'r--', label='Satellites-Lo')

    w = np.where(SatelliteFractionHi > 0)[0]
    plt.plot(Mass[w], SatelliteFractionHi[w], 'r-.', label='Satellites-Hi')
    
    plt.xlabel(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')  # Set the x-axis label
    plt.ylabel(r'$\mathrm{Quescient\ Fraction}$')  # Set the y-axis label
    
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
    
    plt.axis([9.5, 12.0, 0.0, 1.05])
    
    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
    
    outputFile = OutputDir + '9.QuiescentFraction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the mass fraction of galaxies')

    w = np.where(StellarMass > 0.0)[0]
    fBulge = BulgeMass[w] / StellarMass[w]
    fDisk = 1.0 - fBulge
    mass = np.log10(StellarMass[w])
    sSFR = np.log10((SfrDisk[w] + SfrBulge[w]) / StellarMass[w])
    
    binwidth = 0.2
    shift = binwidth/2.0
    mass_range = np.arange(8.5-shift, 12.0+shift, binwidth)
    bins = len(mass_range)
    
    fBulge_ave = np.zeros(bins)
    fBulge_var = np.zeros(bins)
    fDisk_ave = np.zeros(bins)
    fDisk_var = np.zeros(bins)
    
    for i in range(bins-1):
        w = np.where( (mass >= mass_range[i]) & (mass < mass_range[i+1]))[0]
        if(len(w) > 0):
            fBulge_ave[i] = np.mean(fBulge[w])
            fBulge_var[i] = np.var(fBulge[w])
            fDisk_ave[i] = np.mean(fDisk[w])
            fDisk_var[i] = np.var(fDisk[w])
    
    w = np.where(fBulge_ave > 0.0)[0]
    plt.plot(mass_range[w]+shift, fBulge_ave[w], 'r-', label='bulge')
    plt.fill_between(mass_range[w]+shift, 
        fBulge_ave[w]+fBulge_var[w], 
        fBulge_ave[w]-fBulge_var[w], 
        facecolor='red', alpha=0.25)
    
    w = np.where(fDisk_ave > 0.0)[0]
    plt.plot(mass_range[w]+shift, fDisk_ave[w], 'k-', label='disk stars')
    plt.fill_between(mass_range[w]+shift, 
        fDisk_ave[w]+fDisk_var[w], 
        fDisk_ave[w]-fDisk_var[w], 
        facecolor='black', alpha=0.25)
    
    plt.axis([mass_range[0], mass_range[bins-1], 0.0, 1.05])

    plt.ylabel(r'$\mathrm{Stellar\ Mass\ Fraction}$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
            t.set_fontsize('medium')
    
    outputFile = OutputDir + '10.BulgeMassFraction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the average baryon fraction vs halo mass (can take time)')

    # Find halos at log Mvir = 13.5-14.0
    mask = (np.log10(Mvir) > 13.5) & (np.log10(Mvir) < 14.0)

    total_baryons = (StellarMass[mask] + ColdGas[mask] + HotGas[mask] + CGMgas[mask] + IntraClusterStars[mask] + BlackHoleMass[mask] + EjectedMass[mask]) / (0.17 * Mvir[mask])
    print(f"Baryon closure at high mass: {np.mean(total_baryons):.3f}")
    print(f"Should be ~1.0. If < 0.95, baryons are leaking somewhere.")

    # Check component fractions
    print(f"Hot gas fraction: {np.mean(HotGas[mask] / (0.17 * Mvir[mask])):.3f}")
    print(f"Stellar fraction: {np.mean(StellarMass[mask] / (0.17 * Mvir[mask])):.3f}")
    print(f"CGM fraction: {np.mean(CGMgas[mask] / (0.17 * Mvir[mask])):.3f}  # Should be ~0")

    plt.figure()
    ax = plt.subplot(111)

    HaloMass = np.log10(Mvir)
    Baryons = StellarMass + ColdGas + HotGas + CGMgas + IntraClusterStars + BlackHoleMass + EjectedMass

    MinHalo, MaxHalo, Interval = 11.0, 16.0, 0.1
    HaloBins = np.arange(MinHalo, MaxHalo + Interval, Interval)
    Nbins = len(HaloBins) - 1

    MeanCentralHaloMass = []
    MeanBaryonFraction = []
    MeanBaryonFractionU = []
    MeanBaryonFractionL = []
    MeanStars = []
    MeanStarsU = []
    MeanStarsL = []
    MeanCold = []
    MeanColdU = []
    MeanColdL = []
    MeanHot = []
    MeanHotU = []
    MeanHotL = []
    MeanCGM = []
    MeanCGMU = []
    MeanCGML = []
    MeanICS = []
    MeanICSU = []
    MeanICSL = []
    MeanBH = []
    MeanBHU = []
    MeanBHL = []
    MeanEjected = []
    MeanEjectedU = []
    MeanEjectedL = []

    bin_indices = np.digitize(HaloMass, HaloBins) - 1

    # Pre-compute unique CentralGalaxyIndex for faster lookup
    halo_to_galaxies = defaultdict(list)
    for i, central_idx in enumerate(CentralGalaxyIndex):
        halo_to_galaxies[central_idx].append(i)

    for i in range(Nbins - 1):
        w1 = np.where((Type == 0) & (bin_indices == i))[0]
        HalosFound = len(w1)
        
        if HalosFound > 2:
            # Pre-allocate arrays for better performance
            BaryonFractions = np.zeros(HalosFound)
            StarsFractions = np.zeros(HalosFound)
            ColdFractions = np.zeros(HalosFound)
            HotFractions = np.zeros(HalosFound)
            CGMFractions = np.zeros(HalosFound)
            ICSFractions = np.zeros(HalosFound)
            BHFractions = np.zeros(HalosFound)
            EjectedFractions = np.zeros(HalosFound)
            
            # Vectorized calculation for each halo
            for idx, halo_idx in enumerate(w1):
                halo_galaxies = np.array(halo_to_galaxies[CentralGalaxyIndex[halo_idx]])
                halo_mvir = Mvir[halo_idx]
                
                # Use advanced indexing for faster summing
                BaryonFractions[idx] = np.sum(Baryons[halo_galaxies]) / halo_mvir
                StarsFractions[idx] = np.sum(StellarMass[halo_galaxies]) / halo_mvir
                ColdFractions[idx] = np.sum(ColdGas[halo_galaxies]) / halo_mvir
                HotFractions[idx] = np.sum(HotGas[halo_galaxies]) / halo_mvir
                CGMFractions[idx] = np.sum(CGMgas[halo_galaxies]) / halo_mvir
                ICSFractions[idx] = np.sum(IntraClusterStars[halo_galaxies]) / halo_mvir
                BHFractions[idx] = np.sum(BlackHoleMass[halo_galaxies]) / halo_mvir
                EjectedFractions[idx] = np.sum(EjectedMass[halo_galaxies]) / halo_mvir
            
            # Calculate statistics once for all arrays
            CentralHaloMass = np.log10(Mvir[w1])
            MeanCentralHaloMass.append(np.mean(CentralHaloMass))
            
            n_halos = len(BaryonFractions)
            sqrt_n = np.sqrt(n_halos)
            
            # Vectorized mean and std calculations
            means = [np.mean(arr) for arr in [BaryonFractions, StarsFractions, ColdFractions, 
                                             HotFractions, CGMFractions, ICSFractions, BHFractions, EjectedFractions]]
            stds = [np.std(arr) / sqrt_n for arr in [BaryonFractions, StarsFractions, ColdFractions, 
                                                    HotFractions, CGMFractions, ICSFractions, BHFractions, EjectedFractions]]
            
            # Append all means and bounds
            MeanBaryonFraction.append(means[0])
            MeanBaryonFractionU.append(means[0] + stds[0])
            MeanBaryonFractionL.append(means[0] - stds[0])
            
            MeanStars.append(means[1])
            MeanStarsU.append(means[1] + stds[1])
            MeanStarsL.append(means[1] - stds[1])
            
            MeanCold.append(means[2])
            MeanColdU.append(means[2] + stds[2])
            MeanColdL.append(means[2] - stds[2])
            
            MeanHot.append(means[3])
            MeanHotU.append(means[3] + stds[3])
            MeanHotL.append(means[3] - stds[3])
            
            MeanCGM.append(means[4])
            MeanCGMU.append(means[4] + stds[4])
            MeanCGML.append(means[4] - stds[4])
            
            MeanICS.append(means[5])
            MeanICSU.append(means[5] + stds[5])
            MeanICSL.append(means[5] - stds[5])
            
            MeanBH.append(means[6])
            MeanBHU.append(means[6] + stds[6])
            MeanBHL.append(means[6] - stds[6])

            MeanEjected.append(means[7])
            MeanEjectedU.append(means[7] + stds[7])
            MeanEjectedL.append(means[7] - stds[7])

    # Convert lists to arrays and ensure positive values for log scale
    MeanCentralHaloMass = np.array(MeanCentralHaloMass)
    MeanBaryonFraction = np.array(MeanBaryonFraction)
    MeanBaryonFractionU = np.array(MeanBaryonFractionU)
    MeanBaryonFractionL = np.maximum(np.array(MeanBaryonFractionL), 1e-6)  # Prevent negative values on log scale
    
    MeanStars = np.array(MeanStars)
    MeanStarsU = np.array(MeanStarsU)
    MeanStarsL = np.maximum(np.array(MeanStarsL), 1e-6)
    
    MeanCold = np.array(MeanCold)
    MeanColdU = np.array(MeanColdU)
    MeanColdL = np.maximum(np.array(MeanColdL), 1e-6)
    
    MeanHot = np.array(MeanHot)
    MeanHotU = np.array(MeanHotU)
    MeanHotL = np.maximum(np.array(MeanHotL), 1e-6)
    
    MeanCGM = np.array(MeanCGM)
    MeanCGMU = np.array(MeanCGMU)
    MeanCGML = np.maximum(np.array(MeanCGML), 1e-6)
    
    MeanICS = np.array(MeanICS)
    MeanICSU = np.array(MeanICSU)
    MeanICSL = np.maximum(np.array(MeanICSL), 1e-6)

    MeanBH = np.array(MeanBH)
    MeanBHU = np.array(MeanBHU)
    MeanBHL = np.maximum(np.array(MeanBHL), 1e-6)

    MeanEjected = np.array(MeanEjected)
    MeanEjectedU = np.array(MeanEjectedU)
    MeanEjectedL = np.maximum(np.array(MeanEjectedL), 1e-6)

    baryon_frac = 0.17
    plt.axhline(y=baryon_frac, color='grey', linestyle='--', linewidth=1.0, 
            label='Baryon Fraction = {:.2f}'.format(baryon_frac))

    # Add 1-sigma shading for each mass reservoir
    plt.fill_between(MeanCentralHaloMass, MeanBaryonFractionL, MeanBaryonFractionU, 
                     color='black', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanStarsL, MeanStarsU, 
                     color='purple', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanColdL, MeanColdU, 
                     color='blue', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanHotL, MeanHotU, 
                     color='red', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanCGML, MeanCGMU, 
                     color='green', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanICSL, MeanICSU, 
                     color='orange', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanEjectedL, MeanEjectedU, 
                     color='yellow', alpha=0.2)

    plt.plot(MeanCentralHaloMass, MeanBaryonFraction, 'k-', label='Total')
    plt.plot(MeanCentralHaloMass, MeanStars, label='Stars', color='purple', linestyle='--')
    plt.plot(MeanCentralHaloMass, MeanCold, label='Cold gas', color='blue', linestyle=':')
    plt.plot(MeanCentralHaloMass, MeanHot, label='Hot gas', color='red')
    plt.plot(MeanCentralHaloMass, MeanCGM, label='Circumgalactic Medium', color='green', linestyle='-.')
    plt.plot(MeanCentralHaloMass, MeanICS, label='Intracluster Stars', color='orange', linestyle='-.')
    plt.plot(MeanCentralHaloMass, MeanEjected, label='Ejected gas', color='yellow', linestyle='--')

    #plt.yscale('log')

    plt.xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} \mathrm{Baryon\ Fraction}$')
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
    plt.axis([11.1, 15.0, 0.0, 0.2])

    leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1, bbox_to_anchor=(1.0, 0.5))
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    outputFile = OutputDir + '11.BaryonFraction' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()


# -------------------------------------------------------

    print('Plotting the mass in stellar, cold, hot, ejected, ICS reservoirs')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (Mvir > 1.0e10) & (StellarMass > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    HaloMass = np.log10(Mvir[w])
    plt.scatter(HaloMass, np.log10(StellarMass[w]), marker='o', s=0.3, c='k', alpha=0.5, label='Stars')
    plt.scatter(HaloMass, np.log10(ColdGas[w]), marker='o', s=0.3, color='blue', alpha=0.5, label='Cold gas')
    plt.scatter(HaloMass, np.log10(HotGas[w]), marker='o', s=0.3, color='red', alpha=0.5, label='Hot gas')
    plt.scatter(HaloMass, np.log10(EjectedMass[w]), marker='o', s=0.3, color='green', alpha=0.5, label='Ejected gas')
    plt.scatter(HaloMass, np.log10(IntraClusterStars[w]), marker='o', s=10, color='yellow', alpha=0.5, label='Intracluster stars')
    plt.scatter(HaloMass, np.log10(CGMgas[w]), marker='o', s=10, color='orange', alpha=0.5, label='CGM gas')

    plt.ylabel(r'$\mathrm{stellar,\ cold,\ hot,\ ejected,\ CGM,\ ICS\ mass}$')  # Set the y...
    plt.xlabel(r'$\log\ M_{\mathrm{vir}}\ (h^{-1}\ M_{\odot})$')  # and the x-axis labels
    
    plt.axis([10.0, 15.0, 7.5, 14.0])

    leg = plt.legend(loc='upper left')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '12.MassReservoirScatter' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the spatial distribution of all galaxies')

    plt.figure()  # New figure

    w = np.where((Mvir > 0.0) & (StellarMass > 1.0e9))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]
    zz = Posz[w]

    buff = BoxSize*0.1

    ax = plt.subplot(221)  # 1 plot on the figure
    plt.scatter(xx, yy, marker='o', s=0.3, c='k', alpha=0.5)
    plt.axis([0.0-buff, BoxSize+buff, 0.0-buff, BoxSize+buff])
    plt.ylabel(r'$\mathrm{x}$')  # Set the y...
    plt.xlabel(r'$\mathrm{y}$')  # and the x-axis labels
    
    ax = plt.subplot(222)  # 1 plot on the figure
    plt.scatter(xx, zz, marker='o', s=0.3, c='k', alpha=0.5)
    plt.axis([0.0-buff, BoxSize+buff, 0.0-buff, BoxSize+buff])
    plt.ylabel(r'$\mathrm{x}$')  # Set the y...
    plt.xlabel(r'$\mathrm{z}$')  # and the x-axis labels
    
    ax = plt.subplot(223)  # 1 plot on the figure
    plt.scatter(yy, zz, marker='o', s=0.3, c='k', alpha=0.5)
    plt.axis([0.0-buff, BoxSize+buff, 0.0-buff, BoxSize+buff])
    plt.ylabel(r'$\mathrm{y}$')  # Set the y...
    plt.xlabel(r'$\mathrm{z}$')  # and the x-axis labels
        
    outputFile = OutputDir + '13.SpatialDistribution' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    print('Plotting the SFR')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w2 = np.where(StellarMass > 0.01)[0]
    if(len(w2) > dilute): w2 = sample(list(range(len(w2))), dilute)
    mass = np.log10(StellarMass[w2])
    starformationrate =  (SfrDisk[w2] + SfrBulge[w2])

    # Create scatter plot with metallicity coloring
    plt.scatter(mass, np.log10(starformationrate), c='b', marker='o', s=1, alpha=0.7)

    plt.ylabel(r'$\log_{10} \mathrm{SFR}\ (M_{\odot}\ \mathrm{yr^{-1}})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.xlim(6.0, 12.2)
    plt.ylim(-5, 3)  # Set y-axis limits for SFR

    outputFile = OutputDir + '14.StarFormationRate' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting H2 surface density vs SFR surface density')

    plt.figure()  # New figure
    # Σ_H2 in M_sun/pc^2, Σ_SFR in M_sun/yr/kpc^2

    sfrdot = SfrDisk + SfrBulge
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    H2Gas = read_hdf(snap_num = Snapshot, param = 'H2gas') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    DiskRadius = read_hdf(snap_num = Snapshot, param = 'DiskRadius')  # in Mpc/h

    w = np.where((StellarMass > 0.0) & (H2Gas > 0.0) & (sfrdot > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    disk_radius = DiskRadius[w] * 1.0e6 / Hubble_h
    disk_area = 2 * np.pi * disk_radius**2

    sigma_H2 = H2Gas[w] / disk_area # DiskRadius in kpc, area in pc^2
    sigma_SFR = sfrdot[w] / disk_area * 1.0e6 # area in kpc^2
    log10_sigma_H2 = np.log10(sigma_H2)
    log10_sigma_SFR = np.log10(sigma_SFR)
    # Color by Mvir (virial mass)
    sc = plt.scatter(log10_sigma_H2, log10_sigma_SFR, c=np.log10(StellarMass[w]), cmap='plasma',
                      alpha=0.6, s=5, vmin=8, vmax=12, label='SAGE25')
    cb = plt.colorbar(sc)

    cb.set_label(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    # Add canonical Kennicutt-Schmidt law (Kennicutt 1998): log(Sigma_SFR) = 1.4*log(Sigma_gas) - 3.6
    sigma_gas_range = np.linspace(-4, 4, 100)
    ks_law = 1.4 * sigma_gas_range - 3.6
    plt.plot(sigma_gas_range, ks_law, linestyle='--', color='red', label='Kennicutt (1998)')

    gas_range = np.logspace(-4, 4, 100)
        
    # Bigiel et al. (2008) - resolved regions in nearby galaxies
    # Σ_SFR = 1.6e-3 × (Σ_H2)^1.0
    ks_bigiel = np.log10(1.6e-3) + 1.0 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_bigiel, linestyle=':', color='red', linewidth=2.5, alpha=0.8, 
            label='Bigiel+ (2008) - resolved', zorder=2)
    
    # Schruba et al. (2011) - different normalization
    # Σ_SFR = 2.1e-3 × (Σ_H2)^1.0
    ks_schruba = np.log10(2.1e-3) + 1.0 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_schruba, linestyle='--', color='red', linewidth=2, alpha=0.6, 
            label='Schruba+ (2011)', zorder=2)
            
    # Leroy et al. (2013) - whole galaxy integrated
    # Σ_SFR = 1.4e-3 × (Σ_H2)^1.1
    ks_leroy = np.log10(1.4e-3) + 1.1 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_leroy, linestyle='-', color='red', linewidth=2, alpha=0.7, 
            label='Leroy+ (2013) - galaxies', zorder=2)
            
    # Saintonge et al. (2011) - COLD GASS survey
    # Σ_SFR = 1.0e-3 × (Σ_H2)^0.96
    ks_saintonge = np.log10(1.0e-3) + 0.96 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_saintonge, linestyle='-.', color='red', linewidth=1.5, alpha=0.5, 
            label='Saintonge+ (2011)', zorder=2)
    
    plt.xlabel(r'$\log_{10} \Sigma_{\mathrm{H}_2}\ (M_{\odot}/\mathrm{pc}^2)$')
    plt.ylabel(r'$\log_{10} \Sigma_{\mathrm{SFR}}\ (M_{\odot}/yr/\mathrm{kpc}^2)$')
    # # plt.title('H$_2$ Surface Density vs SFR Surface Density (K-S Law)')
    plt.legend(loc='lower right', fontsize='small', frameon=False)
    plt.xlim(-3, 4)
    plt.ylim(-6, 1)
    # plt.grid(True, alpha=0.3)
    plt.tight_layout()
    outputFile = OutputDir + '15.h2_vs_sfr_surface_density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()


    # -------------------------------------------------------

    print('Plotting Size-Mass relation split by star-forming and quiescent')

    plt.figure()  # New figure

    w = np.where((Mvir > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_disk_radius = np.log10(DiskRadius[w] / 0.001)
    log10_disk_radius_quiescent = np.log10(DiskRadius[w] / 0.001 / 1.67)
    SFR = SfrDisk[w] + SfrBulge[w]
    sSFR = np.full_like(SFR, -99.0)
    mask = (StellarMass[w] > 0)
    sSFR[mask] = np.log10(SFR[mask] / StellarMass[w][mask])

    star_forming = sSFR > sSFRcut
    quiescent = sSFR <= sSFRcut


    plt.scatter(log10_stellar_mass[star_forming], log10_disk_radius[star_forming], c='darkblue', s=5, alpha=0.1)
    plt.scatter(log10_stellar_mass[quiescent], log10_disk_radius_quiescent[quiescent], c='darkred', s=5, alpha=0.1)

    # Add median lines for both populations

    def median_and_sigma(x, y, bins):
        bin_centers = []
        medians = []
        sig_low = []
        sig_high = []
        for i in range(len(bins)-1):
            mask = (x >= bins[i]) & (x < bins[i+1])
            if np.any(mask):
                bin_centers.append(0.5*(bins[i]+bins[i+1]))
                medians.append(np.median(y[mask]))
                sig_low.append(np.percentile(y[mask], 16))
                sig_high.append(np.percentile(y[mask], 84))
        return (np.array(bin_centers), np.array(medians), np.array(sig_low), np.array(sig_high))

    bins = np.arange(6, 12.1, 0.3)

    # Star-forming median and 1-sigma
    x_sf, y_sf, y_sf_low, y_sf_high = median_and_sigma(log10_stellar_mass[star_forming], log10_disk_radius[star_forming], bins)
    plt.plot(x_sf, y_sf, c='darkblue', lw=2.5, label='Median SF')
    plt.fill_between(x_sf, y_sf_low, y_sf_high, color='darkblue', alpha=0.18, label='SF 1$\sigma$')
    # Quiescent median and 1-sigma
    x_q, y_q, y_q_low, y_q_high = median_and_sigma(log10_stellar_mass[quiescent], log10_disk_radius_quiescent[quiescent], bins)
    plt.plot(x_q, y_q, c='darkred', lw=2.5, label='Median Q')
    plt.fill_between(x_q, y_q_low, y_q_high, color='darkred', alpha=0.18, label='Q 1$\sigma$')

    # Approximate Shen+2003 relation
    M_star = np.logspace(6, 12, 100)
    R_shen_sf = 3.0 * (M_star/1e10)**0.14  # Star-forming
    R_shen_q = 1.5 * (M_star/1e10)**0.12   # Quiescent (smaller, shallower)

    plt.plot(np.log10(M_star), np.log10(R_shen_sf), 'b-', linewidth=2, label='Shen+03 SF')
    plt.plot(np.log10(M_star), np.log10(R_shen_q), 'r-', linewidth=2, label='Shen+03 Q')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$')
    # # plt.title('Size-Mass Relation: Star Forming (blue) vs Quiescent (red)')
    plt.legend(loc='upper left', fontsize='small', frameon=False)
    plt.xlim(6, 12)
    plt.ylim(-1, 2.5)

    outputFile = OutputDir + '16.size_mass_relation_split' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting CGM gas fraction vs stellar mass')

    plt.figure()

    w = np.where((Mvir > 0.0) & (StellarMass > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])

    # Calculate total hot-type gas and CGM fraction
    total_hot_gas = EjectedMass[w] + HotGas[w]
    # Avoid division by zero
    mask = total_hot_gas > 0
    f_CGM = np.zeros_like(total_hot_gas)
    f_CGM[mask] = EjectedMass[w][mask] / total_hot_gas[mask]

    # Only plot where there's actually gas
    valid = mask & (f_CGM >= 0) & (f_CGM <= 1)

    plt.scatter(log10_stellar_mass[valid], f_CGM[valid], c='purple', s=5, alpha=0.6)

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$f_{\mathrm{CGM}} = M_{\mathrm{CGM}}/(M_{\mathrm{CGM}} + M_{\mathrm{hot}})$')
    plt.xlim(8, 12)
    plt.ylim(0, 1)

    outputFile = OutputDir + '17.cgm_gas_fraction' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

    # -------------------------------------------------------

    print('Plotting Black Hole Mass vs Stellar Mass')

    # In your plotting script
    plt.figure()
    w = np.where((Mvir > 0.0) & (StellarMass > 0.0) & (BlackHoleMass > 0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_BH_mass = np.log10(BlackHoleMass[w])

    # Calculate total hot-type gas and CGM fraction
    total_hot_gas = EjectedMass[w] + HotGas[w]
    # Avoid division by zero
    mask = total_hot_gas > 0
    f_CGM = np.zeros_like(total_hot_gas)
    f_CGM[mask] = EjectedMass[w][mask] / total_hot_gas[mask]

    # Only plot where there's actually gas
    valid = mask & (f_CGM >= 0) & (f_CGM <= 1)

    plt.scatter(log10_stellar_mass[valid], log10_BH_mass[valid], c=f_CGM[valid], s=5, cmap='plasma')
    plt.colorbar(label='f_CGM')
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{BH}}\ (M_{\odot})$')

    plt.xlim(8, 12)
    plt.ylim(4, 10)

    outputFile = OutputDir + '18.BH_mass_vs_stellar_mass' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

        # -------------------------------------------------------

    print('Plotting CGM vs Stellar Mass')

    plt.figure()
    w = np.where((StellarMass > 0.0) & (CGMgas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_CGM_mass = np.log10(CGMgas[w])
    tvir = np.log10(35.9 * Vvir[w]**2)  # in Kelvin

    data = """10.06276150627615, 10.48936170212766
        10.112970711297072, 10.510638297872342
        10.175732217573222, 10.531914893617023
        10.242677824267782, 10.574468085106384
        10.322175732217573, 10.617021276595747
        10.401673640167363, 10.680851063829788
        10.481171548117155, 10.702127659574469
        10.560669456066945, 10.74468085106383
        10.644351464435147, 10.765957446808512
        10.719665271966527, 10.829787234042554
        10.786610878661088, 10.872340425531917
        10.866108786610878, 10.914893617021278
        10.94560669456067, 10.97872340425532
        11.02510460251046, 11.085106382978724
        11.108786610878662, 11.127659574468087
        11.196652719665272, 11.297872340425533
        11.276150627615063, 11.425531914893618
        11.359832635983263, 11.574468085106384
        11.426778242677823, 11.765957446808512
        11.497907949790795, 11.936170212765958
        11.581589958158995, 12.106382978723406
        11.652719665271967, 12.255319148936172
        11.728033472803347, 12.340425531914896
        11.782426778242678, 12.425531914893618
        11.832635983263598, 12.468085106382981
        11.870292887029288, 12.638297872340427
        11.912133891213388, 12.787234042553193
        11.94979079497908, 12.893617021276597
        12, 12.829787234042556
        12.05020920502092, 12.808510638297875
        12.09623430962343, 12.872340425531917
        12.138075313807532, 12.95744680851064
        12.184100418410042, 13.085106382978726"""

    # Split the data into lines and extract x, y coordinates
    lines = data.strip().split('\n')
    x = []
    y = []

    for line in lines:
        coords = line.split(', ')
        x.append(float(coords[0]))
        y.append(float(coords[1]))

    # Convert to numpy arrays (optional, but often useful for plotting)
    tng_x = np.array(x)
    tng_y = np.array(y)

    plt.scatter(log10_stellar_mass, log10_CGM_mass, c=Vvir[w], cmap='seismic', s=5)
    plt.plot(tng_x, tng_y, 'k--', lw=2, label='TNG-Cluster')
    plt.colorbar(label=r'$V_{\mathrm{vir}}\ (\mathrm{km/s})$')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')

    plt.xlim(8, 12)
    plt.ylim(6, 12)

    outputFile = OutputDir + '19.CGM_mass_vs_stellar_mass_temperature' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

        # -------------------------------------------------------

    print('Plotting CGM vs Stellar Mass')

    plt.figure()
    w = np.where((StellarMass > 0.0) & (CGMgas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_CGM_mass = np.log10(CGMgas[w])
    Z = np.log10((MetalsCGMgas[w] / CGMgas[w]) / 0.02) + 9.0

    data = """10.06276150627615, 10.48936170212766
        10.112970711297072, 10.510638297872342
        10.175732217573222, 10.531914893617023
        10.242677824267782, 10.574468085106384
        10.322175732217573, 10.617021276595747
        10.401673640167363, 10.680851063829788
        10.481171548117155, 10.702127659574469
        10.560669456066945, 10.74468085106383
        10.644351464435147, 10.765957446808512
        10.719665271966527, 10.829787234042554
        10.786610878661088, 10.872340425531917
        10.866108786610878, 10.914893617021278
        10.94560669456067, 10.97872340425532
        11.02510460251046, 11.085106382978724
        11.108786610878662, 11.127659574468087
        11.196652719665272, 11.297872340425533
        11.276150627615063, 11.425531914893618
        11.359832635983263, 11.574468085106384
        11.426778242677823, 11.765957446808512
        11.497907949790795, 11.936170212765958
        11.581589958158995, 12.106382978723406
        11.652719665271967, 12.255319148936172
        11.728033472803347, 12.340425531914896
        11.782426778242678, 12.425531914893618
        11.832635983263598, 12.468085106382981
        11.870292887029288, 12.638297872340427
        11.912133891213388, 12.787234042553193
        11.94979079497908, 12.893617021276597
        12, 12.829787234042556
        12.05020920502092, 12.808510638297875
        12.09623430962343, 12.872340425531917
        12.138075313807532, 12.95744680851064
        12.184100418410042, 13.085106382978726"""

    # Split the data into lines and extract x, y coordinates
    lines = data.strip().split('\n')
    x = []
    y = []

    for line in lines:
        coords = line.split(', ')
        x.append(float(coords[0]))
        y.append(float(coords[1]))

    # Convert to numpy arrays (optional, but often useful for plotting)
    tng_x = np.array(x)
    tng_y = np.array(y)

    plt.scatter(log10_stellar_mass, log10_CGM_mass, c=Z, cmap='plasma', s=5, vmin=7, vmax=9)
    plt.plot(tng_x, tng_y, 'k--', lw=2, label='TNG-Cluster')
    plt.colorbar(label=r'$12\ +\ \log_{10}[\mathrm{O/H}]$')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')

    plt.xlim(8, 12)
    plt.ylim(6, 12)

    outputFile = OutputDir + '20.CGM_mass_vs_stellar_mass_metallicity' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

    # -------------------------------------------------------

    print('Plotting Ejected vs Stellar Mass')

    plt.figure()
    w = np.where((StellarMass > 0.0) & (EjectedMass > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_CGM_mass = np.log10(EjectedMass[w])
    tvir = np.log10(35.9 * Vvir[w]**2)  # in Kelvin

    plt.scatter(log10_stellar_mass, log10_CGM_mass, c=Vvir[w], cmap='seismic', s=5)
    plt.colorbar(label=r'$V_{\mathrm{vir}}\ (\mathrm{km/s})$')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{ejected}}\ (M_{\odot})$')

    plt.xlim(8, 12)
    plt.ylim(6, 12)

    outputFile = OutputDir + '19.Ejected_mass_vs_stellar_mass_temperature' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

    # -------------------------------------------------------

    print('Plotting Ejected vs Stellar Mass')

    plt.figure()
    w = np.where((StellarMass > 0.0) & (EjectedMass > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_CGM_mass = np.log10(CGMgas[w])
    Z = np.log10((MetalsCGMgas[w] / CGMgas[w]) / 0.02) + 9.0

    plt.scatter(log10_stellar_mass, log10_CGM_mass, c=Z, cmap='plasma', s=5, vmin=7, vmax=9)
    plt.colorbar(label=r'$12\ +\ \log_{10}[\mathrm{O/H}]$')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{ejected}}\ (M_{\odot})$')

    plt.xlim(8, 12)
    plt.ylim(6, 12)

    outputFile = OutputDir + '20.Ejected_mass_vs_stellar_mass_metallicity' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

    # -------------------------------------------------------

    print('Plotting Hot gas vs Stellar Mass')

    plt.figure()
    w = np.where((StellarMass > 0.0) & (HotGas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_CGM_mass = np.log10(HotGas[w])
    Z = np.log10((MetalsHotGas[w] / HotGas[w]) / 0.02) + 9.0

    plt.scatter(log10_stellar_mass, log10_CGM_mass, c=Z, cmap='plasma', s=5, vmin=7, vmax=9)
    plt.colorbar(label=r'$12\ +\ \log_{10}[\mathrm{O/H}]$')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{hot}}\ (M_{\odot})$')

    plt.xlim(8, 12)
    plt.ylim(6, 12)

    outputFile = OutputDir + '20.Hot_gas_vs_stellar_mass_metallicity' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

    # -------------------------------------------------------

    print('Plotting ICS vs Hot Gas Mass')

    plt.figure()
    w = np.where((IntraClusterStars > 0.0) & (CGMgas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(CGMgas[w] + HotGas[w])
    log10_CGM_mass = np.log10(IntraClusterStars[w])
    Z = np.log10((MetalsCGMgas[w] + MetalsHotGas[w]) / (CGMgas[w] + HotGas[w]) / 0.02) + 9.0

    plt.scatter(log10_stellar_mass, log10_CGM_mass, c=Z, cmap='plasma', s=5, vmin=7, vmax=9)
    plt.colorbar(label=r'$12\ +\ \log_{10}[\mathrm{O/H}]$')

    plt.xlabel(r'$\log_{10} M_{\mathrm{CGM\ +\ hot}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{ICS}}\ (M_{\odot})$')

    plt.xlim(8, 12)
    plt.ylim(6, 12)

    outputFile = OutputDir + '20.ICS_vs_hot_gas_mass_metallicity' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

    # -------------------------------------------------------

    print('Plotting outflow vs stellar mass')

    plt.figure()
    w = np.where((StellarMass > 0.0) & (OutflowRate > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    mass_loading = OutflowRate[w] / (SfrDisk[w] + SfrBulge[w])

    plt.scatter(Vvir[w], mass_loading, c='green', s=5, alpha=0.6)

    plt.xlabel(r'$V_{\mathrm{vir}}\ (\mathrm{km/s})$')
    plt.ylabel(r'$\eta = \dot{M}_{\mathrm{outflow}}/\mathrm{SFR}$')

    # Add vertical line at critical velocity
    plt.axvline(x=60, color='gray', linestyle=':', linewidth=2, alpha=0.7, 
                label='$V_{\\mathrm{crit}} = 60$ km/s')

    plt.xlim(min(Vvir[w]), 300)
    plt.ylim(0.01, max(mass_loading)*1.1)

    outputFile = OutputDir + '21.outflow_rate_vs_stellar_mass' + OutputFormat
    plt.savefig(outputFile)
    plt.close()

    # -------------------------------------------------------

    # Regime = read_hdf(snap_num = Snapshot, param = 'Regime')

    print('Regime fractions:')
    print('Cool regime:', np.mean(Regime == 0))
    print('Hot regime:', np.mean(Regime == 1))

    print('Plotting stellar mass vs halo mass colored by regime')

    plt.figure()

    w = np.where((Mvir > 0.0) & (StellarMass > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_halo_mass = np.log10(Mvir[w])
    log10_stellar_mass = np.log10(StellarMass[w])
    regime_values = Regime[w]
    
    # Separate the data by regime for different colors
    cgm_regime = (regime_values == 0)
    hot_regime = (regime_values == 1)
    
    # Plot each regime separately with different colors
    if np.any(cgm_regime):
        plt.scatter(log10_halo_mass[cgm_regime], log10_stellar_mass[cgm_regime], 
                   c='blue', s=5, alpha=0.6, label='CGM Regime')
    
    if np.any(hot_regime):
        plt.scatter(log10_halo_mass[hot_regime], log10_stellar_mass[hot_regime], 
                   c='red', s=5, alpha=0.6, label='Hot Regime')

    plt.xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.xlim(10, 15)
    plt.ylim(8, 12)
    
    # Add legend
    plt.legend(loc='upper left', frameon=False)

    outputFile = OutputDir + '22.stellar_vs_halo_mass_by_regime' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting specific SFR vs stellar mass')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    dilute = 100000

    w2 = np.where(StellarMass > 0.0)[0]
    if(len(w2) > dilute): w2 = sample(list(range(len(w2))), dilute)
    mass = np.log10(StellarMass[w2])
    starformationrate = (SfrDisk[w2] + SfrBulge[w2])
    sSFR = np.full_like(starformationrate, -99.0)
    mask = (StellarMass[w2] > 0)
    sSFR[mask] = np.log10(starformationrate[mask] / StellarMass[w2][mask])

    sSFRcut = -11.0
    print(f'sSFR cut at {sSFRcut} yr^-1')

    # Separate populations
    sf_mask = (sSFR > sSFRcut) & (sSFR > -99.0)  # Star-forming
    q_mask = (sSFR <= sSFRcut) & (sSFR > -99.0)  # Quiescent

    mass_sf = mass[sf_mask]
    sSFR_sf = sSFR[sf_mask]
    mass_q = mass[q_mask]
    sSFR_q = sSFR[q_mask]

    # Define grid for density calculation
    x_bins = np.linspace(8.0, 12.2, 100)
    y_bins = np.linspace(-13, -8, 100)

    def plot_density_contours(x, y, color, label, clip_above=None, clip_below=None):
        """Plot filled contours with 1, 2, 3 sigma levels"""
        if len(x) < 10:
            return
        
        # Create 2D histogram
        H, xedges, yedges = np.histogram2d(x, y, bins=[x_bins, y_bins])
        H = H.T  # Transpose for correct orientation
        
        # Smooth the histogram
        from scipy.ndimage import gaussian_filter
        H_smooth = gaussian_filter(H, sigma=1.5)
        
        # Apply clipping if specified
        y_centers = (yedges[:-1] + yedges[1:]) / 2
        if clip_above is not None:
            # Mask out regions above the clip line
            mask_2d = y_centers[:, np.newaxis] <= clip_above
            H_smooth = H_smooth * mask_2d
        if clip_below is not None:
            # Mask out regions below the clip line
            mask_2d = y_centers[:, np.newaxis] >= clip_below
            H_smooth = H_smooth * mask_2d
        
        # Calculate contour levels
        sorted_H = np.sort(H_smooth.flatten())[::-1]
        sorted_H = sorted_H[sorted_H > 0]  # Remove zeros
        if len(sorted_H) == 0:
            return
            
        cumsum = np.cumsum(sorted_H)
        cumsum = cumsum / cumsum[-1]
        
        level_3sigma = sorted_H[np.where(cumsum >= 0.997)[0][0]] if np.any(cumsum >= 0.997) else sorted_H[-1]
        level_2sigma = sorted_H[np.where(cumsum >= 0.95)[0][0]] if np.any(cumsum >= 0.95) else sorted_H[-1]
        level_1sigma = sorted_H[np.where(cumsum >= 0.68)[0][0]] if np.any(cumsum >= 0.68) else sorted_H[-1]
        
        levels = [level_3sigma, level_2sigma, level_1sigma]
        alphas = [0.3, 0.5, 0.7]
        
        x_centers = (xedges[:-1] + xedges[1:]) / 2
        
        # Plot filled contours
        for i, (level, alpha) in enumerate(zip(levels, alphas)):
            if i == len(levels) - 1:
                ax.contourf(x_centers, y_centers, H_smooth, 
                        levels=[level, H_smooth.max()],
                        colors=[color], alpha=alpha, label=label)
            else:
                ax.contourf(x_centers, y_centers, H_smooth, 
                        levels=[level, levels[i+1] if i+1 < len(levels) else H_smooth.max()],
                        colors=[color], alpha=alpha)
        
        # Add contour lines
        ax.contour(x_centers, y_centers, H_smooth, 
                levels=levels, colors=color, linewidths=1.0, alpha=0.8)

    # Plot quiescent population (red) - clip above -11
    if len(mass_q) > 0:
        plot_density_contours(mass_q, sSFR_q, 'red', 'Quiescent', clip_above=sSFRcut)

    # Plot star-forming population (blue) - clip below -11
    if len(mass_sf) > 0:
        plot_density_contours(mass_sf, sSFR_sf, 'blue', 'Star-forming', clip_below=sSFRcut)

    # Add the sSFR cut line
    plt.axhline(y=sSFRcut, color='black', linestyle='--', linewidth=2, 
            label=f'sSFR cut = {sSFRcut}', zorder=10)

    plt.ylabel(r'$\log_{10} \mathrm{sSFR}\ (\mathrm{yr^{-1}})$', fontsize=14)
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=14)

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.xlim(8.0, 12.2)
    plt.ylim(-13, -8)

    plt.legend(loc='upper right', fontsize=12, framealpha=0.9)
    plt.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)
    plt.tight_layout()

    plt.savefig(OutputDir + '23.specific_star_formation_rate' + OutputFormat, dpi=150)
    print('Saved to', OutputDir + '23.specific_star_formation_rate' + OutputFormat, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Mass loading factor statistics:')
    print('Mean:', np.mean(MassLoading))
    print('Median:', np.median(MassLoading))
    print('Std Dev:', np.std(MassLoading))
    print('Max:', np.max(MassLoading))
    print('Min:', np.min(MassLoading))
    print('Sample of values:', MassLoading[:10])

    print('Plotting Mass Loading Factor vs Stellar Mass')

    plt.figure()
    plt.scatter(np.log10(StellarMass), MassLoading, c='b', marker='o', s=1, alpha=0.7)
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\mathrm{Mass\ Loading\ Factor}$')
    plt.xlim(8.0, 12.2)
    plt.ylim(0, None)

    plt.savefig(OutputDir + '24.mass_loading_factor_vs_stellar_mass' + OutputFormat)
    print('Saved to', OutputDir + '24.mass_loading_factor_vs_stellar_mass' + OutputFormat, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Cooling Rate vs. Temperature')

    plt.figure()

    print('Cooling Rate statistics:')
    print('Mean:', np.mean(Cooling))
    print('Median:', np.median(Cooling))
    print('Std Dev:', np.std(Cooling))
    print('Max:', np.max(Cooling))
    print('Min:', np.min(Cooling))
    print('Sample of values:', Cooling[:10])

    print('Temperature statistics:')
    print('Mean:', np.mean(Tvir))
    print('Median:', np.median(Tvir))
    print('Std Dev:', np.std(Tvir))
    print('Max:', np.max(Tvir))
    print('Min:', np.min(Tvir))
    print('Sample of values:', Tvir[:10])

    # Filter for valid cooling and temperature
    w = np.where((Cooling > 0) & (Tvir > 0))[0]

    # Convert Temperature to keV and take log10
    log_T_keV = np.log10(Tvir[w] * 8.6e-8)

    # Convert Cooling to units of 10^40 erg/s (log scale)
    # Cooling is already log10(erg/s), so we subtract 40
    log_Cooling_40 = Cooling[w] - 40.0

    plt.scatter(log_T_keV, log_Cooling_40, c='grey', marker='x', s=50, alpha=0.3)
    plt.xlabel(r'$\log_{10} T_{\mathrm{vir}}\ [\mathrm{keV}]$')
    plt.ylabel(r'$\log_{10} \mathrm{Net\ Cooling}\ [10^{40}\ \mathrm{erg\ s^{-1}}]$')

    plt.xlim(-0.2, 1.0)
    plt.ylim(-1.0, 6.0)

    plt.savefig(OutputDir + '25.cooling_rate_vs_temperature' + OutputFormat)
    print('Saved to', OutputDir + '25.cooling_rate_vs_temperature' + OutputFormat, '\n')
    plt.close()

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

    # Add text annotation
    plt.text(11.0, 1.5, 'Shen+03 (early-type)\nGadotti 09 (bulges)', 
            fontsize=10, color='black', alpha=0.7)

    plt.xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} R_{\mathrm{bulge}}\ (\mathrm{kpc})$')
    plt.xlim(8, 12)
    plt.ylim(-0.5, 2.0)
    plt.legend(loc='upper left', frameon=False)
    plt.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)

    outputFile = OutputDir + '26.bulge_size_mass_relation' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Bulge vs Disk Size')

    plt.figure()
    w = np.where((BulgeMass > 0.0) & (BulgeRadius > 0.0) & (DiskRadius > 0.0) & 
                (StellarMass > 1e9))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_disk_radius = np.log10(DiskRadius[w] / 0.001)  # Convert to kpc
    log10_bulge_radius = np.log10(BulgeRadius[w] / 0.001)  # Convert to kpc
    log10_stellar_mass = np.log10(StellarMass[w])

    # Color by total stellar mass
    # sc = plt.scatter(log10_disk_radius, log10_bulge_radius, c=log10_stellar_mass,
    #                 cmap='plasma', s=5, alpha=0.6, vmin=9, vmax=12)
    # plt.colorbar(sc, label=r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')

    # Add Merger Bulge Radius
    w_merger = np.where((MergerBulgeRadius > 0.0) & (DiskRadius > 0.0) & (StellarMass > 1e9))[0]
    if(len(w_merger) > dilute): w_merger = sample(list(w_merger), dilute)

    log10_merger_radius = np.log10(MergerBulgeRadius[w_merger] / 0.001)
    log10_disk_radius_merger = np.log10(DiskRadius[w_merger] / 0.001)
    log10_stellar_mass = np.log10(StellarMass[w_merger])
    sc = plt.scatter(log10_disk_radius_merger, log10_merger_radius, c=log10_stellar_mass, marker='d', edgecolors='k',
                    cmap='plasma', s=50, alpha=0.4, label='Merger Bulge')

    # Add Instability Bulge Radius
    w_instab = np.where((InstabilityBulgeRadius > 0.0) & (DiskRadius > 0.0) & (StellarMass > 1e9))[0]
    if(len(w_instab) > dilute): w_instab = sample(list(w_instab), dilute)
    log10_instab_radius = np.log10(InstabilityBulgeRadius[w_instab] / 0.001)
    log10_disk_radius_instab = np.log10(DiskRadius[w_instab] / 0.001)
    log10_stellar_mass = np.log10(StellarMass[w_instab])
    plt.scatter(log10_disk_radius_instab, log10_instab_radius, c=log10_stellar_mass, marker='s', 
                    cmap='plasma', s=5, alpha=0.4, label='Instability Bulge')

    plt.colorbar(sc, label=r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')

    # Add 1:1 line
    plt.plot([-1, 3], [-1, 3], 'k:', linewidth=1, alpha=0.5, label='1:1')

    # Add typical ratio line (bulge ~ 0.1 * disk)
    disk_range = np.linspace(-1, 3, 100)
    plt.plot(disk_range, disk_range + np.log10(0.1), 'r--', 
            linewidth=2, label=r'$R_{\mathrm{bulge}} = 0.1 R_{\mathrm{disk}}$', alpha=0.7)

    plt.xlabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$')
    plt.ylabel(r'$\log_{10} R_{\mathrm{bulge}}\ (\mathrm{kpc})$')
    plt.xlim(-0.5, 2.5)
    plt.ylim(-1.5, 2.0)
    plt.legend(loc='upper left', frameon=False)
    plt.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)

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

    plt.scatter(log10_stellar_mass, np.log10(disk_mass), c='b', s=10, alpha=0.8, label='Disk Mass', marker='s')
    plt.scatter(log10_stellar_mass, np.log10(merger_bulge_mass), c='r', s=10, alpha=0.6, label='Merger Bulge Mass', marker='s')
    plt.scatter(log10_stellar_mass, np.log10(instability_bulge_mass), c='greenyellow', s=10, alpha=0.3, label='Instability Bulge Mass', marker='s')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{component}}\ (M_{\odot})$')
    plt.xlim(9, 12)
    plt.ylim(8, 12)
    plt.legend(loc='upper left', frameon=False)
    plt.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)

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

    plt.scatter(log10_stellar_mass, disk_ratio, c='b', s=10, alpha=0.8, label='Disk Fraction', marker='s')
    plt.scatter(log10_stellar_mass, merger_ratio, c='r', s=10, alpha=0.6, label='Merger Bulge Fraction', marker='s')
    plt.scatter(log10_stellar_mass, instability_ratio, c='greenyellow', s=10, alpha=0.3, label='Instability Bulge Fraction', marker='s')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'Mass Fraction')
    plt.xlim(9, 12)
    plt.ylim(0, 1.05)
    # plt.legend(loc='center left', frameon=False)
    plt.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)

    outputFile = OutputDir + '29.mass_ratios_vs_stellar_mass' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Disk Scale Radius vs Disk Mass')

    plt.figure()
    w = np.where((StellarMass > 1e9) & (DiskRadius > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    disk_mass = StellarMass[w] - BulgeMass[w]
    # Ensure positive values for log
    w_pos = np.where(disk_mass > 0)[0]
    disk_mass = disk_mass[w_pos]
    disk_radius = DiskRadius[w][w_pos]
    stellar_mass = StellarMass[w][w_pos]

    log10_disk_mass = np.log10(disk_mass)
    log10_disk_radius = np.log10(disk_radius / 0.001) # Convert to kpc
    log10_stellar_mass = np.log10(stellar_mass)

    # Create 2D histogram
    h = plt.hist2d(log10_disk_mass, log10_disk_radius, bins=150, cmap='viridis', cmin=1)
    plt.colorbar(h[3], label='Number of Galaxies')

    plt.ylabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{disk}}\ (M_{\odot})$')
    plt.xlim(9.0, 12.0)
    plt.ylim(1.0e-3, 1.0e2)
    plt.yscale('log')
    plt.grid(True, alpha=0.3, linestyle=':', linewidth=0.5, color='white')

    outputFile = OutputDir + '30.disk_mass_vs_disk_radius' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()
