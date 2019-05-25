#!/usr/bin/env python
"""
Main driver script to handle plotting the output of the ``SAGE`` model.  Multiple models
can be placed onto the plots by extending the variables in the ``__main__`` function call.

To add your own data format, create a subclass module (e.g., ``sage_binary.py``) and add an
option to ``Results.__init__``.  This subclass module needs methods ``set_cosmology()``,
``determine_num_gals()`` and  ``read_gals()``.

To calculate and plot extra properties, first add the name of your new plot to the
``plot_toggles`` dictionary.  You will need to create a method in ``model.py`` to
calculate your properties and name it ``calc_<Name of your plot toggle>``.  To plot your
new property, you will need to create a function in ``plots.py`` called ``plot_<Name of
your plot toggle>``.

For example, to generate and plot data for the ``SMF`` plot, we have methods ``calc_SMF()``
and ``plot_SMF()``.

Refer to the documentation inside the ``model.py`` and ``plot.py`` modules for more
details.

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

import numpy as np

# Sometimes we divide a galaxy that has zero mass (e.g., no cold gas). Ignore these
# warnings as they spam stdout.
old_error_settings = np.seterr()
np.seterr(all="ignore")

# Refer to the project for a list of TODO's, issues and other notes.
# https://github.com/sage-home/sage-model/projects/7

class Results:
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

        model_dicts : List of dictionaries. Length is the number of models
            Each element of this list is a dictionary containing the parameter values for
            the resepctive ``Model`` Refer to the ``Model`` class for full details on this
            dictionary.

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

        self.num_models = len(model_dicts)
        self.plot_output_path = plot_output_path

        if not os.path.exists(self.plot_output_path):
            os.makedirs(self.plot_output_path)

        self.plot_output_format = plot_output_format

        # We will create a list that holds the Model class for each model.
        all_models = []

        # Now let's go through each model, build an individual dictionary for
        # that model and then create a Model instance using it.
        for model_dict in model_dicts:

            # Use the correct subclass depending upon the format SAGE wrote in.
            if model_dict["sage_output_format"] == "sage_binary":
                model = SageBinaryModel(model_dict, plot_toggles)
            elif model_dict["sage_output_format"] == "sage_hdf5":
                model = SageHdf5Model(model_dict, plot_toggles)

            model.plot_output_format = plot_output_format

            model.set_cosmology()

            all_models.append(model)

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

        plots.setup_matplotlib_options()

        # Go through all the plot toggles and seach for a plot routine named
        # "plot_<Toggle>".
        for toggle in self.plot_toggles.keys():
            if self.plot_toggles[toggle]:
                method_name = "plot_{0}".format(toggle)

                # If the method doesn't exist, we will hit an `AttributeError`.
                try:
                    getattr(plots, method_name)(self)
                except AttributeError:
                    msg = "Tried to plot '{0}'.  However, no " \
                          "method/function named '{1}' exists in the 'plots.py' module.\n" \
                          "Check either that your plot toggles are set correctly or add " \
                          "a method/function called '{1}' to the 'plots.py' module.".format(toggle, \
                          method_name)
                    msg += "\nPLEASE SCROLL UP AND MAKE SURE YOU'RE READING ALL ERROR " \
                           "MESSAGES! THEY'RE EASY TO MISS! :)"
                    raise AttributeError(msg)


if __name__ == "__main__":

    import os

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_sage_output_format = ...", "model1_dir_name = ...".
    # `first_file`, `last_file`, `simulation` and `num_tree_files` only need to be
    # specified if using binary output. HDF5 will automatically detect these.
    # `hdf5_snapshot` is only nedded if using HDF5 output.

    model0_sage_output_format  = "sage_binary"  # Format SAGE output in. "sage_binary" or "sage_hdf5".
    model0_dir_name            = "../output/millennium/"
    model0_file_name           = "model_z0.000"
    model0_IMF                 = "Chabrier"  # Chabrier or Salpeter.
    model0_model_label         = "Mini-Millennium"
    model0_color               = "r"
    model0_linestyle           = "-"
    model0_marker              = "x"
    model0_first_file          = 0  # The files read in will be [first_file, last_file]
    model0_last_file           = 0  # This is a closed interval.
    model0_simulation          = "Mini-Millennium"  # Sets the cosmology. Required for "sage_binary".
    model0_hdf5_snapshot       = 63  # Snapshot we're plotting the HDF5 data at.
    model0_num_tree_files_used = 8  # Number of tree files processed by SAGE to produce this output.

    # Then extend each of these lists for all the models that you want to plot.
    # E.g., 'dir_names = [model0_dir_name, model1_dir_name, ..., modelN_dir_name]
    sage_output_formats = [model0_sage_output_format]
    dir_names           = [model0_dir_name]
    file_names          = [model0_file_name]
    IMFs                = [model0_IMF]
    model_labels        = [model0_model_label]
    colors              = [model0_color]
    linestyles          = [model0_linestyle]
    markers             = [model0_marker]
    first_files         = [model0_first_file]
    last_files          = [model0_last_file]
    simulations         = [model0_simulation]
    hdf5_snapshots      = [model0_hdf5_snapshot]
    num_tree_files_used = [model0_num_tree_files_used]

    # A couple of extra variables...
    plot_output_format    = ".png"
    plot_output_path = "./plots"  # Will be created if path doesn't exist.

    # These toggles specify which plots you want to be made.
    plot_toggles = {"SMF"             : 1,  # Stellar mass function.
                    "BMF"             : 1,  # Baryonic mass function.
                    "GMF"             : 1,  # Gas mass function (cold gas).
                    "BTF"             : 1,  # Baryonic Tully-Fisher.
                    "sSFR"            : 1,  # Specific star formation rate.
                    "gas_frac"        : 1,  # Fraction of galaxy that is cold gas.
                    "metallicity"     : 1,  # Metallicity scatter plot.
                    "bh_bulge"        : 1,  # Black hole-bulge relationship.
                    "quiescent"       : 1,  # Fraction of galaxies that are quiescent.
                    "bulge_fraction"  : 1,  # Fraction of galaxies that are bulge/disc dominated.
                    "baryon_fraction" : 1,  # Fraction of baryons in galaxy/reservoir.
                    "reservoirs"      : 1,  # Mass in each reservoir.
                    "spatial"         : 1}  # Spatial distribution of galaxies.

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

    # Generate a dictionary for each model containing the required information.
    # We store these in `model_dicts` which will be a list of dictionaries.
    model_dicts = []
    for model_num in range(len(model_paths)):
        this_model_dict = { "sage_output_format"  : sage_output_formats[model_num],
                            "model_path"          : model_paths[model_num],
                            "output_path"         : output_paths[model_num],
                            "IMF"                 : IMFs[model_num],
                            "model_label"         : model_labels[model_num],
                            "color"               : colors[model_num],
                            "linestyle"           : linestyles[model_num],
                            "marker"              : markers[model_num],
                            "first_file"          : first_files[model_num],
                            "last_file"           : last_files[model_num],
                            "simulation"          : simulations[model_num],
                            "hdf5_snapshot"       : hdf5_snapshots[model_num],
                            "num_tree_files_used" : num_tree_files_used[model_num]}

        model_dicts.append(this_model_dict)

    # First initialise all the Models. This will set the cosmologies, the paths etc.
    results = Results(model_dicts, plot_toggles, plot_output_path, plot_output_format,
                      debug=False)

    # Then calculate all the required property for each model.
    for model in results.models:

        # First populate the `calculation_methods` dictionary. This dictionary will control
        # which properties each model will calculate.
        model.calculation_methods = {}

        # Only populate those methods that have been marked in the `plot_toggles`
        # dictionary.
        for toggle in results.plot_toggles.keys():
            if results.plot_toggles[toggle]:

                # Each toggle has a corresponding `calc_<toggle>` method inside the
                # `Method` class.
                method_name = "calc_{0}".format(toggle)

                # Be careful.  Maybe the method for a specified `plot_toggle` value wasn't
                # added to the `model.py` module.
                try:
                    method = getattr(model, method_name)
                except AttributeError:
                    msg = "Tried to get the method named '{0}' corresponding to "
                          "'plot_toggle' value '{1}'.  However, no method named '{0}' "
                          "could be found in 'model.py' module.".format(method_name,
                          toggle)
                    raise AttributeError(msg)

                # Then add this method to the `calculation_methods` dictionary.
                model.add_method(method_name, method)

        # To be more memory concious, we calculate the required properties on a
        # file-by-file basis. This ensures we do not keep ALL the galaxy data in memory.
        model.calc_properties_all_files(debug=False)

    results.do_plots()

    # Set the error settings to the previous ones so we don't annoy the user.
    np.seterr(divide=old_error_settings["divide"], over=old_error_settings["over"],
              under=old_error_settings["under"], invalid=old_error_settings["invalid"])
