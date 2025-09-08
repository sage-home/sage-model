#!/usr/bin/env python3
"""
Comprehensive diagnostic script for the new mass-regime aware CGM physics
Analyzes gas distribution and regime consistency at z=0
"""

import os
import numpy as np
import matplotlib.pyplot as plt
import h5py as h5
from random import seed

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'  # z=0

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model

# Mass thresholds for regime classification
MASS_THRESHOLD = 1e12  # M_sun - threshold for CGM vs Hot regime

# Plotting options
plt.style.use('default')  # Use default style for now
plt.rcParams["figure.figsize"] = (12, 10)
plt.rcParams["figure.dpi"] = 150
plt.rcParams["font.size"] = 12

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

def analyze_mass_regime():
    """Main analysis function for mass-regime aware CGM physics"""
    
    print('=' * 60)
    print('PHYSICS-BASED REGIME CGM DIAGNOSTIC')
    print('=' * 60)
    print(f'Analyzing snapshot: {Snapshot} (z=0)')
    print('Using physics-based regime determination (rcool/Rvir ratio)')
    print()
    
    # Read galaxy properties
    print('Reading galaxy properties...')
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    Vvir = read_hdf(snap_num=Snapshot, param='Vvir')
    Rvir = read_hdf(snap_num=Snapshot, param='Rvir')
    SfrDisk = read_hdf(snap_num=Snapshot, param='SfrDisk')
    SfrBulge = read_hdf(snap_num=Snapshot, param='SfrBulge')
    Cooling = read_hdf(snap_num=Snapshot, param='Cooling')
    Regime = read_hdf(snap_num=Snapshot, param='Regime')
    RcoolToRvir = read_hdf(snap_num=Snapshot, param='RcoolToRvir')
    Tvir = 35.9 * Vvir * Vvir  # Virial temperature in K
    TmaxTvir = 2.5e5 / Tvir  # Threshold ratio for regime determination
    
    print(f'Total galaxies loaded: {len(Mvir):,}')
    
    # Filter for central galaxies with reasonable masses
    mask = (Type == 0) & (Mvir > 1e9) & (Vvir > 0)
    
    Mvir_c = Mvir[mask]
    StellarMass_c = StellarMass[mask]
    ColdGas_c = ColdGas[mask]
    HotGas_c = HotGas[mask]
    CGMgas_c = CGMgas[mask]
    Vvir_c = Vvir[mask]
    Rvir_c = Rvir[mask]
    SfrDisk_c = SfrDisk[mask]
    SfrBulge_c = SfrBulge[mask]
    Cooling_c = Cooling[mask]
    Regime_c = Regime[mask]
    RcoolToRvir_c = RcoolToRvir[mask]
    Tvir_c = Tvir[mask]
    TmaxTvir_c = TmaxTvir[mask]

    print(f'Central galaxies analyzed: {len(Mvir_c):,}')
    print(Regime_c)
    
    # Calculate derived properties
    TotalSfr = SfrDisk_c + SfrBulge_c
    TotalGas = HotGas_c + CGMgas_c + ColdGas_c
    Tvir = Tvir_c  # Virial temperature in K

    # Physics-based regime classification
    cgm_regime = (Regime_c == 0)  # CGM regime: rcool > Rvir
    hot_regime = (Regime_c == 1)  # HOT regime: rcool < Rvir
    
    # Also keep mass-based for comparison
    low_mass = Mvir_c < MASS_THRESHOLD
    high_mass = Mvir_c >= MASS_THRESHOLD
    
    # Gas content classification
    has_hotgas = HotGas_c > 1.0e-10  # Significant HotGas (>1e6 M_sun)
    has_cgmgas = CGMgas_c > 1.0e-10  # Significant CGMgas (>1e6 M_sun)
    has_both_gas = has_hotgas & has_cgmgas
    has_no_gas = (~has_hotgas) & (~has_cgmgas)
    
    # Regime compliance check (physics-based)
    cgm_compliant = cgm_regime & (~has_hotgas)  # CGM regime should have no HotGas
    hot_compliant = hot_regime & (~has_cgmgas)  # HOT regime should have no CGMgas
    
    # Cross-contamination
    cgm_contaminated = cgm_regime & has_hotgas  # CGM regime with HotGas (violation)
    hot_contaminated = hot_regime & has_cgmgas  # HOT regime with CGMgas (violation)

    # Check for ANY non-zero contamination, even tiny amounts
    tiny_cgm_violations = cgm_regime & (HotGas_c > 0) & (HotGas_c <= 1.0e-10)
    tiny_hot_violations = hot_regime & (CGMgas_c > 0) & (CGMgas_c <= 1.0e-10)

    print(f"CGM regime with tiny HotGas (≤1e-10): {np.sum(tiny_cgm_violations)} galaxies")
    print(f"HOT regime with tiny CGMgas (≤1e-10): {np.sum(tiny_hot_violations)} galaxies")

    if np.sum(tiny_cgm_violations) > 0:
        print(f"  HotGas range: {np.min(HotGas_c[tiny_cgm_violations]):.2e} to {np.max(HotGas_c[tiny_cgm_violations]):.2e}")

    if np.sum(tiny_hot_violations) > 0:
        print(f"  CGMgas range: {np.min(CGMgas_c[tiny_hot_violations]):.2e} to {np.max(CGMgas_c[tiny_hot_violations]):.2e}")
    
    print_physics_diagnostics(Mvir_c, cgm_regime, hot_regime, low_mass, high_mass, 
                             has_hotgas, has_cgmgas, has_both_gas, has_no_gas, 
                             cgm_compliant, hot_compliant, cgm_contaminated, hot_contaminated)
    
    create_diagnostic_plots(Mvir_c, StellarMass_c, HotGas_c, CGMgas_c, ColdGas_c, 
                           TotalSfr, Tvir, cgm_regime, hot_regime, has_hotgas, has_cgmgas,
                           cgm_compliant, hot_compliant)
    
    print('SAMPLE of r_cool/Rvir values:')
    print(RcoolToRvir_c)
    print(f"  Min: {np.min(RcoolToRvir_c):.2e}, Max: {np.max(RcoolToRvir_c):.2e}")

    create_rcool_diagnostic_plot(Mvir_c, HotGas_c, CGMgas_c, Regime_c, RcoolToRvir_c)
    create_regime_colored_plot(Mvir_c, HotGas_c, CGMgas_c, Regime_c, RcoolToRvir_c)
    create_single_regime_plot(Mvir_c, HotGas_c, CGMgas_c, Regime_c, RcoolToRvir_c)

    # In your plotting function, before creating scatter plots:
    print(f"Galaxies with any HotGas > 0: {np.sum(HotGas > 0)}")
    print(f"Galaxies with any CGMgas > 0: {np.sum(CGMgas > 0)}")
    print(f"CGM regime galaxies with HotGas > 0: {np.sum((Regime_c == 0) & (HotGas_c > 0))}")
    print(f"HOT regime galaxies with CGMgas > 0: {np.sum((Regime_c == 1) & (CGMgas_c > 0))}")
    
    return {
        'total_galaxies': len(Mvir_c),
        'cgm_regime_count': np.sum(cgm_regime),
        'hot_regime_count': np.sum(hot_regime),
        'cgm_compliant': np.sum(cgm_compliant),
        'hot_compliant': np.sum(hot_compliant),
        'cgm_contaminated': np.sum(cgm_contaminated),
        'hot_contaminated': np.sum(hot_contaminated),
        'compliance_rate_cgm': np.sum(cgm_compliant) / np.sum(cgm_regime) * 100 if np.sum(cgm_regime) > 0 else 0,
        'compliance_rate_hot': np.sum(hot_compliant) / np.sum(hot_regime) * 100 if np.sum(hot_regime) > 0 else 0
    }

def print_physics_diagnostics(Mvir, cgm_regime, hot_regime, low_mass, high_mass, 
                            has_hotgas, has_cgmgas, has_both_gas, has_no_gas, 
                            cgm_compliant, hot_compliant, cgm_contaminated, hot_contaminated):
    """Print detailed diagnostic information based on physics regime"""
    
    print('PHYSICS-BASED REGIME DISTRIBUTION:')
    print(f'  CGM regime (rcool > Rvir): {np.sum(cgm_regime):,} ({100*np.sum(cgm_regime)/len(Mvir):.1f}%)')
    print(f'  HOT regime (rcool < Rvir): {np.sum(hot_regime):,} ({100*np.sum(hot_regime)/len(Mvir):.1f}%)')
    print()
    
    print('MASS DISTRIBUTION BY REGIME:')
    if np.sum(cgm_regime) > 0:
        cgm_mvir = Mvir[cgm_regime]
        print(f'  CGM regime Mvir range: {cgm_mvir.min():.2e} to {cgm_mvir.max():.2e} M☉')
    if np.sum(hot_regime) > 0:
        hot_mvir = Mvir[hot_regime]
        print(f'  HOT regime Mvir range: {hot_mvir.min():.2e} to {hot_mvir.max():.2e} M☉')
    print()
    
    print('GAS CONTENT DISTRIBUTION:')
    print(f'  Galaxies with significant HotGas: {np.sum(has_hotgas):,} ({100*np.sum(has_hotgas)/len(Mvir):.1f}%)')
    print(f'  Galaxies with significant CGMgas: {np.sum(has_cgmgas):,} ({100*np.sum(has_cgmgas)/len(Mvir):.1f}%)')
    print(f'  Galaxies with both gas types: {np.sum(has_both_gas):,} ({100*np.sum(has_both_gas)/len(Mvir):.1f}%)')
    print(f'  Galaxies with no significant gas: {np.sum(has_no_gas):,} ({100*np.sum(has_no_gas)/len(Mvir):.1f}%)')
    print()
    
    print('REGIME COMPLIANCE (Physics-based):')
    print(f'  CGM regime galaxies (should have no HotGas):')
    print(f'    Total: {np.sum(cgm_regime):,}')
    print(f'    Compliant: {np.sum(cgm_compliant):,} ({100*np.sum(cgm_compliant)/np.sum(cgm_regime):.1f}%)')
    print(f'    Contaminated: {np.sum(cgm_contaminated):,} ({100*np.sum(cgm_contaminated)/np.sum(cgm_regime):.1f}%)')
    print()
    print(f'  HOT regime galaxies (should have no CGMgas):')
    print(f'    Total: {np.sum(hot_regime):,}')
    print(f'    Compliant: {np.sum(hot_compliant):,} ({100*np.sum(hot_compliant)/np.sum(hot_regime):.1f}%)')
    print(f'    Contaminated: {np.sum(hot_contaminated):,} ({100*np.sum(hot_contaminated)/np.sum(hot_regime):.1f}%)')
    print()
    
    # Show mass-based vs physics-based disagreement
    physics_hot_mass_low = hot_regime & low_mass
    physics_cgm_mass_high = cgm_regime & high_mass
    
    if np.sum(physics_hot_mass_low) > 0:
        print(f'  Low-mass galaxies in HOT regime: {np.sum(physics_hot_mass_low):,}')
        print(f'    (These are correctly in HOT regime despite low mass)')
    
    if np.sum(physics_cgm_mass_high) > 0:
        print(f'  High-mass galaxies in CGM regime: {np.sum(physics_cgm_mass_high):,}')
        print(f'    (These are correctly in CGM regime despite high mass)')
    
    # Detailed breakdown of actual contamination
    if np.sum(cgm_contaminated) > 0:
        problem_mvir = Mvir[cgm_contaminated]
        print(f'  ACTUAL CONTAMINATION - CGM regime with HotGas:')
        print(f'    Mvir range: {problem_mvir.min():.2e} to {problem_mvir.max():.2e} M☉')
    
    if np.sum(hot_contaminated) > 0:
        problem_mvir = Mvir[hot_contaminated]
        print(f'  ACTUAL CONTAMINATION - HOT regime with CGMgas:')
        print(f'    Mvir range: {problem_mvir.min():.2e} to {problem_mvir.max():.2e} M☉')

def print_diagnostics(Mvir, low_mass, high_mass, has_hotgas, has_cgmgas, 
                     has_both_gas, has_no_gas, low_mass_compliant, high_mass_compliant):
    """Print detailed diagnostic information - DEPRECATED: Use print_physics_diagnostics"""
    
    print('MASS DISTRIBUTION:')
    print(f'  Low mass (<{MASS_THRESHOLD:.0e} M☉): {np.sum(low_mass):,} ({100*np.sum(low_mass)/len(Mvir):.1f}%)')
    print(f'  High mass (≥{MASS_THRESHOLD:.0e} M☉): {np.sum(high_mass):,} ({100*np.sum(high_mass)/len(Mvir):.1f}%)')
    print()
    
    print('GAS CONTENT DISTRIBUTION:')
    print(f'  Galaxies with significant HotGas: {np.sum(has_hotgas):,} ({100*np.sum(has_hotgas)/len(Mvir):.1f}%)')
    print(f'  Galaxies with significant CGMgas: {np.sum(has_cgmgas):,} ({100*np.sum(has_cgmgas)/len(Mvir):.1f}%)')
    print(f'  Galaxies with both gas types: {np.sum(has_both_gas):,} ({100*np.sum(has_both_gas)/len(Mvir):.1f}%)')
    print(f'  Galaxies with no significant gas: {np.sum(has_no_gas):,} ({100*np.sum(has_no_gas)/len(Mvir):.1f}%)')
    print()
    
    print('REGIME COMPLIANCE:')
    print(f'  Low mass galaxies (should have no HotGas):')
    print(f'    Total: {np.sum(low_mass):,}')
    print(f'    Compliant: {np.sum(low_mass_compliant):,} ({100*np.sum(low_mass_compliant)/np.sum(low_mass):.1f}%)')
    print(f'    Non-compliant: {np.sum(low_mass & has_hotgas):,} ({100*np.sum(low_mass & has_hotgas)/np.sum(low_mass):.1f}%)')
    print()
    print(f'  High mass galaxies (should have no CGMgas):')
    print(f'    Total: {np.sum(high_mass):,}')
    print(f'    Compliant: {np.sum(high_mass_compliant):,} ({100*np.sum(high_mass_compliant)/np.sum(high_mass):.1f}%)')
    print(f'    Non-compliant: {np.sum(high_mass & has_cgmgas):,} ({100*np.sum(high_mass & has_cgmgas)/np.sum(high_mass):.1f}%)')
    print()
    
    # Detailed breakdown of problem cases
    if np.sum(low_mass & has_hotgas) > 0:
        problem_mvir = Mvir[low_mass & has_hotgas]
        print(f'  Problem low-mass galaxies (with HotGas):')
        print(f'    Mvir range: {problem_mvir.min():.2e} to {problem_mvir.max():.2e} M☉')
    
    if np.sum(high_mass & has_cgmgas) > 0:
        problem_mvir = Mvir[high_mass & has_cgmgas]
        print(f'  Problem high-mass galaxies (with CGMgas):')
        print(f'    Mvir range: {problem_mvir.min():.2e} to {problem_mvir.max():.2e} M☉')

# Add this to your diagnostic script to create the key figure

def create_rcool_diagnostic_plot(Mvir, HotGas, CGMgas, Regime_c, rcool_to_rvir,OutputDir='./output/millennium/plots/'):
    """Create diagnostic plot showing r_cool/R_vir vs mass with regime coloring"""
    
    # Calculate r_cool/R_vir for each galaxy
    # You'll need to read these from your HDF5 file or calculate them
    # For now, let's approximate based on regime assignments
    # rcool_to_rvir = np.ones(len(Mvir))  # Placeholder
    
    # If you have the actual r_cool/R_vir values in your output, read them:
    # rcool_to_rvir = read_hdf(snap_num=Snapshot, param='RcoolToRvir') 
    
    # For demonstration, let's create approximate values based on regime
    cgm_regime = (Regime_c == 0)
    hot_regime = (Regime_c == 1)
    
    # Create the diagnostic figure
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    # Plot 1: r_cool/R_vir vs Mass, colored by regime
    ax1.scatter(Mvir[cgm_regime], rcool_to_rvir[cgm_regime], 
               c='blue', alpha=0.6, s=12, label='CGM regime (r_cool > R_vir)', 
               edgecolors='none')
    ax1.scatter(Mvir[hot_regime], rcool_to_rvir[hot_regime], 
               c='red', alpha=0.6, s=12, label='HOT regime (r_cool < R_vir)', 
               edgecolors='none')
    
    # Add the critical boundary
    ax1.axhline(y=1.0, color='black', linestyle='--', linewidth=2, 
               label='r_cool = R_vir (regime boundary)')
    ax1.axvline(x=1e12, color='gray', linestyle=':', alpha=0.7, 
               label='1e12 M☉ (mass threshold)')
    
    ax1.set_xscale('log')
    ax1.set_yscale('log')
    ax1.set_xlabel('Halo Mass [M☉]')
    ax1.set_ylabel('r_cool / R_vir')
    ax1.set_title('Regime Determination: r_cool/R_vir vs Halo Mass')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0.1, 10)
    
    # Plot 2: Same plot but colored by gas content
    has_hotgas = HotGas > 1e-10
    has_cgmgas = CGMgas > 1e-10
    has_no_gas = (~has_hotgas) & (~has_cgmgas)
    
    ax2.scatter(Mvir[has_hotgas], rcool_to_rvir[has_hotgas], 
               c='red', alpha=0.6, s=12, label=f'Has HotGas ({np.sum(has_hotgas)})', 
               edgecolors='none')
    ax2.scatter(Mvir[has_cgmgas], rcool_to_rvir[has_cgmgas], 
               c='blue', alpha=0.6, s=12, label=f'Has CGMgas ({np.sum(has_cgmgas)})', 
               edgecolors='none')
    ax2.scatter(Mvir[has_no_gas], rcool_to_rvir[has_no_gas], 
               c='gray', alpha=0.3, s=8, label=f'No significant gas ({np.sum(has_no_gas)})', 
               edgecolors='none')
    
    ax2.axhline(y=1.0, color='black', linestyle='--', linewidth=2, 
               label='r_cool = R_vir')
    ax2.axvline(x=1e12, color='gray', linestyle=':', alpha=0.7, 
               label='1e12 M☉')
    
    ax2.set_xscale('log')
    ax2.set_yscale('log')
    ax2.set_xlabel('Halo Mass [M☉]')
    ax2.set_ylabel('r_cool / R_vir')
    ax2.set_title('Gas Content vs r_cool/R_vir')
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0.1, 10)
    
    plt.tight_layout()
    
    # Add annotations to explain the physics
    ax1.text(0.05, 0.95, 
             'CGM regime: r_cool > R_vir\n→ Cold accretion\n→ CGMgas reservoir', 
             transform=ax1.transAxes, fontsize=10, va='top',
             bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))
    
    ax1.text(0.05, 0.45, 
             'HOT regime: r_cool < R_vir\n→ Virial shock heating\n→ HotGas reservoir', 
             transform=ax1.transAxes, fontsize=10, va='top',
             bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.8))
    
    plt.savefig(OutputDir + 'rcool_regime_diagnostic.pdf', dpi=300, bbox_inches='tight')
    plt.savefig(OutputDir + 'rcool_regime_diagnostic.png', dpi=300, bbox_inches='tight')
    print(f'r_cool diagnostic plots saved to {OutputDir}rcool_regime_diagnostic.[pdf/png]')
    
    # Print key statistics
    low_mass = Mvir < 1e12
    high_mass = Mvir >= 1e12
    
    print(f"\nKEY PHYSICS INSIGHTS:")

    n_low_hot = np.sum(low_mass & hot_regime)
    print(f"Low-mass haloes in HOT regime: {n_low_hot:,}")
    print(f"  These have r_cool < R_vir despite low mass")
    subset = Mvir[low_mass & hot_regime]
    if subset.size > 0:
        print(f"  Mass range: {subset.min():.2e} to {subset.max():.2e} M☉")
    else:
        print("  Mass range: N/A (no low-mass HOT regime haloes)")
    
    print(f"\nHigh-mass haloes in CGM regime: {np.sum(high_mass & cgm_regime):,}")
    print(f"  These have r_cool > R_vir despite high mass")
    if np.sum(high_mass & cgm_regime) > 0:
        print(f"  Mass range: {Mvir[high_mass & cgm_regime].min():.2e} to {Mvir[high_mass & cgm_regime].max():.2e} M☉")

def create_regime_colored_plot(Mvir, HotGas, CGMgas, Regime_c, rcool_to_rvir, OutputDir='./output/millennium/plots/'):
    """Create r_cool/R_vir vs mass plot colored by regime (like Keres et al.)"""
    
    cgm_regime = (Regime_c == 0)
    hot_regime = (Regime_c == 1)

    print(f"CGM regime count: {np.sum(cgm_regime)}")
    print(f"CGM regime Mvir range: {Mvir[cgm_regime].min() if np.sum(cgm_regime) else 'N/A'} to {Mvir[cgm_regime].max() if np.sum(cgm_regime) else 'N/A'}")
    print(f"CGM regime rcool_to_rvir range: {rcool_to_rvir[cgm_regime].min() if np.sum(cgm_regime) else 'N/A'} to {rcool_to_rvir[cgm_regime].max() if np.sum(cgm_regime) else 'N/A'}")
    
    # Create the figure in Keres-style layout
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8))
    
    # Left panel: CGM regime systems (blue)
    ax1.scatter(Mvir[cgm_regime], rcool_to_rvir[cgm_regime], 
               c='blue', alpha=0.6, s=8, edgecolors='none')
    
    # Add regime boundary and mass threshold
    ax1.axhline(y=1.0, color='black', linestyle='--', linewidth=2)
    ax1.axvline(x=1e12, color='gray', linestyle=':', alpha=0.7)
    
    # Add text annotations like Keres
    ax1.text(0.5, 0.85, 'systems in the\n"rapid cooling"\ninfall regime', 
             transform=ax1.transAxes, fontsize=12, ha='center',
             bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))
    
    ax1.set_xlim(1e10, 5e11)
    ax1.set_ylim(0.8, 7.0)
    ax1.set_xscale('log')
    ax1.set_xlabel('M_vir / M☉')
    ax1.set_ylabel('r_cool / R_vir')
    ax1.grid(True, alpha=0.3)
    
    # Right panel: HOT regime systems (red)  
    ax2.scatter(Mvir[hot_regime], rcool_to_rvir[hot_regime], 
               c='red', alpha=0.6, s=8, edgecolors='none')
    
    # Add regime boundary and mass threshold
    ax2.axhline(y=1.0, color='black', linestyle='--', linewidth=2)
    ax2.axvline(x=1e12, color='gray', linestyle=':', alpha=0.7)
    
    # Add text annotations
    ax2.text(0.5, 0.85, 'systems that have\nformed a static\nhot halo', 
             transform=ax2.transAxes, fontsize=12, ha='center',
             bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.8))
    
    ax2.set_xlim(5e10, 1e15)
    ax2.set_ylim(0, 1.0)
    ax2.set_xscale('log')
    ax2.set_xlabel('M_vir / M☉')
    ax2.set_ylabel('r_cool / R_vir')
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # Add figure caption
    fig.text(0.5, 0.02, 
             'Figure: The ratio of cooling radius to virial radius for simulated systems at z=0 plotted against their dark\n'
             'matter virial mass. Systems in the "rapid cooling" regime are shown in the left panel, while those that have\n'
             'formed a static hot halo are shown in the right panel. A sharp transition between the two regimes is seen\n'
             'close to that found by Kereš et al. (2005).', 
             fontsize=10, ha='center', va='bottom', wrap=True)
    
    plt.subplots_adjust(bottom=0.15)  # Make room for caption
    
    plt.savefig(OutputDir + 'keres_style_regime_plot.pdf', dpi=300, bbox_inches='tight')
    plt.savefig(OutputDir + 'keres_style_regime_plot.png', dpi=300, bbox_inches='tight')
    print(f'Keres-style regime plots saved to {OutputDir}keres_style_regime_plot.[pdf/png]')
    
    # Print statistics
    print(f"\nREGIME STATISTICS:")
    print(f"CGM regime galaxies: {np.sum(cgm_regime):,} ({100*np.sum(cgm_regime)/len(Mvir):.1f}%)")
    print(f"HOT regime galaxies: {np.sum(hot_regime):,} ({100*np.sum(hot_regime)/len(Mvir):.1f}%)")
    
    # Mass ranges
    if np.sum(cgm_regime) > 0:
        cgm_masses = Mvir[cgm_regime]
        print(f"CGM regime mass range: {cgm_masses.min():.2e} to {cgm_masses.max():.2e} M☉")
    
    if np.sum(hot_regime) > 0:
        hot_masses = Mvir[hot_regime]
        print(f"HOT regime mass range: {hot_masses.min():.2e} to {hot_masses.max():.2e} M☉")

# Alternative single panel version (like your original plot)
def create_single_regime_plot(Mvir, HotGas, CGMgas, Regime_c, rcool_to_rvir, OutputDir='./output/millennium/plots/'):
    """Create single panel version colored by regime"""
    
    cgm_regime = (Regime_c == 0)
    hot_regime = (Regime_c == 1)
    
    fig, ax = plt.subplots(1, 1, figsize=(10, 8))
    
    # Plot both regimes on same panel
    ax.scatter(Mvir[cgm_regime], rcool_to_rvir[cgm_regime], 
               c='blue', alpha=0.6, s=12, label=f'CGM regime ({np.sum(cgm_regime):,})', 
               edgecolors='none')
    ax.scatter(Mvir[hot_regime], rcool_to_rvir[hot_regime], 
               c='red', alpha=0.6, s=12, label=f'HOT regime ({np.sum(hot_regime):,})', 
               edgecolors='none')
    
    # Add boundaries
    ax.axhline(y=1.0, color='black', linestyle='--', linewidth=2, 
               label='r_cool = R_vir (regime boundary)')
    ax.axvline(x=2e11, color='black', linestyle=':', alpha=0.7)
    
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlabel('Halo Mass [M☉]')
    ax.set_ylabel('r_cool / R_vir')
    ax.set_title('Regime Classification: r_cool/R_vir vs Halo Mass')
    ax.legend()
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0.1, 10)
    ax.set_xlim(1e10, 1e15)
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'single_regime_plot.pdf', dpi=300, bbox_inches='tight')
    plt.savefig(OutputDir + 'single_regime_plot.png', dpi=300, bbox_inches='tight')
    print(f'Single regime plot saved to {OutputDir}single_regime_plot.[pdf/png]')

# Call whichever version you prefer:
# create_regime_colored_plot(Mvir_c, HotGas_c, CGMgas_c, Regime_c, rcool_to_rvir_c)
# create_single_regime_plot(Mvir_c, HotGas_c, CGMgas_c, Regime_c, rcool_to_rvir_c)

def create_diagnostic_plots(Mvir, StellarMass, HotGas, CGMgas, ColdGas, TotalSfr, Tvir,
                          cgm_regime, hot_regime, has_hotgas, has_cgmgas,
                          cgm_compliant, hot_compliant):
    """Create comprehensive diagnostic plots"""
    
    # Create output directory
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)
    
    # Main diagnostic figure
    fig = plt.figure(figsize=(20, 16))
    
    # Plot 1: Gas masses vs halo mass
    ax1 = plt.subplot(3, 4, 1)
    valid_hot = HotGas > 0
    valid_cgm = CGMgas > 0
    
    plt.scatter(Mvir[valid_hot], HotGas[valid_hot], alpha=0.6, s=8, c='red', 
               label=f'HotGas ({np.sum(valid_hot):,})', edgecolors='none')
    plt.scatter(Mvir[valid_cgm], CGMgas[valid_cgm], alpha=0.6, s=8, c='blue', 
               label=f'CGMgas ({np.sum(valid_cgm):,})', edgecolors='none')
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7, 
               label=f'{MASS_THRESHOLD:.0e} M☉ threshold')
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Gas Mass [M☉]')
    plt.title('Gas Masses vs Halo Mass')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    
    # Plot 2: Gas fractions vs halo mass
    ax2 = plt.subplot(3, 4, 2)
    total_gas = HotGas + CGMgas + ColdGas
    hot_frac = HotGas / (total_gas + 1e-20)
    cgm_frac = CGMgas / (total_gas + 1e-20)
    
    plt.scatter(Mvir, hot_frac, alpha=0.6, s=8, c='red', label='HotGas fraction', edgecolors='none')
    plt.scatter(Mvir, cgm_frac, alpha=0.6, s=8, c='blue', label='CGMgas fraction', edgecolors='none')
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7)
    plt.xscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Gas Fraction')
    plt.title('Gas Fractions vs Halo Mass')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.ylim(0, 1)
    
    # Plot 3: Temperature vs CGMgas (colored by Mvir)
    ax3 = plt.subplot(3, 4, 3)
    valid_cgm = CGMgas > 1e6
    if np.sum(valid_cgm) > 0:
        scatter = plt.scatter(Tvir[valid_cgm], CGMgas[valid_cgm], 
                             c=np.log10(Mvir[valid_cgm]), alpha=0.7, s=12, 
                             cmap='viridis', edgecolors='none')
        plt.colorbar(scatter, label='log₁₀(Mvir [M☉])')
        plt.axvline(1e4, color='red', linestyle='--', alpha=0.7, label='10⁴ K')
        plt.xscale('log')
        plt.yscale('log')
        plt.xlabel('Virial Temperature [K]')
        plt.ylabel('CGM Gas Mass [M☉]')
        plt.title('Temperature vs CGM Gas\n(colored by halo mass)')
        plt.legend(fontsize=10)
        plt.grid(True, alpha=0.3)
    else:
        plt.text(0.5, 0.5, 'No galaxies with\nsignificant CGMgas', 
                ha='center', va='center', transform=ax3.transAxes)
        plt.title('Temperature vs CGM Gas')
    
    # Plot 4: Temperature vs HotGas (colored by Mvir)
    ax4 = plt.subplot(3, 4, 4)
    valid_hot = HotGas > 1e6
    if np.sum(valid_hot) > 0:
        scatter = plt.scatter(Tvir[valid_hot], HotGas[valid_hot], 
                             c=np.log10(Mvir[valid_hot]), alpha=0.7, s=12, 
                             cmap='plasma', edgecolors='none')
        plt.colorbar(scatter, label='log₁₀(Mvir [M☉])')
        plt.axvline(1e4, color='red', linestyle='--', alpha=0.7, label='10⁴ K')
        plt.xscale('log')
        plt.yscale('log')
        plt.xlabel('Virial Temperature [K]')
        plt.ylabel('Hot Gas Mass [M☉]')
        plt.title('Temperature vs Hot Gas\n(colored by halo mass)')
        plt.legend(fontsize=10)
        plt.grid(True, alpha=0.3)
    else:
        plt.text(0.5, 0.5, 'No galaxies with\nsignificant HotGas', 
                ha='center', va='center', transform=ax4.transAxes)
        plt.title('Temperature vs Hot Gas')
    
    # Plot 5: Regime compliance
    ax5 = plt.subplot(3, 4, 5)
    mass_bins = np.logspace(9, 14, 25)
    mass_centers = (mass_bins[1:] + mass_bins[:-1]) / 2
    
    hot_fraction = []
    cgm_fraction = []
    
    for i in range(len(mass_bins)-1):
        mask_bin = (Mvir >= mass_bins[i]) & (Mvir < mass_bins[i+1])
        if np.sum(mask_bin) > 10:  # Only if enough galaxies in bin
            hot_fraction.append(np.sum(has_hotgas[mask_bin]) / np.sum(mask_bin))
            cgm_fraction.append(np.sum(has_cgmgas[mask_bin]) / np.sum(mask_bin))
        else:
            hot_fraction.append(np.nan)
            cgm_fraction.append(np.nan)
    
    plt.plot(mass_centers, hot_fraction, 'r-', linewidth=2, label='HotGas fraction')
    plt.plot(mass_centers, cgm_fraction, 'b-', linewidth=2, label='CGMgas fraction')
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7)
    plt.xscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Fraction with Gas Type')
    plt.title('Gas Type Fractions vs Mass')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    plt.ylim(0, 1)
    
    # Plot 6: Stellar mass vs halo mass colored by gas type
    ax6 = plt.subplot(3, 4, 6)
    valid_stellar = StellarMass > 0
    
    # Plot background of all galaxies
    plt.scatter(Mvir[valid_stellar], StellarMass[valid_stellar], alpha=0.3, s=4, c='gray', 
               edgecolors='none', label='All galaxies')
    
    # Highlight regime violations
    violations_cgm = cgm_regime & has_hotgas  # CGM regime with HotGas
    violations_hot = hot_regime & has_cgmgas  # HOT regime with CGMgas
    
    if np.sum(violations_cgm) > 0:
        plt.scatter(Mvir[violations_cgm], StellarMass[violations_cgm], alpha=0.8, s=20, 
                   c='orange', edgecolors='black', linewidth=0.5,
                   label=f'CGM violations ({np.sum(violations_cgm)})')
    
    if np.sum(violations_hot) > 0:
        plt.scatter(Mvir[violations_hot], StellarMass[violations_hot], alpha=0.8, s=20, 
                   c='purple', edgecolors='black', linewidth=0.5,
                   label=f'HOT violations ({np.sum(violations_hot)})')
    
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7)
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Stellar Mass [M☉]')
    plt.title('Stellar Mass vs Halo Mass\n(Regime Violations Highlighted)')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    
    # Plot 7: Temperature vs mass
    ax7 = plt.subplot(3, 4, 7)
    plt.scatter(Mvir, Tvir, alpha=0.6, s=8, c='green', edgecolors='none')
    plt.axhline(1e4, color='red', linestyle='--', alpha=0.7, label='10^4 K')
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7)
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Virial Temperature [K]')
    plt.title('Virial Temperature vs Halo Mass')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    
    # Plot 8: Mass histogram with regime breakdown
    ax8 = plt.subplot(3, 4, 8)
    bins = np.logspace(9, 14, 30)
    
    plt.hist(Mvir[cgm_compliant], bins=bins, alpha=0.7, color='blue', 
             label=f'CGM compliant ({np.sum(cgm_compliant):,})')
    plt.hist(Mvir[hot_compliant], bins=bins, alpha=0.7, color='red', 
             label=f'HOT compliant ({np.sum(hot_compliant):,})')
    
    if np.sum(violations_cgm) > 0:
        plt.hist(Mvir[violations_cgm], bins=bins, alpha=0.7, color='orange', 
                 label=f'CGM violations ({np.sum(violations_cgm):,})')
    
    if np.sum(violations_hot) > 0:
        plt.hist(Mvir[violations_hot], bins=bins, alpha=0.7, color='purple', 
                 label=f'HOT violations ({np.sum(violations_hot):,})')
    
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7, label='Mass threshold')
    plt.xscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Number of Galaxies')
    plt.title('Mass Distribution by Regime Compliance')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    
    # Plot 9: Gas content summary
    ax9 = plt.subplot(3, 4, 9)
    categories = ['CGM\nRegime', 'HOT\nRegime']
    compliant_counts = [np.sum(cgm_compliant), np.sum(hot_compliant)]
    violation_counts = [np.sum(violations_cgm), np.sum(violations_hot)]
    
    x = np.arange(len(categories))
    width = 0.35
    
    plt.bar(x - width/2, compliant_counts, width, label='Compliant', color='green', alpha=0.7)
    plt.bar(x + width/2, violation_counts, width, label='Violations', color='red', alpha=0.7)
    
    plt.xlabel('Regime')
    plt.ylabel('Number of Galaxies')
    plt.title('Regime Compliance Summary')
    plt.xticks(x, categories)
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # Add text annotations
    for i, (comp, viol) in enumerate(zip(compliant_counts, violation_counts)):
        total = comp + viol
        if total > 0:
            plt.text(i, max(comp, viol) + total*0.05, 
                    f'{comp/total*100:.1f}%\ncompliant', 
                    ha='center', va='bottom', fontsize=10)
    
    # Plot 10: Star formation activity
    ax10 = plt.subplot(3, 4, 10)
    active_sf = TotalSfr > 0
    
    plt.scatter(Mvir[active_sf & cgm_regime], TotalSfr[active_sf & cgm_regime], 
               alpha=0.6, s=8, c='blue', label='CGM regime', edgecolors='none')
    plt.scatter(Mvir[active_sf & hot_regime], TotalSfr[active_sf & hot_regime], 
               alpha=0.6, s=8, c='red', label='HOT regime', edgecolors='none')
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7)
    plt.xscale('log')
    plt.yscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Star Formation Rate [M☉/yr]')
    plt.title('Star Formation vs Halo Mass')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    
    # Plot 11: Summary statistics
    ax11 = plt.subplot(3, 4, 11)
    ax11.axis('off')
    
    stats_text = f"""PHYSICS-BASED REGIME ANALYSIS

Total Galaxies: {len(Mvir):,}

CGM REGIME (rcool/Rvir < 1.0):
• Total: {np.sum(cgm_regime):,}
• Compliant: {np.sum(cgm_compliant):,} ({100*np.sum(cgm_compliant)/np.sum(cgm_regime):.1f}%)
• Violations: {np.sum(violations_cgm):,}

HOT REGIME (rcool/Rvir ≥ 1.0):
• Total: {np.sum(hot_regime):,}
• Compliant: {np.sum(hot_compliant):,} ({100*np.sum(hot_compliant)/np.sum(hot_regime):.1f}%)
• Violations: {np.sum(violations_hot):,}

OVERALL: {100*(np.sum(cgm_compliant) + np.sum(hot_compliant))/len(Mvir):.1f}% compliant
"""
    
    ax11.text(0.05, 0.95, stats_text, transform=ax11.transAxes, fontsize=10,
             verticalalignment='top', fontfamily='monospace',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(OutputDir + 'mass_regime_diagnostic.pdf', dpi=300, bbox_inches='tight')
    plt.savefig(OutputDir + 'mass_regime_diagnostic.png', dpi=300, bbox_inches='tight')
    print(f'Diagnostic plots saved to {OutputDir}mass_regime_diagnostic.[pdf/png]')

if __name__ == "__main__":
    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction
    
    results = analyze_mass_regime()

    
    
    print('=' * 60)
    print('FINAL SUMMARY:')
    print(f'Overall compliance rate: {(results["compliance_rate_cgm"] * results["cgm_regime_count"] + results["compliance_rate_hot"] * results["hot_regime_count"]) / results["total_galaxies"]:.1f}%')
    print('=' * 60)
