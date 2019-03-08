#!/usr/bin/env python

import matplotlib
matplotlib.use('Agg')

import time
import h5py as h5
import numpy as np
import pylab as plt
from os.path import getsize as getFileSize
from scipy import stats

try:
    from tqdm import tqdm
except ImportError:
    print("Package 'tqdm' not found. Not showing pretty progress bars :(")
else:
    pass

from mpl_toolkits.mplot3d import Axes3D

import observations as obs

np.warnings.filterwarnings('ignore')

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

        # How should we bin Halo mass.
        self.halo_bin_low      = 8.0 
        self.halo_bin_high     = 14.0 
        self.halo_bin_width    = 0.2
        self.halo_mass_bins    = np.arange(self.halo_bin_low,
                                           self.halo_bin_high + self.halo_bin_width,
                                           self.halo_bin_width)


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

        self.fraction_bulge_sum = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.float64)
        self.fraction_bulge_var = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.float64)

        self.fraction_disk_sum = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.float64)
        self.fraction_disk_var = np.zeros(len(self.stellar_mass_bins)-1, dtype=np.float64) 

        self.fof_HMF = np.zeros(len(self.halo_mass_bins)-1, dtype=np.int64) 
        self.halo_baryon_fraction_sum = np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64) 
        self.halo_stars_fraction_sum = np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64) 
        self.halo_cold_fraction_sum = np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64) 
        self.halo_hot_fraction_sum = np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64) 
        self.halo_ejected_fraction_sum = np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64) 
        self.halo_ICS_fraction_sum = np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64) 
        self.halo_bh_fraction_sum = np.zeros(len(self.halo_mass_bins)-1, dtype=np.float64) 

        # How should we bin the Spin Parameter?
        self.spin_bin_low   = -0.02
        self.spin_bin_high  = 0.5
        self.spin_bin_width = 0.01
        self.spin_bins      = np.arange(self.spin_bin_low,
                                        self.spin_bin_high + self.spin_bin_width,
                                        self.spin_bin_width)
        self.spin_counts = np.zeros(len(self.spin_bins)-1, dtype=np.int64)

        # How should we bin the Velocity?
        self.vel_bin_low   = -40.0
        self.vel_bin_high  = 40.0
        self.vel_bin_width = 0.5
        self.vel_bins      = np.arange(self.vel_bin_low,
                                       self.vel_bin_high + self.vel_bin_width,
                                       self.vel_bin_width)
        self.los_vel_counts = np.zeros(len(self.vel_bins)-1, dtype=np.int64)
        self.x_vel_counts = np.zeros(len(self.vel_bins)-1, dtype=np.int64)
        self.y_vel_counts = np.zeros(len(self.vel_bins)-1, dtype=np.int64)
        self.z_vel_counts = np.zeros(len(self.vel_bins)-1, dtype=np.int64)

        self.reservoir_mvir = []
        self.reservoir_stars = []
        self.reservoir_cold = []
        self.reservoir_hot = []
        self.reservoir_ejected = []
        self.reservoir_ICS = []


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
        print(file_sample_size)

        if plot_toggles["SMF"] or plot_toggles["sSFR"]:

            non_zero_stellar = np.where(gals["StellarMass"] > 0.0)[0]
            stellar_mass = np.log10(gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)
            sSFR = (gals["SfrDisk"][non_zero_stellar] + gals["SfrBulge"][non_zero_stellar]) / \
                   (gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)

            if plot_toggles["SMF"]:
                gals_per_bin, _ = np.histogram(stellar_mass, bins=self.stellar_mass_bins)
                self.SMF += gals_per_bin 

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

        if plot_toggles["bulge_fraction"]:

            non_zero_stellar = np.where(gals["StellarMass"] > 0.0)[0]

            stellar_mass = np.log10(gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)
            sSFR = (gals["SfrDisk"][non_zero_stellar] + gals["SfrBulge"][non_zero_stellar]) / \
                   (gals["StellarMass"][non_zero_stellar] * 1.0e10 / self.hubble_h)

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

        if plot_toggles["spin"]:

            spin_param = np.sqrt(gals["Spin"][:,0]*gals["Spin"][:,0] + \
                                 gals["Spin"][:,1]*gals["Spin"][:,1] + \
                                 gals["Spin"][:,2]*gals["Spin"][:,2]) / \
                                (np.sqrt(2) * gals["Vvir"] * gals["Rvir"]);

            spin_counts, _ = np.histogram(spin_param, bins=self.spin_bins)
            self.spin_counts += spin_counts

        if plot_toggles["velocity"]:

            pos_x = gals["Pos"][:,0] / self.hubble_h
            pos_y = gals["Pos"][:,1] / self.hubble_h
            pos_z = gals["Pos"][:,2] / self.hubble_h

            vel_x = gals["Vel"][:,0]
            vel_y = gals["Vel"][:,1]
            vel_z = gals["Vel"][:,2]

            los_dist = np.sqrt(pos_x*pos_x + pos_y*pos_y + pos_z*pos_z)
            los_vel = (pos_x/los_dist)*vel_x + (pos_y/los_dist)*vel_y + (pos_z/los_dist)*vel_z

            attr_names = ["los", "x", "y", "z"]
            vels = [los_vel, vel_x, vel_y, vel_z]

            for (attr_name, vel) in zip(attr_names, vels):

                counts, _ = np.histogram(vel / (self.hubble_h*100.0), bins=self.vel_bins)

                attribute_name = "{0}_vel_counts".format(attr_name)
                new_attribute_value = counts + getattr(self, attribute_name)
                setattr(self, attribute_name, new_attribute_value)

        if plot_toggles["reservoirs"]:

            centrals = np.where((gals["Type"] == 0) & (gals["Mvir"] > 1.0) & \
                                (gals["StellarMass"] > 0.0))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)

            components = ["Mvir", "StellarMass", "ColdGas", "HotGas",
                          "EjectedMass", "IntraClusterStars"]
            attribute_names = ["mvir", "stars", "cold", "hot", "ejected", "ICS"]

            for (component, attribute_name) in zip(components, attribute_names):

                mass = np.log10(gals[component][centrals] * 1.0e10 / self.hubble_h)

                # Extend the previous list of masses with these new values. 
                attr_name = "reservoir_{0}".format(attribute_name)
                old_attribute_value = getattr(self, attr_name) 
                new_attribute_value = old_attribute_value.extend(list(mass))
                setattr(self, attribute_name, new_attribute_value)


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

    def __init__(self, all_models_dict, plot_toggles, plot_output_path,
                 output_format=".png", debug=0):
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
        self.plot_output_path = plot_output_path
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

        if plot_toggles["bulge_fraction"]:
            print("Plotting the bulge mass fraction.")
            self.plot_bulge_mass_fraction()

        if plot_toggles["baryon_fraction"]:
            print("Plotting the baryon fraction as a function of FoF Halo mass.")
            self.plot_baryon_fraction()

        if plot_toggles["spin"]:
            print("Plotting the Spin distribution.")
            self.plot_spin_distribution()

        if plot_toggles["velocity"]:
            print("Plotting the velocity distribution.")
            self.plot_velocity_distribution()

        if plot_toggles["reservoirs"]:
            print("Plotting a scatter of the mass reservoirs.")
            self.plot_mass_reservoirs()

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

        outputFile = "{0}/1.StellarMassFunction{1}".format(self.plot_output_path,
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

        outputFile = "{0}/2.BaryonicMassFunction{1}".format(self.plot_output_path, self.output_format) 
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

        outputFile = "{0}/3.GasMassFunction{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)  # Save the figure
        print("Saved file to {0}".format(outputFile))
        plt.close()

# ---------------------------------------------------------
    
    def plot_BTF(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

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
            
        outputFile = "{0}/4.BaryonicTullyFisher{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------
    
    def plot_sSFR(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

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
            
        outputFile = "{0}/5.SpecificStarFormationRate{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------

    def plot_gas_frac(self):
        
        fig = plt.figure()
        ax = fig.add_subplot(111)

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
           
        outputFile = "{0}/6.GasFraction{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------

    def plot_metallicity(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

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
       
        outputFile = "{0}/7.Metallicity{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------

    def plot_bh_bulge(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

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
            
        outputFile = "{0}/8.BlackHoleBulgeRelationship{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()
            
# ---------------------------------------------------------
    
    def plot_quiescent(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

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
            
        outputFile = "{0}/9.QuiescentFraction{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def plot_bulge_mass_fraction(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        for model in self.models:

            tag = model.tag
            color = model.color
            linestyle = model.linestyle

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.stellar_mass_bins + 0.5 * model.stellar_bin_width

            # Remember we need to average the properties in each bin.
            bulge_mean = model.fraction_bulge_sum / model.SMF
            disk_mean = model.fraction_disk_sum / model.SMF

            # The variance has already been weighted when we calculated it.
            bulge_var = model.fraction_bulge_var
            disk_var = model.fraction_disk_var

            # We will keep the colour scheme consistent, but change the line styles.
            ax.plot(bin_middles[:-1], bulge_mean, label=tag + " bulge",
                    color=color, linestyle="-")

            ax.fill_between(bin_middles[:-1], bulge_mean+bulge_var, bulge_mean-bulge_var,
                            facecolor=color, alpha=0.25)

            ax.plot(bin_middles[:-1], disk_mean, label=tag + " disk",
                    color=color, linestyle="--")
            ax.fill_between(bin_middles[:-1], disk_mean+disk_var, disk_mean-disk_var,
                            facecolor=color, alpha=0.25)

        ax.set_xlim([8.0, 12.0])
        ax.set_ylim([0.0, 1.05])

        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        ax.set_ylabel(r'$\mathrm{Stellar\ Mass\ Fraction}$')

        self.adjust_legend(ax, location="upper left", scatter_plot=0)

        outputFile = "{0}/10.BulgeMassFraction{1}".format(self.plot_output_path, self.output_format) 
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# ---------------------------------------------------------
    
    def plot_baryon_fraction(self, plot_sub_populations=1):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        for model in self.models:

            tag = model.tag
            color = model.color
            linestyle = model.linestyle

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.halo_mass_bins + 0.5 * model.halo_bin_width

            # Remember we need to average the properties in each bin.
            baryon_mean = model.halo_baryon_fraction_sum / model.fof_HMF
            stars_mean = model.halo_stars_fraction_sum / model.fof_HMF
            cold_mean = model.halo_cold_fraction_sum / model.fof_HMF
            hot_mean = model.halo_hot_fraction_sum / model.fof_HMF
            ejected_mean = model.halo_ejected_fraction_sum / model.fof_HMF
            ICS_mean = model.halo_ICS_fraction_sum / model.fof_HMF

            # We will keep the linestyle constant but change the color. 
            ax.plot(bin_middles[:-1], baryon_mean, label=tag + " Total",
                    color=color, linestyle=linestyle)

            if self.num_models == 1 or plot_sub_populations:
                ax.plot(bin_middles[:-1], stars_mean, label=tag + " Stars",
                        color='k', linestyle=linestyle)
                ax.plot(bin_middles[:-1], cold_mean, label=tag + " Cold",
                        color='b', linestyle=linestyle)
                ax.plot(bin_middles[:-1], hot_mean, label=tag + " Hot",
                        color='r', linestyle=linestyle)
                ax.plot(bin_middles[:-1], ejected_mean, label=tag + " Ejected",
                        color='g', linestyle=linestyle)
                ax.plot(bin_middles[:-1], ICS_mean, label=tag + " ICS",
                        color='y', linestyle=linestyle)

        ax.set_xlabel(r"$\mathrm{Central}\ \log_{10} M_{\mathrm{vir}}\ (M_{\odot})$")
        ax.set_ylabel(r"$\mathrm{Baryon\ Fraction}$")

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        ax.set_xlim([8.0, 14.0])
        ax.set_ylim([0.0, 0.23])

        self.adjust_legend(ax, location="upper left", scatter_plot=0)

        outputFile = "{0}/11.BaryonFraction{1}".format(self.plot_output_path, self.output_format)
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def plot_spin_distribution(self):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        max_counts = -999

        for model in self.models:

            tag = model.tag
            color = model.color
            linestyle = model.linestyle

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.spin_bins + 0.5 * model.spin_bin_width

            # Normalize by number of galaxies; allows better comparison between models.
            norm_count = model.spin_counts / model.num_gals / model.spin_bin_width
            ax.plot(bin_middles[:-1], norm_counts, label=tag, color=color, linestyle=linestyle)

            if np.max(norm_count):
                max_counts = np.max(norm_count)

        ax.set_xlim([-0.02, 0.5])
        ax.set_ylim([0.0, max_counts*1.15])

        ax.set_xlabel(r"$\mathrm{Spin\ Parameter}$")
        ax.set_ylabel(r"$\mathrm{Normalized Count}$")

        self.adjust_legend(ax, location="upper left", scatter_plot=0)

        outputFile = "{0}/12.SpinDistribution{1}".format(self.plot_output_path, self.output_format)
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def plot_velocity_distribution(self):
    
        fig = plt.figure()
        ax = fig.add_subplot(111)

        # First do some junk plotting to get legend correct. We keep the linestyles but
        # use different colours.
        for model in self.models:
            ax.plot(np.nan, np.nan, color='m', linestyle=model.linestyle,
                    label=model.tag)

        for (num_model, model) in enumerate(self.models):

            linestyle = model.linestyle

            # Set the x-axis values to be the centre of the bins.
            bin_middles = model.vel_bins + 0.5 * model.vel_bin_width

            labels = ["los", "x", "y", "z"]
            colors = ["k", "r", "g", "b"]
            vels = [model.los_vel_counts, model.x_vel_counts,
                    model.y_vel_counts, model.z_vel_counts]
            normalization = model.vel_bin_width * model.num_gals

            for (vel, label, color) in zip(vels, labels, colors):

                # We only want the labels to be plotted once.
                if self.num_models == 0:
                    label = label
                else:
                    label = ""

                ax.plot(bin_middles[:-1], vel / normalization, color=color, label=label,
                        linestyle=linestyle)

        ax.set_yscale("log", nonposy="clip")

        ax.set_xlim([-40, 40])
        ax.set_ylim([1e-5, 0.5])

        ax.set_xlabel(r"$\mathrm{Velocity / H}_{0}$")
        ax.set_ylabel(r"$\mathrm{Box\ Normalised\ Count}$")

        self.adjust_legend(ax, location="upper left", scatter_plot=0)

        outputFile = "{0}/13.VelocityDistribution{1}".format(self.plot_output_path, self.output_format)
        fig.savefig(outputFile)
        print("Saved file to {0}".format(outputFile))
        plt.close()

# --------------------------------------------------------

    def plot_mass_reservoirs(self):

        # This scatter plot will be messy so we're going to make one for each model.
        for model in self.models:

            fig = plt.figure()
            ax = fig.add_subplot(111)

            tag = model.tag
            marker = model.marker

            components = ["StellarMass", "ColdGas", "HotGas", "EjectedMass",
                          "IntraClusterStars"]
            attribute_names = ["stars", "cold", "hot", "ejected", "ICS"]
            labels = ["Stars", "Cold Gas", "Hot Gas", "Ejected Gas", "Intracluster Stars"]
            colors = ["k", "b", "r", "g", "y"]

            for (component, attribute_name, color, label) in zip(components,
                                                                 attribute_names, colors,
                                                                 labels):

                attr_name = "reservoir_{0}".format(attribute_name)
                ax.scatter(model.reservoir_mvir, getattr(model, attr_name), marker=marker,
                           s=0.3, color=color, label=label)

            ax.set_xlabel(r"$\log\ M_{\mathrm{vir}}\ (M_{\odot})$")
            ax.set_ylabel(r"$\mathrm{Reservoir\ Mass\ (M_{\odot})}$")

            ax.set_xlim([10.0, 14.0])
            ax.set_ylim([7.5, 12.5])

            self.adjust_legend(ax, location="upper left", scatter_plot=1)

            outputFile = "{0}/14.MassReservoirs_{1}{2}".format(self.plot_output_path, tag,
                                                               self.output_format)
            fig.savefig(outputFile)
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
                    "quiescent" : 0,
                    "bulge_fraction" : 0,
                    "baryon_fraction" : 0,
                    "spin" : 0,
                    "velocity" : 0,
                    "reservoirs" : 1}

    output_format = ".png"
    plot_output_path = "./plots"

    results = Results(model_dict, plot_toggles, plot_output_path, output_format,
                      debug=1)
    results.do_plots()
