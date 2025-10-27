#!/usr/bin/env python3
"""
Group Properties Six-Panel Scatter Plotter

Creates a six-panel figure showing group properties vs halo mass (Mvir) 
colored by number of members for a single snapshot.

Usage:
    python plot_group_scatter_panels.py --input group_aggregated_properties.csv
    python plot_group_scatter_panels.py --input snapshot_data.csv --output_dir plots/
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import argparse
from pathlib import Path
import warnings
warnings.filterwarnings('ignore')

def calculate_metallicity(metals_cold_gas, cold_gas):
    """
    Calculate metallicity using the formula:
    Z = log10((MetalsColdGas / ColdGas) / 0.02) + 9.0
    
    Parameters:
    metals_cold_gas: Array of metal mass in cold gas
    cold_gas: Array of cold gas mass
    
    Returns:
    metallicity: Array of metallicity values
    """
    # Avoid division by zero
    valid_mask = (cold_gas > 0) & (metals_cold_gas > 0)
    metallicity = np.full_like(cold_gas, np.nan)
    
    metallicity[valid_mask] = np.log10((metals_cold_gas[valid_mask] / cold_gas[valid_mask]) / 0.02) + 9.0
    
    return metallicity

def plot_group_scatter_panels(data_file, output_dir='./', figsize=(18, 12)):
    """
    Create six-panel scatter plot showing group properties vs Mvir colored by N_galaxies.
    
    Parameters:
    data_file: Path to group_aggregated_properties.csv
    output_dir: Output directory for plots
    figsize: Figure size
    """
    
    print("Loading group properties data...")
    data = pd.read_csv(data_file)
    print(f"Loaded {len(data)} groups")
    
    # Debug: Show N_galaxies distribution
    print(f"N_galaxies distribution:")
    print(f"  Min: {data['N_galaxies'].min()}")
    print(f"  Max: {data['N_galaxies'].max()}")
    print(f"  Mean: {data['N_galaxies'].mean():.1f}")
    print(f"  Median: {data['N_galaxies'].median():.1f}")
    
    # Show histogram of group sizes
    unique_sizes, counts = np.unique(data['N_galaxies'], return_counts=True)
    print(f"Group size histogram:")
    for size, count in zip(unique_sizes[:10], counts[:10]):  # Show first 10
        print(f"  {size} members: {count} groups")
    if len(unique_sizes) > 10:
        print(f"  ... and {len(unique_sizes)-10} more group sizes")
    
    # Calculate metallicity
    if 'total_MetalsColdGas' in data.columns and 'total_ColdGas' in data.columns:
        data['metallicity'] = calculate_metallicity(data['total_MetalsColdGas'], data['total_ColdGas'])
        print("Calculated metallicity from MetalsColdGas and ColdGas")
    else:
        print("Warning: Cannot calculate metallicity - missing MetalsColdGas or ColdGas columns")
        data['metallicity'] = np.nan
    
    # Handle missing total_SFR - create from components if needed
    if 'total_SFR' not in data.columns:
        if 'total_SfrDisk' in data.columns and 'total_SfrBulge' in data.columns:
            data['total_SFR'] = data['total_SfrDisk'].fillna(0) + data['total_SfrBulge'].fillna(0)
            print("Created total_SFR from total_SfrDisk + total_SfrBulge")
        else:
            print("Warning: No SFR data available")
            data['total_SFR'] = np.nan
    
    # Define properties to plot
    properties = [
        {
            'name': 'Stellar Mass vs Halo Mass', 
            'x_column': 'median_Mvir', 
            'y_column': 'total_StellarMass',
            'xlabel': 'Halo Mass [M☉]',
            'ylabel': 'Stellar Mass [M☉]'
        },
        {
            'name': 'HI Gas vs Halo Mass', 
            'x_column': 'median_Mvir', 
            'y_column': 'total_HI_gas',
            'xlabel': 'Halo Mass [M☉]',
            'ylabel': 'HI Gas Mass [M☉]'
        },
        {
            'name': 'H2 Gas vs Halo Mass', 
            'x_column': 'median_Mvir', 
            'y_column': 'total_H2_gas',
            'xlabel': 'Halo Mass [M☉]',
            'ylabel': 'H₂ Gas Mass [M☉]'
        },
        {
            'name': 'Black Hole Mass vs Halo Mass', 
            'x_column': 'median_Mvir', 
            'y_column': 'total_BlackHoleMass',
            'xlabel': 'Halo Mass [M☉]',
            'ylabel': 'Black Hole Mass [M☉]'
        },
        {
            'name': 'SFR vs Halo Mass', 
            'x_column': 'median_Mvir', 
            'y_column': 'total_SFR',
            'xlabel': 'Halo Mass [M☉]',
            'ylabel': 'Star Formation Rate [M☉ yr⁻¹]'
        },
        {
            'name': 'Metallicity vs Halo Mass', 
            'x_column': 'median_Mvir', 
            'y_column': 'metallicity',
            'xlabel': 'Halo Mass [M☉]',
            'ylabel': '12 + log(O/H)'
        }
    ]
    
    # Set up the figure
    fig, axes = plt.subplots(2, 3, figsize=figsize)
    axes = axes.flatten()
    
    # Get color data (number of members)
    color_data = data['N_galaxies']
    
    # Set up colormap
    cmap = plt.cm.plasma
    
    # Plot each property
    for prop_idx, prop in enumerate(properties):
        ax = axes[prop_idx]
        
        # Check if required columns exist
        x_col = prop['x_column']
        y_col = prop['y_column']
        
        if x_col not in data.columns or y_col not in data.columns:
            ax.text(0.5, 0.5, f"Missing data:\n{x_col} or {y_col}", 
                   transform=ax.transAxes, ha='center', va='center', fontsize=12)
            ax.set_title(prop['name'])
            continue
        
        # Get data for this plot
        x_data = data[x_col]
        y_data = data[y_col]
        
        # Remove NaN values
        valid_mask = ~(pd.isna(x_data) | pd.isna(y_data) | pd.isna(color_data))
        
        if not valid_mask.any():
            ax.text(0.5, 0.5, f"No valid data for\n{prop['name']}", 
                   transform=ax.transAxes, ha='center', va='center', fontsize=12)
            ax.set_title(prop['name'])
            continue
        
        x_valid = x_data[valid_mask]
        y_valid = y_data[valid_mask]
        c_valid = color_data[valid_mask]
        
        # Remove zeros for log scale (if needed)
        if prop_idx in [0, 1, 2, 3, 4]:  # Log scale for mass and SFR plots
            positive_mask = (x_valid > 0) & (y_valid > 0)
            x_valid = x_valid[positive_mask]
            y_valid = y_valid[positive_mask]
            c_valid = c_valid[positive_mask]
        
        if len(x_valid) == 0:
            ax.text(0.5, 0.5, f"No positive data for\n{prop['name']}", 
                   transform=ax.transAxes, ha='center', va='center', fontsize=12)
            ax.set_title(prop['name'])
            continue
        
        # Create scatter plot
        scatter = ax.scatter(x_valid, y_valid, c=c_valid, cmap=cmap, 
                           alpha=0.7, s=30, edgecolors='none')
        
        # Set scales
        if prop_idx in [0, 1, 2, 3, 4]:  # Log scale for mass and SFR plots
            ax.set_xscale('log')
            ax.set_yscale('log')
        elif prop_idx == 5:  # Metallicity plot
            ax.set_xscale('log')
            # Metallicity is already in log units, so linear scale for y
        
        # Set labels
        ax.set_xlabel(prop['xlabel'])
        ax.set_ylabel(prop['ylabel'])
        ax.set_title(prop['name'], fontsize=12, fontweight='bold')
        
        # Add grid
        ax.grid(True, alpha=0.3)
        
        # Add colorbar to last panel
        if prop_idx == len(properties) - 1:
            cbar = plt.colorbar(scatter, ax=ax)
            cbar.set_label('Number of Members', rotation=270, labelpad=15)
    
    # Print data statistics
    print(f"\nData Statistics:")
    print(f"Total groups: {len(data)}")
    print(f"N_galaxies range: {data['N_galaxies'].min():.0f} - {data['N_galaxies'].max():.0f}")
    
    if 'median_Mvir' in data.columns:
        valid_mvir = data['median_Mvir'].dropna()
        if len(valid_mvir) > 0:
            print(f"Mvir range: {valid_mvir.min():.2e} - {valid_mvir.max():.2e} M☉")
    
    for prop in properties:
        if prop['y_column'] in data.columns:
            valid_data = data[prop['y_column']].dropna()
            if len(valid_data) > 0:
                if prop['y_column'] == 'metallicity':
                    print(f"{prop['y_column']} range: {valid_data.min():.2f} - {valid_data.max():.2f}")
                else:
                    print(f"{prop['y_column']} range: {valid_data.min():.2e} - {valid_data.max():.2e}")
    
    # Add overall title
    fig.suptitle('Group Properties vs Halo Mass (colored by Number of Members)', 
                fontsize=16, fontweight='bold')
    
    plt.tight_layout()
    plt.subplots_adjust(top=0.93)
    
    # Save plot
    output_path = Path(output_dir)
    output_path.mkdir(exist_ok=True)
    
    output_file = output_path / 'group_properties_scatter_panels.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"\nSaved six-panel scatter plot to {output_file}")
    
    # Also save as PDF for publications
    output_file_pdf = output_path / 'group_properties_scatter_panels.pdf'
    plt.savefig(output_file_pdf, bbox_inches='tight')
    print(f"Saved PDF version to {output_file_pdf}")
    
    # Show summary of plotted data
    print(f"\nPlot Summary:")
    for prop_idx, prop in enumerate(properties):
        if prop['x_column'] in data.columns and prop['y_column'] in data.columns:
            x_data = data[prop['x_column']]
            y_data = data[prop['y_column']]
            valid_mask = ~(pd.isna(x_data) | pd.isna(y_data))
            n_points = valid_mask.sum()
            print(f"  {prop['name']}: {n_points} data points")
        else:
            print(f"  {prop['name']}: No data available")

def main():
    parser = argparse.ArgumentParser(description='Plot group properties in six scatter panels')
    
    parser.add_argument('--input', type=str, required=True,
                       help='Path to group_aggregated_properties.csv')
    parser.add_argument('--output_dir', type=str, default='./',
                       help='Output directory for plots')
    parser.add_argument('--figsize', type=float, nargs=2, default=[18, 12],
                       help='Figure size in inches (width height)')
    
    args = parser.parse_args()
    
    # Check if input file exists
    if not Path(args.input).exists():
        print(f"Error: Input file {args.input} not found!")
        return
    
    print("="*60)
    print("GROUP PROPERTIES SIX-PANEL SCATTER PLOTTER")
    print("="*60)
    print(f"Input file: {args.input}")
    print(f"Output directory: {args.output_dir}")
    
    # Create the plot
    plot_group_scatter_panels(
        args.input, 
        args.output_dir, 
        tuple(args.figsize)
    )
    
    print("\n" + "="*60)
    print("PLOTTING COMPLETE")
    print("="*60)

if __name__ == "__main__":
    main()