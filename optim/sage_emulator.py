import numpy as np
import pandas as pd
import os
import pickle
import matplotlib.pyplot as plt
from sklearn.ensemble import RandomForestRegressor, GradientBoostingRegressor
from sklearn.gaussian_process import GaussianProcessRegressor
from sklearn.gaussian_process.kernels import RBF, ConstantKernel, Matern
from sklearn.neural_network import MLPRegressor
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import mean_squared_error, r2_score
from sklearn.model_selection import train_test_split
import joblib
import time

class SAGEEmulator:
    """
    Machine learning emulator for SAGE galaxy formation model.
    Predicts model outputs for given parameter sets without running the full model.
    
    This emulator can be trained on data from PSO runs and then used to quickly
    explore parameter space or accelerate PSO itself.
    """
    
    def __init__(self, constraint_type, output_dir='./emulators'):
        """
        Initialize the emulator for a specific constraint type.
        
        Parameters:
        -----------
        constraint_type : str
            Type of constraint to emulate (e.g., 'SMF_z0', 'BHBM_z0')
        output_dir : str
            Directory to save/load trained emulators
        """
        self.constraint_type = constraint_type
        self.output_dir = output_dir
        self.models = {}
        self.x_scaler = None
        self.y_scaler = None
        self.is_trained = False
        self.trained_parameters = []
        self.x_bins = None
        self.param_bounds = None
        
        # Create output directory if it doesn't exist
        os.makedirs(output_dir, exist_ok=True)
        
    def _prepare_data(self, pso_runs_dirs, param_names):
        """
        Extract training data from multiple PSO run outputs.
        
        Parameters:
        -----------
        pso_runs_dirs : list
            List of directories containing PSO run outputs
        param_names : list
            Names of model parameters
            
        Returns:
        --------
        X : ndarray
            Parameter values
        y : ndarray
            Model outputs for each parameter set
        """
        print(f"Preparing training data for {self.constraint_type} from {len(pso_runs_dirs)} directories...")
        
        # Initialize data containers
        all_X_list = []
        all_data_blocks = []
        
        # Process each directory
        for pso_dir in pso_runs_dirs:
            print(f"Processing directory: {pso_dir}")
            
            # 1. Get parameter values from track files
            # Look for track files in the tracks subdirectory
            tracks_dir = os.path.join(pso_dir, 'tracks')
            if not os.path.exists(tracks_dir):
                tracks_dir = pso_dir  # Use the main directory if no tracks subdirectory
                print(f"No 'tracks' subdirectory found, using {tracks_dir} for track files")
                
            track_files = sorted([f for f in os.listdir(tracks_dir) if f.startswith('track_') and f.endswith('_pos.npy')])
            
            if not track_files:
                print(f"Warning: No track files found in {tracks_dir}, skipping this directory")
                continue
                
            X_list = []
            for track_file in track_files:
                pos = np.load(os.path.join(tracks_dir, track_file))
                X_list.append(pos)
            
            all_X_list.extend(X_list)
            
            # 2. Get constraint outputs from dump files
            # Look for dump files directly in the main directory
            dump_file = os.path.join(pso_dir, f"{self.constraint_type}_dump.txt")
            if not os.path.exists(dump_file):
                print(f"Warning: Dump file not found: {dump_file}, skipping this directory")
                continue
            
            data_blocks = []
            current_block = []
            
            with open(dump_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line.startswith("# New Data Block"):
                        if current_block:
                            data_blocks.append(np.array(current_block))
                            current_block = []
                    elif line:
                        values = list(map(float, line.split('\t')))
                        current_block.append(values)
                        
                # Add the last block if it exists
                if current_block:
                    data_blocks.append(np.array(current_block))
            
            all_data_blocks.extend(data_blocks)
        
        # Combine all parameter data
        if not all_X_list:
            raise ValueError("No valid parameter data found in any directory")
        
        X = np.vstack(all_X_list)
        print(f"Collected {X.shape[0]} parameter sets with {X.shape[1]} parameters each")
        
        # Extract y values from the data blocks
        if not all_data_blocks:
            raise ValueError("No valid output data found in any directory")
        
        # Save the x bins from the first data block
        self.x_bins = all_data_blocks[0][:, 0]
        
        # Extract model outputs for each parameter set
        y_list = []
        for block in all_data_blocks:
            if block.shape[0] > 0:
                # Store the model y values (index 2)
                y_list.append(block[:, 2])
        
        # Verify we have enough y values for all parameter sets
        if len(y_list) < X.shape[0]:
            print(f"Warning: Only found {len(y_list)} output blocks for {X.shape[0]} parameter sets")
            # Truncate X to match y
            X = X[:len(y_list)]
        elif len(y_list) > X.shape[0]:
            print(f"Warning: Found {len(y_list)} output blocks but only {X.shape[0]} parameter sets")
            # Truncate y to match X
            y_list = y_list[:X.shape[0]]
        
        # If data blocks have different lengths, pad with NaN
        max_length = max(block.shape[0] for block in all_data_blocks)
        y = np.full((len(y_list), max_length), np.nan)
        
        for i, block in enumerate(y_list):
            if i < len(y):  # Make sure we don't go out of bounds
                y[i, :len(block)] = block
                
        # Store parameter bounds for later validation
        self.param_bounds = {
            'min': np.min(X, axis=0),
            'max': np.max(X, axis=0)
        }
        
        # Store parameter names
        self.trained_parameters = param_names[:X.shape[1]]
        
        print(f"Prepared dataset with shape: X={X.shape}, y={y.shape}")
        return X, y
        
    def train(self, pso_runs_dirs, param_names, method='random_forest', test_size=0.2):
        """
        Train the emulator using data from PSO runs.
        
        Parameters:
        -----------
        pso_runs_dirs : str or list
            Directory or list of directories containing PSO run outputs
        param_names : list
            Names of model parameters
        method : str
            ML method to use ('random_forest', 'gradient_boosting', 'gaussian_process', 'neural_network')
        test_size : float
            Fraction of data to use for testing
            
        Returns:
        --------
        metrics : dict
            Training and testing metrics
        """
        start_time = time.time()
        
        # Convert single directory to list for consistent handling
        if isinstance(pso_runs_dirs, str):
            pso_runs_dirs = [pso_runs_dirs]
        
        X, y = self._prepare_data(pso_runs_dirs, param_names)
        
        # Split the data
        X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=test_size, random_state=42)
        
        # Scale the data
        self.x_scaler = StandardScaler().fit(X_train)
        X_train_scaled = self.x_scaler.transform(X_train)
        X_test_scaled = self.x_scaler.transform(X_test)
        
        # Train individual models for each bin
        metrics = {'train': [], 'test': []}
        
        for bin_idx in range(y.shape[1]):
            print(f"Training model for bin {bin_idx+1}/{y.shape[1]}")
            
            # Extract training data for this bin
            y_train_bin = y_train[:, bin_idx]
            y_test_bin = y_test[:, bin_idx]
            
            # Skip bins with NaN values
            if np.isnan(y_train_bin).any() or np.isnan(y_test_bin).any():
                print(f"  Skipping bin {bin_idx} due to NaN values")
                self.models[bin_idx] = None
                metrics['train'].append(None)
                metrics['test'].append(None)
                continue
            
            # Choose the model based on the method
            if method == 'random_forest':
                model = RandomForestRegressor(n_estimators=100, random_state=42, n_jobs=-1)
            elif method == 'gradient_boosting':
                model = GradientBoostingRegressor(n_estimators=100, random_state=42)
            elif method == 'gaussian_process':
                kernel = ConstantKernel() * Matern(length_scale=1.0, nu=1.5)
                model = GaussianProcessRegressor(kernel=kernel, n_restarts_optimizer=10, random_state=42)
            elif method == 'neural_network':
                model = MLPRegressor(hidden_layer_sizes=(100, 50), max_iter=1000, random_state=42)
            else:
                raise ValueError(f"Unknown method: {method}")
            
            # Train the model
            model.fit(X_train_scaled, y_train_bin)
            self.models[bin_idx] = model
            
            # Evaluate the model
            y_train_pred = model.predict(X_train_scaled)
            y_test_pred = model.predict(X_test_scaled)
            
            train_mse = mean_squared_error(y_train_bin, y_train_pred)
            test_mse = mean_squared_error(y_test_bin, y_test_pred)
            train_r2 = r2_score(y_train_bin, y_train_pred)
            test_r2 = r2_score(y_test_bin, y_test_pred)
            
            metrics['train'].append({'mse': train_mse, 'r2': train_r2})
            metrics['test'].append({'mse': test_mse, 'r2': test_r2})
            
            print(f"  Train MSE: {train_mse:.6f}, R²: {train_r2:.6f}")
            print(f"  Test MSE: {test_mse:.6f}, R²: {test_r2:.6f}")
        
        self.is_trained = True
        training_time = time.time() - start_time
        print(f"Training completed in {training_time:.2f} seconds")
        
        # Save the trained model
        self.save()
        
        return metrics
    
    def predict(self, parameters):
        """
        Predict model outputs for given parameter sets.
        
        Parameters:
        -----------
        parameters : ndarray
            Parameter values to predict outputs for
            
        Returns:
        --------
        predictions : ndarray
            Predicted model outputs
        """
        if not self.is_trained:
            raise ValueError("Emulator is not trained yet")
        
        # Convert to numpy array if it's not already
        if isinstance(parameters, list):
            parameters = np.array(parameters).reshape(1, -1)
        elif len(parameters.shape) == 1:
            parameters = parameters.reshape(1, -1)
        
        # Check input dimensions
        if parameters.shape[1] != len(self.trained_parameters):
            raise ValueError(f"Expected {len(self.trained_parameters)} parameters, got {parameters.shape[1]}")
        
        # Check parameter bounds
        for i, param in enumerate(self.trained_parameters):
            min_val = self.param_bounds['min'][i]
            max_val = self.param_bounds['max'][i]
            for j, val in enumerate(parameters[:, i]):
                if val < min_val or val > max_val:
                    print(f"Warning: Parameter {param} value {val} is outside training range [{min_val}, {max_val}]")
        
        # Scale the parameters
        parameters_scaled = self.x_scaler.transform(parameters)
        
        # Make predictions for each bin
        predictions = np.zeros((parameters.shape[0], len(self.x_bins)))
        
        for bin_idx, model in self.models.items():
            if model is not None:
                predictions[:, bin_idx] = model.predict(parameters_scaled)
            else:
                predictions[:, bin_idx] = np.nan
        
        return predictions
    
    def save(self):
        """Save the trained emulator to disk."""
        if not self.is_trained:
            raise ValueError("Cannot save untrained emulator")
        
        model_path = os.path.join(self.output_dir, f"{self.constraint_type}_emulator.pkl")
        
        # Create a dictionary with all the components to save
        emulator_data = {
            'models': self.models,
            'x_scaler': self.x_scaler,
            'x_bins': self.x_bins,
            'param_bounds': self.param_bounds,
            'trained_parameters': self.trained_parameters,
            'constraint_type': self.constraint_type
        }
        
        # Save using joblib for efficient storage of large numpy arrays
        joblib.dump(emulator_data, model_path)
        print(f"Emulator saved to {model_path}")
    
    def load(self):
        """Load a trained emulator from disk."""
        model_path = os.path.join(self.output_dir, f"{self.constraint_type}_emulator.pkl")
        
        if not os.path.exists(model_path):
            raise FileNotFoundError(f"No saved emulator found at {model_path}")
        
        # Load the emulator data
        emulator_data = joblib.load(model_path)
        
        # Update the emulator attributes
        self.models = emulator_data['models']
        self.x_scaler = emulator_data['x_scaler']
        self.x_bins = emulator_data['x_bins']
        self.param_bounds = emulator_data['param_bounds']
        self.trained_parameters = emulator_data['trained_parameters']
        self.constraint_type = emulator_data['constraint_type']
        self.is_trained = True
        
        print(f"Loaded emulator for {self.constraint_type}")
        return self
    
    def visualize_predictions(self, parameters, fig_size=(12, 8)):
        """
        Visualize predictions for a given parameter set.
        
        Parameters:
        -----------
        parameters : ndarray
            Parameter values to visualize predictions for
        fig_size : tuple
            Figure size (width, height)
            
        Returns:
        --------
        fig : matplotlib figure
            Figure with the visualization
        """
        if not self.is_trained:
            raise ValueError("Emulator is not trained yet")
        
        predictions = self.predict(parameters)
        
        # Create a figure
        fig, ax = plt.subplots(figsize=fig_size)
        
        # Plot the predictions
        for i in range(predictions.shape[0]):
            label = f"Parameter set {i+1}" if predictions.shape[0] > 1 else "Prediction"
            ax.plot(self.x_bins, predictions[i], label=label)
        
        # Add labels and legend
        ax.set_xlabel('x')
        ax.set_ylabel('y')
        ax.set_title(f'Emulator predictions for {self.constraint_type}')
        ax.legend()
        
        return fig
    
    def compare_with_sage(self, parameters, sage_outputs, fig_size=(12, 8)):
        """
        Compare emulator predictions with actual SAGE outputs.
        
        Parameters:
        -----------
        parameters : ndarray
            Parameter values used for prediction
        sage_outputs : ndarray
            Actual SAGE outputs for the same parameter values
        fig_size : tuple
            Figure size (width, height)
            
        Returns:
        --------
        fig : matplotlib figure
            Figure with the comparison
        metrics : dict
            Metrics for the comparison (MSE, R²)
        """
        if not self.is_trained:
            raise ValueError("Emulator is not trained yet")
        
        predictions = self.predict(parameters)
        
        # Create a figure
        fig, ax = plt.subplots(figsize=fig_size)
        
        # Plot the predictions and actual outputs
        ax.plot(self.x_bins, predictions[0], label='Emulator')
        ax.plot(self.x_bins, sage_outputs, label='SAGE')
        
        # Add labels and legend
        ax.set_xlabel('x')
        ax.set_ylabel('y')
        ax.set_title(f'Emulator vs SAGE for {self.constraint_type}')
        ax.legend()
        
        # Calculate metrics
        valid_idx = ~np.isnan(predictions[0]) & ~np.isnan(sage_outputs)
        if np.sum(valid_idx) > 0:
            mse = mean_squared_error(sage_outputs[valid_idx], predictions[0, valid_idx])
            r2 = r2_score(sage_outputs[valid_idx], predictions[0, valid_idx])
            metrics = {'mse': mse, 'r2': r2}
            
            # Add metrics to the plot
            ax.text(0.05, 0.95, f'MSE: {mse:.6f}\nR²: {r2:.6f}', 
                    transform=ax.transAxes, fontsize=12, 
                    verticalalignment='top', bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        else:
            metrics = {'mse': None, 'r2': None}
        
        return fig, metrics


class EmulatorPSO:
    """
    PSO implementation that uses emulators to accelerate parameter optimization.
    
    This class combines emulators for multiple constraints to quickly estimate
    the fitness of parameter sets without running the full SAGE model.
    """
    
    def __init__(self, emulators, weights, space_file, output_dir='./emulator_pso'):
        """
        Initialize the emulator-accelerated PSO.
        
        Parameters:
        -----------
        emulators : dict
            Dictionary of trained emulators keyed by constraint type
        weights : dict
            Weights for each constraint type
        space_file : str
            Path to the space file defining parameter bounds
        output_dir : str
            Directory to save PSO outputs
        """
        self.emulators = emulators
        self.weights = weights
        self.space_file = space_file
        self.output_dir = output_dir
        self.param_names = []
        self.lb = []
        self.ub = []
        
        # Create output directory if it doesn't exist
        os.makedirs(output_dir, exist_ok=True)
        
        # Load the search space
        self._load_space()
        
        # Check that all emulators are trained on the same parameters
        self._check_emulators()
    
    def _load_space(self):
        """Load the search space from the space file."""
        # Read the space file
        with open(self.space_file, 'r') as f:
            lines = f.readlines()
        
        # Parse each line
        for line in lines:
            parts = line.strip().split(',')
            if len(parts) >= 5:
                self.param_names.append(parts[0])
                self.lb.append(float(parts[3]))
                self.ub.append(float(parts[4]))
        
        print(f"Loaded search space with {len(self.param_names)} parameters")
        print(f"Parameters: {self.param_names}")
        print(f"Lower bounds: {self.lb}")
        print(f"Upper bounds: {self.ub}")
    
    def _check_emulators(self):
        """Check that all emulators are trained on the same parameters."""
        # Get the parameters from the first emulator
        first_emulator = list(self.emulators.values())[0]
        first_params = first_emulator.trained_parameters
        
        # Check all other emulators
        for constraint_type, emulator in self.emulators.items():
            if not emulator.is_trained:
                raise ValueError(f"Emulator for {constraint_type} is not trained")
            
            if emulator.trained_parameters != first_params:
                raise ValueError(f"Emulator for {constraint_type} is trained on different parameters")
    
    def evaluate_fitness(self, parameters):
        """
        Evaluate the fitness of parameter sets using emulators.
        
        Parameters:
        -----------
        parameters : ndarray
            Parameter values to evaluate
            
        Returns:
        --------
        fitness : ndarray
            Fitness values for each parameter set
        """
        # Convert to numpy array if it's not already
        if isinstance(parameters, list):
            parameters = np.array(parameters)
        
        # Reshape if needed
        if len(parameters.shape) == 1:
            parameters = parameters.reshape(1, -1)
        
        # Initialize fitness
        fitness = np.zeros(parameters.shape[0])
        
        # Evaluate each constraint
        for constraint_type, emulator in self.emulators.items():
            weight = self.weights.get(constraint_type, 1.0)
            predictions = emulator.predict(parameters)
            
            # Calculate fitness for this constraint (e.g., chi-square)
            # For simplicity, we'll use mean squared error
            for i in range(predictions.shape[0]):
                valid_idx = ~np.isnan(predictions[i])
                if np.sum(valid_idx) > 0:
                    # For now, just use the mean squared error as a simple fitness measure
                    # This could be replaced with a more sophisticated measure
                    fitness[i] += weight * np.mean(predictions[i, valid_idx]**2)
        
        return fitness
    
    def run_pso(self, swarm_size=20, max_iter=50, omega=0.5, phip=0.7, phig=0.3):
        """
        Run PSO optimization using emulators.
        
        Parameters:
        -----------
        swarm_size : int
            Number of particles in the swarm
        max_iter : int
            Maximum number of iterations
        omega : float
            Particle velocity scaling factor
        phip : float
            Scaling factor to search away from particle's best known position
        phig : float
            Scaling factor to search away from swarm's best known position
            
        Returns:
        --------
        best_position : ndarray
            Best position found
        best_fitness : float
            Fitness value at the best position
        """
        # Initialize the swarm
        lb_array = np.array(self.lb)
        ub_array = np.array(self.ub)
        
        # Initialize particle positions randomly within bounds
        positions = np.random.rand(swarm_size, len(self.lb))
        positions = lb_array + positions * (ub_array - lb_array)
        
        # Initialize velocities
        velocities = np.zeros_like(positions)
        
        # Initialize personal best
        personal_best = positions.copy()
        personal_best_fitness = np.ones(swarm_size) * np.inf
        
        # Initialize global best
        global_best = np.zeros(len(self.lb))
        global_best_fitness = np.inf
        
        # Arrays to track progress
        fitness_history = np.zeros(max_iter)
        position_history = np.zeros((max_iter, len(self.lb)))
        
        # Main PSO loop
        for iteration in range(max_iter):
            # Evaluate fitness
            current_fitness = self.evaluate_fitness(positions)
            
            # Update personal best
            update_idx = current_fitness < personal_best_fitness
            personal_best[update_idx] = positions[update_idx].copy()
            personal_best_fitness[update_idx] = current_fitness[update_idx]
            
            # Update global best
            min_idx = np.argmin(personal_best_fitness)
            if personal_best_fitness[min_idx] < global_best_fitness:
                global_best = personal_best[min_idx].copy()
                global_best_fitness = personal_best_fitness[min_idx]
            
            # Save current best
            fitness_history[iteration] = global_best_fitness
            position_history[iteration] = global_best
            
            # Print progress
            print(f"Iteration {iteration+1}/{max_iter}: Best fitness = {global_best_fitness:.6f}")
            print(f"Best position: {global_best}")
            
            # Update velocities and positions
            rp = np.random.rand(swarm_size, len(self.lb))
            rg = np.random.rand(swarm_size, len(self.lb))
            
            velocities = (omega * velocities + 
                         phip * rp * (personal_best - positions) + 
                         phig * rg * (global_best - positions))
            
            positions = positions + velocities
            
            # Enforce bounds
            positions = np.maximum(positions, lb_array)
            positions = np.minimum(positions, ub_array)
        
        # Save results
        results = {
            'best_position': global_best,
            'best_fitness': global_best_fitness,
            'fitness_history': fitness_history,
            'position_history': position_history,
            'param_names': self.param_names
        }
        
        # Save results to disk
        results_path = os.path.join(self.output_dir, 'pso_results.pkl')
        with open(results_path, 'wb') as f:
            pickle.dump(results, f)
        
        # Save results as CSV for easy viewing
        results_df = pd.DataFrame({
            'Parameter': self.param_names,
            'Best Value': global_best,
            'Lower Bound': self.lb,
            'Upper Bound': self.ub
        })
        results_df.to_csv(os.path.join(self.output_dir, 'best_parameters.csv'), index=False)
        
        # Also save fitness history
        fitness_df = pd.DataFrame({
            'Iteration': np.arange(1, max_iter+1),
            'Fitness': fitness_history
        })
        fitness_df.to_csv(os.path.join(self.output_dir, 'fitness_history.csv'), index=False)
        
        return global_best, global_best_fitness
    
    def visualize_results(self, fig_size=(12, 8)):
        """
        Visualize PSO results.
        
        Parameters:
        -----------
        fig_size : tuple
            Figure size (width, height)
            
        Returns:
        --------
        figs : list
            List of matplotlib figures
        """
        # Load results
        results_path = os.path.join(self.output_dir, 'pso_results.pkl')
        with open(results_path, 'rb') as f:
            results = pickle.load(f)
        
        figs = []
        
        # 1. Fitness history
        fig, ax = plt.subplots(figsize=fig_size)
        ax.plot(np.arange(1, len(results['fitness_history'])+1), results['fitness_history'])
        ax.set_xlabel('Iteration')
        ax.set_ylabel('Fitness')
        ax.set_title('PSO Fitness History')
        figs.append(fig)
        
        # 2. Parameter evolution
        fig, axes = plt.subplots(len(self.param_names), 1, figsize=(fig_size[0], fig_size[1]*len(self.param_names)/2))
        
        if len(self.param_names) == 1:
            axes = [axes]
        
        for i, param_name in enumerate(self.param_names):
            axes[i].plot(np.arange(1, len(results['position_history'])+1), results['position_history'][:, i])
            axes[i].set_xlabel('Iteration')
            axes[i].set_ylabel(param_name)
            axes[i].set_title(f'Evolution of {param_name}')
        
        plt.tight_layout()
        figs.append(fig)
        
        # 3. Visualize best parameter set with all emulators
        for constraint_type, emulator in self.emulators.items():
            fig = emulator.visualize_predictions(results['best_position'].reshape(1, -1), fig_size=fig_size)
            ax = fig.axes[0]
            ax.set_title(f'Best prediction for {constraint_type}')
            figs.append(fig)
        
        # Save figures
        for i, fig in enumerate(figs):
            fig.savefig(os.path.join(self.output_dir, f'figure_{i+1}.png'), dpi=300, bbox_inches='tight')
        
        return figs


# Example usage
if __name__ == "__main__":
    # 1. Train emulators for different constraints
    pso_runs_dir = "./sage-model/output/millennium_pso/tracks"
    param_names = ["SfrEfficiency", "FeedbackReheatingEpsilon", "FeedbackEjectionRatio", 
                  "ReIncorporationFactor", "RadioModeEfficiency", "QuasarModeEfficiency", 
                  "BlackHoleGrowthRate"]
    
    # Train emulators for multiple constraints
    constraints = ["SMF_z0", "BHBM_z0", "BHMF_z0"]
    emulators = {}
    
    for constraint in constraints:
        emulator = SAGEEmulator(constraint)
        metrics = emulator.train(pso_runs_dir, param_names, method='random_forest')
        emulators[constraint] = emulator
    
    # 2. Run emulator-based PSO
    weights = {"SMF_z0": 1.0, "BHBM_z0": 0.5, "BHMF_z0": 0.5}
    space_file = "./sage-model/optim/space.txt"
    
    pso = EmulatorPSO(emulators, weights, space_file)
    best_position, best_fitness = pso.run_pso(swarm_size=20, max_iter=50)
    
    print(f"Best position found: {best_position}")
    print(f"Best fitness: {best_fitness}")
    
    # 3. Visualize results
    pso.visualize_results()