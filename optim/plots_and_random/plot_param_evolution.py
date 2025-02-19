#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np
import os
from pathlib import Path
import argparse

def z2tL(z, h=0.6774, Omega_M=0.3089, Omega_Lambda=0.6911, Omega_R=0, nele=100000):
    """Convert redshift z to look-back time tL."""
    if z <= 0:
        return 0.
    
    H_0 = 100*h
    Omega_k = 1 - Omega_R - Omega_M - Omega_Lambda
    Mpc_km = 3.08567758e19
    yr_s = 60*60*24*365.24

    zprime = np.linspace(0, z, nele)
    integrand = 1./((1+zprime)*np.sqrt(Omega_R*(1+zprime)**4 + Omega_M*(1+zprime)**3 + Omega_k*(1+zprime)**2 + Omega_Lambda))
    integrated = 0.5 * np.sum(np.diff(zprime)*(integrand[:-1] + integrand[1:]))
    tL = np.divide(integrated*Mpc_km, H_0*yr_s*1e9)

    return tL

def setup_plot_style():
    """Setup consistent plot styling"""
    sns.set_theme(style="white", font_scale=1.2)
    sns.set_palette("deep")
    plt.rcParams['figure.facecolor'] = 'white'
    plt.rcParams['axes.facecolor'] = 'white'

def load_space_file(space_file):
    """Load parameter information from space.txt"""
    space_data = {}
    param_name_mapping = {
        'SfrEfficiency': 'SFR efficiency',
        'FeedbackReheatingEpsilon': 'Reheating epsilon',
        'FeedbackEjectionEfficiency': 'Ejection efficiency',
        'ReIncorporationFactor': 'Reincorporation Factor',
        'RadioModeEfficiency': 'Radio Mode',
        'QuasarModeEfficiency': 'Quasar Mode',
        'BlackHoleGrowthRate': 'Black Hole growth'
    }
    
    with open(space_file, 'r') as f:
        for line in f:
            parts = line.strip().split(',')
            if len(parts) >= 5:
                internal_name = parts[0]
                if internal_name in param_name_mapping:
                    display_name = param_name_mapping[internal_name]
                    space_data[display_name] = {
                        'internal_name': internal_name,
                        'is_log': int(parts[2]),
                        'lb': float(parts[3]),
                        'ub': float(parts[4])
                    }
    return space_data

def load_params_from_csv(filename, param_names):
    """Load parameter values from a single CSV file"""
    try:
        df = pd.read_csv(filename, delimiter='\t', header=None, names=param_names + ['score'])
        particle_data = df.iloc[:-2]
        best_params = df.iloc[-2].values[:-1]
        best_score = float(df.iloc[-1].iloc[0])
        return particle_data, best_params, best_score
    except Exception as e:
        print(f"Error processing {filename}: {str(e)}")
        return None, None, None

def extract_redshift(filename):
    """Extract redshift value from filename"""
    base = os.path.basename(filename)
    if '_z' not in base:
        return None
        
    z_str = base.split('_z')[1].split('_')[0].split('.')[0]
    
    # Mapping of filename strings to actual redshift values
    redshift_map = {
        '0': 0.0,
        '02': 0.2,
        '05': 0.5,
        '08': 0.8,
        '11': 1.1,
        '15': 1.5,
        '20': 2.0,
        '24': 2.4,
        '31': 3.1,
        '36': 3.6,
        '46': 4.6,
        '57': 5.7,
        '63': 6.3,
        '77': 7.7,
        '85': 8.5,
        '104': 10.4
    }
    
    if z_str in redshift_map:
        return redshift_map[z_str]
    
    return None

def create_evolution_plot(param_names, redshifts, times, particle_data, best_params, space_data, 
                         output_file, x_values, xlabel):
    """Create parameter evolution plot with specified x-axis"""
    fig, axs = plt.subplots(4, 2, figsize=(15, 20))
    axs_flat = axs.flatten()
    
    for i, param in enumerate(param_names):
        ax = axs_flat[i]
        
        best_values = [best_params[z][i] for z in redshifts]
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
        
        # Plot mean with shaded error region
        ax.fill_between(x_values, means - stds, means + stds, 
                       alpha=0.3, color='orange', label='±1σ range')
        
        # Plot mean line
        ax.plot(x_values, means, '-', color='red', 
                linewidth=1.5, label='Mean')
        
        # Plot best values
        ax.plot(x_values, best_values, 'k--o', 
                linewidth=2, markersize=8, label='Best fit')
        
        # Set x-axis limits with small padding
        ax.set_xlim(left=min(x_values)-0.1, right=max(x_values)+0.1)
        
        # Plot global mean
        ax.axhline(global_mean, color='blue', linestyle=':', 
                   linewidth=2, label='Global Mean')
        
        # Add parameter bounds
        if param in space_data:
            param_info = space_data[param]
            ax.axhspan(param_info['lb'], param_info['ub'], color='grey', alpha=0.1)
            ax.axhline(param_info['lb'], color='grey', linestyle='--', alpha=0.5)
            ax.axhline(param_info['ub'], color='grey', linestyle='--', alpha=0.5)
            bound_margin = (param_info['ub'] - param_info['lb']) * 0.1
            ax.set_ylim(param_info['lb'] - bound_margin, param_info['ub'] + bound_margin)
        
        ax.set_xlabel(xlabel)
        ax.set_ylabel('Value')
        ax.set_title(param)
        
        if i == 0:
            ax.legend()
    
    if len(param_names) < len(axs_flat):
        fig.delaxes(axs_flat[-1])
    
    plt.tight_layout()
    plt.savefig(output_file, bbox_inches='tight', dpi=300)
    plt.close()

def plot_parameter_evolution(csv_dir, space_file, output_dir='parameter_plots'):
    """Create parameter evolution plots from CSV files"""
    os.makedirs(output_dir, exist_ok=True)
    setup_plot_style()
    space_data = load_space_file(space_file)
    
    param_names = ['SFR efficiency', 'Reheating epsilon', 'Ejection efficiency', 
                   'Reincorporation Factor', 'Radio Mode', 'Quasar Mode', 'Black Hole growth']
    
    # Find and process CSV files
    csv_files = []
    for file in os.listdir(csv_dir):
        if file.endswith('.csv') and 'params_z' in file:
            file_path = os.path.join(csv_dir, file)
            z = extract_redshift(file)
            if z is not None:
                csv_files.append((file_path, z))
    
    if not csv_files:
        print("No parameter CSV files found!")
        return
    
    csv_files.sort(key=lambda x: x[1])
    
    particle_data = {}
    best_params = {}
    best_scores = {}
    redshifts = []
    
    for file_path, z in csv_files:
        data, params, score = load_params_from_csv(file_path, param_names)
        if data is not None:
            particle_data[z] = data
            best_params[z] = params
            best_scores[z] = score
            redshifts.append(z)
    
    if not particle_data:
        print("No valid data found in CSV files!")
        return
    
    # Calculate lookback times
    lookback_times = [z2tL(z) for z in redshifts]
    
    # Sort data by redshift for redshift plot
    redshifts_sorted = sorted(redshifts)
    
    # Create redshift plot with data sorted by redshift
    create_evolution_plot(param_names, redshifts, lookback_times, particle_data, best_params, 
                         space_data, os.path.join(output_dir, 'parameter_evolution_redshift.png'), 
                         redshifts_sorted, 'Redshift')
    
    # Sort data by lookback time for lookback time plot
    lookback_times_sorted = sorted([z2tL(z) for z in redshifts])
    redshifts_by_time = sorted(redshifts, key=lambda z: z2tL(z))
    
    # Create lookback time plot with data sorted by lookback time
    create_evolution_plot(param_names, redshifts_by_time, lookback_times_sorted, particle_data, best_params, 
                         space_data, os.path.join(output_dir, 'parameter_evolution_lookback.png'), 
                         lookback_times_sorted, 'Look-back Time (Gyr)')
    
    print(f"Plots saved to:\n"
          f"  {os.path.join(output_dir, 'parameter_evolution_redshift.png')}\n"
          f"  {os.path.join(output_dir, 'parameter_evolution_lookback.png')}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Create parameter evolution plots from CSV files')
    parser.add_argument('csv_dir', help='Directory containing parameter CSV files')
    parser.add_argument('space_file', help='Path to space.txt file containing parameter bounds')
    parser.add_argument('-o', '--output-dir', default='parameter_plots', 
                        help='Output directory for plots')
    args = parser.parse_args()
    
    plot_parameter_evolution(args.csv_dir, args.space_file, args.output_dir)