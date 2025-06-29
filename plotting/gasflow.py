#!/usr/bin/env python3
"""
SAGE 2.0 Gas Flow Analysis - Complete Modified Script

This script works with the specific SAGE output structure:
- Data organized by snapshots (e.g., 'Snap_63')
- Multiple model files to combine
- Gas flow tracking fields
- Star formation rates from disk and bulge components
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.patches import FancyBboxPatch, ConnectionPatch
import h5py as h5
import os
import seaborn as sns

# Configuration (modify these for your setup)
DirName = './output/millennium_CGMfirst/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        
BoxSize = 62.5         
VolumeFraction = 1.0  

# Plotting options
whichimf = 1        
dilute = 7500       
sSFRcut = -11.0     

OutputFormat = '.pdf'
plt.rcParams["figure.figsize"] = (8.34, 6.25)
plt.rcParams["figure.dpi"] = 500
plt.rcParams["font.size"] = 14

# For full LaTeX formatting with \rm, uncomment the line below (requires LaTeX installation):
# plt.rcParams['text.usetex'] = True

def read_hdf(snap_num=None, param=None):
    """Read data from all SAGE model files and combine"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        try:
            with h5.File(DirName + model_file, 'r') as property_file:
                if snap_num in property_file and param in property_file[snap_num]:
                    data = np.array(property_file[snap_num][param])
                    
                    if combined_data is None:
                        combined_data = data
                    else:
                        combined_data = np.concatenate((combined_data, data))
                else:
                    print(f"Warning: {param} not found in {snap_num} of {model_file}")
        except Exception as e:
            print(f"Error reading {model_file}: {e}")
            
    return combined_data

def check_available_fields(snap_num):
    """Check what fields are available in the output"""
    print(f"Checking available fields in {snap_num}...")
    
    model_file = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')][0]
    
    with h5.File(DirName + model_file, 'r') as f:
        if snap_num in f:
            fields = list(f[snap_num].keys())
            print(f"Available fields: {fields}")
            return fields
        else:
            print(f"Snapshot {snap_num} not found")
            return []

def load_sage_gas_data(snap_num):
    """Load all available gas-related data for a snapshot"""
    print(f"Loading SAGE data for {snap_num}...")
    
    # Check what fields are available first
    available_fields = check_available_fields(snap_num)
    
    # Define field mappings (SAGE field names may vary)
    field_mappings = {
        # Basic properties
        'Type': ['Type', 'GalaxyType'],
        'SnapNum': ['SnapNum', 'Snapshot'],
        'Mvir': ['Mvir', 'HaloMass'],
        'StellarMass': ['StellarMass', 'Mstar'],
        
        # Gas reservoirs
        'ColdGas': ['ColdGas', 'DiskMass', 'ColdGasMass'],
        'HotGas': ['HotGas', 'HaloGas', 'HotGasMass'],
        'CGMgas': ['CGMgas', 'CGM', 'CircumgalacticGas'],
        'CGMgas_pristine': ['CGMgas_pristine', 'CGM_pristine'],
        'CGMgas_enriched': ['CGMgas_enriched', 'CGM_enriched'],
        
        # Flow rates
        'InfallRate_to_CGM': ['InfallRate_to_CGM', 'CGMInfall'],
        'TransferRate_CGM_to_Hot': ['TransferRate_CGM_to_Hot', 'CGMTransfer'],
        'OutflowRate': ['OutflowRate', 'Outflow'],
        'Cooling': ['Cooling', 'CoolingRate'],
        'Heating': ['Heating', 'HeatingRate'],
        
        # Star formation rates
        'SfrDisk': ['SfrDisk', 'StarFormationRateDisk'],
        'SfrBulge': ['SfrBulge', 'StarFormationRateBulge'],
    }
    
    data = {}
    
    # Try to read each field
    for standard_name, possible_names in field_mappings.items():
        field_found = False
        for field_name in possible_names:
            if field_name in available_fields:
                try:
                    data[standard_name] = read_hdf(snap_num, field_name)
                    print(f"Loaded {standard_name} from field {field_name}")
                    field_found = True
                    break
                except Exception as e:
                    print(f"Error loading {field_name}: {e}")
                    
        if not field_found:
            print(f"Warning: Could not find {standard_name} in any expected field names")
            data[standard_name] = None
    
    return data

def create_simple_flow_diagram(gas_data, flow_data, title="Gas Flow Diagram", save_name=None):
    """
    Create a simple gas flow diagram using matplotlib
    """
    
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Convert gas masses to proper units (M_sun)
    def convert_mass(mass):
        if mass > 0:
            return mass * 1.0e+10 / Hubble_h  # Convert to M_sun
        else:
            return 0
    
    # Convert masses and format for display
    cgm_mass = convert_mass(gas_data.get('CGMgas', 0))
    hot_mass = convert_mass(gas_data.get('HotGas', 0))
    cold_mass = convert_mass(gas_data.get('ColdGas', 0))
    
    # Calculate total infall mass and get star formation rate
    total_infall = flow_data.get('InfallRate_to_CGM', 0)
    infall_mass = convert_mass(total_infall) if total_infall > 0 else 0
    
    # Get total star formation rate (already calculated as disk + bulge)
    total_sfr = flow_data.get('TotalSFR', 0)
    
    # Format masses in readable scientific notation
    def format_mass(mass):
        if mass > 0:
            # Convert to scientific notation
            exponent = int(np.floor(np.log10(mass)))
            coefficient = mass / (10**exponent)
            
            # Option 1: With matplotlib math mode (no LaTeX installation needed)
            return f'{coefficient:.2f} × $10^{{{exponent}}}$ $M_⊙\\ h^{{-1}}$'
            
            # Option 2: With full LaTeX (uncomment if you have LaTeX installed and want \rm formatting)
            # return f'{coefficient:.2f} × $\\rm 10^{{{exponent}}}$ $\\rm M_⊙\\ h^{{-1}}$'
        else:
            return 'No Data'
    
    # Define reservoir positions and properties
    reservoirs = {
        'IGM': {'pos': (1, 4), 'size': 0.8, 'color': 'lightcyan', 
                'name': 'IGM\n(Infall)', 'mass': format_mass(infall_mass) if infall_mass > 0 else ''},
        'CGM': {'pos': (4, 4), 'size': max(0.5, np.sqrt(gas_data.get('CGMgas', 1))*0.1), 
                'color': 'lightblue', 'name': 'CGM', 'mass': format_mass(cgm_mass)},
        'Hot': {'pos': (7, 4), 'size': max(0.5, np.sqrt(gas_data.get('HotGas', 1))*0.1), 
                'color': 'orange', 'name': 'Hot Gas', 'mass': format_mass(hot_mass)},
        'Cold': {'pos': (10, 4), 'size': max(0.5, np.sqrt(gas_data.get('ColdGas', 1))*0.1), 
                 'color': 'lightgreen', 'name': 'Cold Gas', 'mass': format_mass(cold_mass)},
        'Stars': {'pos': (13, 4), 'size': 0.6, 'color': 'gold', 'name': 'Stars', 'mass': ''}
    }
    
    # Draw reservoirs as circles
    for name, props in reservoirs.items():
        circle = plt.Circle(props['pos'], props['size'], color=props['color'], 
                          alpha=0.7, ec='black', linewidth=2)
        ax.add_patch(circle)
        
        # Draw reservoir name (bold) at top of circle
        ax.text(props['pos'][0], props['pos'][1] + 0.15, props['name'], 
               ha='center', va='center', fontsize=10, fontweight='bold')
        
        # Draw mass value (normal) at bottom of circle
        if props['mass']:
            ax.text(props['pos'][0], props['pos'][1] - 0.15, props['mass'], 
                   ha='center', va='center', fontsize=9, fontweight='normal')
    
    # Define flows and draw arrows
    flows = [
        # (start_pos, end_pos, rate, label, color)
        ((1.8, 4), (3.2, 4), flow_data.get('InfallRate_to_CGM', 0), 'Infall→CGM', 'blue'),
        ((4.8, 4), (6.2, 4), flow_data.get('TransferRate_CGM_to_Hot', 0), 'CGM→Hot', 'purple'),
        ((7.8, 4), (9.2, 4), flow_data.get('Cooling', 0), 'Cooling', 'darkgreen'),
        ((10.8, 4), (12.2, 4), total_sfr, 'SF', 'gold'),  # Use actual star formation rate
        
        # Outflows
        ((9.2, 3.5), (7.8, 3.5), flow_data.get('OutflowRate', 0) * 0.3, 'Outflow', 'brown'),
        ((6.2, 3.2), (4.8, 3.2), flow_data.get('OutflowRate', 0) * 0.7, 'Outflow', 'brown'),
    ]
    
    # Draw flow arrows
    max_flow = max([abs(flow[2]) for flow in flows if flow[2] != 0] + [1e-10])
    
    for start, end, rate, label, color in flows:
        if rate > 0:
            # Arrow width proportional to flow rate
            width = max(rate / max_flow * 5, 0.5)
            
            arrow = patches.FancyArrowPatch(start, end, 
                                          arrowstyle='->', mutation_scale=15,
                                          linewidth=width, color=color, alpha=0.7)
            ax.add_patch(arrow)
            
            # Add flow rate labels
            if rate > max_flow * 0.01:  # Only label significant flows
                mid_x, mid_y = (start[0] + end[0])/2, (start[1] + end[1])/2 + 0.2
                ax.text(mid_x, mid_y, f'{rate:.2e}', ha='center', fontsize=8,
                       bbox=dict(boxstyle='round,pad=0.2', facecolor='white', alpha=0.8))
    
    # Add note about mass units
    # Option 1: Basic math mode formatting
    units_text = 'Rates: $M_⊙\\ h^{-1}$ per timestep'
    
    # Option 2: Full LaTeX formatting (uncomment if using LaTeX)
    # units_text = 'Masses: $\\rm M_⊙\\ h^{-1}$\nRates: $\\rm M_⊙\\ h^{-1}$ per timestep'
    
    ax.text(0.02, 0.98, units_text, 
            transform=ax.transAxes, fontsize=9, verticalalignment='top',
            bbox=dict(boxstyle='round,pad=0.3', facecolor='white', alpha=0.8))
    
    # Formatting
    ax.set_xlim(0, 14)
    ax.set_ylim(2, 6)
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_title(title, fontsize=16, fontweight='bold', pad=20)
    
    # Add horizontal legend at the bottom
    legend_elements = [
        plt.Line2D([0], [0], color='blue', lw=3, label='Infall to CGM'),
        plt.Line2D([0], [0], color='purple', lw=3, label='CGM Transfer'),
        plt.Line2D([0], [0], color='darkgreen', lw=3, label='Cooling'),
        plt.Line2D([0], [0], color='brown', lw=3, label='Outflows'),
        plt.Line2D([0], [0], color='gold', lw=3, label='Star Formation')
    ]
    
    # Place legend horizontally at the bottom
    ax.legend(handles=legend_elements, loc='upper center', bbox_to_anchor=(0.5, -0.05),
              ncol=len(legend_elements), frameon=True, fontsize=10)
    
    plt.tight_layout()
    plt.subplots_adjust(bottom=0.15)  # Make room for the legend
    
    if save_name:
        plt.savefig(save_name + OutputFormat, dpi=plt.rcParams["figure.dpi"], bbox_inches='tight')
        print(f"Saved plot: {save_name}{OutputFormat}")
    
    return fig

def create_reservoir_evolution_plot(snapshots, gas_evolution, title="Gas Reservoir Evolution", save_name=None):
    """
    Plot evolution of gas reservoirs over time using seaborn styling
    """
    
    # Set seaborn style
    sns.set_style("whitegrid")
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    # Plot each gas component
    if 'ColdGas' in gas_evolution and gas_evolution['ColdGas'] is not None:
        ax.semilogy(snapshots, gas_evolution['ColdGas'], 'o-', color='green', 
                    label='Cold Gas', linewidth=2, markersize=6)
    
    if 'HotGas' in gas_evolution and gas_evolution['HotGas'] is not None:
        ax.semilogy(snapshots, gas_evolution['HotGas'], 'o-', color='red', 
                    label='Hot Gas', linewidth=2, markersize=6)
    
    if 'CGMgas' in gas_evolution and gas_evolution['CGMgas'] is not None:
        ax.semilogy(snapshots, gas_evolution['CGMgas'], 'o-', color='blue', 
                    label='CGM Gas', linewidth=2, markersize=6)
    
    ax.set_xlabel('Snapshot Number')
    ax.set_ylabel('Gas Mass [10^10 M_sun/h]')
    ax.set_title(title, fontsize=14, fontweight='bold')
    ax.legend()
    
    plt.tight_layout()
    
    if save_name:
        plt.savefig(save_name + OutputFormat, dpi=plt.rcParams["figure.dpi"], bbox_inches='tight')
        print(f"Saved plot: {save_name}{OutputFormat}")
    
    return fig

def analyze_sage_gas_flows_millennium(snapshot='Snap_63', output_prefix="sage_flows"):
    """
    Complete analysis workflow for SAGE millennium data
    """
    
    print(f"Analyzing SAGE gas flows for {snapshot}")
    print("=" * 50)
    
    # Load data
    data = load_sage_gas_data(snapshot)
    
    # Check if we have basic galaxy data
    if data['Type'] is None:
        print("Cannot proceed without galaxy Type information")
        print("Available fields might be different - check your SAGE output structure")
        return
    
    # Select central galaxies if Type field exists
    if data['Type'] is not None:
        central_mask = data['Type'] == 0
        print(f"Found {np.sum(central_mask)} central galaxies out of {len(data['Type'])} total")
    else:
        # Use all galaxies if we can't distinguish
        central_mask = np.ones(len(list(data.values())[0]), dtype=bool)
        print("Using all galaxies (cannot distinguish central/satellite)")
    
    # Calculate total values for central galaxies
    gas_data = {}
    flow_data = {}
    
    # Process gas reservoirs
    gas_fields = ['ColdGas', 'HotGas', 'CGMgas']
    for field in gas_fields:
        if data[field] is not None:
            gas_data[field] = np.sum(data[field][central_mask])
            print(f"Total {field}: {gas_data[field]:.2e}")
        else:
            gas_data[field] = 0
            print(f"{field} not available")
    
    # Print mass information if available
    if data['Mvir'] is not None:
        mvir_converted = data['Mvir'] * 1.0e+10 / Hubble_h  # Convert to 10^10 M_sun/h
        total_mvir = np.sum(mvir_converted[central_mask])
        print(f"Total Mvir (central galaxies): {total_mvir:.2e} [10^10 M_sun/h]")
    
    # Process flow rates (removed InfallRate_to_Hot)
    flow_fields = ['InfallRate_to_CGM', 'TransferRate_CGM_to_Hot', 
                   'OutflowRate', 'Cooling', 'SfrDisk', 'SfrBulge']
    for field in flow_fields:
        if data[field] is not None:
            flow_data[field] = np.sum(data[field][central_mask])
            print(f"Total {field}: {flow_data[field]:.2e}")
        else:
            flow_data[field] = 0
            print(f"{field} not available")
    
    # Calculate total star formation rate
    total_sfr = flow_data.get('SfrDisk', 0) + flow_data.get('SfrBulge', 0)
    flow_data['TotalSFR'] = total_sfr
    print(f"Total Star Formation Rate (Disk+Bulge): {total_sfr:.2e}")
    
    # Create flow diagram
    fig = create_simple_flow_diagram(gas_data, flow_data, 
                                   f"SAGE Gas Flows - {snapshot}",
                                   f"{output_prefix}_{snapshot}")
    
    # Print summary
    print(f"\n{snapshot} Summary:")
    print("-" * 30)
    for field, value in gas_data.items():
        print(f"  {field:<15}: {value:.2e}")
    
    print("\nFlow Rates:")
    for field, value in flow_data.items():
        if field not in ['SfrDisk', 'SfrBulge']:  # Don't double-print individual SFR components
            print(f"  {field:<25}: {value:.2e}")
            
    print(f"  {'SfrDisk':<25}: {flow_data.get('SfrDisk', 0):.2e}")
    print(f"  {'SfrBulge':<25}: {flow_data.get('SfrBulge', 0):.2e}")
    print(f"  {'TotalSFR (Disk+Bulge)':<25}: {flow_data.get('TotalSFR', 0):.2e}")
    
    return fig, gas_data, flow_data

def analyze_flows_by_mass_and_time(first_snap=0, last_snap=63, output_prefix="mass_flow_evolution"):
    """
    Track median gas flows for different halo mass bins across cosmic time
    
    Parameters:
    -----------
    first_snap : int
        First snapshot to analyze
    last_snap : int  
        Last snapshot to analyze
    output_prefix : str
        Prefix for output files
    """
    
    # Redshift array from the simulation
    redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                 0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    print(f"Analyzing gas flows by mass and time from snap {first_snap} to {last_snap}")
    print("This may take a while as we process all snapshots...")
    
    # Flow fields to track (removed InfallRate_to_Hot as it's not available)
    flow_fields = ['InfallRate_to_CGM', 'TransferRate_CGM_to_Hot', 
                   'OutflowRate', 'Cooling', 'SfrDisk', 'SfrBulge']
    
    # Initialize storage for results
    results = {
        'redshifts': [],
        'snapshots': [],
        'mass_bins': ['Low Mass', 'Medium Mass', 'High Mass'],
        'flows': {flow: {'Low Mass': [], 'Medium Mass': [], 'High Mass': []} 
                 for flow in flow_fields + ['TotalSFR']}  # Add TotalSFR to tracked flows
    }
    
    # Process each snapshot
    valid_snaps = 0
    for snap_num in range(first_snap, last_snap + 1):
        snapshot = f'Snap_{snap_num}'
        print(f"\nProcessing {snapshot} (z = {redshifts[snap_num]:.2f})...")
        
        # Load data
        data = load_sage_gas_data(snapshot)
        
        # Skip if no data or essential fields missing
        if data['Mvir'] is None:
            print(f"  Skipping {snapshot} - no Mvir data")
            continue
            
        # Select central galaxies
        if data['Type'] is not None:
            central_mask = data['Type'] == 0
        else:
            central_mask = np.ones(len(data['Mvir']), dtype=bool)
        
        mvir_central = data['Mvir'][central_mask]
        
        # Apply mass conversion factor: Mvir is in code units, need to convert to 10^10 M_sun/h
        mvir_central = mvir_central * 1.0e+10 / Hubble_h
        
        if len(mvir_central) == 0:
            print(f"  Skipping {snapshot} - no central galaxies")
            continue
        
        # Define mass bins (33rd and 67th percentiles)
        mass_percentiles = np.percentile(mvir_central, [33, 67])
        
        # Create mass bin masks
        low_mass_mask = (mvir_central <= mass_percentiles[0])
        med_mass_mask = (mvir_central > mass_percentiles[0]) & (mvir_central <= mass_percentiles[1])
        high_mass_mask = (mvir_central > mass_percentiles[1])
        
        mass_masks = {
            'Low Mass': central_mask.copy(),
            'Medium Mass': central_mask.copy(), 
            'High Mass': central_mask.copy()
        }
        
        # Apply mass cuts to the original central mask
        central_indices = np.where(central_mask)[0]
        
        # Reset all masks to False
        for key in mass_masks:
            mass_masks[key][:] = False
            
        # Set True for appropriate indices
        mass_masks['Low Mass'][central_indices[low_mass_mask]] = True
        mass_masks['Medium Mass'][central_indices[med_mass_mask]] = True
        mass_masks['High Mass'][central_indices[high_mass_mask]] = True
        
        print(f"  Mass bins: Low={np.sum(low_mass_mask)}, Med={np.sum(med_mass_mask)}, High={np.sum(high_mass_mask)}")
        print(f"  Mass ranges (10^10 M_sun/h): Low=<{mass_percentiles[0]:.2e}, Med={mass_percentiles[0]:.2e}-{mass_percentiles[1]:.2e}, High=>{mass_percentiles[1]:.2e}")
        
        # Calculate median flows for each mass bin
        snapshot_flows = {}
        for flow_field in flow_fields:
            snapshot_flows[flow_field] = {}
            
            if data[flow_field] is not None:
                for mass_bin, mask in mass_masks.items():
                    if np.sum(mask) > 0:
                        flow_values = data[flow_field][mask]
                        # Use median and filter out zeros/negatives for flow rates
                        positive_flows = flow_values[flow_values > 0]
                        if len(positive_flows) > 0:
                            median_flow = np.median(positive_flows)
                        else:
                            median_flow = 0.0
                    else:
                        median_flow = 0.0
                    
                    snapshot_flows[flow_field][mass_bin] = median_flow
            else:
                # Field not available
                for mass_bin in mass_masks.keys():
                    snapshot_flows[flow_field][mass_bin] = 0.0
                print(f"  {flow_field} not available")
        
        # Calculate total star formation rate (Disk + Bulge) for each mass bin
        snapshot_flows['TotalSFR'] = {}
        for mass_bin in mass_masks.keys():
            total_sfr = snapshot_flows.get('SfrDisk', {}).get(mass_bin, 0) + \
                       snapshot_flows.get('SfrBulge', {}).get(mass_bin, 0)
            snapshot_flows['TotalSFR'][mass_bin] = total_sfr
        
        # Store results
        results['redshifts'].append(redshifts[snap_num])
        results['snapshots'].append(snap_num)
        
        for flow_field in flow_fields + ['TotalSFR']:  # Include TotalSFR
            for mass_bin in results['mass_bins']:
                results['flows'][flow_field][mass_bin].append(snapshot_flows[flow_field][mass_bin])
        
        valid_snaps += 1
        
        # Print summary for this snapshot
        print(f"  Flow medians:")
        for flow_field in flow_fields:
            if data[flow_field] is not None:
                print(f"    {flow_field}: Low={snapshot_flows[flow_field]['Low Mass']:.2e}, "
                      f"Med={snapshot_flows[flow_field]['Medium Mass']:.2e}, "
                      f"High={snapshot_flows[flow_field]['High Mass']:.2e}")
        
        # Print total SFR separately
        print(f"    TotalSFR (Disk+Bulge): Low={snapshot_flows['TotalSFR']['Low Mass']:.2e}, "
              f"Med={snapshot_flows['TotalSFR']['Medium Mass']:.2e}, "
              f"High={snapshot_flows['TotalSFR']['High Mass']:.2e}")
    
    print(f"\nProcessed {valid_snaps} snapshots successfully")
    
    # Convert lists to arrays
    results['redshifts'] = np.array(results['redshifts'])
    results['snapshots'] = np.array(results['snapshots'])
    for flow_field in flow_fields + ['TotalSFR']:
        for mass_bin in results['mass_bins']:
            results['flows'][flow_field][mass_bin] = np.array(results['flows'][flow_field][mass_bin])
    
    # Create the evolution plot
    fig = create_mass_flow_evolution_plot(results, output_prefix)
    
    return fig, results

def create_mass_flow_evolution_plot(results, output_prefix="mass_flow_evolution"):
    """
    Create a multi-panel plot showing gas flow evolution for different mass bins
    """
    
    # Set up the plot with subplots for different flows
    # Use main flow fields plus TotalSFR, but exclude individual SfrDisk/SfrBulge
    plot_fields = ['InfallRate_to_CGM', 'TransferRate_CGM_to_Hot', 'OutflowRate', 'Cooling', 'TotalSFR']
    available_fields = [field for field in plot_fields if field in results['flows']]
    
    n_flows = len(available_fields)
    
    # Create subplots (2 columns, ceil(n_flows/2) rows)
    n_cols = 2
    n_rows = int(np.ceil(n_flows / n_cols))
    
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(15, 4*n_rows))
    if n_rows == 1:
        axes = axes.reshape(1, -1)
    
    # Colors for mass bins
    colors = {'Low Mass': 'blue', 'Medium Mass': 'orange', 'High Mass': 'red'}
    linestyles = {'Low Mass': '-', 'Medium Mass': '--', 'High Mass': '-.'}
    
    redshifts = results['redshifts']
    
    # Plot each flow type
    for i, flow_field in enumerate(available_fields):
        row = i // n_cols
        col = i % n_cols
        ax = axes[row, col]
        
        # Plot each mass bin
        for mass_bin in results['mass_bins']:
            flow_values = results['flows'][flow_field][mass_bin]
            
            # Only plot if we have non-zero data
            if np.any(flow_values > 0):
                ax.loglog(redshifts, flow_values, 'o-', 
                         color=colors[mass_bin], linestyle=linestyles[mass_bin],
                         label=mass_bin, linewidth=2, markersize=4, alpha=0.8)
        
        # Format field name for display
        display_name = flow_field.replace('_', ' ')
        if flow_field == 'TotalSFR':
            display_name = 'Total SFR (Disk+Bulge)'
        
        ax.set_xlabel('Redshift (z)')
        ax.set_ylabel(f'{display_name} Rate\n[10^10 M_sun/h]')
        ax.set_title(f'{display_name}')
        # Remove individual legends - we'll add a single one at the bottom
        ax.grid(True, alpha=0.3)
        
        # Set x-axis to go from z=0 (today) on left to high-z on right
        ax.set_xlim(redshifts.min(), redshifts.max())
        
        # Format x-axis to show actual redshift values instead of 10^x
        from matplotlib.ticker import LogFormatter
        ax.xaxis.set_major_formatter(LogFormatter(base=10, labelOnlyBase=False))
        # Set custom ticks at meaningful redshift values
        xticks = [0.02, 0.1, 0.5, 1, 2, 5, 10, 20, 50, 100]
        xticks = [x for x in xticks if redshifts.min() <= x <= redshifts.max()]
        if xticks:
            ax.set_xticks(xticks)
            ax.set_xticklabels([f'{x:g}' for x in xticks])
    
    # Hide any unused subplots
    for i in range(n_flows, n_rows * n_cols):
        row = i // n_cols
        col = i % n_cols
        axes[row, col].axis('off')
    
    # Add single horizontal legend at the bottom
    handles = []
    labels = []
    for mass_bin in results['mass_bins']:
        handle = plt.Line2D([0], [0], color=colors[mass_bin], linestyle=linestyles[mass_bin],
                           linewidth=2, marker='o', markersize=4)
        handles.append(handle)
        labels.append(mass_bin)
    
    # Place legend below the subplots
    fig.legend(handles, labels, loc='lower center', bbox_to_anchor=(0.5, -0.05), 
               ncol=len(results['mass_bins']), frameon=True, fontsize=12)
    
    plt.suptitle('Gas Flow Evolution by Halo Mass\n(z=0 today → high-z past)', fontsize=16, fontweight='bold')
    plt.tight_layout()
    plt.subplots_adjust(bottom=0.15)  # Make room for the legend
    
    # Save plot
    save_name = f"{output_prefix}_by_mass"
    plt.savefig(save_name + OutputFormat, dpi=plt.rcParams["figure.dpi"], bbox_inches='tight')
    print(f"Saved plot: {save_name}{OutputFormat}")
    
    return fig

def analyze_multiple_snapshots(snapshot_list=None, output_prefix="sage_flows"):
    """
    Analyze multiple snapshots and create evolution plots
    """
    
    if snapshot_list is None:
        snapshot_list = ['Snap_63', 'Snap_50', 'Snap_30']  # Example snapshots
    
    print(f"Analyzing multiple snapshots: {snapshot_list}")
    
    gas_evolution = {'ColdGas': [], 'HotGas': [], 'CGMgas': []}
    snap_numbers = []
    
    for snapshot in snapshot_list:
        print(f"\nProcessing {snapshot}...")
        
        # Extract snapshot number for plotting
        snap_num = int(snapshot.split('_')[1])
        snap_numbers.append(snap_num)
        
        # Load data
        data = load_sage_gas_data(snapshot)
        
        if data['Type'] is not None:
            central_mask = data['Type'] == 0
        else:
            central_mask = np.ones(len(list(data.values())[0]), dtype=bool)
        
        # Calculate totals
        for field in gas_evolution.keys():
            if data[field] is not None:
                gas_evolution[field].append(np.sum(data[field][central_mask]))
            else:
                gas_evolution[field].append(0)
    
    # Convert to arrays
    snap_numbers = np.array(snap_numbers)
    for field in gas_evolution:
        gas_evolution[field] = np.array(gas_evolution[field])
    
    # Create evolution plot
    fig = create_reservoir_evolution_plot(snap_numbers, gas_evolution,
                                        "Gas Reservoir Evolution",
                                        f"{output_prefix}_evolution")
    
    return fig, snap_numbers, gas_evolution

def quick_mass_evolution_analysis(snap_range=(50, 63), output_name="quick_mass_flows"):
    """
    Quick version for testing - analyze a small range of snapshots
    
    Parameters:
    -----------
    snap_range : tuple
        (first_snap, last_snap) to analyze
    output_name : str
        Output file prefix
    """
    
    print(f"Quick analysis: snapshots {snap_range[0]} to {snap_range[1]}")
    fig, results = analyze_flows_by_mass_and_time(
        first_snap=snap_range[0], 
        last_snap=snap_range[1], 
        output_prefix=output_name
    )
    
    # Print summary
    print(f"\nQuick Summary (z = {results['redshifts'][-1]:.2f} to {results['redshifts'][0]:.2f}):")
    print("-" * 60)
    
    for flow_field in ['InfallRate_to_CGM', 'TransferRate_CGM_to_Hot', 'OutflowRate', 'Cooling', 'TotalSFR']:
        if flow_field in results['flows']:
            print(f"\n{flow_field.replace('_', ' ')}:")
            for mass_bin in results['mass_bins']:
                final_rate = results['flows'][flow_field][mass_bin][-1]
                print(f"  {mass_bin:12}: {final_rate:.2e} (at z~0)")
    
    return fig, results

# Example usage
if __name__ == "__main__":
    print("SAGE 2.0 Gas Flow Analysis - Millennium Format")
    print("=" * 50)
    
    # Check available fields first
    check_available_fields(Snapshot)
    
    # Option 1: Analyze single snapshot
    print("\n1. Single Snapshot Analysis:")
    fig, gas_data, flow_data = analyze_sage_gas_flows_millennium(Snapshot)
    
    # Option 2: Analyze flow evolution by mass bins across cosmic time
    print("\n2. Mass-dependent Flow Evolution Analysis:")
    print("This will process all snapshots - may take several minutes...")
    
    # For testing with a smaller range first (recommended):
    print("Starting with quick test (last ~14 snapshots)...")
    fig_quick, results_quick = quick_mass_evolution_analysis(snap_range=(50, 63), output_name="quick_test")
    
    # For full analysis across all cosmic time, uncomment this:
    print("Running full cosmic time analysis (all 64 snapshots)...")
    fig_mass, results = analyze_flows_by_mass_and_time(first_snap=0, last_snap=63)
    
    # Option 3: Simple multiple snapshot analysis
    fig_evo, snaps, gas_evo = analyze_multiple_snapshots(['Snap_63', 'Snap_50', 'Snap_30'])
    
    print("\nAnalysis complete!")
    print("Generated plots:")
    print(f"- Single snapshot flow diagram: sage_flows_{Snapshot}{OutputFormat}")
    print(f"- Quick mass evolution plot: quick_test_by_mass{OutputFormat}")
    print("\nTo run full cosmic time analysis, uncomment the full analysis line in the code.")
    
    # plt.show()