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
    best_pos : array
        Position of the particle with the best overall score
    best_score : float
        Best score across all iterations
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
    all_positions = np.vstack(positions_list)
    all_scores = np.concatenate(scores_list)
    
    # Create a mask for valid (non-NaN) scores
    valid_mask = ~np.isnan(all_scores)
    if not np.any(valid_mask):
        raise ValueError("All scores are NaN! Cannot find best solution.")
    
    # Filter positions and scores to only include valid entries
    valid_positions = all_positions[valid_mask]
    valid_scores = all_scores[valid_mask]
    
    # Find the best position across all valid iterations
    best_idx = np.argmin(valid_scores)
    best_pos = valid_positions[best_idx]
    best_score = valid_scores[best_idx]
    
    # Print info about the best solution found from tracks
    print(f"Found best solution in tracks:")
    print(f"Best score: {best_score}")
    print(f"Best position: {best_pos}")
    print(f"Total particles: {len(all_scores)}, Valid particles: {np.sum(valid_mask)}")
    
    return valid_positions, valid_scores, best_pos, best_score

def analyze_pso_uncertainties(positions, scores, param_names, best_position):
    """
    Analyze PSO results to determine parameter uncertainties.
    All statistics are calculated relative to the final best position.
    
    Parameters:
    -----------
    positions : array
        Particle positions
    scores : array
        Particle scores
    param_names : array
        Parameter names
    best_position : array
        The best position found (from final iteration or tracks)
        
    Returns:
    --------
    dict : Contains parameter statistics including:
        - best_value : Best value for each parameter
        - std : Standard deviation (symmetric error)
        - percentile_errors : Asymmetric errors based on 16th/84th percentiles
        - relative_uncertainty : Relative error (std/abs(best_value))
    """
    results = {}
    
    # Calculate statistics for each parameter
    for i, param in enumerate(param_names):
        param_values = positions[:, i]
        
        # Make sure we don't have any NaN values
        valid_values = param_values[~np.isnan(param_values)]
        if len(valid_values) == 0:
            print(f"Warning: Parameter {param} has no valid values!")
            continue
            
        # Use the best position as reference
        ref_value = best_position[i]
        
        # Calculate standard deviation
        std = np.std(valid_values)
        
        # Calculate percentiles
        p16, p50, p84 = np.percentile(valid_values, [16, 50, 84])
        
        # Calculate asymmetric errors relative to reference value
        lower_error = ref_value - p16
        upper_error = p84 - ref_value
        
        # Calculate relative uncertainty
        relative_uncertainty = std / abs(ref_value) if ref_value != 0 else np.inf
        
        # Store results
        results[param] = {
            'ref_value': ref_value,      # The best final value
            'median': p50,               # Median value
            'std': std,                  # Standard deviation
            'percentile_errors': (lower_error, upper_error),  # Asymmetric errors
            'percentiles': (p16, p50, p84),  # Raw percentiles
            'relative_uncertainty': relative_uncertainty,
            'values': valid_values       # Store for plotting (only valid values)
        }
    
    return results

def plot_corner_with_uncertainties(results, scores, output_dir=None, best_position=None, cmap='viridis'):
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
    best_position : array, optional
        Best parameter position to mark on plot
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
    
    # Custom histogram function
    def hist_func(x, **kwargs):
        plt.hist(x, bins=10, alpha=0.8)
    
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
    handles, labels = plt.gca().get_legend_handles_labels()
    if handles:
        g.fig.legend(handles, labels, loc='upper right')
    
    plt.tight_layout()
    
    if output_dir:
        plt.savefig(os.path.join(output_dir, 'parameter_correlations.png'), 
                   dpi=300, bbox_inches='tight')
    
    return g

def create_uncertainty_report(results):
    """
    Generate a text report of parameter uncertainties
    
    Parameters:
    -----------
    results : dict
        Parameter analysis results from analyze_pso_uncertainties
    """
    lines = ["Parameter Uncertainty Analysis", "="*30, ""]
    
    for param, stats in results.items():
        lines.append(f"\n{param}:")
            
        # Report best value (from final position)
        lines.append(f"  Best value: {stats['ref_value']:.6f}")
        
        # Report distribution statistics (for reference only)
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

def plot_parameter_distributions(results, scores, output_dir=None, best_position=None):
    """
    Create visualization of parameter distributions from ALL PSO iterations.
    Only marks the best final position, not distribution best.
    
    Parameters:
    -----------
    results : dict
        Output from analyze_pso_uncertainties
    scores : array
        Particle fitness scores
    output_dir : str, optional
        Directory to save plots
    best_position : array, optional
        Best position found by PSO
    """
    n_params = len(results)
    fig, axes = plt.subplots(n_params, 1, figsize=(10, 4*n_params))
    if n_params == 1:
        axes = [axes]
    
    for i, (ax, (param, stats)) in enumerate(zip(axes, results.items())):
        # Create histogram with KDE overlay
        sns.histplot(stats['values'], ax=ax, stat='density', alpha=0.6, 
                   kde=True, color='skyblue', edgecolor='none')
        
        # Add vertical lines for percentiles
        p16, p50, p84 = stats['percentiles']
        ax.axvline(p16, color='k', linestyle='--', alpha=0.7, 
                   label='16th/84th percentiles')
        ax.axvline(p84, color='k', linestyle='--', alpha=0.7)
        ax.axvline(p50, color='k', linestyle='-', alpha=0.7, label='Median')
        
        # Add vertical line for final best value
        ax.axvline(stats['ref_value'], color='red', linestyle='-', linewidth=2,
                  label='Final best value')
        
        # Add text with statistics
        text_lines = []
        text_lines.append(f'Best value: {stats["ref_value"]:.6f}')
        text_lines.append(f'Median: {stats["median"]:.6f}')
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
    Uses the best position from tracks rather than distribution statistics.
    
    Parameters:
    -----------
    tracks_dir : str
        Directory containing PSO track files
    space_file : str
        Path to space.txt file defining parameter bounds
    output_dir : str, optional
        Directory to save plots and reports
    csv_output_path : str, optional (not used anymore)
        Path to CSV file with final PSO results
    """
    try:
        # Load data - now uses updated function that returns best position
        space = load_space(space_file)
        positions, scores, best_position, best_score = load_pso_data(tracks_dir)
        
        # Print the parameter names and best values
        param_names = space['name']
        print("\nBest parameter values:")
        for i, param in enumerate(param_names):
            print(f"  {param}: {best_position[i]}")
        
        # Analyze uncertainties using the best position from tracks
        results = analyze_pso_uncertainties(positions, scores, param_names, best_position)
        
        # Create visualizations - passing best_position from tracks
        plot_corner_with_uncertainties(results, scores, output_dir, best_position)
        plot_parameter_distributions(results, scores, output_dir, best_position)
        
        # Generate report
        report = create_uncertainty_report(results)
        print(report)
        
        if output_dir:
            report_path = os.path.join(output_dir, 'uncertainty_report.txt')
            with open(report_path, 'w') as f:
                f.write(report)
        
        return results, scores
        
    except Exception as e:
        print(f"Error during analysis: {str(e)}")
        import traceback
        traceback.print_exc()
        
        # Try to load the CSV file as a fallback
        if csv_output_path and os.path.exists(csv_output_path):
            print(f"\nTrying to use CSV file as fallback: {csv_output_path}")
            try:
                # Read the CSV file - PSO results should be in second-to-last row
                with open(csv_output_path, 'r') as f:
                    lines = f.readlines()
                
                if len(lines) >= 2:
                    best_values = [float(x) for x in lines[-2].strip().split('\t')]
                    best_score = float(lines[-1].strip())
                    
                    print(f"Successfully loaded from CSV:")
                    print(f"Best score: {best_score}")
                    
                    # Print parameter values
                    param_names = space['name']
                    best_position = np.array(best_values[:len(param_names)])
                    
                    print("\nBest parameter values (from CSV):")
                    for i, param in enumerate(param_names):
                        if i < len(best_position):
                            print(f"  {param}: {best_position[i]}")
                    
                    return None, None
            except Exception as csv_error:
                print(f"Error reading CSV file: {str(csv_error)}")
        
        return None, None


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
        parser.add_argument('csv_output_path', nargs='?', help='Path to CSV file with final PSO results (not used)')
        
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
            
        try:
            results, scores = analyze_and_plot(
                args.tracks_dir, 
                args.space_file, 
                args.output_dir
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
        
        print("No command line arguments provided. Using default paths:")
        print(f"  tracks_dir: {tracks_dir}")
        print(f"  space_file: {space_file}")
        print(f"  output_dir: {output_dir}")
        print("To run with your own paths, use:")
        print("  python pso_uncertainty.py tracks_dir space_file output_dir")