#!/usr/bin/env python
"""
Visual PSO test - generates plots to visualize PSO convergence.

This script tests PSO on simple 2D functions and creates visualizations
showing the optimization landscape and particle trajectories.

Requires: matplotlib

Usage:
    python visual_pso_test.py
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib import cm
from mpl_toolkits.mplot3d import Axes3D
import pso as pso_module


def sphere_2d(x):
    """2D sphere function: f(x,y) = x^2 + y^2"""
    return x[0]**2 + x[1]**2


def rosenbrock_2d(x):
    """2D Rosenbrock: f(x,y) = (1-x)^2 + 100(y-x^2)^2"""
    return (1 - x[0])**2 + 100 * (x[1] - x[0]**2)**2


def plot_function_surface(func, bounds, title, ax=None):
    """
    Plot the surface of a 2D function.
    
    Parameters:
    -----------
    func : callable
        2D function to plot
    bounds : tuple
        ((xmin, xmax), (ymin, ymax))
    title : str
        Plot title
    ax : matplotlib axis, optional
        Axis to plot on
    """
    if ax is None:
        fig = plt.figure(figsize=(10, 8))
        ax = fig.add_subplot(111, projection='3d')
    
    # Create grid
    x_range = np.linspace(bounds[0][0], bounds[0][1], 100)
    y_range = np.linspace(bounds[1][0], bounds[1][1], 100)
    X, Y = np.meshgrid(x_range, y_range)
    
    # Evaluate function
    Z = np.zeros_like(X)
    for i in range(X.shape[0]):
        for j in range(X.shape[1]):
            Z[i, j] = func([X[i, j], Y[i, j]])
    
    # Plot surface
    surf = ax.plot_surface(X, Y, Z, cmap=cm.viridis, alpha=0.6)
    ax.set_xlabel('x')
    ax.set_ylabel('y')
    ax.set_zlabel('f(x, y)')
    ax.set_title(title)
    
    return ax


def plot_contour_with_optimum(func, bounds, optimum, result, title, filename):
    """
    Plot contour of function with PSO result.
    
    Parameters:
    -----------
    func : callable
        2D function to plot
    bounds : tuple
        ((xmin, xmax), (ymin, ymax))
    optimum : tuple
        (x_opt, y_opt) - true optimum
    result : tuple
        PSO result (best_pos, best_fitness, ...)
    title : str
        Plot title
    filename : str
        Output filename
    """
    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Create grid
    x_range = np.linspace(bounds[0][0], bounds[0][1], 200)
    y_range = np.linspace(bounds[1][0], bounds[1][1], 200)
    X, Y = np.meshgrid(x_range, y_range)
    
    # Evaluate function
    Z = np.zeros_like(X)
    for i in range(X.shape[0]):
        for j in range(X.shape[1]):
            Z[i, j] = func([X[i, j], Y[i, j]])
    
    # Plot contour
    levels = np.logspace(np.log10(Z.min() + 1e-6), np.log10(Z.max()), 30)
    contour = ax.contour(X, Y, Z, levels=levels, cmap='viridis', alpha=0.6)
    ax.clabel(contour, inline=True, fontsize=8)
    
    # Plot true optimum
    ax.plot(optimum[0], optimum[1], 'r*', markersize=20, label='True optimum', zorder=5)
    
    # Plot PSO result
    best_pos = result[0]
    ax.plot(best_pos[0], best_pos[1], 'go', markersize=15, label='PSO result', zorder=5)
    
    # Add arrow from PSO result to true optimum
    ax.annotate('', xy=optimum, xytext=best_pos,
                arrowprops=dict(arrowstyle='->', color='red', lw=2, alpha=0.5))
    
    ax.set_xlabel('x')
    ax.set_ylabel('y')
    ax.set_title(title)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(filename, dpi=150)
    print(f"Saved plot: {filename}")
    plt.close()


def test_sphere():
    """Test PSO on sphere function."""
    print("\n" + "="*60)
    print("Testing: Sphere Function")
    print("="*60)
    
    bounds = ([-5, 5], [-5, 5])
    optimum = (0, 0)
    
    result = pso_module.pso(
        sphere_2d,
        [-5, -5], [5, 5],
        swarmsize=30,
        maxiter=50,
        omega=0.729,
        phip=1.49445,
        phig=1.49445,
        debug=True,
        random_seed=42
    )
    
    best_pos = result[0]
    best_fitness = result[1]
    
    print(f"\nTrue optimum:  f({optimum[0]}, {optimum[1]}) = 0")
    print(f"PSO result:    f({best_pos[0]:.6f}, {best_pos[1]:.6f}) = {best_fitness:.6f}")
    print(f"Error:         {abs(best_fitness):.6f}")
    
    # Create visualization
    plot_contour_with_optimum(
        sphere_2d, bounds, optimum, result,
        'Sphere Function: PSO Optimization',
        'sphere_pso_test.png'
    )
    
    return abs(best_fitness) < 0.01


def test_rosenbrock():
    """Test PSO on Rosenbrock function."""
    print("\n" + "="*60)
    print("Testing: Rosenbrock Function")
    print("="*60)
    
    bounds = ([-2, 2], [-1, 3])
    optimum = (1, 1)
    
    result = pso_module.pso(
        rosenbrock_2d,
        [-2, -1], [2, 3],
        swarmsize=50,
        maxiter=200,
        omega=0.729,
        phip=1.49445,
        phig=1.49445,
        debug=True,
        random_seed=42
    )
    
    best_pos = result[0]
    best_fitness = result[1]
    
    print(f"\nTrue optimum:  f({optimum[0]}, {optimum[1]}) = 0")
    print(f"PSO result:    f({best_pos[0]:.6f}, {best_pos[1]:.6f}) = {best_fitness:.6f}")
    print(f"Error:         {abs(best_fitness):.6f}")
    
    # Create visualization
    plot_contour_with_optimum(
        rosenbrock_2d, bounds, optimum, result,
        'Rosenbrock Function: PSO Optimization',
        'rosenbrock_pso_test.png'
    )
    
    return abs(best_fitness) < 0.1


def main():
    """Run visual PSO tests."""
    print("="*60)
    print("Visual PSO Testing Suite")
    print("="*60)
    print("\nThis will generate visualization plots of PSO optimization.")
    print("Plots will be saved as PNG files in the current directory.\n")
    
    results = {}
    
    # Test 1: Sphere function
    results['sphere'] = test_sphere()
    
    # Test 2: Rosenbrock function
    results['rosenbrock'] = test_rosenbrock()
    
    # Summary
    print("\n" + "="*60)
    print("SUMMARY")
    print("="*60)
    
    for test_name, passed in results.items():
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"{test_name:20s} {status}")
    
    total_passed = sum(results.values())
    total_tests = len(results)
    print(f"\nPassed: {total_passed}/{total_tests}")
    print("="*60)
    
    return 0 if all(results.values()) else 1


if __name__ == '__main__':
    import sys
    try:
        sys.exit(main())
    except ImportError as e:
        print(f"Error: {e}")
        print("\nThis script requires matplotlib. Install it with:")
        print("  pip install matplotlib")
        sys.exit(1)
