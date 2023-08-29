#!/usr/bin/env python
from __future__ import print_function

import os
import sys
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


def compare_catalogs(fname1, num_files_file1, fname2, num_files_file2, mode, ignored_fields, multidim_fields=None,
                     rtol=1e-9, atol=5e-5):
    """
    Compares two SAGE catalogs exactly
    """
    import h5py

    # For both modes, the first file will be binary.  So lets initialize it.
    g1 = BinarySage(fname1, ignored_fields, num_files_file1)

    # The second file will be either binary or HDF5.
    if mode == "binary-binary":
        g2 = BinarySage(fname2, ignored_fields, num_files_file2)
        compare_binary_catalogs(g1, g2, rtol, atol)
    else:
        with h5py.File(fname2, "r") as hdf5_file:
            compare_binary_hdf5_catalogs(g1, hdf5_file, multidim_fields,
                                         rtol, atol)


def determine_binary_redshift(fname):

    # We assume the file name for the binary file is of the form
    # /base/path/<ModelPrefix>_zW.XYZ.

    # First pull out the model name fully.
    model_name = fname.split("/")[-1]

    # Then get the redshift in string form.
    redshift_string = model_name.split("z")[-1]

    # Cast and return.
    redshift = float(redshift_string)

    return redshift


def compare_binary_hdf5_catalogs(g1, hdf5_file, multidim_fields, rtol=1e-9,
                                 atol=5e-5, verbose=False):

    # We need to first determine the snapshot that corresponds to the redshift we're
    # checking.  This is because the HDF5 file will contain multiple snapshots of data
    # whereas we only passed a single redshift binary file.
    binary_redshift = determine_binary_redshift(g1.filename)
    _, snap_key = determine_snap_from_binary_z(hdf5_file,
                                               binary_redshift,
                                               verbose=verbose)

    # SAGE could have been run in parallel in which the HDF5 master file will have
    # multiple core datasets.
    ncores = hdf5_file["Header"]["Misc"].attrs["num_cores"]

    # Load all the galaxies from all trees in the binary file.
    binary_gals = g1.read_tree(None)

    # Check that number of galaxies is equal.
    ngals_binary = g1.totngals
    ngals_hdf5 = determine_num_gals_at_snap(hdf5_file, ncores, snap_key)

    if ngals_binary != ngals_hdf5:
        print("The binary file had {0} galaxies whereas the HDF5 file had {1} galaxies. "
              "We determined that the binary file was at redshift {2} and that the "
              "corresponding dataset in the HDF5 file was {3}".format(ngals_binary,
              ngals_hdf5, binary_redshift, snap_key))
        raise ValueError

    # We will key via the binary file because the HDF5 file has some multidimensional
    # fields split across mutliple datasets.
    dim_names = ["x", "y", "z"]
    failed_fields = []

    for key in g1.dtype.names:

        if key in g1.ignored_fields:
            continue

        offset = 0

        # Create an array to hold all the HDF5 data.  This may need to be an Nx3 array...
        if key in multidim_fields:
            hdf5_data = np.zeros((ngals_hdf5, 3))
        else:
            hdf5_data = np.zeros((ngals_hdf5))

        # Iterate through all the core groups and slice the data into the array.
        for core_idx in range(ncores):

            core_name = "Core_{0}".format(core_idx)
            num_gals_this_file = hdf5_file[core_name][snap_key].attrs["num_gals"]

            if key in multidim_fields:
                # In the HDF5 file, the fields are named <BaseKey><x/y/z>.
                for dim_num, dim_name in enumerate(dim_names):
                    hdf5_name = "{0}{1}".format(key, dim_name)

                    data_this_file = hdf5_file[core_name][snap_key][hdf5_name][:]
                    hdf5_data[offset:offset+num_gals_this_file, dim_num] = data_this_file

            else:
                data_this_file = hdf5_file[core_name][snap_key][key][:]

                hdf5_data[offset:offset+num_gals_this_file] = data_this_file

            offset += num_gals_this_file

        binary_data = binary_gals[key]

        # The two arrays should now have the identical shape. Compare them.
        if binary_data.shape != hdf5_data.shape:
            print("For field {0}, the shape of the binary and HDF5 data were not "
                  "identical.  The binary data had a shape of {1} and the HDF5 had shape "
                  "of {2}".format(key, binary_data.shape, hdf5_data.shape))
            raise ValueError

        return_value = compare_field_equality(binary_data, hdf5_data, key, rtol, atol)

        if not return_value:
            failed_fields.append(key)

    if failed_fields:
        print("The following fields failed: {0}".format(failed_fields))
        raise ValueError


def determine_num_gals_at_snap(hdf5_file, ncores, snap_key):

    num_gals = 0

    for core_idx in range(ncores):

        core_key = "Core_{0}".format(core_idx)
        num_gals += hdf5_file[core_key][snap_key].attrs["num_gals"]

    return num_gals


def determine_snap_from_binary_z(hdf5_file, redshift, verbose=False):

    hdf5_snap_keys = []
    hdf5_redshifts = []

    # We're handling the HDF5 master file. Hence let's look at the Core_0 group because
    # it's guaranteed to always be present.

    for key in hdf5_file["Core_0"].keys():

        # We need to be careful here. We have a "Header" group that we don't
        # want to count when we're trying to work out the correct snapshot.
        if 'Snap' not in key:
            continue

        hdf5_snap_keys.append(key)
        hdf5_redshifts.append(hdf5_file["Core_0"][key].attrs["redshift"])

    # Find the snapshot that is closest to the redshift.
    z_array = np.array(hdf5_redshifts)
    idx = (np.abs(z_array - redshift)).argmin()
    snap_key = hdf5_snap_keys[idx]
    snap_num = snap_key_to_snap_num(snap_key)

    if verbose:
        print("Determined Snapshot {0} with Key {1} corresponds to the binary "
              "file at redshift {2}".format(snap_num, snap_key, redshift))

    return snap_num, snap_key


def snap_key_to_snap_num(snap_key):
    """
    Given the name of a snapshot key, finds the associated snapshot number.

    This is necessary because the 0th snapshot key may not be snapshot 000 and
    there could be missing snapshots. This function searches backwards for a
    group of digits that identify the snapshot number.  If there are numbers
    outside of this cluster they will be disregarded and a warning raised.

    For example, if the key is "Snap1_030", the function will return 30 and
    issue a warning that there were digits ignored.

    Parameters
    ----------

    snap_key: String.
        The name of the snapshot key.

    Returns
    ----------

    snapnum: Integer.
        The snapshot number that corresponds to the snapshot key.

    Examples
    ----------

    >>> snap_key_to_snapnum('Snap_018')
    18

    >>> snap_key_to_snapnum('018_Snap')
    18

    >>> snap_key_to_snapnum('Sn3p_018')
    --WARNING--
    For Snapshot key 'Sn3p_018' there were numbers that were not \
clustered together at the end of the key.
    We assume the snapshot number corresponding to this key is 18; \
please check that this is correct.
    18
    """

    snapnum = ""
    reached_numbers = None

    for letter in reversed(snap_key):  # Go backwards through the key.
        if letter.isdigit():
            if reached_numbers is False and len(snapnum):
                print("--WARNING--")
                print("For Snapshot key '{0}' there were numbers that were not"
                      " clustered together at the end of the key.\nWe assume "
                      "the snapshot number corresponding to this key is {1}; "
                      "please check that this is correct."
                      .format(snap_key, int(snapnum[::-1])))
                break
            # When a number is found, we concatenate it with the others and
            # flag that we have encountered a cluster of numbers.
            snapnum = "{0}{1}".format(snapnum, letter)
            reached_numbers = True

        else:
            # When we reach something that's not a number, turn flag off.
            reached_numbers = False

    snapnum = snapnum[::-1]  # We searched backwards so flip the string around.

    return int(snapnum)  # Cast as integer before returning.


def compare_binary_catalogs(g1, g2, rtol=1e-9, atol=5e-5):

    if not (isinstance(g1, BinarySage) and
            isinstance(g2, BinarySage)):
        msg = "Both inputs must be objects the class 'BinarySage'"\
            "type(Object1) = {0} type(Object2) = {1}\n"\
            .format(type(g1), type(g2))
        raise ValueError

    # If SAGE we run in parallel, the data is split over multiple files.  To ensure
    # correct comparisons, sum basic info (such as total number of trees/galaxies)
    # across all these files.
    g1.update_metadata()
    g2.update_metadata()

    msg = "Total number of trees must be identical\n"
    if g1.totntrees_all_files != g2.totntrees_all_files:
        msg += "catalog1 has {0} trees while catalog2 has {1} trees\n"\
            .format(g1.totntrees_all_files, g2.totntrees_all_files)
        raise ValueError(msg)

    msg = "Total number of galaxies must be identical\n"
    if g1.totngals_all_files != g2.totngals_all_files:
        msg += "catalog1 has {0} galaxies while catalog2 has {1} "\
               "galaxies\n".format(g1.totngals_all_files, g2.totngals_all_files)
        raise ValueError(msg)

    for treenum, (n1, n2) in enumerate(zip(g1.ngal_per_tree_all_files,
                                           g2.ngal_per_tree_all_files)):
        msg = "Treenumber {0} should have exactly the same number of "\
            "galaxies\n".format(treenum)
        if n1 != n2:
            msg += "catalog1 has {0} galaxies while catalog2 has {1} "\
                   "galaxies\n".format(n1, n2)
            raise ValueError(msg)

    ignored_fields = [a for a in g1.ignored_fields if a in g2.ignored_fields]
    failed_fields = []

    # Load all the galaxies in from all trees.
    gals1 = g1.read_gals()
    gals2 = g2.read_gals()

    for field in g1.dtype.names:
        if field in ignored_fields:
            continue

        field1 = gals1[field]
        field2 = gals2[field]

        return_value = compare_field_equality(field1, field2, field, rtol, atol)

        if not return_value:
            failed_fields.append(field)

    if failed_fields:
        print("The following fields failed: {0}".format(failed_fields))
        raise ValueError


def compare_field_equality(field1, field2, field_name, rtol, atol):

    if np.array_equal(field1, field2):
        return True

    if np.allclose(field1, field2, rtol=rtol,  atol=atol):
        print("`numpy.array_equal` failed for field = '{2}' "\
              "but `np.allclose` with rtol={0} and atol={1} passed"\
              .format(rtol, atol, field_name), file=sys.stderr)

        return True


    # `isclose` is True for all elements of `field1` that are close to the corresponding
    # element in `field2`.
    bool_mask = np.isclose(field1, field2, rtol=rtol, atol=atol)

    bad_field1 = field1[bool_mask == False]
    bad_field2 = field2[bool_mask == False]

    # If control reaches here, then the arrays are not equal
    print("`np.allclose` failed for field = '{0}' - "\
          "(max, min) diff = ({1}, {2}).\nNow printing values that "\
          "were different side-by side\n".format(field_name,
                                                 max(bad_field1 - bad_field2),
                                                 min(bad_field1 - bad_field2),
                                                 file=sys.stderr))
    print("#######################################", file=sys.stderr)
    print("# index          {0}1          {0}2       Diff".format(field_name),
          file=sys.stderr)
    
    for idx, (field1_val, field2_val) in enumerate(zip(bad_field1, bad_field2)):
        print("{0} {1} {2} {3}".format(idx, field1_val, field2_val,
              field1_val-field2_val), file=sys.stderr)

    # printed out the offending values
    print("------ Found {0} mis-matched values out of a total of {1} "
          "------".format(len(bad_field1), len(field1)), file=sys.stderr)
    print("#######################################\n", file=sys.stderr)

    return False


if __name__ == '__main__':

    import argparse
    description = "Show differences between two SAGE catalogs"
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("file1", metavar="FILE",
                        help="the basename for the first set of files (say, model1_z0.000)")
    parser.add_argument("file2", metavar="FILE",
                        help="the basename for the second set of files (say, model2_z0.000)")
    parser.add_argument("mode", metavar="MODE",
                        help="Either 'binary-binary' or 'binary-hdf5'.")
    parser.add_argument("num_files_file1", metavar="NUM_FILES_FILE1", type=int,
                        help="Number of files the first file was split over.")
    parser.add_argument("num_files_file2", metavar="NUM_FILES_FILE2", type=int,
                        help="Number of files the second file was split over.")

    parser.add_argument("verbose", metavar="verbose", type=bool, default=False,
                        nargs='?', help="print lots of info messages.")

    args = parser.parse_args()

    if not (args.mode == "binary-binary" or args.mode == "binary-hdf5"):
        print("We only accept comparisons between 'binary-binary' files "
              "or 'binary-hdf5' files. Please set the 'mode' argument "
              "to one of these options.")
        raise ValueError

    if args.mode == "binary-hdf5" and args.num_files_file2 > 1:
        print("You're comparing a binary with a HDF5 file but are saying "
              "the HDF5 file is split over multiple files. This shouldn't "
              "be the case; simply specify the master file and set "
              "'num_files_file2' to 1.")
        raise ValueError

    ignored_fields = []

    # Some multi-dimensional values (e.g., Position) are saved in multiple datasets for
    # the HDF5 file.
    multidim_fields = ["Pos", "Vel", "Spin"]

    # Tolerance levels for the comparisons.
    rtol = 1e-07
    atol = 1e-11

    if args.verbose:
        print("Running sagediff on files {0} and {1} in mode {2}. The first "\
              "file was split over {3} files and the second file was split "\
              "over {4} files.".format(args.file1, args.file2,
                                       args.mode, args.num_files_file1,
                                       args.num_files_file2))

    compare_catalogs(args.file1, args.num_files_file1, args.file2, args.num_files_file2,
                     args.mode, ignored_fields, multidim_fields, rtol, atol)

    print("========================")
    print("All tests passed for files {0} and {1}. Yay!".format(args.file1, args.file2))
    print("========================")
    print("")
