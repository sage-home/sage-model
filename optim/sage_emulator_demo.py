#!/usr/bin/env python3
"""
Demonstration script for SAGE emulators.
This script shows how to:
1. Train emulators on existing PSO run data
2. Use emulators to evaluate parameter sets
3. Run accelerated PSO using emulators
4. Compare emulator predictions with actual SAGE outputs
"""

import os
import numpy as np
import matplotlib.pyplot as plt
import argparse
import time
import pandas as pd
from sage_emulator import SAGEEmulator, EmulatorPSO

def parse_args():
    """Parse command-line arguments"""
    parser = argparse.ArgumentParser(description='SAGE emulator demonstration')
    # Support both old and new ways of specifying directories
    dir_group = parser.add_mutually_exclusive_group(required=False)  # Changed to not required
    dir_group.add_argument('--pso-dir', help='Directory containing PSO run outputs (legacy)')
    dir_group.add_argument('--pso-dirs', nargs='+', help='Multiple directories containing PSO run outputs')
    parser.add_argument('--space-file', default='./space.txt',
                        help='Path to space file defining parameter bounds')
    parser.add_argument('--output-dir', default='./emulator_output',
                        help='Directory to save emulator outputs')
    parser.add_argument('--constraints', nargs='+',
                        default=['SMF_z0', 'BHBM_z0', 'BHMF_z0'],
                        help='Constraints to train emulators for')
    parser.add_argument('--method', default='random_forest',
                        choices=['random_forest', 'gradient_boosting', 'gaussian_process', 'neural_network'],
                        help='ML method to use for emulation')
    parser.add_argument('--mode', default='train',
                        choices=['train', 'predict', 'pso', 'compare'],
                        help='Operation mode')
    parser.add_argument('--swarm-size', type=int, default=20,
                        help='Number of particles in PSO swarm')
    parser.add_argument('--max-iter', type=int, default=50,
                        help='Maximum number of PSO iterations')
    
    return parser.parse_args()

def train_emulators(args):
    """Train emulators for the specified constraints"""
    print(f"Training emulators for: {args.constraints}")
    
    # Define parameter names from space file
    param_names = []
    with open(args.space_file, 'r') as f:
        for line in f:
            parts = line.strip().split(',')
            if len(parts) >= 1:
                param_names.append(parts[0])
    
    print(f"Parameters: {param_names}")
    
    # Create output directory
    emulator_dir = os.path.join(args.output_dir, 'emulators')
    os.makedirs(emulator_dir, exist_ok=True)
    
    # Determine which directories to use
    if hasattr(args, 'pso_dirs') and args.pso_dirs:
        pso_dirs = args.pso_dirs
    else:
        pso_dirs = [args.pso_dir]
    
    # Train an emulator for each constraint
    emulators = {}
    training_metrics = {}
    
    for constraint in args.constraints:
        print(f"\n=== Training emulator for {constraint} ===")
        
        # Create and train the emulator
        emulator = SAGEEmulator(constraint, output_dir=emulator_dir)
        
        # Track time to evaluate performance
        start_time = time.time()
        
        try:
            # Train on all specified directories
            metrics = emulator.train(pso_dirs, param_names, method=args.method)
            training_metrics[constraint] = metrics
            emulators[constraint] = emulator
            
            elapsed_time = time.time() - start_time
            print(f"Training completed in {elapsed_time:.2f} seconds")
            
            # Create a plot of training metrics
            plot_training_metrics(metrics, constraint, os.path.join(args.output_dir, f'{constraint}_training.png'))
        
        except Exception as e:
            print(f"Error training emulator for {constraint}: {str(e)}")
    
    print("\nEmulator training summary:")
    for constraint, metrics in training_metrics.items():
        # Calculate average R² across all bins with metrics
        valid_r2 = [m['r2'] for m in metrics['test'] if m is not None]
        if valid_r2:
            avg_r2 = np.mean(valid_r2)
            print(f"{constraint}: Average test R² = {avg_r2:.4f}")
        else:
            print(f"{constraint}: No valid test metrics available")
    
    return emulators

def plot_training_metrics(metrics, constraint, output_path):
    """Plot training and testing metrics"""
    test_mse = [m['mse'] if m is not None else np.nan for m in metrics['test']]
    test_r2 = [m['r2'] if m is not None else np.nan for m in metrics['test']]
    
    fig, axs = plt.subplots(1, 2, figsize=(12, 5))
    
    # Plot MSE
    bins = np.arange(len(test_mse))
    axs[0].bar(bins, test_mse)
    axs[0].set_xlabel('Bin index')
    axs[0].set_ylabel('Mean Squared Error')
    axs[0].set_title(f'{constraint} - Test MSE')
    
    # Plot R²
    axs[1].bar(bins, test_r2)
    axs[1].set_xlabel('Bin index')
    axs[1].set_ylabel('R² Score')
    axs[1].set_title(f'{constraint} - Test R²')
    
    # Add overall average
    avg_r2 = np.nanmean(test_r2)
    axs[1].axhline(avg_r2, color='r', linestyle='--', label=f'Avg R² = {avg_r2:.4f}')
    axs[1].legend()
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=300)
    plt.close()

def predict_with_emulators(args):
    """Predict model outputs for a range of parameter values"""
    print("Running predictions with emulators")
    
    # Load the emulators
    emulator_dir = os.path.join(args.output_dir, 'emulators')
    emulators = {}
    
    for constraint in args.constraints:
        try:
            emulator = SAGEEmulator(constraint, output_dir=emulator_dir)
            emulator.load()
            emulators[constraint] = emulator
            print(f"Loaded emulator for {constraint}")
        except Exception as e:
            print(f"Error loading emulator for {constraint}: {str(e)}")
    
    if not emulators:
        print("No emulators loaded. Please train emulators first.")
        return
    
    # Load parameter bounds from space file
    param_names = []
    lb = []
    ub = []
    
    with open(args.space_file, 'r') as f:
        for line in f:
            parts = line.strip().split(',')
            if len(parts) >= 5:
                param_names.append(parts[0])
                lb.append(float(parts[3]))
                ub.append(float(parts[4]))
    
    # Generate a grid of parameter values for visualization
    first_emulator = list(emulators.values())[0]
    param_count = len(first_emulator.trained_parameters)
    
    # For simplicity, vary only the first two parameters
    if param_count >= 2:
        # Create a grid for the first two parameters
        p1_values = np.linspace(lb[0], ub[0], 5)
        p2_values = np.linspace(lb[1], ub[1], 5)
        
        # Use mean values for other parameters
        other_params = [(lb[i] + ub[i]) / 2 for i in range(2, param_count)]
        
        # Directory for prediction plots
        prediction_dir = os.path.join(args.output_dir, 'predictions')
        os.makedirs(prediction_dir, exist_ok=True)
        
        # For each constraint, show predictions for different parameter values
        for constraint, emulator in emulators.items():
            print(f"\nGenerating predictions for {constraint}")
            
            # Create a figure showing predictions for different parameter values
            fig, axs = plt.subplots(len(p1_values), len(p2_values), figsize=(15, 15))
            
            for i, p1 in enumerate(p1_values):
                for j, p2 in enumerate(p2_values):
                    # Create parameter set
                    params = np.array([p1, p2] + other_params).reshape(1, -1)
                    
                    # Get predictions
                    predictions = emulator.predict(params)
                    
                    # Plot
                    axs[i, j].plot(emulator.x_bins, predictions[0])
                    axs[i, j].set_title(f'{param_names[0]}={p1:.3f}, {param_names[1]}={p2:.3f}')
                    
                    # If this is the leftmost column, add y-axis label
                    if j == 0:
                        axs[i, j].set_ylabel('Value')
                    
                    # If this is the bottom row, add x-axis label
                    if i == len(p1_values) - 1:
                        axs[i, j].set_xlabel('Bin')
            
            plt.tight_layout()
            plt.savefig(os.path.join(prediction_dir, f'{constraint}_parameter_grid.png'), dpi=300)
            plt.close()
            
            print(f"Saved parameter grid plot to {prediction_dir}/{constraint}_parameter_grid.png")
    else:
        print("Not enough parameters to create a grid visualization")

def run_emulator_pso(args):
    """Run PSO optimization using emulators"""
    print("Running PSO with emulators")
    
    # Load the emulators
    emulator_dir = os.path.join(args.output_dir, 'emulators')
    emulators = {}
    
    for constraint in args.constraints:
        try:
            emulator = SAGEEmulator(constraint, output_dir=emulator_dir)
            emulator.load()
            emulators[constraint] = emulator
            print(f"Loaded emulator for {constraint}")
        except Exception as e:
            print(f"Error loading emulator for {constraint}: {str(e)}")
    
    if not emulators:
        print("No emulators loaded. Please train emulators first.")
        return
    
    # Set up weights for each constraint
    weights = {constraint: 1.0 for constraint in args.constraints}
    
    # Create output directory for PSO results
    pso_dir = os.path.join(args.output_dir, 'pso_results')
    os.makedirs(pso_dir, exist_ok=True)
    
    # Run PSO
    start_time = time.time()
    
    pso = EmulatorPSO(emulators, weights, args.space_file, output_dir=pso_dir)
    best_position, best_fitness = pso.run_pso(
        swarm_size=args.swarm_size,
        max_iter=args.max_iter
    )
    
    elapsed_time = time.time() - start_time
    print(f"\nPSO completed in {elapsed_time:.2f} seconds")
    print(f"Best position: {best_position}")
    print(f"Best fitness: {best_fitness}")
    
    # Visualize results
    print("\nVisualizing PSO results...")
    figures = pso.visualize_results()
    print(f"Generated {len(figures)} visualization plots in {pso_dir}")
    
    # Print parameter values in a nice table
    results_file = os.path.join(pso_dir, 'best_parameters.csv')
    if os.path.exists(results_file):
        results = pd.read_csv(results_file)
        print("\nBest parameter values:")
        print(results)

def compare_with_sage(args):
    """Compare emulator predictions with actual SAGE outputs"""
    print("This functionality requires running the SAGE model with specific parameters.")
    print("It is recommended to implement this in your specific workflow.")
    print("The basic approach would be:")
    print("1. Select a parameter set")
    print("2. Run SAGE with those parameters")
    print("3. Extract the outputs for each constraint")
    print("4. Load the emulators and get their predictions for the same parameters")
    print("5. Compare the predictions with the actual outputs")

def main():
    args = parse_args()
    
    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)

    # Check if we need PSO directory based on the mode
    if args.mode == 'train' or args.mode == 'compare':
        # Ensure at least one directory is specified
        if not hasattr(args, 'pso_dir') and not hasattr(args, 'pso_dirs'):
            print("Error: Either --pso-dir or --pso-dirs must be specified for train or compare modes")
            return
    
    if args.mode == 'train':
        train_emulators(args)
    elif args.mode == 'predict':
        predict_with_emulators(args)
    elif args.mode == 'pso':
        run_emulator_pso(args)
    elif args.mode == 'compare':
        compare_with_sage(args)
    else:
        print(f"Unknown mode: {args.mode}")
if __name__ == "__main__":
    main()