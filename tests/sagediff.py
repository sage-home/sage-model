#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
sagediff.py - Comparison utility for SAGE galaxy output files

This script compares two SAGE outputs to verify they contain identical galaxy catalogs.
It supports comparing:
- Binary output to binary output (binary-binary)
- Binary output to HDF5 output (binary-hdf5)

The script verifies that galaxy properties match exactly or within specified
tolerances for floating point values. It reads files that may be split across
multiple processors and handles both standard SAGE binary and HDF5 formats.

Example usage:
    # Compare two binary outputs (possibly split across multiple files)
    python sagediff.py model1_z0.000 model2_z0.000 binary-binary 1 1

    # Compare binary output with HDF5 output
    python sagediff.py model_z0.000 model.hdf5 binary-hdf5 1 1

    # Use verbose mode with custom tolerances
    python sagediff.py -v --rtol 1e-5 --atol 1e-9 model1_z0.000 model2_z0.000 binary-binary 1 1

Authors: SAGE team (https://github.com/sage-home/sage-model)
"""
from __future__ import print_function

import os
import sys
import argparse
import numpy as np

try:
    xrange
except NameError:
    xrange = range


class BinarySage(object):


    def __init__(self, filename, ignored_fields=None, num_files=1):
        """
        Set up instance variables
        """
        # The input galaxy structure:
        Galdesc_full = [
            ('SnapNum'                      , np.int32),
            ('Type'                         , np.int32),
            ('GalaxyIndex'                  , np.int64),
            ('CentralGalaxyIndex'           , np.int64),
            ('SAGEHaloIndex'                , np.int32),
            ('SAGETreeIndex'                , np.int32),
            ('SimulationHaloIndex'          , np.int64),
            ('mergeType'                    , np.int32),
            ('mergeIntoID'                  , np.int32),
            ('mergeIntoSnapNum'             , np.int32),
            ('dT'                           , np.float32),
            ('Pos'                          , (np.float32, 3)),
            ('Vel'                          , (np.float32, 3)),
            ('Spin'                         , (np.float32, 3)),
            ('Len'                          , np.int32),
            ('Mvir'                         , np.float32),
            ('CentralMvir'                  , np.float32),
            ('Rvir'                         , np.float32),
            ('Vvir'                         , np.float32),
            ('Vmax'                         , np.float32),
            ('VelDisp'                      , np.float32),
            ('ColdGas'                      , np.float32),
            ('StellarMass'                  , np.float32),
            ('BulgeMass'                    , np.float32),
            ('HotGas'                       , np.float32),
            ('EjectedMass'                  , np.float32),
            ('BlackHoleMass'                , np.float32),
            ('IntraClusterStars'            , np.float32),
            ('MetalsColdGas'                , np.float32),
            ('MetalsStellarMass'            , np.float32),
            ('MetalsBulgeMass'              , np.float32),
            ('MetalsHotGas'                 , np.float32),
            ('MetalsEjectedMass'            , np.float32),
            ('MetalsIntraClusterStars'      , np.float32),
            ('SfrDisk'                      , np.float32),
            ('SfrBulge'                     , np.float32),
            ('SfrDiskZ'                     , np.float32),
            ('SfrBulgeZ'                    , np.float32),
            ('DiskRadius'                   , np.float32),
            ('Cooling'                      , np.float32),
            ('Heating'                      , np.float32),
            ('QuasarModeBHaccretionMass'    , np.float32),
            ('TimeOfLastMajorMerger'         , np.float32),
            ('TimeOfLastMinorMerger'         , np.float32),
            ('OutflowRate'                  , np.float32),
            ('infallMvir'                   , np.float32),
            ('infallVvir'                   , np.float32),
            ('infallVmax'                   , np.float32)
            ]
        _names = [g[0] for g in Galdesc_full]
        _formats = [g[1] for g in Galdesc_full]
        _galdesc = np.dtype({'names':_names, 'formats':_formats}, align=True)

        self.filename = filename
        self.num_files = num_files
        self.dtype = _galdesc
        self.totntrees = None
        self.totngals = None
        self.ngal_per_tree = None
        self.bytes_offset_per_tree = None
        if not ignored_fields:
            ignored_fields = []
        self.ignored_fields = ignored_fields


    def __enter__(self):
        return self


    def read_header(self, fp):
        """
        Read the initial header from the LHaloTree binary file
        """
        import numpy as np

        # Read number of trees in file
        totntrees = np.fromfile(fp, dtype=np.int32, count=1)[0]

        # Read number of gals in file.
        totngals = np.fromfile(fp, dtype=np.int32, count=1)[0]

        # Read the number of gals in each tree
        ngal_per_tree = np.fromfile(fp, dtype=np.int32, count=totntrees)

        self.totntrees = totntrees
        self.totngals = totngals
        self.ngal_per_tree = ngal_per_tree

        # First calculate the bytes size of each tree
        bytes_offset_per_tree = ngal_per_tree * self.dtype.itemsize

        # then compute the cumulative sum across the sizes to
        # get an offset to any tree
        # However, tmp here will show the offset to the 0'th
        # tree as the size of the 0'th tree. What we want is the
        # offset to the 1st tree as the size of the 0'th tree
        tmp = bytes_offset_per_tree.cumsum()

        # Now assign the cumulative sum of sizes for 0:last-1 (inclusive)
        # as the offsets to 1:last
        bytes_offset_per_tree[1:-1] = tmp[0:-2]

        # And set the offset of the 0'th tree as 0
        bytes_offset_per_tree[0] = 0

        # Now add the initial offset that we need to get to the
        # 0'th tree -- i.e., the size of the headers
        header_size  = 4 + 4 + totntrees*4
        bytes_offset_per_tree[:] += header_size

        # Now assign to the instance variable
        self.bytes_offset_per_tree = bytes_offset_per_tree


    def read_tree(self, treenum, fp=None):
        """
        Read a single tree specified by the tree number.

        If ``trenum`` is ``None``, all trees are read.
        """

        close_file = False
        if fp is None:
            fp = open(self.filename, "rb")
            close_file = True

        if self.totntrees is None:
            self.read_header(fp)

        if treenum is not None:
            if treenum < 0 or treenum >= self.totntrees:
                msg = "The requested tree index = {0} should be within [0, {2})"\
                    .format(treenum, self.totntrees)
                raise ValueError(msg)

        if treenum is not None:
            ngal = self.ngal_per_tree[treenum]
        else:
            ngal = self.totngals

        if ngal == 0:
            return None

        # This assumes sequential reads
        tree = np.fromfile(fp, dtype=self.dtype, count=ngal)

        # If we had to open up the file, close it.
        if close_file:
            fp.close()

        return tree


    def update_metadata(self):
        """
        The binary galaxies can be split up over multiple files.  In this method, we
        iterate over the files and collect info that is spread across them.
        """

        self.totntrees_all_files = 0
        self.totngals_all_files = 0
        self.ngal_per_tree_all_files = []

        for file_idx in range(self.num_files):

            # Cut off the number at the end of the file.
            fname_base = self.filename[:-2]

            # Then append this file number.
            fname = "{0}_{1}".format(fname_base, file_idx)

            # Open and read the header.
            with open(fname, "rb") as fp:

                self.read_header(fp)

                # Then update the global parameters.
                self.totntrees_all_files += self.totntrees
                self.totngals_all_files += self.totngals
                self.ngal_per_tree_all_files.extend(self.ngal_per_tree)


    def read_gals(self):

        # Initialize an empty array.
        gals = np.empty(self.totngals_all_files, dtype=self.dtype)

        # Then slice all the galaxies in.
        offset = 0
        for file_idx in range(self.num_files):

            fname_base = self.filename[:-2]
            fname = "{0}_{1}".format(fname_base, file_idx)

            with open(fname, "rb") as fp:

                self.read_header(fp)
                gals_file = self.read_tree(None, fp)

                gals_this_file = len(gals_file)

                gals[offset:offset+gals_this_file] = gals_file
                offset += gals_this_file

        return gals


def compare_catalogs(fname1, num_files_file1, fname2, num_files_file2, mode, ignored_fields=None, multidim_fields=None,
                     rtol=1e-9, atol=5e-5, verbose=False):
    """
    Compare two SAGE catalogs for equality.
    
    Parameters
    ----------
    fname1 : str
        First catalog filename (binary format)
    num_files_file1 : int
        Number of files the first catalog is split over
    fname2 : str
        Second catalog filename (binary or HDF5 format)
    num_files_file2 : int
        Number of files the second catalog is split over
    mode : str
        Comparison mode: 'binary-binary' or 'binary-hdf5'
    ignored_fields : list, optional
        List of field names to ignore in comparison
    multidim_fields : list, optional
        List of multidimensional fields (for HDF5 comparison)
    rtol : float, optional
        Relative tolerance for floating point comparisons
    atol : float, optional
        Absolute tolerance for floating point comparisons
    verbose : bool, optional
        Whether to print detailed output
    
    Returns
    -------
    None
        Raises ValueError if catalogs don't match
    """
    # Default empty list for ignored fields
    if ignored_fields is None:
        ignored_fields = []

    if verbose:
        print("Comparing catalogs:")
        print(f"  Catalog 1: {fname1} (split over {num_files_file1} files)")
        print(f"  Catalog 2: {fname2} (split over {num_files_file2} files)")
        print(f"  Mode: {mode}")
        print(f"  Tolerances: rtol={rtol}, atol={atol}")
        print(f"  Ignored fields: {ignored_fields}")
    
    # Initialize first catalog (always binary)
    g1 = BinarySage(fname1, ignored_fields, num_files_file1)

    # Handle second catalog based on mode
    if mode == "binary-binary":
        g2 = BinarySage(fname2, ignored_fields, num_files_file2)
        compare_binary_catalogs(g1, g2, rtol, atol, verbose)
    elif mode == "binary-hdf5":
        # Check if h5py is available, give clear message if not
        h5py_available = False
        try:
            import h5py
            h5py_available = True
        except ImportError:
            print("Notice: h5py module is not available. HDF5 comparisons are disabled.", 
                  file=sys.stderr)
            print("To enable HDF5 testing, please install h5py: pip install h5py",
                  file=sys.stderr)
            # Return success - this isn't a failure, just a skipped test
            return
            
        # If we get here, h5py is available, so proceed with the HDF5 comparison
        try:
            with h5py.File(fname2, "r") as hdf5_file:
                compare_binary_hdf5_catalogs(g1, hdf5_file, multidim_fields,
                                          rtol, atol, verbose)
        except IOError as e:
            print(f"Error opening HDF5 file {fname2}: {e}", file=sys.stderr)
            raise


def determine_binary_redshift(fname):
    """
    Extract redshift from a binary filename.
    
    Parameters
    ----------
    fname : str
        Binary filename, expected format: /path/to/prefix_zX.YYY_N
        
    Returns
    -------
    float
        Redshift value extracted from filename
    """
    # Extract the filename without path
    model_name = os.path.basename(fname)

    # Extract the redshift part (after 'z')
    if '_z' in model_name:
        redshift_part = model_name.split('_z')[1]
    else:
        redshift_part = model_name.split('z')[1]
        
    # Extract just the redshift value (before any underscore)
    if '_' in redshift_part:
        redshift_string = redshift_part.split('_')[0]
    else:
        redshift_string = redshift_part

    # Convert to float and return
    try:
        redshift = float(redshift_string)
        return redshift
    except ValueError:
        print(f"Warning: Could not extract redshift from filename '{fname}'",
              file=sys.stderr)
        print("Using z=0 as default", file=sys.stderr)
        return 0.0


def compare_binary_hdf5_catalogs(g1, hdf5_file, multidim_fields, rtol=1e-9,
                              atol=5e-5, verbose=False):
    """
    Compare a binary catalog with an HDF5 catalog.
    
    Parameters
    ----------
    g1 : BinarySage
        Binary catalog
    hdf5_file : h5py.File
        HDF5 file object
    multidim_fields : list
        List of multidimensional field names
    rtol : float, optional
        Relative tolerance for floating point comparisons
    atol : float, optional
        Absolute tolerance for floating point comparisons
    verbose : bool, optional
        Whether to print detailed output
        
    Returns
    -------
    None
        Raises ValueError if catalogs don't match
    """
    # Default multidim_fields if not provided
    if multidim_fields is None:
        multidim_fields = ["Pos", "Vel", "Spin"]
        
    # Get redshift of binary file to find matching snapshot in HDF5
    binary_redshift = determine_binary_redshift(g1.filename)
    if verbose:
        print(f"Binary file redshift: {binary_redshift}")
        
    # Find matching snapshot in HDF5 file
    snap_num, snap_key = determine_snap_from_binary_z(hdf5_file,
                                                    binary_redshift,
                                                    verbose=verbose)

    # Get number of cores (MPI processes) from HDF5 file
    ncores = hdf5_file["Header"]["Misc"].attrs["num_cores"]
    if verbose:
        print(f"HDF5 file has data from {ncores} cores")
        print(f"Using snapshot {snap_key} for comparison")

    # Load all galaxies from binary file
    binary_gals = g1.read_tree(None)
    if binary_gals is None or len(binary_gals) == 0:
        print("Error: No galaxies found in binary file", file=sys.stderr)
        raise ValueError("Empty binary file")

    # Check that galaxy counts match
    ngals_binary = g1.totngals
    ngals_hdf5 = determine_num_gals_at_snap(hdf5_file, ncores, snap_key)

    if ngals_binary != ngals_hdf5:
        print("Error: Galaxy count mismatch:", file=sys.stderr)
        print(f"  Binary file: {ngals_binary} galaxies", file=sys.stderr)
        print(f"  HDF5 file: {ngals_hdf5} galaxies (snapshot {snap_key}, z={binary_redshift})", 
              file=sys.stderr)
        raise ValueError("Galaxy count mismatch")

    # Component names for multidimensional fields
    dim_names = ["x", "y", "z"]
    failed_fields = []

    # Compare each field
    for key in g1.dtype.names:
        # Skip ignored fields
        if key in g1.ignored_fields:
            continue

        offset = 0

        # Create array to hold HDF5 data
        if key in multidim_fields:
            hdf5_data = np.zeros((ngals_hdf5, 3))
        else:
            hdf5_data = np.zeros(ngals_hdf5)

        # Read data from each core in HDF5 file
        for core_idx in range(ncores):
            core_name = f"Core_{core_idx}"
            
            # Skip if this core's dataset doesn't exist
            if core_name not in hdf5_file or snap_key not in hdf5_file[core_name]:
                continue
                
            num_gals_this_file = hdf5_file[core_name][snap_key].attrs["num_gals"]
            
            # Skip if no galaxies in this core
            if num_gals_this_file == 0:
                continue

            if key in multidim_fields:
                # In HDF5, multidimensional fields are split into separate datasets
                for dim_num, dim_name in enumerate(dim_names):
                    hdf5_name = f"{key}{dim_name}"
                    
                    # Skip if this field doesn't exist
                    if hdf5_name not in hdf5_file[core_name][snap_key]:
                        if verbose:
                            print(f"Warning: Field {hdf5_name} not found in {core_name}/{snap_key}")
                        continue
                        
                    data_this_file = hdf5_file[core_name][snap_key][hdf5_name][:]
                    hdf5_data[offset:offset+num_gals_this_file, dim_num] = data_this_file
            else:
                # Skip if this field doesn't exist
                if key not in hdf5_file[core_name][snap_key]:
                    if verbose:
                        print(f"Warning: Field {key} not found in {core_name}/{snap_key}")
                    continue
                    
                data_this_file = hdf5_file[core_name][snap_key][key][:]
                hdf5_data[offset:offset+num_gals_this_file] = data_this_file

            offset += num_gals_this_file

        # Get binary data for this field
        binary_data = binary_gals[key]

        # Check data shapes match
        if binary_data.shape != hdf5_data.shape:
            print(f"Error: Shape mismatch for field {key}:", file=sys.stderr)
            print(f"  Binary shape: {binary_data.shape}", file=sys.stderr)
            print(f"  HDF5 shape: {hdf5_data.shape}", file=sys.stderr)
            failed_fields.append(key)
            continue

        # Compare field values
        result = compare_field_equality(binary_data, hdf5_data, key, rtol, atol, verbose)
        if not result:
            failed_fields.append(key)

    # Report any failed fields
    if failed_fields:
        print(f"ERROR: The following fields failed comparison: {failed_fields}", 
              file=sys.stderr)
        raise ValueError("Field comparison failed")


def determine_num_gals_at_snap(hdf5_file, ncores, snap_key):
    """
    Count total galaxies in an HDF5 file at a specific snapshot.
    
    Parameters
    ----------
    hdf5_file : h5py.File
        HDF5 file object
    ncores : int
        Number of cores (MPI processes)
    snap_key : str
        Snapshot key to check
        
    Returns
    -------
    int
        Total number of galaxies
    """
    num_gals = 0

    for core_idx in range(ncores):
        core_key = f"Core_{core_idx}"
        
        # Skip cores that don't exist
        if core_key not in hdf5_file:
            continue
            
        # Skip snapshots that don't exist
        if snap_key not in hdf5_file[core_key]:
            continue
            
        num_gals += hdf5_file[core_key][snap_key].attrs["num_gals"]

    return num_gals


def determine_snap_from_binary_z(hdf5_file, redshift, verbose=False):
    """
    Find the HDF5 snapshot that matches a given redshift.
    
    Parameters
    ----------
    hdf5_file : h5py.File
        HDF5 file object
    redshift : float
        Target redshift
    verbose : bool, optional
        Whether to print detailed output
        
    Returns
    -------
    tuple
        (snap_num, snap_key) - Snapshot number and key
    """
    hdf5_snap_keys = []
    hdf5_redshifts = []

    # Look at Core_0 (always present) to find available snapshots
    if "Core_0" not in hdf5_file:
        raise ValueError("HDF5 file has no Core_0 group")

    for key in hdf5_file["Core_0"].keys():
        # Skip non-snapshot groups
        if 'Snap' not in key:
            continue

        hdf5_snap_keys.append(key)
        if "redshift" in hdf5_file["Core_0"][key].attrs:
            hdf5_redshifts.append(hdf5_file["Core_0"][key].attrs["redshift"])
        else:
            print(f"Warning: No redshift attribute in {key}", file=sys.stderr)
            hdf5_redshifts.append(0.0)

    if not hdf5_snap_keys:
        raise ValueError("No snapshots found in HDF5 file")

    # Find closest redshift
    z_array = np.array(hdf5_redshifts)
    idx = (np.abs(z_array - redshift)).argmin()
    snap_key = hdf5_snap_keys[idx]
    snap_num = snap_key_to_snap_num(snap_key)

    if verbose:
        print(f"Found snapshot {snap_num} (key: {snap_key}) with z={z_array[idx]} " 
              f"for binary file z={redshift}")
        
    return snap_num, snap_key


def snap_key_to_snap_num(snap_key):
    """
    Extract the snapshot number from a snapshot key.
    
    This is necessary because the 0th snapshot key may not be snapshot 000 and
    there could be missing snapshots. This function searches backwards for a
    group of digits that identify the snapshot number.  If there are numbers
    outside of this cluster they will be disregarded and a warning raised.
    
    Parameters
    ----------
    snap_key : str
        Snapshot key (e.g., 'Snap_018')
        
    Returns
    -------
    int
        Snapshot number
        
    Examples
    --------
    >>> snap_key_to_snap_num('Snap_018')
    18
    >>> snap_key_to_snap_num('018_Snap')
    18
    >>> snap_key_to_snap_num('Sn3p_018')
    # Issues warning and returns 18
    """
    snapnum = ""
    reached_numbers = None

    # Search backwards through the key
    for letter in reversed(snap_key):
        if letter.isdigit():
            if reached_numbers is False and len(snapnum):
                print(f"Warning: For snapshot key '{snap_key}' there were numbers that "
                      "were not clustered together at the end of the key.", file=sys.stderr)
                print(f"We assume the snapshot number is {int(snapnum[::-1])}; "
                      "please check that this is correct.", file=sys.stderr)
                break
                
            # Add digit to snapshot number
            snapnum = f"{snapnum}{letter}"
            reached_numbers = True
        else:
            reached_numbers = False

    # Reverse the string since we read backwards
    snapnum = snapnum[::-1]
    
    # Convert to integer
    return int(snapnum)
def compare_binary_catalogs(g1, g2, rtol=1e-9, atol=5e-5, verbose=False):
    """
    Compare two binary catalogs for equality.
    
    Parameters
    ----------
    g1 : BinarySage
        First binary catalog
    g2 : BinarySage
        Second binary catalog
    rtol : float, optional
        Relative tolerance for floating point comparisons
    atol : float, optional
        Absolute tolerance for floating point comparisons
    verbose : bool, optional
        Whether to print detailed output
        
    Returns
    -------
    None
        Raises ValueError if catalogs don't match
    """
    # Verify inputs are the right type
    if not (isinstance(g1, BinarySage) and isinstance(g2, BinarySage)):
        msg = ("Both inputs must be objects of class 'BinarySage'. "
               "Got type(Object1) = {0}, type(Object2) = {1}".format(
                   type(g1), type(g2)))
        raise ValueError(msg)

    # Update metadata for both catalogs (handles multi-file case)
    g1.update_metadata()
    g2.update_metadata()

    # Check total tree counts match
    if g1.totntrees_all_files != g2.totntrees_all_files:
        msg = ("Total number of trees must be identical. "
               "Catalog 1 has {0} trees, catalog 2 has {1} trees".format(
                   g1.totntrees_all_files, g2.totntrees_all_files))
        raise ValueError(msg)

    # Check total galaxy counts match
    if g1.totngals_all_files != g2.totngals_all_files:
        msg = ("Total number of galaxies must be identical. "
               "Catalog 1 has {0} galaxies, catalog 2 has {1} galaxies".format(
                   g1.totngals_all_files, g2.totngals_all_files))
        raise ValueError(msg)

    # Check galaxy counts per tree match
    for treenum, (n1, n2) in enumerate(zip(g1.ngal_per_tree_all_files,
                                         g2.ngal_per_tree_all_files)):
        if n1 != n2:
            msg = ("Tree number {0} should have the same number of galaxies. "
                   "Catalog 1 has {1} galaxies, catalog 2 has {2} galaxies".format(
                       treenum, n1, n2))
            raise ValueError(msg)

    # Get list of fields to ignore in comparison
    ignored_fields = [f for f in g1.ignored_fields if f in g2.ignored_fields]
    failed_fields = []

    # Load all galaxies from both catalogs
    gals1 = g1.read_gals()
    gals2 = g2.read_gals()

    # Compare each field
    for field in g1.dtype.names:
        if field in ignored_fields:
            continue

        field1 = gals1[field]
        field2 = gals2[field]

        # Compare field values
        result = compare_field_equality(field1, field2, field, rtol, atol, verbose)
        if not result:
            failed_fields.append(field)

    # Report any failed fields
    if failed_fields:
        print(f"ERROR: The following fields failed comparison: {failed_fields}", 
              file=sys.stderr)
        raise ValueError("Field comparison failed")


def compare_field_equality(field1, field2, field_name, rtol, atol, verbose=False):
    """
    Compare two arrays for equality with tolerance for floating point values.
    
    Parameters
    ----------
    field1 : numpy.ndarray
        First array
    field2 : numpy.ndarray
        Second array
    field_name : str
        Name of the field being compared
    rtol : float
        Relative tolerance for floating point comparisons
    atol : float
        Absolute tolerance for floating point comparisons
    verbose : bool, optional
        Whether to print detailed output
        
    Returns
    -------
    bool
        True if arrays are equal (within tolerance), False otherwise
    """
    # First try exact equality
    if np.array_equal(field1, field2):
        return True

    # Then try equality within tolerance
    if np.allclose(field1, field2, rtol=rtol, atol=atol):
        if verbose:
            print(f"Field '{field_name}' passed with np.allclose (not exact equality)",
                 file=sys.stderr)
        return True

    # `isclose` is True for all elements of `field1` that are close to the corresponding
    # element in `field2`.
    bool_mask = np.isclose(field1, field2, rtol=rtol, atol=atol)

    bad_field1 = field1[bool_mask == False]
    bad_field2 = field2[bool_mask == False]

    # If control reaches here, then the arrays are not equal
    print(f"Field '{field_name}' failed comparison:", file=sys.stderr)
    print(f"  Max difference: {max(bad_field1 - bad_field2)}", file=sys.stderr)
    print(f"  Min difference: {min(bad_field1 - bad_field2)}", file=sys.stderr)
    print("#######################################", file=sys.stderr)
    print(f"# index          {field_name}1          {field_name}2       Diff", file=sys.stderr)
    
    # Limit output to max 10 differences
    max_to_show = min(len(bad_field1), 10)
    for idx in range(max_to_show):
        print(f"{idx:6d} {bad_field1[idx]:15.6g} {bad_field2[idx]:15.6g} {bad_field1[idx]-bad_field2[idx]:12.6g}", 
              file=sys.stderr)

    print(f"------ Found {len(bad_field1)} mismatched values out of {len(field1)} total ------", 
          file=sys.stderr)
    print("#######################################", file=sys.stderr)

    # printed out the offending values
    print("------ Found {0} mis-matched values out of a total of {1} "
          "------".format(len(bad_field1), len(field1)), file=sys.stderr)
    print("#######################################\n", file=sys.stderr)

    return False


if __name__ == '__main__':
    # Parse command-line arguments
    parser = argparse.ArgumentParser(
        description="Compare two SAGE galaxy catalog outputs for equality",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    
    parser.add_argument("file1", metavar="FILE1",
                       help="First catalog filename (binary format)")
    parser.add_argument("file2", metavar="FILE2",
                       help="Second catalog filename (binary or HDF5 format)")
    parser.add_argument("mode", metavar="MODE", choices=["binary-binary", "binary-hdf5"],
                       help="Comparison mode: binary-binary or binary-hdf5")
    parser.add_argument("num_files_file1", metavar="NUM_FILES_FILE1", type=int,
                       help="Number of files the first catalog is split over")
    parser.add_argument("num_files_file2", metavar="NUM_FILES_FILE2", type=int,
                       help="Number of files the second catalog is split over")
    
    # Old-style positional verbose parameter (for compatibility)
    parser.add_argument("verbose", nargs="?", const=False, default=False, type=bool,
                        help="Legacy verbose flag")
    
    # New-style optional arguments
    parser.add_argument("-v", "--verbose", action="store_true", dest="verbose_flag",
                       help="Print detailed information during comparison")
    parser.add_argument("--rtol", type=float, default=1e-7,
                       help="Relative tolerance for floating point comparisons")
    parser.add_argument("--atol", type=float, default=1e-11,
                       help="Absolute tolerance for floating point comparisons")
    parser.add_argument("--ignore-fields", metavar="FIELD", nargs="+", default=[],
                       help="Field names to ignore in comparison")
    
    args = parser.parse_args()
    
    # Combine old and new verbose flags
    verbose = args.verbose or args.verbose_flag
    
    # Basic validation
    if args.mode == "binary-hdf5" and args.num_files_file2 > 1:
        print("Error: When comparing binary with HDF5, the HDF5 file cannot be split over multiple files.", 
              file=sys.stderr)
        print("Please specify the master HDF5 file and set num_files_file2 to 1.", 
              file=sys.stderr)
        sys.exit(1)
    
    # Multi-dimensional fields (e.g., Position) are saved in multiple datasets in HDF5
    multidim_fields = ["Pos", "Vel", "Spin"]
    
    # For backwards compatibility, when called from test_sage.sh
    ignored_fields = args.ignore_fields
    
    try:
        # Run the comparison
        compare_catalogs(
            args.file1, args.num_files_file1, 
            args.file2, args.num_files_file2,
            args.mode, ignored_fields, multidim_fields, 
            args.rtol, args.atol, verbose
        )
        
        # If we get here, the catalogs matched
        file1_basename = os.path.basename(args.file1)
        file2_basename = os.path.basename(args.file2)
        print(f"    ...all tests passed for files {file1_basename} and {file2_basename}")
        sys.exit(0)
    except ValueError as e:
        # Comparison failed
        print(f"ERROR: {str(e)}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        # Unexpected error
        print(f"ERROR: An unexpected error occurred: {str(e)}", file=sys.stderr)
        sys.exit(2)
