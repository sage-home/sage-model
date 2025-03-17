#!/usr/bin/env python

"""
SAGE Plotting Tool - Master plotting script for SAGE galaxy formation model output

This script provides a centralized plotting facility for SAGE galaxy formation model
output data. It supports reading binary output files and generating various plots 
of galaxy properties and evolution.

Usage:
  python sage-plot.py --param-file=<param_file> [options]

Options:
  --param-file=<file>    SAGE parameter file (required)
  --first-file=<num>     First file to read [default: 0]
  --last-file=<num>      Last file to read [default: use MaxFileNum from param file]
  --snapshot=<num>       Process only this snapshot number
  --all-snapshots        Process all available snapshots
  --evolution            Generate evolution plots
  --snapshot-plots       Generate snapshot plots [default]
  --output-dir=<dir>     Output directory for plots [default: ./plots]
  --format=<format>      Output format (.png, .pdf) [default: .png]
  --plots=<list>         Comma-separated list of plots to generate
                         [default: all available plots]
  --use-tex              Use LaTeX for text rendering (not recommended)
  --verbose              Show detailed output
  --help                 Show this help message
"""

import argparse
import glob
import importlib
import os
import random
import re
import sys
import time
import traceback  # For detailed error tracing
from collections import OrderedDict

import matplotlib
import matplotlib.pyplot as plt
import numpy as np
from tqdm import tqdm
import h5py  # For HDF5 file reading

random.seed(42)

# Import figure modules
from figures import *

# Import the SnapshotRedshiftMapper
from snapshot_redshift_mapper import SnapshotRedshiftMapper


# Galaxy data structure definition
def get_galaxy_dtype():
    """
    Return the NumPy dtype for SAGE galaxy data.
    
    This function defines the exact structure of galaxy records in SAGE binary files.
    The structure must match the C structure used to write the files.
    Fields must be in the correct order and with the correct data types.
    
    Returns:
        numpy.dtype: The dtype specification for reading galaxy data
    """
    # Define the galaxy structure according to SAGE binary format specification
    galdesc_full = [
        ("SnapNum", np.int32),           # Snapshot number
        ("Type", np.int32),              # Galaxy type (0: central, 1: satellite)
        ("GalaxyIndex", np.int64),       # Unique identifier for this galaxy
        ("CentralGalaxyIndex", np.int64),# Index of the central galaxy in the same halo
        ("SAGEHaloIndex", np.int32),     # Halo index in SAGE
        ("SAGETreeIndex", np.int32),     # Merger tree index in SAGE
        ("SimulationHaloIndex", np.int64),# Halo index in the original simulation
        ("mergeType", np.int32),         # Type of merger (if any)
        ("mergeIntoID", np.int32),       # ID of galaxy this one merged into
        ("mergeIntoSnapNum", np.int32),  # Snapshot when the merger occurred
        ("dT", np.float32),              # Time step
        ("Pos", (np.float32, 3)),        # 3D position coordinates (x,y,z) in cMpc/h
        ("Vel", (np.float32, 3)),        # 3D velocity components in km/s
        ("Spin", (np.float32, 3)),       # 3D angular momentum vector
        ("Len", np.int32),               # Number of particles in the halo
        ("Mvir", np.float32),            # Virial mass (10^10 Msun/h)
        ("CentralMvir", np.float32),     # Virial mass of the central galaxy (10^10 Msun/h)
        ("Rvir", np.float32),            # Virial radius (cMpc/h)
        ("Vvir", np.float32),            # Virial velocity (km/s)
        ("Vmax", np.float32),            # Maximum circular velocity (km/s)
        ("VelDisp", np.float32),         # Velocity dispersion (km/s)
        ("ColdGas", np.float32),         # Cold gas mass (10^10 Msun/h)
        ("StellarMass", np.float32),     # Stellar mass (10^10 Msun/h)
        ("BulgeMass", np.float32),       # Bulge mass (10^10 Msun/h)
        ("HotGas", np.float32),          # Hot gas mass (10^10 Msun/h)
        ("EjectedMass", np.float32),     # Mass of ejected gas (10^10 Msun/h)
        ("BlackHoleMass", np.float32),   # Black hole mass (10^10 Msun/h)
        ("IntraClusterStars", np.float32),# Intracluster stellar mass (10^10 Msun/h)
        ("MetalsColdGas", np.float32),    # Metals in cold gas (10^10 Msun/h)
        ("MetalsStellarMass", np.float32),# Metals in stars (10^10 Msun/h)
        ("MetalsBulgeMass", np.float32),  # Metals in bulge (10^10 Msun/h)
        ("MetalsHotGas", np.float32),     # Metals in hot gas (10^10 Msun/h)
        ("MetalsEjectedMass", np.float32),# Metals in ejected gas (10^10 Msun/h)
        ("MetalsIntraClusterStars", np.float32),# Metals in intracluster stars (10^10 Msun/h)
        ("SfrDisk", np.float32),          # Star formation rate in disk (Msun/yr)
        ("SfrBulge", np.float32),         # Star formation rate in bulge (Msun/yr)
        ("SfrDiskZ", np.float32),         # Star formation rate metallicity in disk (Msun/yr)
        ("SfrBulgeZ", np.float32),        # Star formation rate metallicity in bulge (Msun/yr)
        ("DiskRadius", np.float32),       # Disk radius (cMpc/h)
        ("Cooling", np.float32),          # Cooling rate (erg/s)
        ("Heating", np.float32),          # Heating rate (erg/s)
        ("QuasarModeBHaccretionMass", np.float32),# Black hole accretion mass in quasar mode (10^10 Msun/h)
        ("TimeOfLastMajorMerger", np.float32),    # Time of last major merger (Gyr)
        ("TimeOfLastMinorMerger", np.float32),    # Time of last minor merger (Gyr)
        ("OutflowRate", np.float32),              # Rate of outflow (Msun/yr)
        ("infallMvir", np.float32),               # Virial mass at infall (10^10 Msun/h)
        ("infallVvir", np.float32),               # Virial velocity at infall (km/s)
        ("infallVmax", np.float32),               # Maximum circular velocity at infall (km/s)
    ]
    
    # Create lists of names and formats
    names = [galdesc_full[i][0] for i in range(len(galdesc_full))]
    formats = [galdesc_full[i][1] for i in range(len(galdesc_full))]
    
    # Create the dtype with alignment
    return np.dtype({"names": names, "formats": formats}, align=True)


class SAGEParameters:
    """Class to parse and store SAGE parameter file settings."""

    def __init__(self, param_file):
        """Initialize with parameter file path."""
        self.param_file = param_file
        self.params = {}
        self.parse_param_file()

    def parse_param_file(self):
        """Parse the SAGE parameter file."""
        if not os.path.exists(self.param_file):
            raise FileNotFoundError(f"Parameter file not found: {self.param_file}")
            
        # Set default OutputFormat to sage_hdf5
        self.params["OutputFormat"] = "sage_hdf5"

        # Check for snapshot output list line
        output_snapshots = []

        # Parse the parameter file
        with open(self.param_file, "r") as f:
            for line in f:
                # Skip empty lines and full comment lines
                if (
                    line.strip() == ""
                    or line.strip().startswith("#")
                    or line.strip().startswith("%")
                ):
                    continue

                # Check for arrow notation for snapshots (e.g., "-> 63 37 32 27 23 20 18 16")
                if "->" in line:
                    snapshot_list = line.split("->")[1].strip().split()
                    output_snapshots = [int(snap) for snap in snapshot_list]
                    self.params["OutputSnapshots"] = output_snapshots
                    continue

                # Parse key-value pairs
                if "=" in line:
                    # Standard equals-separated key-value
                    parts = line.split("=")
                    key = parts[0].strip()
                    value_part = parts[1].strip()
                else:
                    # Handle space-separated key-value pairs (common in parameter files)
                    parts = line.split(None, 1)  # Split on whitespace, max 1 split
                    if len(parts) >= 2:
                        key = parts[0].strip()
                        value_part = parts[1].strip()
                    else:
                        continue  # Skip lines that don't match our format

                # Handle inline comments
                if ";" in value_part:
                    value = value_part.split(";")[0].strip()
                elif "#" in value_part:
                    value = value_part.split("#")[0].strip()
                elif "%" in value_part:
                    value = value_part.split("%")[0].strip()
                else:
                    value = value_part

                # Clean the value - especially important for paths
                value = value.strip()

                # Convert to appropriate type
                if value.isdigit():
                    value = int(value)
                elif self._is_float(value):
                    value = float(value)
                elif key in ["OutputDir", "SimulationDir"]:
                    # Ensure directory paths are properly formatted
                    value = value.strip('"').strip("'")
                    # Make sure directory paths have a trailing slash
                    if value and not value.endswith("/"):
                        value = value + "/"
                elif key in ["FileWithSnapList"]:
                    # Ensure file paths are properly formatted
                    value = value.strip('"').strip("'")
                    # Don't add trailing slash to file paths

                self.params[key] = value

        # Validate OutputFormat if present
        if "OutputFormat" in self.params:
            output_format = self.params["OutputFormat"]
            if output_format not in ["sage_binary", "sage_hdf5"]:
                print(f"Warning: Invalid OutputFormat '{output_format}', must be 'sage_binary' or 'sage_hdf5'")
                print("Defaulting to 'sage_hdf5'")
                self.params["OutputFormat"] = "sage_hdf5"

    def _is_float(self, value):
        """Check if a string can be converted to float."""
        try:
            float(value)
            return True
        except ValueError:
            return False

    def get(self, key, default=None):
        """Get a parameter value."""
        return self.params.get(key, default)

    def __getitem__(self, key):
        """Allow dictionary-like access to parameters."""
        return self.params[key]

    def __contains__(self, key):
        """Check if a parameter exists."""
        return key in self.params


def setup_matplotlib(use_tex=False):
    """Set up matplotlib with standard settings."""
    matplotlib.rcdefaults()
    plt.rc("xtick", labelsize="x-large")
    plt.rc("ytick", labelsize="x-large")
    plt.rc("lines", linewidth="2.0")
    plt.rc("legend", numpoints=1, fontsize="x-large")

    # Only use LaTeX if explicitly requested
    if use_tex:
        try:
            plt.rc("text", usetex=True)
            print("LaTeX rendering enabled for text")
        except Exception as e:
            print(f"Warning: Could not enable LaTeX: {e}")
            # Fall back to regular text rendering
            plt.rc("text", usetex=False)
    else:
        # Explicitly disable LaTeX
        plt.rc("text", usetex=False)

    # Set up nice math rendering even without LaTeX
    plt.rcParams["mathtext.fontset"] = "dejavusans"
    plt.rcParams["mathtext.default"] = "regular"


def read_galaxies(model_path, first_file, last_file, params=None, snapshot_number=None, verbose=False):
    """
    Read galaxy data from SAGE output files, either binary or HDF5 format.
    
    This is a router function that calls the appropriate reader based on the OutputFormat parameter.
    
    Args:
        model_path: Path to model files (base path, without file number)
                   Example: "/path/to/model_z0.000" for binary or "/path/to/model" for HDF5
        first_file: First file number to read (inclusive)
        last_file: Last file number to read (inclusive)
        params: Dictionary with SAGE parameters
                Must contain at least Hubble_h and BoxSize
        snapshot_number: Optional snapshot number (needed for HDF5 evolution plots)
        verbose: Whether to print verbose output

    Returns:
        Tuple containing:
            - Numpy recarray of galaxy data
            - Volume of the simulation in (Mpc/h)^3
            - Dictionary of metadata with keys:
              - hubble_h: Hubble constant
              - box_size: Simulation box size
              - volume: Effective volume analyzed
              - ntrees: Total number of trees read (binary only)
              - ngals: Total number of galaxies read
              - good_files: Number of files successfully read (binary only)
              - snapshot: Snapshot number (if provided)
    """
    # This is important information, so show regardless of verbose flag
    print(f"Reading galaxy data from {model_path}")

    # Get required parameters from the parameter file
    if not params:
        print("Error: Parameter dictionary is required.")
        sys.exit(1)
        
    # Ensure required parameters exist
    required_params = ["Hubble_h", "BoxSize"]
    missing_params = [p for p in required_params if p not in params]
    if missing_params:
        print(f"Error: Required parameters missing from parameter file: {', '.join(missing_params)}")
        sys.exit(1)
    
    # Get the OutputFormat parameter (default to sage_hdf5 if not specified)
    output_format = params.get("OutputFormat", "sage_hdf5")
    
    # Get verbose setting from global args if available
    if 'args' in globals() and hasattr(args, 'verbose'):
        verbose = args.verbose
    
    # Use the appropriate reader based on the OutputFormat
    if output_format == "sage_binary":
        if verbose:
            print("Using binary file reader for SAGE data")
        return read_galaxies_binary(model_path, first_file, last_file, params, verbose)
    elif output_format == "sage_hdf5":
        if verbose:
            print("Using HDF5 file reader for SAGE data")
        return read_galaxies_hdf5(model_path, first_file, last_file, params, snapshot_number, verbose)
    else:
        print(f"Error: Invalid OutputFormat '{output_format}', must be 'sage_binary' or 'sage_hdf5'")
        print("Defaulting to 'sage_hdf5'")
        return read_galaxies_hdf5(model_path, first_file, last_file, params, snapshot_number, verbose)


def read_galaxies_binary(model_path, first_file, last_file, params=None, verbose=False):
    """
    Read galaxy data from SAGE binary output files.
    
    This function defines the exact structure of galaxy records in SAGE binary files.
    The structure must match the C structure used to write the files.
    Fields must be in the correct order and with the correct data types.
    
    Args:
        model_path: Path to model files (base path, without file number)
                   Example: "/path/to/model_z0.000"
        first_file: First file number to read (inclusive)
        last_file: Last file number to read (inclusive)
        params: Dictionary with SAGE parameters
                Must contain at least Hubble_h and BoxSize

    Returns:
        Tuple containing:
            - Numpy recarray of galaxy data
            - Volume of the simulation in (Mpc/h)^3
            - Dictionary of metadata with keys:
              - hubble_h: Hubble constant
              - box_size: Simulation box size
              - volume: Effective volume analyzed
              - ntrees: Total number of trees read
              - ngals: Total number of galaxies read
              - good_files: Number of files successfully read
    """
    # This is important information, so show regardless of verbose flag
    print(f"Reading galaxy data from {model_path}")

    # Get required parameters from the parameter file
    if not params:
        print("Error: Parameter dictionary is required.")
        sys.exit(1)
        
    # Ensure required parameters exist
    required_params = ["Hubble_h", "BoxSize"]
    missing_params = [p for p in required_params if p not in params]
    if missing_params:
        print(f"Error: Required parameters missing from parameter file: {', '.join(missing_params)}")
        sys.exit(1)
        
    # Ensure parameters are numeric types
    try:
        hubble_h = float(params["Hubble_h"])
        box_size = float(params["BoxSize"])
    except (ValueError, TypeError) as e:
        print(f"Error: Parameter conversion failed: {e}")
        print(f"  Hubble_h = {params['Hubble_h']} (type: {type(params['Hubble_h'])})")
        print(f"  BoxSize = {params['BoxSize']} (type: {type(params['BoxSize'])})")
        sys.exit(1)
    
    # SAGE output uses a single file with suffix "_0" for each redshift
    if args.verbose:
        print(f"Processing galaxies from virtual files {first_file} to {last_file}")
    
    # Check if the model_path is correct
    if os.path.isfile(model_path):
        print(f"Error: model_path should be a base path without file number, not a file: {model_path}")
        print("Correct format should be like: /path/to/model_z0.000")
        sys.exit(1)
    
    # Ensure model_path doesn't already end with a file number
    if re.search(r'_\d+$', model_path):
        print(f"Warning: model_path appears to end with a file number: {model_path}")
        print("This may cause issues with file identification.")
    
    # Add the file number suffix - always use file 0 for binary format
    fname = f"{model_path}_0"
    
    # Check if the file exists
    if os.path.isfile(fname):
        if args.verbose:
            print(f"Found file: {fname}")
    else:
        print(f"Error: File not found: {fname}")
        print(f"Expected file with suffix '_0'")
        sys.exit(1)

    # Get the galaxy data dtype
    galdesc = get_galaxy_dtype()

    # Initialize variables
    tot_ntrees = 0
    tot_ngals = 0
    good_files = 0
    
    # First pass: Count galaxies
    if args.verbose:
        print("Counting galaxies in file...")
    
    try:
        with open(fname, "rb") as fin:
            # Read header information
            ntrees_data = np.fromfile(fin, np.dtype(np.int32), 1)
            if len(ntrees_data) == 0:
                print(f"Error: Could not read number of trees from file {fname}")
                sys.exit(1)
            ntrees = ntrees_data[0]
            
            ngals_data = np.fromfile(fin, np.dtype(np.int32), 1)
            if len(ngals_data) == 0:
                print(f"Error: Could not read number of galaxies from file {fname}")
                sys.exit(1)
            ntotgals = ngals_data[0]
            
            # Read the tree array
            tree_array = np.fromfile(fin, np.dtype(np.int32), ntrees)
            if len(tree_array) != ntrees:
                print(f"Error: Expected {ntrees} trees, but read {len(tree_array)} from file {fname}")
                sys.exit(1)
            
            # Validate tree array sum
            tree_sum = np.sum(tree_array)
            if tree_sum != ntotgals:
                print(f"Warning: Sum of tree array ({tree_sum}) doesn't match num_gals ({ntotgals}) in file {fname}")
            
            # Update totals
            tot_ntrees += ntrees
            tot_ngals += ntotgals
            good_files += 1
            
            if args.verbose:
                header_size = 4 + 4 + (ntrees * 4)
                print(f"  {fname}: {ntrees} trees, {ntotgals} galaxies, header_size={header_size} bytes")
    except Exception as e:
        print(f"Error reading header from file {fname}: {e}")
        if args.verbose:
            print(f"Exception details: {traceback.format_exc()}")
        sys.exit(1)

    print(f"Input files contain: {tot_ntrees} trees, {tot_ngals} galaxies.")

    # Check if we found any galaxies
    if tot_ngals == 0:
        print("Error: No galaxies found in the model file.")
        print(f"Please check that the file exists and is not empty.")
        sys.exit(1)

    # Initialize the storage array
    if args.verbose:
        print(f"Allocating memory for {tot_ngals} galaxies...")
    try:
        galaxies = np.empty(tot_ngals, dtype=galdesc)
    except MemoryError:
        print(f"Error: Not enough memory to allocate array for {tot_ngals} galaxies.")
        print(f"Each galaxy requires {galdesc.itemsize} bytes, total memory needed: {tot_ngals * galdesc.itemsize / (1024**2):.1f} MB")
        print("Try reducing the number of files read or processing in batches.")
        sys.exit(1)

    # Second pass: Read the galaxy data
    offset = 0
    
    try:
        with open(fname, "rb") as fin:
            # Read header again
            ntrees = np.fromfile(fin, np.dtype(np.int32), 1)[0]
            ntotgals = np.fromfile(fin, np.dtype(np.int32), 1)[0]
            tree_array = np.fromfile(fin, np.dtype(np.int32), ntrees)
            
            # Calculate header size
            header_size = 4 + 4 + (ntrees * 4)
            
            # Position file pointer at the start of galaxy data
            fin.seek(header_size)
            
            if args.verbose:
                print(f"Reading {ntotgals} galaxies from file: {fname}")
                print(f"  Header size: {header_size} bytes")
            
            # Read galaxy data
            gg = np.fromfile(fin, galdesc, ntotgals)
            
            # Check if we read the expected number
            num_galaxies_read = len(gg)
            if num_galaxies_read != ntotgals:
                print(f"Warning: Expected to read {ntotgals} galaxies but got {num_galaxies_read}")
                ntotgals = num_galaxies_read
            
            # Copy data to main array
            if ntotgals > 0:
                if offset + ntotgals <= len(galaxies):
                    galaxies[offset:offset + ntotgals] = gg[0:ntotgals]
                    offset += ntotgals
                    if args.verbose:
                        print(f"  Successfully copied {ntotgals} galaxies to main array")
                else:
                    print(f"Error: Too many galaxies to fit in allocated array ({offset}+{ntotgals} > {len(galaxies)})")
                    remaining = len(galaxies) - offset
                    if remaining > 0:
                        print(f"  Copying only {remaining} galaxies instead")
                        galaxies[offset:] = gg[0:remaining]
    except Exception as e:
        print(f"Error reading galaxy data from file {fname}: {e}")
        if args.verbose:
            print(f"Exception details: {traceback.format_exc()}")
        sys.exit(1)

    # Convert to recarray for attribute access
    galaxies = galaxies.view(np.recarray)

    # Calculate the volume based on the box size cubed
    volume = box_size**3.0

    # Calculate volume fraction based on virtual file range
    if "NumSimulationTreeFiles" in params:
        num_files_processed = last_file - first_file + 1
        total_files = int(params["NumSimulationTreeFiles"])
        
        if total_files > 0 and num_files_processed > 0:
            volume_fraction = num_files_processed / total_files
            volume = volume * volume_fraction
            
            if verbose:
                print(f"Adjusted volume for file range {first_file}-{last_file} out of {total_files} total files")
                print(f"  Volume fraction: {num_files_processed}/{total_files} = {volume_fraction:.4f}")
                print(f"  Adjusted volume: {volume:.2f} (Mpc/h)³")
    else:
        # Missing parameters - show a warning
        if verbose:
            print("Warning: Unable to adjust volume - missing NumSimulationTreeFiles parameter")
            print("  Using unadjusted volume - results may be incorrect")

    # Create metadata dictionary
    metadata = {
        "hubble_h": hubble_h,
        "box_size": box_size,
        "volume": volume,
        "ntrees": tot_ntrees,
        "ngals": tot_ngals,
        "good_files": good_files,
    }

    return galaxies, volume, metadata


def parse_arguments():
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description="SAGE Plotting Tool")
    parser.add_argument("--param-file", required=True, help="SAGE parameter file (required)")
    parser.add_argument("--first-file", type=int, help="First file to read (overrides parameter file)")
    parser.add_argument("--last-file", type=int, help="Last file to read (overrides parameter file)")
    parser.add_argument(
        "--snapshot", type=int, help="Process only this snapshot number (overrides parameter file)"
    )
    parser.add_argument(
        "--all-snapshots", action="store_true", help="Process all available snapshots"
    )
    parser.add_argument(
        "--evolution", action="store_true", help="Generate evolution plots"
    )
    parser.add_argument(
        "--snapshot-plots", action="store_true", help="Generate snapshot plots"
    )
    parser.add_argument(
        "--output-dir",
        help="DEPRECATED - Output directory is always <OutputDir>/plots",
    )
    parser.add_argument("--format", default=".png", help="Output format (.png, .pdf)")
    parser.add_argument(
        "--plots", help="Comma-separated list of plots to generate (default: all available plots)"
    )
    parser.add_argument(
        "--use-tex",
        action="store_true",
        help="Use LaTeX for text rendering (not recommended)",
    )
    parser.add_argument("--verbose", action="store_true", help="Show detailed output")

    args = parser.parse_args()

    # Default to snapshot plots if neither is specified
    if not args.evolution and not args.snapshot_plots:
        args.snapshot_plots = True
        
    # Default to all plots if not specified
    if args.plots is None:
        args.plots = "all"

    return args


def get_available_plot_modules(plot_type):
    """
    Get available plot modules of a specific type.

    Args:
        plot_type: 'snapshot' or 'evolution'

    Returns:
        Dictionary mapping plot names to their modules
    """
    modules = {}

    # Get the module patterns from the figures module
    if plot_type == "snapshot":
        module_patterns = SNAPSHOT_PLOTS
    else:  # 'evolution'
        module_patterns = EVOLUTION_PLOTS

    # Import modules
    for pattern in module_patterns:
        try:
            module_name = f"figures.{pattern}"
            module = importlib.import_module(module_name)
            # Get the main plotting function from the module
            plot_func = getattr(module, "plot", None)
            if plot_func:
                modules[pattern] = plot_func
        except (ImportError, AttributeError) as e:
            if args.verbose:
                print(f"Warning: Could not import {module_name}: {e}")

    return modules


def main():
    """Main execution function."""
    global args
    args = parse_arguments()

    # Parse the parameter file
    try:
        params = SAGEParameters(args.param_file)
        if args.verbose:
            print(f"Loaded parameters from {args.param_file}")

            # Print important parameters
            important_params = [
                "OutputDir",
                "FileNameGalaxies",
                "LastSnapShotNr",
                "FirstFile",
                "LastFile",
                "Hubble_h",
                "WhichIMF",
            ]
            for key in important_params:
                if key in params.params:
                    print(f"  {key} = {params.params[key]}")

        # Print raw params for debugging
        if args.verbose:
            print("\nRaw parameter values:")
            for key, value in params.params.items():
                print(f"  {key} = {value} (type: {type(value)})")
    except Exception as e:
        print(f"Error loading parameter file: {e}")
        sys.exit(1)

    # Verify all required parameters exist
    required_params = [
        "OutputDir", 
        "FileNameGalaxies", 
        "FirstFile", 
        "LastFile",
        "BoxSize",
        "Hubble_h",
        "FileWithSnapList",
        "NumSimulationTreeFiles"
    ]
    
    missing_params = []
    for param in required_params:
        if param not in params.params:
            missing_params.append(param)
    
    if missing_params:
        print(f"Error: Required parameters missing from parameter file: {', '.join(missing_params)}")
        sys.exit(1)
    
    # Verify paths from parameter file exist
    output_dir = params["OutputDir"]
    simulation_dir = params.get("SimulationDir")  # May not be used directly
    file_name_galaxies = params["FileNameGalaxies"]
    file_with_snap_list = params["FileWithSnapList"]

    if args.verbose:
        print(f"Parameter file details:")
        print(f"  OutputDir: {output_dir}")
        print(f"  SimulationDir: {simulation_dir if simulation_dir else 'Not specified'}")
        print(f"  FileNameGalaxies: {file_name_galaxies}")
        print(f"  FirstFile: {params['FirstFile']}")
        print(f"  LastFile: {params['LastFile']}")
        print(f"  FileWithSnapList: {file_with_snap_list}")
        print(f"  BoxSize: {params['BoxSize']}")
        print(f"  Hubble_h: {params['Hubble_h']}")

    # Check if OutputDir exists
    if not os.path.exists(output_dir):
        print(f"Error: OutputDir '{output_dir}' from parameter file does not exist.")
        sys.exit(1)
        
    # Check if FileWithSnapList exists
    file_with_snap_list = file_with_snap_list.strip().strip("'").strip('"')
    if not os.path.isabs(file_with_snap_list) and simulation_dir:
        # Try relative to simulation directory if it's not absolute
        full_path = os.path.join(simulation_dir, file_with_snap_list)
        if os.path.exists(full_path):
            file_with_snap_list = full_path
    
    if not os.path.exists(file_with_snap_list):
        print(f"Error: FileWithSnapList '{file_with_snap_list}' not found.")
        print("Please verify the path is correct in the parameter file.")
        sys.exit(1)

    # Set up matplotlib
    setup_matplotlib(args.use_tex)

    # Get output directory from parameter file - required parameter
    if "OutputDir" not in params:
        print("Error: OutputDir parameter is required in the parameter file.")
        sys.exit(1)
    
    # Get and clean the output directory path
    model_output_dir = params["OutputDir"]
    model_output_dir = model_output_dir.strip().strip("'").strip('"')
    model_output_dir = os.path.expanduser(model_output_dir)
    
    if args.verbose:
        print(f"\nOutput directory handling:")
        print(f"  model_output_dir from params: '{model_output_dir}'")
    
    # Check if output directory exists
    if not os.path.exists(model_output_dir):
        print(f"Error: OutputDir '{model_output_dir}' specified in parameter file does not exist.")
        sys.exit(1)
    
    # Check if output directory is writable
    if not os.access(model_output_dir, os.W_OK):
        print(f"Error: OutputDir '{model_output_dir}' specified in parameter file is not writable.")
        sys.exit(1)
    
    # Set the plots directory as a subdirectory of the output directory
    output_dir = os.path.join(model_output_dir, "plots")
    
    if args.verbose:
        print(f"  Using output directory: '{output_dir}'")
    
    # Create the plots directory if it doesn't exist
    try:
        os.makedirs(output_dir, exist_ok=True)
        if args.verbose:
            print(f"  Successfully created/verified output directory: {output_dir}")
    except Exception as e:
        print(f"Error: Could not create output directory {output_dir}: {e}")
        sys.exit(1)
    
    # If output_dir argument was provided, inform user we're ignoring it
    if args.output_dir:
        print(f"Note: Ignoring --output-dir argument. Using directory from parameter file: {output_dir}")

    # Determine which plots to generate
    if args.plots == "all":
        selected_plots = None  # All available
    else:
        selected_plots = [p.strip() for p in args.plots.split(",")]

        # Check if any evolution plots are specifically requested but --evolution flag is not set
        requested_evolution_plots = [p for p in selected_plots if p in EVOLUTION_PLOTS]
        if requested_evolution_plots and not args.evolution:
            print(
                f"Warning: Evolution plots requested ({', '.join(requested_evolution_plots)}) but --evolution flag not set."
            )
            print(
                f"These plots require data from multiple snapshots to work correctly."
            )
            print(f"Adding --evolution flag automatically.")
            args.evolution = True

    # Generate snapshot plots
    if args.snapshot_plots:
        # Get required parameters for finding model files
        if "OutputDir" not in params:
            print("Error: OutputDir parameter is required in the parameter file.")
            sys.exit(1)
            
        if "FileNameGalaxies" not in params:
            print("Error: FileNameGalaxies parameter is required in the parameter file.")
            sys.exit(1)
            
        # Get output model path and snapshot number
        model_path = params["OutputDir"]
        snapshot = args.snapshot or params.get("LastSnapShotNr")
        
        if not snapshot:
            print("Error: LastSnapShotNr not found in parameter file and no snapshot specified.")
            sys.exit(1)
        
        # Clean up the path
        model_path = model_path.strip().strip("'").strip('"')
        model_path = os.path.expanduser(model_path)
        
        # File name from parameter file
        file_name_galaxies = params["FileNameGalaxies"]
        
        if args.verbose:
            print(f"\nModel file discovery:")
            print(f"  model_path from params: '{model_path}'")
            print(f"  file_name_galaxies: '{file_name_galaxies}'")
            print(f"  Using snapshot: {snapshot}")
        
        # Check if model_path exists
        if not os.path.exists(model_path):
            print(f"Error: OutputDir '{model_path}' from parameter file does not exist.")
            sys.exit(1)

        # Get the redshift for this snapshot using the mapper
        mapper = SnapshotRedshiftMapper(args.param_file, params.params, model_path)
        output_format = params.params.get("OutputFormat", "sage_hdf5")
        redshift_str = mapper.get_redshift_str(snapshot)
        
        if args.verbose:
            print(f"  Redshift string for snapshot {snapshot}: {redshift_str}")
        
        # Construct the base model file path (the actual path is determined in read_galaxies_hdf5 for HDF5 format)
        base_model_file = mapper.get_model_file_path(snapshot, 0, output_format)
        
        if args.verbose:
            print(f"  Using model file base: {base_model_file}")

        # Required parameters check
        required_params = ["FirstFile", "LastFile"]
        missing_params = [p for p in required_params if p not in params.params]
        if missing_params:
            print(f"Error: Required parameters missing from parameter file: {', '.join(missing_params)}")
            sys.exit(1)
            
        # Get first and last file numbers, prioritizing command-line arguments
        if args.first_file is not None:
            first_file = args.first_file
            if args.verbose:
                print(f"Using first_file={first_file} from command-line argument")
        else:
            first_file = params["FirstFile"]
            if args.verbose:
                print(f"Using first_file={first_file} from parameter file")
                
        if args.last_file is not None:
            last_file = args.last_file
            if args.verbose:
                print(f"Using last_file={last_file} from command-line argument")
        else:
            last_file = params["LastFile"]
            if args.verbose:
                print(f"Using last_file={last_file} from parameter file")
                
        # Validate file range
        if first_file > last_file:
            print(f"Error: FirstFile ({first_file}) is greater than LastFile ({last_file})")
            sys.exit(1)

        # Read galaxy data
        try:
            galaxies, volume, metadata = read_galaxies(
                model_path=base_model_file,
                first_file=first_file,
                last_file=last_file,
                params=params.params,
                snapshot_number=snapshot
            )
            if args.verbose:
                if metadata.get("sample_data", False):
                    print(
                        "USING SAMPLE DATA: No real galaxy data was found, using generated test data instead."
                    )
                    print(
                        "This is useful for testing the plotting code but the plots will not reflect real SAGE output."
                    )
                print(
                    f"Read {len(galaxies)} galaxies from volume {volume:.2f} (Mpc/h)³"
                )
        except Exception as e:
            print(f"Error reading galaxy data: {e}")
            if args.verbose:
                print(f"Exception details: {traceback.format_exc()}")
            sys.exit(1)

        # Get available snapshot plot modules
        plot_modules = get_available_plot_modules("snapshot")

        if args.verbose:
            print(f"Available snapshot plots: {', '.join(plot_modules.keys())}")

        # Filter to selected plots if specified
        if selected_plots:
            plot_modules = {
                k: v for k, v in plot_modules.items() if k in selected_plots
            }

        # Generate each plot
        generated_plots = []
        for plot_name, plot_func in plot_modules.items():
            try:
                if args.verbose:
                    print(f"Generating {plot_name}...")
                plot_path = plot_func(
                    galaxies=galaxies,
                    volume=volume,
                    metadata=metadata,
                    params=params.params,
                    output_dir=output_dir,
                    output_format=args.format,
                    verbose=args.verbose,
                )
                generated_plots.append(plot_path)
                print(f"Generated: {plot_path}")
            except Exception as e:
                print(f"Error generating {plot_name}: {e}")

        if args.verbose:
            print(f"Generated {len(generated_plots)} snapshot plots.")

    # Generate evolution plots
    if args.evolution:
        # Get available evolution plot modules
        plot_modules = get_available_plot_modules("evolution")

        if args.verbose:
            print(f"Available evolution plots: {', '.join(plot_modules.keys())}")

        # Filter to selected plots if specified
        if selected_plots:
            plot_modules = {
                k: v for k, v in plot_modules.items() if k in selected_plots
            }

        # Create a snapshot-to-redshift mapper
        mapper = SnapshotRedshiftMapper(
            args.param_file, params.params, model_output_dir
        )
        if args.verbose:
            print(mapper.debug_info())

        # Create the mapper from parameter file
        mapper = SnapshotRedshiftMapper(args.param_file, params.params, params["OutputDir"])
        # Get the output format
        output_format = params.params.get("OutputFormat", "sage_hdf5")
        
        # Determine which snapshots to process
        if args.all_snapshots:
            # Process all available snapshots
            snapshots = mapper.get_all_snapshots()
            if args.verbose:
                print(f"Using all {len(snapshots)} available snapshots")
        elif args.snapshot:
            # Process only the specified snapshot
            # Verify this snapshot exists in our mapping
            if args.snapshot not in mapper.snapshots:
                print(f"Error: Specified snapshot {args.snapshot} not found in redshift mapping")
                print(f"Available snapshots: {mapper.snapshots}")
                sys.exit(1)
            
            snapshots = [args.snapshot]
            if args.verbose:
                print(f"Using single snapshot: {args.snapshot}")
        else:
            # Use the evolution snapshots determined by the mapper
            # This will prioritize OutputSnapshots from parameter file
            snapshots = mapper.get_evolution_snapshots()
            
            # Check that we have at least 2 snapshots for a meaningful evolution plot
            if len(snapshots) < 2:
                print("Error: At least 2 snapshots are required for evolution plots")
                print(f"Available snapshots: {snapshots}")
                sys.exit(1)
                
            # Check for diverse redshift coverage
            redshifts = [mapper.get_redshift(snap) for snap in snapshots]
            min_z = min(redshifts)
            max_z = max(redshifts)
            
            if args.verbose:
                print(f"Using {len(snapshots)} snapshots for evolution plots")
                print(f"Redshift range: z={min_z:.3f} to z={max_z:.3f}")

        if args.verbose:
            print(f"Selected snapshots for evolution plots: {snapshots}")
            print(
                f"Corresponding redshifts: {[mapper.get_redshift(snap) if snap >= 0 else 0.0 for snap in snapshots]}"
            )

        # Read galaxy data for each snapshot
        snapshot_data = {}
        # Only show progress bar when verbose is enabled
        snapshot_iterator = (
            tqdm(snapshots, desc="Loading snapshot data for evolution plots")
            if args.verbose
            else snapshots
        )
        for snap in snapshot_iterator:
            # Special case for -1 snap code that was used for sample z=0 data
            if snap == -1:
                print("Error: Sample data generation is no longer supported.")
                print("Please ensure all necessary data files exist.")
                sys.exit(1)

            # Regular case - read actual snapshot data
            # Get redshift and model file path from mapper
            redshift = mapper.get_redshift(snap)
            model_file_base = mapper.get_model_file_path(snap, 0, output_format)

            if args.verbose:
                print(f"Processing snapshot {snap} (z={redshift:.3f})")
                print(f"Using model file pattern: {model_file_base}")

            # Required parameters check
            required_params = ["FirstFile", "LastFile", "NumSimulationTreeFiles"]
            missing_params = [p for p in required_params if p not in params.params]
            if missing_params:
                print(f"Error: Required parameters missing from parameter file: {', '.join(missing_params)}")
                sys.exit(1)
                
            # Get first and last file numbers, prioritizing command-line arguments
            if args.first_file is not None:
                first_file = args.first_file
                if args.verbose:
                    print(f"Using first_file={first_file} from command-line argument")
            else:
                first_file = params["FirstFile"]
                if args.verbose:
                    print(f"Using first_file={first_file} from parameter file")
                    
            if args.last_file is not None:
                last_file = args.last_file
                if args.verbose:
                    print(f"Using last_file={last_file} from command-line argument")
            else:
                last_file = params["LastFile"]
                if args.verbose:
                    print(f"Using last_file={last_file} from parameter file")
                    
            # Validate file range
            if first_file > last_file:
                print(f"Error: FirstFile ({first_file}) is greater than LastFile ({last_file})")
                sys.exit(1)

            try:
                # For HDF5 files, pass the snapshot number to read_galaxies
                galaxies, volume, metadata = read_galaxies(
                    model_path=model_file_base,
                    first_file=first_file,
                    last_file=last_file,
                    params=params.params,
                    snapshot_number=snap
                )
                # Add redshift to metadata
                metadata["redshift"] = redshift
                snapshot_data[snap] = (galaxies, volume, metadata)
                if args.verbose:
                    print(f"  Read {len(galaxies)} galaxies at z={redshift:.2f}")
            except Exception as e:
                print(f"Error reading snapshot {snap}: {e}")
                if args.verbose:
                    print(f"Exception details: {traceback.format_exc()}")

        # Generate each evolution plot
        generated_plots = []
        for plot_name, plot_func in plot_modules.items():
            try:
                if args.verbose:
                    print(f"Generating {plot_name}...")
                plot_path = plot_func(
                    snapshots=snapshot_data,
                    params=params.params,
                    output_dir=output_dir,
                    output_format=args.format,
                    verbose=args.verbose,
                )
                generated_plots.append(plot_path)
                print(f"Generated: {plot_path}")
            except Exception as e:
                print(f"Error generating {plot_name}: {e}")

        if args.verbose:
            print(f"Generated {len(generated_plots)} evolution plots.")


def read_galaxies_hdf5(model_path, first_file, last_file, params=None, snapshot_number=None, verbose=False):
    """
    Read galaxy data from SAGE HDF5 output files.
    
    This function reads galaxy data from HDF5 files which have a different structure
    than the binary files. It handles both master files and core files.
    
    Args:
        model_path: Path to model files (base path, without file number)
                   Example: "/path/to/model"
        first_file: First file number to read (inclusive)
        last_file: Last file number to read (inclusive)
        params: Dictionary with SAGE parameters
                Must contain at least Hubble_h and BoxSize
        snapshot_number: Snapshot number to read from the HDF5 file.
                        If None, will try to determine from parameter file.
        verbose: Whether to print verbose output

    Returns:
        Tuple containing:
            - Numpy recarray of galaxy data
            - Volume of the simulation in (Mpc/h)^3
            - Dictionary of metadata with keys:
              - hubble_h: Hubble constant
              - box_size: Simulation box size
              - volume: Effective volume analyzed
              - ngals: Total number of galaxies read
              - snapshot: Snapshot number
    """
    # Ensure parameters are numeric types
    try:
        hubble_h = float(params["Hubble_h"])
        box_size = float(params["BoxSize"])
    except (ValueError, TypeError) as e:
        print(f"Error: Parameter conversion failed: {e}")
        print(f"  Hubble_h = {params['Hubble_h']} (type: {type(params['Hubble_h'])})")
        print(f"  BoxSize = {params['BoxSize']} (type: {type(params['BoxSize'])})")
        sys.exit(1)
    
    # Determine which snapshot to use
    # Priority: 1. Provided snapshot_number, 2. LastSnapShotNr from params
    if snapshot_number is not None:
        snapshot = snapshot_number
    elif "LastSnapShotNr" in params:
        snapshot = params["LastSnapShotNr"]
    else:
        print("Warning: No snapshot number provided or found in parameters. Using default snapshot 63.")
        snapshot = 63  # Default to snapshot 63 (typically z=0)
    
    if verbose:
        print(f"Using snapshot number: {snapshot}")
    
    # Construct the HDF5 file path
    # HDF5 files always use the pattern: model_0.hdf5 (always file 0)
    # All galaxies from FirstFile to LastFile are stored in this single file
    fname = f"{model_path}_0.hdf5"
    
    # Check if the file exists
    if os.path.isfile(fname):
        if verbose:
            print(f"Found HDF5 file: {fname}")
    else:
        print(f"Error: HDF5 file not found: {fname}")
        print(f"SAGE HDF5 output is always written to a single file with suffix '_0.hdf5'")
        print(f"This single file contains data for the range specified by FirstFile and LastFile")
        sys.exit(1)
    
    # Get the galaxy data dtype
    galdesc = get_galaxy_dtype()
    
    # Read the HDF5 file
    try:
        with h5py.File(fname, "r") as f:
            # Access the snapshot data
            snap_key = f"Snap_{snapshot}"
            
            # Check if the snapshot exists
            if snap_key in f:
                snap_group = f[snap_key]
            else:
                print(f"Error: Snapshot {snapshot} not found in the HDF5 file")
                print(f"Available groups: {list(f.keys())}")
                sys.exit(1)
            
            # Get the list of galaxy properties available in this snapshot
            properties = list(snap_group.keys())
            
            # Get the number of galaxies in this snapshot
            for key in properties:
                if isinstance(snap_group[key], h5py.Dataset):
                    num_gals = snap_group[key].shape[0]
                    break
            else:
                print(f"Error: Could not determine number of galaxies in snapshot {snapshot}")
                sys.exit(1)
            
            print(f"Found {num_gals} galaxies in snapshot {snapshot}")
            
            # Initialize the storage array
            try:
                galaxies = np.empty(num_gals, dtype=galdesc)
            except MemoryError:
                print(f"Error: Not enough memory to allocate array for {num_gals} galaxies")
                print(f"Each galaxy requires {galdesc.itemsize} bytes, total memory needed: {num_gals * galdesc.itemsize / (1024**2):.1f} MB")
                sys.exit(1)
            
            # Read all regular properties
            for field_name in galdesc.names:
                # Skip vector properties which are stored as separate components
                if field_name in ["Pos", "Vel", "Spin"]:
                    continue
                
                if field_name in properties:
                    # Direct read if property exists
                    galaxies[field_name] = snap_group[field_name][:]
                elif field_name + "x" in properties and field_name + "y" in properties and field_name + "z" in properties:
                    # Skip - these will be handled in vector processing
                    pass
                else:
                    # Initialize with zeros if property doesn't exist
                    galaxies[field_name].fill(0)
                    if verbose:
                        print(f"Property {field_name} not found in HDF5 file, initializing with zeros")
            
            # Handle vector properties which are stored as components
            # Position
            if "Posx" in properties and "Posy" in properties and "Posz" in properties:
                posx = snap_group["Posx"][:]
                posy = snap_group["Posy"][:]
                posz = snap_group["Posz"][:]
                for i in range(num_gals):
                    galaxies["Pos"][i] = (posx[i], posy[i], posz[i])
            
            # Velocity
            if "Velx" in properties and "Vely" in properties and "Velz" in properties:
                velx = snap_group["Velx"][:]
                vely = snap_group["Vely"][:]
                velz = snap_group["Velz"][:]
                for i in range(num_gals):
                    galaxies["Vel"][i] = (velx[i], vely[i], velz[i])
            
            # Spin
            if "Spinx" in properties and "Spiny" in properties and "Spinz" in properties:
                spinx = snap_group["Spinx"][:]
                spiny = snap_group["Spiny"][:]
                spinz = snap_group["Spinz"][:]
                for i in range(num_gals):
                    galaxies["Spin"][i] = (spinx[i], spiny[i], spinz[i])
    
    except Exception as e:
        print(f"Error reading HDF5 file {fname}: {e}")
        if verbose:
            print(f"Exception details: {traceback.format_exc()}")
        sys.exit(1)
    
    # Convert to recarray for attribute access
    galaxies = galaxies.view(np.recarray)
    
    # Calculate the volume based on the box size cubed
    volume = box_size**3.0

    # Calculate volume fraction based on virtual file range
    if "NumSimulationTreeFiles" in params:
        num_files_processed = last_file - first_file + 1
        total_files = int(params["NumSimulationTreeFiles"])
        
        if total_files > 0 and num_files_processed > 0:
            volume_fraction = num_files_processed / total_files
            volume = volume * volume_fraction
            
            if verbose:
                print(f"Adjusted volume for file range {first_file}-{last_file} out of {total_files} total files")
                print(f"  Volume fraction: {num_files_processed}/{total_files} = {volume_fraction:.4f}")
                print(f"  Adjusted volume: {volume:.2f} (Mpc/h)³")
    else:
        # Missing parameters - show a warning
        if verbose:
            print("Warning: Unable to adjust volume - missing NumSimulationTreeFiles parameter")
            print("  Using unadjusted volume - results may be incorrect")
    
    # Create metadata dictionary
    metadata = {
        "hubble_h": hubble_h,
        "box_size": box_size,
        "volume": volume,
        "ngals": num_gals,
        "snapshot": snapshot,
    }
    
    return galaxies, volume, metadata


if __name__ == "__main__":
    main()
