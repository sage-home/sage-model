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
    plt.style.use('/Users/mbradley/Documents/cohare_palatino_sty.mplstyle')
    
    # # Figure settings
    # plt.rcParams['figure.figsize'] = (5, 5)
    # plt.rcParams['figure.dpi'] = 500
    # plt.rcParams['savefig.dpi'] = 500
    # plt.rcParams['savefig.bbox'] = 'tight'
    # plt.rcParams['savefig.transparent'] = False
    
    # # Font settings
    # plt.rcParams['font.family'] = 'serif'
    # plt.rcParams['font.serif'] = ['Times New Roman', 'DejaVu Serif', 'Computer Modern Roman']
    # plt.rcParams['font.size'] = 16
    
    # # Axis label settings
    # plt.rcParams['axes.labelsize'] = 16
    # plt.rcParams['axes.labelweight'] = 'normal'
    # plt.rcParams['axes.labelpad'] = 8
    
    # # Tick settings
    # plt.rcParams['xtick.labelsize'] = 16
    # plt.rcParams['ytick.labelsize'] = 16    
    # plt.rcParams['xtick.major.size'] = 6
    # plt.rcParams['ytick.major.size'] = 6
    # plt.rcParams['xtick.minor.size'] = 3
    # plt.rcParams['ytick.minor.size'] = 3
    # plt.rcParams['xtick.major.width'] = 1.2
    # plt.rcParams['ytick.major.width'] = 1.2
    # plt.rcParams['xtick.minor.width'] = 0.8
    # plt.rcParams['ytick.minor.width'] = 0.8
    # plt.rcParams['xtick.direction'] = 'in'
    # plt.rcParams['ytick.direction'] = 'in'
    # plt.rcParams['xtick.top'] = True
    # plt.rcParams['ytick.right'] = True
    # plt.rcParams['xtick.minor.visible'] = True
    # plt.rcParams['ytick.minor.visible'] = True
    
    # # Axis settings
    # plt.rcParams['axes.linewidth'] = 1.2
    # plt.rcParams['axes.spines.left'] = True
    # plt.rcParams['axes.spines.bottom'] = True
    # plt.rcParams['axes.spines.top'] = True
    # plt.rcParams['axes.spines.right'] = True
    
    # # Line settings
    # plt.rcParams['lines.linewidth'] = 2.0
    # plt.rcParams['lines.markersize'] = 8
    # plt.rcParams['lines.markeredgewidth'] = 1.0
    
    # # Legend settings
    # plt.rcParams['legend.fontsize'] = 16
    # plt.rcParams['legend.frameon'] = False
    # plt.rcParams['legend.columnspacing'] = 1.0
    # plt.rcParams['legend.handlelength'] = 2.0
    # plt.rcParams['legend.handletextpad'] = 0.5
    # plt.rcParams['legend.labelspacing'] = 0.3
    
    # # Math text settings
    # plt.rcParams['mathtext.fontset'] = 'stix'
    # plt.rcParams['mathtext.default'] = 'regular'

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
    'HI_gas': '#1f77b4',
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
    {
        'path': '/Users/mbradley/Documents/PhD/SAGE_BROKEN/sage-model/output/millennium/', 
        'label': 'Broken Model', 
        'color': 'blue', 
        'linestyle': '-',
        'BoxSize': 62.5,
        'Hubble_h': 0.73,
        'VolumeFraction': 1.0,
        'linewidth': 3,
        'alpha': 0.9
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
    """Plot H1 and H2 mass functions for multiple models with standardized styling and error shading"""
    logger.info(f'=== Multi-Model Gas Mass Functions Analysis ===')
    
    # Mass function parameters
    binwidth = 0.1
    
    # =============== H1 MASS FUNCTION PLOT ===============
    logger.info('Creating multi-model H1 mass function plot...')
    
    fig, ax = create_figure()
    
    # Lists to collect legend handles and labels
    obs_handles_h1 = []
    obs_labels_h1 = []
    model_handles_h1 = []
    model_labels_h1 = []
    
    # Read observational data from files
    # Use the first simulation's Hubble parameter for observations (assuming similar cosmology)
    hubble_h_obs = sim_configs[0]['Hubble_h']
    
    # Load Jones et al. 2018 ALFALFA data
    try:
        jones_data = np.loadtxt('./data/HIMF_Jones18.dat')
        jones_h_original = 0.7  # h value used in Jones et al. 2018
        
        # Convert from Jones h=0.7 to simulation h value
        jones_mass_log = jones_data[:, 0] + 2*np.log10(jones_h_original/hubble_h_obs)
        jones_phi_log = jones_data[:, 1] + 3*np.log10(hubble_h_obs/jones_h_original)
        jones_phi_lower = jones_data[:, 2] + 3*np.log10(hubble_h_obs/jones_h_original)
        jones_phi_upper = jones_data[:, 3] + 3*np.log10(hubble_h_obs/jones_h_original)
        
        # Plot Jones et al. 2018 data with error bars
        jones_errorbar = ax.errorbar(jones_mass_log, jones_phi_log, 
                                   yerr=[jones_phi_log - jones_phi_lower, jones_phi_upper - jones_phi_log],
                                   color='red', marker='o', markersize=6, linestyle='None', 
                                   capsize=3, capthick=1, elinewidth=1,
                                   label='Jones et al. 2018 (ALFALFA)', zorder=5)
        obs_handles_h1.append(jones_errorbar)
        obs_labels_h1.append('Jones et al. 2018 (ALFALFA)')
        logger.info('Successfully loaded Jones et al. 2018 HIMF data')
    except Exception as e:
        logger.warning(f'Could not load Jones et al. 2018 data: {e}')
    
    # Load Zwaan et al. 2005 data
    try:
        zwaan_data = np.loadtxt('./data/HIMF_Zwaan2005.dat')
        zwaan_h_original = 0.75  # H0=75 means h=0.75
        
        # Convert from Zwaan h=0.75 to simulation h value
        zwaan_mass_log = zwaan_data[:, 0] + 2*np.log10(zwaan_h_original/hubble_h_obs)
        zwaan_phi_log = zwaan_data[:, 1] + 3*np.log10(hubble_h_obs/zwaan_h_original)
        zwaan_phi_lower = zwaan_data[:, 2]  # These are already error values, not absolute values
        zwaan_phi_upper = zwaan_data[:, 3]
        
        # Plot Zwaan et al. 2005 data with error bars
        zwaan_errorbar = ax.errorbar(zwaan_mass_log, zwaan_phi_log, 
                                   yerr=[zwaan_phi_lower, zwaan_phi_upper],
                                   color='blue', marker='^', markersize=6, linestyle='None', 
                                   capsize=3, capthick=1, elinewidth=1,
                                   label='Zwaan et al. 2005', zorder=5)
        obs_handles_h1.append(zwaan_errorbar)
        obs_labels_h1.append('Zwaan et al. 2005')
        logger.info('Successfully loaded Zwaan et al. 2005 HIMF data')
    except Exception as e:
        logger.warning(f'Could not load Zwaan et al. 2005 data: {e}')
    
    # Keep original hardcoded observational data as backup
    ObrCold = np.array([[8.009, -1.042], [8.215, -1.156], [8.409, -0.990], [8.604, -1.156],
                        [8.799, -1.208], [9.020, -1.333], [9.194, -1.385], [9.404, -1.552],
                        [9.599, -1.677], [9.788, -1.812], [9.999, -2.312], [10.172, -2.656],
                        [10.362, -3.500], [10.551, -3.635], [10.740, -5.010]], dtype=np.float32)
    
    # Convert observational data
    ObrCold_xval = np.log10(10**(ObrCold[:, 0]) / hubble_h_obs / hubble_h_obs)
    ObrCold_yval_log = ObrCold[:, 1] + np.log10(hubble_h_obs * hubble_h_obs * hubble_h_obs)
    
    # Plot observational data with standardized styling
    obr_cold_scatter = ax.scatter(ObrCold_xval, ObrCold_yval_log, color='grey', marker='s', s=80, 
               label='Obr. & Raw. 2009 (Cold Gas)', zorder=5, edgecolors='black', linewidth=0.5)
    obs_handles_h1.append(obr_cold_scatter)
    obs_labels_h1.append('Obr. & Raw. 2009 (Cold Gas)')
    
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
            H1Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
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
            model_HI_gas_log, mask_h1 = safe_log10_with_mask(counts_h1 / volume / binwidth)
            
            # Calculate error bars for SAGE 2.0 (main model only)
            if i == 0 and len(w_h1) > 0:  # Only for the main model
                # Calculate bin-by-bin errors for H1
                error_h1_upper = []
                error_h1_lower = []
                
                for bin_idx in range(NB):
                    bin_left = mi + bin_idx * binwidth
                    bin_right = mi + (bin_idx + 1) * binwidth
                    
                    # Find galaxies in this mass bin
                    bin_mask = (h1 >= bin_left) & (h1 < bin_right)
                    bin_data = h1[bin_mask]
                    
                    if len(bin_data) > 1:
                        # Calculate standard error using the provided formula
                        error_mass = np.std(bin_data) / np.sqrt(len(bin_data))
                        
                        # Convert to error in number density (fractional error)
                        if counts_h1[bin_idx] > 0:
                            # Estimate fractional error based on mass uncertainty
                            fractional_error = error_mass / (binwidth * 0.5)  # Normalize by bin width
                            log_error = fractional_error / np.log(10)  # Convert to log space
                            
                            error_h1_upper.append(model_HI_gas_log[bin_idx] + log_error)
                            error_h1_lower.append(model_HI_gas_log[bin_idx] - log_error)
                        else:
                            error_h1_upper.append(np.nan)
                            error_h1_lower.append(np.nan)
                    else:
                        error_h1_upper.append(np.nan)
                        error_h1_lower.append(np.nan)
                
                error_h1_upper = np.array(error_h1_upper)
                error_h1_lower = np.array(error_h1_lower)
                
                # Plot error shading for H1
                valid_error_mask = ~(np.isnan(error_h1_upper) | np.isnan(error_h1_lower)) & mask_h1
                if np.any(valid_error_mask):
                    ax.fill_between(xaxeshisto_h1[valid_error_mask], 
                                  error_h1_lower[valid_error_mask], 
                                  error_h1_upper[valid_error_mask],
                                  color=color, alpha=0.2, zorder=1)
            
            # Plot cold gas line (dotted) only for main model
            if i == 0:  # Only for the main (first) model
                cold_gas_line = ax.plot(xaxeshisto_cold[mask_cold], model_cold_gas_log[mask_cold], 
                       color=PLOT_COLORS['cold_gas'], linestyle=':', linewidth=3, 
                       label='Cold Gas', alpha=0.9)[0]
                model_handles_h1.append(cold_gas_line)
                model_labels_h1.append('Cold Gas')
            
            # Plot H1 gas line for each model - use original label for main model
            if i == 0:
                h1_label = 'SAGE 2.0'
            else:
                h1_label = label
            
            h1_line = ax.plot(xaxeshisto_h1[mask_h1], model_HI_gas_log[mask_h1], 
                   color=color, linestyle=linestyle, linewidth=linewidth, 
                   label=h1_label, alpha=alpha, zorder=3)[0]
            model_handles_h1.append(h1_line)
            model_labels_h1.append(h1_label)
            
            # For the main model (first one), also plot central/satellite breakdown
            if i == 0:  # Only for the first (main) model
                sats = np.where((H1Gas > 0.0) & (Type == 1))[0]
                cent = np.where((H1Gas > 0.0) & (Type == 0))[0]
                
                if len(sats) > 0:
                    mass_sat = np.log10(H1Gas[sats])
                    (countssat, _) = np.histogram(mass_sat, range=(mi, ma), bins=NB)
                    model_sats_log, mask_sats = safe_log10_with_mask(countssat / volume / binwidth)
                    sats_line = ax.plot(xaxeshisto_h1[mask_sats], model_sats_log[mask_sats], 
                           color=PLOT_COLORS['satellites'], linestyle=':', linewidth=2, 
                           label='Satellites', alpha=0.7)[0]
                    model_handles_h1.append(sats_line)
                    model_labels_h1.append('Satellites')
                
                if len(cent) > 0:
                    mass_cent = np.log10(H1Gas[cent])
                    (countscent, _) = np.histogram(mass_cent, range=(mi, ma), bins=NB)
                    model_cents_log, mask_cents = safe_log10_with_mask(countscent / volume / binwidth)
                    cents_line = ax.plot(xaxeshisto_h1[mask_cents], model_cents_log[mask_cents], 
                           color=PLOT_COLORS['centrals'], linestyle=':', linewidth=2, 
                           label='Centrals', alpha=0.7)[0]
                    model_handles_h1.append(cents_line)
                    model_labels_h1.append('Centrals')
            
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
    
    # Create two separate legends for H1 plot
    # First legend: Observations (lower left)
    obs_legend_h1 = ax.legend(obs_handles_h1, obs_labels_h1, loc='lower left', frameon=False)
    ax.add_artist(obs_legend_h1)  # Add this legend to the plot
    
    # Second legend: Models (upper right)
    model_legend_h1 = ax.legend(model_handles_h1, model_labels_h1, loc='upper right', frameon=False)
    
    output_filename_h1 = output_dir + 'h1_mass_function' + OutputFormat
    finalize_plot(fig, output_filename_h1)
    
    # =============== H2 MASS FUNCTION PLOT ===============
    logger.info('Creating multi-model H2 mass function plot...')
    
    fig, ax = create_figure()
    
    # Lists to collect legend handles and labels for H2 plot
    obs_handles_h2 = []
    obs_labels_h2 = []
    model_handles_h2 = []
    model_labels_h2 = []
    
    # Read observational data from files for H2
    # Load Fletcher et al. 2021 H2 mass function data
    try:
        fletcher_detnondet_data = np.loadtxt('./data/H2MF_Fletcher21_DetNonDet.dat')
        fletcher_h_original = 0.7  # Assuming h=0.7 for Fletcher et al. 2021 (standard value)
        
        # Convert from Fletcher h=0.7 to simulation h value
        fletcher_detnondet_mass_log = fletcher_detnondet_data[:, 0] + 2*np.log10(fletcher_h_original/hubble_h_obs)
        fletcher_detnondet_phi_log = fletcher_detnondet_data[:, 1] + 3*np.log10(hubble_h_obs/fletcher_h_original)
        fletcher_detnondet_phi_lower = fletcher_detnondet_data[:, 2] + 3*np.log10(hubble_h_obs/fletcher_h_original)  # Column 2 is lower
        fletcher_detnondet_phi_upper = fletcher_detnondet_data[:, 3] + 3*np.log10(hubble_h_obs/fletcher_h_original)  # Column 3 is upper
        
        # Plot Fletcher et al. 2021 Det+NonDet data with error bars
        fletcher_detnondet_errorbar = ax.errorbar(fletcher_detnondet_mass_log, fletcher_detnondet_phi_log, 
                                   yerr=[fletcher_detnondet_phi_log - fletcher_detnondet_phi_lower, 
                                         fletcher_detnondet_phi_upper - fletcher_detnondet_phi_log],
                                   color='red', marker='o', markersize=6, linestyle='None', 
                                   capsize=3, capthick=1, elinewidth=1,
                                   label='Fletcher et al. 2021 (Det+NonDet)', zorder=5)
        obs_handles_h2.append(fletcher_detnondet_errorbar)
        obs_labels_h2.append('Fletcher et al. 2021 (Det+NonDet)')
        logger.info('Successfully loaded Fletcher et al. 2021 Det+NonDet H2MF data')
    except Exception as e:
        logger.warning(f'Could not load Fletcher et al. 2021 Det+NonDet data: {e}')
    
    try:
        fletcher_estimated_data = np.loadtxt('./data/H2MF_Fletcher21_Estimated.dat')
        
        # Convert from Fletcher h=0.7 to simulation h value
        fletcher_estimated_mass_log = fletcher_estimated_data[:, 0] + 2*np.log10(fletcher_h_original/hubble_h_obs)
        fletcher_estimated_phi_log = fletcher_estimated_data[:, 1] + 3*np.log10(hubble_h_obs/fletcher_h_original)
        fletcher_estimated_phi_lower = fletcher_estimated_data[:, 2] + 3*np.log10(hubble_h_obs/fletcher_h_original)  # Column 2 is lower
        fletcher_estimated_phi_upper = fletcher_estimated_data[:, 3] + 3*np.log10(hubble_h_obs/fletcher_h_original)  # Column 3 is upper
        
        # Plot Fletcher et al. 2021 Estimated data with error bars
        fletcher_estimated_errorbar = ax.errorbar(fletcher_estimated_mass_log, fletcher_estimated_phi_log, 
                                   yerr=[fletcher_estimated_phi_log - fletcher_estimated_phi_lower, 
                                         fletcher_estimated_phi_upper - fletcher_estimated_phi_log],
                                   color='blue', marker='^', markersize=6, linestyle='None', 
                                   capsize=3, capthick=1, elinewidth=1,
                                   label='Fletcher et al. 2021 (Estimated)', zorder=5)
        obs_handles_h2.append(fletcher_estimated_errorbar)
        obs_labels_h2.append('Fletcher et al. 2021 (Estimated)')
        logger.info('Successfully loaded Fletcher et al. 2021 Estimated H2MF data')
    except Exception as e:
        logger.warning(f'Could not load Fletcher et al. 2021 Estimated data: {e}')
    
    # Keep original hardcoded observational data as backup
    ObrRaw = np.array([[7.300, -1.104], [7.576, -1.302], [7.847, -1.250], [8.133, -1.240],
                       [8.409, -1.344], [8.691, -1.479], [8.956, -1.792], [9.231, -2.271],
                       [9.507, -3.198], [9.788, -5.062]], dtype=np.float32)
    
    # Convert observational data
    ObrRaw_xval = np.log10(10**(ObrRaw[:, 0]) / hubble_h_obs / hubble_h_obs)
    ObrRaw_yval_log = ObrRaw[:, 1] + np.log10(hubble_h_obs * hubble_h_obs * hubble_h_obs)
    
    # Plot observational data with standardized styling
    obr_cold_scatter_h2 = ax.scatter(ObrCold_xval, ObrCold_yval_log, color='grey', marker='s', s=80, 
               label='Obr. & Raw. 2009 (Cold Gas)', zorder=5, edgecolors='black', linewidth=0.5)
    obs_handles_h2.append(obr_cold_scatter_h2)
    obs_labels_h2.append('Obr. & Raw. 2009 (Cold Gas)')
    
    obr_raw_scatter = ax.scatter(ObrRaw_xval, ObrRaw_yval_log, color='black', marker='^', s=80, 
               label='Obr. & Raw. 2009', zorder=5, edgecolors='black', linewidth=0.5)
    obs_handles_h2.append(obr_raw_scatter)
    obs_labels_h2.append('Obr. & Raw. 2009')

    
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
            
            # Calculate error bars for SAGE 2.0 (main model only)
            if i == 0 and len(w_h2) > 0:  # Only for the main model
                # Calculate bin-by-bin errors for H2
                error_h2_upper = []
                error_h2_lower = []
                
                for bin_idx in range(NB):
                    bin_left = mi + bin_idx * binwidth
                    bin_right = mi + (bin_idx + 1) * binwidth
                    
                    # Find galaxies in this mass bin
                    bin_mask = (h2 >= bin_left) & (h2 < bin_right)
                    bin_data = h2[bin_mask]
                    
                    if len(bin_data) > 1:
                        # Calculate standard error using the provided formula
                        error_mass = np.std(bin_data) / np.sqrt(len(bin_data))
                        
                        # Convert to error in number density (fractional error)
                        if counts_h2[bin_idx] > 0:
                            # Estimate fractional error based on mass uncertainty
                            fractional_error = error_mass / (binwidth * 0.5)  # Normalize by bin width
                            log_error = fractional_error / np.log(10)  # Convert to log space
                            
                            error_h2_upper.append(model_h2_gas_log[bin_idx] + log_error)
                            error_h2_lower.append(model_h2_gas_log[bin_idx] - log_error)
                        else:
                            error_h2_upper.append(np.nan)
                            error_h2_lower.append(np.nan)
                    else:
                        error_h2_upper.append(np.nan)
                        error_h2_lower.append(np.nan)
                
                error_h2_upper = np.array(error_h2_upper)
                error_h2_lower = np.array(error_h2_lower)
                
                # Plot error shading for H2
                valid_error_mask = ~(np.isnan(error_h2_upper) | np.isnan(error_h2_lower)) & mask_h2
                if np.any(valid_error_mask):
                    ax.fill_between(xaxeshisto_h2[valid_error_mask], 
                                  error_h2_lower[valid_error_mask], 
                                  error_h2_upper[valid_error_mask],
                                  color=color, alpha=0.2, zorder=1)
            
            # Plot cold gas line (dotted) only for main model
            if i == 0:  # Only for the main (first) model
                cold_gas_line_h2 = ax.plot(xaxeshisto_cold[mask_cold], model_cold_gas_log[mask_cold], 
                       color=PLOT_COLORS['cold_gas'], linestyle=':', linewidth=3, 
                       label='Cold Gas', alpha=0.9)[0]
                model_handles_h2.append(cold_gas_line_h2)
                model_labels_h2.append('Cold Gas')
            
            # Plot H2 gas line for each model - use original label for main model
            if i == 0:
                h2_label = 'SAGE 2.0'
            else:
                h2_label = label
            
            h2_line = ax.plot(xaxeshisto_h2[mask_h2], model_h2_gas_log[mask_h2], 
                   color=color, linestyle=linestyle, linewidth=linewidth, 
                   label=h2_label, alpha=alpha, zorder=3)[0]
            model_handles_h2.append(h2_line)
            model_labels_h2.append(h2_label)
            
            # For the main model (first one), also plot central/satellite breakdown
            if i == 0:  # Only for the first (main) model
                sats = np.where((H2Gas > 0.0) & (Type == 1))[0]
                cent = np.where((H2Gas > 0.0) & (Type == 0))[0]
                
                if len(sats) > 0:
                    mass_sat = np.log10(H2Gas[sats])
                    (countssat, _) = np.histogram(mass_sat, range=(mi, ma), bins=NB)
                    model_sats_log, mask_sats = safe_log10_with_mask(countssat / volume / binwidth)
                    sats_line_h2 = ax.plot(xaxeshisto_h2[mask_sats], model_sats_log[mask_sats], 
                           color=PLOT_COLORS['satellites'], linestyle=':', linewidth=2, 
                           label='Satellites', alpha=0.7)[0]
                    model_handles_h2.append(sats_line_h2)
                    model_labels_h2.append('Satellites')
                
                if len(cent) > 0:
                    mass_cent = np.log10(H2Gas[cent])
                    (countscent, _) = np.histogram(mass_cent, range=(mi, ma), bins=NB)
                    model_cents_log, mask_cents = safe_log10_with_mask(countscent / volume / binwidth)
                    cents_line_h2 = ax.plot(xaxeshisto_h2[mask_cents], model_cents_log[mask_cents], 
                           color=PLOT_COLORS['centrals'], linestyle=':', linewidth=2, 
                           label='Centrals', alpha=0.7)[0]
                    model_handles_h2.append(cents_line_h2)
                    model_labels_h2.append('Centrals')
            
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

    # Create two separate legends for H2 plot
    # First legend: Observations (lower left)
    obs_legend_h2 = ax.legend(obs_handles_h2, obs_labels_h2, loc='lower left', fontsize=14, frameon=False)
    ax.add_artist(obs_legend_h2)  # Add this legend to the plot
    
    # Second legend: Models (upper right)
    model_legend_h2 = ax.legend(model_handles_h2, model_labels_h2, loc='upper right', frameon=False)
    
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
            H1Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
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
            HI_Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            logger.info(f'  Total galaxies: {len(StellarMass)}')
            
            # Calculate sSFR for star-forming galaxy selection
            sSFR = np.log10((SfrDisk + SfrBulge) / StellarMass)
            
            # Select star-forming galaxies with valid data
            w = np.where((StellarMass > 1e8) & (ColdGas > 0) & (Type == 0) & (H2Gas > 1e7))[0]  # Central, star-forming galaxies

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
            h2_fraction_2 = H2Gas / (H2Gas + HI_Gas)  # Alternative fraction
            
            # Alternative: H2 fraction relative to cold gas
            h2_frac_cold = h2_gas_sel / cold_gas_sel
            
            # Bin by stellar mass
            log_stellar_mass = np.log10(stellar_mass_sel)
            mass_bins = np.arange(8.0, 12.0, 0.125)
            mass_centers = (mass_bins[:-1] + mass_bins[1:]) / 2  # True bin centers

            # Calculate median and error bars in each bin
            median_h2_frac = []
            error_h2_frac = []
            
            for j in range(len(mass_bins)-1):
                mask = (log_stellar_mass >= mass_bins[j]) & (log_stellar_mass < mass_bins[j+1])
                if np.sum(mask) > 20:  # Require at least 20 galaxies per bin
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
                    model_line = ax.plot(mass_centers_valid, median_h2_frac_valid, 
                        color=color, linewidth=linewidth,
                        label=label, alpha=alpha, zorder=6)[0]
                    # plt.scatter(np.log10(StellarMass), h2_fraction_2, s=1, alpha=0.25, 
                    #             color=color, rasterized=True)

                    # Shaded error region
                    ax.fill_between(mass_centers_valid, 
                                median_h2_frac_valid - error_h2_frac_valid,
                                median_h2_frac_valid + error_h2_frac_valid,
                                color=color, alpha=0.2, zorder=5)
                    
                    model_handles.append(model_line)
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

def plot_h2_fraction_vs_halo_mass(sim_configs, snapshot, output_dir):
    """Plot H1 fraction vs halo mass with both HI_Gas and HI_Gas_broken"""
    logger.info('=== H1 Fraction vs Halo Mass Analysis ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Lists to collect legend handles and labels
    obs_handles = []
    obs_labels = []
    model_handles = []
    model_labels = []

    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        
        logger.info(f'Processing {label} for H1 fraction analysis...')
        
        try:
            # Read required galaxy properties
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            H2_Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            HI_Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
            HI_Gas_broken = read_hdf_ultra_optimized(snap_num=snapshot, param='H1_gas', directory=directory) * 1.0e10 / hubble_h
            H2_Gas_broken = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
            
            logger.info(f'  Total galaxies: {len(StellarMass)}')
            
            # Debug: Check data ranges safely
            logger.info(f'  StellarMass range: {np.min(StellarMass[StellarMass > 0]):.2e} - {np.max(StellarMass):.2e}')
            logger.info(f'  Mvir range: {np.min(Mvir[Mvir > 0]):.2e} - {np.max(Mvir):.2e}')
            
            # Check HI_Gas data
            hi_gas_positive = H2_Gas[H2_Gas > 0]
            if len(hi_gas_positive) > 0:
                logger.info(f'  H2_Gas range: {np.min(hi_gas_positive):.2e} - {np.max(hi_gas_positive):.2e}')
                logger.info(f'  H2_Gas positive count: {len(hi_gas_positive)}/{len(HI_Gas)}')
            else:
                logger.info(f'  H2_Gas: NO POSITIVE VALUES (all ≤ 0)')
            
            # Check HI_Gas_broken data
            hi_gas_broken_positive = H2_Gas_broken[H2_Gas_broken > 0]
            if len(hi_gas_broken_positive) > 0:
                logger.info(f'  H2_Gas_broken range: {np.min(hi_gas_broken_positive):.2e} - {np.max(hi_gas_broken_positive):.2e}')
                logger.info(f'  H2_Gas_broken positive count: {len(hi_gas_broken_positive)}/{len(HI_Gas_broken)}')
            else:
                logger.info(f'  H2_Gas_broken: NO POSITIVE VALUES (all ≤ 0)')
            
            logger.info(f'  Log Mvir range: {np.min(np.log10(Mvir[Mvir > 0])):.2f} - {np.max(np.log10(Mvir[Mvir > 0])):.2f}')
            
            # More lenient selection criteria - start with basic requirements
            w1 = np.where((Mvir > 0) & (StellarMass > 0))[0]
            logger.info(f'  Galaxies with valid Mvir and StellarMass: {len(w1)}')
            
            # Check if arrays are properly shaped before using in logical operations
            w_hi = np.array([], dtype=int)  # Default empty with proper dtype
            w_hi_broken = np.array([], dtype=int)  # Default empty with proper dtype
            
            # Handle HI_Gas safely
            if len(H2_Gas) == len(Mvir) and len(H2_Gas) > 0:
                w2 = np.where((Mvir > 0) & (StellarMass > 0) & (H2_Gas > 0))[0]
                logger.info(f'  Galaxies with valid Mvir, StellarMass, and HI_Gas: {len(w2)}')
                w_hi = w2
            else:
                logger.info(f'  H2_Gas array incompatible: shape {H2_Gas.shape} vs expected {Mvir.shape}')
            
            # Handle HI_Gas_broken safely  
            if len(H2_Gas_broken) == len(Mvir) and len(H2_Gas_broken) > 0:
                w3 = np.where((Mvir > 0) & (StellarMass > 0) & (H2_Gas_broken > 0))[0]
                logger.info(f'  Galaxies with valid Mvir, StellarMass, and HI_Gas_broken: {len(w3)}')
                w_hi_broken = w3
            else:
                logger.info(f'  H2_Gas_broken array incompatible: shape {H2_Gas_broken.shape} vs expected {Mvir.shape}')

            if len(w_hi) == 0 and len(w_hi_broken) == 0:
                logger.warning(f'  No valid galaxies for {label}')
                continue
            
            logger.info(f'  Valid galaxies for HI_Gas: {len(w_hi)}')
            logger.info(f'  Valid galaxies for HI_Gas_broken: {len(w_hi_broken)}')
            
            # Adjust mass bins based on actual data range
            min_mass, max_mass = 10.0, 15.0  # Default values
            
            if len(w_hi) > 0:
                log_halo_mass_hi = np.log10(Mvir[w_hi])
                min_mass = np.floor(np.min(log_halo_mass_hi) * 2) / 2  # Round down to nearest 0.5
                max_mass = np.ceil(np.max(log_halo_mass_hi) * 2) / 2   # Round up to nearest 0.5
                logger.info(f'  H2_Gas mass range: {np.min(log_halo_mass_hi):.2f} - {np.max(log_halo_mass_hi):.2f}')
            
            if len(w_hi_broken) > 0:
                log_halo_mass_broken = np.log10(Mvir[w_hi_broken])
                if len(w_hi) == 0:  # Only use broken data for range if HI_Gas is empty
                    min_mass = np.floor(np.min(log_halo_mass_broken) * 2) / 2
                    max_mass = np.ceil(np.max(log_halo_mass_broken) * 2) / 2
                else:  # Extend range to include both datasets
                    min_mass = min(min_mass, np.floor(np.min(log_halo_mass_broken) * 2) / 2)
                    max_mass = max(max_mass, np.ceil(np.max(log_halo_mass_broken) * 2) / 2)
                logger.info(f'  H2_Gas_broken mass range: {np.min(log_halo_mass_broken):.2f} - {np.max(log_halo_mass_broken):.2f}')
                
            logger.info(f'  Using mass range: {min_mass:.1f} - {max_mass:.1f}')
            
            # Create bins based on data range
            mass_bins = np.arange(min_mass, max_mass + 0.125, 0.25)  # Wider bins for better statistics
            mass_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            
            logger.info(f'  Number of mass bins: {len(mass_bins)-1}')

            # Function to calculate binned statistics
            def calculate_binned_fraction(indices, hi_data_name):
                if len(indices) == 0:
                    return np.array([]), np.array([])
                    
                stellar_mass_sel = StellarMass[indices]
                halo_mass_sel = Mvir[indices]
                
                if hi_data_name == 'H2_Gas':
                    hi_gas_sel = H2_Gas[indices]
                else:  # H2_Gas_broken
                    hi_gas_sel = H2_Gas_broken[indices]

                h1_fraction = np.log10(hi_gas_sel / stellar_mass_sel)
                log_halo_mass = np.log10(halo_mass_sel)
                
                # Check for invalid values
                valid_fraction = np.isfinite(h1_fraction)
                logger.info(f'    Valid {hi_data_name} fractions: {np.sum(valid_fraction)}/{len(h1_fraction)}')
                
                if np.sum(valid_fraction) == 0:
                    return np.array([]), np.array([])
                
                h1_fraction = h1_fraction[valid_fraction]
                log_halo_mass = log_halo_mass[valid_fraction]
                
                logger.info(f'    {hi_data_name} fraction range: {np.min(h1_fraction):.3f} - {np.max(h1_fraction):.3f}')
                
                median_fraction = []
                error_fraction = []
                
                for j in range(len(mass_bins)-1):
                    mask = (log_halo_mass >= mass_bins[j]) & (log_halo_mass < mass_bins[j+1])
                    n_in_bin = np.sum(mask)
                    
                    if n_in_bin >= 3:  # Reduced requirement to 3 galaxies per bin
                        bin_data = h1_fraction[mask]
                        median_fraction.append(np.median(bin_data))
                        error_fraction.append(np.std(bin_data) / np.sqrt(len(bin_data)))
                        logger.info(f'      Bin {mass_bins[j]:.2f}-{mass_bins[j+1]:.2f}: {n_in_bin} galaxies, median={np.median(bin_data):.3f}')
                    else:
                        median_fraction.append(np.nan)
                        error_fraction.append(np.nan)
                        if n_in_bin > 0:
                            logger.info(f'      Bin {mass_bins[j]:.2f}-{mass_bins[j+1]:.2f}: {n_in_bin} galaxies (too few)')
                
                return np.array(median_fraction), np.array(error_fraction)

            # Calculate binned statistics for HI_Gas
            median_h1_frac, error_h1_frac = calculate_binned_fraction(w_hi, 'HI_Gas')
            
            # Calculate binned statistics for HI_Gas_broken
            median_h1_frac_broken, error_h1_frac_broken = calculate_binned_fraction(w_hi_broken, 'HI_Gas_broken')
            
            # Plot HI_Gas fraction
            if len(median_h1_frac) > 0:
                valid_bins = ~np.isnan(median_h1_frac)
                if np.any(valid_bins):
                    mass_centers_valid = mass_centers[valid_bins]
                    median_h1_frac_valid = median_h1_frac[valid_bins]
                    error_h1_frac_valid = error_h1_frac[valid_bins]
                    
                    logger.info(f'    Plotting HI_Gas with {len(mass_centers_valid)} valid bins')
                    
                    # Plot line
                    if i == 0:  # SAGE 2.0 model - add error bars
                        model_line = ax.plot(mass_centers_valid, median_h1_frac_valid,
                            color=color, linewidth=linewidth,
                            label=f'{label} (HI_Gas)', alpha=alpha, zorder=6)[0]

                        # Shaded error region
                        ax.fill_between(mass_centers_valid, 
                                    median_h1_frac_valid - error_h1_frac_valid,
                                    median_h1_frac_valid + error_h1_frac_valid,
                                    color=color, alpha=0.2, zorder=5)
                        
                        model_handles.append(model_line)
                    else:  # Other models - regular line
                        model_line = ax.plot(mass_centers_valid, median_h1_frac_valid, 
                                        color=color, linestyle=linestyle, linewidth=linewidth,
                                        label=f'{label} (HI_Gas)', alpha=alpha)[0]
                        model_handles.append(model_line)
                    
                    model_labels.append(f'{label} (HI_Gas)')

            # Plot HI_Gas_broken fraction
            if len(median_h1_frac_broken) > 0:
                valid_bins_broken = ~np.isnan(median_h1_frac_broken)
                if np.any(valid_bins_broken):
                    mass_centers_valid_broken = mass_centers[valid_bins_broken]
                    median_h1_frac_broken_valid = median_h1_frac_broken[valid_bins_broken]
                    error_h1_frac_broken_valid = error_h1_frac_broken[valid_bins_broken]
                    
                    logger.info(f'    Plotting HI_Gas_broken with {len(mass_centers_valid_broken)} valid bins')
                    
                    # Use dashed line style to distinguish from HI_Gas
                    broken_linestyle = '--' if linestyle == '-' else ':'
                    
                    if i == 0:  # SAGE 2.0 model - add error bars
                        model_line_broken = ax.plot(mass_centers_valid_broken, median_h1_frac_broken_valid,
                            color=color, linewidth=linewidth, linestyle=broken_linestyle,
                            label=f'{label} (HI_Gas_broken)', alpha=alpha, zorder=6)[0]

                        # Shaded error region for broken
                        ax.fill_between(mass_centers_valid_broken, 
                                    median_h1_frac_broken_valid - error_h1_frac_broken_valid,
                                    median_h1_frac_broken_valid + error_h1_frac_broken_valid,
                                    color=color, alpha=0.15, zorder=4)
                        
                        model_handles.append(model_line_broken)
                    else:  # Other models - regular line
                        model_line_broken = ax.plot(mass_centers_valid_broken, median_h1_frac_broken_valid, 
                                        color=color, linestyle=broken_linestyle, linewidth=linewidth,
                                        label=f'{label} (HI_Gas_broken)', alpha=alpha)[0]
                        model_handles.append(model_line_broken)
                    
                    model_labels.append(f'{label} (HI_Gas_broken)')
                
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            import traceback
            logger.error(traceback.format_exc())
            continue
    
    # =============== FORMATTING ===============
    
    ax.set_xlim(10.0, 15.0)
        
    ax.set_ylim(-4.0, 2.0)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.05))
    
    ax.set_xlabel(r'$\log_{10} M_{vir}\ (M_{\odot})$')
    ax.set_ylabel(r'$\log_{10} M_{H_2} / M_{\star}$')
    
    # Create legends only if we have data
    if len(obs_handles) > 0:
        obs_legend = ax.legend(obs_handles, obs_labels, loc='upper left', fontsize=14, frameon=False)
        ax.add_artist(obs_legend)
    
    if len(model_handles) > 0:
        model_legend = ax.legend(model_handles, model_labels, loc='upper right', fontsize=12, frameon=False)
    else:
        logger.warning('No model data to plot in legend')
    
    # Save plot
    output_filename = output_dir + 'h2_fraction_vs_halo_mass' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info(f'H2 fraction vs halo mass analysis complete. Plotted {len(model_handles)} data series.')

def plot_h1_fraction_vs_halo_mass(sim_configs, snapshot, output_dir):
    """Plot H1 fraction vs halo mass with both HI_Gas and HI_Gas_broken"""
    logger.info('=== H1 Fraction vs Halo Mass Analysis ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Lists to collect legend handles and labels
    obs_handles = []
    obs_labels = []
    model_handles = []
    model_labels = []

    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        
        logger.info(f'Processing {label} for H1 fraction analysis...')
        
        try:
            # Read required galaxy properties
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            H2Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            HI_Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
            HI_Gas_broken = read_hdf_ultra_optimized(snap_num=snapshot, param='H1_gas', directory=directory) * 1.0e10 / hubble_h
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
            
            logger.info(f'  Total galaxies: {len(StellarMass)}')
            
            # Debug: Check data ranges safely
            logger.info(f'  StellarMass range: {np.min(StellarMass[StellarMass > 0]):.2e} - {np.max(StellarMass):.2e}')
            logger.info(f'  Mvir range: {np.min(Mvir[Mvir > 0]):.2e} - {np.max(Mvir):.2e}')
            
            # Check HI_Gas data
            hi_gas_positive = HI_Gas[HI_Gas > 0]
            if len(hi_gas_positive) > 0:
                logger.info(f'  HI_Gas range: {np.min(hi_gas_positive):.2e} - {np.max(hi_gas_positive):.2e}')
                logger.info(f'  HI_Gas positive count: {len(hi_gas_positive)}/{len(HI_Gas)}')
            else:
                logger.info(f'  HI_Gas: NO POSITIVE VALUES (all ≤ 0)')
            
            # Check HI_Gas_broken data
            hi_gas_broken_positive = HI_Gas_broken[HI_Gas_broken > 0]
            if len(hi_gas_broken_positive) > 0:
                logger.info(f'  HI_Gas_broken range: {np.min(hi_gas_broken_positive):.2e} - {np.max(hi_gas_broken_positive):.2e}')
                logger.info(f'  HI_Gas_broken positive count: {len(hi_gas_broken_positive)}/{len(HI_Gas_broken)}')
            else:
                logger.info(f'  HI_Gas_broken: NO POSITIVE VALUES (all ≤ 0)')
            
            logger.info(f'  Log Mvir range: {np.min(np.log10(Mvir[Mvir > 0])):.2f} - {np.max(np.log10(Mvir[Mvir > 0])):.2f}')
            
            # More lenient selection criteria - start with basic requirements
            w1 = np.where((Mvir > 0) & (StellarMass > 0))[0]
            logger.info(f'  Galaxies with valid Mvir and StellarMass: {len(w1)}')
            
            # Check if arrays are properly shaped before using in logical operations
            w_hi = np.array([], dtype=int)  # Default empty with proper dtype
            w_hi_broken = np.array([], dtype=int)  # Default empty with proper dtype
            
            # Handle HI_Gas safely
            if len(HI_Gas) == len(Mvir) and len(HI_Gas) > 0:
                w2 = np.where((Mvir > 0) & (StellarMass > 0) & (HI_Gas > 0))[0]
                logger.info(f'  Galaxies with valid Mvir, StellarMass, and HI_Gas: {len(w2)}')
                w_hi = w2
            else:
                logger.info(f'  HI_Gas array incompatible: shape {HI_Gas.shape} vs expected {Mvir.shape}')
            
            # Handle HI_Gas_broken safely  
            if len(HI_Gas_broken) == len(Mvir) and len(HI_Gas_broken) > 0:
                w3 = np.where((Mvir > 0) & (StellarMass > 0) & (HI_Gas_broken > 0))[0]
                logger.info(f'  Galaxies with valid Mvir, StellarMass, and HI_Gas_broken: {len(w3)}')
                w_hi_broken = w3
            else:
                logger.info(f'  HI_Gas_broken array incompatible: shape {HI_Gas_broken.shape} vs expected {Mvir.shape}')

            if len(w_hi) == 0 and len(w_hi_broken) == 0:
                logger.warning(f'  No valid galaxies for {label}')
                continue
            
            logger.info(f'  Valid galaxies for HI_Gas: {len(w_hi)}')
            logger.info(f'  Valid galaxies for HI_Gas_broken: {len(w_hi_broken)}')
            
            # Adjust mass bins based on actual data range
            min_mass, max_mass = 10.0, 15.0  # Default values
            
            if len(w_hi) > 0:
                log_halo_mass_hi = np.log10(Mvir[w_hi])
                min_mass = np.floor(np.min(log_halo_mass_hi) * 2) / 2  # Round down to nearest 0.5
                max_mass = np.ceil(np.max(log_halo_mass_hi) * 2) / 2   # Round up to nearest 0.5
                logger.info(f'  HI_Gas mass range: {np.min(log_halo_mass_hi):.2f} - {np.max(log_halo_mass_hi):.2f}')
            
            if len(w_hi_broken) > 0:
                log_halo_mass_broken = np.log10(Mvir[w_hi_broken])
                if len(w_hi) == 0:  # Only use broken data for range if HI_Gas is empty
                    min_mass = np.floor(np.min(log_halo_mass_broken) * 2) / 2
                    max_mass = np.ceil(np.max(log_halo_mass_broken) * 2) / 2
                else:  # Extend range to include both datasets
                    min_mass = min(min_mass, np.floor(np.min(log_halo_mass_broken) * 2) / 2)
                    max_mass = max(max_mass, np.ceil(np.max(log_halo_mass_broken) * 2) / 2)
                logger.info(f'  HI_Gas_broken mass range: {np.min(log_halo_mass_broken):.2f} - {np.max(log_halo_mass_broken):.2f}')
                
            logger.info(f'  Using mass range: {min_mass:.1f} - {max_mass:.1f}')
            
            # Create bins based on data range
            mass_bins = np.arange(min_mass, max_mass + 0.125, 0.25)  # Wider bins for better statistics
            mass_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            
            logger.info(f'  Number of mass bins: {len(mass_bins)-1}')

            # Function to calculate binned statistics
            def calculate_binned_fraction(indices, hi_data_name):
                if len(indices) == 0:
                    return np.array([]), np.array([])
                    
                stellar_mass_sel = StellarMass[indices]
                halo_mass_sel = Mvir[indices]
                
                if hi_data_name == 'HI_Gas':
                    hi_gas_sel = HI_Gas[indices]
                else:  # HI_Gas_broken
                    hi_gas_sel = HI_Gas_broken[indices]

                h1_fraction = np.log10(hi_gas_sel / stellar_mass_sel)
                log_halo_mass = np.log10(halo_mass_sel)
                
                # Check for invalid values
                valid_fraction = np.isfinite(h1_fraction)
                logger.info(f'    Valid {hi_data_name} fractions: {np.sum(valid_fraction)}/{len(h1_fraction)}')
                
                if np.sum(valid_fraction) == 0:
                    return np.array([]), np.array([])
                
                h1_fraction = h1_fraction[valid_fraction]
                log_halo_mass = log_halo_mass[valid_fraction]
                
                logger.info(f'    {hi_data_name} fraction range: {np.min(h1_fraction):.3f} - {np.max(h1_fraction):.3f}')
                
                median_fraction = []
                error_fraction = []
                
                for j in range(len(mass_bins)-1):
                    mask = (log_halo_mass >= mass_bins[j]) & (log_halo_mass < mass_bins[j+1])
                    n_in_bin = np.sum(mask)
                    
                    if n_in_bin >= 3:  # Reduced requirement to 3 galaxies per bin
                        bin_data = h1_fraction[mask]
                        median_fraction.append(np.median(bin_data))
                        error_fraction.append(np.std(bin_data) / np.sqrt(len(bin_data)))
                        logger.info(f'      Bin {mass_bins[j]:.2f}-{mass_bins[j+1]:.2f}: {n_in_bin} galaxies, median={np.median(bin_data):.3f}')
                    else:
                        median_fraction.append(np.nan)
                        error_fraction.append(np.nan)
                        if n_in_bin > 0:
                            logger.info(f'      Bin {mass_bins[j]:.2f}-{mass_bins[j+1]:.2f}: {n_in_bin} galaxies (too few)')
                
                return np.array(median_fraction), np.array(error_fraction)

            # Calculate binned statistics for HI_Gas
            median_h1_frac, error_h1_frac = calculate_binned_fraction(w_hi, 'HI_Gas')
            
            # Calculate binned statistics for HI_Gas_broken
            median_h1_frac_broken, error_h1_frac_broken = calculate_binned_fraction(w_hi_broken, 'HI_Gas_broken')
            
            # Plot HI_Gas fraction
            if len(median_h1_frac) > 0:
                valid_bins = ~np.isnan(median_h1_frac)
                if np.any(valid_bins):
                    mass_centers_valid = mass_centers[valid_bins]
                    median_h1_frac_valid = median_h1_frac[valid_bins]
                    error_h1_frac_valid = error_h1_frac[valid_bins]
                    
                    logger.info(f'    Plotting HI_Gas with {len(mass_centers_valid)} valid bins')
                    
                    # Plot line
                    if i == 0:  # SAGE 2.0 model - add error bars
                        model_line = ax.plot(mass_centers_valid, median_h1_frac_valid,
                            color=color, linewidth=linewidth,
                            label=f'{label} (HI_Gas)', alpha=alpha, zorder=6)[0]

                        # Shaded error region
                        ax.fill_between(mass_centers_valid, 
                                    median_h1_frac_valid - error_h1_frac_valid,
                                    median_h1_frac_valid + error_h1_frac_valid,
                                    color=color, alpha=0.2, zorder=5)
                        
                        model_handles.append(model_line)
                    else:  # Other models - regular line
                        model_line = ax.plot(mass_centers_valid, median_h1_frac_valid, 
                                        color=color, linestyle=linestyle, linewidth=linewidth,
                                        label=f'{label} (HI_Gas)', alpha=alpha)[0]
                        model_handles.append(model_line)
                    
                    model_labels.append(f'{label} (HI_Gas)')

            # Plot HI_Gas_broken fraction
            if len(median_h1_frac_broken) > 0:
                valid_bins_broken = ~np.isnan(median_h1_frac_broken)
                if np.any(valid_bins_broken):
                    mass_centers_valid_broken = mass_centers[valid_bins_broken]
                    median_h1_frac_broken_valid = median_h1_frac_broken[valid_bins_broken]
                    error_h1_frac_broken_valid = error_h1_frac_broken[valid_bins_broken]
                    
                    logger.info(f'    Plotting HI_Gas_broken with {len(mass_centers_valid_broken)} valid bins')
                    
                    # Use dashed line style to distinguish from HI_Gas
                    broken_linestyle = '--' if linestyle == '-' else ':'
                    
                    if i == 0:  # SAGE 2.0 model - add error bars
                        model_line_broken = ax.plot(mass_centers_valid_broken, median_h1_frac_broken_valid,
                            color=color, linewidth=linewidth, linestyle=broken_linestyle,
                            label=f'{label} (HI_Gas_broken)', alpha=alpha, zorder=6)[0]

                        # Shaded error region for broken
                        ax.fill_between(mass_centers_valid_broken, 
                                    median_h1_frac_broken_valid - error_h1_frac_broken_valid,
                                    median_h1_frac_broken_valid + error_h1_frac_broken_valid,
                                    color=color, alpha=0.15, zorder=4)
                        
                        model_handles.append(model_line_broken)
                    else:  # Other models - regular line
                        model_line_broken = ax.plot(mass_centers_valid_broken, median_h1_frac_broken_valid, 
                                        color=color, linestyle=broken_linestyle, linewidth=linewidth,
                                        label=f'{label} (HI_Gas_broken)', alpha=alpha)[0]
                        model_handles.append(model_line_broken)
                    
                    model_labels.append(f'{label} (HI_Gas_broken)')
                
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            import traceback
            logger.error(traceback.format_exc())
            continue
    
    # =============== FORMATTING ===============
    
    ax.set_xlim(10.0, 15.0)
        
    ax.set_ylim(-3.0, 2.0)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.05))
    
    ax.set_xlabel(r'$\log_{10} M_{vir}\ (M_{\odot})$')
    ax.set_ylabel(r'$\log_{10} M_{H_I} / M_{\star}$')
    
    # Create legends only if we have data
    if len(obs_handles) > 0:
        obs_legend = ax.legend(obs_handles, obs_labels, loc='upper left', fontsize=14, frameon=False)
        ax.add_artist(obs_legend)
    
    if len(model_handles) > 0:
        model_legend = ax.legend(model_handles, model_labels, loc='upper right', fontsize=12, frameon=False)
    else:
        logger.warning('No model data to plot in legend')
    
    # Save plot
    output_filename = output_dir + 'h1_fraction_vs_halo_mass' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info(f'H1 fraction vs halo mass analysis complete. Plotted {len(model_handles)} data series.')

def plot_h2_fraction_vs_stellar_mass_with_selection(sim_configs, snapshot, output_dir):
    """Plot H2 fraction vs stellar mass showing selection effects"""
    logger.info('=== H2 Fraction vs Stellar Mass with Selection Effects ===')
    
    # Create figure with two subplots
    fig = plt.figure(figsize=(16, 6))
    
    # Main plot: H2 fraction vs stellar mass
    ax1 = plt.subplot(1, 2, 1)
    
    # Subplot: H2 fraction distribution 
    ax2 = plt.subplot(1, 2, 2)
    
    # =============== OBSERVATIONAL DATA ===============
    
    # Saintonge et al. 2011 (COLD GASS) - most commonly used
    saintonge_mass = np.array([9.0, 9.2, 9.4, 9.6, 9.8, 10.0, 10.2, 10.4, 10.6, 10.8, 11.0, 11.2])
    saintonge_h2_frac = np.array([0.08, 0.12, 0.15, 0.18, 0.20, 0.22, 0.20, 0.18, 0.15, 0.12, 0.08, 0.05])
    saintonge_h2_frac_err = saintonge_h2_frac * 0.3
    
    # Boselli et al. 2014 
    boselli_mass = np.array([8.5, 9.0, 9.5, 10.0, 10.5, 11.0, 11.5])
    boselli_h2_frac = np.array([0.05, 0.10, 0.18, 0.25, 0.20, 0.12, 0.06])
    
    # Plot observational data
    ax1.errorbar(saintonge_mass, saintonge_h2_frac, yerr=saintonge_h2_frac_err,
                fmt='-', color='purple', linewidth=3, markersize=6, 
                capsize=3, capthick=1, label='Saintonge et al. 2011 (COLD GASS)')
    
    ax1.scatter(boselli_mass, boselli_h2_frac, marker='s', s=80, 
               color='darkgreen', edgecolors='black', linewidth=1,
               label='Boselli et al. 2014', zorder=5)
    
    # =============== MODEL DATA WITH MULTIPLE SELECTIONS ===============
    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        hubble_h = sim_config['Hubble_h']
        
        logger.info(f'Processing {label} for selection effect analysis...')
        
        try:
            # Read required galaxy properties
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            H2Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            HI_Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            # Calculate sSFR for star-forming galaxy selection
            sSFR = np.log10((SfrDisk + SfrBulge) / StellarMass)
            
            # Calculate H2 fraction
            h2_fraction = H2Gas / (H2Gas + HI_Gas)
            log_stellar_mass = np.log10(StellarMass)
            
            # =============== DEFINE MULTIPLE SELECTIONS ===============
            
            # Selection 1: Full star-forming population
            w_full = np.where(
                (StellarMass > 1e9) & 
                (ColdGas > 1e7) & 
                (Type == 0) & 
                (sSFR > -11.5) &
                (H2Gas > 0) &
                (HI_Gas > 0)
            )[0]
            
            # Selection 2: COLD GASS-like sample  
            w_coldgass = np.where(
                (StellarMass > 5e9) &      # Intermediate mass cut
                (ColdGas > 5e7) & 
                (Type == 0) & 
                (sSFR > -10.8) &           # More active SF
                (H2Gas > 1e7)              # CO detection limit proxy
            )[0]
            
            # Selection 3: High molecular gas galaxies (like CO surveys actually detect)
            w_molecular = np.where(
                (StellarMass > 1e10) &     # High mass
                (ColdGas > 1e8) & 
                (Type == 0) & 
                (sSFR > -10.5) &           # Very active SF
                (H2Gas > 5e7) &            # Strong CO detection
                (h2_fraction > 0.08)       # Significant molecular fraction
            )[0]
            
            logger.info(f'  Selection sizes: Full={len(w_full)}, COLD GASS-like={len(w_coldgass)}, Molecular={len(w_molecular)}')
            
            # =============== PLOT DIFFERENT SELECTIONS ===============
            
            selections = [
                (w_full, 'Full Population', '--', 1.5, 0.6),
                (w_coldgass, 'COLD GASS-like', '-', 2.5, 0.8),
                (w_molecular, 'CO-detected', '-', 3.0, 1.0)
            ]
            
            mass_bins = np.arange(8.5, 12.0, 0.15)
            mass_centers = (mass_bins[:-1] + mass_bins[1:]) / 2  # True bin centers
            
            all_h2_fractions = []  # For distribution plot
            
            for j, (selection, sel_label, linestyle, linewidth, alpha) in enumerate(selections):
                if len(selection) < 10:
                    continue
                    
                # Calculate binned medians
                median_h2_frac = []
                for k in range(len(mass_bins)-1):
                    mask = (log_stellar_mass[selection] >= mass_bins[k]) & (log_stellar_mass[selection] < mass_bins[k+1])
                    if np.sum(mask) > 3:
                        median_h2_frac.append(np.median(h2_fraction[selection][mask]))
                    else:
                        median_h2_frac.append(np.nan)
                
                median_h2_frac = np.array(median_h2_frac)
                valid_bins = ~np.isnan(median_h2_frac)
                
                if np.any(valid_bins):
                    # Adjust color intensity based on selection
                    plot_color = color if j == 0 else plt.cm.Blues(0.4 + 0.3*j)
                    if j == 2:  # CO-detected sample
                        plot_color = 'red'
                    
                    ax1.plot(mass_centers[valid_bins], median_h2_frac[valid_bins], 
                            color=plot_color, linestyle=linestyle, linewidth=linewidth,
                            label=f'{label} ({sel_label})', alpha=alpha)
                    
                    # Store data for distribution plot
                    all_h2_fractions.append({
                        'data': h2_fraction[selection],
                        'label': sel_label,
                        'color': plot_color,
                        'alpha': alpha * 0.7
                    })
                    
                    # Log statistics
                    median_val = np.median(h2_fraction[selection])
                    logger.info(f'    {sel_label}: N={len(selection)}, median f_H2={median_val:.3f}')
            
            # =============== DISTRIBUTION SUBPLOT ===============
            
            # Plot H2 fraction distributions
            for dist_data in all_h2_fractions:
                # Use log scale for better visualization
                hist_data = dist_data['data']
                hist_data = hist_data[hist_data > 0.001]  # Remove very low values
                
                ax2.hist(hist_data, bins=np.logspace(-3, 0, 30), alpha=dist_data['alpha'],
                        color=dist_data['color'], label=dist_data['label'], 
                        density=True, histtype='step', linewidth=2)
            
            # Add observational "detection limit" line
            ax2.axvline(0.05, color='gray', linestyle=':', linewidth=2, 
                       label='Typical CO detection limit')
            
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
    # =============== FORMATTING ===============
    
    # Main plot formatting
    ax1.set_xlim(8.5, 11.5)
    ax1.set_ylim(0.0, 0.4)
    ax1.set_xlabel(r'$\log_{10} M_\star\ (M_{\odot})$', fontsize=14)
    ax1.set_ylabel(r'$f_{\mathrm{H_2}} = M_{\mathrm{H_2}} / (M_{\mathrm{H_2}} + M_{\mathrm{HI}})$', fontsize=14)
    ax1.legend(loc='upper right', fontsize=11, frameon=True, fancybox=True, shadow=True)
    ax1.grid(True, alpha=0.3)
    ax1.set_title('Selection Effects in Molecular Gas Surveys', fontsize=16, weight='bold')
    
    # Distribution plot formatting  
    ax2.set_xscale('log')
    ax2.set_yscale('log')
    ax2.set_xlim(0.001, 1.0)
    ax2.set_xlabel(r'$f_{\mathrm{H_2}}$', fontsize=14)
    ax2.set_ylabel('Probability Density', fontsize=14)
    ax2.legend(loc='upper right', fontsize=11)
    ax2.grid(True, alpha=0.3)
    ax2.set_title('H₂ Fraction Distributions', fontsize=16, weight='bold')
    
    # Add annotation showing selection bias
    ax1.annotate('Selection Bias:\nSurveys preferentially\ndetect high f$_{H_2}$ galaxies', 
                xy=(10.5, 0.25), xytext=(9.2, 0.35),
                fontsize=12, ha='center',
                bbox=dict(boxstyle="round,pad=0.3", facecolor="yellow", alpha=0.7),
                arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=0.2'))
    
    plt.tight_layout()
    
    # Save plot
    output_filename = output_dir + 'h2_fraction_selection_effects' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info('H2 fraction selection effects analysis complete')



def plot_h2_detection_statistics(sim_configs, snapshot, output_dir):
    """Additional plot showing detection statistics and selection quantification"""
    
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))
    
    for sim_config in sim_configs:
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        hubble_h = sim_config['Hubble_h']
        
        # Read data (same as before)
        StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
        H2Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
        HI_Gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
        ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
        SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
        SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
        Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
        
        sSFR = np.log10((SfrDisk + SfrBulge) / StellarMass)
        h2_fraction = H2Gas / (H2Gas + HI_Gas)
        
        # Basic selection
        basic_sel = (StellarMass > 1e9) & (Type == 0) & (sSFR > -11.5) & (ColdGas > 0)
        
        # Plot 1: Detection fraction vs stellar mass
        mass_bins = np.logspace(9, 11.5, 15)
        detection_fractions = []
        mass_centers = []
        
        for i in range(len(mass_bins)-1):
            mask = basic_sel & (StellarMass >= mass_bins[i]) & (StellarMass < mass_bins[i+1])
            if np.sum(mask) > 10:
                detectable = mask & (h2_fraction > 0.05) & (H2Gas > 1e7)
                detection_frac = np.sum(detectable) / np.sum(mask)
                detection_fractions.append(detection_frac)
                mass_centers.append(np.sqrt(mass_bins[i] * mass_bins[i+1]))
        
        ax1.plot(np.log10(mass_centers), detection_fractions, 'o-', color=color, label=label)
        
        # Plot 2: H2 mass vs fraction colored by detectability
        sample_mask = basic_sel & (np.random.random(len(StellarMass)) < 0.1)  # Sample for speed
        detectable_mask = sample_mask & (h2_fraction > 0.05) & (H2Gas > 1e7)
        undetectable_mask = sample_mask & ~(detectable_mask)
        
        ax2.scatter(h2_fraction[undetectable_mask], np.log10(H2Gas[undetectable_mask]), 
                   s=1, alpha=0.3, color='gray', label='Undetectable')
        ax2.scatter(h2_fraction[detectable_mask], np.log10(H2Gas[detectable_mask]), 
                   s=1, alpha=0.7, color=color, label=f'{label} (Detectable)')
        
        # Plot 3: Cumulative H2 mass in different populations
        all_sf = basic_sel
        detectable = basic_sel & (h2_fraction > 0.05) & (H2Gas > 1e7)
        
        total_h2_all = np.sum(H2Gas[all_sf])
        total_h2_det = np.sum(H2Gas[detectable])
        
        ax3.bar([f'{label}\nAll SF'], [total_h2_all], color=color, alpha=0.5)
        ax3.bar([f'{label}\nDetectable'], [total_h2_det], color=color, alpha=1.0)
        
        logger.info(f'{label}: {total_h2_det/total_h2_all*100:.1f}% of H2 mass in detectable galaxies')
    
    # Formatting
    ax1.set_xlabel('log₁₀ M* (M☉)')
    ax1.set_ylabel('CO Detection Fraction')
    ax1.set_title('Detection Fraction vs Stellar Mass')
    ax1.legend()
    
    ax2.set_xlabel('f_H₂')
    ax2.set_ylabel('log₁₀ M_H₂ (M☉)')
    ax2.set_title('H₂ Mass vs Fraction')
    ax2.legend()
    
    ax3.set_ylabel('Total H₂ Mass (M☉)')
    ax3.set_title('H₂ Mass Budget')
    
    plt.tight_layout()
    
    output_filename = output_dir + 'h2_detection_statistics' + OutputFormat
    finalize_plot(fig, output_filename)

def plot_bh_bulge_mass_relation(sim_configs, snapshot, output_dir):
    """Plot Black Hole - Bulge Mass relation with observational data and standardized styling"""
    logger.info('=== Black Hole - Bulge Mass Relation Analysis ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Store model data for median calculations
    model_data = {}
    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        
        logger.info(f'Processing {label} for BHBM relation...')
        
        try:
            # Read required galaxy properties
            BulgeMass = read_hdf_ultra_optimized(snap_num=snapshot, param='BulgeMass', directory=directory) * 1.0e10 / hubble_h
            BlackHoleMass = read_hdf_ultra_optimized(snap_num=snapshot, param='BlackHoleMass', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            logger.info(f'  Total galaxies: {len(BulgeMass)}')
            
            # Apply selection criteria: BulgeMass > 1e8 and BlackHoleMass > 1e6
            w = np.where((BulgeMass > 1.0e8) & (BlackHoleMass > 1.0e6))[0]
            logger.info(f'  Galaxies passing selection: {len(w)}')
            
            if len(w) == 0:
                logger.warning(f'  No galaxies meet selection criteria for {label}')
                continue
            
            # Calculate log masses for all valid galaxies (not sampled)
            bh_all = np.log10(BlackHoleMass[w])
            bulge_all = np.log10(BulgeMass[w])
            type_all = Type[w]
            
            # Store data for median calculations
            model_data[label] = {
                'bulge_mass': bulge_all,
                'bh_mass': bh_all,
                'type': type_all
            }
            
            # Sample down for scatter plot if too many galaxies
            if len(w) > dilute:
                w_sample = sample(list(range(len(w))), dilute)
                bh = bh_all[w_sample]
                bulge = bulge_all[w_sample]
                type_sel = type_all[w_sample]
            else:
                bh = bh_all
                bulge = bulge_all
                type_sel = type_all
            
            # # Plot scatter points with different styles for each model
            # if i == 0:  # Main model - scatter with smaller points
            #     # scatter = ax.scatter(bulge, bh, marker='o', s=2, c=color, alpha=0.5, 
            #     #                    label=f'{label}', rasterized=True)
                
            #     # Also separate central vs satellite galaxies for main model
            #     centrals = type_sel == 0
            #     satellites = type_sel == 1
                
            #     if np.any(centrals):
            #         ax.scatter(bulge[centrals], bh[centrals], marker='o', s=3, 
            #                  c=PLOT_COLORS['model_main'], alpha=0.3, rasterized=True)
                
            #     # if np.any(satellites):
            #         # ax.scatter(bulge[satellites], bh[satellites], marker='s', s=2, 
            #         #          c=PLOT_COLORS['satellites'], alpha=0.4, label='Satellites', rasterized=True)
            # else:
            #     # Other models - different markers and colors
            #     markers = ['s', '^', 'D', 'v', '<', '>']
            #     marker = markers[min(i-1, len(markers)-1)]
            #     # ax.scatter(bulge, bh, marker=marker, s=4, c=PLOT_COLORS['model_alt'], alpha=alpha, 
            #     #          label=f'{label}', rasterized=True)
            
            logger.info(f'  BH mass range: {np.min(bh_all):.2f} - {np.max(bh_all):.2f}')
            logger.info(f'  Bulge mass range: {np.min(bulge_all):.2f} - {np.max(bulge_all):.2f}')
            
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
     # =============== CALCULATE AND PLOT MEDIAN LINES ===============
    
    # Define mass bins for median calculation
    mass_bins = np.arange(8.0, 12.5, 0.2)  # 0.2 dex bins
    mass_centers = mass_bins[:-1] + 0.1
    
    for i, (model_name, data) in enumerate(model_data.items()):
        bulge_mass = data['bulge_mass']
        bh_mass = data['bh_mass']
        
        median_bh = []
        std_bh = []
        valid_centers = []
        
        # Calculate median and std in each mass bin
        for j in range(len(mass_bins)-1):
            mask = (bulge_mass >= mass_bins[j]) & (bulge_mass < mass_bins[j+1])
            if np.sum(mask) >= 5:  # Require at least 5 galaxies per bin
                bin_bh = bh_mass[mask]
                median_bh.append(np.median(bin_bh))
                std_bh.append(np.std(bin_bh))
                valid_centers.append(mass_centers[j])
        
        if len(median_bh) > 0:
            median_bh = np.array(median_bh)
            std_bh = np.array(std_bh)
            valid_centers = np.array(valid_centers)
            
            if 'SAGE 2.0' in model_name or i == 0:  # Main model - thick black line with grey shading
                # Plot thick black median line
                ax.plot(valid_centers, median_bh, color='black', linewidth=2.5, 
                       label='SAGE 2.0', alpha=0.9, zorder=10)
                
                # Plot grey shading for 1-sigma errors
                ax.fill_between(valid_centers, median_bh - std_bh, median_bh + std_bh,
                               color='grey', alpha=0.3, zorder=8)
                
                logger.info(f'  SAGE 2.0 median line: {len(valid_centers)} bins')
                
            elif 'C16' in model_name or 'Vanilla' in model_name:  # C16 model - dashed dark red line
                # Plot dashed dark red median line (no error bars)
                ax.plot(valid_centers, median_bh, color='darkred', linewidth=1.5, 
                       linestyle='--', label='SAGE C16', alpha=0.9, zorder=8)
                
                logger.info(f'  SAGE (C16) median line: {len(valid_centers)} bins')
    
    # =============== OBSERVATIONAL DATA ===============
    
    # Load observational data from files
    try:
        # Terrazas et al. 2017 - stellar mass vs BH mass
        terrazas_data = np.loadtxt('./data/MBH_host_gals_Terrazas17.dat')
        terrazas_mstar = terrazas_data[:, 0]  # log10(Mstar/Msun)
        terrazas_mbh = terrazas_data[:, 3]    # log10(MBH/Msun)
        terrazas_mbh_err = terrazas_data[:, 4]  # BH mass error
        
        # Plot Terrazas et al. data
        ax.errorbar(terrazas_mstar, terrazas_mbh, yerr=terrazas_mbh_err,
                   fmt='o', color='darkred', markersize=4, capsize=2, capthick=1,
                   alpha=0.3, label='Terrazas et al. 2017', zorder=4)
        
        logger.info(f'  Loaded {len(terrazas_mstar)} galaxies from Terrazas et al. 2017')
        
    except Exception as e:
        logger.warning(f'Could not load Terrazas et al. 2017 data: {e}')
    
    try:
        # Davis et al. 2019 - spiral galaxies (spheroid mass vs BH mass)
        davis_data = np.loadtxt('./data/MBH_Mstar_Davis2019.dat')
        davis_mbh = davis_data[:, 0]          # log10(MBH/Msun)
        davis_mbh_errup = davis_data[:, 1]    # upper error
        davis_mbh_errdn = davis_data[:, 2]    # lower error
        davis_msph = davis_data[:, 3]         # log10(Msph/Msun) - spheroid mass
        davis_mstar = davis_data[:, 5]        # log10(Mstar/Msun) - total stellar mass
        
        # Remove entries with missing spheroid mass data (marked as 00000)
        valid_sph = davis_msph > 5.0  # Reasonable lower limit
        davis_mbh_valid = davis_mbh[valid_sph]
        davis_msph_valid = davis_msph[valid_sph]
        davis_mbh_err_valid = (davis_mbh_errup[valid_sph] + davis_mbh_errdn[valid_sph]) / 2
        
        # Plot Davis et al. data (spheroid mass - more appropriate for BHBM relation)
        ax.errorbar(davis_msph_valid, davis_mbh_valid, yerr=davis_mbh_err_valid,
                   fmt='s', color='darkgreen', markersize=4, capsize=2, capthick=1,
                   alpha=0.3, label='Davis et al. 2019 (Spirals)', zorder=4)
        
        logger.info(f'  Loaded {len(davis_mbh_valid)} spiral galaxies from Davis et al. 2019')
        
    except Exception as e:
        logger.warning(f'Could not load Davis et al. 2019 data: {e}')
    
    try:
        # Sahu et al. 2019 - elliptical and S0 galaxies
        sahu_data = np.loadtxt('./data/MBH_Mstar_Sahu2019_complete.dat')
        sahu_mbh = sahu_data[:, 0]      # log10(MBH/Msun)
        sahu_mbh_err = sahu_data[:, 1]  # BH mass error
        sahu_mgal = sahu_data[:, 2]     # log10(Mgal/Msun) - total galaxy mass
        
        # Plot Sahu et al. data
        ax.errorbar(sahu_mgal, sahu_mbh, yerr=sahu_mbh_err,
                   fmt='^', color='purple', markersize=4, capsize=2, capthick=1,
                   alpha=0.3, label='Sahu et al. 2019 (E/S0)', zorder=4)
        
        logger.info(f'  Loaded {len(sahu_mbh)} elliptical/S0 galaxies from Sahu et al. 2019')
        
    except Exception as e:
        logger.warning(f'Could not load Sahu et al. 2019 data: {e}')
    
    try:
        # Sahu et al. 2019 - spheroid mass data (alternative file)
        sahu_sph_data = np.loadtxt('./data/MBH_Mstar_Sahu2019.dat')
        sahu_sph_mbh = sahu_sph_data[:, 0]      # log10(MBH/Msun)
        sahu_sph_mbh_err = sahu_sph_data[:, 1]  # BH mass error
        sahu_sph_msph = sahu_sph_data[:, 2]     # log10(Msph/Msun) - spheroid mass
        
        # Only plot if significantly different from total galaxy mass
        # Plot as smaller points to avoid overcrowding
        ax.errorbar(sahu_sph_msph, sahu_sph_mbh, yerr=sahu_sph_mbh_err,
                   fmt='v', color='darkviolet', markersize=3, capsize=1, capthick=0.5,
                   alpha=0.1, label='Sahu et al. 2019 (Spheroids)', zorder=3)
        
        logger.info(f'  Loaded {len(sahu_sph_mbh)} spheroid masses from Sahu et al. 2019')
        
    except Exception as e:
        logger.warning(f'Could not load Sahu et al. 2019 spheroid data: {e}')
    
    # =============== THEORETICAL RELATIONS ===============
    
    # Häring & Rix 2004 relation
    w_bulge = 10. ** np.arange(8.0, 12.5, 0.1)  # More reasonable range
    BHdata_haring = 10. ** (8.2 + 1.12 * np.log10(w_bulge / 1.0e11))
    ax.plot(np.log10(w_bulge), np.log10(BHdata_haring), color='blue', linewidth=3, 
            label="Häring & Rix 2004", alpha=0.7)
    
    # McConnell & Ma 2013 relation (alternative/updated)
    BHdata_mcconnell = 10. ** (8.32 + 1.05 * np.log10(w_bulge / 1.0e11))
    ax.plot(np.log10(w_bulge), np.log10(BHdata_mcconnell), color='red', linewidth=2, 
            linestyle='--', label="McConnell & Ma 2013", alpha=0.6)
    
    # Kormendy & Ho 2013 relation
    BHdata_kormendy = 10. ** (8.39 + 1.09 * np.log10(w_bulge / 1.0e11))
    ax.plot(np.log10(w_bulge), np.log10(BHdata_kormendy), color='green', linewidth=2, 
            linestyle=':', label="Kormendy & Ho 2013", alpha=0.6)
    
    # Add scatter around the relations to show observational uncertainty
    # Typical scatter is ~0.3-0.4 dex
    scatter_dex = 0.34  # Häring & Rix 2004 intrinsic scatter
    
    # Plot scatter region around Häring & Rix relation
    upper_bound = np.log10(BHdata_haring) + scatter_dex
    lower_bound = np.log10(BHdata_haring) - scatter_dex
    ax.fill_between(np.log10(w_bulge), lower_bound, upper_bound, 
                   color='blue', alpha=0.05)
    
    # =============== FORMATTING ===============
    
    # Standardized axis formatting
    ax.set_ylabel(r'$\log_{10}$ $M_{\mathrm{BH}}$ [$M_\odot$]')
    ax.set_xlabel(r'$\log_{10}$ $M_{\mathrm{*}}$ [$M_\odot$]')
    
    # Set axis limits
    ax.set_xlim(8.0, 12.5)
    ax.set_ylim(6.0, 10.5)
    
    # Set minor ticks with standardized styling
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.1))
    
    # # Add reference lines for key mass scales
    # ax.axvline(x=10.0, color='gray', linestyle=':', alpha=0.5, linewidth=1)  # MW bulge mass
    # ax.axhline(y=6.6, color='gray', linestyle=':', alpha=0.5, linewidth=1)   # MW BH mass
    
    # # Add text annotations for reference masses
    # ax.text(10.05, 4.6, 'MW bulge', rotation=90, alpha=0.7, fontsize=10)
    # ax.text(8.1, 6.7, 'MW BH', alpha=0.7, fontsize=10)
    
    # Create separate legends for models and observations
    
    # Collect handles and labels for different categories
    model_handles = []
    model_labels = []
    obs_handles = []
    obs_labels = []
    theory_handles = []
    theory_labels = []
    
    # Get all legend elements
    handles, labels = ax.get_legend_handles_labels()
    
    # Categorize legend entries
    for handle, label in zip(handles, labels):
        if any(model_name in label for model_name in ['SAGE 2.0', 'SAGE C16', 'Centrals', 'Satellites', 'Median', '1σ']):
            model_handles.append(handle)
            model_labels.append(label)
        elif any(obs_name in label for obs_name in ['Terrazas', 'Davis', 'Sahu']):
            obs_handles.append(handle)
            obs_labels.append(label)
        else:
            obs_handles.append(handle)
            obs_labels.append(label)
    
    # Create observations legend (upper left)
    if obs_handles:
        obs_legend = ax.legend(obs_handles, obs_labels, loc='upper left', 
                              fontsize=12, frameon=False)
        obs_legend.get_title().set_fontsize(12)
        ax.add_artist(obs_legend)
    
    # Combine models and theory for lower right legend
    combined_handles = model_handles + theory_handles
    combined_labels = model_labels + theory_labels
    
    # Create models + theory legend (lower right)
    if combined_handles:
        model_legend = ax.legend(combined_handles, combined_labels, loc='lower right', 
                               fontsize=12, frameon=False)
        model_legend.get_title().set_fontsize(12)
    
    # Save plot
    output_filename = output_dir + 'bh_bulge_mass_relation' + OutputFormat
    finalize_plot(fig, output_filename)
    
    # Print summary statistics
    logger.info('=== BHBM Relation Statistics ===')
    try:
        # Calculate statistics for the main simulation using stored data
        if model_data:
            main_model_name = list(model_data.keys())[0]
            main_data = model_data[main_model_name]
            
            bh_log = main_data['bh_mass']
            bulge_log = main_data['bulge_mass']
            
            # Calculate correlation coefficient
            correlation = np.corrcoef(bulge_log, bh_log)[0, 1]
            
            # Fit a linear relation
            coeffs = np.polyfit(bulge_log, bh_log, 1)
            slope, intercept = coeffs
            
            logger.info(f'Main simulation ({main_model_name}):')
            logger.info(f'  Total galaxies in relation: {len(bh_log)}')
            logger.info(f'  Correlation coefficient: {correlation:.3f}')
            logger.info(f'  Best-fit slope: {slope:.3f}')
            logger.info(f'  Best-fit intercept: {intercept:.3f}')
            logger.info(f'  Relation: log(M_BH) = {intercept:.2f} + {slope:.2f} * log(M_bulge)')
            
            # Compare to Häring & Rix
            haring_slope = 1.12
            haring_intercept = 8.2 - 1.12 * np.log10(1e11)  # Convert to same form
            logger.info(f'  Difference from Häring & Rix:')
            logger.info(f'    Slope difference: {slope - haring_slope:.3f}')
            logger.info(f'    Intercept difference: {intercept - haring_intercept:.3f}')
            
            # Print median line statistics
            logger.info(f'  Median lines plotted with 0.2 dex bins')
            if 'SAGE 2.0' in main_model_name or main_model_name == list(model_data.keys())[0]:
                logger.info(f'  SAGE 2.0: Thick black line with 1σ error bars')
            if len(model_data) > 1:
                second_model_name = list(model_data.keys())[1]
                if 'C16' in second_model_name or 'Vanilla' in second_model_name:
                    logger.info(f'  SAGE (C16): Dashed dark red line (no error bars)')
    
    except Exception as e:
        logger.warning(f'Could not calculate BHBM statistics: {e}')
    
    logger.info('Black Hole - Bulge Mass relation analysis complete')

def plot_mass_metallicity_relation(sim_configs, snapshot, output_dir):
    """Plot Mass-Metallicity relation with observational data and standardized styling"""
    logger.info('=== Mass-Metallicity Relation Analysis ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Lists to collect legend handles and labels
    obs_handles = []
    obs_labels = []
    model_handles = []
    model_labels = []
    
    # =============== MODEL DATA ===============
    
    # Define mass bins for median calculation
    mass_bins = np.arange(8.0, 12.0, 0.2)
    mass_centers = mass_bins[:-1] + 0.1
    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        
        logger.info(f'Processing {label} for mass-metallicity relation...')
        
        try:
            # Read required galaxy properties
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            MetalsColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='MetalsColdGas', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            logger.info(f'  Total galaxies: {len(StellarMass)}')
            
            # Apply selection criteria: central galaxies with gas fraction > 0.1 and stellar mass > 1e8
            w = np.where((Type == 0) & (ColdGas / (StellarMass + ColdGas) > 0.1) & (StellarMass > 1.0e8))[0]
            logger.info(f'  Galaxies passing selection: {len(w)}')
            
            if len(w) == 0:
                logger.warning(f'  No galaxies meet selection criteria for {label}')
                continue
            
            # Calculate mass and metallicity for selected galaxies
            mass = np.log10(StellarMass[w])
            Z = np.log10((MetalsColdGas[w] / ColdGas[w]) / 0.02) + 9.0
            
            # Calculate median and 1-sigma error in each mass bin
            median_Z = []
            sigma_Z = []
            valid_centers = []
            
            for j in range(len(mass_bins)-1):
                mask = (mass >= mass_bins[j]) & (mass < mass_bins[j+1])
                if np.sum(mask) >= 5:  # Require at least 5 galaxies per bin
                    bin_Z = Z[mask]
                    n_gal = len(bin_Z)
                    median_Z.append(np.median(bin_Z))
                    # 1-sigma error on the mean (standard error)
                    sigma_Z.append(np.std(bin_Z) / np.sqrt(n_gal))
                    valid_centers.append(mass_centers[j])
            
            if len(median_Z) > 0:
                median_Z = np.array(median_Z)
                sigma_Z = np.array(sigma_Z)
                valid_centers = np.array(valid_centers)
                
                if 'SAGE 2.0' in label or i == 0:  # Main model - thick black line with error bars
                    # Plot thick black median line
                    line = ax.plot(valid_centers, median_Z, color='black', linewidth=2.5, 
                                  label='SAGE 2.0', alpha=0.9, zorder=10)[0]
                    model_handles.append(line)
                    model_labels.append('SAGE 2.0')
                    
                    # Plot 1-sigma error bars (standard error of the mean)
                    # Plot grey shading for 1-sigma errors
                    ax.fill_between(valid_centers, median_Z - sigma_Z, median_Z + sigma_Z,
                                   color='grey', alpha=0.3, zorder=8)
                    
                    logger.info(f'  SAGE 2.0 median line: {len(valid_centers)} bins')
                    
                elif 'C16' in label or 'Vanilla' in label:  # C16 model - dashed dark red line
                    # Plot dashed dark red median line (no error bars)
                    line = ax.plot(valid_centers, median_Z, color='darkred', linewidth=1.5, 
                                  linestyle='--', label='SAGE (C16)', alpha=0.9, zorder=8)[0]
                    model_handles.append(line)
                    model_labels.append('SAGE (C16)')
                    
                    logger.info(f'  SAGE (C16) median line: {len(valid_centers)} bins')
                
                else:  # Other models - use config colors/styles
                    line = ax.plot(valid_centers, median_Z, color=color, linewidth=linewidth,
                                  linestyle=linestyle, label=label, alpha=alpha, zorder=7)[0]
                    model_handles.append(line)
                    model_labels.append(label)
                    
                    logger.info(f'  {label} median line: {len(valid_centers)} bins')
            
            logger.info(f'  Metallicity range: {np.min(Z):.2f} - {np.max(Z):.2f}')
            logger.info(f'  Stellar mass range: {np.min(mass):.2f} - {np.max(mass):.2f}')
            
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
    # =============== OBSERVATIONAL DATA ===============
    
    # Tremonti et al. 2004 - the primary observational reference
    try:
        tremonti_data = np.loadtxt('./data/Tremonti04.dat')
        tremonti_mass = tremonti_data[:, 0]
        tremonti_Z = tremonti_data[:, 1]
        tremonti_Z_err_low = tremonti_data[:, 2]
        tremonti_Z_err_high = tremonti_data[:, 3]
        
        # Convert IMF if needed
        if whichimf == 0:
            # Conversion from Kroupa IMF to Salpeter IMF
            tremonti_mass_corrected = np.log10(10**tremonti_mass * 1.5)
        elif whichimf == 1:
            # Conversion from Kroupa IMF to Salpeter IMF to Chabrier IMF
            tremonti_mass_corrected = np.log10(10**tremonti_mass * 1.5 / 1.8)
        else:
            tremonti_mass_corrected = tremonti_mass
        
        # Plot main line
        tremonti_line = ax.plot(tremonti_mass_corrected, tremonti_Z, '-', 
                               color='orange', linewidth=1.5, alpha=0.7,
                               label='Tremonti et al. 2004')[0]
        
        # Plot error shading
        ax.fill_between(tremonti_mass_corrected, tremonti_Z_err_low, tremonti_Z_err_high,
                       color='orange', alpha=0.2, zorder=5)
        
        obs_handles.append(tremonti_line)
        obs_labels.append('Tremonti et al. 2004')
        
        logger.info(f'Loaded Tremonti et al. 2004: {len(tremonti_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Tremonti et al. 2004 data: {e}')
        
        # Fallback to original polynomial fit
        w_obs = np.arange(7.0, 13.0, 0.1)
        Zobs = -1.492 + 1.847*w_obs - 0.08026*w_obs*w_obs
        
        if whichimf == 0:
            tremonti_line = ax.plot(np.log10((10**w_obs * 1.5)), Zobs, 'o-', linewidth=3, 
                                   label='Tremonti et al. 2004')[0]
        elif whichimf == 1:
            tremonti_line = ax.plot(np.log10((10**w_obs * 1.5 / 1.8)), Zobs, 'o-', linewidth=3, 
                                   label='Tremonti et al. 2004')[0]
        
        obs_handles.append(tremonti_line)
        obs_labels.append('Tremonti et al. 2004')
    
    # Curti et al. 2020
    try:
        curti_data = np.loadtxt('./data/Curti2020.dat')
        curti_mass = curti_data[:, 0]
        curti_Z = curti_data[:, 1]
        curti_Z_low = curti_data[:, 2]
        curti_Z_high = curti_data[:, 3]
        
        # Convert to error bars
        curti_err_low = curti_Z - curti_Z_low
        curti_err_high = curti_Z_high - curti_Z
        
        # Plot main line  
        curti_line = ax.plot(curti_mass, curti_Z, 'o-', color='red', 
                            linewidth=2, markersize=3, alpha=0.9,
                            label='Curti et al. 2020')[0]
        
        # Plot error shading
        ax.fill_between(curti_mass, curti_Z_low, curti_Z_high,
                       color='red', alpha=0.2, zorder=5)
        
        obs_handles.append(curti_line)
        obs_labels.append('Curti et al. 2020')
        
        logger.info(f'Loaded Curti et al. 2020: {len(curti_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Curti et al. 2020 data: {e}')
    
    # Andrews & Martini 2013
    try:
        andrews_data = np.loadtxt('./data/MMAdrews13.dat')
        andrews_mass = andrews_data[:, 0]
        andrews_Z = andrews_data[:, 1]
        
        # Convert IMF if needed (data is in Kroupa)
        if whichimf == 0:
            # Conversion from Kroupa IMF to Salpeter IMF
            andrews_mass_corrected = np.log10(10**andrews_mass * 1.5)
        elif whichimf == 1:
            # Conversion from Kroupa IMF to Salpeter IMF to Chabrier IMF
            andrews_mass_corrected = np.log10(10**andrews_mass * 1.5 / 1.8)
        else:
            andrews_mass_corrected = andrews_mass
        
        andrews_scatter = ax.scatter(andrews_mass_corrected, andrews_Z, marker='s', s=30,
                                   color='green', edgecolors='darkgreen', linewidth=0.5,
                                   alpha=0.7, label='Andrews & Martini 2013')
        obs_handles.append(andrews_scatter)
        obs_labels.append('Andrews & Martini 2013')
        
        logger.info(f'Loaded Andrews & Martini 2013: {len(andrews_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Andrews & Martini 2013 data: {e}')
    
    # Kewley & Ellison 2008 - T04 calibration (most commonly used)
    try:
        kewley_data = np.loadtxt('./data/MMR-Kewley08.dat')
        
        # Find T04 calibration data (starts around line with comment "#T04 (1)")
        # The T04 data appears to be from indices 59-74 based on the file structure
        t04_start = 59  # Approximate start of T04 data
        t04_end = 74    # Approximate end of T04 data
        
        kewley_mass_t04 = kewley_data[t04_start:t04_end, 0]
        kewley_Z_t04 = kewley_data[t04_start:t04_end, 1]
        
        # Convert IMF if needed (data is in Kroupa)
        if whichimf == 0:
            # Conversion from Kroupa IMF to Salpeter IMF
            kewley_mass_corrected = np.log10(10**kewley_mass_t04 * 1.5)
        elif whichimf == 1:
            # Conversion from Kroupa IMF to Salpeter IMF to Chabrier IMF
            kewley_mass_corrected = np.log10(10**kewley_mass_t04 * 1.5 / 1.8)
        else:
            kewley_mass_corrected = kewley_mass_t04
        
        kewley_scatter = ax.scatter(kewley_mass_corrected, kewley_Z_t04, 
                                   marker='D', s=40, color='purple', 
                                   edgecolors='darkmagenta', linewidth=0.8,
                                   alpha=0.8, label='Kewley & Ellison 2008')
        obs_handles.append(kewley_scatter)
        obs_labels.append('Kewley & Ellison 2008')
        
        logger.info(f'Loaded Kewley & Ellison 2008 T04: {len(kewley_mass_t04)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Kewley & Ellison 2008 data: {e}')
    
    # Gallazzi et al. 2005 - Stellar metallicity (note: this is different from gas metallicity)
    try:
        gallazzi_data = np.loadtxt('./data/MSZR-Gallazzi05.dat')
        # Skip Kirby+13 data (first 7 points) and use only Gallazzi+05 data
        gallazzi_mass = gallazzi_data[7:, 0]
        gallazzi_Z_stellar = gallazzi_data[7:, 1]  # This is log(Z*/Z_sun), not 12+log(O/H)
        
        # Convert stellar metallicity to gas-phase metallicity (approximate)
        # Typical relation: [O/H]_gas ≈ [Z/H]_stellar + 0.1 (rough approximation)
        # Convert log(Z*/Z_sun) to 12+log(O/H): 12+log(O/H) = log(Z*/Z_sun) + 8.69
        gallazzi_Z_gas_approx = gallazzi_Z_stellar + 8.69
        
        # Convert IMF if needed (data should already be in Chabrier based on comment)
        gallazzi_mass_corrected = gallazzi_mass  # Already corrected to Chabrier
        
        gallazzi_scatter = ax.scatter(gallazzi_mass_corrected, gallazzi_Z_gas_approx, 
                                    marker='^', s=25, color='orange', 
                                    edgecolors='darkorange', linewidth=0.5,
                                    alpha=0.6, label='Gallazzi et al. 2005 (stellar)')
        obs_handles.append(gallazzi_scatter)
        obs_labels.append('Gallazzi et al. 2005 (stellar)')
        
        logger.info(f'Loaded Gallazzi et al. 2005: {len(gallazzi_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Gallazzi et al. 2005 data: {e}')
    
    # =============== FORMATTING ===============
    
    # Standardized axis formatting
    ax.set_ylabel(r'$12 + \log_{10}[\mathrm{O/H}]$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    
    # Set axis limits
    ax.set_xlim(8.0, 12.0)
    ax.set_ylim(8.0, 9.5)
    
    # Set minor ticks with standardized styling
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
    
    # Create separate legends for observations and models
    
    # Observations legend (upper left)
    if obs_handles:
        obs_legend = ax.legend(obs_handles, obs_labels, loc='upper left', 
                              fontsize=12, frameon=False)
        ax.add_artist(obs_legend)
    
    # Models legend (lower right)
    if model_handles:
        model_legend = ax.legend(model_handles, model_labels, loc='lower right', 
                               fontsize=14, frameon=False)
    
    # Save plot
    output_filename = output_dir + 'mass_metallicity_relation' + OutputFormat
    finalize_plot(fig, output_filename)
    
    # Print summary statistics
    logger.info('=== Mass-Metallicity Relation Statistics ===')
    try:
        # Calculate statistics for the main simulation
        main_sim = sim_configs[0]
        directory = main_sim['path']
        hubble_h = main_sim['Hubble_h']
        
        # Re-read data for statistics (not sampled)
        StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
        ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
        MetalsColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='MetalsColdGas', directory=directory) * 1.0e10 / hubble_h
        Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
        
        w = np.where((Type == 0) & (ColdGas / (StellarMass + ColdGas) > 0.1) & (StellarMass > 1.0e8))[0]
        
        if len(w) > 0:
            mass_all = np.log10(StellarMass[w])
            Z_all = np.log10((MetalsColdGas[w] / ColdGas[w]) / 0.02) + 9.0
            
            # Calculate correlation coefficient
            correlation = np.corrcoef(mass_all, Z_all)[0, 1]
            
            # Fit a linear relation
            coeffs = np.polyfit(mass_all, Z_all, 1)
            slope, intercept = coeffs
            
            logger.info(f'Main simulation ({main_sim["label"]}):')
            logger.info(f'  Total galaxies in relation: {len(w)}')
            logger.info(f'  Correlation coefficient: {correlation:.3f}')
            logger.info(f'  Best-fit slope: {slope:.3f}')
            logger.info(f'  Best-fit intercept: {intercept:.3f}')
            logger.info(f'  Relation: 12+log(O/H) = {intercept:.2f} + {slope:.2f} * log(M*)')
            
            # Compare to Tremonti et al. at 10^10 M_sun
            tremonti_Z_10 = -1.492 + 1.847*10.0 - 0.08026*10.0*10.0
            model_Z_10 = intercept + slope * 10.0
            logger.info(f'  Metallicity at log(M*)=10.0:')
            logger.info(f'    Model: {model_Z_10:.3f}')
            logger.info(f'    Tremonti: {tremonti_Z_10:.3f}')
            logger.info(f'    Difference: {model_Z_10 - tremonti_Z_10:.3f} dex')
    
    except Exception as e:
        logger.warning(f'Could not calculate mass-metallicity statistics: {e}')
    
    logger.info('Mass-Metallicity relation analysis complete')

def plot_mass_bulge_fraction(sim_configs, snapshot, output_dir):
    """Plot Stellar Mass vs Bulge Fraction relation with standardized styling"""
    logger.info('=== Mass-Bulge Fraction Relation Analysis ===')
    
    # Create standardized figure
    fig, ax = create_figure()
    
    # Lists to collect legend handles and labels
    obs_handles = []
    obs_labels = []
    model_handles = []
    model_labels = []
    
    # =============== MODEL DATA ===============
    
    # Define mass bins for median calculation
    mass_bins = np.arange(8.0, 12.5, 0.1)
    mass_centers = mass_bins[:-1] + 0.1
    
    # Process each simulation model
    for i, sim_config in enumerate(sim_configs):
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        linestyle = sim_config['linestyle']
        linewidth = sim_config.get('linewidth', 2)
        alpha = sim_config.get('alpha', 0.8)
        hubble_h = sim_config['Hubble_h']
        
        logger.info(f'Processing {label} for mass-bulge fraction relation...')
        
        try:
            # Read required galaxy properties
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            BulgeMass = read_hdf_ultra_optimized(snap_num=snapshot, param='BulgeMass', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            
            logger.info(f'  Total galaxies: {len(StellarMass)}')
            
            # Apply selection criteria: central galaxies with stellar mass > 1e9
            w = np.where((Type == 0) & (StellarMass > 0))[0]
            logger.info(f'  Galaxies passing selection: {len(w)}')
            
            if len(w) == 0:
                logger.warning(f'  No galaxies meet selection criteria for {label}')
                continue
            
            # Calculate mass and bulge fraction for selected galaxies
            mass = np.log10(StellarMass[w])
            bulge_fraction = BulgeMass[w] / StellarMass[w]
            
            # Remove any invalid values (NaN, inf, negative)
            valid_mask = (bulge_fraction >= 0) & np.isfinite(bulge_fraction) & (bulge_fraction <= 1.0)
            mass = mass[valid_mask]
            bulge_fraction = bulge_fraction[valid_mask]
            
            logger.info(f'  Valid galaxies after cleaning: {len(mass)}')
            
            if len(mass) == 0:
                logger.warning(f'  No valid galaxies after cleaning for {label}')
                continue
            
            # Calculate median and 1-sigma error in each mass bin
            median_bf = []
            sigma_bf = []
            valid_centers = []
            
            for j in range(len(mass_bins)-1):
                mask = (mass >= mass_bins[j]) & (mass < mass_bins[j+1])
                if np.sum(mask) >= 1:  # Require at least 5 galaxies per bin
                    bin_bf = bulge_fraction[mask]
                    n_gal = len(bin_bf)
                    median_bf.append(np.median(bin_bf))
                    # 1-sigma error on the mean (standard error)
                    sigma_bf.append(np.std(bin_bf) / np.sqrt(n_gal))
                    valid_centers.append(mass_centers[j])
            
            if len(median_bf) > 0:
                median_bf = np.array(median_bf)
                sigma_bf = np.array(sigma_bf)
                valid_centers = np.array(valid_centers)
                
                if 'SAGE 2.0' in label or i == 0:  # Main model - thick black line with error bars
                    # Plot thick black median line
                    line = ax.plot(valid_centers, median_bf, color='black', linewidth=2.5, 
                                  label='SAGE 2.0', alpha=0.9, zorder=10)[0]
                    model_handles.append(line)
                    model_labels.append('SAGE 2.0')
                    
                    # Plot grey shading for 1-sigma errors
                    ax.fill_between(valid_centers, median_bf - sigma_bf, median_bf + sigma_bf,
                                   color='grey', alpha=0.3, zorder=8)
                    
                    logger.info(f'  SAGE 2.0 median line: {len(valid_centers)} bins')
                    
                elif 'C16' in label or 'Vanilla' in label:  # C16 model - dashed dark red line
                    # Plot dashed dark red median line (no error bars)
                    line = ax.plot(valid_centers, median_bf, color='darkred', linewidth=1.5, 
                                  linestyle='--', label='SAGE (C16)', alpha=0.9, zorder=8)[0]
                    model_handles.append(line)
                    model_labels.append('SAGE (C16)')
                    
                    logger.info(f'  SAGE (C16) median line: {len(valid_centers)} bins')
                
                else:  # Other models - use config colors/styles
                    line = ax.plot(valid_centers, median_bf, color=color, linewidth=linewidth,
                                  linestyle=linestyle, label=label, alpha=alpha, zorder=7)[0]
                    model_handles.append(line)
                    model_labels.append(label)
                    
                    logger.info(f'  {label} median line: {len(valid_centers)} bins')
            
            logger.info(f'  Bulge fraction range: {np.min(bulge_fraction):.3f} - {np.max(bulge_fraction):.3f}')
            logger.info(f'  Stellar mass range: {np.min(mass):.2f} - {np.max(mass):.2f}')
            
        except Exception as e:
            logger.error(f'Error processing {label}: {e}')
            continue
    
    # =============== OBSERVATIONAL DATA ===============
    
    # Moffet et al. 2016 - Primary observational reference
    try:
        moffet_data = np.loadtxt('./data/Moffet16.dat')
        moffet_mass = moffet_data[:, 0]  # log stellar mass
        moffet_bf = moffet_data[:, 1]    # bulge fraction
        moffet_bf_low = moffet_data[:, 2]   # -1sigma boundary
        moffet_bf_high = moffet_data[:, 3]  # +1sigma boundary
        
        # Plot main line
        moffet_line = ax.plot(moffet_mass, moffet_bf, 'o-', color='blue', 
                             linewidth=3, markersize=5, alpha=0.9,
                             label='Moffet et al. 2016')[0]
        
        # Plot error shading
        ax.fill_between(moffet_mass, moffet_bf_low, moffet_bf_high,
                       color='blue', alpha=0.2, zorder=5)
        
        obs_handles.append(moffet_line)
        obs_labels.append('Moffet et al. 2016')
        
        logger.info(f'Loaded Moffet et al. 2016: {len(moffet_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Moffet et al. 2016 data: {e}')
    
    # =============== FORMATTING ===============
    
    # Standardized axis formatting
    ax.set_ylabel(r'Bulge Fraction ($M_{\mathrm{bulge}}/M_{\mathrm{stars}}$)')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    
    # Set axis limits
    ax.set_xlim(8.0, 12.5)
    ax.set_ylim(0.0, 1.0)
    
    # Set minor ticks with standardized styling
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.05))
    
    # Add horizontal reference lines
    ax.axhline(y=0.0, color='gray', linestyle=':', alpha=0.5, zorder=1)  # Pure disk
    ax.axhline(y=0.5, color='gray', linestyle=':', alpha=0.5, zorder=1)  # Equal bulge/disk
    ax.axhline(y=1.0, color='gray', linestyle=':', alpha=0.5, zorder=1)  # Pure bulge
    
    # Create separate legends for observations and models
    
    # Observations legend (upper left)
    if obs_handles:
        obs_legend = ax.legend(obs_handles, obs_labels, loc='upper left', 
                              fontsize=12, frameon=False)
        ax.add_artist(obs_legend)
    
    # Models legend (lower right)
    if model_handles:
        model_legend = ax.legend(model_handles, model_labels, loc='lower right', 
                               fontsize=14, frameon=False)
    
    # Save plot
    output_filename = output_dir + 'mass_bulge_fraction_relation' + OutputFormat
    finalize_plot(fig, output_filename)
    
    # Print summary statistics
    logger.info('=== Mass-Bulge Fraction Relation Statistics ===')
    try:
        # Calculate statistics for the main simulation
        main_sim = sim_configs[0]
        directory = main_sim['path']
        hubble_h = main_sim['Hubble_h']
        
        # Re-read data for statistics
        StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
        BulgeMass = read_hdf_ultra_optimized(snap_num=snapshot, param='BulgeMass', directory=directory) * 1.0e10 / hubble_h
        Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
        
        w = np.where((Type == 0) & (StellarMass > 1.0e9))[0]
        
        if len(w) > 0:
            mass_all = np.log10(StellarMass[w])
            bulge_fraction_all = BulgeMass[w] / StellarMass[w]
            
            # Clean data
            valid_mask = (bulge_fraction_all >= 0) & np.isfinite(bulge_fraction_all) & (bulge_fraction_all <= 1.0)
            mass_all = mass_all[valid_mask]
            bulge_fraction_all = bulge_fraction_all[valid_mask]
            
            if len(mass_all) > 0:
                # Calculate correlation coefficient
                correlation = np.corrcoef(mass_all, bulge_fraction_all)[0, 1]
                
                # Fit a linear relation
                coeffs = np.polyfit(mass_all, bulge_fraction_all, 1)
                slope, intercept = coeffs
                
                # Calculate fraction statistics
                disk_dominated = np.sum(bulge_fraction_all < 0.2) / len(bulge_fraction_all)
                intermediate = np.sum((bulge_fraction_all >= 0.2) & (bulge_fraction_all < 0.8)) / len(bulge_fraction_all)
                bulge_dominated = np.sum(bulge_fraction_all >= 0.8) / len(bulge_fraction_all)
                
                logger.info(f'Main simulation ({main_sim["label"]}):')
                logger.info(f'  Total galaxies in relation: {len(mass_all)}')
                logger.info(f'  Correlation coefficient: {correlation:.3f}')
                logger.info(f'  Best-fit slope: {slope:.3f}')
                logger.info(f'  Best-fit intercept: {intercept:.3f}')
                logger.info(f'  Relation: B/T = {intercept:.3f} + {slope:.3f} * log(M*)')
                logger.info(f'  Morphological fractions:')
                logger.info(f'    Disk-dominated (B/T < 0.2): {disk_dominated:.1%}')
                logger.info(f'    Intermediate (0.2 ≤ B/T < 0.8): {intermediate:.1%}')
                logger.info(f'    Bulge-dominated (B/T ≥ 0.8): {bulge_dominated:.1%}')
                logger.info(f'  Mean bulge fraction: {np.mean(bulge_fraction_all):.3f}')
                logger.info(f'  Median bulge fraction: {np.median(bulge_fraction_all):.3f}')
    
    except Exception as e:
        logger.warning(f'Could not calculate mass-bulge fraction statistics: {e}')
    
    logger.info('Mass-Bulge Fraction relation analysis complete')

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
    logger.info('Loading Mass loading factors...')
    MassLoading = read_hdf_ultra_optimized(snap_num=Snapshot, param='MassLoading')

    logger.info(f'Total galaxies: {len(Vvir)}')
    
    if len(Vvir) > 0:
        logger.info(f'Vvir range: {np.min(Vvir):.1f} - {np.max(Vvir):.1f} km/s')

        # Vectorized filtering for better performance
        valid_mask = (Vvir > 0) & (StellarMass > 0) & np.isfinite(Vvir) & np.isfinite(StellarMass)
        Vvir_valid = Vvir[valid_mask]
        StellarMass_valid = StellarMass[valid_mask]
        Type_valid = Type[valid_mask]
        MassLoading_valid = MassLoading[valid_mask]
        logger.info(f'Mass loading factors: {MassLoading_valid}')
        logger.info(f'Valid galaxies: {len(Vvir_valid)}')

        # Calculate mass loading for all valid galaxies (vectorized)
        mass_loading = calculate_muratov_mass_loading(Vvir_valid, z=0.0)
        # mass_loading_sage = MassLoading_valid
        
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
            MassLoading_plot = MassLoading[indices]
            Type_plot = Type_valid[indices]
            StellarMass_plot = StellarMass_valid[indices]
        else:
            Vvir_plot = Vvir_valid
            mass_loading_plot = mass_loading
            MassLoading_plot = MassLoading
            Type_plot = Type_valid
            StellarMass_plot = StellarMass_valid

        # ax.scatter(Vvir, MassLoading, c='lightblue', s=5, alpha=0.7)

        ax.scatter(shark_x, shark_y, color='grey', marker='^', s=50, label='SHARK v2')
        ax.scatter(chisholm_x, chisholm_y, color='grey', marker='o', s=50, label='Chisholm et al. 2017')
        ax.scatter(heckman_x, heckman_y, color='grey', marker='x', s=50, label='Heckman et al. 2015')
        ax.scatter(rupke_x, rupke_y, color='grey', marker='s', s=50, label='Rupke et al. 2005')
        ax.scatter(sugahara_x, sugahara_y, color='grey', marker='d', s=50, label='Sugahara et al. 2017')
        
        # Color SAGE galaxies by stellar mass
        scatter = ax.scatter(Vvir_plot, MassLoading_plot, c=np.log10(StellarMass_plot), 
                           s=5, alpha=0.7, edgecolors='none', 
                           label='SAGE 2.0 galaxies', zorder=5, cmap='viridis')
        
        # Add colorbar for stellar mass
        cbar = plt.colorbar(scatter, ax=ax, pad=0.02, shrink=0.8)
        cbar.set_label(r'$\log_{10}(M_* / M_{\odot})$', fontsize=16)
        cbar.ax.tick_params(labelsize=14)
        
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
    # plot_reionization_comparison(OutputDir)
    
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
    
    # plot_stellar_mass_function_comparison(SMF_SimConfigs, Snapshot, OutputDir)
    plot_h2_fraction_vs_stellar_mass(GAS_SimConfigs, Snapshot, OutputDir)
    plot_h2_fraction_vs_halo_mass(GAS_SimConfigs, Snapshot, OutputDir)
    plot_h1_fraction_vs_halo_mass(GAS_SimConfigs, Snapshot, OutputDir)
    # plot_h2_fraction_vs_stellar_mass_with_selection(GAS_SimConfigs, Snapshot, OutputDir)

    # Add this line to your main execution section after the other plotting calls:
    plot_bh_bulge_mass_relation(SMF_SimConfigs, Snapshot, OutputDir)
    plot_mass_metallicity_relation(SMF_SimConfigs, Snapshot, OutputDir)
    plot_mass_bulge_fraction(SMF_SimConfigs, Snapshot, OutputDir)


    
    logger.info(f'Total execution time: {time.time() - start_time:.2f} seconds')
    logger.info('Analysis complete!')
    
    force_cleanup()  # Final cleanup