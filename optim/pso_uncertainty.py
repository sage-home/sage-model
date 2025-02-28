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

def load_pso_data(tracks_dir):
    """
    Load PSO particle positions and scores from ALL track files.
    
    Parameters:
    -----------
    tracks_dir : str
        Directory containing PSO track files
        
    Returns:
    --------
    positions : array
        Particle positions across all iterations
    scores : array
        Particle scores across all iterations
    """
    # Get list of track files
    pos_files = sorted(glob.glob(os.path.join(tracks_dir, "track_*_pos.npy")))
    fx_files = sorted(glob.glob(os.path.join(tracks_dir, "track_*_fx.npy")))
    
    if not pos_files or not fx_files:
        raise ValueError(f"No track files found in directory: {tracks_dir}")
    
    # Load ALL iterations
    positions_list = []
    scores_list = []
    
    for pos_file, fx_file in zip(pos_files, fx_files):
        try:
            positions = np.load(pos_file)
            scores = np.load(fx_file)
            
            positions_list.append(positions)
            scores_list.append(scores)
        except Exception as e:
            print(f"Error loading {pos_file} or {fx_file}: {str(e)}")
    
    # Concatenate across all iterations
    return np.vstack(positions_list), np.concatenate(scores_list)

def load_final_pso_values(csv_path, param_names):
    """
    Load the final best PSO values from the output CSV file
    
    Parameters:
    -----------
    csv_path : str
        Path to the CSV file containing PSO results
    param_names : list
        List of parameter names to extract
        
    Returns:
    --------
    dict : Parameter names mapped to their final best values
    """
    if not csv_path or not os.path.exists(csv_path):
        print(f"CSV file not found: {csv_path}")
        return None
    
    try:
        # Read the CSV file - PSO results are stored with parameters in the second-to-last row
        # and the final score in the last row
        with open(csv_path, 'r') as f:
            lines = f.readlines()
            
        if len(lines) < 2:
            print(f"CSV file format unexpected: {csv_path}")
            return None
            
        # Parse the second-to-last line which contains the best parameter values
        final_values_line = lines[-2].strip().split('\t')
        
        # Map parameters to values
        final_values = {}
        for i, param in enumerate(param_names):
            if i < len(final_values_line):
                final_values[param] = float(final_values_line[i])
        
        return final_values
    
    except Exception as e:
        print(f"Error reading CSV file {csv_path}: {str(e)}")
        return None

def analyze_pso_uncertainties(positions, scores, param_names, final_values=None):
    """
    Analyze PSO results to determine parameter uncertainties.
    Always use final values as reference points when available.
    
    Parameters:
    -----------
    positions : array
        Particle positions
    scores : array
        Particle scores
    param_names : array
        Parameter names
    final_values : dict, optional
        Final best parameter values from PSO to use as reference points
        
    Returns:
    --------
    dict : Contains parameter statistics including:
        - best_value : Final or distribution best value for each parameter
        - std : Standard deviation (symmetric error)
        - percentile_errors : Asymmetric errors based on 16th/84th percentiles
        - relative_uncertainty : Relative error (std/abs(best_value))
    """
    results = {}
    
    # Find distribution best particle (only as fallback if final values aren't available)
    best_idx = np.argmin(scores)
    
    # Calculate statistics for each parameter
    for i, param in enumerate(param_names):
        param_values = positions[:, i]
        
        # Always use final PSO value when available
        if final_values and param in final_values:
            ref_value = final_values[param]
            ref_source = 'final'
        else:
            # Fallback to distribution best only if final values aren't available
            ref_value = param_values[best_idx]
            ref_source = 'distribution'
        
        # Calculate standard deviation
        std = np.std(param_values)
        
        # Calculate percentiles
        p16, p50, p84 = np.percentile(param_values, [16, 50, 84])
        
        # Calculate asymmetric errors relative to reference value
        lower_error = ref_value - p16
        upper_error = p84 - ref_value
        
        # Calculate relative uncertainty
        relative_uncertainty = std / abs(ref_value) if ref_value != 0 else np.inf
        
        # Store results
        results[param] = {
            'ref_value': ref_value,  # The reference value used for stats
            'ref_source': ref_source,  # Where the reference came from (final or distribution)
            'median': p50,           # Median value
            'std': std,              # Standard deviation
            'percentile_errors': (lower_error, upper_error),  # Asymmetric errors
            'percentiles': (p16, p50, p84),  # Raw percentiles
            'relative_uncertainty': relative_uncertainty,
            'values': param_values   # Store for plotting
        }
    
    return results, scores

def plot_corner_with_uncertainties(results, scores, output_dir=None, final_values=None, cmap='viridis'):
    """
    Create corner plot showing parameter correlations with uncertainties.
    Points are colored by their raw fitness scores across all iterations.
    
    Parameters:
    -----------
    results : dict
        Parameter analysis results from analyze_pso_uncertainties
    scores : array
        All particle scores
    output_dir : str, optional
        Directory to save output plot
    final_values : dict, optional
        Final best parameter values from PSO
    cmap : str
        Colormap for points
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
        
        # Add final best value if available
        if final_values:
            param_x = g.x_vars[plt.gca().get_subplotspec().colspan.start]
            param_y = g.y_vars[plt.gca().get_subplotspec().rowspan.start]
            if param_x in final_values and param_y in final_values:
                plt.scatter(final_values[param_x], final_values[param_y], 
                           color='red', marker='X', s=100, 
                           label='Final Best', edgecolor='black', zorder=10)
    
    # Custom histogram function
    def hist_func(x, **kwargs):
        plt.hist(x, bins=10, alpha=0.8)
        
        # Add final value marker if available
        param = g.x_vars[plt.gca().get_subplotspec().colspan.start]
        if final_values and param in final_values:
            plt.axvline(final_values[param], color='red', linewidth=2, 
                       linestyle='--', zorder=10, label='Final Best')
    
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
                              ax=ax, levels=[0.683, 0.955, 0.997], color='r', alpha=1.0, linestyles='-')
                except (ValueError, np.linalg.LinAlgError):
                    pass
    
    # Add legend to the first plot
    if final_values:
        handles, labels = plt.gca().get_legend_handles_labels()
        if handles:
            g.fig.legend(handles, labels, loc='upper right')
    
    plt.tight_layout()
    
    if output_dir:
        plt.savefig(os.path.join(output_dir, 'parameter_correlations.png'), 
                   dpi=300, bbox_inches='tight')
    
    return g

def create_uncertainty_report(results, final_values=None):
    """
    Generate a text report of parameter uncertainties
    
    Parameters:
    -----------
    results : dict
        Parameter analysis results from analyze_pso_uncertainties
    final_values : dict, optional
        Final best parameter values from PSO
    """
    lines = ["Parameter Uncertainty Analysis", "="*30, ""]
    
    for param, stats in results.items():
        lines.append(f"\n{param}:")
        
        # Report best value
        lines.append(f"  Best value: {stats['ref_value']:.6f}")
            
        # Report distribution statistics
        lines.append(f"  Distribution median: {stats['median']:.6f}")
            
        # Report error statistics
        lines.append(f"  Symmetric error (±1σ): {stats['std']:.6f}")
        lines.append(f"  Asymmetric errors: +{stats['percentile_errors'][1]:.6f}/-{stats['percentile_errors'][0]:.6f}")
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

def plot_parameter_distributions(results, scores, output_dir=None, final_values=None):
    """
    Create visualization of parameter distributions from ALL PSO iterations.
    
    Parameters:
    -----------
    results : dict
        Output from analyze_pso_uncertainties
    scores : array
        Particle fitness scores
    output_dir : str, optional
        Directory to save plots
    final_values : dict, optional
        Final best parameter values from PSO
    """
    n_params = len(results)
    fig, axes = plt.subplots(n_params, 1, figsize=(10, 4*n_params))
    if n_params == 1:
        axes = [axes]
    
    for ax, (param, stats) in zip(axes, results.items()):
        # Create histogram with KDE overlay
        sns.histplot(stats['values'], ax=ax, stat='density', alpha=0.6, 
                   kde=True, color='skyblue', edgecolor='none')
        
        # Add vertical lines for percentiles
        p16, p50, p84 = stats['percentiles']
        ax.axvline(p16, color='k', linestyle='--', alpha=0.7, 
                   label='16th/84th percentiles')
        ax.axvline(p84, color='k', linestyle='--', alpha=0.7)
        ax.axvline(p50, color='k', linestyle='-', alpha=0.7, label='Median')
        
        # Add vertical line for reference value (PSO final value)
        ax.axvline(stats['ref_value'], color='red', linestyle='-', linewidth=2,
                  label='PSO value (reference)')
        
        # Add text with statistics
        text_lines = []
        
        # Reference value information
        text_lines.append(f'Reference: PSO value ({stats["ref_value"]:.6f})')
            
        # Distribution statistics
        text_lines.append(f'Distribution median: {stats["median"]:.6f}')
            
        # Error statistics
        text_lines.append(f'Std dev: {stats["std"]:.6f}')
        text_lines.append(f'Asymmetric errors: +{stats["percentile_errors"][1]:.6f}/-{stats["percentile_errors"][0]:.6f}')
        text_lines.append(f'Relative uncertainty: {stats["relative_uncertainty"]:.2%}')
        
        # Add constraint quality
        if stats['relative_uncertainty'] < 0.2:
            quality = "Well constrained"
        elif stats['relative_uncertainty'] < 0.5:
            quality = "Moderately constrained"
        else:
            quality = "Poorly constrained"
        text_lines.append(f'Constraint quality: {quality}')
        
        text = '\n'.join(text_lines)
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

def analyze_and_plot(tracks_dir, space_file, output_dir=None, csv_output_path=None):
    """
    Full analysis pipeline - load data, analyze uncertainties, and create plots.
    
    Parameters:
    -----------
    tracks_dir : str
        Directory containing PSO track files
    space_file : str
        Path to space.txt file defining parameter bounds
    output_dir : str, optional
        Directory to save plots and reports
    csv_output_path : str, optional
        Path to CSV file with final PSO results
    """
    # Load data
    space = load_space(space_file)
    positions, scores = load_pso_data(tracks_dir)
    
    # Load final PSO values - critical for using as reference
    final_values = None
    if csv_output_path:
        final_values = load_final_pso_values(csv_output_path, space['name'])
        if final_values:
            print(f"Loaded final PSO values: {final_values}")
    
    # Analyze uncertainties - passing final_values to use as reference
    results, scores = analyze_pso_uncertainties(positions, scores, space['name'], final_values)
    
    # Create visualizations
    plot_corner_with_uncertainties(results, scores, output_dir, final_values)
    plot_parameter_distributions(results, scores, output_dir, final_values)
    
    # Generate report
    report = create_uncertainty_report(results, final_values)
    print(report)
    
    if output_dir:
        report_path = os.path.join(output_dir, 'uncertainty_report.txt')
        with open(report_path, 'w') as f:
            f.write(report)
    
    return results, scores


if __name__ == "__main__":
    import argparse
    import sys
    
    # Check if arguments were provided
    if len(sys.argv) > 1:
        # Create argument parser
        parser = argparse.ArgumentParser(description='Analyze PSO uncertainty and create plots')
        parser.add_argument('tracks_dir', help='Directory containing PSO track files')
        parser.add_argument('space_file', help='Path to space.txt file defining parameter bounds')
        parser.add_argument('output_dir', help='Directory to save plots and reports')
        parser.add_argument('csv_output_path', nargs='?', help='Path to CSV file with final PSO results')
        
        # Parse arguments
        args = parser.parse_args()
        
        # Get space filename if a directory was passed
        if os.path.isdir(args.space_file):
            print(f"Space file argument is a directory. Looking for space.txt in {args.space_file}")
            potential_files = [
                os.path.join(args.space_file, "space.txt"),
                os.path.join(args.space_file, "autocalibration", "space.txt")
            ]
            
            for potential_file in potential_files:
                if os.path.exists(potential_file):
                    args.space_file = potential_file
                    print(f"Found space file: {args.space_file}")
                    break
            else:
                raise FileNotFoundError(f"Could not find space.txt in {args.space_file}")
                
        # Run analysis with provided arguments
        print(f"Analyzing PSO data from: {args.tracks_dir}")
        print(f"Using space file: {args.space_file}")
        print(f"Saving output to: {args.output_dir}")
        if args.csv_output_path:
            print(f"Using CSV results: {args.csv_output_path}")
            
        try:
            results, scores = analyze_and_plot(
                args.tracks_dir, 
                args.space_file, 
                args.output_dir, 
                args.csv_output_path
            )
            print("Analysis complete!")
        except Exception as e:
            print(f"Error during analysis: {e}")
            import traceback
            traceback.print_exc()
    else:
        # Default values for testing or if no arguments provided
        tracks_dir = "path/to/tracks"
        space_file = "path/to/space.txt"
        output_dir = "path/to/output"
        csv_output_path = "path/to/params_results.csv"
        
        print("No command line arguments provided. Using default paths:")
        print(f"  tracks_dir: {tracks_dir}")
        print(f"  space_file: {space_file}")
        print(f"  output_dir: {output_dir}")
        print(f"  csv_output_path: {csv_output_path}")
        print("To run with your own paths, use:")
        print("  python pso_uncertainty.py tracks_dir space_file output_dir [csv_output_path]")