import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
from matplotlib.legend_handler import HandlerTuple

def load_all_params(run_dirs, param_names):
    all_run_data = []
    for run_dir in run_dirs:
        particle_data = {}
        best_params = {}
        best_scores = {}
        
        # Check for a single parameter file in the directory
        filename = os.path.join(run_dir, 'params_z0.csv')
        if os.path.exists(filename):
            df = pd.read_csv(filename, delimiter='\t', header=None, names=param_names + ['score'])
            particle_data = df.iloc[:-2]  # All rows except last two
            best_params = df.iloc[-2].values[:-1]  # Second-to-last row as best parameters
            best_scores = float(df.iloc[-1].iloc[0])  # Last row as best score
            all_run_data.append((particle_data, best_params, best_scores))
        else:
            print(f"Warning: No parameter file found in {run_dir}")
            all_run_data.append((None, None, None))
    return all_run_data

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

def plot_parameter_evolution_comparison(
    run_data, param_names, output_dir, run_labels=None, colors=None,
    additional_run_data=None, additional_run_labels=None, additional_colors=None,
    legend_loc='lower left',space_file='/fred/oz004/mbradley/sage-model/autocalibration/space.txt'
):
    os.makedirs(output_dir, exist_ok=True)
    space_data = load_space_file(space_file)
    n_params = len(param_names)
    n_runs = len(run_data)
    if colors is None:
        colors = plt.cm.rainbow(np.linspace(0, 1, n_runs))
    if run_labels is None:  
        run_labels = [f'Run {i+1}' for i in range(n_runs)]
    
    # Handle additional result sets
    if additional_run_data:
        n_additional_runs = len(additional_run_data)
        if additional_colors is None:
            additional_colors = plt.cm.Paired(np.linspace(0, 1, n_additional_runs))
        if additional_run_labels is None:
            additional_run_labels = [f'Additional Run {i+1}' for i in range(n_additional_runs)]
    
    n_rows = (n_params + 1) // 2  # Calculate rows for 4x2 grid (adjust dynamically)
    fig, axs = plt.subplots(n_rows, 2, figsize=(15, n_rows * 5))
    axs_flat = axs.flatten() if n_params > 1 else [axs]  # Handle cases with 1 parameter
    
    for i, param in enumerate(param_names):
        ax = axs_flat[i]
        global_best_values = []  # Collect all best values for this parameter
        
        # Plot main run data
        for run_idx, (particle_data, best_params, best_scores) in enumerate(run_data):
            if particle_data is None or best_params is None:
                continue
            
            param_values = particle_data.iloc[:, i].values
            mean_value = np.mean(param_values)
            std_value = np.std(param_values)
            best_value = best_params[i]
            global_best_values.append(best_value)

            color = colors[run_idx]
            ax.errorbar(
                run_labels[run_idx], best_value, yerr=std_value, fmt='*', color=color, 
                label='Total score' if run_idx == 0 else None, capsize=5
            )
            if run_idx > 0:
                prev_best_value = run_data[run_idx - 1][1][i]
                ax.plot([run_labels[run_idx - 1], run_labels[run_idx]], 
                        [prev_best_value, best_value], '-', color=color)
        
        # Plot additional run data
        if additional_run_data:
            for run_idx, (particle_data, best_params, best_scores) in enumerate(additional_run_data):
                if particle_data is None or best_params is None:
                    continue
                
                param_values = particle_data.iloc[:, i].values
                mean_value = np.mean(param_values)
                std_value = np.std(param_values)
                best_value = best_params[i]
                global_best_values.append(best_value)
                
                color = additional_colors[run_idx]
                ax.errorbar(
                    additional_run_labels[run_idx], best_value, yerr=std_value, fmt='d', color='k', 
                    label='Normalised score' if run_idx == 0 else None, capsize=5,alpha=0.5
                )
                if run_idx > 0:
                    prev_best_value = additional_run_data[run_idx - 1][1][i]
                    ax.plot([additional_run_labels[run_idx - 1], additional_run_labels[run_idx]], 
                            [prev_best_value, best_value], '--', color='k',alpha=0.5)

        # Add global mean line
        if global_best_values:
            global_mean = np.mean(global_best_values)
            ax.axhline(global_mean, color='red', linestyle='--', linewidth=1, label='Global Mean')
        
        # Add parameter bounds
        if param in space_data:
            param_info = space_data[param]
            ax.axhspan(param_info['lb'], param_info['ub'], color='grey', alpha=0.1)
            ax.axhline(param_info['lb'], color='grey', linestyle='--', alpha=0.5)
            ax.axhline(param_info['ub'], color='grey', linestyle='--', alpha=0.5)
            bound_margin = (param_info['ub'] - param_info['lb']) * 0.1
            ax.set_ylim(param_info['lb'] - bound_margin, param_info['ub'] + bound_margin)
        
        #ax.set_xlabel('Runs')  
        #ax.set_ylabel('Value')
        ax.set_title(param)
        
        # Rotate x-axis labels
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=15, ha='right')
        
        # Add the legend
        if i == 0:
            handles, labels = ax.get_legend_handles_labels()
            unique_labels = dict(zip(labels, handles))  # Keep unique entries
            ax.legend(unique_labels.values(), unique_labels.keys(), loc=legend_loc)
    
    # Hide any unused subplots
    for j in range(len(axs_flat)):
        if j >= n_params:
            axs_flat[j].axis('off')
            
    fig.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_evolution_comparison.png'), dpi=300)
"""
def plot_parameter_evolution_comparison(run_data, param_names, output_dir, run_labels=None, colors=None, space_file='/Users/Documents/mbradley/sage-model/autocalibration/space.txt',
                                        additional_run_data=None, additional_run_labels=None, additional_colors=None):
    #os.makedirs(output_dir, exist_ok=True)
    space_data = load_space_file(space_file)
    #os.makedirs(output_dir, exist_ok=True)
    
    n_params = len(param_names)
    n_runs = len(run_data)
    if colors is None:
        colors = plt.cm.rainbow(np.linspace(0, 1, n_runs))
    if run_labels is None:  
        run_labels = [f'Run {i+1}' for i in range(n_runs)]
    
    # Handle additional result sets
    if additional_run_data:
        n_additional_runs = len(additional_run_data)
        if additional_colors is None:
            additional_colors = plt.cm.Paired(np.linspace(0, 1, n_additional_runs))
        if additional_run_labels is None:
            additional_run_labels = [f'Additional Run {i+1}' for i in range(n_additional_runs)]
    
    n_rows = (n_params + 1) // 2  # Calculate rows for 4x2 grid (adjust dynamically)
    fig, axs = plt.subplots(n_rows, 2, figsize=(15, n_rows * 5))
    axs_flat = axs.flatten() if n_params > 1 else [axs]  # Handle cases with 1 parameter
    
    for i, param in enumerate(param_names):
        ax = axs_flat[i]
        global_best_values = []  # Collect all best values for this parameter
        
        # Plot main run data
        for run_idx, (particle_data, best_params, best_scores) in enumerate(run_data):
            if particle_data is None or best_params is None:
                continue
            
            param_values = particle_data.iloc[:, i].values
            mean_value = np.mean(param_values)
            std_value = np.std(param_values)
            best_value = best_params[i]
            global_best_values.append(best_value)

            color = colors[run_idx]
            ax.errorbar(
                run_labels[run_idx], best_value, yerr=std_value, fmt='o', color=color, 
                label='Runs' if run_idx == 0 else None, capsize=5
            )
            if run_idx > 0:
                prev_best_value = run_data[run_idx - 1][1][i]
                ax.plot([run_labels[run_idx - 1], run_labels[run_idx]], 
                        [prev_best_value, best_value], '-o', color=color)
        
        # Plot additional run data
        if additional_run_data:
            for run_idx, (particle_data, best_params, best_scores) in enumerate(additional_run_data):
                if particle_data is None or best_params is None:
                    continue
                
                param_values = particle_data.iloc[:, i].values
                mean_value = np.mean(param_values)
                std_value = np.std(param_values)
                best_value = best_params[i]
                global_best_values.append(best_value)
                
                color = additional_colors[run_idx]
                ax.errorbar(
                    additional_run_labels[run_idx], best_value, yerr=std_value, fmt='s', color=color, 
                    label='Additional Runs' if run_idx == 0 else None, capsize=5
                )
                if run_idx > 0:
                    prev_best_value = additional_run_data[run_idx - 1][1][i]
                    ax.plot([additional_run_labels[run_idx - 1], additional_run_labels[run_idx]], 
                            [prev_best_value, best_value], '--', color='k')

        
        # Add global mean line
        if global_best_values:
            global_mean = np.mean(global_best_values)
            ax.axhline(global_mean, color='r', linestyle='--', linewidth=1, label='Global Mean')

        # Add parameter bounds
        if param in space_data:
            param_info = space_data[param]
            ax.axhspan(param_info['lb'], param_info['ub'], color='grey', alpha=0.1)
            ax.axhline(param_info['lb'], color='grey', linestyle='--', alpha=0.5)
            ax.axhline(param_info['ub'], color='grey', linestyle='--', alpha=0.5)
            bound_margin = (param_info['ub'] - param_info['lb']) * 0.1
            ax.set_ylim(param_info['lb'] - bound_margin, param_info['ub'] + bound_margin)
        
        #ax.set_xlabel('Runs')  
        #ax.set_ylabel('Value')
        ax.set_title(param)

        # Rotate x-axis labels to avoid overlap
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=15, ha='right')
        
        # Add the legend with proper marker style
        if i == 0:
            handles, labels = ax.get_legend_handles_labels()
            unique_labels = dict(zip(labels, handles))  # Keep unique entries
            
            # Update legend with the correct circle marker for 'Runs'
            legend = ax.legend(
                unique_labels.values(), 
                unique_labels.keys(), 
                loc='upper right', 
                handler_map={'Runs': HandlerTuple(ndivide=None)}
            )
    
    
    # Hide any unused subplots
    for j in range(len(axs_flat)):
        if j >= n_params:
            axs_flat[j].axis('off')
            
    fig.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_evolution_comparison.png'), dpi=300)
"""

def main():
    run_dirs = ["/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0", 
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1z2",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0BHBMz0",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1BHBMz0"]
    
    additional_run_dirs = ["/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0_difftotal", 
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1_difftotal",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1z2_difftotal",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0BHBMz0_difftotal",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1BHBMz0_difftotal2",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_SMFz0z1z2BHBMz0_difftotal2",
                "/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_testingHSMR"]
                
    run_labels = ['SMF z=0', 'SMF z=0,1', 'SMF z=0,1,2', 'SMF z=0/BHBM z=0', 'SMF z=0,1/BHBM z=0']
    additional_run_labels = ['SMF z=0', 'SMF z=0,1', 'SMF z=0,1,2', 'SMF z=0/BHBM z=0', 'SMF z=0,1/BHBM z=0',
                              'SMF z=0,1,2/BHBM z=0', 'SMF/HSMR/BHBM z=0']

    colors = ['darkblue', 'mediumblue', 'darkorchid', 'darkmagenta', 'mediumvioletred']
    additional_colors = ['darkblue', 'mediumblue', 'darkorchid', 'darkmagenta', 'mediumvioletred', 'black', 'black']
    
    output_dir = "/fred/oz004/mbradley/sage-model/output/"
    
    param_names = ['SFR efficiency', 'Reheating epsilon', 'Ejection efficiency', 
                   'Reincorporation Factor', 'Radio Mode', 'Quasar Mode', 'Black Hole growth']
                   
    run_data = load_all_params(run_dirs, param_names)
    additional_run_data = load_all_params(additional_run_dirs, param_names)
    plot_parameter_evolution_comparison(run_data, param_names, output_dir, run_labels, colors,
                                        additional_run_data, additional_run_labels, additional_colors)

if __name__ == '__main__':
    main()
