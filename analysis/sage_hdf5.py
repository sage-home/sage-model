#!/usr/bin/env python
"""
In this module, we define the ``SageHdf5Model`` class, a subclass of the ``Model`` class
defined in the ``model.py`` module.  This subclass contains methods for setting the
simulation cosmology and reading galaxy data. If you wish to provide your own data format,
you require ``set_cosmology()``, ``determine_num_gals()`` and ``read_gals()`` methods.

Author: Jacob Seiler.
"""

# Import the base class.
from model import Model

import numpy as np
import h5py

import os

## DO NOT TOUCH ##
sage_data_version = "1.00"
## DO NOT TOUCH ##

class SageHdf5Model(Model):
    """
    Subclass of the ``Model`` class from the ``model.py`` module.  It contains methods
    specifically for reading in the ``sage_hdf5`` output from ``SAGE``.

    Extra Attributes
    ================

    hdf5_file : Open ``h5py`` file
        The HDF5 file we are reading from.  This is closed by ``close_file()`` in
        ``model.py`` after all galaxies have been read and their properties calculated.
    """

    def __init__(self, model_dict, plot_toggles):
        """
        Initializes the super ``Model`` class and opens the HDF5 file.
        """

        Model.__init__(self, model_dict, plot_toggles)
        self.hdf5_file = h5py.File(self.model_path, "r")

        self.sage_version = self.hdf5_file["Header"]["Misc"].attrs["sage_version"]
        self.sage_data_version = self.hdf5_file["Header"]["Misc"].attrs["data_version"]

        # Check that this module is current for the SAGE data version.
        if self.sage_data_version != sage_data_version:
            msg = "The 'sage_hdf5.py' module was written to be compatible with " \
                  "sage_data_version {0}.  Your version of SAGE HDF5 has data version " \
                  "{1}. Please update your version of SAGE or submit an issue at " \
                  "https://github.com/sage-home/sage-model/issues".format(sage_data_version, \
                  self.sage_data_version)
            raise ValueError(msg)

        # For the HDF5 file, "first_file" and "last_file" correspond to the "Core_%d"
        # groups.
        self.first_file = 0
        self.last_file = self.hdf5_file["Header"]["Misc"].attrs["num_cores"] - 1
        self.num_files = self.hdf5_file["Header"]["Misc"].attrs["num_cores"]


    def set_cosmology(self):
        """
        Sets the relevant cosmological values, size of the simulation box and
        number of galaxy files. The ``hubble_h``, ``box_size``, ``total_num_files``
        and ``volume`` attributes are updated directly.

        ..note :: ``box_size`` is in units of Mpc/h.
        """

        f = self.hdf5_file

        self.hubble_h = f["Header"]["Simulation"].attrs["hubble_h"]
        self.box_size = f["Header"]["Simulation"].attrs["box_size"]

        # Each core may have processed a different fraction of the volume. Hence find out
        # total volume processed.
        volume_processed = 0.0
        for core_num in range(self.first_file, self.last_file + 1):
            core_key = "Core_{0}".format(core_num)
            volume_processed += f[core_key]["Header"]["Runtime"].attrs["frac_volume_processed"]

        # Scale the volume by the number of files that we will read. Used to ensure
        # properties scaled by volume (e.g., SMF) gives sensible results.
        self.volume = pow(self.box_size, 3) * volume_processed 


    def determine_num_gals(self):
        """
        Determines the number of galaxies in all cores for this model at the snapshot
        specified by the ``hdf5_snapshot`` attribute. The ``num_gals`` is updated
        directly.
        """

        ngals = 0
        snap_key = "Snap_{0}".format(self.hdf5_snapshot)

        for core_idx in range(self.first_file, self.last_file + 1):

            core_key = "Core_{0}".format(core_idx)            
            ngals += self.hdf5_file[core_key][snap_key].attrs["num_gals"]

        self.num_gals = ngals 


    def read_gals(self, core_num, pbar=None, plot_galaxies=False, debug=False):
        """
        Reads the galaxies of a single core at the snapshot spoecified ``hdf5_snapshot``.

        Parameters 
        ----------

        core_num : Integer
            The core group we're reading.

        pbar : ``tqdm`` class instance
            Bar showing the progress of galaxy reading.

        plot_galaxies : Boolean, default False
            If set, plots and saves the 3D distribution of galaxies for this file.

        debug : Boolean, default False
            If set, prints out extra useful debug information.

        ..note:: ``tqdm`` does not play nicely with printing to stdout. Hence we
        disable the ``tqdm`` progress bar if ``debug=True``.

        Returns
        -------

        gals : ``h5py`` group
            The galaxies for this file.
        """

        core_key = "Core_{0}".format(core_num)
        snap_key = "Snap_{0}".format(self.hdf5_snapshot)

        num_gals_read = self.hdf5_file[core_key][snap_key].attrs["num_gals"]
        if num_gals_read == 0:
            return None

        gals = self.hdf5_file[core_key][snap_key]


        # If we're using the `tqdm` package, update the progress bar.
        if pbar:
            pbar.set_postfix(file=core_key, refresh=False)
            pbar.update(num_gals_read)

        if debug:
            print("")
            print("Core {0}, Snapshot {1} contained {2} galaxies".format(core_num, self.hdf5_snapshot, num_gals_read))

            w = np.where(gals["StellarMass"][:] > 1.0)[0]
            print("{0} of these galaxies have mass greater than 10^10Msun/h".format(len(w)))

        if plot_galaxies:
            # Show the distribution of galaxies in 3D.
            import plots

            pos = np.empty((len(gals["Posx"]), 3), dtype=np.float32)

            dim_name = ["x", "y", "z"]
            for (dim_num, dim_name) in enumerate(dim_name):
                key = "Pos{0}".format(dim_num)
                pos[:, dim_num] = gals[key][:]

            output_file = "./galaxies_{0}{1}".format(core_num, self.plot_output_format)
            plots.plot_spatial_3d(pos, output_file, self.box_size)

        return gals


    def update_snapshot(self, snapshot):
        """
        Updates the HDF5 snapshot to ``snapshot``.
        """
        self.hdf5_snapshot = snapshot

    def close_file(self):
        """
        Closes the open HDF5 file.
        """

        self.hdf5_file.close()
