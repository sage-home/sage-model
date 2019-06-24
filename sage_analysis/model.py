#!/usr/bin/env python
"""
In this module, we define the ``Model`` superclass.  This class contains all the attributes
and methods for ingesting galaxy data and calculating properties.

The methods for reading setting the simulation cosmology and reading galaxy data are kept
in their own Data Class modules (e.g., ``sage_binary.py``).  If you wish to provide your own
data format, copy the format inside one of those modules.

To calculate extra properties, you need to specify both the x- and y-axes on which the
properties are calculated.

We offer three options for how the x-data is handled. Properties are stored as
``Model.properties["property_name"]``, e.g., ``Model.properties["SMF"]``:

    * Binned properties such as "How many galaxies with mass between 10^8.0 and 10^8.1
      Msun". Call ``Model.init_binned_properties`` with the parameters that define your bins
      and your new property. Bins will be stored as ``Model.bins["bin_name"]``, e.g.,
      ``Model.bins["stellar_mass_bins"]``

    * Scatter properties. These properties are useful if you want to (e.g.,) plot the star
      formation rate versus stellar mass for 1000 randomly selected points. Call
      ``Model.init_scatter_properties`` with your property names. The properties will be
      initialized as empty lists.

    * Single values. These properties could be (e.g.,) the total stellar mass summed across
      all galaxies. Call ``Model.init_single_properties`` with your property names. The
      properties will be initialized as values of 0.0.

To handle the y-data, you need to create a custom module (``.py`` file) and add new functions to it.
These functions are suggested to be named ``calc_<name of the plot_toggle you're using>()``.
The function signature MUST be ``func(Model, gals)``. Inside this function , you
must update the ``Model.properties[<Name of your new property>]`` attribute. Feel free to
base your function on an existing one used to calculate a property similar to your new property!

Finally, to plot your new property, refer to the documentation in the ``plots.py`` module.

Author: Jacob Seiler.
"""

import numpy as np
import time
from scipy import stats

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
            :py:attr:`~IMF`, :py:attr:`~model_label` and :py:attr:`~sage_file`. If
            ``read_sage_file`` is set to ``False``, all model parameters must be specified
            in this dict instead.

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
        self._sample_size = sample_size
        self.sSFRcut = -11.0  # The specific star formation rate above which a galaxy is
                              # 'star forming'.  Units are log10.

        self._bins = {}
        self._properties = {}

    @property
    def snapshot(self):
        """
        int: Snapshot being read in and processed.
        """

        return(self._snapshot)

    @property
    def redshifts(self):
        """
        :obj:`~numpy.ndarray`: Redshifts for this simulation.
        """

        return(self._redshifts)

    @redshifts.setter
    def redshifts(self, redshifts):
        self._redshifts = redshifts

    @property
    def sage_output_format(self):
        """
        {``"sage_binary"``, ``"sage_binary"``}: The output format *SAGE* wrote in.
        A specific Data Class (e.g., :py:class:`~sage_analysis.sage_binary.SageBinaryData`
        and :py:class:`~sage_analysis.sage_hdf5.SageHdf5Data`) must be written and
        used for each :py:attr:`~sage_output_format` option. We refer to
        :doc:`../user/data_class` for more information about adding your own Data Class to ingest
        data.
        """

        return(self._sage_output_format)

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

        return(self._model_path)

    @model_path.setter
    def model_path(self, path):
        self._model_path = path

    @property
    def output_path(self):
        """
        string: Path to where some plots will be saved. Used for
        :py:meth:`!sage_analysis.plots.plot_spatial_3d`.
        """

        return(self._output_path)

    @output_path.setter
    def output_path(self, path):
        self._output_path = path

    @property
    def IMF(self):
        """
        {``"Chabrier"``, ``"Salpeter"``}: The initial mass function.
        """

        return(self._IMF)

    @IMF.setter
    def IMF(self, IMF):
        # Only allow Chabrier or Salpeter IMF.
        allowed_IMF = ["Chabrier", "Salpeter"]
        if IMF not in allowed_IMF:
            raise ValueErorr("Value of IMF selected ({0}) is not allowed. Only {1} are "
                             "allowed.".format(IMF, allowed_IMF))
        self._IMF = IMF

    @property
    def model_label(self):
        """
        string: Label that will go on axis legends for this :py:class:`~Model`.
        """

        return(self._model_label)

    @model_label.setter
    def model_label(self, label):
        self._model_label = label

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

        return(self._first_file)

    @first_file.setter
    def first_file(self, file_num):
        self._first_file = file_num

    @property
    def last_file(self):
        """
        int: The last *SAGE* sub-file to be read. If :py:attr:`~sage_output_format` is
        ``sage_binary``, files read must be labelled :py:attr:`~model_path`.XXX.
        If :py:attr:`~sage_output_format` is ``sage_hdf5``, the file read will be
        :py:attr:`~model_path` and the groups accessed will be Core_XXX. In both cases,
        ``XXX`` represents the numbers in the range
        [:py:attr:`~first_file`, :py:attr:`~last_file`] inclusive.
        """

        return(self._last_file)

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

        return(self._simulation)

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

        return(self._snapshot)

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

        return(self._bins)

    @property
    def properties(self):
        """
        dict [string, :obj:`~numpy.ndarray` ] or [string, float]: The galaxy properties
        stored across the input files. These properties are updated within the respective
        ``calc_<plot_toggle>`` functions. We refer to :doc:`../user/calc` for
        information on how :py:class:`~Model` properties are handled.
        """

        return(self._properties)

    @property
    def sample_size(self):
        """
        int: Specifies the length of the :py:attr:`~properties` attributes stored as 1-dimensional
        :obj:`~numpy.ndarray`.  These :py:attr:`~properties` are initialized using
        :py:meth:`~init_scatter_properties`.
        """

        return(self._sample_size)


    def read_sage_params(self, sage_file_path):
        """
        Reads the **SAGE** parameter file values.

        Parameters
        ----------

        sage_file_path: String
            Path to the **SAGE** parameter file.

        Returns
        ---------

        model_dict: dict [string, variable], optional
            Dictionary containing the parameter values for this class instance. Attributes
            of the class are set with name defined by the key with corresponding values.

        Errors
        ---------

        FileNotFoundError
            Raised if the specified **SAGE** parameter file is not found.
        """

        SAGE_fields = ["FileNameGalaxies", "OutputDir",
                       "FirstFile", "LastFile", "OutputFormat", "NumSimulationTreeFiles",
                       "FileWithSnapList"]
        SAGE_dict = {}

        comment_characters = [";", "%", "-"]

        try:
            with open (sage_file_path, "r") as SAGE_file:
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
                    if split[0] in SAGE_fields:

                        SAGE_dict[split[0]] = split[1]

        except FileNotFoundError:
            raise FileNotFoundError("Could not file SAGE ini file {0}".format(fname))

        # Now we have all the fields, rebuild the dictionary to be exactly what we need for
        # initialising the model.
        model_dict = {}

        alist = np.loadtxt(SAGE_dict["FileWithSnapList"])
        redshifts = 1.0/alist - 1.0
        model_dict["redshifts"] = redshifts

        # If the output format was 'sage_binary', need to use the redshift. If the output
        # format was 'sage_hdf5', then we just append '.hdf5'.
        if SAGE_dict["OutputFormat"] == "sage_binary":

            try:
                snap = self.snapshot
            except KeyError:
                print("A Model was instantiated without specifying an initial snapshot to "
                      "read. This is allowed, but `Data_Class.update_snapshot` must be "
                      "called before any reading is done.")
                output_tag = "NOT_SET"
            else:
                output_tag = "_z{0:.3f}".format(redshifts[snap])

        elif SAGE_dict["OutputFormat"] == "sage_hdf5":

            output_tag = ".hdf5"

        model_path = "{0}/{1}{2}".format(SAGE_dict["OutputDir"],
                                         SAGE_dict["FileNameGalaxies"], output_tag)
        model_dict["model_path"] = model_path

        model_dict["sage_output_format"] = SAGE_dict["OutputFormat"]
        model_dict["output_path"] = "{0}/plots/".format(SAGE_dict["OutputDir"])
        model_dict["num_tree_files_used"] = int(SAGE_dict["LastFile"]) - \
                                            int(SAGE_dict["FirstFile"]) + 1

        return model_dict


    def init_binned_properties(self, bin_low, bin_high, bin_width, bin_name,
                               property_names):
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
        bins =  np.arange(bin_low, bin_high + bin_width, bin_width)

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


    def calc_properties_all_files(self, calculation_functions, close_file=True,
                                  use_pbar=True, debug=False):
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

        close_file: boolean, default ``True``
            Some data formats have a single file data is read from rather than opening and
            closing the sub-files in :py:meth:`read_gals`. Hence once the properties are
            calculated, the file must be closed. This variable flags whether the data
            class  specific :py:meth:`~close_file` method should be called upon completion of
            this method.

        use_pbar : Boolean, default ``True``
            If set, uses the ``tqdm`` package to create a progress bar.

        debug : Boolean, default ``False``
            If set, prints out extra useful debug information.
        """

        start_time = time.time()

        # First determine how many galaxies are in all files.
        self.data_class.determine_num_gals(self)
        if self.num_gals == 0:
            return

        # The `tqdm` package provides a beautiful progress bar.
        try:
            if debug or not use_pbar:
                pbar = None
            else:
                pbar = tqdm(total=self.num_gals, unit="Gals", unit_scale=True)
        except NameError:
            pbar = None
        else:
            pass

        # Now read the galaxies and calculate the properties.
        for file_num in range(self.first_file, self.last_file+1):

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
            print("Took {0:.2f} seconds ({1:.2f} minutes)".format(duration, duration/60.0))
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

        # When we create some plots, we do a scatter plot. For these, we only plot a
        # subset of galaxies. We need to ensure we get a representative sample from each file.
        self.file_sample_size = int(len(gals["StellarMass"][:]) / self.num_gals * self.sample_size)

        # Now check which plots the user is creating and hence decide which properties
        # they need.
        for func_name in calculation_functions.keys():
            func = calculation_functions[func_name][0]
            keyword_args = calculation_functions[func_name][1]

            func(self, gals, **keyword_args)


def calc_SMF(model, gals):

    non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]

    stellar_mass = np.log10(gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h)
    sSFR = (gals["SfrDisk"][:][non_zero_stellar] + gals["SfrBulge"][:][non_zero_stellar]) / \
               (gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h)

    gals_per_bin, _ = np.histogram(stellar_mass, bins=model.bins["stellar_mass_bins"])
    model.properties["SMF"] += gals_per_bin

    # We often want to plot the red and blue subpopulations. So bin them as well.
    red_gals = np.where(sSFR < 10.0**model.sSFRcut)[0]
    red_mass = stellar_mass[red_gals]
    counts, _ = np.histogram(red_mass, bins=model.bins["stellar_mass_bins"])
    model.properties["red_SMF"] += counts

    blue_gals = np.where(sSFR > 10.0**model.sSFRcut)[0]
    blue_mass = stellar_mass[blue_gals]
    counts, _ = np.histogram(blue_mass, bins=model.bins["stellar_mass_bins"])
    model.properties["blue_SMF"] += counts


def calc_BMF(model, gals):

    non_zero_baryon = np.where(gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0)[0]
    baryon_mass = np.log10((gals["StellarMass"][:][non_zero_baryon] + gals["ColdGas"][:][non_zero_baryon]) * 1.0e10 \
                           / model.hubble_h)

    (counts, _) = np.histogram(baryon_mass, bins=model.bins["stellar_mass_bins"])
    model.properties["BMF"] += counts


def calc_GMF(model, gals):

    non_zero_cold = np.where(gals["ColdGas"][:] > 0.0)[0]
    cold_mass = np.log10(gals["ColdGas"][:][non_zero_cold] * 1.0e10 / model.hubble_h)

    (counts, _) = np.histogram(cold_mass, bins=model.bins["stellar_mass_bins"])
    model.properties["GMF"] += counts


def calc_BTF(model, gals):

    # Make sure we're getting spiral galaxies. That is, don't include galaxies
    # that are too bulgy.
    spirals = np.where((gals["Type"][:] == 0) & (gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0) & \
                       (gals["StellarMass"][:] > 0.0) & (gals["ColdGas"][:] > 0.0) & \
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] > 0.1) & \
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] < 0.5))[0]

    if len(spirals) > model.file_sample_size:
        spirals = np.random.choice(spirals, size=model.file_sample_size)

    baryon_mass = np.log10((gals["StellarMass"][:][spirals] + gals["ColdGas"][:][spirals]) * 1.0e10 / model.hubble_h)
    velocity = np.log10(gals["Vmax"][:][spirals])

    model.properties["BTF_mass"] = np.append(model.properties["BTF_mass"], baryon_mass)
    model.properties["BTF_vel"] = np.append(model.properties["BTF_vel"], velocity)


def calc_sSFR(model, gals):

    non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]

    stellar_mass = np.log10(gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h)
    sSFR = (gals["SfrDisk"][:][non_zero_stellar] + gals["SfrBulge"][:][non_zero_stellar]) / \
               (gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h)

    # `stellar_mass` and `sSFR` have length < length(gals).
    # Hence when we take a random sample, we use length of those arrays.
    if len(non_zero_stellar) > model.file_sample_size:
        random_inds = np.random.choice(np.arange(len(non_zero_stellar)), size=model.file_sample_size)
    else:
        random_inds = np.arange(len(non_zero_stellar))

    model.properties["sSFR_mass"] = np.append(model.properties["sSFR_mass"], stellar_mass[random_inds])
    model.properties["sSFR_sSFR"] = np.append(model.properties["sSFR_sSFR"], np.log10(sSFR[random_inds]))


def calc_gas_frac(model, gals):

    # Make sure we're getting spiral galaxies. That is, don't include galaxies
    # that are too bulgy.
    spirals = np.where((gals["Type"][:] == 0) & (gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0) &
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] > 0.1) & \
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] < 0.5))[0]

    if len(spirals) > model.file_sample_size:
        spirals = np.random.choice(spirals, size=model.file_sample_size)

    stellar_mass = np.log10(gals["StellarMass"][:][spirals] * 1.0e10 / model.hubble_h)
    gas_fraction = gals["ColdGas"][:][spirals] / (gals["StellarMass"][:][spirals] + gals["ColdGas"][:][spirals])

    model.properties["gas_frac_mass"] = np.append(model.properties["gas_frac_mass"], stellar_mass)
    model.properties["gas_frac"] = np.append(model.properties["gas_frac"], gas_fraction)


def calc_metallicity(model, gals):

    # Only care about central galaxies (Type 0) that have appreciable mass.
    centrals = np.where((gals["Type"][:] == 0) & \
                        (gals["ColdGas"][:] / (gals["StellarMass"][:] + gals["ColdGas"][:]) > 0.1) & \
                        (gals["StellarMass"][:] > 0.01))[0]

    if len(centrals) > model.file_sample_size:
        centrals = np.random.choice(centrals, size=model.file_sample_size)

    stellar_mass = np.log10(gals["StellarMass"][:][centrals] * 1.0e10 / model.hubble_h)
    Z = np.log10((gals["MetalsColdGas"][:][centrals] / gals["ColdGas"][:][centrals]) / 0.02) + 9.0

    model.properties["metallicity_mass"] = np.append(model.properties["metallicity_mass"], stellar_mass)
    model.properties["metallicity"] = np.append(model.properties["metallicity"], Z)


def calc_bh_bulge(model, gals):

    # Only care about galaxies that have appreciable masses.
    my_gals = np.where((gals["BulgeMass"][:] > 0.01) & (gals["BlackHoleMass"][:] > 0.00001))[0]

    if len(my_gals) > model.file_sample_size:
        my_gals = np.random.choice(my_gals, size=model.file_sample_size)

    bh = np.log10(gals["BlackHoleMass"][:][my_gals] * 1.0e10 / model.hubble_h)
    bulge = np.log10(gals["BulgeMass"][:][my_gals] * 1.0e10 / model.hubble_h)

    model.properties["bh_mass"] = np.append(model.properties["bh_mass"], bh)
    model.properties["bulge_mass"] = np.append(model.properties["bulge_mass"], bulge)


def calc_quiescent(model, gals):

    non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]

    mass = np.log10(gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h)
    gal_type = gals["Type"][:][non_zero_stellar]

    # For the sSFR, we will create a mask that is True for quiescent galaxies and
    # False for star-forming galaxies.
    sSFR = (gals["SfrDisk"][:][non_zero_stellar] + gals["SfrBulge"][:][non_zero_stellar]) / \
           (gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h)
    quiescent = sSFR < 10.0 ** model.sSFRcut

    # When plotting, we scale the number of quiescent galaxies by the total number of
    # galaxies in that bin.  This is the Stellar Mass Function.
    # So check if the SMF has been initialized.  If not, then it should be specified.
    if model.calc_SMF:
        pass
    else:
        raise ValueError("When calculating the quiescent galaxy population, we "
                         "scale the results by the number of galaxies in each bin. "
                         "This requires the stellar mass function to be calculated. "
                         "Ensure that the 'SMF' plot toggle is switched on and that "
                         "the 'SMF' binned property is initialized.")

    # Mass function for number of centrals/satellites.
    centrals_counts, _ = np.histogram(mass[gal_type == 0], bins=model.bins["stellar_mass_bins"])
    model.properties["centrals_MF"] += centrals_counts

    satellites_counts, _ = np.histogram(mass[gal_type == 1], bins=model.bins["stellar_mass_bins"])
    model.properties["satellites_MF"] += satellites_counts

    # Then bin those galaxies/centrals/satellites that are quiescent.
    quiescent_counts, _ = np.histogram(mass[quiescent], bins=model.bins["stellar_mass_bins"])
    model.properties["quiescent_galaxy_counts"] += quiescent_counts

    quiescent_centrals_counts, _ = np.histogram(mass[(gal_type == 0) & (quiescent == True)],
                                                bins=model.bins["stellar_mass_bins"])
    model.properties["quiescent_centrals_counts"] += quiescent_centrals_counts

    quiescent_satellites_counts, _ = np.histogram(mass[(gal_type == 1) & (quiescent == True)],
                                                  bins=model.bins["stellar_mass_bins"])

    model.properties["quiescent_satellites_counts"] += quiescent_satellites_counts


def calc_bulge_fraction(model, gals):

    non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]

    stellar_mass = np.log10(gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h)
    fraction_bulge = gals["BulgeMass"][:][non_zero_stellar] / gals["StellarMass"][:][non_zero_stellar]
    fraction_disk = 1.0 - (gals["BulgeMass"][:][non_zero_stellar] / gals["StellarMass"][:][non_zero_stellar])

    # When plotting, we scale the fraction of each galaxy type the total number of
    # galaxies in that bin. This is the Stellar Mass Function.
    # So check if we're calculating the SMF already, and if not, calculate it here.
    if model.calc_SMF:
        pass
    else:
        raise ValueError("When calculating the bulge fraction, we "
                         "scale the results by the number of galaxies in each bin. "
                         "This requires the stellar mass function to be calculated. "
                         "Ensure that the 'SMF' plot toggle is switched on and that "
                         "the 'SMF' binned property is initialized.")


    # We want the mean bulge/disk fraction as a function of stellar mass. To allow
    # us to sum across each file, we will record the sum in each bin and then average later.
    fraction_bulge_sum, _, _ = stats.binned_statistic(stellar_mass, fraction_bulge,
                                                      statistic=np.sum,
                                                      bins=model.bins["stellar_mass_bins"])
    model.properties["fraction_bulge_sum"] += fraction_bulge_sum

    fraction_disk_sum, _, _ = stats.binned_statistic(stellar_mass, fraction_disk,
                                                     statistic=np.sum,
                                                     bins=model.bins["stellar_mass_bins"])
    model.properties["fraction_disk_sum"] += fraction_disk_sum

    # For the variance, weight these by the total number of samples we will be
    # averaging over (i.e., number of files).
    fraction_bulge_var, _, _ = stats.binned_statistic(stellar_mass, fraction_bulge,
                                                      statistic=np.var,
                                                      bins=model.bins["stellar_mass_bins"])
    model.properties["fraction_bulge_var"] += fraction_bulge_var / model.num_files

    fraction_disk_var, _, _ = stats.binned_statistic(stellar_mass, fraction_disk,
                                                     statistic=np.var,
                                                     bins=model.bins["stellar_mass_bins"])
    model.properties["fraction_disk_var"] += fraction_disk_var / model.num_files


def calc_baryon_fraction(model, gals):

    # Careful here, our "Halo Mass Function" is only counting the *BACKGROUND FOF HALOS*.
    centrals = np.where((gals["Type"][:] == 0) & (gals["Mvir"][:] > 0.0))[0]
    centrals_fof_mass = np.log10(gals["Mvir"][:][centrals] * 1.0e10 / model.hubble_h)
    halos_binned, _ = np.histogram(centrals_fof_mass, bins=model.bins["halo_mass_bins"])
    model.properties["fof_HMF"] += halos_binned

    non_zero_mvir = np.where((gals["CentralMvir"][:] > 0.0))[0]  # Will only be dividing by this value.

    # These are the *BACKGROUND FOF HALO* for each galaxy.
    fof_halo_mass = gals["CentralMvir"][:][non_zero_mvir]
    fof_halo_mass_log = np.log10(gals["CentralMvir"][:][non_zero_mvir] * 1.0e10 / model.hubble_h)

    # We want to calculate the fractions as a function of the FoF mass. To allow
    # us to sum across each file, we will record the sum in each bin and then
    # average later.
    components = ["StellarMass", "ColdGas", "HotGas", "EjectedMass",
                  "IntraClusterStars", "BlackHoleMass"]
    attrs_different_name = ["stars", "cold", "hot", "ejected", "ICS", "bh"]

    for (component_key, attr_name) in zip(components, attrs_different_name):

        # The bins are defined in log. However the other properties are all in 1.0e10 Msun/h.
        fraction_sum, _, _ = stats.binned_statistic(fof_halo_mass_log,
                                                    gals[component_key][:][non_zero_mvir] / fof_halo_mass,
                                                    statistic=np.sum,
                                                    bins=model.bins["halo_mass_bins"])

        dict_key = "halo_{0}_fraction_sum".format(attr_name)
        new_attribute_value = fraction_sum + model.properties[dict_key]
        model.properties[dict_key] = new_attribute_value

    # Finally want the sum across all components.
    baryons = np.sum(gals[component_key][:][non_zero_mvir] for component_key in components)
    baryon_fraction_sum, _, _ = stats.binned_statistic(fof_halo_mass_log, baryons / fof_halo_mass,
                                                        statistic=np.sum,
                                                        bins=model.bins["halo_mass_bins"])
    model.properties["halo_baryon_fraction_sum"] += baryon_fraction_sum


def calc_reservoirs(model, gals):

    # To reduce scatter, only use galaxies in halos with mass > 1.0e10 Msun/h.
    centrals = np.where((gals["Type"][:] == 0) & (gals["Mvir"][:] > 1.0) & \
                        (gals["StellarMass"][:] > 0.0))[0]

    if len(centrals) > model.file_sample_size:
        centrals = np.random.choice(centrals, size=model.file_sample_size)

    reservoirs = ["Mvir", "StellarMass", "ColdGas", "HotGas",
                  "EjectedMass", "IntraClusterStars"]
    attribute_names = ["mvir", "stars", "cold", "hot", "ejected", "ICS"]

    for (reservoir, attribute_name) in zip(reservoirs, attribute_names):

        mass = np.log10(gals[reservoir][:][centrals] * 1.0e10 / model.hubble_h)

        # Extend the previous list of masses with these new values.
        dict_key = "reservoir_{0}".format(attribute_name)
        model.properties[dict_key] = np.append(model.properties[dict_key], mass)


def calc_spatial(model, gals):

    non_zero = np.where((gals["Mvir"][:] > 0.0) & (gals["StellarMass"][:] > 0.1))[0]

    if len(non_zero) > model.file_sample_size:
        non_zero = np.random.choice(non_zero, size=model.file_sample_size)

    attribute_names = ["x_pos", "y_pos", "z_pos"]
    data_names = ["Posx", "Posy", "Posz"]

    for (attribute_name, data_name) in zip(attribute_names, data_names):

        # Units are Mpc/h.
        pos = gals[data_name][:][non_zero]

        model.properties[attribute_name] = np.append(model.properties[attribute_name], pos)


def calc_SFRD(model, gals):

    # Check if the Snapshot is required.
    if gals["SnapNum"][0] in model.density_snaps:

        SFR = gals["SfrDisk"][:] + gals["SfrBulge"][:]
        model.properties["SFRD"] += np.sum(SFR)


def calc_SMD(model, gals):

    # Check if the Snapshot is required.
    if gals["SnapNum"][0] in model.density_snaps:

        non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]
        stellar_mass = gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h

        model.properties["SMD"] += np.sum(stellar_mass)
