#!/usr/bin/env python
"""
FFB Galaxy Fate Diagnostics (FIXED VERSION)
============================================
Traces what happens to FFB galaxies and why they disappear from the catalog.

Key fixes:
- Properly detects mergers by reading mergeType from the snapshot BEFORE galaxy disappears
- Tracks galaxies using both GalaxyIndex AND snapshot-to-snapshot continuity
- Handles the fact that merged galaxies don't appear in the snapshot where they merge
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

DirName = './output/millennium/'
FileName = 'model_0.hdf5'

Hubble_h = 0.73
BoxSize = 62.5
VolumeFraction = 1.0
FirstSnap = 0
LastSnap = 63

redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

OutputDir = './ffb_plots/'
OutputFormat = '.png'

min_stellar_mass = 1e7  # Msun/h

# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read parameter from HDF5 file"""
    property = h5.File(DirName + FileName, 'r')
    snap_key = f'Snap_{snap_num}'
    return np.array(property[snap_key][param])


def ensure_output_dir():
    """Create output directory if it doesn't exist"""
    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)


def trace_ffb_fates():
    """
    Track all FFB galaxies and determine their ultimate fate
    
    KEY FIX: When a galaxy has mergeType > 0 at snapshot N, it means the galaxy
    will DISAPPEAR at snapshot N+1 (it merges/disrupts between N and N+1).
    The code writes the merger info to snapshot N, then removes the galaxy from N+1.
    """
    print("\n" + "="*80)
    print("TRACING FFB GALAXY FATES (FIXED VERSION)")
    print("="*80)
    
    # Data structures
    galaxy_tracker = {}  # GalaxyIndex -> fate info
    
    # Fate categories
    fates = {
        'survived_z0': [],       # Still exists at z=0
        'merged_minor': [],      # Merged as minor merger (Type 1)
        'merged_major': [],      # Merged as major merger (Type 2)
        'merged_disk': [],       # Merged via disk instability (Type 3)
        'disrupted': [],         # Tidally disrupted (Type 4)
        'mass_loss': [],         # Dropped below mass threshold
        'catalog_dropout': [],   # Disappeared from catalog (no clear merger/disruption)
        'unknown': []            # Unclear fate
    }
    
    # Statistics per snapshot
    ffb_present = []
    z_array = []
    
    print("\nReading all snapshots and tracking galaxy fates...")
    
    # Pass 1: Identify all FFB galaxies ever
    ffb_galaxies = set()
    
    for snap in range(FirstSnap, LastSnap + 1):
        try:
            ffb_regime = read_hdf(snap_num=snap, param='FFBRegime')
            galaxy_index = read_hdf(snap_num=snap, param='GalaxyIndex')
            
            is_ffb = (ffb_regime == 1)
            ffb_gal_indices = galaxy_index[is_ffb]
            ffb_galaxies.update(ffb_gal_indices.tolist())
            
        except Exception as e:
            continue
    
    print(f"Total unique FFB galaxies ever: {len(ffb_galaxies)}")
    
    # Pass 2: Track each FFB galaxy through time
    prev_snap_data = None  # Store previous snapshot data
    
    for snap in range(FirstSnap, LastSnap + 1):
        z = redshifts[snap]
        
        try:
            # Read current snapshot data
            galaxy_index = read_hdf(snap_num=snap, param='GalaxyIndex')
            stellar_mass = read_hdf(snap_num=snap, param='StellarMass') * 1e10 / Hubble_h
            mvir = read_hdf(snap_num=snap, param='Mvir') * 1e10 / Hubble_h
            ffb_regime = read_hdf(snap_num=snap, param='FFBRegime')
            
            try:
                merge_type = read_hdf(snap_num=snap, param='mergeType')
            except:
                merge_type = np.zeros_like(stellar_mass, dtype=int)
            
            try:
                gal_type = read_hdf(snap_num=snap, param='Type')
            except:
                gal_type = np.zeros_like(stellar_mass, dtype=int)
            
            try:
                sfr_disk = read_hdf(snap_num=snap, param='SfrDisk')
                sfr_bulge = read_hdf(snap_num=snap, param='SfrBulge')
                sfr = sfr_disk + sfr_bulge
            except:
                sfr = np.zeros_like(stellar_mass)
            
            # Create lookup dictionary for current snapshot
            snap_data = {}
            for i, gal_idx in enumerate(galaxy_index):
                snap_data[gal_idx] = {
                    'stellar_mass': stellar_mass[i],
                    'mvir': mvir[i],
                    'ffb_regime': ffb_regime[i],
                    'merge_type': merge_type[i],
                    'type': gal_type[i],
                    'sfr': sfr[i],
                    'snap': snap,
                    'redshift': z,
                    'index_in_arrays': i
                }
            
            # Track each FFB galaxy
            for gal_idx in ffb_galaxies:
                if gal_idx not in galaxy_tracker:
                    galaxy_tracker[gal_idx] = {
                        'snapshots': [],
                        'redshifts': [],
                        'masses': [],
                        'mvirs': [],
                        'ffb_regime': [],
                        'merge_types': [],
                        'types': [],
                        'sfrs': [],
                        'first_ffb_snap': None,
                        'first_ffb_z': None,
                        'last_seen_snap': None,
                        'last_seen_z': None,
                        'fate': 'unknown',
                        'fate_snap': None,
                        'fate_z': None,
                        'fate_details': {}
                    }
                
                tracker = galaxy_tracker[gal_idx]
                
                # Is galaxy present in this snapshot?
                if gal_idx in snap_data:
                    data = snap_data[gal_idx]
                    
                    # Store all tracking data
                    tracker['snapshots'].append(snap)
                    tracker['redshifts'].append(z)
                    tracker['masses'].append(data['stellar_mass'])
                    tracker['mvirs'].append(data['mvir'])
                    tracker['ffb_regime'].append(data['ffb_regime'])
                    tracker['merge_types'].append(data['merge_type'])
                    tracker['types'].append(data['type'])
                    tracker['sfrs'].append(data['sfr'])
                    
                    # Track first FFB occurrence
                    if data['ffb_regime'] == 1 and tracker['first_ffb_snap'] is None:
                        tracker['first_ffb_snap'] = snap
                        tracker['first_ffb_z'] = z
                    
                    # Update last seen
                    tracker['last_seen_snap'] = snap
                    tracker['last_seen_z'] = z
                    
                    # *** KEY FIX: Check for mergeType in THIS snapshot ***
                    # If mergeType > 0 here, the galaxy will DISAPPEAR in the NEXT snapshot
                    if data['merge_type'] > 0 and tracker['fate'] == 'unknown':
                        # This galaxy is flagged to merge/disrupt
                        # It will be gone in the next snapshot
                        tracker['fate_snap'] = snap  # Flag set at this snap
                        tracker['fate_z'] = z
                        
                        # The actual merger happens between this snap and the next
                        # So we record the state at this snapshot (last time we see it)
                        tracker['fate_details'] = {
                            'mass_at_fate': data['stellar_mass'],
                            'mvir_at_fate': data['mvir'],
                            'type_at_fate': data['type'],
                            'sfr_at_fate': data['sfr'],
                            'merge_type': data['merge_type']
                        }
                        
                        # Classify the fate
                        if data['merge_type'] == 1:
                            tracker['fate'] = 'merged_minor'
                        elif data['merge_type'] == 2:
                            tracker['fate'] = 'merged_major'
                        elif data['merge_type'] == 3:
                            tracker['fate'] = 'merged_disk'
                        elif data['merge_type'] == 4:
                            tracker['fate'] = 'disrupted'
                        
                        if snap % 10 == 0 and len(fates[tracker['fate']]) < 3:  # Print first few examples
                            print(f"\n  Found {tracker['fate']}: GalIdx={gal_idx}, Snap={snap}, z={z:.2f}, M*={data['stellar_mass']:.2e}")
                
                else:
                    # Galaxy NOT present in this snapshot
                    # If it was present before, check if we missed a merger flag
                    if len(tracker['snapshots']) > 0 and tracker['fate'] == 'unknown':
                        prev_snap = tracker['snapshots'][-1]
                        
                        # Check if we have info from previous snapshot
                        # The mergeType flag should have been set there
                        prev_merge_type = tracker['merge_types'][-1]
                        
                        if prev_merge_type > 0:
                            # We should have caught this in the previous snapshot
                            # But just in case, mark it here
                            if tracker['fate'] == 'unknown':
                                if prev_merge_type == 1:
                                    tracker['fate'] = 'merged_minor'
                                elif prev_merge_type == 2:
                                    tracker['fate'] = 'merged_major'
                                elif prev_merge_type == 3:
                                    tracker['fate'] = 'merged_disk'
                                elif prev_merge_type == 4:
                                    tracker['fate'] = 'disrupted'
                                
                                tracker['fate_snap'] = prev_snap
                                tracker['fate_z'] = tracker['redshifts'][-1]
                                tracker['fate_details'] = {
                                    'mass_at_fate': tracker['masses'][-1],
                                    'mvir_at_fate': tracker['mvirs'][-1],
                                    'merge_type': prev_merge_type
                                }
                        else:
                            # No merger flag was set - this is a true dropout
                            tracker['fate'] = 'catalog_dropout'
                            tracker['fate_snap'] = prev_snap
                            tracker['fate_z'] = tracker['redshifts'][-1]
                            tracker['fate_details'] = {
                                'mass_at_fate': tracker['masses'][-1],
                                'mvir_at_fate': tracker['mvirs'][-1],
                                'next_snap_checked': snap,
                                'last_merge_type': prev_merge_type
                            }
                            
                            # Check if mass dropped below threshold
                            if tracker['masses'][-1] < min_stellar_mass:
                                tracker['fate'] = 'mass_loss'
                            
                            if snap % 10 == 0 and len(fates['catalog_dropout']) < 3:
                                print(f"\n  Found dropout: GalIdx={gal_idx}, LastSnap={prev_snap}, M*={tracker['masses'][-1]:.2e}, mergeType={prev_merge_type}")
            
            # Count how many FFB galaxies are still present
            n_ffb_present = sum(1 for gal_idx in ffb_galaxies if gal_idx in snap_data)
            ffb_present.append(n_ffb_present)
            z_array.append(z)
            
            if snap % 5 == 0:
                print(f"Snap {snap:3d} (z={z:6.2f}): {n_ffb_present:6d} FFB galaxies still in catalog")
            
            # Store current snapshot for next iteration
            prev_snap_data = snap_data
                
        except Exception as e:
            print(f"Warning: Error reading snap {snap}: {e}")
            continue
    
    # Pass 3: Final classification
    for gal_idx, tracker in galaxy_tracker.items():
        if len(tracker['snapshots']) == 0:
            continue
        
        # Check if survived to z=0
        if tracker['last_seen_snap'] == LastSnap and tracker['fate'] == 'unknown':
            tracker['fate'] = 'survived_z0'
        
        # Categorize
        fate = tracker['fate']
        fates[fate].append(gal_idx)
    
    # Convert lists to arrays
    for gal_idx in galaxy_tracker:
        for key in ['snapshots', 'redshifts', 'masses', 'mvirs', 'ffb_regime', 'merge_types', 'types', 'sfrs']:
            if key in galaxy_tracker[gal_idx]:
                galaxy_tracker[gal_idx][key] = np.array(galaxy_tracker[gal_idx][key])
    
    return galaxy_tracker, fates, np.array(z_array), np.array(ffb_present)


def print_fate_statistics(galaxy_tracker, fates):
    """Print detailed statistics about galaxy fates"""
    print("\n" + "="*80)
    print("FFB GALAXY FATE STATISTICS")
    print("="*80)
    
    total = len(galaxy_tracker)
    
    print(f"\nTotal FFB galaxies tracked: {total}")
    print("\nFate breakdown:")
    print("-" * 80)
    
    fate_names = {
        'survived_z0': 'Survived to z=0',
        'merged_minor': 'Minor merger (Type 1)',
        'merged_major': 'Major merger (Type 2)',
        'merged_disk': 'Disk instability merger (Type 3)',
        'disrupted': 'Tidal disruption (Type 4)',
        'mass_loss': 'Mass dropped below threshold',
        'catalog_dropout': 'Catalog dropout (unclear)',
        'unknown': 'Unknown fate'
    }
    
    for fate_key, name in fate_names.items():
        count = len(fates[fate_key])
        pct = 100 * count / total if total > 0 else 0
        print(f"{name:40s}: {count:6d} ({pct:5.1f}%)")
    
    # Detailed analysis of catalog dropouts
    print("\n" + "="*80)
    print("DETAILED ANALYSIS: CATALOG DROPOUTS")
    print("="*80)
    
    dropout_redshifts = []
    dropout_masses = []
    
    for gal_idx in fates['catalog_dropout']:
        tracker = galaxy_tracker[gal_idx]
        dropout_redshifts.append(tracker['fate_z'])
        if 'mass_at_fate' in tracker['fate_details']:
            dropout_masses.append(tracker['fate_details']['mass_at_fate'])
    
    if len(dropout_redshifts) > 0:
        print(f"\nDropout redshift range: z = {min(dropout_redshifts):.2f} to {max(dropout_redshifts):.2f}")
        print(f"Median dropout redshift: z = {np.median(dropout_redshifts):.2f}")
        
        if len(dropout_masses) > 0:
            print(f"Dropout mass range: {min(dropout_masses):.2e} to {max(dropout_masses):.2e} Msun")
            print(f"Median dropout mass: {np.median(dropout_masses):.2e} Msun")
            print(f"Fraction below min_stellar_mass: {100*sum(m < min_stellar_mass for m in dropout_masses)/len(dropout_masses):.1f}%")
    else:
        print("\nNo catalog dropouts found!")
    
    # Merger analysis
    print("\n" + "="*80)
    print("DETAILED ANALYSIS: MERGERS")
    print("="*80)
    
    all_mergers = fates['merged_minor'] + fates['merged_major'] + fates['merged_disk']
    merger_redshifts = []
    merger_masses = []
    
    for gal_idx in all_mergers:
        tracker = galaxy_tracker[gal_idx]
        if tracker['fate_z'] is not None:
            merger_redshifts.append(tracker['fate_z'])
        if 'mass_at_fate' in tracker['fate_details']:
            merger_masses.append(tracker['fate_details']['mass_at_fate'])
    
    if len(merger_redshifts) > 0:
        print(f"\nMerger redshift range: z = {min(merger_redshifts):.2f} to {max(merger_redshifts):.2f}")
        print(f"Median merger redshift: z = {np.median(merger_redshifts):.2f}")
        
        if len(merger_masses) > 0:
            print(f"Merger mass range: {min(merger_masses):.2e} to {max(merger_masses):.2e} Msun")
            print(f"Median merger mass: {np.median(merger_masses):.2e} Msun")
    
    # Disruption analysis
    print("\n" + "="*80)
    print("DETAILED ANALYSIS: DISRUPTIONS")
    print("="*80)
    
    disrupt_redshifts = []
    disrupt_masses = []
    disrupt_mvirs = []
    
    for gal_idx in fates['disrupted']:
        tracker = galaxy_tracker[gal_idx]
        if tracker['fate_z'] is not None:
            disrupt_redshifts.append(tracker['fate_z'])
        if 'mass_at_fate' in tracker['fate_details']:
            disrupt_masses.append(tracker['fate_details']['mass_at_fate'])
        if 'mvir_at_fate' in tracker['fate_details']:
            disrupt_mvirs.append(tracker['fate_details']['mvir_at_fate'])
    
    if len(disrupt_redshifts) > 0:
        print(f"\nDisruption redshift range: z = {min(disrupt_redshifts):.2f} to {max(disrupt_redshifts):.2f}")
        print(f"Median disruption redshift: z = {np.median(disrupt_redshifts):.2f}")
        
        if len(disrupt_masses) > 0:
            print(f"Disruption mass range: {min(disrupt_masses):.2e} to {max(disrupt_masses):.2e} Msun")
            print(f"Median disruption mass: {np.median(disrupt_masses):.2e} Msun")
        
        if len(disrupt_mvirs) > 0 and len(disrupt_masses) > 0:
            mass_ratios = np.array(disrupt_masses) / np.array(disrupt_mvirs)
            print(f"M*/Mvir at disruption: median = {np.median(mass_ratios):.3f}, mean = {np.mean(mass_ratios):.3f}")
            print(f"  (High M*/Mvir → baryon-rich systems prone to tidal disruption)")


def plot_fate_diagnostics(galaxy_tracker, fates, z_array, ffb_present):
    """Create diagnostic plots showing where FFB galaxies go"""
    print("\n" + "="*80)
    print("CREATING DIAGNOSTIC PLOTS")
    print("="*80)
    
    fig = plt.figure(figsize=(16, 12))
    
    # Plot 1: Number of FFB galaxies vs redshift
    ax1 = plt.subplot(3, 3, 1)
    ax1.plot(z_array, ffb_present, 'b-', lw=2)
    ax1.set_xlabel('Redshift')
    ax1.set_ylabel('Number of FFB galaxies in catalog')
    ax1.set_title('FFB Galaxy Survival')
    ax1.grid(True, alpha=0.3)
    ax1.set_yscale('log')
    
    # Plot 2: Fate distribution (pie chart)
    ax2 = plt.subplot(3, 3, 2)
    
    fate_labels = ['Survived z=0', 'Minor merger', 'Major merger', 
                   'Disk merger', 'Disrupted', 'Mass loss', 'Dropout', 'Unknown']
    fate_counts = [len(fates['survived_z0']), len(fates['merged_minor']), 
                   len(fates['merged_major']), len(fates['merged_disk']),
                   len(fates['disrupted']), len(fates['mass_loss']),
                   len(fates['catalog_dropout']), len(fates['unknown'])]
    
    colors = ['green', 'orange', 'red', 'purple', 'brown', 'gray', 'black', 'white']
    
    # Only plot non-zero categories
    plot_labels = [fate_labels[i] for i in range(len(fate_counts)) if fate_counts[i] > 0]
    plot_counts = [fate_counts[i] for i in range(len(fate_counts)) if fate_counts[i] > 0]
    plot_colors = [colors[i] for i in range(len(fate_counts)) if fate_counts[i] > 0]
    
    ax2.pie(plot_counts, labels=plot_labels, colors=plot_colors, autopct='%1.1f%%', startangle=90)
    ax2.set_title('FFB Galaxy Fates (FIXED)')
    
    # Plot 3: Dropout redshift histogram
    ax3 = plt.subplot(3, 3, 3)
    dropout_z = [galaxy_tracker[gal_idx]['fate_z'] for gal_idx in fates['catalog_dropout'] 
                 if galaxy_tracker[gal_idx]['fate_z'] is not None]
    
    if len(dropout_z) > 0:
        ax3.hist(dropout_z, bins=30, color='black', alpha=0.7, edgecolor='white')
        ax3.set_xlabel('Redshift')
        ax3.set_ylabel('Number of dropouts')
        ax3.set_title('Catalog Dropout Redshift Distribution')
        ax3.grid(True, alpha=0.3, axis='y')
    else:
        ax3.text(0.5, 0.5, 'No catalog dropouts', ha='center', va='center', transform=ax3.transAxes, fontsize=12)
        ax3.set_title('Catalog Dropout Redshift Distribution')
    
    # Plot 4: Merger redshift histogram
    ax4 = plt.subplot(3, 3, 4)
    merger_z = []
    for fate_key in ['merged_minor', 'merged_major', 'merged_disk', 'disrupted']:
        merger_z.extend([galaxy_tracker[gal_idx]['fate_z'] for gal_idx in fates[fate_key]
                        if galaxy_tracker[gal_idx]['fate_z'] is not None])
    
    if len(merger_z) > 0:
        ax4.hist(merger_z, bins=30, color='orange', alpha=0.7, edgecolor='white')
        ax4.set_xlabel('Redshift')
        ax4.set_ylabel('Number of mergers')
        ax4.set_title('Merger Redshift Distribution')
        ax4.grid(True, alpha=0.3, axis='y')
    else:
        ax4.text(0.5, 0.5, 'No mergers', ha='center', va='center', transform=ax4.transAxes)
    
    # Plot 5: Mass at fate vs redshift (dropouts)
    ax5 = plt.subplot(3, 3, 5)
    dropout_masses = []
    dropout_z_for_mass = []
    
    for gal_idx in fates['catalog_dropout']:
        tracker = galaxy_tracker[gal_idx]
        if 'mass_at_fate' in tracker['fate_details'] and tracker['fate_z'] is not None:
            dropout_masses.append(tracker['fate_details']['mass_at_fate'])
            dropout_z_for_mass.append(tracker['fate_z'])
    
    if len(dropout_masses) > 0:
        ax5.scatter(dropout_z_for_mass, dropout_masses, alpha=0.5, s=10, c='black')
        ax5.axhline(min_stellar_mass, color='red', linestyle='--', lw=2, label='Min mass threshold')
        ax5.set_xlabel('Redshift at dropout')
        ax5.set_ylabel('Stellar mass at dropout [Msun]')
        ax5.set_yscale('log')
        ax5.set_title('Dropout Mass vs Redshift')
        ax5.legend()
        ax5.grid(True, alpha=0.3)
    else:
        ax5.text(0.5, 0.5, 'No dropout mass data', ha='center', va='center', transform=ax5.transAxes)
        ax5.set_title('Dropout Mass vs Redshift')
    
    # Plot 6: Mass at fate vs redshift (mergers)
    ax6 = plt.subplot(3, 3, 6)
    merger_masses = []
    merger_z_for_mass = []
    
    for fate_key in ['merged_minor', 'merged_major', 'merged_disk', 'disrupted']:
        for gal_idx in fates[fate_key]:
            tracker = galaxy_tracker[gal_idx]
            if 'mass_at_fate' in tracker['fate_details'] and tracker['fate_z'] is not None:
                merger_masses.append(tracker['fate_details']['mass_at_fate'])
                merger_z_for_mass.append(tracker['fate_z'])
    
    if len(merger_masses) > 0:
        ax6.scatter(merger_z_for_mass, merger_masses, alpha=0.5, s=10, c='orange')
        ax6.set_xlabel('Redshift at merger')
        ax6.set_ylabel('Stellar mass at merger [Msun]')
        ax6.set_yscale('log')
        ax6.set_title('Merger Mass vs Redshift')
        ax6.grid(True, alpha=0.3)
    else:
        ax6.text(0.5, 0.5, 'No merger mass data', ha='center', va='center', transform=ax6.transAxes)
    
    # Plot 7: M*/Mvir at disruption
    ax7 = plt.subplot(3, 3, 7)
    disrupt_mass_ratios = []
    disrupt_z_for_ratio = []
    
    for gal_idx in fates['disrupted']:
        tracker = galaxy_tracker[gal_idx]
        if 'mass_at_fate' in tracker['fate_details'] and 'mvir_at_fate' in tracker['fate_details']:
            if tracker['fate_details']['mvir_at_fate'] > 0:
                ratio = tracker['fate_details']['mass_at_fate'] / tracker['fate_details']['mvir_at_fate']
                disrupt_mass_ratios.append(ratio)
                disrupt_z_for_ratio.append(tracker['fate_z'])
    
    if len(disrupt_mass_ratios) > 0:
        ax7.scatter(disrupt_z_for_ratio, disrupt_mass_ratios, alpha=0.5, s=10, c='brown')
        ax7.set_xlabel('Redshift at disruption')
        ax7.set_ylabel('M*/Mvir at disruption')
        ax7.set_yscale('log')
        ax7.set_title('Baryon Fraction at Disruption')
        ax7.grid(True, alpha=0.3)
        ax7.text(0.02, 0.98, f'Median: {np.median(disrupt_mass_ratios):.3f}', 
                transform=ax7.transAxes, va='top', fontsize=10,
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
    else:
        ax7.text(0.5, 0.5, 'No disruption data', ha='center', va='center', transform=ax7.transAxes)
        ax7.set_title('Baryon Fraction at Disruption')
    
    # Plot 8: Example mass evolution tracks (mergers)
    ax8 = plt.subplot(3, 3, 8)
    merger_examples = []
    for fate_key in ['merged_minor', 'merged_major', 'merged_disk', 'disrupted']:
        merger_examples.extend(fates[fate_key])
    
    n_examples = min(20, len(merger_examples))
    
    if n_examples > 0:
        for gal_idx in merger_examples[:n_examples]:
            tracker = galaxy_tracker[gal_idx]
            if len(tracker['masses']) > 0:
                ax8.plot(tracker['redshifts'], tracker['masses'], alpha=0.5, lw=1)
                # Mark merger point
                if tracker['fate_snap'] is not None:
                    fate_idx = np.where(tracker['snapshots'] == tracker['fate_snap'])[0]
                    if len(fate_idx) > 0:
                        ax8.plot(tracker['redshifts'][fate_idx], tracker['masses'][fate_idx], 
                                'ro', markersize=6, alpha=0.7)
        
        ax8.set_xlabel('Redshift')
        ax8.set_ylabel('Stellar mass [Msun]')
        ax8.set_yscale('log')
        ax8.set_title(f'Mass Evolution (Mergers, n={n_examples})')
        ax8.grid(True, alpha=0.3)
    else:
        ax8.text(0.5, 0.5, 'No mergers to show', ha='center', va='center', transform=ax8.transAxes)
    
    # Plot 9: FFB duration vs fate
    ax9 = plt.subplot(3, 3, 9)
    
    fate_durations = {}
    for fate_key in ['survived_z0', 'merged_minor', 'merged_major', 'merged_disk', 
                     'disrupted', 'mass_loss', 'catalog_dropout']:
        durations = []
        for gal_idx in fates[fate_key]:
            tracker = galaxy_tracker[gal_idx]
            if len(tracker['ffb_regime']) > 0:
                n_ffb_snaps = np.sum(tracker['ffb_regime'] == 1)
                durations.append(n_ffb_snaps)
        fate_durations[fate_key] = durations
    
    # Box plot
    plot_data = []
    plot_labels = []
    for key in ['survived_z0', 'merged_minor', 'merged_major', 'disrupted', 'mass_loss', 'catalog_dropout']:
        if len(fate_durations[key]) > 0:
            plot_data.append(fate_durations[key])
            plot_labels.append(key.replace('_', ' ').title())
    
    if len(plot_data) > 0:
        ax9.boxplot(plot_data, labels=plot_labels)
        ax9.set_ylabel('Number of snapshots in FFB mode')
        ax9.set_title('FFB Duration by Fate')
        ax9.grid(True, alpha=0.3, axis='y')
        plt.setp(ax9.xaxis.get_majorticklabels(), rotation=45, ha='right')
    else:
        ax9.text(0.5, 0.5, 'No FFB duration data', ha='center', va='center', transform=ax9.transAxes)
    
    plt.tight_layout()
    filename = OutputDir + 'ffb_fate_diagnostics_fixed' + OutputFormat
    plt.savefig(filename, dpi=150, bbox_inches='tight')
    print(f"Saved: {filename}")
    plt.close()


def analyze_specific_examples(galaxy_tracker, fates, n_examples=5):
    """Print detailed evolution of specific example galaxies for each fate"""
    print("\n" + "="*80)
    print("DETAILED EXAMPLES OF EACH FATE")
    print("="*80)
    
    fate_keys = ['disrupted', 'merged_minor', 'merged_major', 'catalog_dropout', 'mass_loss', 'survived_z0']
    
    for fate_key in fate_keys:
        print(f"\n{'-'*80}")
        print(f"FATE: {fate_key.upper().replace('_', ' ')}")
        print(f"{'-'*80}")
        
        examples = fates[fate_key][:n_examples]
        
        if len(examples) == 0:
            print("No examples for this fate.")
            continue
        
        for i, gal_idx in enumerate(examples):
            tracker = galaxy_tracker[gal_idx]
            
            print(f"\nExample {i+1}: GalaxyIndex = {gal_idx}")
            print(f"  First FFB: Snap {tracker['first_ffb_snap']} (z={tracker['first_ffb_z']:.2f})")
            print(f"  Last seen: Snap {tracker['last_seen_snap']} (z={tracker['last_seen_z']:.2f})")
            
            if tracker['fate_snap'] is not None:
                print(f"  Fate occurred: Snap {tracker['fate_snap']} (z={tracker['fate_z']:.2f})")
            
            if 'mass_at_fate' in tracker['fate_details']:
                print(f"  Mass at fate: {tracker['fate_details']['mass_at_fate']:.2e} Msun")
            
            if 'mvir_at_fate' in tracker['fate_details']:
                mvir = tracker['fate_details']['mvir_at_fate']
                mass = tracker['fate_details']['mass_at_fate']
                if mvir > 0:
                    print(f"  Mvir at fate: {mvir:.2e} Msun")
                    print(f"  M*/Mvir at fate: {mass/mvir:.3f}")
            
            print(f"  Total snapshots tracked: {len(tracker['snapshots'])}")
            print(f"  Snapshots in FFB mode: {np.sum(tracker['ffb_regime'] == 1)}")
            
            if len(tracker['masses']) > 0:
                print(f"  Mass range: {tracker['masses'].min():.2e} to {tracker['masses'].max():.2e} Msun")
                print(f"  Redshift range: {tracker['redshifts'].min():.2f} to {tracker['redshifts'].max():.2f}")
            
            # Show last few snapshots
            if len(tracker['snapshots']) > 0:
                print(f"  Last 3 snapshots:")
                for j in range(max(-3, -len(tracker['snapshots'])), 0):
                    snap = tracker['snapshots'][j]
                    print(f"    Snap {snap}: M*={tracker['masses'][j]:.2e}, "
                          f"Mvir={tracker['mvirs'][j]:.2e}, "
                          f"Type={tracker['types'][j]}, "
                          f"mergeType={tracker['merge_types'][j]}")


def main():
    """Main diagnostic routine"""
    print("\n" + "="*80)
    print(" "*20 + "FFB FATE DIAGNOSTICS (FIXED)")
    print("="*80)
    
    ensure_output_dir()
    
    # Trace fates
    galaxy_tracker, fates, z_array, ffb_present = trace_ffb_fates()
    
    # Print statistics
    print_fate_statistics(galaxy_tracker, fates)
    
    # Create plots
    plot_fate_diagnostics(galaxy_tracker, fates, z_array, ffb_present)
    
    # Show specific examples
    analyze_specific_examples(galaxy_tracker, fates, n_examples=3)
    
    print("\n" + "="*80)
    print("DIAGNOSTICS COMPLETE!")
    print(f"Plots saved to: {OutputDir}")
    print("="*80 + "\n")


if __name__ == "__main__":
    main()