#!/usr/bin/env python
"""
Example script for plotting the data from the Mini-Millennium simulation.

To calculate and plot extra properties, see the documentation at the top of the
``results.py`` module.

Author: Jacob Seiler.
"""

import model
import plots

# Import the subclasses that handle the different SAGE output formats.
from sage_binary import SageBinaryModel

try:
    from sage_hdf5 import SageHdf5Model
except ImportError:
    print("h5py not found.  If you're reading in HDF5 output from SAGE, please install "
          "this package.")

from results import Results

import numpy as np

# Sometimes we divide a galaxy that has zero mass (e.g., no cold gas). Ignore these
# warnings as they spam stdout.
old_error_settings = np.seterr()
np.seterr(all="ignore")


def generate_func_dict(plot_toggles, module_prefix, function_prefix):
    """
    Generates a dictionary where the keys are the function name and the value is the
    function itself.  Functions are only generated when the ``plot_toggles`` value is
    specified.

    Functions are required to be named ``<module_prefix><function_prefix><plot_toggle_key>``
    For example, the default calculation function are kept in the ``model.py`` module and
    are named ``calc_<toggle>``.  E.g., ``model.calc_SMF()``, ``model.calc_BTF()``,
    ``model.calc_sSFR()`` etc.

    Parameters
    ----------

    plot_toggles : dictionary
        Dictionary specifying the name of each property/plot and whether the values
        will be generated + plotted. A value of 1 denotes plotting, whilst a value of
        0 denotes not plotting.  Entries with a value of 1 will be added to the function
        dictionary.

        Example
        -------

        plot_toggles = {"SMF" : 0,
                        "BTF" : 1,
                        "sSFR" : 1}

    module_prefix : string
        Name of the module where the functions are located. Must include the trailing full
        stop.

        Example
        -------

        module_prefix = "model."

    function_prefix : string
        Prefix that is added to the start of each function.

        Example
        -------

        function_prefix = "calc_"
    """

    # Only populate those methods that have been marked in the `plot_toggles`
    # dictionary.
    func_dict = {}
    for toggle in plot_toggles.keys():
        if plot_toggles[toggle]:

            func_name = "{0}{1}{2}".format(module_prefix, function_prefix, toggle)

            # Be careful.  Maybe the func for a specified `plot_toggle` value wasn't
            # added to the module.
            try:
                func = eval(func_name)
            except AttributeError:
                msg = "Tried to get the func named '{0}' corresponding to " \
                      "'plot_toggle' value '{1}'.  However, no func named '{0}' " \
                      "could be found in '{2}' module.".format(func_name,
                      toggle, module_prefix)
                raise AttributeError(msg)

            func_dict[func_name] = func

    return func_dict


if __name__ == "__main__":

    import os

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_sage_output_format = ...", "model1_dir_name = ...".
    # `first_file`, `last_file`, `simulation` and `num_tree_files` only need to be
    # specified if using binary output. HDF5 will automatically detect these.
    # `hdf5_snapshot` is only nedded if using HDF5 output.

    model0_sage_output_format  = "sage_hdf5"  # Format SAGE output in. "sage_binary" or "sage_hdf5".
    model0_dir_name            = "../output/millennium/"
    model0_file_name           = "model.hdf5"
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
                    "BMF"             : 0,  # Baryonic mass function.
                    "GMF"             : 0,  # Gas mass function (cold gas).
                    "BTF"             : 0,  # Baryonic Tully-Fisher.
                    "sSFR"            : 0,  # Specific star formation rate.
                    "gas_frac"        : 0,  # Fraction of galaxy that is cold gas.
                    "metallicity"     : 0,  # Metallicity scatter plot.
                    "bh_bulge"        : 0,  # Black hole-bulge relationship.
                    "quiescent"       : 0,  # Fraction of galaxies that are quiescent.
                    "bulge_fraction"  : 0,  # Fraction of galaxies that are bulge/disc dominated.
                    "baryon_fraction" : 0,  # Fraction of baryons in galaxy/reservoir.
                    "reservoirs"      : 0,  # Mass in each reservoir.
                    "spatial"         : 0}  # Spatial distribution of galaxies.

    ############## DO NOT TOUCH BELOW #############
    ### IF NOT ADDING EXTRA PROPERTIES OR PLOTS ###
    ############## DO NOT TOUCH BELOW #############

    # Everything has been specified, now initialize.
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

    # Now go through each model and calculate all the required properties.
    models = []
    for model_dict in model_dicts:

        # Initialize a Model class and set all the paths.
        # Note: Use the correct subclass depending upon the format SAGE wrote in.
        if model_dict["sage_output_format"] == "sage_binary":
            my_model = SageBinaryModel(model_dict, plot_toggles)
        elif model_dict["sage_output_format"] == "sage_hdf5":
            my_model = SageHdf5Model(model_dict, plot_toggles)
        else:
            msg = "Invalid value for `sage_output_format`. Value was " \
                  "{0}".format(model-dict["sage_output_format"])
            raise ValueError(msg)

        my_model.set_cosmology()

        # Then populate the `calculation_methods` dictionary. This dictionary will control
        # which properties each model will calculate.  The dictionary is populated using
        # the plot_toggles defined above.
        # Our functions are inside the `model.py` module and are named "calc_<toggle>". If
        # your functions are in a different module or different function prefix, change it
        # here (remembering the full stop after the `module_prefix`).
        # ALL FUNCTIONS MUST HAVE A FUNCTION SIGNATURE `func(Model, gals)`.
        my_model.calculation_functions = generate_func_dict(results.plot_toggles, module_prefix="model.",
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
        my_model.init_binned_properties(8.0, 14.0, 0.1, "halo_mass_bins",
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
        my_model.calc_properties_all_files(debug=False)

        models.append(my_model)

    # Similar to the calculation functions, all of the plotting functions are in the
    # `plots.py` module and are labelled `plot_<toggle>`.
    plot_dict = generate_func_dict(results.plot_toggles, module_prefix="plots.",
                                   function_prefix="plot_")

    # Now do the plotting.
    for plot_func in plot_dict.values():
        plot_func(models, plot_output_path, plot_output_format)

    # Set the error settings to the previous ones so we don't annoy the user.
    np.seterr(divide=old_error_settings["divide"], over=old_error_settings["over"],
              under=old_error_settings["under"], invalid=old_error_settings["invalid"])
