import h5py
import numpy as np
import matplotlib.pyplot as plt
import os
from scipy.spatial import cKDTree

def find_most_massive_halo(base_dir, snap_num, min_mass=1e11, Hubble_h=0.73):
    """
    Find the most massive halo in a given snapshot with mass above min_mass.
    
    Parameters:
    -----------
    base_dir : str
        Directory where model files are stored
    snap_num : int
        Snapshot number to search in
    min_mass : float
        Minimum halo mass in Msun (not in log10)
    Hubble_h : float
        Hubble parameter (default 0.73)
    
    Returns:
    --------
    tuple
        (galaxy_id, halo_mass, file_name, index, position) of the most massive halo
    """
    
    snapshot_name = f'Snap_{snap_num}'
    model_files = [f for f in os.listdir(base_dir) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    max_mass = 0
    max_galaxy_id = None
    max_file = None
    max_idx = None
    max_pos = None
    
    print(f"Searching for the most massive halo in snapshot {snap_num}...")
    
    for model_file in model_files:
        try:
            with h5py.File(os.path.join(base_dir, model_file), 'r') as f:
                if snapshot_name not in f:
                    continue
                
                # Get masses and convert to physical units
                mvir = np.array(f[snapshot_name]['Mvir']) * 1.0e10 / Hubble_h
                
                # Find halos above minimum mass
                valid_indices = np.where(mvir > min_mass)[0]
                
                if len(valid_indices) > 0:
                    # Find the most massive among these
                    local_max_idx = valid_indices[np.argmax(mvir[valid_indices])]
                    local_max_mass = mvir[local_max_idx]
                    
                    if local_max_mass > max_mass:
                        max_mass = local_max_mass
                        # Use GalaxyIndex as the unique identifier
                        max_galaxy_id = f[snapshot_name]['GalaxyIndex'][local_max_idx]
                        max_file = model_file
                        max_idx = local_max_idx
                        
                        # Get position
                        max_pos = [
                            f[snapshot_name]['Posx'][local_max_idx],
                            f[snapshot_name]['Posy'][local_max_idx],
                            f[snapshot_name]['Posz'][local_max_idx]
                        ]
                        
                        # Print some information about this halo
                        stellar_mass = f[snapshot_name]['StellarMass'][local_max_idx] * 1.0e10 / Hubble_h
                        print(f"  Found halo with mass {np.log10(max_mass):.3f} log10 Msun in {model_file}")
                        print(f"  Stellar mass: {np.log10(stellar_mass):.3f} log10 Msun, GalaxyIndex: {max_galaxy_id}")
                    
        except Exception as e:
            print(f"Error processing file {model_file}: {e}")
    
    if max_galaxy_id is None:
        print(f"No halos found with mass > {np.log10(min_mass):.3f} log10 Msun")
        return None, None, None, None, None
    
    print(f"\nSelected most massive halo: GalaxyIndex {max_galaxy_id}, Mass {np.log10(max_mass):.3f} log10 Msun")
    return max_galaxy_id, max_mass, max_file, max_idx, max_pos

def track_galaxy_by_id(base_dir, start_snap, end_snap, galaxy_id=None, min_mass=1e12, Hubble_h=0.73):
    """
    Track a galaxy across snapshots using its GalaxyIndex.
    If galaxy_id is None, will find the most massive halo in start_snap and track it.
    
    Parameters:
    -----------
    base_dir : str
        Directory where model files are stored
    start_snap : int
        Starting snapshot number (typically z=0)
    end_snap : int
        Ending snapshot number (higher redshift)
    galaxy_id : int or None
        GalaxyIndex to track. If None, find most massive halo in start_snap.
    min_mass : float
        Minimum halo mass to consider (only used if galaxy_id is None)
    Hubble_h : float
        Hubble parameter
        
    Returns:
    --------
    dict
        Dictionary containing the tracked properties across snapshots
    """
    # Initialize storage for tracked properties
    tracked_data = {
        'snapnum': [],
        'log10_mvir': [],
        'log10_stellar_mass': [],
        'position': [],
        'galaxy_id': [],
        'black_hole_mass': [],
        'sfr': [],
        'type': [],
        'cold_gas': [],
        'hot_gas': [],
        'vvir': []
    }
    
    # If no galaxy_id provided, find most massive halo in starting snapshot
    if galaxy_id is None:
        galaxy_id, halo_mass, _, _, _ = find_most_massive_halo(
            base_dir, start_snap, min_mass, Hubble_h)
        
        if galaxy_id is None:
            print(f"Failed to find suitable halo in snapshot {start_snap}")
            return tracked_data
    
    # Determine direction of tracking
    step = -1 if end_snap < start_snap else 1
    snaps_range = range(start_snap, end_snap + step, step)
    
    # Track the galaxy across snapshots
    for snap in snaps_range:
        snapshot_name = f'Snap_{snap}'
        model_files = [f for f in os.listdir(base_dir) if f.startswith('model_') and f.endswith('.hdf5')]
        model_files.sort()
        
        found = False
        properties = {}
        
        # Search for the galaxy by GalaxyIndex in all files for this snapshot
        for model_file in model_files:
            try:
                with h5py.File(os.path.join(base_dir, model_file), 'r') as f:
                    if snapshot_name not in f:
                        continue
                        
                    # Get all GalaxyIndexs in this file
                    galaxy_ids = np.array(f[snapshot_name]['GalaxyIndex'])
                    
                    # Find the index of our target galaxy
                    indices = np.where(galaxy_ids == galaxy_id)[0]
                    
                    if len(indices) > 0:
                        idx = indices[0]
                        properties = extract_properties(f, snapshot_name, idx, Hubble_h)
                        found = True
                        break
            except Exception as e:
                print(f"Error processing snapshot {snap} in file {model_file}: {e}")
        
        # Add data if found
        if found:
            for key, value in properties.items():
                if key in tracked_data:
                    tracked_data[key].append(value)
            
            # Add snapshot number separately
            tracked_data['snapnum'].append(snap)
            
            print(f"Successfully tracked galaxy in snapshot {snap}: "\
                  f"GalaxyIndex={properties['galaxy_id']}, log10(Mvir)={properties['log10_mvir']:.3f}")
        else:
            print(f"Failed to find galaxy with ID={galaxy_id} in snapshot {snap}")
            # Could be that the galaxy doesn't exist yet at this redshift
    
    return tracked_data

def extract_properties(file_handle, snapshot_name, index, Hubble_h):
    """Extract all relevant properties for a galaxy at the given index"""
    properties = {}
    
    # Extract basic properties
    mvir = file_handle[snapshot_name]['Mvir'][index] * 1.0e10 / Hubble_h
    properties['log10_mvir'] = np.log10(mvir)
    
    stellar_mass = file_handle[snapshot_name]['StellarMass'][index] * 1.0e10 / Hubble_h
    properties['log10_stellar_mass'] = np.log10(stellar_mass) if stellar_mass > 0 else -999
    
    # Extract position
    properties['position'] = [
        file_handle[snapshot_name]['Posx'][index],
        file_handle[snapshot_name]['Posy'][index],
        file_handle[snapshot_name]['Posz'][index]
    ]
    
    # Extract GalaxyIndex
    properties['galaxy_id'] = file_handle[snapshot_name]['GalaxyIndex'][index]
    
    # Additional properties - handle potential missing fields
    try:
        bh_mass = file_handle[snapshot_name]['BlackHoleMass'][index] * 1.0e10 / Hubble_h
        properties['black_hole_mass'] = np.log10(bh_mass) if bh_mass > 0 else -999
    except:
        properties['black_hole_mass'] = -999
    
    try:
        sfr = file_handle[snapshot_name]['SfrDisk'][index] + file_handle[snapshot_name]['SfrBulge'][index]
        properties['sfr'] = sfr
    except:
        properties['sfr'] = 0
    
    try:
        properties['type'] = file_handle[snapshot_name]['Type'][index]
    except:
        properties['type'] = -1
    
    try:
        cold_gas = file_handle[snapshot_name]['ColdGas'][index] * 1.0e10 / Hubble_h
        properties['cold_gas'] = np.log10(cold_gas) if cold_gas > 0 else -999
    except:
        properties['cold_gas'] = -999
    
    try:
        hot_gas = file_handle[snapshot_name]['HotGas'][index] * 1.0e10 / Hubble_h
        properties['hot_gas'] = np.log10(hot_gas) if hot_gas > 0 else -999
    except:
        properties['hot_gas'] = -999

    try:
        vvir = file_handle[snapshot_name]['Vvir'][index]
        properties['vvir'] = vvir
    except:
        properties['vvir'] = -999
    
    return properties

def plot_halo_evolution(tracked_data, output_dir="./plots", Hubble_h=0.73):
    """
    Create plots showing the evolution of the tracked halo with redshift on x-axis.
    
    Parameters:
    -----------
    tracked_data : dict
        Dictionary containing tracked properties
    output_dir : str
        Directory to save plots
    Hubble_h : float
        Hubble parameter for unit conversions
    """
    if len(tracked_data['snapnum']) == 0:
        print("No data to plot")
        return
    
    # Make sure output directory exists
    os.makedirs(output_dir, exist_ok=True)
    
    # Convert lists to arrays for easier plotting
    snapnums = np.array(tracked_data['snapnum'])
    log10_mvir = np.array(tracked_data['log10_mvir'])
    log10_stellar_mass = np.array(tracked_data['log10_stellar_mass'])
    positions = np.array(tracked_data['position'])
    galaxy_ids = np.array(tracked_data['galaxy_id'])
    black_hole_mass = np.array(tracked_data['black_hole_mass'])
    sfr = np.array(tracked_data['sfr'])
    galaxy_type = np.array(tracked_data['type'])
    log10_cold_gas = np.array(tracked_data['cold_gas'])
    log10_hot_gas = np.array(tracked_data['hot_gas'])
    vvir = np.array(tracked_data['vvir'])
    
    # Calculate redshift for each snapshot
    redshifts = snapshot_to_redshift(snapnums)
    
    # Sort by redshift (high to low)
    sort_idx = np.argsort(redshifts)[::-1]
    redshifts = redshifts[sort_idx]
    log10_mvir = log10_mvir[sort_idx]
    log10_stellar_mass = log10_stellar_mass[sort_idx]
    positions = positions[sort_idx]
    galaxy_ids = galaxy_ids[sort_idx]
    black_hole_mass = black_hole_mass[sort_idx]
    sfr = sfr[sort_idx]
    galaxy_type = galaxy_type[sort_idx]
    log10_cold_gas = log10_cold_gas[sort_idx]
    log10_hot_gas = log10_hot_gas[sort_idx]
    vvir = vvir[sort_idx]
    snapnums = snapnums[sort_idx]
    
    # Create figure with multiple subplots
    fig, axes = plt.subplots(3, 2, figsize=(14, 15))
    
    # Plot mass evolution
    axes[0, 0].plot(redshifts, log10_mvir, 'o-', label='Virial Mass')
    axes[0, 0].set_xlabel('Redshift (z)')
    axes[0, 0].set_ylabel('log$_{10}$ M$_{vir}$ [M$_{\odot}$]')
    axes[0, 0].set_title('Virial Mass Evolution')
    # Invert x-axis to show time flowing from high redshift to low redshift
    axes[0, 0].invert_xaxis()
    axes[0, 0].grid(True)
    
    # Plot stellar mass evolution
    valid_stellar = log10_stellar_mass > -900
    if np.any(valid_stellar):
        axes[0, 1].plot(redshifts[valid_stellar], log10_stellar_mass[valid_stellar], 'o-', label='Stellar Mass')
        axes[0, 1].set_xlabel('Redshift (z)')
        axes[0, 1].set_ylabel('log$_{10}$ M$_{*}$ [M$_{\odot}$]')
        axes[0, 1].set_title('Stellar Mass Evolution')
        axes[0, 1].invert_xaxis()
        axes[0, 1].grid(True)
    
    # Plot trajectory (x, y)
    axes[1, 0].plot(positions[:, 0], positions[:, 1], 'o-', label='Path')
    for i, z in enumerate(redshifts):
        axes[1, 0].annotate(f"z={z:.1f}", (positions[i, 0], positions[i, 1]), fontsize=8)
    axes[1, 0].set_xlabel('X Position [Mpc/h]')
    axes[1, 0].set_ylabel('Y Position [Mpc/h]')
    axes[1, 0].set_title('Spatial Trajectory (X-Y)')
    axes[1, 0].grid(True)
    
    # Plot black hole mass
    valid_bh = black_hole_mass > -900
    if np.any(valid_bh):
        axes[1, 1].plot(redshifts[valid_bh], black_hole_mass[valid_bh], 'o-', color='black')
        axes[1, 1].set_xlabel('Redshift (z)')
        axes[1, 1].set_ylabel('log$_{10}$ M$_{BH}$ [M$_{\odot}$]')
        axes[1, 1].set_title('Black Hole Mass Evolution')
        axes[1, 1].invert_xaxis()
        axes[1, 1].grid(True)
    else:
        axes[1, 1].text(0.5, 0.5, 'No Black Hole Data Available', 
                      horizontalalignment='center', verticalalignment='center',
                      transform=axes[1, 1].transAxes)
    
    # Plot gas masses (including H1 and H2 if available)
    valid_cold = log10_cold_gas > -900
    valid_hot = log10_hot_gas > -900
    
    # Simulate H1 and H2 data (in a real scenario, these would come from the tracked data)
    # Here we're creating mock values for illustration
    # Note: In real implementation, you would extract these from the simulation data
    log10_h1 = np.full_like(log10_cold_gas, -999)
    log10_h2 = np.full_like(log10_cold_gas, -999)
    
    # Estimate H1 and H2 as fractions of cold gas for demonstration
    if np.any(valid_cold):
        h1_fraction = 0.7  # Assume 70% of cold gas is H1
        h2_fraction = 0.2  # Assume 20% of cold gas is H2
        for i in range(len(log10_cold_gas)):
            if log10_cold_gas[i] > -900:
                cold_gas_mass = 10**log10_cold_gas[i]
                h1_mass = cold_gas_mass * h1_fraction
                h2_mass = cold_gas_mass * h2_fraction
                log10_h1[i] = np.log10(h1_mass)
                log10_h2[i] = np.log10(h2_mass)
    
    valid_h1 = log10_h1 > -900
    valid_h2 = log10_h2 > -900
    
    if np.any(valid_cold) or np.any(valid_hot) or np.any(valid_h1) or np.any(valid_h2):
        if np.any(valid_cold):
            axes[2, 0].plot(redshifts[valid_cold], log10_cold_gas[valid_cold], 'o-', color='blue', label='Cold Gas')
        if np.any(valid_hot):
            axes[2, 0].plot(redshifts[valid_hot], log10_hot_gas[valid_hot], 'o-', color='red', label='Hot Gas')
        if np.any(valid_h1):
            axes[2, 0].plot(redshifts[valid_h1], log10_h1[valid_h1], 'o-', color='cyan', label='H1 Gas')
        if np.any(valid_h2):
            axes[2, 0].plot(redshifts[valid_h2], log10_h2[valid_h2], 'o-', color='green', label='H2 Gas')
        
        axes[2, 0].set_xlabel('Redshift (z)')
        axes[2, 0].set_ylabel('log$_{10}$ M$_{gas}$ [M$_{\odot}$]')
        axes[2, 0].set_title('Gas Mass Evolution')
        axes[2, 0].legend()
        axes[2, 0].invert_xaxis()
        axes[2, 0].grid(True)
    
    # Plot velocity evolution
    valid_vvir = vvir > 0
    if np.any(valid_vvir):
        axes[2, 1].plot(redshifts[valid_vvir], vvir[valid_vvir], 'o-', color='purple')
        axes[2, 1].set_xlabel('Redshift (z)')
        axes[2, 1].set_ylabel('V$_{vir}$ [km/s]')
        axes[2, 1].set_title('Virial Velocity Evolution')
        axes[2, 1].invert_xaxis()
        axes[2, 1].grid(True)
    else:
        axes[2, 1].text(0.5, 0.5, 'No Velocity Data Available', 
                      horizontalalignment='center', verticalalignment='center',
                      transform=axes[2, 1].transAxes)
    
    plt.tight_layout()
    plt.savefig(f"{output_dir}/galaxy_evolution.png", dpi=300)
    plt.close()
    
    # Create 3D trajectory plot
    fig = plt.figure(figsize=(10, 8))
    ax = fig.add_subplot(111, projection='3d')
    
    # Color points by redshift
    sc = ax.scatter(positions[:, 0], positions[:, 1], positions[:, 2], 
                   c=redshifts, cmap='viridis')
    
    # Connect points with a line
    ax.plot(positions[:, 0], positions[:, 1], positions[:, 2], 'k-', alpha=0.3)
    
    # Add redshift labels
    for i, z in enumerate(redshifts):
        ax.text(positions[i, 0], positions[i, 1], positions[i, 2], f"z={z:.1f}", fontsize=8)
    
    ax.set_xlabel('X Position [Mpc/h]')
    ax.set_ylabel('Y Position [Mpc/h]')
    ax.set_zlabel('Z Position [Mpc/h]')
    ax.set_title('3D Trajectory (colored by redshift)')
    
    # Add colorbar
    cbar = plt.colorbar(sc)
    cbar.set_label('Redshift (z)')
    
    plt.savefig(f"{output_dir}/galaxy_3d_trajectory.png", dpi=300)
    plt.close()
    
    # Export data to CSV
    data = {
        'Snapshot': snapnums,
        'Redshift': redshifts,
        'GalaxyIndex': galaxy_ids,
        'log10_Mvir': log10_mvir,
        'log10_StellarMass': log10_stellar_mass,
        'X_Position': positions[:, 0],
        'Y_Position': positions[:, 1], 
        'Z_Position': positions[:, 2],
        'log10_BlackHoleMass': black_hole_mass,
        'StarFormationRate': sfr,
        'GalaxyType': galaxy_type,
        'log10_ColdGas': log10_cold_gas,
        'log10_HotGas': log10_hot_gas,
        'Vvir': vvir
    }
    
    # Add H1 and H2 data if available
    if np.any(valid_h1):
        data['log10_H1Gas'] = log10_h1
    if np.any(valid_h2):
        data['log10_H2Gas'] = log10_h2
    
    # Save to CSV file
    header = ','.join(data.keys())
    rows = []
    for i in range(len(snapnums)):
        row = [str(data[key][i]) for key in data.keys()]
        rows.append(','.join(row))
    
    with open(f"./galaxy_evolution_data.csv", 'w') as f:
        f.write(header + '\n')
        f.write('\n'.join(rows))
    
    return data

def snapshot_to_redshift(snapshot, max_snapshot=63):
    """
    Convert snapshot number to redshift.
    This is an approximation for Millennium simulation snapshots.
    
    Parameters:
    -----------
    snapshot : int or array
        Snapshot number
    max_snapshot : int
        Maximum snapshot number (typically 63 for Millennium)
    
    Returns:
    --------
    float or array
        Corresponding redshift
    """
    # Define some known points
    # For Millennium simulation: z=0 at snapshot 63, z≈127 at snapshot 0
    snapshots = np.array([0, 15, 30, 40, 50, 60, 63])
    redshifts = np.array([127.0, 8.55, 2.07, 0.99, 0.32, 0.04, 0.0])
    
    # Interpolate to get the redshift for the given snapshot
    return np.interp(snapshot, snapshots, redshifts)


def plot_redshift_mvir_vvir(tracked_data, output_dir="./plots", normalize=True):
    """
    Create a plot with redshift on x-axis and normalized log10(Mvir) and Vvir on dual y-axes.
    
    Parameters:
    -----------
    tracked_data : dict
        Dictionary containing tracked properties
    output_dir : str
        Directory to save plots
    normalize : bool
        Whether to normalize the scales
    """
    
    # Make sure output directory exists
    os.makedirs(output_dir, exist_ok=True)
    
    if len(tracked_data['snapnum']) == 0:
        print("No data to plot")
        return
    
    # Convert lists to arrays
    snapnums = np.array(tracked_data['snapnum'])
    log10_mvir = np.array(tracked_data['log10_mvir'])
    
    # Check if vvir is in the tracked data
    if 'vvir' not in tracked_data or not tracked_data['vvir']:
        print("Vvir data not found in tracking results, calculating from Mvir...")
        # Use a simple estimation for Vvir based on Mvir
        # Vvir ~ Mvir^(1/3) relationship
        vvir = 200 * (10**log10_mvir / 1e12)**(1/3)  # Simple scaling
    else:
        # Use tracked Vvir values
        vvir = np.array(tracked_data['vvir'])
    
    # Calculate redshift for each snapshot
    redshifts = snapshot_to_redshift(snapnums)
    
    # Sort by redshift (high to low)
    sort_idx = np.argsort(redshifts)[::-1]
    redshifts = redshifts[sort_idx]
    log10_mvir_sorted = log10_mvir[sort_idx]
    vvir = vvir[sort_idx]
    snapnums = snapnums[sort_idx]
    
    # Create figure with dual y-axes
    fig, ax1 = plt.subplots(figsize=(12, 8))
    
    # Set up colors
    color1 = 'tab:blue'
    color2 = 'tab:red'
    
    # Primary y-axis (log10 Mvir)
    ax1.set_xlabel('Redshift (z)', fontsize=14)
    ax1.set_ylabel('log$_{10}$ $M_{vir}$ [$M_{\odot}$]', color=color1, fontsize=14)
    lns1 = ax1.plot(redshifts, log10_mvir_sorted, 'o-', color=color1, label='log$_{10}$ $M_{vir}$')
    ax1.tick_params(axis='y', labelcolor=color1)
    
    # Secondary y-axis (Vvir)
    ax2 = ax1.twinx()
    ax2.set_ylabel('$V_{vir}$ [km/s]', color=color2, fontsize=14)
    lns2 = ax2.plot(redshifts, vvir, 's-', color=color2, label='$V_{vir}$')
    ax2.tick_params(axis='y', labelcolor=color2)
    
    # Normalize scales if requested
    if normalize:
        # Find the position of maximum Mvir
        max_mvir_idx = np.argmax(log10_mvir_sorted)
        max_mvir = log10_mvir_sorted[max_mvir_idx]
        max_vvir = vvir[max_mvir_idx]
        
        # Set the y-limits to make the maxima align
        y1_min, y1_max = ax1.get_ylim()
        y2_min, y2_max = ax2.get_ylim()
        
        # Calculate the scale factors
        scale1 = (max_mvir - y1_min) / (y1_max - y1_min)
        scale2 = (max_vvir - y2_min) / (y2_max - y2_min)
        
        # Adjust the limits to align the maxima
        if scale1 > scale2:
            # Adjust y2 (Vvir) scale
            new_y2_max = max_vvir + (y2_max - max_vvir) * scale1 / scale2
            ax2.set_ylim(y2_min, new_y2_max)
        else:
            # Adjust y1 (Mvir) scale
            new_y1_max = max_mvir + (y1_max - max_mvir) * scale2 / scale1
            ax1.set_ylim(y1_min, new_y1_max)
    
    # Format the redshift axis to use regular numbers, not scientific notation
    ax1.xaxis.set_major_formatter(plt.FormatStrFormatter('%.1f'))
    
    # Add grid
    ax1.grid(True, alpha=0.3)
    
    # Create legend
    lns = lns1 + lns2
    labs = [l.get_label() for l in lns]
    ax1.legend(lns, labs, loc='upper right', fontsize=12)
    
    # Set title
    plt.title('Evolution of log$_{10}$ $M_{vir}$ and $V_{vir}$ with Redshift', fontsize=16)
    
    # Adjust x-axis to be in descending order (high z to low z)
    ax1.invert_xaxis()
    
    
    
    # Adjust layout
    plt.tight_layout()
    
    # Save figure
    plt.savefig(f"{output_dir}/redshift_mvir_vvir.png", dpi=300, bbox_inches='tight')
    plt.close()
    
    print(f"Plot saved to {output_dir}/redshift_mvir_vvir.png")
    
    # Return the calculated data for reference
    return {'redshift': redshifts, 'log10_mvir': log10_mvir_sorted, 'vvir': vvir, 'snapnums': snapnums}

def main():
    # User-defined parameters - you can modify these
    base_dir = './output/millennium/'  # Change to your output directory
    start_snap = 63  # Latest snapshot (z=0)
    end_snap = 15    # Earlier snapshot to track back to
    min_mass = 1e13  # Minimum halo mass to consider (in Msun, not log10)
    Hubble_h = 0.73  # Hubble parameter
    output_dir = "./output/millennium/plots"  # Directory to save output plots

    # Ask the user if they want to use the most massive halo or specify a GalaxyIndex
    print("\nGalaxy/Halo Selection Options:")
    print("1. Use most massive halo at z=0")
    print("2. Find top N most massive halos and choose")
    print("3. Enter a specific GalaxyIndex")
    
    while True:
        try:
            choice = int(input("\nEnter your choice (1-3): "))
            if 1 <= choice <= 3:
                break
            else:
                print("Invalid choice. Please enter 1, 2, or 3.")
        except ValueError:
            print("Please enter a valid number.")
    
    galaxy_id = None
    
    if choice == 1:
        # Get the GalaxyIndex to track (from most massive halo at z=0)
        galaxy_id, halo_mass, _, _, _ = find_most_massive_halo(
            base_dir, start_snap, min_mass, Hubble_h)
        
    elif choice == 2:
        # Find top N most massive halos and let user choose
        try:
            top_n = int(input("\nHow many top halos to display? "))
        except ValueError:
            print("Invalid input. Using default value of 5.")
            top_n = 5
        
        snapshot_name = f'Snap_{start_snap}'
        model_files = [f for f in os.listdir(base_dir) if f.startswith('model_') and f.endswith('.hdf5')]
        model_files.sort()
        
        # Collect halos above the mass threshold from all files
        all_halos = []
        
        for model_file in model_files:
            try:
                with h5py.File(os.path.join(base_dir, model_file), 'r') as f:
                    if snapshot_name not in f:
                        continue
                    
                    # Get masses and convert to physical units
                    mvir = np.array(f[snapshot_name]['Mvir']) * 1.0e10 / Hubble_h
                    
                    # Find halos above minimum mass
                    valid_indices = np.where(mvir > min_mass)[0]
                    
                    for idx in valid_indices:
                        galaxy_id = f[snapshot_name]['GalaxyIndex'][idx]
                        mass = mvir[idx]
                        pos = [
                            f[snapshot_name]['Posx'][idx],
                            f[snapshot_name]['Posy'][idx],
                            f[snapshot_name]['Posz'][idx]
                        ]
                        
                        # Get stellar mass if available
                        try:
                            stellar_mass = f[snapshot_name]['StellarMass'][idx] * 1.0e10 / Hubble_h
                            log_stellar = np.log10(stellar_mass) if stellar_mass > 0 else -999
                        except:
                            log_stellar = -999
                        
                        # Get galaxy type if available
                        try:
                            gal_type = f[snapshot_name]['Type'][idx]
                        except:
                            gal_type = -1
                        
                        all_halos.append({
                            'galaxy_id': galaxy_id,
                            'mass': mass,
                            'log10_mass': np.log10(mass),
                            'log10_stellar': log_stellar,
                            'position': pos,
                            'type': gal_type,
                            'file': model_file
                        })
            except Exception as e:
                print(f"Error processing file {model_file}: {e}")
        
        # Sort halos by mass in descending order
        all_halos.sort(key=lambda x: x['mass'], reverse=True)
        
        # Display top N halos
        print("\nTop most massive halos at z=0 (snapshot {}):".format(start_snap))
        print("#  | GalaxyIndex | log10(Mvir) | log10(M*) | Position (x,y,z) | Type")
        print("-" * 80)
        
        for i, halo in enumerate(all_halos[:min(top_n, len(all_halos))]):
            pos = halo['position']
            stellar_str = f"{halo['log10_stellar']:9.3f}" if halo['log10_stellar'] > -900 else "    N/A   "
            
            print(f"{i+1:2d} | {halo['galaxy_id']:8d} | {halo['log10_mass']:10.3f} | "
                  f"{stellar_str} | ({pos[0]:.2f}, {pos[1]:.2f}, {pos[2]:.2f}) | "
                  f"{int(halo['type'])}")
        
        # Let user choose a halo
        while True:
            try:
                selection = int(input("\nSelect a halo number (1-{}): ".format(min(top_n, len(all_halos)))))
                if 1 <= selection <= min(top_n, len(all_halos)):
                    galaxy_id = all_halos[selection-1]['galaxy_id']
                    print(f"Selected GalaxyIndex: {galaxy_id}")
                    break
                else:
                    print("Invalid selection. Please try again.")
            except ValueError:
                print("Please enter a valid number.")
        
    elif choice == 3:
        # Manually enter a GalaxyIndex
        while True:
            try:
                galaxy_id = int(input("\nEnter the GalaxyIndex to track: "))
                break
            except ValueError:
                print("Please enter a valid integer for GalaxyIndex.")
    
    if galaxy_id is None:
        print("Failed to get a valid GalaxyIndex to track.")
        return
    
    # Track the galaxy across snapshots
    print(f"Tracking galaxy with ID {galaxy_id} from snapshot {start_snap} to {end_snap}...")
    tracked_data = track_galaxy_by_id(
        base_dir, start_snap, end_snap, galaxy_id, min_mass, Hubble_h)
    
    # Print summary
    if len(tracked_data['snapnum']) > 0:
        print(f"\nSuccessfully tracked galaxy across {len(tracked_data['snapnum'])} snapshots")
        
        # Create a summary table
        print("\nSummary:")
        print("Snapshot | GalaxyIndex | log10(Mvir) | log10(M*) | Position (x,y,z) | Type")
        print("-" * 80)
        for i in range(len(tracked_data['snapnum'])):
            pos = tracked_data['position'][i]
            stellar = tracked_data['log10_stellar_mass'][i]
            stellar_str = f"{stellar:9.3f}" if stellar > -900 else "    N/A   "
            
            print(f"{tracked_data['snapnum'][i]:8d} | {tracked_data['galaxy_id'][i]:8d} | {tracked_data['log10_mvir'][i]:10.3f} | "
                  f"{stellar_str} | ({pos[0]:.2f}, {pos[1]:.2f}, {pos[2]:.2f}) | "
                  f"{int(tracked_data['type'][i])}")
        
        # Plot the evolution
        print("\nCreating evolution plots...")
        plot_halo_evolution(tracked_data, output_dir, Hubble_h)
        print(f"Plots saved to {output_dir}/galaxy_evolution.png and {output_dir}/galaxy_3d_trajectory.png")
        print(f"Data saved to {output_dir}/galaxy_evolution_data.csv")
        print("\nCreating Mvir vs Vvir plot...")
        plot_redshift_mvir_vvir(tracked_data, output_dir, normalize=True)
        print(f"Redshift plot saved to {output_dir}/redshift_mvir_vvir.png")
    else:
        print("Could not track the galaxy. Check the parameters and snapshot range.")

if __name__ == "__main__":
    main()