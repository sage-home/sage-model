#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
from scipy.interpolate import interp1d
from random import sample, seed
import pandas as pd

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
VolumeFraction = 1.0  # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7500       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

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

    print('Running allresults (local)\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    BulgeMass = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    Vvir = read_hdf(snap_num = Snapshot, param = 'Vvir')
    Vmax = read_hdf(snap_num = Snapshot, param = 'Vmax')
    Rvir = read_hdf(snap_num = Snapshot, param = 'Rvir')
    SfrDisk = read_hdf(snap_num = Snapshot, param = 'SfrDisk')
    SfrBulge = read_hdf(snap_num = Snapshot, param = 'SfrBulge')

    cgm = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h

# -------------------------------------------------------

    print('Plotting CGM outflow diagnostics')

    plt.figure(figsize=(10, 8))  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Vmax > 0))[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    mass = np.log10(Mvir[w])
    cgm = np.log10(cgm[w])

    # Color points by halo mass
    halo_mass = np.log10(StellarMass[w])
    sc = plt.scatter(mass, cgm, c=halo_mass, cmap='viridis', s=5, alpha=0.7)
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')

    plt.ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    plt.axis([9.5, 14.0, 7.0, 10.5])
    plt.title('CGM vs Mvir')

    #plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '19.CGMMass' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting CGM Mass Fuctions')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width
    cgm = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h

    # calculate all
    filter = np.where((Mvir > 0.0))[0]
    if(len(filter) > dilute): filter = sample(list(range(len(filter))), dilute)

    mass = np.log10(cgm[filter])

    mi = 7
    ma = 10
    NB = 25
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    plt.plot(xaxeshisto, counts    / volume / binwidth, 'k-', lw=2, label='Model - CGM')

    plt.yscale('log')
    plt.axis([7.0, 10.2, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '20.CGMMassFunction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

    # Calculate sSFR

    # Filter out invalid values if needed (e.g., where StellarMass is zero)
    #valid_indices = np.isfinite(sSFR) & (StellarMass > 0)
    filter = np.where(StellarMass > 0.01)[0]
    if(len(filter) > dilute): filter = sample(list(range(len(filter))), dilute)

    mvir = (read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h)[filter]
    HotGas = (read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h)[filter]
    cgm = (read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h)[filter]
    sSFR = (np.log10((SfrDisk + SfrBulge) / StellarMass))[filter]

    # Calculate CGM mass fraction
    f_CGM = (cgm / mvir)
    f_CGM_normalized = f_CGM / 0.17  # Normalized by cosmic baryon fraction
    combined_gas = (cgm + HotGas) / mvir
    f_combined_gas = combined_gas / 0.17
    
    log_mvir = np.log10(mvir)
    sSFR_valid = sSFR
    f_combined_gas_valid = f_combined_gas

    # Create a DataFrame with the provided variables
    df = pd.DataFrame({
        'log_mvir': log_mvir,
        'sSFR': sSFR_valid,
        'f_combined_gas': f_combined_gas_valid
    })

    # Sort by log_mvir
    df = df.sort_values('log_mvir')

    # Calculate running median (adjust window size as needed)
    window_size = 101  # This should be an odd number
    df['sSFR_median'] = df['sSFR'].rolling(window=window_size, center=True).median()

    # Handle NaN values at the edges due to rolling window
    # Only try to fill edges if we have any valid median values
    if df['sSFR_median'].notna().any():
        first_valid = df['sSFR_median'].first_valid_index()
        last_valid = df['sSFR_median'].last_valid_index()
        if first_valid is not None and last_valid is not None:
            df.loc[:first_valid, 'sSFR_median'] = df.loc[first_valid, 'sSFR_median']
            df.loc[last_valid:, 'sSFR_median'] = df.loc[last_valid, 'sSFR_median']

    # Alternative: use interpolation to handle NaN values
    # Create an interpolation function
    mask = df['sSFR_median'].notna()
    if mask.sum() > 1:  # Make sure we have at least 2 points for interpolation
        f_interp = interp1d(
            df.loc[mask, 'log_mvir'], 
            df.loc[mask, 'sSFR_median'],
            bounds_error=False,
            fill_value=(df.loc[mask, 'sSFR_median'].iloc[0], df.loc[mask, 'sSFR_median'].iloc[-1])
        )
        df['sSFR_median'] = f_interp(df['log_mvir'])

    # Calculate residuals
    df['sSFR_residual'] = df['sSFR'] - df['sSFR_median']

    # CALCULATE RUNNING MEDIAN OF f_combined_gas VS LOG_MVIR
    # This will be shown as a black line
    df['f_combined_gas_median'] = df['f_combined_gas'].rolling(window=window_size, center=True).median()

    # Handle NaN values for f_combined_gas_median
    mask_gas = df['f_combined_gas_median'].notna()
    if mask_gas.sum() > 1:
        f_gas_interp = interp1d(
            df.loc[mask_gas, 'log_mvir'], 
            df.loc[mask_gas, 'f_combined_gas_median'],
            bounds_error=False,
            fill_value=(df.loc[mask_gas, 'f_combined_gas_median'].iloc[0], 
                    df.loc[mask_gas, 'f_combined_gas_median'].iloc[-1])
        )
        df['f_combined_gas_median'] = f_gas_interp(df['log_mvir'])

    # Create the plot
    plt.figure(figsize=(8, 6))

    sc = plt.scatter(np.log10(mvir), f_combined_gas, c=df['sSFR_residual'], cmap='jet_r', s=5, vmin = -1.5, vmax = 1.5)

    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} sSFR$')

    plt.plot(
            df['log_mvir'], 
            df['f_combined_gas_median'], 
            'k-', 
            linewidth=2
        )

    plt.axis([9.75, 14.5, 0, 1.0])

    # Customize plot
    plt.xlabel(r'$\log_{10}(M_{vir}) [M_{\odot}]$')
    plt.ylabel(r'$f_{\mathrm{CGM}}/{\Omega_b}$')

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '21.CGMMassFraction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    def bin_average(x, y, num_bins=10):
        """
        Calculate the average of array x for y value bins.
        
        Parameters:
        x (array-like): Values to be averaged
        y (array-like): Values to determine bins
        num_bins (int): Number of bins to create
        
        Returns:
        bin_centers (array): Centers of each bin
        bin_averages (array): Average x value for each bin
        bin_counts (array): Number of values in each bin
        """
        # Create bins based on y values
        y_min, y_max = np.min(y), np.max(y)
        bins = np.linspace(y_min, y_max, num_bins + 1)
        
        # Initialize arrays to store results
        bin_averages = np.zeros(num_bins)
        bin_counts = np.zeros(num_bins)
        bin_centers = (bins[:-1] + bins[1:]) / 2
        
        # Calculate averages for each bin
        for i in range(num_bins):
            # Find indices of y values that fall into this bin
            mask = (y >= bins[i]) & (y < bins[i+1])
            
            # If last bin, include the upper boundary
            if i == num_bins - 1:
                mask = (y >= bins[i]) & (y <= bins[i+1])
                
            # Get x values that correspond to y values in this bin
            x_in_bin = x[mask]
            
            # Calculate average if there are values in this bin
            if len(x_in_bin) > 0:
                bin_averages[i] = np.mean(x_in_bin)
                bin_counts[i] = len(x_in_bin)
            else:
                bin_averages[i] = np.nan  # Set NaN for empty bins
        
        return bin_centers, bin_averages, bin_counts

    # Sample data
    mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    cgm = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h

    cgm_fraction = np.log10(cgm / (0.17 * mvir))  # Normalized by cosmic baryon fraction
    hotgas_fraction = np.log10(HotGas / (0.17 * mvir))  # Normalized by cosmic baryon fraction

    print('CGM fraction:', cgm_fraction)
    #print(np.log10(x))

    x = np.log10(mvir)

    valid_indices = np.isfinite(cgm_fraction) & np.isfinite(np.log10(mvir)) & np.isfinite(np.log10(HotGas))
    cgm_fraction_filtered = cgm_fraction[valid_indices]
    mvir_filtered = mvir[valid_indices]
    hotgas_fraction_filtered = hotgas_fraction[valid_indices]

    combined_gas = cgm + HotGas
    combined_gas_fraction = np.log10(combined_gas / (0.17 * mvir))  # Normalized by cosmic baryon fraction

    # Filter the combined fraction with the same criteria
    combined_gas_fraction_filtered = combined_gas_fraction[valid_indices]

    # Calculate bin averages for the combined fraction
    bin_centers_combined, bin_averages_combined, bin_counts_combined = bin_average(
        combined_gas_fraction_filtered, np.log10(mvir_filtered), num_bins=20)

   
    # Calculate bin averages
    bin_centers, bin_averages, bin_counts = bin_average(cgm_fraction_filtered, np.log10(mvir_filtered), num_bins=20)
    bin_centers_hgas, bin_averages_hgas, bin_counts_hgas = bin_average(hotgas_fraction_filtered, np.log10(mvir_filtered), num_bins=20)
    # Print results
    print("Bin centers:", bin_centers)
    print("Bin averages:", bin_averages)
    print("Number of values in each bin:", bin_counts)
    
    # Plot results
    plt.figure(figsize=(10, 6))
    plt.scatter(np.log10(mvir_filtered), cgm_fraction_filtered, alpha=0.1, c='b', s=0.5)
    plt.scatter(np.log10(mvir_filtered), hotgas_fraction_filtered, alpha=0.1, c='r', s=0.5)

    plt.plot(bin_centers, bin_averages, '--', linewidth=1, c='b', label = 'CGM')
    plt.plot(bin_centers_hgas, bin_averages_hgas, '--', linewidth=1, c='r', label = 'Hot Gas')
    plt.plot(bin_centers_combined, bin_averages_combined, '-', linewidth=2, c='k', label = 'Combined Gas')

    # Customize plot
    plt.axis([10, 14.5, -2.5, 0.0])
    plt.xlabel(r'$\log_{10}(M_{vir}) [M_{\odot}]$')
    plt.ylabel(r'$\log_{10}M_{CGM}\ /\ f_b\ M_h$')

    leg = plt.legend(loc='lower right', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '22.CGMMassFraction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()
