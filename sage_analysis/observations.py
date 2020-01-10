#!/usr/bin/env python
"""
Module to handle plotting of observational data.

Author: Jacob Seiler
"""

import numpy as np


def plot_smf_data(ax, hubble_h, imf):
    """
    Plots stellar mass function observational data. Uses data from Balry et al., 2008.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    hubble_h : Float
        Little h value (between 0 and 1). Used to scale the y-values of the Baldry data
        which is irrespective of h.

    imf : {"Salpeter", "Chabrier"}
        If "Chabrier", reduces the x-values of the Baldry data by 0.26 dex.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    # Baldry+ 2008 modified data used for the MCMC fitting
    Baldry = np.array([
        [7.05, 1.3531e-01, 6.0741e-02],
        [7.15, 1.3474e-01, 6.0109e-02],
        [7.25, 2.0971e-01, 7.7965e-02],
        [7.35, 1.7161e-01, 3.1841e-02],
        [7.45, 2.1648e-01, 5.7832e-02],
        [7.55, 2.1645e-01, 3.9988e-02],
        [7.65, 2.0837e-01, 4.8713e-02],
        [7.75, 2.0402e-01, 7.0061e-02],
        [7.85, 1.5536e-01, 3.9182e-02],
        [7.95, 1.5232e-01, 2.6824e-02],
        [8.05, 1.5067e-01, 4.8824e-02],
        [8.15, 1.3032e-01, 2.1892e-02],
        [8.25, 1.2545e-01, 3.5526e-02],
        [8.35, 9.8472e-02, 2.7181e-02],
        [8.45, 8.7194e-02, 2.8345e-02],
        [8.55, 7.0758e-02, 2.0808e-02],
        [8.65, 5.8190e-02, 1.3359e-02],
        [8.75, 5.6057e-02, 1.3512e-02],
        [8.85, 5.1380e-02, 1.2815e-02],
        [8.95, 4.4206e-02, 9.6866e-03],
        [9.05, 4.1149e-02, 1.0169e-02],
        [9.15, 3.4959e-02, 6.7898e-03],
        [9.25, 3.3111e-02, 8.3704e-03],
        [9.35, 3.0138e-02, 4.7741e-03],
        [9.45, 2.6692e-02, 5.5029e-03],
        [9.55, 2.4656e-02, 4.4359e-03],
        [9.65, 2.2885e-02, 3.7915e-03],
        [9.75, 2.1849e-02, 3.9812e-03],
        [9.85, 2.0383e-02, 3.2930e-03],
        [9.95, 1.9929e-02, 2.9370e-03],
        [10.05, 1.8865e-02, 2.4624e-03],
        [10.15, 1.8136e-02, 2.5208e-03],
        [10.25, 1.7657e-02, 2.4217e-03],
        [10.35, 1.6616e-02, 2.2784e-03],
        [10.45, 1.6114e-02, 2.1783e-03],
        [10.55, 1.4366e-02, 1.8819e-03],
        [10.65, 1.2588e-02, 1.8249e-03],
        [10.75, 1.1372e-02, 1.4436e-03],
        [10.85, 9.1213e-03, 1.5816e-03],
        [10.95, 6.1125e-03, 9.6735e-04],
        [11.05, 4.3923e-03, 9.6254e-04],
        [11.15, 2.5463e-03, 5.0038e-04],
        [11.25, 1.4298e-03, 4.2816e-04],
        [11.35, 6.4867e-04, 1.6439e-04],
        [11.45, 2.8294e-04, 9.9799e-05],
        [11.55, 1.0617e-04, 4.9085e-05],
        [11.65, 3.2702e-05, 2.4546e-05],
        [11.75, 1.2571e-05, 1.2571e-05],
        [11.85, 8.4589e-06, 8.4589e-06],
        [11.95, 7.4764e-06, 7.4764e-06],
        ], dtype=np.float32)

    Baldry_xval = np.log10(10 ** Baldry[:, 0] / hubble_h / hubble_h)
    if imf == "Chabrier":
        # Convert the Baldry data to Chabrier.
        Baldry_xval = Baldry_xval - 0.26

    Baldry_yvalU = (Baldry[:, 1]+Baldry[:, 2]) * pow(hubble_h, 3)
    Baldry_yvalL = (Baldry[:, 1]-Baldry[:, 2]) * pow(hubble_h, 3)

    ax.fill_between(Baldry_xval, Baldry_yvalU, Baldry_yvalL,
                    facecolor='purple', alpha=0.25,
                    label='Baldry et al. 2008 (z=0.1)')

    # This next line is just to get the shaded region to appear correctly in the legend.
    ax.plot(np.nan, np.nan, label='Baldry et al. 2008', color='purple', alpha=0.3)

    return ax

def plot_temporal_smf_data(ax, imf):
    """
    Plots stellar mass function observational data at a variety of redshifts. Uses data
    from Marchesini et al., 2009.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    imf : {"Salpeter", "Chabrier"}
         If "Salpeter", scales the x-values of the Marchesini data by a factor of 1.6.
         If "Chabrier", scales the x-values of the Marchesini data by a factor of 1.6 / 1.8.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    # Marchesini et al. 2009ApJ...701.1765M SMF, h=0.7

    # We add plots for z=[0.1], z=[1.3,2.0], z=[2.0,3.0] and z=[3.0,4.0].
    labels = ["Marchesini et al. 2009 z=[0.1]", "... z=[1.3,2.0]",
              "... z=[2.0,3.0]", "... z=[3.0,4.0]"]
    colors = ["k", "b", "g", "r"]
    Mstar_exponent = [10.96, 10.91, 10.96, 11.38]
    alpha = [-1.18, -0.99, -1.01, -1.39]
    phistar = [30.87*1e-4, 10.17*1e-4, 3.95*1e-4, 0.53*1e-4]

    # Each redshift is valid over a slightly different mass range.
    M = [np.arange(7.0, 11.8, 0.01), np.arange(9.3, 11.8, 0.01),
         np.arange(9.7, 11.8, 0.01), np.arange(10.0, 11.8, 0.01)]

    # When we're plotting, need to check which IMF we're using.
    if imf == "Salpeter":
        M_plot = [np.log10(10.0**M_vals * 1.6) for M_vals in M]
    elif imf == "Chabrier":
        M_plot = [np.log10(10.0**M_vals * 1.6/1.8) for M_vals in M]
    else:
        print("plot_temporal_smf_data() called with an IMF value of {0}.  Only Salpeter "
              "or Chabrier allowed.".format(imf))
        raise ValueError

    # Shift the mass by Mstar for each.
    shifted_mass = []
    for (mass, Mstar) in zip(M, Mstar_exponent):
        shifted_mass.append(10 ** (mass - Mstar))

    # Then calculate the Phi.
    phi = []
    for (shifted_M, phi_val, alpha_val) in zip(shifted_mass, phistar, alpha):
        phi.append(np.log(10.0) * phi_val * shifted_M ** (alpha_val + 1) * np.exp(-shifted_M))

    # Then plot!
    for (M_vals, phi_vals, label, color) in zip(M_plot, phi, labels, colors):

        ax.plot(M_vals, phi_vals, color=color, label=label, lw=10, ls=":", alpha=0.3)

    return ax


def plot_bmf_data(ax, hubble_h, imf):
    """
    Plots baryonic mass function observational data. Uses data from Bell et al., 2003.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    hubble_h : Float
        Little h value (between 0 and 1). Used to scale the y-values of the Bell data
        which is irrespective of h.

    imf : {"Salpeter", "Chabrier"}
        If "Salpeter", scales the x-values of the Bell data by a factor of 0.7.
        If "Chabrier", scales the x-values of the Bell data by a factor of 0.7 / 1.8.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    # Bell et al. 2003 BMF. They assume h=1.0.
    M = np.arange(7.0, 13.0, 0.01)
    Mstar = np.log10(5.3*1.0e10 /hubble_h/hubble_h)
    alpha = -1.21
    phistar = 0.0108 *hubble_h*hubble_h*hubble_h
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)

    # Bell use a diet Salpeter IMF.
    if imf == "Salpeter":
        # Then to Salpeter.
        mass_shifted = np.log10(10.0**M / 0.7)
    elif imf == "Chabrier":
        # To Salpeter then to Chabrier.
        mass_shifted = np.log10(10.0**M / 0.7 / 1.8)
    else:
        print("plot_bmf_data() called with an IMF value of {0}.  Only Salpeter or "
              "Chabrier allowed.".format(imf))

    ax.plot(mass_shifted, yval, 'g--', lw=1.5,
            label='Bell et al. 2003')

    return ax

def plot_gmf_data(ax, hubble_h):
    """
    Plots gas mass function observational data. Uses data from Baldry et al., 2008.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    hubble_h : Float
        Little h value (between 0 and 1). Used to scale the y-values of the Baldry data
        which is irrespective of h.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    # Baldry+ 2008 modified data used for the MCMC fitting
    Zwaan = np.array([[6.933,   -0.333],
        [7.057,   -0.490],
        [7.209,   -0.698],
        [7.365,   -0.667],
        [7.528,   -0.823],
        [7.647,   -0.958],
        [7.809,   -0.917],
        [7.971,   -0.948],
        [8.112,   -0.927],
        [8.263,   -0.917],
        [8.404,   -1.062],
        [8.566,   -1.177],
        [8.707,   -1.177],
        [8.853,   -1.312],
        [9.010,   -1.344],
        [9.161,   -1.448],
        [9.302,   -1.604],
        [9.448,   -1.792],
        [9.599,   -2.021],
        [9.740,   -2.406],
        [9.897,   -2.615],
        [10.053,  -3.031],
        [10.178,  -3.677],
        [10.335,  -4.448],
        [10.492,  -5.083]        ], dtype=np.float32)

    ObrRaw = np.array([
        [7.300,   -1.104],
        [7.576,   -1.302],
        [7.847,   -1.250],
        [8.133,   -1.240],
        [8.409,   -1.344],
        [8.691,   -1.479],
        [8.956,   -1.792],
        [9.231,   -2.271],
        [9.507,   -3.198],
        [9.788,   -5.062 ]        ], dtype=np.float32)

    ObrCold = np.array([
        [8.009,   -1.042],
        [8.215,   -1.156],
        [8.409,   -0.990],
        [8.604,   -1.156],
        [8.799,   -1.208],
        [9.020,   -1.333],
        [9.194,   -1.385],
        [9.404,   -1.552],
        [9.599,   -1.677],
        [9.788,   -1.812],
        [9.999,   -2.312],
        [10.172,  -2.656],
        [10.362,  -3.500],
        [10.551,  -3.635],
        [10.740,  -5.010]        ], dtype=np.float32)

    ObrCold_xval = np.log10(10**(ObrCold[:, 0])  /hubble_h/hubble_h)
    ObrCold_yval = (10**(ObrCold[:, 1]) * hubble_h*hubble_h*hubble_h)
    Zwaan_xval = np.log10(10**(Zwaan[:, 0]) /hubble_h/hubble_h)
    Zwaan_yval = (10**(Zwaan[:, 1]) * hubble_h*hubble_h*hubble_h)
    ObrRaw_xval = np.log10(10**(ObrRaw[:, 0])  /hubble_h/hubble_h)
    ObrRaw_yval = (10**(ObrRaw[:, 1]) * hubble_h*hubble_h*hubble_h)

    ax.plot(ObrCold_xval, ObrCold_yval, color='black', lw = 7, alpha=0.25,
            label='Obr. \& Raw. 2009 (Cold Gas)')
    ax.plot(Zwaan_xval, Zwaan_yval, color='cyan', lw = 7, alpha=0.25,
            label='Zwaan et al. 2005 (HI)')
    ax.plot(ObrRaw_xval, ObrRaw_yval, color='magenta', lw = 7, alpha=0.25,
            label='Obr. \& Raw. 2009 (H2)')

    return ax


def plot_btf_data(ax):
    """
    Plots baryonic Tully-Fisher observational data. Uses data from Stark, McGaugh &
    Swatter, 2009.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    w = np.arange(0.5, 10.0, 0.5)
    TF = 3.94*w + 1.79
    ax.plot(w, TF, 'b-', lw=2.0, label='Stark, McGaugh \& Swatters 2009')

    return ax


def plot_metallicity_data(ax, imf):
    """
    Plots metallicity observational data. Uses data from Tremonti et al., 2003.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    imf : {"Salpeter", "Chabrier"}
        If "Salpeter", scales the x-values of the Tremonti data by a factor of 1.5.
        If "Chabrier", scales the x-values of the Tremonti data by a factor of 1.5 / 1.8.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    # Tremonti et al. 2003 (h=0.7)
    M = np.arange(7.0, 13.0, 0.1)
    Zobs = -1.492 + 1.847*M - 0.08026*M*M
    # Tremonti use a Kroupa IMF.
    if imf == "Salpeter":
        # Conversion from Kroupa IMF to Salpeter IMF.
        mass_shifted = np.log10(10**M * 1.5)
    elif imf == "Chabrier":
        # Conversion from Kroupa IMF to Salpeter IMF to Chabrier IMF.
        mass_shifted = np.log10(10**M * 1.5 / 1.8)
    else:
        print("plot_metallicity_data() called with an IMF value of {0}.  Only Salpeter "
              "or Chabrier allowed.".format(imf))
        raise ValueError

    ax.plot(mass_shifted, Zobs, 'b-', lw=2.0, label='Tremonti et al. 2003')

    return ax


def plot_bh_bulge_data(ax):
    """
    Plots black hole-bulge relationship observational data. Uses data from Haring & Rix
    2004.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    # Haring & Rix 2004
    w = 10. ** np.arange(20)
    BHdata = 10. ** (8.2 + 1.12 * np.log10(w / 1.0e11))
    ax.plot(np.log10(w), np.log10(BHdata), 'b-', label="Haring \& Rix 2004")

    return ax


def plot_sfrd_data(ax):
    """
    Plots observational data for the evolution of the star formation rate density.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    ObsSFRdensity = np.array([
        [0, 0.0158489, 0, 0, 0.0251189, 0.01000000],
        [0.150000, 0.0173780, 0, 0.300000, 0.0181970, 0.0165959],
        [0.0425000, 0.0239883, 0.0425000, 0.0425000, 0.0269153, 0.0213796],
        [0.200000, 0.0295121, 0.100000, 0.300000, 0.0323594, 0.0269154],
        [0.350000, 0.0147911, 0.200000, 0.500000, 0.0173780, 0.0125893],
        [0.625000, 0.0275423, 0.500000, 0.750000, 0.0331131, 0.0229087],
        [0.825000, 0.0549541, 0.750000, 1.00000, 0.0776247, 0.0389045],
        [0.625000, 0.0794328, 0.500000, 0.750000, 0.0954993, 0.0660693],
        [0.700000, 0.0323594, 0.575000, 0.825000, 0.0371535, 0.0281838],
        [1.25000, 0.0467735, 1.50000, 1.00000, 0.0660693, 0.0331131],
        [0.750000, 0.0549541, 0.500000, 1.00000, 0.0389045, 0.0776247],
        [1.25000, 0.0741310, 1.00000, 1.50000, 0.0524807, 0.104713],
        [1.75000, 0.0562341, 1.50000, 2.00000, 0.0398107, 0.0794328],
        [2.75000, 0.0794328, 2.00000, 3.50000, 0.0562341, 0.112202],
        [4.00000, 0.0309030, 3.50000, 4.50000, 0.0489779, 0.0194984],
        [0.250000, 0.0398107, 0.00000, 0.500000, 0.0239883, 0.0812831],
        [0.750000, 0.0446684, 0.500000, 1.00000, 0.0323594, 0.0776247],
        [1.25000, 0.0630957, 1.00000, 1.50000, 0.0478630, 0.109648],
        [1.75000, 0.0645654, 1.50000, 2.00000, 0.0489779, 0.112202],
        [2.50000, 0.0831764, 2.00000, 3.00000, 0.0512861, 0.158489],
        [3.50000, 0.0776247, 3.00000, 4.00000, 0.0416869, 0.169824],
        [4.50000, 0.0977237, 4.00000, 5.00000, 0.0416869, 0.269153],
        [5.50000, 0.0426580, 5.00000, 6.00000, 0.0177828, 0.165959],
        [3.00000, 0.120226, 2.00000, 4.00000, 0.173780, 0.0831764],
        [3.04000, 0.128825, 2.69000, 3.39000, 0.151356, 0.109648],
        [4.13000, 0.114815, 3.78000, 4.48000, 0.144544, 0.0912011],
        [0.350000, 0.0346737, 0.200000, 0.500000, 0.0537032, 0.0165959],
        [0.750000, 0.0512861, 0.500000, 1.00000, 0.0575440, 0.0436516],
        [1.50000, 0.0691831, 1.00000, 2.00000, 0.0758578, 0.0630957],
        [2.50000, 0.147911, 2.00000, 3.00000, 0.169824, 0.128825],
        [3.50000, 0.0645654, 3.00000, 4.00000, 0.0776247, 0.0512861],
        ], dtype=np.float32)

    ObsRedshift = ObsSFRdensity[:, 0]
    xErrLo = ObsSFRdensity[:, 0]-ObsSFRdensity[:, 2]
    xErrHi = ObsSFRdensity[:, 3]-ObsSFRdensity[:, 0]

    ObsSFR = np.log10(ObsSFRdensity[:, 1])
    yErrLo = np.log10(ObsSFRdensity[:, 1])-np.log10(ObsSFRdensity[:, 4])
    yErrHi = np.log10(ObsSFRdensity[:, 5])-np.log10(ObsSFRdensity[:, 1])

    ax.errorbar(ObsRedshift, ObsSFR, yerr=[yErrLo, yErrHi], xerr=[xErrLo, xErrHi], color='g', lw=1.0, alpha=0.3, marker='o', ls='none', label='Observations')

    return ax


def plot_smd_data(ax, imf):
    """
    Plots observational data for the evolution of the stellar mass density. Uses data from
    Dickenson et al., 2003; Drory et al., 2005; Perez-Gonzalez et al., 2008; Glazebrook et
    al., 2004; Fontana et al., 2006; Rudnick et al., 2006; Elsner et al., 2008.

    Parameters
    ----------

    ax : ``matplotlib`` axes object
        Axis to plot the data on.

    imf : {"Salpeter", "Chabrier"}
        If "Salpeter", scales the x-values by a factor of 1.6.
        If "Chabrier", scales the x-values by a factor of 1.6 / 1.8.

    Returns
    -------

    ax : ``matplotlib`` axes object
        Axis with the data plotted on it.
    """

    # SMD observations taken from Marchesini+ 2009, h=0.7
    # Values are (minz, maxz, rho,-err,+err)
    dickenson2003 = np.array(((0.6,1.4,8.26,0.08,0.08),
                     (1.4,2.0,7.86,0.22,0.33),
                     (2.0,2.5,7.58,0.29,0.54),
                     (2.5,3.0,7.52,0.51,0.48)),float)

    drory2005 = np.array(((0.25,0.75,8.3,0.15,0.15),
                (0.75,1.25,8.16,0.15,0.15),
                (1.25,1.75,8.0,0.16,0.16),
                (1.75,2.25,7.85,0.2,0.2),
                (2.25,3.0,7.75,0.2,0.2),
                (3.0,4.0,7.58,0.2,0.2)),float)

    # Perez-Gonzalez (2008)
    pg2008 = np.array(((0.2,0.4,8.41,0.06,0.06),
             (0.4,0.6,8.37,0.04,0.04),
             (0.6,0.8,8.32,0.05,0.05),
             (0.8,1.0,8.24,0.05,0.05),
             (1.0,1.3,8.15,0.05,0.05),
             (1.3,1.6,7.95,0.07,0.07),
             (1.6,2.0,7.82,0.07,0.07),
             (2.0,2.5,7.67,0.08,0.08),
             (2.5,3.0,7.56,0.18,0.18),
             (3.0,3.5,7.43,0.14,0.14),
             (3.5,4.0,7.29,0.13,0.13)),float)

    glazebrook2004 = np.array(((0.8,1.1,7.98,0.14,0.1),
                     (1.1,1.3,7.62,0.14,0.11),
                     (1.3,1.6,7.9,0.14,0.14),
                     (1.6,2.0,7.49,0.14,0.12)),float)

    fontana2006 = np.array(((0.4,0.6,8.26,0.03,0.03),
                  (0.6,0.8,8.17,0.02,0.02),
                  (0.8,1.0,8.09,0.03,0.03),
                  (1.0,1.3,7.98,0.02,0.02),
                  (1.3,1.6,7.87,0.05,0.05),
                  (1.6,2.0,7.74,0.04,0.04),
                  (2.0,3.0,7.48,0.04,0.04),
                  (3.0,4.0,7.07,0.15,0.11)),float)

    rudnick2006 = np.array(((0.0,1.0,8.17,0.27,0.05),
                  (1.0,1.6,7.99,0.32,0.05),
                  (1.6,2.4,7.88,0.34,0.09),
                  (2.4,3.2,7.71,0.43,0.08)),float)

    elsner2008 = np.array(((0.25,0.75,8.37,0.03,0.03),
                 (0.75,1.25,8.17,0.02,0.02),
                 (1.25,1.75,8.02,0.03,0.03),
                 (1.75,2.25,7.9,0.04,0.04),
                 (2.25,3.0,7.73,0.04,0.04),
                 (3.0,4.0,7.39,0.05,0.05)),float)

    obs = (dickenson2003,drory2005,pg2008,glazebrook2004,
           fontana2006,rudnick2006,elsner2008)

    for o in obs:
        xval = ((o[:,1]-o[:,0])/2.)+o[:,0]
        if imf == "Salpeter":
            yval = np.log10(10**o[:, 2] * 1.6)
        elif imf == "Chabrier":
            yval = np.log10(10**o[:, 2] * 1.6 / 1.8)

        ax.errorbar(xval, yval, xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')

    return ax
