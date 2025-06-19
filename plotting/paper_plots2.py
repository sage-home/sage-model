import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
import time
from matplotlib.ticker import ScalarFormatter
from random import sample, seed
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor, as_completed
import multiprocessing as mp
import gc  # For garbage collection
import psutil  # For memory monitoring
from functools import partial
from pathlib import Path
import logging
import pandas as pd

import warnings
warnings.filterwarnings("ignore")

# Set up logging for better debugging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# ========================== PLOTTING STYLE CONFIGURATION ==========================

def setup_paper_style():
    """Configure matplotlib for consistent paper-quality plots"""
    plt.style.use('default')
    
    # Figure settings
    plt.rcParams['figure.figsize'] = (5, 5)
    plt.rcParams['figure.dpi'] = 500
    plt.rcParams['savefig.dpi'] = 500
    plt.rcParams['savefig.bbox'] = 'tight'
    plt.rcParams['savefig.transparent'] = False
    
    # Font settings
    plt.rcParams['font.family'] = 'serif'
    plt.rcParams['font.serif'] = ['Times New Roman', 'DejaVu Serif', 'Computer Modern Roman']
    plt.rcParams['font.size'] = 16
    
    # Axis label settings
    plt.rcParams['axes.labelsize'] = 16
    plt.rcParams['axes.labelweight'] = 'normal'
    plt.rcParams['axes.labelpad'] = 8
    
    # Tick settings
    plt.rcParams['xtick.labelsize'] = 16
    plt.rcParams['ytick.labelsize'] = 16    
    plt.rcParams['xtick.major.size'] = 6
    plt.rcParams['ytick.major.size'] = 6
    plt.rcParams['xtick.minor.size'] = 3
    plt.rcParams['ytick.minor.size'] = 3
    plt.rcParams['xtick.major.width'] = 1.2
    plt.rcParams['ytick.major.width'] = 1.2
    plt.rcParams['xtick.minor.width'] = 0.8
    plt.rcParams['ytick.minor.width'] = 0.8
    plt.rcParams['xtick.direction'] = 'in'
    plt.rcParams['ytick.direction'] = 'in'
    plt.rcParams['xtick.top'] = True
    plt.rcParams['ytick.right'] = True
    plt.rcParams['xtick.minor.visible'] = True
    plt.rcParams['ytick.minor.visible'] = True
    
    # Axis settings
    plt.rcParams['axes.linewidth'] = 1.2
    plt.rcParams['axes.spines.left'] = True
    plt.rcParams['axes.spines.bottom'] = True
    plt.rcParams['axes.spines.top'] = True
    plt.rcParams['axes.spines.right'] = True
    
    # Line settings
    plt.rcParams['lines.linewidth'] = 2.0
    plt.rcParams['lines.markersize'] = 8
    plt.rcParams['lines.markeredgewidth'] = 1.0
    
    # Legend settings
    plt.rcParams['legend.fontsize'] = 16
    plt.rcParams['legend.frameon'] = False
    plt.rcParams['legend.columnspacing'] = 1.0
    plt.rcParams['legend.handlelength'] = 2.0
    plt.rcParams['legend.handletextpad'] = 0.5
    plt.rcParams['legend.labelspacing'] = 0.3
    
    # Math text settings
    plt.rcParams['mathtext.fontset'] = 'stix'
    plt.rcParams['mathtext.default'] = 'regular'

def create_figure(figsize=(10, 8)):
    """Create a standardized figure with consistent styling"""
    fig, ax = plt.subplots(figsize=figsize)
    ax.tick_params(which='both', direction='in', top=True, right=True)
    return fig, ax

def style_legend(ax, loc='best', ncol=1, fontsize=16, frameon=False, **kwargs):
    """Create a consistently styled legend"""
    legend = ax.legend(loc=loc, ncol=ncol, fontsize=fontsize, frameon=frameon, **kwargs)
    return legend

def finalize_plot(fig, filename):
    """Finalize and save plot with consistent settings"""
    plt.tight_layout()
    fig.savefig(filename, dpi=500, bbox_inches='tight')
    logger.info(f'Plot saved as: {filename}')
    plt.close(fig)

# Color scheme for consistency
PLOT_COLORS = {
    'model_main': '#1f77b4',
    'model_alt': '#ff7f0e', 
    'model_ref': '#2ca02c',
    'observations': '#d62728',
    'theory': '#9467bd',
    'centrals': '#1f77b4',
    'satellites': "#d62728",
    'all_galaxies': "#030603",
    'cold_gas': '#7f7f7f',
    'h1_gas': '#1f77b4',
    'h2_gas': '#ff7f0e',
    'millennium': '#000000',
    'miniuchuu': '#1f77b4',
    'c16_millennium': '#000000',
    # Reionization model colors
    'enhanced_gnedin': '#1f77b4',  # blue
    'sobacchi_mesinger': '#d62728',  # red  
    'patchy_reionization': '#2ca02c',  # green
    'original_gnedin': '#000000'  # black
}

# ========================== USER OPTIONS ==========================

# File details for the main analysis (mass loading plot)
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'
OutputDir = DirName + 'plots/'

# Main simulation details (for mass loading plot only)
Main_Hubble_h = 0.73        # Hubble parameter
Main_BoxSize = 62.5         # h-1 Mpc
Main_VolumeFraction = 1.0  # Fraction of the full volume output by the model

# Additional simulation directories for SFR density comparison
SFR_SimDirs = [
    {
        'path': './output/millennium/', 
        'label': 'Millennium', 
        'color': 'black', 
        'linestyle': '-',
        'BoxSize': 62.5,  # h-1 Mpc
        'Hubble_h': 0.73,
        'VolumeFraction': 1.0,
        'FirstSnap': 0,
        'LastSnap': 63,
        'redshifts': [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                     9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                     2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                     0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                     0.116, 0.089, 0.064, 0.041, 0.020, 0.000],
        'SMFsnaps': [63, 40, 32, 27, 23, 20, 18, 16],
        'BHMFsnaps': [63, 40, 32, 27, 23, 20, 18, 16]
    },
    {
        'path': './output/miniuchuu_full/', 
        'label': 'miniUchuu', 
        'color': 'blue', 
        'linestyle': '--',
        'BoxSize': 400,  # h-1 Mpc  (example - adjust to your actual values)
        'Hubble_h': 0.677,  # (example - adjust to your actual values)
        'VolumeFraction': 1.0,
        'FirstSnap': 0,  # (adjust to your actual values)
        'LastSnap': 49,  # (adjust to your actual values)
        'redshifts': [13.9334, 12.67409, 11.50797, 10.44649, 9.480752, 8.58543, 7.77447, 7.032387, 6.344409, 5.721695,
            5.153127, 4.629078, 4.26715, 3.929071, 3.610462, 3.314082, 3.128427, 2.951226, 2.77809, 2.616166,
            2.458114, 2.309724, 2.16592, 2.027963, 1.8962, 1.770958, 1.65124, 1.535928, 1.426272, 1.321656,
            1.220303, 1.124166, 1.031983, 0.9441787, 0.8597281, 0.779046, 0.7020205, 0.6282588, 0.5575475, 0.4899777,
            0.4253644, 0.3640053, 0.3047063, 0.2483865, 0.1939743, 0.1425568, 0.09296665, 0.0455745, 0.02265383, 0.0001130128],
        'SMFsnaps': [49, 38, 32, 23, 17, 13, 10, 8, 7, 5, 4],  # Example snapshots
        'BHMFsnaps': [49, 38, 32, 23, 17, 13, 10, 8, 7, 5, 4]  # Example snapshots
    },
    {
        'path': './output/millennium_vanilla/', 
        'label': ' SAGE (C16) Millennium', 
        'color': 'black', 
        'linestyle': ':',
        'BoxSize': 62.5,  # h-1 Mpc  (example - adjust to your actual values)
        'Hubble_h': 0.73,  # (example - adjust to your actual values)
        'VolumeFraction': 1.0,
        'FirstSnap': 0,  # (adjust to your actual values)
        'LastSnap': 63,  # (adjust to your actual values)
        'redshifts': [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                     9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                     2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                     0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                     0.116, 0.089, 0.064, 0.041, 0.020, 0.000],
        'SMFsnaps': [63, 40, 32, 27, 23, 20, 18, 16],
        'BHMFsnaps': [63, 40, 32, 27, 23, 20, 18, 16]
    }
]

# Define simulation configurations for SMF comparison
SMF_SimConfigs = [
    # SAGE 2.0 simulations (solid lines)
    {
        'path': './output/millennium/', 
        'label': 'SAGE 2.0', 
        'color': PLOT_COLORS['millennium'], 
        'linestyle': '-',  # solid line
        'BoxSize': 62.5,  # h-1 Mpc
        'Hubble_h': 0.73,
        'VolumeFraction': 1.0
    },
    {
        'path': './output/miniuchuu_full/', 
        'label': 'miniUchuu ', 
        'color': 'darkred', 
        'linestyle': '-',  # solid line
        'BoxSize': 62.5,  # h-1 Mpc
        'Hubble_h': 0.73,
        'VolumeFraction': 1.0
    },
    # Vanilla SAGE simulation (dashed lines)
    {
        'path': './output/millennium_vanilla/', 
        'label': 'SAGE (C16)', 
        'color': PLOT_COLORS['c16_millennium'], 
        'linestyle': '--',  # dashed line
        'BoxSize': 62.5,  # h-1 Mpc
        'Hubble_h': 0.73,
        'VolumeFraction': 1.0
    }
]

GAS_SimConfigs = [
    # Main simulation (your current one)
    {
        'path': './output/millennium/', 
        'label': 'SAGE 2.0', 
        'color': PLOT_COLORS['millennium'], 
        'linestyle': '-',
        'BoxSize': 62.5,
        'Hubble_h': 0.73,
        'VolumeFraction': 1.0,
        'linewidth': 3,
        'alpha': 0.9
    },
    # Your second model
    {
        'path': './output/millennium_br/', 
        'label': 'Pressure-based model', 
        'color': PLOT_COLORS['model_main'], 
        'linestyle': '--',
        'BoxSize': 62.5,  # adjust to your actual values
        'Hubble_h': 0.73,  # adjust to your actual values
        'VolumeFraction': 1.0,
        'linewidth': 2,
        'alpha': 0.8
    }
]

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7000       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

# Main simulation snapshot details (for mass loading plot)
FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot
SMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF
BHMFsnaps = [63, 40, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF

OutputFormat = '.pdf'

# Enhanced performance optimization options
USE_PARALLEL = True  # Enable parallel processing
MAX_WORKERS = min(32, mp.cpu_count())  # Increased for larger datasets
AGGRESSIVE_CLEANUP = True  # Force garbage collection between operations
SMART_SAMPLING = True  # Enable smart sampling for large datasets
MIN_SNAPSHOTS_FOR_SAMPLING = 50  # Use sampling if more than this many snapshots
CHUNK_SIZE = 10  # Process snapshots in chunks to manage memory

# Memory management
MAX_MEMORY_GB = 32  # Maximum memory to use (adjust based on your system)
MEMORY_CHECK_INTERVAL = 5  # Check memory every N snapshots

# HDF5 optimization
HDF5_CACHE_SIZE = 64 * 1024 * 1024  # 64MB cache per file
USE_MEMMAP = True  # Use memory mapping for large arrays

# Data caching
ENABLE_CACHING = True
CACHE_DIR = './cache/'

# ========================== REIONIZATION MODEL FUNCTIONS ==========================

def calculate_filtering_mass(z, run_params):
    """From calculate_filtering_mass() in model_infall.c"""
    Mfilt_zr = run_params['FilteringMassNorm']  # 0.7 in 10^10 Msun/h
    
    if z >= run_params['Reionization_zr']:
        result = 0.1 * Mfilt_zr
    elif z >= run_params['Reionization_z0']:
        z_frac = (z - run_params['Reionization_z0']) / (run_params['Reionization_zr'] - run_params['Reionization_z0'])
        result = Mfilt_zr * (10**(-3.0 * z_frac))
    else:
        alpha = run_params['PostReionSlope']  # -2/3 default
        result = Mfilt_zr * ((1.0 + z)/(1.0 + run_params['Reionization_z0']))**alpha
    
    return result

def enhanced_gnedin_model(Mvir, z, run_params):
    """From enhanced_gnedin_model() in model_infall.c"""
    Mfilt = calculate_filtering_mass(z, run_params)
    ratio = 2.0 * Mfilt / Mvir
    result = 1.0 / (1.0 + ratio**2.0)**1.5
    return result

def original_gnedin_model(Mvir, z, run_params):
    """Original SAGE Gnedin model - simplified version"""
    Mfilt = calculate_filtering_mass(z, run_params)
    ratio = 0.26 * Mfilt / Mvir
    result = 1.0 / (1.0 + ratio)**3
    return result

def sobacchi_mesinger_model(Mvir, z, run_params):
    """Simplified version focusing on the key physics"""
    Mc0 = run_params['FilteringMassNorm']
    zion = run_params['Reionization_zr']
    
    if z <= zion:
        # Simplified critical mass evolution
        Mc = Mc0 * ((1.0 + z)/(1.0 + zion))**(-0.5)
    else:
        Mc = 0.1 * Mc0
    
    x = Mvir / Mc
    result = (1.0 + (3.0*x)**(-0.7))**(-2.5)
    return result

def patchy_reionization_model(Mvir, z, run_params):
    """From patchy_reionization_model() in model_infall.c"""
    # Calculate local reionization redshift using halo mass as density proxy
    base_zreion = run_params['Reionization_zr']
    variance = run_params['LocalReionVariance']  # 0.5 default
    
    # M_avg from your code: 1.0 * (1 + z)^(-2.5) in 10^10 Msun/h
    M_avg = 1.0 * (1.0 + base_zreion)**(-2.5)
    density_factor = np.log10(Mvir / M_avg)
    density_factor = np.clip(density_factor, -1.0, 1.0)
    
    # Local reionization redshift
    z_reion_local = base_zreion + 2.0 * variance * density_factor
    dz = run_params['PatchyReionWidth']  # 1.0 default
    
    if z <= z_reion_local - dz:
        # Fully reionized - use Sobacchi & Mesinger
        return sobacchi_mesinger_model(Mvir, z_reion_local - dz, run_params)
    elif z >= z_reion_local + dz:
        # Not yet reionized
        return 0.95
    else:
        # During transition
        phase = (z_reion_local + dz - z) / (2.0 * dz)
        post_reion_modifier = sobacchi_mesinger_model(Mvir, z_reion_local - dz, run_params)
        return 0.95 - phase * (0.95 - post_reion_modifier)

def plot_reionization_comparison(output_dir):
    """Plot reionization model comparison with standardized styling"""
    logger.info('=== Reionization Models Comparison ===')
    
    # Default SAGE parameters
    run_params = {
        'FilteringMassNorm': 0.7,  # 10^10 Msun/h
        'Reionization_zr': 8.0,
        'Reionization_z0': 7.0,
        'PostReionSlope': -2.0/3.0,
        'LocalReionVariance': 0.5,
        'PatchyReionWidth': 1.0
    }
    
    # Create mass range - now from 10^8 to 10^15 Msun/h
    Mvir_range = np.logspace(-2, 5, 150)  # 0.01 to 10^5 in 10^10 Msun/h (= 10^8 to 10^15 Msun/h)
    redshifts = [8.0, 7.5, 7.0]
    
    # Convert mass to actual units and then to log10 for plotting
    Mvir_plot = np.log10(Mvir_range * 1e10)  # Convert to log10(Msun/h)
    
    # Line style setup with standardized styling
    line_styles = {8.0: '-', 7.5: '--', 7.0: ':'}
    alphas = {8.0: 1.0, 7.5: 0.8, 7.0: 0.6}
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Store legend entries
    legend_entries = []
    
    for z in redshifts:
        logger.info(f"Calculating for z = {z}")
        
        # Calculate suppression for each model at this redshift
        original = [original_gnedin_model(M, z, run_params) for M in Mvir_range]
        enhanced = [enhanced_gnedin_model(M, z, run_params) for M in Mvir_range]
        sobacchi = [sobacchi_mesinger_model(M, z, run_params) for M in Mvir_range]
        patchy = [patchy_reionization_model(M, z, run_params) for M in Mvir_range]
        
        # Plot each model with standardized colors
        line1, = ax.plot(Mvir_plot, original, color=PLOT_COLORS['original_gnedin'], linestyle=line_styles[z], 
                    alpha=alphas[z], linewidth=2)
        line2, = ax.plot(Mvir_plot, enhanced, color=PLOT_COLORS['enhanced_gnedin'], linestyle=line_styles[z], 
                    alpha=alphas[z], linewidth=2)
        line3, = ax.plot(Mvir_plot, sobacchi, color=PLOT_COLORS['sobacchi_mesinger'], linestyle=line_styles[z], 
                    alpha=alphas[z], linewidth=2)
        line4, = ax.plot(Mvir_plot, patchy, color=PLOT_COLORS['patchy_reionization'], linestyle=line_styles[z], 
                    alpha=alphas[z], linewidth=2)
        
        # Add to legend only for first redshift to avoid duplicates
        if z == 8.0:
            legend_entries.extend([
                (line1, f'Original Gnedin'),
                (line2, f'Enhanced Gnedin'),
                (line3, f'Sobacchi & Mesinger'),
                (line4, f'Patchy Reionization')
            ])
    
    # Add redshift info to legend
    legend_entries.extend([
        (plt.Line2D([0], [0], color='gray', linestyle='-', linewidth=2), 'z = 8.0'),
        (plt.Line2D([0], [0], color='gray', linestyle='--', linewidth=2), 'z = 7.5'),
        (plt.Line2D([0], [0], color='gray', linestyle=':', linewidth=2), 'z = 7.0')
    ])
    
    # Standardized axis formatting
    ax.set_xlabel(r'$\log_{10}$ $M_{vir}$ [$M_\odot$]')
    ax.set_ylabel(r'Reionization Suppression Factor ($f_{\rm reion}$)')
    
    # Create combined legend with standardized styling
    lines, labels = zip(*legend_entries)
    ax.legend(lines, labels, loc='center right', fontsize=16, frameon=False)
    
    ax.set_ylim(0, 1.1)
    ax.set_xlim(9, 15)  # log10 scale: 10^9 to 10^15
    
    # Add some reference lines and labels with standardized styling
    ax.axhline(y=0.5, color='gray', linestyle=':', alpha=0.5)
    ax.axvline(x=9, color='gray', linestyle=':', alpha=0.5)
    ax.axvline(x=11, color='gray', linestyle=':', alpha=0.5)
    ax.axvline(x=13, color='gray', linestyle=':', alpha=0.5)
    ax.axvline(x=14, color='gray', linestyle=':', alpha=0.5)
    
    # Add mass scale labels with standardized styling
    text_props = {'rotation': 90, 'alpha': 0.7, 'fontsize': 9}
    ax.text(9.05, 0.05, 'Dwarf mass', **text_props)
    ax.text(11.05, 0.05, 'MW mass', **text_props)
    ax.text(13.05, 0.05, 'Group mass', **text_props)
    ax.text(14.05, 0.05, 'Cluster mass', **text_props)
    
    # Move 50% suppression text to middle of plot
    ax.text(11.5, 0.52, '50% suppression', alpha=0.7, fontsize=10, ha='center')
    
    # Save with standardized function
    output_filename = output_dir + 'reionization_comparison' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info('Reionization comparison plot complete')

# ========================== EXISTING FUNCTIONS (KEEPING ALL ORIGINAL FUNCTIONALITY) ==========================

def setup_cache():
    """Setup caching directory"""
    if ENABLE_CACHING:
        Path(CACHE_DIR).mkdir(exist_ok=True)

def get_cache_filename(directory, snap_num, param):
    """Generate cache filename"""
    if not ENABLE_CACHING:
        return None
    dir_hash = abs(hash(directory)) % 10000
    return f"{CACHE_DIR}/cache_{dir_hash}_{snap_num}_{param}.npy"

def load_from_cache(cache_file):
    """Load data from cache if available"""
    if cache_file and os.path.exists(cache_file):
        try:
            return np.load(cache_file, mmap_mode='r' if USE_MEMMAP else None)
        except:
            return None
    return None

def save_to_cache(data, cache_file):
    """Save data to cache"""
    if cache_file and ENABLE_CACHING:
        try:
            np.save(cache_file, data)
        except:
            pass

def get_memory_usage():
    """Get current memory usage in GB"""
    try:
        process = psutil.Process(os.getpid())
        return process.memory_info().rss / 1024 / 1024 / 1024
    except:
        return 0.0

def force_cleanup():
    """Force garbage collection and memory cleanup"""
    if AGGRESSIVE_CLEANUP:
        gc.collect()

def check_memory_usage():
    """Check if memory usage is getting too high"""
    current_memory = get_memory_usage()
    if current_memory > MAX_MEMORY_GB * 0.8:  # 80% threshold
        logger.warning(f"High memory usage ({current_memory:.1f} GB), forcing cleanup...")
        force_cleanup()
        return True
    return False

def get_file_list(directory):
    """Get sorted list of HDF5 model files with caching"""
    cache_file = f"{CACHE_DIR}/filelist_{abs(hash(directory)) % 10000}.txt"
    
    if ENABLE_CACHING and os.path.exists(cache_file):
        try:
            with open(cache_file, 'r') as f:
                files = [line.strip() for line in f.readlines()]
            # Verify files still exist
            if all(os.path.exists(os.path.join(directory, f)) for f in files):
                return files
        except:
            pass
    
    # Generate file list
    files = [f for f in os.listdir(directory) if f.startswith('model_') and f.endswith('.hdf5')]
    files.sort()
    
    # Cache the file list
    if ENABLE_CACHING:
        try:
            with open(cache_file, 'w') as f:
                for file in files:
                    f.write(file + '\n')
        except:
            pass
    
    return files

def read_single_hdf_file_optimized(args):
    """Optimized single file reader with better error handling and caching"""
    model_file, directory, snap_num, param = args
    filepath = os.path.join(directory, model_file)
    
    try:
        # Use optimized HDF5 settings
        with h5.File(filepath, 'r', rdcc_nbytes=HDF5_CACHE_SIZE) as property_file:
            if snap_num in property_file and param in property_file[snap_num]:
                dataset = property_file[snap_num][param]
                # Read data efficiently
                if dataset.size > 0:
                    data = dataset[:]  # More efficient than np.array()
                    return data
        return np.array([])
    except Exception as e:
        logger.warning(f"Error reading {model_file}: {e}")
        return np.array([])

def read_hdf_ultra_optimized(filename=None, snap_num=None, param=None, directory=None):
    """Ultra-optimized HDF5 reading with caching and batching"""
    if directory is None:
        directory = DirName
    
    # Check cache first
    cache_file = get_cache_filename(directory, snap_num, param)
    cached_data = load_from_cache(cache_file)
    if cached_data is not None:
        logger.debug(f"Loaded {param} from cache")
        return cached_data
    
    if not os.path.exists(directory):
        logger.warning(f"Directory {directory} does not exist")
        return np.array([])
    
    model_files = get_file_list(directory)
    
    if not model_files:
        logger.warning(f"No model files found in {directory}")
        return np.array([])
    
    start_time = time.time()
    initial_memory = get_memory_usage()
    
    if USE_PARALLEL and len(model_files) > 1:
        # Optimized parallel reading with batching
        batch_size = min(MAX_WORKERS * 2, len(model_files))
        
        # Process in batches to control memory usage
        all_results = []
        for i in range(0, len(model_files), batch_size):
            batch_files = model_files[i:i + batch_size]
            args_list = [(f, directory, snap_num, param) for f in batch_files]
            
            # Use ThreadPoolExecutor for I/O bound operations
            with ThreadPoolExecutor(max_workers=min(MAX_WORKERS, len(batch_files))) as executor:
                batch_results = list(executor.map(read_single_hdf_file_optimized, args_list))
            
            # Filter valid results and add to collection
            valid_results = [r for r in batch_results if len(r) > 0]
            all_results.extend(valid_results)
            
            # Clean up batch results
            del batch_results, valid_results
            
            # Check memory usage
            if check_memory_usage():
                logger.info(f"Memory cleanup performed during batch {i//batch_size + 1}")
        
        # Combine all results efficiently
        if all_results:
            # Pre-calculate total size for efficient concatenation
            total_size = sum(len(arr) for arr in all_results)
            if total_size > 0:
                # Use more efficient concatenation for large arrays
                if len(all_results) == 1:
                    combined_data = all_results[0]
                else:
                    # Pre-allocate array for better performance
                    dtype = all_results[0].dtype
                    combined_data = np.empty(total_size, dtype=dtype)
                    offset = 0
                    for arr in all_results:
                        combined_data[offset:offset + len(arr)] = arr
                        offset += len(arr)
            else:
                combined_data = np.array([])
            del all_results
        else:
            combined_data = np.array([])
    else:
        # Optimized sequential reading
        data_list = []
        for model_file in model_files:
            try:
                filepath = os.path.join(directory, model_file)
                with h5.File(filepath, 'r', rdcc_nbytes=HDF5_CACHE_SIZE) as property_file:
                    if snap_num in property_file and param in property_file[snap_num]:
                        dataset = property_file[snap_num][param]
                        if dataset.size > 0:
                            data_list.append(dataset[:])
            except Exception as e:
                logger.warning(f"Error reading {model_file}: {e}")
                continue
        
        if data_list:
            combined_data = np.concatenate(data_list) if len(data_list) > 1 else data_list[0]
            del data_list
        else:
            combined_data = np.array([])
    
    # Cache the result
    save_to_cache(combined_data, cache_file)
    
    # Performance reporting
    end_time = time.time()
    final_memory = get_memory_usage()
    read_time = end_time - start_time
    
    if read_time > 1.0 or final_memory > initial_memory + 0.5:
        logger.info(f"Read {param}: {len(combined_data)} elements in {read_time:.2f}s, "
                   f"memory: +{final_memory-initial_memory:.1f}GB")
    
    return combined_data

def calculate_muratov_mass_loading(vvir, z=0.0):
    """
    Calculate mass loading factor using Muratov et al. (2015) formulation
    Vectorized for better performance
    """
    # Constants from Muratov et al. (2015) and SAGE implementation
    V_CRIT = 60.0      # Critical velocity where the power law breaks
    NORM = 2.9         # Normalization factor
    Z_EXP = 1.3        # Redshift power-law exponent
    LOW_V_EXP = -3.2   # Low velocity power-law exponent
    HIGH_V_EXP = -1.0  # High velocity power-law exponent
    
    # Vectorized calculation for better performance
    z_term = np.power(1.0 + z, Z_EXP)
    v_ratio = vvir / V_CRIT
    
    # Vectorized broken power law
    v_term = np.where(vvir < V_CRIT, 
                      np.power(v_ratio, LOW_V_EXP),
                      np.power(v_ratio, HIGH_V_EXP))
    
    # Calculate final mass loading factor
    eta = NORM * z_term * v_term
    
    # Vectorized capping and finite value handling
    eta = np.clip(eta, 0.0, 100.0)
    eta = np.where(np.isfinite(eta), eta, 0.0)
    
    return eta

def plot_stellar_mass_function_comparison(sim_configs, snapshot, output_dir):
    """Plot stellar mass function comparison between SAGE 2.0 and Vanilla SAGE with standardized styling"""
    logger.info('=== Stellar Mass Function Comparison ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    binwidth = 0.1  # mass function histogram bin width
    
    # Process each simulation
    for sim_config in sim_configs:
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        hubble_h = sim_config['Hubble_h']
        box_size = sim_config['BoxSize']
        volume_fraction = sim_config['VolumeFraction']
        
        logger.info(f'Processing {label}...')
        
        try:
            # Calculate volume for this simulation
            volume = (box_size/hubble_h)**3.0 * volume_fraction
            
            # Read required galaxy properties
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            
            logger.info(f'  Total galaxies: {len(StellarMass)}')
            
            # Calculate SMF for all galaxies
            w = np.where((StellarMass > 0.0))[0]
            if len(w) == 0:
                logger.warning(f'No galaxies with stellar mass > 0 for {label}')
                continue
                
            mass = np.log10(StellarMass[w])
            sSFR = np.log10((SfrDisk[w] + SfrBulge[w]) / StellarMass[w])
            
            # Set histogram range
            mi = np.floor(min(mass)) - 2
            ma = np.floor(max(mass)) + 2
            NB = int((ma - mi) / binwidth)
            
            # Calculate histograms
            (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
            xaxeshisto = binedges[:-1] + 0.5 * binwidth
            
            # Additionally calculate red galaxies
            w_red = np.where(sSFR < sSFRcut)[0]
            massRED = mass[w_red] if len(w_red) > 0 else np.array([])
            (countsRED, _) = np.histogram(massRED, range=(mi, ma), bins=NB)
            
            # Additionally calculate blue galaxies  
            w_blue = np.where(sSFR > sSFRcut)[0]
            massBLU = mass[w_blue] if len(w_blue) > 0 else np.array([])
            (countsBLU, _) = np.histogram(massBLU, range=(mi, ma), bins=NB)
            
            # Convert to number density
            phi_all = counts / volume / binwidth
            phi_red = countsRED / volume / binwidth
            phi_blue = countsBLU / volume / binwidth
            
            # Convert to log10 for plotting (handle zeros)
            phi_all_log = np.where(phi_all > 0, np.log10(phi_all), np.nan)
            phi_red_log = np.where(phi_red > 0, np.log10(phi_red), np.nan)
            phi_blue_log = np.where(phi_blue > 0, np.log10(phi_blue), np.nan)
            
            # Plot with appropriate line styles
            # All galaxies - adjust linewidth for Vanilla SAGE
            linewidth_all = 2 if 'Vanilla' in label else 3
            label_all = f'{label} - All Galaxies' if 'Vanilla' not in label else f'{label}'
            ax.plot(xaxeshisto, phi_all_log, color=color, linestyle=linestyle, 
                   linewidth=linewidth_all, label=label_all, alpha=0.9)
            
            # Red galaxies - thinner lines, use dotted style for Vanilla SAGE
            if np.any(~np.isnan(phi_red_log)):
                red_linestyle = ':' if 'Vanilla' in label else linestyle
                ax.plot(xaxeshisto, phi_red_log, color='red', linestyle=red_linestyle, 
                       linewidth=2, alpha=0.7)
            
            # Blue galaxies - thinner lines, use dotted style for Vanilla SAGE
            if np.any(~np.isnan(phi_blue_log)):
                blue_linestyle = ':' if 'Vanilla' in label else linestyle
                ax.plot(xaxeshisto, phi_blue_log, color='blue', linestyle=blue_linestyle, 
                       linewidth=2, alpha=0.7)
            
            logger.info(f'  Processed {len(w)} galaxies with stellar mass')
            logger.info(f'  Red galaxies: {len(w_red)}, Blue galaxies: {len(w_blue)}')
            
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
    # Add observational data
    # Baldry+ 2008 modified data used for the MCMC fitting
    Baldry = np.array([
        [7.05, 1.3531e-01, 6.0741e-02],
        [7.15, 1.3474e-01, 6.0109e-02],
        [7.25, 2.0971e-01, 7.7965e-02],
        [7.35, 1.7161e-01, 3.1841e-02],
        [7.45, 2.1648e-01, 5.7832e-02],
        [7.55, 2.1645e-01, 3.9988e-02],
        [7.65, 2.0837e-01, 4.8713e-02],
        [7.75, 2.0402e-01, 7.0061e-02],
        [7.85, 1.5536e-01, 3.9182e-02],
        [7.95, 1.5232e-01, 2.6824e-02],
        [8.05, 1.5067e-01, 4.8824e-02],
        [8.15, 1.3032e-01, 2.1892e-02],
        [8.25, 1.2545e-01, 3.5526e-02],
        [8.35, 9.8472e-02, 2.7181e-02],
        [8.45, 8.7194e-02, 2.8345e-02],
        [8.55, 7.0758e-02, 2.0808e-02],
        [8.65, 5.8190e-02, 1.3359e-02],
        [8.75, 5.6057e-02, 1.3512e-02],
        [8.85, 5.1380e-02, 1.2815e-02],
        [8.95, 4.4206e-02, 9.6866e-03],
        [9.05, 4.1149e-02, 1.0169e-02],
        [9.15, 3.4959e-02, 6.7898e-03],
        [9.25, 3.3111e-02, 8.3704e-03],
        [9.35, 3.0138e-02, 4.7741e-03],
        [9.45, 2.6692e-02, 5.5029e-03],
        [9.55, 2.4656e-02, 4.4359e-03],
        [9.65, 2.2885e-02, 3.7915e-03],
        [9.75, 2.1849e-02, 3.9812e-03],
        [9.85, 2.0383e-02, 3.2930e-03],
        [9.95, 1.9929e-02, 2.9370e-03],
        [10.05, 1.8865e-02, 2.4624e-03],
        [10.15, 1.8136e-02, 2.5208e-03],
        [10.25, 1.7657e-02, 2.4217e-03],
        [10.35, 1.6616e-02, 2.2784e-03],
        [10.45, 1.6114e-02, 2.1783e-03],
        [10.55, 1.4366e-02, 1.8819e-03],
        [10.65, 1.2588e-02, 1.8249e-03],
        [10.75, 1.1372e-02, 1.4436e-03],
        [10.85, 9.1213e-03, 1.5816e-03],
        [10.95, 6.1125e-03, 9.6735e-04],
        [11.05, 4.3923e-03, 9.6254e-04],
        [11.15, 2.5463e-03, 5.0038e-04],
        [11.25, 1.4298e-03, 4.2816e-04],
        [11.35, 6.4867e-04, 1.6439e-04],
        [11.45, 2.8294e-04, 9.9799e-05],
        [11.55, 1.0617e-04, 4.9085e-05],
        [11.65, 3.2702e-05, 2.4546e-05],
        [11.75, 1.2571e-05, 1.2571e-05],
        [11.85, 8.4589e-06, 8.4589e-06],
        [11.95, 7.4764e-06, 7.4764e-06],
        ], dtype=np.float32)
    
    # Baldry et al. 2012 red and blue data points
    Baldry_Red_x = [7.488977955911820, 7.673346693386770, 7.881763527054110, 8.074148296593190, 8.306613226452910, 
        8.490981963927860, 8.69939879759519, 8.891783567134270, 9.084168336673350, 9.308617234468940, 
        9.501002004008020, 9.701402805611220, 9.901803607214430, 10.094188376753500, 10.270541082164300, 
        10.494989979959900, 10.711422845691400, 10.919839679358700, 11.08817635270540, 11.296593186372700, 
        11.488977955911800]

    Baldry_Red_y = [-1.9593147751606, -2.9785867237687400, -2.404710920770880, -3.038543897216280, -3.004282655246250, 
    -2.56745182012848, -2.6959314775160600, -2.464668094218420, -2.730192719486080, -2.618843683083510, 
    -2.6445396145610300, -2.687366167023560, -2.610278372591010, -2.490364025695930, -2.490364025695930, 
    -2.4389721627409000, -2.533190578158460, -2.6959314775160600, -2.8929336188436800, -3.4925053533190600, 
    -4.366167023554600]

    Baldry_Red_y = [10**x for x in Baldry_Red_y]

    Baldry_Blue_x = [7.088176352705410, 7.264529058116230, 7.488977955911820, 7.681362725450900, 7.897795591182370, 
        8.098196392785570, 8.298597194388780, 8.474949899799600, 8.69939879759519, 8.907815631262530, 
        9.092184368737470, 9.30060120240481, 9.48496993987976, 9.701402805611220, 9.885771543086170, 
        10.07815631262530, 10.294589178356700, 10.494989979959900, 10.703406813627300, 10.90380761523050, 
        11.064128256513000, 11.288577154308600]

    Baldry_Blue_y = [-1.7537473233404700, -1.376873661670240, -1.7023554603854400, -1.4796573875803000, -1.7023554603854400, 
    -1.5653104925053500, -1.6338329764454000, -1.7965738758030000, -1.7965738758030000, -1.9764453961456100, 
    -2.0792291220556700, -2.130620985010710, -2.293361884368310, -2.396145610278370, -2.490364025695930, 
    -2.6445396145610300, -2.635974304068520, -2.8244111349036400, -3.1327623126338300, -3.3897216274089900, 
    -4.383297644539620, -4.665952890792290]
    
    Baldry_Blue_y = [10**x for x in Baldry_Blue_y]
    
    # Weaver et al. 2022 data
    x_blue = [8.131868131868131, 8.36923076923077, 8.606593406593406, 8.861538461538462, 9.107692307692307,
          9.362637362637361, 9.608791208791208, 9.872527472527473, 10.10989010989011, 10.364835164835164,
            10.61978021978022, 10.857142857142858, 11.112087912087912, 11.367032967032968]

    y_blue = [-1.7269230769230768, -1.773076923076923, -1.8653846153846152, -1.9576923076923076, -2.096153846153846,
          -2.223076923076923, -2.338461538461538, -2.476923076923077, -2.592307692307692,
            -2.6846153846153844, -2.8692307692307693, -3.0884615384615386, -3.4923076923076923,
              -4.126923076923077]
    y_blue = [10**x for x in y_blue]

    x_red = [8.123076923076923, 8.378021978021978, 8.624175824175824, 8.861538461538462, 9.125274725274725,
          9.371428571428572, 9.635164835164835, 9.863736263736264, 10.118681318681318, 10.373626373626374,
            10.61978021978022, 10.874725274725275, 11.12967032967033, 11.367032967032968, 11.63076923076923]

    y_red = [-2.3846153846153846, -2.6384615384615384, -2.8346153846153848, -2.9153846153846152, -3.019230769230769,
          -3.0769230769230766, -3.0538461538461537, -3.0307692307692307, -3.019230769230769,
            -3.0076923076923077, -3.0076923076923077, -3.1115384615384616, -3.319230769230769,
              -3.85, -4.265384615384615]
    y_red = [10**x for x in y_red]

    # Use the first simulation's Hubble parameter for observations (assuming similar cosmology)
    if len(sim_configs) > 0:
        hubble_h_obs = sim_configs[0]['Hubble_h']
    else:
        hubble_h_obs = 0.73  # default
    
    # Convert observational data
    Baldry_xval = np.log10(10 ** Baldry[:, 0] / hubble_h_obs / hubble_h_obs)
    if whichimf == 1:  
        Baldry_xval = Baldry_xval - 0.26  # convert back to Chabrier IMF
    Baldry_yvalU = (Baldry[:, 1] + Baldry[:, 2]) * hubble_h_obs * hubble_h_obs * hubble_h_obs
    Baldry_yvalL = (Baldry[:, 1] - Baldry[:, 2]) * hubble_h_obs * hubble_h_obs * hubble_h_obs
    
    # Convert observational data to log10
    Baldry_yvalU_log = np.log10(Baldry_yvalU)
    Baldry_yvalL_log = np.log10(Baldry_yvalL)
    Baldry_Blue_y_log = np.log10(Baldry_Blue_y)
    Baldry_Red_y_log = np.log10(Baldry_Red_y)
    
    # # Plot observational data with standardized styling
    # ax.fill_between(Baldry_xval, Baldry_yvalU_log, Baldry_yvalL_log, 
    #     facecolor='purple', alpha=0.25, label='Baldry et al. 2008 (z=0.1)')
    
    # Plot additional observational data (removed Weaver et al.)
    ax.scatter(Baldry_Blue_x, Baldry_Blue_y_log, marker='s', s=50, edgecolor='navy', 
                facecolor='navy', alpha=0.3, label='Baldry et al. 2012 - Star forming Galaxies')
    ax.scatter(Baldry_Red_x, Baldry_Red_y_log, marker='s', s=50, edgecolor='maroon', 
                facecolor='maroon', alpha=0.3, label='Baldry et al. 2012 - Quiescent Galaxies')
    ax.scatter(x_blue, np.log10(y_blue), marker='o', s=50, edgecolor='blue',
                facecolor='blue', alpha=0.3, label='Weaver et al. 2022 - Star forming Galaxies')
    ax.scatter(x_red, np.log10(y_red), marker='o', s=50, edgecolor='red',
                facecolor='red', alpha=0.3, label='Weaver et al. 2022 - Quiescent Galaxies')
    
    # Add color legend for red/blue model divisions
    if len(sim_configs) > 0:
        # Add dummy lines for red/blue legend entries - solid for SAGE 2.0, dotted for Vanilla
        ax.plot([], [], color='red', linestyle='-', linewidth=2, alpha=0.7, label='Quiescent Galaxies')
        ax.plot([], [], color='blue', linestyle='-', linewidth=2, alpha=0.7, label='Star forming Galaxies')
        ax.plot([], [], color='red', linestyle=':', linewidth=2, alpha=0.7)
        ax.plot([], [], color='blue', linestyle=':', linewidth=2, alpha=0.7)
    
    # Formatting with standardized styling  
    ax.set_yscale('linear')  # Changed from log since we're plotting log10 values
    ax.set_xlim(8.0, 12.2)
    ax.set_ylim(-6.0, -1.0)  # Changed to log10 scale
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    ax.set_ylabel(r'$\log_{10} \phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}$')
    ax.set_xlabel(r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$')

    # Standardized legend with appropriate location
    style_legend(ax, loc='lower left', fontsize=16)
    
    # Save plot
    output_filename = output_dir + 'stellar_mass_function_comparison' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info('Stellar mass function comparison plot complete')

def plot_gas_mass_functions(sim_configs, snapshot, output_dir):
    """Plot H1 and H2 mass functions for multiple models with standardized styling"""
    logger.info(f'=== Multi-Model Gas Mass Functions Analysis ===')
    
    # Mass function parameters
    binwidth = 0.1
    
    # =============== H1 MASS FUNCTION PLOT ===============
    logger.info('Creating multi-model H1 mass function plot...')
    
    fig, ax = create_figure()
    
    # Plot observational data first (same as before)
    # Observational data
    Zwaan = np.array([[6.933, -0.333], [7.057, -0.490], [7.209, -0.698], [7.365, -0.667],
                      [7.528, -0.823], [7.647, -0.958], [7.809, -0.917], [7.971, -0.948],
                      [8.112, -0.927], [8.263, -0.917], [8.404, -1.062], [8.566, -1.177],
                      [8.707, -1.177], [8.853, -1.312], [9.010, -1.344], [9.161, -1.448],
                      [9.302, -1.604], [9.448, -1.792], [9.599, -2.021], [9.740, -2.406],
                      [9.897, -2.615], [10.053, -3.031], [10.178, -3.677], [10.335, -4.448],
                      [10.492, -5.083]], dtype=np.float32)
    
    ObrCold = np.array([[8.009, -1.042], [8.215, -1.156], [8.409, -0.990], [8.604, -1.156],
                        [8.799, -1.208], [9.020, -1.333], [9.194, -1.385], [9.404, -1.552],
                        [9.599, -1.677], [9.788, -1.812], [9.999, -2.312], [10.172, -2.656],
                        [10.362, -3.500], [10.551, -3.635], [10.740, -5.010]], dtype=np.float32)
    
    # Use the first simulation's Hubble parameter for observations (assuming similar cosmology)
    hubble_h_obs = sim_configs[0]['Hubble_h']
    
    # Convert observational data
    ObrCold_xval = np.log10(10**(ObrCold[:, 0]) / hubble_h_obs / hubble_h_obs)
    ObrCold_yval_log = ObrCold[:, 1] + np.log10(hubble_h_obs * hubble_h_obs * hubble_h_obs)
    
    Zwaan_xval = np.log10(10**(Zwaan[:, 0]) / hubble_h_obs / hubble_h_obs)
    Zwaan_yval_log = Zwaan[:, 1] + np.log10(hubble_h_obs * hubble_h_obs * hubble_h_obs)
    
    # Plot observational data with standardized styling
    ax.scatter(ObrCold_xval, ObrCold_yval_log, color='grey', marker='s', s=80, 
               label='Obr. & Raw. 2009 (Cold Gas)', zorder=5, edgecolors='black', linewidth=0.5)
    ax.scatter(Zwaan_xval, Zwaan_yval_log, color='black', marker='^', s=80, 
               label='Zwaan et al. 2005', zorder=5, edgecolors='black', linewidth=0.5)
    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        box_size = sim_config['BoxSize']
        volume_fraction = sim_config['VolumeFraction']
        
        logger.info(f'Processing {label} for H1 mass function...')
        
        try:
            # Calculate volume for this simulation
            volume = (box_size/hubble_h)**3.0 * volume_fraction
            
            # Read required galaxy properties
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            H1Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H1_gas', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            logger.info(f'  Total galaxies: {len(ColdGas)}')
            
            # Calculate for galaxies with cold gas > 0 (for both cold gas and H1)
            w_cold = np.where(ColdGas > 0.0)[0]
            w_h1 = np.where(H1Gas > 0.0)[0]
            logger.info(f'  Galaxies with cold gas > 0: {len(w_cold)}')
            logger.info(f'  Galaxies with H1 gas > 0: {len(w_h1)}')
            
            if len(w_cold) == 0 and len(w_h1) == 0:
                logger.warning(f'  No galaxies with gas > 0 for {label}')
                continue
            
            # Calculate histograms for both cold gas and H1
            if len(w_cold) > 0:
                cold = np.log10(ColdGas[w_cold])
                mi_cold = np.floor(min(cold)) - 2
                ma_cold = np.floor(max(cold)) + 2
            else:
                mi_cold, ma_cold = 8, 12
            
            if len(w_h1) > 0:
                h1 = np.log10(H1Gas[w_h1])
                mi_h1 = np.floor(min(h1)) - 2
                ma_h1 = np.floor(max(h1)) + 2
            else:
                mi_h1, ma_h1 = 8, 12
            
            # Use combined range for consistency
            mi = min(mi_cold, mi_h1)
            ma = max(ma_cold, ma_h1)
            NB = int((ma - mi) / binwidth)
            
            # Calculate histograms
            if len(w_cold) > 0:
                (counts_cold, binedges_cold) = np.histogram(cold, range=(mi, ma), bins=NB)
                xaxeshisto_cold = binedges_cold[:-1] + 0.5 * binwidth
            else:
                counts_cold = np.zeros(NB)
                xaxeshisto_cold = np.linspace(mi + 0.5*binwidth, ma - 0.5*binwidth, NB)
            
            if len(w_h1) > 0:
                (counts_h1, binedges_h1) = np.histogram(h1, range=(mi, ma), bins=NB)
                xaxeshisto_h1 = binedges_h1[:-1] + 0.5 * binwidth
            else:
                counts_h1 = np.zeros(NB)
                xaxeshisto_h1 = np.linspace(mi + 0.5*binwidth, ma - 0.5*binwidth, NB)
            
            # Convert model data to log10 for plotting
            def safe_log10_with_mask(values):
                """Safely convert to log10, return both log values and mask for valid points"""
                mask = values > 0
                log_vals = np.full_like(values, np.nan)
                log_vals[mask] = np.log10(values[mask])
                return log_vals, mask
            
            model_cold_gas_log, mask_cold = safe_log10_with_mask(counts_cold / volume / binwidth)
            model_h1_gas_log, mask_h1 = safe_log10_with_mask(counts_h1 / volume / binwidth)
            
            # Plot cold gas line (dotted) only for main model
            if i == 0:  # Only for the main (first) model
                ax.plot(xaxeshisto_cold[mask_cold], model_cold_gas_log[mask_cold], 
                       color=PLOT_COLORS['cold_gas'], linestyle=':', linewidth=3, 
                       label='Cold Gas', alpha=0.9)
            
            # Plot H2 gas line for each model - use original label for main model
            if i == 0:
                h1_label = 'All Galaxies'
            else:
                h1_label = label
            
            ax.plot(xaxeshisto_h1[mask_h1], model_h1_gas_log[mask_h1], 
                   color=color, linestyle=linestyle, linewidth=linewidth, 
                   label=h1_label, alpha=alpha)
            
            # For the main model (first one), also plot central/satellite breakdown
            if i == 0:  # Only for the first (main) model
                sats = np.where((H1Gas > 0.0) & (Type == 1))[0]
                cent = np.where((H1Gas > 0.0) & (Type == 0))[0]
                
                if len(sats) > 0:
                    mass_sat = np.log10(H1Gas[sats])
                    (countssat, _) = np.histogram(mass_sat, range=(mi, ma), bins=NB)
                    model_sats_log, mask_sats = safe_log10_with_mask(countssat / volume / binwidth)
                    ax.plot(xaxeshisto_h1[mask_sats], model_sats_log[mask_sats], 
                           color=PLOT_COLORS['satellites'], linestyle='--', linewidth=2, 
                           label='Satellites', alpha=0.7)
                
                if len(cent) > 0:
                    mass_cent = np.log10(H1Gas[cent])
                    (countscent, _) = np.histogram(mass_cent, range=(mi, ma), bins=NB)
                    model_cents_log, mask_cents = safe_log10_with_mask(countscent / volume / binwidth)
                    ax.plot(xaxeshisto_h1[mask_cents], model_cents_log[mask_cents], 
                           color=PLOT_COLORS['centrals'], linestyle='--', linewidth=2, 
                           label='Centrals', alpha=0.7)
            
            logger.info(f'  Processed {len(w_h1)} galaxies with H1 gas, {len(w_cold)} with cold gas')
            
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
    # Set axis limits and formatting for log scale
    ax.set_xlim(8.0, 11.5)
    ax.set_ylim(-6.0, -1.0)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    
    ax.set_ylabel(r'$\log_{10} \phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{HI}}\ (M_{\odot})$')
    
    style_legend(ax, loc='lower left', fontsize=16)
    
    output_filename_h1 = output_dir + 'h1_mass_function' + OutputFormat
    finalize_plot(fig, output_filename_h1)
    
    # =============== H2 MASS FUNCTION PLOT ===============
    logger.info('Creating multi-model H2 mass function plot...')
    
    fig, ax = create_figure()
    
    # Observational data for H2
    ObrRaw = np.array([[7.300, -1.104], [7.576, -1.302], [7.847, -1.250], [8.133, -1.240],
                       [8.409, -1.344], [8.691, -1.479], [8.956, -1.792], [9.231, -2.271],
                       [9.507, -3.198], [9.788, -5.062]], dtype=np.float32)
    
    # Convert observational data
    ObrRaw_xval = np.log10(10**(ObrRaw[:, 0]) / hubble_h_obs / hubble_h_obs)
    ObrRaw_yval_log = ObrRaw[:, 1] + np.log10(hubble_h_obs * hubble_h_obs * hubble_h_obs)
    
    # Plot observational data with standardized styling
    ax.scatter(ObrCold_xval, ObrCold_yval_log, color='grey', marker='s', s=80, 
               label='Obr. & Raw. 2009 (Cold Gas)', zorder=5, edgecolors='black', linewidth=0.5)
    ax.scatter(ObrRaw_xval, ObrRaw_yval_log, color='black', marker='^', s=80, 
               label='Obr. & Raw. 2009', zorder=5, edgecolors='black', linewidth=0.5)
    
    # Process each simulation model for H2
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        box_size = sim_config['BoxSize']
        volume_fraction = sim_config['VolumeFraction']
        
        logger.info(f'Processing {label} for H2 mass function...')
        
        try:
            # Calculate volume for this simulation
            volume = (box_size/hubble_h)**3.0 * volume_fraction
            
            # Read required galaxy properties
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            H2Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            # Calculate for galaxies with gas > 0
            w_cold = np.where(ColdGas > 0.0)[0]
            w_h2 = np.where(H2Gas > 0.0)[0]
            logger.info(f'  Galaxies with cold gas > 0: {len(w_cold)}')
            logger.info(f'  Galaxies with H2 gas > 0: {len(w_h2)}')
            
            if len(w_cold) == 0 and len(w_h2) == 0:
                logger.warning(f'  No galaxies with gas > 0 for {label}')
                continue
            
            # Calculate histograms for both cold gas and H2
            if len(w_cold) > 0:
                cold = np.log10(ColdGas[w_cold])
                mi_cold = np.floor(min(cold)) - 2
                ma_cold = np.floor(max(cold)) + 2
            else:
                mi_cold, ma_cold = 8, 12
            
            if len(w_h2) > 0:
                h2 = np.log10(H2Gas[w_h2])
                mi_h2 = np.floor(min(h2)) - 2
                ma_h2 = np.floor(max(h2)) + 2
            else:
                mi_h2, ma_h2 = 8, 12
            
            # Use combined range for consistency
            mi = min(mi_cold, mi_h2)
            ma = max(ma_cold, ma_h2)
            NB = int((ma - mi) / binwidth)
            
            # Calculate histograms
            if len(w_cold) > 0:
                (counts_cold, binedges_cold) = np.histogram(cold, range=(mi, ma), bins=NB)
                xaxeshisto_cold = binedges_cold[:-1] + 0.5 * binwidth
            else:
                counts_cold = np.zeros(NB)
                xaxeshisto_cold = np.linspace(mi + 0.5*binwidth, ma - 0.5*binwidth, NB)
            
            if len(w_h2) > 0:
                (counts_h2, binedges_h2) = np.histogram(h2, range=(mi, ma), bins=NB)
                xaxeshisto_h2 = binedges_h2[:-1] + 0.5 * binwidth
            else:
                counts_h2 = np.zeros(NB)
                xaxeshisto_h2 = np.linspace(mi + 0.5*binwidth, ma - 0.5*binwidth, NB)
            
            # Convert model data to log10 for plotting
            model_cold_gas_log, mask_cold = safe_log10_with_mask(counts_cold / volume / binwidth)
            model_h2_gas_log, mask_h2 = safe_log10_with_mask(counts_h2 / volume / binwidth)
            
            # Plot cold gas line (dotted) only for main model
            if i == 0:  # Only for the main (first) model
                ax.plot(xaxeshisto_cold[mask_cold], model_cold_gas_log[mask_cold], 
                       color=PLOT_COLORS['cold_gas'], linestyle=':', linewidth=3, 
                       label='Cold Gas', alpha=0.9)
            
            # Plot H2 gas line for each model - use original label for main model
            if i == 0:
                h2_label = 'All Galaxies'
            else:
                h2_label = label
            
            ax.plot(xaxeshisto_h2[mask_h2], model_h2_gas_log[mask_h2], 
                   color=color, linestyle=linestyle, linewidth=linewidth, 
                   label=h2_label, alpha=alpha)
            
            # For the main model (first one), also plot central/satellite breakdown
            if i == 0:  # Only for the first (main) model
                sats = np.where((H2Gas > 0.0) & (Type == 1))[0]
                cent = np.where((H2Gas > 0.0) & (Type == 0))[0]
                
                if len(sats) > 0:
                    mass_sat = np.log10(H2Gas[sats])
                    (countssat, _) = np.histogram(mass_sat, range=(mi, ma), bins=NB)
                    model_sats_log, mask_sats = safe_log10_with_mask(countssat / volume / binwidth)
                    ax.plot(xaxeshisto_h2[mask_sats], model_sats_log[mask_sats], 
                           color=PLOT_COLORS['satellites'], linestyle='--', linewidth=2, 
                           label='Satellites', alpha=0.7)
                
                if len(cent) > 0:
                    mass_cent = np.log10(H2Gas[cent])
                    (countscent, _) = np.histogram(mass_cent, range=(mi, ma), bins=NB)
                    model_cents_log, mask_cents = safe_log10_with_mask(countscent / volume / binwidth)
                    ax.plot(xaxeshisto_h2[mask_cents], model_cents_log[mask_cents], 
                           color=PLOT_COLORS['centrals'], linestyle='--', linewidth=2, 
                           label='Centrals', alpha=0.7)
            
            logger.info(f'  Processed {len(w_h2)} galaxies with H2 gas, {len(w_cold)} with cold gas')
            
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
    # Set axis limits and formatting for log scale
    ax.set_xlim(8.0, 11.5)
    ax.set_ylim(-6.0, -1.0)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    
    ax.set_ylabel(r'$\log_{10} \phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{H_2}}\ (M_{\odot})$')

    style_legend(ax, loc='lower left', fontsize=16)
    
    output_filename_h2 = output_dir + 'h2_mass_function' + OutputFormat
    finalize_plot(fig, output_filename_h2)

    # Print comparison statistics
    logger.info('=== Multi-Model Gas Mass Function Statistics ===')
    for sim_config in sim_configs:
        try:
            directory = sim_config['path']
            label = sim_config['label']
            hubble_h = sim_config['Hubble_h']
            
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            H1Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H1_gas', directory=directory) * 1.0e10 / hubble_h
            H2Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            
            w = np.where(ColdGas > 0.0)[0]
            if len(w) > 0:
                logger.info(f'{label}:')
                logger.info(f'  Total cold gas mass: {np.sum(ColdGas[w]):.2e} M_sol')
                logger.info(f'  Total H1 mass: {np.sum(H1Gas[w]):.2e} M_sol')
                logger.info(f'  Total H2 mass: {np.sum(H2Gas[w]):.2e} M_sol')
                logger.info(f'  H1/Cold gas ratio: {np.sum(H1Gas[w])/np.sum(ColdGas[w]):.3f}')
                logger.info(f'  H2/Cold gas ratio: {np.sum(H2Gas[w])/np.sum(ColdGas[w]):.3f}')
                logger.info(f'  H2/H1 ratio: {np.sum(H2Gas[w])/np.sum(H1Gas[w]):.3f}')
        except Exception as e:
            logger.warning(f'Could not calculate statistics for {label}: {e}')
    
    force_cleanup()
    logger.info('Gas mass function analysis complete')

def process_snapshot_batch(args):
    """Process a batch of snapshots for SFR calculation"""
    snap_batch, sim_config, volume = args
    
    batch_results = {}
    for snap in snap_batch:
        snap_name = f'Snap_{snap}'
        snap_idx = snap - sim_config['FirstSnap']
        
        try:
            # Read SFR data with optimized function
            sfr_disk = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrDisk', 
                                               directory=sim_config['path'])
            sfr_bulge = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrBulge', 
                                                directory=sim_config['path'])
            
            if len(sfr_disk) > 0 and len(sfr_bulge) > 0:
                # Use more efficient summation
                total_sfr = float(np.sum(sfr_disk, dtype=np.float64) + 
                                np.sum(sfr_bulge, dtype=np.float64))
                sfr_density = total_sfr / volume
            else:
                sfr_density = 0.0
            
            batch_results[snap_idx] = sfr_density
            
            # Clean up immediately
            del sfr_disk, sfr_bulge
            
        except Exception as e:
            logger.warning(f"Error processing snapshot {snap}: {e}")
            batch_results[snap_idx] = 0.0
    
    return batch_results

def calculate_sfr_density_ultra_optimized(sim_config):
    """Ultra-optimized SFR density calculation with batching and caching"""
    sim_path = sim_config['path']
    sim_label = sim_config['label']
    box_size = sim_config['BoxSize']
    hubble_h = sim_config['Hubble_h']
    volume_fraction = sim_config['VolumeFraction']
    first_snap = sim_config['FirstSnap']
    last_snap = sim_config['LastSnap']
    sim_redshifts = sim_config['redshifts']
    
    logger.info(f"Processing simulation: {sim_label}")
    logger.info(f"  Path: {sim_path}")
    logger.info(f"  BoxSize: {box_size} h^-1 Mpc, Hubble_h: {hubble_h}")
    logger.info(f"  Snapshot range: {first_snap} to {last_snap} ({last_snap-first_snap+1} snapshots)")
    
    # Calculate volume
    volume = (box_size/hubble_h)**3.0 * volume_fraction
    logger.info(f"  Volume: {volume:.2e} Mpc^3")
    
    # Check cache for full SFR density array
    cache_key = f"sfr_density_{abs(hash(sim_path)) % 10000}_{first_snap}_{last_snap}"
    full_cache_file = f"{CACHE_DIR}/{cache_key}.npy" if ENABLE_CACHING else None
    
    cached_sfr = load_from_cache(full_cache_file)
    if cached_sfr is not None and len(cached_sfr) == (last_snap - first_snap + 1):
        logger.info(f"  Loaded full SFR density from cache")
        return cached_sfr, sim_redshifts
    
    # Initialize SFR density array
    num_snaps = last_snap - first_snap + 1
    SFR_density = np.zeros(num_snaps, dtype=np.float64)
    
    # Determine processing strategy
    use_sampling = SMART_SAMPLING and num_snaps > MIN_SNAPSHOTS_FOR_SAMPLING
    use_parallel = USE_PARALLEL and num_snaps > CHUNK_SIZE
    
    if use_sampling:
        # Smart sampling for very large datasets
        sample_factor = max(2, num_snaps // 50)
        sample_snaps = list(range(first_snap, last_snap + 1, sample_factor))
        if first_snap not in sample_snaps:
            sample_snaps.insert(0, first_snap)
        if last_snap not in sample_snaps:
            sample_snaps.append(last_snap)
        processing_snaps = sorted(set(sample_snaps))
        logger.info(f"  Using smart sampling: {len(processing_snaps)}/{num_snaps} snapshots")
    else:
        processing_snaps = list(range(first_snap, last_snap + 1))
    
    if use_parallel and len(processing_snaps) > CHUNK_SIZE:
        # Parallel processing with batching
        snap_batches = [processing_snaps[i:i + CHUNK_SIZE] 
                       for i in range(0, len(processing_snaps), CHUNK_SIZE)]
        
        logger.info(f"  Processing {len(snap_batches)} batches in parallel")
        
        # Process batches in parallel
        args_list = [(batch, sim_config, volume) for batch in snap_batches]
        
        with ProcessPoolExecutor(max_workers=min(MAX_WORKERS, len(snap_batches))) as executor:
            future_to_batch = {executor.submit(process_snapshot_batch, args): args 
                              for args in args_list}
            
            completed = 0
            for future in as_completed(future_to_batch):
                batch_results = future.result()
                for snap_idx, sfr_density in batch_results.items():
                    if 0 <= snap_idx < len(SFR_density):
                        SFR_density[snap_idx] = sfr_density
                
                completed += 1
                if completed % 5 == 0 or completed == len(snap_batches):
                    progress = completed / len(snap_batches) * 100
                    logger.info(f"  Progress: {progress:.1f}% ({completed}/{len(snap_batches)} batches)")
    else:
        # Sequential processing with progress reporting
        for i, snap in enumerate(processing_snaps):
            snap_name = f'Snap_{snap}'
            snap_idx = snap - first_snap
            
            try:
                sfr_disk = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrDisk', 
                                                   directory=sim_path)
                sfr_bulge = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrBulge', 
                                                    directory=sim_path)
                
                if len(sfr_disk) > 0 and len(sfr_bulge) > 0:
                    total_sfr = float(np.sum(sfr_disk, dtype=np.float64) + 
                                    np.sum(sfr_bulge, dtype=np.float64))
                    SFR_density[snap_idx] = total_sfr / volume
                else:
                    SFR_density[snap_idx] = 0.0
                
                del sfr_disk, sfr_bulge
                
                # Progress reporting
                if (i + 1) % MEMORY_CHECK_INTERVAL == 0 or i == len(processing_snaps) - 1:
                    progress = (i + 1) / len(processing_snaps) * 100
                    logger.info(f"  Progress: {progress:.1f}% ({i+1}/{len(processing_snaps)})")
                    check_memory_usage()
                    
            except Exception as e:
                logger.warning(f"Error processing snapshot {snap}: {e}")
                SFR_density[snap_idx] = 0.0
    
    # Interpolation for sampled data
    if use_sampling and len(processing_snaps) < num_snaps:
        logger.info(f"  Interpolating to full resolution...")
        sample_indices = [snap - first_snap for snap in processing_snaps]
        sample_sfr = SFR_density[sample_indices]
        
        nonzero_mask = sample_sfr > 0
        if np.any(nonzero_mask):
            sample_redshifts = [sim_redshifts[i] for i in sample_indices]
            valid_z = np.array(sample_redshifts)[nonzero_mask]
            valid_sfr = sample_sfr[nonzero_mask]
            
            # Interpolate in log space for better results
            log_sfr = np.log10(valid_sfr + 1e-15)
            full_z = np.array(sim_redshifts)
            interpolated_log_sfr = np.interp(full_z, valid_z[::-1], log_sfr[::-1])
            SFR_density = 10**interpolated_log_sfr - 1e-15
            SFR_density = np.maximum(SFR_density, 0.0)
    
    # Cache the full result
    save_to_cache(SFR_density, full_cache_file)
    
    # Print statistics
    nonzero_mask = SFR_density > 0.0
    if np.any(nonzero_mask):
        logger.info(f"  SFR density range: {np.min(SFR_density[nonzero_mask]):.2e} - "
                   f"{np.max(SFR_density[nonzero_mask]):.2e}")
        peak_idx = np.argmax(SFR_density)
        peak_redshift = sim_redshifts[peak_idx] if peak_idx < len(sim_redshifts) else "unknown"
        logger.info(f"  Peak SFR density: {np.max(SFR_density):.2e} at z={peak_redshift}")
        logger.info(f"  Non-zero snapshots: {np.sum(nonzero_mask)} / {len(SFR_density)}")
    
    force_cleanup()
    return SFR_density, sim_redshifts

def plot_sfr_density_comparison():
    """Plot SFR density evolution comparison across simulations with standardized styling"""
    logger.info('=== SFR Density Evolution Comparison ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Process each simulation
    for i, sim_config in enumerate(SFR_SimDirs):
        logger.info(f"Processing {sim_config['label']}...")
        
        try:
            # Calculate SFR density evolution
            sfr_density, redshifts = calculate_sfr_density_ultra_optimized(sim_config)
            
            # Convert redshift to time or keep as redshift for x-axis
            z_array = np.array(redshifts)
            
            # Plot with standardized styling
            color = sim_config.get('color', PLOT_COLORS['model_main'])
            linestyle = sim_config.get('linestyle', '-')
            label = sim_config['label']
            
            # Only plot non-zero values
            mask = sfr_density > 0
            if np.any(mask):
                ax.semilogy(z_array[mask], sfr_density[mask], 
                           color=color, linestyle=linestyle, linewidth=3,
                           label=label, alpha=0.9)
            else:
                logger.warning(f"No valid SFR density data for {label}")
                
        except Exception as e:
            logger.error(f"Error processing {sim_config['label']}: {e}")
            continue
    
    # Formatting with standardized styling
    ax.set_xlabel('Redshift')
    ax.set_ylabel('SFR Density [M$_{\\odot}$ yr$^{-1}$ Mpc$^{-3}$]')
    
    # Set reasonable axis limits (adjust based on your data)
    ax.set_xlim(0, 6)
    ax.set_ylim(1e-4, 1)
    
    # Invert x-axis to show cosmic time progression (higher z on left)
    ax.invert_xaxis()
    
    # Standardized legend
    style_legend(ax, loc='upper right')
    
    # Save plot
    output_filename = OutputDir + 'sfr_density_evolution' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info('SFR density comparison plot complete')

def plot_h2_fraction_vs_stellar_mass(sim_configs, snapshot, output_dir):
    """Plot H2 fraction vs stellar mass with observational data"""
    logger.info('=== H2 Fraction vs Stellar Mass Analysis ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Lists to collect legend handles and labels
    obs_handles = []
    obs_labels = []
    model_handles = []
    model_labels = []
    
    # =============== OBSERVATIONAL DATA ===============
    
    # Saintonge et al. 2011 (COLD GASS) - most commonly used
    # Median H2/H* vs stellar mass for star-forming galaxies
    saintonge_mass = np.array([9.0, 9.2, 9.4, 9.6, 9.8, 10.0, 10.2, 10.4, 10.6, 10.8, 11.0, 11.2])
    saintonge_h2_frac = np.array([0.08, 0.12, 0.15, 0.18, 0.20, 0.22, 0.20, 0.18, 0.15, 0.12, 0.08, 0.05])
    saintonge_h2_frac_err = saintonge_h2_frac * 0.3  # Approximate error bars
    
    # Boselli et al. 2014 (Herschel Reference Survey) - Local Universe
    boselli_mass = np.array([8.5, 9.0, 9.5, 10.0, 10.5, 11.0, 11.5])
    boselli_h2_frac = np.array([0.05, 0.10, 0.18, 0.25, 0.20, 0.12, 0.06])
    
    # Tacconi et al. 2020 - High redshift (z~1-3) compilation
    tacconi_mass_z1 = np.array([9.5, 10.0, 10.5, 11.0, 11.5])
    tacconi_h2_frac_z1 = np.array([0.25, 0.35, 0.40, 0.30, 0.20])  # Higher at z~1
    
    # Plot observational data with standardized styling
    # Saintonge et al. 2011 - with error bars instead of shading
    saintonge_errorbar = ax.errorbar(saintonge_mass, saintonge_h2_frac, yerr=saintonge_h2_frac_err,
                                    fmt='-', color='purple', linewidth=2, markersize=6, 
                                    capsize=3, capthick=1, label='Saintonge et al. 2011 (z~0)')
    obs_handles.append(saintonge_errorbar)
    obs_labels.append('Saintonge et al. 2011 (z~0)')
    
    # Boselli et al. 2014
    boselli_scatter = ax.scatter(boselli_mass, boselli_h2_frac, marker='s', s=80, 
                                color='darkgreen', edgecolors='black', linewidth=1,
                                label='Boselli et al. 2014 (z~0)', zorder=5)
    obs_handles.append(boselli_scatter)
    obs_labels.append('Boselli et al. 2014 (z~0)')
    
    # Tacconi et al. 2020 (high-z)
    tacconi_scatter = ax.scatter(tacconi_mass_z1, tacconi_h2_frac_z1, marker='^', s=100,
                                color='red', edgecolors='black', linewidth=1,
                                label='Tacconi et al. 2020 (z~1)', zorder=5)
    obs_handles.append(tacconi_scatter)
    obs_labels.append('Tacconi et al. 2020 (z~1)')
    
    # =============== MODEL DATA ===============
    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        
        logger.info(f'Processing {label} for H2 fraction analysis...')
        
        try:
            # Read required galaxy properties
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            H2Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            HI_Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H1_gas', directory=directory) * 1.0e10 / hubble_h
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            logger.info(f'  Total galaxies: {len(StellarMass)}')
            
            # Calculate sSFR for star-forming galaxy selection
            sSFR = np.log10((SfrDisk + SfrBulge) / StellarMass)
            
            # Select star-forming galaxies with valid data
            w = np.where((StellarMass > 1e8) & (ColdGas > 0) & (H2Gas > 0))[0]  # Central, star-forming galaxies
            
            if len(w) == 0:
                logger.warning(f'  No valid galaxies for {label}')
                continue
            
            logger.info(f'  Star-forming centrals with H2: {len(w)}')
            
            # Calculate H2 fraction: f_H2 = M_H2 / (M_H2 + M_HI)
            stellar_mass_sel = StellarMass[w]
            h2_gas_sel = H2Gas[w] 
            hi_gas_sel = HI_Gas[w]
            cold_gas_sel = ColdGas[w]
            
            # Calculate molecular fraction
            h2_fraction = h2_gas_sel / (h2_gas_sel + hi_gas_sel)
            
            # Alternative: H2 fraction relative to cold gas
            h2_frac_cold = h2_gas_sel / cold_gas_sel
            
            # Bin by stellar mass
            log_stellar_mass = np.log10(stellar_mass_sel)
            mass_bins = np.arange(8.0, 12.0, 0.25)
            mass_centers = mass_bins[:-1] + 0.1
            
            # Calculate median and error bars in each bin
            median_h2_frac = []
            error_h2_frac = []
            
            for j in range(len(mass_bins)-1):
                mask = (log_stellar_mass >= mass_bins[j]) & (log_stellar_mass < mass_bins[j+1])
                if np.sum(mask) > 5:  # Require at least 5 galaxies per bin
                    bin_data = h2_fraction[mask]
                    median_h2_frac.append(np.median(bin_data))
                    
                    # Option 1: Standard error of the mean (recommended)
                    error_h2_frac.append(np.std(bin_data) / np.sqrt(len(bin_data)))
                    
                    # Option 2: Interquartile range (uncomment to use instead)
                    # p25, p75 = np.percentile(bin_data, [25, 75])
                    # error_h2_frac.append((p75 - p25) / 2)  # Half the IQR
                    
                    # Option 3: Reduced standard deviation (uncomment to use instead)
                    # error_h2_frac.append(0.3 * np.std(bin_data))  # 30% of std
                    
                else:
                    median_h2_frac.append(np.nan)
                    error_h2_frac.append(np.nan)
            
            # Convert to arrays and remove NaN values
            median_h2_frac = np.array(median_h2_frac)
            error_h2_frac = np.array(error_h2_frac)
            valid_bins = ~np.isnan(median_h2_frac)
            
            if np.any(valid_bins):
                mass_centers_valid = mass_centers[valid_bins]
                median_h2_frac_valid = median_h2_frac[valid_bins]
                error_h2_frac_valid = error_h2_frac[valid_bins]
                
                # Plot with error bars for SAGE 2.0 (first model), regular line for others
                if i == 0:  # SAGE 2.0 model - add error bars
                    model_errorbar = ax.errorbar(mass_centers_valid, median_h2_frac_valid, 
                                               yerr=error_h2_frac_valid,
                                               fmt='-', color=color, linewidth=linewidth,
                                               markersize=4, capsize=4, capthick=1.5,
                                               label=label, alpha=alpha, zorder=6)
                    model_handles.append(model_errorbar)
                else:  # Other models - regular line
                    model_line = ax.plot(mass_centers_valid, median_h2_frac_valid, 
                                       color=color, linestyle=linestyle, linewidth=linewidth,
                                       label=label, alpha=alpha)[0]
                    model_handles.append(model_line)
                
                model_labels.append(label)
                
                # Plot individual galaxies as scatter (subsample for visibility)
                if len(w) > 2000:
                    indices = sample(range(len(w)), 2000)
                    scatter_mass = log_stellar_mass[indices]
                    scatter_h2_frac = h2_fraction[indices]
                else:
                    scatter_mass = log_stellar_mass
                    scatter_h2_frac = h2_fraction
                
                # # Only for main model, add scatter points
                # if i == 0:
                #     ax.scatter(scatter_mass, scatter_h2_frac, s=1, alpha=0.25, 
                #              color=color, rasterized=True)
                
                logger.info(f'  H2 fraction range: {np.min(h2_fraction):.3f} - {np.max(h2_fraction):.3f}')
                logger.info(f'  Median H2 fraction: {np.median(h2_fraction):.3f}')
                
                # Log statistics for SAGE 2.0 error bars
                if i == 0:
                    logger.info(f'  Error bar range: {np.min(error_h2_frac_valid):.3f} - {np.max(error_h2_frac_valid):.3f}')
                    logger.info(f'  Mean error bar size: {np.mean(error_h2_frac_valid):.3f}')
                
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
    # =============== FORMATTING ===============
    
    # Set axis limits and formatting
    ax.set_xlim(8.0, 12.2)
    ax.set_ylim(0.0, 0.5)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.05))
    
    ax.set_xlabel(r'$\log_{10} M_\star\ (M_{\odot})$')
    ax.set_ylabel(r'$f_{\mathrm{H_2}} = M_{\mathrm{H_2}} / (M_{\mathrm{H_2}} + M_{\mathrm{HI}})$')
    
    # Create two separate legends with new locations
    # First legend: Observations (lower left)
    obs_legend = ax.legend(obs_handles, obs_labels, loc='upper left', fontsize=14, frameon=False)
    ax.add_artist(obs_legend)
    
    # Second legend: Models (upper right)
    model_legend = ax.legend(model_handles, model_labels, loc='upper right', fontsize=14, frameon=False)
    
    # Save plot
    output_filename = output_dir + 'h2_fraction_vs_stellar_mass' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info('H2 fraction vs stellar mass analysis complete')

# ========================== MAIN EXECUTION ==========================

if __name__ == '__main__':
    
    print('Running ultra-optimized mass loading vs virial velocity analysis\n')
    
    # Setup paper plotting style FIRST
    setup_paper_style()
    
    # Setup
    setup_cache()
    start_time = time.time()
    
    logger.info(f'Parallel processing: {USE_PARALLEL}')
    logger.info(f'Max workers: {MAX_WORKERS}')
    logger.info(f'Smart sampling: {SMART_SAMPLING}')
    logger.info(f'Caching enabled: {ENABLE_CACHING}')
    logger.info(f'Memory mapping: {USE_MEMMAP}')

    seed(2222)
    volume = (Main_BoxSize/Main_Hubble_h)**3.0 * Main_VolumeFraction

    logger.info(f'Reading galaxy properties from {DirName}')
    model_files = get_file_list(DirName)
    logger.info(f'Found {len(model_files)} model files')

    # Read galaxy properties with ultra-optimized function
    logger.info('Loading Vvir...')
    Vvir = read_hdf_ultra_optimized(snap_num=Snapshot, param='Vvir')
    logger.info('Loading StellarMass...')
    StellarMass = read_hdf_ultra_optimized(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Main_Hubble_h
    logger.info('Loading Type...')
    Type = read_hdf_ultra_optimized(snap_num=Snapshot, param='Type')
    
    logger.info(f'Total galaxies: {len(Vvir)}')
    
    if len(Vvir) > 0:
        logger.info(f'Vvir range: {np.min(Vvir):.1f} - {np.max(Vvir):.1f} km/s')

        # Vectorized filtering for better performance
        valid_mask = (Vvir > 0) & (StellarMass > 0) & np.isfinite(Vvir) & np.isfinite(StellarMass)
        Vvir_valid = Vvir[valid_mask]
        StellarMass_valid = StellarMass[valid_mask]
        Type_valid = Type[valid_mask]
        
        logger.info(f'Valid galaxies: {len(Vvir_valid)}')

        # Calculate mass loading for all valid galaxies (vectorized)
        mass_loading = calculate_muratov_mass_loading(Vvir_valid, z=0.0)
        
        # Create theoretical curves for comparison
        vvir_theory = np.logspace(1, 3, 100)  # 10 to 1000 km/s
        mass_loading_theory = calculate_muratov_mass_loading(vvir_theory, z=0.0)
        
        # Also create curves for different redshifts
        mass_loading_z1 = calculate_muratov_mass_loading(vvir_theory, z=1.0)
        mass_loading_z2 = calculate_muratov_mass_loading(vvir_theory, z=2.0)

        shark_ml = pd.read_csv('./data/SHARK_v2_ml.csv', header=None, delimiter='\t')
        shark_x = shark_ml[0]  # First column
        shark_y = shark_ml[1]  # Second column

        chisholm_ml = pd.read_csv('./data/Chisholm_17_ml.csv', header=None, delimiter='\t')
        chisholm_x = chisholm_ml[0]  # First column
        chisholm_y = chisholm_ml[1]  # Second column

        heckman_ml = pd.read_csv('./data/Heckman_15_ml.csv', header=None, delimiter='\t')
        heckman_x = heckman_ml[0]  # First column
        heckman_y = heckman_ml[1]  # Second column

        rupke_ml = pd.read_csv('./data/Rupke_05_ml.csv', header=None, delimiter='\t')
        rupke_x = rupke_ml[0]  # First column
        rupke_y = rupke_ml[1]  # Second column

        sugahara_ml = pd.read_csv('./data/Sugahara_17_ml.csv', header=None, delimiter='\t')
        sugahara_x = sugahara_ml[0]  # First column
        sugahara_y = sugahara_ml[1]  # Second column  

        # Create the plot with standardized styling
        fig, ax = create_figure()
        
        # Plot theoretical curves with standardized colors
        ax.loglog(vvir_theory, mass_loading_theory, color='black', linewidth=3, 
                  label='z=0', alpha=0.9)
        ax.loglog(vvir_theory, mass_loading_z1, color=PLOT_COLORS['model_main'], 
                  linewidth=2, linestyle='--', label='z=1', alpha=0.8)
        ax.loglog(vvir_theory, mass_loading_z2, color=PLOT_COLORS['observations'], 
                  linewidth=2, linestyle='--', label='z=2', alpha=0.8)

        # Sample galaxies for plotting if there are too many
        if len(Vvir_valid) > dilute:
            indices = sample(range(len(Vvir_valid)), dilute)
            Vvir_plot = Vvir_valid[indices]
            mass_loading_plot = mass_loading[indices]
            Type_plot = Type_valid[indices]
        else:
            Vvir_plot = Vvir_valid
            mass_loading_plot = mass_loading
            Type_plot = Type_valid

        ax.scatter(shark_x, shark_y, color='grey', marker='^', s=50, label='SHARK v2')
        ax.scatter(chisholm_x, chisholm_y, color='grey', marker='o', s=50, label='Chisholm et al. 2017')
        ax.scatter(heckman_x, heckman_y, color='grey', marker='x', s=50, label='Heckman et al. 2015')
        ax.scatter(rupke_x, rupke_y, color='grey', marker='s', s=50, label='Rupke et al. 2005')
        ax.scatter(sugahara_x, sugahara_y, color='grey', marker='d', s=50, label='Sugahara et al. 2017')
        
        # Add vertical line at critical velocity
        ax.axvline(x=60, color='gray', linestyle=':', linewidth=2, alpha=0.7, 
                   label='$V_{\\mathrm{crit}} = 60$ km/s')
        
        # Add reference galaxy types with standardized styling
        # Milky Way: Vvir ~ 160 km/s
        mw_vvir = 160
        mw_eta = calculate_muratov_mass_loading(np.array([mw_vvir]), z=0.0)[0]
        ax.scatter(mw_vvir, mw_eta, marker='*', s=300, c='gold', 
                   edgecolors='black', linewidth=2, label='Milky Way-type', zorder=10)
        
        # Low-mass dwarf: Vvir ~ 25 km/s
        dwarf_vvir = 25
        dwarf_eta = calculate_muratov_mass_loading(np.array([dwarf_vvir]), z=0.0)[0]
        ax.scatter(dwarf_vvir, dwarf_eta, marker='D', s=100, c='purple', 
                   edgecolors='black', linewidth=1.5, label='Dwarf galaxy', zorder=10)
        
        # Massive cluster central: Vvir ~ 500 km/s
        cluster_vvir = 500
        cluster_eta = calculate_muratov_mass_loading(np.array([cluster_vvir]), z=0.0)[0]
        ax.scatter(cluster_vvir, cluster_eta, marker='d', s=100, c='orange', 
                   edgecolors='black', linewidth=1.5, label='Cluster', zorder=10)
        
        # Formatting with standardized styling
        ax.set_xlabel('Virial Velocity [km/s]')
        ax.set_ylabel('Mass Loading $\eta_{\\mathrm{Muratov}}$', fontsize=16)
    
        # Set axis limits
        ax.set_xlim(10, 1000)
        ax.set_ylim(0.01, 200)

        # Set x-axis to show actual values instead of scientific notation
        ax.xaxis.set_major_formatter(ScalarFormatter())
        ax.xaxis.get_major_formatter().set_scientific(False)
        
        # Set specific x-ticks for better readability
        x_ticks = [10, 20, 50, 100, 200, 500, 1000]
        ax.set_xticks(x_ticks)
        ax.set_xticklabels([f'{tick:g}' for tick in x_ticks])
        
        # Set y-axis to show actual values instead of scientific notation
        ax.yaxis.set_major_formatter(ScalarFormatter())
        ax.yaxis.get_major_formatter().set_scientific(False)
        
        # Set specific y-ticks for better readability
        y_ticks = [0.01, 0.1, 1, 10, 100]
        ax.set_yticks(y_ticks)
        ax.set_yticklabels([f'{tick:g}' for tick in y_ticks])
        
        # Standardized legend
        style_legend(ax, loc='lower left')
        
        # Save the plot with standardized function
        output_filename = OutputDir + 'mass_loading_vs_vvir' + OutputFormat
        finalize_plot(fig, output_filename)
        
        # Print some statistics
        logger.info(f'Statistics:')
        logger.info(f'Median mass loading (all): {np.median(mass_loading):.2f}')
        logger.info(f'Mean mass loading (all): {np.mean(mass_loading):.2f}')
        logger.info(f'Mass loading range: {np.min(mass_loading):.3f} - {np.max(mass_loading):.1f}')
        
        # Statistics by velocity bins
        low_v_mask = Vvir_valid < 60
        high_v_mask = Vvir_valid >= 60
        
        if np.any(low_v_mask):
            logger.info(f'Low velocity (<60 km/s): median η = {np.median(mass_loading[low_v_mask]):.2f}')
        if np.any(high_v_mask):
            logger.info(f'High velocity (≥60 km/s): median η = {np.median(mass_loading[high_v_mask]):.2f}')

    # -----------------------------------------------------------------------
    # Reionization Models Comparison Plot with standardized styling
    
    # Call the reionization plotting function
    plot_reionization_comparison(OutputDir)
    
    # -----------------------------------------------------------------------
    # Gas Mass Functions Analysis with standardized styling
    
    # Call the gas mass function plotting function
    plot_gas_mass_functions(GAS_SimConfigs, Snapshot, OutputDir)
    
    # -----------------------------------------------------------------------
    # Optional: SFR density evolution comparison
    # Uncomment the line below if you want to include SFR density plots
    # Note: This may take significant time depending on your data size
    
    # if len(SFR_SimDirs) > 1:  # Only if multiple simulations are configured
    #     plot_sfr_density_comparison()

    # -----------------------------------------------------------------------
    
    plot_stellar_mass_function_comparison(SMF_SimConfigs, Snapshot, OutputDir)
    plot_h2_fraction_vs_stellar_mass(GAS_SimConfigs, Snapshot, OutputDir)
    
    logger.info(f'Total execution time: {time.time() - start_time:.2f} seconds')
    logger.info('Analysis complete!')
    
    force_cleanup()  # Final cleanup