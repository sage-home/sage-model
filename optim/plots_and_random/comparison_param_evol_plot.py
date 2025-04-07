#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os
from pathlib import Path
import argparse

def setup_plot_style():
    """Setup consistent plot styling"""
    sns.set_theme(style="white", font_scale=1.2)
    sns.set_palette("deep")
    plt.rcParams['figure.facecolor'] = 'white'
    plt.rcParams['axes.facecolor'] = 'white'

def load_all_params(directory, param_names, redshifts):
    """
    Load all parameter values and scores from CSV files for a single run
    Returns both the full particle data and best values
    """
    best_params = {}
    best_scores = {}
    particle_data = {}
    
    for z in redshifts:
        # Format filename based on redshift value
        if z == 0.0:
            z_str = '0'
        elif z == 0.2:
            z_str = '02'
        else:
            z_str = str(z).replace('.', '')
            
        filename = os.path.join(directory, f'params_z{z_str}.csv')
        
        try:
            if os.path.exists(filename):
                # Read CSV with tab delimiter, no header
                df = pd.read_csv(filename, delimiter='\t', header=None, names=param_names + ['score'])
                
                # Get particle data (all rows except last two)
                particle_data[z] = df.iloc[:-2]
                
                # Get second to last row for parameters and last row for score
                best_param_values = df.iloc[-2].values
                score = float(df.iloc[-1].iloc[0])
                
                best_params[z] = best_param_values[:-1]  # Exclude the score column
                best_scores[z] = score
                print(f"Successfully loaded data for z={z} from {filename}")
            else:
                print(f"File not found: {filename}")
                
        except Exception as e:
            print(f"Error processing file for z={z}: {str(e)}")
            continue
    
    return particle_data, best_params, best_scores

def plot_parameter_evolution_comparison(run_data, param_names, output_dir, run_labels=None, colors=None):
    """
    Create a single figure with all parameters arranged in a grid, comparing multiple runs
    
    Parameters:
    -----------
    run_data : list of tuples
        List of (particle_data, best_params, best_scores) for each run
    param_names : list
        List of parameter names
    output_dir : str
        Directory to save output plots
    run_labels : list, optional
        Labels for each run in the legend
    colors : list, optional
        List of colors to use for each run. If None, uses default color scheme
    """
    os.makedirs(output_dir, exist_ok=True)
    setup_plot_style()
    
    # Set up colors for different runs
    #n_runs = len(run_data)
    #colors = plt.cm.brg_r(np.linspace(0, 1, n_runs))

    # Set up colors for different runs
    n_runs = len(run_data)
    if colors is None:
        colors = plt.cm.brg_r(np.linspace(0, 1, n_runs))
    elif len(colors) < n_runs:
        print("Warning: Not enough colors provided. Using default colors for remaining runs.")
        colors = list(colors) + list(plt.cm.tab10(np.linspace(0, 1, n_runs - len(colors))))
    
    if run_labels is None:
        run_labels = [f'Run {i+1}' for i in range(n_runs)]
    
    # Create figure with subplots
    fig, axs = plt.subplots(4, 2, figsize=(15, 20))
    axs_flat = axs.flatten()
    
    # Plot parameters
    for i, param in enumerate(param_names):
        ax = axs_flat[i]
        
        # Process each run
        for run_idx, (particle_data, best_params, best_scores) in enumerate(run_data):
            redshifts = sorted(best_params.keys())
            
            # Get best values for this parameter
            best_values = [best_params[z][i] for z in redshifts]
            
            # Calculate mean and std for each redshift
            means = []
            stds = []
            all_values = []
            
            for z in redshifts:
                param_values = particle_data[z].iloc[:, i].values
                means.append(np.mean(param_values))
                stds.append(np.std(param_values))
                all_values.extend(param_values)
            
            means = np.array(means)
            stds = np.array(stds)
            global_mean = np.mean(all_values)
            
            # Plot with unique color for this run
            color = colors[run_idx]
            
            # Plot mean with shaded error region
            ax.fill_between(redshifts, means - stds, means + stds, 
                          alpha=0.15, color=color)
            
            # Plot mean line
            #ax.plot(redshifts, means, '-', color=color, 
                   #linewidth=1.5, label=f'{run_labels[run_idx]} (Mean)')
            
            # Plot best values
            ax.plot(redshifts, best_values, '-', color=color, marker='o',
                   linewidth=2, markersize=8, label=f'{run_labels[run_idx]}')
            
            # Plot global mean as a horizontal line
            ax.axhline(global_mean, color=color, linestyle='--', 
                      linewidth=2, alpha=0.8)
        
        ax.set_xlabel('Redshift')
        ax.set_ylabel('Value')
        ax.set_title(param)
        
        if i == 0:  # Only add legend to first plot
            leg = ax.legend(loc='lower right')
            leg.get_frame().set_linewidth(0.0)  # Remove box around legend
    
    # Remove the last subplot if we have an odd number of parameters
    if len(param_names) < len(axs_flat):
        fig.delaxes(axs_flat[-1])
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_evolution_comparison.png'), 
                bbox_inches='tight', dpi=300)
    plt.close()

def main():
    parser = argparse.ArgumentParser(description='Compare parameter evolution across multiple PSO runs')
    parser.add_argument('run_dirs', nargs='+', help='Directories containing PSO run results')
    parser.add_argument('--labels', nargs='+', help='Labels for each run')
    parser.add_argument('-o', '--output-dir', default='parameter_plots', help='Output directory for plots')
    parser.add_argument('--colors', nargs='+', help='Colors for each run (any valid matplotlib color)')
    args = parser.parse_args()

    # Parameter names (in order they appear in the CSV files)
    param_names = ['SFR efficiency', 'Reheating epsilon', 'Ejection efficiency', 
                  'Reincorporation Factor', 'Radio Mode', 'Quasar Mode', 'Black Hole growth']
    
    # Redshifts to analyze
    redshifts = [0.0, 0.2, 0.5, 0.8, 1.1, 1.5, 2.0, 2.4, 3.1, 3.6, 
                 4.6, 5.7, 6.3, 7.7, 8.5, 10.4]

    # Load data from each run directory
    run_data = []
    for run_dir in args.run_dirs:
        print(f"\nProcessing run directory: {run_dir}")
        run_results = load_all_params(run_dir, param_names, redshifts)
        if any(result for result in run_results):
            run_data.append(run_results)
        else:
            print(f"Warning: No valid data found in {run_dir}")

    if not run_data:
        print("No valid data found in any of the specified directories!")
        return

    # Create comparison plots
    print("\nCreating comparison plots...")
    plot_parameter_evolution_comparison(
        run_data, 
        param_names, 
        args.output_dir,
        args.labels if args.labels else None,
        args.colors if args.colors else None
    )

    print(f"\nPlots saved to: {args.output_dir}")

if __name__ == '__main__':
    main()