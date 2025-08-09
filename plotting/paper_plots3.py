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
    'model_main': 'black',
    'model_alt': '#ff7f0e', 
    'model_ref': '#2ca02c',
    'observations': '#d62728',
    'theory': '#9467bd',
    'centrals': 'purple',
    'satellites': 'darkgreen',
    'all_galaxies': "#030603",
    'cold_gas': '#7f7f7f',
    'HI_gas': '#1f77b4',
    'h2_gas': '#ff7f0e',
    'millennium': '#000000',
    'miniuchuu': '#1f77b4',
    'c16_millennium': 'darkred',
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
        'path': './output/miniuchuu_full_mod_lowerbaryonicfrac_28052025/', 
        'label': 'miniUchuu', 
        'color': 'blue', 
        'linestyle': '-',
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
        'label': 'C16 Millennium',
        'color': 'darkred',
        'linestyle': '--',
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
        'path': './output/miniuchuu_full_mod_lowerbaryonicfrac_28052025/', 
        'label': 'SAGE 2.0 miniUchuu', 
        'color': PLOT_COLORS['miniuchuu'], 
        'linestyle': '-',  # solid line
        'BoxSize': 400,  # h-1 Mpc
        'Hubble_h': 0.677,
        'VolumeFraction': 1.0
    },
    # Vanilla SAGE simulation (dashed lines)
    {
        'path': './output/millennium_vanilla', 
        'label': 'SAGE C16', 
        'color': 'darkred', 
        'linestyle': '--',  # dashed line
        'BoxSize': 62.5,  # h-1 Mpc
        'Hubble_h': 0.73,
        'VolumeFraction': 1.0
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

def plot_stellar_mass_function_comparison(sim_configs, snapshot, output_dir):
    """Plot stellar mass function comparison with 3 panels: All, Quiescent, and Star-forming galaxies"""
    logger.info('=== Stellar Mass Function Comparison (3 Panels) ===')
    
    # Create 3-panel figure
    fig, (ax1, ax2, ax3) = plt.subplots(1, 3, figsize=(18, 6))
    axes = [ax1, ax2, ax3]
    panel_titles = ['All Galaxies', 'Quiescent Galaxies', 'Star-forming Galaxies']
    
    binwidth = 0.1  # mass function histogram bin width
    
    # Lists to collect legend handles and labels (shared across panels)
    obs_handles = []
    obs_labels = []
    model_handles = []
    model_labels = []
    
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
            
            # Calculate histograms for different populations
            (counts_all, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
            xaxeshisto = binedges[:-1] + 0.5 * binwidth
            
            # Red (quiescent) galaxies
            w_red = np.where(sSFR < sSFRcut)[0]
            massRED = mass[w_red] if len(w_red) > 0 else np.array([])
            (countsRED, _) = np.histogram(massRED, range=(mi, ma), bins=NB)
            
            # Blue (star-forming) galaxies  
            w_blue = np.where(sSFR > sSFRcut)[0]
            massBLU = mass[w_blue] if len(w_blue) > 0 else np.array([])
            (countsBLU, _) = np.histogram(massBLU, range=(mi, ma), bins=NB)
            
            # Convert to number density
            phi_all = counts_all / volume / binwidth
            phi_red = countsRED / volume / binwidth
            phi_blue = countsBLU / volume / binwidth
            
            # Convert to log10 for plotting (handle zeros)
            phi_all_log = np.where(phi_all > 0, np.log10(phi_all), np.nan)
            phi_red_log = np.where(phi_red > 0, np.log10(phi_red), np.nan)
            phi_blue_log = np.where(phi_blue > 0, np.log10(phi_blue), np.nan)
            
            # Plot data for each panel
            linewidth_all = 2 if 'C16' in label else 3
            label_all = f'{label}' if 'C16' not in label else f'{label}'
            
            # Calculate 1-sigma uncertainties using simple approach
            # Use a fixed fractional uncertainty for now to ensure shading appears
            frac_error = 0.1  # 10% uncertainty as test
            
            phi_all_upper = phi_all_log + frac_error
            phi_all_lower = phi_all_log - frac_error
            phi_red_upper = phi_red_log + frac_error  
            phi_red_lower = phi_red_log - frac_error
            phi_blue_upper = phi_blue_log + frac_error
            phi_blue_lower = phi_blue_log - frac_error
            
            # Panel 1: All galaxies
            line_main = ax1.plot(xaxeshisto, phi_all_log, color=color, linestyle=linestyle, 
                               linewidth=linewidth_all, label=label_all, alpha=0.9)[0]
            if len(model_handles) < len(sim_configs):  # Only add to legend once
                model_handles.append(line_main)
                model_labels.append(label_all)
            
            # Check if this is a SAGE 2.0 model (exclude C16)
            is_sage2 = 'C16' not in label
            logger.info(f'  Adding shading for {label}: {is_sage2}')
            
            if is_sage2:
                # Grey shading for all galaxies
                valid_mask = ~np.isnan(phi_all_log)
                valid_points = np.sum(valid_mask)
                logger.info(f'  Panel 1 (All): {valid_points} valid points, phi_all_log range: {np.nanmin(phi_all_log):.2f} to {np.nanmax(phi_all_log):.2f}')
                if valid_points > 1:
                    ax1.fill_between(xaxeshisto[valid_mask], phi_all_lower[valid_mask], phi_all_upper[valid_mask],
                                   color='grey', alpha=0.5, linewidth=0, zorder=1)
                    logger.info(f'  Applied grey shading to Panel 1')
            
            # Panel 2: Quiescent galaxies
            if np.any(~np.isnan(phi_red_log)):
                ax2.plot(xaxeshisto, phi_red_log, color=color, linestyle=linestyle, 
                        linewidth=linewidth_all, alpha=0.9)
                
                # Red shading for SAGE 2.0 only
                if is_sage2:
                    valid_mask = ~np.isnan(phi_red_log)
                    valid_points = np.sum(valid_mask)
                    logger.info(f'  Panel 2 (Red): {valid_points} valid points')
                    if valid_points > 1:
                        ax2.fill_between(xaxeshisto[valid_mask], phi_red_lower[valid_mask], phi_red_upper[valid_mask],
                                       color='red', alpha=0.3, linewidth=0, zorder=1)
                        logger.info(f'  Applied red shading to Panel 2')
            
            # Panel 3: Star-forming galaxies
            if np.any(~np.isnan(phi_blue_log)):
                ax3.plot(xaxeshisto, phi_blue_log, color=color, linestyle=linestyle, 
                        linewidth=linewidth_all, alpha=0.9)
                
                # Blue shading for SAGE 2.0 only
                if is_sage2:
                    valid_mask = ~np.isnan(phi_blue_log)
                    valid_points = np.sum(valid_mask)
                    logger.info(f'  Panel 3 (Blue): {valid_points} valid points')
                    if valid_points > 1:
                        ax3.fill_between(xaxeshisto[valid_mask], phi_blue_lower[valid_mask], phi_blue_upper[valid_mask],
                                       color='blue', alpha=0.3, linewidth=0, zorder=1)
                        logger.info(f'  Applied blue shading to Panel 3')
            
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

    # Add CARNage observational data to Panel 3 (Star-forming galaxies)
    # This should be added after the Weaver data and before the legend creation
    
    try:
        # Read z=0.0 data
        carnage_z0_data = np.loadtxt('./data/Knebe_z0pt0_blue.dat', comments='#')
        carnage_z0_mass = carnage_z0_data[:, 0]  # Mass
        carnage_z0_phi = carnage_z0_data[:, 1]   # Distribution (already in log10)
        carnage_z0_err_plus = carnage_z0_data[:, 2]  # Error in +y
        carnage_z0_err_minus = carnage_z0_data[:, 3] # Error in -y
        
        # Plot z=0.0 data with error bars
        carnage_z0_plot = ax3.errorbar(carnage_z0_mass, carnage_z0_phi, 
                                      yerr=[carnage_z0_err_minus, carnage_z0_err_plus],
                                      marker='D', markersize=6, color='darkblue', 
                                      markerfacecolor='lightblue', markeredgecolor='darkblue',
                                      linestyle='none', capsize=3, capthick=1,
                                      label='CARNage z=0.0', alpha=0.8)
        
        logger.info(f'Added CARNage z=0.0 data: {len(carnage_z0_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load z0pt0.dat: {e}')
        carnage_z0_plot = None

    # Add Bell observational data to Panel 3 (Star-forming galaxies)
    # This should be added after the CARNage blue data and before the legend creation
    
    try:
        # Read Bell blue/star-forming galaxy data
        bell_blue_data = np.loadtxt('./data/Bell_z0pt0_blue.dat', comments='#')
        bell_blue_mass = bell_blue_data[:, 0]  # Mass
        bell_blue_phi = bell_blue_data[:, 1]   # Distribution (already in log10)
        bell_blue_err_plus = bell_blue_data[:, 2]  # Error in +y
        bell_blue_err_minus = bell_blue_data[:, 3] # Error in -y
        
        # Plot Bell blue data with error bars
        bell_blue_plot = ax3.errorbar(bell_blue_mass, bell_blue_phi, 
                                     yerr=[bell_blue_err_minus, bell_blue_err_plus],
                                     marker='^', markersize=6, color='mediumblue', 
                                     markerfacecolor='skyblue', markeredgecolor='mediumblue',
                                     linestyle='none', capsize=3, capthick=1,
                                     label='Bell et al. z=0.0', alpha=0.8)
        
        logger.info(f'Added Bell blue galaxy data: {len(bell_blue_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Bell blue data: {e}')
        bell_blue_plot = None
    
    # Add CARNage observational data to Panel 2 (Quiescent galaxies)
    # This should be added after the Weaver red data and before the legend creation
    
    try:
        # Read red galaxy data (assuming this is the quiescent/red galaxy data)
        carnage_red_data = np.loadtxt('./data/Knebe_z0pt0_red.dat', comments='#')
        carnage_red_mass = carnage_red_data[:, 0]  # Mass
        carnage_red_phi = carnage_red_data[:, 1]   # Distribution (already in log10)
        carnage_red_err_plus = carnage_red_data[:, 2]  # Error in +y
        carnage_red_err_minus = carnage_red_data[:, 3] # Error in -y
        
        # Plot red galaxy data with error bars
        carnage_red_plot = ax2.errorbar(carnage_red_mass, carnage_red_phi, 
                                       yerr=[carnage_red_err_minus, carnage_red_err_plus],
                                       marker='D', markersize=6, color='darkred', 
                                       markerfacecolor='lightcoral', markeredgecolor='darkred',
                                       linestyle='none', capsize=3, capthick=1,
                                       label='CARNage z=0.0', alpha=0.8)
        
        logger.info(f'Added CARNage red galaxy data: {len(carnage_red_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load red galaxy data: {e}')
        carnage_red_plot = None

    # Add Bell observational data to Panel 2 (Quiescent galaxies)
    # This should be added after the CARNage red data and before the legend creation
    
    try:
        # Read Bell red/quiescent galaxy data
        bell_red_data = np.loadtxt('./data/Bell_z0pt0_red.dat', comments='#')
        bell_red_mass = bell_red_data[:, 0]  # Mass
        bell_red_phi = bell_red_data[:, 1]   # Distribution (already in log10)
        bell_red_err_plus = bell_red_data[:, 2]  # Error in +y
        bell_red_err_minus = bell_red_data[:, 3] # Error in -y
        
        # Plot Bell red data with error bars
        bell_red_plot = ax2.errorbar(bell_red_mass, bell_red_phi, 
                                    yerr=[bell_red_err_minus, bell_red_err_plus],
                                    marker='^', markersize=6, color='firebrick', 
                                    markerfacecolor='lightpink', markeredgecolor='firebrick',
                                    linestyle='none', capsize=3, capthick=1,
                                    label='Bell et al. z=0.0', alpha=0.8)
        
        logger.info(f'Added Bell red galaxy data: {len(bell_red_mass)} points')
        
    except Exception as e:
        logger.warning(f'Could not load Bell red data: {e}')
        bell_red_plot = None


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
    Weaver_blue_y_log = np.log10(y_blue)
    Weaver_red_y_log = np.log10(y_red)
    
    # Add observational data to appropriate panels
    # Panel 1: All galaxies - Baldry et al. 2008
    fill_patch = ax1.fill_between(Baldry_xval, Baldry_yvalU_log, Baldry_yvalL_log, 
        facecolor='purple', alpha=0.25, label='Baldry et al. 2008 (z=0.1)')
    obs_handles.append(fill_patch)
    obs_labels.append('Baldry et al. 2008 (z=0.1)')
    
    # Panel 2: Quiescent galaxies - Baldry 2012 and Weaver 2023 red data
    scatter_red_baldry = ax2.scatter(Baldry_Red_x, Baldry_Red_y_log, marker='s', s=50, edgecolor='maroon', 
                facecolor='maroon', alpha=0.6, label='Baldry et al. 2012')
    scatter_red_weaver = ax2.scatter(x_red, Weaver_red_y_log, marker='o', s=50, edgecolor='red', 
                facecolor='red', alpha=0.6, label='Weaver et al. 2023')
    obs_handles.extend([scatter_red_baldry, scatter_red_weaver])
    obs_labels.extend(['Baldry et al. 2012', 'Weaver et al. 2023'])
    
    # Panel 3: Star-forming galaxies - Baldry 2012 and Weaver 2023 blue data
    scatter_blue_baldry = ax3.scatter(Baldry_Blue_x, Baldry_Blue_y_log, marker='s', s=50, edgecolor='navy', 
                facecolor='navy', alpha=0.6, label='Baldry et al. 2012')
    scatter_blue_weaver = ax3.scatter(x_blue, Weaver_blue_y_log, marker='o', s=50, edgecolor='blue', 
                facecolor='blue', alpha=0.6, label='Weaver et al. 2023')
    
    # Format all panels with standardized styling  
    for i, ax in enumerate(axes):
        ax.set_yscale('linear')  # Changed from log since we're plotting log10 values
        ax.set_xlim(8.0, 12.2)
        ax.set_ylim(-6.0, -1.0)  # Changed to log10 scale
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        ax.set_ylabel(r'$\log_{10} \phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$')
        ax.set_title(panel_titles[i], fontsize=18, pad=15)
        ax.tick_params(which='both', direction='in', top=True, right=True)

    # Create legends
    # Model legend on first panel (upper right)
    model_legend = ax1.legend(model_handles, model_labels, loc='upper right', fontsize=14, frameon=False)
    
    # Observation legend for Panel 1 (All Galaxies) - Baldry 2008 only
    obs_legend_all = ax1.legend([obs_handles[0]], ['Baldry et al. 2008 (z=0.1)'], 
                               loc='lower left', fontsize=14, frameon=False)
    ax1.add_artist(model_legend)  # Add model legend back since obs_legend overwrites it
    
    # Red observation legend for Panel 2 (Quiescent Galaxies)
    red_legend_handles = []
    red_legend_labels = []
    
    # Add Baldry 2012 red data
    red_baldry_handle = ax2.scatter([], [], marker='s', s=50, edgecolor='maroon', 
                                   facecolor='maroon', alpha=0.6)
    red_legend_handles.append(red_baldry_handle)
    red_legend_labels.append('Baldry et al. 2012')
    
    # Add Weaver 2023 red data  
    red_weaver_handle = ax2.scatter([], [], marker='o', s=50, edgecolor='red', 
                                   facecolor='red', alpha=0.6)
    red_legend_handles.append(red_weaver_handle)
    red_legend_labels.append('Weaver et al. 2023')

    # Add CARNage red data if successfully loaded
    if 'carnage_red_plot' in locals() and carnage_red_plot is not None:
        carnage_red_handle = ax2.scatter([], [], marker='D', s=50, edgecolor='darkred', 
                                        facecolor='lightcoral', alpha=0.8)
        red_legend_handles.append(carnage_red_handle)
        red_legend_labels.append('Knebe et al. 2018')
    
    # Add Bell red data if successfully loaded
    if 'bell_red_plot' in locals() and bell_red_plot is not None:
        bell_red_handle = ax2.scatter([], [], marker='^', s=50, edgecolor='firebrick', 
                                     facecolor='lightpink', alpha=0.8)
        red_legend_handles.append(bell_red_handle)
        red_legend_labels.append('Bell et al. 2003')
    
    red_legend = ax2.legend(red_legend_handles, red_legend_labels, 
                           loc='lower left', fontsize=14, frameon=False)
    
    # Blue observation legend for Panel 3 (Star-forming Galaxies)
    blue_legend_handles = []
    blue_legend_labels = []
    
    # Add Baldry 2012 blue data
    blue_baldry_handle = ax3.scatter([], [], marker='s', s=50, edgecolor='navy', 
                                    facecolor='navy', alpha=0.6)
    blue_legend_handles.append(blue_baldry_handle)
    blue_legend_labels.append('Baldry et al. 2012')
    
    # Add Weaver 2023 blue data
    blue_weaver_handle = ax3.scatter([], [], marker='o', s=50, edgecolor='blue', 
                                    facecolor='blue', alpha=0.6)
    blue_legend_handles.append(blue_weaver_handle)
    blue_legend_labels.append('Weaver et al. 2023')

    # Add CARNage data if successfully loaded
    if 'carnage_z0_plot' in locals() and carnage_z0_plot is not None:
        carnage_z0_handle = ax3.scatter([], [], marker='D', s=50, edgecolor='darkblue', 
                                       facecolor='lightblue', alpha=0.8)
        blue_legend_handles.append(carnage_z0_handle)
        blue_legend_labels.append('Knebe et al. 2018')

    # Add Bell blue data if successfully loaded
    if 'bell_blue_plot' in locals() and bell_blue_plot is not None:
        bell_blue_handle = ax3.scatter([], [], marker='^', s=50, edgecolor='mediumblue', 
                                      facecolor='skyblue', alpha=0.8)
        blue_legend_handles.append(bell_blue_handle)
        blue_legend_labels.append('Bell et al. 2003')
    
    blue_legend = ax3.legend(blue_legend_handles, blue_legend_labels, 
                            loc='lower left', fontsize=14, frameon=False)
    
    # Adjust layout for better spacing
    plt.tight_layout()
    
    # Save plot
    output_filename = output_dir + 'stellar_mass_function_comparison_3panel' + OutputFormat
    finalize_plot(fig, output_filename)
    
    logger.info('Stellar mass function comparison (3-panel) plot complete')


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

    OutputDir = DirName + 'plots/'
    Path(OutputDir).mkdir(exist_ok=True)

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

    
    plot_stellar_mass_function_comparison(SMF_SimConfigs, Snapshot, OutputDir)

    logger.info(f'Total execution time: {time.time() - start_time:.2f} seconds')
    logger.info('Analysis complete!')
    
    force_cleanup()  # Final cleanup