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

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (8.34,6.25)
plt.rcParams["figure.dpi"] = 500
plt.rcParams["font.size"] = 14

FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

CGMsnaps = [63, 50, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF


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

def plot_cgm_mass_histogram():
    """
    Create CGM mass histogram divided by halo mass bins
    """
    print('Creating CGM mass histogram by halo mass bins')
    
    # Read data
    cgm = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    
    # Filter out invalid data
    valid_mask = (cgm > 0) & (mvir > 0)
    cgm_valid = cgm[valid_mask]
    mvir_valid = mvir[valid_mask]
    
    # Calculate volume (following your reference function format)
    BoxSize_corrected = 62.5  # Use the actual BoxSize from millennium
    box_fraction = 1.0
    volume = (BoxSize_corrected / Hubble_h)**3.0 * box_fraction
    
    # Histogram parameters for CGM mass
    mi = 7.5
    ma = 12.75
    binwidth = 0.25
    NB = int((ma - mi) / binwidth)
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (9.5, 12.0),    # Low mass halos
        (12.0, 13.5),    # Intermediate mass halos
        (13.5, 15.0)     # High mass halos
    ]
    
    subset_colors = ['red', 'green', 'blue']
    legend_labels = [
        r'$10^{9.5} < M_{\mathrm{halo}} < 10^{12}$',
        r'$10^{12} < M_{\mathrm{halo}} < 10^{13.5}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Create figure
    fig, ax = plt.subplots(figsize=(10, 8))
    
    custom_lines = []
    
    # Create subsets based on halo mass and plot histograms
    for i, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, subset_colors, legend_labels)):
        # Create mask for this halo mass bin
        log_mvir = np.log10(mvir_valid)
        if i == len(halo_mass_bins) - 1:  # Last bin: no upper limit
            mask = log_mvir >= mass_min
        else:
            mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
        
        # Get CGM masses for this halo mass bin
        cgm_subset = cgm_valid[mask]
        
        if len(cgm_subset) > 0:
            # Create histogram
            (counts, binedges) = np.histogram(np.log10(cgm_subset), range=(mi, ma), bins=NB)
            xaxeshisto = binedges[:-1] + 0.5 * binwidth
            
            # Plot line and fill
            line, = ax.plot(xaxeshisto, counts / volume / binwidth, 
                           color=color, label=label, alpha=0.8, linewidth=2)
            ax.fill_between(xaxeshisto, 0, counts / volume / binwidth, 
                           color=color, alpha=0.2)
            custom_lines.append(line)
    
    # Plot overall histogram (all CGM masses)
    (counts_all, binedges_all) = np.histogram(np.log10(cgm_valid), range=(mi, ma), bins=NB)
    xaxeshisto_all = binedges_all[:-1] + 0.5 * binwidth
    line_all, = ax.plot(xaxeshisto_all, counts_all / volume / binwidth, 
                       color='black', label='Overall', linewidth=3)
    
    # Set plot properties
    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
   
    # Set axis limits
    ax.set_xlim(mi, ma)
    ax.set_ylim(1e-6, 1e-1)
    
    # Create legend
    handles = custom_lines + [line_all]
    ax.legend(handles=handles, loc='upper right', frameon=False, fontsize='medium')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    outputFile = OutputDir + '23.CGM_MassFunction_HaloBins' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass histogram to {outputFile}')
    plt.close()

# ==================================================================

def plot_cgm_mass_histogram_grid():
    """
    Create CGM mass histogram grid divided by halo mass bins across redshifts
    """
    print('Creating CGM mass histogram grid by halo mass bins across redshifts')
    
    # Read CGM and halo mass data for all snapshots
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    StellarMassFull = [0]*(LastSnap-FirstSnap+1)

    print('Reading galaxy properties from', DirName)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        cgmFull[snap] = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    
    # Calculate volume
    volume = (BoxSize / Hubble_h)**3.0 * VolumeFraction
    
    # Histogram parameters for CGM mass
    mi = 7.5
    ma = 12.75
    binwidth = 0.25
    NB = int((ma - mi) / binwidth)
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (9.5, 12.0),    # Low mass halos
        (12.0, 13.5),    # Intermediate mass halos
        (13.5, 15.0)     # High mass halos
    ]
    
    subset_colors = ['red', 'green', 'blue']
    legend_labels = [
        r'$10^{9.5} < M_{\mathrm{halo}} < 10^{12}$',
        r'$10^{12} < M_{\mathrm{halo}} < 10^{13.5}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Determine grid size based on number of snapshots
    n_snaps = len(CGMsnaps)
    if n_snaps <= 4:
        rows, cols = 2, 2
    elif n_snaps <= 6:
        rows, cols = 2, 3
    elif n_snaps <= 9:
        rows, cols = 3, 3
    else:
        rows, cols = 4, 3
    
    # Create subplot grid
    fig, axes = plt.subplots(rows, cols, figsize=(8*cols, 6*rows))
    if n_snaps == 1:
        axes = [axes]  # Make it a list for consistency
    else:
        axes = axes.flatten()
    
    # Colors for different redshifts (matching your script style)
    redshift_colors = ['black', 'blue', 'green', 'red']
    
    for plot_idx, snap in enumerate(CGMsnaps):
        ax = axes[plot_idx]
        z = redshifts[snap]
        
        # Filter out invalid data for this snapshot
        cgm_snap = cgmFull[snap]
        mvir_snap = HaloMassFull[snap]
        stellar_snap = StellarMassFull[snap]
        
        valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
        cgm_valid = cgm_snap[valid_mask]
        mvir_valid = mvir_snap[valid_mask]
        
        custom_lines = []
        
        # Create subsets based on halo mass and plot histograms
        for i, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, subset_colors, legend_labels)):
            # Create mask for this halo mass bin
            log_mvir = np.log10(mvir_valid)
            if i == len(halo_mass_bins) - 1:  # Last bin: no upper limit
                mask = log_mvir >= mass_min
            else:
                mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            # Get CGM masses for this halo mass bin
            cgm_subset = cgm_valid[mask]
            
            if len(cgm_subset) > 0:
                # Create histogram
                (counts, binedges) = np.histogram(np.log10(cgm_subset), range=(mi, ma), bins=NB)
                xaxeshisto = binedges[:-1] + 0.5 * binwidth
                
                # Plot line and fill
                line, = ax.plot(xaxeshisto, counts / volume / binwidth, 
                               color=color, label=label, alpha=0.8, linewidth=2)
                ax.fill_between(xaxeshisto, 0, counts / volume / binwidth, 
                               color=color, alpha=0.2)
                custom_lines.append(line)
        
        # Plot overall histogram (all CGM masses)
        (counts_all, binedges_all) = np.histogram(np.log10(cgm_valid), range=(mi, ma), bins=NB)
        xaxeshisto_all = binedges_all[:-1] + 0.5 * binwidth
        line_all, = ax.plot(xaxeshisto_all, counts_all / volume / binwidth, 
                           color='black', label='Overall', linewidth=3)
        
        # Set plot properties
        ax.set_yscale('log')
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
        ax.set_title(f'z = {z:.1f}')
        
        # Set axis limits
        ax.set_xlim(mi, ma)
        ax.set_ylim(1e-6, 1e-1)
        
        # Add legend only to the first plot
        if plot_idx == 0:
            handles = custom_lines + [line_all]
            ax.legend(handles=handles, loc='upper right', frameon=False, fontsize='small')
        
        # Set minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        
        # Print some statistics for this redshift
        print(f'\nRedshift z = {z:.1f} (Snap {snap}):')
        print(f'Total number of galaxies: {len(cgm_valid)}')
        for i, ((mass_min, mass_max), label) in enumerate(zip(halo_mass_bins, legend_labels)):
            log_mvir = np.log10(mvir_valid)
            if i == len(halo_mass_bins) - 1:
                mask = log_mvir >= mass_min
            else:
                mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            print(f'  {label}: {np.sum(mask)} galaxies')
    
    # Hide unused subplots
    for plot_idx in range(len(CGMsnaps), rows * cols):
        if plot_idx < len(axes):
            axes[plot_idx].axis('off')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    outputFile = OutputDir + '24.CGM_MassFunction_HaloBins_Grid' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'\nSaved CGM mass histogram grid to {outputFile}')
    plt.close()

# ==================================================================

def plot_halo_mass_histogram_grid():
    """
    Create Halo mass histogram grid divided by gas component types across redshifts
    Subsets: Hot Gas, CGM mass, and IntraCluster Stars
    """
    print('Creating Halo mass histogram grid by gas component types across redshifts')
    
    # Read all necessary data for all snapshots
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    StellarMassFull = [0]*(LastSnap-FirstSnap+1)
    HotGasFull = [0]*(LastSnap-FirstSnap+1)
    IntraClusterStarsFull = [0]*(LastSnap-FirstSnap+1)

    print('Reading galaxy properties from', DirName)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        cgmFull[snap] = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        HotGasFull[snap] = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        # Try to read IntraClusterStars - adjust parameter name as needed
        try:
            IntraClusterStarsFull[snap] = read_hdf(snap_num=Snapshot, param='IntraClusterStars') * 1.0e10 / Hubble_h
        except:
            # If IntraClusterStars doesn't exist, try alternative names or use zeros
            try:
                IntraClusterStarsFull[snap] = read_hdf(snap_num=Snapshot, param='ICM') * 1.0e10 / Hubble_h
            except:
                print(f"Warning: IntraClusterStars parameter not found for snap {snap}, using zeros")
                IntraClusterStarsFull[snap] = np.zeros_like(HaloMassFull[snap])
    
    # Calculate volume
    volume = (BoxSize / Hubble_h)**3.0 * VolumeFraction
    
    # Histogram parameters for Halo mass
    mi = 10.0
    ma = 15.0
    binwidth = 0.25
    NB = int((ma - mi) / binwidth)
    
    # Define component types and their properties
    component_properties = [
        ('HotGas', 'Hot Gas'),
        ('CGM', 'CGM Gas'),
        ('IntraClusterStars', 'IntraCluster Stars'),
        ('StellarMass', 'Stellar Mass')
    ]

    subset_colors = ['red', 'green', 'blue']
    legend_labels = [
        r'CGM Gas',
        r'IntraCluster Stars',
        r'Stellar Mass'
    ]
    
    # Determine grid size based on number of snapshots
    n_snaps = len(CGMsnaps)
    if n_snaps <= 4:
        rows, cols = 2, 2
    elif n_snaps <= 6:
        rows, cols = 2, 3
    elif n_snaps <= 9:
        rows, cols = 3, 3
    else:
        rows, cols = 4, 3
    
    # Create subplot grid
    fig, axes = plt.subplots(rows, cols, figsize=(8*cols, 6*rows))
    if n_snaps == 1:
        axes = [axes]  # Make it a list for consistency
    else:
        axes = axes.flatten()
    
    for plot_idx, snap in enumerate(CGMsnaps):
        ax = axes[plot_idx]
        z = redshifts[snap]
        
        # Get data for this snapshot
        halo_mass = HaloMassFull[snap]
        hot_gas = HotGasFull[snap]
        cgm_gas = cgmFull[snap]
        ic_stars = IntraClusterStarsFull[snap]
        stellar_mass = StellarMassFull[snap]
        
        # Filter out invalid data
        valid_mask = (halo_mass > 0) & (stellar_mass > 0)
        halo_mass_valid = halo_mass[valid_mask]
        hot_gas_valid = hot_gas[valid_mask]
        cgm_gas_valid = cgm_gas[valid_mask]
        ic_stars_valid = ic_stars[valid_mask]
        stellar_mass_valid = stellar_mass[valid_mask]
        
        custom_lines = []
        
        # Create subsets based on component mass thresholds and plot histograms
        component_data = [cgm_gas_valid, ic_stars_valid, stellar_mass_valid]

        for i, (component_mass, color, label) in enumerate(zip(component_data, subset_colors, legend_labels)):
            # Create mask for galaxies where this component has significant mass
            # Use a threshold: component mass > 1e8 solar masses
            mass_threshold = 1e8
            mask = component_mass > mass_threshold
            
            # Get halo masses for galaxies with significant mass in this component
            halo_subset = halo_mass_valid[mask]
            
            if len(halo_subset) > 0:
                # Create histogram
                (counts, binedges) = np.histogram(np.log10(halo_subset), range=(mi, ma), bins=NB)
                xaxeshisto = binedges[:-1] + 0.5 * binwidth
                
                # Plot line and fill
                line, = ax.plot(xaxeshisto, counts / volume / binwidth, 
                               color=color, label=label, alpha=0.8, linewidth=2)
                ax.fill_between(xaxeshisto, 0, counts / volume / binwidth, 
                               color=color, alpha=0.2)
                custom_lines.append(line)
        
        # Plot overall histogram (all halo masses)
        (counts_all, binedges_all) = np.histogram(np.log10(halo_mass_valid), range=(mi, ma), bins=NB)
        xaxeshisto_all = binedges_all[:-1] + 0.5 * binwidth
        line_all, = ax.plot(xaxeshisto_all, counts_all / volume / binwidth, 
                           color='black', label='Overall', linewidth=3)
        
        # Set plot properties
        ax.set_yscale('log')
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
        ax.set_title(f'Halo Mass Function by Components, z = {z:.1f}')
        
        # Set axis limits
        ax.set_xlim(mi, ma)
        ax.set_ylim(1e-6, 1e-1)
        
        # Add grid for better readability
        ax.grid(True, alpha=0.3)
        
        # Add legend only to the first plot
        if plot_idx == 0:
            handles = custom_lines + [line_all]
            ax.legend(handles=handles, loc='upper right', frameon=False, fontsize='small')
        
        # Set minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        
        # Print some statistics for this redshift
        print(f'\nRedshift z = {z:.1f} (Snap {snap}):')
        print(f'Total number of galaxies: {len(halo_mass_valid)}')
        for i, (component_mass, label) in enumerate(zip(component_data, legend_labels)):
            mass_threshold = 1e8
            mask = component_mass > mass_threshold
            print(f'  {label} (M > 1e8): {np.sum(mask)} galaxies')
    
    # Hide unused subplots
    for plot_idx in range(len(CGMsnaps), rows * cols):
        if plot_idx < len(axes):
            axes[plot_idx].axis('off')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)
    
    outputFile = OutputDir + '25.Halo_MassFunction_ComponentBins_Grid' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'\nSaved Halo mass histogram grid to {outputFile}')
    plt.close()

# ==================================================================

def plot_component_mass_function_evolution():
    """
    Create evolution plot showing Hot Gas, CGM, and IntraCluster Stars mass functions
    """
    print('Creating component mass function evolution plot')
    
    # Read data for all snapshots
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    StellarMassFull = [0]*(LastSnap-FirstSnap+1)
    HotGasFull = [0]*(LastSnap-FirstSnap+1)

    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        cgmFull[snap] = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        HotGasFull[snap] = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    
    # Calculate volume
    volume = (BoxSize / Hubble_h)**3.0 * VolumeFraction
    
    # Create three subplots: one for each component
    fig, axes = plt.subplots(1, 3, figsize=(24, 6))
    
    component_data = [HotGasFull, cgmFull]
    component_names = ['Hot Gas', 'CGM Gas']
    
    # Colors for different redshifts
    colors = ['black', 'blue', 'green', 'red', 'orange', 'purple']
    
    for comp_idx, (component_full, comp_name) in enumerate(zip(component_data, component_names)):
        ax = axes[comp_idx]
        
        # Plot mass function for each redshift
        for i, snap in enumerate(CGMsnaps[:6]):  # Limit to 6 redshifts
            z = redshifts[snap]
            color = colors[i % len(colors)]
            
            # Filter data
            w = np.where((StellarMassFull[snap] > 0.0) & (component_full[snap] > 0.0))[0]
            mass = np.log10(component_full[snap][w])
            
            if len(mass) > 0:
                binwidth = 0.1
                mi = 7.0
                ma = 14.0
                NB = int((ma - mi) / binwidth)
                (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
                xaxeshisto = binedges[:-1] + 0.5 * binwidth
                
                label = f'z = {z:.1f}' if i == 0 else f'{z:.1f}'
                ax.plot(xaxeshisto, counts / volume / binwidth, color=color, 
                       linewidth=2, label=label)
        
        ax.set_yscale('log')
        ax.set_xlim(7.0, 12.2)
        ax.set_ylim(1.0e-6, 1.0e-0)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(f'$\\log_{{10}} M_{{{comp_name.replace(" ", "\\ ")}}}$ $(M_{{\\odot}})$')
        ax.set_title(f'{comp_name} Mass Function Evolution')
        ax.grid(True, alpha=0.3)
        
        if comp_idx == 0:
            leg = ax.legend(loc='lower left', numpoints=1, labelspacing=0.1)
            leg.draw_frame(False)
            for t in leg.get_texts():
                t.set_fontsize('small')
    
    # Third subplot for combined comparison at z=0
    ax = axes[2]
    snap = CGMsnaps[0]  # z=0
    z = redshifts[snap]
    
    for comp_idx, (component_full, comp_name, color) in enumerate(zip(
            [HotGasFull, cgmFull], 
            ['Hot Gas', 'CGM Gas'],
            ['red', 'blue'])):
        
        w = np.where((StellarMassFull[snap] > 0.0) & (component_full[snap] > 0.0))[0]
        mass = np.log10(component_full[snap][w])
        
        if len(mass) > 0:
            binwidth = 0.1
            mi = 7.0
            ma = 14.0
            NB = int((ma - mi) / binwidth)
            (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
            xaxeshisto = binedges[:-1] + 0.5 * binwidth
            
            ax.plot(xaxeshisto, counts / volume / binwidth, color=color, 
                   linewidth=3, label=comp_name)
    
    ax.set_yscale('log')
    ax.set_xlim(7.0, 12.2)
    ax.set_ylim(1.0e-6, 1.0e-0)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{component}}\ (M_{\odot})$')
    ax.set_title(f'Component Comparison at z = {z:.1f}')
    ax.grid(True, alpha=0.3)
    
    leg = ax.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('small')
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '26.Component_MassFunction_Evolution' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved component mass function evolution to {outputFile}')
    plt.close()

def plot_cgm_mass_evolution():
    """
    Plot CGM mass evolution over redshift for different halo mass bins
    Shows how median CGM mass changes with time for 3 different halo mass ranges
    Uses specific redshifts: 0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5
    Bottom x-axis: Age of Universe (increasing left to right)
    Top x-axis: Redshift (decreasing left to right)
    """
    print('Creating CGM mass evolution plot for different halo mass bins')
    
    # Import cosmology for age calculations
    try:
        from astropy.cosmology import Planck18
        cosmo = Planck18
    except ImportError:
        print("Warning: astropy not available, using approximate age calculation")
        cosmo = None
    
    # Read data for all snapshots
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    StellarMassFull = [0]*(LastSnap-FirstSnap+1)

    print('Reading galaxy properties from', DirName)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        cgmFull[snap] = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (11.5, 12.5),    # Low mass halos
        (12.5, 13.5),    # Intermediate mass halos  
        (13.5, 16.5)     # High mass halos
    ]
    
    colors = ['red', 'green', 'blue']
    labels = [
        r'$10^{11.5} < M_{\mathrm{halo}} < 10^{12.5}$ M$_{\odot}$',
        r'$10^{12.5} < M_{\mathrm{halo}} < 10^{13.5}$ M$_{\odot}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Function to calculate age of Universe (approximate if astropy not available)
    def redshift_to_age(z):
        if cosmo is not None:
            return cosmo.age(z).value  # Returns age in Gyr
        else:
            # Approximate formula for Planck cosmology
            # Age ~ 13.8 Gyr / (1+z)^1.5 (very rough approximation)
            # Better approximation using integral approximation
            H0 = 67.4  # km/s/Mpc
            Omega_m = 0.315
            Omega_lambda = 0.685
            
            # Convert H0 to 1/Gyr
            H0_Gyr = H0 / 977.8  # Conversion factor
            
            # Approximate age calculation
            def integrand(z_prime):
                E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                return 1.0 / ((1 + z_prime) * E_z)
            
            # Numerical integration (simple trapezoidal rule)
            z_vals = np.linspace(z, 30, 1000)
            y_vals = [integrand(z_val) for z_val in z_vals]
            age = np.trapz(y_vals, z_vals) / H0_Gyr
            return age
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 4))
    
    # Define desired redshifts
    desired_redshifts = [0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5]
    
    # Find snapshots closest to desired redshifts
    selected_snapshots = []
    actual_redshifts = []
    
    for target_z in desired_redshifts:
        # Find the snapshot with redshift closest to target_z
        differences = [abs(z - target_z) for z in redshifts]
        closest_snap = differences.index(min(differences))
        selected_snapshots.append(closest_snap)
        actual_redshifts.append(redshifts[closest_snap])
        print(f'Target z={target_z:.2f} -> Snap {closest_snap}, actual z={redshifts[closest_snap]:.3f}')
    
    # For each halo mass bin, track CGM mass evolution
    for bin_idx, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, colors, labels)):
        redshift_values = []
        age_values = []
        median_cgm_masses = []
        std_cgm_masses = []
        
        # Loop through selected snapshots only
        for i, snap in enumerate(selected_snapshots):
            z = actual_redshifts[i]  # Use the actual redshift for this snapshot
            
            # Get data for this snapshot
            cgm_snap = cgmFull[snap]
            mvir_snap = HaloMassFull[snap]
            stellar_snap = StellarMassFull[snap]
            
            # Filter valid data
            valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
            cgm_valid = cgm_snap[valid_mask]
            mvir_valid = mvir_snap[valid_mask]
            
            # Filter by halo mass bin
            log_mvir = np.log10(mvir_valid)
            mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            if np.sum(mass_mask) > 1:  # Require at least 10 galaxies for statistics
                cgm_in_bin = cgm_valid[mass_mask]
                median_cgm = np.median(cgm_in_bin)
                std_cgm = np.std(np.log10(cgm_in_bin))
                
                # Calculate age of Universe at this redshift
                age = redshift_to_age(z)
                
                redshift_values.append(z)
                age_values.append(age)
                median_cgm_masses.append(median_cgm)
                std_cgm_masses.append(std_cgm)
                
                print(f'z={z:.3f}, age={age:.2f} Gyr, Halo bin {bin_idx+1}: {np.sum(mass_mask)} galaxies, median CGM = {median_cgm:.2e} M_sun')
        
        # Plot evolution for this halo mass bin using age on x-axis
        if len(age_values) > 0:
            age_values = np.array(age_values)
            median_cgm_masses = np.array(median_cgm_masses)
            
            # Sort by age to ensure proper line plotting
            sort_idx = np.argsort(age_values)
            age_values = age_values[sort_idx]
            median_cgm_masses = median_cgm_masses[sort_idx]
            
            ax.plot(age_values, np.log10(median_cgm_masses), 
                   color=color, label=label, linewidth=3)
            
            # Add shaded error region
            log_median_cgm = np.log10(median_cgm_masses)
            upper_bound = log_median_cgm + std_cgm_masses
            lower_bound = log_median_cgm - std_cgm_masses

            ax.fill_between(age_values, lower_bound, upper_bound, 
                        color=color, alpha=0.1, edgecolor='none')
    
    # Set up bottom x-axis (age)
    ax.set_xlabel('Age of Universe (Gyr)', fontsize=14)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$', fontsize=14)
    
    # Create top x-axis for redshift
    ax2 = ax.twiny()
    
    # Set up the relationship between age and redshift for the top axis
    # Create a range of redshifts and corresponding ages for the secondary axis
    z_ticks = np.array([0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5])
    age_ticks = np.array([redshift_to_age(z) for z in z_ticks])
    
    # Set the secondary axis limits and ticks
    ax2.set_xlim(ax.get_xlim())  # Match the primary axis limits
    ax2.set_xticks(age_ticks)
    ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
    ax2.set_xlabel('Redshift', fontsize=14)
    
    # Create legend
    leg = ax.legend(loc='lower left', frameon=False, fontsize=12)
    
    # Set reasonable axis limits
    ax.set_ylim(9.0, 11.5)
    
    # Determine age range from the data
    all_ages = []
    for z in actual_redshifts:
        all_ages.append(redshift_to_age(z))
    
    age_min = min(all_ages) - 0.5
    age_max = max(all_ages) + 0.5
    ax.set_xlim(age_min, age_max)
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '27.CGM_Mass_Evolution_HaloBins' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass evolution plot to {outputFile}')
    plt.close()

def plot_cgm_mass_evolution2():
    """
    Plot CGM mass evolution over redshift for different halo mass bins
    Shows how median CGM mass changes with time for 3 different halo mass ranges
    Includes scatter plot of individual galaxies behind the median lines
    Uses specific redshifts: 0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5
    Bottom x-axis: Age of Universe (increasing left to right)
    Top x-axis: Redshift (decreasing left to right)
    """
    print('Creating CGM mass evolution plot for different halo mass bins with scatter')
    
    # Import cosmology for age calculations
    try:
        from astropy.cosmology import Planck18
        cosmo = Planck18
    except ImportError:
        print("Warning: astropy not available, using approximate age calculation")
        cosmo = None
    
    # Read data for all snapshots
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    StellarMassFull = [0]*(LastSnap-FirstSnap+1)

    print('Reading galaxy properties from', DirName)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        cgmFull[snap] = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (11.5, 12.5),    # Low mass halos
        (12.5, 13.5),    # Intermediate mass halos  
        (13.5, 16.5)     # High mass halos
    ]
    
    colors = ['red', 'green', 'blue']
    labels = [
        r'$10^{11.5} < M_{\mathrm{halo}} < 10^{12.5}$ M$_{\odot}$',
        r'$10^{12.5} < M_{\mathrm{halo}} < 10^{13.5}$ M$_{\odot}$',
        r'$10^{13.5} < M_{\mathrm{halo}}$'
    ]
    
    # Function to calculate age of Universe (approximate if astropy not available)
    def redshift_to_age(z):
        if cosmo is not None:
            return cosmo.age(z).value  # Returns age in Gyr
        else:
            # Approximate formula for Planck cosmology
            # Age ~ 13.8 Gyr / (1+z)^1.5 (very rough approximation)
            # Better approximation using integral approximation
            H0 = 67.4  # km/s/Mpc
            Omega_m = 0.315
            Omega_lambda = 0.685
            
            # Convert H0 to 1/Gyr
            H0_Gyr = H0 / 977.8  # Conversion factor
            
            # Approximate age calculation
            def integrand(z_prime):
                E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                return 1.0 / ((1 + z_prime) * E_z)
            
            # Numerical integration (simple trapezoidal rule)
            z_vals = np.linspace(z, 30, 1000)
            y_vals = [integrand(z_val) for z_val in z_vals]
            age = np.trapz(y_vals, z_vals) / H0_Gyr
            return age
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 4))
    
    # Define desired redshifts for median line calculation
    desired_redshifts = [0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5]
    
    # Find snapshots closest to desired redshifts for median lines
    selected_snapshots = []
    actual_redshifts = []
    
    for target_z in desired_redshifts:
        # Find the snapshot with redshift closest to target_z
        differences = [abs(z - target_z) for z in redshifts]
        closest_snap = differences.index(min(differences))
        selected_snapshots.append(closest_snap)
        actual_redshifts.append(redshifts[closest_snap])
        print(f'Target z={target_z:.2f} -> Snap {closest_snap}, actual z={redshifts[closest_snap]:.3f}')
    
    # For scatter plot, use more snapshots for continuity
    # Use every 2nd snapshot to get good coverage
    scatter_snapshots = list(range(FirstSnap, LastSnap+1, 2))
    scatter_redshifts = [redshifts[snap] for snap in scatter_snapshots]
    print(f'Using {len(scatter_snapshots)} snapshots for scatter plot (every 2nd snapshot)')
    
    # First pass: collect scatter plot data for each mass bin using broader snapshot range
    scatter_data = {bin_idx: {'ages': [], 'cgm_masses': []} for bin_idx in range(len(halo_mass_bins))}
    
    # Target number of points per mass bin
    target_points_per_bin = 750
    
    # Calculate typical time spacing for jitter
    age_spacings = []
    for i in range(len(scatter_snapshots)-1):
        age1 = redshift_to_age(redshifts[scatter_snapshots[i]])
        age2 = redshift_to_age(redshifts[scatter_snapshots[i+1]])
        age_spacings.append(abs(age2 - age1))
    typical_spacing = np.median(age_spacings) if age_spacings else 0.1
    jitter_range = typical_spacing * 0.9  # Use 80% of typical spacing for jitter
    
    # For each halo mass bin, collect scatter points across many snapshots
    for bin_idx, (mass_min, mass_max) in enumerate(halo_mass_bins):
        all_ages = []
        all_cgm_masses = []
        
        # Collect data points from scatter_snapshots for continuous coverage
        for snap in scatter_snapshots:
            z = redshifts[snap]
            base_age = redshift_to_age(z)
            
            # Get data for this snapshot
            cgm_snap = cgmFull[snap]
            mvir_snap = HaloMassFull[snap]
            stellar_snap = StellarMassFull[snap]
            
            # Filter valid data
            valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
            cgm_valid = cgm_snap[valid_mask]
            mvir_valid = mvir_snap[valid_mask]
            
            # Filter by halo mass bin
            log_mvir = np.log10(mvir_valid)
            mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            if np.sum(mass_mask) > 0:
                cgm_in_bin = cgm_valid[mass_mask]
                
                # Sample a few points from each snapshot to spread them out
                max_points_per_snap = max(1, target_points_per_bin // len(scatter_snapshots))
                if len(cgm_in_bin) > max_points_per_snap:
                    sample_indices = np.random.choice(len(cgm_in_bin), max_points_per_snap, replace=False)
                    cgm_sampled = cgm_in_bin[sample_indices]
                else:
                    cgm_sampled = cgm_in_bin
                
                # Add temporal jitter to create smoother flow
                n_points = len(cgm_sampled)
                jittered_ages = base_age + np.random.uniform(-jitter_range/2, jitter_range/2, n_points)
                
                all_ages.extend(jittered_ages)
                all_cgm_masses.extend(cgm_sampled)
        
        # Store the collected data
        scatter_data[bin_idx]['ages'] = np.array(all_ages)
        scatter_data[bin_idx]['cgm_masses'] = np.array(all_cgm_masses)
        
        print(f'Mass bin {bin_idx+1}: collected {len(scatter_data[bin_idx]["ages"])} scatter points across {len(scatter_snapshots)} snapshots')
    
    # Plot scatter points first (so they appear behind the lines)
    for bin_idx, (color, label) in enumerate(zip(colors, labels)):
        if len(scatter_data[bin_idx]['ages']) > 0:
            ax.scatter(scatter_data[bin_idx]['ages'], 
                      np.log10(scatter_data[bin_idx]['cgm_masses']),
                      c=color, alpha=0.1, s=5, rasterized=True)
    
    # Second pass: calculate and plot median lines
    for bin_idx, ((mass_min, mass_max), color, label) in enumerate(zip(halo_mass_bins, colors, labels)):
        redshift_values = []
        age_values = []
        median_cgm_masses = []
        std_cgm_masses = []
        
        # Loop through selected snapshots only
        for i, snap in enumerate(selected_snapshots):
            z = actual_redshifts[i]  # Use the actual redshift for this snapshot
            
            # Get data for this snapshot
            cgm_snap = cgmFull[snap]
            mvir_snap = HaloMassFull[snap]
            stellar_snap = StellarMassFull[snap]
            
            # Filter valid data
            valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
            cgm_valid = cgm_snap[valid_mask]
            mvir_valid = mvir_snap[valid_mask]
            
            # Filter by halo mass bin
            log_mvir = np.log10(mvir_valid)
            mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
            
            if np.sum(mass_mask) > 1:  # Require at least 2 galaxies for statistics
                cgm_in_bin = cgm_valid[mass_mask]
                median_cgm = np.median(cgm_in_bin)
                std_cgm = np.std(np.log10(cgm_in_bin))
                
                # Calculate age of Universe at this redshift
                age = redshift_to_age(z)
                
                redshift_values.append(z)
                age_values.append(age)
                median_cgm_masses.append(median_cgm)
                std_cgm_masses.append(std_cgm)
                
                print(f'z={z:.3f}, age={age:.2f} Gyr, Halo bin {bin_idx+1}: {np.sum(mass_mask)} galaxies, median CGM = {median_cgm:.2e} M_sun')
        
        # Plot evolution for this halo mass bin using age on x-axis
        if len(age_values) > 0:
            age_values = np.array(age_values)
            median_cgm_masses = np.array(median_cgm_masses)
            
            # Sort by age to ensure proper line plotting
            sort_idx = np.argsort(age_values)
            age_values = age_values[sort_idx]
            median_cgm_masses = median_cgm_masses[sort_idx]
            
            ax.plot(age_values, np.log10(median_cgm_masses), 
                   color=color, label=label, linewidth=3, zorder=10)
            
            # Add shaded error region
            log_median_cgm = np.log10(median_cgm_masses)
            upper_bound = log_median_cgm + std_cgm_masses
            lower_bound = log_median_cgm - std_cgm_masses

            #ax.fill_between(age_values, lower_bound, upper_bound, 
            #            color=color, alpha=0.3, zorder=5)
    
    # Set up bottom x-axis (age)
    ax.set_xlabel('Age of Universe (Gyr)', fontsize=14)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$', fontsize=14)
    
    # Create top x-axis for redshift
    ax2 = ax.twiny()
    
    # Set up the relationship between age and redshift for the top axis
    # Create a range of redshifts and corresponding ages for the secondary axis
    z_ticks = np.array([0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5])
    age_ticks = np.array([redshift_to_age(z) for z in z_ticks])
    
    # Set the secondary axis limits and ticks
    ax2.set_xlim(ax.get_xlim())  # Match the primary axis limits
    ax2.set_xticks(age_ticks)
    ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
    ax2.set_xlabel('Redshift', fontsize=14)
    
    # Create legend
    leg = ax.legend(loc='lower left', frameon=False, fontsize=12)
    
    # Set reasonable axis limits
    ax.set_ylim(9.0, 11.5)
    
    # Determine age range from the data
    all_ages = []
    for z in actual_redshifts:
        all_ages.append(redshift_to_age(z))
    
    age_min = min(all_ages) - 0.5
    age_max = max(all_ages) + 0.5
    ax.set_xlim(age_min, age_max)
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '27.CGM_Mass_Evolution_HaloBins_Scatter' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass evolution plot with scatter to {outputFile}')
    plt.close()

def plot_cgm_mass_evolution_central_satellite():
    """
    Plot CGM mass evolution over redshift for different halo mass bins separated by centrals and satellites
    Shows how median CGM mass changes with time for 3 different halo mass ranges, each split by Type
    Uses specific redshifts: 0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5
    Bottom x-axis: Age of Universe (increasing left to right)
    Top x-axis: Redshift (decreasing left to right)
    Creates 6 lines total: 3 mass bins × 2 galaxy types (central/satellite)
    """
    print('Creating CGM mass evolution plot for centrals vs satellites in different halo mass bins')
    
    # Import cosmology for age calculations
    try:
        from astropy.cosmology import Planck18
        cosmo = Planck18
    except ImportError:
        print("Warning: astropy not available, using approximate age calculation")
        cosmo = None
    
    # Read data for all snapshots
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    StellarMassFull = [0]*(LastSnap-FirstSnap+1)
    TypeFull = [0]*(LastSnap-FirstSnap+1)

    print('Reading galaxy properties from', DirName)
    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        cgmFull[snap] = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        TypeFull[snap] = read_hdf(snap_num=Snapshot, param='Type')
    
    # Define halo mass bins (in log10 solar masses)
    halo_mass_bins = [
        (11.5, 12.5),    # Low mass halos
        (12.5, 13.5),    # Intermediate mass halos  
        (13.5, 14.5)     # High mass halos
    ]
    
    # Define galaxy types
    galaxy_types = [0, 1]  # Central, Satellite
    type_names = ['Central', 'Satellite']
    
    # Define colors and line styles for each combination
    colors = ['red', 'green', 'blue']  # One for each mass bin
    linestyles = ['-', '--']  # Solid for centrals, dashed for satellites
    
    # Create labels for each combination
    mass_bin_labels = [
        r'$10^{11.5} < M_{\mathrm{halo}} < 10^{12.5}$ M$_{\odot}$',
        r'$10^{12.5} < M_{\mathrm{halo}} < 10^{13.5}$ M$_{\odot}$',
        r'$10^{13.5} < M_{\mathrm{halo}} < 10^{14.5}$ M$_{\odot}$'
    ]
    
    # Function to calculate age of Universe (approximate if astropy not available)
    def redshift_to_age(z):
        if cosmo is not None:
            return cosmo.age(z).value  # Returns age in Gyr
        else:
            # Approximate formula for Planck cosmology
            # Age ~ 13.8 Gyr / (1+z)^1.5 (very rough approximation)
            # Better approximation using integral approximation
            H0 = 67.4  # km/s/Mpc
            Omega_m = 0.315
            Omega_lambda = 0.685
            
            # Convert H0 to 1/Gyr
            H0_Gyr = H0 / 977.8  # Conversion factor
            
            # Approximate age calculation
            def integrand(z_prime):
                E_z = np.sqrt(Omega_m * (1 + z_prime)**3 + Omega_lambda)
                return 1.0 / ((1 + z_prime) * E_z)
            
            # Numerical integration (simple trapezoidal rule)
            z_vals = np.linspace(z, 30, 1000)
            y_vals = [integrand(z_val) for z_val in z_vals]
            age = np.trapz(y_vals, z_vals) / H0_Gyr
            return age
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Define desired redshifts
    desired_redshifts = [0.0, 0.1, 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5]
    
    # Find snapshots closest to desired redshifts
    selected_snapshots = []
    actual_redshifts = []
    
    for target_z in desired_redshifts:
        # Find the snapshot with redshift closest to target_z
        differences = [abs(z - target_z) for z in redshifts]
        closest_snap = differences.index(min(differences))
        selected_snapshots.append(closest_snap)
        actual_redshifts.append(redshifts[closest_snap])
        print(f'Target z={target_z:.2f} -> Snap {closest_snap}, actual z={redshifts[closest_snap]:.3f}')
    
    # For each halo mass bin and galaxy type combination, track CGM mass evolution
    for mass_bin_idx, (mass_min, mass_max) in enumerate(halo_mass_bins):
        for type_idx, galaxy_type in enumerate(galaxy_types):
            redshift_values = []
            age_values = []
            median_cgm_masses = []
            std_cgm_masses = []
            
            # Create label for this combination
            type_name = type_names[type_idx]
            mass_label = mass_bin_labels[mass_bin_idx]
            label = f'{type_name} - {mass_label}'
            
            color = colors[mass_bin_idx]
            linestyle = linestyles[type_idx]
            
            # Loop through selected snapshots only
            for i, snap in enumerate(selected_snapshots):
                z = actual_redshifts[i]  # Use the actual redshift for this snapshot
                
                # Get data for this snapshot
                cgm_snap = cgmFull[snap]
                mvir_snap = HaloMassFull[snap]
                stellar_snap = StellarMassFull[snap]
                type_snap = TypeFull[snap]
                
                # Filter valid data
                valid_mask = (cgm_snap > 0) & (mvir_snap > 0) & (stellar_snap > 0)
                cgm_valid = cgm_snap[valid_mask]
                mvir_valid = mvir_snap[valid_mask]
                stellar_valid = stellar_snap[valid_mask]
                type_valid = type_snap[valid_mask]
                
                # Filter by halo mass bin
                log_mvir = np.log10(mvir_valid)
                mass_mask = (log_mvir >= mass_min) & (log_mvir < mass_max)
                
                # Apply mass mask to all arrays
                cgm_mass_filtered = cgm_valid[mass_mask]
                type_mass_filtered = type_valid[mass_mask]
                
                # Filter by galaxy type within this mass bin
                type_mask = (type_mass_filtered == galaxy_type)
                
                if np.sum(type_mask) > 5:  # Require at least 5 galaxies for statistics (reduced threshold)
                    cgm_final = cgm_mass_filtered[type_mask]
                    median_cgm = np.median(cgm_final)
                    std_cgm = np.std(np.log10(cgm_final))
                    
                    # Calculate age of Universe at this redshift
                    age = redshift_to_age(z)
                    
                    redshift_values.append(z)
                    age_values.append(age)
                    median_cgm_masses.append(median_cgm)
                    std_cgm_masses.append(std_cgm)
                    
                    print(f'z={z:.3f}, age={age:.2f} Gyr, Mass bin {mass_bin_idx+1}, {type_name}: {np.sum(type_mask)} galaxies, median CGM = {median_cgm:.2e} M_sun')
            
            # Plot evolution for this combination using age on x-axis
            if len(age_values) > 0:
                age_values = np.array(age_values)
                median_cgm_masses = np.array(median_cgm_masses)
                std_cgm_masses = np.array(std_cgm_masses)
                
                # Sort by age to ensure proper line plotting
                sort_idx = np.argsort(age_values)
                age_values = age_values[sort_idx]
                median_cgm_masses = median_cgm_masses[sort_idx]
                std_cgm_masses = std_cgm_masses[sort_idx]
                
                # Plot the main line
                ax.plot(age_values, np.log10(median_cgm_masses), 
                       color=color, label=label, linewidth=3, linestyle=linestyle)
                
                # Add shaded error region
                log_median_cgm = np.log10(median_cgm_masses)
                upper_bound = log_median_cgm + std_cgm_masses
                lower_bound = log_median_cgm - std_cgm_masses
                
                # ax.fill_between(age_values, lower_bound, upper_bound, 
                #                color=color, alpha=0.2)  # Reduced alpha since we have more lines
    
    # Set up bottom x-axis (age)
    ax.set_xlabel('Age of Universe (Gyr)', fontsize=14)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$', fontsize=14)
    
    # Create top x-axis for redshift
    ax2 = ax.twiny()
    
    # Set up the relationship between age and redshift for the top axis
    # Create a range of redshifts and corresponding ages for the secondary axis
    z_ticks = np.array([0.0, 0.25, 0.5, 1.0, 1.5, 2.0, 2.5])
    age_ticks = np.array([redshift_to_age(z) for z in z_ticks])
    
    # Set the secondary axis limits and ticks
    ax2.set_xlim(ax.get_xlim())  # Match the primary axis limits
    ax2.set_xticks(age_ticks)
    ax2.set_xticklabels([f'{z:.1f}' for z in z_ticks])
    ax2.set_xlabel('Redshift', fontsize=14)
    
    # Create legend
    leg = ax.legend(loc='lower left', frameon=False, fontsize=10)  # Smaller font since more entries
    
    # Set reasonable axis limits
    ax.set_ylim(9.0, 11.0)
    
    # Determine age range from the data
    all_ages = []
    for z in actual_redshifts:
        all_ages.append(redshift_to_age(z))
    
    age_min = min(all_ages) - 0.5
    age_max = max(all_ages) + 0.5
    ax.set_xlim(age_min, age_max)
    
    plt.tight_layout()
    
    # Save the plot
    OutputDir = DirName + 'plots/'
    outputFile = OutputDir + '27.CGM_Mass_Evolution_CentralSatellite' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    print(f'Saved CGM mass evolution plot (central vs satellite) to {outputFile}')
    plt.close()

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

    plot_cgm_mass_histogram()
    plot_cgm_mass_histogram_grid()
    plot_halo_mass_histogram_grid()
    plot_component_mass_function_evolution()
    plot_cgm_mass_evolution()
    plot_cgm_mass_evolution2()
    plot_cgm_mass_evolution_central_satellite()

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
