from model import Model

import numpy as np

import os

class SageHdf5Model(Model):

    def __init__(self, model_dict):

        Model.__init__(self, model_dict)
        self.hdf5_file = h5py.File(self.model_path)

        # For the HDF5 file, "first_file" and "last_file" correspond to the "Core_%d"
        # groups.
        self.first_file = 0
        self.last_file = self.hdf5_file["Header"].attrs["num_cores"] - 1
        self.num_files = self.hdf5_file["Header"].attrs["num_cores"]


    def set_cosmology(self):
        """
        Sets the relevant cosmological values, size of the simulation box and
        number of galaxy files.

        ..note:: ``box_size`` is in units of Mpc/h.

        Parameters 
        ----------

        None.

        Returns
        -------

        None.
        """

        f = self.hdf5_file

        self.hubble_h = f["Header"].attrs["hubble_h"]
        self.box_size = f["Header"].attrs["box_size"]

        # Scale the volume by the number of files that we will read. Used to ensure
        # properties scaled by volume (e.g., SMF) gives sensible results.
        self.volume = pow(self.box_size, 3) * f["Header"].attrs["frac_volume_processed"] 


    def determine_num_gals(self):
        """
        Determines the number of galaxies in all files for this model.

        Parameters 
        ----------

        None.

        Returns
        -------

        None.
        """

    def determine_ngals_at_snap(self):

        ngals = 0
        snap_key = "Snap_{0}".format(self.hdf5_snapshot)

        for core_idx in range(self.num_files):

            core_key = "Core_{0}".format(core_idx)
            ngals += self.hdf5_file[core_key][snap_key].attrs["ngals"]


        self.num_gals = num_gals


    def read_gals(self, core_num, pbar=None, plot_galaxies=False, debug=False):
        """
        Reads a single galaxy file.

        Parameters 
        ----------

        file_num : Integer
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

        gals : ``numpy`` structured array with format given by ``Model.get_galaxy_struct()``
            The galaxies for this file.
        """

        core_key = "Core_{0}".format(core_num)
        snap_key = "Snap_{0}".format(self.hdf5_snapshot)


        # Then the actual galaxies.
        gals = self.hdf5_file[core_key][snap_key]
        num_gals_read = self.hdf5_file[core_key][snap_key].attrs["ngals"]

        # If we're using the `tqdm` package, update the progress bar.
        if pbar:
            pbar.set_postfix(file=fname, refresh=False)
            pbar.update(num_gals_read)

        if debug:
            print("")
            print("Core {0}, Snapshot {1} contained {2} galaxies".format(core_num, self.hdf5_snapshot, num_gals_read))

            w = np.where(gals["StellarMass"] > 1.0)[0]
            print("{0} of these galaxies have mass greater than 10^10Msun/h".format(len(w)))

        if plot_galaxies:

            # Show the distribution of galaxies in 3D.
            output_file = "./galaxies_{0}{1}".format(core_num, self.output_format)
            plots.plot_spatial_3d(gals, output_file, self.box_size)

        return gals
