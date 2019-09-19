#!/usr/bin/env python
"""
Here we show a myriad of functions that can be used to plot properties calculated from the
**SAGE** output.

We refer to :doc:`../user/plot` for more information on how plotting is handled.

Author: Jacob Seiler
"""

import matplotlib
from matplotlib import pyplot as plt
import numpy as np

import sage_analysis.observations as obs

colors = ["r", "b", "g", "m", "c"]
linestyles = ["-", ":", "--", "-.", "-:"]
markers = ["x", "o", "v", "*", "D"]


def setup_matplotlib_options():
    """
    Set the default plotting parameters.
    """

    matplotlib.rcdefaults()
    plt.rc('xtick', labelsize='x-large')
    plt.rc('ytick', labelsize='x-large')
    plt.rc('lines', linewidth='2.0')
    plt.rc('legend', numpoints=1, fontsize='x-large')


def adjust_legend(ax, location="upper right", scatter_plot=0):
    """
    Adjusts the legend of a specified axis.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        The axis whose legend we're adjusting

    location : String, default "upper right". See ``matplotlib`` docs for full options
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
        t.set_fontsize("medium")

    # For scatter plots, we want to increase the marker size.
    if scatter_plot:
        for handle in handles:
            # We may have lines in the legend which we don't want to touch here.
            if isinstance(handle, matplotlib.collections.PathCollection):
                handle.set_alpha(1.0)
                handle.set_sizes([10.0])


def plot_SMF(models, plot_output_path, plot_output_format=".png", plot_sub_populations=False):
    """
    Plots the stellar mass function for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    plot_sub_populations : Boolean, default False
        If ``True``, plots the stellar mass function for red and blue sub-populations.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/1.StellarMassFunction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    # Go through each of the models and plot.
    for (model_num, model) in enumerate(models):

        label = model.label

        # If we only have one model, we will split it into red and blue
        # sub-populations.
        if len(models) > 1:
            color = colors[model_num]
            ls = linestyles[model_num]
        else:
            color = "k"
            ls = "-"

        # Set the x-axis values to be the centre of the bins.
        bin_width = model.bins["stellar_mass_bins"][1] - model.bins["stellar_mass_bins"][0]
        bin_middles = model.bins["stellar_mass_bins"] + bin_width

        # The SMF is normalized by the simulation volume which is in Mpc/h.
        norm_SMF = model.properties["SMF"]/model.volume*pow(model.hubble_h, 3)/bin_width
        ax.plot(bin_middles[:-1], norm_SMF, color=color, ls=ls, label=label + " - All")

        # Be careful to not overcrowd the plot.
        if len(models) == 1 or plot_sub_populations:
            norm_red = model.properties["red_SMF"]/model.volume*pow(model.hubble_h, 3)/bin_width
            norm_blue = model.properties["blue_SMF"]/model.volume*pow(model.hubble_h, 3)/bin_width

            ax.plot(bin_middles[:-1], norm_red, "r:", lw=2, label=label + " - Red")
            ax.plot(bin_middles[:-1], norm_blue, "b:", lw=2, label=label + " - Blue")

    # For scaling the observational data, we use the values of the zeroth
    # model.
    zeroth_hubble_h = models[0].hubble_h
    zeroth_IMF = models[0].IMF
    ax = obs.plot_smf_data(ax, zeroth_hubble_h, zeroth_IMF)

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$")

    ax.set_yscale("log", nonposy="clip")

    ax.set_xlim([8.0, 12.0])
    ax.set_ylim([1.0e-6, 1.0e-1])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    adjust_legend(ax, location="lower left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/1.StellarMassFunction.{1}".format(plot_output_path,
                                                         plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_temporal_SMF(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the evolution of the stellar mass function for the specified models. Unlike
    ``plot_SMF()`` which only plots a SMF at a single snapshot, this function loops over
    the value of ``model.SMF_snaps`` and plots and the SMFs at each snapshots.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``. In
        particular, we acces the ``Model.properties["SMF_dict"][<snap>]`` values.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/A.StellarMassFunction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    # Go through each of the models and plot.
    for (model_num, model) in enumerate(models):

        ls = linestyles[model_num]

        # Set the x-axis values to be the centre of the bins.
        bin_width = model.bins["stellar_mass_bins"][1] - model.bins["stellar_mass_bins"][0]
        bin_middles = model.bins["stellar_mass_bins"] + bin_width

        # Iterate over the snapshots.
        for snap in model.SMF_snaps:
            label = "{0} z = {1:.3f}".format(model.label, model.redshifts[snap])

            # The SMF is normalized by the simulation volume which is in Mpc/h.
            ax.plot(bin_middles[:-1], model.properties["SMF_dict"][snap] / model.volume*pow(model.hubble_h, 3)/bin_width,
                    ls=ls, label=label)

    # For scaling the observational data, we use the values of the zeroth
    # model.
    zeroth_IMF = models[0].IMF
    ax = obs.plot_temporal_smf_data(ax, zeroth_IMF)

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$")

    ax.set_yscale("log", nonposy="clip")

    ax.set_xlim([8.0, 12.0])
    ax.set_ylim([1.0e-6, 1.0e-1])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    adjust_legend(ax, location="lower left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/A.StellarMassFunction.{1}".format(plot_output_path,
                                                         plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_BMF(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the baryonic mass function for the specified models. This is the mass
    function for the stellar mass + cold gas.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/2.BaryonicMassFunction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        ls = linestyles[model_num]

        # Set the x-axis values to be the centre of the bins.
        bin_width = model.bins["stellar_mass_bins"][1] - model.bins["stellar_mass_bins"][0]
        bin_middles = model.bins["stellar_mass_bins"] + bin_width

        # The MF is normalized by the simulation volume which is in Mpc/h.
        ax.plot(bin_middles[:-1], model.properties["BMF"]/model.volume*pow(model.hubble_h, 3)/bin_width,
                color=color, ls=ls, label=label + " - All")

    # For scaling the observational data, we use the values of the zeroth
    # model.
    zeroth_hubble_h = models[0].hubble_h
    zeroth_IMF = models[0].IMF
    ax = obs.plot_bmf_data(ax, zeroth_hubble_h, zeroth_IMF)

    ax.set_xlabel(r"$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$")

    ax.set_yscale("log", nonposy="clip")

    ax.set_xlim([7.8, 12.2])
    ax.set_ylim([1.0e-6, 1.0e-1])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    adjust_legend(ax, location="lower left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/2.BaryonicMassFunction.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_GMF(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the gas mass function for the specified models. This is the mass function for the cold gas.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/3.GasMassFunction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        ls = linestyles[model_num]

        # Set the x-axis values to be the centre of the bins.
        bin_width = model.bins["stellar_mass_bins"][1] - model.bins["stellar_mass_bins"][0]
        bin_middles = model.bins["stellar_mass_bins"] + bin_width

        # The MMF is normalized by the simulation volume which is in Mpc/h.
        ax.plot(bin_middles[:-1], model.properties["GMF"]/model.volume*pow(model.hubble_h, 3)/bin_width,
                color=color, ls=ls, label=label + " - Cold Gas")

    # For scaling the observational data, we use the values of the zeroth
    # model.
    zeroth_hubble_h = models[0].hubble_h
    obs.plot_gmf_data(ax, zeroth_hubble_h)

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{X}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$")

    ax.set_yscale("log", nonposy="clip")

    # Find the models that have the smallest/largest stellar mass bin.
    ax.set_xlim([7.8, 12.2])
    ax.set_ylim([1.0e-6, 1.0e-1])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    adjust_legend(ax, location="lower left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/3.GasMassFunction.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)  # Save the figure
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_BTF(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the baryonic Tully-Fisher relationship for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/4.BaryonicTullyFisher<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        marker = markers[model_num]

        ax.scatter(model.properties["BTF_vel"], model.properties["BTF_mass"], marker=marker, s=1,
                   color=color, alpha=0.5, label=label + " Sb/c galaxies")

    ax.set_xlim([1.4, 2.6])
    ax.set_ylim([8.0, 12.0])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    ax.set_xlabel(r"$\log_{10}V_{max}\ (km/s)$")
    ax.set_ylabel(r"$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$")

    ax = obs.plot_btf_data(ax)

    adjust_legend(ax, location="upper left", scatter_plot=1)

    fig.tight_layout()

    output_file = "{0}/4.BaryonicTullyFisher.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_sSFR(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the specific star formation rate as a function of stellar mass for the specified
    models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/5.SpecificStarFormationRate<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        marker = markers[model_num]

        ax.scatter(model.properties["sSFR_mass"], model.properties["sSFR_sSFR"], marker=marker, s=1, color=color,
                   alpha=0.5, label=label)

    # Overplot a dividing line between passive and SF galaxies.
    w = np.arange(7.0, 13.0, 1.0)
    min_sSFRcut = np.min([model.sSFRcut for model in models])
    ax.plot(w, w/w*min_sSFRcut, "b:", lw=2.0)

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\log_{10}\ s\mathrm{SFR}\ (\mathrm{yr^{-1}})$")

    ax.set_xlim([8.0, 12.0])
    ax.set_ylim([-16.0, -8.0])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    adjust_legend(ax, scatter_plot=1)

    fig.tight_layout()

    output_file = "{0}/5.SpecificStarFormationRate.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_gas_frac(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the fraction of baryons that are in the cold gas reservoir as a function of
    stellar mass for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/6.GasFraction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        marker = markers[model_num]

        ax.scatter(model.properties["gas_frac_mass"], model.properties["gas_frac"], marker=marker, s=1,
                   color=color, alpha=0.5, label=label + " Sb/c galaxies")

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\mathrm{Cold\ Mass\ /\ (Cold+Stellar\ Mass)}$")

    ax.set_xlim([7.8, 12.2])
    ax.set_ylim([0.0, 1.0])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    adjust_legend(ax, scatter_plot=1)

    fig.tight_layout()

    output_file = "{0}/6.GasFraction.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_metallicity(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the metallicity as a function of stellar mass for the speicifed models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/7.Metallicity<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        marker = markers[model_num]

        ax.scatter(model.properties["metallicity_mass"], model.properties["metallicity"],
                   marker=marker, s=1, color=color, alpha=0.5, label=label + " galaxies")

    # Use the IMF of the zeroth model to scale the observational results.
    zeroth_IMF = models[0].IMF
    ax = obs.plot_metallicity_data(ax, zeroth_IMF)

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$")
    ax.set_ylabel(r"$12\ +\ \log_{10}[\mathrm{O/H}]$")

    ax.set_xlim([7.8, 12.2])
    ax.set_ylim([8.0, 9.5])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    # Since we're doing a scatter plot, we need to resize the legend points.
    adjust_legend(ax, location="upper right", scatter_plot=1)

    fig.tight_layout()

    output_file = "{0}/7.Metallicity.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_bh_bulge(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the black-hole bulge relationship for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/8.BlackHoleBulgeRelationship<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        marker = markers[model_num]

        ax.scatter(model.properties["bulge_mass"], model.properties["bh_mass"], marker=marker, s=1, color=color,
                   alpha=0.5, label=label + " galaxies")

    ax = obs.plot_bh_bulge_data(ax)

    ax.set_xlabel(r"$\log\ M_{\mathrm{bulge}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\log\ M_{\mathrm{BH}}\ (M_{\odot})$")

    ax.set_xlim([7.8, 12.2])
    ax.set_ylim([6.0, 10.0])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    adjust_legend(ax, location="upper right", scatter_plot=1)

    fig.tight_layout()

    output_file = "{0}/8.BlackHoleBulgeRelationship.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_quiescent(models, plot_output_path, plot_output_format=".png",
                   plot_sub_populations=False):
    """
    Plots the fraction of galaxies that are quiescent as a function of stellar mass for the
    specified models.  The quiescent cut is defined by ``Model.sSFRcut``.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    plot_sub_populations : Boolean, default False
        If ``True``, plots the centrals and satellite sub-populations.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/9.QuiescentFraction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        linestyle = linestyles[model_num]

        # Set the x-axis values to be the centre of the bins.
        bin_width = model.bins["stellar_mass_bins"][1] - model.bins["stellar_mass_bins"][0]
        bin_middles = model.bins["stellar_mass_bins"] + bin_width

        # We will keep the colour scheme consistent, but change the line styles.
        ax.plot(bin_middles[:-1], model.properties["quiescent_galaxy_counts"] / model.properties["SMF"],
                label=label + " All", color=color, linestyle="-")

        if len(models) == 1 or plot_sub_populations:
            ax.plot(bin_middles[:-1], model.properties["quiescent_centrals_counts"] / model.properties["centrals_MF"],
                    label=label + " Centrals", color=color, linestyle="--")

            ax.plot(bin_middles[:-1], model.properties["quiescent_satellites_counts"] / model.properties["satellites_MF"],
                    label=label + " Satellites", color=color, linestyle="-.")

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\mathrm{Quescient\ Fraction}$")

    ax.set_xlim([7.8, 12.2])
    ax.set_ylim([0.0, 1.05])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.25))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.10))

    adjust_legend(ax, location="upper left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/9.QuiescentFraction.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_bulge_fraction(models, plot_output_path, plot_output_format=".png",
                        plot_var=False):
    """
    Plots the fraction of the stellar mass that is located in the bulge/disk as a function
    of stellar mass for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    plot_var : Boolean, default False
        If ``True``, plots the variance as shaded regions.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/10.BulgeMassFraction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        linestyle = linestyles[model_num]

        # Set the x-axis values to be the centre of the bins.
        bin_width = model.bins["stellar_mass_bins"][1] - model.bins["stellar_mass_bins"][0]
        bin_middles = model.bins["stellar_mass_bins"] + bin_width

        # Remember we need to average the properties in each bin.
        bulge_mean = model.properties["fraction_bulge_sum"] / model.properties["SMF"]
        disk_mean = model.properties["fraction_disk_sum"] / model.properties["SMF"]

        # The variance has already been weighted when we calculated it.
        bulge_var = model.properties["fraction_bulge_var"]
        disk_var = model.properties["fraction_disk_var"]

        # We will keep the colour scheme consistent, but change the line styles.
        ax.plot(bin_middles[:-1], bulge_mean, label=label + " bulge",
                color=color, linestyle="-")
        ax.plot(bin_middles[:-1], disk_mean, label=label + " disk",
                color=color, linestyle="--")

        if plot_var:
            ax.fill_between(bin_middles[:-1], bulge_mean+bulge_var, bulge_mean-bulge_var,
                            facecolor=color, alpha=0.25)
            ax.fill_between(bin_middles[:-1], disk_mean+disk_var, disk_mean-disk_var,
                            facecolor=color, alpha=0.25)

    ax.set_xlabel(r"$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\mathrm{Stellar\ Mass\ Fraction}$")

    ax.set_xlim([7.8, 12.2])
    ax.set_ylim([0.0, 1.05])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.25))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.10))

    adjust_legend(ax, location="upper left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/10.BulgeMassFraction.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_baryon_fraction(models, plot_output_path, plot_output_format=".png",
                         plot_sub_populations=False):
    """
    Plots the total baryon fraction as afunction of halo mass for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    plot_sub_populations : Boolean, default False
        If ``True``, plots the baryon fraction for each reservoir. Otherwise, only plots
        the total baryon fraction.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/11.BaryonFraction<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        linestyle = linestyles[model_num]

        # Set the x-axis values to be the centre of the bins.
        bin_width = model.bins["halo_mass_bins"][1] - model.bins["halo_mass_bins"][0]
        bin_middles = model.bins["halo_mass_bins"] + bin_width

        # Remember we need to average the properties in each bin.
        baryon_mean = model.properties["halo_baryon_fraction_sum"] / model.properties["fof_HMF"]

        # We will keep the linestyle constant but change the color.
        ax.plot(bin_middles[:-1], baryon_mean, label=label + " Total",
                color=color, linestyle=linestyle)

        # If we have multiple models, we want to be careful of overcrowding the plot.
        if len(models) == 1 or plot_sub_populations:
            attrs = ["stars", "cold", "hot", "ejected", "ICS"]
            labels = ["Stars", "Cold", "Hot", "Ejected", "ICS"]
            res_colors = ["k", "b", "r", "g", "y"]

            for (attr, label, color) in zip(attrs, labels, res_colors):
                dict_key = "halo_{0}_fraction_sum".format(attr)
                mean = model.properties[dict_key] / model.properties["fof_HMF"]

                ax.plot(bin_middles[:-1], mean, label=label + " " + label,
                        color=color, linestyle=linestyle)

    ax.set_xlabel(r"$\mathrm{Central}\ \log_{10} M_{\mathrm{vir}}\ (M_{\odot})$")
    ax.set_ylabel(r"$\mathrm{Baryon\ Fraction}$")

    ax.set_xlim([9.8, 14.2])
    ax.set_ylim([0.0, 0.23])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.25))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.05))

    adjust_legend(ax, location="upper left", scatter_plot=0)

    output_file = "{0}/11.BaryonFraction.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_reservoirs(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the mass in each reservoir as a function of halo mass for the
    specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    A plot will be saved as
    "<plot_output_path>/12.MassReservoirs<model.label><plot_output_path>"
    for each mode.
    """

    # This scatter plot will be messy so we're going to make one for each model.
    for (model_num, model) in enumerate(models):

        fig = plt.figure()
        ax = fig.add_subplot(111)

        label = model.label
        marker = markers[model_num]

        attribute_names = ["stars", "cold", "hot", "ejected", "ICS"]
        labels = ["Stars", "Cold Gas", "Hot Gas", "Ejected Gas", "Intracluster Stars"]

        for (attribute_name, label) in zip(attribute_names, labels):

            dict_key = "reservoir_{0}".format(attribute_name)
            ax.scatter(model.properties["reservoir_mvir"], model.properties[dict_key], marker=marker,
                       s=0.3, label=label)

        ax.set_xlabel(r"$\log\ M_{\mathrm{vir}}\ (M_{\odot})$")
        ax.set_ylabel(r"$\mathrm{Reservoir\ Mass\ (M_{\odot})}$")

        ax.set_xlim([9.8, 14.2])
        ax.set_ylim([7.5, 12.5])

        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.25))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

        adjust_legend(ax, location="upper left", scatter_plot=1)

        fig.tight_layout()

        output_file = "{0}/12.MassReservoirs_{1}.{2}".format(plot_output_path, label,
                                                             plot_output_format)
        fig.savefig(output_file)
        print("Saved file to {0}".format(output_file))
        plt.close()


def plot_spatial(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the spatial distribution of the galaxies for specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    A plot will be saved as
    "<plot_output_path>/13.SpatialDistribution<model.label><plot_output_path>"
    for each model.
    """

    fig = plt.figure()

    # 4-panel plot.
    ax1 = fig.add_subplot(221)
    ax2 = fig.add_subplot(222)
    ax3 = fig.add_subplot(223)
    ax4 = fig.add_subplot(224)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        marker = markers[model_num]

        ax1.scatter(model.properties["x_pos"], model.properties["y_pos"],
                    marker=marker, s=0.3, color=color, alpha=0.5)
        ax2.scatter(model.properties["x_pos"], model.properties["z_pos"],
                    marker=marker, s=0.3, color=color, alpha=0.5)
        ax3.scatter(model.properties["y_pos"], model.properties["z_pos"], marker=marker, s=0.3, color=color,
                    alpha=0.5)

        # The bottom right panel will only contain the legend.
        # For some odd reason, plotting `np.nan` causes some legend entries to not
        # appear. Plot junk and we'll adjust the axis to not show it.
        ax4.scatter(-999, -999, marker=marker, color=color, label=label)
        ax4.axis("off")

    ax1.set_xlabel(r"$\mathrm{x}\ [\mathrm{Mpc}/h]$")
    ax1.set_ylabel(r"$\mathrm{y}\ [\mathrm{Mpc}/h]$")

    ax2.set_xlabel(r"$\mathrm{x}\ [\mathrm{Mpc}/h]$")
    ax2.set_ylabel(r"$\mathrm{z}\ [\mathrm{Mpc}/h]$")

    ax3.set_xlabel(r"$\mathrm{y}\ [\mathrm{Mpc}/h]$")
    ax3.set_ylabel(r"$\mathrm{z}\ [\mathrm{Mpc}/h]$")

    # Find the model with the largest box.
    max_box = np.min([model.box_size for model in models]) - 0.5
    buffer = max_box*0.05
    for ax in [ax1, ax2, ax3, ax4]:
        ax.set_xlim([0.0-buffer, max_box+buffer])
        ax.set_ylim([0.0-buffer, max_box+buffer])

        ax.xaxis.set_minor_locator(plt.MultipleLocator(5))
        ax.yaxis.set_minor_locator(plt.MultipleLocator(5))

    adjust_legend(ax4, location="upper left", scatter_plot=1)

    # Make sure everything remains nicely layed out.
    fig.tight_layout()

    output_file = "{0}/13.SpatialDistribution.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_spatial_3d(pos, output_file, box_size):
    """
    Plots the 3D spatial distribution of galaxies.

    Parameters
    ==========

    pos : ``numpy`` 3D array with length equal to the number of galaxies
        The position (in Mpc/h) of the galaxies.

    output_file : String
        Name of the file the plot will be saved as.

    Returns
    =======

    None.  A plot will be saved as ``output_file``.
    """

    from mpl_toolkits.mplot3d import Axes3D
    from random import sample

    fig = plt.figure()
    ax = fig.add_subplot(111, projection="3d")

    # Generate a subsample if necessary.
    num_gals = len(pos)
    sample_size = 10000
    if num_gals > sample_size:
        w = sample(list(np.arange(num_gals)), sample_size)
    else:
        w = np.arange(num_gals)

    ax.scatter(pos[w,0], pos[w,1], pos[w,2], alpha=0.5)

    ax.set_xlim([0.0, box_size])
    ax.set_ylim([0.0, box_size])
    ax.set_zlim([0.0, box_size])

    ax.set_xlabel(r"$\mathbf{x \: [h^{-1}Mpc]}$")
    ax.set_ylabel(r"$\mathbf{y \: [h^{-1}Mpc]}$")
    ax.set_zlabel(r"$\mathbf{z \: [h^{-1}Mpc]}$")

    fig.tight_layout()

    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_SFRD(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the evolution of star formation rate density for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``. In
        particular, we acces the ``Model.properties["SFRD_dict"][<snap>]`` values.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/B.SFRDensity<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        linestyle = linestyles[model_num]

        # The SFRD is in a dictionary. Pull it out into a array for plotting.
        SFRD = np.array([model.properties["SFRD_dict"][snap] for snap in model.properties["SFRD_dict"].keys()])
        ax.plot(model.redshifts[model.density_snaps], np.log10(SFRD / model.volume*pow(model.hubble_h, 3)),
                label=label, color=color, ls=linestyle)

    ax = obs.plot_sfrd_data(ax)

    ax.set_xlabel(r"$\mathrm{redshift}$")
    ax.set_ylabel(r"$\log_{10} \mathrm{SFR\ density}\ (M_{\odot}\ \mathrm{yr}^{-1}\ \mathrm{Mpc}^{-3})$")

    ax.set_xlim([0.0, 8.0])
    ax.set_ylim([-3.0, -0.4])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    adjust_legend(ax, location="lower left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/B.SFRDensity.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()


def plot_SMD(models, plot_output_path, plot_output_format=".png"):
    """
    Plots the evolution of stellar mass density for the specified models.

    Parameters
    ----------

    models : List of ``Model`` class instance
        Models that will be plotted. These instances contain the properties necessary to
        create the plot, accessed via ``Model.properties["property_name"]``. In
        particular, we acces the ``Model.properties["SMD_dict"][<snap>]`` values.

    plot_output_path : string
        Path to where the plot will be saved.

    plot_output_format : string, default ".png"
        Format the plot will be saved in, includes the full stop.

    Generates
    ---------

    The plot will be saved as "<plot_output_path>/C.StellarMassDensity<plot_output_format>"
    """

    fig = plt.figure()
    ax = fig.add_subplot(111)

    for (model_num, model) in enumerate(models):

        label = model.label
        color = colors[model_num]
        linestyle = linestyles[model_num]

        # The SMD is in a dictionary. Pull it out into a array for plotting.
        SMD = np.array([model.properties["SMD_dict"][snap] for snap in model.properties["SMD_dict"].keys()])
        ax.plot(model.redshifts[model.density_snaps], np.log10(SMD / model.volume * pow(model.hubble_h, 3)),
                label=label, color=color, ls=linestyle)

    # For scaling the observational data, we use the values of the zeroth
    # model.
    zeroth_IMF = models[0].IMF
    ax = obs.plot_smd_data(ax, zeroth_IMF)

    ax.set_xlabel(r"$\mathrm{redshift}$")
    ax.set_ylabel(r'$\log_{10}\ \phi\ (M_{\odot}\ \mathrm{Mpc}^{-3})$')

    ax.set_xlim([0.0, 4.2])
    ax.set_ylim([6.5, 9.0])

    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    adjust_legend(ax, location="lower left", scatter_plot=0)

    fig.tight_layout()

    output_file = "{0}/C.StellarMassDensity.{1}".format(plot_output_path, plot_output_format)
    fig.savefig(output_file)
    print("Saved file to {0}".format(output_file))
    plt.close()
