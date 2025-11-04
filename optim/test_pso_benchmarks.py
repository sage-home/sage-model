#!/usr/bin/env python
"""
Test PSO implementation with standard benchmark functions.

This module provides well-known optimization benchmark functions with known
global minima to validate the PSO implementation works correctly.

All test functions have their global minimum at f(x*) = 0 at x* = [0, 0, ...]
except Rosenbrock which has its minimum at f(1, 1) = 0.

Usage:
    python test_pso_benchmarks.py --test all
    python test_pso_benchmarks.py --test sphere
    python test_pso_benchmarks.py --test rosenbrock
"""

import numpy as np
import argparse
import logging
import sys
import os
import time

# Import the PSO module
import pso as pso_module

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('pso_test')


# ==============================================================================
# BENCHMARK FUNCTIONS
# ==============================================================================

def sphere_function(x):
    """
    Sphere function (unimodal, convex)
    
    Global minimum: f(0, 0, ..., 0) = 0
    Search domain: typically [-5.12, 5.12]^n
    
    This is the simplest test function. If PSO can't solve this,
    there's a fundamental problem.
    """
    return np.sum(x**2)


def rosenbrock_function(x):
    """
    Rosenbrock function (unimodal, valley-shaped)
    
    Global minimum: f(1, 1, ..., 1) = 0
    Search domain: typically [-5, 10]^n
    
    This function has a curved valley. The global minimum is inside
    a long, narrow, parabolic-shaped flat valley.
    """
    return np.sum(100.0 * (x[1:] - x[:-1]**2)**2 + (1 - x[:-1])**2)


def rastrigin_function(x):
    """
    Rastrigin function (multimodal, highly oscillatory)
    
    Global minimum: f(0, 0, ..., 0) = 0
    Search domain: typically [-5.12, 5.12]^n
    
    This function has many local minima. Tests PSO's ability to
    escape local optima and find the global minimum.
    """
    n = len(x)
    A = 10
    return A * n + np.sum(x**2 - A * np.cos(2 * np.pi * x))


def ackley_function(x):
    """
    Ackley function (multimodal, nearly flat outer region)
    
    Global minimum: f(0, 0, ..., 0) = 0
    Search domain: typically [-32.768, 32.768]^n
    
    This function has a nearly flat outer region and a large hole
    at the center. Tests exploration capabilities.
    """
    n = len(x)
    sum_sq = np.sum(x**2)
    sum_cos = np.sum(np.cos(2 * np.pi * x))
    
    term1 = -20 * np.exp(-0.2 * np.sqrt(sum_sq / n))
    term2 = -np.exp(sum_cos / n)
    
    return term1 + term2 + 20 + np.e


def beale_function(x):
    """
    Beale function (2D only, unimodal)
    
    Global minimum: f(3, 0.5) = 0
    Search domain: typically [-4.5, 4.5]^2
    
    Simple 2D function for quick testing.
    """
    assert len(x) == 2, "Beale function is only defined for 2D"
    x1, x2 = x[0], x[1]
    
    term1 = (1.5 - x1 + x1*x2)**2
    term2 = (2.25 - x1 + x1*x2**2)**2
    term3 = (2.625 - x1 + x1*x2**3)**2
    
    return term1 + term2 + term3


def shifted_sphere_function(x):
    """
    Shifted sphere function (offset minimum)
    
    Global minimum: f(2, 2, ..., 2) = 0
    Search domain: typically [-5.12, 5.12]^n
    
    Tests if PSO can find minimum not at origin.
    """
    offset = np.full_like(x, 2.0)
    return np.sum((x - offset)**2)


# ==============================================================================
# TEST CONFIGURATION
# ==============================================================================

BENCHMARK_TESTS = {
    'sphere': {
        'function': sphere_function,
        'dimensions': 2,
        'bounds': ([-5.12, -5.12], [5.12, 5.12]),
        'expected_min': 0.0,
        'expected_pos': [0.0, 0.0],
        'tolerance': 1e-2,
        'description': 'Simple convex function (easiest test)'
    },
    'sphere_5d': {
        'function': sphere_function,
        'dimensions': 5,
        'bounds': ([-5.12]*5, [5.12]*5),
        'expected_min': 0.0,
        'expected_pos': [0.0]*5,
        'tolerance': 1e-2,
        'description': 'Simple convex function in 5D'
    },
    'shifted_sphere': {
        'function': shifted_sphere_function,
        'dimensions': 2,
        'bounds': ([-5.12, -5.12], [5.12, 5.12]),
        'expected_min': 0.0,
        'expected_pos': [2.0, 2.0],
        'tolerance': 1e-2,
        'description': 'Sphere with offset minimum'
    },
    'rosenbrock': {
        'function': rosenbrock_function,
        'dimensions': 2,
        'bounds': ([-5.0, -5.0], [10.0, 10.0]),
        'expected_min': 0.0,
        'expected_pos': [1.0, 1.0],
        'tolerance': 0.1,  # More lenient for Rosenbrock
        'description': 'Curved valley function (moderate difficulty)'
    },
    'beale': {
        'function': beale_function,
        'dimensions': 2,
        'bounds': ([-4.5, -4.5], [4.5, 4.5]),
        'expected_min': 0.0,
        'expected_pos': [3.0, 0.5],
        'tolerance': 1e-2,
        'description': 'Simple 2D test function'
    },
    'rastrigin': {
        'function': rastrigin_function,
        'dimensions': 2,
        'bounds': ([-5.12, -5.12], [5.12, 5.12]),
        'expected_min': 0.0,
        'expected_pos': [0.0, 0.0],
        'tolerance': 1.0,  # Harder function, more lenient
        'description': 'Many local minima (challenging test)'
    },
    'ackley': {
        'function': ackley_function,
        'dimensions': 2,
        'bounds': ([-32.768, -32.768], [32.768, 32.768]),
        'expected_min': 0.0,
        'expected_pos': [0.0, 0.0],
        'tolerance': 0.5,  # Harder function
        'description': 'Nearly flat with central hole (challenging test)'
    }
}


# ==============================================================================
# TEST EXECUTION
# ==============================================================================

def run_single_test(test_name, config, pso_params=None):
    """
    Run a single benchmark test.
    
    Parameters:
    -----------
    test_name : str
        Name of the test
    config : dict
        Test configuration
    pso_params : dict, optional
        PSO parameters to override defaults
    
    Returns:
    --------
    dict : Test results including success status
    """
    logger.info(f"\n{'='*70}")
    logger.info(f"Running test: {test_name}")
    logger.info(f"Description: {config['description']}")
    logger.info(f"Dimensions: {config['dimensions']}")
    logger.info(f"Expected minimum: {config['expected_min']} at {config['expected_pos']}")
    logger.info(f"{'='*70}")
    
    # Default PSO parameters (standard values from Clerc & Kennedy 2002)
    default_params = {
        'swarmsize': 30,
        'omega': 0.729,      # Standard constriction coefficient
        'phip': 1.49445,     # Standard cognitive parameter
        'phig': 1.49445,     # Standard social parameter
        'maxiter': 100,
        'minstep': 1e-8,
        'minfunc': 1e-8,
        'debug': False,
        'processes': 1,
        'random_seed': 42  # For reproducibility
    }
    
    # Override with provided parameters
    if pso_params:
        default_params.update(pso_params)
    
    # Run PSO
    start_time = time.time()
    
    try:
        result = pso_module.pso(
            config['function'],
            config['bounds'][0],
            config['bounds'][1],
            **default_params
        )
        
        best_pos = result[0]
        best_fitness = result[1]
        
        elapsed_time = time.time() - start_time
        
        # Check if result is within tolerance
        fitness_error = abs(best_fitness - config['expected_min'])
        position_error = np.linalg.norm(np.array(best_pos) - np.array(config['expected_pos']))
        
        success = fitness_error < config['tolerance']
        
        # Log results
        logger.info(f"\nResults:")
        logger.info(f"  Best fitness found: {best_fitness:.6f}")
        logger.info(f"  Expected fitness:   {config['expected_min']:.6f}")
        logger.info(f"  Fitness error:      {fitness_error:.6f}")
        logger.info(f"  Best position:      {best_pos}")
        logger.info(f"  Expected position:  {config['expected_pos']}")
        logger.info(f"  Position error:     {position_error:.6f}")
        logger.info(f"  Time elapsed:       {elapsed_time:.2f}s")
        logger.info(f"  Status:             {'✓ PASS' if success else '✗ FAIL'}")
        
        return {
            'test_name': test_name,
            'success': success,
            'best_fitness': best_fitness,
            'expected_fitness': config['expected_min'],
            'fitness_error': fitness_error,
            'best_position': best_pos,
            'expected_position': config['expected_pos'],
            'position_error': position_error,
            'tolerance': config['tolerance'],
            'elapsed_time': elapsed_time
        }
        
    except Exception as e:
        logger.error(f"Test failed with exception: {e}", exc_info=True)
        return {
            'test_name': test_name,
            'success': False,
            'error': str(e),
            'elapsed_time': time.time() - start_time
        }


def run_all_tests(test_names='all', pso_params=None):
    """
    Run multiple benchmark tests.
    
    Parameters:
    -----------
    test_names : str or list
        'all' to run all tests, or list of test names
    pso_params : dict, optional
        PSO parameters to override defaults
    
    Returns:
    --------
    list : Results from all tests
    """
    if test_names == 'all':
        tests_to_run = list(BENCHMARK_TESTS.keys())
    elif isinstance(test_names, str):
        tests_to_run = [test_names]
    else:
        tests_to_run = test_names
    
    results = []
    
    for test_name in tests_to_run:
        if test_name not in BENCHMARK_TESTS:
            logger.warning(f"Unknown test: {test_name}")
            continue
        
        result = run_single_test(test_name, BENCHMARK_TESTS[test_name], pso_params)
        results.append(result)
    
    # Print summary
    logger.info(f"\n{'='*70}")
    logger.info("SUMMARY")
    logger.info(f"{'='*70}")
    
    passed = sum(1 for r in results if r.get('success', False))
    total = len(results)
    
    for result in results:
        status = '✓ PASS' if result.get('success', False) else '✗ FAIL'
        logger.info(f"{result['test_name']:20s} {status}")
    
    logger.info(f"\nTotal: {passed}/{total} tests passed")
    logger.info(f"{'='*70}\n")
    
    return results


# ==============================================================================
# MAIN
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Test PSO implementation with benchmark functions'
    )
    parser.add_argument(
        '--test',
        default='all',
        help='Test to run: "all", "sphere", "rosenbrock", "rastrigin", "ackley", etc.'
    )
    parser.add_argument(
        '--swarmsize',
        type=int,
        default=30,
        help='Number of particles (default: 30)'
    )
    parser.add_argument(
        '--maxiter',
        type=int,
        default=100,
        help='Maximum iterations (default: 100)'
    )
    parser.add_argument(
        '--omega',
        type=float,
        default=0.729,
        help='Inertia weight (default: 0.729, standard constriction coefficient)'
    )
    parser.add_argument(
        '--phip',
        type=float,
        default=1.49445,
        help='Cognitive parameter (default: 1.49445, standard value)'
    )
    parser.add_argument(
        '--phig',
        type=float,
        default=1.49445,
        help='Social parameter (default: 1.49445, standard value)'
    )
    parser.add_argument(
        '--seed',
        type=int,
        default=42,
        help='Random seed for reproducibility (default: 42)'
    )
    parser.add_argument(
        '--list',
        action='store_true',
        help='List available tests'
    )
    
    args = parser.parse_args()
    
    # List tests if requested
    if args.list:
        logger.info("Available benchmark tests:")
        for name, config in BENCHMARK_TESTS.items():
            logger.info(f"  {name:20s} - {config['description']}")
        return
    
    # PSO parameters from command line
    pso_params = {
        'swarmsize': args.swarmsize,
        'maxiter': args.maxiter,
        'omega': args.omega,
        'phip': args.phip,
        'phig': args.phig,
        'random_seed': args.seed,
        'debug': True
    }
    
    # Run tests
    results = run_all_tests(args.test, pso_params)
    
    # Return exit code based on results
    all_passed = all(r.get('success', False) for r in results)
    sys.exit(0 if all_passed else 1)


if __name__ == '__main__':
    main()
