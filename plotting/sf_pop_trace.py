import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
import time
from matplotlib.ticker import ScalarFormatter
from random import sample, seed
import gc  # For garbage collection
import psutil  # For memory monitoring
from pathlib import Path
import logging
import pandas as pd

# MPI for grid parallelization
try:
    from mpi4py import MPI
    HAS_MPI = True
except ImportError:
    HAS_MPI = False
    MPI = None

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
    'miniUchuu': '#ff7f0e',  # Orange for your miniUchuu model
    'c16_millennium': 'darkred',
    'your_model_name': '#ff7f0e',  # Orange - change this to match your model name
    # Reionization model colors
    'enhanced_gnedin': '#1f77b4',  # blue
    'sobacchi_mesinger': '#d62728',  # red  
    'patchy_reionization': '#2ca02c',  # green
    'original_gnedin': '#000000'  # black
}

# ========================== USER OPTIONS ==========================

# Define model configurations to run the full analysis on
MODEL_CONFIGS = [
    # Model 1: Millennium
    {
        'name': 'millennium',
        'directory': './output/millennium/',
        'filename': 'model_0.hdf5',
        'snapshot': 'Snap_63',
        'hubble_h': 0.73,
        'box_size': 62.5,  # h-1 Mpc
        'volume_fraction': 1.0,
        'first_snap': 0,
        'last_snap': 63,
        'redshifts': [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
                     9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
                     2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
                     0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
                     0.116, 0.089, 0.064, 0.041, 0.020, 0.000],
        'smf_snaps': [63, 40, 32, 27, 23, 20, 18, 16],
        'bhmf_snaps': [63, 40, 32, 27, 23, 20, 18, 16]
    },
    # Model 2: Add your second model here
    # TEMPLATE - uncomment and fill in your values:
    # {
    #     'name': 'miniUchuu',  # This should match a key in PLOT_COLORS for coloring
    #     'directory': './output/miniuchuu_full_mod_21072025/',  # Path to your model output directory
    #     'filename': 'model_0.hdf5',  # HDF5 filename (usually stays the same)
    #     'snapshot': 'Snap_49',  # Your final snapshot (e.g., 'Snap_127' if you have 128 snapshots)
    #     'hubble_h': 0.6774,  # Your Hubble parameter (e.g., 0.6774)
    #     'box_size': 400.0,  # Your box size in h-1 Mpc (e.g., 200.0)
    #     'volume_fraction': 1.0,  # Usually 1.0 unless you're using a subvolume
    #     'first_snap': 0,  # Usually 0
    #     'last_snap': 49,  # Your last snapshot number (e.g., 127)
    #     'redshifts': [13.9334, 12.67409, 11.50797, 10.44649, 9.480752, 8.58543, 7.77447, 7.032387, 6.344409, 5.721695,
    #        5.153127, 4.629078, 4.26715, 3.929071, 3.610462, 3.314082, 3.128427, 2.951226, 2.77809, 2.616166,
    #        2.458114, 2.309724, 2.16592, 2.027963, 1.8962, 1.770958, 1.65124, 1.535928, 1.426272, 1.321656,
    #        1.220303, 1.124166, 1.031983, 0.9441787, 0.8597281, 0.779046, 0.7020205, 0.6282588, 0.5575475, 0.4899777,
    #        0.4253644, 0.3640053, 0.3047063, 0.2483865, 0.1939743, 0.1425568, 0.09296665, 0.0455745, 0.02265383, 0.0001130128],  # Array of redshifts for each snapshot
    #     'smf_snaps': [49, 38, 32, 23, 17, 13, 10, 8, 7, 5, 4],  # Which snapshots to use for SMF plots
    #     'bhmf_snaps': [49, 38, 32, 23, 17, 13, 10, 8, 7, 5, 4]  # Which snapshots to use for BHMF plots
    # }
    # {
    #     'name': 'Broken model',  # This should match a key in PLOT_COLORS for coloring
    #     'directory': '/Users/mbradley/Documents/PhD/SAGE-VANILLA/sage-model/output/millennium/',  # Path to your model output directory
    #     'filename': 'model_0.hdf5',  # HDF5 filename (usually stays the same)
    #     'snapshot': 'Snap_63',  # Your final snapshot (e.g., 'Snap_127' if you have 128 snapshots)
    #     'hubble_h': 0.73,  # Your Hubble parameter (e.g., 0.6774)
    #     'box_size': 62.5,  # Your box size in h-1 Mpc (e.g., 200.0)
    #     'volume_fraction': 1.0,  # Usually 1.0 unless you're using a subvolume
    #     'first_snap': 0,  # Usually 0
    #     'last_snap': 63,  # Your last snapshot number (e.g., 127)
    #     'redshifts': [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
    #                  9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
    #                  2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
    #                  0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
    #                  0.116, 0.089, 0.064, 0.041, 0.020, 0.000],
    #     'smf_snaps': [63, 40, 32, 27, 23, 20, 18, 16],
    #     'bhmf_snaps': [63, 40, 32, 27, 23, 20, 18, 16]
    # }
    ]

# Global variables (will be updated for each model during execution)
DirName = None
FileName = None
Snapshot = None
Main_Hubble_h = None
Main_BoxSize = None
Main_VolumeFraction = None
FirstSnap = None
LastSnap = None
redshifts = None
SMFsnaps = None
BHMFsnaps = None

# Define simulation configurations for SMF comparison (will be updated per model)
SMF_SimConfigs = []

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7000       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

OutputFormat = '.pdf'

def setup_model_config(model_config):
    """
    Set up global variables for a specific model configuration
    
    Parameters:
    -----------
    model_config : dict
        Dictionary containing model configuration parameters
    """
    global DirName, FileName, Snapshot, Main_Hubble_h, Main_BoxSize, Main_VolumeFraction
    global FirstSnap, LastSnap, redshifts, SMFsnaps, BHMFsnaps, SMF_SimConfigs
    
    # Update global variables
    DirName = model_config['directory']
    FileName = model_config['filename']
    Snapshot = model_config['snapshot']
    Main_Hubble_h = model_config['hubble_h']
    Main_BoxSize = model_config['box_size']
    Main_VolumeFraction = model_config['volume_fraction']
    FirstSnap = model_config['first_snap']
    LastSnap = model_config['last_snap']
    redshifts = model_config['redshifts']
    SMFsnaps = model_config['smf_snaps']
    BHMFsnaps = model_config['bhmf_snaps']
    
    # Update SMF_SimConfigs for this model
    SMF_SimConfigs = [
        {
            'path': model_config['directory'],
            'label': f'SAGE 2.0 ({model_config["name"]})',
            'color': PLOT_COLORS.get(model_config['name'], '#1f77b4'),  # Default color if not found
            'linestyle': '-',
            'BoxSize': model_config['box_size'],
            'Hubble_h': model_config['hubble_h'],
            'VolumeFraction': model_config['volume_fraction']
        }
    ]
    
    logger.info(f'Configured for model: {model_config["name"]}')
    logger.info(f'  Directory: {DirName}')
    logger.info(f'  Snapshot: {Snapshot}')
    logger.info(f'  Hubble h: {Main_Hubble_h}')
    logger.info(f'  Box size: {Main_BoxSize} h-1 Mpc')
    logger.info(f'  Volume fraction: {Main_VolumeFraction}')
    logger.info(f'  Snapshots: {FirstSnap} - {LastSnap}')

# Enhanced performance optimization options
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
    
    # Simple sequential reading for all files
    logger.info(f"Using sequential reading for {len(model_files)} file(s)")
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

def compute_grid_cell_quiescent_fraction(args):
    """
    Compute quiescent fraction for a single grid cell.
    This function is designed to be used with MPI or multiprocessing.
    """
    (i, j, halo_grid, stellar_grid, all_log_halo, all_log_stellar, 
     all_quiescent, max_distance) = args
    
    # Define bin boundaries
    halo_bin_min, halo_bin_max = halo_grid[i], halo_grid[i + 1]
    stellar_bin_min, stellar_bin_max = stellar_grid[j], stellar_grid[j + 1]
    
    # Get center of this grid cell
    halo_center = (halo_bin_min + halo_bin_max) / 2
    stellar_center = (stellar_bin_min + stellar_bin_max) / 2
    
    # Calculate distance to nearest data point
    distances = np.sqrt((all_log_halo - halo_center)**2 + (all_log_stellar - stellar_center)**2)
    min_distance = np.min(distances)
    
    # Only fill grid cells that are close enough to actual data
    if min_distance <= max_distance:
        # Find galaxies in this bin
        mask = ((all_log_halo >= halo_bin_min) & (all_log_halo < halo_bin_max) &
               (all_log_stellar >= stellar_bin_min) & (all_log_stellar < stellar_bin_max))

        if np.sum(mask) >= 5:  # Include every bin with at least 5 galaxies
            quiescent_frac = np.mean(all_quiescent[mask])
            return (j, i, quiescent_frac)
        else:
            # For empty bins, interpolate from nearest neighbors
            # Use the 10 nearest neighbors for interpolation
            nearest_indices = np.argsort(distances)[:min(5, len(distances))]
            if len(nearest_indices) > 0:
                quiescent_frac = np.mean(all_quiescent[nearest_indices])
                return (j, i, quiescent_frac)
    
    return (j, i, np.nan)


def compute_quiescent_fraction_grid_mpi(all_log_halo, all_log_stellar, all_quiescent, 
                                       halo_grid, stellar_grid, max_distance=0.3):
    """
    Compute quiescent fraction grid using MPI parallelization over grid cells.
    Each MPI rank processes a subset of grid cells.
    """
    if not HAS_MPI:
        logger.info('MPI not available, falling back to serial processing')
        return compute_quiescent_fraction_grid_serial(all_log_halo, all_log_stellar, all_quiescent, 
                                                     halo_grid, stellar_grid, max_distance)
    
    comm = MPI.COMM_WORLD
    rank = comm.Get_rank()
    size = comm.Get_size()
    
    # Only use MPI if we have multiple processes and a reasonable number of grid cells
    total_cells = (len(halo_grid) - 1) * (len(stellar_grid) - 1)
    if size == 1 or total_cells < size * 100:  # At least 100 cells per process
        if rank == 0:
            logger.info('Grid too small for MPI, using serial processing')
            return compute_quiescent_fraction_grid_serial(all_log_halo, all_log_stellar, all_quiescent, 
                                                         halo_grid, stellar_grid, max_distance)
        else:
            return None
    
    if rank == 0:
        logger.info(f'Using MPI grid processing with {size} processes')
        logger.info(f'Total grid cells: {total_cells}, cells per process: ~{total_cells//size}')
    
    # Broadcast data to all ranks (everyone needs the full dataset)
    all_log_halo = comm.bcast(all_log_halo, root=0)
    all_log_stellar = comm.bcast(all_log_stellar, root=0)
    all_quiescent = comm.bcast(all_quiescent, root=0)
    halo_grid = comm.bcast(halo_grid, root=0)
    stellar_grid = comm.bcast(stellar_grid, root=0)
    max_distance = comm.bcast(max_distance, root=0)
    
    # Generate all grid cell indices
    cell_indices = []
    for i in range(len(halo_grid) - 1):
        for j in range(len(stellar_grid) - 1):
            cell_indices.append((i, j))
    
    # Distribute grid cells across ranks
    cells_per_rank = len(cell_indices) // size
    remainder = len(cell_indices) % size
    
    # Calculate start and end indices for this rank
    if rank < remainder:
        start_idx = rank * (cells_per_rank + 1)
        end_idx = start_idx + cells_per_rank + 1
    else:
        start_idx = rank * cells_per_rank + remainder
        end_idx = start_idx + cells_per_rank
    
    my_cells = cell_indices[start_idx:end_idx]
    
    if rank == 0:
        logger.info(f'Distributing {len(cell_indices)} cells across {size} ranks')
    
    # Process assigned cells
    local_results = []
    for cell_idx, (i, j) in enumerate(my_cells):
        if cell_idx % 1000 == 0 and len(my_cells) > 1000:
            logger.info(f'Rank {rank}: Processing cell {cell_idx}/{len(my_cells)} ({100*cell_idx/len(my_cells):.1f}%)')
        
        args = (i, j, halo_grid, stellar_grid, all_log_halo, all_log_stellar, 
                all_quiescent, max_distance)
        j_idx, i_idx, quiescent_frac = compute_grid_cell_quiescent_fraction(args)
        local_results.append((i_idx, j_idx, quiescent_frac))
    
    # Gather results to rank 0
    all_results = comm.gather(local_results, root=0)
    
    if rank == 0:
        # Combine results from all ranks
        quiescent_fraction_grid = np.full((len(stellar_grid)-1, len(halo_grid)-1), np.nan)
        
        total_processed = 0
        for rank_results in all_results:
            for i_idx, j_idx, quiescent_frac in rank_results:
                quiescent_fraction_grid[j_idx, i_idx] = quiescent_frac
                total_processed += 1
        
        logger.info(f'MPI grid computation complete: {total_processed} cells processed')
        return quiescent_fraction_grid
    
    return None


def compute_quiescent_fraction_grid_serial(all_log_halo, all_log_stellar, all_quiescent, 
                                          halo_grid, stellar_grid, max_distance=0.3):
    """
    Serial version of grid computation
    """
    logger.info('Computing grid using serial processing')
    
    quiescent_fraction_grid = np.full((len(stellar_grid)-1, len(halo_grid)-1), np.nan)
    total_cells = (len(halo_grid) - 1) * (len(stellar_grid) - 1)
    
    cell_count = 0
    for i in range(len(halo_grid) - 1):
        for j in range(len(stellar_grid) - 1):
            if cell_count % 5000 == 0:
                logger.info(f'Processing cell {cell_count}/{total_cells} ({100*cell_count/total_cells:.1f}%)')
            
            args = (i, j, halo_grid, stellar_grid, all_log_halo, all_log_stellar, 
                    all_quiescent, max_distance)
            j_idx, i_idx, quiescent_frac = compute_grid_cell_quiescent_fraction(args)
            quiescent_fraction_grid[j_idx, i_idx] = quiescent_frac
            
            cell_count += 1
    
    logger.info('Serial grid computation complete')
    return quiescent_fraction_grid


def compute_quiescent_fraction_grid(all_log_halo, all_log_stellar, all_quiescent, 
                                   halo_grid, stellar_grid, max_distance=0.3):
    """
    Smart dispatcher that chooses between MPI and serial processing
    """
    total_cells = (len(halo_grid) - 1) * (len(stellar_grid) - 1)
    
    # Use MPI if available and grid is large enough
    if HAS_MPI and total_cells > 10000:
        comm = MPI.COMM_WORLD
        size = comm.Get_size()
        if size > 1:
            return compute_quiescent_fraction_grid_mpi(all_log_halo, all_log_stellar, all_quiescent, 
                                                      halo_grid, stellar_grid, max_distance)
    
    # Fall back to serial processing
    return compute_quiescent_fraction_grid_serial(all_log_halo, all_log_stellar, all_quiescent, 
                                                 halo_grid, stellar_grid, max_distance)


def plot_stellar_halo_mass_relation(sim_configs, snapshot, output_dir):
    """Plot stellar-halo mass relation comparison with quiescent fraction overlay"""
    # Determine MPI status
    if HAS_MPI:
        comm = MPI.COMM_WORLD
        rank = comm.Get_rank()
        size = comm.Get_size()
    else:
        rank = 0
        size = 1
    
    if rank == 0:
        logger.info('=== Stellar-Halo Mass Relation ===')
    
    # ALL ranks collect data (needed for grid computation)
    all_log_halo = []
    all_log_stellar = []
    all_quiescent = []
    sim_data = []  # Store simulation data for plotting
    
    # Process each simulation (all ranks do this)
    for sim_config in sim_configs:
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        hubble_h = sim_config['Hubble_h']
        
        # Skip C16 simulation
        if 'C16' in label:
            continue
            
        if rank == 0:
            logger.info(f'Processing {label}...')
        
        try:
            # Read required galaxy properties (all ranks)
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            
            # Filter for central galaxies with reasonable masses
            w = np.where((StellarMass > 0) & (Mvir > 0))[0]
            
            if len(w) == 0:
                if rank == 0:
                    logger.warning(f'No suitable galaxies found for {label}')
                continue
            
            stellar_mass = StellarMass[w]
            halo_mass = Mvir[w]
            sfr_total = SfrDisk[w] + SfrBulge[w]
            
            if rank == 0:
                logger.info(f'  Found {len(w)} central galaxies with M* > 10^8 and Mvir > 10^10')
            
            # Convert to log10
            log_stellar = np.log10(stellar_mass)
            log_halo = np.log10(halo_mass)
            
            # Calculate specific star formation rate (sSFR)
            sSFR = np.log10(sfr_total / stellar_mass)
            
            # Determine quiescent galaxies (sSFR < -11.0)
            is_quiescent = (sSFR < sSFRcut).astype(float)
            
            # Collect ALL data for density plot (all ranks)
            all_log_halo.extend(log_halo)
            all_log_stellar.extend(log_stellar)
            all_quiescent.extend(is_quiescent)
            
            # Store simulation data for plotting (only rank 0 needs this)
            if rank == 0:
                sim_data.append({
                    'log_stellar': log_stellar,
                    'log_halo': log_halo,
                    'label': label,
                    'color': color
                })
            
        except Exception as e:
            if rank == 0:
                logger.error(f'Error processing {label}: {e}')
            continue
    
    # Create density plot overlay based on quiescent fraction (all ranks participate)
    quiescent_fraction_grid = None
    if len(all_log_halo) > 0:
        if rank == 0:
            logger.info('Creating quiescent fraction density overlay...')
        
        # Convert to numpy arrays (all ranks)
        all_log_halo = np.array(all_log_halo)
        all_log_stellar = np.array(all_log_stellar)
        all_quiescent = np.array(all_quiescent)
        
        # Get actual data range with generous padding
        halo_min = np.min(all_log_halo)
        halo_max = np.max(all_log_halo)
        stellar_min = np.min(all_log_stellar)
        stellar_max = np.max(all_log_stellar) 
        
        if rank == 0:
            logger.info(f'Data ranges: Halo [{halo_min:.2f}, {halo_max:.2f}], Stellar [{stellar_min:.2f}, {stellar_max:.2f}]')
        
        # Define ULTRA-FINE 2D grid for COMPLETE coverage of ALL data
        halo_grid = np.arange(halo_min, halo_max + 0.02, 0.02)
        stellar_grid = np.arange(stellar_min, stellar_max + 0.02, 0.02)

        if rank == 0:
            logger.info(f'Grid dimensions: {len(halo_grid)-1} x {len(stellar_grid)-1} = {(len(halo_grid)-1)*(len(stellar_grid)-1)} cells')
        
        # MPI grid computation (all ranks participate)
        max_distance = 0.2
        quiescent_fraction_grid = compute_quiescent_fraction_grid(
            all_log_halo, all_log_stellar, all_quiescent, 
            halo_grid, stellar_grid, max_distance)
    
    # Only rank 0 does the plotting
    if rank == 0 and output_dir is not None and quiescent_fraction_grid is not None:
        # Create figure
        fig, ax = create_figure(figsize=(10, 8))
        
        # Plot scatter data for each simulation
        for sim_data_item in sim_data:
            ax.scatter(sim_data_item['log_halo'], sim_data_item['log_stellar'], 
                      c=sim_data_item['color'], s=0.1, alpha=0.2, 
                      label=sim_data_item['label'], rasterized=True)
        
        # Plot the density overlay
        cmap = plt.cm.coolwarm
        extent = [halo_grid[0], halo_grid[-1], stellar_grid[0], stellar_grid[-1]]
        im = ax.imshow(quiescent_fraction_grid, extent=extent, origin='lower', 
                      cmap=cmap, vmin=0, vmax=1, alpha=1.0, aspect='auto', 
                      interpolation='bilinear', rasterized=True, zorder=5)
        
        # Add colorbar
        cbar = plt.colorbar(im, ax=ax, pad=0.02, aspect=30)
        cbar.set_label(r'Quiescent Fraction', fontsize=16, labelpad=15)
        cbar.ax.tick_params(labelsize=14)
        
        # Count valid grid cells
        valid_cells = np.sum(~np.isnan(quiescent_fraction_grid))
        total_cells = quiescent_fraction_grid.size
        logger.info(f'Grid coverage: {valid_cells}/{total_cells} cells ({100*valid_cells/total_cells:.1f}%)')
        logger.info(f'Grid resolution: {len(halo_grid)}x{len(stellar_grid)} = {total_cells} total cells')
        
        # Verify that all galaxies are covered by checking if any fall outside grid bounds
        galaxies_outside = np.sum((all_log_halo < halo_grid[0]) | (all_log_halo > halo_grid[-1]) |
                                 (all_log_stellar < stellar_grid[0]) | (all_log_stellar > stellar_grid[-1]))
        logger.info(f'Galaxies outside grid bounds: {galaxies_outside}/{len(all_log_halo)}')
        
        # Formatting
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$')
        ax.set_ylabel(r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$')
        ax.set_xlim(9.85, 15.0)
        ax.set_ylim(7.5, 12.5)
        
        # Legend
        ax.legend(loc='lower right', fontsize=14, frameon=False)
        
        # Save plot
        output_filename = output_dir + 'stellar_halo_mass_relation' + OutputFormat
        finalize_plot(fig, output_filename)

        plt.close(fig)  # Close the figure to free memory
        
        logger.info('Stellar-halo mass relation plot complete')


def plot_halo_stellar_mass_relation(sim_configs, snapshot, output_dir):
    """Plot halo-stellar mass relation (axes swapped from SHMR) with quiescent fraction overlay"""
    # Determine MPI status
    if HAS_MPI:
        comm = MPI.COMM_WORLD
        rank = comm.Get_rank()
        size = comm.Get_size()
    else:
        rank = 0
        size = 1
    
    if rank == 0:
        logger.info('=== Halo-Stellar Mass Relation (Swapped Axes) ===')
    
    # ALL ranks collect data (needed for grid computation)
    all_log_halo = []
    all_log_stellar = []
    all_quiescent = []
    sim_data = []  # Store simulation data for plotting
    
    # Process each simulation (all ranks do this)
    for sim_config in sim_configs:
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        hubble_h = sim_config['Hubble_h']
        
        # Skip C16 simulation
        if 'C16' in label:
            continue
            
        if rank == 0:
            logger.info(f'Processing {label}...')
        
        try:
            # Read required galaxy properties (all ranks)
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            
            # Filter for central galaxies with reasonable masses
            w = np.where((StellarMass > 0) & (Mvir > 0))[0]
            
            if len(w) == 0:
                if rank == 0:
                    logger.warning(f'No suitable galaxies found for {label}')
                continue
            
            stellar_mass = StellarMass[w]
            halo_mass = Mvir[w]
            sfr_total = SfrDisk[w] + SfrBulge[w]
            
            if rank == 0:
                logger.info(f'  Found {len(w)} central galaxies with M* > 10^8 and Mvir > 10^10')
                logger.info(f'  Using all {len(w)} galaxies for plotting (no sampling)')
            
            # Convert to log10
            log_stellar = np.log10(stellar_mass)
            log_halo = np.log10(halo_mass)
            
            # Calculate specific star formation rate (sSFR)
            sSFR = np.log10(sfr_total / stellar_mass)
            
            # Determine quiescent galaxies (sSFR < -11.0)
            is_quiescent = (sSFR < sSFRcut).astype(float)
            
            # Collect ALL data for density plot (all ranks)
            all_log_halo.extend(log_halo)
            all_log_stellar.extend(log_stellar)
            all_quiescent.extend(is_quiescent)
            
            # Store simulation data for plotting (only rank 0 needs this)
            if rank == 0:
                sim_data.append({
                    'log_stellar': log_stellar,
                    'log_halo': log_halo,
                    'label': label,
                    'color': color
                })
            
        except Exception as e:
            if rank == 0:
                logger.error(f'Error processing {label}: {e}')
            continue
    
    # Convert to numpy arrays for MPI grid computation
    if len(all_log_halo) > 0:
        all_log_halo = np.array(all_log_halo)
        all_log_stellar = np.array(all_log_stellar) 
        all_quiescent = np.array(all_quiescent)
        
        if rank == 0:
            logger.info('Creating quiescent fraction density overlay...')
            logger.info(f'Total data points: {len(all_log_halo)} galaxies from all simulations')
        
        # Define grid bounds
        halo_min = np.min(all_log_halo)
        halo_max = np.max(all_log_halo)
        stellar_min = np.min(all_log_stellar)
        stellar_max = np.max(all_log_stellar)
        
        if rank == 0:
            logger.info(f'Data ranges: Halo [{halo_min:.2f}, {halo_max:.2f}], Stellar [{stellar_min:.2f}, {stellar_max:.2f}]')
        
        # Define grid
        halo_grid = np.arange(halo_min, halo_max + 0.02, 0.02)
        stellar_grid = np.arange(stellar_min, stellar_max + 0.02, 0.02)
        
        if rank == 0:
            logger.info(f'Grid dimensions: {len(halo_grid)-1} x {len(stellar_grid)-1} = {(len(halo_grid)-1)*(len(stellar_grid)-1)} cells')
        
        # Compute quiescent fraction grid using MPI
        if HAS_MPI and size > 1:
            if rank == 0:
                logger.info(f'Using MPI grid processing with {size} processes')
            max_distance = 0.2
            quiescent_fraction_grid = compute_quiescent_fraction_grid_mpi(
                all_log_halo, all_log_stellar, all_quiescent,
                halo_grid, stellar_grid, max_distance)
        else:
            if rank == 0:
                logger.info('Using serial grid processing')
            max_distance = 0.2
            quiescent_fraction_grid = compute_quiescent_fraction_grid(
                all_log_halo, all_log_stellar, all_quiescent,
                halo_grid, stellar_grid, max_distance)
    
    # Only rank 0 does the plotting
    if rank == 0:
        if output_dir is None:
            return
        
        # Create figure
        fig, ax = create_figure(figsize=(10, 8))
        
        # Plot scatter data for each simulation
        for sim_data_item in sim_data:
            log_stellar = sim_data_item['log_stellar']
            log_halo = sim_data_item['log_halo']
            label = sim_data_item['label']
            color = sim_data_item['color']
            
            # Scatter plot with swapped axes: stellar mass on x, halo mass on y
            ax.scatter(log_stellar, log_halo, c=color, s=0.1, alpha=0.2,
                      label=label, rasterized=True)
        
        # Add density overlay if we have data
        if len(all_log_halo) > 0:
            # Create color map: coolwarm colormap for quiescent fraction
            cmap = plt.cm.coolwarm
            
            # Plot the grid as an image with swapped axes: stellar on x, halo on y
            extent = [stellar_grid[0], stellar_grid[-1], halo_grid[0], halo_grid[-1]]
            im = ax.imshow(quiescent_fraction_grid.T, extent=extent, origin='lower',
                          cmap=cmap, vmin=0, vmax=1, alpha=1.0, aspect='auto',
                          interpolation='bilinear', rasterized=True, zorder=5)
            
            # Add colorbar
            cbar = plt.colorbar(im, ax=ax, pad=0.02, aspect=30)
            cbar.set_label(r'Quiescent Fraction', fontsize=16, labelpad=15)
            cbar.ax.tick_params(labelsize=14)
            
            # Log grid statistics
            valid_cells = np.sum(~np.isnan(quiescent_fraction_grid))
            total_cells = quiescent_fraction_grid.size
            logger.info(f'Grid coverage: {valid_cells}/{total_cells} cells ({100*valid_cells/total_cells:.1f}%)')
        
        # Formatting with swapped axis labels
        ax.set_xlabel(r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$')
        ax.set_xlim(7.5, 12.5)   # stellar mass range
        ax.set_ylim(9.85, 15.0)  # halo mass range
        
        # Legend
        ax.legend(loc='lower right', fontsize=14, frameon=False)
        
        # Save plot
        output_filename = output_dir + 'halo_stellar_mass_relation' + OutputFormat
        finalize_plot(fig, output_filename)
        
        plt.close(fig)  # Close the figure to free memory
        
        logger.info('Halo-stellar mass relation plot (swapped axes) complete')


def plot_halo_stellar_mass_relation_centrals(sim_configs, snapshot, output_dir):
    """Plot halo-stellar mass relation for CENTRAL galaxies only (Type == 0) with quiescent fraction overlay"""
    # Determine MPI status
    if HAS_MPI:
        comm = MPI.COMM_WORLD
        rank = comm.Get_rank()
        size = comm.Get_size()
    else:
        rank = 0
        size = 1
    
    if rank == 0:
        logger.info('=== Halo-Stellar Mass Relation - CENTRALS ONLY (Type == 0) ===')
    
    # ALL ranks collect data (needed for grid computation)
    all_log_halo = []
    all_log_stellar = []
    all_quiescent = []
    sim_data = []  # Store simulation data for plotting
    
    # Process each simulation (all ranks do this)
    for sim_config in sim_configs:
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        hubble_h = sim_config['Hubble_h']
        
        # Skip C16 simulation
        if 'C16' in label:
            continue
            
        if rank == 0:
            logger.info(f'Processing {label}...')
        
        try:
            # Read required galaxy properties (all ranks)
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            
            # Filter for CENTRAL galaxies ONLY (Type == 0) with reasonable masses
            w = np.where((StellarMass > 0) & (Mvir > 0) & (Type == 0))[0]
            
            if len(w) == 0:
                if rank == 0:
                    logger.warning(f'No suitable central galaxies found for {label}')
                continue
            
            stellar_mass = StellarMass[w]
            halo_mass = Mvir[w]
            sfr_total = SfrDisk[w] + SfrBulge[w]
            
            if rank == 0:
                logger.info(f'  Found {len(w)} central galaxies (Type==0) with M* > 0 and Mvir > 0')
                logger.info(f'  Using all {len(w)} central galaxies for plotting (no sampling)')
            
            # Convert to log10
            log_stellar = np.log10(stellar_mass)
            log_halo = np.log10(halo_mass)
            
            # Calculate specific star formation rate (sSFR)
            sSFR = np.log10(sfr_total / stellar_mass)
            
            # Determine quiescent galaxies (sSFR < -11.0)
            is_quiescent = (sSFR < sSFRcut).astype(float)
            
            # Collect ALL data for density plot (all ranks)
            all_log_halo.extend(log_halo)
            all_log_stellar.extend(log_stellar)
            all_quiescent.extend(is_quiescent)
            
            # Store simulation data for plotting (only rank 0 needs this)
            if rank == 0:
                sim_data.append({
                    'log_stellar': log_stellar,
                    'log_halo': log_halo,
                    'label': label,
                    'color': color
                })
            
        except Exception as e:
            if rank == 0:
                logger.error(f'Error processing {label}: {e}')
            continue
    
    # Convert to numpy arrays for MPI grid computation
    if len(all_log_halo) > 0:
        all_log_halo = np.array(all_log_halo)
        all_log_stellar = np.array(all_log_stellar) 
        all_quiescent = np.array(all_quiescent)
        
        if rank == 0:
            logger.info('Creating quiescent fraction density overlay for centrals...')
            logger.info(f'Total data points: {len(all_log_halo)} central galaxies from all simulations')
        
        # Define grid bounds
        halo_min = np.min(all_log_halo)
        halo_max = np.max(all_log_halo)
        stellar_min = np.min(all_log_stellar)
        stellar_max = np.max(all_log_stellar)
        
        if rank == 0:
            logger.info(f'Data ranges: Halo [{halo_min:.2f}, {halo_max:.2f}], Stellar [{stellar_min:.2f}, {stellar_max:.2f}]')
        
        # Define grid (finer for centrals only)
        halo_grid = np.arange(halo_min, halo_max + 0.02, 0.02)
        stellar_grid = np.arange(stellar_min, stellar_max + 0.02, 0.02)
        
        if rank == 0:
            logger.info(f'Grid dimensions: {len(halo_grid)-1} x {len(stellar_grid)-1} = {(len(halo_grid)-1)*(len(stellar_grid)-1)} cells')
        
        # Compute quiescent fraction grid using MPI
        if HAS_MPI and size > 1:
            if rank == 0:
                logger.info(f'Using MPI grid processing with {size} processes')
            max_distance = 0.2
            quiescent_fraction_grid = compute_quiescent_fraction_grid_mpi(
                all_log_halo, all_log_stellar, all_quiescent,
                halo_grid, stellar_grid, max_distance)
        else:
            if rank == 0:
                logger.info('Using serial grid processing')
            max_distance = 0.2
            quiescent_fraction_grid = compute_quiescent_fraction_grid(
                all_log_halo, all_log_stellar, all_quiescent,
                halo_grid, stellar_grid, max_distance)
    
    # Only rank 0 does the plotting
    if rank == 0:
        if output_dir is None:
            return
        
        # Create figure
        fig, ax = create_figure(figsize=(10, 8))
        
        # Plot scatter data for each simulation
        for sim_data_item in sim_data:
            log_stellar = sim_data_item['log_stellar']
            log_halo = sim_data_item['log_halo']
            label = sim_data_item['label']
            color = sim_data_item['color']
            
            # Scatter plot with swapped axes: stellar mass on x, halo mass on y
            ax.scatter(log_stellar, log_halo, c=color, s=0.1, alpha=0.2,
                      label=label, rasterized=True)
        
        # Add density overlay if we have data
        if len(all_log_halo) > 0:
            # Create color map: coolwarm colormap for quiescent fraction
            cmap = plt.cm.coolwarm
            
            # Plot the grid as an image with swapped axes: stellar on x, halo on y
            extent = [stellar_grid[0], stellar_grid[-1], halo_grid[0], halo_grid[-1]]
            im = ax.imshow(quiescent_fraction_grid.T, extent=extent, origin='lower',
                          cmap=cmap, vmin=0, vmax=1, alpha=1.0, aspect='auto',
                          interpolation='bilinear', rasterized=True, zorder=5)
            
            # Add colorbar
            cbar = plt.colorbar(im, ax=ax, pad=0.02, aspect=30)
            cbar.set_label(r'Quiescent Fraction', fontsize=16, labelpad=15)
            cbar.ax.tick_params(labelsize=14)
            
            # Log grid statistics
            valid_cells = np.sum(~np.isnan(quiescent_fraction_grid))
            total_cells = quiescent_fraction_grid.size
            logger.info(f'Grid coverage: {valid_cells}/{total_cells} cells ({100*valid_cells/total_cells:.1f}%)')
        
        # Formatting with swapped axis labels
        ax.set_xlabel(r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$')
        ax.set_xlim(7.5, 12.5)   # stellar mass range
        ax.set_ylim(9.85, 15.0)  # halo mass range
        
        # Title to indicate centrals only
        ax.set_title(r'Central Galaxies Only (Type = 0)', fontsize=18, pad=20)
        
        # Legend
        ax.legend(loc='lower right', fontsize=14, frameon=False)
        
        # Save plot
        output_filename = output_dir + 'halo_stellar_mass_relation_centrals' + OutputFormat
        finalize_plot(fig, output_filename)
        
        plt.close(fig)  # Close the figure to free memory
        
        logger.info('Halo-stellar mass relation plot for CENTRALS complete')


def plot_halo_stellar_mass_relation_satellites(sim_configs, snapshot, output_dir):
    """Plot halo-stellar mass relation for SATELLITE galaxies only (Type == 1) with quiescent fraction overlay"""
    # Determine MPI status
    if HAS_MPI:
        comm = MPI.COMM_WORLD
        rank = comm.Get_rank()
        size = comm.Get_size()
    else:
        rank = 0
        size = 1
    
    if rank == 0:
        logger.info('=== Halo-Stellar Mass Relation - SATELLITES ONLY (Type == 1) ===')
    
    # ALL ranks collect data (needed for grid computation)
    all_log_halo = []
    all_log_stellar = []
    all_quiescent = []
    sim_data = []  # Store simulation data for plotting
    
    # Process each simulation (all ranks do this)
    for sim_config in sim_configs:
        directory = sim_config['path']
        label = sim_config['label']
        color = sim_config['color']
        hubble_h = sim_config['Hubble_h']
        
        # Skip C16 simulation
        if 'C16' in label:
            continue
            
        if rank == 0:
            logger.info(f'Processing {label}...')
        
        try:
            # Read required galaxy properties (all ranks)
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
            Type = read_hdf_ultra_optimized(snap_num=snapshot, param='Type', directory=directory)
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            
            # Filter for SATELLITE galaxies ONLY (Type == 1) with reasonable masses
            w = np.where((StellarMass > 0) & (Type == 1))[0]
            
            if len(w) == 0:
                if rank == 0:
                    logger.warning(f'No suitable satellite galaxies found for {label}')
                continue
            
            stellar_mass = StellarMass[w]
            halo_mass = Mvir[w]
            sfr_total = SfrDisk[w] + SfrBulge[w]
            
            if rank == 0:
                logger.info(f'  Found {len(w)} satellite galaxies (Type==1) with M* > 0')
                logger.info(f'  Using all {len(w)} satellite galaxies for plotting (no sampling)')
            
            # Convert to log10
            log_stellar = np.log10(stellar_mass)
            log_halo = np.log10(halo_mass)
            
            # Calculate specific star formation rate (sSFR)
            sSFR = np.log10(sfr_total / stellar_mass)
            
            # Determine quiescent galaxies (sSFR < -11.0)
            is_quiescent = (sSFR < sSFRcut).astype(float)
            
            # Collect ALL data for density plot (all ranks)
            all_log_halo.extend(log_halo)
            all_log_stellar.extend(log_stellar)
            all_quiescent.extend(is_quiescent)
            
            # Store simulation data for plotting (only rank 0 needs this)
            if rank == 0:
                sim_data.append({
                    'log_stellar': log_stellar,
                    'log_halo': log_halo,
                    'label': label,
                    'color': color
                })
            
        except Exception as e:
            if rank == 0:
                logger.error(f'Error processing {label}: {e}')
            continue
    
    # Convert to numpy arrays for MPI grid computation
    if len(all_log_halo) > 0:
        all_log_halo = np.array(all_log_halo)
        all_log_stellar = np.array(all_log_stellar) 
        all_quiescent = np.array(all_quiescent)
        
        if rank == 0:
            logger.info('Creating quiescent fraction density overlay for satellites...')
            logger.info(f'Total data points: {len(all_log_halo)} satellite galaxies from all simulations')
        
        # Define grid bounds
        halo_min = np.min(all_log_halo)
        halo_max = np.max(all_log_halo)
        stellar_min = np.min(all_log_stellar)
        stellar_max = np.max(all_log_stellar)
        
        if rank == 0:
            logger.info(f'Data ranges: Halo [{halo_min:.2f}, {halo_max:.2f}], Stellar [{stellar_min:.2f}, {stellar_max:.2f}]')
        
        # Define grid (finer for satellites only)
        halo_grid = np.arange(halo_min, halo_max + 0.02, 0.02)
        stellar_grid = np.arange(stellar_min, stellar_max + 0.02, 0.02)
        
        if rank == 0:
            logger.info(f'Grid dimensions: {len(halo_grid)-1} x {len(stellar_grid)-1} = {(len(halo_grid)-1)*(len(stellar_grid)-1)} cells')
        
        # Compute quiescent fraction grid using MPI
        if HAS_MPI and size > 1:
            if rank == 0:
                logger.info(f'Using MPI grid processing with {size} processes')
            max_distance = 0.2
            quiescent_fraction_grid = compute_quiescent_fraction_grid_mpi(
                all_log_halo, all_log_stellar, all_quiescent,
                halo_grid, stellar_grid, max_distance)
        else:
            if rank == 0:
                logger.info('Using serial grid processing')
            max_distance = 0.2
            quiescent_fraction_grid = compute_quiescent_fraction_grid(
                all_log_halo, all_log_stellar, all_quiescent,
                halo_grid, stellar_grid, max_distance)
    
    # Only rank 0 does the plotting
    if rank == 0:
        if output_dir is None:
            return
        
        # Create figure
        fig, ax = create_figure(figsize=(10, 8))
        
        # Plot scatter data for each simulation
        for sim_data_item in sim_data:
            log_stellar = sim_data_item['log_stellar']
            log_halo = sim_data_item['log_halo']
            label = sim_data_item['label']
            color = sim_data_item['color']
            
            # Scatter plot with swapped axes: stellar mass on x, halo mass on y
            ax.scatter(log_stellar, log_halo, c=color, s=0.1, alpha=0.2,
                      label=label, rasterized=True)
        
        # Add density overlay if we have data
        if len(all_log_halo) > 0:
            # Create color map: coolwarm colormap for quiescent fraction
            cmap = plt.cm.coolwarm
            
            # Plot the grid as an image with swapped axes: stellar on x, halo on y
            extent = [stellar_grid[0], stellar_grid[-1], halo_grid[0], halo_grid[-1]]
            im = ax.imshow(quiescent_fraction_grid.T, extent=extent, origin='lower',
                          cmap=cmap, vmin=0, vmax=1, alpha=1.0, aspect='auto',
                          interpolation='bilinear', rasterized=True, zorder=5)
            
            # Add colorbar
            cbar = plt.colorbar(im, ax=ax, pad=0.02, aspect=30)
            cbar.set_label(r'Quiescent Fraction', fontsize=16, labelpad=15)
            cbar.ax.tick_params(labelsize=14)
            
            # Log grid statistics
            valid_cells = np.sum(~np.isnan(quiescent_fraction_grid))
            total_cells = quiescent_fraction_grid.size
            logger.info(f'Grid coverage: {valid_cells}/{total_cells} cells ({100*valid_cells/total_cells:.1f}%)')
        
        # Formatting with swapped axis labels
        ax.set_xlabel(r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$')
        ax.set_ylabel(r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$')
        ax.set_xlim(7.5, 12.5)   # stellar mass range  
        ax.set_ylim(9.85, 15.0)  # halo mass range
        
        # Title to indicate satellites only
        ax.set_title(r'Satellite Galaxies Only (Type = 1)', fontsize=18, pad=20)
        
        # Legend
        ax.legend(loc='lower right', fontsize=14, frameon=False)
        
        # Save plot
        output_filename = output_dir + 'halo_stellar_mass_relation_satellites' + OutputFormat
        finalize_plot(fig, output_filename)
        
        plt.close(fig)  # Close the figure to free memory
        
        logger.info('Halo-stellar mass relation plot for SATELLITES complete')


# ========================== GALAXY EVOLUTION TRACKING ==========================

def identify_target_galaxies(directory, snapshot, stellar_mass_min=11.0, mvir_min=14.0, 
                           quiescent_fraction_max=0.4, hubble_h=0.73):
    """
    Identify galaxies with high stellar and virial masses that are actively star-forming
    and located in environments with low quiescent fractions.
    
    This function finds galaxies that meet the following criteria:
    1. log10(StellarMass) > stellar_mass_min
    2. log10(Mvir) > mvir_min  
    3. sSFR > -11.0 (star-forming, not quiescent)
    4. Local quiescent fraction <= quiescent_fraction_max
    
    The local quiescent fraction is calculated within a 0.3 dex radius in 
    log(M*)-log(Mvir) space around each galaxy.
    
    Parameters:
    -----------
    directory : str
        Directory containing HDF5 files
    snapshot : str
        Snapshot name (e.g., 'Snap_63')
    stellar_mass_min : float
        Minimum log10 stellar mass
    mvir_min : float
        Minimum log10 virial mass
    quiescent_fraction_max : float
        Maximum local quiescent fraction
    hubble_h : float
        Hubble parameter
        
    Returns:
    --------
    list : List of GalaxyIndex values for target galaxies
    """
    logger.info(f'Identifying target galaxies in {snapshot}...')
    
    # Read required properties
    StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
    Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
    SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
    SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
    GalaxyIndex = read_hdf_ultra_optimized(snap_num=snapshot, param='GalaxyIndex', directory=directory)
    
    # Calculate sSFR and identify quiescent galaxies
    total_sfr = SfrDisk + SfrBulge
    sSFR = np.log10(total_sfr / StellarMass)
    
    # Apply mass cuts
    mass_cut = (np.log10(StellarMass) > stellar_mass_min) & (np.log10(Mvir) > mvir_min)
    
    # Identify quiescent galaxies using global sSFR cut
    is_quiescent = sSFR < sSFRcut  # Using global sSFRcut = -11.0
    
    # Calculate log masses for local environment analysis
    log_stellar_mass = np.log10(StellarMass)
    log_mvir = np.log10(Mvir)
    
    # Find galaxies that meet mass criteria and are star-forming (individual criterion)
    star_forming_massive = mass_cut & ~is_quiescent
    
    # Now filter by local quiescent fraction
    target_mask = np.zeros(len(StellarMass), dtype=bool)
    
    # For each star-forming massive galaxy, calculate local quiescent fraction
    sf_massive_indices = np.where(star_forming_massive)[0]
    logger.info(f'Checking local quiescent fraction for {len(sf_massive_indices)} star-forming massive galaxies...')
    
    for idx in sf_massive_indices:
        # Define local environment (0.3 dex radius in log(M*)-log(Mvir) space)
        local_radius = 0.3
        gal_log_stellar = log_stellar_mass[idx]
        gal_log_mvir = log_mvir[idx]
        
        # Find neighbors within the local radius
        distances = np.sqrt((log_stellar_mass - gal_log_stellar)**2 + (log_mvir - gal_log_mvir)**2)
        local_mask = distances <= local_radius
        
        # Calculate local quiescent fraction if we have enough neighbors
        if np.sum(local_mask) >= 5:  # Need at least 5 galaxies for meaningful statistics
            local_quiescent_fraction = np.mean(is_quiescent[local_mask])
        else:
            # Use nearest neighbors if local density is too low
            nearest_indices = np.argsort(distances)[:min(10, len(distances))]
            local_quiescent_fraction = np.mean(is_quiescent[nearest_indices])
        
        # Include galaxy if local quiescent fraction is below threshold
        if local_quiescent_fraction <= quiescent_fraction_max:
            target_mask[idx] = True
    
    target_indices = np.where(target_mask)[0]
    
    logger.info(f'Found {len(target_indices)} target galaxies out of {len(StellarMass)} total')
    logger.info(f'Mass cuts: log10(M*) > {stellar_mass_min}, log10(Mvir) > {mvir_min}')
    logger.info(f'Star-forming criteria: sSFR > {sSFRcut}')
    logger.info(f'Local quiescent fraction criteria: f_quiescent <= {quiescent_fraction_max}')
    
    if len(target_indices) > 0:
        target_galaxy_ids = GalaxyIndex[target_indices]
        
        # Calculate local quiescent fractions for logging
        local_quiescent_fractions = []
        for idx in target_indices:
            gal_log_stellar = log_stellar_mass[idx]
            gal_log_mvir = log_mvir[idx]
            distances = np.sqrt((log_stellar_mass - gal_log_stellar)**2 + (log_mvir - gal_log_mvir)**2)
            local_mask = distances <= 0.3
            if np.sum(local_mask) >= 5:
                local_qf = np.mean(is_quiescent[local_mask])
            else:
                nearest_indices = np.argsort(distances)[:min(10, len(distances))]
                local_qf = np.mean(is_quiescent[nearest_indices])
            local_quiescent_fractions.append(local_qf)
        
        local_quiescent_fractions = np.array(local_quiescent_fractions)
        
        # Log some statistics
        logger.info(f'Sample properties:')
        logger.info(f'  Stellar mass range: {np.log10(StellarMass[target_indices]).min():.2f} - {np.log10(StellarMass[target_indices]).max():.2f}')
        logger.info(f'  Mvir range: {np.log10(Mvir[target_indices]).min():.2f} - {np.log10(Mvir[target_indices]).max():.2f}')
        logger.info(f'  sSFR range: {sSFR[target_indices].min():.2f} - {sSFR[target_indices].max():.2f}')
        logger.info(f'  Local quiescent fraction range: {local_quiescent_fractions.min():.3f} - {local_quiescent_fractions.max():.3f}')
        logger.info(f'  Mean local quiescent fraction: {local_quiescent_fractions.mean():.3f} ± {local_quiescent_fractions.std():.3f}')
        
        return target_galaxy_ids.tolist()
    else:
        logger.warning('No target galaxies found!')
        return []

def track_galaxy_evolution(directory, galaxy_ids, hubble_h=0.73, max_galaxies=10):
    """
    Track the evolution of specified galaxies across all available snapshots.
    
    Parameters:
    -----------
    directory : str
        Directory containing HDF5 files
    galaxy_ids : list
        List of GalaxyIndex values to track
    hubble_h : float
        Hubble parameter
    max_galaxies : int
        Maximum number of galaxies to track (for performance)
        
    Returns:
    --------
    dict : Dictionary containing tracked data for each galaxy
    """
    if len(galaxy_ids) > max_galaxies:
        logger.info(f'Limiting to {max_galaxies} galaxies for performance')
        galaxy_ids = galaxy_ids[:max_galaxies]
    
    logger.info(f'Tracking evolution of {len(galaxy_ids)} galaxies across all snapshots...')
    
    # Initialize storage
    tracked_data = {}
    for gal_id in galaxy_ids:
        tracked_data[gal_id] = {
            'snapnum': [],
            'redshift': [],
            'log10_mvir': [],
            'log10_stellar_mass': [],
            'sfr': [],
            'ssfr': [],
            'log10_cold_gas': [],
            'h2_fraction': [],
            'found_in_snap': [],
            'time_last_major_merger': [],
            'time_last_minor_merger': [],
            'has_recent_major_merger': [],
            'has_recent_minor_merger': [],
            'merger_status': [],
            'time_since_last_merger': []
        }
    
    # Get all available snapshots
    test_files = get_file_list(directory)
    if not test_files:
        logger.error(f'No model files found in {directory}')
        return tracked_data
    
    # Check what snapshots are available
    with h5.File(os.path.join(directory, test_files[0]), 'r') as f:
        available_snaps = [key for key in f.keys() if key.startswith('Snap_')]
        snap_nums = sorted([int(snap.split('_')[1]) for snap in available_snaps])
    
    logger.info(f'Found {len(snap_nums)} snapshots: {snap_nums[0]} to {snap_nums[-1]}')
    
    # Track across all snapshots
    for snap_num in snap_nums:
        snapshot = f'Snap_{snap_num}'
        logger.info(f'Processing {snapshot}...')
        
        try:
            # Read all galaxy data for this snapshot
            GalaxyIndex = read_hdf_ultra_optimized(snap_num=snapshot, param='GalaxyIndex', directory=directory)
            StellarMass = read_hdf_ultra_optimized(snap_num=snapshot, param='StellarMass', directory=directory) * 1.0e10 / hubble_h
            Mvir = read_hdf_ultra_optimized(snap_num=snapshot, param='Mvir', directory=directory) * 1.0e10 / hubble_h
            SfrDisk = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrDisk', directory=directory)
            SfrBulge = read_hdf_ultra_optimized(snap_num=snapshot, param='SfrBulge', directory=directory)
            ColdGas = read_hdf_ultra_optimized(snap_num=snapshot, param='ColdGas', directory=directory) * 1.0e10 / hubble_h
            H1_gas = read_hdf_ultra_optimized(snap_num=snapshot, param='HI_gas', directory=directory) * 1.0e10 / hubble_h
            H2_gas = read_hdf_ultra_optimized(snap_num=snapshot, param='H2_gas', directory=directory) * 1.0e10 / hubble_h
            
            # Read merger timing information
            TimeOfLastMajorMerger = read_hdf_ultra_optimized(snap_num=snapshot, param='TimeOfLastMajorMerger', directory=directory)
            TimeOfLastMinorMerger = read_hdf_ultra_optimized(snap_num=snapshot, param='TimeOfLastMinorMerger', directory=directory)
            
            # Calculate derived quantities
            total_sfr = SfrDisk + SfrBulge
            redshift = redshifts[snap_num]  # Using global redshift array
            
            # Define merger recency threshold (1 Gyr = 1000 Myr)
            # Note: In SAGE, times are typically in Myr from the Big Bang
            merger_recency_threshold = 1000.0  # Myr
            
            # Find each target galaxy in this snapshot
            for gal_id in galaxy_ids:
                indices = np.where(GalaxyIndex == gal_id)[0]
                
                if len(indices) > 0:
                    idx = indices[0]  # Should only be one match
                    
                    # Extract properties
                    stellar_mass = StellarMass[idx]
                    mvir = Mvir[idx]
                    sfr = total_sfr[idx]
                    cold_gas = ColdGas[idx]
                    h1 = H1_gas[idx]
                    h2 = H2_gas[idx]
                    time_last_major = TimeOfLastMajorMerger[idx]
                    time_last_minor = TimeOfLastMinorMerger[idx]
                    
                    # Calculate derived quantities
                    log10_stellar_mass = np.log10(stellar_mass) if stellar_mass > 0 else np.nan
                    log10_mvir = np.log10(mvir) if mvir > 0 else np.nan
                    ssfr = np.log10(sfr / stellar_mass) if stellar_mass > 0 and sfr > 0 else np.nan
                    log10_cold_gas = np.log10(cold_gas) if cold_gas > 0 else np.nan
                    h2_fraction = h2 / (h1 + h2) if (h1 + h2) > 0 else np.nan
                    
                    # Analyze merger history
                    # Check for recent mergers (within 1 Gyr)
                    # Note: We use the difference between merger times to identify recent events
                    has_recent_major = False
                    has_recent_minor = False
                    merger_status = 'none'
                    time_since_last_merger = np.inf
                    
                    # Find the most recent merger
                    last_major_time = time_last_major if time_last_major > 0 else -np.inf
                    last_minor_time = time_last_minor if time_last_minor > 0 else -np.inf
                    
                    if last_major_time > -np.inf or last_minor_time > -np.inf:
                        most_recent_merger_time = max(last_major_time, last_minor_time)
                        
                        # For this snapshot, check if there was a recent merger
                        # We'll determine this by comparing with previous snapshots later
                        if last_major_time >= last_minor_time and last_major_time > 0:
                            merger_status = 'major'
                            time_since_last_merger = 0 if last_major_time == most_recent_merger_time else np.inf
                        elif last_minor_time > last_major_time and last_minor_time > 0:
                            merger_status = 'minor'
                            time_since_last_merger = 0 if last_minor_time == most_recent_merger_time else np.inf
                    
                    # Store data
                    tracked_data[gal_id]['snapnum'].append(snap_num)
                    tracked_data[gal_id]['redshift'].append(redshift)
                    tracked_data[gal_id]['log10_mvir'].append(log10_mvir)
                    tracked_data[gal_id]['log10_stellar_mass'].append(log10_stellar_mass)
                    tracked_data[gal_id]['sfr'].append(sfr)
                    tracked_data[gal_id]['ssfr'].append(ssfr)
                    tracked_data[gal_id]['log10_cold_gas'].append(log10_cold_gas)
                    tracked_data[gal_id]['h2_fraction'].append(h2_fraction)
                    tracked_data[gal_id]['found_in_snap'].append(True)
                    tracked_data[gal_id]['time_last_major_merger'].append(time_last_major)
                    tracked_data[gal_id]['time_last_minor_merger'].append(time_last_minor)
                    tracked_data[gal_id]['has_recent_major_merger'].append(has_recent_major)
                    tracked_data[gal_id]['has_recent_minor_merger'].append(has_recent_minor)
                    tracked_data[gal_id]['merger_status'].append(merger_status)
                    tracked_data[gal_id]['time_since_last_merger'].append(time_since_last_merger)
                    
        except Exception as e:
            logger.warning(f'Error processing {snapshot}: {e}')
            continue
    
    # Log tracking statistics
    for gal_id in galaxy_ids:
        n_found = len(tracked_data[gal_id]['snapnum'])
        logger.info(f'Galaxy {gal_id}: found in {n_found}/{len(snap_nums)} snapshots')
    
    # Post-process merger detection: identify when mergers actually occurred
    logger.info('Post-processing merger detection...')
    for gal_id in galaxy_ids:
        gal_data = tracked_data[gal_id]
        if len(gal_data['snapnum']) < 2:
            continue
            
        # Convert to numpy arrays for easier processing
        major_times = np.array(gal_data['time_last_major_merger'])
        minor_times = np.array(gal_data['time_last_minor_merger'])
        redshifts_arr = np.array(gal_data['redshift'])
        
        # Reset merger status arrays
        gal_data['has_recent_major_merger'] = [False] * len(gal_data['snapnum'])
        gal_data['has_recent_minor_merger'] = [False] * len(gal_data['snapnum'])
        gal_data['merger_status'] = ['none'] * len(gal_data['snapnum'])
        
        # Look for changes in merger times between snapshots
        for i in range(1, len(major_times)):
            # Check for new major merger
            if major_times[i] > major_times[i-1] and major_times[i] > 0:
                gal_data['has_recent_major_merger'][i] = True
                gal_data['merger_status'][i] = 'major'
                logger.info(f'Galaxy {gal_id}: Major merger detected at snapshot {gal_data["snapnum"][i]} (z={redshifts_arr[i]:.2f})')
            
            # Check for new minor merger
            if minor_times[i] > minor_times[i-1] and minor_times[i] > 0:
                gal_data['has_recent_minor_merger'][i] = True
                # Only mark as minor if no major merger in same snapshot
                if gal_data['merger_status'][i] == 'none':
                    gal_data['merger_status'][i] = 'minor'
                logger.info(f'Galaxy {gal_id}: Minor merger detected at snapshot {gal_data["snapnum"][i]} (z={redshifts_arr[i]:.2f})')
    
    return tracked_data

def plot_galaxy_evolution_6panel(tracked_data, output_dir, hubble_h=0.73):
    """
    Create a 6-panel figure showing galaxy evolution:
    1. Mvir vs Redshift
    2. StellarMass vs Redshift  
    3. SFR vs Redshift
    4. sSFR vs Redshift
    5. ColdGas vs Redshift
    6. H2 fraction vs Redshift
    """
    logger.info('Creating 6-panel galaxy evolution plot...')
    
    # Setup figure
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    # Panel titles and labels
    panel_configs = [
        {'title': r'Virial Mass Evolution', 'ylabel': r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_mvir'},
        {'title': r'Stellar Mass Evolution', 'ylabel': r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$', 'data_key': 'log10_stellar_mass'},
        {'title': r'Star Formation Rate', 'ylabel': r'$\mathrm{SFR}\ (\mathrm{M}_\odot\ \mathrm{yr}^{-1})$', 'data_key': 'sfr'},
        {'title': r'Specific Star Formation Rate', 'ylabel': r'$\log_{10} \mathrm{sSFR}\ (\mathrm{yr}^{-1})$', 'data_key': 'ssfr'},
        {'title': r'Cold Gas Mass', 'ylabel': r'$\log_{10} M_{\mathrm{cold}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_cold_gas'},
        {'title': r'H$_2$ Fraction', 'ylabel': r'$f_{\mathrm{H_2}} = M_{\mathrm{H_2}} / (M_{\mathrm{H_2}} + M_{\mathrm{HI}})$', 'data_key': 'h2_fraction'}
    ]
    
    # Color palette for different galaxies
    colors = plt.cm.plasma(np.linspace(0, 1, len(tracked_data)))
    
    # Find the most massive halo at z=0 (or latest available redshift)
    most_massive_galaxy_id = None
    max_mvir = -np.inf
    
    for gal_id, gal_data in tracked_data.items():
        if len(gal_data['log10_mvir']) > 0:
            # Use the latest (lowest redshift) Mvir value
            latest_mvir = gal_data['log10_mvir'][-1]  # Assuming data is sorted by redshift
            if latest_mvir > max_mvir:
                max_mvir = latest_mvir
                most_massive_galaxy_id = gal_id
    
    logger.info(f'Most massive galaxy: {most_massive_galaxy_id} with log10(Mvir) = {max_mvir:.2f}')
    
    # Track if merger labels have been added
    major_merger_labeled = False
    minor_merger_labeled = False
    
    # Plot each galaxy's evolution
    for i, (gal_id, gal_data) in enumerate(tracked_data.items()):
        if len(gal_data['redshift']) == 0:
            continue
            
        color = colors[i % len(colors)]
        redshifts = np.array(gal_data['redshift'])
        merger_status = np.array(gal_data['merger_status'])
        has_major = np.array(gal_data['has_recent_major_merger'])
        has_minor = np.array(gal_data['has_recent_minor_merger'])
        
        # Set alpha based on whether this is the most massive galaxy
        alpha = 1.0 if gal_id == most_massive_galaxy_id else 0.1
        linewidth = 3.0 if gal_id == most_massive_galaxy_id else 1.0
        
        # Sort by redshift for clean lines
        sort_idx = np.argsort(redshifts)
        redshifts_sorted = redshifts[sort_idx]
        merger_status_sorted = merger_status[sort_idx]
        has_major_sorted = has_major[sort_idx]
        has_minor_sorted = has_minor[sort_idx]
        
        for j, config in enumerate(panel_configs):
            ax = axes[j]
            data_key = config['data_key']
            
            if data_key in gal_data and len(gal_data[data_key]) > 0:
                y_data = np.array(gal_data[data_key])[sort_idx]
                
                # Handle special case for SFR (not in log)
                if data_key == 'sfr':
                    y_data = np.log10(y_data)
                    y_data[~np.isfinite(y_data)] = np.nan
                
                # Plot line with markers
                valid_mask = np.isfinite(y_data)
                if np.any(valid_mask):
                    # Plot the main evolution line (no label for individual galaxies)
                    ax.plot(redshifts_sorted[valid_mask], y_data[valid_mask], color=color, alpha=alpha, linewidth=linewidth)
                    
                    # Add merger markers (only add labels once for legend)
                    # Major mergers: red stars
                    major_mask = valid_mask & has_major_sorted
                    if np.any(major_mask):
                        ax.scatter(redshifts_sorted[major_mask], y_data[major_mask], 
                                 marker='*', s=100, c='red', edgecolors='darkred', linewidth=linewidth, alpha=alpha,
                                 label='Major Merger' if j == 0 and not major_merger_labeled else "", zorder=10)
                        if j == 0:
                            major_merger_labeled = True
                    
                    # Minor mergers: orange triangles
                    minor_mask = valid_mask & has_minor_sorted
                    if np.any(minor_mask):
                        ax.scatter(redshifts_sorted[minor_mask], y_data[minor_mask], 
                                 marker='^', s=60, c='orange', edgecolors='darkorange', linewidth=linewidth, alpha=alpha,
                                 label='Minor Merger' if j == 0 and not minor_merger_labeled else "", zorder=9)
                        if j == 0:
                            minor_merger_labeled = True
    
    # Format each panel
    for j, (ax, config) in enumerate(zip(axes, panel_configs)):
        # ax.set_title(config['title'], fontsize=14, pad=10)
        ax.set_ylabel(config['ylabel'], fontsize=12)
        ax.set_xlabel('Redshift', fontsize=12)
        # ax.grid(True, alpha=0.3)
        ax.tick_params(labelsize=10)
        
        # Invert x-axis (higher redshift on left)
        # ax.invert_xaxis()
        
        # Add legend showing only merger events
        if j == 0:  # Only add legend to first panel
            handles, labels = ax.get_legend_handles_labels()
            
            # Filter to only merger-related entries
            merger_handles = []
            merger_labels = []
            
            for handle, label in zip(handles, labels):
                if 'Merger' in label:
                    merger_handles.append(handle)
                    merger_labels.append(label)
            
            # Create legend for merger events only
            if merger_handles:
                ax.legend(merger_handles, merger_labels, loc='upper right', fontsize=10,
                         frameon=True, framealpha=0.8, title='Merger Events')
    
    # Adjust layout and save
    plt.tight_layout()
    output_filename = os.path.join(output_dir, 'galaxy_evolution_6panel' + OutputFormat)
    fig.savefig(output_filename, dpi=500, bbox_inches='tight')
    logger.info(f'6-panel evolution plot saved as: {output_filename}')
    plt.close(fig)

def plot_galaxy_evolution_6panel_zoom(tracked_data, output_dir, hubble_h=0.73):
    """
    Create a 6-panel figure showing galaxy evolution:
    1. Mvir vs Redshift
    2. StellarMass vs Redshift  
    3. SFR vs Redshift
    4. sSFR vs Redshift
    5. ColdGas vs Redshift
    6. H2 fraction vs Redshift
    """
    logger.info('Creating 6-panel galaxy evolution plot...')
    
    # Setup figure
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    # Panel titles and labels
    panel_configs = [
        {'title': r'Virial Mass Evolution', 'ylabel': r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_mvir'},
        {'title': r'Stellar Mass Evolution', 'ylabel': r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$', 'data_key': 'log10_stellar_mass'},
        {'title': r'Star Formation Rate', 'ylabel': r'$\mathrm{SFR}\ (\mathrm{M}_\odot\ \mathrm{yr}^{-1})$', 'data_key': 'sfr'},
        {'title': r'Specific Star Formation Rate', 'ylabel': r'$\log_{10} \mathrm{sSFR}\ (\mathrm{yr}^{-1})$', 'data_key': 'ssfr'},
        {'title': r'Cold Gas Mass', 'ylabel': r'$\log_{10} M_{\mathrm{cold}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_cold_gas'},
        {'title': r'H$_2$ Fraction', 'ylabel': r'$f_{\mathrm{H_2}} = M_{\mathrm{H_2}} / (M_{\mathrm{H_2}} + M_{\mathrm{HI}})$', 'data_key': 'h2_fraction'}
    ]
    
    # Color palette for different galaxies
    colors = plt.cm.plasma(np.linspace(0, 1, len(tracked_data)))
    
    # Find the most massive halo at z=0 (or latest available redshift)
    most_massive_galaxy_id = None
    max_mvir = -np.inf
    
    for gal_id, gal_data in tracked_data.items():
        if len(gal_data['log10_mvir']) > 0:
            # Use the latest (lowest redshift) Mvir value
            latest_mvir = gal_data['log10_mvir'][-1]  # Assuming data is sorted by redshift
            if latest_mvir > max_mvir:
                max_mvir = latest_mvir
                most_massive_galaxy_id = gal_id
    
    logger.info(f'Most massive galaxy: {most_massive_galaxy_id} with log10(Mvir) = {max_mvir:.2f}')
    
    # Track if merger labels have been added
    major_merger_labeled = False
    minor_merger_labeled = False
    
    # Plot each galaxy's evolution
    for i, (gal_id, gal_data) in enumerate(tracked_data.items()):
        if len(gal_data['redshift']) == 0:
            continue
            
        color = colors[i % len(colors)]
        redshifts = np.array(gal_data['redshift'])
        merger_status = np.array(gal_data['merger_status'])
        has_major = np.array(gal_data['has_recent_major_merger'])
        has_minor = np.array(gal_data['has_recent_minor_merger'])
        
        # Set alpha based on whether this is the most massive galaxy
        alpha = 1.0 if gal_id == most_massive_galaxy_id else 0.1
        linewidth = 3.0 if gal_id == most_massive_galaxy_id else 1.0
        
        # Sort by redshift for clean lines
        sort_idx = np.argsort(redshifts)
        redshifts_sorted = redshifts[sort_idx]
        merger_status_sorted = merger_status[sort_idx]
        has_major_sorted = has_major[sort_idx]
        has_minor_sorted = has_minor[sort_idx]
        
        for j, config in enumerate(panel_configs):
            ax = axes[j]
            data_key = config['data_key']
            
            if data_key in gal_data and len(gal_data[data_key]) > 0:
                y_data = np.array(gal_data[data_key])[sort_idx]
                
                # Handle special case for SFR (not in log)
                if data_key == 'sfr':
                    y_data = np.log10(y_data)
                    y_data[~np.isfinite(y_data)] = np.nan
                
                # Plot line with markers
                valid_mask = np.isfinite(y_data)
                if np.any(valid_mask):
                    # Plot the main evolution line (no label for individual galaxies)
                    ax.plot(redshifts_sorted[valid_mask], y_data[valid_mask], color=color, alpha=alpha, linewidth=linewidth)
                    
                    # Add merger markers (only add labels once for legend)
                    # Major mergers: red stars
                    major_mask = valid_mask & has_major_sorted
                    if np.any(major_mask):
                        ax.scatter(redshifts_sorted[major_mask], y_data[major_mask], 
                                 marker='*', s=100, c='red', edgecolors='darkred', linewidth=linewidth, alpha=alpha,
                                 label='Major Merger' if j == 0 and not major_merger_labeled else "", zorder=10)
                        if j == 0:
                            major_merger_labeled = True
                    
                    # Minor mergers: orange triangles
                    minor_mask = valid_mask & has_minor_sorted
                    if np.any(minor_mask):
                        ax.scatter(redshifts_sorted[minor_mask], y_data[minor_mask], 
                                 marker='^', s=60, c='orange', edgecolors='darkorange', linewidth=linewidth, alpha=alpha,
                                 label='Minor Merger' if j == 0 and not minor_merger_labeled else "", zorder=9)
                        if j == 0:
                            minor_merger_labeled = True
    
    # Format each panel
    for j, (ax, config) in enumerate(zip(axes, panel_configs)):
        # ax.set_title(config['title'], fontsize=14, pad=10)
        ax.set_ylabel(config['ylabel'], fontsize=12)
        ax.set_xlabel('Redshift', fontsize=12)
        # ax.grid(True, alpha=0.3)
        ax.tick_params(labelsize=10)
        
        ax.set_xlim(0, 1.5)  # Set x-axis limits for zoomed view
        
        # Add legend showing only merger events
        if j == 0:  # Only add legend to first panel
            handles, labels = ax.get_legend_handles_labels()
            
            # Filter to only merger-related entries
            merger_handles = []
            merger_labels = []
            
            for handle, label in zip(handles, labels):
                if 'Merger' in label:
                    merger_handles.append(handle)
                    merger_labels.append(label)
            
            # Create legend for merger events only
            if merger_handles:
                ax.legend(merger_handles, merger_labels, loc='upper right', fontsize=10,
                         frameon=True, framealpha=0.8, title='Merger Events')
    
    # Adjust layout and save
    plt.tight_layout()
    output_filename = os.path.join(output_dir, 'galaxy_evolution_6panel_zoom' + OutputFormat)
    fig.savefig(output_filename, dpi=500, bbox_inches='tight')
    logger.info(f'6-panel evolution zoom plot saved as: {output_filename}')
    plt.close(fig)

def plot_galaxy_evolution_6panel_median(tracked_data, output_dir, hubble_h=0.73):
    """
    Create a 6-panel figure showing median galaxy evolution across the population:
    1. Mvir vs Redshift
    2. StellarMass vs Redshift  
    3. SFR vs Redshift
    4. sSFR vs Redshift
    5. ColdGas vs Redshift
    6. H2 fraction vs Redshift
    """
    logger.info('Creating 6-panel median galaxy evolution plot...')
    
    # Setup figure
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    # Panel titles and labels
    panel_configs = [
        {'title': r'Virial Mass Evolution', 'ylabel': r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_mvir'},
        {'title': r'Stellar Mass Evolution', 'ylabel': r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$', 'data_key': 'log10_stellar_mass'},
        {'title': r'Star Formation Rate', 'ylabel': r'$\mathrm{SFR}\ (\mathrm{M}_\odot\ \mathrm{yr}^{-1})$', 'data_key': 'sfr'},
        {'title': r'Specific Star Formation Rate', 'ylabel': r'$\log_{10} \mathrm{sSFR}\ (\mathrm{yr}^{-1})$', 'data_key': 'ssfr'},
        {'title': r'Cold Gas Mass', 'ylabel': r'$\log_{10} M_{\mathrm{cold}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_cold_gas'},
        {'title': r'H$_2$ Fraction', 'ylabel': r'$f_{\mathrm{H_2}} = M_{\mathrm{H_2}} / (M_{\mathrm{H_2}} + M_{\mathrm{HI}})$', 'data_key': 'h2_fraction'}
    ]
    
    # Collect all redshifts to determine common redshift grid
    all_redshifts = []
    for gal_id, gal_data in tracked_data.items():
        if len(gal_data['redshift']) > 0:
            all_redshifts.extend(gal_data['redshift'])
    
    if len(all_redshifts) == 0:
        logger.warning('No redshift data found!')
        return
    
    # Create common redshift grid
    unique_redshifts = sorted(set(all_redshifts))
    logger.info(f'Computing medians for {len(unique_redshifts)} redshift points')
    
    # For each panel, compute median evolution
    for j, config in enumerate(panel_configs):
        ax = axes[j]
        data_key = config['data_key']
        
        medians = []
        percentile_16 = []
        percentile_84 = []
        redshift_points = []
        
        # Calculate median at each redshift
        for z in unique_redshifts:
            values_at_z = []
            
            # Collect all galaxy values at this redshift
            for gal_id, gal_data in tracked_data.items():
                if (data_key in gal_data and len(gal_data[data_key]) > 0 and 
                    len(gal_data['redshift']) > 0):
                    
                    redshifts = np.array(gal_data['redshift'])
                    z_indices = np.where(np.abs(redshifts - z) < 1e-10)[0]
                    
                    if len(z_indices) > 0:
                        idx = z_indices[0]
                        value = gal_data[data_key][idx]
                        
                        # Handle special case for SFR (convert to log)
                        if data_key == 'sfr' and value > 0:
                            value = np.log10(value)
                        
                        if np.isfinite(value):
                            values_at_z.append(value)
            
            # Calculate statistics if we have enough data points
            if len(values_at_z) >= 3:  # Minimum for meaningful statistics
                values_at_z = np.array(values_at_z)
                medians.append(np.median(values_at_z))
                percentile_16.append(np.percentile(values_at_z, 16))
                percentile_84.append(np.percentile(values_at_z, 84))
                redshift_points.append(z)
        
        # Plot median evolution with uncertainty band
        if len(medians) > 0:
            redshift_points = np.array(redshift_points)
            medians = np.array(medians)
            percentile_16 = np.array(percentile_16)
            percentile_84 = np.array(percentile_84)
            
            # Plot median line
            ax.plot(redshift_points, medians, color='black', linewidth=3, zorder=10)
            
            # Plot uncertainty band (1-sigma)
            ax.fill_between(redshift_points, percentile_16, percentile_84, 
                           color='gray', alpha=0.3, zorder=5)
        
        # Format panel
        ax.set_ylabel(config['ylabel'], fontsize=12)
        ax.set_xlabel('Redshift', fontsize=12)
        ax.tick_params(labelsize=10)
        
        # Add legend to first panel with galaxy count
        if j == 0 and len(medians) > 0:
            # Count total number of galaxies in the tracked data
            n_galaxies = len(tracked_data)
            ax.legend([f'Median ({n_galaxies} galaxies)'], 
                     loc='upper right', fontsize=10, frameon=True, framealpha=0.8)
    
    # Adjust layout and save
    plt.tight_layout()
    output_filename = os.path.join(output_dir, 'galaxy_evolution_6panel_median' + OutputFormat)
    fig.savefig(output_filename, dpi=500, bbox_inches='tight')
    logger.info(f'6-panel median evolution plot saved as: {output_filename}')
    plt.close(fig)

def plot_galaxy_evolution_6panel_median_zoom(tracked_data, output_dir, hubble_h=0.73):
    """
    Create a 6-panel figure showing median galaxy evolution across the population (zoomed to z=0-1.5):
    1. Mvir vs Redshift
    2. StellarMass vs Redshift  
    3. SFR vs Redshift
    4. sSFR vs Redshift
    5. ColdGas vs Redshift
    6. H2 fraction vs Redshift
    """
    logger.info('Creating 6-panel median galaxy evolution plot (zoomed)...')
    
    # Setup figure
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    # Panel titles and labels
    panel_configs = [
        {'title': r'Virial Mass Evolution', 'ylabel': r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_mvir'},
        {'title': r'Stellar Mass Evolution', 'ylabel': r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$', 'data_key': 'log10_stellar_mass'},
        {'title': r'Star Formation Rate', 'ylabel': r'$\mathrm{SFR}\ (\mathrm{M}_\odot\ \mathrm{yr}^{-1})$', 'data_key': 'sfr'},
        {'title': r'Specific Star Formation Rate', 'ylabel': r'$\log_{10} \mathrm{sSFR}\ (\mathrm{yr}^{-1})$', 'data_key': 'ssfr'},
        {'title': r'Cold Gas Mass', 'ylabel': r'$\log_{10} M_{\mathrm{cold}}\ (\mathrm{M}_\odot)$', 'data_key': 'log10_cold_gas'},
        {'title': r'H$_2$ Fraction', 'ylabel': r'$f_{\mathrm{H_2}} = M_{\mathrm{H_2}} / (M_{\mathrm{H_2}} + M_{\mathrm{HI}})$', 'data_key': 'h2_fraction'}
    ]
    
    # Collect all redshifts to determine common redshift grid (filtered for zoom range)
    all_redshifts = []
    for gal_id, gal_data in tracked_data.items():
        if len(gal_data['redshift']) > 0:
            redshifts = np.array(gal_data['redshift'])
            zoom_redshifts = redshifts[(redshifts >= 0) & (redshifts <= 1.5)]
            all_redshifts.extend(zoom_redshifts)
    
    if len(all_redshifts) == 0:
        logger.warning('No redshift data found in zoom range!')
        return
    
    # Create common redshift grid
    unique_redshifts = sorted(set(all_redshifts))
    logger.info(f'Computing medians for {len(unique_redshifts)} redshift points in zoom range (z=0-1.5)')
    
    # For each panel, compute median evolution
    for j, config in enumerate(panel_configs):
        ax = axes[j]
        data_key = config['data_key']
        
        medians = []
        percentile_16 = []
        percentile_84 = []
        redshift_points = []
        
        # Calculate median at each redshift
        for z in unique_redshifts:
            values_at_z = []
            
            # Collect all galaxy values at this redshift
            for gal_id, gal_data in tracked_data.items():
                if (data_key in gal_data and len(gal_data[data_key]) > 0 and 
                    len(gal_data['redshift']) > 0):
                    
                    redshifts = np.array(gal_data['redshift'])
                    z_indices = np.where(np.abs(redshifts - z) < 1e-10)[0]
                    
                    if len(z_indices) > 0:
                        idx = z_indices[0]
                        value = gal_data[data_key][idx]
                        
                        # Handle special case for SFR (convert to log)
                        if data_key == 'sfr' and value > 0:
                            value = np.log10(value)
                        
                        if np.isfinite(value):
                            values_at_z.append(value)
            
            # Calculate statistics if we have enough data points
            if len(values_at_z) >= 3:  # Minimum for meaningful statistics
                values_at_z = np.array(values_at_z)
                medians.append(np.median(values_at_z))
                percentile_16.append(np.percentile(values_at_z, 16))
                percentile_84.append(np.percentile(values_at_z, 84))
                redshift_points.append(z)
        
        # Plot median evolution with uncertainty band
        if len(medians) > 0:
            redshift_points = np.array(redshift_points)
            medians = np.array(medians)
            percentile_16 = np.array(percentile_16)
            percentile_84 = np.array(percentile_84)
            
            # Plot median line
            ax.plot(redshift_points, medians, color='black', linewidth=3, zorder=10)
            
            # Plot uncertainty band (1-sigma)
            ax.fill_between(redshift_points, percentile_16, percentile_84, 
                           color='gray', alpha=0.3, zorder=5)
        
        # Format panel
        ax.set_ylabel(config['ylabel'], fontsize=12)
        ax.set_xlabel('Redshift', fontsize=12)
        ax.tick_params(labelsize=10)
        
        # Set zoom limits
        ax.set_xlim(0, 1.5)
        
        # Add legend to first panel with galaxy count
        if j == 0 and len(medians) > 0:
            # Count total number of galaxies in the tracked data
            n_galaxies = len(tracked_data)
            ax.legend([f'Median ({n_galaxies} galaxies)'], 
                     loc='upper right', fontsize=10, frameon=True, framealpha=0.8)
    
    # Adjust layout and save
    plt.tight_layout()
    output_filename = os.path.join(output_dir, 'galaxy_evolution_6panel_median_zoom' + OutputFormat)
    fig.savefig(output_filename, dpi=500, bbox_inches='tight')
    logger.info(f'6-panel median evolution zoom plot saved as: {output_filename}')
    plt.close(fig)

def plot_merger_timeline(tracked_data, output_dir, hubble_h=0.73):
    """
    Create a dedicated plot showing merger timeline for tracked galaxies
    """
    logger.info('Creating merger timeline plot...')
    
    fig, ax = create_figure(figsize=(12, 8))
    
    # Color palette for different galaxies
    colors = plt.cm.plasma(np.linspace(0, 1, len(tracked_data)))
    
    y_positions = {}
    y_counter = 0
    
    # Plot each galaxy's merger history
    major_merger_labeled = False
    minor_merger_labeled = False
    
    for i, (gal_id, gal_data) in enumerate(tracked_data.items()):
        if len(gal_data['redshift']) == 0:
            continue
            
        color = colors[i % len(colors)]
        redshifts = np.array(gal_data['redshift'])
        has_major = np.array(gal_data['has_recent_major_merger'])
        has_minor = np.array(gal_data['has_recent_minor_merger'])
        
        # Assign y position for this galaxy
        y_positions[gal_id] = y_counter
        y_pos = y_counter
        
        # Plot galaxy evolution line (no individual labels)
        ax.plot(redshifts, [y_pos] * len(redshifts), color=color, alpha=0.5, linewidth=2)
        
        # Plot merger events
        major_redshifts = redshifts[has_major]
        minor_redshifts = redshifts[has_minor]
        
        if len(major_redshifts) > 0:
            ax.scatter(major_redshifts, [y_pos] * len(major_redshifts), 
                      marker='*', s=150, c='red', edgecolors='darkred', linewidth=1.5,
                      label='Major Merger' if not major_merger_labeled else "", zorder=10)
            major_merger_labeled = True
            
        if len(minor_redshifts) > 0:
            ax.scatter(minor_redshifts, [y_pos] * len(minor_redshifts), 
                      marker='^', s=100, c='orange', edgecolors='darkorange', linewidth=1.5,
                      label='Minor Merger' if not minor_merger_labeled else "", zorder=9)
            minor_merger_labeled = True
        
        y_counter += 1
    
    # Format the plot
    ax.set_xlabel('Redshift', fontsize=14)
    ax.set_ylabel('Galaxy', fontsize=14)
    ax.set_title('Merger Timeline for Tracked Galaxies', fontsize=16, pad=20)
    
    # Set y-axis labels to generic galaxy labels
    # if y_positions:
    #     ax.set_yticks(list(y_positions.values()))
    #     ax.set_yticklabels([f'Galaxy {i+1}' for i in range(len(y_positions))])
    
    # Add grid and legend
    # ax.grid(True, alpha=0.3, axis='x')
    ax.legend(loc='upper right', fontsize=12, frameon=True, framealpha=0.8)
    
    # Adjust layout and save
    plt.tight_layout()
    output_filename = os.path.join(output_dir, 'merger_timeline' + OutputFormat)
    fig.savefig(output_filename, dpi=500, bbox_inches='tight')
    logger.info(f'Merger timeline plot saved as: {output_filename}')
    plt.close(fig)

def plot_merger_timeline_zoom(tracked_data, output_dir, hubble_h=0.73):
    """
    Create a dedicated plot showing merger timeline for tracked galaxies
    """
    logger.info('Creating merger timeline plot...')
    
    fig, ax = create_figure(figsize=(12, 8))
    
    # Color palette for different galaxies
    colors = plt.cm.plasma(np.linspace(0, 1, len(tracked_data)))
    
    y_positions = {}
    y_counter = 0
    
    # Plot each galaxy's merger history
    major_merger_labeled = False
    minor_merger_labeled = False
    
    for i, (gal_id, gal_data) in enumerate(tracked_data.items()):
        if len(gal_data['redshift']) == 0:
            continue
            
        color = colors[i % len(colors)]
        redshifts = np.array(gal_data['redshift'])
        has_major = np.array(gal_data['has_recent_major_merger'])
        has_minor = np.array(gal_data['has_recent_minor_merger'])
        
        # Assign y position for this galaxy
        y_positions[gal_id] = y_counter
        y_pos = y_counter
        
        # Plot galaxy evolution line (no individual labels)
        ax.plot(redshifts, [y_pos] * len(redshifts), color=color, alpha=0.5, linewidth=2)
        
        # Plot merger events
        major_redshifts = redshifts[has_major]
        minor_redshifts = redshifts[has_minor]
        
        if len(major_redshifts) > 0:
            ax.scatter(major_redshifts, [y_pos] * len(major_redshifts), 
                      marker='*', s=150, c='red', edgecolors='darkred', linewidth=1.5,
                      label='Major Merger' if not major_merger_labeled else "", zorder=10)
            major_merger_labeled = True
            
        if len(minor_redshifts) > 0:
            ax.scatter(minor_redshifts, [y_pos] * len(minor_redshifts), 
                      marker='^', s=100, c='orange', edgecolors='darkorange', linewidth=1.5,
                      label='Minor Merger' if not minor_merger_labeled else "", zorder=9)
            minor_merger_labeled = True
        
        y_counter += 1
    
    # Format the plot
    ax.set_xlabel('Redshift', fontsize=14)
    ax.set_ylabel('Galaxy', fontsize=14)
    ax.set_title('Merger Timeline for Tracked Galaxies', fontsize=16, pad=20)

    # Set x-axis limits for zoomed view
    ax.set_xlim(0, 1.5)
    
    # Set y-axis labels to generic galaxy labels
    # if y_positions:
    #     ax.set_yticks(list(y_positions.values()))
    #     ax.set_yticklabels([f'Galaxy {i+1}' for i in range(len(y_positions))])
    
    # Add grid and legend
    # ax.grid(True, alpha=0.3, axis='x')
    ax.legend(loc='upper right', fontsize=12, frameon=True, framealpha=0.8)
    
    # Adjust layout and save
    plt.tight_layout()
    output_filename = os.path.join(output_dir, 'merger_timeline_zoom' + OutputFormat)
    fig.savefig(output_filename, dpi=500, bbox_inches='tight')
    logger.info(f'Merger timeline zoom plot saved as: {output_filename}')
    plt.close(fig)

def print_merger_summary(tracked_data):
    """
    Print a summary of merger events for all tracked galaxies
    """
    logger.info('=== MERGER SUMMARY ===')
    
    total_major_mergers = 0
    total_minor_mergers = 0
    galaxies_with_major = 0
    galaxies_with_minor = 0
    
    for gal_id, gal_data in tracked_data.items():
        has_major = np.array(gal_data['has_recent_major_merger'])
        has_minor = np.array(gal_data['has_recent_minor_merger'])
        redshifts = np.array(gal_data['redshift'])
        
        major_count = np.sum(has_major)
        minor_count = np.sum(has_minor)
        
        total_major_mergers += major_count
        total_minor_mergers += minor_count
        
        if major_count > 0:
            galaxies_with_major += 1
        if minor_count > 0:
            galaxies_with_minor += 1
        
        if major_count > 0 or minor_count > 0:
            logger.info(f'Galaxy {gal_id}: {major_count} major mergers, {minor_count} minor mergers')
            
            # Print redshifts of merger events
            if major_count > 0:
                major_z = redshifts[has_major]
                logger.info(f'  Major mergers at z = {major_z}')
            if minor_count > 0:
                minor_z = redshifts[has_minor]
                logger.info(f'  Minor mergers at z = {minor_z}')
    
    logger.info(f'\nOVERALL SUMMARY:')
    logger.info(f'Total galaxies tracked: {len(tracked_data)}')
    logger.info(f'Galaxies with major mergers: {galaxies_with_major} ({100*galaxies_with_major/len(tracked_data):.1f}%)')
    logger.info(f'Galaxies with minor mergers: {galaxies_with_minor} ({100*galaxies_with_minor/len(tracked_data):.1f}%)')
    logger.info(f'Total major merger events: {total_major_mergers}')
    logger.info(f'Total minor merger events: {total_minor_mergers}')
    logger.info(f'Average major mergers per galaxy: {total_major_mergers/len(tracked_data):.2f}')
    logger.info(f'Average minor mergers per galaxy: {total_minor_mergers/len(tracked_data):.2f}')


def plot_galaxy_properties_distributions(tracked_data, output_dir, redshift_range=(0.0, 1.5), hubble_h=0.73, use_density=True):
    """
    Create a 6-panel figure showing normalized distributions of galaxy properties:
    1. Mvir distribution
    2. StellarMass distribution  
    3. SFR distribution
    4. sSFR distribution
    5. ColdGas distribution
    6. H2 fraction distribution
    
    Parameters:
    -----------
    tracked_data : dict
        Dictionary containing tracked galaxy data
    output_dir : str
        Output directory for saving plots
    redshift_range : tuple
        Redshift range to include in analysis (min, max)
    hubble_h : float
        Hubble parameter
    use_density : bool
        If True, use probability density (area under curve = 1, y-axis can be > 1)
        If False, use frequency counts (y-axis shows actual counts)
    """
    density_str = "density" if use_density else "frequency"
    logger.info(f'Creating galaxy properties {density_str} distributions plot (z={redshift_range[0]}-{redshift_range[1]})...')
    
    # Setup figure
    fig, axes = plt.subplots(2, 3, figsize=(18, 12))
    axes = axes.flatten()
    
    # Panel configurations
    panel_configs = [
        {
            'title': r'Virial Mass Distribution',
            'xlabel': r'$\log_{10} M_{\mathrm{vir}}\ (\mathrm{M}_\odot)$',
            'data_key': 'log10_mvir',
            'bins': np.arange(13.0, 15.5, 0.1),
            'color': '#1f77b4'
        },
        {
            'title': r'Stellar Mass Distribution',
            'xlabel': r'$\log_{10} M_\star\ (\mathrm{M}_\odot)$',
            'data_key': 'log10_stellar_mass',
            'bins': np.arange(10.5, 12.5, 0.05),
            'color': '#ff7f0e'
        },
        {
            'title': r'Star Formation Rate Distribution',
            'xlabel': r'$\log_{10} \mathrm{SFR}\ (\mathrm{M}_\odot\ \mathrm{yr}^{-1})$',
            'data_key': 'sfr',
            'bins': np.arange(-2.0, 3.0, 0.1),
            'color': '#2ca02c'
        },
        {
            'title': r'Specific SFR Distribution',
            'xlabel': r'$\log_{10} \mathrm{sSFR}\ (\mathrm{yr}^{-1})$',
            'data_key': 'ssfr',
            'bins': np.arange(-12.0, -8.0, 0.1),
            'color': '#d62728'
        },
        {
            'title': r'Cold Gas Mass Distribution',
            'xlabel': r'$\log_{10} M_{\mathrm{cold}}\ (\mathrm{M}_\odot)$',
            'data_key': 'log10_cold_gas',
            'bins': np.arange(9.0, 12.0, 0.1),
            'color': '#9467bd'
        },
        {
            'title': r'H$_2$ Fraction Distribution',
            'xlabel': r'$f_{\mathrm{H_2}} = M_{\mathrm{H_2}} / (M_{\mathrm{HI}} + M_{\mathrm{H_2}})$',
            'data_key': 'h2_fraction',
            'bins': np.arange(0.0, 1.0, 0.02),
            'color': '#8c564b'
        }
    ]
    
    # Collect data from z=0 snapshot only (one data point per galaxy)
    all_data = {config['data_key']: [] for config in panel_configs}
    total_galaxies_found = 0
    
    for gal_id, gal_data in tracked_data.items():
        if len(gal_data['redshift']) == 0:
            continue
            
        redshifts = np.array(gal_data['redshift'])
        
        # Find the z=0 snapshot (closest to redshift 0.0)
        z0_idx = np.argmin(np.abs(redshifts))
        z0_redshift = redshifts[z0_idx]
        
        # Only include if this is actually close to z=0 (within redshift range)
        if z0_redshift >= redshift_range[0] and z0_redshift <= redshift_range[1]:
            total_galaxies_found += 1
            
            # Collect data for each property at z=0
            for config in panel_configs:
                data_key = config['data_key']
                
                if data_key in gal_data and len(gal_data[data_key]) > 0:
                    data_values = np.array(gal_data[data_key])
                    
                    # Get the value at z=0 snapshot
                    z0_value = data_values[z0_idx]
                    
                    # Handle special case for SFR (convert to log)
                    if data_key == 'sfr':
                        if z0_value > 0:
                            log_sfr = np.log10(z0_value)
                            if np.isfinite(log_sfr):
                                all_data[data_key].append(log_sfr)
                    else:
                        # Only include finite values
                        if np.isfinite(z0_value):
                            all_data[data_key].append(z0_value)
    
    logger.info(f'Collected z=0 data from {total_galaxies_found} galaxies (one data point per galaxy)')
    for config in panel_configs:
        data_key = config['data_key']
        logger.info(f'{data_key}: {len(all_data[data_key])} data points')
    
    # Create histograms for each panel
    for j, config in enumerate(panel_configs):
        ax = axes[j]
        data_key = config['data_key']
        
        if len(all_data[data_key]) == 0:
            logger.warning(f'No data available for {data_key}')
            ax.text(0.5, 0.5, 'No Data', transform=ax.transAxes, 
                   ha='center', va='center', fontsize=16)
            ax.set_xlabel(config['xlabel'], fontsize=12)
            ax.set_title(config['title'], fontsize=14, pad=10)
            continue
        
        data_values = np.array(all_data[data_key])
        
        # Create histogram with chosen normalization
        y_label = 'Probability Density' if use_density else 'Frequency'
        counts, bin_edges, patches = ax.hist(data_values, bins=config['bins'], 
                                           density=use_density, alpha=0.7, 
                                           color=config['color'], 
                                           edgecolor='black', linewidth=0.5)
        
        # Add statistics text
        median_val = np.median(data_values)
        mean_val = np.mean(data_values)
        std_val = np.std(data_values)
        n_points = len(data_values)
        
        # Position stats text
        stats_text = f'N = {n_points}\nMedian = {median_val:.2f}\nMean = {mean_val:.2f}\nStd = {std_val:.2f}'
        ax.text(0.05, 0.95, stats_text, transform=ax.transAxes, 
               verticalalignment='top', fontsize=10,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        
        # Add vertical line for median
        ax.axvline(median_val, color='red', linestyle='--', linewidth=2, alpha=0.8, label=f'Median')
        
        # Formatting
        ax.set_xlabel(config['xlabel'], fontsize=12)
        ax.set_ylabel(y_label, fontsize=12)
        ax.set_title(config['title'], fontsize=14, pad=10)
        ax.tick_params(labelsize=10)
        # ax.grid(True, alpha=0.3)
        
        # Add legend for median line
        ax.legend(loc='upper right', fontsize=10, framealpha=0.8)
        
        logger.info(f'{data_key}: N={n_points}, median={median_val:.2f}, mean={mean_val:.2f}, std={std_val:.2f}')
    
    # Add overall title
    fig.suptitle(f'Galaxy Properties {density_str.title()} Distributions (z={redshift_range[0]}-{redshift_range[1]})', 
                fontsize=16, y=0.98)
    
    # Adjust layout and save
    plt.tight_layout()
    plt.subplots_adjust(top=0.93)  # Make room for suptitle
    
    # Create filename with redshift range and type
    z_str = f'z{redshift_range[0]:.1f}-{redshift_range[1]:.1f}'.replace('.', 'p')
    output_filename = os.path.join(output_dir, f'galaxy_properties_{density_str}_{z_str}' + OutputFormat)
    fig.savefig(output_filename, dpi=500, bbox_inches='tight')
    logger.info(f'Galaxy properties {density_str} distributions plot saved as: {output_filename}')
    plt.close(fig)

def analyze_massive_galaxy_evolution(directory=None, snapshot='Snap_63', output_dir=None):
    """
    Main function to identify and track low quiescent fraction massive galaxies.
    """
    if directory is None:
        directory = DirName
    if output_dir is None:
        output_dir = directory + 'plots/'
    
    # Ensure output directory exists
    Path(output_dir).mkdir(exist_ok=True)
    
    logger.info('=== Massive Galaxy Evolution Analysis ===')
    logger.info(f'Using directory: {directory}')
    logger.info(f'Reference snapshot: {snapshot}')
    
    # Step 1: Identify target galaxies
    target_galaxy_ids = identify_target_galaxies(
        directory=directory,
        snapshot=snapshot,
        stellar_mass_min=11.0,
        mvir_min=14.0,
        quiescent_fraction_max=0.5,
        hubble_h=Main_Hubble_h
    )
    
    if not target_galaxy_ids:
        logger.error('No target galaxies found!')
        return
    
    # Step 2: Track their evolution
    tracked_data = track_galaxy_evolution(
        directory=directory,
        galaxy_ids=target_galaxy_ids,
        hubble_h=Main_Hubble_h,
        max_galaxies=100000  # Limit for performance and readability
    )
    
    # Step 3: Create evolution plots with merger markers
    plot_galaxy_evolution_6panel(
        tracked_data=tracked_data,
        output_dir=output_dir,
        hubble_h=Main_Hubble_h
    )
    
    # Step 4: Create evolution plots with merger markers
    plot_galaxy_evolution_6panel_zoom(
        tracked_data=tracked_data,
        output_dir=output_dir,
        hubble_h=Main_Hubble_h
    )

    # Step 5: Create median evolution plots (no merger markers)
    plot_galaxy_evolution_6panel_median(
        tracked_data=tracked_data,
        output_dir=output_dir,
        hubble_h=Main_Hubble_h
    )
    
    # Step 6: Create median evolution plots (zoomed, no merger markers)
    plot_galaxy_evolution_6panel_median_zoom(
        tracked_data=tracked_data,
        output_dir=output_dir,
        hubble_h=Main_Hubble_h
    )

    # Step 7: Create dedicated merger timeline plot
    plot_merger_timeline(
        tracked_data=tracked_data,
        output_dir=output_dir,
        hubble_h=Main_Hubble_h
    )
    
# Step 7: Create dedicated merger timeline plot
    plot_merger_timeline_zoom(
        tracked_data=tracked_data,
        output_dir=output_dir,
        hubble_h=Main_Hubble_h
    )

    # Step 8: Print merger summary
    print_merger_summary(tracked_data)

    # Step 9: Create galaxy properties distributions at z=0 (frequency counts)
    plot_galaxy_properties_distributions(
        tracked_data=tracked_data,
        output_dir=output_dir,
        redshift_range=(0.0, 0.1),  # z=0 snapshot only
        hubble_h=Main_Hubble_h,
        use_density=False  # Raw frequency counts
    )
    
    logger.info('Massive galaxy evolution analysis with merger tracking complete!')
    return tracked_data

# ========================== MAIN EXECUTION ==========================

def main():
    """Main function for plotting with MPI support - runs analysis for all configured models"""
    
    # Determine MPI status
    if HAS_MPI:
        comm = MPI.COMM_WORLD
        rank = comm.Get_rank()
        size = comm.Get_size()
    else:
        rank = 0
        size = 1
    
    if rank == 0:
        print('Running stellar-halo mass relation plotting for multiple models')
        print(f'MPI processes: {size}')
        print('Ultra-optimized mass loading vs virial velocity analysis\n')
        
        # Setup paper plotting style FIRST
        setup_paper_style()
        
        # Setup
        setup_cache()
        
        logger.info(f'Smart sampling: {SMART_SAMPLING}')
        logger.info(f'Caching enabled: {ENABLE_CACHING}')
        logger.info(f'Memory mapping: {USE_MEMMAP}')
        logger.info(f'MPI available: {HAS_MPI}')
        logger.info(f'Number of models to process: {len(MODEL_CONFIGS)}')
    
    overall_start_time = time.time()
    
    # Loop through each model configuration
    for model_idx, model_config in enumerate(MODEL_CONFIGS):
        if rank == 0:
            logger.info(f'\n' + '='*80)
            logger.info(f'PROCESSING MODEL {model_idx + 1}/{len(MODEL_CONFIGS)}: {model_config["name"].upper()}')
            logger.info('='*80)
            
            # Check if model directory exists
            model_directory = model_config['directory']
            if not os.path.exists(model_directory):
                logger.warning(f'Model directory does not exist: {model_directory}')
                logger.warning(f'Skipping model {model_config["name"]} and moving to next model...')
                skip_model = True
            elif not os.path.isdir(model_directory):
                logger.warning(f'Model path exists but is not a directory: {model_directory}')
                logger.warning(f'Skipping model {model_config["name"]} and moving to next model...')
                skip_model = True
            else:
                logger.info(f'Model directory found: {model_directory}')
                skip_model = False
        else:
            skip_model = False
        
        # Broadcast directory check result to all ranks
        if HAS_MPI:
            skip_model = comm.bcast(skip_model, root=0)
        
        # All ranks skip together if directory doesn't exist
        if skip_model:
            continue
        
        # Set up global variables for this model
        if rank == 0:
            setup_model_config(model_config)
        
        # Synchronize all processes after config setup
        if HAS_MPI:
            comm.Barrier()
            # Broadcast the configuration to all ranks
            model_config = comm.bcast(model_config, root=0)
            if rank != 0:
                setup_model_config(model_config)
        
        model_start_time = time.time()
        
        # Initialize OutputDir for all ranks
        OutputDir = None
        
        if rank == 0:
            seed(2222)
            volume = (Main_BoxSize/Main_Hubble_h)**3.0 * Main_VolumeFraction

            OutputDir = DirName + 'sf_trace_plots/'
            Path(OutputDir).mkdir(exist_ok=True)

            logger.info(f'Reading galaxy properties from {DirName}')
            model_files = get_file_list(DirName)
            logger.info(f'Found {len(model_files)} model files')

            # Read galaxy properties with optimized function
            logger.info('Loading basic properties for validation...')
            try:
                Vvir = read_hdf_ultra_optimized(snap_num=Snapshot, param='Vvir')
                logger.info(f'Total galaxies: {len(Vvir)}')
                validation_success = True
            except Exception as e:
                logger.error(f'Error reading {Snapshot} from {DirName}: {e}')
                logger.warning(f'Skipping model {model_config["name"]} due to read error')
                validation_success = False
        else:
            validation_success = True
        
        # Broadcast validation result to all ranks
        if HAS_MPI:
            validation_success = comm.bcast(validation_success, root=0)
        
        # All ranks skip together if validation failed
        if not validation_success:
            continue
        
        # Synchronize before plotting
        if HAS_MPI:
            comm.Barrier()
        
        try:
            # All ranks participate in plotting (MPI grid computation happens inside plot functions)
            plot_stellar_halo_mass_relation(SMF_SimConfigs, Snapshot, OutputDir if rank == 0 else None)
            plot_halo_stellar_mass_relation(SMF_SimConfigs, Snapshot, OutputDir if rank == 0 else None)
            plot_halo_stellar_mass_relation_centrals(SMF_SimConfigs, Snapshot, OutputDir if rank == 0 else None)
            plot_halo_stellar_mass_relation_satellites(SMF_SimConfigs, Snapshot, OutputDir if rank == 0 else None)
            
            # NEW: Massive galaxy evolution analysis (only rank 0)
            if rank == 0:
                logger.info('\n' + '='*50)
                logger.info('Starting massive galaxy evolution analysis...')
                tracked_data = analyze_massive_galaxy_evolution(
                    directory=DirName,
                    snapshot=Snapshot,
                    output_dir=OutputDir
                )
            
            if rank == 0:
                model_time = time.time() - model_start_time
                logger.info(f'Model {model_config["name"]} execution time: {model_time:.2f} seconds')
                logger.info(f'Model {model_config["name"]} analysis complete!')
        
        except Exception as e:
            if rank == 0:
                logger.error(f'Error processing model {model_config["name"]}: {e}')
                logger.warning(f'Continuing with next model...')
        
        # Cleanup between models (only rank 0)
        if rank == 0 and AGGRESSIVE_CLEANUP:
            force_cleanup()
        
        # Synchronize all processes before next model
        if HAS_MPI:
            comm.Barrier()

    # Final summary and cleanup
    if rank == 0:
        total_time = time.time() - overall_start_time
        logger.info(f'\n' + '='*80)
        logger.info(f'ALL MODELS COMPLETE!')
        logger.info(f'Total execution time: {total_time:.2f} seconds')
        if len(MODEL_CONFIGS) > 0:
            logger.info(f'Average time per model: {total_time/len(MODEL_CONFIGS):.2f} seconds')
        logger.info('='*80)
        
        force_cleanup()  # Final cleanup
    
    # Final synchronization - ensure all ranks reach this point
    if HAS_MPI:
        comm.Barrier()
        if rank == 0:
            logger.info('All MPI processes synchronized. Exiting...')


if __name__ == '__main__':
    main()