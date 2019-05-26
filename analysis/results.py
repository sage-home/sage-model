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

    def __init__(self, model_dicts, plot_toggles, plot_output_path="./plots",
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

        import os

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
