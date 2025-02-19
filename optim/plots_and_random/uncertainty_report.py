#!/usr/bin/env python3

import re
import os
import numpy as np
import matplotlib.pyplot as plt

class ParameterData:
    def __init__(self, name):
        self.name = name
        self.best_value = None
        self.symmetric_error = None
        self.asymmetric_errors = None

def extract_parameter_data(filepath):
    """Extract parameter values and errors from uncertainty report"""
    parameters = {}
    current_param = None
    
    print(f"\nReading file: {filepath}")
    
    with open(filepath, 'r') as f:
        lines = f.readlines()
        
    for line in lines:
        line = line.strip()
        
        # Skip empty lines and separators
        if not line or line.startswith('=') or "Parameter Uncertainty Analysis" in line:
            continue
            
        # Check for parameter name (lines ending with colon and not indented)
        if not line.startswith(' ') and line.endswith(':'):
            current_param = line.rstrip(':')
            if current_param not in parameters:
                parameters[current_param] = ParameterData(current_param)
            continue
            
        # Extract values if we have a current parameter
        if current_param:
            param = parameters[current_param]
            if "Best value:" in line:
                param.best_value = float(re.search(r'Best value: ([\d.-]+)', line).group(1))
            elif "Symmetric error" in line:
                param.symmetric_error = float(re.search(r'Symmetric error \(±1σ\): ([\d.-]+)', line).group(1))
            elif "Asymmetric errors:" in line:
                match = re.search(r'Asymmetric errors: \+([\d.-]+)/-([\d.-]+)', line)
                if match:
                    param.asymmetric_errors = (float(match.group(1)), float(match.group(2)))
                
    return parameters


def plot_parameter_grid():

    def get_marker_style(label):
        if 'SMF @ z=0,1,2 ALL swarm' in label:
            return '*'  # Star marker
        return 'o'     # Default circular marker
    
    folder_data = {
        "SFRSN": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSN",
        "SFRSN restricted": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSN_restrictedbounds",
        "SFRSN restricted swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSN_restrictedbounds2",
        "SFRSN restricted 2 swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSN_restrictedbounds3",
        "SFRSNREIN": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSNREIN",
        "SFRSNREIN restricted": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSNREIN_restrictedbounds",
        "SFRSNREIN restricted swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSNREIN_restrictedbounds2",
        "SFRSNREIN restricted 2 swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_SFRSNREIN_restrictedbounds3",
        "FREE": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_free",
        "FREE restricted": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_free_restrictedbounds",
        "FREE restricted swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_free_restrictedbounds2",
        "FREE restricted 2 swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_free_restrictedbounds3",
        "ALL": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_all",
        "ALL restricted": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_all_restrictedbounds",
        "ALL restricted swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_all_restrictedbounds2",
        "ALL restricted 2 swarm": "/fred/oz004/mbradley/sage-model/output/testing/miniuchuu_pso_SMF_all_restrictedbounds3",
        "SMF @ z=0 focused swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0",
        "SMF @ z=1 focused swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz1",
        "SMF @ z=2 focused swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz2",
        "SMF @ z=0,1 focused swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1",
        "SMF @ z=0,1,2 focused swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1z2",
        "SMF @ z=0 ALL swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0_all",
        "SMF @ z=1 ALL swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz1_all",
        "SMF @ z=2 ALL swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz2_all",
        "SMF @ z=0,1 ALL swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1_all",
        "SMF @ z=0,1,2 ALL swarm": "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1z2_all"
    }

    space_files = {
        "SFRSN": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN.txt",
        "SFRSNREIN": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN_REIN.txt",
        "FREE": "/fred/oz004/mbradley/sage-model/autocalibration/space_allfree.txt",
        "ALL": "/fred/oz004/mbradley/sage-model/autocalibration/space.txt",
        "SFRSN restricted": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN_restricted_bounds.txt",
        "SFRSNREIN restricted": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN_REIN_restricted_bounds.txt",
        "FREE restricted": "/fred/oz004/mbradley/sage-model/autocalibration/space_allfree_restricted_bounds.txt",
        "ALL restricted": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt",
        "SFRSN restricted swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN_restricted_bounds.txt",
        "SFRSNREIN restricted swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN_REIN_restricted_bounds.txt",
        "FREE restricted swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_allfree_restricted_bounds.txt",
        "ALL restricted swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt",
        "SFRSN restricted 2 swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN_restricted_bounds.txt",
        "SFRSNREIN restricted 2 swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SFR_SN_REIN_restricted_bounds.txt",
        "FREE restricted 2 swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_allfree_restricted_bounds.txt",
        "ALL restricted 2 swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt",
        "SMF @ z=0 focused swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SMF.txt",
        "SMF @ z=1 focused swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SMF.txt",
        "SMF @ z=2 focused swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SMF.txt",
        "SMF @ z=0,1 focused swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SMF.txt",
        "SMF @ z=0,1,2 focused swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_SMF.txt",
        "SMF @ z=0 ALL swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt",
        "SMF @ z=1 ALL swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt",
        "SMF @ z=2 ALL swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt",
        "SMF @ z=0,1 ALL swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt",
        "SMF @ z=0,1,2 ALL swarm": "/fred/oz004/mbradley/sage-model/autocalibration/space_restricted_bounds.txt"
    }

    # Collect parameter data from all folders
    all_data = {}
    for label, folder_path in folder_data.items():
        report_path = os.path.join(folder_path, "uncertainty_report.txt")
        if os.path.exists(report_path):
            params = extract_parameter_data(report_path)
            if params:
                all_data[label] = params
                print(f"Loaded {len(params)} parameters from {label}")
    
    if not all_data:
        print("No data found!")
        return

    # Get unique parameters
    all_params = sorted(set().union(*[set(data.keys()) for data in all_data.values()]))
    print(f"Parameters found: {all_params}")
    
    # Load space files and extract parameter bounds
    parameter_bounds = {}
    for label, space_file in space_files.items():
        with open(space_file, 'r') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                name, _, _, lb, ub = line.split(',')
                parameter_bounds[label, name] = (float(lb), float(ub))
    
    # Calculate grid dimensions
    n_params = len(all_params)
    n_cols = 2
    n_rows = (n_params + n_cols - 1) // n_cols
    
    # Create figure
    fig, axes = plt.subplots(n_rows, n_cols, figsize=(15, 5*n_rows))
    if n_rows == 1:
        axes = axes.reshape(1, -1)
    
    # List of labels in the order we want to plot them
    labels = list(folder_data.keys())
    x_pos = np.arange(len(labels))

    # Plot each parameter
    for idx, param in enumerate(all_params):
        row = idx // n_cols
        col = idx % n_cols
        ax = axes[row, col]
        
        # Collect data for this parameter
        plot_data = {'x': [], 'y': [], 'err_minus': [], 'err_plus': [], 'sym_err': []}
        
        for i, label in enumerate(labels):
            if label in all_data and param in all_data[label]:
                param_data = all_data[label][param]
                if param_data.best_value is not None:
                    plot_data['x'].append(i)
                    plot_data['y'].append(param_data.best_value)
                    
                    if param_data.asymmetric_errors:
                        plot_data['err_plus'].append(abs(param_data.asymmetric_errors[0]))
                        plot_data['err_minus'].append(abs(param_data.asymmetric_errors[1]))
                        plot_data['sym_err'].append(np.mean([abs(param_data.asymmetric_errors[0]), abs(param_data.asymmetric_errors[1])]))
                    elif param_data.symmetric_error:
                        plot_data['err_plus'].append(abs(param_data.symmetric_error))
                        plot_data['err_minus'].append(abs(param_data.symmetric_error))
                        plot_data['sym_err'].append(abs(param_data.symmetric_error))
                    else:
                        plot_data['err_plus'].append(0)
                        plot_data['err_minus'].append(0)
                        plot_data['sym_err'].append(0)
        
        if plot_data['x']:
            # Convert lists to arrays for plotting
            x = np.array(plot_data['x'])
            y = np.array(plot_data['y'])
            err_minus = np.array(plot_data['err_minus'])
            err_plus = np.array(plot_data['err_plus'])
            sym_err = np.array(plot_data['sym_err'])
            
            for i, (xi, yi) in enumerate(zip(x, y)):
                marker = get_marker_style(labels[int(xi)])
                markersize = 500 if marker == '*' else 50  # Make stars bigger
                color = 'gold' if marker == '*' else 'k'
                ax.scatter(xi, yi, color=color, s=markersize, marker=marker, edgecolors='k', zorder=3)

            # Plot points and error bars
            #ax.scatter(x, y, color='blue', s=50, zorder=3)
            ax.errorbar(x, y, yerr=[err_minus, err_plus],
                       fmt='none', color='red', alpha=0.3, zorder=2, capsize=5)
            
            # Plot connecting line
            #ax.plot(x, y, ':', color='k', alpha=0.5, zorder=1)
            
            # Plot shaded region
            #ax.fill_between(x, y - err_minus, y + err_plus,
                          #color='orange', alpha=0.1)
            
            # Calculate and plot global mean line
            global_mean = np.mean(y)
            global_sym_err = np.mean(sym_err)
            ax.axhline(y=global_mean, color='red', linestyle='--', alpha=0.7, label='Global Mean')
            
            # Add average value and symmetric error in the top left corner
            avg_text = f"Avg: {global_mean:.3f} ± {global_sym_err:.3f}"
            ax.text(0.02, 0.98, avg_text, transform=ax.transAxes, fontsize=9,
                    verticalalignment='top', horizontalalignment='left',
                    bbox=dict(facecolor='white', alpha=0.7, edgecolor='none'))
        
        # Retrieve parameter bounds from space file
        for label in labels:
            if (label, param) in parameter_bounds:
                lb, ub = parameter_bounds[label, param]
                ax.axhspan(lb, ub, color='gray', alpha=0.05, zorder=0)
        
        # Customize subplot
        ax.set_title(param)
        ax.set_xticks(x_pos)
        ax.set_xticklabels(labels, rotation=45, ha='right')
        #ax.grid(True, alpha=0.3)
        
        # Add legend
        #ax.legend(loc='best', fontsize='small')
        
        # Add y=0 line if appropriate
        if len(plot_data['y']) > 0:  # Check if we have any data
            ymin, ymax = min(y - err_minus), max(y + err_plus)
            if ymin < 0 < ymax:
                ax.axhline(y=0, color='black', linestyle='-', alpha=0.2)
    
    # Remove empty subplots
    for idx in range(len(all_params), n_rows * n_cols):
        row = idx // n_cols
        col = idx % n_cols
        if row < len(axes) and col < len(axes[0]):  # Check if indices are valid
            fig.delaxes(axes[row, col])
    
    plt.tight_layout()
    plt.savefig('parameter_grid.png', dpi=300, bbox_inches='tight')
    print("\nCreated parameter_grid.png")

if __name__ == '__main__':
    plot_parameter_grid()