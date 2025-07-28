import numpy as np
import matplotlib.pyplot as plt
import h5py as h5
import os
from random import seed, sample

# Memory optimization settings for large HDF5 files
import gc  # For garbage collection
gc.enable()  # Enable automatic garbage collection

# File details
DirName = './output/millennium_FIRE/'

# Simulation details
Hubble_h = 0.73
FirstSnap = 0
LastSnap = 63
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

# Your original data
original_data = """3.1664167916041976, 11.540892193308549
3.2743628185907045, 11.475836431226766
3.4182908545727138, 11.568773234200743
3.6521739130434776, 11.447955390334572
5.037481259370314, 11.345724907063197
5.235382308845577, 11.122676579925649
5.271364317841079, 10.983271375464684
5.109445277361319, 10.973977695167285
5.199400299850074, 10.881040892193308
5.217391304347826, 10.788104089219331
5.217391304347826, 10.788104089219331
5.667166416791604, 10.75092936802974
6.08095952023988, 10.602230483271375
6.152923538230884, 10.490706319702602
6.332833583208395, 10.472118959107807
6.566716641679159, 10.527881040892193
6.602698650674662, 10.425650557620816
6.4947526236881545, 10.434944237918215
6.944527736131934, 10.369888475836431
7.37631184407796, 10.481412639405203
7.05247376311844, 10.565055762081784
6.926536731634182, 10.769516728624534
6.638680659670164, 10.760223048327138
5.991004497751124, 10.741635687732341
6.044977511244377, 10.741635687732341
6.206896551724138, 10.936802973977695
6.260869565217391, 10.992565055762082
6.2968515742128925, 11.066914498141264
6.350824587706147, 11.122676579925649
6.656671664167916, 11.215613382899628
7.124437781109444, 11.308550185873607
7.214392803598201, 10.927509293680297
7.250374812593702, 10.853159851301115
7.520239880059969, 10.360594795539033
7.9880059970014985, 10.276951672862452
7.754122938530734, 10.026022304832713
8.18590704647676, 10.211895910780669
8.581709145427284, 10.555762081784387
8.761619190404796, 10.118959107806692
8.32983508245877, 9.895910780669144
8.599700149925036, 9.87732342007435
9.13943028485757, 9.895910780669144
9.15742128935532, 9.821561338289962
9.409295352323836, 9.812267657992564
9.409295352323836, 9.74721189591078
9.499250374812593, 9.654275092936803
9.661169415292353, 9.78438661710037
9.733133433283356, 9.905204460966543"""

def read_hdf(snap_num=None, param=None):
    """Read data from SAGE model files - optimized for large files"""
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    data_chunks = []
    for model_file in model_files:
        try:
            with h5.File(DirName + model_file, 'r') as f:
                if snap_num in f and param in f[snap_num]:
                    # Read as chunks to save memory
                    data = f[snap_num][param][:]
                    data_chunks.append(data)
        except:
            continue
    
    if not data_chunks:
        return None
    
    # Efficient concatenation
    return np.concatenate(data_chunks) if len(data_chunks) > 1 else data_chunks[0]

def read_hdf_bulk(snap_num=None, params=None):
    """Read multiple parameters at once to minimize file I/O"""
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    result = {param: [] for param in params}
    
    for model_file in model_files:
        try:
            with h5.File(DirName + model_file, 'r') as f:
                if snap_num in f:
                    for param in params:
                        if param in f[snap_num]:
                            data = f[snap_num][param][:]
                            result[param].append(data)
                        else:
                            # Add empty array to maintain alignment
                            result[param].append(np.array([]))
        except:
            continue
    
    # Concatenate all chunks for each parameter
    for param in params:
        if result[param]:
            result[param] = np.concatenate([chunk for chunk in result[param] if len(chunk) > 0])
        else:
            result[param] = None
    
    return result

def get_cosmos_redshift_range():
    """Extract redshift range from COSMOS-Web data"""
    lines = original_data.strip().split('\n')
    obs_z = []
    for line in lines:
        z, mass = line.split(', ')
        obs_z.append(float(z))
    
    return min(obs_z), max(obs_z)

def find_relevant_snapshots(z_min, z_max):
    """Find snapshot indices within the COSMOS-Web redshift range"""
    relevant_snaps = []
    for i, z in enumerate(redshifts):
        if z_min <= z <= z_max:
            relevant_snaps.append(i)
    return relevant_snaps

def find_light_blue_star_galaxies(n_per_snapshot=6):
    """Find multiple light blue star galaxies (top N most massive at each snapshot) in COSMOS-Web redshift range - optimized"""
    
    # Get redshift range from COSMOS-Web data
    z_min, z_max = get_cosmos_redshift_range()
    print(f"COSMOS-Web redshift range: {z_min:.2f} - {z_max:.2f}")
    
    # Find relevant snapshots
    snap_indices = find_relevant_snapshots(z_min, z_max)
    print(f"Looking for light blue star galaxies in {len(snap_indices)} snapshots")
    
    light_blue_star_galaxies = []
    
    for snap_idx in snap_indices:
        try:
            snapshot = f'Snap_{snap_idx}'
            
            # Read only the essential parameters at once
            essential_params = ['StellarMass', 'GalaxyIndex', 'Type']
            data = read_hdf_bulk(snap_num=snapshot, params=essential_params)
            
            stellar_masses = data['StellarMass']
            galaxy_indices = data['GalaxyIndex']
            galaxy_types = data['Type']
            
            if stellar_masses is None or len(stellar_masses) == 0:
                continue
            
            # Convert units
            stellar_masses = stellar_masses * 1.0e10 / Hubble_h
            
            # Filter for central galaxies with stellar mass > 10^8 M☉ (like in original plot)
            valid_mask = (stellar_masses > 1e8)
            
            if np.any(valid_mask):
                valid_masses = stellar_masses[valid_mask]
                valid_indices = galaxy_indices[valid_mask]
                
                # Find the top N most massive galaxies (not just the single most massive)
                if len(valid_masses) >= n_per_snapshot:
                    # Get indices of top N most massive
                    top_indices = np.argsort(valid_masses)[-n_per_snapshot:][::-1]  # Descending order
                else:
                    # Take all available galaxies if fewer than N
                    top_indices = np.argsort(valid_masses)[::-1]
                
                print(f"Snap {snap_idx:2d} (z={redshifts[snap_idx]:.3f}): Found {len(top_indices)} massive galaxies")
                
                for rank, idx in enumerate(top_indices):
                    galaxy_id = valid_indices[idx]
                    stellar_mass = valid_masses[idx]
                    
                    light_blue_star_galaxies.append({
                        'galaxy_index': galaxy_id,
                        'discovery_snap': snap_idx,
                        'discovery_redshift': redshifts[snap_idx],
                        'discovery_mass': stellar_mass,
                        'rank': rank + 1
                    })
                    
                    print(f"    Rank {rank+1}: Galaxy {galaxy_id} with M* = {stellar_mass:.2e} M☉")
                
        except Exception as e:
            print(f"Error processing snapshot {snap_idx}: {e}")
            continue
    
    # Remove duplicates based on galaxy_index, keeping the highest redshift discovery
    unique_galaxies = {}
    for galaxy in light_blue_star_galaxies:
        gal_id = galaxy['galaxy_index']
        if gal_id not in unique_galaxies:
            unique_galaxies[gal_id] = galaxy
        else:
            # Keep the one discovered at higher redshift (where it was first among the most massive)
            if galaxy['discovery_redshift'] > unique_galaxies[gal_id]['discovery_redshift']:
                unique_galaxies[gal_id] = galaxy
    
    final_galaxies = list(unique_galaxies.values())
    print(f"\nFound {len(final_galaxies)} unique light blue star galaxies to trace")
    
    # Sort by discovery redshift for nice output
    final_galaxies.sort(key=lambda x: x['discovery_redshift'], reverse=True)
    
    print("\nUnique galaxies to trace:")
    for galaxy in final_galaxies:
        print(f"  Galaxy {galaxy['galaxy_index']}: first detected at z={galaxy['discovery_redshift']:.3f} "
              f"with M*={galaxy['discovery_mass']:.2e} M☉")
    
    return final_galaxies

def trace_galaxy_evolution(light_blue_star_galaxies):
    """Trace the evolution of specific light blue star galaxies through all snapshots - optimized"""
    
    params = ['StellarMass', 'Mvir', 'SfrDisk', 'SfrBulge', 'ColdGas', 'HI_gas', 'H2_gas', 
              'BulgeMass', 'Type', 'HotGas', 'MetalsColdGas']
    
    galaxy_tracks = {}
    target_galaxy_ids = set()
    
    # Initialize tracks for each galaxy and create set for fast lookup
    for galaxy in light_blue_star_galaxies:
        gal_id = galaxy['galaxy_index']
        target_galaxy_ids.add(gal_id)
        galaxy_tracks[gal_id] = {
            'discovery_info': galaxy,
            'redshifts': [],
            'snapshots': [],
            **{param: [] for param in params}
        }
    
    print(f"\nTracing {len(light_blue_star_galaxies)} light blue star galaxies through all snapshots...")
    
    # Go through all snapshots from high-z to low-z
    for snap_idx in range(LastSnap + 1):
        try:
            snapshot = f'Snap_{snap_idx}'
            
            # Read all necessary parameters at once to minimize I/O
            all_params = ['GalaxyIndex'] + params
            data = read_hdf_bulk(snap_num=snapshot, params=all_params)
            
            galaxy_indices = data['GalaxyIndex']
            if galaxy_indices is None or len(galaxy_indices) == 0:
                continue
            
            # Find which of our target galaxies exist in this snapshot
            target_mask = np.isin(galaxy_indices, list(target_galaxy_ids))
            
            if not np.any(target_mask):
                continue  # None of our target galaxies in this snapshot
            
            # Extract data only for our target galaxies
            target_indices = galaxy_indices[target_mask]
            
            # Process each target galaxy found in this snapshot
            for i, target_id in enumerate(target_indices):
                if target_id in galaxy_tracks:
                    # Find the position of this galaxy in the masked arrays
                    pos_in_mask = np.where(target_mask)[0][i]
                    
                    # Add redshift and snapshot info
                    galaxy_tracks[target_id]['redshifts'].append(redshifts[snap_idx])
                    galaxy_tracks[target_id]['snapshots'].append(snap_idx)
                    
                    # Extract all parameters for this galaxy
                    for param in params:
                        if data[param] is not None and len(data[param]) > pos_in_mask:
                            value = data[param][pos_in_mask]
                            # Convert mass units
                            if param in ['StellarMass', 'Mvir', 'ColdGas', 'HI_gas', 'H2_gas', 'BulgeMass', 'HotGas', 'MetalsColdGas']:
                                value *= 1.0e10 / Hubble_h
                            galaxy_tracks[target_id][param].append(value)
                        else:
                            galaxy_tracks[target_id][param].append(np.nan)
            
            # Force garbage collection after each snapshot to free memory
            del data
            gc.collect()
            
        except Exception as e:
            print(f"Error processing snapshot {snap_idx}: {e}")
            continue
    
    # Convert lists to numpy arrays and sort by redshift (high to low)
    for gal_id in galaxy_tracks:
        track = galaxy_tracks[gal_id]
        if len(track['redshifts']) > 0:
            # Sort by redshift (descending - high z to low z)
            sort_idx = np.argsort(track['redshifts'])[::-1]
            
            track['redshifts'] = np.array(track['redshifts'])[sort_idx]
            track['snapshots'] = np.array(track['snapshots'])[sort_idx]
            
            for param in params:
                track[param] = np.array(track[param])[sort_idx]
            
            print(f"Galaxy {gal_id}: tracked through {len(track['redshifts'])} snapshots "
                  f"(z={track['redshifts'][0]:.3f} to z={track['redshifts'][-1]:.3f})")
    
    return galaxy_tracks

def plot_galaxy_tracks(galaxy_tracks):
    """Plot evolutionary tracks for light blue star galaxies"""
    
    if len(galaxy_tracks) == 0:
        print("No galaxy tracks to plot!")
        return
    
    # Create figure with 6 subplots (2 rows, 3 columns)
    fig, ((ax1, ax2, ax3), (ax4, ax5, ax6)) = plt.subplots(2, 3, figsize=(18, 12))
    
    # Sort galaxies by discovery mass (highest mass first) for alpha assignment
    sorted_tracks = sorted(galaxy_tracks.items(), 
                          key=lambda x: x[1]['discovery_info']['discovery_mass'], 
                          reverse=True)
    
    # Colors for different galaxies using plasma_r (earlier/higher z = yellow/orange)
    # Sort by discovery redshift for color assignment
    redshift_sorted_tracks = sorted(galaxy_tracks.items(), 
                                   key=lambda x: x[1]['discovery_info']['discovery_redshift'], 
                                   reverse=True)
    colors = plt.cm.plasma_r(np.linspace(0, 1, len(redshift_sorted_tracks)))
    
    # Create color mapping based on redshift order
    color_map = {}
    for i, (gal_id, track) in enumerate(redshift_sorted_tracks):
        color_map[gal_id] = colors[i]
    
    # Track if we have any satellites for legend
    has_satellites = False
    has_centrals = False
    
    for i, (gal_id, track) in enumerate(sorted_tracks):
        if len(track['redshifts']) < 2:
            continue
        # Assign alpha based on mass ranking (20 least massive get lower alpha)
        if i >= len(sorted_tracks) - 64:  # Last 64 galaxies (least massive)
            alpha = 0.10
            linewidth = 1.0
        else:  # Most massive galaxies
            alpha = 1.0
            linewidth = 2.0

        redshifts_gal = track['redshifts']
        stellar_masses = track['StellarMass']
        halo_masses = track['Mvir']
        cold_gas = track['ColdGas']
        h1_gas = track['HI_gas']
        h2_gas = track['H2_gas']
        metals_cold_gas = track['MetalsColdGas']
        galaxy_types = track['Type']
        sfr_disk = np.nan_to_num(track['SfrDisk'], nan=0.0)
        sfr_bulge = np.nan_to_num(track['SfrBulge'], nan=0.0)
        total_sfr = sfr_disk + sfr_bulge
        
        color = color_map[gal_id]  # Use color based on redshift order
        
        # Determine line style based on galaxy type
        is_ever_satellite = np.any(galaxy_types == 1)
        linestyle = '--' if is_ever_satellite else '-'
        
        # Track what types we have for legend
        if is_ever_satellite:
            has_satellites = True
        else:
            has_centrals = True
        
        # Plot 1: Stellar Mass vs Redshift
        valid_mask = ~np.isnan(stellar_masses) & (stellar_masses > 0)
        if np.any(valid_mask):
            ax2.plot(redshifts_gal[valid_mask], np.log10(stellar_masses[valid_mask]), 
                     linestyle=linestyle, linewidth=linewidth, color=color, alpha=alpha)
        
        # Plot 2: Halo Mass vs Redshift
        valid_mask = ~np.isnan(halo_masses) & (halo_masses > 0)
        if np.any(valid_mask):
            ax1.plot(redshifts_gal[valid_mask], np.log10(halo_masses[valid_mask]), 
                     linestyle=linestyle, linewidth=linewidth, color=color, alpha=alpha)
        
        # Plot 3: Total SFR vs Redshift (log scale)
        valid_mask = total_sfr > 0
        if np.any(valid_mask):
            ax3.plot(redshifts_gal[valid_mask], np.log10(total_sfr[valid_mask]), 
                     linestyle=linestyle, linewidth=linewidth, color=color, alpha=alpha)
        
        # Plot 4: Cold Gas vs Redshift
        valid_mask = ~np.isnan(cold_gas) & (cold_gas > 0)
        if np.any(valid_mask):
            ax4.plot(redshifts_gal[valid_mask], np.log10(cold_gas[valid_mask]), 
                     linestyle=linestyle, linewidth=linewidth, color=color, alpha=alpha)
        
        # Plot 5: H2 fraction (H2_gas / ColdGas) vs Redshift
        h2_fraction = np.zeros_like(h2_gas)
        valid_gas = cold_gas > 0
        h2_fraction[valid_gas] = h2_gas[valid_gas] / (h2_gas[valid_gas] + h1_gas[valid_gas])
        # Ensure fraction is between 0 and 1
        h2_fraction = np.clip(h2_fraction, 0.0, 1.0)
        valid_mask = ~np.isnan(h2_fraction) & (h2_fraction > 0)
        if np.any(valid_mask):
            ax5.plot(redshifts_gal[valid_mask], h2_fraction[valid_mask], 
                     linestyle=linestyle, linewidth=linewidth, color=color, alpha=alpha)
        
        # Plot 6: Metallicity vs Redshift (using exact formula provided)
        metallicity = np.full_like(metals_cold_gas, np.nan)
        
        # Apply the exact filtering condition: central galaxies with gas fraction > 0.1 and M* > 1e8
        gas_fraction = np.zeros_like(cold_gas)
        valid_total = (stellar_masses + cold_gas) > 0
        gas_fraction[valid_total] = cold_gas[valid_total] / (stellar_masses[valid_total] + cold_gas[valid_total])
        
        w = (gas_fraction > 0.1) & (stellar_masses > 1.0e8) & (cold_gas > 0) & (metals_cold_gas > 0)
        
        if np.any(w):
            # Use exact formula: Z = log10((MetalsColdGas / ColdGas) / 0.02) + 9.0
            metallicity[w] = np.log10((metals_cold_gas[w] / cold_gas[w]) / 0.02) + 9.0
            
        valid_mask = ~np.isnan(metallicity) & np.isfinite(metallicity)
        if np.any(valid_mask):
            ax6.plot(redshifts_gal[valid_mask], metallicity[valid_mask], 
                     linestyle=linestyle, linewidth=linewidth, color=color, alpha=alpha)
    
    # Format plots (no titles, no grids)
    # Top row
    # ax2.set_xlabel('Redshift', fontsize=12)
    ax2.set_ylabel(r'$\log_{10}\ M_{star}\ (M_\odot)$', fontsize=12)
    ax2.set_xlim(0,14)
    
    # ax1.set_xlabel('Redshift', fontsize=12)
    ax1.set_ylabel(r'$\log_{10}\ M_{vir}\ (M_\odot)$', fontsize=12)
    ax1.set_xlim(0,14)

    # ax3.set_xlabel('Redshift', fontsize=12)
    ax3.set_ylabel(r'$\log_{10}\ \mathrm{SFR}\ (M_\odot/\mathrm{yr})$', fontsize=12)
    ax3.set_xlim(0,14)
    
    # Bottom row
    ax4.set_xlabel('Redshift', fontsize=12)
    ax4.set_ylabel(r'$\log_{10}\ M_{cold}\ (M_\odot)$', fontsize=12)
    ax4.set_xlim(0,14)
    
    ax5.set_xlabel('Redshift', fontsize=12)
    ax5.set_ylabel(r'$\mathrm{H_{2}}\ \mathrm{fraction}$', fontsize=12)
    ax5.set_xlim(0,14)

    ax6.set_xlabel('Redshift', fontsize=12)
    ax6.set_ylabel(r'$12\ +\ \log_{10}[\mathrm{O/H}]$', fontsize=12)
    ax6.set_xlim(0,14)
    
    # Calculate percentage of galaxies that end up as satellites and quiescent
    total_galaxies = len(sorted_tracks)
    satellite_end_count = 0
    quiescent_end_count = 0
    quiescent_start_count = 0
    sSFR_cut = -11.0  # log10(sSFR) cut for quiescence
    
    for gal_id, track in sorted_tracks:
        if len(track['Type']) > 0:
            final_type = track['Type'][-1]  # Last (z=0 or latest) type
            if final_type == 1:  # Satellite
                satellite_end_count += 1
        
        # Calculate final sSFR for quiescence determination
        if (len(track['StellarMass']) > 0 and len(track['SfrDisk']) > 0 and len(track['SfrBulge']) > 0):
            final_stellar_mass = track['StellarMass'][-1]
            final_sfr_disk = track['SfrDisk'][-1] if not np.isnan(track['SfrDisk'][-1]) else 0.0
            final_sfr_bulge = track['SfrBulge'][-1] if not np.isnan(track['SfrBulge'][-1]) else 0.0
            final_total_sfr = final_sfr_disk + final_sfr_bulge
            
            if final_stellar_mass > 0 and final_total_sfr > 0:
                final_sSFR = np.log10(final_total_sfr / final_stellar_mass)
                if final_sSFR < sSFR_cut:
                    quiescent_end_count += 1
            elif final_total_sfr == 0:  # No star formation = quiescent
                quiescent_end_count += 1
        
        # Calculate initial sSFR for quiescence determination (at discovery)
        if (len(track['StellarMass']) > 0 and len(track['SfrDisk']) > 0 and len(track['SfrBulge']) > 0):
            initial_stellar_mass = track['StellarMass'][0]  # First snapshot (highest z)
            initial_sfr_disk = track['SfrDisk'][0] if not np.isnan(track['SfrDisk'][0]) else 0.0
            initial_sfr_bulge = track['SfrBulge'][0] if not np.isnan(track['SfrBulge'][0]) else 0.0
            initial_total_sfr = initial_sfr_disk + initial_sfr_bulge
            
            if initial_stellar_mass > 0 and initial_total_sfr > 0:
                initial_sSFR = np.log10(initial_total_sfr / initial_stellar_mass)
                if initial_sSFR < sSFR_cut:
                    quiescent_start_count += 1
            elif initial_total_sfr == 0:  # No star formation = quiescent
                quiescent_start_count += 1
    
    satellite_percentage = (satellite_end_count / total_galaxies * 100) if total_galaxies > 0 else 0
    quiescent_percentage = (quiescent_end_count / total_galaxies * 100) if total_galaxies > 0 else 0
    quiescent_start_percentage = (quiescent_start_count / total_galaxies * 100) if total_galaxies > 0 else 0
    
    # Create simple legend with dummy lines
    legend_elements = []
    if has_centrals:
        legend_elements.append(plt.Line2D([0], [0], color='gray', linestyle='-', linewidth=2, label='Central'))
    if has_satellites:
        legend_elements.append(plt.Line2D([0], [0], color='gray', linestyle='--', linewidth=2, label='Satellite'))
    
    # Add satellite and quiescent percentage entries
    legend_elements.append(plt.Line2D([0], [0], color='none', label=f'{satellite_percentage:.1f}% end as satellites'))
    legend_elements.append(plt.Line2D([0], [0], color='none', label=f'{quiescent_start_percentage:.1f}% start quenched (sSFR < -11)'))
    legend_elements.append(plt.Line2D([0], [0], color='none', label=f'{quiescent_percentage:.1f}% end quenched (sSFR < -11)'))

    # Place legend horizontally at bottom, outside plots
    if legend_elements:
        fig.legend(handles=legend_elements, loc='lower center', ncol=len(legend_elements), 
                  bbox_to_anchor=(0.5, -0.02), fontsize=12)
    
    plt.tight_layout()
    plt.savefig('light_blue_star_galaxy_tracks.pdf', dpi=500, bbox_inches='tight')
    print(f"\nEvolutionary tracks saved as 'light_blue_star_galaxy_tracks.pdf'")
    print("Color scheme: plasma_r (yellow/orange = earlier discovery redshift)")
    print("Line styles: solid = central, dashed = satellite")
    
    return galaxy_tracks

def plot_z0_descendants(galaxy_tracks):
    """Plot 'Where Are They Now?' - z=0 descendants of JWST-like galaxies"""
    
    if len(galaxy_tracks) == 0:
        print("No galaxy tracks to plot!")
        return
    
    # Create single figure
    fig, ax = plt.subplots(1, 1, figsize=(10, 8))
    
    # Collect z=0 data
    z0_stellar_masses = []
    z0_types = []
    z0_halo_masses = []
    discovery_redshifts = []
    galaxy_ids = []
    
    for gal_id, track in galaxy_tracks.items():
        if len(track['redshifts']) == 0:
            continue
            
        # Get final (z=0 or latest) properties
        final_stellar_mass = track['StellarMass'][-1] if len(track['StellarMass']) > 0 else np.nan
        final_type = track['Type'][-1] if len(track['Type']) > 0 else np.nan
        final_halo_mass = track['Mvir'][-1] if len(track['Mvir']) > 0 else np.nan
        discovery_z = track['discovery_info']['discovery_redshift']
        
        if not (np.isnan(final_stellar_mass) or np.isnan(final_type)):
            z0_stellar_masses.append(final_stellar_mass)
            z0_types.append(final_type)
            z0_halo_masses.append(final_halo_mass)
            discovery_redshifts.append(discovery_z)
            galaxy_ids.append(gal_id)
    
    z0_stellar_masses = np.array(z0_stellar_masses)
    z0_types = np.array(z0_types)
    z0_halo_masses = np.array(z0_halo_masses)
    discovery_redshifts = np.array(discovery_redshifts)
    
    if len(z0_stellar_masses) == 0:
        print("No z=0 data to plot!")
        return
    
    # Define environment categories based on galaxy type and halo mass
    # Type 0 = Central, Type 1 = Satellite, Type 2 = Disrupted
    environment_labels = []
    y_positions = []
    colors = []
    
    for i, (gtype, halo_mass, stellar_mass) in enumerate(zip(z0_types, z0_halo_masses, z0_stellar_masses)):
        if gtype == 0:  # Central galaxy
            if not np.isnan(halo_mass) and halo_mass > 1e14:  # Very massive halo
                if stellar_mass > 1e11:  # Very massive stellar mass
                    env_label = "BCG/Massive Central"
                    y_pos = 3.0
                    color = 'red'
                else:
                    env_label = "Group Central"
                    y_pos = 2.5
                    color = 'orange'
            elif not np.isnan(halo_mass) and halo_mass > 1e13:  # Group-scale halo
                env_label = "Group Central"
                y_pos = 2.0
                color = 'orange'
            else:  # Field central
                env_label = "Field Central"
                y_pos = 1.0
                color = 'blue'
        elif gtype == 1:  # Satellite
            if not np.isnan(halo_mass) and halo_mass > 1e14:  # In massive cluster
                env_label = "Cluster Satellite"
                y_pos = 0.5
                color = 'purple'
            else:  # In group
                env_label = "Group Satellite"
                y_pos = 0.0
                color = 'darkred'
        else:  # Disrupted or other
            env_label = "Disrupted/Other"
            y_pos = -0.5
            color = 'gray'
        
        environment_labels.append(env_label)
        y_positions.append(y_pos)
        colors.append(color)
    
    y_positions = np.array(y_positions)
    
    # Add some scatter to y-positions to avoid overlap
    np.random.seed(42)  # For reproducibility
    y_jitter = np.random.normal(0, 0.1, len(y_positions))
    y_positions_jittered = y_positions + y_jitter
    
    # Create color map based on discovery redshift (same as main plot)
    discovery_colors = plt.cm.plasma_r(np.linspace(0, 1, len(discovery_redshifts)))
    redshift_order = np.argsort(discovery_redshifts)[::-1]  # Highest z first
    
    # Plot points
    for i in range(len(z0_stellar_masses)):
        color_idx = np.where(redshift_order == i)[0][0]
        ax.scatter(np.log10(z0_stellar_masses[i]), y_positions_jittered[i], 
                  c=[discovery_colors[color_idx]], s=300, alpha=0.8, 
                  edgecolors='black', linewidth=1, marker='*', zorder=3)

    # Formatting
    ax.set_xlabel(r'$\log_{10}\ M_{star}\ (M_\odot)$', fontsize=14)
    # ax.set_ylabel('Environment (z=0)', fontsize=14)
    
    # Set y-axis labels
    unique_y_pos = []
    unique_labels = []
    for y_pos, label in zip(y_positions, environment_labels):
        if y_pos not in unique_y_pos:
            unique_y_pos.append(y_pos)
            unique_labels.append(label)
    
    # Sort by y position
    sorted_indices = np.argsort(unique_y_pos)
    unique_y_pos = [unique_y_pos[i] for i in sorted_indices]
    unique_labels = [unique_labels[i] for i in sorted_indices]
    
    ax.set_yticks(unique_y_pos)
    ax.set_yticklabels(unique_labels, fontsize=12)
    ax.set_ylim(-1, 3.5)

    # Add colorbar for discovery redshift
    sm = plt.cm.ScalarMappable(cmap=plt.cm.plasma_r, 
                              norm=plt.Normalize(vmin=min(discovery_redshifts), 
                                               vmax=max(discovery_redshifts)))
    sm.set_array([])
    cbar = plt.colorbar(sm, ax=ax, pad=0.02)
    cbar.set_label('Discovery Redshift', fontsize=12)
    
    # Add statistics text
    n_bcg = sum([1 for y in y_positions if y >= 2.5])
    n_centrals = sum([1 for y in y_positions if 1.0 <= y < 2.5])
    n_satellites = sum([1 for y in y_positions if -0.5 <= y < 1.0])
    total = len(y_positions)
    
    stats_text = f'Sample: {total} JWST-like galaxies\n'
    stats_text += f'BCG/Massive Centrals: {n_bcg} ({n_bcg/total*100:.1f}%)\n'
    stats_text += f'Group/Field Centrals: {n_centrals} ({n_centrals/total*100:.1f}%)\n'
    stats_text += f'Satellites: {n_satellites} ({n_satellites/total*100:.1f}%)'
    
    ax.text(0.02, 0.98, stats_text, transform=ax.transAxes, fontsize=10,
        verticalalignment='top', horizontalalignment='left',
        bbox=dict(facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig('jwst_galaxies_z0_descendants.pdf', dpi=500, bbox_inches='tight')
    print(f"\nz=0 descendants plot saved as 'jwst_galaxies_z0_descendants.pdf'")
    print(f"Color scheme: plasma_r (yellow/orange = earlier discovery redshift)")
    
    return

def plot_quenching_timeline(galaxy_tracks):
    """Plot quenching timeline analysis - quiescent fraction vs redshift for different mass bins"""
    
    if len(galaxy_tracks) == 0:
        print("No galaxy tracks to plot!")
        return
    
    # Create single figure
    fig, ax = plt.subplots(1, 1, figsize=(10, 8))
    
    # Define stellar mass bins (log10 solar masses)
    mass_bins = [
        (8.0, 8.5, r"$10^{8.0-8.5}\ M_{\odot}$"),
        (8.5, 9.0, r"$10^{8.5-9.0}\ M_{\odot}$"),
        (9.0, 9.5, r"$10^{9.0-9.5}\ M_{\odot}$"),
        (9.5, 10.0, r"$10^{9.5-10.0}\ M_{\odot}$"),
        (10.0, 10.5, r"$10^{10.0-10.5}\ M_{\odot}$"),
        (10.5, 11.0, r"$10^{10.5-11.0}\ M_{\odot}$"),
        (11.0, 11.5, r"$10^{11.0-11.5}\ M_{\odot}$"),
        (11.5, 12.0, r"$10^{11.5-12.0}\ M_{\odot}$"),
        (12.0, 12.5, r"$10^{12.0-12.5}\ M_{\odot}$"),
        (12.5, 13.0, r"$10^{12.5-13.0}\ M_{\odot}$"),
        (13.0, 13.5, r"$10^{13.0-13.5}\ M_{\odot}$"),
        (13.5, 14.0, r"$10^{13.5-14.0}\ M_{\odot}$"),
        (14.0, 14.5, r"$10^{14.0-14.5}\ M_{\odot}$"),
        (14.5, 15.0, r"$10^{14.5-15.0}\ M_{\odot}$")
    ]
    
    # Use plasma_r colormap for mass bins
    colors = plt.cm.plasma_r(np.linspace(0, 1, len(mass_bins)))
    sSFR_cut = -11.0  # log10(sSFR) cut for quiescence
    
    # Get unique redshifts from all tracks and sort from high to low z
    all_redshifts = set()
    for track in galaxy_tracks.values():
        all_redshifts.update(track['redshifts'])
    unique_redshifts = sorted(list(all_redshifts), reverse=True)
    
    # For each mass bin, calculate quiescent fraction at each redshift
    for bin_idx, (mass_min, mass_max, mass_label) in enumerate(mass_bins):
        quiescent_fractions = []
        redshift_points = []
        
        for z in unique_redshifts:
            # Find galaxies in this mass bin at this redshift
            galaxies_in_bin = []
            quiescent_count = 0
            
            for gal_id, track in galaxy_tracks.items():
                if len(track['redshifts']) == 0:
                    continue
                
                # Find the index for this redshift (or closest)
                z_diffs = np.abs(np.array(track['redshifts']) - z)
                if np.min(z_diffs) < 0.1:  # Within 0.1 in redshift
                    z_idx = np.argmin(z_diffs)
                    
                    # Get stellar mass at this redshift
                    stellar_mass = track['StellarMass'][z_idx]
                    
                    if not np.isnan(stellar_mass) and mass_min <= np.log10(stellar_mass) < mass_max:
                        galaxies_in_bin.append(gal_id)
                        
                        # Check if quiescent at this redshift
                        sfr_disk = track['SfrDisk'][z_idx] if not np.isnan(track['SfrDisk'][z_idx]) else 0.0
                        sfr_bulge = track['SfrBulge'][z_idx] if not np.isnan(track['SfrBulge'][z_idx]) else 0.0
                        total_sfr = sfr_disk + sfr_bulge
                        
                        if stellar_mass > 0 and total_sfr > 0:
                            sSFR = np.log10(total_sfr / stellar_mass)
                            if sSFR < sSFR_cut:
                                quiescent_count += 1
                        elif total_sfr == 0:  # No star formation = quiescent
                            quiescent_count += 1
            
            # Calculate quiescent fraction if we have enough galaxies
            if len(galaxies_in_bin) >= 3:  # Minimum sample size
                quiescent_fraction = quiescent_count / len(galaxies_in_bin)
                quiescent_fractions.append(quiescent_fraction)
                redshift_points.append(z)
        
        # Plot the line for this mass bin
        if len(redshift_points) > 1:
            ax.plot(redshift_points, quiescent_fractions, 
                   color=colors[bin_idx], linewidth=3.0, label=f'M* = {mass_label}')
    
    # Formatting
    ax.set_xlabel('Redshift', fontsize=14)
    ax.set_ylabel('Quiescent Fraction', fontsize=14)
    
    # Set limits and grid
    ax.set_xlim(0, 5)
    ax.set_ylim(0, 1.0)
    
    # Add legend
    ax.legend(fontsize=12, loc='upper right', frameon= False)
    
    plt.tight_layout()
    plt.savefig('jwst_galaxies_quenching_timeline.pdf', dpi=500, bbox_inches='tight')
    print(f"\nQuenching timeline plot saved as 'jwst_galaxies_quenching_timeline.pdf'")
    print("Shows quiescent fraction vs redshift for different stellar mass bins")
    
    return

def print_track_summary(galaxy_tracks):
    """Print summary of light blue star galaxy tracks"""
    print("\n" + "="*80)
    print("LIGHT BLUE STAR GALAXY EVOLUTIONARY TRACKS SUMMARY")
    print("="*80)
    
    for gal_id, track in galaxy_tracks.items():
        if len(track['redshifts']) == 0:
            continue
            
        discovery = track['discovery_info']
        galaxy_types = track['Type']
        
        # Analyze galaxy type evolution
        is_ever_satellite = np.any(galaxy_types == 1)
        is_ever_central = np.any(galaxy_types == 0)
        
        if is_ever_satellite and is_ever_central:
            type_evolution = "central → satellite"
        elif is_ever_satellite:
            type_evolution = "always satellite"
        else:
            type_evolution = "always central"
            
        print(f"\nGalaxy {gal_id}:")
        print(f"  Light blue star at: z = {discovery['discovery_redshift']:.3f} (Snap {discovery['discovery_snap']})")
        print(f"  Star discovery mass: {discovery['discovery_mass']:.2e} M☉")
        print(f"  Type evolution: {type_evolution}")
        print(f"  Tracked through: {len(track['redshifts'])} snapshots")
        print(f"  Redshift range: z = {track['redshifts'][0]:.3f} to z = {track['redshifts'][-1]:.3f}")
        
        # Final properties (z=0 or latest)
        final_stellar = track['StellarMass'][-1] if len(track['StellarMass']) > 0 else np.nan
        final_halo = track['Mvir'][-1] if len(track['Mvir']) > 0 else np.nan
        final_type = track['Type'][-1] if len(track['Type']) > 0 else np.nan
        type_name = "central" if final_type == 0 else "satellite" if final_type == 1 else "disrupted" if final_type == 2 else f"type_{final_type}"
        
        print(f"  Final stellar mass: {final_stellar:.2e} M☉")
        print(f"  Final halo mass: {final_halo:.2e} M☉")
        print(f"  Final type: {type_name}")
        
        # Count time spent as each type
        central_count = np.sum(galaxy_types == 0)
        satellite_count = np.sum(galaxy_types == 1)
        total_count = len(galaxy_types)
        
        if total_count > 0:
            central_fraction = central_count / total_count * 100
            satellite_fraction = satellite_count / total_count * 100
            print(f"  Time as central: {central_fraction:.1f}% ({central_count}/{total_count} snapshots)")
            print(f"  Time as satellite: {satellite_fraction:.1f}% ({satellite_count}/{total_count} snapshots)")

if __name__ == '__main__':
    if not os.path.exists(DirName):
        print(f"Error: Directory {DirName} not found!")
        print("Please update DirName to point to your SAGE output directory.")
    else:
        print("="*80)
        print("TRACING LIGHT BLUE STAR GALAXY EVOLUTION")
        print("="*80)
        
        # Step 1: Find light blue star galaxies (top 5 most massive per snapshot during COSMOS-Web epoch)
        light_blue_star_galaxies = find_light_blue_star_galaxies(n_per_snapshot=25)
        
        # Step 2: Trace their evolution through all snapshots
        galaxy_tracks = trace_galaxy_evolution(light_blue_star_galaxies)
        
        # Step 3: Plot evolutionary tracks
        plot_galaxy_tracks(galaxy_tracks)
        
        # Step 4: Plot z=0 descendants
        plot_z0_descendants(galaxy_tracks)
        
        # Step 5: Plot quenching timeline
        plot_quenching_timeline(galaxy_tracks)
        
        # Step 6: Print summary
        print_track_summary(galaxy_tracks)