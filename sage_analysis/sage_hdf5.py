#!/usr/bin/env python
"""
This module defines the ``SageHdf5Data`` class. This class interfaces with the
:py:class:`~Model` class to read in binary data written by **SAGE**.  The value of
:py:attr:`~Model.sage_output_format` is generally ``sage_hdf5`` if it is to be read
with this class.

We refer to :doc:`../user/data_class` for more information about adding your own Data
Class to ingest data.

Author: Jacob Seiler.
"""

import numpy as np
import h5py

## DO NOT TOUCH ##
sage_data_version = "1.00"
## DO NOT TOUCH ##

class SageHdf5Data():
    """
    Class intended to inteface with the :py:class:`~Model` class to ingest the data
    written by **SAGE**. It includes methods for reading the output galaxies, setting
    cosmology etc. It is specifically written for when
    :py:attr:`~Model.sage_output_format` is ``sage_hdf5``.
    """

    def __init__(self, model, *args, **kwargs):
        """
        Instantiates the Data Class for reading in **SAGE** HDF5 data. In particular,
        opens up the file and ensures the data version matches the expected value.

        Parameters
        ----------

        model: :py:class:`~Model` class
            The corresponding :py:class:`~Model` we're reading the **SAGE** data for.
        """

        model.hdf5_file = h5py.File(model.model_path, "r")

        # Due to how attributes are created in C, they will need to be decoded to get cast
        # to a string.
        model.sage_version = model.hdf5_file["Header"]["Misc"].attrs["sage_version"].decode("ascii")
        model.sage_data_version = model.hdf5_file["Header"]["Misc"].attrs["sage_data_version"].decode("ascii")

        # Check that this module is current for the SAGE data version.
        if model.sage_data_version != sage_data_version:
            msg = "The 'sage_hdf5.py' module was written to be compatible with " \
                  "sage_data_version {0}.  Your version of SAGE HDF5 has data version " \
                  "{1}. Please update your version of SAGE or submit an issue at " \
                  "https://github.com/sage-home/sage-model/issues".format(sage_data_version, \
                  model.sage_data_version)
            raise ValueError(msg)

        # For the HDF5 file, "first_file" and "last_file" correspond to the "Core_%d"
        # groups.
        model.first_file = 0
        model.last_file = model.hdf5_file["Header"]["Misc"].attrs["num_cores"] - 1
        model.num_files = model.hdf5_file["Header"]["Misc"].attrs["num_cores"]


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

        f = model.hdf5_file

        model.hubble_h = f["Header"]["Simulation"].attrs["hubble_h"]
        model.box_size = f["Header"]["Simulation"].attrs["box_size"]

        # Each core may have processed a different fraction of the volume. Hence find out
        # total volume processed.
        volume_processed = 0.0
        for core_num in range(model.first_file, model.last_file + 1):
            core_key = "Core_{0}".format(core_num)

            processed_this_core = f[core_key]["Header"]["Runtime"].attrs["frac_volume_processed"]
            volume_processed += processed_this_core

        # Scale the volume by the number of files that we will read. Used to ensure
        # properties scaled by volume (e.g., SMF) gives sensible results.
        model.volume = pow(model.box_size, 3) * volume_processed


    def determine_num_gals(self, model):
        """
        Determines the number of galaxies in all cores for this model at the specified
        :py:attr:`~Model.snapshot`.

        Parameters
        ----------

        model: :py:class:`~Model` class
            The :py:class:`~Model` we're reading data for.
        """

        ngals = 0
        snap_key = "Snap_{0}".format(model.snapshot)

        for core_idx in range(model.first_file, model.last_file + 1):

            core_key = "Core_{0}".format(core_idx)
            ngals += model.hdf5_file[core_key][snap_key].attrs["num_gals"]

        model.num_gals_all_files = ngals


    def read_gals(self, model, core_num, pbar=None, plot_galaxies=False, debug=False):
        """
        Reads the galaxies of a single core at the specified :py:attr:`~Model.snapshot`.

        Parameters
        ----------

        model: :py:class:`~Model` class
            The :py:class:`~Model` we're reading data for.

        core_num: Integer
            The core group we're reading.

        pbar: ``tqdm`` class instance, optional
            Bar showing the progress of galaxy reading.  If ``None``, progress bar will
            not show.

        plot_galaxies : Boolean, optional
            If set, plots and saves the 3D distribution of galaxies for this file.

        debug : Boolean, optional
            If set, prints out extra useful debug information.

        Returns
        -------

        gals : ``h5py`` group
            The galaxies for this file.

        Notes
        -----

        ``tqdm`` does not play nicely with printing to stdout. Hence we disable
        the ``tqdm`` progress bar if ``debug=True``.
        """

        core_key = "Core_{0}".format(core_num)
        snap_key = "Snap_{0}".format(model.snapshot)

        num_gals_read = model.hdf5_file[core_key][snap_key].attrs["num_gals"]

        # If there aren't any galaxies, exit here.
        if num_gals_read == 0:
            return None

        gals = model.hdf5_file[core_key][snap_key]

        # If we're using the `tqdm` package, update the progress bar.
        if pbar:
            pbar.set_postfix(file=core_key, refresh=False)
            pbar.update(num_gals_read)

        if debug:
            print("")
            print("Core {0}, Snapshot {1} contained {2} galaxies".format(core_num,
                                                                         model.snapshot,
                                                                         num_gals_read))

            w = np.where(gals["StellarMass"][:] > 1.0)[0]
            print("{0} of these galaxies have mass greater than 10^10Msun/h".format(len(w)))

        if plot_galaxies:
            # Show the distribution of galaxies in 3D.
            from sage_analysis.plots import plot_spatial_3d

            pos = np.empty((len(gals["Posx"]), 3), dtype=np.float32)

            dim_name = ["x", "y", "z"]
            for (dim_num, dim_name) in enumerate(dim_name):
                key = "Pos{0}".format(dim_num)
                pos[:, dim_num] = gals[key][:]

            output_file = "./galaxies_{0}.{1}".format(core_num, model.plot_output_format)
            plot_spatial_3d(pos, output_file, model.box_size)

        return gals


    def update_snapshot(self, model, snapshot):
        """
        Updates the :py:attr:`~Model.snapshot` attribute to ``snapshot``.
        """
        model._snapshot = snapshot


    def close_file(self, model):
        """
        Closes the open HDF5 file.
        """

        model.hdf5_file.close()
