#!/usr/bin/env python

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
import csv
import pandas as pd

from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details - Define all directories and their labels
directories = {
    'millennium': './output/millennium/',
    '2res': './output/millennium_2res/',
    '3res': './output/millennium_3res/',
    '4res': './output/millennium_4res/',
    '5res': './output/millennium_5res/',
    '10res': './output/millennium_10res/',
    '15res': './output/millennium_15res/'
}

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
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 10000       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies
SMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF
BHMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (8.34,6.25)
plt.rcParams["figure.dpi"] = 500
plt.rcParams["font.size"] = 14

# Define colors for different resolutions
colors = ['red', 'blue', 'green', 'purple', 'orange', 'brown', 'pink']

# ==================================================================

def plot_behroozi13(ax, z, labels=True, label='Behroozi+13'):
    """Plot Behroozi+13 stellar mass-halo mass relation"""
    # Define halo mass range
    xmf = np.linspace(10.0, 15.0, 100)  # log10(Mvir/Msun)
    
    a = 1.0/(1.0+z)
    nu = np.exp(-4*a*a)
    log_epsilon = -1.777 + (-0.006*(a-1)) * nu
    M1 = 11.514 + (-1.793 * (a-1) - 0.251 * z) * nu
    alpha = -1.412 + 0.731 * nu * (a-1)
    delta = 3.508 + (2.608*(a-1)-0.043 * z) * nu
    gamma = 0.316 + (1.319*(a-1)+0.279 * z) * nu
    Min = xmf - M1
    fx = -np.log10(np.power(10, alpha*Min) + 1.0) + delta * np.power(np.log10(1+np.exp(Min)), gamma) / (1+np.exp(np.power(10, -Min)))
    f = -0.3 + delta * np.power(np.log10(2.0), gamma) / (1+np.exp(1))
    m = log_epsilon + M1 + fx - f
    
    if not labels:
        ax.plot(xmf, m, 'r', linestyle='dashdot', linewidth=1.5)
    else:
        ax.plot(xmf, m, 'r', linestyle='dashdot', linewidth=1.5, label=label)

def calculate_median_std(log_mvir_data, log_stellar_data, bin_edges, label_prefix=""):
    """Calculate median and standard error in bins"""
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
    medians = []
    std_errors = []
    valid_bins = []
    
    for i in range(len(bin_edges) - 1):
        bin_mask = (log_mvir_data >= bin_edges[i]) & (log_mvir_data < bin_edges[i+1])
        bin_stellar = log_stellar_data[bin_mask]
        
        min_galaxies = 5
        
        if len(bin_stellar) >= min_galaxies:
            median_val = np.median(bin_stellar)
            std_val = np.std(bin_stellar)
            
            # Use standard error of the mean instead of full standard deviation
            if len(bin_stellar) > 1:
                std_error = std_val / np.sqrt(len(bin_stellar))
            else:
                std_error = 0.3  # Default uncertainty for single galaxy bins
            
            # Set minimum error bar to avoid overly small error bars
            if std_error < 0.05:
                std_error = 0.05
            
            medians.append(median_val)
            std_errors.append(std_error)
            valid_bins.append(bin_centers[i])
            
        print(f"{label_prefix}Bin {bin_edges[i]:.1f}-{bin_edges[i+1]:.1f}: {len(bin_stellar)} galaxies")
    
    return np.array(valid_bins), np.array(medians), np.array(std_errors)

def filter_centrals_only(Mvir, StellarMass, Type):
    """Filter to keep only central galaxies (Type == 0) with valid masses"""
    valid_mask = (Type == 0) & (Mvir > 0) & (StellarMass > 0)
    return np.where(valid_mask)[0]

def read_hdf(filename=None, snap_num=None, param=None, DirName=None):
    """Read data from one or more SAGE model files"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        property = h5.File(DirName + model_file, 'r')
        data = np.array(property[snap_num][param])
        
        if combined_data is None:
            combined_data = data
        else:
            combined_data = np.concatenate((combined_data, data))
            
    return combined_data


# ==================================================================

if __name__ == '__main__':

    print('Running allresults (history) for multiple resolutions\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    # Create output directory (using first directory as base)
    first_dir = list(directories.values())[0]
    OutputDir = first_dir + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    # Dictionaries to store densities for each resolution
    all_sfr_densities = {}
    all_smd_densities = {}
    all_smhm_data = {}  # For stellar mass-halo mass data

    # Loop through all directories
    for res_label, dir_name in directories.items():
        print(f'Reading galaxy properties from {dir_name}\n')
        
        # Check if directory exists
        if not os.path.exists(dir_name):
            print(f'Warning: Directory {dir_name} does not exist. Skipping {res_label}.')
            continue

        # Initialize arrays for this resolution
        SfrDiskFull = [0]*(LastSnap-FirstSnap+1)
        SfrBulgeFull = [0]*(LastSnap-FirstSnap+1)
        StellarMassFull = [0]*(LastSnap-FirstSnap+1)
        MvirFull = [0]*(LastSnap-FirstSnap+1)
        TypeFull = [0]*(LastSnap-FirstSnap+1)

        # Read data for all snapshots
        try:
            for snap in range(FirstSnap, LastSnap+1):
                Snapshot = 'Snap_'+str(snap)
                SfrDiskFull[snap] = read_hdf(snap_num=Snapshot, param='SfrDisk', DirName=dir_name)
                SfrBulgeFull[snap] = read_hdf(snap_num=Snapshot, param='SfrBulge', DirName=dir_name)
                StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass', DirName=dir_name) * 1.0e10 / Hubble_h
                MvirFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir', DirName=dir_name) * 1.0e10 / Hubble_h
                TypeFull[snap] = read_hdf(snap_num=Snapshot, param='Type', DirName=dir_name)

            # Calculate SFR density for this resolution
            SFR_density = np.zeros((LastSnap+1-FirstSnap))       
            for snap in range(FirstSnap, LastSnap+1):
                SFR_density[snap-FirstSnap] = sum(SfrDiskFull[snap]+SfrBulgeFull[snap]) / volume

            # Calculate stellar mass density for this resolution
            SMD_density = np.zeros((LastSnap+1-FirstSnap))       
            for snap in range(FirstSnap, LastSnap+1):
                w = np.where((StellarMassFull[snap] > 1.0e8) & (StellarMassFull[snap] < 1.0e13))[0]
                if(len(w) > 0):
                    SMD_density[snap-FirstSnap] = sum(StellarMassFull[snap][w]) / volume

            # Store the densities
            all_sfr_densities[res_label] = SFR_density
            all_smd_densities[res_label] = SMD_density
            
            # Store SMHM data for z=0 (LastSnap)
            stellar_z0 = StellarMassFull[LastSnap]
            mvir_z0 = MvirFull[LastSnap]
            type_z0 = TypeFull[LastSnap]
            
            # Filter for central galaxies only
            central_indices = filter_centrals_only(mvir_z0, stellar_z0, type_z0)
            
            if len(central_indices) > 0:
                all_smhm_data[res_label] = {
                    'log_mvir': np.log10(mvir_z0[central_indices]),
                    'log_stellar': np.log10(stellar_z0[central_indices])
                }
                print(f'SMHM data: {len(central_indices)} central galaxies')
            else:
                print(f'Warning: No valid central galaxies found for SMHM relation')
            
            print(f'Successfully processed {res_label}')

        except Exception as e:
            print(f'Error processing {res_label}: {e}')
            continue

    # -------------------------------------------------------

    print('\nPlotting SFR density evolution for all resolutions')

    def read_ecsv_data(filename):
        """
        Read the ECSV file and extract the SFR density data
        """
        data = []
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Find where data starts (after header)
        data_start = 0
        for i, line in enumerate(lines):
            if line.startswith('Redshift sfrd_50'):
                data_start = i + 1
                break
        
        # Parse data lines
        for line in lines[data_start:]:
            line = line.strip()
            if line and not line.startswith('#'):
                values = line.split()
                if len(values) == 4:
                    try:
                        redshift = float(values[0])
                        sfrd_50 = float(values[1])  # median
                        sfrd_16 = float(values[2])  # 16th percentile
                        sfrd_84 = float(values[3])  # 84th percentile
                        data.append([redshift, sfrd_50, sfrd_16, sfrd_84])
                    except ValueError:
                        continue
        
        return np.array(data)

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    # Read and plot the ECSV data if it exists
    ecsv_file = './data/CSFRD_inferred_from_SMD.ecsv'
    if os.path.exists(ecsv_file):
        ecsv_data = read_ecsv_data(ecsv_file)
        
        # Extract columns
        ecsv_redshift = ecsv_data[:, 0]
        ecsv_sfrd_median = ecsv_data[:, 1]
        ecsv_sfrd_16 = ecsv_data[:, 2]
        ecsv_sfrd_84 = ecsv_data[:, 3]

        # Convert to log10 for plotting
        log_sfrd_median = np.log10(ecsv_sfrd_median)
        log_sfrd_16 = np.log10(ecsv_sfrd_16)
        log_sfrd_84 = np.log10(ecsv_sfrd_84)

        # Calculate error bars (differences from median in log space)
        yerr_low = log_sfrd_median - log_sfrd_16
        yerr_high = log_sfrd_84 - log_sfrd_median

        # Plot the ECSV data with error bars
        plt.errorbar(ecsv_redshift, log_sfrd_median, 
                    yerr=[yerr_low, yerr_high], 
                    color='orange', lw=1.0, alpha=0.2, 
                    marker='o', markersize=2, ls='none', 
                    label='COSMOS Web')

    # Original observational data (compilation used in Croton et al. 2006)
    ObsSFRdensity = np.array([
        [0, 0.0158489, 0, 0, 0.0251189, 0.01000000],
        [0.150000, 0.0173780, 0, 0.300000, 0.0181970, 0.0165959],
        [0.0425000, 0.0239883, 0.0425000, 0.0425000, 0.0269153, 0.0213796],
        [0.200000, 0.0295121, 0.100000, 0.300000, 0.0323594, 0.0269154],
        [0.350000, 0.0147911, 0.200000, 0.500000, 0.0173780, 0.0125893],
        [0.625000, 0.0275423, 0.500000, 0.750000, 0.0331131, 0.0229087],
        [0.825000, 0.0549541, 0.750000, 1.00000, 0.0776247, 0.0389045],
        [0.625000, 0.0794328, 0.500000, 0.750000, 0.0954993, 0.0660693],
        [0.700000, 0.0323594, 0.575000, 0.825000, 0.0371535, 0.0281838],
        [1.25000, 0.0467735, 1.50000, 1.00000, 0.0660693, 0.0331131],
        [0.750000, 0.0549541, 0.500000, 1.00000, 0.0389045, 0.0776247],
        [1.25000, 0.0741310, 1.00000, 1.50000, 0.0524807, 0.104713],
        [1.75000, 0.0562341, 1.50000, 2.00000, 0.0398107, 0.0794328],
        [2.75000, 0.0794328, 2.00000, 3.50000, 0.0562341, 0.112202],
        [4.00000, 0.0309030, 3.50000, 4.50000, 0.0489779, 0.0194984],
        [0.250000, 0.0398107, 0.00000, 0.500000, 0.0239883, 0.0812831],
        [0.750000, 0.0446684, 0.500000, 1.00000, 0.0323594, 0.0776247],
        [1.25000, 0.0630957, 1.00000, 1.50000, 0.0478630, 0.109648],
        [1.75000, 0.0645654, 1.50000, 2.00000, 0.0489779, 0.112202],
        [2.50000, 0.0831764, 2.00000, 3.00000, 0.0512861, 0.158489],
        [3.50000, 0.0776247, 3.00000, 4.00000, 0.0416869, 0.169824],
        [4.50000, 0.0977237, 4.00000, 5.00000, 0.0416869, 0.269153],
        [5.50000, 0.0426580, 5.00000, 6.00000, 0.0177828, 0.165959],
        [3.00000, 0.120226, 2.00000, 4.00000, 0.173780, 0.0831764],
        [3.04000, 0.128825, 2.69000, 3.39000, 0.151356, 0.109648],
        [4.13000, 0.114815, 3.78000, 4.48000, 0.144544, 0.0912011],
        [0.350000, 0.0346737, 0.200000, 0.500000, 0.0537032, 0.0165959],
        [0.750000, 0.0512861, 0.500000, 1.00000, 0.0575440, 0.0436516],
        [1.50000, 0.0691831, 1.00000, 2.00000, 0.0758578, 0.0630957],
        [2.50000, 0.147911, 2.00000, 3.00000, 0.169824, 0.128825],
        [3.50000, 0.0645654, 3.00000, 4.00000, 0.0776247, 0.0512861],
        ], dtype=np.float32)

    ObsRedshift = ObsSFRdensity[:, 0]
    xErrLo = np.abs(ObsSFRdensity[:, 0]-ObsSFRdensity[:, 2])
    xErrHi = np.abs(ObsSFRdensity[:, 3]-ObsSFRdensity[:, 0])

    ObsSFR = np.log10(ObsSFRdensity[:, 1])
    yErrLo = np.abs(np.log10(ObsSFRdensity[:, 1])-np.log10(ObsSFRdensity[:, 4]))
    yErrHi = np.abs(np.log10(ObsSFRdensity[:, 5])-np.log10(ObsSFRdensity[:, 1]))

    # plot observational data (compilation used in Croton et al. 2006)
    plt.errorbar(ObsRedshift, ObsSFR, yerr=[yErrLo, yErrHi], xerr=[xErrLo, xErrHi], 
                color='g', lw=1.0, alpha=0.3, marker='o', ls='none', 
                label='Observations')
    
    # Plot all resolutions
    z = np.array(redshifts)
    for i, (res_label, SFR_density) in enumerate(all_sfr_densities.items()):
        nonzero = np.where(SFR_density > 0.0)[0]
        if len(nonzero) > 0:
            plt.plot(z[nonzero], np.log10(SFR_density[nonzero]), 
                    lw=3.0, color=colors[i % len(colors)], 
                    label=f'Model {res_label}')

    # Madau & Dickinson 2014 comparison
    def MD14_sfrd(z):
        psi = 0.015 * (1+z)**2.7 / (1 + ((1+z)/2.9)**5.6) # Msol yr-1 Mpc-3
        return psi

    f_chab_to_salp = 1/0.63
    f_salp_to_chab = 0.63
    f_krup_to_chab = 0.67

    z_values = np.linspace(0,8,200)
    md14= np.log10(MD14_sfrd(z_values) * f_chab_to_salp)
            
    plt.plot(z_values, md14, color='gray', lw=1.5, alpha=0.7, label='Madau & Dickinson 2014')

    plt.ylabel(r'$\log_{10} \mathrm{SFR\ density}\ (M_{\odot}\ \mathrm{yr}^{-1}\ \mathrm{Mpc}^{-3})$')  # Set the y...
    plt.xlabel(r'$\mathrm{redshift}$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    plt.axis([0.0, 8.0, -3.0, -0.4])   

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('small')         

    outputFile = OutputDir + 'D.History-SFR-density-all-resolutions' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('\nPlotting stellar mass density evolution for all resolutions')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    # SMD observations taken from Marchesini+ 2009, h=0.7
    # Values are (minz, maxz, rho,-err,+err)

    dickenson2003 = np.array(((0.6,1.4,8.26,0.08,0.08),
                     (1.4,2.0,7.86,0.22,0.33),
                     (2.0,2.5,7.58,0.29,0.54),
                     (2.5,3.0,7.52,0.51,0.48)),float)
    drory2005 = np.array(((0.25,0.75,8.3,0.15,0.15),
                (0.75,1.25,8.16,0.15,0.15),
                (1.25,1.75,8.0,0.16,0.16),
                (1.75,2.25,7.85,0.2,0.2),
                (2.25,3.0,7.75,0.2,0.2),
                (3.0,4.0,7.58,0.2,0.2)),float)
    PerezGonzalez2008 = np.array(((0.2,0.4,8.41,0.06,0.06),
             (0.4,0.6,8.37,0.04,0.04),
             (0.6,0.8,8.32,0.05,0.05),
             (0.8,1.0,8.24,0.05,0.05),
             (1.0,1.3,8.15,0.05,0.05),
             (1.3,1.6,7.95,0.07,0.07),
             (1.6,2.0,7.82,0.07,0.07),
             (2.0,2.5,7.67,0.08,0.08),
             (2.5,3.0,7.56,0.18,0.18),
             (3.0,3.5,7.43,0.14,0.14),
             (3.5,4.0,7.29,0.13,0.13)),float)
    glazebrook2004 = np.array(((0.8,1.1,7.98,0.14,0.1),
                     (1.1,1.3,7.62,0.14,0.11),
                     (1.3,1.6,7.9,0.14,0.14),
                     (1.6,2.0,7.49,0.14,0.12)),float)
    fontana2006 = np.array(((0.4,0.6,8.26,0.03,0.03),
                  (0.6,0.8,8.17,0.02,0.02),
                  (0.8,1.0,8.09,0.03,0.03),
                  (1.0,1.3,7.98,0.02,0.02),
                  (1.3,1.6,7.87,0.05,0.05),
                  (1.6,2.0,7.74,0.04,0.04),
                  (2.0,3.0,7.48,0.04,0.04),
                  (3.0,4.0,7.07,0.15,0.11)),float)
    rudnick2006 = np.array(((0.0,1.0,8.17,0.27,0.05),
                  (1.0,1.6,7.99,0.32,0.05),
                  (1.6,2.4,7.88,0.34,0.09),
                  (2.4,3.2,7.71,0.43,0.08)),float)
    elsner2008 = np.array(((0.25,0.75,8.37,0.03,0.03),
                 (0.75,1.25,8.17,0.02,0.02),
                 (1.25,1.75,8.02,0.03,0.03),
                 (1.75,2.25,7.9,0.04,0.04),
                 (2.25,3.0,7.73,0.04,0.04),
                 (3.0,4.0,7.39,0.05,0.05)),float)
    
    obs = (dickenson2003,drory2005,PerezGonzalez2008,glazebrook2004,fontana2006,rudnick2006,elsner2008)
    
    for o in obs:
        xval = ((o[:,1]-o[:,0])/2.)+o[:,0]
        if(whichimf == 0):
            ax.errorbar(xval, np.log10(10**o[:,2] *1.6), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')
        elif(whichimf == 1):
            ax.errorbar(xval, np.log10(10**o[:,2] *1.6/1.8), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')

    # Plot all resolutions for stellar mass density
    z = np.array(redshifts)
    for i, (res_label, SMD_density) in enumerate(all_smd_densities.items()):
        nonzero = np.where(SMD_density > 0.0)[0]
        if len(nonzero) > 0:
            plt.plot(z[nonzero], np.log10(SMD_density[nonzero]), 
                    lw=3.0, color=colors[i % len(colors)], 
                    label=f'Model {res_label}')

    # Load and plot SMD data from the .ecsv file if it exists
    def load_smd_data(filename):
        """Load SMD data from ECSV file"""
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Skip header lines (those starting with #)
        data_lines = [line.strip() for line in lines if not line.startswith('#') and line.strip()]
        
        # Skip the column header line
        data_lines = data_lines[1:]
        
        z_vals = []
        rho_50_vals = []
        rho_16_vals = []
        rho_84_vals = []
        
        for line in data_lines: 
            parts = line.split()
            z_vals.append(float(parts[0]))
            rho_50_vals.append(float(parts[1]))
            rho_16_vals.append(float(parts[2]))
            rho_84_vals.append(float(parts[3]))
        
        return np.array(z_vals), np.array(rho_50_vals), np.array(rho_16_vals), np.array(rho_84_vals)

    # Load the SMD data if file exists
    smd_file = './data/SMD.ecsv'
    if os.path.exists(smd_file):
        smd_z, smd_rho_50, smd_rho_16, smd_rho_84 = load_smd_data(smd_file)

        # Convert to log10 and plot with error bars
        smd_log_rho_50 = np.log10(smd_rho_50)
        smd_log_rho_16 = np.log10(smd_rho_16)
        smd_log_rho_84 = np.log10(smd_rho_84)

        # Plot SMD data as line with shaded error region
        ax.plot(smd_z, smd_log_rho_50, color='red', linewidth=2, label='COSMOS Web')
        ax.fill_between(smd_z, smd_log_rho_16, smd_log_rho_84, 
                        color='red', alpha=0.3, interpolate=True)

    plt.ylabel(r'$\log_{10}\ \phi\ (M_{\odot}\ \mathrm{Mpc}^{-3})$')  # Set the y...
    plt.xlabel(r'$\mathrm{redshift}$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    plt.axis([0.0, 4.2, 6.5, 9.0])   

    leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'E.History-stellar-mass-density-all-resolutions' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('\nPlotting stellar mass-halo mass relation for all resolutions (z=0)')

    plt.figure(figsize=(10, 8))
    ax = plt.gca()

    # Load observational data first
    obs_handles = []
    obs_labels = []
    
    # Romeo+20 data
    try:
        obs_data = np.loadtxt('./data/Romeo20_SMHM.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        log_ms_obs = log_mh_obs + log_ms_mh_obs
        handle = plt.scatter(log_mh_obs, log_ms_obs, s=50, alpha=0.8, 
                   color='white', marker='*', edgecolors='orange', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Romeo+20')
    except FileNotFoundError:
        print("Romeo20_SMHM.dat file not found")

    # Romeo+20 ETGs (same style, no separate legend)
    try:
        obs_data = np.loadtxt('./data/Romeo20_SMHM_ETGs.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        log_ms_obs = log_mh_obs + log_ms_mh_obs
        plt.scatter(log_mh_obs, log_ms_obs, s=50, alpha=0.8, color='white', 
                   marker='*', edgecolors='orange', linewidth=0.5)
    except FileNotFoundError:
        print("Romeo20_SMHM_ETGs.dat file not found")

    # Kravtsov+18 data
    try:
        obs_data = np.loadtxt('./data/SatKinsAndClusters_Kravtsov18.dat', comments='#')
        log_mh_obs = obs_data[:, 0]
        log_ms_mh_obs = obs_data[:, 1]
        handle = plt.scatter(log_mh_obs, log_ms_mh_obs, s=50, alpha=0.8,
                   color='purple', marker='s', linewidth=0.5)
        obs_handles.append(handle)
        obs_labels.append('Kravtsov+18')
    except FileNotFoundError:
        print("SatKinsAndClusters_Kravtsov18.dat file not found")

    # Moster+13 data
    try:
        try:
            moster_data = np.loadtxt('./optim/data/Moster_2013.csv', delimiter=None)
            if moster_data.ndim == 1:
                moster_data = moster_data.reshape(-1, 2)
        except:
            # Alternative loading method
            try:
                with open('./optim/Moster_2013.csv', 'r') as f:
                    lines = f.readlines()
                all_values = []
                for line in lines:
                    if not line.strip() or line.startswith('#'):
                        continue
                    values = [float(x) for x in line.strip().split() if x.strip()]
                    all_values.extend(values)
                moster_data = np.array(all_values).reshape(-1, 2)
            except:
                # Try loading from data/ directory
                moster_data = np.loadtxt('./data/Moster_2013.csv', delimiter=None)
                if moster_data.ndim == 1:
                    moster_data = moster_data.reshape(-1, 2)
        
        log_mh_moster = moster_data[:, 0]
        log_ms_moster = moster_data[:, 1]
        handle, = plt.plot(log_mh_moster, log_ms_moster, 
                   linestyle='-.', linewidth=1.5, color='blue')
        obs_handles.append(handle)
        obs_labels.append('Moster+13')
    except Exception as e:
        print(f"Error loading Moster13 data: {e}")

    # Add Behroozi+13 model (z=0)
    behroozi_handle, = plt.plot([], [], 'r', linestyle='dashdot', linewidth=1.5)  # Dummy for legend
    plot_behroozi13(ax, z=0.0, labels=False)
    obs_handles.append(behroozi_handle)
    obs_labels.append('Behroozi+13')

    # Plot all resolutions
    bin_edges = np.linspace(10.0, 15.0, 17)  # Halo mass bins
    sage_handles = []
    sage_labels = []

    for i, (res_label, smhm_data) in enumerate(all_smhm_data.items()):
        if len(smhm_data['log_mvir']) > 0:
            # Calculate median and error in bins
            valid_bins, medians, std_errors = calculate_median_std(
                smhm_data['log_mvir'], smhm_data['log_stellar'], bin_edges, f"{res_label} ")
            
            if len(valid_bins) > 0:
                # Plot median with error bars
                handle = plt.errorbar(valid_bins, medians, yerr=std_errors, 
                            fmt='-', color=colors[i % len(colors)], linewidth=2, 
                            capsize=3, capthick=1.5, label=f'Model {res_label}', zorder=10)
                sage_handles.append(handle)
                sage_labels.append(f'Model {res_label}')

    # Set labels and limits
    plt.xlabel(r'$\log_{10}(M_{\rm vir}/M_\odot)$', fontsize=14)
    plt.ylabel(r'$\log_{10}(M_*/M_\odot)$', fontsize=14)
    plt.xlim(10.0, 15.0)
    plt.ylim(8.0, 12.0)
    
    # Add grid for better readability
    plt.grid(True, alpha=0.3)

    # Create two separate legends
    # Upper left: SAGE models
    if sage_handles:
        legend1 = plt.legend(sage_handles, sage_labels, loc='upper left', fontsize=10, frameon=False)
        plt.gca().add_artist(legend1)
    
    # Lower right: Observations and theory
    if obs_handles:
        legend2 = plt.legend(obs_handles, obs_labels, loc='lower right', fontsize=10, frameon=False)

    plt.title('Stellar Mass-Halo Mass Relation (z=0)', fontsize=16)
    plt.tight_layout()
    
    outputFile = OutputDir + 'F.SMHM-relation-all-resolutions' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # Print summary of processed resolutions
    print(f'Successfully processed {len(all_sfr_densities)} resolutions:')
    for res_label in all_sfr_densities.keys():
        print(f'  - {res_label}')
    print(f'\nGenerated plots:')
    print(f'  - SFR density evolution: D.History-SFR-density-all-resolutions{OutputFormat}')
    print(f'  - Stellar mass density evolution: E.History-stellar-mass-density-all-resolutions{OutputFormat}')
    print(f'  - Stellar mass-halo mass relation (z=0): F.SMHM-relation-all-resolutions{OutputFormat}')
    print(f'\nTotal galaxies analyzed for SMHM (central galaxies only):')
    for res_label, smhm_data in all_smhm_data.items():
        if smhm_data:
            print(f'  - {res_label}: {len(smhm_data["log_mvir"])} galaxies')