"""
Here we show a myriad of functions that can be used to calculate properties from the
**SAGE** output.  By setting the correct plot toggle and calling
:py:func:`~generate_func_dict`, a dictionary containing these functions can be generated
and passed to a :py:class:`~Model` instance to calculate the property.

The properties are stored (and updated) in the :py:attr:`~Model.properties` attribute.

We refer to :doc:`../user/calc` for more information on how the calculations are handled.

Author: Jacob Seiler
"""

import numpy as np
from scipy import stats

from sage_analysis.utils import select_random_values


def calc_SMF(model, gals):
    """
    Calculates the stellar mass function of the given galaxies.  That is, the number of
    galaxies at a given stellar mass.

    The ``Model.properties["SMF"]`` array will be updated. We also split the galaxy
    population into "red" and "blue" based on the value of :py:attr:`~Model.sSFRcut` and
    update the ``Model.properties["red_SMF"]`` and ``Model.properties["blue_SMF"]``
    arrays.
    """

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
    """
    Calculates the baryon mass function of the given galaxies.  That is, the number of
    galaxies at a given baryon (stellar + cold gas) mass.

    The ``Model.properties["BMF"]`` array will be updated.
    """

    non_zero_baryon = np.where(gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0)[0]
    baryon_mass = np.log10((gals["StellarMass"][:][non_zero_baryon] + gals["ColdGas"][:][non_zero_baryon]) * 1.0e10 \
                           / model.hubble_h)

    (counts, _) = np.histogram(baryon_mass, bins=model.bins["stellar_mass_bins"])
    model.properties["BMF"] += counts


def calc_GMF(model, gals):
    """
    Calculates the gas mass function of the given galaxies.  That is, the number of
    galaxies at a given cold gas mass.

    The ``Model.properties["GMF"]`` array will be updated.
    """

    non_zero_cold = np.where(gals["ColdGas"][:] > 0.0)[0]
    cold_mass = np.log10(gals["ColdGas"][:][non_zero_cold] * 1.0e10 / model.hubble_h)

    (counts, _) = np.histogram(cold_mass, bins=model.bins["stellar_mass_bins"])
    model.properties["GMF"] += counts


def calc_BTF(model, gals):
    """
    Calculates the baryonic Tully-Fisher relation for spiral galaxies in the given set of
    galaxies.

    The number of galaxies added to ``Model.properties["BTF_mass"]`` and
    ``Model.properties["BTF_vel"]`` arrays is given by :py:attr:`~Model.sample_size`
    weighted by ``number_spirals_passed /`` :py:attr:`~Model.num_gals_all_files`. If
    this value is greater than ``number_spirals_passed``, then all spiral galaxies will
    be used.
    """

    # Make sure we're getting spiral galaxies. That is, don't include galaxies
    # that are too bulgy.
    spirals = np.where((gals["Type"][:] == 0) & (gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0) & \
                       (gals["StellarMass"][:] > 0.0) & (gals["ColdGas"][:] > 0.0) & \
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] > 0.1) & \
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] < 0.5))[0]

    # Select a random subset of galaxies (if necessary).
    spirals = select_random_values(spirals, model.num_gals_all_files, model.sample_size)

    baryon_mass = np.log10((gals["StellarMass"][:][spirals] + gals["ColdGas"][:][spirals]) * 1.0e10 / model.hubble_h)
    velocity = np.log10(gals["Vmax"][:][spirals])

    model.properties["BTF_mass"] = np.append(model.properties["BTF_mass"], baryon_mass)
    model.properties["BTF_vel"] = np.append(model.properties["BTF_vel"], velocity)


def calc_sSFR(model, gals):
    """
    Calculates the specific star formation rate (star formation divided by the stellar
    mass of the galaxy) as a function of stellar mass.

    The number of galaxies added to ``Model.properties["sSFR_mass"]`` and
    ``Model.properties["sSFR_sSFR"]`` arrays is given by :py:attr:`~Model.sample_size`
    weighted by ``number_gals_passed /`` :py:attr:`~Model.num_gals_all_files`. If
    this value is greater than ``number_gals_passed``, then all galaxies with non-zero
    stellar mass will be used.
    """

    non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]

    # Select a random subset of galaxies (if necessary).
    random_inds = select_random_values(non_zero_stellar, model.num_gals_all_files,
                                     model.sample_size)

    stellar_mass = np.log10(gals["StellarMass"][:][random_inds] * 1.0e10 / model.hubble_h)
    sSFR = (gals["SfrDisk"][:][random_inds] + gals["SfrBulge"][:][random_inds]) / \
               (gals["StellarMass"][:][random_inds] * 1.0e10 / model.hubble_h)


    model.properties["sSFR_mass"] = np.append(model.properties["sSFR_mass"], stellar_mass)
    model.properties["sSFR_sSFR"] = np.append(model.properties["sSFR_sSFR"], np.log10(sSFR))


def calc_gas_frac(model, gals):
    """
    Calculates the fraction of baryons that are in the cold gas reservoir as a function of
    stellar mass.

    The number of galaxies added to ``Model.properties["gas_frac_mass"]`` and
    ``Model.properties["gas_frac"]`` arrays is given by :py:attr:`~Model.sample_size`
    weighted by ``number_spirals_passed /`` :py:attr:`~Model.num_gals_all_files`. If
    this value is greater than ``number_spirals_passed``, then all spiral galaxies will
    be used.
    """

    # Make sure we're getting spiral galaxies. That is, don't include galaxies
    # that are too bulgy.
    spirals = np.where((gals["Type"][:] == 0) & (gals["StellarMass"][:] + gals["ColdGas"][:] > 0.0) &
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] > 0.1) & \
                       (gals["BulgeMass"][:] / gals["StellarMass"][:] < 0.5))[0]

    # Select a random subset of galaxies (if necessary).
    spirals = select_random_values(spirals, model.num_gals_all_files, model.sample_size)

    stellar_mass = np.log10(gals["StellarMass"][:][spirals] * 1.0e10 / model.hubble_h)
    gas_fraction = gals["ColdGas"][:][spirals] / (gals["StellarMass"][:][spirals] + gals["ColdGas"][:][spirals])

    model.properties["gas_frac_mass"] = np.append(model.properties["gas_frac_mass"], stellar_mass)
    model.properties["gas_frac"] = np.append(model.properties["gas_frac"], gas_fraction)


def calc_metallicity(model, gals):
    """
    Calculates the metallicity as a function of stellar mass.

    The number of galaxies added to ``Model.properties["metallicity_mass"]`` and
    ``Model.properties["metallicity"]`` arrays is given by :py:attr:`~Model.sample_size`
    weighted by ``number_centrals_passed /`` :py:attr:`~Model.num_gals_all_files`. If
    this value is greater than ``number_centrals_passed``, then all central galaxies will
    be used.
    """

    # Only care about central galaxies (Type 0) that have appreciable mass.
    centrals = np.where((gals["Type"][:] == 0) & \
                        (gals["ColdGas"][:] / (gals["StellarMass"][:] + gals["ColdGas"][:]) > 0.1) & \
                        (gals["StellarMass"][:] > 0.01))[0]

    # Select a random subset of galaxies (if necessary).
    centrals = select_random_values(centrals, model.num_gals_all_files, model.sample_size)

    stellar_mass = np.log10(gals["StellarMass"][:][centrals] * 1.0e10 / model.hubble_h)
    Z = np.log10((gals["MetalsColdGas"][:][centrals] / gals["ColdGas"][:][centrals]) / 0.02) + 9.0

    model.properties["metallicity_mass"] = np.append(model.properties["metallicity_mass"], stellar_mass)
    model.properties["metallicity"] = np.append(model.properties["metallicity"], Z)


def calc_bh_bulge(model, gals):
    """
    Calculates the black hole mass as a function of bulge mass.

    The number of galaxies added to ``Model.properties["BlackHoleMass"]`` and
    ``Model.properties["BulgeMass"]`` arrays is given by :py:attr:`~Model.sample_size`
    weighted by ``number_galaxies_passed /`` :py:attr:`~Model.num_gals_all_files`. If
    this value is greater than ``number_galaxies_passed``, then all galaxies will
    be used.

    Notes
    -----

    We only consider galaxies with bulge mass greater than 10^8 Msun/h and a black
    hole mass greater than 10^5 Msun/h.
    """

    # Only care about galaxies that have appreciable masses.
    my_gals = np.where((gals["BulgeMass"][:] > 0.01) & (gals["BlackHoleMass"][:] > 0.00001))[0]

    # Select a random subset of galaxies (if necessary).
    my_gals = select_random_values(my_gals, model.num_gals_all_files, model.sample_size)

    bh = np.log10(gals["BlackHoleMass"][:][my_gals] * 1.0e10 / model.hubble_h)
    bulge = np.log10(gals["BulgeMass"][:][my_gals] * 1.0e10 / model.hubble_h)

    model.properties["bh_mass"] = np.append(model.properties["bh_mass"], bh)
    model.properties["bulge_mass"] = np.append(model.properties["bulge_mass"], bulge)


def calc_quiescent(model, gals):
    """
    Calculates the quiescent galaxy fraction as a function of stellar mass.  The galaxy
    population is also split into central and satellites and the quiescent fraction of
    these are calculated.

    The ``Model.properties["centrals_MF"]``, ``Model.properties["satellites_MF"],
    ``Model.properties["quiescent_galaxy_counts"]``,
    ``Model.properties["quiescent_centrals_counts"]``, and
    ``Model.properties["quiescent_satellites_counts"]`` arrayss will be updated.

    Notes
    -----

    We only **count** the number of quiescent galaxies in each stellar mass bin.  When
    converting this to the quiescent fraction, one must divide by the number of galaxies
    in each stellar mass bin, the stellar mass function ``Model.properties["SMF"]``. See
    :func:`~plot_quiescent` for an example implementation.

    If the stellar mass function has not been calculated (``Model.calc_SMF`` is
    ``False``), a ``ValueError`` is thrown.  Ensure that ``SMF`` has been switched on for
     the ``plot_toggles`` variable when the ``Model`` is instantiated. Also ensure that
    the ``SMF`` binned property is initialized.
    """

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
    """ Calculates the ``bulge_mass / stellar_mass`` and ``disk_mass / stellar_mass`` ratios
    as a function of stellar mass.

    The ``Model.properties["fraction_bulge_sum"]``, ``Model.properties["fraction_disk_sum"],
    ``Model.properties["fraction_bulge_var"]``,
    ``Model.properties["fraction_disk_var"]`` arrayss will be updated.

    Notes
    -----

    We only **sum** the bulge/disk mass in each stellar mass bin.  When converting this to
    the mass fraction, one must divide by the number of galaxies in each stellar mass bin,
    the stellar mass function ``Model.properties["SMF"]``. See :func:`~plot_bulge_fraction`
    for full implementation.

    If the stellar mass function has not been calculated (``Model.calc_SMF`` is
    ``False``), a ``ValueError`` is thrown.  Ensure that ``SMF`` has been switched on for
     the ``plot_toggles`` variable when the ``Model`` is instantiated. Also ensure that
    the ``SMF`` binned property is initialized.
    """

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
    """
    Calculates the ``mass_baryons / halo_virial_mass`` as a function of halo virial mass
    for each baryon reseroivr (stellar, cold, hot, ejected, intra-cluster stars and black
    hole). Also calculates the ratio for the total baryonic mass.

    The ``Model.properties["halo_<reservoir_name>_fraction_sum"]`` arrays are updated for
    each reservoir. In addition, ``Model.properties["halo_baryon_fraction_sum"]`` is
    updated.

    Notes
    -----

    The halo virial mass we use is the **background FoF halo**, not the immediate host
    halo of each galaxy.

    We only **sum** the baryon mass in each stellar mass bin.  When converting this to
    the mass fraction, one must divide by the number of halos in each halo mass bin,
    ``Model.properties["fof_HMF"]``. See :func:`~plot_baryon_fraction`
    for full implementation.

    If the ``Model.properties["fof_HMF"]`` property, with associated bins
    ``Model.bins["halo_mass"bin"]`` have not been initialized, a ``ValueError`` is thrown.
    """

    # Careful here, our "Halo Mass Function" is only counting the *BACKGROUND FOF HALOS*.
    centrals = np.where((gals["Type"][:] == 0) & (gals["Mvir"][:] > 0.0))[0]
    centrals_fof_mass = np.log10(gals["Mvir"][:][centrals] * 1.0e10 / model.hubble_h)
    try:
        halos_binned, _ = np.histogram(centrals_fof_mass, bins=model.bins["halo_mass_bins"])
    except KeyError:
        print("The `halo_mass_bins` bin array was not initialised.")
        raise ValueError

    try:
        model.properties["fof_HMF"] += halos_binned
    except KeyError:
        print("The `fof_HMF` property was not iniitalized.")
        raise ValueError

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
        model.properties[dict_key] += fraction_sum

    # Finally want the sum across all components.
    baryons = np.sum(gals[component_key][:][non_zero_mvir] for component_key in components)
    baryon_fraction_sum, _, _ = stats.binned_statistic(fof_halo_mass_log, baryons / fof_halo_mass,
                                                        statistic=np.sum,
                                                        bins=model.bins["halo_mass_bins"])
    model.properties["halo_baryon_fraction_sum"] += baryon_fraction_sum


def calc_reservoirs(model, gals):
    """
    Calculates the mass of each reservoir as a function of halo virial mass.

    The number of galaxies added to ``Model.properties["reservoir_mvir"]`` and
    ``Model.properties["reservoir_<reservoir_name>"]`` arrays is given by
    :py:attr:`~Model.sample_size` weighted by
    ``number_centrals_passed /`` :py:attr:`~Model.num_gals_all_files`. If
    this value is greater than ``number_centrals_passed``, then all central galaxies will
    be used.
    """

    # To reduce scatter, only use galaxies in halos with mass > 1.0e10 Msun/h.
    centrals = np.where((gals["Type"][:] == 0) & (gals["Mvir"][:] > 1.0) & \
                        (gals["StellarMass"][:] > 0.0))[0]

    # Select a random subset of galaxies (if necessary).
    centrals = select_random_values(centrals, model.num_gals_all_files, model.sample_size)

    reservoirs = ["Mvir", "StellarMass", "ColdGas", "HotGas",
                  "EjectedMass", "IntraClusterStars"]
    attribute_names = ["mvir", "stars", "cold", "hot", "ejected", "ICS"]

    for (reservoir, attribute_name) in zip(reservoirs, attribute_names):

        mass = np.log10(gals[reservoir][:][centrals] * 1.0e10 / model.hubble_h)

        # Extend the previous list of masses with these new values.
        dict_key = "reservoir_{0}".format(attribute_name)
        model.properties[dict_key] = np.append(model.properties[dict_key], mass)


def calc_spatial(model, gals):
    """
    Calculates the spatial position of the galaxies.

    The number of galaxies added to ``Model.properties["<x/y/z>_pos"]`` arrays is given by
    :py:attr:`~Model.sample_size` weighted by
    ``number_galaxies_passed /`` :py:attr:`~Model.num_gals_all_files`. If
    this value is greater than ``number_galaxies_passed``, then all galaxies will be used.
    """

    non_zero = np.where((gals["Mvir"][:] > 0.0) & (gals["StellarMass"][:] > 0.1))[0]

    # Select a random subset of galaxies (if necessary).
    non_zero = select_random_values(non_zero, model.num_gals_all_files, model.sample_size)

    attribute_names = ["x_pos", "y_pos", "z_pos"]
    data_names = ["Posx", "Posy", "Posz"]

    for (attribute_name, data_name) in zip(attribute_names, data_names):

        # Units are Mpc/h.
        pos = gals[data_name][:][non_zero]

        model.properties[attribute_name] = np.append(model.properties[attribute_name], pos)


def calc_SFRD(model, gals):
    """
    Calculates the sum of the star formation across all galaxies. This will be normalized
    by the simulation volume to determine the density. See :funct:`~plot_SFRD` for full
    implementation.

    The ``Model.properties["SFRD"]`` value is updated.
    """

    # Check if the Snapshot is required.
    if gals["SnapNum"][0] in model.density_snaps:

        SFR = gals["SfrDisk"][:] + gals["SfrBulge"][:]
        model.properties["SFRD"] += np.sum(SFR)


def calc_SMD(model, gals):
    """
    Calculates the sum of the stellar mass across all galaxies. This will be normalized
    by the simulation volume to determine the density. See :funct:`~plot_SMD` for full
    implementation.

    The ``Model.properties["SMD"]`` value is updated.
    """

    # Check if the Snapshot is required.
    if gals["SnapNum"][0] in model.density_snaps:

        non_zero_stellar = np.where(gals["StellarMass"][:] > 0.0)[0]
        stellar_mass = gals["StellarMass"][:][non_zero_stellar] * 1.0e10 / model.hubble_h

        model.properties["SMD"] += np.sum(stellar_mass)
