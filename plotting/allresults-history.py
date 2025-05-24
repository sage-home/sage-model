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

OutputFormat = '.png'
plt.rcParams["figure.figsize"] = (8.34,6.25)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 14


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

    for snap in range(FirstSnap,LastSnap+1):

        Snapshot = 'Snap_'+str(snap)

        StellarMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
        SfrDiskFull[snap] = read_hdf(snap_num = Snapshot, param = 'SfrDisk')
        SfrBulgeFull[snap] = read_hdf(snap_num = Snapshot, param = 'SfrBulge')
        BlackHoleMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'BlackHoleMass') * 1.0e10 / Hubble_h
        BulgeMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
        cgmFull[snap] = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h

    Wright = pd.read_csv('./optim/Wright_2018_z1_z2.csv', delimiter='\t', header=None)

    z1_x = Wright.iloc[:, 0]   # First column
    z1_y = Wright.iloc[:, 1]   # Second column  

    z2_x = Wright.iloc[:, 2]   # First column
    z2_y = Wright.iloc[:, 3]   # Second column

    Shuntov = pd.read_csv('./optim/shuntov_2024_all.csv', delimiter='\t', header=None)

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

    outputFile = OutputDir + 'A.HaloStellarMass_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
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

    outputFile = OutputDir + 'A.StellarMassFunction_z' + OutputFormat
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
    ecsv_data = read_ecsv_data('CSFRD_inferred_from_SMD.ecsv')

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

    outputFile = OutputDir + 'B.History-SFR-density' + OutputFormat
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
    smd_z, smd_rho_50, smd_rho_16, smd_rho_84 = load_smd_data('SMD.ecsv')

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

    plt.axis([0.0, 12.2, 3.5, 9.0])   

    leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'C.History-stellar-mass-density' + OutputFormat
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

    outputFile = OutputDir + 'A.StellarMassFunction_Filtered_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting the stellar mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    ###### z=0

    # Define minimum number of particles
    min_particles = 30  # Change this to your desired minimum number of particles
    min_halo_mass = min_particles * 0.08 * 1.0e10  # Convert to solar masses

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

    outputFile = OutputDir + 'D.CGMMassFunction_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()