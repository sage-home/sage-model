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

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model

# Simulation details
#Hubble_h = 0.677400        # Hubble parameter
#BoxSize = 400         # h-1 Mpc
#VolumeFraction = 1.0   # Fraction of the full volume output by the model
FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

#redshifts = [13.9334, 12.67409, 11.50797, 10.44649, 9.480752, 8.58543, 7.77447, 7.032387, 6.344409, 5.721695,
           # 5.153127, 4.629078, 4.26715, 3.929071, 3.610462, 3.314082, 3.128427, 2.951226, 2.77809, 2.616166,
           # 2.458114, 2.309724, 2.16592, 2.027963, 1.8962, 1.770958, 1.65124, 1.535928, 1.426272, 1.321656,
           # 1.220303, 1.124166, 1.031983, 0.9441787, 0.8597281, 0.779046, 0.7020205, 0.6282588, 0.5575475, 0.4899777,
           # 0.4253644, 0.3640053, 0.3047063, 0.2483865, 0.1939743, 0.1425568, 0.09296665, 0.0455745, 0.02265383, 0.0001130128]

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 10000       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies
SMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF
BHMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF
#SMFsnaps = [49, 38, 32, 23, 17, 13, 10, 8, 7, 5, 4]  # Snapshots to plot the SMF
#BHMFsnaps = [49, 38, 32, 23, 17, 13, 10, 8, 7, 5, 4]  # Snapshots to plot the BHMF

OutputFormat = '.pdf'
# plt.rcParams["figure.figsize"] = (8.34,6.25)
# plt.rcParams["figure.dpi"] = 500
# plt.rcParams["font.size"] = 14
plt.style.use('/Users/mbradley/Documents/cohare_palatino_sty.mplstyle')


# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
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

    print('Running allresults (history)\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    # Read galaxy properties
    print('Reading galaxy properties from', DirName+FileName, '\n')

    StellarMassFull = [0]*(LastSnap-FirstSnap+1)
    SfrDiskFull = [0]*(LastSnap-FirstSnap+1)
    SfrBulgeFull = [0]*(LastSnap-FirstSnap+1)
    BlackHoleMassFull = [0]*(LastSnap-FirstSnap+1)
    BulgeMassFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    TypeFull = [0]*(LastSnap-FirstSnap+1)

    for snap in range(FirstSnap,LastSnap+1):

        Snapshot = 'Snap_'+str(snap)

        StellarMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
        SfrDiskFull[snap] = read_hdf(snap_num = Snapshot, param = 'SfrDisk')
        SfrBulgeFull[snap] = read_hdf(snap_num = Snapshot, param = 'SfrBulge')
        BlackHoleMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'BlackHoleMass') * 1.0e10 / Hubble_h
        BulgeMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
        cgmFull[snap] = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
        TypeFull[snap] = read_hdf(snap_num = Snapshot, param = 'Type')

    Wright = pd.read_csv('./optim/data/Wright_2018_z1_z2.csv', delimiter='\t', header=None)

    z1_x = Wright.iloc[:, 0]   # First column
    z1_y = Wright.iloc[:, 1]   # Second column  

    z2_x = Wright.iloc[:, 2]   # First column
    z2_y = Wright.iloc[:, 3]   # Second column

    Shuntov = pd.read_csv('./optim/data/shuntov_2024_all.csv', delimiter='\t', header=None)

    z3_x = Shuntov.iloc[:, 14]   # First column
    z3_y = Shuntov.iloc[:, 15]   # Second column

    z4_x = Shuntov.iloc[:, 18]   # First column
    z4_y = Shuntov.iloc[:, 19]   # Second column

    z5_x = Shuntov.iloc[:, 20]   # First column
    z5_y = Shuntov.iloc[:, 21]   # Second column

# --------------------------------------------------------

    print('Plotting the halo-stellar mass relation')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    
    ###### z=0

    w = np.where(StellarMassFull[SMFsnaps[0]] > 0.0)[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
    y = np.log10(StellarMassFull[SMFsnaps[0]][w])
    x = np.log10(HaloMassFull[SMFsnaps[0]][w])

    plt.scatter(x, y, c='k', alpha=0.3, s=5, label='Model galaxies')

    print('Plotting the halo-stellar mass relation')

    # Initialize list to store binned data for each redshift
    binned_data = []

    # Define number of bins
    n_bins = 50

    # Define colors for different redshifts
    colors = plt.cm.viridis(np.linspace(0, 1, len(SMFsnaps)))

    for i, snap in enumerate(SMFsnaps):
        # Get valid data points - check for positive values before taking log
        w = np.where((StellarMassFull[snap] > 0.0) & (HaloMassFull[snap] > 0.0))[0]
        
        if len(w) == 0:
            print(f"No valid data for redshift {i}")
            continue
            
        y = np.log10(StellarMassFull[snap][w])
        x = np.log10(HaloMassFull[snap][w])
        
        # Remove any remaining infinities or NaNs
        valid_mask = np.isfinite(x) & np.isfinite(y)
        x = x[valid_mask]
        y = y[valid_mask]
        
        print(f"\nRedshift {i}:")
        print(f"Number of valid points: {len(x)}")
        print(f"X range: {np.min(x):.2f} to {np.max(x):.2f}")
        print(f"Y range: {np.min(y):.2f} to {np.max(y):.2f}")
        
        if len(x) == 0:
            continue
        
        # Create bins
        x_min, x_max = np.min(x), np.max(x)
        bins = np.linspace(x_min, x_max, n_bins+1)
        bin_centers = 0.5 * (bins[1:] + bins[:-1])
        
        # Calculate statistics in each bin
        bin_means_y = np.zeros(n_bins)
        bin_stds_y = np.zeros(n_bins)
        bin_counts = np.zeros(n_bins)
        
        for j in range(n_bins):
            mask = (x >= bins[j]) & (x < bins[j+1])
            n_points = np.sum(mask)
            
            if n_points > 0:
                bin_means_y[j] = np.mean(y[mask])
                bin_stds_y[j] = np.std(y[mask])
                bin_counts[j] = n_points
            else:
                bin_means_y[j] = np.nan
                bin_stds_y[j] = np.nan
                bin_counts[j] = 0
        
        # Store binned data
        binned_data.append((bin_centers, bin_means_y, bin_stds_y, bin_counts))
        
    # Write binned data to CSV
    try:
        with open('halostellar_binned_all_redshifts.csv', 'w', newline='') as file:
            writer = csv.writer(file, delimiter='\t')
            
            # Create and write header
            #header = []
            #for i in range(len(SMFsnaps)):
                #header.extend([f'BulgeMass_z{i}', f'BlackHoleMass_z{i}', f'Std_z{i}', f'Count_z{i}'])
            #writer.writerow(header)
            
            # Write data rows
            for i in range(n_bins):
                row = []
                for bin_centers, means, stds, counts in binned_data:
                    if i < len(bin_centers):  # Make sure we have data for this bin
                        # Replace nan with NaN in the output
                        center = 'NaN' if np.isnan(bin_centers[i]) else bin_centers[i]
                        mean = 'NaN' if np.isnan(means[i]) else means[i]
                        std = 'NaN' if np.isnan(stds[i]) else stds[i]
                        count = 'NaN' if np.isnan(counts[i]) else counts[i]
                        row.extend([center, mean, std, count])
                    else:
                        row.extend(['NaN', 'NaN', 'NaN', 'NaN'])
                writer.writerow(row)
                
        print(f"\nSuccessfully wrote binned data to halostellar_binned_all_redshifts.csv")
        
    except Exception as e:
        print(f"\nError writing to CSV: {e}")

    plt.ylabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
    plt.legend()


    ######

    plt.axis([10.0, 15.0, 7, 13])

    # Set the x-axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    leg = plt.legend(loc='lower right', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'B.HaloStellarMass_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')

    # --------------------------------------------------------
    # Reincorporation diagnostics: (2) Fraction of reincorporated gas to hot gas vs. redshift
    # and (6) Cumulative reincorporated gas per galaxy vs. redshift

    print('Plotting reincorporation diagnostics')

    # Read ReincorporatedGas and HotGas for all snapshots
    ReincorporatedGasFull = [0]*(LastSnap-FirstSnap+1)
    HotGasFull = [0]*(LastSnap-FirstSnap+1)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        ReincorporatedGasFull[snap] = read_hdf(snap_num=Snapshot, param='ReincorporatedGas') * 1.0e10 / Hubble_h
        HotGasFull[snap] = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h

    # (2) Fraction of reincorporated gas to hot gas vs. redshift
    reinc_frac = []
    for snap in range(FirstSnap, LastSnap+1):
        total_reinc = np.sum(ReincorporatedGasFull[snap])
        total_hot = np.sum(HotGasFull[snap])
        frac = total_reinc / total_hot if total_hot > 0 else np.nan
        reinc_frac.append(frac)

    plt.figure()
    plt.plot(redshifts, reinc_frac, 'o-', color='purple')
    plt.xlabel('Redshift')
    plt.ylabel('Total Reincorporated Gas / Total Hot Gas')
    plt.title('Fraction of Reincorporated Gas to Hot Gas vs. Redshift')
    plt.gca().invert_xaxis()
    plt.grid(True, which='both', ls=':')
    outputFile = OutputDir + 'reincorporation_fraction_vs_redshift' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')

    # --------------------------------------------------------
    # Plot median halo mass vs. redshift
    print('Plotting median halo mass vs. redshift')
    median_halo_mass = []
    for snap in range(FirstSnap, LastSnap+1):
        vals = HaloMassFull[snap]
        # Only consider positive halo masses
        valid = vals[vals > 0]
        if len(valid) > 0:
            median_halo_mass.append(np.median(valid))
        else:
            median_halo_mass.append(np.nan)

    plt.figure()
    plt.plot(redshifts, median_halo_mass, 'o-', color='green', label='Median Halo Mass')
    plt.xlabel('Redshift')
    plt.ylabel('Median Halo Mass ($M_\odot$)')
    plt.title('Median Halo Mass vs. Redshift')
    plt.yscale('log')
    plt.gca().invert_xaxis()
    plt.legend()
    plt.grid(True, which='both', ls=':')
    outputFile = OutputDir + 'median_halo_mass_vs_redshift' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

    # (6) Cumulative reincorporated gas per galaxy vs. redshift (mean and median)
    mean_cum_reinc = []
    median_cum_reinc = []
    for snap in range(FirstSnap, LastSnap+1):
        vals = ReincorporatedGasFull[snap]
        mean_cum_reinc.append(np.mean(vals))
        median_cum_reinc.append(np.median(vals))

    plt.figure()
    plt.plot(redshifts, mean_cum_reinc, 'o-', label='Mean', color='blue')
    plt.plot(redshifts, median_cum_reinc, 's--', label='Median', color='orange')
    plt.xlabel('Redshift')
    plt.ylabel('Cumulative Reincorporated Gas per Galaxy ($M_\odot$)')
    plt.title('Cumulative Reincorporated Gas per Galaxy vs. Redshift')
    plt.legend()
    plt.gca().invert_xaxis()
    plt.grid(True, which='both', ls=':')
    outputFile = OutputDir + 'cumulative_reincorporated_gas_vs_redshift' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

# --------------------------------------------------------

    print('Plotting the black hole-bulge relation')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    
    ###### z=0

    w = np.where(BlackHoleMassFull[SMFsnaps[0]] > 0.0)[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
    y = np.log10(BlackHoleMassFull[SMFsnaps[0]][w])
    x = np.log10(BulgeMassFull[SMFsnaps[0]][w])

    plt.scatter(x, y, s=0.5, c='k', alpha=0.6, label='Model galaxies')
    # overplot Haring & Rix 2004
    w = 10. ** np.arange(20)
    BHdata = 10. ** (8.2 + 1.12 * np.log10(w / 1.0e11))
    plt.plot(np.log10(w), np.log10(BHdata), 'b-', label="Haring \& Rix 2004")

    

    ###### z=0

    w = np.where(BlackHoleMassFull[SMFsnaps[1]] > 0.0)[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
    y = np.log10(BlackHoleMassFull[SMFsnaps[1]][w])
    x = np.log10(BulgeMassFull[SMFsnaps[1]][w])

    plt.scatter(x, y, s=0.5, c='k', alpha=0.6, label='Model galaxies')

    ###### z=0
    
    w = np.where(BlackHoleMassFull[SMFsnaps[2]] > 0.0)[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
    y = np.log10(BlackHoleMassFull[SMFsnaps[2]][w])
    x = np.log10(BulgeMassFull[SMFsnaps[2]][w])

    plt.scatter(x, y, s=0.5, c='k', alpha=0.6, label='Model galaxies')
    
    print('Plotting the black hole-bulge relation')

    # Initialize list to store binned data for each redshift
    binned_data = []

    # Define number of bins
    n_bins = 50

    # Define colors for different redshifts
    colors = plt.cm.viridis(np.linspace(0, 1, len(SMFsnaps)))

    for i, snap in enumerate(SMFsnaps):
        # Get valid data points - check for positive values before taking log
        w = np.where((BlackHoleMassFull[snap] > 0.0) & (BulgeMassFull[snap] > 0.0))[0]
        
        if len(w) == 0:
            print(f"No valid data for redshift {i}")
            continue
            
        y = np.log10(BlackHoleMassFull[snap][w])
        x = np.log10(BulgeMassFull[snap][w])
        
        # Remove any remaining infinities or NaNs
        valid_mask = np.isfinite(x) & np.isfinite(y)
        x = x[valid_mask]
        y = y[valid_mask]
        
        print(f"\nRedshift {i}:")
        print(f"Number of valid points: {len(x)}")
        print(f"X range: {np.min(x):.2f} to {np.max(x):.2f}")
        print(f"Y range: {np.min(y):.2f} to {np.max(y):.2f}")
        
        if len(x) == 0:
            continue
        
        # Create bins
        x_min, x_max = np.min(x), np.max(x)
        bins = np.linspace(x_min, x_max, n_bins+1)
        bin_centers = 0.5 * (bins[1:] + bins[:-1])
        
        # Calculate statistics in each bin
        bin_means_y = np.zeros(n_bins)
        bin_stds_y = np.zeros(n_bins)
        bin_counts = np.zeros(n_bins)
        
        for j in range(n_bins):
            mask = (x >= bins[j]) & (x < bins[j+1])
            n_points = np.sum(mask)
            
            if n_points > 0:
                bin_means_y[j] = np.mean(y[mask])
                bin_stds_y[j] = np.std(y[mask])
                bin_counts[j] = n_points
            else:
                bin_means_y[j] = np.nan
                bin_stds_y[j] = np.nan
                bin_counts[j] = 0
        
        # Store binned data
        binned_data.append((bin_centers, bin_means_y, bin_stds_y, bin_counts))
        
        # Plot binned data as a line with shaded error region
        valid = ~np.isnan(bin_means_y)
        if np.any(valid):
            plt.plot(bin_centers[valid], bin_means_y[valid], '-', 
                    color=colors[i], linewidth=2, label=f'z = {i}')
            plt.fill_between(bin_centers[valid], 
                            bin_means_y[valid] - bin_stds_y[valid],
                            bin_means_y[valid] + bin_stds_y[valid],
                            color=colors[i], alpha=0.2)

    # Write binned data to CSV
    try:
        with open('bhbm_binned_all_redshifts.csv', 'w', newline='') as file:
            writer = csv.writer(file, delimiter='\t')
            
            # Create and write header
            #header = []
            #for i in range(len(SMFsnaps)):
                #header.extend([f'BulgeMass_z{i}', f'BlackHoleMass_z{i}', f'Std_z{i}', f'Count_z{i}'])
            #writer.writerow(header)
            
            # Write data rows
            for i in range(n_bins):
                row = []
                for bin_centers, means, stds, counts in binned_data:
                    if i < len(bin_centers):  # Make sure we have data for this bin
                        # Replace nan with NaN in the output
                        center = 'NaN' if np.isnan(bin_centers[i]) else bin_centers[i]
                        mean = 'NaN' if np.isnan(means[i]) else means[i]
                        std = 'NaN' if np.isnan(stds[i]) else stds[i]
                        count = 'NaN' if np.isnan(counts[i]) else counts[i]
                        row.extend([center, mean, std, count])
                    else:
                        row.extend(['NaN', 'NaN', 'NaN', 'NaN'])
                writer.writerow(row)
                
        print(f"\nSuccessfully wrote binned data to bhbm_binned_all_redshifts.csv")
        
    except Exception as e:
        print(f"\nError writing to CSV: {e}")

    plt.ylabel(r'$\log_{10} M_{\mathrm{BH}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    plt.legend()


    ######

    plt.axis([8.0, 12.0, 6.0, 10.0])

    # Set the x-axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'A.BlackholeBulgeMass_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# --------------------------------------------------------

    print('Plotting the stellar mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    x_z0 = [11.625, 11.5, 11.375, 11.25, 11.125, 11, 10.875, 10.75, 10.625, 10.5, 10.375, 10.25,
          10.125, 10, 9.875, 9.75, 9.625, 9.5, 9.375, 9.25, 9.125, 9, 8.875, 8.75, 8.625,
            8.5, 8.375, 8.25, 8.125, 8, 7.875, 7.75, 7.625, 7.5, 7.375, 7.25, 7.125, 7, 6.875]
    y_z0 = [-4.704, -3.944, -3.414, -3.172, -2.937, -2.698, -2.566, -2.455, -2.355, -2.301,
          -2.305, -2.274, -2.281, -2.259, -2.201, -2.176, -2.151, -2.095, -2.044, -1.986,
            -1.906, -1.812, -1.806, -1.767, -1.627, -1.646, -1.692, -1.599, -1.581,
              -1.377, -1.417, -1.242, -1.236, -1.236, -1.043, -0.969, -0.937, -0.799, -1.009]
    y_z0 = [10**y_z0 for y_z0 in y_z0]
    plt.plot(x_z0, y_z0, 'b:', lw=10, alpha=0.5, label='Driver et al., 2022 z=[0.1]')

    plt.plot(z1_x, 10**z1_y, 'b:', lw=10, alpha=0.5, label='Wright et al., z=[1.0]')
    plt.plot(z2_x, 10**z2_y, 'g:', lw=10, alpha=0.5, label='... z=[2.0]')
    plt.plot(z3_x, 10**z3_y, 'r:', lw=10, alpha=0.5, label='Shuntov et al., z=[3.0]')
    plt.plot(z4_x, 10**z4_y, 'r:', lw=10, alpha=0.5, label='... z=[4.0]')
    plt.plot(z5_x, 10**z5_y, 'r:', lw=10, alpha=0.5, label='..., z=[5.0]')


    ###### z=0

    # Define minimum number of particles
    min_particles = 30  # Change this to your desired minimum number of particles
    min_halo_mass = min_particles * 0.08 * 1.0e10  # Convert to solar masses

    w = np.where((StellarMassFull[SMFsnaps[0]] > 0.0) & (HaloMassFull[SMFsnaps[0]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[0]][w])

    binwidth = 0.1
    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model galaxies')

    ###### z=1.3
    
    w = np.where((StellarMassFull[SMFsnaps[1]] > 0.0) & (HaloMassFull[SMFsnaps[1]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[1]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'b-')

    ###### z=2
    
    w = np.where((StellarMassFull[SMFsnaps[2]] > 0.0) & (HaloMassFull[SMFsnaps[2]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[2]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'g-')

    ###### z=3
    
    w = np.where((StellarMassFull[SMFsnaps[3]] > 0.0) & (HaloMassFull[SMFsnaps[3]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[3]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')

     ###### z=4
  
    w = np.where((StellarMassFull[SMFsnaps[4]] > 0.0) & (HaloMassFull[SMFsnaps[4]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[4]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')

     ###### z=5
    
    w = np.where((StellarMassFull[SMFsnaps[5]] > 0.0) & (HaloMassFull[SMFsnaps[5]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[5]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')

# Calculate the data for each redshift
    data = []
    for snap in SMFsnaps:
        w = np.where(StellarMassFull[snap] > 0.0)[0]
        mass = np.log10(StellarMassFull[snap][w])

        binwidth = 0.1
        mi = 7.0
        ma = 15.0
        NB = int((ma - mi) / binwidth)
        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        yaxeshisto = counts / volume / binwidth

        data.append((xaxeshisto, yaxeshisto))

    # Find the maximum length of x and y arrays
    max_length = max(len(x) for x, _ in data)

    # Open a single tab-delimited file for writing
    with open('smf_all_redshifts.csv', 'w', newline='') as file:
        writer = csv.writer(file, delimiter='\t')

        # Write header
        #header = ['x', 'y'] * len(SMFsnaps)
        #writer.writerow(header)

        # Write data rows
        for i in range(max_length):
            row = []
            for x, y in data:
                if i < len(x):
                    row.extend([x[i], y[i]])
                else:
                    row.extend(['', ''])
            writer.writerow(row)


    ######

    plt.yscale('log')
    plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])

    # Set the x-axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}$)')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'C.StellarMassFunction_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# --------------------------------------------------------

    print('Plotting the black hole mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    ###### z=0

    w = np.where(BlackHoleMassFull[BHMFsnaps[0]] > 0.0)[0]
    mass = np.log10(BlackHoleMassFull[BHMFsnaps[0]][w])

    binwidth = 0.1
    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    zhang_x0 = [
        4.03157894736842, 4.26315789473684, 4.64210526315789, 5.0, 5.41052631578947,
        5.8, 6.23157894736842, 6.64210526315789, 7.06315789473684, 7.48421052631579,
        7.91578947368421, 8.30526315789474, 8.58947368421053, 8.98947368421053,
        9.29473684210526, 9.55789473684211, 9.83157894736842, 10.0736842105263,
        10.2631578947368, 10.4315789473684
    ]

    zhang_y0 = [
        -1.66472303206997, -1.73469387755102, -1.8396501457726, -1.94460641399417,
        -2.04956268221574, -2.15451895043732, -2.29446064139942, -2.32944606413994,
        -2.43440233236152, -2.46938775510204, -2.64431486880467, -2.78425655976676,
        -2.99416909620991, -3.51895043731779, -4.11370262390671, -4.74344023323615,
        -5.47813411078717, -6.24781341107872, -6.94752186588921, -7.47230320699708
    ]
    zhang_y0 = [10**zhang_y0 for zhang_y0 in zhang_y0]

    plt.plot(zhang_x0, zhang_y0, 'k:', lw=10, alpha=0.5, label='Zhang et al., 2023 z=[0.1]')
    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model galaxies')

    ###### z=1.3
    
    w = np.where(BlackHoleMassFull[BHMFsnaps[1]] > 0.0)[0]
    mass = np.log10(BlackHoleMassFull[BHMFsnaps[1]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    zhang_x1 = [
        4.02105263157895, 4.43157894736842, 4.85263157894737, 5.31578947368421, 5.74736842105263,
        6.15789473684211, 6.64210526315789, 7.15789473684211, 7.57894736842105, 8.10526315789474,
        8.57894736842105, 9.0421052631579, 9.34736842105263, 9.63157894736842, 9.81052631578947,
        10.0315789473684, 10.2526315789474, 10.4210526315789, -20, -20
    ]

    zhang_y1 = [
        -1.80466472303207, -1.90962099125364, -2.08454810495627, -2.22448979591837, -2.29446064139942,
        -2.36443148688047, -2.46938775510204, -2.64431486880467, -2.67930029154519, -2.81924198250729,
        -3.20408163265306, -3.69387755102041, -4.35860058309038, -5.05830903790088, -5.54810495626822,
        -6.35276967930029, -7.15743440233236, -7.82215743440233, -20, -20
    ]
    zhang_y1 = [10**zhang_y1 for zhang_y1 in zhang_y1]

    plt.plot(zhang_x1, zhang_y1, 'b:', lw=10, alpha=0.5, label='... z=[1.0]')
    plt.plot(xaxeshisto, counts / volume / binwidth, 'b-')

    ###### z=2
    
    w = np.where(BlackHoleMassFull[BHMFsnaps[2]] > 0.0)[0]
    mass = np.log10(BlackHoleMassFull[BHMFsnaps[2]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    zhang_x2 = [
        4.05263157894737, 4.35789473684211, 4.71578947368421, 5.13684210526316, 5.47368421052632,
        5.8, 6.18947368421053, 6.58947368421053, 6.91578947368421, 7.27368421052632,
        7.66315789473684, 8.08421052631579, 8.50526315789474, 8.87368421052632, 9.22105263157895,
        9.52631578947369, 9.77894736842105, 10.0, 10.2210526315789, 10.3894736842105
    ]

    zhang_y2 = [
        -1.8396501457726, -2.01457725947522, -2.18950437317784, -2.32944606413994, -2.43440233236152,
        -2.50437317784257, -2.64431486880467, -2.78425655976676, -2.88921282798834, -2.95918367346939,
        -3.13411078717201, -3.23906705539359, -3.48396501457726, -3.83381924198251, -4.35860058309038,
        -4.98833819241983, -5.61807580174927, -6.42274052478134, -7.15743440233236, -7.85714285714286
    ]
    zhang_y2 = [10**zhang_y2 for zhang_y2 in zhang_y2]

    plt.plot(zhang_x2, zhang_y2, 'g:', lw=10, alpha=0.5, label='... z=[2.0]')

    plt.plot(xaxeshisto, counts / volume / binwidth, 'g-')

    ###### z=3
    
    w = np.where(BlackHoleMassFull[BHMFsnaps[3]] > 0.0)[0]
    mass = np.log10(BlackHoleMassFull[BHMFsnaps[3]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    zhang_x3 = [
        4.04137931034483, 4.41379310344828, 4.82758620689655, 5.22068965517241, 5.63448275862069,
        6.05862068965517, 6.52413793103448, 7.02068965517241, 7.53793103448276, 7.95172413793103,
        8.39655172413793, 8.83103448275862, 9.18275862068965, 9.47241379310345, 9.79310344827586,
        10.0827586206897, 10.3310344827586, 10.4448275862069, -20, -20
    ]

    zhang_y3 = [
        -1.93913043478261, -2.07826086956522, -2.28695652173913, -2.39130434782609, -2.56521739130435,
        -2.73913043478261, -2.94782608695652, -3.19130434782609, -3.43478260869565, -3.71304347826087,
        -4.09565217391304, -4.54782608695652, -5.13913043478261, -5.76521739130435, -6.63478260869565,
        -7.57391304347826, -8.37391304347826, -8.93043478260869, -20, -20
    ]
    zhang_y3 = [10**zhang_y3 for zhang_y3 in zhang_y3]

    plt.plot(zhang_x3, zhang_y3, 'r:', lw=10, alpha=0.5, label='... z=[3.0]')

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')
   
# Calculate the data for each redshift
    data = []
    for snap in BHMFsnaps:
        w = np.where(BlackHoleMassFull[snap] > 0.0)[0]
        mass = np.log10(BlackHoleMassFull[snap][w])

        binwidth = 0.1
        mi = 6.0
        ma = 10.0
        NB = int((ma - mi) / binwidth)
        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        yaxeshisto = counts / volume / binwidth

        data.append((xaxeshisto, yaxeshisto))

    # Find the maximum length of x and y arrays
    max_length = max(len(x) for x, _ in data)

    # Open a single tab-delimited file for writing
    with open('bhmf_all_redshifts.csv', 'w', newline='') as file:
        writer = csv.writer(file, delimiter='\t')

        # Write header
        #header = ['x', 'y'] * len(BHMFsnaps)
        #writer.writerow(header)

        # Write data rows
        for i in range(max_length):
            row = []
            for x, y in data:
                if i < len(x):
                    row.extend([x[i], y[i]])
                else:
                    row.extend(['', ''])
            writer.writerow(row)


    ######

    plt.yscale('log')
    plt.axis([6.0, 10.2, 1.0e-6, 1.0e-1])

    # Set the x-axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}$)')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{blackhole}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'A.BlackHoleMassFunction_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting SFR density evolution for all galaxies')

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

    print('Plotting SFR density evolution using ECSV data')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    # Read the ECSV data
    ecsv_data = read_ecsv_data('./data/CSFRD_inferred_from_SMD.ecsv')

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

    # Also plot just the median line for clarity
    #plt.plot(ecsv_redshift, log_sfrd_median, 
            #color='darkblue', lw=2.0, alpha=0.8,
            #label='ECSV Data (median)')

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
    
    SFR_density = np.zeros((LastSnap+1-FirstSnap))       
    for snap in range(FirstSnap,LastSnap+1):
        SFR_density[snap-FirstSnap] = sum(SfrDiskFull[snap]+SfrBulgeFull[snap]) / volume

    z = np.array(redshifts)
    nonzero = np.where(SFR_density > 0.0)[0]
    plt.plot(z[nonzero], np.log10(SFR_density[nonzero]), lw=3.0, label='Model galaxies')

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

    outputFile = OutputDir + 'D.History-SFR-density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting stellar mass density evolution')

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
            
    smd = np.zeros((LastSnap+1-FirstSnap))       
    for snap in range(FirstSnap,LastSnap+1):
      w = np.where((StellarMassFull[snap] > 1.0e8) & (StellarMassFull[snap] < 1.0e13))[0]
      if(len(w) > 0):
        smd[snap-FirstSnap] = sum(StellarMassFull[snap][w]) / volume

    z = np.array(redshifts)
    nonzero = np.where(smd > 0.0)[0]
    plt.plot(z[nonzero], np.log10(smd[nonzero]), 'k-', lw=3.0, label='Model galaxies')

    # Load and plot SMD data from the .ecsv file
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

    # Load the SMD data
    smd_z, smd_rho_50, smd_rho_16, smd_rho_84 = load_smd_data('./data/SMD.ecsv')

    # Convert to log10 and plot with error bars
    smd_log_rho_50 = np.log10(smd_rho_50)
    smd_log_rho_16 = np.log10(smd_rho_16)
    smd_log_rho_84 = np.log10(smd_rho_84)

    # Calculate asymmetric error bars
    smd_yerr_lower = smd_log_rho_50 - smd_log_rho_16
    smd_yerr_upper = smd_log_rho_84 - smd_log_rho_50

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

    outputFile = OutputDir + 'E.History-stellar-mass-density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()


    print('Plotting the stellar mass function with halo particle count filtering')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    x_z0 = [11.625, 11.5, 11.375, 11.25, 11.125, 11, 10.875, 10.75, 10.625, 10.5, 10.375, 10.25,
        10.125, 10, 9.875, 9.75, 9.625, 9.5, 9.375, 9.25, 9.125, 9, 8.875, 8.75, 8.625,
            8.5, 8.375, 8.25, 8.125, 8, 7.875, 7.75, 7.625, 7.5, 7.375, 7.25, 7.125, 7, 6.875]
    y_z0 = [-4.704, -3.944, -3.414, -3.172, -2.937, -2.698, -2.566, -2.455, -2.355, -2.301,
        -2.305, -2.274, -2.281, -2.259, -2.201, -2.176, -2.151, -2.095, -2.044, -1.986,
            -1.906, -1.812, -1.806, -1.767, -1.627, -1.646, -1.692, -1.599, -1.581,
            -1.377, -1.417, -1.242, -1.236, -1.236, -1.043, -0.969, -0.937, -0.799, -1.009]
    y_z0 = [10**y_z0 for y_z0 in y_z0]
    plt.plot(x_z0, y_z0, 'b:', lw=10, alpha=0.5, label='Driver et al., 2022 z=[0.1]')

    plt.plot(z1_x, 10**z1_y, 'b:', lw=10, alpha=0.5, label='Wright et al., z=[1.0]')
    plt.plot(z2_x, 10**z2_y, 'g:', lw=10, alpha=0.5, label='... z=[2.0]')
    plt.plot(z3_x, 10**z3_y, 'r:', lw=10, alpha=0.5, label='Shuntov et al., z=[3.0]')

    # Load "Len" property for each snapshot
    LenFull = [0]*(LastSnap-FirstSnap+1)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        LenFull[snap] = read_hdf(snap_num=Snapshot, param='Len')

    # The minimum number of particles we want in halos
    min_particles = 30

    ###### z=0
    w = np.where((StellarMassFull[SMFsnaps[0]] > 0.0) & (LenFull[SMFsnaps[0]] > min_particles))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[0]][w])

    binwidth = 0.1
    mi = np.floor(min(mass)) - 2 if len(mass) > 0 else 8.0
    ma = np.floor(max(mass)) + 2 if len(mass) > 0 else 12.0
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model galaxies')

    ###### z=1.3
    w = np.where((StellarMassFull[SMFsnaps[1]] > 0.0) & (LenFull[SMFsnaps[1]] > min_particles))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[1]][w])

    mi = np.floor(min(mass)) - 2 if len(mass) > 0 else 8.0
    ma = np.floor(max(mass)) + 2 if len(mass) > 0 else 12.0
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'b-')

    ###### z=2
    w = np.where((StellarMassFull[SMFsnaps[2]] > 0.0) & (LenFull[SMFsnaps[2]] > min_particles))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[2]][w])

    mi = np.floor(min(mass)) - 2 if len(mass) > 0 else 8.0
    ma = np.floor(max(mass)) + 2 if len(mass) > 0 else 12.0
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'g-')

    ###### z=3
    w = np.where((StellarMassFull[SMFsnaps[3]] > 0.0) & (LenFull[SMFsnaps[3]] > min_particles))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[3]][w])

    mi = np.floor(min(mass)) - 2 if len(mass) > 0 else 8.0
    ma = np.floor(max(mass)) + 2 if len(mass) > 0 else 12.0
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')
    """
    ###### z=4
    w = np.where((StellarMassFull[SMFsnaps[4]] > 0.0) & (LenFull[SMFsnaps[4]] > min_particles))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[4]][w])

    mi = np.floor(min(mass)) - 2 if len(mass) > 0 else 8.0
    ma = np.floor(max(mass)) + 2 if len(mass) > 0 else 12.0
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')

    ###### z=5
    w = np.where((StellarMassFull[SMFsnaps[5]] > 0.0) & (LenFull[SMFsnaps[5]] > min_particles))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[5]][w])

    mi = np.floor(min(mass)) - 2 if len(mass) > 0 else 8.0
    ma = np.floor(max(mass)) + 2 if len(mass) > 0 else 12.0
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')
    """
    # Calculate the data for each redshift for CSV export
    data = []
    for snap in SMFsnaps:
        w = np.where((StellarMassFull[snap] > 0.0) & (LenFull[snap] > min_particles))[0]
        mass = np.log10(StellarMassFull[snap][w])

        binwidth = 0.1
        mi = 7.0
        ma = 15.0
        NB = int((ma - mi) / binwidth)
        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        yaxeshisto = counts / volume / binwidth

        data.append((xaxeshisto, yaxeshisto))

    # Find the maximum length of x and y arrays
    max_length = max(len(x) for x, _ in data)

    # Open a single tab-delimited file for writing
    with open('smf_all_redshifts_filtered.csv', 'w', newline='') as file:
        writer = csv.writer(file, delimiter='\t')

        # Write data rows
        for i in range(max_length):
            row = []
            for x, y in data:
                if i < len(x):
                    row.extend([x[i], y[i]])
                else:
                    row.extend(['', ''])
            writer.writerow(row)

    ######

    plt.yscale('log')
    plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])

    # Set the x-axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}$)')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'C.StellarMassFunction_Filtered_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting the CGM mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    ###### z=0

    w = np.where((StellarMassFull[SMFsnaps[0]] > 0.0) & (HaloMassFull[SMFsnaps[0]] >= 0))[0]
    mass = np.log10(cgmFull[SMFsnaps[0]][w])

    binwidth = 0.1
    mi = 7
    ma = 14
    NB = 25
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model galaxies')

    ###### z=1.3
    
    w = np.where((StellarMassFull[SMFsnaps[1]] > 0.0) & (HaloMassFull[SMFsnaps[1]] >= min_halo_mass))[0]
    mass = np.log10(cgmFull[SMFsnaps[1]][w])

    mi = 7
    ma = 14
    NB = 25
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'b-')

    ###### z=2
    
    w = np.where((StellarMassFull[SMFsnaps[2]] > 0.0) & (HaloMassFull[SMFsnaps[2]] >= min_halo_mass))[0]
    mass = np.log10(cgmFull[SMFsnaps[2]][w])

    mi = 7
    ma = 14
    NB = 25
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'g-')

    ###### z=3
    
    w = np.where((StellarMassFull[SMFsnaps[3]] > 0.0) & (HaloMassFull[SMFsnaps[3]] >= min_halo_mass))[0]
    mass = np.log10(cgmFull[SMFsnaps[3]][w])

    mi = 7
    ma = 14
    NB = 25
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')

     ###### z=4
      
    w = np.where((StellarMassFull[SMFsnaps[4]] > 0.0) & (HaloMassFull[SMFsnaps[4]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[4]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')

     ###### z=5
    
    w = np.where((StellarMassFull[SMFsnaps[5]] > 0.0) & (HaloMassFull[SMFsnaps[5]] >= min_halo_mass))[0]
    mass = np.log10(StellarMassFull[SMFsnaps[5]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')
  

    plt.yscale('log')
    plt.axis([7.0, 12.2, 1.0e-6, 1.0e-0])

    # Set the x-axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}$)')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{CGMs}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'F.CGMMassFunction_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# --------------------------------------------------------
    print('Plotting stellar mass function evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Define redshift bins to match the figure
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    # Define colormap - from dark to light/orange
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    # Function to find snapshots within a redshift range
    def find_snapshots_in_z_range(z_min, z_max, redshifts):
        """Find snapshot indices within redshift range"""
        snapshot_indices = []
        for i, z in enumerate(redshifts):
            if z_min <= z <= z_max:
                snapshot_indices.append(i)
        return snapshot_indices

    # Calculate stellar mass function for each redshift bin
    for i, (z_min, z_max) in enumerate(z_bins):
        # Find snapshots in this redshift range
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        
        if len(snap_indices) == 0:
            continue

        # Use different mass bins for the last 4 redshift bins (highest z)
        if i >= len(z_bins) - 4:  # Last 4 redshift bins
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.2, 12.5, 0.1)
        
        mass_centers = mass_bins[:-1] + 0.05
            
        # Calculate stellar mass function for each snapshot separately
        phi_snapshots = []
        bin_width = mass_bins[1] - mass_bins[0]
        
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                # Apply minimum halo mass filter if desired
                if 'min_halo_mass' in locals():
                    w = np.where((StellarMassFull[snap_idx] > 0.0) & 
                               (HaloMassFull[snap_idx] >= min_halo_mass))[0]
                else:
                    w = np.where(StellarMassFull[snap_idx] > 0.0)[0]
                
                if len(w) > 0:
                    masses = np.log10(StellarMassFull[snap_idx][w])
                    counts, bin_edges = np.histogram(masses, bins=mass_bins)
                    phi_snap = counts / (volume * bin_width)
                    phi_snapshots.append(phi_snap)
        
        if len(phi_snapshots) == 0:
            continue
            
        # Convert to numpy array for easier manipulation
        phi_snapshots = np.array(phi_snapshots)
        
        # Calculate mean and 1-sigma error (standard error of the mean)
        phi = np.mean(phi_snapshots, axis=0)
        phi_err = np.std(phi_snapshots, axis=0) / np.sqrt(len(phi_snapshots))
        
        # Only plot where we have data
        valid = (phi > 0) & (phi_err > 0)
        if not np.any(valid):
            continue
            
        # Plot the main curve
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], phi[valid], 
                color=colors[i], linewidth=2, label=label)
        
        # Add shaded error region
        ax.fill_between(mass_centers[valid], 
                       phi[valid] - phi_err[valid], 
                       phi[valid] + phi_err[valid],
                       color=colors[i], alpha=0.3)

    # Set log scale and limits to match the figure and other SMF plots
    ax.set_yscale('log')
    ax.set_xlim(7.2, 12.0)
    ax.set_ylim(1e-6, 1e-1)

    # Labels and formatting
    ax.set_xlabel(r'$\log_{10} M_* [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\phi$ [Mpc$^{-3}$ dex$^{-1}$]', fontsize=14)

    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    # Create legend
    leg = ax.legend(loc='lower left', fontsize=10, frameon=False)
    for text in leg.get_texts():
        text.set_fontsize(10)

    # Grid
    # ax.grid(True, alpha=0.3)

    plt.tight_layout()
    
    # Save the plot
    outputFile = OutputDir + 'G.StellarMassFunctionEvolution' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
    # SFR as a function of StellarMass across the same redshift bins
    print('Plotting SFR (SFRDisk + SFRBulge) as a function of StellarMass evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Use the same z_bins and colors as above
    # z_bins and colors already defined

    for i, (z_min, z_max) in enumerate(z_bins):
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        if len(snap_indices) == 0:
            continue

        # Use same mass bins as above
        if i >= len(z_bins) - 4:
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.2, 12.5, 0.1)
        mass_centers = mass_bins[:-1] + 0.05
        bin_width = mass_bins[1] - mass_bins[0]

        sfr_snapshots = []
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                # Only consider galaxies with positive stellar mass and SFR
                sfr_total = SfrDiskFull[snap_idx] + SfrBulgeFull[snap_idx]
                w = np.where((StellarMassFull[snap_idx] > 0.0) & (sfr_total > 0.0))[0]
                if len(w) > 0:
                    masses = np.log10(StellarMassFull[snap_idx][w])
                    sfrs = np.log10(sfr_total[w])
                    # Bin by stellar mass
                    sfr_bin_means = np.zeros(len(mass_centers))
                    sfr_bin_means.fill(np.nan)
                    sfr_bin_stds = np.zeros(len(mass_centers))
                    sfr_bin_stds.fill(np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1])
                        if np.sum(mask) > 0:
                            sfr_bin_means[j] = np.mean(sfrs[mask])
                            sfr_bin_stds[j] = np.std(sfrs[mask])
                    sfr_snapshots.append((sfr_bin_means, sfr_bin_stds))
        if len(sfr_snapshots) == 0:
            continue
        # Stack and compute mean and error
        sfr_bin_means_stack = np.array([x[0] for x in sfr_snapshots])
        sfr_bin_stds_stack = np.array([x[1] for x in sfr_snapshots])
        sfr_mean = np.nanmean(sfr_bin_means_stack, axis=0)
        sfr_err = np.nanstd(sfr_bin_means_stack, axis=0) / np.sqrt(len(sfr_bin_means_stack))
        valid = ~np.isnan(sfr_mean) & ~np.isnan(sfr_err)
        if not np.any(valid):
            continue
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], sfr_mean[valid], color=colors[i], linewidth=2, label=label)
        ax.fill_between(mass_centers[valid], sfr_mean[valid] - sfr_err[valid], sfr_mean[valid] + sfr_err[valid], color=colors[i], alpha=0.3)

    ax.set_xlim(8.0, 12.2)
    ax.set_ylim(-4, 3)
    ax.set_xlabel(r'$\log_{10} M_* [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\log_{{10}}\mathrm{{SFR}}\ [M_\odot/yr]$', fontsize=14)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    leg = ax.legend(loc='lower right', fontsize=10, frameon=False, ncol=3)
    for text in leg.get_texts():
        text.set_fontsize(10)
    plt.tight_layout()
    outputFile = OutputDir + 'J.SFR.StellarMassFunctionEvolution' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
    print('Plotting stellar mass-halo mass relation evolution')

    plt.figure()
    ax = plt.subplot(111)

    # Define redshift bins (same as before)
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    # Define colormap and line styles
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))
    line_styles = ['-', ':', '--', '-.'] * 4  # Cycle through line styles

    # Halo mass bins
    halo_mass_bins = np.arange(10.0, 15.0, 0.1)
    halo_mass_centers = halo_mass_bins[:-1] + 0.05

    # Function to find snapshots within a redshift range
    def find_snapshots_in_z_range(z_min, z_max, redshifts):
        """Find snapshot indices within redshift range"""
        snapshot_indices = []
        for i, z in enumerate(redshifts):
            if z_min <= z <= z_max:
                snapshot_indices.append(i)
        return snapshot_indices

    # Calculate SMHM relation for each redshift bin
    for i, (z_min, z_max) in enumerate(z_bins):
        # Find snapshots in this redshift range
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        
        if len(snap_indices) == 0:
            continue
            
        # Calculate SMHM relation for each snapshot separately
        stellar_mass_binned = []
        
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                # Get valid galaxies (positive stellar and halo masses)
                w = np.where((StellarMassFull[snap_idx] > 0.0) & 
                           (HaloMassFull[snap_idx] > 0.0))[0]
                
                if len(w) > 0:
                    stellar_masses = StellarMassFull[snap_idx][w]  # Keep in linear units
                    halo_masses = np.log10(HaloMassFull[snap_idx][w])
                    
                    # Bin by halo mass and calculate mean stellar mass in each bin
                    stellar_mass_in_bins = np.zeros(len(halo_mass_centers))
                    stellar_mass_in_bins.fill(np.nan)
                    
                    for j, halo_center in enumerate(halo_mass_centers):
                        bin_mask = (halo_masses >= halo_mass_bins[j]) & (halo_masses < halo_mass_bins[j+1])
                        if np.sum(bin_mask) > 0:
                            stellar_mass_in_bins[j] = np.mean(stellar_masses[bin_mask])
                    
                    stellar_mass_binned.append(stellar_mass_in_bins)
        
        if len(stellar_mass_binned) == 0:
            continue
            
        # Convert to numpy array for easier manipulation
        stellar_mass_binned = np.array(stellar_mass_binned)
        
        # Calculate mean and 1-sigma error across snapshots
        stellar_mass_mean = np.nanmean(stellar_mass_binned, axis=0)
        stellar_mass_err = np.nanstd(stellar_mass_binned, axis=0) / np.sqrt(len(stellar_mass_binned))
        
        # Only plot where we have valid data
        valid = ~np.isnan(stellar_mass_mean) & ~np.isnan(stellar_mass_err)
        if not np.any(valid):
            continue
            
        # Plot the main curve
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(halo_mass_centers[valid], stellar_mass_mean[valid], 
                color=colors[i], linewidth=2, linestyle=line_styles[i], label=label)
        
        # Add shaded error region
        ax.fill_between(halo_mass_centers[valid], 
                       stellar_mass_mean[valid] - stellar_mass_err[valid], 
                       stellar_mass_mean[valid] + stellar_mass_err[valid],
                       color=colors[i], alpha=0.3)

    # Set log scale and limits
    ax.set_yscale('log')
    ax.set_xscale('linear')  # x-axis is already log10 halo mass values
    ax.set_xlim(10.0, 15.0)
    ax.set_ylim(1e7, 1e12)

    # Labels and formatting
    ax.set_xlabel(r'$M_{\rm vir}$ [$M_\odot$]', fontsize=14)
    ax.set_ylabel(r'$M_*$ [$M_\odot$]', fontsize=14)

    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    # Create legend
    leg = ax.legend(loc='lower right', fontsize=10, frameon=False, ncol=2)
    for text in leg.get_texts():
        text.set_fontsize(10)

    # Grid
    # ax.grid(True, alpha=0.3)

    plt.tight_layout()
    
    # Save the plot
    outputFile = OutputDir + 'H.StellarMassHaloMassRelation' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    # Quenched fraction as a function of StellarMass across the same redshift bins (centrals/satellites separated)
    print('Plotting quenched fraction as a function of StellarMass evolution with redshift bins (centrals/satellites)')

    plt.figure()
    ax = plt.subplot(111)

    for i, (z_min, z_max) in enumerate(z_bins):
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        if len(snap_indices) == 0:
            continue

        if i >= len(z_bins) - 4:
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.2, 12.5, 0.1)
        mass_centers = mass_bins[:-1] + 0.05

        all_snapshots = []
        central_snapshots = []
        satellite_snapshots = []
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                sfr_total = SfrDiskFull[snap_idx] + SfrBulgeFull[snap_idx]
                stellar_mass = StellarMassFull[snap_idx]
                galtype = TypeFull[snap_idx]
                # Only consider galaxies with positive stellar mass and SFR
                w = np.where((stellar_mass > 0.0) & (sfr_total > 0.0))[0]
                if len(w) > 0:
                    masses = np.log10(stellar_mass[w])
                    sSFR = np.log10(sfr_total[w] / stellar_mass[w])
                    gtype = galtype[w]
                    # All galaxies
                    quenched = sSFR < sSFRcut
                    all_frac = np.full(len(mass_centers), np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1])
                        if np.sum(mask) > 0:
                            all_frac[j] = np.sum(quenched[mask]) / np.sum(mask)
                    all_snapshots.append(all_frac)
                    # Centrals
                    cen_mask = gtype == 0
                    cen_frac = np.full(len(mass_centers), np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1]) & cen_mask
                        if np.sum(mask) > 0:
                            cen_quenched = quenched[mask]
                            cen_frac[j] = np.sum(cen_quenched) / np.sum(mask)
                    central_snapshots.append(cen_frac)
                    # Satellites
                    sat_mask = gtype == 1
                    sat_frac = np.full(len(mass_centers), np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1]) & sat_mask
                        if np.sum(mask) > 0:
                            sat_quenched = quenched[mask]
                            sat_frac[j] = np.sum(sat_quenched) / np.sum(mask)
                    satellite_snapshots.append(sat_frac)
        if len(all_snapshots) == 0:
            continue
        all_snapshots_arr = np.array(all_snapshots)
        central_snapshots_arr = np.array(central_snapshots)
        satellite_snapshots_arr = np.array(satellite_snapshots)
        all_frac_mean = np.nanmean(all_snapshots_arr, axis=0)
        cen_frac_mean = np.nanmean(central_snapshots_arr, axis=0)
        sat_frac_mean = np.nanmean(satellite_snapshots_arr, axis=0)
        # 1-sigma for centrals
        cen_frac_std = np.nanstd(central_snapshots_arr, axis=0)
        valid_all = ~np.isnan(all_frac_mean)
        valid_cen = ~np.isnan(cen_frac_mean)
        valid_sat = ~np.isnan(sat_frac_mean)
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        # ax.plot(mass_centers[valid_all], all_frac_mean[valid_all], color=colors[i], linewidth=2, label=label+' (All)')
        # Plot centrals with 1-sigma shading
        ax.plot(mass_centers[valid_cen], cen_frac_mean[valid_cen], color=colors[i], linestyle='-', linewidth=2, label=label)
        ax.fill_between(
            mass_centers[valid_cen],
            (cen_frac_mean - cen_frac_std)[valid_cen],
            (cen_frac_mean + cen_frac_std)[valid_cen],
            color=colors[i], alpha=0.18, linewidth=0)
        # Plot satellites
        ax.plot(mass_centers[valid_sat], sat_frac_mean[valid_sat], color=colors[i], linestyle=':', linewidth=2, alpha=0.25)

    ax.set_xlim(8.0, 12.2)
    ax.set_ylim(0, 0.8)
    ax.set_xlabel(r'$\log_{10} M_* [M_\odot]$', fontsize=14)
    ax.set_ylabel('Quenched Fraction', fontsize=14)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    leg = ax.legend(loc='upper left', fontsize=8, frameon=False, ncol=3)
    for text in leg.get_texts():
        text.set_fontsize(8)
    plt.tight_layout()
    outputFile = OutputDir + 'K.QuenchedFraction.StellarMassFunctionEvolution' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
    # Quenched fraction as a function of redshift for fixed stellar mass bins
    print('Plotting quenched fraction as a function of redshift for fixed stellar mass bins')

    # Define mass bins: (log10(M*) lower, upper, label)
    fixed_mass_bins = [
        (8.0, 8.5, r'$10^8-10^{8.5}\ M_\odot$'),
        (8.5, 9.0, r'$10^{8.5}-10^9\ M_\odot$'),
        (9.0, 9.5, r'$10^9-10^{9.5}\ M_\odot$'),
        (9.5, 10.0, r'$10^{9.5}-10^{10}\ M_\odot$'),
        (10.0, 10.5, r'$10^{10}-10^{10.5}\ M_\odot$'),
        (10.5, 11.0, r'$10^{10.5}-10^{11}\ M_\odot$'),
        (11.0, 11.5, r'$10^{11}-10^{11.5}\ M_\odot$'),
        (11.5, 12.0, r'$10^{11.5}-10^{12}\ M_\odot$'),
        (12.0, 12.5, r'$10^{12}-10^{12.5}\ M_\odot$')
    ]


    import matplotlib.cm as cm
    colors_mass = [cm.plasma_r(i / max(1, len(fixed_mass_bins)-1)) for i in range(len(fixed_mass_bins))]

    redshift_vals = []
    quenched_fractions_all = [[] for _ in fixed_mass_bins]
    quenched_errors_all = [[] for _ in fixed_mass_bins]
    quenched_fractions_cen = [[] for _ in fixed_mass_bins]
    quenched_errors_cen = [[] for _ in fixed_mass_bins]
    quenched_fractions_sat = [[] for _ in fixed_mass_bins]
    quenched_errors_sat = [[] for _ in fixed_mass_bins]

    for snap in range(FirstSnap, LastSnap+1):
        z = redshifts[snap]
        redshift_vals.append(z)
        sfr_total = SfrDiskFull[snap] + SfrBulgeFull[snap]
        stellar_mass = StellarMassFull[snap]
        galtype = TypeFull[snap]
        # Only consider galaxies with positive stellar mass and SFR
        w = np.where((stellar_mass > 0.0) & (sfr_total > 0.0))[0]
        if len(w) == 0:
            for i in range(len(fixed_mass_bins)):
                quenched_fractions_all[i].append(np.nan)
                quenched_errors_all[i].append(np.nan)
                quenched_fractions_cen[i].append(np.nan)
                quenched_errors_cen[i].append(np.nan)
                quenched_fractions_sat[i].append(np.nan)
                quenched_errors_sat[i].append(np.nan)
            continue
        masses = np.log10(stellar_mass[w])
        sSFR = np.log10(sfr_total[w] / stellar_mass[w])
        gtype = galtype[w]
        quenched = sSFR < sSFRcut
        for i, (mlow, mhigh, _) in enumerate(fixed_mass_bins):
            # All galaxies
            mask_all = (masses >= mlow) & (masses < mhigh)
            if np.sum(mask_all) > 0:
                frac = np.sum(quenched[mask_all]) / np.sum(mask_all)
                err = np.sqrt(frac * (1 - frac) / np.sum(mask_all))
                quenched_fractions_all[i].append(frac)
                quenched_errors_all[i].append(err)
            else:
                quenched_fractions_all[i].append(np.nan)
                quenched_errors_all[i].append(np.nan)
            # Centrals
            mask_cen = mask_all & (gtype == 0)
            if np.sum(mask_cen) > 0:
                frac_cen = np.sum(quenched[mask_cen]) / np.sum(mask_cen)
                err_cen = np.sqrt(frac_cen * (1 - frac_cen) / np.sum(mask_cen))
                quenched_fractions_cen[i].append(frac_cen)
                quenched_errors_cen[i].append(err_cen)
            else:
                quenched_fractions_cen[i].append(np.nan)
                quenched_errors_cen[i].append(np.nan)
            # Satellites
            mask_sat = mask_all & (gtype == 1)
            if np.sum(mask_sat) > 0:
                frac_sat = np.sum(quenched[mask_sat]) / np.sum(mask_sat)
                err_sat = np.sqrt(frac_sat * (1 - frac_sat) / np.sum(mask_sat))
                quenched_fractions_sat[i].append(frac_sat)
                quenched_errors_sat[i].append(err_sat)
            else:
                quenched_fractions_sat[i].append(np.nan)
                quenched_errors_sat[i].append(np.nan)

    # Convert to arrays and sort by redshift (z=0 left)
    redshift_vals = np.array(redshift_vals)
    sort_idx = np.argsort(redshift_vals)[::-1]  # z=0 left
    redshift_vals = redshift_vals[sort_idx]

    plt.figure()
    ax = plt.subplot(111)

    for i, (_, _, label) in enumerate(fixed_mass_bins):
        # All galaxies
        # frac_arr_all = np.array(quenched_fractions_all[i])[sort_idx]
        # err_arr_all = np.array(quenched_errors_all[i])[sort_idx]
        # ax.plot(redshift_vals, frac_arr_all, label=label+' (All)', color=colors_mass[i], linestyle='-')
        # ax.fill_between(redshift_vals, frac_arr_all - err_arr_all, frac_arr_all + err_arr_all, color=colors_mass[i], alpha=0.2)
        # Centrals
        frac_arr_cen = np.array(quenched_fractions_cen[i])[sort_idx]
        err_arr_cen = np.array(quenched_errors_cen[i])[sort_idx]
        ax.plot(redshift_vals, frac_arr_cen, label=label, color=colors_mass[i], linestyle='-')
        ax.fill_between(redshift_vals, frac_arr_cen - err_arr_cen, frac_arr_cen + err_arr_cen, color=colors_mass[i], alpha=0.2)
        # Satellites
        frac_arr_sat = np.array(quenched_fractions_sat[i])[sort_idx]
        err_arr_sat = np.array(quenched_errors_sat[i])[sort_idx]
        ax.plot(redshift_vals, frac_arr_sat, color=colors_mass[i], linestyle=':', alpha=0.25)
        # ax.fill_between(redshift_vals, frac_arr_sat - err_arr_sat, frac_arr_sat + err_arr_sat, color=colors_mass[i], alpha=0.04)

    ax.set_xlim(0, 2.2)
    ax.set_ylim(0, 1.0)
    ax.set_xlabel('Redshift', fontsize=14)
    ax.set_ylabel('Quenched Fraction', fontsize=14)
    ax.legend(loc='upper right', fontsize=10, frameon=False, ncol=3)
    # ax.grid(True, alpha=0.3)
    plt.tight_layout()
    outputFile = OutputDir + 'L.QuenchedFraction.RedshiftEvolution' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    ###############################################################
    # Cold Gas Metallicity (Z) vs StellarMass in redshift bins (median + 1-sigma shading)
    print('Plotting Cold Gas Metallicity (Z) as a function of StellarMass in redshift bins')

    # Read ColdGas and MetalsColdGas for all snapshots
    ColdGasFull = [0]*(LastSnap-FirstSnap+1)
    MetalsColdGasFull = [0]*(LastSnap-FirstSnap+1)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_' + str(snap)
        ColdGasFull[snap] = read_hdf(snap_num=Snapshot, param='ColdGas')
        MetalsColdGasFull[snap] = read_hdf(snap_num=Snapshot, param='MetalsColdGas')

    # Use same z_bins and colors as previous plots
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    for i, (z_min, z_max) in enumerate(z_bins):
        # Find snapshots in this redshift range
        snap_indices = [idx for idx, z in enumerate(redshifts) if z_min <= z <= z_max]
        if len(snap_indices) == 0:
            continue

        # Use same mass bins as previous plots
        if i >= len(z_bins) - 4:
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.2, 12.5, 0.1)
        mass_centers = mass_bins[:-1] + 0.05

        # Collect binned medians and 1-sigma for each snapshot
        medians = []
        sigmas = []
        for snap_idx in snap_indices:
            stellar_mass = StellarMassFull[snap_idx]
            cold_gas = ColdGasFull[snap_idx]
            metals_cold_gas = MetalsColdGasFull[snap_idx]
            w = np.where((stellar_mass > 0.0) & (cold_gas > 0.0) & (metals_cold_gas > 0.0))[0]
            if len(w) == 0:
                medians.append([np.nan]*len(mass_centers))
                sigmas.append([np.nan]*len(mass_centers))
                continue
            masses = np.log10(stellar_mass[w])
            Z = np.log10((metals_cold_gas[w] / cold_gas[w]) / 0.02) + 9.0
            median_vals = []
            sigma_vals = []
            for j in range(len(mass_centers)):
                mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1])
                if np.sum(mask) > 0:
                    median_vals.append(np.median(Z[mask]))
                    sigma_vals.append(np.std(Z[mask]))
                else:
                    median_vals.append(np.nan)
                    sigma_vals.append(np.nan)
            medians.append(median_vals)
            sigmas.append(sigma_vals)
        medians = np.array(medians)
        sigmas = np.array(sigmas)
        # Median and 1-sigma across snapshots
        median_curve = np.nanmedian(medians, axis=0)
        sigma_curve = np.nanstd(medians, axis=0)
        valid = ~np.isnan(median_curve)
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], median_curve[valid], color=colors[i], linewidth=2, label=label)
        ax.fill_between(mass_centers[valid], median_curve[valid] - sigma_curve[valid], median_curve[valid] + sigma_curve[valid], color=colors[i], alpha=0.3)

    ax.set_xlim(7.2, 12.0)
    ax.set_xlabel(r'$\log_{{10}} M_* [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$Z_\mathrm{{cold\,gas}}$', fontsize=14)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    leg = ax.legend(loc='lower right', fontsize=10, frameon=False, ncol=3)
    for text in leg.get_texts():
        text.set_fontsize(10)
    plt.tight_layout()
    outputFile = OutputDir + 'M.ColdGasMetallicity_vs_StellarMass' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()