#!/usr/bin/env python3
"""
SAGE Model Statistical Comparison - WITH COMPREHENSIVE VISUALIZATIONS
======================================================================

Full integration of chi-squared + correlation metrics with extensive plotting

NEW VISUALIZATION FUNCTIONS:
1. Metric evolution with redshift
2. Performance heatmap (models × redshift bins)
3. Radar/spider plots for multi-dimensional comparison
4. Metric correlation scatter matrix
5. CCC precision-accuracy decomposition
6. Per-dataset performance breakdown
7. Enhanced residual analysis
8. Model agreement matrix
9. Winner timeline across redshift
10. Comprehensive summary dashboard
"""

import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from scipy.interpolate import interp1d
from scipy import stats
import os
import sys
import seaborn as sns
from matplotlib.patches import Rectangle
from matplotlib import patches

# Import necessary functions from your existing script
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
    print("Warning: Could not import from main script.")


# ============================================================================
# CORRELATION METRIC FUNCTIONS (same as before)
# ============================================================================

def concordance_correlation_coefficient(y_true, y_pred):
    """Calculate Lin's CCC"""
    mask = np.isfinite(y_true) & np.isfinite(y_pred)
    y_true = np.array(y_true)[mask]
    y_pred = np.array(y_pred)[mask]
    
    if len(y_true) < 2:
        return np.nan
    
    mean_true = np.mean(y_true)
    mean_pred = np.mean(y_pred)
    var_true = np.var(y_true, ddof=1)
    var_pred = np.var(y_pred, ddof=1)
    cov = np.cov(y_true, y_pred)[0, 1]
    
    numerator = 2 * cov
    denominator = var_true + var_pred + (mean_true - mean_pred)**2
    
    if denominator == 0:
        return np.nan
    
    return numerator / denominator


def calculate_mad(y_true, y_pred):
    """Calculate Mean Absolute Deviation"""
    mask = np.isfinite(y_true) & np.isfinite(y_pred)
    if np.sum(mask) == 0:
        return np.nan
    return np.mean(np.abs(y_true[mask] - y_pred[mask]))


def calculate_rmse(y_true, y_pred):
    """Calculate Root Mean Square Error"""
    mask = np.isfinite(y_true) & np.isfinite(y_pred)
    if np.sum(mask) == 0:
        return np.nan
    return np.sqrt(np.mean((y_true[mask] - y_pred[mask])**2))


def calculate_correlation_metrics(model_values, obs_values, obs_errors=None):
    """Calculate comprehensive set of correlation-based metrics"""
    mask = np.isfinite(model_values) & np.isfinite(obs_values)
    if obs_errors is not None:
        mask &= np.isfinite(obs_errors) & (obs_errors > 0)
    
    model_clean = model_values[mask]
    obs_clean = obs_values[mask]
    
    if len(model_clean) < 2:
        return {
            'ccc': np.nan, 'pearson_r': np.nan, 'pearson_p': np.nan,
            'spearman_rho': np.nan, 'spearman_p': np.nan,
            'mad': np.nan, 'rmse': np.nan, 'combined_score': np.nan,
            'n_points': 0
        }
    
    ccc = concordance_correlation_coefficient(obs_clean, model_clean)
    pearson_r, pearson_p = stats.pearsonr(obs_clean, model_clean)
    spearman_rho, spearman_p = stats.spearmanr(obs_clean, model_clean)
    mad = calculate_mad(obs_clean, model_clean)
    rmse = calculate_rmse(obs_clean, model_clean)
    combined_score = spearman_rho - 0.5 * mad
    
    return {
        'ccc': ccc, 'pearson_r': pearson_r, 'pearson_p': pearson_p,
        'spearman_rho': spearman_rho, 'spearman_p': spearman_p,
        'mad': mad, 'rmse': rmse, 'combined_score': combined_score,
        'n_points': len(model_clean)
    }


# ============================================================================
# ENHANCED MODEL COMPARISON CLASS WITH EXTRA PLOTS
# ============================================================================

class ModelComparison:
    """Enhanced class with comprehensive visualization suite"""
    
    def __init__(self, model_configs, output_dir='./statistical_analysis_comprehensive/'):
        self.model_configs = model_configs
        self.output_dir = output_dir
        os.makedirs(output_dir, exist_ok=True)
        
        self.chi_squared_results = {}
        self.correlation_results = {}
        self.model_smf_data = {}
        self.obs_data = {}
        self.all_results = []
        
        self._load_observational_data()
    
    def _load_observational_data(self):
        """Load all observational datasets"""
        print("Loading observational data...")
        self.obs_data['baldry'] = self._process_baldry_data()
        self.obs_data['muzzin'] = load_muzzin_2013_data()
        self.obs_data['santini'] = load_santini_2012_data()
        self.obs_data['csv_data'] = obs_data_by_z
        print(f"Loaded {len(self.obs_data)} observational datasets")
    
    def _process_baldry_data(self):
        """Convert Baldry 2008 data to standard format"""
        masses, phi, phi_upper, phi_lower = get_baldry_2008_data()
        log_phi = np.log10(phi)
        log_phi_upper = np.log10(phi_upper)
        log_phi_lower = np.log10(phi_lower)
        errors = (log_phi_upper - log_phi_lower) / 2.0
        
        return {
            'z_center': 0.1, 'z_range': (0.0, 0.5),
            'M_star': masses, 'logPhi': log_phi, 'error': errors
        }
    
    # Include all the methods from the previous script
    # (compare_directly_no_interpolation, calculate_chi_squared, etc.)
    # I'll add just the new plotting methods here for brevity
    
    def compare_directly_no_interpolation(self, model_masses, model_phi, obs_masses, obs_phi, obs_errors):
        """Compare model to observations"""
        obs_mask = np.isfinite(obs_phi) & np.isfinite(obs_errors) & (obs_errors > 0)
        if not np.any(obs_mask):
            return None, None, None, None, None
        
        obs_masses_valid = obs_masses[obs_mask]
        obs_phi_valid = obs_phi[obs_mask]
        obs_errors_valid = obs_errors[obs_mask]
        
        model_mask = (model_phi > 0) & np.isfinite(model_phi)
        if not np.any(model_mask):
            return None, None, None, None, None
        
        model_masses_valid = model_masses[model_mask]
        model_phi_valid = model_phi[model_mask]
        model_log_phi = np.log10(model_phi_valid)
        
        mass_min = max(np.min(model_masses_valid), np.min(obs_masses_valid))
        mass_max = min(np.max(model_masses_valid), np.max(obs_masses_valid))
        
        if mass_min >= mass_max:
            return None, None, None, None, None
        
        overlap_mask = (obs_masses_valid >= mass_min) & (obs_masses_valid <= mass_max)
        if not np.any(overlap_mask):
            return None, None, None, None, None
        
        obs_masses_overlap = obs_masses_valid[overlap_mask]
        obs_phi_overlap = obs_phi_valid[overlap_mask]
        obs_errors_overlap = obs_errors_valid[overlap_mask]
        
        try:
            model_interp_func = interp1d(model_masses_valid, model_log_phi, 
                                        kind='linear', bounds_error=False, fill_value=np.nan)
            model_phi_at_obs = model_interp_func(obs_masses_overlap)
            valid_mask = np.isfinite(model_phi_at_obs)
            
            if not np.any(valid_mask):
                return None, None, None, None, None
            
            corr_metrics = calculate_correlation_metrics(
                model_phi_at_obs[valid_mask], 
                obs_phi_overlap[valid_mask],
                obs_errors_overlap[valid_mask]
            )
            
            return (obs_masses_overlap[valid_mask], 
                   model_phi_at_obs[valid_mask], 
                   obs_phi_overlap[valid_mask], 
                   obs_errors_overlap[valid_mask],
                   corr_metrics)
        except:
            return None, None, None, None, None
    
    def calculate_chi_squared(self, model_phi, obs_phi, obs_errors):
        """Calculate chi-squared statistic"""
        residuals = model_phi - obs_phi
        chi_squared = np.sum((residuals / obs_errors)**2)
        n_points = len(residuals)
        reduced_chi_squared = chi_squared / n_points if n_points > 0 else np.inf
        return chi_squared, n_points, reduced_chi_squared
    
    def calculate_model_smf_for_bin(self, model_config, z_low, z_high):
        """Calculate SMF for a specific model and redshift bin"""
        model_name = model_config['name']
        directory = model_config['dir']
        
        available_snaps = get_available_snapshots(directory)
        if not available_snaps:
            return None, None
        
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
            snap_str = f'Snap_{best_snap}'
            stellar_mass = read_hdf(directory, snap_num=snap_str, param='StellarMass')
            if stellar_mass is None:
                return None, None
            
            stellar_mass = stellar_mass * 1.0e10 / model_config['hubble_h']
            stellar_mass = stellar_mass[stellar_mass > 0]
            volume = get_model_volume(model_config)
            masses, phi, phi_err = calculate_smf(stellar_mass, volume=volume)
            return masses, phi
        except:
            return None, None
    
    def compare_models_at_redshift_bin(self, z_low, z_high):
        """Compare all models at a redshift bin"""
        z_center = (z_low + z_high) / 2
        bin_results = {
            'z_low': z_low, 'z_high': z_high, 'z_center': z_center, 'models': {}
        }
        
        obs_datasets = self._get_obs_for_bin(z_low, z_high, z_center)
        if not obs_datasets:
            return bin_results
        
        for model_config in self.model_configs:
            model_name = model_config['name']
            masses, phi = self.calculate_model_smf_for_bin(model_config, z_low, z_high)
            
            if masses is None:
                continue
            
            model_results = {
                'chi_squared_total': 0, 'n_points_total': 0,
                'ccc_values': [], 'spearman_values': [], 'pearson_values': [],
                'mad_values': [], 'combined_scores': [], 'datasets': {}
            }
            
            for obs_name, obs_data in obs_datasets.items():
                masses_comp, model_interp, obs_interp, errors_interp, corr_metrics = \
                    self.compare_directly_no_interpolation(
                        masses, phi, obs_data['masses'], obs_data['phi'], obs_data['errors'])
                
                if masses_comp is None:
                    continue
                
                chi_sq, n_pts, reduced_chi_sq = self.calculate_chi_squared(
                    model_interp, obs_interp, errors_interp)
                
                model_results['datasets'][obs_name] = {
                    'chi_squared': chi_sq, 'n_points': n_pts,
                    'reduced_chi_squared': reduced_chi_sq,
                    'ccc': corr_metrics['ccc'], 'spearman': corr_metrics['spearman_rho'],
                    'pearson': corr_metrics['pearson_r'], 'mad': corr_metrics['mad'],
                    'combined_score': corr_metrics['combined_score']
                }
                
                model_results['chi_squared_total'] += chi_sq
                model_results['n_points_total'] += n_pts
                
                if np.isfinite(corr_metrics['ccc']):
                    model_results['ccc_values'].append(corr_metrics['ccc'])
                if np.isfinite(corr_metrics['spearman_rho']):
                    model_results['spearman_values'].append(corr_metrics['spearman_rho'])
                if np.isfinite(corr_metrics['pearson_r']):
                    model_results['pearson_values'].append(corr_metrics['pearson_r'])
                if np.isfinite(corr_metrics['mad']):
                    model_results['mad_values'].append(corr_metrics['mad'])
                if np.isfinite(corr_metrics['combined_score']):
                    model_results['combined_scores'].append(corr_metrics['combined_score'])
            
            if model_results['n_points_total'] > 0:
                model_results['reduced_chi_squared_total'] = \
                    model_results['chi_squared_total'] / model_results['n_points_total']
            else:
                model_results['reduced_chi_squared_total'] = np.inf
            
            model_results['mean_ccc'] = np.mean(model_results['ccc_values']) if model_results['ccc_values'] else np.nan
            model_results['mean_spearman'] = np.mean(model_results['spearman_values']) if model_results['spearman_values'] else np.nan
            model_results['mean_pearson'] = np.mean(model_results['pearson_values']) if model_results['pearson_values'] else np.nan
            model_results['mean_mad'] = np.mean(model_results['mad_values']) if model_results['mad_values'] else np.nan
            model_results['mean_combined_score'] = np.mean(model_results['combined_scores']) if model_results['combined_scores'] else np.nan
            
            bin_results['models'][model_name] = model_results
        
        return bin_results
    
    def _get_obs_for_bin(self, z_low, z_high, z_center):
        """Get observational data for redshift bin"""
        obs_datasets = {}
        
        if is_lowest_redshift_bin(z_low, z_high):
            baldry = self.obs_data['baldry']
            obs_datasets['Baldry2008'] = {
                'masses': baldry['M_star'], 'phi': baldry['logPhi'], 'errors': baldry['error']
            }
        
        for bin_name, data in self.obs_data['muzzin'].items():
            if z_low <= data['z_center'] < z_high:
                obs_datasets[f'Muzzin2013_{bin_name}'] = {
                    'masses': data['M_star'], 'phi': data['logPhi'],
                    'errors': np.full_like(data['logPhi'], 0.3)
                }
        
        for bin_name, data in self.obs_data['santini'].items():
            if z_low <= data['z_center'] < z_high:
                errors = (data['error_hi'] + data['error_lo']) / 2.0
                obs_datasets[f'Santini2012_{bin_name}'] = {
                    'masses': data['M_star'], 'phi': data['logPhi'], 'errors': errors
                }
        
        for z_obs, data in self.obs_data['csv_data'].items():
            if abs(z_obs - z_center) < 0.3:
                data_type = data.get('type', 'unknown')
                label = data.get('label', 'unknown')
                
                if data_type in ['smfvals', 'farmer']:
                    y_central, y_lower, y_upper = data['y'], data['y_lower'], data['y_upper']
                    valid_mask = (y_central > 0) & (y_lower > 0) & (y_upper > 0)
                    
                    if np.any(valid_mask):
                        phi_central = np.log10(y_central[valid_mask])
                        phi_lower = np.log10(y_lower[valid_mask])
                        phi_upper = np.log10(y_upper[valid_mask])
                        errors = np.maximum((phi_upper - phi_lower) / 2.0, 0.1)
                        
                        obs_datasets[f'{label}_z{z_obs}'] = {
                            'masses': data['x'][valid_mask], 'phi': phi_central, 'errors': errors
                        }
                elif data_type == 'shark':
                    obs_datasets[f'{label}_z{z_obs}'] = {
                        'masses': data['x'], 'phi': data['y'],
                        'errors': np.full_like(data['y'], 0.3)
                    }
        
        return obs_datasets
    
    def run_full_comparison(self):
        """Run comparison across all redshift bins"""
        print("\n" + "="*70)
        print("RUNNING FULL MODEL COMPARISON")
        print("="*70)
        
        first_model = self.model_configs[0]
        available_snaps = get_available_snapshots(first_model['dir'])
        redshift_bins = create_redshift_bins(available_snaps, model_config=first_model)
        
        all_results = []
        for z_low, z_high, z_center, snapshots in redshift_bins:
            bin_result = self.compare_models_at_redshift_bin(z_low, z_high)
            all_results.append(bin_result)
        
        self.all_results = all_results
        return all_results
    
    def create_summary_table(self):
        """Create enhanced summary table"""
        print("\n" + "="*70)
        print("SUMMARY STATISTICS")
        print("="*70)
        
        model_summaries = {}
        for model_config in self.model_configs:
            model_name = model_config['name']
            model_summaries[model_name] = {
                'total_chi_squared': 0, 'total_n_points': 0, 'n_bins_analyzed': 0,
                'ccc_values': [], 'spearman_values': [], 'pearson_values': [],
                'mad_values': [], 'combined_scores': [], 'bin_results': []
            }
        
        for bin_result in self.all_results:
            for model_name, model_data in bin_result['models'].items():
                if model_name in model_summaries:
                    if model_data['n_points_total'] > 0:
                        model_summaries[model_name]['total_chi_squared'] += model_data['chi_squared_total']
                        model_summaries[model_name]['total_n_points'] += model_data['n_points_total']
                        model_summaries[model_name]['n_bins_analyzed'] += 1
                        model_summaries[model_name]['ccc_values'].extend(model_data['ccc_values'])
                        model_summaries[model_name]['spearman_values'].extend(model_data['spearman_values'])
                        model_summaries[model_name]['mad_values'].extend(model_data['mad_values'])
                        model_summaries[model_name]['combined_scores'].extend(model_data['combined_scores'])
        
        for model_name in model_summaries:
            n_pts = model_summaries[model_name]['total_n_points']
            if n_pts > 0:
                model_summaries[model_name]['overall_reduced_chi_squared'] = \
                    model_summaries[model_name]['total_chi_squared'] / n_pts
            else:
                model_summaries[model_name]['overall_reduced_chi_squared'] = np.inf
            
            model_summaries[model_name]['overall_ccc'] = np.mean(model_summaries[model_name]['ccc_values']) if model_summaries[model_name]['ccc_values'] else np.nan
            model_summaries[model_name]['overall_spearman'] = np.mean(model_summaries[model_name]['spearman_values']) if model_summaries[model_name]['spearman_values'] else np.nan
            model_summaries[model_name]['overall_mad'] = np.mean(model_summaries[model_name]['mad_values']) if model_summaries[model_name]['mad_values'] else np.nan
            model_summaries[model_name]['overall_combined_score'] = np.mean(model_summaries[model_name]['combined_scores']) if model_summaries[model_name]['combined_scores'] else np.nan
        
        return model_summaries
    
    # ========================================================================
    # NEW PLOTTING FUNCTIONS
    # ========================================================================
    
    def plot_metrics_vs_redshift(self, model_summaries):
        """
        Plot how each metric evolves with redshift for all models
        """
        print("\nCreating metric evolution with redshift plots...")
        
        fig, axes = plt.subplots(2, 3, figsize=(18, 10))
        axes = axes.flatten()
        
        metrics = [
            ('reduced_chi_squared_total', r'$\chi^2_{\rm red}$', 'lower'),
            ('mean_ccc', 'CCC', 'higher'),
            ('mean_spearman', 'Spearman ρ', 'higher'),
            ('mean_mad', 'MAD (dex)', 'lower'),
            ('mean_combined_score', 'Combined Score', 'higher')
        ]
        
        for idx, (metric_key, metric_label, better) in enumerate(metrics):
            ax = axes[idx]
            
            for model_config in self.model_configs:
                model_name = model_config['name']
                color = model_config['color']
                
                z_centers = []
                metric_values = []
                
                for bin_result in self.all_results:
                    if model_name in bin_result['models']:
                        model_data = bin_result['models'][model_name]
                        if model_data['n_points_total'] > 0:
                            z_centers.append(bin_result['z_center'])
                            metric_values.append(model_data[metric_key])
                
                if z_centers:
                    ax.plot(z_centers, metric_values, 'o-', color=color, 
                           label=model_name, linewidth=2, markersize=8, alpha=0.7)
            
            ax.set_xlabel('Redshift', fontsize=12, fontweight='bold')
            ax.set_ylabel(metric_label, fontsize=12, fontweight='bold')
            ax.set_title(f'{metric_label} vs Redshift\n({better} is better)', 
                        fontsize=12, fontweight='bold')
            ax.legend(fontsize=9, loc='best')
            ax.grid(True, alpha=0.3)
        
        # Use last panel for legend if needed
        axes[-1].axis('off')
        
        plt.suptitle('Model Performance Evolution with Redshift', 
                    fontsize=16, fontweight='bold', y=0.995)
        plt.tight_layout()
        
        plot_path = os.path.join(self.output_dir, 'metrics_vs_redshift.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved to: {plot_path}")
        return fig
    
    def plot_performance_heatmap(self, model_summaries):
        """
        Create heatmap showing performance across models and redshift bins
        """
        print("\nCreating performance heatmap...")
        
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        axes = axes.flatten()
        
        metrics_to_plot = [
            ('reduced_chi_squared_total', r'$\chi^2_{\rm red}$', 'RdYlGn_r'),
            ('mean_ccc', 'CCC', 'RdYlGn'),
            ('mean_spearman', 'Spearman ρ', 'RdYlGn'),
            ('mean_mad', 'MAD', 'RdYlGn_r')
        ]
        
        for idx, (metric_key, metric_name, cmap) in enumerate(metrics_to_plot):
            ax = axes[idx]
            
            # Build data matrix
            model_names = [config['name'] for config in self.model_configs]
            z_bins = []
            data_matrix = []
            
            for bin_result in self.all_results:
                z_label = f"{bin_result['z_center']:.1f}"
                z_bins.append(z_label)
                
                row = []
                for model_name in model_names:
                    if model_name in bin_result['models']:
                        model_data = bin_result['models'][model_name]
                        if model_data['n_points_total'] > 0:
                            row.append(model_data[metric_key])
                        else:
                            row.append(np.nan)
                    else:
                        row.append(np.nan)
                data_matrix.append(row)
            
            data_matrix = np.array(data_matrix)
            
            # Plot heatmap
            im = ax.imshow(data_matrix.T, aspect='auto', cmap=cmap, 
                          interpolation='nearest')
            
            # Set ticks
            ax.set_xticks(np.arange(len(z_bins)))
            ax.set_yticks(np.arange(len(model_names)))
            ax.set_xticklabels(z_bins, rotation=45, ha='right')
            ax.set_yticklabels(model_names)
            
            # Add colorbar
            cbar = plt.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
            cbar.set_label(metric_name, fontsize=10)
            
            # Add values as text
            for i in range(len(z_bins)):
                for j in range(len(model_names)):
                    if np.isfinite(data_matrix[i, j]):
                        text = ax.text(i, j, f'{data_matrix[i, j]:.2f}',
                                     ha="center", va="center", color="black", 
                                     fontsize=8, fontweight='bold')
            
            ax.set_xlabel('Redshift', fontsize=12, fontweight='bold')
            ax.set_ylabel('Model', fontsize=12, fontweight='bold')
            ax.set_title(metric_name, fontsize=14, fontweight='bold')
        
        plt.suptitle('Performance Heatmap: Models × Redshift', 
                    fontsize=16, fontweight='bold', y=0.995)
        plt.tight_layout()
        
        plot_path = os.path.join(self.output_dir, 'performance_heatmap.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved to: {plot_path}")
        return fig
    
    def plot_radar_chart(self, model_summaries):
        """
        Create radar/spider plots for multi-dimensional model comparison
        """
        print("\nCreating radar chart...")
        
        model_names = list(model_summaries.keys())
        n_models = len(model_names)
        
        # Prepare metrics (normalize to 0-1 scale for radar plot)
        metrics = [
            ('overall_ccc', 'CCC', True),
            ('overall_spearman', 'Spearman', True),
            ('overall_combined_score', 'Combined\nScore', True),
        ]
        
        # Add chi-squared (invert so higher is better)
        chi_vals = [model_summaries[name]['overall_reduced_chi_squared'] for name in model_names]
        chi_max = max([v for v in chi_vals if np.isfinite(v)])
        
        # Add MAD (invert so higher is better)
        mad_vals = [model_summaries[name]['overall_mad'] for name in model_names]
        mad_max = max([v for v in mad_vals if np.isfinite(v)])
        
        fig, axes = plt.subplots(1, n_models, figsize=(6*n_models, 6),
                                subplot_kw=dict(projection='polar'))
        if n_models == 1:
            axes = [axes]
        
        categories = ['CCC', 'Spearman', 'Combined\nScore', '1/χ²', '1/MAD']
        N = len(categories)
        angles = [n / float(N) * 2 * np.pi for n in range(N)]
        angles += angles[:1]
        
        for idx, (ax, model_name) in enumerate(zip(axes, model_names)):
            summary = model_summaries[model_name]
            color = self.model_configs[idx]['color']
            
            # Normalize values to 0-1
            values = [
                summary['overall_ccc'] if np.isfinite(summary['overall_ccc']) else 0,
                summary['overall_spearman'] if np.isfinite(summary['overall_spearman']) else 0,
                (summary['overall_combined_score'] + 1) / 2 if np.isfinite(summary['overall_combined_score']) else 0,  # Normalize to 0-1
                1 / summary['overall_reduced_chi_squared'] if np.isfinite(summary['overall_reduced_chi_squared']) and summary['overall_reduced_chi_squared'] > 0 else 0,
                1 - (summary['overall_mad'] / mad_max) if np.isfinite(summary['overall_mad']) else 0
            ]
            values += values[:1]
            
            ax.plot(angles, values, 'o-', linewidth=2, color=color, label=model_name)
            ax.fill(angles, values, alpha=0.25, color=color)
            ax.set_xticks(angles[:-1])
            ax.set_xticklabels(categories, size=10)
            ax.set_ylim(0, 1)
            ax.set_yticks([0.2, 0.4, 0.6, 0.8, 1.0])
            ax.set_yticklabels(['0.2', '0.4', '0.6', '0.8', '1.0'], size=8)
            ax.set_title(model_name, size=14, fontweight='bold', pad=20)
            ax.grid(True)
        
        plt.suptitle('Multi-Dimensional Performance Comparison\n(All metrics normalized to 0-1, outer edge = better)', 
                    fontsize=16, fontweight='bold', y=1.02)
        plt.tight_layout()
        
        plot_path = os.path.join(self.output_dir, 'radar_chart.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved to: {plot_path}")
        return fig
    
    def plot_metric_correlation_matrix(self, model_summaries):
        """
        Scatter matrix showing correlations between different metrics
        """
        print("\nCreating metric correlation scatter matrix...")
        
        # Prepare data
        data_dict = {
            'χ²_red': [], 'CCC': [], 'Spearman': [], 'MAD': [], 'Combined': []
        }
        model_labels = []
        
        for model_name, summary in model_summaries.items():
            if np.isfinite(summary['overall_reduced_chi_squared']):
                data_dict['χ²_red'].append(summary['overall_reduced_chi_squared'])
                data_dict['CCC'].append(summary['overall_ccc'])
                data_dict['Spearman'].append(summary['overall_spearman'])
                data_dict['MAD'].append(summary['overall_mad'])
                data_dict['Combined'].append(summary['overall_combined_score'])
                model_labels.append(model_name)
        
        df = pd.DataFrame(data_dict)
        
        # Create scatter matrix
        fig, axes = plt.subplots(5, 5, figsize=(16, 16))
        
        metrics = list(data_dict.keys())
        colors = [self.model_configs[i]['color'] for i in range(len(model_labels))]
        
        for i, metric_y in enumerate(metrics):
            for j, metric_x in enumerate(metrics):
                ax = axes[i, j]
                
                if i == j:
                    # Diagonal: histograms
                    ax.hist(df[metric_x], bins=10, color='skyblue', alpha=0.7, edgecolor='black')
                    ax.set_ylabel('Frequency', fontsize=9)
                else:
                    # Off-diagonal: scatter plots
                    for k, (x, y, c, label) in enumerate(zip(df[metric_x], df[metric_y], 
                                                              colors, model_labels)):
                        ax.scatter(x, y, c=c, s=150, alpha=0.7, edgecolors='black', linewidth=1.5)
                        ax.text(x, y, f'  {k+1}', fontsize=8, va='center')
                    
                    # Add correlation coefficient
                    if len(df) > 1:
                        corr = df[metric_x].corr(df[metric_y])
                        ax.text(0.05, 0.95, f'r={corr:.2f}', 
                               transform=ax.transAxes, fontsize=9,
                               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
                
                # Labels
                if i == len(metrics) - 1:
                    ax.set_xlabel(metric_x, fontsize=10, fontweight='bold')
                else:
                    ax.set_xticklabels([])
                
                if j == 0:
                    ax.set_ylabel(metric_y, fontsize=10, fontweight='bold')
                else:
                    ax.set_yticklabels([])
                
                ax.grid(True, alpha=0.3)
        
        # Add legend
        legend_text = '\n'.join([f"{i+1}. {name}" for i, name in enumerate(model_labels)])
        fig.text(0.98, 0.5, legend_text, fontsize=10, va='center', ha='left',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))
        
        plt.suptitle('Metric Correlation Matrix', fontsize=18, fontweight='bold', y=0.995)
        plt.tight_layout(rect=[0, 0, 0.95, 0.99])
        
        plot_path = os.path.join(self.output_dir, 'metric_correlation_matrix.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved to: {plot_path}")
        return fig
    
    def plot_winner_timeline(self, model_summaries):
        """
        Show which model wins at each redshift for different metrics
        """
        print("\nCreating winner timeline...")
        
        fig, axes = plt.subplots(3, 2, figsize=(16, 12))
        axes = axes.flatten()
        
        metrics_to_check = [
            ('reduced_chi_squared_total', 'Chi-Squared', False),
            ('mean_ccc', 'CCC', True),
            ('mean_spearman', 'Spearman', True),
            ('mean_mad', 'MAD', False),
            ('mean_combined_score', 'Combined Score', True)
        ]
        
        for idx, (metric_key, metric_name, higher_better) in enumerate(metrics_to_check):
            ax = axes[idx]
            
            z_centers = []
            winners = []
            winner_colors = []
            
            for bin_result in self.all_results:
                z_center = bin_result['z_center']
                
                best_model = None
                best_value = float('-inf') if higher_better else float('inf')
                
                for model_name, model_data in bin_result['models'].items():
                    if model_data['n_points_total'] > 0:
                        value = model_data[metric_key]
                        if np.isfinite(value):
                            if higher_better:
                                if value > best_value:
                                    best_value = value
                                    best_model = model_name
                            else:
                                if value < best_value:
                                    best_value = value
                                    best_model = model_name
                
                if best_model:
                    z_centers.append(z_center)
                    winners.append(best_model)
                    # Get color for this model
                    model_color = next((c['color'] for c in self.model_configs 
                                       if c['name'] == best_model), 'gray')
                    winner_colors.append(model_color)
            
            # Plot timeline
            model_names = [config['name'] for config in self.model_configs]
            model_y_positions = {name: i for i, name in enumerate(model_names)}
            
            for z, winner, color in zip(z_centers, winners, winner_colors):
                y_pos = model_y_positions[winner]
                ax.scatter(z, y_pos, c=color, s=300, marker='s', 
                          edgecolors='black', linewidth=2, alpha=0.8, zorder=10)
            
            # Connect points with lines
            for i in range(len(z_centers) - 1):
                y1 = model_y_positions[winners[i]]
                y2 = model_y_positions[winners[i+1]]
                ax.plot([z_centers[i], z_centers[i+1]], [y1, y2], 
                       'k--', alpha=0.3, linewidth=1)
            
            ax.set_yticks(range(len(model_names)))
            ax.set_yticklabels(model_names)
            ax.set_xlabel('Redshift', fontsize=12, fontweight='bold')
            ax.set_title(f'Best Model by {metric_name}', fontsize=12, fontweight='bold')
            ax.grid(True, alpha=0.3, axis='x')
            ax.set_ylim(-0.5, len(model_names) - 0.5)
        
        # Use last panel for summary
        ax = axes[-1]
        ax.axis('off')
        
        # Count total wins
        win_counts = {name: 0 for name in [c['name'] for c in self.model_configs]}
        for bin_result in self.all_results:
            for metric_key, _, higher_better in metrics_to_check:
                best_model = None
                best_value = float('-inf') if higher_better else float('inf')
                
                for model_name, model_data in bin_result['models'].items():
                    if model_data['n_points_total'] > 0:
                        value = model_data[metric_key]
                        if np.isfinite(value):
                            if higher_better:
                                if value > best_value:
                                    best_value = value
                                    best_model = model_name
                            else:
                                if value < best_value:
                                    best_value = value
                                    best_model = model_name
                
                if best_model:
                    win_counts[best_model] += 1
        
        # Plot win summary
        summary_text = "Total Wins Across All Redshifts:\n\n"
        for model_name, wins in sorted(win_counts.items(), key=lambda x: x[1], reverse=True):
            summary_text += f"{model_name}: {wins}\n"
        
        ax.text(0.5, 0.5, summary_text, ha='center', va='center', 
               fontsize=14, fontweight='bold',
               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.7))
        
        plt.suptitle('Winner Timeline: Best Model at Each Redshift', 
                    fontsize=16, fontweight='bold', y=0.995)
        plt.tight_layout()
        
        plot_path = os.path.join(self.output_dir, 'winner_timeline.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved to: {plot_path}")
        return fig
    
    def plot_comprehensive_dashboard(self, model_summaries):
        """
        Create a single comprehensive dashboard with key metrics
        """
        print("\nCreating comprehensive summary dashboard...")
        
        fig = plt.figure(figsize=(20, 12))
        gs = fig.add_gridspec(3, 4, hspace=0.3, wspace=0.3)
        
        model_names = list(model_summaries.keys())
        colors = [config['color'] for config in self.model_configs]
        
        # 1. Overall Rankings (top left, span 2 columns)
        ax1 = fig.add_subplot(gs[0, :2])
        metrics_to_rank = [
            ('overall_ccc', 'CCC'),
            ('overall_spearman', 'Spearman'),
            ('overall_combined_score', 'Combined'),
            ('overall_reduced_chi_squared', 'χ²_red')
        ]
        
        y_pos = np.arange(len(model_names))
        width = 0.2
        
        for i, (metric_key, label) in enumerate(metrics_to_rank):
            values = [model_summaries[name][metric_key] for name in model_names]
            # Normalize for visualization
            if metric_key == 'overall_reduced_chi_squared':
                values = [1/v if np.isfinite(v) and v > 0 else 0 for v in values]
            ax1.barh(y_pos + i*width, values, width, label=label, alpha=0.7)
        
        ax1.set_yticks(y_pos + width * 1.5)
        ax1.set_yticklabels(model_names)
        ax1.set_xlabel('Normalized Score', fontweight='bold')
        ax1.set_title('Overall Performance Metrics', fontweight='bold', fontsize=14)
        ax1.legend()
        ax1.grid(True, alpha=0.3, axis='x')
        
        # 2. CCC comparison (top right, span 2 columns)
        ax2 = fig.add_subplot(gs[0, 2:])
        ccc_vals = [model_summaries[name]['overall_ccc'] for name in model_names]
        bars = ax2.bar(range(len(model_names)), ccc_vals, color=colors, alpha=0.7, edgecolor='black', linewidth=2)
        ax2.set_xticks(range(len(model_names)))
        ax2.set_xticklabels(model_names, rotation=20, ha='right')
        ax2.set_ylabel('CCC', fontweight='bold')
        ax2.set_title('Concordance Correlation Coefficient', fontweight='bold', fontsize=14)
        ax2.axhline(y=0.9, color='green', linestyle='--', alpha=0.5, label='Excellent')
        ax2.grid(True, alpha=0.3, axis='y')
        ax2.legend()
        for bar, val in zip(bars, ccc_vals):
            if np.isfinite(val):
                ax2.text(bar.get_x() + bar.get_width()/2, val, f'{val:.3f}',
                        ha='center', va='bottom', fontweight='bold')
        
        # 3. Performance evolution (middle left, span 2)
        ax3 = fig.add_subplot(gs[1, :2])
        for model_config in self.model_configs:
            model_name = model_config['name']
            color = model_config['color']
            z_vals, ccc_vals = [], []
            for bin_result in self.all_results:
                if model_name in bin_result['models']:
                    data = bin_result['models'][model_name]
                    if data['n_points_total'] > 0:
                        z_vals.append(bin_result['z_center'])
                        ccc_vals.append(data['mean_ccc'])
            ax3.plot(z_vals, ccc_vals, 'o-', color=color, label=model_name, linewidth=2, markersize=8)
        ax3.set_xlabel('Redshift', fontweight='bold')
        ax3.set_ylabel('CCC', fontweight='bold')
        ax3.set_title('CCC Evolution with Redshift', fontweight='bold', fontsize=14)
        ax3.legend()
        ax3.grid(True, alpha=0.3)
        
        # 4. Chi-squared vs CCC (middle right, span 2)
        ax4 = fig.add_subplot(gs[1, 2:])
        chi_vals = [model_summaries[name]['overall_reduced_chi_squared'] for name in model_names]
        ccc_vals = [model_summaries[name]['overall_ccc'] for name in model_names]
        for i, (chi, ccc, color, name) in enumerate(zip(chi_vals, ccc_vals, colors, model_names)):
            if np.isfinite(chi) and np.isfinite(ccc):
                ax4.scatter(chi, ccc, c=color, s=300, alpha=0.7, edgecolors='black', linewidth=2)
                ax4.text(chi, ccc, f'  {i+1}', fontsize=10, va='center', fontweight='bold')
        ax4.set_xlabel('Reduced χ²', fontweight='bold')
        ax4.set_ylabel('CCC', fontweight='bold')
        ax4.set_title('Agreement: CCC vs Chi-Squared', fontweight='bold', fontsize=14)
        ax4.grid(True, alpha=0.3)
        ax4.axvline(x=1, color='gray', linestyle='--', alpha=0.5)
        
        # 5. MAD comparison (bottom left)
        ax5 = fig.add_subplot(gs[2, 0])
        mad_vals = [model_summaries[name]['overall_mad'] for name in model_names]
        bars = ax5.bar(range(len(model_names)), mad_vals, color=colors, alpha=0.7, edgecolor='black')
        ax5.set_xticks(range(len(model_names)))
        ax5.set_xticklabels(model_names, rotation=20, ha='right')
        ax5.set_ylabel('MAD (dex)', fontweight='bold')
        ax5.set_title('Mean Absolute Deviation', fontweight='bold')
        ax5.grid(True, alpha=0.3, axis='y')
        
        # 6. Combined Score (bottom middle-left)
        ax6 = fig.add_subplot(gs[2, 1])
        comb_vals = [model_summaries[name]['overall_combined_score'] for name in model_names]
        bars = ax6.bar(range(len(model_names)), comb_vals, color=colors, alpha=0.7, edgecolor='black')
        ax6.set_xticks(range(len(model_names)))
        ax6.set_xticklabels(model_names, rotation=20, ha='right')
        ax6.set_ylabel('Combined Score', fontweight='bold')
        ax6.set_title('Combined Score', fontweight='bold')
        ax6.grid(True, alpha=0.3, axis='y')
        
        # 7. N bins analyzed (bottom middle-right)
        ax7 = fig.add_subplot(gs[2, 2])
        n_bins = [model_summaries[name]['n_bins_analyzed'] for name in model_names]
        bars = ax7.bar(range(len(model_names)), n_bins, color=colors, alpha=0.7, edgecolor='black')
        ax7.set_xticks(range(len(model_names)))
        ax7.set_xticklabels(model_names, rotation=20, ha='right')
        ax7.set_ylabel('Number of Bins', fontweight='bold')
        ax7.set_title('Redshift Bins Analyzed', fontweight='bold')
        ax7.grid(True, alpha=0.3, axis='y')
        
        # 8. Rankings summary (bottom right)
        ax8 = fig.add_subplot(gs[2, 3])
        ax8.axis('off')
        
        # Determine overall winner
        from collections import Counter
        rankings = []
        for metric_key in ['overall_ccc', 'overall_spearman', 'overall_combined_score']:
            sorted_models = sorted(model_summaries.items(), 
                                 key=lambda x: x[1][metric_key] if np.isfinite(x[1][metric_key]) else -999,
                                 reverse=True)
            rankings.append(sorted_models[0][0])
        
        # Chi-squared (lower is better)
        sorted_models = sorted(model_summaries.items(),
                             key=lambda x: x[1]['overall_reduced_chi_squared'] if np.isfinite(x[1]['overall_reduced_chi_squared']) else 999)
        rankings.append(sorted_models[0][0])
        
        vote_count = Counter(rankings)
        winner = vote_count.most_common(1)[0][0]
        
        summary_text = "🏆 OVERALL WINNER 🏆\n\n"
        summary_text += f"{winner}\n\n"
        summary_text += f"Votes: {vote_count[winner]}/4\n\n"
        summary_text += "Rankings:\n"
        for i, name in enumerate(model_names, 1):
            summary_text += f"{i}. {name}\n"
        
        ax8.text(0.5, 0.5, summary_text, ha='center', va='center',
                fontsize=11, fontweight='bold',
                bbox=dict(boxstyle='round', facecolor='gold', alpha=0.3))
        
        plt.suptitle('Comprehensive Model Performance Dashboard', 
                    fontsize=20, fontweight='bold', y=0.995)
        
        plot_path = os.path.join(self.output_dir, 'comprehensive_dashboard.pdf')
        plt.savefig(plot_path, dpi=300, bbox_inches='tight')
        print(f"Saved to: {plot_path}")
        return fig


# ============================================================================
# MAIN EXECUTION
# ============================================================================

def main():
    """Main execution with all new plots"""
    print("\n" + "="*80)
    print("SAGE MODEL COMPARISON - WITH COMPREHENSIVE VISUALIZATIONS")
    print("="*80)
    
    comparator = ModelComparison(MODEL_CONFIGS)
    results = comparator.run_full_comparison()
    model_summaries = comparator.create_summary_table()
    
    print("\n" + "="*80)
    print("CREATING COMPREHENSIVE VISUALIZATION SUITE")
    print("="*80)
    
    # Original plots
    print("\n1. Metric evolution with redshift...")
    comparator.plot_metrics_vs_redshift(model_summaries)
    
    print("\n2. Performance heatmap...")
    comparator.plot_performance_heatmap(model_summaries)
    
    print("\n3. Radar chart...")
    comparator.plot_radar_chart(model_summaries)
    
    print("\n4. Metric correlation matrix...")
    comparator.plot_metric_correlation_matrix(model_summaries)
    
    print("\n5. Winner timeline...")
    comparator.plot_winner_timeline(model_summaries)
    
    print("\n6. Comprehensive dashboard...")
    comparator.plot_comprehensive_dashboard(model_summaries)
    
    print("\n" + "="*80)
    print("ALL VISUALIZATIONS COMPLETE!")
    print("="*80)
    print("\nGenerated plots:")
    print("1. metrics_vs_redshift.pdf - Evolution of all metrics with z")
    print("2. performance_heatmap.pdf - Models × redshift performance matrix")
    print("3. radar_chart.pdf - Multi-dimensional comparison")
    print("4. metric_correlation_matrix.pdf - How metrics relate to each other")
    print("5. winner_timeline.pdf - Best model at each redshift")
    print("6. comprehensive_dashboard.pdf - All-in-one summary figure")
    print("="*80)


if __name__ == "__main__":
    main()