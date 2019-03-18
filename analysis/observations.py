#!/usr/bin/env python
"""
Module to handle plotting of observational data.

Authors: Jacob Seiler, Manodeep Sinha, Darren Croton
"""

import numpy as np
import matplotlib


def plot_smf_data(ax, hubble_h, imf):

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
    if(imf == 1):
        Baldry_xval = Baldry_xval - 0.26  # convert back to Chabrier IMF

    Baldry_yvalU = (Baldry[:, 1]+Baldry[:, 2]) * pow(hubble_h, 3)
    Baldry_yvalL = (Baldry[:, 1]-Baldry[:, 2]) * pow(hubble_h, 3) 

    ax.fill_between(Baldry_xval, Baldry_yvalU, Baldry_yvalL,
                    facecolor='purple', alpha=0.25,
                    label='Baldry et al. 2008 (z=0.1)')

    # This next line is just to get the shaded region to appear correctly in the legend
    ax.plot(np.nan, np.nan, label='Baldry et al. 2008', color='purple', alpha=0.3)

    # Finally plot the data
    # plt.errorbar(
    #     Baldry[:, 0],
    #     Baldry[:, 1],
    #     yerr=Baldry[:, 2],
    #     color='g',
    #     linestyle=':',
    #     lw = 1.5,
    #     label='Baldry et al. 2008',
    #     )

    # # Cole et al. 2001 SMF (h=1.0 converted to h=0.73)
    # M = np.arange(7.0, 13.0, 0.01)
    # Mstar = np.log10(7.07*1.0e10 /model.Hubble_h/model.Hubble_h)
    # alpha = -1.18
    # phistar = 0.009 *model.Hubble_h*model.Hubble_h*model.Hubble_h
    # xval = 10.0 ** (M-Mstar)
    # yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)      
    # plt.plot(M, yval, 'g--', lw=1.5, label='Cole et al. 2001')  # Plot the SMF

    return ax

def plot_bmf_data(ax, hubble_h, imf):

    # Bell et al. 2003 BMF. They assume h=1.0.
    M = np.arange(7.0, 13.0, 0.01)
    Mstar = np.log10(5.3*1.0e10 /hubble_h/hubble_h)
    alpha = -1.21
    phistar = 0.0108 *hubble_h*hubble_h*hubble_h
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)

    if(imf == 0):
        # Converted diet Salpeter IMF to Salpeter IMF.
        ax.plot(np.log10(10.0**M /0.7), yval, 'b-', lw=2.0,
                label='Bell et al. 2003')
    elif(imf == 1):
        # Converted diet Salpeter IMF to Salpeter IMF, then to Chabrier IMF
        ax.plot(np.log10(10.0**M /0.7 /1.8), yval, 'g--', lw=1.5,
                label='Bell et al. 2003')

    return ax

def plot_gmf_data(ax, hubble_h):

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

    w = np.arange(0.5, 10.0, 0.5)
    TF = 3.94*w + 1.79
    ax.plot(w, TF, 'b-', lw=2.0, label='Stark, McGaugh \& Swatters 2009')

    return ax


def plot_metallicity_data(ax, imf):

    # Tremonti et al. 2003 (h=0.7)
    w = np.arange(7.0, 13.0, 0.1)
    Zobs = -1.492 + 1.847*w - 0.08026*w*w
    if imf == 0:
        # Conversion from Kroupa IMF to Salpeter IMF
        ax.plot(np.log10((10**w *1.5)), Zobs, 'b-', lw=2.0, label='Tremonti et al. 2003')
    elif imf == 1:
        # Conversion from Kroupa IMF to Salpeter IMF to Chabrier IMF
        ax.plot(np.log10((10**w *1.5 /1.8)), Zobs, 'b-', lw=2.0, label='Tremonti et al. 2003')

    return ax


def plot_bh_bulge_data(ax): 
    # Haring & Rix 2004
    w = 10. ** np.arange(20)
    BHdata = 10. ** (8.2 + 1.12 * np.log10(w / 1.0e11))
    ax.plot(np.log10(w), np.log10(BHdata), 'b-', label="Haring \& Rix 2004")

    return ax
