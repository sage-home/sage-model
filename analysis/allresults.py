#!/usr/bin/env python
"""
Main driver script to handle plotting the output of the ``SAGE`` model.  Multiple models
can be placed onto the plots by extending the variables in the ``__main__`` function call.

Authors: Jacob Seiler, Manodeep Sinha, Darren Croton
"""

from sage_binary import SageBinaryModel
from sage_hdf5 import SageHdf5Model 

import numpy as np

import plots as plots

# Sometimes we divide a galaxy that has zero mass (e.g., no cold gas). Ignore these
# warnings as they spam stdout.
np.warnings.filterwarnings("ignore")

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
        Models that we will be plotting.

    plot_toggles : Dictionary
        Specifies which plots will be generated. An entry of `1` denotes
        plotting, otherwise it will be skipped.

    plot_output_path : String
        Base path where the plots will be saved.

    output_format : String
        Format the plots are saved as.
    """

    def __init__(self, all_models_dict, plot_toggles, plot_output_path="./plots",
                 output_format=".png", debug=False):
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

        output_format : String, default ".png"
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

        self.output_format = output_format

        # We will create a list that holds the Model class for each model.
        all_models = []

        # Now let's go through each model, build an individual dictionary for
        # that model and then create a Model instance using it.
        for model_num in range(self.num_models):

            model_dict = {}
            for field in all_models_dict.keys():
                model_dict[field] = all_models_dict[field][model_num]

            if model_dict["output_format"] == "sage_binary":      
                model = SageBinaryModel(model_dict)
            elif model_dict["output_format"] == "sage_hdf5":
                model = SageHdf5Model(model_dict)

            model.set_cosmology()                              

            # To be more memory concious, we calculate the required properties on a
            # file-by-file basis. This ensures we do not keep ALL the galaxy data in memory. 
            model.calc_properties_all_files(plot_toggles, debug=debug)

            all_models.append(model)

        self.models = all_models
        self.plot_toggles = plot_toggles


    def do_plots(self):
        """
        Wrapper method to perform all the plotting for the models.

        Parameters
        ----------

        None

        Returns 
        -------

        None. The plots are saved individually by each method.
        """

        plot_toggles = self.plot_toggles

        # Depending upon the toggles, make the plots.

        if plot_toggles["SMF"] == 1:
            print("Plotting the Stellar Mass Function.")
            plots.plot_SMF(self)

        if plot_toggles["BMF"] == 1:
            print("Plotting the Baryon Mass Function.")
            plots.plot_BMF(self)

        if plot_toggles["GMF"] == 1:
            print("Plotting the Cold Gas Mass Function.")
            plots.plot_GMF(self)

        if plot_toggles["BTF"] == 1:
            print("Plotting the Baryonic Tully-Fisher relation.")
            plots.plot_BTF(self)

        if plot_toggles["sSFR"] == 1:
            print("Plotting the specific star formation rate.")
            plots.plot_sSFR(self)

        if plot_toggles["gas_frac"] == 1:
            print("Plotting the gas fraction.")
            plots.plot_gas_frac(self)

        if plot_toggles["metallicity"] == 1:
            print("Plotting the metallicity.")
            plots.plot_metallicity(self)

        if plot_toggles["bh_bulge"] == 1:
            print("Plotting the black hole-bulge relationship.")
            plots.plot_bh_bulge(self)

        if plot_toggles["quiescent"] == 1:
            print("Plotting the fraction of quiescent galaxies.")
            plots.plot_quiescent(self)

        if plot_toggles["bulge_fraction"]:
            print("Plotting the bulge mass fraction.")
            plots.plot_bulge_mass_fraction(self)

        if plot_toggles["baryon_fraction"]:
            print("Plotting the baryon fraction as a function of FoF Halo mass.")
            plots.plot_baryon_fraction(self)

        if plot_toggles["reservoirs"]:
            print("Plotting a scatter of the mass reservoirs.")
            plots.plot_mass_reservoirs(self)

        if plot_toggles["spatial"]:
            print("Plotting the spatial distribution.")
            plots.plot_spatial_distribution(self)


if __name__ == "__main__":

    import os

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_dir_name = ...",
    # "first_file", "last_file", "simulation" and "num_tree_files" only need to be
    # specified if using binary output. HDF5 will automatically detect these.
    model0_output_format       = "sage_binary"  # Format SAGE output in. "sage_binary" or "sage_hdf5".
    model0_dir_name            = "../tests/test_data/"
    model0_file_name           = "test_sage_z0.000"
    model0_IMF                 = "Chabrier"  # Chabrier or Salpeter.
    model0_model_label         = "Mini-Millennium"
    model0_color               = "r"
    model0_linestyle           = "-"
    model0_marker              = "x"
    model0_first_file          = 0  # The files read in will be [first_file, last_file]
    model0_last_file           = 0  # This is a closed interval.
    model0_simulation          = "Mini-Millennium"  # Sets the cosmology.
    model0_hdf5_snapshot       = 63
    model0_num_tree_files_used = 8  # Number of tree files processed by SAGE to produce this output.

    # Then extend each of these lists for all the models that you want to plot.
    # E.g., 'dir_names = [model0_dir_name, model1_dir_name, ..., modelN_dir_name]
    output_formats      = [model0_output_format]
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
    output_format    = ".png"
    plot_output_path = "./plots_binary"  # Will be created if path doesn't exist.

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

    model_dict = { "output_format"       : output_formats,
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
                   "hdf5_snapshot"       : hdf5_snapshots,
                   "num_tree_files_used" : num_tree_files_used}

    # Read in the galaxies and calculate properties for each model.
    results = Results(model_dict, plot_toggles, plot_output_path, output_format,
                      debug=False)
    results.do_plots()
