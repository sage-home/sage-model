#!/usr/bin/env python3
"""
Script to analyze results from multiple PSO runs.
Computes median and 1-sigma uncertainties for optimized parameters.
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
import sys
import glob
from pathlib import Path

def load_space(space_file):
    """Load parameter space definition"""
    space = np.genfromtxt(space_file, 
                        dtype=[('name', 'U30'), ('plot_label', 'U30'), 
                              ('is_log', 'i4'), ('lb', 'f8'), ('ub', 'f8')],
                        delimiter=',')
    return space

def read_pso_csv(csv_path):
    """
    Read PSO results from CSV file.
    
    The CSV format is:
    - All rows except last 2: particle positions and fitness values
    - Second to last row: best position (may have blank line before it)
    - Last row: best fitness
    
    Returns:
    --------
    best_position : array
        Best parameter values found
    best_fitness : float
        Best fitness value
    """
    try:
        data = pd.read_csv(csv_path, sep='\t', header=None)
        
        # Remove any rows that are all NaN
        data = data.dropna(how='all')
        
        # Last row contains best fitness
        best_fitness = data.iloc[-1, 0]
        
        # Second to last row contains best position
        # Get only non-NaN values from this row
        best_position = data.iloc[-2, :].dropna().values
        
        return best_position, best_fitness
    except Exception as e:
        print(f"Error reading {csv_path}: {e}")
        return None, None

def analyze_multiple_runs(multi_run_dir, space_file):
    """
    Analyze results from multiple PSO runs.
    
    Parameters:
    -----------
    multi_run_dir : str
        Directory containing multiple PSO run subdirectories
    space_file : str
        Path to space.txt file defining parameters
        
    Returns:
    --------
    results_df : DataFrame
        Statistics for each parameter
    """
    # Load parameter space
    space = load_space(space_file)
    param_names = space['name']
    n_params = len(param_names)
    
    # Find all CSV files from individual runs
    csv_files = sorted(glob.glob(os.path.join(multi_run_dir, "run_*/params.csv")))
    
    if not csv_files:
        print(f"ERROR: No CSV files found in {multi_run_dir}")
        return None
    
    print(f"Found {len(csv_files)} PSO runs to analyze")
    print(f"Parameters: {', '.join(param_names)}")
    print("")
    
    # Collect best positions from all runs
    best_positions = []
    best_fitnesses = []
    
    for i, csv_file in enumerate(csv_files, 1):
        print(f"Loading run {i}: {os.path.basename(os.path.dirname(csv_file))}")
        best_pos, best_fit = read_pso_csv(csv_file)
        
        if best_pos is not None and len(best_pos) == n_params:
            best_positions.append(best_pos)
            best_fitnesses.append(best_fit)
            print(f"  Best fitness: {best_fit:.6f}")
        else:
            print(f"  WARNING: Skipping run (invalid data)")
    
    if not best_positions:
        print("ERROR: No valid PSO results found!")
        return None
    
    # Convert to numpy arrays
    best_positions = np.array(best_positions)
    best_fitnesses = np.array(best_fitnesses)
    
    print(f"\nSuccessfully loaded {len(best_positions)} runs")
    print("="*80)
    
    # Calculate statistics for each parameter
    results = []
    
    for i, param in enumerate(param_names):
        values = best_positions[:, i]
        
        # Calculate statistics
        median = np.median(values)
        mean = np.mean(values)
        std = np.std(values)
        
        # Calculate percentiles for asymmetric errors
        p16, p50, p84 = np.percentile(values, [16, 50, 84])
        lower_error = p50 - p16
        upper_error = p84 - p50
        
        # Find best run
        best_run_idx = np.argmin(best_fitnesses)
        best_value = best_positions[best_run_idx, i]
        
        # Calculate range
        min_val = np.min(values)
        max_val = np.max(values)
        
        results.append({
            'Parameter': param,
            'Best Value': best_value,
            'Median': median,
            'Mean': mean,
            'Std Dev': std,
            '16th Percentile': p16,
            '84th Percentile': p84,
            'Lower Error (-)': lower_error,
            'Upper Error (+)': upper_error,
            'Min': min_val,
            'Max': max_val,
            'Relative Error (%)': (std / abs(median) * 100) if median != 0 else np.inf
        })
    
    results_df = pd.DataFrame(results)
    
    return results_df, best_positions, best_fitnesses, param_names

def create_summary_plots(results_df, best_positions, best_fitnesses, param_names, output_dir):
    """Create summary plots of the analysis"""
    
    # Set style
    sns.set_style("whitegrid")
    plt.rcParams['figure.dpi'] = 150
    
    n_params = len(param_names)
    
    # 1. Parameter distributions with median and 1-sigma
    fig, axes = plt.subplots(n_params, 1, figsize=(10, 3*n_params))
    if n_params == 1:
        axes = [axes]
    
    for i, (ax, param) in enumerate(zip(axes, param_names)):
        values = best_positions[:, i]
        
        # Histogram
        ax.hist(values, bins=20, alpha=0.7, color='steelblue', edgecolor='black')
        
        # Get statistics
        median = results_df.loc[i, 'Median']
        std = results_df.loc[i, 'Std Dev']
        p16 = results_df.loc[i, '16th Percentile']
        p84 = results_df.loc[i, '84th Percentile']
        
        # Add vertical lines
        ax.axvline(median, color='red', linestyle='--', linewidth=2, label=f'Median: {median:.4f}')
        ax.axvline(p16, color='orange', linestyle=':', linewidth=1.5, label=f'16th: {p16:.4f}')
        ax.axvline(p84, color='orange', linestyle=':', linewidth=1.5, label=f'84th: {p84:.4f}')
        
        # Shade 1-sigma region
        ax.axvspan(p16, p84, alpha=0.2, color='orange', label=f'1σ region')
        
        ax.set_xlabel(f'{param} Value')
        ax.set_ylabel('Count')
        ax.set_title(f'{param}: {median:.4f} +{p84-median:.4f} -{median-p16:.4f}')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_distributions.png'), dpi=150, bbox_inches='tight')
    plt.close()
    
    # 2. Fitness distribution across runs
    fig, ax = plt.subplots(figsize=(10, 6))
    
    run_numbers = np.arange(1, len(best_fitnesses) + 1)
    colors = plt.cm.viridis(np.linspace(0, 1, len(best_fitnesses)))
    
    # Sort by fitness
    sorted_indices = np.argsort(best_fitnesses)
    
    ax.bar(run_numbers, best_fitnesses[sorted_indices], color=colors[sorted_indices], 
           edgecolor='black', alpha=0.8)
    ax.set_xlabel('Run (sorted by fitness)', fontsize=12)
    ax.set_ylabel('Best Fitness Value', fontsize=12)
    ax.set_title('PSO Fitness Across Multiple Runs', fontsize=14, fontweight='bold')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add horizontal line for mean
    mean_fitness = np.mean(best_fitnesses)
    ax.axhline(mean_fitness, color='red', linestyle='--', linewidth=2, 
               label=f'Mean: {mean_fitness:.4f}')
    ax.legend()
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'fitness_distribution.png'), dpi=150, bbox_inches='tight')
    plt.close()
    
    # 3. Corner plot (if we have more than 1 parameter)
    if n_params > 1:
        fig, axes = plt.subplots(n_params, n_params, figsize=(3*n_params, 3*n_params))
        
        for i in range(n_params):
            for j in range(n_params):
                ax = axes[i, j]
                
                if i == j:
                    # Diagonal: histograms
                    values = best_positions[:, i]
                    ax.hist(values, bins=15, alpha=0.7, color='steelblue', edgecolor='black')
                    median = results_df.loc[i, 'Median']
                    ax.axvline(median, color='red', linestyle='--', linewidth=2)
                    
                    if i == 0:
                        ax.set_title(param_names[i], fontsize=10)
                    if i == n_params - 1:
                        ax.set_xlabel(param_names[i], fontsize=9)
                    ax.set_yticks([])
                    
                elif i > j:
                    # Lower triangle: scatter plots
                    x_values = best_positions[:, j]
                    y_values = best_positions[:, i]
                    
                    # Color by fitness
                    scatter = ax.scatter(x_values, y_values, c=best_fitnesses, 
                                       cmap='viridis_r', s=50, alpha=0.6, edgecolors='black')
                    
                    # Add medians
                    x_median = results_df.loc[j, 'Median']
                    y_median = results_df.loc[i, 'Median']
                    ax.axvline(x_median, color='red', linestyle='--', linewidth=1, alpha=0.5)
                    ax.axhline(y_median, color='red', linestyle='--', linewidth=1, alpha=0.5)
                    ax.plot(x_median, y_median, 'r*', markersize=15)
                    
                    if j == 0:
                        ax.set_ylabel(param_names[i], fontsize=9)
                    if i == n_params - 1:
                        ax.set_xlabel(param_names[j], fontsize=9)
                else:
                    # Upper triangle: empty
                    ax.axis('off')
        
        # Adjust layout first to make room for colorbar
        plt.tight_layout()
        
        # Add colorbar for fitness below all plots
        fig.subplots_adjust(bottom=0.12)  # Make room at bottom
        cbar_ax = fig.add_axes([0.15, 0.04, 0.7, 0.02])  # [left, bottom, width, height]
        cbar = fig.colorbar(scatter, cax=cbar_ax, orientation='horizontal')
        cbar.set_label('Fitness Value', fontsize=10)
        
        plt.savefig(os.path.join(output_dir, 'parameter_corner_plot.png'), dpi=150, bbox_inches='tight')
        plt.close()
    
    print(f"\nPlots saved in {output_dir}")

def save_results(results_df, best_positions, best_fitnesses, output_dir):
    """Save results to files"""
    
    # 1. Summary statistics table
    summary_file = os.path.join(output_dir, 'parameter_summary.txt')
    with open(summary_file, 'w') as f:
        f.write("="*80 + "\n")
        f.write("Multiple PSO Run Analysis - Parameter Summary\n")
        f.write("="*80 + "\n\n")
        
        f.write(results_df.to_string(index=False))
        f.write("\n\n")
        
        f.write("="*80 + "\n")
        f.write("Best Run Summary\n")
        f.write("="*80 + "\n")
        best_run_idx = np.argmin(best_fitnesses)
        f.write(f"Best run fitness: {best_fitnesses[best_run_idx]:.6f}\n")
        f.write(f"Best run index: {best_run_idx + 1}\n\n")
        
        f.write("Fitness Statistics:\n")
        f.write(f"  Mean: {np.mean(best_fitnesses):.6f}\n")
        f.write(f"  Median: {np.median(best_fitnesses):.6f}\n")
        f.write(f"  Std Dev: {np.std(best_fitnesses):.6f}\n")
        f.write(f"  Min: {np.min(best_fitnesses):.6f}\n")
        f.write(f"  Max: {np.max(best_fitnesses):.6f}\n")
    
    print(f"\nSummary saved to: {summary_file}")
    
    # 2. CSV with all data
    csv_file = os.path.join(output_dir, 'parameter_summary.csv')
    results_df.to_csv(csv_file, index=False)
    print(f"CSV saved to: {csv_file}")
    
    # 3. LaTeX-friendly table
    latex_file = os.path.join(output_dir, 'parameter_summary_latex.txt')
    with open(latex_file, 'w') as f:
        f.write("% LaTeX table for parameter summary\n")
        f.write("\\begin{table}[h]\n")
        f.write("\\centering\n")
        f.write("\\caption{PSO Parameter Summary}\n")
        f.write("\\begin{tabular}{lcccc}\n")
        f.write("\\hline\n")
        f.write("Parameter & Best Value & Median & $1\\sigma$ Error & Relative Error (\\%) \\\\\n")
        f.write("\\hline\n")
        
        for _, row in results_df.iterrows():
            param = row['Parameter'].replace('_', '\\_')
            best = row['Best Value']
            median = row['Median']
            lower = row['Lower Error (-)']
            upper = row['Upper Error (+)']
            rel_err = row['Relative Error (%)']
            
            f.write(f"{param} & {best:.4f} & {median:.4f} & $^{{+{upper:.4f}}}_{{-{lower:.4f}}}$ & {rel_err:.2f} \\\\\n")
        
        f.write("\\hline\n")
        f.write("\\end{tabular}\n")
        f.write("\\end{table}\n")
    
    print(f"LaTeX table saved to: {latex_file}")
    
    # 4. All runs data
    all_runs_file = os.path.join(output_dir, 'all_runs_data.csv')
    columns = list(results_df['Parameter'].values) + ['Fitness']
    data = np.column_stack([best_positions, best_fitnesses])
    df_all = pd.DataFrame(data, columns=columns)
    df_all.to_csv(all_runs_file, index=False)
    print(f"All runs data saved to: {all_runs_file}")

def print_summary(results_df, best_fitnesses):
    """Print summary to console"""
    
    print("\n" + "="*80)
    print("PARAMETER SUMMARY")
    print("="*80)
    print("\nMedian values with 1-sigma uncertainties:")
    print("-"*80)
    
    for _, row in results_df.iterrows():
        param = row['Parameter']
        median = row['Median']
        lower = row['Lower Error (-)']
        upper = row['Upper Error (+)']
        
        print(f"{param:30s}: {median:.6f} +{upper:.6f} -{lower:.6f}")
    
    print("\n" + "="*80)
    print("FITNESS SUMMARY")
    print("="*80)
    print(f"Number of runs: {len(best_fitnesses)}")
    print(f"Best fitness:   {np.min(best_fitnesses):.6f}")
    print(f"Mean fitness:   {np.mean(best_fitnesses):.6f} ± {np.std(best_fitnesses):.6f}")
    print(f"Median fitness: {np.median(best_fitnesses):.6f}")
    print("="*80 + "\n")

def main():
    """Main function"""
    
    if len(sys.argv) < 3:
        print("Usage: python analyze_multiple_pso.py <multi_run_dir> <space_file>")
        print("\nExample:")
        print("  python analyze_multiple_pso.py /path/to/millennium_pso_multi_20241027_123456 space.txt")
        sys.exit(1)
    
    multi_run_dir = sys.argv[1]
    space_file = sys.argv[2]
    
    if not os.path.exists(multi_run_dir):
        print(f"ERROR: Directory not found: {multi_run_dir}")
        sys.exit(1)
    
    if not os.path.exists(space_file):
        print(f"ERROR: Space file not found: {space_file}")
        sys.exit(1)
    
    print("="*80)
    print("Multiple PSO Run Analysis")
    print("="*80)
    print(f"Input directory: {multi_run_dir}")
    print(f"Space file: {space_file}")
    print("")
    
    # Analyze results
    results = analyze_multiple_runs(multi_run_dir, space_file)
    
    if results is None:
        print("ERROR: Analysis failed!")
        sys.exit(1)
    
    results_df, best_positions, best_fitnesses, param_names = results
    
    # Print summary to console
    print_summary(results_df, best_fitnesses)
    
    # Create plots
    print("Creating summary plots...")
    create_summary_plots(results_df, best_positions, best_fitnesses, param_names, multi_run_dir)
    
    # Save results
    print("\nSaving results...")
    save_results(results_df, best_positions, best_fitnesses, multi_run_dir)
    
    print("\n" + "="*80)
    print("Analysis complete!")
    print("="*80)

if __name__ == "__main__":
    main()
