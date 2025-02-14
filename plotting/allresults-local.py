#!/usr/bin/env python

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os

from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = '../output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7500       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

OutputFormat = '.png'
plt.rcParams["figure.figsize"] = (8.34,6.25)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 14


# ==================================================================

def read_hdf(filename = None, snap_num = None, param = None):

    property = h5.File(DirName+FileName,'r')
    return np.array(property[snap_num][param])


# ==================================================================

if __name__ == '__main__':

    print('Running allresults (local)\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    # Read galaxy properties
    print('Reading galaxy properties from', DirName+FileName)

    CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    BulgeMass = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
    BlackHoleMass = read_hdf(snap_num = Snapshot, param = 'BlackHoleMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
    MetalsColdGas = read_hdf(snap_num = Snapshot, param = 'MetalsColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    EjectedMass = read_hdf(snap_num = Snapshot, param = 'EjectedMass') * 1.0e10 / Hubble_h
    IntraClusterStars = read_hdf(snap_num = Snapshot, param = 'IntraClusterStars') * 1.0e10 / Hubble_h

    Vvir = read_hdf(snap_num = Snapshot, param = 'Vvir')
    Vmax = read_hdf(snap_num = Snapshot, param = 'Vmax')
    Rvir = read_hdf(snap_num = Snapshot, param = 'Rvir')
    SfrDisk = read_hdf(snap_num = Snapshot, param = 'SfrDisk')
    SfrBulge = read_hdf(snap_num = Snapshot, param = 'SfrBulge')

    CentralGalaxyIndex = read_hdf(snap_num = Snapshot, param = 'CentralGalaxyIndex')
    Type = read_hdf(snap_num = Snapshot, param = 'Type')
    Posx = read_hdf(snap_num = Snapshot, param = 'Posx')
    Posy = read_hdf(snap_num = Snapshot, param = 'Posy')
    Posz = read_hdf(snap_num = Snapshot, param = 'Posz')

    w = np.where(StellarMass > 1.0e10)[0]
    print('Number of galaxies read:', len(StellarMass))
    print('Galaxies more massive than 10^10 h-1 Msun:', len(w), '\n')


# --------------------------------------------------------

    print('Plotting the stellar mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width

    # calculate all
    w = np.where(StellarMass > 0.0)[0]
    mass = np.log10(StellarMass[w])
    sSFR = np.log10( (SfrDisk[w] + SfrBulge[w]) / StellarMass[w] )

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins
    
    # additionally calculate red
    w = np.where(sSFR < sSFRcut)[0]
    massRED = mass[w]
    (countsRED, binedges) = np.histogram(massRED, range=(mi, ma), bins=NB)

    # additionally calculate blue
    w = np.where(sSFR > sSFRcut)[0]
    massBLU = mass[w]
    (countsBLU, binedges) = np.histogram(massBLU, range=(mi, ma), bins=NB)

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

    Baldry_xval = np.log10(10 ** Baldry[:, 0]  /Hubble_h/Hubble_h)
    if(whichimf == 1):  Baldry_xval = Baldry_xval - 0.26  # convert back to Chabrier IMF
    Baldry_yvalU = (Baldry[:, 1]+Baldry[:, 2]) * Hubble_h*Hubble_h*Hubble_h
    Baldry_yvalL = (Baldry[:, 1]-Baldry[:, 2]) * Hubble_h*Hubble_h*Hubble_h
    plt.fill_between(Baldry_xval, Baldry_yvalU, Baldry_yvalL, 
        facecolor='purple', alpha=0.25, label='Baldry et al. 2008 (z=0.1)')
    
    # This next line is just to get the shaded region to appear correctly in the legend
    plt.plot(xaxeshisto, counts / volume / binwidth, label='Baldry et al. 2008', color='purple', alpha=0.3)
    
    # Overplot the model histograms
    plt.plot(xaxeshisto, counts    / volume / binwidth, 'k-', label='Model - All')
    plt.plot(xaxeshisto, countsRED / volume / binwidth, 'r:', lw=2, label='Model - Red')
    plt.plot(xaxeshisto, countsBLU / volume / binwidth, 'b:', lw=2, label='Model - Blue')

    plt.yscale('log')
    plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '1.StellarMassFunction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the baryonic mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width
  
    # calculate BMF
    w = np.where(StellarMass + ColdGas > 0.0)[0]
    mass = np.log10( (StellarMass[w] + ColdGas[w]) )

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins
   
    # Bell et al. 2003 BMF (h=1.0 converted to h=0.73)
    M = np.arange(7.0, 13.0, 0.01)
    Mstar = np.log10(5.3*1.0e10 /Hubble_h/Hubble_h)
    alpha = -1.21
    phistar = 0.0108 *Hubble_h*Hubble_h*Hubble_h
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)
    
    if(whichimf == 0):
        # converted diet Salpeter IMF to Salpeter IMF
        plt.plot(np.log10(10.0**M /0.7), yval, 'b-', lw=2.0, label='Bell et al. 2003')  # Plot the SMF
    elif(whichimf == 1):
        # converted diet Salpeter IMF to Salpeter IMF, then to Chabrier IMF
        plt.plot(np.log10(10.0**M /0.7 /1.8), yval, 'g--', lw=1.5, label='Bell et al. 2003')  # Plot the SMF

    # Overplot the model histograms
    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model')

    plt.yscale('log')
    plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
    
    outputFile = OutputDir + '2.BaryonicMassFunction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the cold gas mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width

    # calculate all
    w = np.where(ColdGas > 0.0)[0]
    mass = np.log10(ColdGas[w])
    sSFR = (SfrDisk[w] + SfrBulge[w]) / StellarMass[w]

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins
    
    # additionally calculate red
    w = np.where(sSFR < sSFRcut)[0]
    massRED = mass[w]
    (countsRED, binedges) = np.histogram(massRED, range=(mi, ma), bins=NB)

    # additionally calculate blue
    w = np.where(sSFR > sSFRcut)[0]
    massBLU = mass[w]
    (countsBLU, binedges) = np.histogram(massBLU, range=(mi, ma), bins=NB)

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
    
    ObrCold_xval = np.log10(10**(ObrCold[:, 0])  /Hubble_h/Hubble_h)
    ObrCold_yval = (10**(ObrCold[:, 1]) * Hubble_h*Hubble_h*Hubble_h)
    Zwaan_xval = np.log10(10**(Zwaan[:, 0]) /Hubble_h/Hubble_h)
    Zwaan_yval = (10**(Zwaan[:, 1]) * Hubble_h*Hubble_h*Hubble_h)
    ObrRaw_xval = np.log10(10**(ObrRaw[:, 0])  /Hubble_h/Hubble_h)
    ObrRaw_yval = (10**(ObrRaw[:, 1]) * Hubble_h*Hubble_h*Hubble_h)

    plt.plot(ObrCold_xval, ObrCold_yval, color='black', lw = 7, alpha=0.25, label='Obr. & Raw. 2009 (Cold Gas)')
    plt.plot(Zwaan_xval, Zwaan_yval, color='cyan', lw = 7, alpha=0.25, label='Zwaan et al. 2005 (HI)')
    plt.plot(ObrRaw_xval, ObrRaw_yval, color='magenta', lw = 7, alpha=0.25, label='Obr. & Raw. 2009 (H2)')
    
    # Overplot the model histograms
    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model - Cold Gas')

    plt.yscale('log')
    plt.axis([8.0, 11.5, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{X}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '3.GasMassFunction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the baryonic TF relationship')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (StellarMass + ColdGas > 0.0) & 
      (BulgeMass / StellarMass > 0.1) & (BulgeMass / StellarMass < 0.5))[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    mass = np.log10( (StellarMass[w] + ColdGas[w]) )
    vel = np.log10(Vmax[w])
                
    plt.scatter(vel, mass, marker='o', s=1, c='k', alpha=0.5, label='Model Sb/c galaxies')
            
    # overplot Stark, McGaugh & Swatters 2009 (assumes h=0.75? ... what IMF?)
    w = np.arange(0.5, 10.0, 0.5)
    TF = 3.94*w + 1.79
    plt.plot(w, TF, 'b-', lw=2.0, label='Stark, McGaugh & Swatters 2009')
        
    plt.ylabel(r'$\log_{10}\ M_{\mathrm{bar}}\ (M_{\odot})$')  # Set the y...
    plt.xlabel(r'$\log_{10}V_{max}\ (km/s)$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([1.4, 2.6, 8.0, 12.0])
        
    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '4.BaryonicTullyFisher' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the specific sSFR')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where(StellarMass > 0.01)[0]
    if(len(w) > dilute): w = sample(list(w), dilute)
    mass = np.log10(StellarMass[w])
    sSFR = np.log10( (SfrDisk[w] + SfrBulge[w]) / StellarMass[w] )
    plt.scatter(mass, sSFR, marker='o', s=1, c='k', alpha=0.5, label='Model galaxies')

    # overplot dividing line between SF and passive
    w = np.arange(7.0, 13.0, 1.0)
    plt.plot(w, w/w*sSFRcut, 'b:', lw=2.0)

    plt.ylabel(r'$\log_{10}\ s\mathrm{SFR}\ (\mathrm{yr^{-1}})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.axis([8.0, 12.0, -16.0, -8.0])

    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '5.SpecificStarFormationRate' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the gas fractions')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (StellarMass + ColdGas > 0.0) & 
      (BulgeMass / StellarMass > 0.1) & (BulgeMass / StellarMass < 0.5))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)
    
    mass = np.log10(StellarMass[w])
    fraction = ColdGas[w] / (StellarMass[w] + ColdGas[w])

    plt.scatter(mass, fraction, marker='o', s=1, c='k', alpha=0.5, label='Model Sb/c galaxies')
        
    plt.ylabel(r'$\mathrm{Cold\ Mass\ /\ (Cold+Stellar\ Mass)}$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([8.0, 12.0, 0.0, 1.0])
        
    leg = plt.legend(loc='upper right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '6.GasFraction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the metallicities')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (ColdGas / (StellarMass + ColdGas) > 0.1) & (StellarMass > 1.0e8))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)
    
    mass = np.log10(StellarMass[w])
    Z = np.log10((MetalsColdGas[w] / ColdGas[w]) / 0.02) + 9.0
    
    plt.scatter(mass, Z, marker='o', s=1, c='k', alpha=0.5, label='Model galaxies')
        
    # overplot Tremonti et al. 2003 (h=0.7)
    w = np.arange(7.0, 13.0, 0.1)
    Zobs = -1.492 + 1.847*w - 0.08026*w*w
    if(whichimf == 0):
        # Conversion from Kroupa IMF to Slapeter IMF
        plt.plot(np.log10((10**w *1.5)), Zobs, 'b-', lw=2.0, label='Tremonti et al. 2003')
    elif(whichimf == 1):
        # Conversion from Kroupa IMF to Slapeter IMF to Chabrier IMF
        plt.plot(np.log10((10**w *1.5 /1.8)), Zobs, 'b-', lw=2.0, label='Tremonti et al. 2003')
        
    plt.ylabel(r'$12\ +\ \log_{10}[\mathrm{O/H}]$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([8.0, 12.0, 8.0, 9.5])
        
    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '7.Metallicity' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the black hole-bulge relationship')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((BulgeMass > 1.0e8) & (BlackHoleMass > 01.0e6))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    bh = np.log10(BlackHoleMass[w])
    bulge = np.log10(BulgeMass[w])
                
    plt.scatter(bulge, bh, marker='o', s=1, c='k', alpha=0.5, label='Model galaxies')
            
    # overplot Haring & Rix 2004
    w = 10. ** np.arange(20)
    BHdata = 10. ** (8.2 + 1.12 * np.log10(w / 1.0e11))
    plt.plot(np.log10(w), np.log10(BHdata), 'b-', label="Haring \& Rix 2004")
    plt.ylabel(r'$\log\ M_{\mathrm{BH}}\ (M_{\odot})$')  # Set the y...
    plt.xlabel(r'$\log\ M_{\mathrm{bulge}}\ (M_{\odot})$')  # and the x-axis labels
        
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
        
    plt.axis([8.0, 12.0, 6.0, 10.0])
        
    leg = plt.legend(loc='upper left')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '8.BlackHoleBulgeRelationship' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the quiescent fraction vs stellar mass')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    groupscale = 12.5
    
    w = np.where(StellarMass > 0.0)[0]
    stars = np.log10(StellarMass[w])
    halo = np.log10(CentralMvir[w])
    galtype = Type[w]
    sSFR = (SfrDisk[w] + SfrBulge[w]) / StellarMass[w]

    MinRange = 9.5
    MaxRange = 12.0
    Interval = 0.1
    Nbins = int((MaxRange-MinRange)/Interval)
    Range = np.arange(MinRange, MaxRange, Interval)
    
    Mass = []
    Fraction = []
    CentralFraction = []
    SatelliteFraction = []
    SatelliteFractionLo = []
    SatelliteFractionHi = []

    for i in range(Nbins-1):
        
        w = np.where((stars >= Range[i]) & (stars < Range[i+1]))[0]
        if len(w) > 0:
            wQ = np.where((stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut))[0]
            Fraction.append(1.0*len(wQ) / len(w))
        else:
            Fraction.append(0.0)
        
        w = np.where((galtype == 0) & (stars >= Range[i]) & (stars < Range[i+1]))[0]
        if len(w) > 0:
            wQ = np.where((galtype == 0) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut))[0]
            CentralFraction.append(1.0*len(wQ) / len(w))
        else:
            CentralFraction.append(0.0)
        
        w = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]))[0]
        if len(w) > 0:
            wQ = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut))[0]
            SatelliteFraction.append(1.0*len(wQ) / len(w))
            wQ = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut) & (halo < groupscale))[0]
            SatelliteFractionLo.append(1.0*len(wQ) / len(w))
            wQ = np.where((galtype == 1) & (stars >= Range[i]) & (stars < Range[i+1]) & (sSFR < 10.0**sSFRcut) & (halo > groupscale))[0]
            SatelliteFractionHi.append(1.0*len(wQ) / len(w))                
        else:
            SatelliteFraction.append(0.0)
            SatelliteFractionLo.append(0.0)
            SatelliteFractionHi.append(0.0)
            
        Mass.append((Range[i] + Range[i+1]) / 2.0)                
    
    Mass = np.array(Mass)
    Fraction = np.array(Fraction)
    CentralFraction = np.array(CentralFraction)
    SatelliteFraction = np.array(SatelliteFraction)
    SatelliteFractionLo = np.array(SatelliteFractionLo)
    SatelliteFractionHi = np.array(SatelliteFractionHi)
    
    w = np.where(Fraction > 0)[0]
    plt.plot(Mass[w], Fraction[w], label='All')

    w = np.where(CentralFraction > 0)[0]
    plt.plot(Mass[w], CentralFraction[w], color='Blue', label='Centrals')

    w = np.where(SatelliteFraction > 0)[0]
    plt.plot(Mass[w], SatelliteFraction[w], color='Red', label='Satellites')

    w = np.where(SatelliteFractionLo > 0)[0]
    plt.plot(Mass[w], SatelliteFractionLo[w], 'r--', label='Satellites-Lo')

    w = np.where(SatelliteFractionHi > 0)[0]
    plt.plot(Mass[w], SatelliteFractionHi[w], 'r-.', label='Satellites-Hi')
    
    plt.xlabel(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')  # Set the x-axis label
    plt.ylabel(r'$\mathrm{Quescient\ Fraction}$')  # Set the y-axis label
    
    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
    
    plt.axis([9.5, 12.0, 0.0, 1.05])
    
    leg = plt.legend(loc='lower right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
    
    outputFile = OutputDir + '9.QuiescentFraction' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the mass fraction of galaxies')

    w = np.where(StellarMass > 0.0)[0]
    fBulge = BulgeMass[w] / StellarMass[w]
    fDisk = 1.0 - fBulge
    mass = np.log10(StellarMass[w])
    sSFR = np.log10((SfrDisk[w] + SfrBulge[w]) / StellarMass[w])
    
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
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the average baryon fraction vs halo mass')

    plt.figure()
    ax = plt.subplot(111)

    HaloMass = np.log10(Mvir)
    Baryons = StellarMass + ColdGas + HotGas + EjectedMass + IntraClusterStars + BlackHoleMass

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

    # Pre-compute central indices for faster lookup
    unique_centrals = np.unique(CentralGalaxyIndex)

    for i in range(Nbins-1):
        w1 = np.where((Type == 0) & (HaloMass >= HaloRange[i]) & (HaloMass < HaloRange[i+1]))[0]
        HalosFound = len(w1)
        
        if HalosFound > 2:
            # Get unique central galaxies in this mass bin
            centrals_in_bin = CentralGalaxyIndex[w1]
            
            # Create masks for all galaxies belonging to these centrals
            central_groups = np.isin(CentralGalaxyIndex, centrals_in_bin)
            
            # Calculate statistics for all groups at once
            group_baryons = Baryons[central_groups]
            group_stars = StellarMass[central_groups]
            group_cold = ColdGas[central_groups]
            group_hot = HotGas[central_groups]
            group_ejected = EjectedMass[central_groups]
            group_ics = IntraClusterStars[central_groups]
            group_bh = BlackHoleMass[central_groups]
            
            # Sum quantities per central galaxy
            unique_centrals_in_bin = np.unique(CentralGalaxyIndex[central_groups])
            baryon_sums = np.zeros(len(unique_centrals_in_bin))
            star_sums = np.zeros(len(unique_centrals_in_bin))
            cold_sums = np.zeros(len(unique_centrals_in_bin))
            hot_sums = np.zeros(len(unique_centrals_in_bin))
            ejected_sums = np.zeros(len(unique_centrals_in_bin))
            ics_sums = np.zeros(len(unique_centrals_in_bin))
            bh_sums = np.zeros(len(unique_centrals_in_bin))
            
            for idx, central in enumerate(unique_centrals_in_bin):
                group_mask = CentralGalaxyIndex[central_groups] == central
                baryon_sums[idx] = np.sum(group_baryons[group_mask])
                star_sums[idx] = np.sum(group_stars[group_mask])
                cold_sums[idx] = np.sum(group_cold[group_mask])
                hot_sums[idx] = np.sum(group_hot[group_mask])
                ejected_sums[idx] = np.sum(group_ejected[group_mask])
                ics_sums[idx] = np.sum(group_ics[group_mask])
                bh_sums[idx] = np.sum(group_bh[group_mask])
            
            # Get corresponding halo masses
            central_halos = Mvir[w1]
            
            # Calculate fractions
            baryon_fractions = baryon_sums / central_halos
            star_fractions = star_sums / central_halos
            cold_fractions = cold_sums / central_halos
            hot_fractions = hot_sums / central_halos
            ejected_fractions = ejected_sums / central_halos
            ics_fractions = ics_sums / central_halos
            bh_fractions = bh_sums / central_halos
            
            # Store means and variances
            MeanCentralHaloMass.append(np.mean(np.log10(central_halos)))
            MeanBaryonFraction.append(np.mean(baryon_fractions))
            MeanBaryonFractionU.append(np.mean(baryon_fractions) + np.var(baryon_fractions))
            MeanBaryonFractionL.append(np.mean(baryon_fractions) - np.var(baryon_fractions))
            
            MeanStars.append(np.mean(star_fractions))
            MeanCold.append(np.mean(cold_fractions))
            MeanHot.append(np.mean(hot_fractions))
            MeanEjected.append(np.mean(ejected_fractions))
            MeanICS.append(np.mean(ics_fractions))
            MeanBH.append(np.mean(bh_fractions))

    # Convert lists to arrays for plotting
    MeanCentralHaloMass = np.array(MeanCentralHaloMass)
    MeanBaryonFraction = np.array(MeanBaryonFraction)
    MeanBaryonFractionU = np.array(MeanBaryonFractionU)
    MeanBaryonFractionL = np.array(MeanBaryonFractionL)
    MeanStars = np.array(MeanStars)
    MeanCold = np.array(MeanCold)
    MeanHot = np.array(MeanHot)
    MeanEjected = np.array(MeanEjected)
    MeanICS = np.array(MeanICS)

    # Now do all the plotting
    plt.plot(MeanCentralHaloMass, MeanBaryonFraction, 'k-', label='TOTAL')
    plt.fill_between(MeanCentralHaloMass, MeanBaryonFractionU, MeanBaryonFractionL,
        facecolor='purple', alpha=0.25, label='TOTAL')

    plt.plot(MeanCentralHaloMass, MeanStars, 'k--', label='Stars')
    plt.plot(MeanCentralHaloMass, MeanCold, label='Cold', color='blue')
    plt.plot(MeanCentralHaloMass, MeanHot, label='Hot', color='red')
    plt.plot(MeanCentralHaloMass, MeanEjected, label='Ejected', color='green')
    plt.plot(MeanCentralHaloMass, MeanICS, label='ICS', color='yellow')

    plt.xlabel(r'$\mathrm{Central}\ \log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    plt.ylabel(r'$\mathrm{Baryon\ Fraction}$')

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.axis([10.8, 15.0, 0.0, 0.2])

    leg = plt.legend(bbox_to_anchor=[0.99, 0.6])
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    outputFile = OutputDir + '11.BaryonFraction' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the mass in stellar, cold, hot, ejected, ICS reservoirs')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (Mvir > 1.0e10) & (StellarMass > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    HaloMass = np.log10(Mvir[w])
    plt.scatter(HaloMass, np.log10(StellarMass[w]), marker='o', s=0.3, c='k', alpha=0.5, label='Stars')
    plt.scatter(HaloMass, np.log10(ColdGas[w]), marker='o', s=0.3, color='blue', alpha=0.5, label='Cold gas')
    plt.scatter(HaloMass, np.log10(HotGas[w]), marker='o', s=0.3, color='red', alpha=0.5, label='Hot gas')
    plt.scatter(HaloMass, np.log10(EjectedMass[w]), marker='o', s=0.3, color='green', alpha=0.5, label='Ejected gas')
    plt.scatter(HaloMass, np.log10(IntraClusterStars[w]), marker='o', s=10, color='yellow', alpha=0.5, label='Intracluster stars')

    plt.ylabel(r'$\mathrm{stellar,\ cold,\ hot,\ ejected,\ ICS\ mass}$')  # Set the y...
    plt.xlabel(r'$\log\ M_{\mathrm{vir}}\ (h^{-1}\ M_{\odot})$')  # and the x-axis labels
    
    plt.axis([10.0, 15.0, 7.5, 14.0])

    leg = plt.legend(loc='upper left')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')
        
    outputFile = OutputDir + '12.MassReservoirScatter' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting the spatial distribution of all galaxies')

    plt.figure()  # New figure

    w = np.where((Mvir > 0.0) & (StellarMass > 1.0e9))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]
    zz = Posz[w]

    buff = BoxSize*0.1

    ax = plt.subplot(221)  # 1 plot on the figure
    plt.scatter(xx, yy, marker='o', s=0.3, c='k', alpha=0.5)
    plt.axis([0.0-buff, BoxSize+buff, 0.0-buff, BoxSize+buff])
    plt.ylabel(r'$\mathrm{x}$')  # Set the y...
    plt.xlabel(r'$\mathrm{y}$')  # and the x-axis labels
    
    ax = plt.subplot(222)  # 1 plot on the figure
    plt.scatter(xx, zz, marker='o', s=0.3, c='k', alpha=0.5)
    plt.axis([0.0-buff, BoxSize+buff, 0.0-buff, BoxSize+buff])
    plt.ylabel(r'$\mathrm{x}$')  # Set the y...
    plt.xlabel(r'$\mathrm{z}$')  # and the x-axis labels
    
    ax = plt.subplot(223)  # 1 plot on the figure
    plt.scatter(yy, zz, marker='o', s=0.3, c='k', alpha=0.5)
    plt.axis([0.0-buff, BoxSize+buff, 0.0-buff, BoxSize+buff])
    plt.ylabel(r'$\mathrm{y}$')  # Set the y...
    plt.xlabel(r'$\mathrm{z}$')  # and the x-axis labels
        
    outputFile = OutputDir + '13.SpatialDistribution' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

