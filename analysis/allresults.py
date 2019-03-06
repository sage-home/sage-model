#!/usr/bin/env python

import matplotlib
matplotlib.use('Agg')

import time
import h5py as h5
import numpy as np
import pylab as plt
from os.path import getsize as getFileSize

try:
    from tqdm import tqdm
except ImportError:
    print("Package 'tqdm' not found. Not showing pretty progress bars :(")
else:
    pass

from mpl_toolkits.mplot3d import Axes3D

import observations as obs

#np.warnings.filterwarnings('ignore')

# Refer to the project for a list of TODO's, issues and other notes.
# https://github.com/sage-home/sage-model/projects/7

# ================================================================================
# Basic variables
# ================================================================================

# Set up some basic attributes of the run

sSFRcut = -11.0     # Divide quiescent from star forming galaxies (when plotmags=0)

matplotlib.rcdefaults()
plt.rc('xtick', labelsize='x-large')
plt.rc('ytick', labelsize='x-large')
plt.rc('lines', linewidth='2.0')
plt.rc('legend', numpoints=1, fontsize='x-large')
plt.rc('text', usetex=True)


class Model:
    """
    Handles all the galaxy data (including calculated properties).

    The first 8 attributes (from ``model_path`` to ``linestyle``) are
    passed in a single dictionary (``model_dict``) to the class ``__init__``
    method.

    Attributes
    ----------

    model_path : string 
        File path to the galaxy files.

        ..note:: Does not include the file number.

    output_path : string
        Directory path to where the plots will be saved.

    first_file, last_file : int
        Range (inclusive) of files that are read.

    simulation : {0, 1, 2}
        Specifies which simulation this model corresponds to.
        0: Mini-millennium,
        1: Millennium,
        2: Kali (512^3 particles).

    IMF : {0, 1} 
        Specifies which IMF to used for this model.
        0: Salpeter,
        1: Chabrier.

    tag : string
        Tag placed on the legend for this model.

    color : string
        Line color used for this model.

    linestyle : string
        Line style used for this model

    hubble_h : float
        Hubble 'little h' value. Between 0 and 1.

    BoxSize : float
        Size of the simulation box for this model. Mpc/h.

    MaxTreeFiles : int
        Number of files generated from SAGE for this model.

    gals : Galaxy numpy structured array
        Galaxies read for this model.

    mass : List of floats, length is number of galaxies
        Mass of each galaxy. 1.0e10 Msun.

    stellar_sSFR : List of floats, length is number of galaxies with non-zero
                   stellar mass
        Specific star formation rate of those galaxies with a non-zero stellar
        mass. 

    baryon_mass : list of floats, length is number of galaxies with non-zero
                  stellar mass or cold gas
        Mass of the baryons (stellar mass + cold gas) of each galaxy. 1.0e10
        Msun.

    cold_mass : list of floats, length is the number of galaxies with non-zero
                cold gas.
        Mass of the cold gas within each galaxy. 1.0e10 Msun.

    cold_sSFR : List of floats, length is number of galaxies with non-zero cold
                gas mass
        Specific star formation rate of those galaxies with a non-zero cold gas 
        mass. 
    """

    def __init__(self, model_dict):
        """
        Sets the galaxy path and number of files to be read for a model.

        Parameters 
        ----------

        model_dict : Dictionary 
            Dictionary containing the parameter values for each ``Model``
            instance. Refer to the class-level documentation for a full
            description of this dictionary.

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
        self.stellar_bin_low      = 7.0 
        self.stellar_bin_high     = 13.0 
        self.stellar_bin_width    = 0.1
        self.stellar_mass_bins    = np.arange(self.stellar_bin_low,
                                              self.stellar_bin_high + self.stellar_bin_width,
                                              self.stellar_bin_width)
        self.N_stellar_mass_bins = len(self.stellar_mass_bins)

        # When making histograms, the right-most bin is closed. Hence the length of the
        # produced histogram will be `len(bins)-1`.
        self.SMF = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)
        self.red_SMF = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)
        self.blue_SMF = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)

        self.BMF = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)
        self.GMF = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)

        self.BTF_mass = []
        self.BTF_vel = []

        self.sSFR_mass = []
        self.sSFR_sSFR = []

        self.gas_frac_mass = []
        self.gas_frac = []

        self.metallicity_mass = []
        self.metallicity = []

        self.bh_mass = []
        self.bulge_mass = []

        self.quiescent_mass_counts = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)
        self.quiescent_mass_quiescent_counts = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)

        self.quiescent_mass_centrals_counts = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)
        self.quiescent_mass_centrals_quiescent_counts = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)

        self.quiescent_mass_satellites_counts = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)
        self.quiescent_mass_satellites_quiescent_counts = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.int64)

    def get_galaxy_struct(self):

        galdesc_full = [
            ('SnapNum'                      , np.int32),
            ('Type'                         , np.int32),
            ('GalaxyIndex'                  , np.int64),
            ('CentralGalaxyIndex'           , np.int64),
            ('SAGEHaloIndex'                , np.int32),
            ('SAGETreeIndex'                , np.int32),
            ('SimulationHaloIndex'          , np.int64),
            ('mergeType'                    , np.int32),
            ('mergeIntoID'                  , np.int32),
            ('mergeIntoSnapNum'             , np.int32),
            ('dT'                           , np.float32),
            ('Pos'                          , (np.float32, 3)),
            ('Vel'                          , (np.float32, 3)),
            ('Spin'                         , (np.float32, 3)),
            ('Len'                          , np.int32),
            ('Mvir'                         , np.float32),
            ('CentralMvir'                  , np.float32),
            ('Rvir'                         , np.float32),
            ('Vvir'                         , np.float32),
            ('Vmax'                         , np.float32),
            ('VelDisp'                      , np.float32),
            ('ColdGas'                      , np.float32),
            ('StellarMass'                  , np.float32),
            ('BulgeMass'                    , np.float32),
            ('HotGas'                       , np.float32),
            ('EjectedMass'                  , np.float32),
            ('BlackHoleMass'                , np.float32),
            ('IntraClusterStars'            , np.float32),
            ('MetalsColdGas'                , np.float32),
            ('MetalsStellarMass'            , np.float32),
            ('MetalsBulgeMass'              , np.float32),
            ('MetalsHotGas'                 , np.float32),
            ('MetalsEjectedMass'            , np.float32),
            ('MetalsIntraClusterStars'      , np.float32),
            ('SfrDisk'                      , np.float32),
            ('SfrBulge'                     , np.float32),
            ('SfrDiskZ'                     , np.float32),
            ('SfrBulgeZ'                    , np.float32),
            ('DiskRadius'                   , np.float32),
            ('Cooling'                      , np.float32),
            ('Heating'                      , np.float32),
            ('QuasarModeBHaccretionMass'    , np.float32),
            ('TimeOfLastMajorMerger'        , np.float32),
            ('TimeOfLastMinorMerger'        , np.float32),
            ('OutflowRate'                  , np.float32),
            ('infallMvir'                   , np.float32),
            ('infallVvir'                   , np.float32),
            ('infallVmax'                   , np.float32)
            ]
        names = [galdesc_full[i][0] for i in range(len(galdesc_full))]
        formats = [galdesc_full[i][1] for i in range(len(galdesc_full))]
        galdesc = np.dtype({'names':names, 'formats':formats}, align=True)

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

        if simulation == 0:    # Mini-Millennium
            self.hubble_h = 0.73
            self.box_size = 62.5
            self.total_num_files = 1

        elif simulation == 1:  # Full Millennium
            self.hubble_h = 0.73
            self.box_size = 500
            self.total_num_files = 512

        elif simulation == 2: # Kali 512
            self.hubble_h = 0.681
            self.box_size = 108.96
            self.total_num_files = 8

        elif simulation == 3:
            self.hubble_h = 0.6751
            self.box_size = 500.00
            self.total_num_files = 128

        else:
          print("Please pick a valid simulation!")
          exit(1)

        # Scale the volume by the number of files that we will read. Used to ensure
        # properties scaled by volume (e.g., SMF) gives sensible results.
        self.volume = pow(self.box_size, 3) * (num_files_read / self.total_num_files)


    def read_gals(self, fname, pbar=None, file_num=None, debug=0, plot_galaxies=0):
        """
        Reads a single galaxy file.

        Parameters 
        ----------

        fname : String
            Name of the file we're reading.

        Returns
        -------

        gals : ``numpy`` structured array with format given by ``get_galaxy_struct()``
            The galaxies for this file.
        """

        if not os.path.isfile(fname):
            print("File\t{0} \tdoes not exist!".format(fname))
            raise FileNotFoundError 

        if getFileSize(fname) == 0:
            print("File\t{0} \tis empty!".format(fname))
            raise ValueError 

        with open(fname, "rb") as f:
            Ntrees = np.fromfile(f, np.dtype(np.int32),1)[0]
            num_gals = np.fromfile(f, np.dtype(np.int32),1)[0]
            gals_per_tree = np.fromfile(f, np.dtype((np.int32, Ntrees)), 1)

            gals = np.fromfile(f, self.galaxy_struct, num_gals)

            if pbar:
                pbar.set_postfix(file=fname, refresh=False)
                pbar.update(num_gals)

        if debug:
            print("")
            print("File {0} contained {1} trees with {2} galaxies".format(fname, Ntrees, num_gals))

            w = np.where(gals["StellarMass"] > 1.0)[0]
            print("{0} of these galaxies have mass greater than 10^10Msun/h".format(len(w)))

        if plot_galaxies:

            fig = plt.figure()
            ax = fig.add_subplot(111, projection='3d')

            w = sample(list(np.arange(len(gals))), 10000)

            ax.scatter(gals["Pos"][w,0], gals["Pos"][w,1], gals["Pos"][w,2], alpha=0.5)

            ax.set_xlim([0.0, self.box_size])
            ax.set_ylim([0.0, self.box_size])
            ax.set_zlim([0.0, self.box_size])

            ax.set_xlabel(r"$\mathbf{x \: [h^{-1}Mpc]}$")
            ax.set_ylabel(r"$\mathbf{y \: [h^{-1}Mpc]}$")
            ax.set_zlabel(r"$\mathbf{z \: [h^{-1}Mpc]}$")

            outputFile = "./galaxies_{0}{1}".format(file_num, self.output_format)
            fig.savefig(outputFile)
            print("Saved file to {0}".format(outputFile))
            plt.close()

        return gals


    def calc_properties_all_files(self, plot_toggles, model_path, first_file, last_file,
                                  debug=0):
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

        Returns
        -------

        None.
        """

        print("Processing galaxies and calculating properties for Model {0}".format(self.tag))
        start_time = time.time()

        # First determine how many galaxies are in each file.
        self.num_gals = self.determine_num_gals(model_path, range(first_file, last_file+1), debug=debug)

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

        for file_num in range(first_file, last_file+1):
            fname = "{0}_{1}".format(model_path, file_num)                
            gals = self.read_gals(fname, pbar=pbar, file_num=file_num, debug=debug)
            self.calc_properties(plot_toggles, gals)

        end_time = time.time()
        duration = end_time - start_time
        print("Took {0:.2f} seconds ({1:.2f} minutes)".format(duration, duration/60.0))
        print("")


    def determine_num_gals(self, model_path, file_iterator, debug=0):

        num_gals = 0

        for file_num in file_iterator:

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
        Calculates galaxy properties for single Model.

        ..note:: Refer to the class-level documentation for a full list of the
                 properties calculated and their associated units.

        Parameters 
        ----------

        plot_toggles : Dictionary 
            Specifies which plots will be generated.

        model_path : String
            Base path to the galaxy files.  Does not include the file number or
            trailing underscore.

        first_file, last_file : Integers
            The file range to read.

        Returns
        -------

        None.
        """

        # When we create some plots, we do a scatter plot. For these, we only plot a
        # subset of galaxies. We need to ensure we get a representative sample from each
        # file.
        file_sample_size = int(len(gals) / self.num_gals * self.sample_size) 

        if plot_toggles["SMF"] or plot_toggles["sSFR"]:

            non_zero_stellar = np.where(gals["StellarMass"] > 0.0)[0]
            stellar_mass = np.log10(gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)
            sSFR = (gals["SfrDisk"][non_zero_stellar] + gals["SfrBulge"][non_zero_stellar]) / \
                   (gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)

            if plot_toggles["SMF"]:
                counts, _ = np.histogram(stellar_mass, bins=self.stellar_mass_bins)
                self.SMF += counts

                red_gals = np.where(sSFR < 10.0**sSFRcut)[0]
                red_mass = stellar_mass[red_gals]
                counts, _ = np.histogram(red_mass, bins=self.stellar_mass_bins)
                self.red_SMF += counts

                blue_gals = np.where(sSFR > 10.0**sSFRcut)[0]
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

            centrals = np.where((gals["Type"] == 0) & (gals["StellarMass"] + gals["ColdGas"] > 0.0) & \
                                (gals["StellarMass"] > 0.0) & (gals["ColdGas"] > 0.0) & \
                                (gals["BulgeMass"] / gals["StellarMass"] > 0.1) & \
                                (gals["BulgeMass"] / gals["StellarMass"] < 0.5))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)

            baryon_mass = np.log10((gals["StellarMass"][centrals] + gals["ColdGas"][centrals]) * 1.0e10 / self.hubble_h)
            velocity = np.log10(gals["Vmax"][centrals])

            self.BTF_mass.extend(list(baryon_mass))
            self.BTF_vel.extend(list(velocity))

        if plot_toggles["gas_frac"]:

            
            centrals = np.where((gals["Type"] == 0) & (gals["StellarMass"] + gals["ColdGas"] > 0.0) & 
                                (gals["BulgeMass"] / gals["StellarMass"] > 0.1) & \
                                (gals["BulgeMass"] / gals["StellarMass"] < 0.5))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)
        
            stellar_mass = np.log10(gals["StellarMass"][centrals] * 1.0e10 / self.hubble_h)
            gas_fraction = gals["ColdGas"][centrals] / (gals["StellarMass"][centrals] + gals["ColdGas"][centrals])

            self.gas_frac_mass.extend(list(stellar_mass))
            self.gas_frac.extend(list(gas_fraction))

        if plot_toggles["metallicity"]:

            centrals = np.where((gals["Type"] == 0) & (gals["ColdGas"] / (gals["StellarMass"] + gals["ColdGas"]) > 0.1) & \
                                (gals["StellarMass"] > 0.01))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)

            stellar_mass = np.log10(gals["StellarMass"][centrals] * 1.0e10 / self.hubble_h)
            Z = np.log10((gals["MetalsColdGas"][centrals] / gals["ColdGas"][centrals]) / 0.02) + 9.0

            self.metallicity_mass.extend(list(stellar_mass))
            self.metallicity.extend(list(Z))

        if plot_toggles["bh_bulge"]:

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
            quiescent = sSFR < 10.0 ** sSFRcut

            # There may be some duplication of calculations here with other
            # `plot_toggles`. However, the computational overhead is neglible compared to
            # ensuring that we're ALWAYS calculating the required values regardless of the
            # `plot_toggles` parameters.

            mass_counts, _ = np.histogram(mass, bins=self.stellar_mass_bins)
            mass_quiescent_counts, _ = np.histogram(mass[quiescent], bins=self.stellar_mass_bins)
            self.quiescent_mass_counts += mass_counts
            self.quiescent_mass_quiescent_counts += mass_quiescent_counts

            #print(self.quiescent_mass_quiescent_counts / self.quiescent_mass_counts)

            mass_centrals_counts, _ = np.histogram(mass[type == 0], bins=self.stellar_mass_bins)
            mass_centrals_quiescent_counts, _ = np.histogram(mass[(type == 0) & (quiescent == True)],
                                                             bins=self.stellar_mass_bins)
            self.quiescent_mass_centrals_counts += mass_centrals_counts
            self.quiescent_mass_centrals_quiescent_counts += mass_centrals_quiescent_counts

            mass_satellites_counts, _ = np.histogram(mass[type == 1], bins=self.stellar_mass_bins)
            mass_satellites_quiescent_counts, _ = np.histogram(mass[(type == 1) & (quiescent == True)],
                                                               bins=self.stellar_mass_bins)
            self.quiescent_mass_satellites_counts += mass_satellites_counts
            self.quiescent_mass_satellites_quiescent_counts += mass_satellites_quiescent_counts


class Results:
    """
    Handles the plotting of the models.

    Attributes
    ----------

    num_models : Integer
        Number of models being plotted.

    models : List of ``Model`` class instances with length ``num_models``
        Models that we will be plotting.

    plot_toggles : Dictionary
        Specifies which plots will be generated. An entry of `1` denotes
        plotting, otherwise it will be skipped.

    output_format : String
        Format the plots are saved as.
    """

    def __init__(self, all_models_dict, plot_toggles, output_format=".png", debug=0):
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

        output_format : String, default ".png"
            Format the plots will be saved as.

        debug : {0, 1}, default 0
            Flag whether to print out useful debugging information.

        Returns
        -------

        None.
        """

        self.num_models = len(all_models_dict["model_path"])
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


    def adjust_legend(self, ax, location="upper right", scatter_plot=0): 
        """
        Adjusts the legend of a specified axis.

        Parameters
        ----------

        ax : ``matplotlib`` axes object
            The axis whose legend we're adjusting

        location : String, default "upper right". See ``matplotlib`` docs for full options.
            Location for the legend to be placed.

        scatter_plot : {0, 1}
            For plots involved scattered-plotted data, we adjust the size and alpha of the
            legend points.

        Returns
        -------

        None. The legend is placed directly onto the axis.
        """

        legend = ax.legend(loc=location)
        handles = legend.legendHandles

        legend.draw_frame(False)

        # First adjust the text sizes.
        for t in legend.get_texts():
            t.set_fontsize('medium')

        # For scatter plots, we want to increase the marker size.
        if scatter_plot:
            for handle in handles:

                # We may have lines in the legend which we don't want to touch here.
                if isinstance(handle, matplotlib.collections.PathCollection):
                    handle.set_alpha(1.0) 
                    handle.set_sizes([10.0])


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
            self.plot_SMF()

        if plot_toggles["BMF"] == 1:
            print("Plotting the Baryon Mass Function.")
            self.plot_BMF()

        if plot_toggles["GMF"] == 1:
            print("Plotting the Cold Gas Mass Function.")
            self.plot_GMF()

        if plot_toggles["BTF"] == 1:
            print("Plotting the Baryonic Tully-Fisher relation.")
            self.plot_BTF()

        if plot_toggles["sSFR"] == 1:
            print("Plotting the specific star formation rate.")
            self.plot_sSFR()

        if plot_toggles["gas_frac"] == 1:
            print("Plotting the gas fraction.")
            self.plot_gas_frac()

        if plot_toggles["metallicity"] == 1:
            print("Plotting the metallicity.")
            self.plot_metallicity()

        if plot_toggles["bh_bulge"] == 1:
            print("Plotting the black hole-bulge relationship.")
            self.plot_bh_bulge()

        if plot_toggles["quiescent"] == 1:
            print("Plotting the fraction of quiescent galaxies.")
            self.plot_quiescent()
        #res.BulgeMassFraction(model.gals)
        #res.BaryonFraction(model.gals)
        #res.SpinDistribution(model.gals)
        #res.VelocityDistribution(model.gals)
        #res.MassReservoirScatter(G)
        #res.SpatialDistribution(model.gals)

# --------------------------------------------------------

    def plot_SMF(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # For scaling the observational data, we use the values of the zeroth
        # model. We also save the plots into the output directory of the zeroth
        # model. 
        zeroth_hubble_h = (self.models)[0].hubble_h
        zeroth_IMF = (self.models)[0].IMF
        zeroth_output_path = (self.models)[0].output_path

        ax = obs.plot_smf_data(ax, zeroth_hubble_h, zeroth_IMF) 

        # Go through each of the models and plot. 
        for model in self.models:

            tag = model.tag

            # If we only have one model, we will split it into red and blue
            # sub-populations.
            if len(self.models) > 1:
                color = model.color
                ls = model.linestyle
            else:
                color = "k"
                ls = "-"

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.stellar_mass_bins + 0.5 * model.stellar_bin_width

            # The SMF is normalized by the simulation volume which is in Mpc/h. 
            ax.plot(bin_middles[:-1], model.SMF/model.volume*pow(model.hubble_h, 3)/model.stellar_bin_width,
                    color=color, ls=ls, label=tag + " - All")

            # If we only have one model, plot the sub-populations.
            if self.num_models == 1:
                ax.plot(bin_middles[:-1], model.red_SMF/model.volume*pow(model.hubble_h, 3)/model.stellar_bin_width,
                        'r:', lw=2, label=tag + " - Red")
                ax.plot(bin_middles[:-1], model.blue_SMF/model.volume*pow(model.hubble_h, 3)/model.stellar_bin_width,
                        'b:', lw=2, label=tag + " - Blue")

        ax.set_yscale('log', nonposy='clip')
        ax.set_xlim([8.0, 12.5])
        ax.set_ylim([1.0e-6, 1.0e-1])

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')

        ax.text(12.2, 0.03, model.simulation, size = 'large')

        self.adjust_legend(ax, location="lower left", scatter_plot=0)

        outputFile = "{0}/1.StellarMassFunction{1}".format(zeroth_output_path,
                                                           self.output_format)
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# ---------------------------------------------------------

    def plot_BMF(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # For scaling the observational data, we use the values of the zeroth
        # model. We also save the plots into the output directory of the zeroth
        # model. 
        zeroth_hubble_h = (self.models)[0].hubble_h
        zeroth_IMF = (self.models)[0].IMF
        zeroth_output_path = (self.models)[0].output_path

        ax = obs.plot_bmf_data(ax, zeroth_hubble_h, zeroth_IMF) 

        for model in self.models:

            tag = model.tag
            color = model.color
            ls = model.linestyle

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.stellar_mass_bins + 0.5 * model.stellar_bin_width

            # The MF is normalized by the simulation volume which is in Mpc/h. 
            ax.plot(bin_middles[:-1], model.BMF/model.volume*pow(model.hubble_h, 3)/model.stellar_bin_width,
                    color=color, ls=ls, label=tag + " - All")

        ax.set_yscale('log', nonposy='clip')
        ax.set_xlim([8.0, 12.5])
        ax.set_ylim([1.0e-6, 1.0e-1])

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

        ax.set_xlabel(r'$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')

        self.adjust_legend(ax, location="lower left", scatter_plot=0)

        outputFile = "{0}/2.BaryonicMassFunction{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# ---------------------------------------------------------
   
    def plot_GMF(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # For scaling the observational data, we use the values of the zeroth
        # model. We also save the plots into the output directory of the zeroth
        # model. 
        zeroth_hubble_h = (self.models)[0].hubble_h
        zeroth_output_path = (self.models)[0].output_path

        obs.plot_gmf_data(ax, zeroth_hubble_h)

        for model in self.models:

            tag = model.tag
            color = model.color
            ls = model.linestyle

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.stellar_mass_bins + 0.5 * model.stellar_bin_width

            # The MMF is normalized by the simulation volume which is in Mpc/h. 
            ax.plot(bin_middles[:-1], model.GMF/model.volume*pow(model.hubble_h, 3)/model.stellar_bin_width,
                    color=color, ls=ls, label=tag + " - Cold Gas")

        ax.set_yscale('log', nonposy='clip')
        ax.set_xlim([8.0, 11.5])
        ax.set_ylim([1.0e-6, 1.0e-1])

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{X}}\ (M_{\odot})$')


        self.adjust_legend(ax, location="lower left", scatter_plot=0)

        outputFile = "{0}/3.GasMassFunction{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# ---------------------------------------------------------
    
    def plot_BTF(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # We save the plots into the output directory of the zeroth model. 
        zeroth_output_path = (self.models)[0].output_path

        ax = obs.plot_btf_data(ax) 

        for model in self.models:

            tag = model.tag
            color = model.color
            marker = model.marker

            ax.scatter(model.BTF_vel, model.BTF_mass, marker=marker, s=1,
                       color=color, alpha=0.5, label=tag + " Sb/c galaxies")

        ax.set_ylabel(r'$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$')
        ax.set_xlabel(r'$\log_{10}V_{max}\ (km/s)$')

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        ax.set_xlim([1.4, 2.6])
        ax.set_ylim([8.0, 12.0])

        self.adjust_legend(ax, scatter_plot=1)
            
        outputFile = "{0}/4.BaryonicTullyFisher{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------
    
    def plot_sSFR(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # We save the plots into the output directory of the zeroth model. 
        zeroth_output_path = (self.models)[0].output_path

        for model in self.models:

            tag = model.tag
            color = model.color
            marker = model.marker

            ax.scatter(model.sSFR_mass, model.sSFR_sSFR, marker=marker, s=1, color=color,
                       alpha=0.5, label=tag + "galaxies")

        # overplot dividing line between SF and passive
        w = np.arange(7.0, 13.0, 1.0)
        ax.plot(w, w/w*sSFRcut, 'b:', lw=2.0)

        ax.set_xlabel(r"$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$")
        ax.set_ylabel(r"$\log_{10}\ s\mathrm{SFR}\ (\mathrm{yr^{-1}})$")

        # Set the x and y axis minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        ax.set_xlim([8.0, 12.0])
        ax.set_ylim([-16.0, -8.0])

        self.adjust_legend(ax, scatter_plot=1)
            
        outputFile = "{0}/5.SpecificStarFormationRate{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------

    def plot_gas_frac(self):
        
        fig = plt.figure()
        ax = fig.add_subplot(111)

        # We save the plots into the output directory of the zeroth model. 
        zeroth_output_path = (self.models)[0].output_path

        for model in self.models:

            tag = model.tag
            color = model.color
            marker = model.marker

            ax.scatter(model.gas_frac_mass, model.gas_frac, marker=marker, s=1, color=color,
                       alpha=0.5, label=tag + " Sb/c galaxies")

        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\mathrm{Cold\ Mass\ /\ (Cold+Stellar\ Mass)}$')

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        ax.set_xlim([8.0, 12.0])
        ax.set_ylim([0.0, 1.0])

        self.adjust_legend(ax, scatter_plot=1)
           
        outputFile = "{0}/6.GasFraction{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------

    def plot_metallicity(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # We save the plots into the output directory of the zeroth model. 
        zeroth_output_path = (self.models)[0].output_path       
        zeroth_IMF = (self.models)[0].IMF

        ax = obs.plot_metallicity_data(ax, zeroth_IMF) 

        for model in self.models:

            tag = model.tag
            color = model.color
            marker = model.marker

            ax.scatter(model.metallicity_mass, model.metallicity, marker=marker, s=1, color=color,
                       alpha=0.5, label=tag + " galaxies")

        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        ax.set_ylabel(r'$12\ +\ \log_{10}[\mathrm{O/H}]$')

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        ax.set_xlim([8.0, 12.0])
        ax.set_ylim([8.0, 9.5])

        self.adjust_legend(ax, location="upper right", scatter_plot=1)
       
        outputFile = "{0}/7.Metallicity{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------

    def plot_bh_bulge(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # We save the plots into the output directory of the zeroth model. 
        zeroth_output_path = (self.models)[0].output_path       

        ax = obs.plot_bh_bulge_data(ax) 

        for model in self.models:

            tag = model.tag
            color = model.color
            marker = model.marker

            ax.scatter(model.bulge_mass, model.bh_mass, marker=marker, s=1, color=color,
                       alpha=0.5, label=tag + " galaxies")

        ax.set_xlabel(r'$\log\ M_{\mathrm{bulge}}\ (M_{\odot})$')  # and the x-axis labels
        ax.set_ylabel(r'$\log\ M_{\mathrm{BH}}\ (M_{\odot})$')  # Set the y...

        # Set the x and y axis minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        ax.set_xlim([8.0, 12.0])
        ax.set_ylim([6.0, 10.0])
            
        self.adjust_legend(ax, location="upper right", scatter_plot=1)
            
        outputFile = "{0}/8.BlackHoleBulgeRelationship{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------
    
    def plot_quiescent(self):
    

        fig = plt.figure()
        ax = fig.add_subplot(111)

        # We save the plots into the output directory of the zeroth model. 
        zeroth_output_path = (self.models)[0].output_path       

        for model in self.models:

            tag = model.tag
            color = model.color
            linestyle = model.linestyle

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.stellar_mass_bins + 0.5 * model.stellar_bin_width

            # We will keep the colour scheme consistent, but change the line styles.
            ax.plot(bin_middles[:-1], model.quiescent_mass_quiescent_counts / model.quiescent_mass_counts,
                    label=tag + " All", color=color, linestyle='-') 

            ax.plot(bin_middles[:-1], model.quiescent_mass_centrals_quiescent_counts / model.quiescent_mass_centrals_counts,
                    label=tag + " Centrals", color=color, linestyle='--') 

            ax.plot(bin_middles[:-1], model.quiescent_mass_satellites_quiescent_counts / model.quiescent_mass_satellites_counts,
                    label=tag + " Satellites", color=color, linestyle='-.') 

        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\mathrm{Quescient\ Fraction}$')
            
        # Set the x and y axis minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        ax.set_xlim([8.0, 12.0])
        ax.set_ylim([0.0, 1.05])

        self.adjust_legend(ax, location="upper left", scatter_plot=0)
            
        outputFile = "{0}/9.QuiescentFraction{1}".format(zeroth_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def BulgeMassFraction(self, G):
    
        print("Plotting the mass fraction of galaxies")
    
        seed(2222)

        fBulge = G.BulgeMass / G.StellarMass
        fDisk = 1.0 - (G.BulgeMass) / G.StellarMass
        mass = np.log10(G.StellarMass * 1.0e10 / self.hubble_h)
        sSFR = np.log10((G.SfrDisk + G.SfrBulge) / (G.StellarMass * 1.0e10 / self.hubble_h))
        
        binwidth = 0.2
        shift = binwidth/2.0
        mass_range = np.arange(8.5-shift, 12.0+shift, binwidth)
        bins = len(mass_range)
        
        fBulge_ave = np.zeros(bins)
        fBulge_var = np.zeros(bins)
        fDisk_ave = np.zeros(bins)
        fDisk_var = np.zeros(bins)
        
        for i in range(bins-1):
            w = np.where( (mass >= mass_range[i]) & (mass < mass_range[i+1]))[0]
            # w = np.where( (mass >= mass_range[i]) & (mass < mass_range[i+1]) & (sSFR < sSFRcut))[0]
            if(len(w) > 0):
                fBulge_ave[i] = np.mean(fBulge[w])
                fBulge_var[i] = np.var(fBulge[w])
                fDisk_ave[i] = np.mean(fDisk[w])
                fDisk_var[i] = np.var(fDisk[w])

        w = np.where(fBulge_ave > 0.0)[0]
        plt.plot(mass_range[w]+shift, fBulge_ave[w], 'r-', label='bulge')
        plt.fill_between(mass_range[w]+shift, 
            fBulge_ave[w]+fBulge_var[w], 
            fBulge_ave[w]-fBulge_var[w], 
            facecolor='red', alpha=0.25)

        w = np.where(fDisk_ave > 0.0)[0]
        plt.plot(mass_range[w]+shift, fDisk_ave[w], 'k-', label='disk stars')
        plt.fill_between(mass_range[w]+shift, 
            fDisk_ave[w]+fDisk_var[w], 
            fDisk_ave[w]-fDisk_var[w], 
            facecolor='black', alpha=0.25)

        plt.axis([mass_range[0], mass_range[bins-1], 0.0, 1.05])

        plt.ylabel(r'$\mathrm{Stellar\ Mass\ Fraction}$')  # Set the y...
        plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

        leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
                t.set_fontsize('medium')

        outputFile = OutputDir + '10.BulgeMassFraction' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# ---------------------------------------------------------
    
    def BaryonFraction(self, G):
    
        print("Plotting the average baryon fraction vs halo mass")
    
        seed(2222)
    
        plt.figure()  # New figure
        ax = plt.subplot(111)  # 1 plot on the figure
        
        HaloMass = np.log10(G.Mvir * 1.0e10 / self.hubble_h)
        Baryons = G.StellarMass + G.ColdGas + G.HotGas + G.EjectedMass + G.IntraClusterStars + G.BlackHoleMass

        MinHalo = 11.0
        MaxHalo = 16.0
        Interval = 0.1
        Nbins = int((MaxHalo-MinHalo)/Interval)
        HaloRange = np.arange(MinHalo, MaxHalo, Interval)
        
        MeanCentralHaloMass = []
        MeanBaryonFraction = []
        MeanBaryonFractionU = []
        MeanBaryonFractionL = []

        MeanStars = []
        MeanCold = []
        MeanHot = []
        MeanEjected = []
        MeanICS = []
        MeanBH = []

        for i in range(Nbins-1):
            
            w1 = np.where((G.Type == 0) & (HaloMass >= HaloRange[i]) & (HaloMass < HaloRange[i+1]))[0]
            HalosFound = len(w1)
            
            if HalosFound > 2:  
                
                BaryonFraction = []
                CentralHaloMass = []
                
                Stars = []
                Cold = []
                Hot = []
                Ejected = []
                ICS = []
                BH = []
                
                for j in range(HalosFound):
                    
                    w2 = np.where(G.CentralGalaxyIndex == G.CentralGalaxyIndex[w1[j]])[0]
                    CentralAndSatellitesFound = len(w2)
                    
                    if CentralAndSatellitesFound > 0:
                        BaryonFraction.append(sum(Baryons[w2]) / G.Mvir[w1[j]])
                        CentralHaloMass.append(np.log10(G.Mvir[w1[j]] * 1.0e10 / self.hubble_h))

                        Stars.append(sum(G.StellarMass[w2]) / G.Mvir[w1[j]])
                        Cold.append(sum(G.ColdGas[w2]) / G.Mvir[w1[j]])
                        Hot.append(sum(G.HotGas[w2]) / G.Mvir[w1[j]])
                        Ejected.append(sum(G.EjectedMass[w2]) / G.Mvir[w1[j]])
                        ICS.append(sum(G.IntraClusterStars[w2]) / G.Mvir[w1[j]])
                        BH.append(sum(G.BlackHoleMass[w2]) / G.Mvir[w1[j]])                        
                                
                MeanCentralHaloMass.append(np.mean(CentralHaloMass))
                MeanBaryonFraction.append(np.mean(BaryonFraction))
                MeanBaryonFractionU.append(np.mean(BaryonFraction) + np.var(BaryonFraction))
                MeanBaryonFractionL.append(np.mean(BaryonFraction) - np.var(BaryonFraction))
                
                MeanStars.append(np.mean(Stars))
                MeanCold.append(np.mean(Cold))
                MeanHot.append(np.mean(Hot))
                MeanEjected.append(np.mean(Ejected))
                MeanICS.append(np.mean(ICS))
                MeanBH.append(np.mean(BH))
                
                print("{0} {1} {2} {3}".format(i, HaloRange[i], HalosFound,
                                               np.mean(BaryonFraction)))
        
        plt.plot(MeanCentralHaloMass, MeanBaryonFraction, 'k-', label='TOTAL')#, color='purple', alpha=0.3)
        plt.fill_between(MeanCentralHaloMass, MeanBaryonFractionU, MeanBaryonFractionL, 
            facecolor='purple', alpha=0.25, label='TOTAL')
        
        plt.plot(MeanCentralHaloMass, MeanStars, 'k--', label='Stars')
        plt.plot(MeanCentralHaloMass, MeanCold, label='Cold', color='blue')
        plt.plot(MeanCentralHaloMass, MeanHot, label='Hot', color='red')
        plt.plot(MeanCentralHaloMass, MeanEjected, label='Ejected', color='green')
        plt.plot(MeanCentralHaloMass, MeanICS, label='ICS', color='yellow')
        # plt.plot(MeanCentralHaloMass, MeanBH, 'k:', label='BH')
        
        plt.xlabel(r'$\mathrm{Central}\ \log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')  # Set the x-axis label
        plt.ylabel(r'$\mathrm{Baryon\ Fraction}$')  # Set the y-axis label
            
        # Set the x and y axis minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
            
        plt.axis([10.8, 15.0, 0.0, 0.23])
            
        leg = plt.legend(bbox_to_anchor=[0.99, 0.6])
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
            t.set_fontsize('medium')
            
        outputFile = OutputDir + '11.BaryonFraction' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def SpinDistribution(self, G):
    
        print("Plotting the spin distribution of all galaxies")

        # set up figure
        plt.figure()
        ax = plt.subplot(111)
    
        SpinParameter = np.sqrt(G.Spin[:,0]*G.Spin[:,0] + G.Spin[:,1]*G.Spin[:,1] + G.Spin[:,2]*G.Spin[:,2]) / (np.sqrt(2) * G.Vvir * G.Rvir);
        
        mi = -0.02
        ma = 0.5
        binwidth = 0.01
        NB = (ma - mi) / binwidth

        (counts, binedges) = np.histogram(SpinParameter, range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        plt.plot(xaxeshisto, counts, 'k-', label='simulation')

        plt.axis([mi, ma, 0.0, max(counts)*1.15])

        plt.ylabel(r'$\mathrm{Number}$')  # Set the y...
        plt.xlabel(r'$\mathrm{Spin\ Parameter}$')  # and the x-axis labels

        leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
                t.set_fontsize('medium')

        outputFile = OutputDir + '12.SpinDistribution' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def VelocityDistribution(self, G):
    
        print("Plotting the velocity distribution of all galaxies")
    
        seed(2222)
    
        mi = -40.0
        ma = 40.0
        binwidth = 0.5
        NB = (ma - mi) / binwidth

        # set up figure
        plt.figure()
        ax = plt.subplot(111)

        pos_x = G.Pos[:,0] / self.hubble_h
        pos_y = G.Pos[:,1] / self.hubble_h
        pos_z = G.Pos[:,2] / self.hubble_h

        vel_x = G.Vel[:,0]
        vel_y = G.Vel[:,1]
        vel_z = G.Vel[:,2]

        dist_los = np.sqrt(pos_x*pos_x + pos_y*pos_y + pos_z*pos_z)
        vel_los = (pos_x/dist_los)*vel_x + (pos_y/dist_los)*vel_y + (pos_z/dist_los)*vel_z
        dist_red = dist_los + vel_los/(self.hubble_h*100.0)

        tot_gals = len(pos_x)


        (counts, binedges) = np.histogram(vel_los/(self.hubble_h*100.0), range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        plt.plot(xaxeshisto, counts / binwidth / tot_gals, 'k-', label='los-velocity')

        (counts, binedges) = np.histogram(vel_x/(self.hubble_h*100.0), range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        plt.plot(xaxeshisto, counts / binwidth / tot_gals, 'r-', label='x-velocity')

        (counts, binedges) = np.histogram(vel_y/(self.hubble_h*100.0), range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        plt.plot(xaxeshisto, counts / binwidth / tot_gals, 'g-', label='y-velocity')

        (counts, binedges) = np.histogram(vel_z/(self.hubble_h*100.0), range=(mi, ma), bins=NB)
        xaxeshisto = binedges[:-1] + 0.5 * binwidth
        plt.plot(xaxeshisto, counts / binwidth / tot_gals, 'b-', label='z-velocity')


        plt.yscale('log', nonposy='clip')
        plt.axis([mi, ma, 1e-5, 0.5])
        # plt.axis([mi, ma, 0, 0.13])

        plt.ylabel(r'$\mathrm{Box\ Normalised\ Count}$')  # Set the y...
        plt.xlabel(r'$\mathrm{Velocity / H}_{0}$')  # and the x-axis labels

        leg = plt.legend(loc='upper left', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
                t.set_fontsize('medium')

        outputFile = OutputDir + '13.VelocityDistribution' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def MassReservoirScatter(self, G):
    
        print("Plotting the mass in stellar, cold, hot, ejected, ICS reservoirs")
    
        seed(2222)
    
        plt.figure()  # New figure
        ax = plt.subplot(111)  # 1 plot on the figure
    
        w = np.where((G.Type == 0) & (G.Mvir > 1.0) & (G.StellarMass > 0.0))[0]
        if(len(w) > dilute): w = sample(w, dilute)

        mvir = np.log10(G.Mvir[w] * 1.0e10)
        plt.scatter(mvir, np.log10(G.StellarMass[w] * 1.0e10), marker='o', s=0.3, c='k', alpha=0.5, label='Stars')
        plt.scatter(mvir, np.log10(G.ColdGas[w] * 1.0e10), marker='o', s=0.3, color='blue', alpha=0.5, label='Cold gas')
        plt.scatter(mvir, np.log10(G.HotGas[w] * 1.0e10), marker='o', s=0.3, color='red', alpha=0.5, label='Hot gas')
        plt.scatter(mvir, np.log10(G.EjectedMass[w] * 1.0e10), marker='o', s=0.3, color='green', alpha=0.5, label='Ejected gas')
        plt.scatter(mvir, np.log10(G.IntraClusterStars[w] * 1.0e10), marker='o', s=10, color='yellow', alpha=0.5, label='Intracluster stars')    

        plt.ylabel(r'$\mathrm{stellar,\ cold,\ hot,\ ejected,\ ICS\ mass}$')  # Set the y...
        plt.xlabel(r'$\log\ M_{\mathrm{vir}}\ (h^{-1}\ M_{\odot})$')  # and the x-axis labels
        
        plt.axis([10.0, 14.0, 7.5, 12.5])

        leg = plt.legend(loc='upper left')
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
            t.set_fontsize('medium')

        plt.text(13.5, 8.0, r'$\mathrm{All}')
            
        outputFile = OutputDir + '14.MassReservoirScatter' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def SpatialDistribution(self, G):
    
        print("Plotting the spatial distribution of all galaxies")
    
        seed(2222)
    
        plt.figure()  # New figure
    
        w = np.where((G.Mvir > 0.0) & (G.StellarMass > 0.1))[0]
        if(len(w) > dilute): w = sample(w, dilute)

        xx = G.Pos[w,0]
        yy = G.Pos[w,1]
        zz = G.Pos[w,2]

        buff = self.BoxSize*0.1

        ax = plt.subplot(221)  # 1 plot on the figure
        plt.scatter(xx, yy, marker='o', s=0.3, c='k', alpha=0.5)
        plt.axis([0.0-buff, self.BoxSize+buff, 0.0-buff, self.BoxSize+buff])

        plt.ylabel(r'$\mathrm{x}$')  # Set the y...
        plt.xlabel(r'$\mathrm{y}$')  # and the x-axis labels
        
        ax = plt.subplot(222)  # 1 plot on the figure
        plt.scatter(xx, zz, marker='o', s=0.3, c='k', alpha=0.5)
        plt.axis([0.0-buff, self.BoxSize+buff, 0.0-buff, self.BoxSize+buff])

        plt.ylabel(r'$\mathrm{x}$')  # Set the y...
        plt.xlabel(r'$\mathrm{z}$')  # and the x-axis labels
        
        ax = plt.subplot(223)  # 1 plot on the figure
        plt.scatter(yy, zz, marker='o', s=0.3, c='k', alpha=0.5)
        plt.axis([0.0-buff, self.BoxSize+buff, 0.0-buff, self.BoxSize+buff])
        plt.ylabel(r'$\mathrm{y}$')  # Set the y...
        plt.xlabel(r'$\mathrm{z}$')  # and the x-axis labels
            
        outputFile = OutputDir + '15.SpatialDistribution' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# =================================================================

if __name__ == '__main__':

    import os

    model0_dir_name = "/fred/oz070/jseiler/astro3d/jan2019/L500_N2160_take2/SAGE_output/"
    model0_file_name = "subvolume_SF0.10_z0.000"
    model0_first_file = 0
    model0_last_file = 63 
    model0_simulation = 3
    model0_IMF = 1
    model0_tag = r"$\mathbf{Genesis}$"
    model0_color = "r"
    model0_linestyle = "-"
    model0_marker = "x"

    model1_dir_name = "../output/millennium/"
    model1_file_name = "model_z0.000"
    model1_first_file = 0
    model1_last_file = 0
    model1_simulation = 0
    model1_IMF = 1
    model1_tag = r"$\mathbf{Mini-Millennium}$"
    model1_color = "b"
    model1_linestyle = "--"
    model1_marker = "o"

    dir_names = [model0_dir_name, model1_dir_name]
    file_names = [model0_file_name, model1_file_name]
    first_files = [model0_first_file, model1_first_file]
    last_files = [model0_last_file, model1_last_file]
    simulations = [model0_simulation, model1_simulation]
    IMFs = [model0_IMF, model1_IMF]
    tags = [model0_tag, model1_tag]
    colors = [model0_color, model1_color]
    linestyles = [model0_linestyle, model1_linestyle]
    markers = [model0_marker, model1_marker]

    model_paths = []
    output_paths = []

    for dir_name, file_name  in zip(dir_names, file_names):

        model_path = "{0}/{1}".format(dir_name, file_name) 
        model_paths.append(model_path)

        output_path = dir_name + "plots/"

        if not os.path.exists(output_path):
            os.makedirs(output_path)

        output_paths.append(output_path)

    print("Running allresults...")

    # First lets build a dictionary out of all the model parameters passed.
    model_dict = { "model_path"  : model_paths,
                   "output_path" : output_paths,
                   "first_file"  : first_files,
                   "last_file"   : last_files,
                   "simulation"  : simulations,
                   "IMF"         : IMFs,
                   "tag"         : tags,
                   "color"       : colors,
                   "linestyle"  : linestyles,
                   "marker"      : markers}

    plot_toggles = {"SMF"      : 0,
                    "BMF"      : 0,
                    "GMF"      : 0,
                    "BTF"      : 0,
                    "sSFR"     : 0,
                    "gas_frac" : 0,
                    "metallicity" : 0,
                    "bh_bulge" : 0,
                    "quiescent" : 1}

    output_format = ".png"

    results = Results(model_dict, plot_toggles, output_format, debug=1)
    results.do_plots()
