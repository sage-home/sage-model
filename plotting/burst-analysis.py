#!/usr/bin/env python

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
import csv
import pandas as pd
from scipy.stats import iqr
from random import sample, seed
import imageio
import glob

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
FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

# Plotting options
whichimf = 1          # 0=Slapeter; 1=Chabrier
dilute = 10000        # Number of galaxies to plot in scatter plots
sSFRcut = -11.0       # Divide quiescent from star forming galaxies
SMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF
BHMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF

# Burstiness-specific options
burstiness_percentile = 90  # Percentile threshold to define starbursts
lookback_time = [13.8, 13.5, 13.0, 12.5, 12.0, 11.5, 11.0, 10.0, 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0]  # Approximate lookback times for snapshots, in Gyr

# GIF options
gif_fps = 1           # Frames per second in the GIF
gif_duration = 600    # Duration of each frame in seconds (overrides fps if both specified)
gif_snapshots = list(range(0, 64, 2))  # Which snapshots to include in the GIF (every 2nd snapshot)
final_gif_name = 'burstiness_evolution.gif'  # Name of the final GIF

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
        try:
            property = h5.File(DirName + model_file, 'r')
            data = np.array(property[snap_num][param])
            
            if combined_data is None:
                combined_data = data
            else:
                combined_data = np.concatenate((combined_data, data))
        except KeyError:
            print(f"Warning: Parameter '{param}' not found in snapshot '{snap_num}' of file '{model_file}'")
            if combined_data is None:
                combined_data = np.array([])
    
    return combined_data

def check_sfr_history_availability(snap_num):
    """Check if SFR history is available and in what format"""
    # First check if SfrDiskSteps array exists (all steps in one array)
    try:
        test = read_hdf(snap_num=snap_num, param='SfrDisk')
        if test is not None and len(test) > 0:
            return "steps_array"
    except:
        pass
    
    # Then check for SfrDisk array and SfrBulge array
    try:
        disk = read_hdf(snap_num=snap_num, param='SfrDisk')
        bulge = read_hdf(snap_num=snap_num, param='SfrBulge')
        if disk is not None and bulge is not None:
            return "single_value"
    except:
        pass
    
    # Finally check if we have SfrDisk_0, SfrDisk_1, etc.
    try:
        test = read_hdf(snap_num=snap_num, param='SfrDisk')
        if test is not None and len(test) > 0:
            return "indexed_steps"
    except:
        pass
    
    return "not_available"

def calculate_burstiness(StellarMass, SfrDisk, SfrBulge, BulgeMass, 
                        TimeOfLastMajorMerger=None, TimeOfLastMinorMerger=None,
                        QuasarModeBHaccretionMass=None, current_time=None):
    """
    Calculate a galaxy burstiness parameter using multiple indicators from SAGE.
    
    Parameters:
    -----------
    StellarMass : array_like
        Stellar mass history
    SfrDisk : array_like
        Star formation rate history in the disk
    SfrBulge : array_like
        Star formation rate history in the bulge
    BulgeMass : array_like
        Bulge mass history
    TimeOfLastMajorMerger : array_like, optional
        Time of last major merger
    TimeOfLastMinorMerger : array_like, optional
        Time of last minor merger
    QuasarModeBHaccretionMass : array_like, optional
        Mass accreted onto black hole in quasar mode
    current_time : float, optional
        Current time for calculating time since merger
        
    Returns:
    --------
    burstiness : array_like
        Burstiness parameter for each galaxy
    sfr_variability : array_like
        Star formation rate variability component
    """
    
    # Convert inputs to numpy arrays if they aren't already
    StellarMass = np.array(StellarMass)
    SfrDisk = np.array(SfrDisk)
    SfrBulge = np.array(SfrBulge)
    BulgeMass = np.array(BulgeMass)
    
    # 1. SFR Variability - primary burstiness indicator
    # Calculate total SFR 
    total_sfr = SfrDisk + SfrBulge
    
    # For highly burst systems, the SFR variability will be high
    # but since we only have one timestep, we'll estimate based on bulge-to-disk SFR ratio
    sfr_ratio = np.zeros_like(StellarMass, dtype=np.float64)
    
    # Where both disk and bulge SFRs are positive, calculate ratio
    valid_sfr = (SfrDisk > 0) & (SfrBulge > 0)
    if np.any(valid_sfr):
        # Use max/min ratio to estimate variability
        sfr_ratio[valid_sfr] = np.maximum(SfrDisk[valid_sfr]/SfrBulge[valid_sfr], 
                                        SfrBulge[valid_sfr]/SfrDisk[valid_sfr])
    
    # Where only one component has SFR, set a moderate variability
    disk_only = (SfrDisk > 0) & (SfrBulge == 0)
    bulge_only = (SfrBulge > 0) & (SfrDisk == 0)
    if np.any(disk_only):
        sfr_ratio[disk_only] = 2.0  # Moderate variability for disk-only
    if np.any(bulge_only):
        sfr_ratio[bulge_only] = 3.0  # Higher variability for bulge-only (likely merger-induced)
    
    # Normalize and cap the ratio to prevent extreme values
    sfr_variability = np.minimum(sfr_ratio, 5.0) / 5.0
    
    # 2. Bulge-to-Total Ratio (indicator of merger/instability history)
    # Galaxies with more mergers/instabilities build up larger bulges
    bulge_to_total = np.zeros_like(StellarMass, dtype=np.float64)
    valid_mass = (StellarMass > 0)
    bulge_to_total[valid_mass] = np.minimum(BulgeMass[valid_mass] / StellarMass[valid_mass], 1.0)
    
    # 3. Recent Merger Component (if merger time info is available)
    recent_merger = np.zeros_like(StellarMass, dtype=np.float64)
    if TimeOfLastMajorMerger is not None and current_time is not None:
        # Avoid negative time since merger (can happen due to numerical issues)
        time_since_major = np.maximum(0.0, current_time - TimeOfLastMajorMerger)
        # Weight recent mergers more (exponential decay)
        recent_merger = np.exp(-time_since_major / 0.5)  # Half-life of 0.5 time units
    
    # 4. Quasar Mode BH Accretion (if available)
    # Indicates gas-rich mergers which also trigger starbursts
    bh_growth = np.zeros_like(StellarMass, dtype=np.float64)
    if QuasarModeBHaccretionMass is not None:
        # Normalize by stellar mass to get relative importance
        valid_mass = (StellarMass > 0)
        if np.any(valid_mass):
            bh_growth[valid_mass] = np.minimum(QuasarModeBHaccretionMass[valid_mass] / StellarMass[valid_mass], 1.0)
    
    # Combine metrics into a single burstiness parameter
    # Weights can be adjusted based on desired importance
    burstiness = (0.4 * sfr_variability + 
                 0.4 * bulge_to_total + 
                 0.1 * recent_merger +
                 0.1 * bh_growth)
    
    return burstiness, sfr_variability

def identify_starburst_galaxies(StellarMass, SfrDisk, SfrBulge, burstiness, percentile=90):
    """
    Identify starburst galaxies based on burstiness and SFR
    
    Parameters:
    -----------
    StellarMass : array_like
        Stellar mass of galaxies
    SfrDisk : array_like
        Star formation rate in disk
    SfrBulge : array_like
        Star formation rate in bulge
    burstiness : array_like
        Burstiness parameter
    percentile : float
        Percentile threshold for defining starbursts
        
    Returns:
    --------
    is_starburst : array_like (boolean)
        Boolean array identifying starburst galaxies
    """
    # Calculate specific SFR (SFR/M*)
    total_sfr = SfrDisk + SfrBulge
    ssfr = np.zeros_like(StellarMass, dtype=np.float64)
    valid = (StellarMass > 0)
    ssfr[valid] = total_sfr[valid] / StellarMass[valid]
    
    # Define thresholds for bursts
    valid_burst = (burstiness > 0)
    valid_ssfr = (ssfr > 0)
    
    if np.sum(valid_burst) > 0:
        burstiness_threshold = np.percentile(burstiness[valid_burst], percentile)
    else:
        burstiness_threshold = 0.0
        
    if np.sum(valid_ssfr) > 0:
        ssfr_threshold = np.percentile(ssfr[valid_ssfr], percentile)
    else:
        ssfr_threshold = 0.0
    
    # Identify starbursts (high burstiness OR high sSFR)
    # Changed to OR to capture more starburst galaxies for visualization
    is_starburst = (burstiness > burstiness_threshold) | (ssfr > ssfr_threshold)
    
    return is_starburst

def plot_burstiness_vs_mass(StellarMass, burstiness, starbursts, redshift, OutputDir, frame_num):
    """Plot burstiness vs stellar mass with starburst galaxies highlighted as stars"""
    plt.figure(figsize=(10, 8))
    
    # Get indices for a random subset of galaxies if needed
    n_gals = len(StellarMass)
    if n_gals > dilute:
        random_indices = np.random.choice(n_gals, dilute, replace=False)
    else:
        random_indices = np.arange(n_gals)
    
    # Get valid galaxies (positive mass and defined burstiness)
    valid = (StellarMass > 0) & (~np.isnan(burstiness))
    valid_indices = np.where(valid)[0]
    plot_indices = np.intersect1d(valid_indices, random_indices)
    
    # Plot regular galaxies
    not_starburst = ~starbursts
    regular_indices = np.intersect1d(plot_indices, np.where(not_starburst)[0])
    
    if len(regular_indices) > 0:
        plt.scatter(
            np.log10(StellarMass[regular_indices]), 
            burstiness[regular_indices], 
            s=5, alpha=0.5, c='blue', label='Regular galaxies'
        )
    
    # Highlight starburst galaxies as stars
    starburst_indices = np.intersect1d(plot_indices, np.where(starbursts)[0])
    
    if len(starburst_indices) > 0:
        plt.scatter(
            np.log10(StellarMass[starburst_indices]), 
            burstiness[starburst_indices], 
            s=100, alpha=0.8, c='red', marker='*', label='Starburst galaxies'
        )
    
    plt.xlabel(r'$\log_{10}(M_*/M_\odot)$')
    plt.ylabel('Burstiness parameter')
    plt.title(f'Galaxy Burstiness vs Stellar Mass (z={redshift:.2f})')
    plt.xlim(7, 12)  # Set consistent x-axis limits
    plt.ylim(0, 1)   # Set consistent y-axis limits
    plt.legend()
    plt.tight_layout()
    
    # Add frame number to filename for GIF creation
    output_file = f'{OutputDir}/frame_{frame_num:03d}_z{redshift:.2f}{OutputFormat}'
    plt.savefig(output_file)
    plt.close()
    
    return output_file

def create_gif_from_images(image_files, output_gif, duration=0.5):
    """Create a GIF from a list of image files"""
    print(f"Creating GIF from {len(image_files)} frames...")
    
    # Sort the image files to ensure proper sequence
    image_files.sort()
    
    # Read images and create GIF
    images = []
    for file in image_files:
        images.append(imageio.imread(file))
    
    # Save as GIF
    imageio.mimsave(output_gif, images, duration=duration)
    print(f"GIF created: {output_gif}")
    
    # Delete individual image files
    for file in image_files:
        try:
            os.remove(file)
            #print(f"Deleted: {file}")
        except:
            print(f"Could not delete: {file}")

# ==================================================================

if __name__ == '__main__':

    print('Running galaxy burstiness analysis\n')

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

    # Initialize burstiness arrays
    BurstinessFull = [0]*(LastSnap-FirstSnap+1)
    SFRvariabilityFull = [0]*(LastSnap-FirstSnap+1)
    StarburstGalaxies = [0]*(LastSnap-FirstSnap+1)
    
    # List to store image filenames for GIF creation
    gif_frames = []
    frame_count = 0

    # Process snapshots in reverse order (from high to low redshift)
    # This is because later snapshots (lower redshift) have more galaxies
    # We'll reverse the GIF frames later to show evolution from high to low redshift
    for snap_idx, snap in enumerate(reversed(gif_snapshots)):
        if snap < FirstSnap or snap > LastSnap:
            continue
            
        print(f"Processing snapshot {snap} (z={redshifts[snap]:.2f})...")
        Snapshot = 'Snap_'+str(snap)

        # Read standard galaxy properties
        StellarMassFull[snap] = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        SfrDiskFull[snap] = read_hdf(snap_num=Snapshot, param='SfrDisk')
        SfrBulgeFull[snap] = read_hdf(snap_num=Snapshot, param='SfrBulge')
        BlackHoleMassFull[snap] = read_hdf(snap_num=Snapshot, param='BlackHoleMass') * 1.0e10 / Hubble_h
        BulgeMassFull[snap] = read_hdf(snap_num=Snapshot, param='BulgeMass') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        
        try:
            cgmFull[snap] = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        except:
            print("  CGM gas information not available")

        # Try to read additional properties for burstiness calculation
        try:
            TimeOfLastMajorMerger = read_hdf(snap_num=Snapshot, param='TimeOfLastMajorMerger')
            TimeOfLastMinorMerger = read_hdf(snap_num=Snapshot, param='TimeOfLastMinorMerger')
            merger_info_available = True
        except:
            print("  Merger timing information not available")
            TimeOfLastMajorMerger = None
            TimeOfLastMinorMerger = None
            merger_info_available = False

        try:
            QuasarModeBHaccretionMass = read_hdf(snap_num=Snapshot, param='QuasarModeBHaccretionMass') * 1.0e10 / Hubble_h
            bh_info_available = True
        except:
            print("  Black hole accretion information not available")
            QuasarModeBHaccretionMass = None
            bh_info_available = False

        # Check what SFR history format is available
        sfr_history_format = check_sfr_history_availability(Snapshot)
        print(f"  SFR history format: {sfr_history_format}")
        
        # Calculate burstiness using available data
        print("  Calculating burstiness from galaxy properties...")
        current_time = lookback_time[0] if snap == 0 else lookback_time[min(snap, len(lookback_time)-1)]
        
        burstiness, sfr_variability = calculate_burstiness(
            StellarMassFull[snap],
            SfrDiskFull[snap],
            SfrBulgeFull[snap],
            BulgeMassFull[snap],
            TimeOfLastMajorMerger if merger_info_available else None,
            TimeOfLastMinorMerger if merger_info_available else None,
            QuasarModeBHaccretionMass if bh_info_available else None,
            current_time
        )
        
        # Store results
        BurstinessFull[snap] = burstiness
        SFRvariabilityFull[snap] = sfr_variability
        
        # Identify starburst galaxies
        StarburstGalaxies[snap] = identify_starburst_galaxies(
            StellarMassFull[snap],
            SfrDiskFull[snap],
            SfrBulgeFull[snap],
            burstiness,
            percentile=burstiness_percentile
        )
        
        # Create plot for this snapshot (for GIF)
        print(f"  Creating plot for z={redshifts[snap]:.2f}...")
        frame_file = plot_burstiness_vs_mass(
            StellarMassFull[snap], 
            burstiness, 
            StarburstGalaxies[snap], 
            redshifts[snap], 
            OutputDir,
            frame_count
        )
        gif_frames.append(frame_file)
        frame_count += 1
        
        # Report statistics
        n_starbursts = np.sum(StarburstGalaxies[snap])
        n_galaxies = len(StellarMassFull[snap])
        if n_galaxies > 0:
            print(f"  Found {n_starbursts} starburst galaxies ({n_starbursts/n_galaxies*100:.2f}%) at z={redshifts[snap]:.2f}")
        else:
            print(f"  No galaxies found at z={redshifts[snap]:.2f}")

    # Create GIF showing the evolution of burstiness
    print("\nCreating GIF animation of burstiness evolution...")
    create_gif_from_images(
        gif_frames,
        os.path.join(OutputDir, final_gif_name),
        duration=gif_duration
    )

    print("\nBurstiness analysis complete. Results saved to", OutputDir)