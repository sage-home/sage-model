#!/usr/bin/env python
"""
Example script for plotting the data from the Mini-Millennium simulation.

By extending the model lists (e.g., IMFs, snapshots, etc), you can plot multiple
simulations on the same axis.

Refer to the online documentation (sage-model.readthedocs.io) for full information on how
to add your own data format, property calculations and plots.

Authors: (Jacob Seiler, Manodeep Sinha)
"""

import numpy as np
import os
import sys

# These contain example functions to calculate (and plot) properties such as the
# stellar mass function, quiescent fraction, bulge fraction etc.
import sage_analysis
import sage_analysis.example_calcs
import sage_analysis.example_plots

# Function to determine what we will calculate/plot for each model.
from sage_analysis.utils import generate_func_dict

# Class that handles the calculation of properties.
from sage_analysis.model import Model
# Data Classes that handle reading the different SAGE output formats.
from sage_analysis.sage_binary import SageBinaryData

# If the user is only using binary, then we do not need to fail
# However, then the user can not read in hdf5 output
try:
    from sage_analysis.sage_hdf5 import SageHdf5Data
except ImportError:
    SageHdf5Data = None
    

# Sometimes we divide a galaxy that has zero mass (e.g., no cold gas). Ignore these
# warnings as they spam stdout. Also remember the old settings.
old_error_settings = np.seterr()
np.seterr(all="ignore")


if __name__ == "__main__":

    # Base specifications.
    plot_output_format    = "png"
    plot_output_path = "./plots"  # Will be created if path doesn't exist.

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_snapshot = ...", "model1_IMF = ...".
    millennium = { "snapshot": 63,   # Snapshot we're plotting properties at.
                   "IMF": "Chabrier",  # Chabrier or Salpeter.
                   "label": "Mini-Millennium",  # Legend label.
                   "sage_file": "../input/millennium.par",
                   "sage_output_format": "sage_hdf5",
                   "first_file": 0,  # File range (or core range for HDF5) to plot.
                   "last_file": 0,  # Closed interval, [first_file, last_file].
                   "num_output_files": 1,
                 }

    genesis = { "snapshot": 189,   # Snapshot we're plotting properties at.
                "IMF": "Chabrier",  # Chabrier or Salpeter.
                "label": "Genesis",  # Legend label.
                "sage_file": "/home/msinha/scratch/simulations/genesis/L75_N324-L500_N2160_calibration/genesis_L75_calibration.par",
                "sage_output_format": "sage_hdf5",
                "first_file": 0,  # File range (or core range for HDF5) to plot.
                "last_file": 2,  # Closed interval, [first_file, last_file].
                "num_output_files": 3,
    }

    
    # Extend this list for every model you want to plot.
    #models_to_plot = [genesis]
    models_to_plot = [millennium]

    # These toggles specify which plots you want to be made.
    plot_toggles = {"SMF"             : 1,  # Stellar mass function.
                    "BMF"             : 1,  # Baryonic mass function.
                    "GMF"             : 1,  # Gas mass function (cold gas).
                    "BTF"             : 1,  # Baryonic Tully-Fisher.
                    "sSFR"            : 1,  # Specific star formation rate.
                    "gas_fraction"    : 1,  # Fraction of galaxy that is cold gas.
                    "metallicity"     : 1,  # Metallicity scatter plot.
                    "bh_bulge"        : 1,  # Black hole-bulge relationship.
                    "quiescent"       : 1,  # Fraction of galaxies that are quiescent.
                    "bulge_fraction"  : 1,  # Fraction of galaxies that are bulge/disc dominated.
                    "baryon_fraction" : 1,  # Fraction of baryons in galaxy/reservoir.
                    "reservoirs"      : 1,  # Mass in each reservoir.
                    "spatial"         : 1}  # Spatial distribution of galaxies.

    ############## DO NOT TOUCH BELOW #############
    ### IF NOT ADDING EXTRA PROPERTIES OR PLOTS ###
    ############## DO NOT TOUCH BELOW #############

    # Generate directory for output plots.
    if not os.path.exists(plot_output_path):
        os.makedirs(plot_output_path)

    # Go through each model and calculate all the required properties.
    models = []
    for model_dict in models_to_plot:

        # Instantiate a Model class. This holds the data paths and methods to calculate
        # the required properties.
        my_model = Model()
        my_model.plot_output_format = plot_output_format

        # Each SAGE output has a specific class written to read in the data.
        if model_dict["sage_output_format"] == "sage_binary":
            my_model.data_class = SageBinaryData(my_model,
                                                 model_dict["num_output_files"],
                                                 model_dict["sage_file"],
                                                 model_dict["snapshot"])
        elif model_dict["sage_output_format"] == "sage_hdf5":
            if not SageHdf5Data:
                msg = "Please install 'h5py' if you would like to read HDF5 "\
                      "output from SAGE and re-install this package"
                raise ImportError(msg)

            my_model.data_class = SageHdf5Data(my_model, model_dict["sage_file"])
        else:
            raise ValueError("Unknown sage output format = '{}'".format(model_dict["sage_output_format"]))

        # The data class has read the SAGE ini file.  Update the model with the parameters
        # read and those specified by the user.
        my_model.update_attributes(model_dict, plot_toggles)

        #my_model.data_class.set_cosmology(my_model)

        # Some properties require the stellar mass function to normalize their values. For
        # these, the SMF plot toggle is explicitly required.
        try:
            if plot_toggles["SMF"]:
                my_model.calc_SMF = True
            else:
                my_model.calc_SMF = False
        except KeyError:  # Maybe we've removed "SMF" from plot_toggles...
                my_model.calc_SMF = False

        # Then populate the `calculation_methods` dictionary. This dictionary will control
        # which properties each model will calculate.  The dictionary is populated using
        # the plot_toggles defined above.
        # Our functions are inside the `example_calcs.py` module and are named "calc_<toggle>". If
        # your functions are in a different module or different function prefix, change it
        # here.
        # ALL FUNCTIONS MUST HAVE A FUNCTION SIGNATURE `func(Model, gals, optional_kwargs=...)`.
        calculation_functions = generate_func_dict(plot_toggles, module_name="sage_analysis.example_calcs",
                                                   function_prefix="calc_")

        # Finally, before we calculate the properties, we need to decide how each property
        # is stored. Properties can be binned (e.g., how many galaxies with mass between 10^8.0
        # and 10^8.1), scatter plotted (e.g., for 1000 galaxies plot the specific star
        # formation rate versus stellar mass) or a single number (e.g., the sum
        # of the star formation rate at a snapshot). Properties can be accessed using
        # `Model.properties["property_name"]`; e.g., `Model.properties["SMF"]`.

        # First let's do the properties binned on stellar mass. The bins themselves can be
        # accessed using `Model.bins["bin_name"]`; e.g., `Model.bins["stellar_mass_bins"]
        stellar_properties = ["SMF", "red_SMF", "blue_SMF", "BMF", "GMF",
                              "centrals_MF", "satellites_MF", "quiescent_galaxy_counts",
                              "quiescent_centrals_counts", "quiescent_satellites_counts",
                              "fraction_bulge_sum", "fraction_bulge_var",
                              "fraction_disk_sum", "fraction_disk_var"]
        my_model.init_binned_properties(8.0, 12.0, 0.1, "stellar_mass_bins",
                                        stellar_properties)

        # Properties binned on halo mass.
        halo_properties = ["fof_HMF"]
        component_properties = ["halo_{0}_fraction_sum".format(component) for component in
                               ["baryon", "stars", "cold", "hot", "ejected", "ICS", "bh"]]
        my_model.init_binned_properties(10.0, 14.0, 0.1, "halo_mass_bins",
                                        halo_properties+component_properties)

        # Now properties that will be extended as lists.
        scatter_properties = ["BTF_mass", "BTF_vel", "sSFR_mass", "sSFR_sSFR",
                              "gas_frac_mass", "gas_frac", "metallicity_mass",
                              "metallicity", "bh_mass", "bulge_mass", "reservoir_mvir",
                              "reservoir_stars", "reservoir_cold", "reservoir_hot",
                              "reservoir_ejected", "reservoir_ICS", "x_pos",
                              "y_pos", "z_pos"]
        my_model.init_scatter_properties(scatter_properties)

        # Finally those properties that are stored as a single number.
        single_properties = []
        my_model.init_single_properties(single_properties)

        # To be more memory concious, we calculate the required properties on a
        # file-by-file basis. This ensures we do not keep ALL the galaxy data in memory.
        my_model.calc_properties_all_files(calculation_functions, debug=True)

        models.append(my_model)

    # Similar to the calculation functions, all of the plotting functions are in the
    # `plots.py` module and are labelled `plot_<toggle>`.
    plot_functions = generate_func_dict(plot_toggles, module_name="sage_analysis.example_plots",
                                        function_prefix="plot_")

    # Now do the plotting.
    for func_name in plot_functions.keys():
        func = plot_functions[func_name][0]
        keyword_args = plot_functions[func_name][1]

        func(models, plot_output_path, plot_output_format, **keyword_args)

    # Set the error settings to the previous ones so we don't annoy the user.
    np.seterr(divide=old_error_settings["divide"], over=old_error_settings["over"],
              under=old_error_settings["under"], invalid=old_error_settings["invalid"])
