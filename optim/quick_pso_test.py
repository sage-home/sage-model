#!/usr/bin/env python
"""
Quick PSO sanity check - minimal test to verify PSO works at all.

This runs a simple sphere function minimization that should complete
in seconds and always succeed if PSO is working correctly.

Usage:
    python quick_pso_test.py
"""

import numpy as np
import sys
import pso as pso_module


def simple_quadratic(x):
    """
    Simple 2D quadratic: f(x,y) = x^2 + y^2
    Minimum at (0, 0) with f = 0
    """
    return x[0]**2 + x[1]**2


def main():
    print("="*60)
    print("PSO Quick Sanity Check")
    print("="*60)
    print("Function: f(x,y) = x^2 + y^2")
    print("Expected: minimum at (0, 0) with f = 0")
    print("-"*60)
    
    # Simple PSO parameters
    lb = [-10.0, -10.0]  # Lower bounds
    ub = [10.0, 10.0]    # Upper bounds
    
    # Run PSO with standard parameters
    print("Running PSO with standard parameters...")
    print("  omega=0.729, phip=1.49445, phig=1.49445")
    result = pso_module.pso(
        simple_quadratic,
        lb, ub,
        swarmsize=20,
        maxiter=50,
        omega=0.729,      # Standard constriction coefficient
        phip=1.49445,     # Standard cognitive parameter
        phig=1.49445,     # Standard social parameter
        debug=False,
        random_seed=42
    )
    
    best_pos = result[0]
    best_fitness = result[1]
    
    print(f"\nResults:")
    print(f"  Best position: [{best_pos[0]:.6f}, {best_pos[1]:.6f}]")
    print(f"  Best fitness:  {best_fitness:.6f}")
    print(f"  Expected:      [0.000000, 0.000000] with fitness 0.000000")
    
    # Check success
    error = abs(best_fitness - 0.0)
    tolerance = 0.01
    
    print(f"  Error:         {error:.6f}")
    print(f"  Tolerance:     {tolerance:.6f}")
    
    if error < tolerance:
        print("\n✓ TEST PASSED - PSO is working correctly!")
        print("="*60)
        return 0
    else:
        print("\n✗ TEST FAILED - PSO did not converge to expected minimum")
        print("="*60)
        return 1


if __name__ == '__main__':
    sys.exit(main())
