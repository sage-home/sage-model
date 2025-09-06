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
    print('MASS-REGIME AWARE CGM PHYSICS DIAGNOSTIC')
    print('=' * 60)
    print(f'Analyzing snapshot: {Snapshot} (z=0)')
    print(f'Mass threshold for regime separation: {MASS_THRESHOLD:.0e} M☉')
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
    
    print(f'Central galaxies analyzed: {len(Mvir_c):,}')
    print()
    
    # Calculate derived properties
    TotalSfr = SfrDisk_c + SfrBulge_c
    TotalGas = HotGas_c + CGMgas_c + ColdGas_c
    Tvir = 35.9 * Vvir_c * Vvir_c  # Virial temperature in K
    
    # Mass-based regime classification
    low_mass = Mvir_c < MASS_THRESHOLD
    high_mass = Mvir_c >= MASS_THRESHOLD
    
    # Gas content classification
    has_hotgas = HotGas_c > 1e6  # Significant HotGas (>1e6 M_sun)
    has_cgmgas = CGMgas_c > 1e6  # Significant CGMgas (>1e6 M_sun)
    has_both_gas = has_hotgas & has_cgmgas
    has_no_gas = (~has_hotgas) & (~has_cgmgas)
    
    # Regime compliance check
    low_mass_compliant = low_mass & (~has_hotgas)  # Low mass should have no HotGas
    high_mass_compliant = high_mass & (~has_cgmgas)  # High mass should have no CGMgas
    
    print_diagnostics(Mvir_c, low_mass, high_mass, has_hotgas, has_cgmgas, 
                     has_both_gas, has_no_gas, low_mass_compliant, high_mass_compliant)
    
    create_diagnostic_plots(Mvir_c, StellarMass_c, HotGas_c, CGMgas_c, ColdGas_c, 
                           TotalSfr, Tvir, low_mass, high_mass, has_hotgas, has_cgmgas,
                           low_mass_compliant, high_mass_compliant)
    
    return {
        'total_galaxies': len(Mvir_c),
        'low_mass_count': np.sum(low_mass),
        'high_mass_count': np.sum(high_mass),
        'low_mass_compliant': np.sum(low_mass_compliant),
        'high_mass_compliant': np.sum(high_mass_compliant),
        'compliance_rate_low': np.sum(low_mass_compliant) / np.sum(low_mass) * 100 if np.sum(low_mass) > 0 else 0,
        'compliance_rate_high': np.sum(high_mass_compliant) / np.sum(high_mass) * 100 if np.sum(high_mass) > 0 else 0
    }

def print_diagnostics(Mvir, low_mass, high_mass, has_hotgas, has_cgmgas, 
                     has_both_gas, has_no_gas, low_mass_compliant, high_mass_compliant):
    """Print detailed diagnostic information"""
    
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

def create_diagnostic_plots(Mvir, StellarMass, HotGas, CGMgas, ColdGas, TotalSfr, Tvir,
                          low_mass, high_mass, has_hotgas, has_cgmgas,
                          low_mass_compliant, high_mass_compliant):
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
    violations_low = low_mass & has_hotgas
    violations_high = high_mass & has_cgmgas
    
    if np.sum(violations_low) > 0:
        plt.scatter(Mvir[violations_low], StellarMass[violations_low], alpha=0.8, s=20, 
                   c='orange', edgecolors='black', linewidth=0.5,
                   label=f'Low-mass violations ({np.sum(violations_low)})')
    
    if np.sum(violations_high) > 0:
        plt.scatter(Mvir[violations_high], StellarMass[violations_high], alpha=0.8, s=20, 
                   c='purple', edgecolors='black', linewidth=0.5,
                   label=f'High-mass violations ({np.sum(violations_high)})')
    
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
    
    plt.hist(Mvir[low_mass_compliant], bins=bins, alpha=0.7, color='blue', 
             label=f'Low-mass compliant ({np.sum(low_mass_compliant):,})')
    plt.hist(Mvir[high_mass_compliant], bins=bins, alpha=0.7, color='red', 
             label=f'High-mass compliant ({np.sum(high_mass_compliant):,})')
    
    if np.sum(low_mass & has_hotgas) > 0:
        plt.hist(Mvir[low_mass & has_hotgas], bins=bins, alpha=0.7, color='orange', 
                 label=f'Low-mass violations ({np.sum(low_mass & has_hotgas):,})')
    
    if np.sum(high_mass & has_cgmgas) > 0:
        plt.hist(Mvir[high_mass & has_cgmgas], bins=bins, alpha=0.7, color='purple', 
                 label=f'High-mass violations ({np.sum(high_mass & has_cgmgas):,})')
    
    plt.axvline(MASS_THRESHOLD, color='gray', linestyle='--', alpha=0.7)
    plt.xscale('log')
    plt.xlabel('Halo Mass [M☉]')
    plt.ylabel('Number of Galaxies')
    plt.title('Mass Distribution by Regime Compliance')
    plt.legend(fontsize=10)
    plt.grid(True, alpha=0.3)
    
    # Plot 9: Gas content summary
    ax9 = plt.subplot(3, 4, 9)
    categories = ['Low Mass\n(CGM)', 'High Mass\n(Hot)']
    compliant_counts = [np.sum(low_mass_compliant), np.sum(high_mass_compliant)]
    violation_counts = [np.sum(low_mass & has_hotgas), np.sum(high_mass & has_cgmgas)]
    
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
    
    plt.scatter(Mvir[active_sf & low_mass], TotalSfr[active_sf & low_mass], 
               alpha=0.6, s=8, c='blue', label='Low mass', edgecolors='none')
    plt.scatter(Mvir[active_sf & high_mass], TotalSfr[active_sf & high_mass], 
               alpha=0.6, s=8, c='red', label='High mass', edgecolors='none')
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
    
    stats_text = f"""REGIME COMPLIANCE SUMMARY

Total Galaxies: {len(Mvir):,}

LOW MASS (<{MASS_THRESHOLD:.0e} M☉):
• Total: {np.sum(low_mass):,}
• Compliant: {np.sum(low_mass_compliant):,} ({100*np.sum(low_mass_compliant)/np.sum(low_mass):.1f}%)
• Violations: {np.sum(low_mass & has_hotgas):,}

HIGH MASS (≥{MASS_THRESHOLD:.0e} M☉):
• Total: {np.sum(high_mass):,}
• Compliant: {np.sum(high_mass_compliant):,} ({100*np.sum(high_mass_compliant)/np.sum(high_mass):.1f}%)
• Violations: {np.sum(high_mass & has_cgmgas):,}

OVERALL: {100*(np.sum(low_mass_compliant) + np.sum(high_mass_compliant))/len(Mvir):.1f}% compliant
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
    print(f'Overall compliance rate: {(results["compliance_rate_low"] * results["low_mass_count"] + results["compliance_rate_high"] * results["high_mass_count"]) / results["total_galaxies"]:.1f}%')
    print('=' * 60)
