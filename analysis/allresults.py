#!/usr/bin/env python
"""
Main driver script to handle plotting the output of the ``SAGE`` model.  Multiple models
can be placed onto the plots by extending the variables in the ``__main__`` function call.

Authors: Jacob Seiler, Manodeep Sinha, Darren Croton
"""

import time
import numpy as np

from os.path import getsize as getFileSize
from scipy import stats

try:
    from tqdm import tqdm
except ImportError:
    print("Package 'tqdm' not found. Not showing pretty progress bars :(")
else:
    pass

import plots as plots

# Sometimes we divide a galaxy that has zero mass (e.g., no cold gas). Ignore these
# warnings as they spam stdout.
np.warnings.filterwarnings("ignore")

# Refer to the project for a list of TODO's, issues and other notes.
# https://github.com/sage-home/sage-model/projects/7


class Model:
    """
    Handles all the galaxy data (including calculated properties) for a SAGE model.

    The first 11 attributes (from ``model_path`` to ``marker``) are
    passed in a single dictionary (``model_dict``) to the class ``__init__``
    method.

    Attributes
    ----------

    model_path : String
        File path to the galaxy files.

        ..note:: Does not include the file number.

    output_path : String
        Directory path to where the some plots will be saved.  This will only be used in
        certain circumstances. In general, plots will be saved to the ``plot_output_path``
        path defined in the ``Results`` class.

    first_file, last_file : Integer
        Range (inclusive) of files that are read.

    simulation : {0, 1, 2, 3}
        Specifies which simulation this model corresponds to.
        0: Mini-millennium,
        1: Millennium,
        2: Kali (512^3 particles),
        3: Genesis (L = 500 Mpc/h, N = 2160^3).

    sSFRcut : Float
        The specific star formation rate which defines a "quiescent" galaxy. Units are
        log10.

    IMF : {0, 1}
        Specifies which IMF to use for this model.
        0: Salpeter,
        1: Chabrier.

    model_label : String
        Label placed on the legend for this model.

    color, linestyle : Strings
        Line color and style used for this model.

    marker : String
        The marker used for scatter plots for this model.

    hubble_h : Float
        Hubble 'little h' value. Between 0 and 1.

    BoxSize : Float
        Size of the simulation box for this model. Mpc/h.

    MaxTreeFiles : Integer
        Number of files generated from SAGE for this model.

    TODO: Should I list ALL the properties here? i.e., all the arrays and lists that hold
    the data?
    """

    def __init__(self, model_dict):
        """
        Sets the galaxy path and number of files to be read for a model.

        Parameters 
        ----------

        model_dict : Dictionary 
            Dictionary containing the parameter values for each ``Model``
            instance. Refer to the class-level documentation for a full
            description of this dictionary or to ``model_dict`` in the ``__main__`` call
            of this Module.

        Returns
        -------

        None.
        """

        # Set the attributes we were passed.
        for key in model_dict:
            setattr(self, key, model_dict[key])

        # Then set default values.
        self.sample_size = 10000  # How many values should we plot on scatter plots?

        # How should we bin Stellar mass.
        self.stellar_bin_low      = 7.5 
        self.stellar_bin_high     = 12.0 
        self.stellar_bin_width    = 0.1
        self.stellar_mass_bins    = np.arange(self.stellar_bin_low,
                                              self.stellar_bin_high + self.stellar_bin_width,
                                              self.stellar_bin_width)

        # These attributes will be binned on stellar mass.
        attr_names = ["SMF", "red_SMF", "blue_SMF", "BMF", "GMF",
                      "centrals_MF", "satellites_MF", "quiescent_galaxy_counts",
                      "quiescent_centrals_counts", "quiescent_satellites_counts",
                      "fraction_bulge_sum", "fraction_bulge_var",
                      "fraction_disk_sum", "fraction_disk_var"] 

        # When making histograms, the right-most bin is closed. Hence the length of the
        # produced histogram will be `len(bins)-1`.
        for attr in attr_names:
            setattr(self, attr, np.zeros(len(self.stellar_mass_bins)-1, dtype=np.float64))

        # How should we bin Halo mass.
        self.halo_bin_low      = 9.8 
        self.halo_bin_high     = 14.0 
        self.halo_bin_width    = 0.2
        self.halo_mass_bins    = np.arange(self.halo_bin_low,
                                           self.halo_bin_high + self.halo_bin_width,
                                           self.halo_bin_width)

        # These attributes will be binnned on halo mass.
        attr_names = ["fof_HMF"]
        component_names = ["halo_{0}_fraction_sum".format(component) for component in
                            ["baryon", "stars", "cold", "hot", "ejected", "ICS", "bh"]]

        for attr in (attr_names + component_names):
            setattr(self, attr, np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64))

        # Some plots use scattered points. For these, we will continually add to lists. 
        attr_names = ["BTF_mass", "BTF_vel", "sSFR_mass", "sSFR_sSFR", "gas_frac_mass", "gas_frac",
                      "metallicity_mass", "metallicity", "bh_mass", "bulge_mass",
                      "reservoir_mvir", "reservoir_stars", "reservoir_cold",
                      "reservoir_hot", "reservoir_ejected", "reservoir_ICS",
                      "x_pos", "y_pos", "z_pos"]

        # Hence let's initialize empty lists.
        for attr in attr_names:
            setattr(self, attr, [])


    def get_galaxy_struct(self):
        """
        Sets the ``numpy`` structured array for holding the galaxy data.

        Parameters 
        ----------

        None.

        Returns
        -------

        None. The attribute ``Model.galaxy_struct`` is updated within the function.
        """

        galdesc_full = [
            ("SnapNum"                      , np.int32),
            ("Type"                         , np.int32),
            ("GalaxyIndex"                  , np.int64),
            ("CentralGalaxyIndex"           , np.int64),
            ("SAGEHaloIndex"                , np.int32),
            ("SAGETreeIndex"                , np.int32),
            ("SimulationHaloIndex"          , np.int64),
            ("mergeType"                    , np.int32),
            ("mergeIntoID"                  , np.int32),
            ("mergeIntoSnapNum"             , np.int32),
            ("dT"                           , np.float32),
            ("Pos"                          , (np.float32, 3)),
            ("Vel"                          , (np.float32, 3)),
            ("Spin"                         , (np.float32, 3)),
            ("Len"                          , np.int32),
            ("Mvir"                         , np.float32),
            ("CentralMvir"                  , np.float32),
            ("Rvir"                         , np.float32),
            ("Vvir"                         , np.float32),
            ("Vmax"                         , np.float32),
            ("VelDisp"                      , np.float32),
            ("ColdGas"                      , np.float32),
            ("StellarMass"                  , np.float32),
            ("BulgeMass"                    , np.float32),
            ("HotGas"                       , np.float32),
            ("EjectedMass"                  , np.float32),
            ("BlackHoleMass"                , np.float32),
            ("IntraClusterStars"            , np.float32),
            ("MetalsColdGas"                , np.float32),
            ("MetalsStellarMass"            , np.float32),
            ("MetalsBulgeMass"              , np.float32),
            ("MetalsHotGas"                 , np.float32),
            ("MetalsEjectedMass"            , np.float32),
            ("MetalsIntraClusterStars"      , np.float32),
            ("SfrDisk"                      , np.float32),
            ("SfrBulge"                     , np.float32),
            ("SfrDiskZ"                     , np.float32),
            ("SfrBulgeZ"                    , np.float32),
            ("DiskRadius"                   , np.float32),
            ("Cooling"                      , np.float32),
            ("Heating"                      , np.float32),
            ("QuasarModeBHaccretionMass"    , np.float32),
            ("TimeOfLastMajorMerger"        , np.float32),
            ("TimeOfLastMinorMerger"        , np.float32),
            ("OutflowRate"                  , np.float32),
            ("infallMvir"                   , np.float32),
            ("infallVvir"                   , np.float32),
            ("infallVmax"                   , np.float32)
            ]
        names = [galdesc_full[i][0] for i in range(len(galdesc_full))]
        formats = [galdesc_full[i][1] for i in range(len(galdesc_full))]
        galdesc = np.dtype({"names":names, "formats":formats}, align=True)

        self.galaxy_struct = galdesc


    def set_cosmology(self, simulation, num_files_read):
        """
        Sets the relevant cosmological values, size of the simulation box and
        number of galaxy files.

        ..note:: ``box_size`` is in units of Mpc/h.

        Parameters 
        ----------

        simulation : {0, 1, 2, 3}
            Flags which simulation we are using.
            0: Mini-Millennium,
            1: Full Millennium,
            2: Kali (512^3 particles),
            3: Genesis (L = 500 Mpc/h, N = 2160^3).

        num_files_read : Integer
            How many files will be read for this model. Used to determine the effective
            volume of the simulation.

        Returns
        -------

        None.
        """

        if simulation == 0:  # Mini-Millennium.
            self.hubble_h = 0.73
            self.box_size = 62.5
            self.total_num_files = 1

        elif simulation == 1:  # Full Millennium.
            self.hubble_h = 0.73
            self.box_size = 500
            self.total_num_files = 512

        elif simulation == 2:  # Kali 512.
            self.hubble_h = 0.681
            self.box_size = 108.96
            self.total_num_files = 8

        elif simulation == 3:  # Genesis (L = 500 Mpc/h, N = 2160^3).
            self.hubble_h = 0.6751
            self.box_size = 500.00
            self.total_num_files = 64 

        else:
          print("Please pick a valid simulation!")
          exit(1)

        # Scale the volume by the number of files that we will read. Used to ensure
        # properties scaled by volume (e.g., SMF) gives sensible results.
        self.volume = pow(self.box_size, 3) * (num_files_read / self.total_num_files)


    def read_gals(self, fname, pbar=None, plot_galaxies=False, file_num=0, debug=False):
        """
        Reads a single galaxy file.

        Parameters 
        ----------

        fname : String
            Name of the file we're reading.

        pbar : ``tqdm`` class instance
            Bar showing the progress of galaxy reading.

        plot_galaxies : Boolean, default False
            If set, plots and saves the 3D distribution of galaxies for this file.

        file_num : Integer, default 0
            If ``plot_galaxies`` is set, this variable is used to name the output file
            showing the 3D distribution of galaxies.

        debug : Boolean, default False
            If set, prints out extra useful debug information.

            ..note:: ``tqdm`` does not play nicely with printing to stdout. Hence we
            disable the ``tqdm`` progress bar if ``debug=True``.

        Returns
        -------

        gals : ``numpy`` structured array with format given by ``Model.get_galaxy_struct()``
            The galaxies for this file.
        """

        if not os.path.isfile(fname):
            print("File\t{0} \tdoes not exist!".format(fname))
            raise FileNotFoundError

        if getFileSize(fname) == 0:
            print("File\t{0} \tis empty!".format(fname))
            raise ValueError

        with open(fname, "rb") as f:
            # First read the header information.
            Ntrees = np.fromfile(f, np.dtype(np.int32),1)[0]
            num_gals = np.fromfile(f, np.dtype(np.int32),1)[0]
            gals_per_tree = np.fromfile(f, np.dtype((np.int32, Ntrees)), 1)

            # Then the actual galaxies.
            gals = np.fromfile(f, self.galaxy_struct, num_gals)

            # If we're using the `tqdm` package, update the progress bar.
            if pbar:
                pbar.set_postfix(file=fname, refresh=False)
                pbar.update(num_gals)

        if debug:
            print("")
            print("File {0} contained {1} trees with {2} galaxies".format(fname, Ntrees, num_gals))

            w = np.where(gals["StellarMass"] > 1.0)[0]
            print("{0} of these galaxies have mass greater than 10^10Msun/h".format(len(w)))

        if plot_galaxies:

            # Show the distribution of galaxies in 3D.
            output_file = "./galaxies_{0}{1}".format(file_num, self.output_format)
            plots.plot_spatial_3d(gals, output_file, self.box_size)

        return gals


    def calc_properties_all_files(self, plot_toggles, model_path, first_file, last_file,
                                  debug=False):
        """
        Calculates galaxy properties for all files of a single Model.

        Parameters 
        ----------

        plot_toggles : Dictionary 
            Specifies which plots will be generated.

        model_path : String
            Base path to the galaxy files.  Does not include the file number or
            trailing underscore.

        first_file, last_file : Integers
            The file range to read.

        debug : Boolean, default False
            If set, prints out extra useful debug information.

        Returns
        -------

        None.
        """

        print("Processing galaxies and calculating properties for Model {0}".format(self.model_label))
        start_time = time.time()

        # First determine how many galaxies are in each file.
        self.num_gals = self.determine_num_gals(model_path, first_file, last_file)

        # The `tqdm` package provides a beautiful progress bar.
        try:
            if debug:
                pbar = None
            else:
                pbar = tqdm(total=self.num_gals, unit="Gals", unit_scale=True)
        except NameError:
            pbar = None
        else:
            pass

        # Now read the galaxies and calculate the properties.
        for file_num in range(first_file, last_file+1):
            fname = "{0}_{1}".format(model_path, file_num)                
            gals = self.read_gals(fname, pbar=pbar, file_num=file_num, debug=debug)
            self.calc_properties(plot_toggles, gals)

        end_time = time.time()
        duration = end_time - start_time
        print("Took {0:.2f} seconds ({1:.2f} minutes)".format(duration, duration/60.0))
        print("")


    def determine_num_gals(self, model_path, first_file, last_file):
        """
        Determines the number of galaxies in all files for this model.

        Parameters 
        ----------

        model_path : String
            Base path to the galaxy files.  Does not include the file number or
            trailing underscore.

        first_file, last_file : Integers
            The file range to read.

        Returns
        -------

        None.
        """

        num_gals = 0

        for file_num in range(first_file, last_file+1):

            fname = "{0}_{1}".format(model_path, file_num)

            if not os.path.isfile(fname):
                print("File\t{0} \tdoes not exist!".format(fname))
                raise FileNotFoundError 

            if getFileSize(fname) == 0:
                print("File\t{0} \tis empty!".format(fname))
                raise ValueError 

            with open(fname, "rb") as f:
                Ntrees = np.fromfile(f, np.dtype(np.int32),1)[0]
                num_gals_file = np.fromfile(f, np.dtype(np.int32),1)[0]

                num_gals += num_gals_file


        return num_gals


    def calc_properties(self, plot_toggles, gals):
        """
        Calculates galaxy properties for a single file of galaxies.

        ..note:: Refer to the class-level documentation for a full list of the
                 properties calculated and their associated units.

        Parameters 
        ----------

        plot_toggles : Dictionary 
            Specifies which plots will be generated. Used to determine which properties to
            calculate.

        gals : ``numpy`` structured array with format given by ``Model.get_galaxy_struct()``
            The galaxies for this file.

        Returns
        -------

        None.
        """

        # When we create some plots, we do a scatter plot. For these, we only plot a
        # subset of galaxies. We need to ensure we get a representative sample from each file.
        file_sample_size = int(len(gals) / self.num_gals * self.sample_size) 

        if plot_toggles["SMF"] or plot_toggles["sSFR"]:

            non_zero_stellar = np.where(gals["StellarMass"] > 0.0)[0]
            stellar_mass = np.log10(gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)
            sSFR = (gals["SfrDisk"][non_zero_stellar] + gals["SfrBulge"][non_zero_stellar]) / \
                   (gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)

            if plot_toggles["SMF"]:
                gals_per_bin, _ = np.histogram(stellar_mass, bins=self.stellar_mass_bins)
                self.SMF += gals_per_bin 

                # We often want to plot the red and blue subpopulations. So bin them as well.
                red_gals = np.where(sSFR < 10.0**self.sSFRcut)[0]
                red_mass = stellar_mass[red_gals]
                counts, _ = np.histogram(red_mass, bins=self.stellar_mass_bins)
                self.red_SMF += counts

                blue_gals = np.where(sSFR > 10.0**self.sSFRcut)[0]
                blue_mass = stellar_mass[blue_gals]
                counts, _ = np.histogram(blue_mass, bins=self.stellar_mass_bins)
                self.blue_SMF += counts

            if plot_toggles["sSFR"]:

                # `stellar_mass` and `sSFR` have length < length(gals).
                # Hence when we take a random sample, we use length of those arrays. 
                if len(non_zero_stellar) > file_sample_size:
                    random_inds = np.random.choice(np.arange(len(non_zero_stellar)), size=file_sample_size)
                else:
                    random_inds = np.arange(len(non_zero_stellar))

                self.sSFR_mass.extend(list(stellar_mass[random_inds]))
                self.sSFR_sSFR.extend(list(np.log10(sSFR[random_inds])))

        if plot_toggles["BMF"]:

            non_zero_baryon = np.where(gals["StellarMass"] + gals["ColdGas"] > 0.0)[0]
            baryon_mass = np.log10((gals["StellarMass"][non_zero_baryon] + gals["ColdGas"][non_zero_baryon]) * 1.0e10 \
                                   / self.hubble_h)

            (counts, binedges) = np.histogram(baryon_mass, bins=self.stellar_mass_bins)
            self.BMF += counts

        if plot_toggles["GMF"]:

            non_zero_cold = np.where(gals["ColdGas"] > 0.0)[0]
            cold_mass = np.log10(gals["ColdGas"][non_zero_cold] * 1.0e10 / self.hubble_h)

            (counts, binedges) = np.histogram(cold_mass, bins=self.stellar_mass_bins)
            self.GMF += counts

        if plot_toggles["BTF"]:

            # Make sure we're getting spiral galaxies. That is, don't include galaxies
            # that are too bulgy.
            spirals = np.where((gals["Type"] == 0) & (gals["StellarMass"] + gals["ColdGas"] > 0.0) & \
                               (gals["StellarMass"] > 0.0) & (gals["ColdGas"] > 0.0) & \
                               (gals["BulgeMass"] / gals["StellarMass"] > 0.1) & \
                               (gals["BulgeMass"] / gals["StellarMass"] < 0.5))[0]

            if len(spirals) > file_sample_size:
                spirals = np.random.choice(spirals, size=file_sample_size)

            baryon_mass = np.log10((gals["StellarMass"][spirals] + gals["ColdGas"][spirals]) * 1.0e10 / self.hubble_h)
            velocity = np.log10(gals["Vmax"][spirals])

            self.BTF_mass.extend(list(baryon_mass))
            self.BTF_vel.extend(list(velocity))

        if plot_toggles["gas_frac"]:

            # Make sure we're getting spiral galaxies. That is, don't include galaxies
            # that are too bulgy.
            spirals = np.where((gals["Type"] == 0) & (gals["StellarMass"] + gals["ColdGas"] > 0.0) & 
                                (gals["BulgeMass"] / gals["StellarMass"] > 0.1) & \
                                (gals["BulgeMass"] / gals["StellarMass"] < 0.5))[0]

            if len(spirals) > file_sample_size:
                spirals = np.random.choice(spirals, size=file_sample_size)
        
            stellar_mass = np.log10(gals["StellarMass"][spirals] * 1.0e10 / self.hubble_h)
            gas_fraction = gals["ColdGas"][spirals] / (gals["StellarMass"][spirals] + gals["ColdGas"][spirals])

            self.gas_frac_mass.extend(list(stellar_mass))
            self.gas_frac.extend(list(gas_fraction))

        if plot_toggles["metallicity"]:

            # Only care about central galaxies (Type 0) that have appreciable mass.
            centrals = np.where((gals["Type"] == 0) & (gals["ColdGas"] / (gals["StellarMass"] + gals["ColdGas"]) > 0.1) & \
                                (gals["StellarMass"] > 0.01))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)

            stellar_mass = np.log10(gals["StellarMass"][centrals] * 1.0e10 / self.hubble_h)
            Z = np.log10((gals["MetalsColdGas"][centrals] / gals["ColdGas"][centrals]) / 0.02) + 9.0

            self.metallicity_mass.extend(list(stellar_mass))
            self.metallicity.extend(list(Z))

        if plot_toggles["bh_bulge"]:

            # Only care about galaxies that have appreciable masses.
            my_gals = np.where((gals["BulgeMass"] > 0.01) & (gals["BlackHoleMass"] > 0.00001))[0]

            if len(my_gals) > file_sample_size:
                my_gals = np.random.choice(my_gals, size=file_sample_size)
            
            bh = np.log10(gals["BlackHoleMass"][my_gals] * 1.0e10 / self.hubble_h)
            bulge = np.log10(gals["BulgeMass"][my_gals] * 1.0e10 / self.hubble_h)
                    
            self.bh_mass.extend(list(bh))
            self.bulge_mass.extend(list(bulge))

        if plot_toggles["quiescent"]:

            non_zero_stellar = np.where(gals["StellarMass"] > 0.0)[0]

            mass = np.log10(gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)
            type = gals["Type"][non_zero_stellar]

            # For the sSFR, we will create a mask that is True for quiescent galaxies and
            # False for star-forming galaxies.
            sSFR = (gals["SfrDisk"][non_zero_stellar] + gals["SfrBulge"][non_zero_stellar]) / \
                   (gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)
            quiescent = sSFR < 10.0 ** self.sSFRcut

            if plot_toggles["SMF"]:
                pass
            else:
                gals_per_bin, _ = np.histogram(stellar_mass, bins=self.stellar_mass_bins)
                self.SMF += gals_per_bin 

            # Mass function for number of centrals/satellites.
            centrals_counts, _ = np.histogram(mass[type == 0], bins=self.stellar_mass_bins)
            self.centrals_MF += centrals_counts

            satellites_counts, _ = np.histogram(mass[type == 1], bins=self.stellar_mass_bins)
            self.satellites_MF += satellites_counts

            # Then bin those galaxies/centrals/satellites that are quiescent.
            quiescent_counts, _ = np.histogram(mass[quiescent], bins=self.stellar_mass_bins)
            self.quiescent_galaxy_counts += quiescent_counts

            quiescent_centrals_counts, _ = np.histogram(mass[(type == 0) & (quiescent == True)],
                                                        bins=self.stellar_mass_bins)
            self.quiescent_centrals_counts += quiescent_centrals_counts

            quiescent_satellites_counts, _ = np.histogram(mass[(type == 1) & (quiescent == True)],
                                                          bins=self.stellar_mass_bins)

            self.quiescent_satellites_counts += quiescent_satellites_counts 

        if plot_toggles["bulge_fraction"]:

            non_zero_stellar = np.where(gals["StellarMass"] > 0.0)[0]

            stellar_mass = np.log10(gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)
            fraction_bulge = gals["BulgeMass"][non_zero_stellar] / gals["StellarMass"][non_zero_stellar]
            fraction_disk = 1.0 - (gals["BulgeMass"][non_zero_stellar] / gals["StellarMass"][non_zero_stellar])

            # We want the mean bulge/disk fraction as a function of stellar mass. To allow
            # us to sum across each file, we will record the sum in each bin and then average later.
            if plot_toggles["SMF"]:
                pass
            else:
                gals_per_bin, _ = np.histogram(stellar_mass, bins=self.stellar_mass_bins)
                self.SMF += gals_per_bin 

            fraction_bulge_sum, _, _ = stats.binned_statistic(stellar_mass, fraction_bulge,
                                                              statistic=np.sum, bins=self.stellar_mass_bins)
            self.fraction_bulge_sum += fraction_bulge_sum

            fraction_disk_sum, _, _ = stats.binned_statistic(stellar_mass, fraction_disk,
                                                             statistic=np.sum, bins=self.stellar_mass_bins)
            self.fraction_disk_sum += fraction_disk_sum

            # For the variance, weight these by the total number of samples we will be
            # averaging over (i.e., number of files). 
            fraction_bulge_var, _, _ = stats.binned_statistic(stellar_mass, fraction_bulge,
                                                              statistic=np.var, bins=self.stellar_mass_bins)
            self.fraction_bulge_var += fraction_bulge_var / self.total_num_files

            fraction_disk_var, _, _ = stats.binned_statistic(stellar_mass, fraction_disk,
                                                             statistic=np.var, bins=self.stellar_mass_bins)
            self.fraction_disk_var += fraction_disk_var / self.total_num_files

        if plot_toggles["baryon_fraction"]:

            # Careful here, our "Halo Mass Function" is only counting the *BACKGROUND FOF HALOS*.
            centrals = np.where(gals["Type"] == 0)[0]
            centrals_fof_mass = np.log10(gals["Mvir"][centrals] * 1.0e10 / self.hubble_h)
            halos_binned, _ = np.histogram(centrals_fof_mass, bins=self.halo_mass_bins)
            self.fof_HMF += halos_binned

            non_zero_mvir = np.where((gals["CentralMvir"] > 0.0))[0]  # Will only be dividing by this value.

            # These are the *BACKGROUND FOF HALO* for each galaxy.
            fof_halo_mass = gals["CentralMvir"][non_zero_mvir]
            fof_halo_mass_log = np.log10(gals["CentralMvir"][non_zero_mvir] * 1.0e10 / self.hubble_h)

            # We want to calculate the fractions as a function of the FoF mass. To allow
            # us to sum across each file, we will record the sum in each bin and then
            # average later.
            components = ["StellarMass", "ColdGas", "HotGas", "EjectedMass",
                          "IntraClusterStars", "BlackHoleMass"]
            attrs_different_name = ["stars", "cold", "hot", "ejected", "ICS", "bh"]

            for (component_key, attr_name) in zip(components, attrs_different_name):

                # The bins are defined in log. However the other properties are all in 1.0e10 Msun/h.
                fraction_sum, _, _ = stats.binned_statistic(fof_halo_mass_log,
                                                            gals[component_key][non_zero_mvir] / fof_halo_mass,
                                                            statistic=np.sum, bins=self.halo_mass_bins)

                attribute_name = "halo_{0}_fraction_sum".format(attr_name)
                new_attribute_value = fraction_sum + getattr(self, attribute_name)
                setattr(self, attribute_name, new_attribute_value)

            # Finally want the sum across all components.
            baryons = np.sum(gals[component_key][non_zero_mvir] for component_key in components)
            baryon_fraction_sum, _, _ = stats.binned_statistic(fof_halo_mass_log, baryons / fof_halo_mass,
                                                                statistic=np.sum, bins=self.halo_mass_bins)
            self.halo_baryon_fraction_sum += baryon_fraction_sum

        if plot_toggles["reservoirs"]:

            # To reduce scatter, only use galaxies in halos with mass > 1.0e10 Msun/h.
            centrals = np.where((gals["Type"] == 0) & (gals["Mvir"] > 1.0) & \
                                (gals["StellarMass"] > 0.0))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)

            reservoirs = ["Mvir", "StellarMass", "ColdGas", "HotGas",
                          "EjectedMass", "IntraClusterStars"]
            attribute_names = ["mvir", "stars", "cold", "hot", "ejected", "ICS"]

            for (reservoir, attribute_name) in zip(reservoirs, attribute_names):

                mass = np.log10(gals[reservoir][centrals] * 1.0e10 / self.hubble_h)

                # Extend the previous list of masses with these new values. 
                attr_name = "reservoir_{0}".format(attribute_name)
                attribute_value = getattr(self, attr_name) 
                attribute_value.extend(list(mass))
                setattr(self, attribute_name, attribute_value)

        if plot_toggles["spatial"]:

            non_zero = np.where((gals["Mvir"] > 0.0) & (gals["StellarMass"] > 0.1))[0] 

            if len(non_zero) > file_sample_size:
                non_zero = np.random.choice(non_zero, size=file_sample_size)

            attribute_names = ["x_pos", "y_pos", "z_pos"]

            for (dim, attribute_name) in enumerate(attribute_names):

                # Units are Mpc/h.
                pos = gals["Pos"][non_zero, dim]

                attribute_value = getattr(self, attribute_name)
                attribute_value.extend(list(pos))
                setattr(self, attribute_name, attribute_value)


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

            model = Model(model_dict)

            model.get_galaxy_struct()
            model.set_cosmology(model_dict["simulation"],
                                model_dict["last_file"]-model_dict["first_file"]+1)

            # To be more memory concious, we calculate the required properties on a
            # file-by-file basis. This ensures we do not keep ALL the galaxy data in memory. 
            model.calc_properties_all_files(plot_toggles, model_dict["model_path"],
                                            model_dict["first_file"],
                                            model_dict["last_file"], debug=debug) 

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
    model0_dir_name   = "../output/millennium/"
    model0_file_name  = "model_z0.000"
    model0_first_file = 0
    model0_last_file  = 0 
    model0_simulation = 0  # Mini-millennium.
    model0_IMF        = 0  # Chabrier.
    model0_sSFRcut    = -11.0

    # Aesthetic variables for plotting pretty plots.
    model0_model_label = "Mini-Millennium"
    model0_color       = "r"
    model0_linestyle   = "-"
    model0_marker      = "x"

    # Then extend each of these lists for all the models that you want to plot.
    dir_names    = [model0_dir_name]
    file_names   = [model0_file_name]
    first_files  = [model0_first_file]
    last_files   = [model0_last_file]
    simulations  = [model0_simulation]
    IMFs         = [model0_IMF]
    sSFRcuts     = [model0_sSFRcut]
    model_labels = [model0_model_label]
    colors       = [model0_color]
    linestyles   = [model0_linestyle]
    markers      = [model0_marker]

    # A couple of extra variables...
    output_format    = ".png"
    plot_output_path = "./plots"

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

    model_dict = { "model_path"  : model_paths,
                   "output_path" : output_paths,
                   "first_file"  : first_files,
                   "last_file"   : last_files,
                   "simulation"  : simulations,
                   "sSFRcut"     : sSFRcuts,
                   "IMF"         : IMFs,
                   "model_label" : model_labels,
                   "color"       : colors,
                   "linestyle"   : linestyles,
                   "marker"      : markers}

    # Read in the galaxies and calculate properties for each model.
    results = Results(model_dict, plot_toggles, plot_output_path, output_format,
                      debug=False)
    results.do_plots()
