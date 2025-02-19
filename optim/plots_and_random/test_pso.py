import numpy as np
from pso import pso
import time

def test_pso_reproducibility(n_runs=3):
    """
    Test if PSO gives consistent results across multiple runs
    
    Parameters:
    -----------
    n_runs : int
        Number of times to run the PSO
    """
    
    # Simple test function (sphere function)
    def obj_function(x):
        return np.sum(x**2)
    
    # PSO parameters
    lb = [-5.0] * 4  # 4-dimensional problem
    ub = [5.0] * 4
    swarmsize = 10
    maxiter = 50
    processes = 1  # Use single process for deterministic results
    
    # Store results
    results = []
    times = []
    
    print(f"\nRunning PSO {n_runs} times to test reproducibility...")
    print("\nParameters:")
    print(f"Dimensions: {len(lb)}")
    print(f"Swarm size: {swarmsize}")
    print(f"Max iterations: {maxiter}")
    print(f"Bounds: [{lb[0]}, {ub[0]}]")
    
    for i in range(n_runs):
        start_time = time.time()
        xopt, fopt = pso(obj_function, lb, ub, swarmsize=swarmsize, 
                        maxiter=maxiter, processes=processes)
        end_time = time.time()
        
        results.append((xopt, fopt))
        times.append(end_time - start_time)
        
        print(f"\nRun {i+1}:")
        print(f"Best position: {xopt}")
        print(f"Best fitness: {fopt}")
        print(f"Time taken: {times[-1]:.2f} seconds")
    
    # Check consistency
    print("\nChecking consistency across runs...")
    
    # Compare positions and fitness values
    all_identical = True
    base_xopt, base_fopt = results[0]
    
    for i, (xopt, fopt) in enumerate(results[1:], 1):
        pos_diff = np.max(np.abs(xopt - base_xopt))
        fit_diff = np.abs(fopt - base_fopt)
        
        print(f"\nDifference between run 1 and run {i+1}:")
        print(f"Maximum position difference: {pos_diff}")
        print(f"Fitness difference: {fit_diff}")
        
        if pos_diff > 1e-10 or fit_diff > 1e-10:
            all_identical = False
    
    print("\nSummary:")
    print("All runs identical?" if all_identical else "Runs differ!")
    print(f"Average time per run: {np.mean(times):.2f} seconds")
    
    return all_identical, results

if __name__ == "__main__":
    success, results = test_pso_reproducibility(n_runs=3)
    
    if not success:
        print("\nWARNING: PSO runs produced different results!")
        print("Check the random seed implementation.")