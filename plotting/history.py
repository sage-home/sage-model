#!/usr/bin/env python
"""
Similar to ``galaxy_properties.py``, this script plots data from the Mini-Millennium
simulation.  However, it has been altered to instead of reading and computing properties
for a single simulation snapshot, it calculates properties over a range of snapshots.

By extending the model lists (e.g., IMFs, snapshots, etc), you can plot multiple
simulations on the same axis.

Refer to the online documentation (sage-model.readthedocs.io) for full information on how
to add your own data format, property calculations and plots.

Author: Jacob Seiler
"""

# These contain example functions to calculate (and plot) properties such as the
# stellar mass function, quiescent fraction, bulge fraction etc.
import sage_analysis.example_calcs
import sage_analysis.example_plots

# Function to determine what we will calculate/plot for each model.
from sage_analysis.utils import generate_func_dict

# Class that handles the calculation of properties.
from sage_analysis.model import Model
# Import the Data Classes that handle the different SAGE output formats.
from sage_analysis.sage_binary import SageBinaryData

try:
    from sage_analysis.sage_hdf5 import SageHdf5Data
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
# warnings as they spam stdout. Also remember the old settings.
old_error_settings = np.seterr()
np.seterr(all="ignore")


if __name__ == '__main__':

    import os

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_sage_output_format = ...", "model1_dir_name = ...".
    # `first_file`, `last_file`, `simulation` and `num_tree_files` only need to be
    # specified if using binary output. HDF5 will automatically detect these.
    model0_SMF_z = [0.0, 1.0, 2.0, 3.0]  # Redshifts you wish to plot the stellar mass function at.
                                         # Will search for the closest simulation redshift.
    model0_density_z = "All"  # List of redshifts you wish to plot the evolution of
                              # densities at. Set to "All" for all redshifts.
    model0_IMF = "Chabrier"  # Chabrier or Salpeter.
    model0_label = "Mini-Millennium"
    model0_sage_file = "../input/millennium.par"
    model0_simulation = "Mini-Millennium"  # Used to set cosmology.
    model0_first_file = 0  # File range we're plotting.
    model0_last_file = 0  # Closed interval, [first_file, last_file].

    # Then extend each of these lists for all the models that you want to plot.
    # E.g., 'dir_names = [model0_dir_name, model1_dir_name, ..., modelN_dir_name]
    SMF_zs = [model0_SMF_z]
    density_zs = [model0_density_z]
    IMFs = [model0_IMF]
    labels = [model0_label]
    sage_files = [model0_sage_file]
    simulations = [model0_simulation]
    first_files = [model0_first_file]
    last_files = [model0_last_file]

    # A couple of extra variables...
    plot_output_format    = ".png"
    plot_output_path = "./plots"  # Will be created if path doesn't exist.

    # These toggles specify which plots you want to be made.
    plot_toggles = {"SMF"             : 1,  # Stellar mass function at specified redshifts.
                    "SFRD"            : 1,  # Star formation rate density at specified snapshots.
                    "SMD"             : 1}  # Stellar mass density at specified snapshots.

    debug = False
    ############## DO NOT TOUCH BELOW #############
    ### IF NOT ADDING EXTRA PROPERTIES OR PLOTS ###
    ############## DO NOT TOUCH BELOW #############

    # Generate directory for output plots.
    if not os.path.exists(plot_output_path):
        os.makedirs(plot_output_path)

    # Generate a dictionary for each model containing the required information.
    # We store these in `model_dicts` which will be a list of dictionaries.
    model_dicts = []
    for SMF_z, density_z, IMF, label, sage_file, sim, first_file, last_file in \
        zip(SMF_zs, density_zs, IMFs, labels, sage_files, simulations, first_files, last_files):
        this_model_dict = {"SMF_z": SMF_z,
                           "density_z": density_z,
                           "IMF": IMF,
                           "label": label,
                           "sage_file": sage_file,
                           "simulation": sim,
                           "first_file": first_file,
                           "last_file": last_file}
        model_dicts.append(this_model_dict)

    # Go through each model and calculate all the required properties.
    models = []
    for model_dict in model_dicts:

        # Instantiate a Model class. This holds the data paths and methods to calculate
        # the required properties.
        my_model = sage_analysis.model.Model(model_dict)
        my_model.plot_output_format = plot_output_format

        # Each SAGE output has a specific class written to read in the data.
        if my_model.sage_output_format == "sage_binary":
            my_model.data_class = SageBinaryData(my_model)
        elif my_model.sage_output_format == "sage_hdf5":
            my_model.data_class = SageHdf5Data(my_model)

        my_model.data_class.set_cosmology(my_model)

        # We may be plotting the density at all snapshots...
        if model_dict["density_z"] == "All":
            my_model.density_redshifts = my_model.redshifts
        else:
            my_model.density_redshifts = model_dict["density_z"]

        # Same for SMF
        if model_dict["SMF_z"] == "All":
            my_model.SMF_redshifts = my_model.redshifts
        else:
            my_model.SMF_redshifts = model_dict["SMF_z"]

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

            # Update the snapshot we're reading from. Data Class specific.
            my_model.data_class.update_snapshot(my_model, snap)

            # Calculate all the properties. If we're using a HDF5 file, we want to keep
            # the file open.
            my_model.calc_properties_all_files(calculation_functions, close_file=False,
                                               use_pbar=False, debug=debug)

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
            my_model.data_class.close_file(my_model)
        except AttributeError:
            pass

    # Similar to the calculation functions, all of the plotting functions are in the
    # `plots.py` module and are labelled `plot_<toggle>`.
    plot_functions = generate_func_dict(plot_toggles, module_name="sage_analysis.example_plots",
                                        function_prefix="plot_")

    # Call a slightly different function for plotting the SMF because we're doing it at
    # multiple snapshots.
    try:
        plot_functions["plot_SMF"][0] = sage_analysis.example_plots.plot_temporal_SMF
    except KeyError:
        pass

    # Now do the plotting.
    for func_name in plot_functions.keys():
        func = plot_functions[func_name][0]
        keyword_args = plot_functions[func_name][1]

        func(models, plot_output_path, plot_output_format, **keyword_args)

    # Set the error settings to the previous ones so we don't annoy the user.
    np.seterr(divide=old_error_settings["divide"], over=old_error_settings["over"],
              under=old_error_settings["under"], invalid=old_error_settings["invalid"])
