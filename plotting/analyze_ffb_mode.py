#!/usr/bin/env python3
"""
Analyze and visualize the Feedback-Free Bursting (FFB) mode in SAGE output.

This script demonstrates that the FFB physics is working by:
1. Computing FFB fractions for galaxies based on their mass and redshift
2. Showing the relationship between mass, redshift, and FFB fraction
3. Visualizing how star formation is boosted in the FFB regime
"""

import numpy as np
import matplotlib.pyplot as plt
import h5py
import os
from pathlib import Path

# Set plotting style
plt.style.use('default')
plt.rcParams['figure.figsize'] = (12, 10)
plt.rcParams['font.size'] = 11


def calculate_ffb_threshold_mass(z):
    """
    Calculate the FFB threshold mass at redshift z.
    
    From Li et al. 2024, Equation (2):
    M_v,ffb / 10^10.8 M_sun ~ ((1+z)/10)^-6.2
    
    Returns mass in code units (10^10 Msun/h)
    """
    z_norm = (1.0 + z) / 10.0
    M_norm = 10.8  # log10(M_sun)
    z_exponent = -6.2
    
    # Calculate in absolute units
    log_Mvir_ffb_Msun = M_norm + z_exponent * np.log10(z_norm)
    
    # Convert to code units (10^10 Msun/h)
    log_Mvir_ffb_code = log_Mvir_ffb_Msun - 10.0
    
    return 10.0 ** log_Mvir_ffb_code


def calculate_ffb_fraction(Mvir, z):
    """
    Calculate the fraction of galaxies in FFB regime.
    
    Uses smooth sigmoid transition from Li et al. 2024.
    
    Parameters:
    -----------
    Mvir : array
        Virial mass in code units (10^10 Msun/h)
    z : array
        Redshift
    
    Returns:
    --------
    f_ffb : array
        FFB fraction (0 = not in FFB, 1 = fully in FFB)
    """
    # Calculate FFB threshold mass
    Mvir_ffb = calculate_ffb_threshold_mass(z)
    
    # Width of transition in dex (Li et al. use 0.15 dex)
    delta_log_M = 0.15
    
    # Calculate argument for sigmoid function
    x = np.log10(Mvir / Mvir_ffb) / delta_log_M
    
    # Sigmoid function: S(x) = 1 / (1 + exp(-x))
    # Smoothly varies from 0 (well above threshold) to 1 (well below threshold)
    f_ffb = 1.0 / (1.0 + np.exp(-x))
    
    return f_ffb


def load_sage_data(output_dir, snapnum=None):
    """
    Load SAGE galaxy data from HDF5 files.
    
    Parameters:
    -----------
    output_dir : str
        Path to SAGE output directory
    snapnum : int, optional
        Specific snapshot to load. If None, loads all snapshots.
    
    Returns:
    --------
    data : dict
        Dictionary containing galaxy properties
    """
    output_path = Path(output_dir)
    
    # Find all HDF5 files (excluding the master file)
    hdf5_files = [f for f in sorted(output_path.glob("model_*.hdf5")) 
                  if f.stem != "model"]
    
    if not hdf5_files:
        print(f"No HDF5 files found in {output_dir}")
        print(f"Looking for files matching: model_*.hdf5")
        print(f"Files in directory:")
        for f in output_path.glob("*.hdf5"):
            print(f"  {f.name}")
        return None
    
    print(f"Found {len(hdf5_files)} HDF5 files")
    
    # Initialize storage
    all_data = {
        'Mvir': [],
        'StellarMass': [],
        'Sfr': [],
        'Redshift': [],
        'SnapNum': [],
        'Type': []
    }
    
    # Load redshift information from master file
    master_file = output_path / "model.hdf5"
    redshifts = {}
    
    if master_file.exists():
        with h5py.File(master_file, 'r') as f:
            # Try to read redshift list
            if 'Redshifts' in f.attrs:
                z_list = f.attrs['Redshifts']
                for snap, z in enumerate(z_list):
                    redshifts[snap] = z
                print(f"Loaded {len(redshifts)} redshift values from master file")
    
    # Load data from files
    ngals_total = 0
    for hdf5_file in hdf5_files:
        try:
            with h5py.File(hdf5_file, 'r') as f:
                # Get snapshot number from filename
                file_num = int(hdf5_file.stem.split('_')[-1])
                
                # Find all snapshot groups in the file
                snap_keys = [k for k in f.keys() if k.startswith('Snap_')]
                
                for snap_key in snap_keys:
                    snap_num = int(snap_key.split('_')[1])
                    snap_group = f[snap_key]
                    
                    # Get redshift for this snapshot (try both cases)
                    if 'redshift' in snap_group.attrs:
                        z = snap_group.attrs['redshift']
                    elif 'Redshift' in snap_group.attrs:
                        z = snap_group.attrs['Redshift']
                    elif snap_num in redshifts:
                        z = redshifts[snap_num]
                    else:
                        print(f"Warning: No redshift found for {snap_key}, skipping")
                        continue
                    
                    # Load galaxy data
                    if 'Mvir' not in snap_group or 'StellarMass' not in snap_group:
                        continue
                        
                    mvir = snap_group['Mvir'][:]
                    stellar = snap_group['StellarMass'][:]
                    sfr = snap_group['Sfr'][:] if 'Sfr' in snap_group else np.zeros_like(mvir)
                    gtype = snap_group['Type'][:] if 'Type' in snap_group else np.zeros_like(mvir, dtype=int)
                    
                    # Filter out galaxies with zero mass
                    mask = (mvir > 0) & (stellar > 0)
                    ngals = np.sum(mask)
                    
                    if ngals > 0:
                        all_data['Mvir'].append(mvir[mask])
                        all_data['StellarMass'].append(stellar[mask])
                        all_data['Sfr'].append(sfr[mask])
                        all_data['Redshift'].append(np.full(ngals, z))
                        all_data['SnapNum'].append(np.full(ngals, snap_num))
                        all_data['Type'].append(gtype[mask])
                        ngals_total += ngals
                        
        except Exception as e:
            print(f"Error reading {hdf5_file.name}: {e}")
            continue
    
    if ngals_total == 0:
        print("No valid galaxy data found!")
        return None
    
    # Concatenate all data
    for key in all_data:
        all_data[key] = np.concatenate(all_data[key])
    
    print(f"Loaded {len(all_data['Mvir'])} galaxies from {len(hdf5_files)} files")
    print(f"Redshift range: {all_data['Redshift'].min():.2f} - {all_data['Redshift'].max():.2f}")
    
    return all_data


def plot_ffb_analysis(data, output_dir):
    """
    Create comprehensive plots showing FFB mode effects.
    """
    fig = plt.figure(figsize=(16, 12))
    
    # Select high-z galaxies for analysis
    z_bins = [(7, 8), (8, 9), (9, 10), (10, 12)]
    colors = ['blue', 'green', 'orange', 'red']
    
    # ========== Plot 1: FFB Threshold vs Redshift ==========
    ax1 = plt.subplot(3, 3, 1)
    z_range = np.linspace(0, 15, 100)
    Mvir_threshold = calculate_ffb_threshold_mass(z_range)
    
    ax1.semilogy(z_range, Mvir_threshold, 'k-', lw=2, label='FFB Threshold')
    ax1.axhline(1, color='gray', ls='--', alpha=0.5, label='10^10 Msun/h')
    ax1.set_xlabel('Redshift z')
    ax1.set_ylabel('Mvir threshold (10^10 Msun/h)')
    ax1.set_title('FFB Threshold vs Redshift')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    ax1.set_xlim(0, 15)
    
    # ========== Plot 2: FFB Fraction vs Mass Ratio ==========
    ax2 = plt.subplot(3, 3, 2)
    mass_ratio_range = np.logspace(-2, 1, 100)
    z_test = 10.0
    Mvir_test = mass_ratio_range * calculate_ffb_threshold_mass(z_test)
    f_ffb_curve = calculate_ffb_fraction(Mvir_test, z_test)
    
    ax2.semilogx(mass_ratio_range, f_ffb_curve, 'k-', lw=2)
    ax2.axvline(1, color='red', ls='--', alpha=0.5, label='Threshold')
    ax2.axhline(0.5, color='gray', ls=':', alpha=0.5)
    ax2.set_xlabel('Mvir / Mvir_threshold')
    ax2.set_ylabel('FFB Fraction')
    ax2.set_title('Sigmoid Transition Function')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    ax2.set_xlim(0.01, 10)
    ax2.set_ylim(-0.05, 1.05)
    
    # ========== Plot 3: Galaxy Mass Distribution ==========
    ax3 = plt.subplot(3, 3, 3)
    for (z_min, z_max), color in zip(z_bins, colors):
        mask = (data['Redshift'] >= z_min) & (data['Redshift'] < z_max)
        if np.sum(mask) > 0:
            masses = data['Mvir'][mask]
            ax3.hist(np.log10(masses), bins=30, alpha=0.5, color=color, 
                    label=f'z={z_min}-{z_max} (N={np.sum(mask)})')
    
    ax3.set_xlabel('log10(Mvir) [10^10 Msun/h]')
    ax3.set_ylabel('Number of Galaxies')
    ax3.set_title('Galaxy Mass Distribution')
    ax3.legend(fontsize=9)
    ax3.grid(True, alpha=0.3)
    
    # ========== Plots 4-7: FFB Fraction vs Mass for each z-bin ==========
    for idx, ((z_min, z_max), color) in enumerate(zip(z_bins, colors)):
        ax = plt.subplot(3, 3, 4 + idx)
        
        # Select galaxies in this redshift bin
        mask = (data['Redshift'] >= z_min) & (data['Redshift'] < z_max) & (data['Mvir'] > 0)
        
        if np.sum(mask) > 10:
            mvir = data['Mvir'][mask]
            z_mean = data['Redshift'][mask].mean()
            
            # Calculate FFB fractions
            f_ffb = calculate_ffb_fraction(mvir, data['Redshift'][mask])
            
            # Plot threshold
            M_thresh = calculate_ffb_threshold_mass(z_mean)
            ax.axvline(M_thresh, color='red', ls='--', lw=2, alpha=0.7, 
                      label=f'Threshold = {M_thresh:.2f}')
            
            # 2D histogram of mass vs FFB fraction
            mass_bins = np.logspace(np.log10(mvir.min()), np.log10(mvir.max()), 20)
            ffb_bins = np.linspace(0, 1, 20)
            
            H, xedges, yedges = np.histogram2d(mvir, f_ffb, bins=[mass_bins, ffb_bins])
            X, Y = np.meshgrid(xedges, yedges)
            
            mesh = ax.pcolormesh(X, Y, H.T, cmap='viridis', alpha=0.6)
            
            # Overlay scatter plot
            scatter_mask = np.random.choice(len(mvir), min(500, len(mvir)), replace=False)
            ax.scatter(mvir[scatter_mask], f_ffb[scatter_mask], 
                      c=color, s=10, alpha=0.3, edgecolors='none')
            
            ax.set_xlabel('Mvir (10^10 Msun/h)')
            ax.set_ylabel('FFB Fraction')
            ax.set_title(f'z = {z_min}-{z_max} (z_mean={z_mean:.2f})')
            ax.set_xscale('log')
            ax.legend(fontsize=8)
            ax.grid(True, alpha=0.3)
            ax.set_ylim(-0.05, 1.05)
        else:
            ax.text(0.5, 0.5, f'Not enough data\n(N={np.sum(mask)})', 
                   ha='center', va='center', transform=ax.transAxes)
            ax.set_title(f'z = {z_min}-{z_max}')
    
    # ========== Plot 8: SFR Enhancement ==========
    ax8 = plt.subplot(3, 3, 8)
    
    # Use high-z galaxies with star formation
    mask = (data['Redshift'] > 7) & (data['Sfr'] > 0) & (data['Mvir'] > 0)
    
    if np.sum(mask) > 100:
        mvir = data['Mvir'][mask]
        sfr = data['Sfr'][mask]
        stellar = data['StellarMass'][mask]
        z = data['Redshift'][mask]
        
        # Calculate FFB fractions
        f_ffb = calculate_ffb_fraction(mvir, z)
        
        # Calculate specific SFR
        ssfr = sfr / stellar
        
        # Bin by FFB fraction
        ffb_bin_edges = np.linspace(0, 1, 11)
        ffb_bin_centers = 0.5 * (ffb_bin_edges[1:] + ffb_bin_edges[:-1])
        
        ssfr_median = []
        ssfr_16 = []
        ssfr_84 = []
        
        for i in range(len(ffb_bin_edges) - 1):
            bin_mask = (f_ffb >= ffb_bin_edges[i]) & (f_ffb < ffb_bin_edges[i+1])
            if np.sum(bin_mask) > 5:
                ssfr_median.append(np.median(np.log10(ssfr[bin_mask])))
                ssfr_16.append(np.percentile(np.log10(ssfr[bin_mask]), 16))
                ssfr_84.append(np.percentile(np.log10(ssfr[bin_mask]), 84))
            else:
                ssfr_median.append(np.nan)
                ssfr_16.append(np.nan)
                ssfr_84.append(np.nan)
        
        ssfr_median = np.array(ssfr_median)
        ssfr_16 = np.array(ssfr_16)
        ssfr_84 = np.array(ssfr_84)
        
        valid = ~np.isnan(ssfr_median)
        ax8.plot(ffb_bin_centers[valid], ssfr_median[valid], 'o-', color='darkblue', 
                lw=2, markersize=8, label='Median sSFR')
        ax8.fill_between(ffb_bin_centers[valid], ssfr_16[valid], ssfr_84[valid], 
                         alpha=0.3, color='darkblue', label='16-84 percentile')
        
        ax8.set_xlabel('FFB Fraction')
        ax8.set_ylabel('log10(sSFR) [1/yr]')
        ax8.set_title('sSFR vs FFB Fraction (z>7)')
        ax8.legend()
        ax8.grid(True, alpha=0.3)
        ax8.set_xlim(-0.05, 1.05)
    
    # ========== Plot 9: Statistics Summary ==========
    ax9 = plt.subplot(3, 3, 9)
    ax9.axis('off')
    
    # Calculate statistics
    high_z_mask = data['Redshift'] > 7
    if np.sum(high_z_mask) > 0:
        f_ffb_all = calculate_ffb_fraction(data['Mvir'][high_z_mask], 
                                           data['Redshift'][high_z_mask])
        
        # Count galaxies in different FFB regimes
        n_total = np.sum(high_z_mask)
        n_full_ffb = np.sum(f_ffb_all > 0.9)
        n_partial_ffb = np.sum((f_ffb_all > 0.1) & (f_ffb_all <= 0.9))
        n_no_ffb = np.sum(f_ffb_all <= 0.1)
        
        stats_text = f"""FFB Mode Statistics (z > 7):
        
Total galaxies: {n_total}

FFB Regime Breakdown:
  Full FFB (f > 0.9):     {n_full_ffb:5d} ({100*n_full_ffb/n_total:5.1f}%)
  Partial FFB (0.1-0.9):  {n_partial_ffb:5d} ({100*n_partial_ffb/n_total:5.1f}%)
  No FFB (f < 0.1):       {n_no_ffb:5d} ({100*n_no_ffb/n_total:5.1f}%)

Average FFB fraction: {f_ffb_all.mean():.4f}
Median FFB fraction:  {np.median(f_ffb_all):.4f}

Max boost factor: {1 + f_ffb_all.max() * (6.67 - 1):.2f}x
        """
        
        ax9.text(0.1, 0.9, stats_text, transform=ax9.transAxes, 
                fontsize=10, verticalalignment='top', family='monospace',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    
    plt.tight_layout()
    
    # Save figure
    output_path = Path(output_dir) / 'plots'
    output_path.mkdir(exist_ok=True)
    
    output_file = output_path / 'ffb_mode_analysis.png'
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\nSaved FFB analysis plot to: {output_file}")
    
    plt.show()


def main():
    """Main analysis function."""
    # Configuration
    output_dir = "./output/millennium/"
    
    if not os.path.exists(output_dir):
        print(f"Error: Output directory not found: {output_dir}")
        print("Please run SAGE first to generate output data.")
        return
    
    print("=" * 60)
    print("FFB Mode Analysis for SAGE")
    print("=" * 60)
    
    # Load data
    print("\nLoading SAGE output data...")
    data = load_sage_data(output_dir)
    
    if data is None:
        print("Failed to load data. Exiting.")
        return
    
    # Create analysis plots
    print("\nGenerating FFB analysis plots...")
    plot_ffb_analysis(data, output_dir)
    
    print("\n" + "=" * 60)
    print("Analysis complete!")
    print("=" * 60)


if __name__ == "__main__":
    main()
