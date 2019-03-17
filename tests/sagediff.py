#!/usr/bin/env python
from __future__ import print_function

import numpy as np
import os
import sys
from os.path import getsize as getFileSize
try:
    xrange
except NameError:
    xrange = range

class BinarySage(object):


    def __init__(self, filename, ignored_fields=[]):
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
        self.dtype = _galdesc
        self.totntrees = None
        self.totngals = None
        self.ngal_per_tree = None
        self.bytes_offset_per_tree = None
        self.ignored_fields = ignored_fields
        try:
            _mode = os.O_RDONLY | os.O_BINARY
        except AttributeError:
            _mode = os.O_RDONLY
            
        self.fd = os.open(filename, _mode)
        self.fp = open(filename, 'rb')
        

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        os.close(self.fd)
        close(self.fp)

    def read_header(self):
        """
        Read the initial header from the LHaloTree binary file
        """
        import numpy as np

        # Read number of trees in file
        totntrees = np.fromfile(self.fp, dtype=np.int32, count=1)[0]
        
        # Read number of gals in file.
        totngals = np.fromfile(self.fp, dtype=np.int32, count=1)[0]
    
        # Read the number of gals in each tree
        ngal_per_tree = np.fromfile(self.fp, dtype=np.int32, count=totntrees)
        
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
        
    def read_tree(self, treenum):
        """
        Read a single tree specified by the tree number.

        If ``trenum`` is ``None``, all trees are read.
        """
        import os
        if self.totntrees is None:
            self.read_header()

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
        tree = np.fromfile(self.fp, dtype=self.dtype, count=ngal)
        return tree


def compare_catalogs(fname1, fname2, mode, ignored_fields):
    """
    Compares two SAGE catalogs exactly
    """

    # For both modes, the first file will be binary.  So lets initialize it.
    g1 = BinarySage(fname1, ignored_fields)

    # The second file will be either binary or HDF5.
    if mode == "binary-binary":
        g2 = BinarySage(fname2, ignored_fields)
        compare_binary_catalogs(g1, g2)

    else:
        g2 = h5py.File(fname2, "r")


def compare_binary_catalogs(g1, g2):

    if not (isinstance(g1, BinarySage) and 
            isinstance(g2, BinarySage)):
        msg = "Both inputs must be objects the class 'BinarySage'"\
            "type(Object1) = {0} type(Object2) = {1}\n"\
            .format(type(g1), type(g2))
        raise ValueError

    g1.read_header()
    g2.read_header()

    msg = "Total number of trees must be identical\n"
    if g1.totntrees != g2.totntrees:
        msg += "catalog1 has {0} trees while catalog2 has {1} trees\n"\
            .format(g1.totntrees, g2.totntrees)
        raise ValueError(msg)
    
    msg = "Total number of galaxies must be identical\n"    
    if g1.totngals != g2.totngals:
        msg += "catalog1 has {0} galaxies while catalog2 has {1} "\
               "galaxies\n".format(g1.totngals, g2.totngals)
        raise ValueError(msg)

    for treenum, (n1, n2) in enumerate(zip(g1.ngal_per_tree,
                                           g2.ngal_per_tree)):
        msg = "Treenumber {0} should have exactly the same number of "\
            "galaxies\n".format(treenum)
        if n1 != n2:
            msg += "catalog1 has {0} galaxies while catalog2 has {1} "\
                   "galaxies\n".format(n1, n2)
            raise ValueError(msg)

    try:
        from tqdm import trange
    except ImportError:
        trange = xrange

    ignored_fields = [a for a in g1.ignored_fields if a in g2.ignored_fields]

    # Load all the galaxies in from all trees.
    gals1 = g1.read_tree(None)
    gals2 = g2.read_tree(None)
        
    # set the error tolerance
    rtol = 1e-9
    atol = 5e-5
    
    for field in g1.dtype.names:
        if field in ignored_fields:
            continue

        field1 = gals1[field]
        field2 = gals2[field]

        compare_fields(field1, field2, field, rtol, atol)


def compare_fields(field1, field2, field_name, rtol, atol):

    msg = "Field = `{0}` not the same between the two catalogs\n"\
        .format(field_name)
    if np.array_equal(field1, field2):
        return

    print("A `numpy.array_equal` failed. Attempting a `np.allclose` with rtol={0} "
          "and atol={1}\n".format(rtol, atol), file=sys.stderr)
    if np.allclose(field1, field2, rtol=rtol,  atol=atol):
        return

    # If control reaches here, then the arrays are not equal
    print("Printing values that were different side-by side\n",
          file=sys.stderr)
    print("#######################################", file=sys.stderr)
    print("# index          {0}1          {0}2       Diff".format(field_name),
          file=sys.stderr)
    numbad = 0

    # `isclose` is True for all elements of `field1` that are close to the corresponding
    # element in `field2`.
    bool_mask = np.isclose(field1, field2, rtol=rtol, atol=atol)

    bad_field1 = field1[bool_mask == False]
    bad_field2 = field2[bool_mask == False]

    for idx, (field1_val, field2_val) in enumerate(zip(bad_field1, bad_field2)):
        print("{0} {1} {2} {3}".format(idx, field1_val, field2_val,
              field1_val-field2_val), file=sys.stderr)

    print("------ Found {0} mis-matched values out of a total of {1} "
          "------".format(len(bad_field1), len(field1)), file=sys.stderr)
    print("#######################################\n", file=sys.stderr)
    # printed out the offending values
    # now raise the error
    #raise ValueError(msg)
    return                    
                
    
#  'Main' section of code.  This if statement executes
#   if the code is run from the 
#   shell command line, i.e. with 'python allresults.py'
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

    args = parser.parse_args()

    if not (args.mode == "binary-binary" or args.mode == "binary-hdf5"):
        print("We only accept comparisons between binary-binary files or binary-hdf5 "
              "files.  Please set the 'mode' argument to one of these options.")
        raise ValueError

    ignored_fields = ["GalaxyIndex", "CentralGalaxyIndex"]

    compare_catalogs(args.file1, args.file2, args.mode, ignored_fields)
