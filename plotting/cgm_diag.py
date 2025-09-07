#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
import os
from collections import defaultdict

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
VolumeFraction = 1.0  # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 10000       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

OutputFormat = '.pdf'
plt.style.use('/Users/mbradley/Documents/cohare_palatino_sty.mplstyle')

# Constants for r_cool calculation (matching your C code)
PROTONMASS = 1.67262178e-24  # g
BOLTZMANN = 1.3806485279e-16  # erg/K

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
                
            property.close()
        except Exception as e:
            print(f"Error reading {model_file}: {e}")
            continue
            
    return combined_data

def get_metaldependent_cooling_rate(log_temp, log_metallicity):
    """
    Simplified cooling function matching your C code
    This is a basic approximation - you may want to use your exact cooling tables
    """
    temp = 10**log_temp
    
    # Very basic cooling function - replace with your exact implementation
    if temp < 1e4:
        return 1e-26
    elif temp < 1e6:
        # Power law cooling in intermediate range
        lambda_cool = 1e-22 * (temp / 1e4)**(-0.7)
        # Metallicity enhancement (rough approximation)
        Z_solar = 0.02
        metallicity = 10**log_metallicity
        if metallicity > 0:
            metal_factor = 1 + 30 * (metallicity / Z_solar)
            lambda_cool *= metal_factor
    else:
        # High temperature cooling
        lambda_cool = 1e-22 * (100)**(-0.7) * (temp / 1e6)**(0.5)
    
    return lambda_cool

def calculate_rcool_to_rvir_ratio(vvir, rvir, hotgas, cgmgas, metals_hotgas, metals_cgmgas):
    """
    Calculate r_cool/R_vir ratio following your C code logic
    
    Parameters:
    -----------
    vvir : array - Virial velocity [km/s]
    rvir : array - Virial radius [kpc]  
    hotgas : array - Hot gas mass [Msun]
    cgmgas : array - CGM gas mass [Msun]
    metals_hotgas : array - Hot gas metals [Msun]
    metals_cgmgas : array - CGM gas metals [Msun]
    
    Returns:
    --------
    rcool_rvir : array - r_cool/R_vir ratio
    """
    
    # Check for valid halos
    valid = (vvir > 0) & (rvir > 0)
    rcool_rvir = np.full_like(vvir, 2.0)  # Default to CGM regime for invalid halos
    
    if not np.any(valid):
        return rcool_rvir
    
    # Calculate for valid halos only
    vvir_valid = vvir[valid]
    rvir_valid = rvir[valid]
    hotgas_valid = hotgas[valid]
    cgmgas_valid = cgmgas[valid]
    metals_hotgas_valid = metals_hotgas[valid]
    metals_cgmgas_valid = metals_cgmgas[valid]
    
    # --- UNIT CONVERSIONS ---
    # All input masses are in Msun, radii in kpc, velocities in km/s
    # Convert to cgs
    Msun_cgs = 1.98847e33  # g
    kpc_cgs = 3.08567758e21  # cm
    km_cgs = 1.0e5  # cm
    
    # Convert arrays to cgs
    rvir_cgs = rvir_valid * kpc_cgs
    vvir_cgs = vvir_valid * km_cgs
    hotgas_cgs = hotgas_valid * Msun_cgs
    cgmgas_cgs = cgmgas_valid * Msun_cgs
    total_gas_cgs = hotgas_cgs + cgmgas_cgs

    # Cooling time (s)
    tcool = rvir_cgs / vvir_cgs

    # Virial temperature in Kelvin
    temp = 35.9 * vvir_valid**2

    # Metallicity calculation (use whichever reservoir has gas)
    log_metallicity = np.full_like(temp, -10.0)  # Default very low metallicity
    hot_mask = (hotgas_valid > 0) & (metals_hotgas_valid > 0)
    if np.any(hot_mask):
        log_metallicity[hot_mask] = np.log10(metals_hotgas_valid[hot_mask] / hotgas_valid[hot_mask])
    cgm_mask = ~hot_mask & (cgmgas_valid > 0) & (metals_cgmgas_valid > 0)
    if np.any(cgm_mask):
        log_metallicity[cgm_mask] = np.log10(metals_cgmgas_valid[cgm_mask] / cgmgas_valid[cgm_mask])
    log_temp = np.log10(temp)
    lambda_cool = np.array([get_metaldependent_cooling_rate(lt, lz) for lt, lz in zip(log_temp, log_metallicity)])

    # Calculate critical density (cgs)
    x = PROTONMASS * BOLTZMANN * temp / lambda_cool
    rho_rcool = x / tcool * 0.885  # 0.885 = 3/2 * mu, mu=0.59

    # Handle zero gas or non-physical rho_rcool case
    no_gas_mask = (total_gas_cgs <= 0) | (rho_rcool <= 0) | (~np.isfinite(rho_rcool))
    rcool_rvir_valid = np.full_like(total_gas_cgs, 2.0)  # CGM regime for no gas or bad density
    gas_mask = ~no_gas_mask
    if np.any(gas_mask):
        # Isothermal sphere: density = M / (4 pi r^3)
        rho0 = total_gas_cgs[gas_mask] / (4.0 * np.pi * rvir_cgs[gas_mask]**3)
        # Cooling radius: where density = rho_rcool
        safe_rho_rcool = np.clip(rho_rcool[gas_mask], 1e-30, None)
        rcool = (total_gas_cgs[gas_mask] / (4.0 * np.pi * safe_rho_rcool))**(1.0/3.0)
        # Convert rcool from cm to kpc for ratio
        rcool_kpc = rcool / kpc_cgs
        rcool_rvir_valid[gas_mask] = rcool_kpc / rvir_valid[gas_mask]
    rcool_rvir[valid] = rcool_rvir_valid
    return rcool_rvir

def plot_rcool_rvir_analysis(mvir, rcool_rvir, hotgas, cgmgas, save_path=None):
    """Create r_cool/R_vir analysis plot"""
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
    
    # =================== TOP PANEL: Main r_cool/R_vir plot ===================
    
    # Identify regimes and problems
    cgm_regime = rcool_rvir > 1.0
    has_hotgas = hotgas > 1e-10  # Small threshold for numerical precision
    problematic = cgm_regime & has_hotgas
    
    normal_cgm = cgm_regime & ~has_hotgas
    normal_hot = ~cgm_regime
    
    # Plot regime boundaries
    ax1.axhline(y=1.0, color='red', linestyle='--', linewidth=2, 
                label='Regime Boundary (r_cool/R_vir = 1)')
    ax1.fill_between([1e9, 1e15], [1, 1], [100, 100], 
                     alpha=0.1, color='green', label='CGM Regime')
    ax1.fill_between([1e9, 1e15], [0.01, 0.01], [1, 1], 
                     alpha=0.1, color='blue', label='Hot Regime')
    
    # Plot galaxies by regime
    if np.any(normal_cgm):
        ax1.scatter(mvir[normal_cgm], rcool_rvir[normal_cgm], 
                   c='green', alpha=0.4, s=5, label=f'CGM Regime ({np.sum(normal_cgm)})', rasterized=True)
    
    if np.any(normal_hot):
        ax1.scatter(mvir[normal_hot], rcool_rvir[normal_hot], 
                   c='blue', alpha=0.4, s=5, label=f'Hot Regime ({np.sum(normal_hot)})', rasterized=True)
    
    # Highlight problematic galaxies
    if np.any(problematic):
        ax1.scatter(mvir[problematic], rcool_rvir[problematic], 
                   c='red', s=20, marker='x', linewidth=1.5, alpha=0.8,
                   label=f'⚠ PROBLEMATIC: CGM + HotGas ({np.sum(problematic)})', rasterized=True)
        
        print(f"\n🚨 FOUND {np.sum(problematic)} PROBLEMATIC GALAXIES! 🚨")
        print(f"   Out of {len(mvir)} total galaxies ({np.sum(problematic)/len(mvir)*100:.2f}%)")
        print(f"   Mass range: {mvir[problematic].min():.2e} - {mvir[problematic].max():.2e} M☉")
        print(f"   r_cool/R_vir range: {rcool_rvir[problematic].min():.3f} - {rcool_rvir[problematic].max():.3f}")
        print(f"   HotGas range: {hotgas[problematic].min():.2e} - {hotgas[problematic].max():.2e} M☉")
    else:
        print("✅ NO PROBLEMATIC GALAXIES FOUND!")
    
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.set_xlabel('Halo Mass [M☉]', fontsize=12)
    ax1.set_ylabel('r_cool/R_vir', fontsize=12)
    ax1.set_title('Cooling Radius vs Halo Mass: Your SAGE Data', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=9, loc='best')
    ax1.set_xlim(1e9, 1e15)
    ax1.set_ylim(0.01, 100)
    
    # Add mass scale annotations
    for mass in [1e10, 1e11, 1e12, 1e13, 1e14]:
        ax1.axvline(x=mass, color='gray', alpha=0.2, linestyle=':')
    
    # =================== BOTTOM PANEL: Mass distribution ===================
    
    log_mvir = np.log10(mvir)
    
    bins = np.linspace(9, 15, 60)
    
    if np.any(normal_cgm):
        ax2.hist(log_mvir[normal_cgm], bins=bins, alpha=0.6, color='green', 
                label=f'CGM Regime ({np.sum(normal_cgm)})', density=True)
    
    if np.any(normal_hot):
        ax2.hist(log_mvir[normal_hot], bins=bins, alpha=0.6, color='blue', 
                label=f'Hot Regime ({np.sum(normal_hot)})', density=True)
    
    if np.any(problematic):
        ax2.hist(log_mvir[problematic], bins=bins, alpha=0.9, color='red', 
                label=f'Problematic ({np.sum(problematic)})', density=True, 
                histtype='step', linewidth=3)
    
    ax2.axvline(x=12, color='red', linestyle='--', alpha=0.7, 
               label='~10¹² M☉ (typical transition)')
    
    ax2.set_xlabel('log₁₀(M_vir/M☉)', fontsize=12)
    ax2.set_ylabel('Normalized Count', fontsize=12)
    ax2.set_title('Halo Mass Distribution by Regime', fontsize=12, fontweight='bold')
    ax2.legend(fontsize=10)
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    if save_path:
        plt.savefig(save_path, dpi=300, bbox_inches='tight')
        print(f"\n📊 Figure saved to {save_path}")
    
    plt.show()
    
    return fig

def analyze_problematic_galaxies(mvir, rcool_rvir, hotgas, cgmgas, vvir):
    """Detailed analysis of problematic galaxies"""
    
    cgm_regime = rcool_rvir > 1.0
    has_hotgas = hotgas > 1e-10
    problematic = cgm_regime & has_hotgas
    
    if not np.any(problematic):
        print("✅ No problematic galaxies to analyze!")
        return
    
    print(f"\n🔍 DETAILED ANALYSIS OF {np.sum(problematic)} PROBLEMATIC GALAXIES:")
    print("=" * 60)
    
    # Mass analysis
    prob_masses = mvir[problematic]
    print(f"Mass Statistics:")
    print(f"  Range: {prob_masses.min():.2e} - {prob_masses.max():.2e} M☉")
    print(f"  Median: {np.median(prob_masses):.2e} M☉")
    print(f"  <10¹¹ M☉: {np.sum(prob_masses < 1e11)} ({np.sum(prob_masses < 1e11)/len(prob_masses)*100:.1f}%)")
    print(f"  10¹¹-10¹² M☉: {np.sum((prob_masses >= 1e11) & (prob_masses < 1e12))} ({np.sum((prob_masses >= 1e11) & (prob_masses < 1e12))/len(prob_masses)*100:.1f}%)")
    print(f"  >10¹² M☉: {np.sum(prob_masses >= 1e12)} ({np.sum(prob_masses >= 1e12)/len(prob_masses)*100:.1f}%)")
    
    # r_cool/R_vir analysis
    prob_rcool = rcool_rvir[problematic]
    print(f"\nr_cool/R_vir Statistics:")
    print(f"  Range: {prob_rcool.min():.3f} - {prob_rcool.max():.3f}")
    print(f"  Median: {np.median(prob_rcool):.3f}")
    print(f"  Close to boundary (1.0-1.5): {np.sum((prob_rcool >= 1.0) & (prob_rcool <= 1.5))} ({np.sum((prob_rcool >= 1.0) & (prob_rcool <= 1.5))/len(prob_rcool)*100:.1f}%)")
    
    # HotGas analysis
    prob_hotgas = hotgas[problematic]
    print(f"\nHotGas Statistics:")
    print(f"  Range: {prob_hotgas.min():.2e} - {prob_hotgas.max():.2e} M☉")
    print(f"  Median: {np.median(prob_hotgas):.2e} M☉")
    
    # Gas ratios
    prob_cgmgas = cgmgas[problematic]
    hot_frac = prob_hotgas / (prob_hotgas + prob_cgmgas + 1e-20)
    print(f"\nGas Composition:")
    print(f"  HotGas/(HotGas+CGMgas) ratio:")
    print(f"    Range: {hot_frac.min():.3f} - {hot_frac.max():.3f}")
    print(f"    Median: {np.median(hot_frac):.3f}")
    
    return problematic

# ==================================================================

if __name__ == '__main__':

    print('Running SAGE r_cool/R_vir Analysis\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): 
        os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    # Read all necessary data
    print("Loading galaxy data...")
    CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    cgm = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
    metalshotgas = read_hdf(snap_num = Snapshot, param = 'MetalsHotGas') * 1.0e10 / Hubble_h
    MetalsCGMgas = read_hdf(snap_num = Snapshot, param = 'MetalsCGMgas') * 1.0e10 / Hubble_h
    Vvir = read_hdf(snap_num = Snapshot, param = 'Vvir')
    Rvir = read_hdf(snap_num = Snapshot, param = 'Rvir')
    Type = read_hdf(snap_num = Snapshot, param = 'Type')
    Regime = read_hdf(snap_num = Snapshot, param = 'Regime')
    
    print(f"Loaded data for {len(Mvir)} galaxies")
    
    # Filter for central galaxies only (Type == 0)
    central_mask = (Type == 0)
    print(f"Filtering to {np.sum(central_mask)} central galaxies")
    
    # Apply filter
    mvir_central = Mvir[central_mask]
    hotgas_central = HotGas[central_mask]
    cgm_central = cgm[central_mask]
    metals_hotgas_central = metalshotgas[central_mask]
    metals_cgm_central = MetalsCGMgas[central_mask]
    vvir_central = Vvir[central_mask]
    rvir_central = Rvir[central_mask]
    
    # Calculate r_cool/R_vir for all central galaxies
    print("Calculating r_cool/R_vir ratios...")
    rcool_rvir = calculate_rcool_to_rvir_ratio(
        vvir_central, rvir_central, 
        hotgas_central, cgm_central,
        metals_hotgas_central, metals_cgm_central
    )

    print("r_cool/R_vir calculation complete!")
    print(f"  Range: {rcool_rvir.min():.3f} - {rcool_rvir.max():.3f}")
    print(f"  CGM regime (>1): {np.sum(rcool_rvir > 1.0)} galaxies ({np.sum(rcool_rvir > 1.0)/len(rcool_rvir)*100:.1f}%)")
    print(f"  Hot regime (≤1): {np.sum(rcool_rvir <= 1.0)} galaxies ({np.sum(rcool_rvir <= 1.0)/len(rcool_rvir)*100:.1f}%)")

    # Create the analysis plot
    print("\nCreating r_cool/R_vir analysis plot...")
    save_path = OutputDir + f'rcool_rvir_analysis_{Snapshot}{OutputFormat}'
    plot_rcool_rvir_analysis(mvir_central, rcool_rvir, hotgas_central, cgm_central, save_path)

    # Comparison plot using cached Regime from HDF5
    print("\nCreating cached Regime comparison plot...")
    regime_central = Regime[central_mask]
    # 0=CGM, 1=HOT
    fig, ax = plt.subplots(figsize=(12, 6))
    bins = np.linspace(9, 15, 60)
    ax.hist(np.log10(mvir_central[regime_central == 0]), bins=bins, alpha=0.6, color='green', label='CGM regime (cached)', density=True)
    ax.hist(np.log10(mvir_central[regime_central == 1]), bins=bins, alpha=0.6, color='blue', label='Hot regime (cached)', density=True)
    ax.set_xlabel('log₁₀(M_vir/M☉)', fontsize=12)
    ax.set_ylabel('Normalized Count', fontsize=12)
    ax.set_title('Halo Mass Distribution by Cached Regime', fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    cached_regime_path = OutputDir + f'cached_regime_hist_{Snapshot}{OutputFormat}'
    plt.savefig(cached_regime_path, dpi=300, bbox_inches='tight')
    print(f"📊 Cached Regime histogram saved to {cached_regime_path}")
    plt.show()

    # Detailed analysis of problematic galaxies
    problematic_mask = analyze_problematic_galaxies(
        mvir_central, rcool_rvir, hotgas_central, cgm_central, vvir_central
    )

    # Save problematic galaxy data for further investigation
    if problematic_mask is not None and np.any(problematic_mask):
        prob_data = pd.DataFrame({
            'Mvir': mvir_central[problematic_mask],
            'rcool_rvir': rcool_rvir[problematic_mask],
            'HotGas': hotgas_central[problematic_mask],
            'CGMgas': cgm_central[problematic_mask],
            'Vvir': vvir_central[problematic_mask],
            'Rvir': rvir_central[problematic_mask],
            'MetalsHotGas': metals_hotgas_central[problematic_mask],
            'MetalsCGMgas': metals_cgm_central[problematic_mask]
        })
        prob_filename = OutputDir + f'problematic_galaxies_{Snapshot}.csv'
        prob_data.to_csv(prob_filename, index=False)
        print(f"\n💾 Problematic galaxy data saved to: {prob_filename}")
        print("   Use this file to investigate the source of the bug!")

    print(f"\n🎯 Analysis complete! Check the plot at: {save_path}")

    # Final diagnosis
    print(f"\n🔧 DIAGNOSIS:")
    print("=" * 50)

    problematic_fraction = np.sum((rcool_rvir > 1.0) & (hotgas_central > 1e-10)) / len(rcool_rvir)

    if problematic_fraction > 0.1:
        print("❌ MAJOR ISSUE: >10% of galaxies are problematic!")
        print("   This suggests a fundamental bug in your C code regime system.")
        print("   Key things to check in your C code:")
        print("   1. Are you still calling calculate_rcool_to_rvir_ratio() instead of using cached Regime?")
        print("   2. Is final_regime_consistency_check() actually being called?")
        print("   3. Are mergers/feedback functions using cached regime values?")
        print("   4. Add debug prints to see what's happening!")

    elif problematic_fraction > 0.01:
        print("⚠️  MODERATE ISSUE: 1-10% problematic galaxies")
        print("   This could be a timing issue or boundary effect.")
        print("   Check if problematic galaxies are near the regime boundary.")

    elif problematic_fraction > 0.001:
        print("✅ MINOR ISSUE: <1% problematic galaxies")
        print("   This is likely just numerical precision or rare edge cases.")

    else:
        print("✅ EXCELLENT: No significant issues found!")

    # Check for unit conversion issues
    if np.any(rcool_rvir > 1000):
        print("\n⚠️  WARNING: Some r_cool/R_vir values > 1000")
        print("   This suggests unit conversion issues in the Python calculation.")
        print("   The actual issue in your C code might be different!")

    if np.all(rcool_rvir > 1.0):
        print("\n❌ CRITICAL: ALL galaxies are CGM regime!")
        print("   This is definitely wrong. Either:")
        print("   1. Unit conversions in Python are completely wrong")
        print("   2. Your C code has a fundamental physics bug")
        print("   3. Your simulation setup has unusual parameters")

    print(f"\n💡 NEXT STEPS:")
    print("1. Check if the Python r_cool values match expectations")
    print("2. Add debug output to your C code to see actual regime assignments") 
    print("3. Enable logging in final_regime_consistency_check()")
    print("4. Look for remaining calculate_rcool_to_rvir_ratio() calls in feedback")