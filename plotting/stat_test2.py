#!/usr/bin/env python3
"""
SAGE Model Statistical Comparison - FIXED VERSION
==================================================

Statistical analysis script to compare multiple SAGE model variants against
observational data using chi-squared tests and other goodness-of-fit metrics.

FIXES APPLIED:
- Uses actual observational data points (not interpolated grid)
- Proper error handling in log space
- Conservative error estimates (0.3 dex for datasets without errors)
- Minimum error floor of 0.1 dex
- Interpolates model to obs points (not both to common grid)
- Added diagnostic plots for visual verification

This script:
1. Calculates chi-squared values for each model against observations
2. Aggregates statistics across redshift bins
3. Identifies which model performs best overall and in specific regimes
4. Creates summary tables and visualization plots

Author: Statistical Analysis for SAGE models
"""

import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from scipy.interpolate import interp1d
from scipy import stats
import os
import sys

# Import necessary functions from your existing script
# You'll need to have the main script in the same directory or modify the import
try:
    from smf_analysis_obs_sam import (
        MODEL_CONFIGS, get_model_volume, read_hdf, 
        calculate_smf, get_redshift_from_snapshot,
        create_redshift_bins, get_available_snapshots,
        load_muzzin_2013_data, load_santini_2012_data,
        load_wright_2018_data, get_baldry_2008_data,
        obs_data_by_z, is_lowest_redshift_bin
    )
except ImportError:
    print("Warning: Could not import from main script. Using standalone mode.")
    print("Make sure your main script is in the same directory.")


class ModelComparison:
    """Class to handle statistical comparison of SAGE models"""
    
    def __init__(self, model_configs, output_dir='./statistical_analysis/'):
        """
        Initialize the comparison
        
        Parameters:
        -----------
        model_configs : list
            List of model configuration dictionaries
        output_dir : str
            Directory to save analysis results
        """
        self.model_configs = model_configs
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
        
        # Storage for results
        self.chi_squared_results = {}
        self.model_smf_data = {}
        self.obs_data = {}
        
        # Load observational data
        self._load_observational_data()
        
    def _load_observational_data(self):
        """Load all observational datasets"""
        print("Loading observational data...")
        
        # Load different observational datasets
        self.obs_data['baldry'] = self._process_baldry_data()
        self.obs_data['muzzin'] = load_muzzin_2013_data()
        self.obs_data['santini'] = load_santini_2012_data()
        # self.obs_data['wright'] = load_wright_2018_data()
        self.obs_data['csv_data'] = obs_data_by_z  # Pre-loaded CSV data
        
        print(f"Loaded {len(self.obs_data)} observational datasets")
    
    def _process_baldry_data(self):
        """Convert Baldry 2008 data to standard format"""
        masses, phi, phi_upper, phi_lower = get_baldry_2008_data()
        
        # Convert to log space and calculate errors
        log_phi = np.log10(phi)
        log_phi_upper = np.log10(phi_upper)
        log_phi_lower = np.log10(phi_lower)
        
        # Symmetric errors in log space (average of upper and lower)
        errors = (log_phi_upper - log_phi_lower) / 2.0
        
        return {
            'z_center': 0.1,
            'z_range': (0.0, 0.5),
            'M_star': masses,
            'logPhi': log_phi,
            'error': errors
        }
    
    def calculate_residual_statistics(self):
        """Calculate detailed residual statistics for each model"""
        print("\n" + "="*70)
        print("RESIDUAL ANALYSIS")
        print("="*70)
        
        model_residuals = {config['name']: [] for config in self.model_configs}
        
        # Collect all residuals
        for bin_result in self.all_results:
            z_low = bin_result['z_low']
            z_high = bin_result['z_high']
            z_center = bin_result['z_center']
            
            obs_datasets = self._get_obs_for_bin(z_low, z_high, z_center)
            if not obs_datasets:
                continue
                
            for model_config in self.model_configs:
                model_name = model_config['name']
                
                masses, phi = self.calculate_model_smf_for_bin(model_config, z_low, z_high)
                if masses is None:
                    continue
                    
                for obs_name, obs_data in obs_datasets.items():
                    _, model_interp, obs_interp, errors_interp = \
                        self.compare_directly_no_interpolation(
                            masses, phi, obs_data['masses'], 
                            obs_data['phi'], obs_data['errors'])
                    
                    if model_interp is not None:
                        residuals = (model_interp - obs_interp) / errors_interp
                        model_residuals[model_name].extend(residuals)
        
        # Calculate statistics
        print(f"\n{'Model':<25} {'Mean':<10} {'Median':<10} {'Std':<10} {'|Max|':<10}")
        print("-" * 70)
        
        for model_name in sorted(model_residuals.keys()):
            if len(model_residuals[model_name]) > 0:
                residuals = np.array(model_residuals[model_name])
                mean_res = np.mean(residuals)
                median_res = np.median(residuals)
                std_res = np.std(residuals)
                max_res = np.max(np.abs(residuals))
                
                print(f"{model_name:<25} {mean_res:<10.3f} {median_res:<10.3f} "
                    f"{std_res:<10.3f} {max_res:<10.3f}")
        
        return model_residuals

    def calculate_sigma_fractions(self, model_residuals):
        """Calculate fraction of points within 1σ, 2σ, 3σ"""
        print("\n" + "="*70)
        print("SIGMA COVERAGE (should be ~68%, 95%, 99.7%)")
        print("="*70)
        
        print(f"\n{'Model':<25} {'Within 1σ':<15} {'Within 2σ':<15} {'Within 3σ':<15}")
        print("-" * 70)
        
        for model_name in sorted(model_residuals.keys()):
            if len(model_residuals[model_name]) > 0:
                residuals = np.abs(np.array(model_residuals[model_name]))
                n_total = len(residuals)
                
                within_1sig = np.sum(residuals <= 1.0) / n_total * 100
                within_2sig = np.sum(residuals <= 2.0) / n_total * 100
                within_3sig = np.sum(residuals <= 3.0) / n_total * 100
                
                print(f"{model_name:<25} {within_1sig:<14.1f}% {within_2sig:<14.1f}% "
                    f"{within_3sig:<14.1f}%")

    def calculate_per_redshift_rankings(self):
        """Determine which model wins at each redshift"""
        print("\n" + "="*70)
        print("PER-REDSHIFT-BIN RANKINGS")
        print("="*70)
        
        rankings = {config['name']: 0 for config in self.model_configs}
        
        print(f"\n{'Redshift Bin':<20} {'Winner':<25} {'χ²_red':<10}")
        print("-" * 70)
        
        for bin_result in self.all_results:
            z_range = f"{bin_result['z_low']:.1f}-{bin_result['z_high']:.1f}"
            
            best_model = None
            best_chi_sq = float('inf')
            
            for model_name, model_data in bin_result['models'].items():
                if model_data['n_points_total'] > 0:
                    chi_sq = model_data['reduced_chi_squared_total']
                    if chi_sq < best_chi_sq:
                        best_chi_sq = chi_sq
                        best_model = model_name
            
            if best_model:
                rankings[best_model] += 1
                print(f"{z_range:<20} {best_model:<25} {best_chi_sq:<10.3f}")
        
        print("\n" + "-" * 70)
        print("TOTAL WINS:")
        for model_name in sorted(rankings.keys(), key=lambda x: rankings[x], reverse=True):
            print(f"  {model_name}: {rankings[model_name]} bins")
        
        return rankings

    def calculate_mass_range_performance(self):
        """Compare performance at low vs high masses"""
        print("\n" + "="*70)
        print("PERFORMANCE BY MASS RANGE")
        print("="*70)
        
        mass_split = 10.5  # Log M* split
        
        low_mass_chi = {config['name']: [] for config in self.model_configs}
        high_mass_chi = {config['name']: [] for config in self.model_configs}
        
        for bin_result in self.all_results:
            z_low = bin_result['z_low']
            z_high = bin_result['z_high']
            z_center = bin_result['z_center']
            
            obs_datasets = self._get_obs_for_bin(z_low, z_high, z_center)
            if not obs_datasets:
                continue
                
            for model_config in self.model_configs:
                model_name = model_config['name']
                
                masses, phi = self.calculate_model_smf_for_bin(model_config, z_low, z_high)
                if masses is None:
                    continue
                    
                for obs_name, obs_data in obs_datasets.items():
                    comp_masses, model_interp, obs_interp, errors_interp = \
                        self.compare_directly_no_interpolation(
                            masses, phi, obs_data['masses'], 
                            obs_data['phi'], obs_data['errors'])
                    
                    if comp_masses is not None:
                        # Split by mass
                        low_mask = comp_masses < mass_split
                        high_mask = comp_masses >= mass_split
                        
                        if np.any(low_mask):
                            chi_low = np.sum(((model_interp[low_mask] - obs_interp[low_mask]) / 
                                            errors_interp[low_mask])**2)
                            low_mass_chi[model_name].append(chi_low / np.sum(low_mask))
                        
                        if np.any(high_mask):
                            chi_high = np.sum(((model_interp[high_mask] - obs_interp[high_mask]) / 
                                            errors_interp[high_mask])**2)
                            high_mass_chi[model_name].append(chi_high / np.sum(high_mask))
        
        print(f"\nMass split at log(M*/M☉) = {mass_split}")
        print(f"\n{'Model':<25} {'Low-mass χ²_red':<20} {'High-mass χ²_red':<20}")
        print("-" * 70)
        
        for model_name in sorted(low_mass_chi.keys()):
            if len(low_mass_chi[model_name]) > 0 and len(high_mass_chi[model_name]) > 0:
                low_avg = np.mean(low_mass_chi[model_name])
                high_avg = np.mean(high_mass_chi[model_name])
                print(f"{model_name:<25} {low_avg:<20.3f} {high_avg:<20.3f}")

    def plot_residual_distributions(self, model_residuals):
        """Create violin plots showing residual distributions for each model"""
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        
        # Violin plot
        models_with_data = [(name, res) for name, res in model_residuals.items() if len(res) > 0]
        model_names = [name for name, _ in models_with_data]
        residual_data = [res for _, res in models_with_data]
        
        colors = [next((c['color'] for c in self.model_configs if c['name'] == name), 'gray') 
                for name in model_names]
        
        parts = ax1.violinplot(residual_data, positions=range(len(model_names)), 
                            widths=0.7, showmeans=True, showmedians=True)
        
        for i, pc in enumerate(parts['bodies']):
            pc.set_facecolor(colors[i])
            pc.set_alpha(0.7)
        
        # Add reference lines
        ax1.axhline(y=0, color='black', linestyle='--', alpha=0.5, label='Perfect fit')
        ax1.axhline(y=1, color='gray', linestyle=':', alpha=0.3)
        ax1.axhline(y=-1, color='gray', linestyle=':', alpha=0.3)
        ax1.axhline(y=2, color='gray', linestyle=':', alpha=0.2)
        ax1.axhline(y=-2, color='gray', linestyle=':', alpha=0.2)
        
        ax1.set_xticks(range(len(model_names)))
        ax1.set_xticklabels(model_names, rotation=15, ha='right')
        ax1.set_ylabel('Residuals (σ)', fontsize=12)
        ax1.set_title('Residual Distributions', fontsize=14)
        ax1.set_ylim(-5, 5)
        ax1.grid(True, alpha=0.3, axis='y')
        
        # Box plot overlay showing key statistics
        bp = ax2.boxplot(residual_data, positions=range(len(model_names)), 
                        widths=0.5, patch_artist=True)
        
        for i, (patch, color) in enumerate(zip(bp['boxes'], colors)):
            patch.set_facecolor(color)
            patch.set_alpha(0.7)
        
        ax2.axhline(y=0, color='black', linestyle='--', alpha=0.5)
        ax2.set_xticks(range(len(model_names)))
        ax2.set_xticklabels(model_names, rotation=15, ha='right')
        ax2.set_ylabel('Residuals (σ)', fontsize=12)
        ax2.set_title('Residual Statistics (Box Plot)', fontsize=14)
        ax2.set_ylim(-5, 5)
        ax2.grid(True, alpha=0.3, axis='y')
        
        plt.tight_layout()
        plot_path = os.path.join(self.output_dir, 'residual_distributions.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved residual distributions to: {plot_path}")
        
        return fig

    def plot_sigma_coverage(self, model_residuals):
        """Bar chart comparing sigma coverage across models"""
        fig, ax = plt.subplots(figsize=(10, 6))
        
        model_names = []
        within_1sig = []
        within_2sig = []
        within_3sig = []
        colors_list = []
        
        for model_config in self.model_configs:
            model_name = model_config['name']
            if len(model_residuals[model_name]) > 0:
                residuals = np.abs(np.array(model_residuals[model_name]))
                n_total = len(residuals)
                
                model_names.append(model_name)
                within_1sig.append(np.sum(residuals <= 1.0) / n_total * 100)
                within_2sig.append(np.sum(residuals <= 2.0) / n_total * 100)
                within_3sig.append(np.sum(residuals <= 3.0) / n_total * 100)
                colors_list.append(model_config['color'])
        
        x = np.arange(len(model_names))
        width = 0.25
        
        bars1 = ax.bar(x - width, within_1sig, width, label='Within 1σ', alpha=0.8)
        bars2 = ax.bar(x, within_2sig, width, label='Within 2σ', alpha=0.8)
        bars3 = ax.bar(x + width, within_3sig, width, label='Within 3σ', alpha=0.8)
        
        # Color bars by model
        for bars in [bars1, bars2, bars3]:
            for bar, color in zip(bars, colors_list):
                bar.set_color(color)
        
        # Add expected values as horizontal lines
        ax.axhline(y=68.3, color='black', linestyle='--', alpha=0.5, linewidth=2, label='Expected 1σ')
        ax.axhline(y=95.4, color='black', linestyle=':', alpha=0.5, linewidth=2, label='Expected 2σ')
        ax.axhline(y=99.7, color='black', linestyle='-.', alpha=0.5, linewidth=2, label='Expected 3σ')
        
        ax.set_xlabel('Model', fontsize=12)
        ax.set_ylabel('Percentage of Points', fontsize=12)
        ax.set_title('Sigma Coverage Comparison\n(Closer to expected values = better calibrated errors)', fontsize=14)
        ax.set_xticks(x)
        ax.set_xticklabels(model_names, rotation=15, ha='right')
        ax.legend(fontsize=10)
        ax.grid(True, alpha=0.3, axis='y')
        ax.set_ylim(0, 105)
        
        plt.tight_layout()
        plot_path = os.path.join(self.output_dir, 'sigma_coverage.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved sigma coverage plot to: {plot_path}")
        
        return fig

    def plot_redshift_winners(self):
        """Heatmap showing which model wins at each redshift"""
        fig, ax = plt.subplots(figsize=(12, 8))
        
        # Collect data
        z_bins = []
        model_chi_sq = {config['name']: [] for config in self.model_configs}
        
        for bin_result in self.all_results:
            if not bin_result['models']:
                continue
                
            z_center = bin_result['z_center']
            z_bins.append(z_center)
            
            for model_config in self.model_configs:
                model_name = model_config['name']
                if model_name in bin_result['models']:
                    chi_sq = bin_result['models'][model_name]['reduced_chi_squared_total']
                    model_chi_sq[model_name].append(chi_sq)
                else:
                    model_chi_sq[model_name].append(np.nan)
        
        # Create matrix
        model_names = [config['name'] for config in self.model_configs]
        data_matrix = np.array([model_chi_sq[name] for name in model_names])
        
        # Plot heatmap
        im = ax.imshow(data_matrix, aspect='auto', cmap='RdYlGn_r', 
                    interpolation='nearest', vmin=0, vmax=20)
        
        # Set ticks
        ax.set_yticks(range(len(model_names)))
        ax.set_yticklabels(model_names, fontsize=11)
        ax.set_xticks(range(len(z_bins)))
        ax.set_xticklabels([f'{z:.1f}' for z in z_bins], fontsize=9)
        ax.set_xlabel('Redshift (z)', fontsize=12)
        ax.set_title('χ²_red by Model and Redshift\n(Green = better fit)', fontsize=14)
        
        # Add colorbar
        cbar = plt.colorbar(im, ax=ax)
        cbar.set_label('χ²_red', fontsize=12)
        
        # Add text annotations showing values
        for i in range(len(model_names)):
            for j in range(len(z_bins)):
                if not np.isnan(data_matrix[i, j]):
                    text = ax.text(j, i, f'{data_matrix[i, j]:.1f}',
                                ha="center", va="center", color="black", fontsize=8)
        
        # Mark winners with stars
        for j in range(len(z_bins)):
            col = data_matrix[:, j]
            if not np.all(np.isnan(col)):
                winner_idx = np.nanargmin(col)
                ax.plot(j, winner_idx, marker='*', color='gold', markersize=15, 
                    markeredgecolor='black', markeredgewidth=1)
        
        plt.tight_layout()
        plot_path = os.path.join(self.output_dir, 'redshift_winners_heatmap.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved redshift winners heatmap to: {plot_path}")
        
        return fig

    def plot_mass_dependence(self):
        """Show performance split by mass range""" 
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
        
        mass_split = 10.5
        low_mass_chi = {config['name']: [] for config in self.model_configs}
        high_mass_chi = {config['name']: [] for config in self.model_configs}
        # NEW: Track paired measurements for scatter plot
        paired_chi = {config['name']: {'low': [], 'high': []} for config in self.model_configs}
        
        # Calculate mass-dependent chi-squared
        for bin_result in self.all_results:
            z_low = bin_result['z_low']
            z_high = bin_result['z_high']
            z_center = bin_result['z_center']
            
            obs_datasets = self._get_obs_for_bin(z_low, z_high, z_center)
            if not obs_datasets:
                continue
                
            for model_config in self.model_configs:
                model_name = model_config['name']
                
                masses, phi = self.calculate_model_smf_for_bin(model_config, z_low, z_high)
                if masses is None:
                    continue
                    
                for obs_name, obs_data in obs_datasets.items():
                    comp_masses, model_interp, obs_interp, errors_interp = \
                        self.compare_directly_no_interpolation(
                            masses, phi, obs_data['masses'], 
                            obs_data['phi'], obs_data['errors'])
                    
                    if comp_masses is not None:
                        # Split by mass
                        low_mask = comp_masses < mass_split
                        high_mask = comp_masses >= mass_split
                        
                        chi_low = None
                        chi_high = None
                        
                        if np.any(low_mask):
                            chi_low = np.sum(((model_interp[low_mask] - obs_interp[low_mask]) / 
                                            errors_interp[low_mask])**2) / np.sum(low_mask)
                            low_mass_chi[model_name].append(chi_low)
                        
                        if np.any(high_mask):
                            chi_high = np.sum(((model_interp[high_mask] - obs_interp[high_mask]) / 
                                            errors_interp[high_mask])**2) / np.sum(high_mask)
                            high_mass_chi[model_name].append(chi_high)
                        
                        # NEW: Only add to paired data if BOTH masses have data
                        if chi_low is not None and chi_high is not None:
                            paired_chi[model_name]['low'].append(chi_low)
                            paired_chi[model_name]['high'].append(chi_high)
        
        # Plot low-mass performance (bar chart - unchanged)
        model_names = []
        low_values = []
        high_values = []
        colors = []
        
        for model_config in self.model_configs:
            model_name = model_config['name']
            if len(low_mass_chi[model_name]) > 0 and len(high_mass_chi[model_name]) > 0:
                model_names.append(model_name)
                low_values.append(np.mean(low_mass_chi[model_name]))
                high_values.append(np.mean(high_mass_chi[model_name]))
                colors.append(model_config['color'])
        
        x = np.arange(len(model_names))
        width = 0.35
        
        bars1 = ax1.bar(x - width/2, low_values, width, label=f'log(M*) < {mass_split}', alpha=0.8)
        bars2 = ax1.bar(x + width/2, high_values, width, label=f'log(M*) ≥ {mass_split}', alpha=0.8)
        
        for bars in [bars1, bars2]:
            for bar, color in zip(bars, colors):
                bar.set_color(color)
        
        ax1.axhline(y=1, color='gray', linestyle='--', alpha=0.5, label='Perfect fit')
        ax1.set_xlabel('Model', fontsize=12)
        ax1.set_ylabel('χ²_red', fontsize=12)
        ax1.set_title('Performance by Mass Range', fontsize=14)
        ax1.set_xticks(x)
        ax1.set_xticklabels(model_names, rotation=15, ha='right')
        ax1.legend()
        ax1.grid(True, alpha=0.3, axis='y')
        
        # Scatter plot showing relationship - FIXED to use paired data
        for model_config in self.model_configs:
            model_name = model_config['name']
            color = model_config['color']
            if len(paired_chi[model_name]['low']) > 0:
                ax2.scatter(paired_chi[model_name]['low'], 
                        paired_chi[model_name]['high'], 
                        c=color, label=model_name, s=50, alpha=0.6)
        
        # Set reasonable axis limits
        max_val = 25
        if paired_chi:
            all_low = [v for model in paired_chi.values() for v in model['low']]
            all_high = [v for model in paired_chi.values() for v in model['high']]
            if all_low and all_high:
                max_val = max(25, max(max(all_low), max(all_high)))
        
        ax2.plot([0, max_val], [0, max_val], 'k--', alpha=0.3, label='Equal performance')
        ax2.set_xlabel(f'χ²_red at low mass (log M* < {mass_split})', fontsize=12)
        ax2.set_ylabel(f'χ²_red at high mass (log M* ≥ {mass_split})', fontsize=12)
        ax2.set_title('Low-mass vs High-mass Performance', fontsize=14)
        ax2.legend(fontsize=9)
        ax2.grid(True, alpha=0.3)
        ax2.set_xlim(0, max_val)
        ax2.set_ylim(0, max_val)
        
        plt.tight_layout()
        plot_path = os.path.join(self.output_dir, 'mass_dependence.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved mass dependence plot to: {plot_path}")
        
        return fig
    
    def compare_directly_no_interpolation(self, model_masses, model_phi, obs_masses, obs_phi, obs_errors):
        """
        Compare model to observations using only actual observational data points
        Interpolate MODEL to observation points, not the other way around
        
        Returns:
        --------
        obs_masses, model_phi_at_obs, obs_phi, obs_errors : arrays
        """
        # Filter out invalid observational values
        obs_mask = np.isfinite(obs_phi) & np.isfinite(obs_errors) & (obs_errors > 0)
        
        if not np.any(obs_mask):
            return None, None, None, None
        
        obs_masses_valid = obs_masses[obs_mask]
        obs_phi_valid = obs_phi[obs_mask]
        obs_errors_valid = obs_errors[obs_mask]
        
        # Filter model for valid values
        model_mask = (model_phi > 0) & np.isfinite(model_phi)
        if not np.any(model_mask):
            return None, None, None, None
        
        model_masses_valid = model_masses[model_mask]
        model_phi_valid = model_phi[model_mask]
        
        # Convert model phi to log space
        model_log_phi = np.log10(model_phi_valid)
        
        # Find overlap region
        mass_min = max(np.min(model_masses_valid), np.min(obs_masses_valid))
        mass_max = min(np.max(model_masses_valid), np.max(obs_masses_valid))
        
        if mass_min >= mass_max:
            return None, None, None, None
        
        # Keep only obs points in overlap region
        overlap_mask = (obs_masses_valid >= mass_min) & (obs_masses_valid <= mass_max)
        if not np.any(overlap_mask):
            return None, None, None, None
        
        obs_masses_overlap = obs_masses_valid[overlap_mask]
        obs_phi_overlap = obs_phi_valid[overlap_mask]
        obs_errors_overlap = obs_errors_valid[overlap_mask]
        
        try:
            # Interpolate MODEL to observational mass points
            model_interp_func = interp1d(model_masses_valid, model_log_phi, 
                                        kind='linear', bounds_error=False, fill_value=np.nan)
            
            model_phi_at_obs = model_interp_func(obs_masses_overlap)
            
            # Filter out any NaN values from interpolation
            valid_mask = np.isfinite(model_phi_at_obs)
            
            if not np.any(valid_mask):
                return None, None, None, None
            
            return (obs_masses_overlap[valid_mask], model_phi_at_obs[valid_mask], 
                   obs_phi_overlap[valid_mask], obs_errors_overlap[valid_mask])
            
        except Exception as e:
            print(f"  Comparison failed: {e}")
            return None, None, None, None
    
    def calculate_chi_squared(self, model_phi, obs_phi, obs_errors):
        """
        Calculate chi-squared statistic
        
        Parameters:
        -----------
        model_phi : array
            Model predictions (in log space)
        obs_phi : array
            Observations (in log space)
        obs_errors : array
            Observational errors (in log space)
            
        Returns:
        --------
        chi_squared, n_points, reduced_chi_squared : float
        """
        residuals = model_phi - obs_phi
        chi_squared = np.sum((residuals / obs_errors)**2)
        n_points = len(residuals)
        
        # Reduced chi-squared (assuming no free parameters fitted to this specific data)
        reduced_chi_squared = chi_squared / n_points if n_points > 0 else np.inf
        
        return chi_squared, n_points, reduced_chi_squared
    
    def calculate_model_smf_for_bin(self, model_config, z_low, z_high):
        """
        Calculate SMF for a specific model and redshift bin
        
        Returns:
        --------
        masses, phi : arrays (or None if failed)
        """
        model_name = model_config['name']
        directory = model_config['dir']
        
        # Get available snapshots
        available_snaps = get_available_snapshots(directory)
        if not available_snaps:
            return None, None
        
        # Find best snapshot for this redshift bin
        best_snap = None
        min_diff = float('inf')
        
        for snap in available_snaps:
            z_snap = get_redshift_from_snapshot(snap, model_config)
            if z_low <= z_snap < z_high:
                diff = abs(z_snap - z_low)
                if diff < min_diff:
                    min_diff = diff
                    best_snap = snap
        
        if best_snap is None:
            return None, None
        
        try:
            # Read data
            snap_str = f'Snap_{best_snap}'
            stellar_mass = read_hdf(directory, snap_num=snap_str, param='StellarMass')
            
            if stellar_mass is None:
                return None, None
            
            # Convert to solar masses
            stellar_mass = stellar_mass * 1.0e10 / model_config['hubble_h']
            
            # Filter positive masses
            stellar_mass = stellar_mass[stellar_mass > 0]
            
            # Calculate SMF
            volume = get_model_volume(model_config)
            masses, phi, phi_err = calculate_smf(stellar_mass, volume=volume)
            
            return masses, phi
            
        except Exception as e:
            print(f"Error calculating SMF for {model_name}: {e}")
            return None, None
    
    def compare_models_at_redshift_bin(self, z_low, z_high):
        """
        Compare all models against observations in a specific redshift bin
        
        Returns:
        --------
        results_dict : dict with chi-squared results for each model
        """
        print(f"\nAnalyzing redshift bin {z_low:.1f} < z < {z_high:.1f}")
        
        z_center = (z_low + z_high) / 2
        bin_results = {
            'z_low': z_low,
            'z_high': z_high,
            'z_center': z_center,
            'models': {}
        }
        
        # Get observational data for this bin
        obs_datasets = self._get_obs_for_bin(z_low, z_high, z_center)
        
        if not obs_datasets:
            print(f"  No observational data for this bin")
            return bin_results
        
        # Calculate SMF for each model
        for model_config in self.model_configs:
            model_name = model_config['name']
            print(f"  Processing {model_name}...")
            
            masses, phi = self.calculate_model_smf_for_bin(model_config, z_low, z_high)
            
            if masses is None:
                print(f"    No model data available")
                continue
            
            model_results = {
                'chi_squared_total': 0,
                'n_points_total': 0,
                'datasets': {}
            }
            
            # Compare against each observational dataset
            for obs_name, obs_data in obs_datasets.items():
                obs_masses = obs_data['masses']
                obs_phi = obs_data['phi']  # Should be in log space
                obs_errors = obs_data['errors']  # Should be in log space
                
                # Compare directly - interpolate model to obs points
                masses_comp, model_interp, obs_interp, errors_interp = \
                    self.compare_directly_no_interpolation(masses, phi, obs_masses, 
                                                           obs_phi, obs_errors)
                
                if masses_comp is None:
                    print(f"    No overlap with {obs_name}")
                    continue
                
                # Calculate chi-squared
                chi_sq, n_pts, reduced_chi_sq = self.calculate_chi_squared(
                    model_interp, obs_interp, errors_interp)
                
                model_results['datasets'][obs_name] = {
                    'chi_squared': chi_sq,
                    'n_points': n_pts,
                    'reduced_chi_squared': reduced_chi_sq
                }
                
                model_results['chi_squared_total'] += chi_sq
                model_results['n_points_total'] += n_pts
                
                print(f"    {obs_name}: χ² = {chi_sq:.2f}, n = {n_pts}, χ²_red = {reduced_chi_sq:.3f}")
            
            # Calculate overall reduced chi-squared for this bin
            if model_results['n_points_total'] > 0:
                model_results['reduced_chi_squared_total'] = \
                    model_results['chi_squared_total'] / model_results['n_points_total']
            else:
                model_results['reduced_chi_squared_total'] = np.inf
            
            bin_results['models'][model_name] = model_results
        
        return bin_results
    
    def _get_obs_for_bin(self, z_low, z_high, z_center):
        """Get all relevant observational datasets for a redshift bin"""
        obs_datasets = {}
        
        # Check Baldry (z~0.1)
        if is_lowest_redshift_bin(z_low, z_high):
            baldry = self.obs_data['baldry']
            obs_datasets['Baldry2008'] = {
                'masses': baldry['M_star'],
                'phi': baldry['logPhi'],  # Already in log space
                'errors': baldry['error']  # Already in log space
            }
        
        # Check Muzzin - use ACTUAL errors, not assumed
        for bin_name, data in self.obs_data['muzzin'].items():
            if z_low <= data['z_center'] < z_high:
                # Muzzin doesn't provide explicit errors in the dataset
                # Use typical SMF uncertainty of ~0.3 dex (conservative)
                typical_error = 0.3  # dex
                obs_datasets[f'Muzzin2013_{bin_name}'] = {
                    'masses': data['M_star'],
                    'phi': data['logPhi'],  # Already in log space
                    'errors': np.full_like(data['logPhi'], typical_error)
                }
        
        # Check Santini
        for bin_name, data in self.obs_data['santini'].items():
            if z_low <= data['z_center'] < z_high:
                # Use average of upper and lower errors
                errors = (data['error_hi'] + data['error_lo']) / 2.0
                obs_datasets[f'Santini2012_{bin_name}'] = {
                    'masses': data['M_star'],
                    'phi': data['logPhi'],  # Already in log space
                    'errors': errors  # Already in log space
                }
        
        # Check Wright
        # for z_wright, data in self.obs_data['wright'].items():
        #     if z_low <= z_wright < z_high:
        #         errors = (data['error_hi'] + data['error_lo']) / 2.0
        #         obs_datasets[f'Wright2018_z{z_wright}'] = {
        #             'masses': data['M_star'],
        #             'phi': data['logPhi'],  # Already in log space
        #             'errors': errors  # Already in log space
        #         }
        
        # Check CSV data (SHARK, Weaver, Thorne, etc.)
        for z_obs, data in self.obs_data['csv_data'].items():
            if abs(z_obs - z_center) < 0.3:  # Within tolerance
                data_type = data.get('type', 'unknown')
                label = data.get('label', 'unknown')
                
                # Handle different data formats
                if data_type in ['smfvals', 'farmer']:
                    # Has error bounds - convert to log space properly
                    y_central = data['y']
                    y_lower = data['y_lower']
                    y_upper = data['y_upper']
                    
                    # Filter out zeros before taking log
                    valid_mask = (y_central > 0) & (y_lower > 0) & (y_upper > 0)
                    
                    if np.any(valid_mask):
                        phi_central = np.log10(y_central[valid_mask])
                        phi_lower = np.log10(y_lower[valid_mask])
                        phi_upper = np.log10(y_upper[valid_mask])
                        
                        # Symmetric error in log space
                        errors = (phi_upper - phi_lower) / 2.0
                        
                        # Ensure minimum error of 0.1 dex (instrumental/systematic floor)
                        errors = np.maximum(errors, 0.1)
                        
                        obs_datasets[f'{label}_z{z_obs}'] = {
                            'masses': data['x'][valid_mask],
                            'phi': phi_central,
                            'errors': errors
                        }
                        
                elif data_type == 'shark':
                    # SHARK data - already in log space
                    # Use 0.3 dex uncertainty (typical for model predictions)
                    obs_datasets[f'{label}_z{z_obs}'] = {
                        'masses': data['x'],
                        'phi': data['y'],  # Already in log space
                        'errors': np.full_like(data['y'], 0.3)  # Conservative error
                    }
        
        return obs_datasets
    
    def run_full_comparison(self):
        """Run comparison across all redshift bins"""
        print("\n" + "="*70)
        print("RUNNING FULL MODEL COMPARISON")
        print("="*70)
        
        # Get redshift bins
        first_model = self.model_configs[0]
        available_snaps = get_available_snapshots(first_model['dir'])
        redshift_bins = create_redshift_bins(available_snaps, model_config=first_model)
        
        print(f"\nAnalyzing {len(redshift_bins)} redshift bins")
        print(f"Comparing {len(self.model_configs)} models")
        
        # Run comparison for each bin
        all_results = []
        for z_low, z_high, z_center, snapshots in redshift_bins:
            bin_result = self.compare_models_at_redshift_bin(z_low, z_high)
            all_results.append(bin_result)
        
        self.all_results = all_results
        return all_results
    
    def create_summary_table(self):
        """Create summary table of chi-squared results"""
        print("\n" + "="*70)
        print("SUMMARY STATISTICS")
        print("="*70)
        
        # Aggregate results across all bins
        model_summaries = {}
        
        for model_config in self.model_configs:
            model_name = model_config['name']
            model_summaries[model_name] = {
                'total_chi_squared': 0,
                'total_n_points': 0,
                'n_bins_analyzed': 0,
                'bin_results': []
            }
        
        for bin_result in self.all_results:
            z_range = f"{bin_result['z_low']:.1f}-{bin_result['z_high']:.1f}"
            
            for model_name, model_data in bin_result['models'].items():
                if model_name in model_summaries:
                    chi_sq = model_data['chi_squared_total']
                    n_pts = model_data['n_points_total']
                    
                    if n_pts > 0:
                        model_summaries[model_name]['total_chi_squared'] += chi_sq
                        model_summaries[model_name]['total_n_points'] += n_pts
                        model_summaries[model_name]['n_bins_analyzed'] += 1
                        model_summaries[model_name]['bin_results'].append({
                            'z_range': z_range,
                            'chi_squared': chi_sq,
                            'n_points': n_pts,
                            'reduced_chi_squared': model_data['reduced_chi_squared_total']
                        })
        
        # Calculate overall reduced chi-squared
        for model_name, summary in model_summaries.items():
            if summary['total_n_points'] > 0:
                summary['overall_reduced_chi_squared'] = \
                    summary['total_chi_squared'] / summary['total_n_points']
            else:
                summary['overall_reduced_chi_squared'] = np.inf
        
        # Print summary table
        print("\nOVERALL PERFORMANCE (lower is better):")
        print("-" * 70)
        print(f"{'Model':<25} {'Total χ²':<15} {'N points':<10} {'χ²_red':<10} {'N bins':<10}")
        print("-" * 70)
        
        # Sort by reduced chi-squared
        sorted_models = sorted(model_summaries.items(), 
                             key=lambda x: x[1]['overall_reduced_chi_squared'])
        
        for model_name, summary in sorted_models:
            print(f"{model_name:<25} {summary['total_chi_squared']:<15.1f} "
                  f"{summary['total_n_points']:<10} "
                  f"{summary['overall_reduced_chi_squared']:<10.3f} "
                  f"{summary['n_bins_analyzed']:<10}")
        
        print("-" * 70)
        print(f"\nBest overall model: {sorted_models[0][0]} "
              f"(χ²_red = {sorted_models[0][1]['overall_reduced_chi_squared']:.3f})")
        
        # Save detailed results to CSV
        self._save_detailed_results(model_summaries)
        
        return model_summaries
    
    def _save_detailed_results(self, model_summaries):
        """Save detailed results to CSV files"""
        # Per-redshift-bin results
        bin_data = []
        for bin_result in self.all_results:
            z_range = f"{bin_result['z_low']:.1f}-{bin_result['z_high']:.1f}"
            z_center = bin_result['z_center']
            
            for model_name, model_data in bin_result['models'].items():
                bin_data.append({
                    'redshift_bin': z_range,
                    'z_center': z_center,
                    'model': model_name,
                    'chi_squared': model_data['chi_squared_total'],
                    'n_points': model_data['n_points_total'],
                    'reduced_chi_squared': model_data['reduced_chi_squared_total']
                })
        
        df_bins = pd.DataFrame(bin_data)
        csv_path = os.path.join(self.output_dir, 'chi_squared_by_redshift_bin.csv')
        df_bins.to_csv(csv_path, index=False)
        print(f"\nSaved per-bin results to: {csv_path}")
        
        # Overall summary
        summary_data = []
        for model_name, summary in model_summaries.items():
            summary_data.append({
                'model': model_name,
                'total_chi_squared': summary['total_chi_squared'],
                'total_n_points': summary['total_n_points'],
                'overall_reduced_chi_squared': summary['overall_reduced_chi_squared'],
                'n_bins_analyzed': summary['n_bins_analyzed']
            })
        
        df_summary = pd.DataFrame(summary_data)
        df_summary = df_summary.sort_values('overall_reduced_chi_squared')
        csv_path = os.path.join(self.output_dir, 'overall_model_comparison.csv')
        df_summary.to_csv(csv_path, index=False)
        print(f"Saved overall summary to: {csv_path}")
    
    def plot_chi_squared_by_redshift(self):
        """Create plot showing chi-squared evolution with redshift"""
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10), sharex=True)
        
        # Organize data by model
        model_data = {config['name']: {'z': [], 'chi_sq': [], 'n_pts': [], 'chi_sq_red': []} 
                     for config in self.model_configs}
        
        for bin_result in self.all_results:
            z_center = bin_result['z_center']
            
            for model_name, model_result in bin_result['models'].items():
                if model_name in model_data and model_result['n_points_total'] > 0:
                    model_data[model_name]['z'].append(z_center)
                    model_data[model_name]['chi_sq'].append(model_result['chi_squared_total'])
                    model_data[model_name]['n_pts'].append(model_result['n_points_total'])
                    model_data[model_name]['chi_sq_red'].append(model_result['reduced_chi_squared_total'])
        
        # Plot chi-squared
        for model_config in self.model_configs:
            model_name = model_config['name']
            color = model_config['color']
            linestyle = model_config['linestyle']
            
            if model_data[model_name]['z']:
                ax1.plot(model_data[model_name]['z'], model_data[model_name]['chi_sq'],
                        'o', color=color, linestyle=linestyle, label=model_name, linewidth=2,
                        markersize=6)
                ax1.plot(model_data[model_name]['z'], model_data[model_name]['chi_sq'],
                        linestyle=linestyle, color=color, linewidth=2, alpha=0.5)
        
        ax1.set_ylabel(r'$\chi^2$', fontsize=14)
        ax1.set_yscale('log')
        ax1.legend(fontsize=12)
        ax1.grid(True, alpha=0.3)
        ax1.set_title('Goodness of Fit vs Redshift', fontsize=16)
        
        # Plot reduced chi-squared
        for model_config in self.model_configs:
            model_name = model_config['name']
            color = model_config['color']
            linestyle = model_config['linestyle']
            
            if model_data[model_name]['z']:
                ax2.plot(model_data[model_name]['z'], model_data[model_name]['chi_sq_red'],
                        'o', color=color, linestyle=linestyle, label=model_name, linewidth=2,
                        markersize=6)
                ax2.plot(model_data[model_name]['z'], model_data[model_name]['chi_sq_red'],
                        linestyle=linestyle, color=color, linewidth=2, alpha=0.5)
        
        ax2.set_xlabel('Redshift', fontsize=14)
        ax2.set_ylabel(r'$\chi^2_{\rm red}$', fontsize=14)
        ax2.axhline(y=1, color='gray', linestyle='--', alpha=0.5, label='Perfect fit')
        ax2.set_ylim(0, max(10, min(20, ax2.get_ylim()[1])))
        ax2.legend(fontsize=12)
        ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plot_path = os.path.join(self.output_dir, 'chi_squared_vs_redshift.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"\nSaved chi-squared plot to: {plot_path}")
        
        return fig
    
    def plot_performance_summary(self):
        """Create bar chart comparing overall model performance"""
        # Get overall statistics
        model_names = []
        chi_sq_red_values = []
        colors = []
        
        for model_config in self.model_configs:
            model_name = model_config['name']
            
            # Calculate overall chi-squared
            total_chi_sq = 0
            total_n_pts = 0
            
            for bin_result in self.all_results:
                if model_name in bin_result['models']:
                    model_data = bin_result['models'][model_name]
                    total_chi_sq += model_data['chi_squared_total']
                    total_n_pts += model_data['n_points_total']
            
            if total_n_pts > 0:
                model_names.append(model_name)
                chi_sq_red_values.append(total_chi_sq / total_n_pts)
                colors.append(model_config['color'])
        
        # Create bar chart
        fig, ax = plt.subplots(figsize=(10, 6))
        
        x_pos = np.arange(len(model_names))
        bars = ax.bar(x_pos, chi_sq_red_values, color=colors, alpha=0.7, edgecolor='black')
        
        ax.set_xlabel('Model', fontsize=14)
        ax.set_ylabel(r'Overall $\chi^2_{\rm red}$', fontsize=14)
        ax.set_title('Model Performance Comparison\n(Lower is Better)', fontsize=16)
        ax.set_xticks(x_pos)
        ax.set_xticklabels(model_names, rotation=15, ha='right')
        ax.axhline(y=1, color='gray', linestyle='--', alpha=0.5, label='Perfect fit')
        ax.grid(True, axis='y', alpha=0.3)
        ax.legend()
        
        # Add value labels on bars
        for i, (bar, val) in enumerate(zip(bars, chi_sq_red_values)):
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   f'{val:.2f}', ha='center', va='bottom', fontsize=11)
        
        plt.tight_layout()
        plot_path = os.path.join(self.output_dir, 'model_performance_comparison.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved performance comparison to: {plot_path}")
        
        return fig
    
    def plot_diagnostic_comparison(self, z_low=0.0, z_high=0.5):
        """
        Create diagnostic plot showing model vs observations for a specific redshift bin
        This helps verify the chi-squared calculation is reasonable
        """
        print(f"\nCreating diagnostic comparison plot for {z_low} < z < {z_high}...")
        
        z_center = (z_low + z_high) / 2
        
        # Get observational data
        obs_datasets = self._get_obs_for_bin(z_low, z_high, z_center)
        
        if not obs_datasets:
            print("No observational data for this bin")
            return
        
        # Create figure with subplots
        n_models = len(self.model_configs)
        fig, axes = plt.subplots(2, 2, figsize=(14, 12))
        axes = axes.flatten()
        
        for idx, model_config in enumerate(self.model_configs):
            if idx >= 4:
                break
                
            ax = axes[idx]
            model_name = model_config['name']
            color = model_config['color']
            
            # Calculate model SMF
            masses, phi = self.calculate_model_smf_for_bin(model_config, z_low, z_high)
            
            if masses is not None:
                # Plot model
                mask = phi > 0
                phi_log = np.log10(phi[mask])
                ax.plot(masses[mask], phi_log, '-', color=color, linewidth=2, 
                       label=model_name, alpha=0.8)
                
                # Plot observations
                for obs_name, obs_data in obs_datasets.items():
                    obs_masses = obs_data['masses']
                    obs_phi = obs_data['phi']
                    obs_errors = obs_data['errors']
                    
                    # Simple label for legend
                    simple_label = obs_name.split('_')[0]
                    
                    ax.errorbar(obs_masses, obs_phi, yerr=obs_errors, 
                              fmt='o', markersize=4, alpha=0.6, capsize=3,
                              label=simple_label)
            
            ax.set_xlabel(r'$\log_{10}(M_*/M_{\odot})$', fontsize=12)
            ax.set_ylabel(r'$\log_{10}(\phi)$ [Mpc$^{-3}$ dex$^{-1}$]', fontsize=12)
            ax.set_title(f'{model_name}', fontsize=14)
            ax.set_xlim(8, 12)
            ax.set_ylim(-6, -1)
            ax.legend(fontsize=8, loc='lower left')
            ax.grid(True, alpha=0.3)
        
        plt.suptitle(f'Diagnostic: Model vs Observations\n{z_low} < z < {z_high}', 
                    fontsize=16)
        plt.tight_layout()
        
        plot_path = os.path.join(self.output_dir, f'diagnostic_z{z_low:.1f}_{z_high:.1f}.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved diagnostic plot to: {plot_path}")
        
        return fig


def main():
    """Main execution function"""
    print("\n" + "="*70)
    print("SAGE MODEL STATISTICAL COMPARISON ANALYSIS - FIXED VERSION")
    print("="*70)
    print("\nKey fixes applied:")
    print("  - Using actual observational data points (not interpolated grid)")
    print("  - Proper error handling in log space")
    print("  - Conservative error estimates (0.3 dex for datasets without errors)")
    print("  - Minimum error floor of 0.1 dex to avoid artificially small errors")
    print("="*70)
    
    # Initialize comparison
    comparator = ModelComparison(MODEL_CONFIGS, output_dir='./statistical_analysis/')
    
    # Run full comparison
    results = comparator.run_full_comparison()
    
    # Create summary statistics
    model_summaries = comparator.create_summary_table()

    print("\n" + "="*70)
    print("ADDITIONAL STATISTICAL ANALYSES")
    print("="*70)
    
    model_residuals = comparator.calculate_residual_statistics()
    comparator.calculate_sigma_fractions(model_residuals)
    comparator.calculate_per_redshift_rankings()
    comparator.calculate_mass_range_performance()

    print("\n" + "="*70)
    print("CREATING ADDITIONAL VISUALIZATION PLOTS")
    print("="*70)
    
    comparator.plot_residual_distributions(model_residuals)
    comparator.plot_sigma_coverage(model_residuals)
    comparator.plot_redshift_winners()
    comparator.plot_mass_dependence()
    
    # Create plots
    comparator.plot_chi_squared_by_redshift()
    comparator.plot_performance_summary()
    
    # Create diagnostic plots for key redshift bins
    print("\n" + "="*70)
    print("CREATING DIAGNOSTIC PLOTS")
    print("="*70)
    comparator.plot_diagnostic_comparison(z_low=0.0, z_high=0.5)
    comparator.plot_diagnostic_comparison(z_low=1.5, z_high=2.0)
    comparator.plot_diagnostic_comparison(z_low=3.5, z_high=4.5)
    
    print("\n" + "="*70)
    print("ANALYSIS COMPLETE!")
    print("="*70)
    print("\nGenerated files:")
    print("1. chi_squared_by_redshift_bin.csv - Detailed χ² for each redshift bin")
    print("2. overall_model_comparison.csv - Summary statistics for each model")
    print("3. chi_squared_vs_redshift.pdf - χ² evolution with redshift")
    print("4. model_performance_comparison.pdf - Bar chart of overall performance")
    print("5. diagnostic_z*.pdf - Visual comparison of models vs observations")
    print("\nInterpretation Guide:")
    print("-" * 70)
    print("χ²_red ≈ 1: Good fit (model consistent with observations within errors)")
    print("χ²_red < 1: Model fits better than expected")
    print("           (may indicate overestimated observational errors)")
    print("χ²_red > 1: Model doesn't fully capture observations")
    print("           (systematic model deficiencies or underestimated errors)")
    print("\nTypical ranges:")
    print("  χ²_red = 0.5-2.0: Excellent fit")
    print("  χ²_red = 2.0-5.0: Acceptable fit with some tension")
    print("  χ²_red > 5.0: Poor fit, significant model-data disagreement")
    print("="*70)
    
    # Print interpretation of results
    print("\n" + "="*70)
    print("RESULT INTERPRETATION")
    print("="*70)
    
    sorted_models = sorted(model_summaries.items(), 
                          key=lambda x: x[1]['overall_reduced_chi_squared'])
    
    best_model = sorted_models[0][0]
    best_chi_sq = sorted_models[0][1]['overall_reduced_chi_squared']
    
    print(f"\nBest performing model: {best_model} (χ²_red = {best_chi_sq:.2f})")
    
    if best_chi_sq < 2.0:
        print("→ EXCELLENT: Model reproduces observations very well")
    elif best_chi_sq < 5.0:
        print("→ GOOD: Model captures observations reasonably well")
    elif best_chi_sq < 10.0:
        print("→ ACCEPTABLE: Model has some tension with observations")
    else:
        print("→ POOR: Model has significant disagreement with observations")
    
    print("\nRelative model ranking:")
    for i, (model_name, summary) in enumerate(sorted_models, 1):
        chi_sq = summary['overall_reduced_chi_squared']
        n_bins = summary['n_bins_analyzed']
        print(f"  {i}. {model_name}: χ²_red = {chi_sq:.2f} ({n_bins} redshift bins)")
    
    print("\nCheck the diagnostic plots to visually verify the fits!")
    print("="*70)


if __name__ == "__main__":
    main()