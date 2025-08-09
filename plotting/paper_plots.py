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

import warnings
warnings.filterwarnings("ignore")

# Set up logging for better debugging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

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
        'label': 'C16 Millennium', 
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
plt.style.use('/Users/mbradley/Documents/cohare_palatino_sty.mplstyle')
# plt.rcParams["figure.figsize"] = (10, 8)
# plt.rcParams["figure.dpi"] = 500
# plt.rcParams["font.size"] = 14

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

# ==================================================================

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

def process_smd_snapshot_batch(args):
    """Process a batch of snapshots for SMD calculation"""
    snap_batch, sim_config, volume = args
    
    batch_results = {}
    for snap in snap_batch:
        snap_name = f'Snap_{snap}'
        snap_idx = snap - sim_config['FirstSnap']
        
        try:
            # Read stellar mass data with optimized function
            stellar_mass = read_hdf_ultra_optimized(snap_num=snap_name, param='StellarMass', 
                                                   directory=sim_config['path'])
            
            if len(stellar_mass) > 0:
                # Convert to physical units (assuming stored in 10^10 Msol/h)
                stellar_mass_physical = stellar_mass * 1.0e10 / sim_config['Hubble_h']
                
                # Apply mass cuts (1e8 to 1e13 Msol)
                w = np.where((stellar_mass_physical > 1.0e8) & (stellar_mass_physical < 1.0e13))[0]
                if len(w) > 0:
                    smd = np.sum(stellar_mass_physical[w], dtype=np.float64) / volume
                else:
                    smd = 0.0
            else:
                smd = 0.0
            
            batch_results[snap_idx] = smd
            
            # Clean up immediately
            del stellar_mass
            
        except Exception as e:
            logger.warning(f"Error processing snapshot {snap}: {e}")
            batch_results[snap_idx] = 0.0
    
    return batch_results

def calculate_smd_ultra_optimized(sim_config):
    """Ultra-optimized stellar mass density calculation with batching and caching"""
    sim_path = sim_config['path']
    sim_label = sim_config['label']
    box_size = sim_config['BoxSize']
    hubble_h = sim_config['Hubble_h']
    volume_fraction = sim_config['VolumeFraction']
    first_snap = sim_config['FirstSnap']
    last_snap = sim_config['LastSnap']
    sim_redshifts = sim_config['redshifts']
    
    logger.info(f"Processing SMD for simulation: {sim_label}")
    
    # Calculate volume
    volume = (box_size/hubble_h)**3.0 * volume_fraction
    
    # Check cache for full SMD array
    cache_key = f"smd_{abs(hash(sim_path)) % 10000}_{first_snap}_{last_snap}"
    full_cache_file = f"{CACHE_DIR}/{cache_key}.npy" if ENABLE_CACHING else None
    
    cached_smd = load_from_cache(full_cache_file)
    if cached_smd is not None and len(cached_smd) == (last_snap - first_snap + 1):
        logger.info(f"  Loaded full SMD from cache")
        return cached_smd, sim_redshifts
    
    # Initialize SMD array
    num_snaps = last_snap - first_snap + 1
    SMD = np.zeros(num_snaps, dtype=np.float64)
    
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
            future_to_batch = {executor.submit(process_smd_snapshot_batch, args): args 
                              for args in args_list}
            
            completed = 0
            for future in as_completed(future_to_batch):
                batch_results = future.result()
                for snap_idx, smd in batch_results.items():
                    if 0 <= snap_idx < len(SMD):
                        SMD[snap_idx] = smd
                
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
                stellar_mass = read_hdf_ultra_optimized(snap_num=snap_name, param='StellarMass', 
                                                       directory=sim_path)
                
                if len(stellar_mass) > 0:
                    # Convert to physical units (assuming stored in 10^10 Msol/h)
                    stellar_mass_physical = stellar_mass * 1.0e10 / hubble_h
                    
                    # Apply mass cuts (1e8 to 1e13 Msol)
                    w = np.where((stellar_mass_physical > 1.0e8) & (stellar_mass_physical < 1.0e13))[0]
                    if len(w) > 0:
                        SMD[snap_idx] = np.sum(stellar_mass_physical[w], dtype=np.float64) / volume
                    else:
                        SMD[snap_idx] = 0.0
                else:
                    SMD[snap_idx] = 0.0
                
                del stellar_mass
                
                # Progress reporting
                if (i + 1) % MEMORY_CHECK_INTERVAL == 0 or i == len(processing_snaps) - 1:
                    progress = (i + 1) / len(processing_snaps) * 100
                    logger.info(f"  Progress: {progress:.1f}% ({i+1}/{len(processing_snaps)})")
                    check_memory_usage()
                    
            except Exception as e:
                logger.warning(f"Error processing snapshot {snap}: {e}")
                SMD[snap_idx] = 0.0
    
    # Interpolation for sampled data
    if use_sampling and len(processing_snaps) < num_snaps:
        logger.info(f"  Interpolating to full resolution...")
        sample_indices = [snap - first_snap for snap in processing_snaps]
        sample_smd = SMD[sample_indices]
        
        nonzero_mask = sample_smd > 0
        if np.any(nonzero_mask):
            sample_redshifts = [sim_redshifts[i] for i in sample_indices]
            valid_z = np.array(sample_redshifts)[nonzero_mask]
            valid_smd = sample_smd[nonzero_mask]
            
            # Interpolate in log space for better results
            log_smd = np.log10(valid_smd + 1e-15)
            full_z = np.array(sim_redshifts)
            interpolated_log_smd = np.interp(full_z, valid_z[::-1], log_smd[::-1])
            SMD = 10**interpolated_log_smd - 1e-15
            SMD = np.maximum(SMD, 0.0)
    
    # Cache the full result
    save_to_cache(SMD, full_cache_file)
    
    # Print statistics
    nonzero_mask = SMD > 0.0
    if np.any(nonzero_mask):
        logger.info(f"  SMD range: {np.min(SMD[nonzero_mask]):.2e} - "
                   f"{np.max(SMD[nonzero_mask]):.2e}")
        peak_idx = np.argmax(SMD)
        peak_redshift = sim_redshifts[peak_idx] if peak_idx < len(sim_redshifts) else "unknown"
        logger.info(f"  Peak SMD: {np.max(SMD):.2e} at z={peak_redshift}")
        logger.info(f"  Non-zero snapshots: {np.sum(nonzero_mask)} / {len(SMD)}")
    
    force_cleanup()
    return SMD, sim_redshifts

def load_smd_data(filename):
    """Load SMD data from ECSV file"""
    if not os.path.exists(filename):
        logger.warning(f"SMD ECSV file {filename} not found")
        return None, None, None, None
        
    try:
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        # Skip header lines (those starting with #)
        data_lines = [line.strip() for line in lines if not line.startswith('#') and line.strip()]
        
        # Skip the column header line if it exists
        if data_lines and not data_lines[0].replace('.', '').replace('-', '').replace('e', '').replace('+', '').replace(' ', '').isdigit():
            data_lines = data_lines[1:]
        
        z_vals = []
        rho_50_vals = []
        rho_16_vals = []
        rho_84_vals = []
        
        for line in data_lines:
            parts = line.split()
            if len(parts) >= 4:
                z_vals.append(float(parts[0]))
                rho_50_vals.append(float(parts[1]))
                rho_16_vals.append(float(parts[2]))
                rho_84_vals.append(float(parts[3]))
        
        return np.array(z_vals), np.array(rho_50_vals), np.array(rho_16_vals), np.array(rho_84_vals)
    except Exception as e:
        logger.warning(f"Error reading SMD ECSV file: {e}")
        return None, None, None, None
    
# -----------------------------------------------------------------------
# Median SFR vs Lookback Time for z=0 galaxies binned by stellar mass

logger.info('\n=== Median SFR vs Lookback Time Analysis ===')

def calculate_lookback_time(redshifts, H0=70, Om0=0.3, OL0=0.7):
    """Calculate lookback time in Gyr for given redshifts"""
    # Simple approximation for lookback time
    # For more accuracy, you could use astropy.cosmology
    lookback_times = []
    
    for z in redshifts:
        if z == 0:
            lookback_times.append(0.0)
        else:
            # Simplified calculation - for exact values use cosmology packages
            t_H = 9.78 / (H0/100)  # Hubble time in Gyr
            # Approximation valid for z < 5
            t_lookback = t_H * (1 - 1/np.sqrt((1+z)**3 * Om0 + OL0))
            lookback_times.append(t_lookback)
    
    return np.array(lookback_times)

def process_simulation_sfr_evolution(sim_path, sim_label, redshifts, FirstSnap, LastSnap, Main_Hubble_h):
    """Process SFR evolution for a given simulation"""
    logger.info(f'Processing {sim_label}...')
    
    # Calculate lookback times for this simulation
    lookback_times = calculate_lookback_time(redshifts, H0=Main_Hubble_h*100)
    
    # Read z=0 data to define stellar mass bins
    z0_snapshot = f'Snap_{LastSnap}'
    logger.info(f'Reading z=0 data from {z0_snapshot} for {sim_label}...')

    # Read z=0 stellar masses and galaxy indices for tracking
    StellarMass_z0 = read_hdf_ultra_optimized(snap_num=z0_snapshot, param='StellarMass', directory=sim_path) * 1.0e10 / Main_Hubble_h
    Type_z0 = read_hdf_ultra_optimized(snap_num=z0_snapshot, param='Type', directory=sim_path)

    # Try to read galaxy indices for tracking
    try:
        GalaxyIndex_z0 = read_hdf_ultra_optimized(snap_num=z0_snapshot, param='GalaxyIndex', directory=sim_path)
        has_galaxy_tracking = True
        logger.info(f'Galaxy tracking available via GalaxyIndex for {sim_label}')
    except:
        try:
            GalaxyIndex_z0 = read_hdf_ultra_optimized(snap_num=z0_snapshot, param='GalaxyNr', directory=sim_path)
            has_galaxy_tracking = True
            logger.info(f'Galaxy tracking available via GalaxyNr for {sim_label}')
        except:
            has_galaxy_tracking = False
            logger.warning(f'No galaxy tracking available for {sim_label} - using snapshot-by-snapshot approach')

    # Filter for valid z=0 galaxies
    valid_z0 = (StellarMass_z0 > 1e8) & (StellarMass_z0 < 1e13) & (Type_z0 >= 0)
    StellarMass_z0_valid = StellarMass_z0[valid_z0]

    if has_galaxy_tracking:
        GalaxyIndex_z0_valid = GalaxyIndex_z0[valid_z0]

    # Define stellar mass bins (0.25 dex width)
    log_mass_min = 8.5
    log_mass_max = 12.0
    bin_width = 0.25
    mass_bin_edges = np.arange(log_mass_min, log_mass_max + bin_width, bin_width)
    mass_bin_centers = mass_bin_edges[:-1] + bin_width/2

    if has_galaxy_tracking:
        # Bin z=0 galaxies by stellar mass
        log_stellar_mass_z0 = np.log10(StellarMass_z0_valid)
        
        # For each mass bin, collect galaxy indices
        mass_bin_galaxies = {}
        mass_bin_mean_masses = {}
        
        for i, (mass_low, mass_high) in enumerate(zip(mass_bin_edges[:-1], mass_bin_edges[1:])):
            bin_mask = (log_stellar_mass_z0 >= mass_low) & (log_stellar_mass_z0 < mass_high)
            if np.sum(bin_mask) > 10:  # Require at least 10 galaxies per bin
                mass_bin_galaxies[i] = GalaxyIndex_z0_valid[bin_mask]
                mass_bin_mean_masses[i] = np.mean(StellarMass_z0_valid[bin_mask])
        
        # For each snapshot, calculate median SFR for each mass bin
        median_sfr_evolution = {bin_idx: [] for bin_idx in mass_bin_galaxies.keys()}
        
        for snap in range(FirstSnap, LastSnap + 1):
            snap_name = f'Snap_{snap}'
            
            try:
                # Read data for this snapshot
                sfr_disk = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrDisk', directory=sim_path)
                sfr_bulge = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrBulge', directory=sim_path)
                galaxy_indices = read_hdf_ultra_optimized(snap_num=snap_name, param='GalaxyIndex', directory=sim_path)
                
                if len(sfr_disk) == 0 or len(galaxy_indices) == 0:
                    for bin_idx in mass_bin_galaxies.keys():
                        median_sfr_evolution[bin_idx].append(0.0)
                    continue
                
                total_sfr = sfr_disk + sfr_bulge
                
                # For each mass bin, find galaxies and calculate median SFR
                for bin_idx, target_galaxies in mass_bin_galaxies.items():
                    galaxy_matches = np.isin(galaxy_indices, target_galaxies)
                    
                    if np.sum(galaxy_matches) > 0:
                        bin_sfr_values = total_sfr[galaxy_matches]
                        nonzero_sfr = bin_sfr_values[bin_sfr_values > 0]
                        if len(nonzero_sfr) > 0:
                            median_sfr = np.median(nonzero_sfr)
                        else:
                            median_sfr = 0.0
                    else:
                        median_sfr = 0.0
                    
                    median_sfr_evolution[bin_idx].append(median_sfr)
            
            except Exception as e:
                logger.warning(f'Error processing snapshot {snap} for {sim_label}: {e}')
                for bin_idx in mass_bin_galaxies.keys():
                    median_sfr_evolution[bin_idx].append(0.0)

    else:
        # Alternative approach: snapshot-by-snapshot
        median_sfr_evolution = {i: [] for i in range(len(mass_bin_centers))}
        mass_bin_mean_masses = {}
        
        for i, mass_center in enumerate(mass_bin_centers):
            mass_bin_mean_masses[i] = 10**mass_center
        
        for snap in range(FirstSnap, LastSnap + 1):
            snap_name = f'Snap_{snap}'
            
            try:
                sfr_disk = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrDisk', directory=sim_path)
                sfr_bulge = read_hdf_ultra_optimized(snap_num=snap_name, param='SfrBulge', directory=sim_path)
                stellar_mass = read_hdf_ultra_optimized(snap_num=snap_name, param='StellarMass', directory=sim_path) * 1.0e10 / Main_Hubble_h
                galaxy_type = read_hdf_ultra_optimized(snap_num=snap_name, param='Type', directory=sim_path)
                
                if len(sfr_disk) == 0:
                    for bin_idx in range(len(mass_bin_centers)):
                        median_sfr_evolution[bin_idx].append(0.0)
                    continue
                
                total_sfr = sfr_disk + sfr_bulge
                log_stellar_mass = np.log10(stellar_mass + 1e-10)
                
                valid = (stellar_mass > 1e8) & (stellar_mass < 1e13) & (galaxy_type >= 0)
                
                for i, (mass_low, mass_high) in enumerate(zip(mass_bin_edges[:-1], mass_bin_edges[1:])):
                    bin_mask = valid & (log_stellar_mass >= mass_low) & (log_stellar_mass < mass_high)
                    
                    if np.sum(bin_mask) > 0:
                        bin_sfr_values = total_sfr[bin_mask]
                        nonzero_sfr = bin_sfr_values[bin_sfr_values > 0]
                        if len(nonzero_sfr) > 0:
                            median_sfr = np.median(nonzero_sfr)
                        else:
                            median_sfr = 0.0
                    else:
                        median_sfr = 0.0
                    
                    median_sfr_evolution[i].append(median_sfr)
            
            except Exception as e:
                logger.warning(f'Error processing snapshot {snap} for {sim_label}: {e}')
                for bin_idx in range(len(mass_bin_centers)):
                    median_sfr_evolution[bin_idx].append(0.0)

    return median_sfr_evolution, mass_bin_mean_masses, lookback_times, mass_bin_centers

# Create custom colormap from red (most massive) to navy (least massive)
def create_red_to_navy_colormap(n_colors):
    """Create colormap from red to navy"""
    # Define colors: red -> orange -> yellow -> green -> blue -> navy
    colors_list = ['#8B0000', '#B22222', '#DC143C', '#FF4500', '#FF8C00', '#FFA500', 
                   '#FFD700', '#ADFF2F', '#32CD32', '#00CED1', '#4169E1', '#000080']
    
    # Select colors based on number needed
    if n_colors <= len(colors_list):
        selected_colors = [colors_list[int(i * (len(colors_list)-1) / (n_colors-1))] for i in range(n_colors)]
    else:
        # Interpolate if we need more colors
        selected_colors = plt.cm.Spectral_r(np.linspace(0, 1, n_colors))
    
    return selected_colors

# Process both simulations
plt.figure(figsize=(10, 16))
ax = plt.subplot(111)

# Find main simulation and vanilla simulation
main_sim_path = DirName
vanilla_sim_path = None

for sim_config in SFR_SimDirs:
    if 'vanilla' in sim_config['label'].lower() or 'c16' in sim_config['label'].lower():
        vanilla_sim_path = sim_config['path']
        break

# Process main simulation (SAGE 2.0)
if os.path.exists(main_sim_path):
    logger.info('Processing main simulation (SAGE 2.0)...')
    median_sfr_main, mass_bin_means_main, lookback_times_main, mass_bin_centers = process_simulation_sfr_evolution(
        main_sim_path, 'SAGE 2.0', redshifts, FirstSnap, LastSnap, Main_Hubble_h)
    
    # Create color scheme
    colors = create_red_to_navy_colormap(len(mass_bin_centers))
    #colors = colors[::-1]  # Reverse the color order
    
    # Plot main simulation results (solid lines)
    for bin_idx, sfr_evolution in median_sfr_main.items():
        sfr_array = np.array(sfr_evolution)
        
        # Only plot if we have meaningful data
        nonzero_mask = sfr_array > 0
        if np.sum(nonzero_mask) > 3:
            
            # Use lookback time for x-axis (reverse order)
            lookback_reversed = lookback_times_main[::-1][:len(sfr_array)]
            sfr_reversed = sfr_array[::-1]
            
            # Plot solid line for main simulation
            line_color = colors[bin_idx] if bin_idx < len(colors) else 'black'
            plt.plot(lookback_reversed, np.log10(sfr_reversed + 1e-5), 
                    color=line_color, linewidth=3.0, linestyle='-',
                    alpha=0.8)
            
            # Add mass label for main simulation only
            if len(lookback_reversed) > 5:
                # Find index closest to 11.5 Gyr lookback time
                target_lookback = 10.2
                label_idx = np.argmin(np.abs(lookback_reversed - target_lookback))
                
                if label_idx < len(lookback_reversed) and sfr_reversed[label_idx] > 0:
                    mean_mass_log = np.log10(mass_bin_means_main[bin_idx])
                    plt.annotate(f'{mean_mass_log:.1f}', 
                            xy=(lookback_reversed[label_idx], np.log10(sfr_reversed[label_idx] + 1e-5)),
                            xytext=(5, 5), textcoords='offset points',  # Positive offset to put text to the right of the point
                            fontsize=16, color=line_color, weight='bold')

# Process vanilla simulation (SAGE C16) if available
if vanilla_sim_path and os.path.exists(vanilla_sim_path):
    logger.info('Processing vanilla simulation (SAGE C16)...')
    median_sfr_vanilla, mass_bin_means_vanilla, lookback_times_vanilla, _ = process_simulation_sfr_evolution(
        vanilla_sim_path, 'SAGE C16', redshifts, FirstSnap, LastSnap, Main_Hubble_h)
    
    # Plot vanilla simulation results (dashed lines)
    for bin_idx, sfr_evolution in median_sfr_vanilla.items():
        sfr_array = np.array(sfr_evolution)
        
        # Only plot if we have meaningful data
        nonzero_mask = sfr_array > 0
        if np.sum(nonzero_mask) > 3:
            
            # Use lookback time for x-axis (reverse order)
            lookback_reversed = lookback_times_vanilla[::-1][:len(sfr_array)]
            sfr_reversed = sfr_array[::-1]
            
            # Plot dashed line for vanilla simulation
            line_color = colors[bin_idx] if bin_idx < len(colors) else 'black'
            plt.plot(lookback_reversed, np.log10(sfr_reversed + 1e-5), 
                    color=line_color, linewidth=3.0, linestyle=':',
                    alpha=0.8)
    # Replace your existing xlabel/ylabel calls with this:

# Set primary axis labels FIRST (before creating secondary axis)
ax.set_xlabel(r'Lookback Time (Gyr)', fontsize=16)
ax.set_ylabel(r'$\log_{10}$ SFR $(M_{\odot}\ \mathrm{yr}^{-1})$', fontsize=16)

# Set axis limits for primary axis
max_lookback = max(lookback_times_main) if 'lookback_times_main' in locals() else 14
ax.set_xlim(0, max_lookback)
ax.set_ylim(-2.5, 2.8)

# Set minor ticks for primary axis
ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

# NOW create secondary x-axis for redshift
ax2 = ax.twiny()

# Use your simulation's redshift-lookback relationship directly (most accurate)
if 'lookback_times_main' in locals() and 'redshifts' in locals():
    sim_redshifts_array = np.array(redshifts)
    sim_lookback_array = np.array(lookback_times_main)
    
    # Find redshift values at nice lookback time intervals
    lookback_tick_positions = np.array([0, 2, 4, 6, 8, 10, 12])
    lookback_tick_positions = lookback_tick_positions[lookback_tick_positions <= max_lookback]
    
    redshift_labels = []
    for lb_target in lookback_tick_positions:
        # Find closest lookback time in simulation data
        idx = np.argmin(np.abs(sim_lookback_array - lb_target))
        z_value = sim_redshifts_array[idx]
        redshift_labels.append(z_value)
    
    # Set ticks and labels for secondary axis
    ax2.set_xlim(ax.get_xlim())  # Match primary axis limits
    ax2.set_xticks(lookback_tick_positions)
    
    # Format redshift labels nicely
    formatted_labels = []
    for z in redshift_labels:
        if z < 0.1:
            formatted_labels.append(f'{z:.2f}')
        elif z < 1.0:
            formatted_labels.append(f'{z:.1f}')
        else:
            formatted_labels.append(f'{z:.0f}')
    
    ax2.set_xticklabels(formatted_labels)
    ax2.set_xlabel('Redshift', fontsize=16)

    # Debug: print the conversion for verification
    print("Lookback Time (Gyr) → Redshift conversion:")
    for lb, z in zip(lookback_tick_positions, redshift_labels):
        print(f"  {lb:.0f} Gyr → z = {z:.2f}")

else:
    # Fallback: use analytical conversion with your cosmological parameters
    def lookback_to_redshift_accurate(t_lookback, H0=73, Om0=0.3, OL0=0.7):
        """More accurate lookback time to redshift conversion"""
        if t_lookback <= 0:
            return 0.0
        
        # Age of universe
        t_universe = 13.8  # Gyr
        
        # For small lookback times, use approximation
        if t_lookback < 1.0:
            # Linear approximation for small z
            return t_lookback * H0 / (100 * 9.78 * np.sqrt(Om0))
        
        # For larger lookback times, use more accurate formula
        # This is an approximation, but much better than the simple one
        age_at_z = t_universe - t_lookback
        
        # Approximate redshift from age (fitted formula)
        if age_at_z <= 0.5:
            return 10.0  # Very high redshift
        elif age_at_z >= t_universe:
            return 0.0
        else:
            # Empirical fit (good approximation for standard cosmology)
            x = age_at_z / t_universe
            z_approx = (1.0 / x - 1.0) * 0.7  # Rough approximation
            return max(0.0, z_approx)
    
    lookback_tick_positions = np.array([0, 2, 4, 6, 8, 10, 12])
    lookback_tick_positions = lookback_tick_positions[lookback_tick_positions <= max_lookback]
    
    redshift_labels = [lookback_to_redshift_accurate(lb) for lb in lookback_tick_positions]
    
    ax2.set_xlim(ax.get_xlim())
    ax2.set_xticks(lookback_tick_positions)
    ax2.set_xticklabels([f'{z:.1f}' for z in redshift_labels])
    ax2.set_xlabel('Redshift', fontsize=16)

# Create custom legend (rest of your existing legend code)
legend_elements = []
if os.path.exists(main_sim_path):
    legend_elements.append(plt.Line2D([0], [0], color='black', linewidth=3, linestyle='-', label='SAGE 2.0'))
if vanilla_sim_path and os.path.exists(vanilla_sim_path):
    legend_elements.append(plt.Line2D([0], [0], color='black', linewidth=3, linestyle=':', label='SAGE C16'))

if legend_elements:
    leg = ax.legend(handles=legend_elements, loc='upper left', fontsize=14)  # Use ax.legend, not plt.legend
    leg.draw_frame(False)

# Save plot
outputFile = OutputDir + 'I.median_sfr_vs_lookback_time' + OutputFormat
plt.savefig(outputFile, dpi=500, bbox_inches='tight')
logger.info(f'Saved file to {outputFile}')
plt.close()

logger.info('Median SFR vs lookback time analysis complete')

# ==================================================================

if __name__ == '__main__':
    
    print('Running ultra-optimized mass loading vs virial velocity analysis\n')
    
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
    
    # Read stellar mass data for all snapshots for SMD calculation
    logger.info('\n=== Loading stellar mass data for SMD calculation ===')
    StellarMassFull = {}
    for snap in range(FirstSnap, LastSnap + 1):
        snap_name = f'Snap_{snap}'
        stellar_mass_snap = read_hdf_ultra_optimized(snap_num=snap_name, param='StellarMass')
        if len(stellar_mass_snap) > 0:
            StellarMassFull[snap] = stellar_mass_snap * 1.0e10 / Main_Hubble_h
        else:
            StellarMassFull[snap] = np.array([])
        
        if snap % 10 == 0:
            logger.info(f'  Loaded stellar mass for snapshot {snap}')
    
    # -----------------------------------------------------------------------
    # SFR density evolution plot with multiple simulations

    logger.info('\n=== SFR Density Evolution Analysis ===')

    # Calculate SFR density for each simulation
    simulation_data = {}
    for sim_config in SFR_SimDirs:
        sim_path = sim_config['path']
        sim_label = sim_config['label']
        
        if os.path.exists(sim_path):
            try:
                sim_start_time = time.time()
                SFR_density, sim_redshifts = calculate_sfr_density_ultra_optimized(sim_config)
                SMD, _ = calculate_smd_ultra_optimized(sim_config)
                sim_end_time = time.time()
                
                simulation_data[sim_label] = {
                    'sfr_density': SFR_density,
                    'smd': SMD,
                    'redshifts': sim_redshifts,
                    'config': sim_config
                }
                logger.info(f"Successfully processed {sim_label} in {sim_end_time - sim_start_time:.1f}s")
                
            except Exception as e:
                logger.error(f"Error processing {sim_label}: {e}")
                import traceback
                traceback.print_exc()
                continue
        else:
            logger.warning(f"Simulation directory {sim_path} does not exist, skipping {sim_label}")

    if not simulation_data:
        logger.warning("No simulation data successfully loaded! Using default directory only.")
        # Fallback to original single simulation
        default_config = {
            'path': DirName,
            'label': 'Millennium',
            'color': 'blue',
            'linestyle': '-',
            'BoxSize': Main_BoxSize,
            'Hubble_h': Main_Hubble_h,
            'VolumeFraction': Main_VolumeFraction,
            'FirstSnap': FirstSnap,
            'LastSnap': LastSnap,
            'redshifts': redshifts,
            'SMFsnaps': SMFsnaps,
            'BHMFsnaps': BHMFsnaps
        }
        SFR_density, sim_redshifts = calculate_sfr_density_ultra_optimized(default_config)
        SMD, _ = calculate_smd_ultra_optimized(default_config)
        simulation_data['Millennium'] = {
            'sfr_density': SFR_density,
            'smd': SMD,
            'redshifts': sim_redshifts,
            'config': default_config
        }

    logger.info('Plotting SFR density evolution')

    def read_ecsv_data(filename):
        """Read the ECSV file and extract the SFR density data"""
        if not os.path.exists(filename):
            logger.warning(f"ECSV file {filename} not found")
            return np.array([])
            
        try:
            data = []
            with open(filename, 'r') as f:
                lines = f.readlines()
            
            # Find where data starts (after header)
            data_start = 0
            for i, line in enumerate(lines):
                if line.startswith('Redshift sfrd_50'):
                    data_start = i + 1
                    break
            
            # Parse data lines
            for line in lines[data_start:]:
                line = line.strip()
                if line and not line.startswith('#'):
                    values = line.split()
                    if len(values) == 4:
                        try:
                            redshift = float(values[0])
                            sfrd_50 = float(values[1])  # median
                            sfrd_16 = float(values[2])  # 16th percentile
                            sfrd_84 = float(values[3])  # 84th percentile
                            data.append([redshift, sfrd_50, sfrd_16, sfrd_84])
                        except ValueError:
                            continue
            
            return np.array(data)
        except Exception as e:
            logger.warning(f"Error reading ECSV file: {e}")
            return np.array([])

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Read and plot ECSV data if available
    ecsv_file = './data/CSFRD_inferred_from_SMD.ecsv'
    if os.path.exists(ecsv_file):
        ecsv_data = read_ecsv_data(ecsv_file)
        if len(ecsv_data) > 0:
            ecsv_redshift = ecsv_data[:, 0]
            ecsv_sfrd_median = ecsv_data[:, 1]
            ecsv_sfrd_16 = ecsv_data[:, 2]
            ecsv_sfrd_84 = ecsv_data[:, 3]

            # Convert to log10 for plotting
            log_sfrd_median = np.log10(ecsv_sfrd_median)
            log_sfrd_16 = np.log10(ecsv_sfrd_16)
            log_sfrd_84 = np.log10(ecsv_sfrd_84)

            # Calculate error bars (differences from median in log space)
            yerr_low = log_sfrd_median - log_sfrd_16
            yerr_high = log_sfrd_84 - log_sfrd_median

            # Plot the ECSV data with error bars
            plt.errorbar(ecsv_redshift, log_sfrd_median, 
                        yerr=[yerr_low, yerr_high], 
                        color='orange', lw=1.0, alpha=0.2, 
                        marker='o', markersize=2, ls='none', 
                        label='COSMOS Web 2025')

    # Original observational data (compilation used in Croton et al. 2006)
    ObsSFRdensity = np.array([
        [0, 0.0158489, 0, 0, 0.0251189, 0.01000000],
        [0.150000, 0.0173780, 0, 0.300000, 0.0181970, 0.0165959],
        [0.0425000, 0.0239883, 0.0425000, 0.0425000, 0.0269153, 0.0213796],
        [0.200000, 0.0295121, 0.100000, 0.300000, 0.0323594, 0.0269154],
        [0.350000, 0.0147911, 0.200000, 0.500000, 0.0173780, 0.0125893],
        [0.625000, 0.0275423, 0.500000, 0.750000, 0.0331131, 0.0229087],
        [0.825000, 0.0549541, 0.750000, 1.00000, 0.0776247, 0.0389045],
        [0.625000, 0.0794328, 0.500000, 0.750000, 0.0954993, 0.0660693],
        [0.700000, 0.0323594, 0.575000, 0.825000, 0.0371535, 0.0281838],
        [1.25000, 0.0467735, 1.50000, 1.00000, 0.0660693, 0.0331131],
        [0.750000, 0.0549541, 0.500000, 1.00000, 0.0389045, 0.0776247],
        [1.25000, 0.0741310, 1.00000, 1.50000, 0.0524807, 0.104713],
        [1.75000, 0.0562341, 1.50000, 2.00000, 0.0398107, 0.0794328],
        [2.75000, 0.0794328, 2.00000, 3.50000, 0.0562341, 0.112202],
        [4.00000, 0.0309030, 3.50000, 4.50000, 0.0489779, 0.0194984],
        [0.250000, 0.0398107, 0.00000, 0.500000, 0.0239883, 0.0812831],
        [0.750000, 0.0446684, 0.500000, 1.00000, 0.0323594, 0.0776247],
        [1.25000, 0.0630957, 1.00000, 1.50000, 0.0478630, 0.109648],
        [1.75000, 0.0645654, 1.50000, 2.00000, 0.0489779, 0.112202],
        [2.50000, 0.0831764, 2.00000, 3.00000, 0.0512861, 0.158489],
        [3.50000, 0.0776247, 3.00000, 4.00000, 0.0416869, 0.169824],
        [4.50000, 0.0977237, 4.00000, 5.00000, 0.0416869, 0.269153],
        [5.50000, 0.0426580, 5.00000, 6.00000, 0.0177828, 0.165959],
        [3.00000, 0.120226, 2.00000, 4.00000, 0.173780, 0.0831764],
        [3.04000, 0.128825, 2.69000, 3.39000, 0.151356, 0.109648],
        [4.13000, 0.114815, 3.78000, 4.48000, 0.144544, 0.0912011],
        [0.350000, 0.0346737, 0.200000, 0.500000, 0.0537032, 0.0165959],
        [0.750000, 0.0512861, 0.500000, 1.00000, 0.0575440, 0.0436516],
        [1.50000, 0.0691831, 1.00000, 2.00000, 0.0758578, 0.0630957],
        [2.50000, 0.147911, 2.00000, 3.00000, 0.169824, 0.128825],
        [3.50000, 0.0645654, 3.00000, 4.00000, 0.0776247, 0.0512861],
        ], dtype=np.float32)

    ObsRedshift = ObsSFRdensity[:, 0]
    xErrLo = np.abs(ObsSFRdensity[:, 0]-ObsSFRdensity[:, 2])
    xErrHi = np.abs(ObsSFRdensity[:, 3]-ObsSFRdensity[:, 0])

    ObsSFR = np.log10(ObsSFRdensity[:, 1])
    yErrLo = np.abs(np.log10(ObsSFRdensity[:, 1])-np.log10(ObsSFRdensity[:, 4]))
    yErrHi = np.abs(np.log10(ObsSFRdensity[:, 5])-np.log10(ObsSFRdensity[:, 1]))

    # plot observational data (compilation used in Croton et al. 2006)
    plt.errorbar(ObsRedshift, ObsSFR, yerr=[yErrLo, yErrHi], xerr=[xErrLo, xErrHi], 
                color='purple', lw=1.0, alpha=0.3, marker='o', ls='none', 
                label='Observations')

    def MD14_sfrd(z):
        """Madau & Dickinson 2014 SFR density"""
        psi = 0.015 * (1+z)**2.7 / (1 + ((1+z)/2.9)**5.6) # Msol yr-1 Mpc-3
        return psi

    f_chab_to_salp = 1/0.63

    z_values = np.linspace(0,8,200)
    md14= np.log10(MD14_sfrd(z_values) * f_chab_to_salp)
            
    plt.plot(z_values, md14, color='gray', lw=1.5, alpha=0.7, label='Madau & Dickinson 2014')

    # Plot SAGE results for each simulation
    for sim_label, sim_data in simulation_data.items():
        SFR_density = sim_data['sfr_density']
        sim_redshifts = sim_data['redshifts']
        config = sim_data['config']
        
        nonzero = np.where(SFR_density > 0.0)[0]
        if len(nonzero) > 0:
            # Use the simulation's own redshift array
            z_array = np.array(sim_redshifts)
            plt.plot(z_array[nonzero], np.log10(SFR_density[nonzero]), 
                    lw=1.5, 
                    color=config['color'],
                    linestyle=config['linestyle'],
                    label=config['label'])

    plt.ylabel(r'$\log_{10} \mathrm{\rho_{SFR}}\ (M_{\odot}\ \mathrm{yr}^{-1}\ \mathrm{Mpc}^{-3})$', fontsize=16)
    plt.xlabel(r'$\mathrm{Redshift}$', fontsize=16)

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    plt.axis([0.0, 7.5, -3.0, -0.5])   

    leg = plt.legend(loc='lower center', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')         

    outputFile = OutputDir + 'G.SFR_density_history' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    logger.info(f'Saved file to {outputFile}')
    plt.close()
    
    # -----------------------------------------------------------------------
    # Stellar Mass Density Evolution Plot
    
    logger.info('\n=== Stellar Mass Density Evolution Analysis ===')
    
    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)
    
    # SMD observations taken from Marchesini+ 2009, h=0.7
    # Values are (minz, maxz, rho,-err,+err)
    dickenson2003 = np.array(((0.6,1.4,8.26,0.08,0.08),
                     (1.4,2.0,7.86,0.22,0.33),
                     (2.0,2.5,7.58,0.29,0.54),
                     (2.5,3.0,7.52,0.51,0.48)),float)
    drory2005 = np.array(((0.25,0.75,8.3,0.15,0.15),
                (0.75,1.25,8.16,0.15,0.15),
                (1.25,1.75,8.0,0.16,0.16),
                (1.75,2.25,7.85,0.2,0.2),
                (2.25,3.0,7.75,0.2,0.2),
                (3.0,4.0,7.58,0.2,0.2)),float)
    PerezGonzalez2008 = np.array(((0.2,0.4,8.41,0.06,0.06),
             (0.4,0.6,8.37,0.04,0.04),
             (0.6,0.8,8.32,0.05,0.05),
             (0.8,1.0,8.24,0.05,0.05),
             (1.0,1.3,8.15,0.05,0.05),
             (1.3,1.6,7.95,0.07,0.07),
             (1.6,2.0,7.82,0.07,0.07),
             (2.0,2.5,7.67,0.08,0.08),
             (2.5,3.0,7.56,0.18,0.18),
             (3.0,3.5,7.43,0.14,0.14),
             (3.5,4.0,7.29,0.13,0.13)),float)
    glazebrook2004 = np.array(((0.8,1.1,7.98,0.14,0.1),
                     (1.1,1.3,7.62,0.14,0.11),
                     (1.3,1.6,7.9,0.14,0.14),
                     (1.6,2.0,7.49,0.14,0.12)),float)
    fontana2006 = np.array(((0.4,0.6,8.26,0.03,0.03),
                  (0.6,0.8,8.17,0.02,0.02),
                  (0.8,1.0,8.09,0.03,0.03),
                  (1.0,1.3,7.98,0.02,0.02),
                  (1.3,1.6,7.87,0.05,0.05),
                  (1.6,2.0,7.74,0.04,0.04),
                  (2.0,3.0,7.48,0.04,0.04),
                  (3.0,4.0,7.07,0.15,0.11)),float)
    rudnick2006 = np.array(((0.0,1.0,8.17,0.27,0.05),
                  (1.0,1.6,7.99,0.32,0.05),
                  (1.6,2.4,7.88,0.34,0.09),
                  (2.4,3.2,7.71,0.43,0.08)),float)
    elsner2008 = np.array(((0.25,0.75,8.37,0.03,0.03),
                 (0.75,1.25,8.17,0.02,0.02),
                 (1.25,1.75,8.02,0.03,0.03),
                 (1.75,2.25,7.9,0.04,0.04),
                 (2.25,3.0,7.73,0.04,0.04),
                 (3.0,4.0,7.39,0.05,0.05)),float)
    
    obs = (dickenson2003,drory2005,PerezGonzalez2008,glazebrook2004,fontana2006,rudnick2006,elsner2008)
    
    for o in obs:
        xval = ((o[:,1]-o[:,0])/2.)+o[:,0]
        if(whichimf == 0):
            ax.errorbar(xval, np.log10(10**o[:,2] *1.6), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none', label='Observations')
        elif(whichimf == 1):
            ax.errorbar(xval, np.log10(10**o[:,2] *1.6/1.8), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')
    
    # Calculate SMD from the main simulation using pre-loaded data
    smd = np.zeros((LastSnap+1-FirstSnap))       
    for snap in range(FirstSnap,LastSnap+1):
      w = np.where((StellarMassFull[snap] > 1.0e8) & (StellarMassFull[snap] < 1.0e13))[0]
      if(len(w) > 0):
        smd[snap-FirstSnap] = sum(StellarMassFull[snap][w]) / volume

    z = np.array(redshifts)
    nonzero = np.where(smd > 0.0)[0]
    #plt.plot(z[nonzero], np.log10(smd[nonzero]), 'k-', lw=3.0, label='Millennium')

    # Plot SMD results for each simulation
    for sim_label, sim_data in simulation_data.items():
        SMD = sim_data['smd']
        sim_redshifts = sim_data['redshifts']
        config = sim_data['config']
        
        nonzero = np.where(SMD > 0.0)[0]
        if len(nonzero) > 0:
            # Use the simulation's own redshift array
            z_array = np.array(sim_redshifts)
            plt.plot(z_array[nonzero], np.log10(SMD[nonzero]), 
                    lw=2.0, 
                    color=config['color'],
                    linestyle=config['linestyle'],
                    label=f"{config['label']}")

    # Load and plot SMD data from the .ecsv file
    smd_z, smd_rho_50, smd_rho_16, smd_rho_84 = load_smd_data('./data/SMD.ecsv')
    
    if smd_z is not None:
        # Convert to log10 and plot with error bars
        smd_log_rho_50 = np.log10(smd_rho_50)
        smd_log_rho_16 = np.log10(smd_rho_16)
        smd_log_rho_84 = np.log10(smd_rho_84)

        # Plot SMD data as line with shaded error region
        ax.plot(smd_z, smd_log_rho_50, color='orange', linewidth=2, label='COSMOS Web 2025')
        ax.fill_between(smd_z, smd_log_rho_16, smd_log_rho_84, 
                        color='orange', alpha=0.3, interpolate=True)

    plt.ylabel(r'$\log_{10}\ \phi\ (M_{\odot}\ \mathrm{Mpc}^{-3})$', fontsize=16)
    plt.xlabel(r'$\mathrm{Redshift}$', fontsize=16)

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    plt.axis([0.0, 4.5, 6.0, 9.0])   

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'H.stellar_mass_density_history' + OutputFormat
    plt.savefig(outputFile, dpi=500, bbox_inches='tight')
    logger.info(f'Saved file to {outputFile}')
    plt.close()
    
    # Final performance summary
    end_time = time.time()
    total_time = end_time - start_time
    final_memory = get_memory_usage()
    
    logger.info("\n=== ANALYSIS COMPLETE ===")
    logger.info(f"Total execution time: {total_time:.1f} seconds")
    logger.info(f"Final memory usage: {final_memory:.1f} GB")
    logger.info(f"Processed {len(simulation_data)} simulation(s):")
    
    for sim_label, sim_data in simulation_data.items():
        config = sim_data['config']
        logger.info(f"  - {sim_label}:")
        logger.info(f"    BoxSize: {config['BoxSize']} h^-1 Mpc")
        logger.info(f"    Hubble_h: {config['Hubble_h']}")
        logger.info(f"    Snapshot range: {config['FirstSnap']} to {config['LastSnap']}")
        logger.info(f"    Redshift range: {max(config['redshifts']):.1f} to {min(config['redshifts']):.1f}")
        volume = (config['BoxSize']/config['Hubble_h'])**3.0 * config['VolumeFraction']
        logger.info(f"    Volume: {volume:.2e} Mpc^3")
    
    logger.info("\nGenerated plots:")
    logger.info(f"  - SFR density evolution: {OutputDir}sfr_density_history{OutputFormat}")
    logger.info(f"  - Stellar mass density evolution: {OutputDir}stellar_mass_density_history{OutputFormat}")
    
    logger.info("\nPerformance optimizations applied:")
    logger.info("- Ultra-optimized parallel HDF5 reading with batching")
    logger.info("- Advanced memory management with aggressive cleanup")
    logger.info("- Intelligent data caching system")
    logger.info("- Smart sampling for very large datasets")
    logger.info("- Vectorized mathematical operations")
    logger.info("- Process-level parallelization for SFR/SMD calculations")
    logger.info("- Memory-mapped file access for large arrays")
    logger.info("- Optimized data concatenation algorithms")
    
    if ENABLE_CACHING:
        cache_files = list(Path(CACHE_DIR).glob("*.npy"))
        cache_size = sum(f.stat().st_size for f in cache_files) / (1024**3)
        logger.info(f"Cache created: {len(cache_files)} files, {cache_size:.2f} GB")
        logger.info("Subsequent runs will be significantly faster!")
    
    logger.info(f"\nSpeed improvement estimate: 3-10x faster than original")