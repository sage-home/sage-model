#!/usr/bin/env python
"""
This module defines the ``SageBinaryData`` class. This class interfaces with the
:py:class:`~Model` class to read in binary data written by **SAGE**.  The value of
:py:attr:`~Model.sage_output_format` is generally ``sage_binary`` if it is to be read
with this class.

We refer to :doc:`../user/data_class` for more information about adding your own Data
Class to ingest data.

Author: Jacob Seiler.
"""

from sage_analysis.model import Model

import numpy as np
import os

class SageBinaryData():
    """
    Class intended to inteface with the :py:class:`~Model` class to ingest the data
    written by **SAGE**. It includes methods for reading the output galaxies, setting
    cosmology etc. It is specifically written for when
    :py:attr:`~Model.sage_output_format` is ``sage_binary``.
    """

    def __init__(self, *args, **kwargs):
        """
        Instantiates the Data Class for reading in **SAGE** binary data. In particular,
        generates the ``numpy`` structured array to read the output galaxies.
        """

        self.get_galaxy_struct()


    def get_galaxy_struct(self):
        """
        Sets the ``numpy`` structured array for holding the galaxy data.
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


    def set_cosmology(self, model):
        """
        Sets the relevant cosmological values, size of the simulation box and
        number of galaxy files for a given :py:class:`~Model`.

        Parameters
        ----------

        model: :py:class:`~Model` class
            The :py:class:`~Model` we're setting the cosmology for.

        Notes
        -----

        The :py:attr:`~Model.box_size` attribute is in Mpc/h.
        """

        # Here, total_num_tree_files is the total number of tree files available to the
        # simulation. This is NOT NECESSARILY the number on which SAGE ran.
        if model.simulation == "Mini-Millennium":
            model.hubble_h = 0.73
            model.box_size = 62.5
            model.total_num_tree_files = 8

        elif model.simulation == "Millennium":
            model.hubble_h = 0.73
            model.box_size = 500
            model.total_num_tree_files = 512

        elif model.simulation == "Genesis-L500-N2160":
            model.hubble_h = 0.6751
            model.box_size = 500.00
            model.total_num_tree_files = 125

        else:
            raise ValueError("Please pick a valid simulation.")

        # Scale the volume by the number of tree files used compared to the number that
        # were used for the entire simulation.  This ensures properties scaled by volume
        # (e.g., SMF) gives sensible results.
        model.volume = pow(model.box_size, 3) * (model.num_tree_files_used / model.total_num_tree_files)


    def determine_num_gals(self, model):
        """
        Determines the number of galaxies in all files for this :py:class:`~Model`.

        Parameters
        ----------

        model: :py:class:`~Model` class
            The :py:class:`~Model` we're reading data for.
        """

        num_gals = 0

        for file_num in range(model.first_file, model.last_file+1):

            fname = "{0}_{1}".format(model.model_path, file_num)

            if not os.path.isfile(fname):
                print("File\t{0} \tdoes not exist!".format(fname))
                raise FileNotFoundError

            with open(fname, "rb") as f:
                Ntrees = np.fromfile(f, np.dtype(np.int32),1)[0]
                num_gals_file = np.fromfile(f, np.dtype(np.int32),1)[0]

                num_gals += num_gals_file

        model.num_gals_all_files = num_gals


    def read_gals(self, model, file_num, pbar=None, plot_galaxies=False, debug=False):
        """
        Reads the galaxies of a model file at snapshot specified by
        :py:attr:`~Model.snapshot`.

        Parameters
        ----------

        model: :py:class:`~Model` class
            The :py:class:`~Model` we're reading data for.

        file_num: int
            Suffix of the file we're reading.  The file name will be
            ``model``.:py:attr:`~Model.model_path`_<file_num>.

        pbar: ``tqdm`` class instance, optional
            Bar showing the progress of galaxy reading.  If ``None``, progress bar will
            not show.

        plot_galaxies: bool, optional
            If set, plots and saves the 3D distribution of galaxies for this file.

        debug: bool, optional
            If set, prints out extra useful debug information.

        Returns
        -------

        gals : ``numpy`` structured array with format given by ``get_galaxy_struct()``
            The galaxies for this file.

        Notes
        -----

        ``tqdm`` does not play nicely with printing to stdout. Hence we disable
        the ``tqdm`` progress bar if ``debug=True``.
        """

        fname = "{0}_{1}".format(model.model_path, file_num)

        # We allow the skipping of files.  If we skip, don't increment a counter.
        if not os.path.isfile(fname):
            print("File\t{0} \tdoes not exist!".format(fname))
            return None
        else:
            model.num_files += 1

        with open(fname, "rb") as f:
            # First read the header information.
            Ntrees = np.fromfile(f, np.dtype(np.int32),1)[0]
            num_gals = np.fromfile(f, np.dtype(np.int32),1)[0]
            gals_per_tree = np.fromfile(f, np.dtype((np.int32, Ntrees)), 1)

            # If there aren't any galaxies, exit here.
            if num_gals == 0:
                return None

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

            from sage_analysis.plots import plot_spatial_3d

            # Show the distribution of galaxies in 3D.
            pos = gals["Pos"][:]
            output_file = "./galaxies_{0}.{1}".format(file_num, model.plot_output_format)
            plot_spatial_3d(pos, output_file, self.box_size)

        # For the HDF5 file, some data sets have dimensions Nx1 rather than Nx3
        # (e.g., Position). To ensure the galaxy data format is identical to the binary
        # output, we will split the binary fields into Nx1. This is simpler than creating
        # a new dataset within the HDF5 regime.
        from numpy.lib import recfunctions as rfn
        multidim_fields = ["Pos", "Vel", "Spin"]
        dim_names = ["x", "y", "z"]

        for field in multidim_fields:
            for dim_num, dim_name in enumerate(dim_names):
                dim_field = "{0}{1}".format(field, dim_name)
                gals = rfn.rec_append_fields(gals, dim_field,
                                             gals[field][:, dim_num])

        return gals


    def update_snapshot(self, model, snapshot):
        """
        Updates the :py:attr:`~Model.model_path` to point to a new redshift file. Uses the
        redshift array :py:attr:`~Model.redshifts`.

        Parameters
        ----------

        snapshot: int
            Snapshot we're updating :py:attr:`~Model.model_path` to point to.
        """

        model._snapshot = snapshot

        # First get the new redshift.
        new_redshift = model.redshifts[snapshot]

        # We can't be guaranteed the `model_path` has been set just yet.
        try:
            _ = self.set_redshift
        except AttributeError:
            # model_path hasn't been set so use the full form.
            model.model_path = "{0}_z{1:.3f}".format(self.model_path, new_redshift)
        else:
            # model_path is of the form "<Initial/Path/To/File_zXXX.XXX>"
            # The number of decimal places is always 3.
            # The number of characters before "z" is arbitrary, we could be at z8.539 or z127.031.
            # Hence walk backwards through the model path until we reach a "z".
            letters_from_end = 0
            letter = self.model_path[-(letters_from_end+1)]
            while letter != "z":
                letters_from_end += 1
                letter = self.model_path[-(letters_from_end+1)]

            # Then truncate there and append the new redshift.
            model_path_prefix = self.model_path[:-(letters_from_end+1)]
            model.model_path = "{0}z{1:.3f}".format(model_path_prefix, new_redshift)
