#!/usr/bin/env python
"""
In this module, we define the ``SageBinaryModel`` class, a subclass of the ``Model`` class
defined in the ``model.py`` module.  This subclass contains methods for setting the
simulation cosmology and reading galaxy data. If you wish to provide your own data format,
you require ``set_cosmology()``, ``determine_num_gals()`` and ``read_gals()`` methods.

Author: Jacob Seiler.
"""

from model import Model

import numpy as np

import os

class SageBinaryModel(Model):
    """
    Subclass of the ``Model`` class from the ``model.py`` module.  It contains methods
    specifically for reading in the ``sage_hdf5`` output from ``SAGE``.

    Extra Attributes
    ================

    galaxy_struct : ``numpy`` structured array
        Struct that describes how the binary file is parsed into the galaxy data.
    """

    def __init__(self, model_dict):
        """
        Initializes the super ``Model`` class and generates the array that describes the
        binary file contract.
        """

        Model.__init__(self, model_dict)
        self.get_galaxy_struct()

    def get_galaxy_struct(self):
        """
        Sets the ``numpy`` structured array for holding the galaxy data. The attribute
        ``galaxy_struct`` is updated within the function.
        """

        galdesc_full = [
            ("SnapNum"                      , np.int32),
            ("Type"                         , np.int32),
            ("GalaxyIndex"                  , np.int64),
            ("CentralGalaxyIndex"           , np.int64),
            ("SAGEHaloIndex"                , np.int32),
            ("SAGETreeIndex"                , np.int32),
            ("SimulationHaloIndex"          , np.int64),
            ("mergeType"                    , np.int32),
            ("mergeIntoID"                  , np.int32),
            ("mergeIntoSnapNum"             , np.int32),
            ("dT"                           , np.float32),
            ("Pos"                          , (np.float32, 3)),
            ("Vel"                          , (np.float32, 3)),
            ("Spin"                         , (np.float32, 3)),
            ("Len"                          , np.int32),
            ("Mvir"                         , np.float32),
            ("CentralMvir"                  , np.float32),
            ("Rvir"                         , np.float32),
            ("Vvir"                         , np.float32),
            ("Vmax"                         , np.float32),
            ("VelDisp"                      , np.float32),
            ("ColdGas"                      , np.float32),
            ("StellarMass"                  , np.float32),
            ("BulgeMass"                    , np.float32),
            ("HotGas"                       , np.float32),
            ("EjectedMass"                  , np.float32),
            ("BlackHoleMass"                , np.float32),
            ("IntraClusterStars"            , np.float32),
            ("MetalsColdGas"                , np.float32),
            ("MetalsStellarMass"            , np.float32),
            ("MetalsBulgeMass"              , np.float32),
            ("MetalsHotGas"                 , np.float32),
            ("MetalsEjectedMass"            , np.float32),
            ("MetalsIntraClusterStars"      , np.float32),
            ("SfrDisk"                      , np.float32),
            ("SfrBulge"                     , np.float32),
            ("SfrDiskZ"                     , np.float32),
            ("SfrBulgeZ"                    , np.float32),
            ("DiskRadius"                   , np.float32),
            ("Cooling"                      , np.float32),
            ("Heating"                      , np.float32),
            ("QuasarModeBHaccretionMass"    , np.float32),
            ("TimeOfLastMajorMerger"        , np.float32),
            ("TimeOfLastMinorMerger"        , np.float32),
            ("OutflowRate"                  , np.float32),
            ("infallMvir"                   , np.float32),
            ("infallVvir"                   , np.float32),
            ("infallVmax"                   , np.float32)
            ]
        names = [galdesc_full[i][0] for i in range(len(galdesc_full))]
        formats = [galdesc_full[i][1] for i in range(len(galdesc_full))]
        galdesc = np.dtype({"names":names, "formats":formats}, align=True)

        self.galaxy_struct = galdesc


    def set_cosmology(self):
        """
        Sets the relevant cosmological values, size of the simulation box and
        number of galaxy files. The ``hubble_h``, ``box_size``, ``total_num_files``
        and ``volume`` attributes are updated directly.

        ..note :: ``box_size`` is in units of Mpc/h.
        """

        if self.simulation == "Mini-Millennium":
            self.hubble_h = 0.73
            self.box_size = 62.5
            self.total_num_tree_files = 8

        elif self.simulation == "Millennium":
            self.hubble_h = 0.73
            self.box_size = 500
            self.total_num_files = 512

        elif self.simulation == "Genesis-L500-N2160":
            self.hubble_h = 0.6751
            self.box_size = 500.00
            self.total_num_files = 64 

        else:
            print("Please pick a valid simulation!")
            raise ValueError

        # Scale the volume by the number of tree files used compared to the number that
        # were used for the entire simulation.  This ensures properties scaled by volume
        # (e.g., SMF) gives sensible results.
        self.volume = pow(self.box_size, 3) * (self.num_tree_files_used / self.total_num_tree_files)


    def determine_num_gals(self):
        """
        Determines the number of galaxies in all files for this model. The ``num_gals``
        attribute is updated directly.
        """

        first_file = self.first_file
        last_file = self.last_file
        model_path = self.model_path

        num_gals = 0

        for file_num in range(first_file, last_file+1):

            fname = "{0}_{1}".format(model_path, file_num)

            if not os.path.isfile(fname):
                print("File\t{0} \tdoes not exist!".format(fname))
                raise FileNotFoundError 

            with open(fname, "rb") as f:
                Ntrees = np.fromfile(f, np.dtype(np.int32),1)[0]
                num_gals_file = np.fromfile(f, np.dtype(np.int32),1)[0]

                num_gals += num_gals_file

        self.num_gals = num_gals


    def read_gals(self, file_num, pbar=None, plot_galaxies=False, debug=False):
        """
        Reads a single galaxy file.

        Parameters 
        ----------

        file_num : Integer
            Suffix of the file we're reading.  The file name will be
            ``self.model_path_<file_num>``.

        pbar : ``tqdm`` class instance, default Off
            Bar showing the progress of galaxy reading.

        plot_galaxies : Boolean, default False
            If set, plots and saves the 3D distribution of galaxies for this file.

        debug : Boolean, default False
            If set, prints out extra useful debug information.

        ..note:: ``tqdm`` does not play nicely with printing to stdout. Hence we 
        disable the ``tqdm`` progress bar if ``debug=True``.

        Returns
        -------

        gals : ``numpy`` structured array with format given by ``get_galaxy_struct()``
            The galaxies for this file.
        """

        fname = "{0}_{1}".format(self.model_path, file_num)

        # We allow the skipping of files.  If we skip, don't increment a counter.
        if not os.path.isfile(fname):
            print("File\t{0} \tdoes not exist!".format(fname))
            return None
        else:
            self.num_files += 1

        with open(fname, "rb") as f:
            # First read the header information.
            Ntrees = np.fromfile(f, np.dtype(np.int32),1)[0]
            num_gals = np.fromfile(f, np.dtype(np.int32),1)[0]
            gals_per_tree = np.fromfile(f, np.dtype((np.int32, Ntrees)), 1)

            # Then the actual galaxies.
            gals = np.fromfile(f, self.galaxy_struct, num_gals)

            # If we're using the `tqdm` package, update the progress bar.
            if pbar:
                pbar.set_postfix(file=fname, refresh=False)
                pbar.update(num_gals)

        if debug:
            print("")
            print("File {0} contained {1} trees with {2} galaxies".format(fname, Ntrees, num_gals))

            w = np.where(gals["StellarMass"] > 1.0)[0]
            print("{0} of these galaxies have mass greater than 10^10Msun/h".format(len(w)))

        if plot_galaxies:

            # Show the distribution of galaxies in 3D.
            pos = gals["Pos"][:]
            output_file = "./galaxies_{0}{1}".format(file_num, self.output_format)
            plots.plot_spatial_3d(pos, output_file, self.box_size)

        return gals

    def update_snapshot(self, snapshot):
        """
        Updates the ``model_path`` attribute to point to the file at the redshift given by
        ``snapshot``.

        ..note :: This method must only be called if the ``redshifts`` attribute is
        defined. 

        Parameters
        ==========

        snapshot : Integer
            Snapshot we're updating the ``model_path`` string to point to.

        Returns
        =======

        None.  ``model_path`` is updated directly.
        """

        # model_path is of the form "<Initial/Path/To/File_zX.XXX>"
        # Hence need to update the last 5 characters.
        new_redshift = self.redshifts[snapshot]
        self.model_path = "{0}{1:.3f}".format(self.model_path[:-5], new_redshift)
