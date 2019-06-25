import numpy as np
from scipy import stats


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
