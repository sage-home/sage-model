#!/usr/bin/env python
"""
Handles plotting properties over multiple redshifts.  Multiple models can be placed onto
the plots by extending the variables in the ``__main__`` function call.

To add your own data format, create a subclass module (e.g., ``sage_binary.py``) and add an
option to ``Results.__init__``.  This subclass module needs methods ``set_cosmology()``,
``determine_num_gals()``, ``read_gals()`` and ``update_redshift()``.

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

from sage_analysis.example import generate_func_dict
import sage_analysis.plots

# Import the subclasses that handle the different SAGE output formats.
from sage_analysis.sage_binary import SageBinaryModel

try:
    from sage_analysis.sage_hdf5 import SageHdf5Model
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


if __name__ == '__main__':

    import os

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_sage_output_format = ...", "model1_dir_name = ...".
    # `first_file`, `last_file`, `simulation` and `num_tree_files` only need to be
    # specified if using binary output. HDF5 will automatically detect these.

    model0_SMF_z               = [0.0, 1.0, 2.0, 3.0]  # Redshifts you wish to plot the stellar mass function at.
                                 # Will search for the closest simulation redshift.
    model0_density_z           = "All"  # List of redshifts you wish to plot the evolution of
                                     # densities at. Set to "All" for all redshifts.
    model0_alist_file          = "../input/millennium/trees/millennium.a_list"
    model0_sage_output_format  = "sage_hdf5"  # Format SAGE output in. "sage_binary" or "sage_hdf5".
    model0_dir_name            = "../output/millennium/"
    model0_file_name           = "model.hdf5"  # If using "sage_binary", doesn't have to end in "_zX.XXX"
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
    plot_output_path = "./plots"  # Will be created if path doesn't exist.

    # These toggles specify which plots you want to be made.
    plot_toggles = {"SMF"             : 1,  # Stellar mass function at specified redshifts.
                    "SFRD"            : 1,  # Star formation rate density at specified snapshots.
                    "SMD"             : 1}  # Stellar mass density at specified snapshots.

    debug = True
    ############## DO NOT TOUCH BELOW #############
    ### IF NOT ADDING EXTRA PROPERTIES OR PLOTS ###
    ############## DO NOT TOUCH BELOW #############

    # Everything has been specified, now initialize.
    model_paths = []
    output_paths = []

    # Determine paths for each model.
    for dir_name, file_name in zip(dir_names, file_names):

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
    for model_num, _ in enumerate(model_paths):
        this_model_dict = { "SMF_redshifts"       : SMF_zs[model_num],
                            "density_redshifts"   : density_zs[model_num],
                            "alist_file"          : alist_files[model_num],
                            "sage_output_format"  : sage_output_formats[model_num],
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
                            "num_tree_files_used" : num_tree_files_used[model_num]}

        model_dicts.append(this_model_dict)

    # Go through each model and calculate all the required properties.
    models = []
    for model_dict in model_dicts:

        # Use the correct subclass depending upon the format SAGE wrote in.
        if model_dict["sage_output_format"] == "sage_binary":
            my_model = SageBinaryModel(model_dict, plot_toggles)
        elif model_dict["sage_output_format"] == "sage_hdf5":
            my_model = SageHdf5Model(model_dict, plot_toggles)
        else:
            msg = "Invalid value for `sage_output_format`. Value was " \
                  "{0}".format(model_dict["sage_output_format"])
            raise ValueError(msg)

        my_model.plot_output_format = plot_output_format
        my_model.set_cosmology()

        # We may be plotting the density at all snapshots...
        if my_model.density_redshifts == "All":
            my_model.density_redshifts = my_model.redshifts

        # The SMF and the Density plots may have different snapshot requirements.
        # Find the snapshots that most closely match the requested redshifts.
        my_model.SMF_snaps = [(np.abs(my_model.redshifts - SMF_redshift)).argmin() for
                          SMF_redshift in my_model.SMF_redshifts]
        my_model.properties["SMF_dict"] = {}

        my_model.density_snaps = [(np.abs(my_model.redshifts - density_redshift)).argmin() for
                              density_redshift in my_model.density_redshifts]
        my_model.properties["SFRD_dict"] = {}
        my_model.properties["SMD_dict"] = {}

        # We'll need to loop all snapshots, ignoring duplicates.
        snaps_to_loop = np.unique(my_model.SMF_snaps + my_model.density_snaps)

        # Use a tqdm progress bar if possible.
        try:
            snap_iter = tqdm(snaps_to_loop, unit="Snapshot")
        except NameError:
            snap_iter = snaps_to_loop

        # Then populate the `calculation_methods` dictionary. This dictionary will control
        # which properties each model will calculate.  The dictionary is populated using
        # the plot_toggles defined above.
        # Our functions are inside the `model.py` module and are named "calc_<toggle>". If
        # your functions are in a different module or different function prefix, change it
        # here.
        # ALL FUNCTIONS MUST HAVE A FUNCTION SIGNATURE `func(Model, gals)`.
        my_model.calculation_functions = generate_func_dict(plot_toggles, module_name="sage_analysis.model",
                                                            function_prefix="calc_")

        # Finally, before we calculate the properties, we need to decide how each property
        # is stored. Properties can be binned (e.g., how many galaxies with mass between 10^8.0
        # and 10^8.1), scatter plotted (e.g., for 1000 galaxies plot the specific star
        # formation rate versus stellar mass) or a single number (e.g., the sum
        # of the star formation rate at a snapshot). Properties can be accessed using
        # `Model.properties["property_name"]`; e.g., `Model.properties["SMF"]`.

        # First let's do the properties binned on stellar mass. The bins themselves can be
        # accessed using `Model.bins["bin_name"]`; e.g., `Model.bins["stellar_mass_bins"]
        stellar_properties = ["SMF", "red_SMF", "blue_SMF"]
        my_model.init_binned_properties(8.0, 12.0, 0.1, "stellar_mass_bins",
                                        stellar_properties)

        # Properties that are extended as lists.
        scatter_properties = []
        my_model.init_scatter_properties(scatter_properties)

        # Properties that are stored as a single number.
        single_properties = ["SMFD", "SFRD"]
        my_model.init_single_properties(single_properties)

        # Now we know which snapshots we are looping through, loop through them!
        for snap in snap_iter:

            # Each snapshot is unique. So reset the tracking.
            my_model.properties["SMF"] = np.zeros(len(my_model.bins["stellar_mass_bins"])-1,
                                                  dtype=np.float64)
            my_model.properties["SFRD"] = 0.0
            my_model.properties["SMD"] = 0.0

            # Update the snapshot we're reading from. Subclass specific.
            my_model.update_snapshot(snap)

            # Calculate all the properties. If we're using a HDF5 file, we want to keep
            # the file open.
            my_model.calc_properties_all_files(close_file=False, use_pbar=False, debug=debug)

            # We need to place the SMF inside the dictionary to carry through.
            if snap in my_model.SMF_snaps:
                my_model.properties["SMF_dict"][snap] = my_model.properties["SMF"]

            # Same with the densities.
            if snap in my_model.density_snaps:

                # It's slightly wasteful here because the user may only want the SFRD
                # and not the SMD. However the wasted compute time is neglible
                # compared to the time taken to read the galaxies + compute the
                # properties.
                my_model.properties["SFRD_dict"][snap] = my_model.properties["SFRD"]
                my_model.properties["SMD_dict"][snap] = my_model.properties["SMD"]

        models.append(my_model)

        # If we used a HDF5 file, close it. Binary files are closed as they're used.
        try:
            my_model.close_file()
        except AttributeError:
            pass

    # Similar to the calculation functions, all of the plotting functions are in the
    # `plots.py` module and are labelled `plot_<toggle>`.
    plot_dict = generate_func_dict(plot_toggles, module_name="sage_analysis.plots",
                                   function_prefix="plot_")

    # Call a slightly different function for plotting the SMF because we're doing it at
    # multiple snapshots.
    try:
        plot_dict["plot_SMF"] = sage_analysis.plots.plot_temporal_SMF
    except KeyError:
        pass

    # Now do the plotting.
    for plot_func in plot_dict.values():
        plot_func(models, plot_output_path, plot_output_format)

    # Set the error settings to the previous ones so we don't annoy the user.
    np.seterr(divide=old_error_settings["divide"], over=old_error_settings["over"],
              under=old_error_settings["under"], invalid=old_error_settings["invalid"])
