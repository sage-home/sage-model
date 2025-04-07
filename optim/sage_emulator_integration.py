#!/usr/bin/env python3
#!/usr/bin/env python3
"""
Integration script for combining SAGE emulators with existing PSO workflow.

This script demonstrates how to:
1. Use emulators to pre-screen parameter sets before running the full SAGE model
2. Implement a hybrid approach that alternates between emulator and full model evaluations
3. Update emulators during PSO to improve their accuracy as new data is collected
"""

import os
import sys
import numpy as np
import time
import pickle
import pandas as pd
import matplotlib.pyplot as plt
from sklearn.metrics import mean_squared_error, r2_score
import logging

# Add parent directory to path for imports
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

# Import SAGE PSO components
import common
import constraints
import analysis
from sage_emulator import SAGEEmulator, EmulatorPSO

# Set up logging
logging.basicConfig(level=logging.INFO, 
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger('emulator_integration')

class HybridPSO:
    """
    Hybrid PSO implementation that combines emulators with full SAGE model evaluations.
    
    This class implements several strategies for integrating emulators:
    1. Pre-screening: Use emulators to evaluate many parameter sets and only run 
       the full model on the most promising ones
    2. Alternating: Alternate between emulator and full model evaluations
    3. Dynamic: Use emulators more at the beginning and gradually shift to 
       full model evaluations as the search converges
    """
    
    def __init__(self, func, lb, ub, space, constraints_list, args, 
                 emulator_dir='./emulators', strategy='prescreening', 
                 emulator_ratio=0.8, update_interval=5):
        """
        Initialize the hybrid PSO.
        
        Parameters:
        -----------
        func : function
            Function to run SAGE (from execution.py)
        lb : array
            Lower bounds for parameters
        ub : array
            Upper bounds for parameters
        space : array
            Search space specification
        constraints_list : list
            List of constraint objects
        args : tuple
            Additional arguments for func
        emulator_dir : str
            Directory containing trained emulators
        strategy : str
            Integration strategy ('prescreening', 'alternating', 'dynamic')
        emulator_ratio : float
            Fraction of evaluations to do with emulators (for prescreening)
        update_interval : int
            How often to update emulators with new data (iterations)
        """
        self.func = func
        self.lb = lb
        self.ub = ub
        self.space = space
        self.constraints_list = constraints_list
        self.args = args
        self.emulator_dir = emulator_dir
        self.strategy = strategy
        self.emulator_ratio = emulator_ratio
        self.update_interval = update_interval
        
        # Parameter names
        self.param_names = [name for name in space['name']]
        
        # Load emulators
        self.emulators = {}
        self._load_emulators()
        
        # Data collection for updating emulators
        self.new_data = {
            'parameters': [],
            'outputs': {},
            'constraints': []
        }
        
        # Create output directory for results
        os.makedirs(os.path.join(emulator_dir, 'hybrid_pso'), exist_ok=True)
        
        logger.info(f"Initialized HybridPSO with strategy: {strategy}")
        logger.info(f"Loaded {len(self.emulators)} emulators")
        logger.info(f"Parameter bounds: {list(zip(self.param_names, lb, ub))}")
    
    def _load_emulators(self):
        """Load all available emulators from the emulator directory."""
        # Get all constraint types from the constraint list
        constraint_types = [c.__class__.__name__ for c in self.constraints_list]
        
        for constraint_type in constraint_types:
            try:
                emulator = SAGEEmulator(constraint_type, output_dir=self.emulator_dir)
                emulator.load()
                self.emulators[constraint_type] = emulator
                logger.info(f"Loaded emulator for {constraint_type}")
            except Exception as e:
                logger.warning(f"Could not load emulator for {constraint_type}: {str(e)}")
    
    def _emulator_evaluate(self, parameters):
        """
        Evaluate parameter sets using emulators.
        
        Parameters:
        -----------
        parameters : ndarray
            Parameter values to evaluate
            
        Returns:
        --------
        fitness : ndarray
            Estimated fitness values for each parameter set
        """
        # Convert parameters to the right shape
        if len(parameters.shape) == 1:
            parameters = parameters.reshape(1, -1)
        
        # Initialize fitness
        fitness = np.zeros(parameters.shape[0])
        
        # Evaluate with each available emulator
        for constraint in self.constraints_list:
            constraint_type = constraint.__class__.__name__
            if constraint_type in self.emulators:
                emulator = self.emulators[constraint_type]
                
                # Get predictions for this constraint
                try:
                    predictions = emulator.predict(parameters)
                    
                    # Calculate fitness contribution (use chi-squared-like metric)
                    for i in range(parameters.shape[0]):
                        # Find valid predictions (non-NaN)
                        valid_idx = ~np.isnan(predictions[i])
                        if np.sum(valid_idx) > 0:
                            # Use mean squared difference as proxy for fitness
                            contribution = np.mean(predictions[i, valid_idx]**2)
                            # Weight by the constraint's weight
                            fitness[i] += contribution * constraint.weight
                except Exception as e:
                    logger.warning(f"Error predicting with emulator for {constraint_type}: {str(e)}")
        
        return fitness
    
    def _full_evaluate(self, parameters):
        """
        Evaluate parameter sets using the full SAGE model.
        
        Parameters:
        -----------
        parameters : ndarray
            Parameter values to evaluate
            
        Returns:
        --------
        fitness : ndarray
            Actual fitness values for each parameter set
        outputs : dict
            Model outputs for each constraint
        """
        # Convert parameters to the right shape
        if len(parameters.shape) == 1:
            parameters = parameters.reshape(1, -1)
        
        fitness = np.zeros(parameters.shape[0])
        outputs = {c.__class__.__name__: [] for c in self.constraints_list}
        
        # Evaluate each parameter set
        for i in range(parameters.shape[0]):
            # Call the SAGE evaluation function
            fitness[i] = self.func(parameters[i], *self.args)
            
            # Store parameter set for updating emulators
            self.new_data['parameters'].append(parameters[i])
            self.new_data['constraints'].append([c.__class__.__name__ for c in self.constraints_list])
            
            # We would ideally also store the actual model outputs for each constraint
            # but this depends on how the SAGE evaluation function is structured
            # This is a placeholder for where you would add that code
        
        return fitness, outputs
    
    def _update_emulators(self):
        """Update emulators with new data collected during PSO."""
        logger.info("Updating emulators with new data...")
        
        # Check if we have enough new data
        if len(self.new_data['parameters']) < 10:
            logger.info("Not enough new data to update emulators")
            return
        
        # For each constraint with an emulator, retrain with new data
        for constraint_type, emulator in self.emulators.items():
            try:
                # This is a simplified version - in a real implementation,
                # you would need to combine the new data with the existing training data
                # and retrain the emulator
                logger.info(f"Updating emulator for {constraint_type}...")
                
                # In practice, you would:
                # 1. Combine new_data with original training data
                # 2. Retrain the emulator or update its model incrementally
                # 3. Save the updated emulator
                
                # For now, we'll just log that it would happen
                logger.info(f"Would update emulator for {constraint_type} with {len(self.new_data['parameters'])} new data points")
            except Exception as e:
                logger.error(f"Error updating emulator for {constraint_type}: {str(e)}")
        
        # Clear the new data after updating
        self.new_data = {
            'parameters': [],
            'outputs': {},
            'constraints': []
        }
    
    def pso(self, swarmsize=None, maxiter=100, processes=1, dumpfile_prefix=None, csv_output_path=None):
        """
        Run PSO optimization using the hybrid approach.
        
        Parameters:
        -----------
        swarmsize : int, optional
            Number of particles in the swarm
        maxiter : int
            Maximum number of iterations
        processes : int
            Number of processes to use (for SAGE evaluations)
        dumpfile_prefix : str
            Prefix for dump files
        csv_output_path : str
            Path to save results as CSV
            
        Returns:
        --------
        g : array
            Best position found
        f : float
            Fitness at the best position
        """
        # Set default swarm size if not provided
        if swarmsize is None:
            swarmsize = 10 + int(2 * np.sqrt(len(self.lb)))
        
        # PSO parameters
        omega = 0.5
        phip = 0.7
        phig = 0.3
        
        # Initialize the swarm
        x = np.random.rand(swarmsize, len(self.lb))
        x = self.lb + x * (self.ub - self.lb)
        
        # Initialize velocities
        v = np.zeros_like(x)
        vhigh = np.abs(self.ub - self.lb)
        vlow = -vhigh
        v = vlow + np.random.rand(swarmsize, len(self.lb)) * (vhigh - vlow)
        
        # Initialize personal and global best
        p = np.zeros_like(x)
        fx = np.ones(swarmsize) * np.inf
        fp = np.ones(swarmsize) * np.inf
        g = np.zeros(len(self.lb))
        fg = np.inf
        
        # For storing results
        fitness_history = np.zeros(maxiter)
        position_history = np.zeros((maxiter, len(self.lb)))
        emulator_usage = np.zeros(maxiter)
        time_per_iteration = np.zeros(maxiter)
        
        # Main PSO loop
        for it in range(maxiter):
            start_time = time.time()
            logger.info(f"Iteration {it+1}/{maxiter}")
            
            # Apply the selected hybrid strategy
            if self.strategy == 'prescreening':
                # Pre-screening: Evaluate all particles with emulators first
                emulator_fx = self._emulator_evaluate(x)
                
                # Select the most promising particles for full evaluation
                num_full_eval = max(1, int((1 - self.emulator_ratio) * swarmsize))
                best_idx = np.argsort(emulator_fx)[:num_full_eval]
                
                # Initialize fx with infinity
                fx = np.ones(swarmsize) * np.inf
                
                # Evaluate the selected particles with the full model
                for idx in best_idx:
                    fx[idx], _ = self._full_evaluate(x[idx])
                
                # Use emulator estimates for the rest
                for idx in range(swarmsize):
                    if idx not in best_idx:
                        fx[idx] = emulator_fx[idx]
                
                emulator_usage[it] = 1 - (num_full_eval / swarmsize)
                
            elif self.strategy == 'alternating':
                # Alternating: Switch between emulator and full model
                if it % 2 == 0:
                    # Even iterations: Use full model
                    fx, _ = self._full_evaluate(x)
                    emulator_usage[it] = 0
                else:
                    # Odd iterations: Use emulators
                    fx = self._emulator_evaluate(x)
                    emulator_usage[it] = 1
                    
            elif self.strategy == 'dynamic':
                # Dynamic: Use emulators more at the beginning
                # Gradually shift to full model evaluations
                progress = it / maxiter
                emulator_prob = max(0, 1 - 2 * progress)  # Linear decrease
                
                # Initialize fx
                fx = np.zeros(swarmsize)
                emulator_count = 0
                
                # For each particle, decide which evaluation method to use
                for i in range(swarmsize):
                    if np.random.rand() < emulator_prob:
                        # Use emulator
                        fx[i] = self._emulator_evaluate(x[i].reshape(1, -1))[0]
                        emulator_count += 1
                    else:
                        # Use full model
                        fx[i], _ = self._full_evaluate(x[i].reshape(1, -1))
                
                emulator_usage[it] = emulator_count / swarmsize
            
            else:
                # Default: Use full model for all evaluations
                fx, _ = self._full_evaluate(x)
                emulator_usage[it] = 0
            
            # Update personal best
            improved = fx < fp
            p[improved] = x[improved].copy()
            fp[improved] = fx[improved].copy()
            
            # Update global best
            if np.min(fp) < fg:
                ig = np.argmin(fp)
                fg = fp[ig].copy()
                g = p[ig].copy()
            
            # Save current best
            fitness_history[it] = fg
            position_history[it] = g
            
            # Dump files if requested
            if dumpfile_prefix:
                np.save(f"{dumpfile_prefix}_{it:03d}_pos", x)
                np.save(f"{dumpfile_prefix}_{it:03d}_fx", fx)
            
            # Print progress
            logger.info(f"Iteration {it+1}: Best fitness = {fg}")
            logger.info(f"Emulator usage: {emulator_usage[it]*100:.1f}%")
            
            # Check if we should update emulators
            if it > 0 and it % self.update_interval == 0:
                self._update_emulators()
            
            # Update velocities and positions
            rp = np.random.uniform(size=(swarmsize, len(self.lb)))
            rg = np.random.uniform(size=(swarmsize, len(self.lb)))
            
            v = (omega * v + 
                phip * rp * (p - x) + 
                phig * rg * (g - x))
            
            x = x + v
            
            # Enforce bounds
            x = np.maximum(x, self.lb)
            x = np.minimum(x, self.ub)
            
            # Record time for this iteration
            time_per_iteration[it] = time.time() - start_time
            logger.info(f"Iteration time: {time_per_iteration[it]:.2f} seconds")
        
        # Save results
        result_data = {
            'best_position': g,
            'best_fitness': fg,
            'fitness_history': fitness_history,
            'position_history': position_history,
            'emulator_usage': emulator_usage,
            'time_per_iteration': time_per_iteration,
            'param_names': self.param_names
        }
        
        results_dir = os.path.join(self.emulator_dir, 'hybrid_pso')
        with open(os.path.join(results_dir, 'pso_results.pkl'), 'wb') as f:
            pickle.dump(result_data, f)
        
        # Also save as CSV for easy viewing
        results_df = pd.DataFrame({
            'Parameter': self.param_names,
            'Best Value': g,
            'Lower Bound': self.lb,
            'Upper Bound': self.ub
        })
        results_df.to_csv(os.path.join(results_dir, 'best_parameters.csv'), index=False)
        
        # Save history data
        history_df = pd.DataFrame({
            'Iteration': np.arange(1, maxiter+1),
            'Fitness': fitness_history,
            'Emulator Usage': emulator_usage,
            'Time (seconds)': time_per_iteration
        })
        history_df.to_csv(os.path.join(results_dir, 'pso_history.csv'), index=False)
        
        # Create summary plots
        self._create_summary_plots(results_dir, result_data)
        
        # If requested, save CSV output in the format expected by the original PSO code
        if csv_output_path:
            with open(csv_output_path, 'w') as f:
                # Write best parameters as second-to-last row
                f.write('\t'.join(map(str, g)) + '\n')
                # Write best fitness as last row
                f.write(str(fg) + '\n')
        
        return g, fg
    
    def _create_summary_plots(self, results_dir, result_data):
        """Create summary plots for the PSO run."""
        # 1. Fitness history
        plt.figure(figsize=(10, 6))
        plt.plot(np.arange(1, len(result_data['fitness_history'])+1), result_data['fitness_history'])
        plt.xlabel('Iteration')
        plt.ylabel('Fitness')
        plt.title('Hybrid PSO Fitness History')
        plt.grid(True, alpha=0.3)
        plt.savefig(os.path.join(results_dir, 'fitness_history.png'), dpi=300, bbox_inches='tight')
        plt.close()
        
        # 2. Emulator usage
        plt.figure(figsize=(10, 6))
        plt.plot(np.arange(1, len(result_data['emulator_usage'])+1), result_data['emulator_usage'] * 100)
        plt.xlabel('Iteration')
        plt.ylabel('Emulator Usage (%)')
        plt.title(f'Emulator Usage - {self.strategy} strategy')
        plt.grid(True, alpha=0.3)
        plt.savefig(os.path.join(results_dir, 'emulator_usage.png'), dpi=300, bbox_inches='tight')
        plt.close()
        
        # 3. Time per iteration
        plt.figure(figsize=(10, 6))
        plt.plot(np.arange(1, len(result_data['time_per_iteration'])+1), result_data['time_per_iteration'])
        plt.xlabel('Iteration')
        plt.ylabel('Time (seconds)')
        plt.title('Time per Iteration')
        plt.grid(True, alpha=0.3)
        plt.savefig(os.path.join(results_dir, 'iteration_time.png'), dpi=300, bbox_inches='tight')
        plt.close()
        
        # 4. Parameter evolution
        param_names = result_data['param_names']
        position_history = result_data['position_history']
        
        fig, axes = plt.subplots(len(param_names), 1, figsize=(10, 3*len(param_names)))
        if len(param_names) == 1:
            axes = [axes]
            
        for i, name in enumerate(param_names):
            axes[i].plot(np.arange(1, position_history.shape[0]+1), position_history[:, i])
            axes[i].set_xlabel('Iteration')
            axes[i].set_ylabel(name)
            axes[i].set_title(f'Evolution of {name}')
            axes[i].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(os.path.join(results_dir, 'parameter_evolution.png'), dpi=300, bbox_inches='tight')
        plt.close()


def run_hybrid_pso(config_path, constraints_list, space, subvols, stat_test, 
                  sage_func, emulator_dir, strategy='prescreening', 
                  maxiter=50, swarmsize=None, processes=1):
    """
    Main function to run hybrid PSO optimization.
    
    Parameters:
    -----------
    config_path : str
        Path to SAGE configuration file
    constraints_list : list
        List of constraint objects
    space : array
        Search space specification
    subvols : list
        Subvolumes to process
    stat_test : function
        Statistical test function
    sage_func : function
        Function to run SAGE
    emulator_dir : str
        Directory containing trained emulators
    strategy : str
        Integration strategy ('prescreening', 'alternating', 'dynamic')
    maxiter : int
        Maximum number of iterations
    swarmsize : int, optional
        Number of particles in the swarm
    processes : int
        Number of processes to use
        
    Returns:
    --------
    best_position : array
        Best position found
    best_fitness : float
        Fitness at the best position
    """

    # Create a simple Opts class to mimic the structure expected by run_sage
    class Opts:
        def __init__(self, config_path, outdir, constraints_list):
            self.config = config_path
            self.outdir = outdir
            # Find the SAGE binary path based on the config path
            sage_dir = os.path.dirname(os.path.dirname(config_path))
            self.sage_binary = os.path.join(sage_dir, "sage")
            # Add constraints list
            self.constraints = constraints_list
            # Other required attributes
            self.keep = False
    
    # Create opts object
    opts = Opts(config_path, emulator_dir, constraints_list)
    
    # Ensure the SAGE binary exists
    if not os.path.exists(opts.sage_binary):
        # Try to find it in other common locations
        possible_paths = [
            os.path.join(os.path.dirname(config_path), "sage"),
            "/Users/mbradley/Documents/PhD/SAGE-PSO/sage-model/sage",
            "../sage",
            "./sage"
        ]
        for path in possible_paths:
            if os.path.exists(path):
                opts.sage_binary = path
                break
                
    # Check if sage_binary was found
    if not os.path.exists(opts.sage_binary):
        raise FileNotFoundError(f"Could not find SAGE binary. Please specify its path manually.")
    
    print(f"Using SAGE binary: {opts.sage_binary}")
    
    # Create args tuple for SAGE function
    args = (opts, space, subvols, stat_test)

    # Set up hybrid PSO
    hybrid_pso = HybridPSO(
        func=sage_func,
        lb=space['lb'],
        ub=space['ub'],
        space=space,
        constraints_list=constraints_list,
        args=args,
        emulator_dir=emulator_dir,
        strategy=strategy
    )
    
    # Create dump file directory
    dump_dir = os.path.join(emulator_dir, 'hybrid_pso', 'tracks')
    os.makedirs(dump_dir, exist_ok=True)
    
    # Run PSO
    start_time = time.time()
    best_position, best_fitness = hybrid_pso.pso(
        swarmsize=swarmsize,
        maxiter=maxiter,
        processes=processes,
        dumpfile_prefix=os.path.join(dump_dir, 'track'),
        csv_output_path=os.path.join(emulator_dir, 'hybrid_pso', 'final_result.csv')
    )
    elapsed_time = time.time() - start_time
    
    logger.info(f"Hybrid PSO completed in {elapsed_time:.2f} seconds")
    logger.info(f"Best position: {best_position}")
    logger.info(f"Best fitness: {best_fitness}")
    
    return best_position, best_fitness


def get_required_snapshots(constraints_str):
    """Get all unique snapshots needed for constraints"""
    # Map of constraint classes to their snapshots 
    snapshot_map = {
        'SMF_z0': [63],
        'SMF_z02': [43],
        'SMF_z05': [38],
        'SMF_z08': [34],
        'SMF_z10': [40], 
        'SMF_z11': [38],
        'SMF_z15': [27],
        'SMF_z20': [32],
        'SMF_z24': [20],
        'SMF_z31': [16],
        'SMF_z36': [14],
        'SMF_z46': [11],
        'SMF_z57': [9],
        'SMF_z63': [8],
        'SMF_z77': [6],
        'SMF_z85': [5],
        'SMF_z104': [3],
        'BHMF_z0': [63],
        'BHMF_z20': [32],
        'BHBM_z0': [63],
        'BHBM_z20': [32],
        'HSMR_z0': [63]
    }
    
    snapshots = set()
    print(f"Parsing constraints string: {constraints_str}")
    for constraint in constraints_str.split(','):
        # Remove any weight/domain specifications
        base_constraint = constraint.split('(')[0].split('*')[0]
        print(f"Processing constraint: {constraint}")
        print(f"Base constraint: {base_constraint}")
        if base_constraint in snapshot_map:
            print(f"Found snapshot mapping: {snapshot_map[base_constraint]}")
            snapshots.update(snapshot_map[base_constraint])
        else:
            print(f"Warning: No snapshot mapping found for {base_constraint}")
    
    result = sorted(list(snapshots))
    print(f"Final snapshots list: {result}")
    return result


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='Run hybrid PSO with emulators')
    parser.add_argument('-c', '--config', required=True,
                        help='Path to SAGE configuration file')
    parser.add_argument('-S', '--space-file', default='space.txt',
                        help='File with search space specification')
    parser.add_argument('-v', '--subvolumes', default='0',
                        help='Comma-separated list of subvolumes to process')
    parser.add_argument('-e', '--emulator-dir', default='./emulators',
                        help='Directory containing trained emulators')
    parser.add_argument('-s', '--strategy', default='prescreening',
                        choices=['prescreening', 'alternating', 'dynamic'],
                        help='Hybrid PSO strategy to use')
    parser.add_argument('-m', '--max-iterations', type=int, default=50,
                        help='Maximum number of iterations')
    parser.add_argument('-p', '--processes', type=int, default=1,
                        help='Number of processes to use')
    parser.add_argument('-x', '--constraints', default='SMF_z0,BHBM_z0,BHMF_z0',
                        help='Comma-separated list of constraints')
    parser.add_argument('-t', '--stat-test', default='student-t',
                        choices=['student-t', 'chi2'],
                        help='Statistical test to use')
    parser.add_argument('-o', '--output-dir', default='./emulator_output',
                        help='Directory to save outputs')
                        
    # Add required parameters for constraints
    parser.add_argument('--snapshot', nargs='+',
                       help='Comma-separated list of snapshot numbers to analyze', 
                       type=lambda x: [int(i) for i in x.split(',')], default=None)
    parser.add_argument('--sim', type=int, default=0,
                       help='Simulation to use (0=miniUchuu, 1=miniMillennium, 2=MTNG)')
    parser.add_argument('--boxsize', type=float, default=62.5,
                       help='Size of the simulation box in Mpc/h')
    parser.add_argument('--vol-frac', type=float, default=1.0,
                       help='Volume fraction of the simulation box')
    parser.add_argument('--age-alist-file', 
                       help='Path to the age list file, match with .par file',
                       default=None)
    parser.add_argument('--Omega0', type=float, default=0.25,
                       help='Omega0 value for the simulation')
    parser.add_argument('--h0', type=float, default=0.73,
                       help='H0 value for the simulation')
    
    args = parser.parse_args()
    
    # Process subvolumes
    subvols = common.parse_subvolumes(args.subvolumes)
    
    # Load search space
    space = analysis.load_space(args.space_file)
    
    # Get statistical test function
    stat_test = analysis.stat_tests[args.stat_test]
    
    # Determine snapshots based on constraints if not provided
    if args.snapshot is None:
        args.snapshot = get_required_snapshots(args.constraints)
        
    # Parse constraints with all necessary parameters
    constraints_list = constraints.parse(
        args.constraints, 
        snapshot=args.snapshot,
        sim=args.sim,
        boxsize=args.boxsize,
        vol_frac=args.vol_frac,
        age_alist_file=args.age_alist_file,
        Omega0=args.Omega0, 
        h0=args.h0,
        output_dir=args.output_dir
    )
    
    # Import SAGE function
    from execution import run_sage
    
    # Run hybrid PSO
    best_position, best_fitness = run_hybrid_pso(
        config_path=args.config,
        constraints_list=constraints_list,
        space=space,
        subvols=subvols,
        stat_test=stat_test,
        sage_func=run_sage,
        emulator_dir=args.emulator_dir,
        strategy=args.strategy,
        maxiter=args.max_iterations,
        processes=args.processes
    )
    
    print("\nFinal Results:")
    print("--------------")
    for i, param in enumerate(space['name']):
        print(f"{param}: {best_position[i]}")
    print(f"Fitness: {best_fitness}")