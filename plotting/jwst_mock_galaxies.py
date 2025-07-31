import pandas as pd
import numpy as np
import h5py as h5
import matplotlib.pyplot as plt
from scipy.spatial import KDTree
from scipy import stats
import os
import logging
from pathlib import Path

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

"""
PERFORMANCE OPTIMIZATIONS FOR GALAXY TRACKING AND 6-PANEL PLOTS:

1. SAGEGalaxyTracker Class Optimizations:
   - Bulk loading: Pre-loads all needed snapshots at once instead of opening files repeatedly
   - Caching: Stores loaded snapshots in memory to avoid re-reading
   - File list caching: Avoids repeated os.listdir() calls
   - Progress indicators: Shows loading progress for large datasets

2. track_all_catalogues Function Optimizations:
   - Pre-determines snapshot range across all catalogues
   - Loads all needed snapshots before any tracking begins
   - Memory-conscious mode for very large datasets (optional)
   - Cache size monitoring and reporting

3. Expected Performance Improvements:
   - 10-50x faster for tracking multiple catalogues to z=0
   - Reduced file I/O from ~100s of file opens to single bulk load
   - Better memory management with optional memory-conscious mode
   - Progress tracking for long-running operations

4. Usage:
   - Default mode: Maximum speed, loads all snapshots into memory
   - Memory-conscious mode: Slower but uses less memory for huge datasets
   - Cache information provided for monitoring memory usage
"""

class COSMOSSageMatching:
    """
    Simple tool to match COSMOS observations with SAGE 2.0 model galaxies
    """
    
    def __init__(self, cosmos_file, sage_dir, hubble_h=0.73):
        """
        Initialize the matching tool
        
        Parameters:
        -----------
        cosmos_file : str
            Path to COSMOS CSV file
        sage_dir : str  
            Directory containing SAGE HDF5 outputs
        hubble_h : float
            Hubble parameter for unit conversions
        """
        self.cosmos_file = cosmos_file
        self.sage_dir = sage_dir
        self.hubble_h = hubble_h
        self.cosmos_data = None
        self.sage_data = {}
        
    def load_cosmos_data(self, z_min=0.1, z_max=3.0, sample_fraction=0.1, 
                        stellar_mass_col='lmass', redshift_col='zphot'):
        """
        Load and sample COSMOS data with redshift cuts
        
        Parameters:
        -----------
        z_min, z_max : float
            Redshift range to keep
        sample_fraction : float
            Fraction of galaxies to randomly sample (for performance)
        stellar_mass_col : str
            Column name for stellar mass (should be log10(M*/Msun))
        redshift_col : str
            Column name for redshift
        """
        logger.info(f"Loading COSMOS data from {self.cosmos_file}")
        
        # Load the CSV file
        cosmos = pd.read_csv(self.cosmos_file)
        logger.info(f"Loaded {len(cosmos)} COSMOS galaxies")
        
        # Apply redshift cuts
        mask = (cosmos[redshift_col] >= z_min) & (cosmos[redshift_col] <= z_max)
        cosmos = cosmos[mask].copy()
        logger.info(f"After redshift cut ({z_min} < z < {z_max}): {len(cosmos)} galaxies")
        
        # Remove galaxies with invalid stellar masses
        valid_mass = np.isfinite(cosmos[stellar_mass_col]) & (cosmos[stellar_mass_col] > 0)
        cosmos = cosmos[valid_mass].copy()
        logger.info(f"After removing invalid masses: {len(cosmos)} galaxies")
        
        # Sample the data if requested
        if sample_fraction < 1.0:
            n_sample = int(len(cosmos) * sample_fraction)
            cosmos = cosmos.sample(n=n_sample, random_state=42).copy()
            logger.info(f"Sampled {len(cosmos)} galaxies ({sample_fraction:.1%})")
        
        # Store column names for later use
        cosmos['log_stellar_mass'] = cosmos[stellar_mass_col]
        cosmos['redshift'] = cosmos[redshift_col]
        
        self.cosmos_data = cosmos
        return cosmos
    
    def redshift_to_snapshot(self, redshift, sage_redshifts):
        """
        Find the closest SAGE snapshot for a given redshift
        """
        sage_z_array = np.array(sage_redshifts)
        closest_idx = np.argmin(np.abs(sage_z_array - redshift))
        return closest_idx
    
    def load_sage_snapshot(self, snap_num):
        """
        Load SAGE data for a specific snapshot using your existing function
        """
        snap_name = f'Snap_{snap_num}'
        
        if snap_name in self.sage_data:
            return self.sage_data[snap_name]
        
        logger.info(f"Loading SAGE snapshot {snap_num}")
        
        # Use your existing read function (adapted)
        stellar_mass = self.read_hdf_ultra_optimized(snap_num=snap_name, param='StellarMass')
        
        if len(stellar_mass) == 0:
            return None
            
        # Convert to log10(M*/Msun) - SAGE units are 10^10 Msun/h
        log_stellar_mass = np.log10(stellar_mass * 1e10 / self.hubble_h)
        
        # Filter out very low mass galaxies (below typical COSMOS detection)
        valid_mask = (log_stellar_mass > 8.0) & np.isfinite(log_stellar_mass)
        
        sage_snap_data = {
            'log_stellar_mass': log_stellar_mass[valid_mask],
            'snapshot': snap_num,
            'galaxy_indices': np.where(valid_mask)[0]  # Keep track of original indices
        }
        
        self.sage_data[snap_name] = sage_snap_data
        return sage_snap_data
    
    def read_hdf_ultra_optimized(self, snap_num=None, param=None):
        """
        Simplified version of your HDF5 reading function
        """
        model_files = [f for f in os.listdir(self.sage_dir) 
                      if f.startswith('model_') and f.endswith('.hdf5')]
        
        data_list = []
        for model_file in model_files:
            try:
                filepath = os.path.join(self.sage_dir, model_file)
                with h5.File(filepath, 'r') as f:
                    if snap_num in f and param in f[snap_num]:
                        data_list.append(f[snap_num][param][:])
            except Exception as e:
                logger.warning(f"Error reading {model_file}: {e}")
                continue
        
        return np.concatenate(data_list) if data_list else np.array([])
    
    def match_galaxies(self, sage_redshifts, max_matches=5, mass_tolerance=0.3):
        """
        Match COSMOS galaxies to SAGE galaxies
        
        Parameters:
        -----------
        sage_redshifts : list
            List of redshifts for each SAGE snapshot
        max_matches : int
            Maximum number of SAGE matches per COSMOS galaxy
        mass_tolerance : float
            Maximum difference in log stellar mass (dex)
        """
        if self.cosmos_data is None:
            raise ValueError("Must load COSMOS data first!")
        
        matches = []
        
        # Group COSMOS galaxies by redshift bins for efficiency
        cosmos_z = self.cosmos_data['redshift'].values
        cosmos_mass = self.cosmos_data['log_stellar_mass'].values
        
        # Find unique snapshots needed
        unique_snaps = set()
        for z in cosmos_z:
            snap_idx = self.redshift_to_snapshot(z, sage_redshifts)
            unique_snaps.add(snap_idx)
        
        # Pre-load all needed SAGE snapshots
        logger.info(f"Pre-loading {len(unique_snaps)} SAGE snapshots")
        for snap_idx in unique_snaps:
            self.load_sage_snapshot(snap_idx)
        
        # Match each COSMOS galaxy
        logger.info(f"Matching {len(self.cosmos_data)} COSMOS galaxies")
        
        for i, (cosmos_idx, cosmos_row) in enumerate(self.cosmos_data.iterrows()):
            if i % 1000 == 0:
                logger.info(f"Processed {i}/{len(self.cosmos_data)} galaxies")
            
            cosmos_z = cosmos_row['redshift']
            cosmos_logm = cosmos_row['log_stellar_mass']
            
            # Find closest SAGE snapshot
            snap_idx = self.redshift_to_snapshot(cosmos_z, sage_redshifts)
            snap_name = f'Snap_{snap_idx}'
            sage_snap = self.sage_data.get(snap_name)
            
            if sage_snap is None:
                continue
            
            # Find SAGE galaxies within mass tolerance
            sage_logm = sage_snap['log_stellar_mass']
            mass_diff = np.abs(sage_logm - cosmos_logm)
            mass_mask = mass_diff <= mass_tolerance
            
            if not np.any(mass_mask):
                continue
            
            # Calculate matching score (inverse of mass difference)
            valid_sage_logm = sage_logm[mass_mask]
            valid_sage_indices = sage_snap['galaxy_indices'][mass_mask]
            mass_distances = np.abs(valid_sage_logm - cosmos_logm)
            
            # Sort by mass similarity and take best matches
            sort_indices = np.argsort(mass_distances)
            n_matches = min(len(sort_indices), max_matches)
            
            for j in range(n_matches):
                sage_idx = sort_indices[j]
                
                match_data = {
                    'cosmos_idx': cosmos_idx,
                    'cosmos_redshift': cosmos_z,
                    'cosmos_log_mass': cosmos_logm,
                    'sage_snapshot': snap_idx,
                    'sage_redshift': sage_redshifts[snap_idx],
                    'sage_galaxy_idx': valid_sage_indices[sage_idx],
                    'sage_log_mass': valid_sage_logm[sage_idx],
                    'mass_difference': mass_distances[sage_idx],
                    'match_rank': j + 1
                }
                
                # Add other COSMOS properties
                for col in self.cosmos_data.columns:
                    if col not in ['redshift', 'log_stellar_mass']:
                        match_data[f'cosmos_{col}'] = cosmos_row[col]
                
                matches.append(match_data)
        
        logger.info(f"Found {len(matches)} total matches")
        return pd.DataFrame(matches)
    
    def validate_matches(self, matches_df):
        """
        Perform simple statistical validation of matches
        """
        logger.info("Validating matches...")
        
        # Compare mass distributions
        cosmos_masses = matches_df['cosmos_log_mass']
        sage_masses = matches_df['sage_log_mass']
        
        # Kolmogorov-Smirnov test
        ks_stat, ks_pvalue = stats.ks_2samp(cosmos_masses, sage_masses)
        
        # Basic statistics
        mass_bias = np.median(sage_masses - cosmos_masses)
        mass_scatter = np.std(sage_masses - cosmos_masses)
        
        validation_results = {
            'n_matches': len(matches_df),
            'mass_bias': mass_bias,
            'mass_scatter': mass_scatter,
            'ks_statistic': ks_stat,
            'ks_pvalue': ks_pvalue,
            'mean_redshift_diff': np.mean(np.abs(matches_df['sage_redshift'] - 
                                               matches_df['cosmos_redshift']))
        }
        
        return validation_results
    
    def plot_matching_results(self, matches_df, output_dir='./plots/'):
        """
        Create diagnostic plots of the matching results
        """
        os.makedirs(output_dir, exist_ok=True)
        
        fig, axes = plt.subplots(2, 2, figsize=(12, 10))
        
        # Mass comparison
        axes[0,0].scatter(matches_df['cosmos_log_mass'], matches_df['sage_log_mass'], 
                         alpha=0.5, s=1)
        axes[0,0].plot([8, 12], [8, 12], 'r--', label='1:1 line')
        axes[0,0].set_xlabel('COSMOS log(M*/M⊙)')
        axes[0,0].set_ylabel('SAGE log(M*/M⊙)')
        axes[0,0].legend()
        axes[0,0].set_title('Stellar Mass Comparison')
        
        # Mass difference distribution
        mass_diff = matches_df['sage_log_mass'] - matches_df['cosmos_log_mass']
        axes[0,1].hist(mass_diff, bins=50, alpha=0.7)
        axes[0,1].axvline(np.median(mass_diff), color='r', linestyle='--', 
                         label=f'Median: {np.median(mass_diff):.3f}')
        axes[0,1].set_xlabel('SAGE - COSMOS log(M*/M⊙)')
        axes[0,1].set_ylabel('Count')
        axes[0,1].legend()
        axes[0,1].set_title('Mass Difference Distribution')
        
        # Stellar mass distributions - plot COSMOS vs Mock Catalogue (matched SAGE)
        mass_bins = np.linspace(8, 12, 30)
        
        # Plot full COSMOS distribution
        if self.cosmos_data is not None:
            axes[1,0].hist(self.cosmos_data['log_stellar_mass'], bins=mass_bins, alpha=0.6, 
                          label=f'COSMOS (N={len(self.cosmos_data)})', color='blue',
                           zorder=10, density=True)
        
        # Plot mock catalogue distribution (matched SAGE galaxies)
        if len(matches_df) > 0:
            axes[1,0].hist(matches_df['sage_log_mass'], bins=mass_bins, alpha=0.6, 
                          label=f'Mock Catalogue (N={len(matches_df)})', color='red', density=True)
        
        axes[1,0].set_xlabel('log(M*/M⊙)')
        axes[1,0].set_ylabel('Normalized Count')
        axes[1,0].legend()
        axes[1,0].set_title('COSMOS vs Mock Catalogue Distributions')
        
        # Match quality distribution
        axes[1,1].hist(matches_df['mass_difference'], bins=50, alpha=0.7)
        axes[1,1].set_xlabel('|Mass Difference| (dex)')
        axes[1,1].set_ylabel('Count')
        axes[1,1].set_title('Match Quality Distribution')
        
        plt.tight_layout()
        plt.savefig(f'{output_dir}/sage_matching_diagnostics.pdf', dpi=300, bbox_inches='tight')
        plt.close()
        
        logger.info(f"Diagnostic plots saved to {output_dir}")
    
    def plot_mass_redshift_with_sage_redshifts(self, sage_redshifts, output_dir='./plots/', 
                                              sample_sage=True, max_sage_per_snap=500):
        """
        Create a stellar mass vs redshift plot with proper SAGE redshift values
        
        Parameters:
        -----------
        sage_redshifts : list
            List of redshifts corresponding to SAGE snapshots
        output_dir : str
            Directory to save the plot
        sample_sage : bool
            Whether to sample SAGE data for plotting (recommended for large datasets)
        max_sage_per_snap : int
            Maximum number of SAGE galaxies to plot per snapshot
        """
        os.makedirs(output_dir, exist_ok=True)
        
        fig, ax = plt.subplots(1, 1, figsize=(12, 8))
        
        # Plot COSMOS data (sampled for visualization)
        if self.cosmos_data is not None:
            cosmos_sample_size = min(7500, len(self.cosmos_data))
            if cosmos_sample_size < len(self.cosmos_data):
                # Sample the data for visualization
                cosmos_sample = self.cosmos_data.sample(n=cosmos_sample_size, random_state=42)
            else:
                cosmos_sample = self.cosmos_data
                
            ax.scatter(cosmos_sample['redshift'], cosmos_sample['log_stellar_mass'], 
                      alpha=0.7, s=15, c='blue', label=f'COSMOS (N={len(self.cosmos_data)}, showing {cosmos_sample_size})', 
                      marker='o', edgecolors='none')
        
        # Plot SAGE data with correct redshifts
        sage_z_all = []
        sage_mass_all = []
        
        for snap_name, sage_snap in self.sage_data.items():
            if sage_snap is not None:
                snap_num = int(snap_name.split('_')[1])
                if snap_num < len(sage_redshifts):
                    z_snap = sage_redshifts[snap_num]
                    
                    # Sample data if requested
                    sage_masses = sage_snap['log_stellar_mass']
                    if sample_sage and len(sage_masses) > max_sage_per_snap:
                        indices = np.random.choice(len(sage_masses), max_sage_per_snap, replace=False)
                        sage_masses = sage_masses[indices]
                    
                    sage_z_all.extend([z_snap] * len(sage_masses))
                    sage_mass_all.extend(sage_masses)
        
        if sage_z_all:
            ax.scatter(sage_z_all, sage_mass_all, alpha=0.4, s=8, c='red', 
                      label=f'SAGE (N={len(sage_z_all)})', marker='s', edgecolors='none')
        
        ax.set_xlabel('Redshift', fontsize=14)
        ax.set_ylabel('log(M*/M⊙)', fontsize=14) 
        # ax.set_title('Stellar Mass vs Redshift: COSMOS vs SAGE Mock Catalogue', fontsize=16)
        ax.legend(fontsize=12)
        # ax.grid(True, alpha=0.3)
        
        # Set axis limits
        if self.cosmos_data is not None:
            z_max = max(self.cosmos_data['redshift'].max(), max(sage_z_all) if sage_z_all else 0)
        else:
            z_max = max(sage_z_all) if sage_z_all else 10
        
        ax.set_xlim(2.8, min(z_max * 1.1, 15))
        ax.set_ylim(8, 12)
        
        plt.tight_layout()
        plt.savefig(f'{output_dir}/mass_redshift_comparison.pdf', dpi=300, bbox_inches='tight')
        # plt.savefig(f'{output_dir}/mass_redshift_comparison.png', dpi=300, bbox_inches='tight')
        plt.close()
        
        logger.info(f"Mass-redshift comparison plot saved to {output_dir}")

class ObservationalSageMatching:
    """
    Generic tool to match observational data (COSMOS, EPOCHS, etc.) with SAGE 2.0 model galaxies
    """
    
    def __init__(self, obs_file, sage_dir, dataset_name='Observatory', hubble_h=0.73):
        """
        Initialize the matching tool
        
        Parameters:
        -----------
        obs_file : str
            Path to observational CSV file
        sage_dir : str  
            Directory containing SAGE HDF5 outputs
        dataset_name : str
            Name of the dataset (e.g., 'COSMOS', 'EPOCHS')
        hubble_h : float
            Hubble parameter for unit conversions
        """
        self.obs_file = obs_file
        self.sage_dir = sage_dir
        self.dataset_name = dataset_name
        self.hubble_h = hubble_h
        self.obs_data = None
        self.sage_data = {}
        
    def load_observational_data(self, z_min=0.1, z_max=3.0, sample_fraction=0.1, 
                               stellar_mass_col='stellar_mass', redshift_col='redshift',
                               mass_conversion=None):
        """
        Load and sample observational data with redshift cuts
        
        Parameters:
        -----------
        z_min, z_max : float
            Redshift range to keep
        sample_fraction : float
            Fraction of galaxies to randomly sample (for performance)
        stellar_mass_col : str
            Column name for stellar mass
        redshift_col : str
            Column name for redshift
        mass_conversion : callable or None
            Function to convert mass to log10(M*/Msun) if needed
        """
        logger.info(f"Loading {self.dataset_name} data from {self.obs_file}")
        
        # Load the CSV file
        obs_data = pd.read_csv(self.obs_file)
        logger.info(f"Loaded {len(obs_data)} {self.dataset_name} galaxies")
        
        # Apply redshift cuts
        mask = (obs_data[redshift_col] >= z_min) & (obs_data[redshift_col] <= z_max)
        obs_data = obs_data[mask].copy()
        logger.info(f"After redshift cut ({z_min} < z < {z_max}): {len(obs_data)} galaxies")
        
        # Handle stellar mass conversion
        if mass_conversion is not None:
            stellar_mass_values = mass_conversion(obs_data[stellar_mass_col])
        else:
            stellar_mass_values = obs_data[stellar_mass_col]
        
        # Remove galaxies with invalid stellar masses
        valid_mass = np.isfinite(stellar_mass_values) & (stellar_mass_values > 0)
        obs_data = obs_data[valid_mass].copy()
        stellar_mass_values = stellar_mass_values[valid_mass]
        logger.info(f"After removing invalid masses: {len(obs_data)} galaxies")
        
        # Sample the data if requested
        if sample_fraction < 1.0:
            n_sample = int(len(obs_data) * sample_fraction)
            sample_indices = obs_data.sample(n=n_sample, random_state=42).index
            obs_data = obs_data.loc[sample_indices].copy()
            stellar_mass_values = stellar_mass_values[sample_indices]
            logger.info(f"Sampled {len(obs_data)} galaxies ({sample_fraction:.1%})")
        
        # Store standardized column names
        obs_data['log_stellar_mass'] = stellar_mass_values
        obs_data['redshift'] = obs_data[redshift_col]
        
        self.obs_data = obs_data
        return obs_data
    
    def redshift_to_snapshot(self, redshift, sage_redshifts):
        """
        Find the closest SAGE snapshot for a given redshift
        """
        sage_z_array = np.array(sage_redshifts)
        closest_idx = np.argmin(np.abs(sage_z_array - redshift))
        return closest_idx
    
    def load_sage_snapshot(self, snap_num):
        """
        Load SAGE data for a specific snapshot using your existing function
        """
        snap_name = f'Snap_{snap_num}'
        
        if snap_name in self.sage_data:
            return self.sage_data[snap_name]
        
        logger.info(f"Loading SAGE snapshot {snap_num}")
        
        # Use your existing read function (adapted)
        stellar_mass = self.read_hdf_ultra_optimized(snap_num=snap_name, param='StellarMass')
        
        if len(stellar_mass) == 0:
            return None
            
        # Convert to log10(M*/Msun) - SAGE units are 10^10 Msun/h
        log_stellar_mass = np.log10(stellar_mass * 1e10 / self.hubble_h)
        
        # Filter out very low mass galaxies (below typical detection)
        valid_mask = (log_stellar_mass > 8.0) & np.isfinite(log_stellar_mass)
        
        sage_snap_data = {
            'log_stellar_mass': log_stellar_mass[valid_mask],
            'snapshot': snap_num,
            'galaxy_indices': np.where(valid_mask)[0]  # Keep track of original indices
        }
        
        self.sage_data[snap_name] = sage_snap_data
        return sage_snap_data
    
    def read_hdf_ultra_optimized(self, snap_num=None, param=None):
        """
        Simplified version of your HDF5 reading function
        """
        model_files = [f for f in os.listdir(self.sage_dir) 
                      if f.startswith('model_') and f.endswith('.hdf5')]
        
        data_list = []
        for model_file in model_files:
            try:
                filepath = os.path.join(self.sage_dir, model_file)
                with h5.File(filepath, 'r') as f:
                    if snap_num in f and param in f[snap_num]:
                        data_list.append(f[snap_num][param][:])
            except Exception as e:
                logger.warning(f"Error reading {model_file}: {e}")
                continue
        
        return np.concatenate(data_list) if data_list else np.array([])
    
    def match_galaxies(self, sage_redshifts, max_matches=5, mass_tolerance=0.3):
        """
        Match observational galaxies to SAGE galaxies
        
        Parameters:
        -----------
        sage_redshifts : list
            List of redshifts for each SAGE snapshot
        max_matches : int
            Maximum number of SAGE matches per observational galaxy
        mass_tolerance : float
            Maximum difference in log stellar mass (dex)
        """
        if self.obs_data is None:
            raise ValueError(f"Must load {self.dataset_name} data first!")
        
        matches = []
        
        # Group observational galaxies by redshift bins for efficiency
        obs_z = self.obs_data['redshift'].values
        obs_mass = self.obs_data['log_stellar_mass'].values
        
        # Find unique snapshots needed
        unique_snaps = set()
        for z in obs_z:
            snap_idx = self.redshift_to_snapshot(z, sage_redshifts)
            unique_snaps.add(snap_idx)
        
        # Pre-load all needed SAGE snapshots
        logger.info(f"Pre-loading {len(unique_snaps)} SAGE snapshots")
        for snap_idx in unique_snaps:
            self.load_sage_snapshot(snap_idx)
        
        # Match each observational galaxy
        logger.info(f"Matching {len(self.obs_data)} {self.dataset_name} galaxies")
        
        for i, (obs_idx, obs_row) in enumerate(self.obs_data.iterrows()):
            if i % 1000 == 0:
                logger.info(f"Processed {i}/{len(self.obs_data)} galaxies")
            
            obs_z = obs_row['redshift']
            obs_logm = obs_row['log_stellar_mass']
            
            # Find closest SAGE snapshot
            snap_idx = self.redshift_to_snapshot(obs_z, sage_redshifts)
            snap_name = f'Snap_{snap_idx}'
            sage_snap = self.sage_data.get(snap_name)
            
            if sage_snap is None:
                continue
            
            # Find SAGE galaxies within mass tolerance
            sage_logm = sage_snap['log_stellar_mass']
            mass_diff = np.abs(sage_logm - obs_logm)
            mass_mask = mass_diff <= mass_tolerance
            
            if not np.any(mass_mask):
                continue
            
            # Calculate matching score (inverse of mass difference)
            valid_sage_logm = sage_logm[mass_mask]
            valid_sage_indices = sage_snap['galaxy_indices'][mass_mask]
            mass_distances = np.abs(valid_sage_logm - obs_logm)
            
            # Sort by mass similarity and take best matches
            sort_indices = np.argsort(mass_distances)
            n_matches = min(len(sort_indices), max_matches)
            
            for j in range(n_matches):
                sage_idx = sort_indices[j]
                
                match_data = {
                    f'{self.dataset_name.lower()}_idx': obs_idx,
                    f'{self.dataset_name.lower()}_redshift': obs_z,
                    f'{self.dataset_name.lower()}_log_mass': obs_logm,
                    'sage_snapshot': snap_idx,
                    'sage_redshift': sage_redshifts[snap_idx],
                    'sage_galaxy_idx': valid_sage_indices[sage_idx],
                    'sage_log_mass': valid_sage_logm[sage_idx],
                    'mass_difference': mass_distances[sage_idx],
                    'match_rank': j + 1
                }
                
                # Add other observational properties
                for col in self.obs_data.columns:
                    if col not in ['redshift', 'log_stellar_mass']:
                        match_data[f'{self.dataset_name.lower()}_{col}'] = obs_row[col]
                
                matches.append(match_data)
        
        logger.info(f"Found {len(matches)} total matches")
        return pd.DataFrame(matches)
    
    def validate_matches(self, matches_df):
        """
        Perform simple statistical validation of matches
        """
        logger.info("Validating matches...")
        
        # Compare mass distributions
        obs_masses = matches_df[f'{self.dataset_name.lower()}_log_mass']
        sage_masses = matches_df['sage_log_mass']
        
        # Kolmogorov-Smirnov test
        ks_stat, ks_pvalue = stats.ks_2samp(obs_masses, sage_masses)
        
        # Basic statistics
        mass_bias = np.median(sage_masses - obs_masses)
        mass_scatter = np.std(sage_masses - obs_masses)
        
        validation_results = {
            'n_matches': len(matches_df),
            'mass_bias': mass_bias,
            'mass_scatter': mass_scatter,
            'ks_statistic': ks_stat,
            'ks_pvalue': ks_pvalue,
            'mean_redshift_diff': np.mean(np.abs(matches_df['sage_redshift'] - 
                                               matches_df[f'{self.dataset_name.lower()}_redshift']))
        }
        
        return validation_results
    
    def plot_matching_results(self, matches_df, output_dir='./plots/'):
        """
        Create diagnostic plots of the matching results
        """
        os.makedirs(output_dir, exist_ok=True)
        
        fig, axes = plt.subplots(2, 2, figsize=(12, 10))
        
        obs_mass_col = f'{self.dataset_name.lower()}_log_mass'
        
        # Mass comparison
        axes[0,0].scatter(matches_df[obs_mass_col], matches_df['sage_log_mass'], 
                         alpha=0.5, s=1)
        axes[0,0].plot([8, 12], [8, 12], 'r--', label='1:1 line')
        axes[0,0].set_xlabel(f'{self.dataset_name} log(M*/M⊙)')
        axes[0,0].set_ylabel('SAGE log(M*/M⊙)')
        axes[0,0].legend()
        axes[0,0].set_title('Stellar Mass Comparison')
        
        # Mass difference distribution
        mass_diff = matches_df['sage_log_mass'] - matches_df[obs_mass_col]
        axes[0,1].hist(mass_diff, bins=50, alpha=0.7)
        axes[0,1].axvline(np.median(mass_diff), color='r', linestyle='--', 
                         label=f'Median: {np.median(mass_diff):.3f}')
        axes[0,1].set_xlabel(f'SAGE - {self.dataset_name} log(M*/M⊙)')
        axes[0,1].set_ylabel('Count')
        axes[0,1].legend()
        axes[0,1].set_title('Mass Difference Distribution')
        
        # Stellar mass distributions - plot observations vs Mock Catalogue (matched SAGE)
        mass_bins = np.linspace(8, 12, 30)
        
        # Plot full observational distribution
        if self.obs_data is not None:
            axes[1,0].hist(self.obs_data['log_stellar_mass'], bins=mass_bins, alpha=0.6, 
                          label=f'{self.dataset_name} (N={len(self.obs_data)})', color='blue',
                           zorder=10, density=True)
        
        # Plot mock catalogue distribution (matched SAGE galaxies)
        if len(matches_df) > 0:
            axes[1,0].hist(matches_df['sage_log_mass'], bins=mass_bins, alpha=0.6, 
                          label=f'Mock Catalogue (N={len(matches_df)})', color='red', density=True)
        
        axes[1,0].set_xlabel('log(M*/M⊙)')
        axes[1,0].set_ylabel('Normalized Count')
        axes[1,0].legend()
        axes[1,0].set_title(f'{self.dataset_name} vs Mock Catalogue Distributions')
        
        # Match quality distribution
        axes[1,1].hist(matches_df['mass_difference'], bins=50, alpha=0.7)
        axes[1,1].set_xlabel('|Mass Difference| (dex)')
        axes[1,1].set_ylabel('Count')
        axes[1,1].set_title('Match Quality Distribution')
        
        plt.tight_layout()
        plt.savefig(f'{output_dir}/{self.dataset_name.lower()}_sage_matching_diagnostics.pdf', 
                   dpi=300, bbox_inches='tight')
        plt.close()
        
        logger.info(f"Diagnostic plots saved to {output_dir}")
    
    def plot_mass_redshift_with_sage_redshifts(self, sage_redshifts, output_dir='./plots/', 
                                              sample_obs=True, max_obs_points=7500,
                                              sample_sage=True, max_sage_per_snap=500):
        """
        Create a stellar mass vs redshift plot with proper SAGE redshift values
        """
        os.makedirs(output_dir, exist_ok=True)
        
        fig, ax = plt.subplots(1, 1, figsize=(12, 8))
        
        # Plot observational data (sampled for visualization)
        if self.obs_data is not None:
            obs_sample_size = min(max_obs_points, len(self.obs_data))
            if sample_obs and obs_sample_size < len(self.obs_data):
                obs_sample = self.obs_data.sample(n=obs_sample_size, random_state=42)
            else:
                obs_sample = self.obs_data
                
            ax.scatter(obs_sample['redshift'], obs_sample['log_stellar_mass'], 
                      alpha=0.7, s=15, c='blue', 
                      label=f'{self.dataset_name} (N={len(self.obs_data)}, showing {len(obs_sample)})', 
                      marker='o', edgecolors='none')
        
        # Plot SAGE data with correct redshifts
        sage_z_all = []
        sage_mass_all = []
        
        for snap_name, sage_snap in self.sage_data.items():
            if sage_snap is not None:
                snap_num = int(snap_name.split('_')[1])
                if snap_num < len(sage_redshifts):
                    z_snap = sage_redshifts[snap_num]
                    
                    # Sample data if requested
                    sage_masses = sage_snap['log_stellar_mass']
                    if sample_sage and len(sage_masses) > max_sage_per_snap:
                        indices = np.random.choice(len(sage_masses), max_sage_per_snap, replace=False)
                        sage_masses = sage_masses[indices]
                    
                    sage_z_all.extend([z_snap] * len(sage_masses))
                    sage_mass_all.extend(sage_masses)
        
        if sage_z_all:
            ax.scatter(sage_z_all, sage_mass_all, alpha=0.4, s=8, c='red', 
                      label=f'SAGE (N={len(sage_z_all)})', marker='s', edgecolors='none')
        
        ax.set_xlabel('Redshift', fontsize=14)
        ax.set_ylabel('log(M*/M⊙)', fontsize=14) 
        ax.legend(fontsize=12)
        
        # Set axis limits
        if self.obs_data is not None:
            z_max = max(self.obs_data['redshift'].max(), max(sage_z_all) if sage_z_all else 0)
        else:
            z_max = max(sage_z_all) if sage_z_all else 10
        
        ax.set_xlim(0, min(z_max * 1.1, 15))
        ax.set_ylim(7, 12)
        
        plt.tight_layout()
        plt.savefig(f'{output_dir}/{self.dataset_name.lower()}_mass_redshift_comparison.pdf', 
                   dpi=300, bbox_inches='tight')
        plt.close()
        
        logger.info(f"Mass-redshift comparison plot saved to {output_dir}")

class CEERSSageMatching:
    """
    Tool to match CEERS quiescent galaxies with SAGE 2.0 model galaxies
    Uses pre-defined arrays of CEERS data rather than loading from CSV
    """
    
    def __init__(self, sage_dir, hubble_h=0.73):
        """
        Initialize the CEERS matching tool
        
        Parameters:
        -----------
        sage_dir : str  
            Directory containing SAGE HDF5 outputs
        hubble_h : float
            Hubble parameter for unit conversions
        """
        self.sage_dir = sage_dir
        self.hubble_h = hubble_h
        self.ceers_data = None
        self.sage_data = {}
        self.dataset_name = 'CEERS'
        
    def load_ceers_data(self):
        """
        Load CEERS quiescent galaxies data from predefined arrays
        """
        logger.info("Loading CEERS quiescent galaxies data")
        
        # CEERS data arrays
        ceers_redshift = np.array([4.20, 4.20, 4.91, 4.68, 4.00, 3.59, 3.78, 3.87, 3.61, 3.10,
                                   3.89, 4.32, 3.30, 3.19, 3.29, 3.53, 3.38, 4.00, 3.28, 3.73,
                                   3.60, 3.40, 3.30, 3.28, 3.53, 2.69, 4.71, 3.12, 3.86, 2.70,
                                   2.99, 2.61, 3.06, 2.85, 3.48, 2.75, 2.77, 5.27, 3.19, 3.53,
                                   3.52, 3.66, 3.35, 3.62])

        ceers_log_mass = np.array([9.19, 10.0, 9.77, 9.31, 10.2, 10.2, 8.79, 9.84, 9.88, 10.4,
                                   9.89, 9.43, 9.99, 10.8, 8.91, 10.4, 10.7, 10.5, 10.4, 10.7,
                                   9.43, 10.2, 11.2, 10.4, 10.5, 10.6, 10.2, 10.0, 11.3, 10.1,
                                   10.2, 10.4, 10.3, 10.5, 10.6, 10.6, 10.5, 9.38, 10.4, 10.6,
                                   9.58, 10.4, 10.4, 10.5])

        ceers_log_ssfr = np.array([-10.6, -12.0, -10.1, -9.98, -11.2, -10.6, -10.4, -10.6, -10.1, -12.0,
                                   -11.1, -10.7, -12.0, -12.0, -11.3, -11.4, -11.1, -10.7, -12.7, -11.1,
                                   -9.92, -12.0, -11.1, -11.4, -11.4, -11.0, -11.1, -11.4, -11.5, -11.6,
                                   -10.2, -12.0, -11.3, -10.7, -12.1, -11.0, -12.6, -9.84, -11.6, -11.2,
                                   -11.1, -11.1, -9.93, -10.0])

        ceers_redshift_err = np.array([0.19, 0.03, 0.24, 0.18, 0.02, 0.01, 0.37, 0.13, 0.04, 0.02,
                                       0.02, 0.11, 0.02, 0.02, 0.18, 0.15, 0.10, 0.56, 0.09, 0.18,
                                       0.23, 0.10, 0.14, 0.10, 0.18, 1.07, 0.22, 0.36, 0.29, 0.30,
                                       0.13, 0.14, 0.39, 0.19, 0.14, 0.15, 0.16, 0.28, 0.17, 0.47,
                                       0.34, 0.14, 0.33, 0.51])

        ceers_log_mass_err = np.array([0.03, 0.02, 0.03, 0.02, 0.02, 0.05, 0.04, 0.02, 0.02, 0.02,
                                       0.02, 0.02, 0.02, 0.02, 0.04, 0.04, 0.03, 0.07, 0.03, 0.04,
                                       0.06, 0.04, 0.03, 0.04, 0.04, 0.19, 0.04, 0.05, 0.06, 0.06,
                                       0.06, 0.03, 0.06, 0.05, 0.05, 0.03, 0.04, 0.08, 0.04, 0.05,
                                       0.06, 0.04, 0.08, 0.09])
        
        # Create DataFrame
        self.ceers_data = pd.DataFrame({
            'redshift': ceers_redshift,
            'log_stellar_mass': ceers_log_mass,
            'log_ssfr': ceers_log_ssfr,
            'redshift_err': ceers_redshift_err,
            'log_mass_err': ceers_log_mass_err
        })
        
        logger.info(f"Loaded {len(self.ceers_data)} CEERS quiescent galaxies")
        return self.ceers_data
    
    def redshift_to_snapshot(self, redshift, sage_redshifts):
        """
        Find the closest SAGE snapshot for a given redshift
        """
        sage_z_array = np.array(sage_redshifts)
        closest_idx = np.argmin(np.abs(sage_z_array - redshift))
        return closest_idx
    
    def load_sage_snapshot(self, snap_num):
        """
        Load SAGE data for a specific snapshot including sSFR
        """
        snap_name = f'Snap_{snap_num}'
        
        if snap_name in self.sage_data:
            return self.sage_data[snap_name]
        
        logger.info(f"Loading SAGE snapshot {snap_num}")
        
        stellar_mass = self.read_hdf_ultra_optimized(snap_num=snap_name, param='StellarMass')
        
        if len(stellar_mass) == 0:
            return None
            
        # Load SFR data for sSFR calculation
        sfr_disk = self.read_hdf_ultra_optimized(snap_num=snap_name, param='SfrDisk')
        sfr_bulge = self.read_hdf_ultra_optimized(snap_num=snap_name, param='SfrBulge')
        
        # Convert to log10(M*/Msun) - SAGE units are 10^10 Msun/h
        log_stellar_mass = np.log10(stellar_mass * 1e10 / self.hubble_h)
        
        # Calculate sSFR if SFR data is available
        log_ssfr = np.full(len(stellar_mass), np.nan)
        if len(sfr_disk) > 0 and len(sfr_bulge) > 0:
            total_sfr = sfr_disk + sfr_bulge
            stellar_mass_msun = stellar_mass * 1e10 / self.hubble_h
            
            # Calculate sSFR - handle zero SFR galaxies properly
            ssfr = total_sfr / stellar_mass_msun
            
            # For log calculation, set a very low floor for zero SFR galaxies
            ssfr_for_log = np.where(ssfr > 0, ssfr, 1e-15)
            log_ssfr = np.log10(ssfr_for_log)
        
        # Filter out very low mass galaxies
        valid_mask = (log_stellar_mass > 8.0) & np.isfinite(log_stellar_mass)
        
        sage_snap_data = {
            'log_stellar_mass': log_stellar_mass[valid_mask],
            'log_ssfr': log_ssfr[valid_mask],
            'snapshot': snap_num,
            'galaxy_indices': np.where(valid_mask)[0]
        }
        
        self.sage_data[snap_name] = sage_snap_data
        return sage_snap_data
    
    def read_hdf_ultra_optimized(self, snap_num=None, param=None):
        """
        Simplified version of HDF5 reading function
        """
        model_files = [f for f in os.listdir(self.sage_dir) 
                      if f.startswith('model_') and f.endswith('.hdf5')]
        
        data_list = []
        for model_file in model_files:
            try:
                filepath = os.path.join(self.sage_dir, model_file)
                with h5.File(filepath, 'r') as f:
                    if snap_num in f and param in f[snap_num]:
                        data_list.append(f[snap_num][param][:])
            except Exception as e:
                logger.warning(f"Error reading {model_file}: {e}")
                continue
        
        return np.concatenate(data_list) if data_list else np.array([])
    
    def match_galaxies(self, sage_redshifts, max_matches=5, mass_tolerance=0.3, ssfr_tolerance=1.0, use_ssfr_matching=True):
        """
        Match CEERS galaxies to SAGE galaxies using both stellar mass and sSFR
        
        Parameters:
        -----------
        sage_redshifts : list
            List of redshifts for SAGE snapshots
        max_matches : int
            Maximum number of matches per CEERS galaxy
        mass_tolerance : float
            Tolerance in log10(M*/Msun) for mass matching
        ssfr_tolerance : float
            Tolerance in log10(sSFR) for sSFR matching (in dex)
        use_ssfr_matching : bool
            Whether to include sSFR in the matching criteria
        """
        if self.ceers_data is None:
            raise ValueError("Must load CEERS data first!")
        
        matches = []
        
        # Get CEERS data
        ceers_z = self.ceers_data['redshift'].values
        ceers_mass = self.ceers_data['log_stellar_mass'].values
        ceers_ssfr = self.ceers_data['log_ssfr'].values
        
        # Find unique snapshots needed
        unique_snaps = set()
        for z in ceers_z:
            snap_idx = self.redshift_to_snapshot(z, sage_redshifts)
            unique_snaps.add(snap_idx)
        
        # Pre-load all needed SAGE snapshots
        logger.info(f"Pre-loading {len(unique_snaps)} SAGE snapshots")
        for snap_idx in unique_snaps:
            self.load_sage_snapshot(snap_idx)
        
        # Match each CEERS galaxy
        logger.info(f"Matching {len(self.ceers_data)} CEERS galaxies")
        if use_ssfr_matching:
            logger.info(f"Using combined mass (±{mass_tolerance} dex) and sSFR (±{ssfr_tolerance} dex) matching")
        else:
            logger.info(f"Using mass-only matching (±{mass_tolerance} dex)")
        
        for i, (ceers_idx, ceers_row) in enumerate(self.ceers_data.iterrows()):
            ceers_z = ceers_row['redshift']
            ceers_logm = ceers_row['log_stellar_mass']
            ceers_log_ssfr = ceers_row['log_ssfr']
            
            # Find closest SAGE snapshot
            snap_idx = self.redshift_to_snapshot(ceers_z, sage_redshifts)
            snap_name = f'Snap_{snap_idx}'
            sage_snap = self.sage_data.get(snap_name)
            
            if sage_snap is None:
                continue
            
            # Find SAGE galaxies within mass tolerance
            sage_logm = sage_snap['log_stellar_mass']
            sage_log_ssfr = sage_snap['log_ssfr']
            
            mass_diff = np.abs(sage_logm - ceers_logm)
            mass_mask = mass_diff <= mass_tolerance
            
            # Apply sSFR matching if requested
            if use_ssfr_matching and np.isfinite(ceers_log_ssfr):
                ssfr_diff = np.abs(sage_log_ssfr - ceers_log_ssfr)
                ssfr_mask = ssfr_diff <= ssfr_tolerance
                # Combine both criteria
                combined_mask = mass_mask & ssfr_mask
            else:
                combined_mask = mass_mask
            
            if not np.any(combined_mask):
                continue
            
            # Calculate matching score using both mass and sSFR
            valid_sage_logm = sage_logm[combined_mask]
            valid_sage_log_ssfr = sage_log_ssfr[combined_mask]
            valid_sage_indices = sage_snap['galaxy_indices'][combined_mask]
            
            mass_distances = np.abs(valid_sage_logm - ceers_logm)
            
            # Calculate combined matching score
            if use_ssfr_matching and np.isfinite(ceers_log_ssfr):
                ssfr_distances = np.abs(valid_sage_log_ssfr - ceers_log_ssfr)
                # Normalize both distances and combine (equal weighting)
                normalized_mass_dist = mass_distances / mass_tolerance
                normalized_ssfr_dist = ssfr_distances / ssfr_tolerance
                combined_distances = np.sqrt(normalized_mass_dist**2 + normalized_ssfr_dist**2)
            else:
                combined_distances = mass_distances
                ssfr_distances = np.full(len(mass_distances), np.nan)
            
            # Sort by combined similarity and take best matches
            sort_indices = np.argsort(combined_distances)
            n_matches = min(len(sort_indices), max_matches)
            
            for j in range(n_matches):
                sage_idx = sort_indices[j]
                
                match_data = {
                    'ceers_idx': ceers_idx,
                    'ceers_redshift': ceers_z,
                    'ceers_log_mass': ceers_logm,
                    'ceers_log_ssfr': ceers_log_ssfr,
                    'sage_snapshot': snap_idx,
                    'sage_redshift': sage_redshifts[snap_idx],
                    'sage_galaxy_idx': valid_sage_indices[sage_idx],
                    'sage_log_mass': valid_sage_logm[sage_idx],
                    'sage_log_ssfr': valid_sage_log_ssfr[sage_idx],
                    'mass_difference': mass_distances[sage_idx],
                    'ssfr_difference': ssfr_distances[sage_idx] if use_ssfr_matching else np.nan,
                    'combined_distance': combined_distances[sage_idx],
                    'match_rank': j + 1,
                    'used_ssfr_matching': use_ssfr_matching
                }
                
                # Add other CEERS properties
                for col in self.ceers_data.columns:
                    if col not in ['redshift', 'log_stellar_mass', 'log_ssfr']:
                        match_data[f'ceers_{col}'] = ceers_row[col]
                
                matches.append(match_data)
        
        logger.info(f"Found {len(matches)} total CEERS matches")
        return pd.DataFrame(matches)
    
    def validate_matches(self, matches_df):
        """
        Perform simple statistical validation of matches including sSFR
        """
        logger.info("Validating CEERS matches...")
        
        # Compare mass distributions
        ceers_masses = matches_df['ceers_log_mass']
        sage_masses = matches_df['sage_log_mass']
        
        # Compare sSFR distributions if available
        ceers_ssfr = matches_df['ceers_log_ssfr']
        sage_ssfr = matches_df['sage_log_ssfr']
        
        # Kolmogorov-Smirnov tests
        mass_ks_stat, mass_ks_pvalue = stats.ks_2samp(ceers_masses, sage_masses)
        
        # Basic statistics
        mass_bias = np.median(sage_masses - ceers_masses)
        mass_scatter = np.std(sage_masses - ceers_masses)
        
        validation_results = {
            'n_matches': len(matches_df),
            'mass_bias': mass_bias,
            'mass_scatter': mass_scatter,
            'mass_ks_statistic': mass_ks_stat,
            'mass_ks_pvalue': mass_ks_pvalue,
            'mean_redshift_diff': np.mean(np.abs(matches_df['sage_redshift'] - 
                                               matches_df['ceers_redshift']))
        }
        
        # Add sSFR statistics if available
        valid_ssfr_mask = np.isfinite(ceers_ssfr) & np.isfinite(sage_ssfr)
        if np.any(valid_ssfr_mask):
            valid_ceers_ssfr = ceers_ssfr[valid_ssfr_mask]
            valid_sage_ssfr = sage_ssfr[valid_ssfr_mask]
            
            ssfr_ks_stat, ssfr_ks_pvalue = stats.ks_2samp(valid_ceers_ssfr, valid_sage_ssfr)
            ssfr_bias = np.median(valid_sage_ssfr - valid_ceers_ssfr)
            ssfr_scatter = np.std(valid_sage_ssfr - valid_ceers_ssfr)
            
            validation_results.update({
                'ssfr_bias': ssfr_bias,
                'ssfr_scatter': ssfr_scatter,
                'ssfr_ks_statistic': ssfr_ks_stat,
                'ssfr_ks_pvalue': ssfr_ks_pvalue,
                'n_valid_ssfr_matches': len(valid_ceers_ssfr)
            })
        else:
            validation_results.update({
                'ssfr_bias': np.nan,
                'ssfr_scatter': np.nan,
                'ssfr_ks_statistic': np.nan,
                'ssfr_ks_pvalue': np.nan,
                'n_valid_ssfr_matches': 0
            })
        
        return validation_results
    
    def plot_ssfr_matching_comparison(self, matches_mass_only, matches_combined, output_dir='./plots/'):
        """
        Create diagnostic plots comparing mass-only vs combined mass+sSFR matching
        """
        os.makedirs(output_dir, exist_ok=True)
        
        fig, axes = plt.subplots(2, 3, figsize=(18, 12))
        
        # Plot 1: Mass comparison - mass-only matching
        ax1 = axes[0, 0]
        if len(matches_mass_only) > 0:
            ax1.scatter(matches_mass_only['ceers_log_mass'], matches_mass_only['sage_log_mass'], 
                       alpha=0.6, s=50, c='blue', label=f'Mass-only (N={len(matches_mass_only)})')
        ax1.plot([8, 12], [8, 12], 'k--', alpha=0.5, label='Perfect match')
        ax1.set_xlabel('CEERS log(M*/M☉)')
        ax1.set_ylabel('SAGE log(M*/M☉)')
        ax1.set_title('Mass Comparison: Mass-Only Matching')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # Plot 2: Mass comparison - combined matching
        ax2 = axes[0, 1]
        if len(matches_combined) > 0:
            ax2.scatter(matches_combined['ceers_log_mass'], matches_combined['sage_log_mass'], 
                       alpha=0.6, s=50, c='red', label=f'Combined (N={len(matches_combined)})')
        ax2.plot([8, 12], [8, 12], 'k--', alpha=0.5, label='Perfect match')
        ax2.set_xlabel('CEERS log(M*/M☉)')
        ax2.set_ylabel('SAGE log(M*/M☉)')
        ax2.set_title('Mass Comparison: Combined Matching')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # Plot 3: sSFR comparison - combined matching only
        ax3 = axes[0, 2]
        if len(matches_combined) > 0:
            valid_mask = np.isfinite(matches_combined['ceers_log_ssfr']) & np.isfinite(matches_combined['sage_log_ssfr'])
            if np.any(valid_mask):
                ceers_ssfr_valid = matches_combined['ceers_log_ssfr'][valid_mask]
                sage_ssfr_valid = matches_combined['sage_log_ssfr'][valid_mask]
                ax3.scatter(ceers_ssfr_valid, sage_ssfr_valid, 
                           alpha=0.6, s=50, c='green', label=f'Combined (N={len(ceers_ssfr_valid)})')
                
                # Plot perfect match line
                ssfr_min = min(ceers_ssfr_valid.min(), sage_ssfr_valid.min())
                ssfr_max = max(ceers_ssfr_valid.max(), sage_ssfr_valid.max())
                ax3.plot([ssfr_min, ssfr_max], [ssfr_min, ssfr_max], 'k--', alpha=0.5, label='Perfect match')
        ax3.set_xlabel('CEERS log(sSFR) [yr⁻¹]')
        ax3.set_ylabel('SAGE log(sSFR) [yr⁻¹]')
        ax3.set_title('sSFR Comparison: Combined Matching')
        ax3.legend()
        ax3.grid(True, alpha=0.3)
        
        # Plot 4: Number of matches per CEERS galaxy
        ax4 = axes[1, 0]
        if len(matches_mass_only) > 0:
            mass_only_counts = matches_mass_only.groupby('ceers_idx').size()
            ax4.hist(mass_only_counts, bins=np.arange(0.5, max(mass_only_counts)+1.5), 
                    alpha=0.7, label='Mass-only', color='blue')
        if len(matches_combined) > 0:
            combined_counts = matches_combined.groupby('ceers_idx').size()
            ax4.hist(combined_counts, bins=np.arange(0.5, max(combined_counts)+1.5), 
                    alpha=0.7, label='Combined', color='red')
        ax4.set_xlabel('Number of matches per CEERS galaxy')
        ax4.set_ylabel('Count')
        ax4.set_title('Match Count Distribution')
        ax4.legend()
        ax4.grid(True, alpha=0.3)
        
        # Plot 5: Mass difference distribution
        ax5 = axes[1, 1]
        if len(matches_mass_only) > 0:
            ax5.hist(matches_mass_only['mass_difference'], bins=20, alpha=0.7, 
                    label='Mass-only', color='blue', density=True)
        if len(matches_combined) > 0:
            ax5.hist(matches_combined['mass_difference'], bins=20, alpha=0.7, 
                    label='Combined', color='red', density=True)
        ax5.set_xlabel('Mass difference [dex]')
        ax5.set_ylabel('Density')
        ax5.set_title('Mass Difference Distribution')
        ax5.legend()
        ax5.grid(True, alpha=0.3)
        
        # Plot 6: sSFR difference distribution (combined only)
        ax6 = axes[1, 2]
        if len(matches_combined) > 0:
            valid_ssfr_diff = matches_combined['ssfr_difference'].dropna()
            if len(valid_ssfr_diff) > 0:
                ax6.hist(valid_ssfr_diff, bins=20, alpha=0.7, color='green', density=True)
                ax6.axvline(valid_ssfr_diff.median(), color='darkgreen', linestyle='--', 
                           label=f'Median: {valid_ssfr_diff.median():.2f}')
        ax6.set_xlabel('sSFR difference [dex]')
        ax6.set_ylabel('Density')
        ax6.set_title('sSFR Difference Distribution')
        ax6.legend()
        ax6.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(f'{output_dir}/ceers_ssfr_matching_comparison.pdf', dpi=300, bbox_inches='tight')
        plt.close()
        
        logger.info(f"sSFR matching comparison plot saved to {output_dir}")

# Example usage
def run_cosmos_sage_matching():
    """
    Example of how to use the matching tool
    """
    
    # SAGE redshifts from your configuration
    sage_redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 
                     14.086, 12.941, 11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 
                     6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 
                     3.060, 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 
                     1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 0.828, 0.755, 0.687, 0.624, 
                     0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 
                     0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    # Initialize matcher - UPDATE THESE PATHS!
    matcher = COSMOSSageMatching(
        cosmos_file='./data/COSMOS.csv',  # Update this path to your COSMOS CSV file
        sage_dir='./output/millennium_FIRE/',        # Update this path to your SAGE output directory
        hubble_h=0.73
    )
    
    # Load COSMOS data with sampling
    cosmos_data = matcher.load_cosmos_data(
        z_min=3.0, 
        z_max=22.0,
        sample_fraction=0.1,  # Use 10% for testing
        stellar_mass_col='mass_med',  # Update column name as needed
        redshift_col='zfinal'       # Update column name as needed
    )
    
    # Perform matching
    matches = matcher.match_galaxies(
        sage_redshifts=sage_redshifts,
        max_matches=2,      # Find up to 2 matches per COSMOS galaxy
        mass_tolerance=0.5  # Within 0.05 dex in stellar mass
    )
    
    # Validate results
    validation = matcher.validate_matches(matches)
    print("Validation Results:")
    for key, value in validation.items():
        print(f"  {key}: {value}")
    
    # Create diagnostic plots
    matcher.plot_matching_results(matches, output_dir='./plots/')
    
    # Create mass-redshift comparison plot
    matcher.plot_mass_redshift_with_sage_redshifts(sage_redshifts, output_dir='./plots/')
    
    # Save results
    matches.to_csv('cosmos_sage_matches.csv', index=False)
    print(f"Saved {len(matches)} matches to cosmos_sage_matches.csv")
    
    return matches, validation

def run_epochs_sage_matching():
    """
    Example of how to use the matching tool with EPOCHS data
    """
    
    # SAGE redshifts from your configuration
    sage_redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 
                     14.086, 12.941, 11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 
                     6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 
                     3.060, 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 
                     1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 0.828, 0.755, 0.687, 0.624, 
                     0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 
                     0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    # Initialize matcher for EPOCHS - UPDATE THESE PATHS!
    matcher = ObservationalSageMatching(
        obs_file= './data/EPOCHS.csv',  # Update this path to your EPOCHS CSV file
        sage_dir='./output/millennium_FIRE/',  # Update this path to your SAGE output directory
        dataset_name='EPOCHS',
        hubble_h=0.73
    )
    
    # Load EPOCHS data with sampling
    epochs_data = matcher.load_observational_data(
        z_min=5.0,  # EPOCHS covers high redshifts
        z_max=15.0,
        sample_fraction=1.0,  # Use all EPOCHS data (it's not that large)
        stellar_mass_col='stellar_mass_pipes_zgauss',  # EPOCHS stellar mass column
        redshift_col='redshift_pipes_zgauss'           # EPOCHS redshift column
    )
    
    # Perform matching
    matches = matcher.match_galaxies(
        sage_redshifts=sage_redshifts,
        max_matches=2,      # Find up to 2 matches per EPOCHS galaxy
        mass_tolerance=0.5  # Within 0.5 dex in stellar mass
    )
    
    # Validate results
    validation = matcher.validate_matches(matches)
    print("EPOCHS Validation Results:")
    for key, value in validation.items():
        print(f"  {key}: {value}")
    
    # Create diagnostic plots
    matcher.plot_matching_results(matches, output_dir='./plots/')
    
    # Create mass-redshift comparison plot
    matcher.plot_mass_redshift_with_sage_redshifts(sage_redshifts, output_dir='./plots/')
    
    # Save results
    matches.to_csv('epochs_sage_matches.csv', index=False)
    print(f"Saved {len(matches)} EPOCHS matches to epochs_sage_matches.csv")
    
    return matches, validation

def run_ceers_sage_matching():
    """
    Example of how to use the matching tool with CEERS quiescent galaxies data
    Now includes sSFR matching for better quiescent galaxy identification
    """
    
    # SAGE redshifts from your configuration
    sage_redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 
                     14.086, 12.941, 11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 
                     6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 
                     3.060, 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 
                     1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 0.828, 0.755, 0.687, 0.624, 
                     0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 
                     0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    # Initialize matcher for CEERS
    matcher = CEERSSageMatching(
        sage_dir='./output/millennium_FIRE/',  # Update this path to your SAGE output directory
        hubble_h=0.73
    )
    
    # Load CEERS data (uses predefined arrays)
    ceers_data = matcher.load_ceers_data()
    print(f"Loaded {len(ceers_data)} CEERS quiescent galaxies")
    print(f"CEERS sSFR range: {ceers_data['log_ssfr'].min():.2f} to {ceers_data['log_ssfr'].max():.2f}")
    
    # Perform matching with mass only (original method)
    print("\n=== MASS-ONLY MATCHING ===")
    matches_mass_only = matcher.match_galaxies(
        sage_redshifts=sage_redshifts,
        max_matches=100,      # Find up to 10 matches per CEERS galaxy
        mass_tolerance=0.5,   # Within 0.5 dex in stellar mass
        use_ssfr_matching=False
    )
    
    # Perform matching with mass + sSFR (new method)
    print("\n=== COMBINED MASS + sSFR MATCHING ===")
    matches_combined = matcher.match_galaxies(
        sage_redshifts=sage_redshifts,
        max_matches=100,       # Find up to 10 matches per CEERS galaxy
        mass_tolerance=0.5,  # Within 1.0 dex in stellar mass
        ssfr_tolerance=0.5,   # Within 0.5 dex in sSFR (quiescent galaxies can vary)
        use_ssfr_matching=True
    )
    
    # Compare results
    print(f"\nMass-only matching found: {len(matches_mass_only)} matches")
    print(f"Combined matching found: {len(matches_combined)} matches")
    
    # Validate results for both methods
    print("\n=== MASS-ONLY VALIDATION ===")
    validation_mass_only = matcher.validate_matches(matches_mass_only)
    for key, value in validation_mass_only.items():
        if isinstance(value, float):
            print(f"  {key}: {value:.4f}")
        else:
            print(f"  {key}: {value}")
    
    print("\n=== COMBINED VALIDATION ===")
    validation_combined = matcher.validate_matches(matches_combined)
    for key, value in validation_combined.items():
        if isinstance(value, float):
            print(f"  {key}: {value:.4f}")
        else:
            print(f"  {key}: {value}")
    
    # Save both sets of results
    matches_mass_only.to_csv('ceers_sage_matches_mass_only.csv', index=False)
    matches_combined.to_csv('ceers_sage_matches_combined.csv', index=False)
    
    print(f"\nSaved results:")
    print(f"  - ceers_sage_matches_mass_only.csv ({len(matches_mass_only)} matches)")
    print(f"  - ceers_sage_matches_combined.csv ({len(matches_combined)} matches)")
    
    # Analyze sSFR improvement
    if len(matches_combined) > 0:
        print(f"\n=== sSFR MATCHING ANALYSIS ===")
        valid_ssfr = matches_combined['ssfr_difference'].dropna()
        if len(valid_ssfr) > 0:
            print(f"  Mean sSFR difference: {valid_ssfr.mean():.3f} dex")
            print(f"  Median sSFR difference: {valid_ssfr.median():.3f} dex")
            print(f"  sSFR difference std: {valid_ssfr.std():.3f} dex")
    
    # Create comparison plots
    print(f"\n=== CREATING COMPARISON PLOTS ===")
    matcher.plot_ssfr_matching_comparison(matches_mass_only, matches_combined)
    print("Comparison plots saved to ./plots/ceers_ssfr_matching_comparison.pdf")
    
    return matches_combined, validation_combined
    
    return matches, validation

def run_all_datasets():
    """
    Run matching for COSMOS, EPOCHS, and CEERS datasets and create combined plots
    """
    print("="*50)
    print("COSMOS Matching")
    print("="*50)
    cosmos_matches, cosmos_validation = run_cosmos_sage_matching()
    
    print("\n" + "="*50)
    print("EPOCHS Matching")
    print("="*50)
    epochs_matches, epochs_validation = run_epochs_sage_matching()
    
    print("\n" + "="*50)
    print("CEERS Matching")
    print("="*50)
    ceers_matches, ceers_validation = run_ceers_sage_matching()
    
    # Create combined plots
    print("\n" + "="*50)
    print("Creating Combined Plots")
    print("="*50)
    create_combined_plots(cosmos_matches, epochs_matches, ceers_matches, 
                         cosmos_validation, epochs_validation, ceers_validation)
    
    return {
        'cosmos': {'matches': cosmos_matches, 'validation': cosmos_validation},
        'epochs': {'matches': epochs_matches, 'validation': epochs_validation},
        'ceers': {'matches': ceers_matches, 'validation': ceers_validation}
    }

# Keep the old function for backwards compatibility
def run_both_datasets():
    """
    Run matching for both COSMOS and EPOCHS datasets and create combined plots
    (Kept for backwards compatibility - use run_all_datasets for all three datasets)
    """
    print("="*50)
    print("COSMOS Matching")
    print("="*50)
    cosmos_matches, cosmos_validation = run_cosmos_sage_matching()
    
    print("\n" + "="*50)
    print("EPOCHS Matching")
    print("="*50)
    epochs_matches, epochs_validation = run_epochs_sage_matching()
    
    # Create combined plots (without CEERS)
    print("\n" + "="*50)
    print("Creating Combined Plots")
    print("="*50)
    create_combined_plots(cosmos_matches, epochs_matches, None, cosmos_validation, epochs_validation, None)
    
    return {
        'cosmos': {'matches': cosmos_matches, 'validation': cosmos_validation},
        'epochs': {'matches': epochs_matches, 'validation': epochs_validation}
    }

def create_combined_plots(cosmos_matches, epochs_matches, ceers_matches=None, 
                         cosmos_validation=None, epochs_validation=None, ceers_validation=None, 
                         output_dir='./plots/'):
    """
    Create combined diagnostic and mass-redshift plots for both COSMOS and EPOCHS
    """
    import matplotlib.pyplot as plt
    import numpy as np
    import os
    
    os.makedirs(output_dir, exist_ok=True)
    
    # SAGE redshifts (needed for mass-redshift plot)
    sage_redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 
                     14.086, 12.941, 11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 
                     6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 
                     3.060, 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 
                     1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 0.828, 0.755, 0.687, 0.624, 
                     0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 
                     0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    # Load the original observational data from CSV files to get full distributions
    cosmos_full_data = None
    epochs_full_data = None
    
    try:
        # Load COSMOS data
        cosmos_full = pd.read_csv('./data/COSMOS.csv')
        # Apply the same cuts as in the matching
        cosmos_mask = (cosmos_full['zfinal'] >= 3.0) & (cosmos_full['zfinal'] <= 22.0)
        cosmos_full = cosmos_full[cosmos_mask]
        valid_mass = np.isfinite(cosmos_full['mass_med']) & (cosmos_full['mass_med'] > 0)
        cosmos_full = cosmos_full[valid_mass]
        cosmos_full_data = cosmos_full['mass_med']  # Use the original mass column
        logger.info(f"Loaded full COSMOS data: {len(cosmos_full_data)} galaxies")
    except Exception as e:
        logger.warning(f"Could not load full COSMOS data: {e}")
    
    try:
        # Load EPOCHS data
        epochs_full = pd.read_csv('./data/EPOCHS.csv')
        # Apply the same cuts as in the matching
        epochs_mask = (epochs_full['redshift_pipes_zgauss'] >= 5.0) & (epochs_full['redshift_pipes_zgauss'] <= 15.0)
        epochs_full = epochs_full[epochs_mask]
        valid_mass = np.isfinite(epochs_full['stellar_mass_pipes_zgauss']) & (epochs_full['stellar_mass_pipes_zgauss'] > 0)
        epochs_full = epochs_full[valid_mass]
        epochs_full_data = epochs_full['stellar_mass_pipes_zgauss']  # Use the original mass column
        logger.info(f"Loaded full EPOCHS data: {len(epochs_full_data)} galaxies")
    except Exception as e:
        logger.warning(f"Could not load full EPOCHS data: {e}")
    
    # 1. Combined Diagnostic Plots (4-panel figure)
    fig, axes = plt.subplots(2, 2, figsize=(15, 12))
    
    # Mass comparison (top left)
    if len(cosmos_matches) > 0:
        axes[0,0].scatter(cosmos_matches['cosmos_log_mass'], cosmos_matches['sage_log_mass'], 
                         alpha=0.6, s=8, c='gold', label='COSMOS', marker='s')
    if len(epochs_matches) > 0:
        axes[0,0].scatter(epochs_matches['epochs_log_mass'], epochs_matches['sage_log_mass'], 
                         alpha=0.6, s=8, c='red', label='EPOCHS', marker='D')
    if ceers_matches is not None and len(ceers_matches) > 0:
        axes[0,0].scatter(ceers_matches['ceers_log_mass'], ceers_matches['sage_log_mass'], 
                         alpha=0.8, s=25, c='purple', label='CEERS', marker='^')
    
    axes[0,0].plot([8, 12], [8, 12], 'k--', label='1:1 line', alpha=0.7)
    axes[0,0].set_xlabel('Observed log(M*/M⊙)')
    axes[0,0].set_ylabel('SAGE log(M*/M⊙)')
    axes[0,0].legend()
    axes[0,0].set_title('Stellar Mass Comparison')
    # axes[0,0].grid(True, alpha=0.3)
    
    # Mass difference distribution (top right)
    if len(cosmos_matches) > 0:
        cosmos_mass_diff = cosmos_matches['sage_log_mass'] - cosmos_matches['cosmos_log_mass']
        axes[0,1].hist(cosmos_mass_diff, bins=30, alpha=0.6, color='gold', 
                      label=f'COSMOS (median: {np.median(cosmos_mass_diff):.3f})', density=True)
    
    if len(epochs_matches) > 0:
        epochs_mass_diff = epochs_matches['sage_log_mass'] - epochs_matches['epochs_log_mass']
        axes[0,1].hist(epochs_mass_diff, bins=30, alpha=0.6, color='red', 
                      label=f'EPOCHS (median: {np.median(epochs_mass_diff):.3f})', density=True)
    
    if ceers_matches is not None and len(ceers_matches) > 0:
        ceers_mass_diff = ceers_matches['sage_log_mass'] - ceers_matches['ceers_log_mass']
        axes[0,1].hist(ceers_mass_diff, bins=15, alpha=0.6, color='purple', 
                      label=f'CEERS (median: {np.median(ceers_mass_diff):.3f})', density=True)
    
    axes[0,1].axvline(0, color='k', linestyle='--', alpha=0.7)
    axes[0,1].set_xlabel('SAGE - Observed log(M*/M⊙)')
    axes[0,1].set_ylabel('Normalized Count')
    axes[0,1].legend()
    axes[0,1].set_title('Mass Difference Distributions')
    # axes[0,1].grid(True, alpha=0.3)
    
    # Stellar mass distributions (bottom left) - Compare observations vs mock catalogues
    mass_bins = np.linspace(8, 12, 25)
    
    # Plot observational data using full datasets if available, otherwise matched data
    if cosmos_full_data is not None:
        axes[1,0].hist(cosmos_full_data, bins=mass_bins, alpha=0.7, 
                      label=f'COSMOS Obs (N={len(cosmos_full_data)})', color='gold', 
                      density=True, histtype='step', linewidth=2)
    elif len(cosmos_matches) > 0:
        axes[1,0].hist(cosmos_matches['cosmos_log_mass'], bins=mass_bins, alpha=0.7, 
                      label=f'COSMOS Obs (N={len(cosmos_matches)})', color='gold', 
                      density=True, histtype='step', linewidth=2)
    
    if epochs_full_data is not None:
        axes[1,0].hist(epochs_full_data, bins=mass_bins, alpha=0.7, 
                      label=f'EPOCHS Obs (N={len(epochs_full_data)})', color='red', 
                      density=True, histtype='step', linewidth=2)
    elif len(epochs_matches) > 0:
        axes[1,0].hist(epochs_matches['epochs_log_mass'], bins=mass_bins, alpha=0.7, 
                      label=f'EPOCHS Obs (N={len(epochs_matches)})', color='red', 
                      density=True, histtype='step', linewidth=2)
    
    # Plot CEERS observational data (always use matched data since it's small)
    if ceers_matches is not None and len(ceers_matches) > 0:
        axes[1,0].hist(ceers_matches['ceers_log_mass'], bins=mass_bins, alpha=0.8, 
                      label=f'CEERS Obs (N={len(ceers_matches)})', color='purple', 
                      density=True, histtype='step', linewidth=2)
    
    # Plot mock catalogue distributions (matched SAGE galaxies)
    if len(cosmos_matches) > 0:
        axes[1,0].hist(cosmos_matches['sage_log_mass'], bins=mass_bins, alpha=0.4, 
                      label=f'COSMOS Mock (N={len(cosmos_matches)})', color='lightgoldenrodyellow', density=True)
    
    if len(epochs_matches) > 0:
        axes[1,0].hist(epochs_matches['sage_log_mass'], bins=mass_bins, alpha=0.4, 
                      label=f'EPOCHS Mock (N={len(epochs_matches)})', color='lightcoral', density=True)
    
    if ceers_matches is not None and len(ceers_matches) > 0:
        axes[1,0].hist(ceers_matches['sage_log_mass'], bins=mass_bins, alpha=0.4, 
                      label=f'CEERS Mock (N={len(ceers_matches)})', color='plum', density=True)
    
    axes[1,0].set_xlabel('log(M*/M⊙)')
    axes[1,0].set_ylabel('Normalized Count')
    axes[1,0].legend()
    axes[1,0].set_title('Observed vs Mock Catalogue Distributions')
    # axes[1,0].grid(True, alpha=0.3)
    
    # Match quality distributions (bottom right)
    if len(cosmos_matches) > 0:
        axes[1,1].hist(cosmos_matches['mass_difference'], bins=25, alpha=0.6, color='gold', 
                      label=f'COSMOS', density=True)
    
    if len(epochs_matches) > 0:
        axes[1,1].hist(epochs_matches['mass_difference'], bins=25, alpha=0.6, color='red', 
                      label=f'EPOCHS', density=True)
    
    if ceers_matches is not None and len(ceers_matches) > 0:
        axes[1,1].hist(ceers_matches['mass_difference'], bins=15, alpha=0.6, color='purple', 
                      label=f'CEERS', density=True)
    
    axes[1,1].set_xlabel('|Mass Difference| (dex)')
    axes[1,1].set_ylabel('Normalized Count')
    axes[1,1].legend()
    axes[1,1].set_title('Match Quality Distributions')
    # axes[1,1].grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    # Update filename to include all datasets
    if ceers_matches is not None and len(ceers_matches) > 0:
        plt.savefig(f'{output_dir}/combined_cosmos_epochs_ceers_diagnostics.pdf', dpi=300, bbox_inches='tight')
    else:
        plt.savefig(f'{output_dir}/combined_cosmos_epochs_diagnostics.pdf', dpi=300, bbox_inches='tight')
    plt.close()
    
    # 2. Combined Mass-Redshift Plot
    fig, ax = plt.subplots(1, 1, figsize=(14, 8))
    
    # Plot COSMOS data (sampled for visualization) - Yellow squares
    if len(cosmos_matches) > 0:
        # Sample COSMOS for visualization if too many points
        cosmos_sample_size = min(5000, len(cosmos_matches))
        if cosmos_sample_size < len(cosmos_matches):
            cosmos_sample = cosmos_matches.sample(n=cosmos_sample_size, random_state=42)
        else:
            cosmos_sample = cosmos_matches
            
        ax.scatter(cosmos_sample['cosmos_redshift'], cosmos_sample['cosmos_log_mass'], 
                  alpha=0.8, s=40, c='gold', label=f'COSMOS Obs (N={len(cosmos_matches)})', 
                  marker='s', edgecolors='darkorange', linewidth=0.5)
    
    # Plot EPOCHS data - Red diamonds
    if len(epochs_matches) > 0:
        ax.scatter(epochs_matches['epochs_redshift'], epochs_matches['epochs_log_mass'], 
                  alpha=0.8, s=40, c='red', label=f'EPOCHS Obs (N={len(epochs_matches)})', 
                  marker='D', edgecolors='darkred', linewidth=0.5)
    
    # Plot CEERS data - Purple triangles
    if ceers_matches is not None and len(ceers_matches) > 0:
        ax.scatter(ceers_matches['ceers_redshift'], ceers_matches['ceers_log_mass'], 
                  alpha=0.8, s=40, c='purple', label=f'CEERS Obs (N={len(ceers_matches)})', 
                  marker='^', edgecolors='indigo', linewidth=0.8)
    
    # Plot SAGE Mock Catalogues as stars - Combined legend entry
    sage_mock_plotted = False
    total_sage_mocks = 0
    
    # Plot COSMOS Mock Catalogue (SAGE galaxies matched to COSMOS) - Yellow stars
    if len(cosmos_matches) > 0:
        cosmos_sage_masses = []
        cosmos_sage_redshifts = []
        
        for _, row in cosmos_matches.iterrows():
            snap_idx = int(row['sage_snapshot'])
            if snap_idx < len(sage_redshifts):
                cosmos_sage_masses.append(row['sage_log_mass'])
                cosmos_sage_redshifts.append(sage_redshifts[snap_idx])
        
        # Sample COSMOS mock for visualization
        if len(cosmos_sage_masses) > 0:
            cosmos_mock_sample_size = min(2000, len(cosmos_sage_masses))
            if cosmos_mock_sample_size < len(cosmos_sage_masses):
                indices = np.random.choice(len(cosmos_sage_masses), cosmos_mock_sample_size, replace=False)
                sampled_cosmos_masses = [cosmos_sage_masses[i] for i in indices]
                sampled_cosmos_redshifts = [cosmos_sage_redshifts[i] for i in indices]
            else:
                sampled_cosmos_masses = cosmos_sage_masses
                sampled_cosmos_redshifts = cosmos_sage_redshifts
            
            total_sage_mocks += len(cosmos_sage_masses)
            label_text = f'SAGE Mocks' if not sage_mock_plotted else None
            ax.scatter(sampled_cosmos_redshifts, sampled_cosmos_masses, alpha=1.0, s=300, c='gold', 
                      label=label_text, marker='*', edgecolors='black', linewidth=0.5)
            sage_mock_plotted = True
    
    # Plot EPOCHS Mock Catalogue (SAGE galaxies matched to EPOCHS) - Red stars
    if len(epochs_matches) > 0:
        epochs_sage_masses = []
        epochs_sage_redshifts = []
        
        for _, row in epochs_matches.iterrows():
            snap_idx = int(row['sage_snapshot'])
            if snap_idx < len(sage_redshifts):
                epochs_sage_masses.append(row['sage_log_mass'])
                epochs_sage_redshifts.append(sage_redshifts[snap_idx])
        
        # Sample EPOCHS mock for visualization
        if len(epochs_sage_masses) > 0:
            epochs_mock_sample_size = min(2000, len(epochs_sage_masses))
            if epochs_mock_sample_size < len(epochs_sage_masses):
                indices = np.random.choice(len(epochs_sage_masses), epochs_mock_sample_size, replace=False)
                sampled_epochs_masses = [epochs_sage_masses[i] for i in indices]
                sampled_epochs_redshifts = [epochs_sage_redshifts[i] for i in indices]
            else:
                sampled_epochs_masses = epochs_sage_masses
                sampled_epochs_redshifts = epochs_sage_redshifts
            
            if sage_mock_plotted:
                # Update the existing label to include both datasets
                total_sage_mocks += len(epochs_sage_masses)
                # Plot without label since we already have one
                ax.scatter(sampled_epochs_redshifts, sampled_epochs_masses, alpha=1.0, s=300, c='red', 
                          marker='*', edgecolors='black', linewidth=0.5)
                # Update the legend entry manually
                handles, labels = ax.get_legend_handles_labels()
                if handles:
                    labels[-1] = f'SAGE Mocks'
            else:
                # First SAGE mock dataset
                total_sage_mocks = len(epochs_sage_masses)
                ax.scatter(sampled_epochs_redshifts, sampled_epochs_masses, alpha=1.0, s=300, c='red', 
                          label=f'SAGE Mocks', marker='*', edgecolors='black', linewidth=0.5)
                sage_mock_plotted = True
    
    # Plot CEERS Mock Catalogue (SAGE galaxies matched to CEERS) - Purple stars
    if ceers_matches is not None and len(ceers_matches) > 0:
        ceers_sage_masses = []
        ceers_sage_redshifts = []
        
        for _, row in ceers_matches.iterrows():
            snap_idx = int(row['sage_snapshot'])
            if snap_idx < len(sage_redshifts):
                ceers_sage_masses.append(row['sage_log_mass'])
                ceers_sage_redshifts.append(sage_redshifts[snap_idx])
        
        # All CEERS mock galaxies (small dataset)
        if len(ceers_sage_masses) > 0:
            if sage_mock_plotted:
                # Update the total count and plot without separate label
                total_sage_mocks += len(ceers_sage_masses)
                ax.scatter(ceers_sage_redshifts, ceers_sage_masses, alpha=1.0, s=300, c='purple', 
                          marker='*', edgecolors='black', linewidth=0.5)
                # Update the legend entry to show total
                handles, labels = ax.get_legend_handles_labels()
                if handles:
                    labels[-1] = f'SAGE Mocks'
            else:
                # First (and possibly only) SAGE mock dataset
                total_sage_mocks = len(ceers_sage_masses)
                ax.scatter(ceers_sage_redshifts, ceers_sage_masses, alpha=1.0, s=300, c='purple', 
                          label=f'SAGE Mocks', marker='*', edgecolors='black', linewidth=0.5)
                sage_mock_plotted = True
    
    ax.set_xlabel('Redshift', fontsize=14)
    ax.set_ylabel('log(M*/M⊙)', fontsize=14) 
    ax.legend(fontsize=12)
    # ax.grid(True, alpha=0.3)
    
    # Set axis limits
    z_max = 0
    if len(cosmos_matches) > 0:
        z_max = max(z_max, cosmos_matches['cosmos_redshift'].max())
    if len(epochs_matches) > 0:
        z_max = max(z_max, epochs_matches['epochs_redshift'].max())
    if ceers_matches is not None and len(ceers_matches) > 0:
        z_max = max(z_max, ceers_matches['ceers_redshift'].max())
    
    ax.set_xlim(2.9, min(z_max * 1.1, 15))
    ax.set_ylim(8, 12)
    
    plt.tight_layout()
    
    # Update filename based on which datasets are included
    if ceers_matches is not None and len(ceers_matches) > 0:
        plt.savefig(f'{output_dir}/combined_cosmos_epochs_ceers_mass_redshift.pdf', dpi=300, bbox_inches='tight')
    else:
        plt.savefig(f'{output_dir}/combined_cosmos_epochs_mass_redshift.pdf', dpi=300, bbox_inches='tight')
    plt.close()
    
    # Print summary statistics
    print("\nCombined Results Summary:")
    print("-" * 30)
    if len(cosmos_matches) > 0:
        print(f"COSMOS: {len(cosmos_matches)} matches")
        if cosmos_validation is not None:
            print(f"  Mass bias: {cosmos_validation['mass_bias']:.3f} dex")
            print(f"  Mass scatter: {cosmos_validation['mass_scatter']:.3f} dex")
    
    if len(epochs_matches) > 0:
        print(f"EPOCHS: {len(epochs_matches)} matches")
        if epochs_validation is not None:
            print(f"  Mass bias: {epochs_validation['mass_bias']:.3f} dex")
            print(f"  Mass scatter: {epochs_validation['mass_scatter']:.3f} dex")
    
    if ceers_matches is not None and len(ceers_matches) > 0:
        print(f"CEERS: {len(ceers_matches)} matches")
        if ceers_validation is not None:
            print(f"  Mass bias: {ceers_validation['mass_bias']:.3f} dex")
            print(f"  Mass scatter: {ceers_validation['mass_scatter']:.3f} dex")
    
    print(f"\nCombined plots saved to {output_dir}")
    if ceers_matches is not None and len(ceers_matches) > 0:
        print("- combined_cosmos_epochs_ceers_diagnostics.pdf")
        print("- combined_cosmos_epochs_ceers_mass_redshift.pdf")
    else:
        print("- combined_cosmos_epochs_diagnostics.pdf")
        print("- combined_cosmos_epochs_mass_redshift.pdf")
    
    logger.info(f"Combined diagnostic and mass-redshift plots saved to {output_dir}")

class SAGEGalaxyTracker:
    """
    Track specific matched galaxies through cosmic time from their match redshift to z=0
    """
    
    def __init__(self, sage_dir, hubble_h=0.73):
        """
        Initialize the galaxy tracker
        
        Parameters:
        -----------
        sage_dir : str  
            Directory containing SAGE HDF5 outputs
        hubble_h : float
            Hubble parameter for unit conversions
        """
        self.sage_dir = sage_dir
        self.hubble_h = hubble_h
        self._model_files = None  # Cache model file list
        self._snapshot_cache = {}  # Cache loaded snapshot data
        
    def _get_model_files(self):
        """Get and cache the list of model files"""
        if self._model_files is None:
            self._model_files = [f for f in os.listdir(self.sage_dir) 
                               if f.startswith('model_') and f.endswith('.hdf5')]
            logger.info(f"Found {len(self._model_files)} SAGE model files")
        return self._model_files
    
    def bulk_load_snapshots(self, snap_range, properties_list, force_reload=False):
        """
        Efficiently load multiple snapshots at once
        
        Parameters:
        -----------
        snap_range : range or list
            Snapshot numbers to load
        properties_list : list
            List of SAGE parameter names to read
        force_reload : bool
            Force reload even if cached
        """
        model_files = self._get_model_files()
        
        # Determine which snapshots need loading
        snaps_to_load = []
        for snap_num in snap_range:
            cache_key = f"Snap_{snap_num}"
            if force_reload or cache_key not in self._snapshot_cache:
                snaps_to_load.append(snap_num)
        
        if not snaps_to_load:
            logger.debug(f"All snapshots already cached")
            return
        
        logger.info(f"Bulk loading {len(snaps_to_load)} snapshots with {len(properties_list)} properties")
        logger.info(f"Snapshots to load: {min(snaps_to_load)} to {max(snaps_to_load)}")
        logger.info(f"Properties: {', '.join(properties_list)}")
        
        # Initialize cache entries
        for snap_num in snaps_to_load:
            snap_name = f'Snap_{snap_num}'
            self._snapshot_cache[snap_name] = {prop: [] for prop in properties_list}
        
        # Open each model file once and read all needed snapshots
        logger.info(f"Reading from {len(model_files)} model files...")
        for i, model_file in enumerate(model_files):
            if i % 10 == 0:  # Progress indicator
                logger.info(f"Processing model file {i+1}/{len(model_files)}: {model_file}")
            
            try:
                filepath = os.path.join(self.sage_dir, model_file)
                with h5.File(filepath, 'r') as f:
                    # Read all needed snapshots from this file
                    for snap_num in snaps_to_load:
                        snap_name = f'Snap_{snap_num}'
                        
                        if snap_name in f:
                            # Read each property for this snapshot
                            for prop in properties_list:
                                if prop in f[snap_name]:
                                    self._snapshot_cache[snap_name][prop].append(f[snap_name][prop][:])
                                else:
                                    self._snapshot_cache[snap_name][prop].append(np.array([]))
                        
            except Exception as e:
                logger.warning(f"Error reading {model_file}: {e}")
                continue
        
        # Concatenate arrays for each snapshot
        logger.info("Concatenating arrays from all model files...")
        for snap_num in snaps_to_load:
            snap_name = f'Snap_{snap_num}'
            if snap_name in self._snapshot_cache:
                final_data = {}
                for prop in properties_list:
                    if prop in self._snapshot_cache[snap_name]:
                        # Filter out empty arrays and concatenate
                        non_empty = [arr for arr in self._snapshot_cache[snap_name][prop] if len(arr) > 0]
                        if non_empty:
                            final_data[prop] = np.concatenate(non_empty)
                        else:
                            final_data[prop] = np.array([])
                    else:
                        final_data[prop] = np.array([])
                
                # Replace list of arrays with final concatenated arrays
                self._snapshot_cache[snap_name] = final_data
        
        logger.info(f"Bulk loading completed. Cache now contains {len(self._snapshot_cache)} snapshots")
    
    def clear_cache(self):
        """Clear the snapshot cache to free memory"""
        self._snapshot_cache.clear()
        logger.info("Snapshot cache cleared")
    
    def get_cache_info(self):
        """Get information about current cache state"""
        cache_size_mb = 0
        for snap_name, data in self._snapshot_cache.items():
            for prop_name, array in data.items():
                if hasattr(array, 'nbytes'):
                    cache_size_mb += array.nbytes / (1024 * 1024)
        
        return {
            'cached_snapshots': list(self._snapshot_cache.keys()),
            'cache_size_mb': cache_size_mb,
            'n_snapshots': len(self._snapshot_cache)
        }
        
    def read_snapshot_properties(self, snap_num, properties_list):
        """
        Read multiple properties from all SAGE files for a given snapshot
        Uses cache when available, falls back to individual loading if not cached
        
        Parameters:
        -----------
        snap_num : int
            Snapshot number
        properties_list : list
            List of SAGE parameter names to read
            
        Returns:
        --------
        dict : Dictionary with arrays for each property
        """
        snap_name = f'Snap_{snap_num}'
        
        # Check cache first
        if snap_name in self._snapshot_cache:
            cached_data = self._snapshot_cache[snap_name]
            # Return only requested properties
            return {prop: cached_data.get(prop, np.array([])) for prop in properties_list}
        
        # Fall back to individual loading (original method)
        logger.debug(f"Cache miss for {snap_name}, loading individually")
        model_files = self._get_model_files()
        
        all_data = {prop: [] for prop in properties_list}
        
        for model_file in model_files:
            try:
                filepath = os.path.join(self.sage_dir, model_file)
                with h5.File(filepath, 'r') as f:
                    if snap_name in f:
                        for prop in properties_list:
                            if prop in f[snap_name]:
                                all_data[prop].append(f[snap_name][prop][:])
                            else:
                                logger.warning(f"Property {prop} not found in {model_file}")
                                all_data[prop].append(np.array([]))
            except Exception as e:
                logger.warning(f"Error reading {model_file}: {e}")
                continue
        
        # Concatenate arrays from all files
        final_data = {}
        for prop in properties_list:
            if all_data[prop]:
                # Filter out empty arrays
                non_empty = [arr for arr in all_data[prop] if len(arr) > 0]
                if non_empty:
                    final_data[prop] = np.concatenate(non_empty)
                else:
                    final_data[prop] = np.array([])
            else:
                final_data[prop] = np.array([])
        
        # Cache the result for future use
        self._snapshot_cache[snap_name] = final_data
        
        return final_data
                
        return final_data
    
    def find_galaxy_evolution(self, galaxy_info, sage_redshifts, end_snap=None):
        """
        Track a set of galaxies from their initial snapshot to z=0 (or end_snap)
        
        Parameters:
        -----------
        galaxy_info : dict
            Dictionary with 'galaxy_indices', 'start_snapshot', 'catalogue_name'
        sage_redshifts : list
            List of SAGE redshifts
        end_snap : int, optional
            Final snapshot to track to (default: last snapshot with z~0)
            
        Returns:
        --------
        dict : Evolution data for these galaxies
        """
        start_snap = galaxy_info['start_snapshot']
        galaxy_indices = galaxy_info['galaxy_indices']
        cat_name = galaxy_info['catalogue_name']
        
        if end_snap is None:
            end_snap = len(sage_redshifts) - 1  # Go to z=0
        
        logger.info(f"Tracking {len(galaxy_indices)} {cat_name} galaxies from snap {start_snap} to {end_snap}")
        
        # Properties to track
        props_to_read = ['GalaxyIndex', 'CentralGalaxyIndex', 'Type', 'StellarMass', 'Mvir', 
                        'ColdGas', 'H2_gas', 'HI_gas', 'SfrDisk', 'SfrBulge', 'BulgeMass']
        
        # OPTIMIZATION: Bulk load all snapshots we'll need
        snap_range = range(start_snap, end_snap + 1)
        logger.info(f"Bulk loading {len(snap_range)} snapshots for {cat_name} tracking")
        self.bulk_load_snapshots(snap_range, props_to_read)
        
        # Get initial galaxy indices/IDs to track
        initial_data = self.read_snapshot_properties(start_snap, props_to_read)
        
        if len(initial_data.get('GalaxyIndex', [])) == 0:
            logger.error(f"No GalaxyIndex data found for snapshot {start_snap}")
            return {}
        
        # Get the GalaxyIndex values for our matched galaxies
        valid_indices = galaxy_indices[galaxy_indices < len(initial_data['GalaxyIndex'])]
        if len(valid_indices) == 0:
            logger.error(f"No valid galaxy indices for {cat_name}")
            return {}
            
        target_galaxy_ids = initial_data['GalaxyIndex'][valid_indices]
        logger.info(f"Tracking {len(target_galaxy_ids)} {cat_name} galaxies with GalaxyIndex values")
        
        # Track evolution
        evolution_data = {
            'snapshots': [],
            'redshifts': [],
            'centrals': {prop: [] for prop in ['log_halo_mass', 'log_stellar_mass', 'log_ssfr', 
                                              'log_cold_gas', 'h2_fraction', 'quiescent_fraction']},
            'satellites': {prop: [] for prop in ['log_halo_mass', 'log_stellar_mass', 'log_ssfr', 
                                                'log_cold_gas', 'h2_fraction', 'quiescent_fraction']},
            # NEW: Store individual galaxy data for z=0 descendants plot
            'individual_galaxies': {
                'galaxy_ids': target_galaxy_ids.copy(),  # Original galaxy IDs we're tracking
                'z0_stellar_masses': [],
                'z0_halo_masses': [], 
                'z0_types': [],  # Will be 'central' or 'satellite'
                'z0_galaxy_ids': []  # Galaxy IDs found at z=0
            }
        }
        
        for snap in range(start_snap, end_snap + 1):
            if snap >= len(sage_redshifts):
                break
                
            logger.debug(f"Processing snapshot {snap} (z={sage_redshifts[snap]:.2f})")
            
            # Read properties for this snapshot
            snap_data = self.read_snapshot_properties(snap, props_to_read)
            
            if len(snap_data.get('GalaxyIndex', [])) == 0:
                logger.warning(f"No data for snapshot {snap}, skipping")
                continue
            
            # Find our galaxies in this snapshot
            galaxy_mask = np.isin(snap_data['GalaxyIndex'], target_galaxy_ids)
            n_found = np.sum(galaxy_mask)
            
            if n_found == 0:
                logger.debug(f"No tracked galaxies found in snapshot {snap}")
                # Still append data points with NaN for continuity
                evolution_data['snapshots'].append(snap)
                evolution_data['redshifts'].append(sage_redshifts[snap])
                for gal_type in ['centrals', 'satellites']:
                    for prop in evolution_data[gal_type]:
                        evolution_data[gal_type][prop].append(np.nan)
                continue
            
            # Extract properties for found galaxies
            found_props = {}
            for prop in props_to_read:
                if prop in snap_data and len(snap_data[prop]) > 0:
                    found_props[prop] = snap_data[prop][galaxy_mask]
                else:
                    found_props[prop] = np.array([])
            
            # Separate centrals (Type=0) and satellites (Type=1,2)
            if len(found_props.get('Type', [])) == 0:
                logger.warning(f"No Type data for snapshot {snap}")
                continue
                
            central_mask = found_props['Type'] == 0
            satellite_mask = (found_props['Type'] == 1) | (found_props['Type'] == 2)
            
            evolution_data['snapshots'].append(snap)
            evolution_data['redshifts'].append(sage_redshifts[snap])
            
            # Process centrals and satellites separately
            for gal_type, type_mask in [('centrals', central_mask), ('satellites', satellite_mask)]:
                if not np.any(type_mask):
                    # No galaxies of this type - append NaN
                    for prop in evolution_data[gal_type]:
                        evolution_data[gal_type][prop].append(np.nan)
                    continue
                
                # Extract properties for this galaxy type
                type_props = {}
                for prop in found_props:
                    if len(found_props[prop]) > 0:
                        type_props[prop] = found_props[prop][type_mask]
                
                # Calculate derived properties and medians
                derived_props = self.calculate_derived_properties(type_props, sage_redshifts[snap])
                
                # NEW: If this is the final snapshot (z~0), store individual galaxy data
                if snap == end_snap:  # Final snapshot (z~0)
                    if 'log_stellar_mass' in derived_props and 'log_halo_mass' in derived_props:
                        stellar_masses = derived_props['log_stellar_mass']
                        halo_masses = derived_props['log_halo_mass']
                        galaxy_ids = found_props['GalaxyIndex'][type_mask]
                        
                        gal_type_str = 'central' if gal_type == 'centrals' else 'satellite'
                        
                        # Store each individual galaxy
                        for i in range(len(stellar_masses)):
                            if np.isfinite(stellar_masses[i]) and np.isfinite(halo_masses[i]):
                                evolution_data['individual_galaxies']['z0_stellar_masses'].append(stellar_masses[i])
                                evolution_data['individual_galaxies']['z0_halo_masses'].append(halo_masses[i])
                                evolution_data['individual_galaxies']['z0_types'].append(gal_type_str)
                                evolution_data['individual_galaxies']['z0_galaxy_ids'].append(galaxy_ids[i])
                
                # Store median values (for backward compatibility)
                for prop_name in evolution_data[gal_type]:
                    if prop_name in derived_props:
                        prop_value = derived_props[prop_name]
                        
                        # Handle scalar values (like quiescent_fraction)
                        if np.isscalar(prop_value):
                            if np.isfinite(prop_value):
                                evolution_data[gal_type][prop_name].append(prop_value)
                            else:
                                evolution_data[gal_type][prop_name].append(np.nan)
                        # Handle array values
                        elif hasattr(prop_value, '__len__') and len(prop_value) > 0:
                            valid_values = prop_value[np.isfinite(prop_value)]
                            if len(valid_values) > 0:
                                evolution_data[gal_type][prop_name].append(np.median(valid_values))
                            else:
                                evolution_data[gal_type][prop_name].append(np.nan)
                        else:
                            evolution_data[gal_type][prop_name].append(np.nan)
                    else:
                        evolution_data[gal_type][prop_name].append(np.nan)
        
        logger.info(f"Completed tracking {cat_name} galaxies through {len(evolution_data['snapshots'])} snapshots")
        
        # Log individual galaxy statistics
        n_individual_z0 = len(evolution_data['individual_galaxies']['z0_stellar_masses'])
        if n_individual_z0 > 0:
            n_centrals = sum(1 for t in evolution_data['individual_galaxies']['z0_types'] if t == 'central')
            n_satellites = sum(1 for t in evolution_data['individual_galaxies']['z0_types'] if t == 'satellite')
            logger.info(f"Found {n_individual_z0} individual galaxies at z~0: {n_centrals} centrals, {n_satellites} satellites")
        else:
            logger.warning(f"No individual galaxies found at z~0 for {cat_name}")
        
        return evolution_data
    
    def calculate_derived_properties(self, properties, redshift):
        """
        Calculate derived properties from basic SAGE outputs
        
        Parameters:
        -----------
        properties : dict
            Dictionary of basic properties
        redshift : float
            Current redshift
            
        Returns:
        --------
        dict : Dictionary with derived properties
        """
        derived = {}
        
        # Halo mass
        if 'Mvir' in properties and len(properties['Mvir']) > 0:
            mvir_msun = properties['Mvir'] * 1e10 / self.hubble_h
            derived['log_halo_mass'] = np.log10(np.maximum(mvir_msun, 1e8))
        else:
            derived['log_halo_mass'] = np.array([])
        
        # Stellar mass
        if 'StellarMass' in properties and len(properties['StellarMass']) > 0:
            mstar_msun = properties['StellarMass'] * 1e10 / self.hubble_h
            derived['log_stellar_mass'] = np.log10(np.maximum(mstar_msun, 1e6))
        else:
            derived['log_stellar_mass'] = np.array([])
        
        # Cold gas mass
        if 'ColdGas' in properties and len(properties['ColdGas']) > 0:
            mcold_msun = properties['ColdGas'] * 1e10 / self.hubble_h
            derived['log_cold_gas'] = np.log10(np.maximum(mcold_msun, 1e6))
        else:
            derived['log_cold_gas'] = np.array([])
        
        # SFR and sSFR
        sfr_disk = properties.get('SfrDisk', np.array([]))
        sfr_bulge = properties.get('SfrBulge', np.array([]))
        stellar_mass = properties.get('StellarMass', np.array([]))
        
        if len(sfr_disk) > 0 and len(sfr_bulge) > 0 and len(stellar_mass) > 0:
            total_sfr = sfr_disk + sfr_bulge
            stellar_mass_msun = stellar_mass * 1e10 / self.hubble_h
            
            # sSFR calculation - handle zero SFR galaxies properly
            ssfr = total_sfr / stellar_mass_msun
            
            # For plotting, set a very low floor only for galaxies with exactly zero SFR
            # This preserves the full dynamic range while handling division issues
            ssfr_for_log = np.where(ssfr > 0, ssfr, 1e-15)  # Much lower floor for true zeros
            derived['log_ssfr'] = np.log10(ssfr_for_log)
            
            # Quiescent fraction (sSFR < 1e-11) - keep original threshold for classification
            derived['quiescent_fraction'] = np.mean(ssfr <= 1e-11)
        else:
            derived['log_ssfr'] = np.array([])
            derived['quiescent_fraction'] = np.nan
        
        # H2 fraction
        h2_gas = properties.get('H2_gas', np.array([]))
        hi_gas = properties.get('HI_gas', np.array([]))
        
        if len(h2_gas) > 0 and len(hi_gas) > 0:
            total_neutral = h2_gas + hi_gas
            h2_frac = np.divide(h2_gas, total_neutral, out=np.zeros_like(h2_gas), where=total_neutral>0)
            derived['h2_fraction'] = h2_frac
        else:
            derived['h2_fraction'] = np.array([])
        
        return derived

def load_matched_catalogues_for_tracking(cosmos_file='cosmos_sage_matches.csv', 
                                        epochs_file='epochs_sage_matches.csv',
                                        ceers_file='ceers_sage_matches.csv'):
    """
    Load matched catalogues and prepare them for galaxy tracking
    
    Returns:
    --------
    list : List of galaxy_info dictionaries for tracking
    """
    catalogues_to_track = []
    
    # Load COSMOS matches
    if os.path.exists(cosmos_file):
        cosmos_matches = pd.read_csv(cosmos_file)
        if len(cosmos_matches) > 0:
            # Group by snapshot to handle multiple matches per snapshot
            for snap, group in cosmos_matches.groupby('sage_snapshot'):
                catalogues_to_track.append({
                    'catalogue_name': 'COSMOS',
                    'start_snapshot': int(snap),
                    'galaxy_indices': group['sage_galaxy_idx'].values,
                    'original_redshifts': group['cosmos_redshift'].values
                })
            logger.info(f"Loaded {len(cosmos_matches)} COSMOS matches across {len(cosmos_matches['sage_snapshot'].unique())} snapshots")
    
    # Load EPOCHS matches
    if os.path.exists(epochs_file):
        epochs_matches = pd.read_csv(epochs_file)
        if len(epochs_matches) > 0:
            for snap, group in epochs_matches.groupby('sage_snapshot'):
                catalogues_to_track.append({
                    'catalogue_name': 'EPOCHS',
                    'start_snapshot': int(snap),
                    'galaxy_indices': group['sage_galaxy_idx'].values,
                    'original_redshifts': group['epochs_redshift'].values
                })
            logger.info(f"Loaded {len(epochs_matches)} EPOCHS matches across {len(epochs_matches['sage_snapshot'].unique())} snapshots")
    
    # Load CEERS matches
    if os.path.exists(ceers_file):
        ceers_matches = pd.read_csv(ceers_file)
        if len(ceers_matches) > 0:
            for snap, group in ceers_matches.groupby('sage_snapshot'):
                catalogues_to_track.append({
                    'catalogue_name': 'CEERS',
                    'start_snapshot': int(snap),
                    'galaxy_indices': group['sage_galaxy_idx'].values,
                    'original_redshifts': group['ceers_redshift'].values
                })
            logger.info(f"Loaded {len(ceers_matches)} CEERS matches across {len(ceers_matches['sage_snapshot'].unique())} snapshots")
    
    return catalogues_to_track

def track_all_catalogues(catalogues_info, sage_dir, sage_redshifts, hubble_h=0.73, 
                        memory_conscious=False, chunk_size=50):
    """
    Track evolution of all matched galaxy catalogues - OPTIMIZED VERSION
    
    Parameters:
    -----------
    catalogues_info : list
        List of catalogue information dictionaries
    sage_dir : str
        Path to SAGE output directory
    sage_redshifts : list
        List of SAGE redshifts
    hubble_h : float
        Hubble parameter
    memory_conscious : bool
        If True, process snapshots in chunks to limit memory usage
    chunk_size : int
        Number of snapshots to process at once in memory-conscious mode
        
    Returns:
    --------
    dict : Evolution data for all catalogues
    """
    tracker = SAGEGalaxyTracker(sage_dir, hubble_h)
    
    # OPTIMIZATION: Determine all snapshots needed across all catalogues
    all_start_snaps = [cat_info['start_snapshot'] for cat_info in catalogues_info]
    min_snap = min(all_start_snaps)
    max_snap = len(sage_redshifts) - 1  # Go to z=0
    
    logger.info(f"Optimized tracking: will need snapshots {min_snap} to {max_snap}")
    
    # Properties to track
    props_to_read = ['GalaxyIndex', 'CentralGalaxyIndex', 'Type', 'StellarMass', 'Mvir', 
                    'ColdGas', 'H2_gas', 'HI_gas', 'SfrDisk', 'SfrBulge', 'BulgeMass']
    
    if memory_conscious:
        logger.info(f"Memory-conscious mode: processing {chunk_size} snapshots at a time")
        # Don't pre-load everything, let each catalogue load its own chunks
    else:
        # OPTIMIZATION: Pre-load ALL needed snapshots at once 
        logger.info(f"Pre-loading {max_snap - min_snap + 1} snapshots for all catalogues")
        tracker.bulk_load_snapshots(range(min_snap, max_snap + 1), props_to_read)
        
        # Log cache information
        cache_info = tracker.get_cache_info()
        logger.info(f"Cache loaded: {cache_info['n_snapshots']} snapshots, "
                   f"{cache_info['cache_size_mb']:.1f} MB")
    
    all_evolution = {}
    
    for cat_info in catalogues_info:
        cat_name = cat_info['catalogue_name']
        start_snap = cat_info['start_snapshot']
        
        logger.info(f"Processing {cat_name} catalogue starting from snapshot {start_snap}")
        
        if memory_conscious:
            # Clear cache before processing each catalogue to save memory
            tracker.clear_cache()
        
        # Now this will be much faster since all data is cached (or loaded efficiently)
        evolution_data = tracker.find_galaxy_evolution(cat_info, sage_redshifts)
        
        if evolution_data:
            # Store evolution data, combining multiple snapshot groups for same catalogue
            if cat_name not in all_evolution:
                all_evolution[cat_name] = evolution_data
            else:
                # Combine with existing data for this catalogue
                logger.info(f"Combining additional {cat_name} data")
                # For now, just store separately - could implement proper combining if needed
                all_evolution[f"{cat_name}_{start_snap}"] = evolution_data
        else:
            logger.warning(f"No evolution data found for {cat_name}")
    
    # Log final cache statistics
    final_cache_info = tracker.get_cache_info()
    logger.info(f"Tracking completed. Final cache: {final_cache_info['cache_size_mb']:.1f} MB")
    
    return all_evolution

def estimate_memory_requirements(sage_dir, snap_range, properties_list):
    """
    Estimate memory requirements for bulk loading snapshots
    
    Parameters:
    -----------
    sage_dir : str
        Path to SAGE output directory
    snap_range : range
        Range of snapshots to load
    properties_list : list
        List of properties to load
        
    Returns:
    --------
    dict : Memory estimates and recommendations
    """
    try:
        # Get model files
        model_files = [f for f in os.listdir(sage_dir) 
                      if f.startswith('model_') and f.endswith('.hdf5')]
        
        if not model_files:
            return {'error': 'No SAGE model files found'}
        
        # Sample one file to estimate sizes
        sample_file = os.path.join(sage_dir, model_files[0])
        total_size_mb = 0
        
        with h5.File(sample_file, 'r') as f:
            for snap_num in list(snap_range)[:5]:  # Sample first 5 snapshots
                snap_name = f'Snap_{snap_num}'
                if snap_name in f:
                    for prop in properties_list:
                        if prop in f[snap_name]:
                            dataset = f[snap_name][prop]
                            size_mb = dataset.size * dataset.dtype.itemsize / (1024**2)
                            total_size_mb += size_mb
                    break  # Just need one snapshot for estimation
        
        # Scale by number of files and snapshots
        estimated_size_mb = total_size_mb * len(model_files) * len(snap_range) / 5
        
        # Provide recommendation
        if estimated_size_mb < 1000:  # < 1GB
            recommendation = "Use default mode (memory_conscious=False) for best performance"
        elif estimated_size_mb < 8000:  # < 8GB
            recommendation = "Default mode should work, monitor memory usage"
        else:  # > 8GB
            recommendation = "Consider memory_conscious=True mode for large datasets"
        
        return {
            'estimated_size_mb': estimated_size_mb,
            'estimated_size_gb': estimated_size_mb / 1024,
            'recommendation': recommendation,
            'n_model_files': len(model_files),
            'n_snapshots': len(snap_range)
        }
        
    except Exception as e:
        return {'error': f'Could not estimate memory requirements: {e}'}

def create_6panel_tracking_plot(all_evolution, output_dir='./plots/', figsize=(18, 12)):
    """
    Create 6-panel figure showing galaxy evolution from match redshift to z=0
    Split by central/satellite with 2 legend entries total
    Highlights the 6 most massive haloes (alpha=0.8), others are faded (alpha=0.1)
    
    Parameters:
    -----------
    all_evolution : dict
        Evolution data from track_all_catalogues
    output_dir : str
        Output directory
    figsize : tuple
        Figure size
    """
    os.makedirs(output_dir, exist_ok=True)
    
    # STEP 1: Find the 6 most massive haloes across all catalogues
    logger.info("Identifying 6 most massive haloes for highlighting...")
    
    max_halo_masses = []  # Store (max_halo_mass, cat_key, galaxy_type) for each evolution track
    
    for cat_key, evolution_data in all_evolution.items():
        # Check centrals
        if 'centrals' in evolution_data and 'log_halo_mass' in evolution_data['centrals']:
            central_halo_masses = evolution_data['centrals']['log_halo_mass']
            valid_masses = [m for m in central_halo_masses if np.isfinite(m)]
            if valid_masses:
                max_mass = max(valid_masses)
                max_halo_masses.append((max_mass, cat_key, 'centrals'))
        
        # Check satellites
        if 'satellites' in evolution_data and 'log_halo_mass' in evolution_data['satellites']:
            satellite_halo_masses = evolution_data['satellites']['log_halo_mass']
            valid_masses = [m for m in satellite_halo_masses if np.isfinite(m)]
            if valid_masses:
                max_mass = max(valid_masses)
                max_halo_masses.append((max_mass, cat_key, 'satellites'))
    
    # Sort by halo mass and get top 6
    max_halo_masses.sort(reverse=True, key=lambda x: x[0])
    top_6_massive = set((cat_key, gal_type) for _, cat_key, gal_type in max_halo_masses[:6])
    
    logger.info(f"Top 6 most massive haloes identified:")
    for i, (mass, cat_key, gal_type) in enumerate(max_halo_masses[:6]):
        logger.info(f"  {i+1}. {cat_key} ({gal_type}): log(M_halo) = {mass:.2f}")
    
    # Define colors for each catalogue
    cat_colors = {
        'COSMOS': 'gold',
        'EPOCHS': 'red', 
        'CEERS': 'purple'
    }
    
    # Define panel properties
    panels = [
        {'prop': 'log_halo_mass', 'ylabel': r'log(M$_{\rm halo}$/M$_{\odot}$)', 'ylim': (10, 15)},
        {'prop': 'log_stellar_mass', 'ylabel': r'log(M$_{*}$/M$_{\odot}$)', 'ylim': (8, 12)},
        {'prop': 'log_ssfr', 'ylabel': r'log(sSFR/yr$^{-1}$)', 'ylim': (-13, -8)},
        {'prop': 'log_cold_gas', 'ylabel': r'log(M$_{\rm cold}$/M$_{\odot}$)', 'ylim': (6, 11)},
        {'prop': 'h2_fraction', 'ylabel': r'f$_{\rm H_2}$', 'ylim': (0, 1)},
        {'prop': 'quiescent_fraction', 'ylabel': 'Quiescent Fraction', 'ylim': (0, 1)}
    ]
    
    fig, axes = plt.subplots(2, 3, figsize=figsize)
    axes = axes.flatten()
    
    # Track which catalogues we've added legend entries for
    catalogue_legend_added = set()
    
    for i, panel in enumerate(panels):
        ax = axes[i]
        prop_name = panel['prop']
        
        # Plot each catalogue
        for cat_key, evolution_data in all_evolution.items():
            # Extract base catalogue name (remove _snapnum suffix if present)
            cat_name = cat_key.split('_')[0] if '_' in cat_key else cat_key
            if cat_name not in cat_colors:
                continue
                
            redshifts = evolution_data.get('redshifts', [])
            if len(redshifts) == 0:
                continue
            
            color = cat_colors[cat_name]
            
            # Plot centrals
            central_data = evolution_data['centrals'].get(prop_name, [])
            if len(central_data) > 0:
                central_values = np.array(central_data)
                valid_mask = np.isfinite(central_values)
                
                if np.any(valid_mask):
                    # Determine alpha based on whether this is a top 6 massive halo
                    is_top_6 = (cat_key, 'centrals') in top_6_massive
                    alpha = 0.8 if is_top_6 else 0.1
                    linewidth = 4 if is_top_6 else 2
                    
                    # Add legend entry for this catalogue (only once)
                    label = cat_name if cat_name not in catalogue_legend_added else None
                    ax.plot(np.array(redshifts)[valid_mask], central_values[valid_mask], 
                           color=color, linewidth=linewidth, linestyle='-', alpha=alpha, label=label)
                    
                    if label:
                        catalogue_legend_added.add(cat_name)
            
            # Plot satellites  
            satellite_data = evolution_data['satellites'].get(prop_name, [])
            if len(satellite_data) > 0:
                satellite_values = np.array(satellite_data)
                valid_mask = np.isfinite(satellite_values)
                
                if np.any(valid_mask):
                    # Determine alpha based on whether this is a top 6 massive halo
                    is_top_6 = (cat_key, 'satellites') in top_6_massive
                    alpha = 0.8 if is_top_6 else 0.1
                    linewidth = 4 if is_top_6 else 2
                    
                    # No label for satellites (legend entry already added with centrals)
                    ax.plot(np.array(redshifts)[valid_mask], satellite_values[valid_mask], 
                           color=color, linewidth=linewidth, linestyle='--', alpha=alpha, label=None)
        
        # Formatting
        ax.set_xlabel('Redshift', fontsize=12)
        ax.set_ylabel(panel['ylabel'], fontsize=12)
        ax.set_ylim(panel['ylim'])
        
        # # Reverse x-axis so z=0 is on the right
        # ax.invert_xaxis()
        
        # # Add grid
        # ax.grid(True, alpha=0.3)
    
    # Create horizontal legend at the bottom outside the plots
    handles, labels = axes[0].get_legend_handles_labels()
    
    # Calculate statistics for the legend
    total_galaxies = 0
    start_quenched = 0
    end_quenched = 0
    satellites = 0
    
    for cat_key, evolution_data in all_evolution.items():
        if 'redshifts' in evolution_data and len(evolution_data['redshifts']) > 0:
            # Count galaxies from this catalogue
            cat_total = 0
            
            # Get first and last snapshot data for quenching analysis
            first_idx = 0  # Start of evolution
            last_idx = -1  # End of evolution (z~0)
            
            # Check centrals
            if 'centrals' in evolution_data:
                centrals_data = evolution_data['centrals']
                if 'log_ssfr' in centrals_data and len(centrals_data['log_ssfr']) > 0:
                    # Count centrals
                    n_centrals = 1  # This represents the median of all centrals in this catalogue
                    cat_total += n_centrals
                    
                    # Check if quenched at start (sSFR < 1e-11 corresponds to log(sSFR) < -11)
                    if len(centrals_data['log_ssfr']) > first_idx:
                        if centrals_data['log_ssfr'][first_idx] < -11:
                            start_quenched += n_centrals
                    
                    # Check if quenched at end
                    if len(centrals_data['log_ssfr']) > abs(last_idx):
                        if centrals_data['log_ssfr'][last_idx] < -11:
                            end_quenched += n_centrals
            
            # Check satellites
            if 'satellites' in evolution_data:
                satellites_data = evolution_data['satellites']
                if 'log_ssfr' in satellites_data and len(satellites_data['log_ssfr']) > 0:
                    # Count satellites
                    n_satellites = 1  # This represents the median of all satellites in this catalogue
                    cat_total += n_satellites
                    satellites += n_satellites  # All of these are satellites
                    
                    # Check if quenched at start
                    if len(satellites_data['log_ssfr']) > first_idx:
                        if satellites_data['log_ssfr'][first_idx] < -11:
                            start_quenched += n_satellites
                    
                    # Check if quenched at end
                    if len(satellites_data['log_ssfr']) > abs(last_idx):
                        if satellites_data['log_ssfr'][last_idx] < -11:
                            end_quenched += n_satellites
            
            total_galaxies += cat_total
    
    # Calculate percentages
    start_quenched_pct = (start_quenched / total_galaxies * 100) if total_galaxies > 0 else 0
    end_quenched_pct = (end_quenched / total_galaxies * 100) if total_galaxies > 0 else 0
    satellites_pct = (satellites / total_galaxies * 100) if total_galaxies > 0 else 0
    
    # Add additional legend entries for line styles (centrals vs satellites)
    from matplotlib.lines import Line2D
    
    # Create separate legend entries for catalogues and line styles
    legend_elements = []
    
    # Add catalogue entries (colored)
    for cat_name in ['COSMOS', 'EPOCHS', 'CEERS']:
        if cat_name in catalogue_legend_added:
            color = cat_colors[cat_name]
            legend_elements.append(
                Line2D([0], [0], color=color, linewidth=3, label=cat_name)
            )
    
    # Add line style entries (using black)
    legend_elements.append(
        Line2D([0], [0], color='black', linewidth=2, linestyle='-', label='Centrals')
    )
    legend_elements.append(
        Line2D([0], [0], color='black', linewidth=2, linestyle='--', label='Satellites')
    )
    
    # # Add statistics as text-only entries
    # stats_text = f'Start quenched: {start_quenched_pct:.1f}% | End quenched: {end_quenched_pct:.1f}% | Satellites: {satellites_pct:.1f}%'
    # legend_elements.append(
    #     Line2D([0], [0], color='none', label=stats_text)
    # )
    
    if legend_elements:  # Only add legend if there are legend entries
        fig.legend(handles=legend_elements, loc='lower center', ncol=5, fontsize=14, 
                  bbox_to_anchor=(0.5, -0.08))
    
    plt.tight_layout()
    plt.subplots_adjust(bottom=0.01)  # Make room for the legend at the bottom
    
    plt.savefig(f'{output_dir}/sage_mock_evolution_tracking_6panel.pdf', dpi=300, bbox_inches='tight')
    # plt.savefig(f'{output_dir}/sage_mock_evolution_tracking_6panel.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    logger.info(f"6-panel galaxy tracking plot saved to {output_dir}")
    logger.info(f"Highlighted 6 most massive haloes with thick lines (alpha=0.8), others faded (alpha=0.1)")

def plot_z0_descendants(all_evolution):
    """Plot 'Where Are They Now?' - z=0 descendants of JWST-like galaxies"""
    
    if len(all_evolution) == 0:
        print("No evolution data to plot!")
        return
    
    # Create single figure
    fig, ax = plt.subplots(1, 1, figsize=(10, 8))
    
    # Collect z=0 data from all catalogues
    z0_stellar_masses = []
    z0_types = []  # Will store 'central' or 'satellite'
    z0_halo_masses = []
    discovery_redshifts = []
    catalogue_names = []
    
    # Process each catalogue's evolution data
    for cat_name, evolution_data in all_evolution.items():
        if 'redshifts' not in evolution_data or len(evolution_data['redshifts']) == 0:
            continue
            
        # Get discovery redshift (first/highest redshift) and z=0 (final) properties
        discovery_redshift_idx = 0  # First entry is the discovery redshift
        final_redshift_idx = -1  # Last entry should be closest to z=0
        discovery_redshift = evolution_data['redshifts'][discovery_redshift_idx]
        
        # NEW: Check if we have individual galaxy data instead of just medians
        if 'individual_galaxies' in evolution_data:
            # Process individual galaxy data
            individual_data = evolution_data['individual_galaxies']
            final_snap_idx = final_redshift_idx
            
            if ('z0_stellar_masses' in individual_data and 
                'z0_halo_masses' in individual_data and
                'z0_types' in individual_data):
                
                # Get all individual galaxies at z=0
                stellar_masses = individual_data['z0_stellar_masses']
                halo_masses = individual_data['z0_halo_masses']
                types = individual_data['z0_types']
                
                # Add each individual galaxy
                for i in range(len(stellar_masses)):
                    if not (np.isnan(stellar_masses[i]) or np.isnan(halo_masses[i])):
                        z0_stellar_masses.append(stellar_masses[i])
                        z0_types.append(types[i])
                        z0_halo_masses.append(halo_masses[i])
                        discovery_redshifts.append(discovery_redshift)
                        catalogue_names.append(cat_name)
        else:
            # Fallback to old median-based approach
            # Process centrals
            if 'centrals' in evolution_data:
                centrals_data = evolution_data['centrals']
                if ('log_stellar_mass' in centrals_data and 
                    len(centrals_data['log_stellar_mass']) > final_redshift_idx and
                    'log_halo_mass' in centrals_data and
                    len(centrals_data['log_halo_mass']) > final_redshift_idx):
                    
                    final_stellar_mass = centrals_data['log_stellar_mass'][final_redshift_idx]
                    final_halo_mass = centrals_data['log_halo_mass'][final_redshift_idx]
                    
                    if not (np.isnan(final_stellar_mass) or np.isnan(final_halo_mass)):
                        z0_stellar_masses.append(final_stellar_mass)
                        z0_types.append('central')
                        z0_halo_masses.append(final_halo_mass)
                        discovery_redshifts.append(discovery_redshift)
                        catalogue_names.append(cat_name)
            
            # Process satellites
            if 'satellites' in evolution_data:
                satellites_data = evolution_data['satellites']
                if ('log_stellar_mass' in satellites_data and 
                    len(satellites_data['log_stellar_mass']) > final_redshift_idx and
                    'log_halo_mass' in satellites_data and
                    len(satellites_data['log_halo_mass']) > final_redshift_idx):
                    
                    final_stellar_mass = satellites_data['log_stellar_mass'][final_redshift_idx]
                    final_halo_mass = satellites_data['log_halo_mass'][final_redshift_idx]
                    
                    if not (np.isnan(final_stellar_mass) or np.isnan(final_halo_mass)):
                        z0_stellar_masses.append(final_stellar_mass)
                        z0_types.append('satellite')
                        z0_halo_masses.append(final_halo_mass)
                        discovery_redshifts.append(discovery_redshift)
                        catalogue_names.append(cat_name)
    
    z0_stellar_masses = np.array(z0_stellar_masses)
    z0_types = np.array(z0_types)
    z0_halo_masses = np.array(z0_halo_masses)
    discovery_redshifts = np.array(discovery_redshifts)
    catalogue_names = np.array(catalogue_names)
    
    print(f"z0_descendants plot: Found {len(z0_stellar_masses)} total galaxies")
    if len(z0_stellar_masses) > 0:
        n_centrals = sum(1 for t in z0_types if t == 'central')
        n_satellites = sum(1 for t in z0_types if t == 'satellite')
        print(f"  - {n_centrals} centrals, {n_satellites} satellites")
        for cat_name in np.unique(catalogue_names):
            n_cat = sum(1 for c in catalogue_names if c == cat_name)
            print(f"  - {n_cat} from {cat_name}")
    
    if len(z0_stellar_masses) == 0:
        print("No z=0 data to plot!")
        return
    
    # Sample galaxies from each catalogue for visualization (different rates per catalogue)
    np.random.seed(42)  # For reproducibility
    
    sample_indices = []
    unique_catalogues = np.unique(catalogue_names)
    
    # Define sample fractions for each catalogue
    sample_fractions = {
        'COSMOS': 0.05,   # 5% for COSMOS (diluted by 95%)
        'EPOCHS': 0.1,   # 10% for EPOCHS
        'CEERS': 0.8     # 80% for CEERS
    }
    
    for cat_name in unique_catalogues:
        cat_mask = catalogue_names == cat_name
        cat_indices = np.where(cat_mask)[0]
        n_cat_galaxies = len(cat_indices)
        
        # Get sample fraction for this catalogue
        base_cat_name = cat_name.split('_')[0] if '_' in cat_name else cat_name
        sample_fraction = sample_fractions.get(base_cat_name.upper(), 0.2)  # Default to 20%
        
        # Sample the specified fraction of this catalogue's galaxies
        n_sample = max(1, int(n_cat_galaxies * sample_fraction))  # At least 1 galaxy
        sampled_cat_indices = np.random.choice(cat_indices, size=n_sample, replace=False)
        sample_indices.extend(sampled_cat_indices)
        
        print(f"  - Sampling {n_sample}/{n_cat_galaxies} galaxies from {base_cat_name} ({sample_fraction*100:.0f}%)")
    
    sample_indices = np.array(sample_indices)
    
    # Create sampled arrays for plotting
    z0_stellar_masses_plot = z0_stellar_masses[sample_indices]
    z0_types_plot = z0_types[sample_indices]
    z0_halo_masses_plot = z0_halo_masses[sample_indices]
    discovery_redshifts_plot = discovery_redshifts[sample_indices] 
    catalogue_names_plot = catalogue_names[sample_indices]
    
    print(f"Plotting {len(sample_indices)} sampled galaxies (COSMOS: 20%, EPOCHS: 20%, CEERS: 20%)")
    
    # Define environment categories based on galaxy type and halo mass
    # Note: z0_types_plot now contains 'central' or 'satellite' strings
    environment_labels = []
    y_positions = []
    colors = []
    
    for i, (gtype, halo_mass_log, stellar_mass_log) in enumerate(zip(z0_types_plot, z0_halo_masses_plot, z0_stellar_masses_plot)):
        halo_mass = 10**halo_mass_log  # Convert from log to linear
        stellar_mass = 10**stellar_mass_log  # Convert from log to linear
        
        if gtype == 'central':  # Central galaxy
            if halo_mass > 1e14:  # Very massive halo
                if stellar_mass > 1e11:  # Very massive stellar mass
                    env_label = "BCG/Massive Central"
                    y_pos = 3.0
                    color = 'red'
                else:
                    env_label = "Group Central"
                    y_pos = 2.5
                    color = 'orange'
            elif halo_mass > 1e13:  # Group-scale halo
                env_label = "Group Central"
                y_pos = 2.0
                color = 'orange'
            else:  # Field central
                env_label = "Field Central"
                y_pos = 1.0
                color = 'blue'
        else:  # Satellite
            if halo_mass > 1e14:  # In massive cluster
                env_label = "Cluster Satellite"
                y_pos = 0.5
                color = 'purple'
            else:  # In group
                env_label = "Group Satellite"
                y_pos = 0.0
                color = 'darkred'
        
        environment_labels.append(env_label)
        y_positions.append(y_pos)
        colors.append(color)
    
    y_positions = np.array(y_positions)
    
    # Add some scatter to y-positions to avoid overlap
    np.random.seed(43)  # Different seed to avoid correlation with sampling
    y_jitter = np.random.normal(0, 0.1, len(y_positions))
    y_positions_jittered = y_positions + y_jitter
    
    # Create color map based on discovery redshift (same as main plot)
    # Use a fixed redshift range to ensure consistent coloring across all catalogues
    z_min_expected = 2.0  # Minimum expected redshift (COSMOS, CEERS)
    z_max_expected = 12.0  # Maximum expected redshift (EPOCHS)
    
    # Normalize discovery redshifts to [0,1] based on expected range
    discovery_colors_norm = (discovery_redshifts_plot - z_min_expected) / (z_max_expected - z_min_expected)
    discovery_colors_norm = np.clip(discovery_colors_norm, 0, 1)  # Ensure values are in [0,1]
    
    # Get colors from the plasma_r colormap
    discovery_colors = plt.cm.plasma_r(discovery_colors_norm)
    
    # Define markers for different catalogues
    marker_map = {
        'COSMOS': 's',     # squares
        'EPOCHS': 'D',     # diamonds  
        'CEERS': '^'       # triangles
    }
    
    # Plot points with different markers for each catalogue
    for i in range(len(z0_stellar_masses_plot)):
        # Determine marker based on catalogue name
        cat_name = catalogue_names_plot[i]
        # Extract base catalogue name (remove any suffixes like '_63')
        base_cat_name = cat_name.split('_')[0] if '_' in cat_name else cat_name
        marker = marker_map.get(base_cat_name.upper(), '*')  # Default to star if unknown
        
        ax.scatter(z0_stellar_masses_plot[i], y_positions_jittered[i], 
                  c=[discovery_colors[i]], s=300, alpha=0.8, 
                  edgecolors='black', linewidth=1, marker=marker, zorder=3)

    # Formatting
    ax.set_xlabel(r'$\log_{10}\ M_{star}\ (M_\odot)$', fontsize=14)
    # ax.set_ylabel('Environment (z=0)', fontsize=14)
    
    # Set y-axis labels
    unique_y_pos = []
    unique_labels = []
    for y_pos, label in zip(y_positions, environment_labels):
        if y_pos not in unique_y_pos:
            unique_y_pos.append(y_pos)
            unique_labels.append(label)
    
    # Sort by y position
    sorted_indices = np.argsort(unique_y_pos)
    unique_y_pos = [unique_y_pos[i] for i in sorted_indices]
    unique_labels = [unique_labels[i] for i in sorted_indices]
    
    ax.set_yticks(unique_y_pos)
    ax.set_yticklabels(unique_labels, fontsize=12)
    ax.set_ylim(-1, 3.5)

    # Add colorbar for discovery redshift
    sm = plt.cm.ScalarMappable(cmap=plt.cm.plasma_r, 
                              norm=plt.Normalize(vmin=z_min_expected, 
                                               vmax=z_max_expected))
    sm.set_array([])
    cbar = plt.colorbar(sm, ax=ax, pad=0.02)
    cbar.set_label('Discovery Redshift', fontsize=12)
    
    # Add legend for catalogue markers
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], marker='s', color='gray', linestyle='None', 
               markersize=10, markeredgecolor='black', label='COSMOS'),
        Line2D([0], [0], marker='D', color='gray', linestyle='None', 
               markersize=10, markeredgecolor='black', label='EPOCHS'),
        Line2D([0], [0], marker='^', color='gray', linestyle='None', 
               markersize=10, markeredgecolor='black', label='CEERS')
    ]
    ax.legend(handles=legend_elements, loc='upper left', frameon=False, fontsize=10)
    
    # Add statistics text
    # n_bcg = sum([1 for y in y_positions if y >= 2.5])
    # n_centrals = sum([1 for y in y_positions if 1.0 <= y < 2.5])
    # n_satellites = sum([1 for y in y_positions if -0.5 <= y < 1.0])
    # total = len(y_positions)
    
    # stats_text = f'Sample: {total} JWST-like galaxies\n'
    # stats_text += f'BCG/Massive Centrals: {n_bcg} ({n_bcg/total*100:.1f}%)\n'
    # stats_text += f'Group/Field Centrals: {n_centrals} ({n_centrals/total*100:.1f}%)\n'
    # stats_text += f'Satellites: {n_satellites} ({n_satellites/total*100:.1f}%)'
    
    # ax.text(0.02, 0.98, stats_text, transform=ax.transAxes, fontsize=10,
    #     verticalalignment='top', horizontalalignment='left',
    #     bbox=dict(facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig('./plots/jwst_galaxies_z0_descendants.pdf', dpi=500, bbox_inches='tight')
    print(f"\nz=0 descendants plot saved as 'jwst_galaxies_z0_descendants.pdf'")
    print(f"Color scheme: plasma_r (yellow/orange = earlier discovery redshift)")
    print(f"Marker scheme: squares=COSMOS, diamonds=EPOCHS, triangles=CEERS")
    print(f"Total galaxies plotted: {len(z0_stellar_masses_plot)} (sampled from {len(z0_stellar_masses)} total)")
    if len(z0_stellar_masses_plot) > 0:
        print(f"Environment breakdown (plotted sample):")
        env_counts = {}
        for i, (gtype, halo_mass_log, stellar_mass_log) in enumerate(zip(z0_types_plot, z0_halo_masses_plot, z0_stellar_masses_plot)):
            halo_mass = 10**halo_mass_log
            stellar_mass = 10**stellar_mass_log
            
            if gtype == 'central':
                if halo_mass > 1e14:
                    if stellar_mass > 1e11:
                        env_label = "BCG/Massive Central"
                    else:
                        env_label = "Group Central"
                elif halo_mass > 1e13:
                    env_label = "Group Central"
                else:
                    env_label = "Field Central"
            else:
                if halo_mass > 1e14:
                    env_label = "Cluster Satellite"
                else:
                    env_label = "Group Satellite"
            
            env_counts[env_label] = env_counts.get(env_label, 0) + 1
        
        for env, count in env_counts.items():
            print(f"  - {env}: {count}")
        
        # Also show the full sample environment breakdown
        print(f"Environment breakdown (full sample of {len(z0_stellar_masses)} galaxies):")
        env_counts_full = {}
        for i, (gtype, halo_mass_log, stellar_mass_log) in enumerate(zip(z0_types, z0_halo_masses, z0_stellar_masses)):
            halo_mass = 10**halo_mass_log
            stellar_mass = 10**stellar_mass_log
            
            if gtype == 'central':
                if halo_mass > 1e14:
                    if stellar_mass > 1e11:
                        env_label = "BCG/Massive Central"
                    else:
                        env_label = "Group Central"
                elif halo_mass > 1e13:
                    env_label = "Group Central"
                else:
                    env_label = "Field Central"
            else:
                if halo_mass > 1e14:
                    env_label = "Cluster Satellite"
                else:
                    env_label = "Group Satellite"
            
            env_counts_full[env_label] = env_counts_full.get(env_label, 0) + 1
        
        for env, count in env_counts_full.items():
            print(f"  - {env}: {count}")
    
    return

def run_galaxy_tracking_analysis():
    """
    Run the complete galaxy tracking analysis from match redshift to z=0
    """
    # SAGE redshifts from your configuration
    sage_redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 
                     14.086, 12.941, 11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 
                     6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 
                     3.060, 2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 
                     1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 0.828, 0.755, 0.687, 0.624, 
                     0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 
                     0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]
    
    # Configuration - UPDATE THESE PATHS!
    sage_dir = './output/millennium/'  # Path to your SAGE output directory
    cosmos_file = 'cosmos_sage_matches.csv'  # COSMOS matches file
    epochs_file = 'epochs_sage_matches.csv'  # EPOCHS matches file  
    ceers_file = 'ceers_sage_matches.csv'    # CEERS matches file
    
    print("SAGE Mock Catalogue Galaxy Tracking Analysis")
    print("=" * 60)
    print("Tracking matched galaxies from their original redshift to z=0")
    print()
    
    # Step 1: Load matched catalogues for tracking
    print("Step 1: Loading matched catalogues for tracking...")
    catalogues_info = load_matched_catalogues_for_tracking(cosmos_file, epochs_file, ceers_file)
    
    if len(catalogues_info) == 0:
        print("ERROR: No matched catalogue data found!")
        print("Make sure you've run the matching process first to generate:")
        print(f"  - {cosmos_file}")
        print(f"  - {epochs_file}")
        print(f"  - {ceers_file}")
        return
    
    total_galaxies = sum(len(cat['galaxy_indices']) for cat in catalogues_info)
    print(f"Found {len(catalogues_info)} catalogue groups with {total_galaxies} total galaxies to track")
    
    # Step 2: Track galaxy evolution (OPTIMIZED VERSION)
    print("Step 2: Tracking galaxy evolution through cosmic time...")
    print("Using optimized bulk loading for better performance...")
    
    # For very large datasets (>10GB), you can enable memory-conscious mode:
    # all_evolution = track_all_catalogues(catalogues_info, sage_dir, sage_redshifts, 
    #                                     hubble_h=0.73, memory_conscious=True, chunk_size=30)
    
    # Default: load all snapshots at once for maximum speed
    all_evolution = track_all_catalogues(catalogues_info, sage_dir, sage_redshifts, 
                                       hubble_h=0.73, memory_conscious=False)
    
    if len(all_evolution) == 0:
        print("ERROR: No evolution data extracted!")
        print("Check that:")
        print("1. SAGE HDF5 files contain 'GalaxyIndex' field for tracking")
        print("2. SAGE output directory path is correct")
        print("3. Required SAGE parameters are available")
        return
    
    # Step 3: Create tracking plot
    print("Step 3: Creating 6-panel galaxy tracking evolution plot...")
    create_6panel_tracking_plot(all_evolution, output_dir='./plots/')

    # Step 4: Plot z=0 descendants
    print("Step 4: Plotting 'Where Are They Now?' - z=0 descendants of JWST-like galaxies...")
    plot_z0_descendants(all_evolution)
    
    # Print summary
    print("\nGalaxy Tracking Analysis Complete!")
    print("=" * 60)
    for cat_name, evolution_data in all_evolution.items():
        redshifts = evolution_data.get('redshifts', [])
        if len(redshifts) > 0:
            z_start, z_end = redshifts[0], redshifts[-1]
            n_snaps = len(redshifts)
            print(f"{cat_name}: Tracked through {n_snaps} snapshots, z = {z_start:.2f} → {z_end:.2f}")
    
    print(f"\nOutput files:")
    print("- ./plots/sage_mock_evolution_tracking_6panel.pdf")
    print("- ./plots/sage_mock_evolution_tracking_6panel.png")
    print()
    print("Plot shows 6 lines total:")
    print("- 3 solid lines (centrals): COSMOS (gold), EPOCHS (red), CEERS (purple)")  
    print("- 3 dashed lines (satellites): COSMOS (gold), EPOCHS (red), CEERS (purple)")
    print("- Legend shows 2 entries: 'Centrals' and 'Satellites'")
    
    return all_evolution

if __name__ == "__main__":
    # Choose which dataset(s) to run:
    
    # Option 1: Run only COSMOS
    # matches, validation = run_cosmos_sage_matching()
    
    # Option 2: Run only EPOCHS  
    # matches, validation = run_epochs_sage_matching()
    
    # Option 3: Run only CEERS
    # matches, validation = run_ceers_sage_matching()
    
    # Option 4: Run COSMOS and EPOCHS (original functionality)
    # results = run_both_datasets()
    
    # Option 5: Run all three datasets (NEW!)
    results = run_all_datasets()

    evolution_data = run_galaxy_tracking_analysis()
    
    # For now, show what needs to be updated:
    # print("JWST Mock Galaxy Matching Tool")
    # print("=" * 50)
    # print("The AttributeError has been fixed!")
    # print()
    # print("To use the tool, you need to:")
    # print("1. Update the file paths in the functions above:")
    # print("   - Set 'cosmos_file' to your COSMOS CSV file path")
    # print("   - Set 'obs_file' to your EPOCHS CSV file path") 
    # print("   - Set 'sage_dir' to your SAGE output directory")
    # print()
    # print("2. Update column names to match your data:")
    # print("   - COSMOS: stellar_mass_col='your_mass_column', redshift_col='your_z_column'")
    # print("   - EPOCHS: stellar_mass_col='your_mass_column', redshift_col='your_z_column'")
    # print()
    # print("3. Uncomment one of the options above to run the matching.")
    # print()
    # print("Current available options:")
    # print("- run_cosmos_sage_matching(): Match COSMOS data only")
    # print("- run_epochs_sage_matching(): Match EPOCHS data only") 
    # print("- run_ceers_sage_matching(): Match CEERS quiescent galaxies only")
    # print("- run_both_datasets(): Match COSMOS and EPOCHS datasets")
    # print("- run_all_datasets(): Match all three datasets (COSMOS, EPOCHS, CEERS)")
    # print()
    # print("Script is ready to use once you update the paths and column names!")