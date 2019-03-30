#!/usr/bin/env python
"""
Handles plotting properties over multiple redshifts.  Multiple models can be placed onto
the plots by extending the variables in the ``__main__`` function call.

To add your own data format, create a subclass module (e.g., ``sage_binary.py``) and add an
option to ``Results.__init__``.  This subclass module needs methods ``set_cosmology()``,
``determine_num_gals()``, ``read_gals()`` and ``update_redshift()``.

Author: Jacob Seiler
"""


import plots as plots

# Import the subclasses that handle the different SAGE output formats.
from sage_binary import SageBinaryModel

try:
    from sage_hdf5 import SageHdf5Model
except ImportError:
    print("h5py not found.  If you're reading in HDF5 output from SAGE, please install "
          "this package.")

try:
    from tqdm import tqdm
except ImportError:
    print("Package 'tqdm' not found. Not showing pretty progress bars :(")
else:
    pass

import numpy as np

# Sometimes we divide a galaxy that has zero mass (e.g., no cold gas). Ignore these
# warnings as they spam stdout.
old_error_settings = np.seterr()
np.seterr(all="ignore")


class TemporalResults:
    """
    Defines all the parameters used to plot the models.

    Attributes
    ----------

    num_models : Integer
        Number of models being plotted.

    models : List of ``Model`` class instances with length ``num_models``
        Models that we will be plotting.  Depending upon the format ``SAGE`` output in,
        this will be a ``Model`` subclass with methods to parse the specific data format.

    plot_toggles : Dictionary
        Specifies which plots will be generated. An entry of `1` denotes
        plotting, otherwise it will be skipped.

    plot_output_path : String
        Base path where the plots will be saved.

    plot_output_format : String
        Format the plots are saved as.
    """

    def __init__(self, all_models_dict, plot_toggles, plot_output_path="./plots",
                 plot_output_format=".png", debug=False):
        """
        Initialises the individual ``Model`` class instances and adds them to
        the ``Results`` class instance.

        Parameters
        ----------

        all_models_dict : Dictionary
            Dictionary containing the parameter values for each ``Model``
            instance. Refer to the ``Model`` class for full details on this
            dictionary. Each field of this dictionary must have length equal to
            the number of models we're plotting.

        plot_toggles : Dictionary
            Specifies which plots will be generated. An entry of 1 denotes
            plotting, otherwise it will be skipped.

        plot_output_path : String, default "./plots"
            The path where the plots will be saved.

        plot_output_format : String, default ".png"
            Format the plots will be saved as.

        debug : {0, 1}, default 0
            Flag whether to print out useful debugging information.

        Returns
        -------

        None.
        """

        self.num_models = len(all_models_dict["model_path"])
        self.plot_output_path = plot_output_path

        if not os.path.exists(self.plot_output_path):
            os.makedirs(self.plot_output_path)

        self.plot_output_format = plot_output_format

        # We will create a list that holds the Model class for each model.
        all_models = []

        # Now let's go through each model, build an individual dictionary for
        # that model and then create a Model instance using it.
        for model_num in range(self.num_models):

            model_dict = {}
            for field in all_models_dict.keys():
                model_dict[field] = all_models_dict[field][model_num]

            # Use the correct subclass depending upon the format SAGE wrote in.
            if model_dict["sage_output_format"] == "sage_binary":
                model = SageBinaryModel(model_dict, plot_toggles)
            elif model_dict["sage_output_format"] == "sage_hdf5":
                model = SageHdf5Model(model_dict, plot_toggles)

            print("Processing data for model {0}".format(model.model_label))

            # We may be plotting the density at all snapshots...
            if model.density_redshifts == -1:
                model.density_redshifts = model.redshifts

            model.plot_output_format = plot_output_format
            model.set_cosmology()

            # The SMF and the Density plots may have different snapshot requirements.
            # Find the snapshots that most closely match the requested redshifts.
            model.SMF_snaps = [(np.abs(model.redshifts - SMF_redshift)).argmin() for
                              SMF_redshift in model.SMF_redshifts]
            model.SMF_dict = {}

            model.density_snaps = [(np.abs(model.redshifts - density_redshift)).argmin() for
                                  density_redshift in model.density_redshifts]
            model.SFRD_dict = {}
            model.SMD_dict = {}

            # We'll need to loop all snapshots, ignoring duplicates.
            snaps_to_loop = np.unique(model.SMF_snaps + model.density_snaps)

            # Use a tqdm progress bar if possible.
            try:
                snap_iter = tqdm(snaps_to_loop, unit="Snapshot")
            except NameError:
                snap_iter = snaps_to_loop

            for snap in snap_iter:

                # Reset the tracking.
                model.SMF = np.zeros(len(model.stellar_mass_bins)-1, dtype=np.float64)
                model.SFRD = 0.0
                model.SMD = 0.0

                # Update the snapshot we're reading from. Subclass specific.
                model.update_snapshot(snap)

                # Calculate all the properties. Keep the HDF5 file open always.
                model.calc_properties_all_files(close_file=False, use_pbar=False, debug=debug)

                # We need to place the SMF inside the dictionary to carry through.
                if snap in model.SMF_snaps:
                    model.SMF_dict[snap] = model.SMF

                # Same with the densities.
                if snap in model.density_snaps:
                    if model.SFRD_toggle:
                        model.SFRD_dict[snap] = model.SFRD

                    if model.SMD_toggle:
                        model.SMD_dict[snap] = model.SMD

            all_models.append(model)

            # If we used a HDF5 file, close it.
            try:
                self.close_file()
            except AttributeError:
                pass

        self.models = all_models
        self.plot_toggles = plot_toggles


    def do_plots(self):
        """
        Wrapper method to perform all the plotting for the models.

        Parameters
        ----------

        None.

        Returns
        -------

        None. The plots are saved individually by each method.
        """

        plot_toggles = self.plot_toggles
        plots.setup_matplotlib_options()

        # Depending upon the toggles, make the plots.
        if plot_toggles["SMF"] == 1:
            print("Plotting the evolution of the Stellar Mass Function.")
            plots.plot_temporal_SMF(self)

        if plot_toggles["SFRD"] == 1:
            print("Plotting the evolution of the Star Formation Rate Density.")
            plots.plot_SFRD(self)

        if plot_toggles["SMD"] == 1:
            print("Plotting the evolution of the Stellar Mass Function Density.")
            plots.plot_SMD(self)


if __name__ == '__main__':

    import os

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_sage_output_format = ...", "model1_dir_name = ...".
    # `first_file`, `last_file`, `simulation` and `num_tree_files` only need to be
    # specified if using binary output. HDF5 will automatically detect these.

    model0_SMF_z               = [0.0, 0.5, 1.00, 2.00, 3.00]  # Redshifts you wish to plot the stellar mass function at.
                                 # Will search for the closest simulation redshift.
    model0_density_z           = -1  # Redshifts you wish to plot the evolution of
                                     # densities at. Set to -1 for all redshifts.
    model0_alist_file          = "../input/millennium/trees/millennium.a_list"
    model0_sage_output_format  = "sage_binary"  # Format SAGE output in. "sage_binary" or "sage_hdf5".
    model0_dir_name            = "../output/millennium/"
    model0_file_name           = "model"  # If using "sage_binary", doesn't have to end in "_zX.XXX"
    model0_IMF                 = "Chabrier"  # Chabrier or Salpeter.
    model0_model_label         = "Mini-Millennium"
    model0_color               = "c"
    model0_linestyle           = "-"
    model0_marker              = "x"
    model0_first_file          = 0  # The files read in will be [first_file, last_file]
    model0_last_file           = 0  # This is a closed interval.
    model0_simulation          = "Mini-Millennium"  # Sets the cosmology for "sage_binary".
    model0_num_tree_files_used = 8  # Number of tree files processed by SAGE to produce this output.

    # Then extend each of these lists for all the models that you want to plot.
    # E.g., 'dir_names = [model0_dir_name, model1_dir_name, ..., modelN_dir_name]
    SMF_zs               = [model0_SMF_z]
    density_zs           = [model0_density_z]
    alist_files          = [model0_alist_file]
    sage_output_formats  = [model0_sage_output_format]
    dir_names            = [model0_dir_name]
    file_names           = [model0_file_name]
    IMFs                 = [model0_IMF]
    model_labels         = [model0_model_label]
    colors               = [model0_color]
    linestyles           = [model0_linestyle]
    markers              = [model0_marker]
    first_files          = [model0_first_file]
    last_files           = [model0_last_file]
    simulations          = [model0_simulation]
    num_tree_files_used  = [model0_num_tree_files_used]

    # A couple of extra variables...
    plot_output_format    = ".png"
    plot_output_path = "./plots_history"  # Will be created if path doesn't exist.

    # These toggles specify which plots you want to be made.
    plot_toggles = {"SMF"             : 1,  # Stellar mass function at specified redshifts.
                    "SFRD"            : 1,  # Star formation rate density at specified snapshots. 
                    "SMD"             : 1}  # Stellar mass density at specified snapshots. 

    ############################
    ## DON'T TOUCH BELOW HERE ##
    ############################

    model_paths = []
    output_paths = []

    # Determine paths for each model.
    for dir_name, file_name  in zip(dir_names, file_names):

        model_path = "{0}/{1}".format(dir_name, file_name)
        model_paths.append(model_path)

        # These are model specific. Used for rare circumstances and debugging.
        output_path = dir_name + "plots/"

        if not os.path.exists(output_path):
            os.makedirs(output_path)

        output_paths.append(output_path)

    model_dict = { "SMF_redshifts"       : SMF_zs,
                   "density_redshifts"   : density_zs,
                   "alist_file"          : alist_files,
                   "sage_output_format"  : sage_output_formats,
                   "model_path"          : model_paths,
                   "output_path"         : output_paths,
                   "IMF"                 : IMFs,
                   "model_label"         : model_labels,
                   "color"               : colors,
                   "linestyle"           : linestyles,
                   "marker"              : markers,
                   "first_file"          : first_files,
                   "last_file"           : last_files,
                   "simulation"          : simulations,
                   "num_tree_files_used" : num_tree_files_used}

    # Read in the galaxies and calculate properties for each model.
    results = TemporalResults(model_dict, plot_toggles, plot_output_path, plot_output_format,
                              debug=False)
    results.do_plots()

    # Set the error settings to the previous ones so we don't annoy the user.
    np.seterr(divide=old_error_settings["divide"], over=old_error_settings["over"],
              under=old_error_settings["under"], invalid=old_error_settings["invalid"])
