#!/usr/bin/env python
"""
In this module, we define the ``Model`` superclass.  This class contains all the attributes
and methods for ingesting galaxy data and calculating properties.

The methods for reading setting the simulation cosmology and reading galaxy data are kept
in their own subclass modules (e.g., ``sage_binary.py``).  If you wish to provide your own
data format, copy the format inside one of those modules.

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


class Model:
    """
    Handles all the galaxy data (including calculated properties) for a SAGE model.

    We have only listed the attributes set by the ``model_dict`` dictionary passed into
    the ``__init__`` method.  There are a large number of attributes dealing with all the
    calculated data that we have excluded for brevity.  Refer to ``__init__`` for a full
    list of these calculated property attributes.

    Attributes
    ----------

    sage_output_format : {"sage_binary", "sage_hdf5"}
        The output format that ``SAGE`` wrote in.

    model_path : String
        File path to the galaxy files.

        Note: Does not include the file number.

    output_path : String
        Directory path to where the some plots will be saved.  This will only be used in
        certain circumstances. In general, plots will be saved to the ``plot_output_path``
        path defined in the ``Results`` class.

    IMF : {"Salpeter", "Chabrier"}
        Specifies which IMF to use.

    model_label : String
        Label placed on the legend.

    color, linestyle : Strings
        Line color and style to be used in the plots.

    marker : String
        The marker used for scatter plots for this model.

    first_file, last_file : Integer
        Range (inclusive) of files that are read. Only required if ``sage_output_format``
        isn't ``sage_hdf5``.

    simulation : {"Mini-Millennium", "Millennium"}
        Flags which simulation we are using. Only required if ``sage_output_format`` isn't
        ``sage_hdf5``.
    """

    def __init__(self, model_dict, plot_toggles):
        """
        Sets the galaxy path and number of files to be read for a model. Also initialises
        the plot toggles that dictates which properties will be calculated.

        Parameters 
        ----------

        model_dict : Dictionary 
            Dictionary containing the parameter values for each ``Model``
            instance. Refer to the class-level documentation for a full
            description of this dictionary or to ``model_dict`` in the ``__main__`` call
            of the ``allresults.py`` module.

        plot_toggles : Dictionary 
            Specifies which plots will be generated.

        Returns
        -------

        None.
        """

        # Set the attributes we were passed.
        for key in model_dict:
            setattr(self, key, model_dict[key])

        self.num_files = 0

        # Some error checks.
        acceptable_IMF = ["Chabrier", "Salpeter"]
        if model_dict["IMF"] not in acceptable_IMF:
            print("Invalid IMF entered.  Only {0} are allowed.".format(acceptable_IMF)) 

        acceptable_sage_output_formats = ["sage_binary", "sage_hdf5"]
        if model_dict["sage_output_format"] not in acceptable_sage_output_formats:
            print("Invalid output format entered.  Only {0} are "
                   "allowed.".format(acceptable_sage_output_format))

        # Set the initial plot toggles.
        allowed_plots = ["SMF", "BMF", "GMF", "BTF", "sSFR", "gas_frac", "metallicity",
                         "bh_bulge", "quiescent", "bulge_fraction", "baryon_fraction",
                         "reservoirs", "spatial", "SFRD", "SMD"]
        for plot in allowed_plots:
            toggle_name = "{0}_toggle".format(plot)
            setattr(self, toggle_name, 0)


        # Then update based on what we were passed.
        for plot in plot_toggles.keys():
            toggle_name = "{0}_toggle".format(plot)
            setattr(self, toggle_name, 1)

        # If we're plotting temporal values (i.e., running with ``history.py``) then we
        # were passed a scale factor file.
        try:
            alist_fname = self.alist_file
        except AttributeError:
            pass
        else:
            alist = np.loadtxt(alist_fname)
            self.redshifts = 1.0/alist - 1.0

        # Then set default values.
        self.sample_size = 10000  # How many values should we plot on scatter plots?
        self.sSFRcut = -11.0  # The specific star formation rate above which a galaxy is
                              # 'star forming'.  Units are log10.

        # How should we bin Stellar mass.
        self.stellar_bin_low      = 8.0 
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

        # Densities are the sum across the entire redshift.
        self.SFRD = 0.0
        self.SMD = 0.0


    def calc_properties_all_files(self, use_pbar=True, debug=False):
        """
        Calculates galaxy properties for all files of a single Model.

        Parameters 
        ----------

        use_pbar : Boolean, default True
            If set, uses the ``tqdm`` package to create a progress bar.

        debug : Boolean, default False
            If set, prints out extra useful debug information.

        Returns
        -------

        None.
        """

        print("Processing galaxies and calculating properties for Model {0}".format(self.model_label))
        start_time = time.time()

        # First determine how many galaxies are in all files.
        self.determine_num_gals()
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

            # This is subclass specific. Refer to the relevant module for implementation.
            gals = self.read_gals(file_num, pbar=pbar, debug=debug)

            # We may have skipped a file.
            if gals is None:
                continue

            self.calc_properties(gals)

        # Some data formats (e.g., HDF5) have a single file we read from.
        # For other formats, this method doesn't exist.
        try:
            self.close_file()
        except AttributeError:
            pass

        end_time = time.time()
        duration = end_time - start_time
        print("Took {0:.2f} seconds ({1:.2f} minutes)".format(duration, duration/60.0))
        print("")


    def calc_properties(self, gals):
        """
        Calculates galaxy properties for a single file of galaxies.

        Parameters 
        ----------

        gals : ``numpy`` structured array with format given by ``Model.get_galaxy_struct()``
            The galaxies for this file.

        Returns
        -------

        None.
        """

        # When we create some plots, we do a scatter plot. For these, we only plot a
        # subset of galaxies. We need to ensure we get a representative sample from each file.
        file_sample_size = int(len(gals["StellarMass"][:]) / self.num_gals * self.sample_size) 

        if self.SMF_toggle or self.sSFR_toggle: 

            non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]
            non_zero_SFR = np.where(gals["SfrDisk"][:] + gals["SfrBulge"][:] > 0.0)[0]

            # `stellar_mass` and `sSFR` aren't strictly the same length. However the
            # difference is neglible and will not impact the results.
            stellar_mass = np.log10(gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / self.hubble_h)
            sSFR = (gals["SfrDisk"][:][non_zero_SFR] + gals["SfrBulge"][:][non_zero_SFR]) / \
                   (gals["StellarMass"][:][non_zero_SFR] * 1.0e10 / self.hubble_h)

            if self.SMF_toggle:
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

            if self.sSFR_toggle:

                # `stellar_mass` and `sSFR` have length < length(gals).
                # Hence when we take a random sample, we use length of those arrays. 
                if len(non_zero_stellar) > file_sample_size:
                    random_inds = np.random.choice(np.arange(len(non_zero_SFR)), size=file_sample_size)
                else:
                    random_inds = np.arange(len(non_zero_SFR))

                self.sSFR_mass.extend(list(stellar_mass[random_inds]))
                self.sSFR_sSFR.extend(list(np.log10(sSFR[random_inds])))

        if self.BMF_toggle:

            non_zero_baryon = np.where(gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0)[0]
            baryon_mass = np.log10((gals["StellarMass"][:][non_zero_baryon] + gals["ColdGas"][:][non_zero_baryon]) * 1.0e10 \
                                   / self.hubble_h)

            (counts, binedges) = np.histogram(baryon_mass, bins=self.stellar_mass_bins)
            self.BMF += counts

        if self.GMF_toggle:

            non_zero_cold = np.where(gals["ColdGas"][:] > 0.0)[0]
            cold_mass = np.log10(gals["ColdGas"][:][non_zero_cold] * 1.0e10 / self.hubble_h)

            (counts, binedges) = np.histogram(cold_mass, bins=self.stellar_mass_bins)
            self.GMF += counts

        if self.BTF_toggle:

            # Make sure we're getting spiral galaxies. That is, don't include galaxies
            # that are too bulgy.
            spirals = np.where((gals["Type"][:] == 0) & (gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0) & \
                               (gals["StellarMass"][:] > 0.0) & (gals["ColdGas"][:] > 0.0) & \
                               (gals["BulgeMass"][:] / gals["StellarMass"][:] > 0.1) & \
                               (gals["BulgeMass"][:] / gals["StellarMass"][:] < 0.5))[0]

            if len(spirals) > file_sample_size:
                spirals = np.random.choice(spirals, size=file_sample_size)

            baryon_mass = np.log10((gals["StellarMass"][:][spirals] + gals["ColdGas"][:][spirals]) * 1.0e10 / self.hubble_h)
            velocity = np.log10(gals["Vmax"][:][spirals])

            self.BTF_mass.extend(list(baryon_mass))
            self.BTF_vel.extend(list(velocity))

        if self.gas_frac_toggle:

            # Make sure we're getting spiral galaxies. That is, don't include galaxies
            # that are too bulgy.
            spirals = np.where((gals["Type"][:] == 0) & (gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0) &
                               (gals["BulgeMass"][:] / gals["StellarMass"][:] > 0.1) & \
                               (gals["BulgeMass"][:] / gals["StellarMass"][:] < 0.5))[0]

            if len(spirals) > file_sample_size:
                spirals = np.random.choice(spirals, size=file_sample_size)
        
            stellar_mass = np.log10(gals["StellarMass"][:][spirals] * 1.0e10 / self.hubble_h)
            gas_fraction = gals["ColdGas"][:][spirals] / (gals["StellarMass"][:][spirals] + gals["ColdGas"][:][spirals])

            self.gas_frac_mass.extend(list(stellar_mass))
            self.gas_frac.extend(list(gas_fraction))

        if self.metallicity_toggle:

            # Only care about central galaxies (Type 0) that have appreciable mass.
            centrals = np.where((gals["Type"][:] == 0) & \
                                (gals["ColdGas"][:] / (gals["StellarMass"][:] + gals["ColdGas"][:]) > 0.1) & \
                                (gals["StellarMass"][:] > 0.01))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)

            stellar_mass = np.log10(gals["StellarMass"][:][centrals] * 1.0e10 / self.hubble_h)
            Z = np.log10((gals["MetalsColdGas"][:][centrals] / gals["ColdGas"][:][centrals]) / 0.02) + 9.0

            self.metallicity_mass.extend(list(stellar_mass))
            self.metallicity.extend(list(Z))

        if self.bh_bulge_toggle:

            # Only care about galaxies that have appreciable masses.
            my_gals = np.where((gals["BulgeMass"][:] > 0.01) & (gals["BlackHoleMass"][:] > 0.00001))[0]

            if len(my_gals) > file_sample_size:
                my_gals = np.random.choice(my_gals, size=file_sample_size)
            
            bh = np.log10(gals["BlackHoleMass"][:][my_gals] * 1.0e10 / self.hubble_h)
            bulge = np.log10(gals["BulgeMass"][:][my_gals] * 1.0e10 / self.hubble_h)
                    
            self.bh_mass.extend(list(bh))
            self.bulge_mass.extend(list(bulge))

        if self.quiescent_toggle:

            non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]

            mass = np.log10(gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / self.hubble_h)
            gal_type = gals["Type"][:][non_zero_stellar]

            # For the sSFR, we will create a mask that is True for quiescent galaxies and
            # False for star-forming galaxies.
            sSFR = (gals["SfrDisk"][:][non_zero_stellar] + gals["SfrBulge"][:][non_zero_stellar]) / \
                   (gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / self.hubble_h)
            quiescent = sSFR < 10.0 ** self.sSFRcut

            if self.SMF_toggle:
                pass
            else:
                gals_per_bin, _ = np.histogram(stellar_mass, bins=self.stellar_mass_bins)
                self.SMF += gals_per_bin 

            # Mass function for number of centrals/satellites.
            centrals_counts, _ = np.histogram(mass[gal_type == 0], bins=self.stellar_mass_bins)
            self.centrals_MF += centrals_counts

            satellites_counts, _ = np.histogram(mass[gal_type == 1], bins=self.stellar_mass_bins)
            self.satellites_MF += satellites_counts

            # Then bin those galaxies/centrals/satellites that are quiescent.
            quiescent_counts, _ = np.histogram(mass[quiescent], bins=self.stellar_mass_bins)
            self.quiescent_galaxy_counts += quiescent_counts

            quiescent_centrals_counts, _ = np.histogram(mass[(gal_type == 0) & (quiescent == True)],
                                                        bins=self.stellar_mass_bins)
            self.quiescent_centrals_counts += quiescent_centrals_counts

            quiescent_satellites_counts, _ = np.histogram(mass[(gal_type == 1) & (quiescent == True)],
                                                          bins=self.stellar_mass_bins)

            self.quiescent_satellites_counts += quiescent_satellites_counts 

        if self.bulge_fraction_toggle:

            non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]

            stellar_mass = np.log10(gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / self.hubble_h)
            fraction_bulge = gals["BulgeMass"][:][non_zero_stellar] / gals["StellarMass"][:][non_zero_stellar]
            fraction_disk = 1.0 - (gals["BulgeMass"][:][non_zero_stellar] / gals["StellarMass"][:][non_zero_stellar])

            # We want the mean bulge/disk fraction as a function of stellar mass. To allow
            # us to sum across each file, we will record the sum in each bin and then average later.
            if self.SMF_toggle:
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
            self.fraction_bulge_var += fraction_bulge_var / self.num_files

            fraction_disk_var, _, _ = stats.binned_statistic(stellar_mass, fraction_disk,
                                                             statistic=np.var, bins=self.stellar_mass_bins)
            self.fraction_disk_var += fraction_disk_var / self.num_files

        if self.baryon_fraction_toggle:

            # Careful here, our "Halo Mass Function" is only counting the *BACKGROUND FOF HALOS*.
            centrals = np.where((gals["Type"][:] == 0) & (gals["Mvir"][:] > 0.0))[0]
            centrals_fof_mass = np.log10(gals["Mvir"][:][centrals] * 1.0e10 / self.hubble_h)
            halos_binned, _ = np.histogram(centrals_fof_mass, bins=self.halo_mass_bins)
            self.fof_HMF += halos_binned

            non_zero_mvir = np.where((gals["CentralMvir"][:] > 0.0))[0]  # Will only be dividing by this value.

            # These are the *BACKGROUND FOF HALO* for each galaxy.
            fof_halo_mass = gals["CentralMvir"][:][non_zero_mvir]
            fof_halo_mass_log = np.log10(gals["CentralMvir"][:][non_zero_mvir] * 1.0e10 / self.hubble_h)

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
                                                            statistic=np.sum, bins=self.halo_mass_bins)

                attribute_name = "halo_{0}_fraction_sum".format(attr_name)
                new_attribute_value = fraction_sum + getattr(self, attribute_name)
                setattr(self, attribute_name, new_attribute_value)

            # Finally want the sum across all components.
            baryons = np.sum(gals[component_key][:][non_zero_mvir] for component_key in components)
            baryon_fraction_sum, _, _ = stats.binned_statistic(fof_halo_mass_log, baryons / fof_halo_mass,
                                                                statistic=np.sum, bins=self.halo_mass_bins)
            self.halo_baryon_fraction_sum += baryon_fraction_sum

        if self.reservoirs_toggle:

            # To reduce scatter, only use galaxies in halos with mass > 1.0e10 Msun/h.
            centrals = np.where((gals["Type"][:] == 0) & (gals["Mvir"][:] > 1.0) & \
                                (gals["StellarMass"][:] > 0.0))[0]

            if len(centrals) > file_sample_size:
                centrals = np.random.choice(centrals, size=file_sample_size)

            reservoirs = ["Mvir", "StellarMass", "ColdGas", "HotGas",
                          "EjectedMass", "IntraClusterStars"]
            attribute_names = ["mvir", "stars", "cold", "hot", "ejected", "ICS"]

            for (reservoir, attribute_name) in zip(reservoirs, attribute_names):

                mass = np.log10(gals[reservoir][:][centrals] * 1.0e10 / self.hubble_h)

                # Extend the previous list of masses with these new values. 
                attr_name = "reservoir_{0}".format(attribute_name)
                attribute_value = getattr(self, attr_name) 
                attribute_value.extend(list(mass))
                setattr(self, attribute_name, attribute_value)

        if self.spatial_toggle:

            non_zero = np.where((gals["Mvir"][:] > 0.0) & (gals["StellarMass"][:] > 0.1))[0] 

            if len(non_zero) > file_sample_size:
                non_zero = np.random.choice(non_zero, size=file_sample_size)

            attribute_names = ["x_pos", "y_pos", "z_pos"]
            data_names = ["Posx", "Posy", "Posz"]

            for (attribute_name, data_name) in zip(attribute_names, data_names):

                # Units are Mpc/h.
                pos = gals[data_name][:][non_zero]

                attribute_value = getattr(self, attribute_name)
                attribute_value.extend(list(pos))
                setattr(self, attribute_name, attribute_value)

        # Check if we're plotting the SFRD and the snapshot is requested.
        if self.SFRD_toggle and gals["SnapNum"][0] in self.density_snaps:

            SFR = gals["SfrDisk"][:] + gals["SfrBulge"][:]
            self.SFRD += np.sum(SFR)

        if self.SMD_toggle and gals["SnapNum"][0] in self.density_snaps:

            non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]
            stellar_mass = gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / self.hubble_h

            self.SMD += np.sum(stellar_mass)
