#!/usr/bin/env python
"""
This module contains the ``Model`` class.  The ``Model`` class contains all the data
paths, cosmology etc for calculating galaxy properties.

To read **SAGE** data, we make use of specialized Data Classes (e.g.,
:py:class:`~sage_analysis.sage_binary.SageBinaryData`
and:py:class:`~sage_analysis.sage_hdf5.SageHdf5Data`). We refer to
:doc:`../user/data_class` for more information about adding your own Data Class to ingest
data.

To calculate (and plot) extra properties from the **SAGE** output, we refer to
:doc:`../user/calc.rst` and :doc:`../user/plotting.rst`.

Author: Jacob Seiler.
"""

import numpy as np
import time

try:
    from tqdm import tqdm
except ImportError:
    print("Package 'tqdm' not found. Not showing pretty progress bars :(")
else:
    pass


class Model(object):
    """
    Handles all the galaxy data (including calculated properties) for a ``SAGE`` model.

    Description
    -----------

    The ``sage_analysis`` package is driven through the use of this ``Model`` class. It is
    used to define the paths and parameters for each model that is being plotted.  In this
    way, we can handle multiple different simulations trivially.

    The ingestion of data is handled by inidivudal Data Classes (e.g., :py:class:`~sage_analysis.sage_binary.SageBinaryData`
    and :py:class:`~sage_analysis.sage_hdf5.SageHdf5Data`).  We refer to
    :doc:`../user/data_class` for more information about adding your own Data Class to ingest
    data.
    """

    def __init__(self, model_dict, sample_size=1000, read_sage_file=True):
        """
        Sets the galaxy path and number of files to be read for a model. Also initialises
        the plot toggles that dictates which properties will be calculated.

        Parameters
        ----------

        model_dict: dict [string, variable]
            Dictionary containing parameter values for this class instance that can't be
            read from the **SAGE** parameter file. These are :py:attr:`~snapshot`,
            :py:attr:`~IMF`, :py:attr:`~label`, :py:attr:`~sage_file`,
            :py:attr:`~first_file`, :py:attr:`~last_file` and :py:attr:`~simulation`. If
            ``read_sage_file`` is set to ``False``, all model parameters must be specified
            in this dict instead.

        plot_galaxies: string, {"Random", "All"}, optional
            Controls how galaxies will be plotted for scatter plots. If specified as ``"Random"``
            will plot ``sample_size`` random galaxies.  If ``"All"``, will plot all
            galaxies. Selecting ``"All"`` may result in excessive memory consumption for very large
            simulations.

        sample_size: int, optional
            Specifies the length of the :py:attr:`~properties` attributes stored as 1-dimensional
            :obj:`~numpy.ndarray`.  These :py:attr:`~properties` are initialized using
            :py:meth:`~init_scatter_properties`.

        read_sage_file: bool, optional
            Specifies whether to read the **SAGE** file and update the ``model_dict`` dict
            with the parameters specified inside.  If set to ``False``, all model
            parameters must be specified in ``model_dict`` instead.
        """

        # Need the snapshot to specify the name of the file. However, it's acceptable to
        # not have it named at initialization and call the Data Class specific
        # `update_snapshot` method later.
        try:
            self._snapshot = model_dict["snapshot"]
        except KeyError:
            pass

        # Use the SAGE parameter file to generate a bunch of attributes.
        if read_sage_file:
            sage_dict = self.read_sage_params(model_dict["sage_file"])
            model_dict.update(sage_dict)

        # Set the attributes.
        for key in model_dict:

            # We've already set the snapshot.
            if key == "snapshot":
                continue

            setattr(self, key, model_dict[key])

        self.num_files = 0

        # Then set default values.
        self._plot_galaxies = plot_galaxies
        self._sample_size = sample_size
        self.sSFRcut = -11.0  # The specific star formation rate above which a galaxy is
        # 'star forming'.  Units are log10.
        self._plot_output_format = "png"  # By default, save as a PNG.

        self._bins = {}
        self._properties = {}

    @property
    def plot_output_format(self):
        """
        int: Format plots for this model will be saved as.
        """

        return self._plot_output_format

    @plot_output_format.setter
    def plot_output_format(self, output_format):
        self._plot_output_format = output_format

    @property
    def snapshot(self):
        """
        int: Snapshot being read in and processed.
        """

        return self._snapshot

    @property
    def redshifts(self):
        """
        :obj:`~numpy.ndarray`: Redshifts for this simulation.
        """

        return self._redshifts

    @redshifts.setter
    def redshifts(self, redshifts):
        self._redshifts = redshifts

    @property
    def sage_output_format(self):
        """
        {``"sage_binary"``, ``"sage_binary"``}: The output format **SAGE** wrote in.
        A specific Data Class (e.g., :py:class:`~sage_analysis.sage_binary.SageBinaryData`
        and :py:class:`~sage_analysis.sage_hdf5.SageHdf5Data`) must be written and
        used for each :py:attr:`~sage_output_format` option. We refer to
        :doc:`../user/data_class` for more information about adding your own Data Class to ingest
        data.
        """

        return self._sage_output_format

    @sage_output_format.setter
    def sage_output_format(self, output_format):
        self._sage_output_format = output_format

    @property
    def model_path(self):
        """
        string: Path to the output data. If :py:attr:`~sage_output_format` is
        ``sage_binary``, files read must be labelled :py:attr:`~model_path`.XXX.
        If :py:attr:`~sage_output_format` is ``sage_hdf5``, the file read will be
        :py:attr:`~model_path` and the groups accessed will be Core_XXX at snapshot
        :py:attr:`~snapshot`. In both cases, ``XXX`` represents the numbers in the range
        [:py:attr:`~first_file`, :py:attr:`~last_file`] inclusive.
        """

        return self._model_path

    @model_path.setter
    def model_path(self, path):
        self._model_path = path

    @property
    def output_path(self):
        """
        string: Path to where some plots will be saved. Used for
        :py:meth:`!sage_analysis.plots.plot_spatial_3d`.
        """

        return self._output_path

    @output_path.setter
    def output_path(self, path):
        self._output_path = path

    @property
    def IMF(self):
        """
        {``"Chabrier"``, ``"Salpeter"``}: The initial mass function.
        """

        return self._IMF

    @IMF.setter
    def IMF(self, IMF):
        # Only allow Chabrier or Salpeter IMF.
        allowed_IMF = ["Chabrier", "Salpeter"]
        if IMF not in allowed_IMF:
            raise ValueErorr(
                "Value of IMF selected ({0}) is not allowed. Only {1} are "
                "allowed.".format(IMF, allowed_IMF)
            )
        self._IMF = IMF

    @property
    def label(self):
        """
        string: Label that will go on axis legends for this :py:class:`~Model`.
        """

        return self._label

    @label.setter
    def label(self, label):
        self._label = label

    @property
    def first_file(self):
        """
        int: The first *SAGE* sub-file to be read. If :py:attr:`~sage_output_format` is
        ``sage_binary``, files read must be labelled :py:attr:`~model_path`.XXX.
        If :py:attr:`~sage_output_format` is ``sage_hdf5``, the file read will be
        :py:attr:`~model_path` and the groups accessed will be Core_XXX. In both cases,
        ``XXX`` represents the numbers in the range
        [:py:attr:`~first_file`, :py:attr:`~last_file`] inclusive.
        """

        return self._first_file

    @first_file.setter
    def first_file(self, file_num):
        self._first_file = file_num

    @property
    def last_file(self):
        """
        int: The last **SAGE** sub-file to be read. If :py:attr:`~sage_output_format` is
        ``sage_binary``, files read must be labelled :py:attr:`~model_path`.XXX.
        If :py:attr:`~sage_output_format` is ``sage_hdf5``, the file read will be
        :py:attr:`~model_path` and the groups accessed will be Core_XXX. In both cases,
        ``XXX`` represents the numbers in the range
        [:py:attr:`~first_file`, :py:attr:`~last_file`] inclusive.
        """

        return self._last_file

    @last_file.setter
    def last_file(self, file_num):
        self._last_file = file_num

    @property
    def simulation(self):
        """
        {``"Mini-Millennium"``, ``"Millennium"``, ``"Genesis-L500-N2160"``}: Specifies the
        cosmoloogical values (Omega, box size, etc) for this :py:class:`~Model`.
        Only required if :py:attr:`~sage_output_format` is ``sage_binary``.
        Otherwise, if :py:attr:`sage_output_format` is ``sage_hdf5``, the
        parameters are read from the ``["Header"]["Simulation"]`` attributes.
        """

        return self._simulation

    @simulation.setter
    def simulation(self, simulation):
        self._simulation = simulation

    @property
    def snapshot(self):
        """
        int: Specifies the snapshot to be read. If :py:attr:`~sage_output_format` is
        ``sage_hdf5``, this specifies the HDF5 group to be read. Otherwise, if
        :py:attr:`sage_output_format` is ``sage_binary``, this attribute will be used to
        index :py:attr:`~redshifts` and generate the suffix for :py:attr:`~model_path`.
        """

        return self._snapshot

    @snapshot.setter
    def snapshot(self, snapshot):
        self._snapshot = snapshot

    @property
    def bins(self):
        """
        dict [string, :obj:`~numpy.ndarray` ]: The bins used to bin some
        :py:attr:`properties`. Bins are initialized through
        :py:meth:`~Model.init_binned_properties`. Key is the name of the bin,
        (``bin_name`` in :py:meth:`~Model.init_binned_properties` ).
        """

        return self._bins

    @property
    def properties(self):
        """
        dict [string, :obj:`~numpy.ndarray` ] or [string, float]: The galaxy properties
        stored across the input files. These properties are updated within the respective
        ``calc_<plot_toggle>`` functions. We refer to :doc:`../user/calc` for
        information on how :py:class:`~Model` properties are handled.
        """

        return self._properties

        self._plot_galaxies = plot_galaxies

    @property
    def plot_galaxies(self):
        """
        ``{"Random", "All"}``: Controls how galaxies will be plotted for scatter plots.
        If specified as ``"Random"`` will plot ``sample_size`` random galaxies.  If
        ``"All"``, will plot all galaxies. Selecting ``"All"`` may result in excessive
        memory consumption for very large simulations.
        """

        return self._plot_galaxies

    @property
    def sample_size(self):
        """
        int: Specifies the length of the :py:attr:`~properties` attributes stored as 1-dimensional
        :obj:`~numpy.ndarray`.  These :py:attr:`~properties` are initialized using
        :py:meth:`~init_scatter_properties`.
        """

        return self._sample_size

    @property
    def num_gals_all_files(self):
        """
        int: Number of galaxies across all files. For HDF5 data formats, this represents
        the number of galaxies across all `Core_XXX` sub-groups.
        """
        return self._num_gals_all_files

    @num_gals_all_files.setter
    def num_gals_all_files(self, num_gals):
        self._num_gals_all_files = num_gals

    def read_sage_params(self, sage_file_path):
        """
        Reads the **SAGE** parameter file values.

        Parameters
        ----------

        sage_file_path: String
            Path to the **SAGE** parameter file.

        Returns
        -------

        model_dict: dict [string, variable], optional
            Dictionary containing the parameter values for this class instance. Attributes
            of the class are set with name defined by the key with corresponding values.

        Errors
        ------

        FileNotFoundError
            Raised if the specified **SAGE** parameter file is not found.
        """

        # Fields that we will be reading from the ini file.
        SAGE_fields = [
            "FileNameGalaxies",
            "OutputDir",
            "FirstFile",
            "LastFile",
            "OutputFormat",
            "NumSimulationTreeFiles",
            "FileWithSnapList",
        ]
        SAGE_dict = {}

        # Ignore lines starting with one of these.
        comment_characters = [";", "%", "-"]

        try:
            with open(sage_file_path, "r") as SAGE_file:
                data = SAGE_file.readlines()

                # Each line in the parameter file is of the form...
                # parameter_name       parameter_value.
                for line in range(len(data)):

                    # Remove surrounding whitespace from the line.
                    stripped = data[line].strip()

                    # May have been an empty line.
                    try:
                        first_char = stripped[0]
                    except IndexError:
                        continue

                    # Check for comment.
                    if first_char in comment_characters:
                        continue

                    # Split into [name, value] list.
                    split = stripped.split()

                    # Then check if the field is one we care about.
                    if split[0] in SAGE_fields:

                        SAGE_dict[split[0]] = split[1]

        except FileNotFoundError:
            raise FileNotFoundError("Could not file SAGE ini file {0}".format(fname))

        # Now we have all the fields, rebuild the dictionary to be exactly what we need for
        # initialising the model.
        model_dict = {}

        alist = np.loadtxt(SAGE_dict["FileWithSnapList"])
        redshifts = 1.0 / alist - 1.0
        model_dict["redshifts"] = redshifts

        # If the output format was 'sage_binary', need to use the redshift. If the output
        # format was 'sage_hdf5', then we just append '.hdf5'.
        if SAGE_dict["OutputFormat"] == "sage_binary":

            try:
                snap = self.snapshot
            except KeyError:
                print(
                    "A Model was instantiated without specifying an initial snapshot to "
                    "read. This is allowed, but `Data_Class.update_snapshot` must be "
                    "called before any reading is done."
                )
                output_tag = "NOT_SET"
            else:
                output_tag = "_z{0:.3f}".format(redshifts[snap])

        elif SAGE_dict["OutputFormat"] == "sage_hdf5":

            output_tag = ".hdf5"

        model_path = "{0}/{1}{2}".format(
            SAGE_dict["OutputDir"], SAGE_dict["FileNameGalaxies"], output_tag
        )
        model_dict["model_path"] = model_path

        model_dict["sage_output_format"] = SAGE_dict["OutputFormat"]
        model_dict["output_path"] = "{0}/plots/".format(SAGE_dict["OutputDir"])
        model_dict["num_tree_files_used"] = (
            int(SAGE_dict["LastFile"]) - int(SAGE_dict["FirstFile"]) + 1
        )

        return model_dict

    def init_binned_properties(
        self, bin_low, bin_high, bin_width, bin_name, property_names
    ):
        """
        Initializes the :py:attr:`~properties` (and respective :py:attr:`~bins`) that will
        binned on some variable.  For example, the stellar mass function (SMF) will
        describe the number of galaxies within a stellar mass bin.

        :py:attr:`~bins` can be accessed via ``Model.bins["bin_name"]`` and are
        initialized as :obj:`~numpy.ndarray`. :py:attr:`~properties` can be accessed via
        ``Model.properties["property_name"]`` and are initialized using
        :obj:`numpy.zeros`.

        Parameters
        ----------

        bin_low, bin_high, bin_width : floats
            Values that define the minimum, maximum and width of the bins respectively.
            This defines the binning axis that the ``property_names`` properties will be
            binned on.

        bin_name : string
            Name of the binning axis, accessed by ``Model.bins["bin_name"]``.

        property_names : list of strings
            Name of the properties that will be binned along the defined binning axis.
            Properties can be accessed using ``Model.properties["property_name"]``; e.g.,
            ``Model.properties["SMF"]`` would return the stellar mass function that is binned
            using the ``bin_name`` bins.
        """

        # Parameters that define the specified binning axis.
        bins = np.arange(bin_low, bin_high + bin_width, bin_width)

        # Add the bins to the dictionary.
        self.bins[bin_name] = bins

        # When making histograms, the right-most bin is closed. Hence the length of the
        # produced histogram will be `len(bins)-1`.
        for my_property in property_names:
            self.properties[my_property] = np.zeros(len(bins) - 1, dtype=np.float64)

    def init_scatter_properties(self, property_names):
        """
        Initializes the :py:attr:`~properties` that will be extended as
        :obj:`~numpy.ndarray`. These are used to plot (e.g.,) a the star formation rate
        versus stellar mass for a subset of :py:attr:`~sample_size` galaxies. Initializes
        as empty :obj:`~numpy.ndarray`.

        Parameters
        ----------

        property_names : list of strings
            Name of the properties that will be extended as :obj:`~numpy.ndarray`.
        """

        # Initialize empty arrays.
        for my_property in property_names:
            self.properties[my_property] = np.array([])

    def init_single_properties(self, property_names):
        """
        Initializes the :py:attr:`~properties` that are described using a single number.
        This is used to plot (e.g.,) a the sum of stellar mass across all galaxies.
        Initializes as ``0.0``.

        Parameters
        ----------

        property_names : list of strings
            Name of the properties that will be described using a single number.
        """

        # Initialize as zeros.
        for my_property in property_names:
            self.properties[my_property] = 0.0

    def calc_properties_all_files(
        self, calculation_functions, close_file=True, use_pbar=True, debug=False
    ):
        """
        Calculates galaxy properties for all files of a single :py:class:`~Model`.

        Parameters
        ----------

        calculation_functions: dict [string, list(function, dict[string, variable])]
            Specifies the functions used to calculate the properties of this
            :py:class:`~Model`. The key of this dictionary is the name of the function.
            The value is a list with the 0th element being the function and the 1st
            element being a dictionary of additional keyword arguments to be passed to
            the function. The inner dictionary is keyed by the keyword argument names
            with the value specifying the keyword argument value.

            All functions in this dictionary for called after the galaxies for each
            sub-file have been loaded. The function signature is required to be
            ``func(Model, gals, <Extra Keyword Arguments>)``.

        close_file: boolean, optional
            Some data formats have a single file data is read from rather than opening and
            closing the sub-files in :py:meth:`read_gals`. Hence once the properties are
            calculated, the file must be closed. This variable flags whether the data
            class specific :py:meth:`~close_file` method should be called upon completion of
            this method.

        use_pbar: Boolean, optional
            If set, uses the ``tqdm`` package to create a progress bar.

        debug: Boolean, optional
            If set, prints out extra useful debug information.
        """

        start_time = time.time()

        # First determine how many galaxies are in all files.
        self.data_class.determine_num_gals(self)
        if self.num_gals_all_files == 0:
            print("There were no galaxies associated with this model.")
            return

        # If the user requested the number of galaxies plotted/calculated

        # The `tqdm` package provides a beautiful progress bar.
        try:
            if debug or not use_pbar:
                pbar = None
            else:
                pbar = tqdm(total=self.num_gals_all_files, unit="Gals", unit_scale=True)
        except NameError:
            pbar = None
        else:
            pass

        # Now read the galaxies and calculate the properties.
        for file_num in range(self.first_file, self.last_file + 1):

            # This is Data Class specific. Refer to the relevant module for implementation.
            gals = self.data_class.read_gals(self, file_num, pbar=pbar, debug=debug)

            # We may have skipped a file.
            if gals is None:
                continue

            self.calc_properties(calculation_functions, gals)

        # Some data formats (e.g., HDF5) have a single file we read from.
        # For other formats, this method doesn't exist. Note: If we're calculating
        # temporal results (i.e., running `history.py`) then we won't close here.
        if close_file:
            try:
                self.data_class.close_file(self)
            except AttributeError:
                pass

        end_time = time.time()
        duration = end_time - start_time

        if debug:
            print(
                "Took {0:.2f} seconds ({1:.2f} minutes)".format(duration, duration / 60.0)
            )
            print("")

    def calc_properties(self, calculation_functions, gals):
        """
        Calculates galaxy properties for a single file of galaxies.

        Parameters
        ----------

        calculation_functions: dict [string, function]
            Specifies the functions used to calculate the properties. All functions in
            this dictionary are called on the galaxies. The function signature is required
            to be ``func(Model, gals)``

        gals: exact format given by the :py:class:`~Model` Data Class.
            The galaxies for this file.

        Notes
        -----

        If :py:attr:`~sage_output_format` is ``sage_binary``, ``gals`` is a ``numpy``
        structured array. If :py:attr:`~sage_output_format`: is
        ``sage_hdf5``, ``gals`` is an open HDF5 group. We refer to
        :doc:`../user/data_class` for more information about adding your own Data Class to ingest data.
        """

        # Now check which plots the user is creating and hence decide which properties
        # they need.
        for func_name in calculation_functions.keys():
            func = calculation_functions[func_name][0]
            keyword_args = calculation_functions[func_name][1]

            # **keyword_args unpacks the `keyword_args` dictionary, passing each keyword
            # properly to the function.
            func(self, gals, **keyword_args)
