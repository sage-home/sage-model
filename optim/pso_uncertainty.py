import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats
import os
import glob
import pandas as pd
from matplotlib.colors import Normalize
from matplotlib.cm import ScalarMappable

def load_space(space_file):
    """Load parameter space definition"""
    return np.genfromtxt(space_file, 
                        dtype=[('name', 'U30'), ('plot_label', 'U30'), 
                              ('is_log', 'i4'), ('lb', 'f8'), ('ub', 'f8')],
                        delimiter=',')

def load_pso_data(tracks_dir, skip_iterations=3):
    """
    Load PSO particle positions and scores from track files, skipping early iterations.
    
    Parameters:
    -----------
    tracks_dir : str
        Directory containing PSO track files
    skip_iterations : int
        Number of initial iterations to skip
        
    Returns:
    --------
    positions : array
        Particle positions across iterations (excluding skipped iterations)
    scores : array
        Particle scores across iterations (excluding skipped iterations)
    """
    # Get list of track files
    pos_files = sorted(glob.glob(os.path.join(tracks_dir, "track_*_pos.npy")))
    fx_files = sorted(glob.glob(os.path.join(tracks_dir, "track_*_fx.npy")))
    
    if not pos_files or not fx_files:
        raise ValueError(f"No track files found in directory: {tracks_dir}")
    
    # Load ALL iterations
    positions_list = []
    scores_list = []
    
    # Skip the first N iteration files
    for pos_file, fx_file in zip(pos_files[skip_iterations:], fx_files[skip_iterations:]):
        try:
            positions = np.load(pos_file)
            scores = np.load(fx_file)
            
            positions_list.append(positions)
            scores_list.append(scores)
        except Exception as e:
            print(f"Error loading {pos_file} or {fx_file}: {str(e)}")
    
    # Concatenate across remaining iterations
    return np.vstack(positions_list), np.concatenate(scores_list)

def analyze_pso_uncertainties(positions, scores, param_names):
    """
    Analyze PSO results to determine parameter uncertainties.
    Note: positions and scores should already be filtered for desired iterations.
    
    Parameters:
    -----------
    positions : array
        Particle positions (after skipping iterations)
    scores : array
        Particle scores (after skipping iterations)
    param_names : array
        Parameter names
    """
    results = {}
    
    # Find best particle
    best_idx = np.argmin(scores)
    
    # Calculate statistics for each parameter
    for i, param in enumerate(param_names):
        param_values = positions[:, i]
        best_value = param_values[best_idx]
        
        # Calculate standard deviation
        std = np.std(param_values)
        
        # Calculate asymmetric errors using percentiles
        p16, p84 = np.percentile(param_values, [16, 84])
        lower_error = best_value - p16
        upper_error = p84 - best_value
        
        # Calculate relative uncertainty
        relative_uncertainty = std / abs(best_value) if best_value != 0 else np.inf
        
        # Store results
        results[param] = {
            'best_value': best_value,
            'std': std,
            'percentile_errors': (lower_error, upper_error),
            'relative_uncertainty': relative_uncertainty,
            'values': param_values  # Store for plotting
        }
    
    return results, scores

def plot_corner_with_uncertainties(results, scores, output_dir=None, cmap='viridis'):
    """
    Create corner plot showing parameter correlations with uncertainties.
    Points are colored by their raw fitness scores.
    """
    data = {param: stats['values'] for param, stats in results.items()}
    df = pd.DataFrame(data)
    
    # Create normalization for raw scores
    norm = Normalize(vmin=np.min(scores), vmax=np.max(scores))
    
    # Create corner plot
    g = sns.PairGrid(df)
    
    # Custom scatter plot function with colormapping
    def colored_scatter(x, y, **kwargs):
        plt.scatter(x, y, c=scores, cmap=cmap, norm=norm, alpha=0.3, s=30)
    
    # Custom histogram function
    def hist_func(x, **kwargs):
        plt.hist(x, bins=10, edgecolor='black', color='steelblue')
    
    # Map the plotting functions
    g.map_diag(hist_func)
    g.map_offdiag(colored_scatter)
    
    # Add contours to off-diagonal plots
    for i in range(len(df.columns)):
        for j in range(len(df.columns)):
            if i > j:  # Lower triangle only
                ax = g.axes[i, j]
                try:
                    sns.kdeplot(data=df, x=df.columns[j], y=df.columns[i], 
                              ax=ax, levels=[0.683, 0.954], colors=['firebrick', 'firebrick'], linestyles=['-', '--'], alpha=1.0)
                except (ValueError, np.linalg.LinAlgError):
                    pass
    
    plt.tight_layout()
    
    if output_dir:
        plt.savefig(os.path.join(output_dir, 'parameter_correlations.png'), 
                   dpi=300, bbox_inches='tight')
    
    return g

def create_uncertainty_report(results):
    """Generate a text report of parameter uncertainties"""
    lines = ["Parameter Uncertainty Analysis (Excluding first 10 iterations)", "="*50, ""]
    
    for param, stats in results.items():
        lines.append(f"\n{param}:")
        lines.append(f"  Best value: {stats['best_value']:.3f}")
        lines.append(f"  Symmetric error (±1σ): {stats['std']:.3f}")
        lines.append(f"  Asymmetric errors: +{stats['percentile_errors'][1]:.3f}/-{stats['percentile_errors'][0]:.3f}")
        lines.append(f"  Relative uncertainty: {stats['relative_uncertainty']:.2%}")
        
        # Categorize constraint quality
        if stats['relative_uncertainty'] < 0.2:
            quality = "Well constrained"
        elif stats['relative_uncertainty'] < 0.5:
            quality = "Moderately constrained"
        else:
            quality = "Poorly constrained"
        lines.append(f"  Constraint quality: {quality}")
    
    return "\n".join(lines)

def plot_parameter_distributions(results, scores, output_dir=None):
    """
    Create visualization of parameter distributions from PSO iterations.
    Note: Results and scores should already exclude early iterations.
    """
    n_params = len(results)
    fig, axes = plt.subplots(n_params, 1, figsize=(10, 4*n_params))
    if n_params == 1:
        axes = [axes]
    
    # Find best score and corresponding parameter values
    best_idx = np.argmin(scores)
    
    for ax, (param, stats) in zip(axes, results.items()):
        # Create histogram
        sns.histplot(stats['values'], ax=ax, stat='density', alpha=0.6)
        
        # Add vertical lines for best value and errors
        best_value = stats['best_value']
        ax.axvline(best_value, color='r', linestyle='-', label='Best value')
        ax.axvline(best_value - stats['percentile_errors'][0], color='k', 
                  linestyle='--', label='16th/84th percentiles')
        ax.axvline(best_value + stats['percentile_errors'][1], color='k', 
                  linestyle='--')
        
        # Add text with statistics
        text = f'Best value: {best_value:.3f}\n'
        text += f'Std dev: {stats["std"]:.3f}\n'
        text += f'Asymmetric errors: +{stats["percentile_errors"][1]:.3f}/-{stats["percentile_errors"][0]:.3f}\n'
        text += f'Relative uncertainty: {stats["relative_uncertainty"]:.2%}'
        
        # Add constraint quality
        if stats['relative_uncertainty'] < 0.2:
            quality = "Well constrained"
        elif stats['relative_uncertainty'] < 0.5:
            quality = "Moderately constrained"
        else:
            quality = "Poorly constrained"
        text += f'\nConstraint quality: {quality}'
        
        ax.text(0.98, 0.95, text, transform=ax.transAxes, 
                horizontalalignment='right', verticalalignment='top',
                bbox=dict(facecolor='white', alpha=0.8))
        
        ax.set_title(f'{param} Distribution')
        ax.legend(loc='upper left')
        ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    if output_dir:
        plt.savefig(os.path.join(output_dir, 'parameter_distributions.png'), 
                   dpi=300, bbox_inches='tight')
    
    return fig

def analyze_and_plot(tracks_dir, space_file, output_dir=None):
    """
    Full analysis pipeline - load data, analyze uncertainties, and create plots.
    Skips first 10 iterations by default.
    
    Parameters:
    -----------
    tracks_dir : str
        Directory containing PSO track files
    space_file : str
        Path to space.txt file defining parameter bounds
    output_dir : str, optional
        Directory to save plots and reports
    """
    # Load data (skipping first 10 iterations)
    space = load_space(space_file)
    positions, scores = load_pso_data(tracks_dir)
    
    # Analyze uncertainties
    results, scores = analyze_pso_uncertainties(positions, scores, space['name'])
    
    # Create visualizations
    plot_corner_with_uncertainties(results, scores, output_dir)
    plot_parameter_distributions(results, scores, output_dir)
    
    # Generate report
    report = create_uncertainty_report(results)
    print(report)
    
    if output_dir:
        report_path = os.path.join(output_dir, 'uncertainty_report.txt')
        with open(report_path, 'w') as f:
            f.write(report)
    
    return results, scores

if __name__ == "__main__":
    tracks_dir = "path/to/tracks"
    space_file = "path/to/space.txt"
    output_dir = "path/to/output"
    
    results, scores = analyze_and_plot(tracks_dir, space_file, output_dir)