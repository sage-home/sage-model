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
DirName = './output/millennium/'
FileName = 'model_0.hdf5'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model
FirstSnap = 0          # First snapshot to read
LastSnap = 63          # Last snapshot to read
redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 11.897, 10.944, 10.073, 
             9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 
             2.831, 2.619, 2.422, 2.239, 2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 0.242, 0.208, 0.175, 0.144, 
             0.116, 0.089, 0.064, 0.041, 0.020, 0.000]  # Redshift of each snapshot

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7500       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies
SMFsnaps = [63, 37, 32, 27, 23, 20, 18, 16]  # Snapshots to plot the SMF

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

    print('Running allresults (history)\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    # Read galaxy properties
    print('Reading galaxy properties from', DirName+FileName, '\n')

    StellarMassFull = [0]*(LastSnap-FirstSnap+1)
    SfrDiskFull = [0]*(LastSnap-FirstSnap+1)
    SfrBulgeFull = [0]*(LastSnap-FirstSnap+1)
    BlackHoleMassFull = [0]*(LastSnap-FirstSnap+1)
    BulgeMassFull = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull = [0]*(LastSnap-FirstSnap+1)
    cgmFull = [0]*(LastSnap-FirstSnap+1)
    hotgasFull = [0]*(LastSnap-FirstSnap+1)
    fullcgmFull = [0]*(LastSnap-FirstSnap+1)
    TypeFull = [0]*(LastSnap-FirstSnap+1)
    OutflowRateFull = [0]*(LastSnap-FirstSnap+1)
    coldgasFull = [0]*(LastSnap-FirstSnap+1)
    dT = [0]*(LastSnap-FirstSnap+1)
    RegimeFull = [0]*(LastSnap-FirstSnap+1)

    for snap in range(FirstSnap,LastSnap+1):

        Snapshot = 'Snap_'+str(snap)

        StellarMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
        SfrDiskFull[snap] = read_hdf(snap_num = Snapshot, param = 'SfrDisk')
        SfrBulgeFull[snap] = read_hdf(snap_num = Snapshot, param = 'SfrBulge')
        BlackHoleMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'BlackHoleMass') * 1.0e10 / Hubble_h
        BulgeMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
        HaloMassFull[snap] = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
        cgmFull[snap] = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
        hotgasFull[snap] = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
        fullcgmFull[snap] = (read_hdf(snap_num = Snapshot, param = 'CGMgas') + read_hdf(snap_num = Snapshot, param = 'HotGas')) * 1.0e10 / Hubble_h
        TypeFull[snap] = read_hdf(snap_num = Snapshot, param = 'Type')
        OutflowRateFull[snap] = read_hdf(snap_num = Snapshot, param = 'OutflowRate')
        coldgasFull[snap] = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
        dT[snap] = read_hdf(snap_num = Snapshot, param = 'dT')
        RegimeFull[snap] = read_hdf(snap_num = Snapshot, param = 'Regime')


# --------------------------------------------------------

    print('Plotting the stellar mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
    # Marchesini et al. 2009ApJ...701.1765M SMF, h=0.7

    M = np.arange(7.0, 11.8, 0.01)
    Mstar = np.log10(10.0**10.96)
    alpha = -1.18
    phistar = 30.87*1e-4
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)
    if(whichimf == 0):
        plt.plot(np.log10(10.0**M *1.6), yval, ':', lw=10, alpha=0.5, label='Marchesini et al. 2009 z=[0.1]')
    elif(whichimf == 1):
        plt.plot(np.log10(10.0**M *1.6 /1.8), yval, ':', lw=10, alpha=0.5, label='Marchesini et al. 2009 z=[0.1]')
    
    M = np.arange(9.3, 11.8, 0.01)
    Mstar = np.log10(10.0**10.91)
    alpha = -0.99
    phistar = 10.17*1e-4
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)
    if(whichimf == 0):
        plt.plot(np.log10(10.0**M *1.6), yval, 'b:', lw=10, alpha=0.5, label='... z=[1.3,2.0]')
    elif(whichimf == 1):
        plt.plot(np.log10(10.0**M *1.6/1.8), yval, 'b:', lw=10, alpha=0.5, label='... z=[1.3,2.0]')
    
    M = np.arange(9.7, 11.8, 0.01)
    Mstar = np.log10(10.0**10.96)
    alpha = -1.01
    phistar = 3.95*1e-4
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)
    if(whichimf == 0):
        plt.plot(np.log10(10.0**M *1.6), yval, 'g:', lw=10, alpha=0.5, label='... z=[2.0,3.0]')
    elif(whichimf == 1):
        plt.plot(np.log10(10.0**M *1.6/1.8), yval, 'g:', lw=10, alpha=0.5, label='... z=[2.0,3.0]')
    
    M = np.arange(10.0, 11.8, 0.01)
    Mstar = np.log10(10.0**11.38)
    alpha = -1.39
    phistar = 0.53*1e-4
    xval = 10.0 ** (M-Mstar)
    yval = np.log(10.) * phistar * xval ** (alpha+1) * np.exp(-xval)
    if(whichimf == 0):
        plt.plot(np.log10(10.0**M *1.6), yval, 'r:', lw=10, alpha=0.5, label='... z=[3.0,4.0]')
    elif(whichimf == 1):
        plt.plot(np.log10(10.0**M *1.6/1.8), yval, 'r:', lw=10, alpha=0.5, label='... z=[3.0,4.0]')
    
    ###### z=0

    w = np.where(StellarMassFull[SMFsnaps[0]] > 0.0)[0]
    mass = np.log10(StellarMassFull[SMFsnaps[0]][w])

    binwidth = 0.1
    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'k-', label='Model galaxies')

    ###### z=1.3
    
    w = np.where(StellarMassFull[SMFsnaps[1]] > 0.0)[0]
    mass = np.log10(StellarMassFull[SMFsnaps[1]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'b-')

    ###### z=2
    
    w = np.where(StellarMassFull[SMFsnaps[2]] > 0.0)[0]
    mass = np.log10(StellarMassFull[SMFsnaps[2]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'g-')

    ###### z=3
    
    w = np.where(StellarMassFull[SMFsnaps[3]] > 0.0)[0]
    mass = np.log10(StellarMassFull[SMFsnaps[3]][w])

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth

    plt.plot(xaxeshisto, counts / volume / binwidth, 'r-')

    ######

    plt.yscale('log')
    plt.axis([7.0, 12.2, 1.0e-6, 1.0e-1])

    # Set the x-axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}$)')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + 'A.StellarMassFunction_z' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting SFR density evolution for all galaxies')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

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
    xErrLo = np.abs(ObsSFRdensity[:, 0]-ObsSFRdensity[:, 2])
    xErrHi = np.abs(ObsSFRdensity[:, 3]-ObsSFRdensity[:, 0])
    
    ObsSFR = np.log10(ObsSFRdensity[:, 1])
    yErrLo = np.abs(np.log10(ObsSFRdensity[:, 1])-np.log10(ObsSFRdensity[:, 4]))
    yErrHi = np.abs(np.log10(ObsSFRdensity[:, 5])-np.log10(ObsSFRdensity[:, 1]))

    # plot observational data (compilation used in Croton et al. 2006)
    plt.errorbar(ObsRedshift, ObsSFR, yerr=[yErrLo, yErrHi], xerr=[xErrLo, xErrHi], color='g', lw=1.0, alpha=0.3, marker='o', ls='none', label='Observations')
    
    SFR_density = np.zeros((LastSnap+1-FirstSnap))       
    for snap in range(FirstSnap,LastSnap+1):
        SFR_density[snap-FirstSnap] = sum(SfrDiskFull[snap]+SfrBulgeFull[snap]) / volume

    z = np.array(redshifts)
    nonzero = np.where(SFR_density > 0.0)[0]
    plt.plot(z[nonzero], np.log10(SFR_density[nonzero]), lw=3.0)

    plt.ylabel(r'$\log_{10} \mathrm{SFR\ density}\ (M_{\odot}\ \mathrm{yr}^{-1}\ \mathrm{Mpc}^{-3})$')  # Set the y...
    plt.xlabel(r'$\mathrm{redshift}$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    plt.axis([0.0, 8.0, -3.0, -0.4])            

    outputFile = OutputDir + 'B.History-SFR-density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting stellar mass density evolution')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure
    
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
    PerezGonzalez2008 = np.array(((0.2,0.4,8.41,0.06,0.06),
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
    
    obs = (dickenson2003,drory2005,PerezGonzalez2008,glazebrook2004,fontana2006,rudnick2006,elsner2008)
    
    for o in obs:
        xval = ((o[:,1]-o[:,0])/2.)+o[:,0]
        if(whichimf == 0):
            ax.errorbar(xval, np.log10(10**o[:,2] *1.6), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')
        elif(whichimf == 1):
            ax.errorbar(xval, np.log10(10**o[:,2] *1.6/1.8), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')
            
    smd = np.zeros((LastSnap+1-FirstSnap))       
    for snap in range(FirstSnap,LastSnap+1):
      w = np.where((StellarMassFull[snap] > 1.0e8) & (StellarMassFull[snap] < 1.0e13))[0]
      if(len(w) > 0):
        smd[snap-FirstSnap] = sum(StellarMassFull[snap][w]) / volume

    z = np.array(redshifts)
    nonzero = np.where(smd > 0.0)[0]
    plt.plot(z[nonzero], np.log10(smd[nonzero]), 'k-', lw=3.0)

    plt.ylabel(r'$\log_{10}\ \phi\ (M_{\odot}\ \mathrm{Mpc}^{-3})$')  # Set the y...
    plt.xlabel(r'$\mathrm{redshift}$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(1))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.5))

    plt.axis([0.0, 4.2, 6.5, 9.0])   

    outputFile = OutputDir + 'C.History-stellar-mass-density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
   
    print('Plotting SFR evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Define redshift bins to match the figure
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    # Define colormap - from dark to light/orange
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    # Function to find snapshots within a redshift range
    def find_snapshots_in_z_range(z_min, z_max, redshifts):
        """Find snapshot indices within redshift range"""
        snapshot_indices = []
        for i, z in enumerate(redshifts):
            if z_min <= z <= z_max:
                snapshot_indices.append(i)
        return snapshot_indices

    # Use the same z_bins and colors as above
    # z_bins and colors already defined

    for i, (z_min, z_max) in enumerate(z_bins):
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        if len(snap_indices) == 0:
            continue

        # Use same mass bins as above
        if i >= len(z_bins) - 4:
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.2, 12.5, 0.1)
        mass_centers = mass_bins[:-1] + 0.05
        bin_width = mass_bins[1] - mass_bins[0]

        sfr_snapshots = []
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                # Only consider galaxies with positive stellar mass and SFR
                sfr_total = SfrDiskFull[snap_idx] + SfrBulgeFull[snap_idx]
                w = np.where((StellarMassFull[snap_idx] > 0.0) & (sfr_total > 0.0))[0]
                if len(w) > 0:
                    masses = np.log10(StellarMassFull[snap_idx][w])
                    sfrs = np.log10(sfr_total[w])
                    # Bin by stellar mass
                    sfr_bin_means = np.zeros(len(mass_centers))
                    sfr_bin_means.fill(np.nan)
                    sfr_bin_stds = np.zeros(len(mass_centers))
                    sfr_bin_stds.fill(np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1])
                        if np.sum(mask) > 0:
                            sfr_bin_means[j] = np.mean(sfrs[mask])
                            sfr_bin_stds[j] = np.std(sfrs[mask])
                    sfr_snapshots.append((sfr_bin_means, sfr_bin_stds))
        if len(sfr_snapshots) == 0:
            continue
        # Stack and compute mean and error
        sfr_bin_means_stack = np.array([x[0] for x in sfr_snapshots])
        sfr_bin_stds_stack = np.array([x[1] for x in sfr_snapshots])
        sfr_mean = np.nanmean(sfr_bin_means_stack, axis=0)
        sfr_err = np.nanstd(sfr_bin_means_stack, axis=0) / np.sqrt(len(sfr_bin_means_stack))
        valid = ~np.isnan(sfr_mean) & ~np.isnan(sfr_err)
        if not np.any(valid):
            continue
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], sfr_mean[valid], color=colors[i], linewidth=2, label=label)
        ax.fill_between(mass_centers[valid], sfr_mean[valid] - sfr_err[valid], sfr_mean[valid] + sfr_err[valid], color=colors[i], alpha=0.3)

    ax.set_xlim(8.0, 12.2)
    ax.set_ylim(-4, 3)
    ax.set_xlabel(r'$\log_{10} M_* [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\log_{{10}}\mathrm{{SFR}}\ [M_\odot/yr]$', fontsize=14)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    leg = ax.legend(loc='lower right', fontsize=10, frameon=False, ncol=3)
    for text in leg.get_texts():
        text.set_fontsize(10)
    plt.tight_layout()
    outputFile = OutputDir + 'D.SFR_evolution' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
    print('Plotting stellar mass-halo mass relation evolution')

    plt.figure()
    ax = plt.subplot(111)

    # Define redshift bins (same as before)
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    # Define colormap and line styles
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))
    line_styles = ['-', ':', '--', '-.'] * 4  # Cycle through line styles

    # Halo mass bins
    halo_mass_bins = np.arange(10.0, 15.0, 0.1)
    halo_mass_centers = halo_mass_bins[:-1] + 0.05

    # Function to find snapshots within a redshift range
    def find_snapshots_in_z_range(z_min, z_max, redshifts):
        """Find snapshot indices within redshift range"""
        snapshot_indices = []
        for i, z in enumerate(redshifts):
            if z_min <= z <= z_max:
                snapshot_indices.append(i)
        return snapshot_indices

    # Calculate SMHM relation for each redshift bin
    for i, (z_min, z_max) in enumerate(z_bins):
        # Find snapshots in this redshift range
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        
        if len(snap_indices) == 0:
            continue
            
        # Calculate SMHM relation for each snapshot separately
        stellar_mass_binned = []
        
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                # Get valid galaxies (positive stellar and halo masses)
                w = np.where((StellarMassFull[snap_idx] > 0.0) & 
                           (HaloMassFull[snap_idx] > 0.0))[0]
                
                if len(w) > 0:
                    stellar_masses = StellarMassFull[snap_idx][w]  # Keep in linear units
                    halo_masses = np.log10(HaloMassFull[snap_idx][w])
                    
                    # Bin by halo mass and calculate mean stellar mass in each bin
                    stellar_mass_in_bins = np.zeros(len(halo_mass_centers))
                    stellar_mass_in_bins.fill(np.nan)
                    
                    for j, halo_center in enumerate(halo_mass_centers):
                        bin_mask = (halo_masses >= halo_mass_bins[j]) & (halo_masses < halo_mass_bins[j+1])
                        if np.sum(bin_mask) > 0:
                            stellar_mass_in_bins[j] = np.mean(stellar_masses[bin_mask])
                    
                    stellar_mass_binned.append(stellar_mass_in_bins)
        
        if len(stellar_mass_binned) == 0:
            continue
            
        # Convert to numpy array for easier manipulation
        stellar_mass_binned = np.array(stellar_mass_binned)
        
        # Calculate mean and 1-sigma error across snapshots
        stellar_mass_mean = np.nanmean(stellar_mass_binned, axis=0)
        stellar_mass_err = np.nanstd(stellar_mass_binned, axis=0) / np.sqrt(len(stellar_mass_binned))
        
        # Only plot where we have valid data
        valid = ~np.isnan(stellar_mass_mean) & ~np.isnan(stellar_mass_err)
        if not np.any(valid):
            continue
            
        # Plot the main curve
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(halo_mass_centers[valid], stellar_mass_mean[valid], 
                color=colors[i], linewidth=2, linestyle=line_styles[i], label=label)
        
        # Add shaded error region
        ax.fill_between(halo_mass_centers[valid], 
                       stellar_mass_mean[valid] - stellar_mass_err[valid], 
                       stellar_mass_mean[valid] + stellar_mass_err[valid],
                       color=colors[i], alpha=0.3)

    # Set log scale and limits
    ax.set_yscale('log')
    ax.set_xscale('linear')  # x-axis is already log10 halo mass values
    ax.set_xlim(10.0, 15.0)
    ax.set_ylim(1e7, 1e12)

    # Labels and formatting
    ax.set_xlabel(r'$M_{\rm vir}$ [$M_\odot$]', fontsize=14)
    ax.set_ylabel(r'$M_*$ [$M_\odot$]', fontsize=14)

    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    # Create legend
    leg = ax.legend(loc='lower right', fontsize=10, frameon=False, ncol=2)
    for text in leg.get_texts():
        text.set_fontsize(10)

    # Grid
    # ax.grid(True, alpha=0.3)

    plt.tight_layout()
    
    # Save the plot
    outputFile = OutputDir + 'E.StellarMassHaloMassRelation' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting stellar mass function evolution with redshift bins')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)
    min_halo_mass = 1e10  # Minimum halo mass to consider for SMF

    # Define redshift bins to match the figure
    z_bins = [
        (0.2, 0.5), (0.5, 0.8), (0.8, 1.1), (1.1, 1.5), (1.5, 2.0),
        (2.0, 2.5), (2.5, 3.0), (3.0, 3.5), (3.5, 4.5), (4.5, 5.5),
        (5.5, 6.5), (6.5, 7.5), (7.5, 8.5), (8.5, 10.0), (10.0, 12.0)
    ]

    # Define colormap - from dark to light/orange
    colors = plt.cm.plasma(np.linspace(0.1, 0.9, len(z_bins)))

    # Function to find snapshots within a redshift range
    def find_snapshots_in_z_range(z_min, z_max, redshifts):
        """Find snapshot indices within redshift range"""
        snapshot_indices = []
        for i, z in enumerate(redshifts):
            if z_min <= z <= z_max:
                snapshot_indices.append(i)
        return snapshot_indices

    # Calculate stellar mass function for each redshift bin
    for i, (z_min, z_max) in enumerate(z_bins):
        # Find snapshots in this redshift range
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        
        if len(snap_indices) == 0:
            continue

        # Use different mass bins for the last 4 redshift bins (highest z)
        if i >= len(z_bins) - 4:  # Last 4 redshift bins
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.2, 12.5, 0.1)
        
        mass_centers = mass_bins[:-1] + 0.05
            
        # Calculate stellar mass function for each snapshot separately
        phi_snapshots = []
        bin_width = mass_bins[1] - mass_bins[0]
        
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                # Apply minimum halo mass filter if desired
                if 'min_halo_mass' in locals():
                    w = np.where((StellarMassFull[snap_idx] > 0.0) & 
                               (HaloMassFull[snap_idx] >= min_halo_mass))[0]
                else:
                    w = np.where(StellarMassFull[snap_idx] > 0.0)[0]
                
                if len(w) > 0:
                    masses = np.log10(StellarMassFull[snap_idx][w])
                    counts, bin_edges = np.histogram(masses, bins=mass_bins)
                    phi_snap = counts / (volume * bin_width)
                    phi_snapshots.append(phi_snap)
        
        if len(phi_snapshots) == 0:
            continue
            
        # Convert to numpy array for easier manipulation
        phi_snapshots = np.array(phi_snapshots)
        
        # Calculate mean and 1-sigma error (standard error of the mean)
        phi = np.mean(phi_snapshots, axis=0)
        phi_err = np.std(phi_snapshots, axis=0) / np.sqrt(len(phi_snapshots))
        
        # Only plot where we have data
        valid = (phi > 0) & (phi_err > 0)
        if not np.any(valid):
            continue
            
        # Plot the main curve
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        ax.plot(mass_centers[valid], phi[valid], 
                color=colors[i], linewidth=2, label=label)
        
        # Add shaded error region
        ax.fill_between(mass_centers[valid], 
                       phi[valid] - phi_err[valid], 
                       phi[valid] + phi_err[valid],
                       color=colors[i], alpha=0.3)

    # Set log scale and limits to match the figure and other SMF plots
    ax.set_yscale('log')
    ax.set_xlim(7.2, 12.0)
    ax.set_ylim(1e-6, 1e-1)

    # Labels and formatting
    ax.set_xlabel(r'$\log_{10} M_* [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\phi$ [Mpc$^{-3}$ dex$^{-1}$]', fontsize=14)

    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    # Create legend
    leg = ax.legend(loc='lower left', fontsize=10, frameon=False)
    for text in leg.get_texts():
        text.set_fontsize(10)

    # Grid
    # ax.grid(True, alpha=0.3)

    plt.tight_layout()
    
    # Save the plot
    outputFile = OutputDir + 'F.StellarMassFunctionEvolution' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()
    
   # --------------------------------------------------------

    # Black Hole Mass Function at specific redshifts
    print('Plotting black hole mass function at specific redshifts')

    plt.figure(figsize=(10, 8))
    ax = plt.subplot(111)

    # Define redshifts to plot (these correspond to the observation files)
    bhmf_redshifts = [0.1, 1.0, 2.0, 4.0, 6.0, 8.0]
    
    # Map redshifts to closest snapshots
    bhmf_snapshots = []
    actual_redshifts = []
    for target_z in bhmf_redshifts:
        snap_idx = np.argmin(np.abs(np.array(redshifts) - target_z))
        bhmf_snapshots.append(snap_idx)
        actual_redshifts.append(redshifts[snap_idx])
    
    # Define colormap - plasma from dark to light
    colors_bhmf = plt.cm.plasma(np.linspace(0.1, 0.9, len(bhmf_redshifts)))
    
    # Define mass bins for BHMF
    bhmf_mass_bins = np.arange(6.0, 11.5, 0.1)
    bhmf_mass_centers = bhmf_mass_bins[:-1] + 0.05
    bin_width = bhmf_mass_bins[1] - bhmf_mass_bins[0]
    
    # Plot SAGE model predictions for each redshift
    for i, (snap_idx, target_z, actual_z) in enumerate(zip(bhmf_snapshots, bhmf_redshifts, actual_redshifts)):
        # Filter for galaxies with black holes
        w = np.where(BlackHoleMassFull[snap_idx] > 0.0)[0]
        
        if len(w) > 0:
            bh_masses = np.log10(BlackHoleMassFull[snap_idx][w])
            counts, bin_edges = np.histogram(bh_masses, bins=bhmf_mass_bins)
            phi = counts / (volume * bin_width)
            
            # Only plot where we have data
            valid = phi > 0
            if np.any(valid):
                label = f'z = {actual_z:.1f} (SAGE)'
                ax.plot(bhmf_mass_centers[valid], phi[valid], 
                       color=colors_bhmf[i], linewidth=2, linestyle='-', label=label)
    
    # Load and plot observational data
    data_dir = './data/'
    obs_files = {
        0.1: 'fig4_bhmf_z0.1.txt',
        1.0: 'fig4_bhmf_z1.0.txt',
        2.0: 'fig4_bhmf_z2.0.txt',
        4.0: 'fig4_bhmf_z4.0.txt',
        6.0: 'fig4_bhmf_z6.0.txt',
        8.0: 'fig4_bhmf_z8.0.txt'
    }
    
    for i, target_z in enumerate(bhmf_redshifts):
        if target_z in obs_files:
            obs_file = data_dir + obs_files[target_z]
            try:
                # Load observation data (skip header lines starting with #)
                obs_data = np.loadtxt(obs_file)
                obs_mass = obs_data[:, 0]     # log10(Mbh [Msun])
                obs_phi = obs_data[:, 1]      # BHMF_best [Mpc^-3 dex^-1]
                obs_phi_16th = obs_data[:, 2] # BHMF_16th [Mpc^-3 dex^-1]
                obs_phi_84th = obs_data[:, 3] # BHMF_84th [Mpc^-3 dex^-1]
                
                # Plot observations with dashed line
                label = f'z = {target_z:.1f} (Obs)'
                ax.plot(obs_mass, obs_phi, color=colors_bhmf[i], 
                       linewidth=2, linestyle='--', label=label, alpha=0.8)
                
                # Add shaded error region for observations
                ax.fill_between(obs_mass, obs_phi_16th, obs_phi_84th,
                               color=colors_bhmf[i], alpha=0.2)
            except Exception as e:
                print(f'Warning: Could not load {obs_file}: {e}')
    
    # Set log scale and limits
    ax.set_yscale('log')
    ax.set_xlim(6.0, 11.0)
    ax.set_ylim(1e-5, 1e-1)
    
    # Labels and formatting
    ax.set_xlabel(r'$\log_{10} M_{\rm BH} [M_\odot]$', fontsize=14)
    ax.set_ylabel(r'$\phi$ [Mpc$^{-3}$ dex$^{-1}$]', fontsize=14)
    
    # Set minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.2))
    
    # Create legend
    leg = ax.legend(loc='upper right', fontsize=9, frameon=False, ncol=2)
    for text in leg.get_texts():
        text.set_fontsize(9)
    
    plt.tight_layout()
    
    # Save the plot
    outputFile = OutputDir + 'F2.BlackHoleMassFunction' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()
    
   # --------------------------------------------------------

    # Quenched fraction as a function of StellarMass across the same redshift bins (centrals/satellites separated)
    print('Plotting quenched fraction as a function of StellarMass evolution with redshift bins (centrals/satellites)')

    plt.figure()
    ax = plt.subplot(111)

    for i, (z_min, z_max) in enumerate(z_bins):
        snap_indices = find_snapshots_in_z_range(z_min, z_max, redshifts)
        if len(snap_indices) == 0:
            continue

        if i >= len(z_bins) - 4:
            mass_bins = np.arange(8.0, 12.5, 0.1)
        else:
            mass_bins = np.arange(7.2, 12.5, 0.1)
        mass_centers = mass_bins[:-1] + 0.05

        all_snapshots = []
        central_snapshots = []
        satellite_snapshots = []
        for snap_idx in snap_indices:
            if snap_idx < len(StellarMassFull):
                sfr_total = SfrDiskFull[snap_idx] + SfrBulgeFull[snap_idx]
                stellar_mass = StellarMassFull[snap_idx]
                galtype = TypeFull[snap_idx]
                
                # Include ALL galaxies with positive stellar mass (like the local script)
                w = np.where(stellar_mass > 0.0)[0]
                if len(w) > 0:
                    masses = np.log10(stellar_mass[w])
                    # Calculate sSFR in linear units, then compare to 10^sSFRcut (like local script)
                    sSFR_linear = sfr_total[w] / stellar_mass[w]
                    gtype = galtype[w]
                    
                    # Identify quiescent galaxies using the same method as local script
                    quenched = sSFR_linear < 10.0**sSFRcut
                    
                    # All galaxies
                    all_frac = np.full(len(mass_centers), np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1])
                        if np.sum(mask) > 0:
                            all_frac[j] = np.sum(quenched[mask]) / np.sum(mask)
                    all_snapshots.append(all_frac)
                    
                    # Centrals
                    cen_mask = gtype == 0
                    cen_frac = np.full(len(mass_centers), np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1]) & cen_mask
                        if np.sum(mask) > 0:
                            cen_quenched = quenched[mask]
                            cen_frac[j] = np.sum(cen_quenched) / np.sum(mask)
                    central_snapshots.append(cen_frac)
                    
                    # Satellites
                    sat_mask = gtype == 1
                    sat_frac = np.full(len(mass_centers), np.nan)
                    for j in range(len(mass_centers)):
                        mask = (masses >= mass_bins[j]) & (masses < mass_bins[j+1]) & sat_mask
                        if np.sum(mask) > 0:
                            sat_quenched = quenched[mask]
                            sat_frac[j] = np.sum(sat_quenched) / np.sum(mask)
                    satellite_snapshots.append(sat_frac)
                    
        if len(all_snapshots) == 0:
            continue
            
        all_snapshots_arr = np.array(all_snapshots)
        central_snapshots_arr = np.array(central_snapshots)
        satellite_snapshots_arr = np.array(satellite_snapshots)
        all_frac_mean = np.nanmean(all_snapshots_arr, axis=0)
        cen_frac_mean = np.nanmean(central_snapshots_arr, axis=0)
        sat_frac_mean = np.nanmean(satellite_snapshots_arr, axis=0)
        
        # 1-sigma for centrals
        cen_frac_std = np.nanstd(central_snapshots_arr, axis=0)
        valid_all = ~np.isnan(all_frac_mean)
        valid_cen = ~np.isnan(cen_frac_mean)
        valid_sat = ~np.isnan(sat_frac_mean)
        
        label = f'{z_min:.1f} < z < {z_max:.1f}'
        # ax.plot(mass_centers[valid_all], all_frac_mean[valid_all], color=colors[i], linewidth=2, label=label+' (All)')
        
        # Plot centrals with 1-sigma shading
        ax.plot(mass_centers[valid_cen], cen_frac_mean[valid_cen], color=colors[i], linestyle='-', linewidth=2, label=label)
        ax.fill_between(
            mass_centers[valid_cen],
            (cen_frac_mean - cen_frac_std)[valid_cen],
            (cen_frac_mean + cen_frac_std)[valid_cen],
            color=colors[i], alpha=0.18, linewidth=0)
        
        # Plot satellites
        ax.plot(mass_centers[valid_sat], sat_frac_mean[valid_sat], color=colors[i], linestyle=':', linewidth=2, alpha=0.25)

    ax.set_xlim(8.0, 12.2)
    ax.set_ylim(0, 0.8)
    ax.set_xlabel(r'$\log_{10} M_* [M_\odot]$', fontsize=14)
    ax.set_ylabel('Quenched Fraction', fontsize=14)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
    leg = ax.legend(loc='upper left', fontsize=8, frameon=False, ncol=3)
    for text in leg.get_texts():
        text.set_fontsize(8)
    plt.tight_layout()
    outputFile = OutputDir + 'G.QuenchedFraction.StellarMassFunctionEvolution' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting gas reservoir flow rates evolution')

    from astropy.cosmology import FlatLambdaCDM
    import astropy.units as u

    # Define cosmology to match simulation parameters
    cosmo = FlatLambdaCDM(H0=73, Om0=0.25)  # Adjust Om0 if you know the exact value

    # Calculate cosmic time for each snapshot from redshift
    cosmic_time_Gyr = np.array([cosmo.age(z).value for z in redshifts])

    print(f'Cosmic time at z=0: {cosmic_time_Gyr[-1]:.2f} Gyr')
    print(f'Cosmic time at z={redshifts[0]:.2f}: {cosmic_time_Gyr[0]:.2f} Gyr')

    plt.figure(figsize=(12, 8))

    # Initialize arrays to store mean flow rates
    mean_dHotGas = []
    mean_dColdGas = []
    mean_dCGMgas = []
    mean_dStellarMass = []
    time_centers = []

    # Calculate flow rates between consecutive snapshots
    for snap in range(FirstSnap+1, LastSnap+1):
        prev_snap = snap - 1
        
        # Calculate time difference from cosmology (in Gyr)
        dt_Gyr = cosmic_time_Gyr[snap] - cosmic_time_Gyr[prev_snap]
        
        # Convert to yr for rate calculation (M_sun/yr)
        dt_yr = dt_Gyr * 1e9
        
        if dt_yr <= 0:
            continue
        
        # Calculate changes for all centrals (Type == 0)
        w_prev = np.where(TypeFull[prev_snap] == 0)[0]
        w_curr = np.where(TypeFull[snap] == 0)[0]
        
        if len(w_prev) > 0 and len(w_curr) > 0:
            # Calculate mean change rates
            dHotGas = np.mean(hotgasFull[snap][w_curr]) - np.mean(hotgasFull[prev_snap][w_prev])
            dColdGas = np.mean(coldgasFull[snap][w_curr]) - np.mean(coldgasFull[prev_snap][w_prev])
            dCGMgas = np.mean(cgmFull[snap][w_curr]) - np.mean(cgmFull[prev_snap][w_prev])
            dStellarMass = np.mean(StellarMassFull[snap][w_curr]) - np.mean(StellarMassFull[prev_snap][w_prev])
            
            mean_dHotGas.append(dHotGas / dt_yr)
            mean_dColdGas.append(dColdGas / dt_yr)
            mean_dCGMgas.append(dCGMgas / dt_yr)
            mean_dStellarMass.append(dStellarMass / dt_yr)
            time_centers.append((cosmic_time_Gyr[snap] + cosmic_time_Gyr[prev_snap]) / 2.0)

    # Convert to numpy arrays
    mean_dHotGas = np.array(mean_dHotGas)
    mean_dColdGas = np.array(mean_dColdGas)
    mean_dCGMgas = np.array(mean_dCGMgas)
    mean_dStellarMass = np.array(mean_dStellarMass)
    time_centers = np.array(time_centers)

    # Plot the flow rates
    plt.plot(time_centers, mean_dHotGas, 'r-', linewidth=2, label='HotGas rate')
    plt.plot(time_centers, mean_dColdGas, 'b-', linewidth=2, label='ColdGas rate')
    plt.plot(time_centers, mean_dCGMgas, 'g-', linewidth=2, label='CGM rate')
    plt.plot(time_centers, mean_dStellarMass, 'k-', linewidth=2, label='StellarMass rate')
    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)

    plt.xlabel(r'Cosmic Time [Gyr]', fontsize=14)
    plt.ylabel(r'$\mathrm{d}M/\mathrm{d}t$ [$M_{\odot}$ yr$^{-1}$]', fontsize=14)
    plt.legend(loc='best', frameon=False)

    outputFile = OutputDir + 'H.GasFlowRates_evolution_time' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting gas reservoir flow rates as a function of virial mass (multiple redshifts)')

    plt.figure(figsize=(10, 8))

    # Define snapshot pairs at different redshifts
    redshift_snap_pairs = {
        'z~0.0': [(61, 62), (62, 63)],
        'z~1.0': [(38, 39), (39, 40)],
        'z~2.0': [(28, 29), (29, 30)]
    }

    # Use plasma colormap for redshifts
    n_redshifts = len(redshift_snap_pairs)
    plasma_cmap = plt.cm.plasma
    redshift_colors = {z_label: plasma_cmap(i / (n_redshifts - 1)) 
                    for i, z_label in enumerate(redshift_snap_pairs.keys())}

    # Linestyles for different gas reservoirs
    gas_linestyles = {
        'HotGas': '-',
        'CGM': '--',
        'ColdGas': ':',
        'StellarMass': '-.' 
    }

    # Define virial mass bins - FIXED mass_centers calculation
    mass_bins = np.arange(10.0, 14.0, 0.5)
    mass_centers = mass_bins[:-1] + 0.25  # Correct centering for 0.5 dex bins

    # Plot each gas component at different redshifts
    for gas_type, gas_array in [('HotGas', hotgasFull), 
                                ('CGM', cgmFull), 
                                ('ColdGas', coldgasFull),
                                ('StellarMass', StellarMassFull)]:
        
        first_redshift = True  # Flag to only label once per gas type
        
        for z_label, snap_pairs in redshift_snap_pairs.items():
            # Initialize arrays for this redshift
            dGas_binned = np.zeros(len(mass_centers))
            counts_binned = np.zeros(len(mass_centers))
            
            for prev_snap, snap in snap_pairs:
                # Get time difference with proper error handling
                try:
                    if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                        dt = dT[snap][0]
                    elif isinstance(dT[snap], (int, float)):
                        dt = dT[snap]
                    else:
                        continue
                except:
                    continue
                
                if dt <= 0:
                    continue
                
                # Only look at centrals in CURRENT snapshot
                w_curr = np.where((TypeFull[snap] == 0) & (HaloMassFull[snap] > 0))[0]
                w_prev = np.where((TypeFull[prev_snap] == 0) & (HaloMassFull[prev_snap] > 0))[0]
                
                if len(w_curr) > 0 and len(w_prev) > 0:
                    masses = np.log10(HaloMassFull[snap][w_curr])
                    
                    # For each mass bin, calculate mean change in gas reservoir
                    for i, mass_center in enumerate(mass_centers):
                        mask_curr = (masses >= mass_bins[i]) & (masses < mass_bins[i+1])
                        
                        if np.sum(mask_curr) > 0:
                            galaxies_in_bin_curr = w_curr[mask_curr]
                            mean_gas_curr = np.mean(gas_array[snap][galaxies_in_bin_curr])
                            
                            # Get galaxies in similar mass bin at previous snapshot
                            masses_prev = np.log10(HaloMassFull[prev_snap][w_prev])
                            mask_prev = (masses_prev >= mass_bins[i]) & (masses_prev < mass_bins[i+1])
                            
                            if np.sum(mask_prev) > 0:
                                galaxies_in_bin_prev = w_prev[mask_prev]
                                mean_gas_prev = np.mean(gas_array[prev_snap][galaxies_in_bin_prev])
                                
                                # Calculate rate of change (dM/dt)
                                dGas_rate = (mean_gas_curr - mean_gas_prev) / dt
                                dGas_binned[i] += dGas_rate
                                counts_binned[i] += 1
            
            # Normalize by number of snapshot pairs
            valid = counts_binned > 0
            if np.any(valid):
                dGas_binned[valid] /= counts_binned[valid]
                
                # Plot with appropriate style - only label the first redshift for each gas type
                label = gas_type if first_redshift else None
                plt.plot(mass_centers[valid], dGas_binned[valid], 
                        linestyle=gas_linestyles[gas_type], linewidth=2.5, 
                        color=redshift_colors[z_label],
                        label=label)
                
                first_redshift = False  # Only label once

    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)

    plt.xlabel(r'$\log_{10} M_{\rm vir} [M_\odot]$', fontsize=14)
    plt.ylabel(r'$\mathrm{d}M/\mathrm{d}t\ (M_{\odot}\ \mathrm{yr}^{-1})$', fontsize=14)
    plt.title('Mean gas reservoir flow rates vs virial mass', fontsize=12)
    plt.legend(loc='best', frameon=False, fontsize=11)
    plt.xlim(10.0, 14.0)

    outputFile = OutputDir + 'I.GasFlowRates_vs_VirialMass_MultiZ' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Calculating cosmic time and lookback time from redshifts using astropy')

    from astropy.cosmology import FlatLambdaCDM
    import astropy.units as u

    # Define cosmology to match simulation parameters
    cosmo = FlatLambdaCDM(H0=73, Om0=0.25)

    # Calculate cosmic time for each snapshot from redshift
    cosmic_time_Gyr = np.array([cosmo.age(z).value for z in redshifts])

    # Calculate lookback time (present day = 0)
    lookback_time_Gyr = cosmic_time_Gyr[LastSnap] - cosmic_time_Gyr

    print(f'Lookback time at z=0 (snap {LastSnap}): {lookback_time_Gyr[LastSnap]:.2f} Gyr')
    print(f'Lookback time at z={redshifts[FirstSnap]:.2f} (snap {FirstSnap}): {lookback_time_Gyr[FirstSnap]:.2f} Gyr')

    # --------------------------------------------------------

    # Load second model for comparison
    FileName2 = '../../SAGE_BROKEN/sage-model/output/millennium/model_0.hdf5'

    print(f'Reading second model from {FileName2}')

    StellarMassFull2 = [0]*(LastSnap-FirstSnap+1)
    cgmFull2 = [0]*(LastSnap-FirstSnap+1)
    hotgasFull2 = [0]*(LastSnap-FirstSnap+1)
    coldgasFull2 = [0]*(LastSnap-FirstSnap+1)
    HaloMassFull2 = [0]*(LastSnap-FirstSnap+1)

    def read_hdf2(filename=None, snap_num=None, param=None):
        property = h5.File(FileName2, 'r')
        return np.array(property[snap_num][param])

    for snap in range(FirstSnap, LastSnap+1):
        Snapshot = 'Snap_'+str(snap)
        StellarMassFull2[snap] = read_hdf2(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        cgmFull2[snap] = read_hdf2(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        hotgasFull2[snap] = read_hdf2(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        coldgasFull2[snap] = read_hdf2(snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
        HaloMassFull2[snap] = read_hdf2(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h


    # --------------------------------------------------------

    print('Plotting HotGas dM/dt evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    hotgas_rate_model1 = []
    hotgas_rate_model2 = []
    lookback_centers = []

    for snap in range(FirstSnap+1, LastSnap+1):
        try:
            if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                dt = dT[snap][0]  # in Myr
            elif isinstance(dT[snap], (int, float)):
                dt = dT[snap]  # in Myr
            else:
                continue
        except:
            continue
        
        if dt <= 0:
            continue
        
        # Convert dt from Myr to yr
        dt_yr = dt * 1e6
        
        if len(hotgasFull[snap]) > 0 and len(hotgasFull[snap-1]) > 0:
            mean_curr_1 = np.mean(hotgasFull[snap])
            mean_prev_1 = np.mean(hotgasFull[snap-1])
            rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
        else:
            rate_1 = np.nan
        
        if len(hotgasFull2[snap]) > 0 and len(hotgasFull2[snap-1]) > 0:
            mean_curr_2 = np.mean(hotgasFull2[snap])
            mean_prev_2 = np.mean(hotgasFull2[snap-1])
            rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
        else:
            rate_2 = np.nan
        
        hotgas_rate_model1.append(rate_1)
        hotgas_rate_model2.append(rate_2)
        lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)

    hotgas_rate_model1 = np.array(hotgas_rate_model1)
    hotgas_rate_model2 = np.array(hotgas_rate_model2)
    lookback_centers = np.array(lookback_centers)

    valid1 = ~np.isnan(hotgas_rate_model1)
    valid2 = ~np.isnan(hotgas_rate_model2)

    plt.plot(lookback_centers[valid1], hotgas_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_centers[valid2], hotgas_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'$\mathrm{d}M_{\rm HotGas}/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.tight_layout()

    outputFile = OutputDir + 'J.HotGas_dMdt_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting ColdGas dM/dt evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    coldgas_rate_model1 = []
    coldgas_rate_model2 = []
    lookback_centers = []

    for snap in range(FirstSnap+1, LastSnap+1):
        try:
            if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                dt = dT[snap][0]
            elif isinstance(dT[snap], (int, float)):
                dt = dT[snap]
            else:
                continue
        except:
            continue
        
        if dt <= 0:
            continue
        
        dt_yr = dt * 1e6
        
        if len(coldgasFull[snap]) > 0 and len(coldgasFull[snap-1]) > 0:
            mean_curr_1 = np.mean(coldgasFull[snap])
            mean_prev_1 = np.mean(coldgasFull[snap-1])
            rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
        else:
            rate_1 = np.nan
        
        if len(coldgasFull2[snap]) > 0 and len(coldgasFull2[snap-1]) > 0:
            mean_curr_2 = np.mean(coldgasFull2[snap])
            mean_prev_2 = np.mean(coldgasFull2[snap-1])
            rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
        else:
            rate_2 = np.nan
        
        coldgas_rate_model1.append(rate_1)
        coldgas_rate_model2.append(rate_2)
        lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)

    coldgas_rate_model1 = np.array(coldgas_rate_model1)
    coldgas_rate_model2 = np.array(coldgas_rate_model2)
    lookback_centers = np.array(lookback_centers)

    valid1 = ~np.isnan(coldgas_rate_model1)
    valid2 = ~np.isnan(coldgas_rate_model2)

    plt.plot(lookback_centers[valid1], coldgas_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_centers[valid2], coldgas_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'$\mathrm{d}M_{\rm ColdGas}/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.tight_layout()

    outputFile = OutputDir + 'K.ColdGas_dMdt_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting CGM dM/dt evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    cgm_rate_model1 = []
    cgm_rate_model2 = []
    lookback_centers = []

    for snap in range(FirstSnap+1, LastSnap+1):
        try:
            if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                dt = dT[snap][0]
            elif isinstance(dT[snap], (int, float)):
                dt = dT[snap]
            else:
                continue
        except:
            continue
        
        if dt <= 0:
            continue
        
        dt_yr = dt * 1e6
        
        if len(cgmFull[snap]) > 0 and len(cgmFull[snap-1]) > 0:
            mean_curr_1 = np.mean(cgmFull[snap])
            mean_prev_1 = np.mean(cgmFull[snap-1])
            rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
        else:
            rate_1 = np.nan
        
        if len(cgmFull2[snap]) > 0 and len(cgmFull2[snap-1]) > 0:
            mean_curr_2 = np.mean(cgmFull2[snap])
            mean_prev_2 = np.mean(cgmFull2[snap-1])
            rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
        else:
            rate_2 = np.nan
        
        cgm_rate_model1.append(rate_1)
        cgm_rate_model2.append(rate_2)
        lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)

    cgm_rate_model1 = np.array(cgm_rate_model1)
    cgm_rate_model2 = np.array(cgm_rate_model2)
    lookback_centers = np.array(lookback_centers)

    valid1 = ~np.isnan(cgm_rate_model1)
    valid2 = ~np.isnan(cgm_rate_model2)

    plt.plot(lookback_centers[valid1], cgm_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_centers[valid2], cgm_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'$\mathrm{d}M_{\rm CGM}/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.tight_layout()

    outputFile = OutputDir + 'L.CGM_dMdt_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting StellarMass dM/dt evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    stellar_rate_model1 = []
    stellar_rate_model2 = []
    lookback_centers = []

    for snap in range(FirstSnap+1, LastSnap+1):
        try:
            if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                dt = dT[snap][0]
            elif isinstance(dT[snap], (int, float)):
                dt = dT[snap]
            else:
                continue
        except:
            continue
        
        if dt <= 0:
            continue
        
        dt_yr = dt * 1e6
        
        if len(StellarMassFull[snap]) > 0 and len(StellarMassFull[snap-1]) > 0:
            mean_curr_1 = np.mean(StellarMassFull[snap])
            mean_prev_1 = np.mean(StellarMassFull[snap-1])
            rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
        else:
            rate_1 = np.nan
        
        if len(StellarMassFull2[snap]) > 0 and len(StellarMassFull2[snap-1]) > 0:
            mean_curr_2 = np.mean(StellarMassFull2[snap])
            mean_prev_2 = np.mean(StellarMassFull2[snap-1])
            rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
        else:
            rate_2 = np.nan
        
        stellar_rate_model1.append(rate_1)
        stellar_rate_model2.append(rate_2)
        lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)

    stellar_rate_model1 = np.array(stellar_rate_model1)
    stellar_rate_model2 = np.array(stellar_rate_model2)
    lookback_centers = np.array(lookback_centers)

    valid1 = ~np.isnan(stellar_rate_model1)
    valid2 = ~np.isnan(stellar_rate_model2)

    plt.plot(lookback_centers[valid1], stellar_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_centers[valid2], stellar_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'$\mathrm{d}M_*/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.tight_layout()

    outputFile = OutputDir + 'M.StellarMass_dMdt_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    print('\nAll plots completed!')

    # --------------------------------------------------------

    print('Plotting regime transition function behavior')

    plt.figure(figsize=(10, 8))

    # Define the regime transition function
    def regime_transition(Mvir_physical, transition_width=0.2):
        """
        Calculate the regime transition value for a given virial mass.
        Returns value between 0 (CGM regime) and 1 (hot-ICM regime).
        """
        Mshock = 6.0e11  # Msun
        mass_ratio = Mvir_physical / Mshock
        regime_criterion = mass_ratio ** (4.0/3.0)
        regime_smooth = 0.5 * (1.0 + np.tanh((regime_criterion - 1.0) / transition_width))
        return regime_smooth

    # Create a range of virial masses
    Mvir_range = np.logspace(10, 14, 200)  # 10^10 to 10^14 Msun

    # Plot for different transition widths to show the effect
    transition_widths = [0.1, 0.2, 0.5]
    colors_tw = ['darkblue', 'blue', 'lightblue']
    
    for tw, color in zip(transition_widths, colors_tw):
        regime_values = regime_transition(Mvir_range, transition_width=tw)
        plt.plot(np.log10(Mvir_range), regime_values, '-', color=color, 
                linewidth=2, label=f'Transition width = {tw}')

    # Mark the Mshock threshold
    plt.axvline(x=np.log10(6.0e11), color='red', linestyle='--', 
                linewidth=1.5, alpha=0.7, label=r'$M_{\rm shock} = 6 \times 10^{11} M_\odot$')
    
    # Add horizontal lines at regime boundaries
    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)
    plt.axhline(y=1, color='gray', linestyle=':', alpha=0.5)
    plt.axhline(y=0.5, color='gray', linestyle=':', alpha=0.5, label='Transition midpoint')

    plt.xlabel(r'$\log_{10} M_{\rm vir}$ [$M_\odot$]', fontsize=14)
    plt.ylabel('Regime Value', fontsize=14)
    plt.title('CGM/Hot-ICM Regime Transition Function', fontsize=14)
    plt.xlim(10.5, 13.5)
    plt.ylim(-0.05, 1.05)
    plt.legend(loc='best', frameon=False, fontsize=11)
    plt.grid(True, alpha=0.3)
    
    # Add text annotations
    plt.text(11.0, 0.1, 'CGM Regime', fontsize=12, color='darkgreen', 
            ha='center', bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.3))
    plt.text(13.0, 0.9, 'Hot-ICM Regime', fontsize=12, color='darkred',
            ha='center', bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.3))

    plt.tight_layout()
    outputFile = OutputDir + 'N.RegimeTransitionFunction' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting gas reservoir flow rates evolution (Model Comparison)')

    plt.figure(figsize=(12, 8))

    mean_dHotGas1 = []
    mean_dColdGas1 = []
    mean_dCGMgas1 = []
    mean_dStellarMass1 = []

    mean_dHotGas2 = []
    mean_dColdGas2 = []
    mean_dCGMgas2 = []
    mean_dStellarMass2 = []

    lookback_centers = []

    for snap in range(FirstSnap+1, LastSnap+1):
        try:
            if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                dt = dT[snap][0]
            elif isinstance(dT[snap], (int, float)):
                dt = dT[snap]
            else:
                continue
        except:
            continue
        
        if dt <= 0:
            continue
        
        dt_yr = dt * 1e6
        
        has_model1 = len(hotgasFull[snap]) > 0 and len(hotgasFull[snap-1]) > 0
        has_model2 = len(hotgasFull2[snap]) > 0 and len(hotgasFull2[snap-1]) > 0
        
        if not (has_model1 and has_model2):
            continue
        
        # Model 1 calculations
        dHotGas1 = np.mean(hotgasFull[snap]) - np.mean(hotgasFull[snap-1])
        dColdGas1 = np.mean(coldgasFull[snap]) - np.mean(coldgasFull[snap-1])
        dCGMgas1 = np.mean(cgmFull[snap]) - np.mean(cgmFull[snap-1])
        dStellarMass1 = np.mean(StellarMassFull[snap]) - np.mean(StellarMassFull[snap-1])
        
        mean_dHotGas1.append(dHotGas1 / dt_yr)
        mean_dColdGas1.append(dColdGas1 / dt_yr)
        mean_dCGMgas1.append(dCGMgas1 / dt_yr)
        mean_dStellarMass1.append(dStellarMass1 / dt_yr)
        
        # Model 2 calculations
        dHotGas2 = np.mean(hotgasFull2[snap]) - np.mean(hotgasFull2[snap-1])
        dColdGas2 = np.mean(coldgasFull2[snap]) - np.mean(coldgasFull2[snap-1])
        dCGMgas2 = np.mean(cgmFull2[snap]) - np.mean(cgmFull2[snap-1])
        dStellarMass2 = np.mean(StellarMassFull2[snap]) - np.mean(StellarMassFull2[snap-1])
        
        mean_dHotGas2.append(dHotGas2 / dt_yr)
        mean_dColdGas2.append(dColdGas2 / dt_yr)
        mean_dCGMgas2.append(dCGMgas2 / dt_yr)
        mean_dStellarMass2.append(dStellarMass2 / dt_yr)
        
        lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)

    mean_dHotGas1 = np.array(mean_dHotGas1)
    mean_dColdGas1 = np.array(mean_dColdGas1)
    mean_dCGMgas1 = np.array(mean_dCGMgas1)
    mean_dStellarMass1 = np.array(mean_dStellarMass1)

    mean_dHotGas2 = np.array(mean_dHotGas2)
    mean_dColdGas2 = np.array(mean_dColdGas2)
    mean_dCGMgas2 = np.array(mean_dCGMgas2)
    mean_dStellarMass2 = np.array(mean_dStellarMass2)

    lookback_centers = np.array(lookback_centers)

    # Plot Model 1 (SAGE CGM) - solid lines
    plt.plot(lookback_centers, mean_dHotGas1, 'r-', linewidth=1, label='HotGas (SAGE CGM)')
    plt.plot(lookback_centers, mean_dColdGas1, 'b-', linewidth=1, label='ColdGas (SAGE CGM)')
    plt.plot(lookback_centers, mean_dCGMgas1, 'g-', linewidth=1, label='CGM (SAGE CGM)')
    plt.plot(lookback_centers, mean_dStellarMass1, 'k-', linewidth=1, label='StellarMass (SAGE CGM)')

    # Plot Model 2 (evilSAGE) - dashed lines
    plt.plot(lookback_centers, mean_dHotGas2, 'r--', linewidth=1, label='HotGas (evilSAGE)')
    plt.plot(lookback_centers, mean_dColdGas2, 'b--', linewidth=1, label='ColdGas (evilSAGE)')
    plt.plot(lookback_centers, mean_dCGMgas2, 'g--', linewidth=1, label='CGM (evilSAGE)')
    plt.plot(lookback_centers, mean_dStellarMass2, 'k--', linewidth=1, label='StellarMass (evilSAGE)')

    plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'$\mathrm{d}M/\mathrm{d}t$ [$M_{\odot}$ yr$^{-1}$]', fontsize=14)
    plt.legend(loc='best', frameon=False, fontsize=10)

    outputFile = OutputDir + 'H.GasFlowRates_evolution_time_comparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    # Define halo mass bins (in log10 M_sun)
    mass_bins_dict = {
        'Low': (10.0, 11.5),
        'Intermediate': (11.5, 12.5),
        'High': (12.5, 15.0)
    }

    # --------------------------------------------------------
    # Loop through each mass bin to create separate plots

    for mass_label, (mass_min, mass_max) in mass_bins_dict.items():
        
        print(f'Plotting HotGas dM/dt evolution for {mass_label} mass haloes')
        
        plt.figure(figsize=(10, 6))
        ax = plt.subplot(111)
        
        hotgas_rate_model1 = []
        hotgas_rate_model2 = []
        lookback_centers = []
        
        for snap in range(FirstSnap+1, LastSnap+1):
            try:
                if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                    dt = dT[snap][0]
                elif isinstance(dT[snap], (int, float)):
                    dt = dT[snap]
                else:
                    continue
            except:
                continue
            
            if dt <= 0:
                continue
            
            dt_yr = dt * 1e6
            
            # Filter by halo mass for Model 1
            halo_masses_1 = np.log10(HaloMassFull[snap])
            halo_masses_1_prev = np.log10(HaloMassFull[snap-1])
            mask_1_curr = (halo_masses_1 >= mass_min) & (halo_masses_1 < mass_max)
            mask_1_prev = (halo_masses_1_prev >= mass_min) & (halo_masses_1_prev < mass_max)
            
            if np.sum(mask_1_curr) > 0 and np.sum(mask_1_prev) > 0:
                mean_curr_1 = np.mean(hotgasFull[snap][mask_1_curr])
                mean_prev_1 = np.mean(hotgasFull[snap-1][mask_1_prev])
                rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
            else:
                rate_1 = np.nan
            
            # Filter by halo mass for Model 2
            halo_masses_2 = np.log10(HaloMassFull2[snap])
            halo_masses_2_prev = np.log10(HaloMassFull2[snap-1])
            mask_2_curr = (halo_masses_2 >= mass_min) & (halo_masses_2 < mass_max)
            mask_2_prev = (halo_masses_2_prev >= mass_min) & (halo_masses_2_prev < mass_max)
            
            if np.sum(mask_2_curr) > 0 and np.sum(mask_2_prev) > 0:
                mean_curr_2 = np.mean(hotgasFull2[snap][mask_2_curr])
                mean_prev_2 = np.mean(hotgasFull2[snap-1][mask_2_prev])
                rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
            else:
                rate_2 = np.nan
            
            hotgas_rate_model1.append(rate_1)
            hotgas_rate_model2.append(rate_2)
            lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)
        
        hotgas_rate_model1 = np.array(hotgas_rate_model1)
        hotgas_rate_model2 = np.array(hotgas_rate_model2)
        lookback_centers = np.array(lookback_centers)
        
        valid1 = ~np.isnan(hotgas_rate_model1)
        valid2 = ~np.isnan(hotgas_rate_model2)
        
        plt.plot(lookback_centers[valid1], hotgas_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
        plt.plot(lookback_centers[valid2], hotgas_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
        plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)
        
        plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
        plt.ylabel(r'$\mathrm{d}M_{\rm HotGas}/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
        plt.title(f'{mass_label} Mass Haloes: {mass_min:.1f} < log(M$_{{vir}}$) < {mass_max:.1f}', fontsize=12)
        plt.legend(loc='best', frameon=False)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
        plt.tight_layout()
        
        outputFile = OutputDir + f'J.HotGas_dMdt_{mass_label}Mass' + OutputFormat
        plt.savefig(outputFile, dpi=300, bbox_inches='tight')
        print('Saved file to', outputFile, '\n')
        plt.close()

    # --------------------------------------------------------

    for mass_label, (mass_min, mass_max) in mass_bins_dict.items():
        
        print(f'Plotting ColdGas dM/dt evolution for {mass_label} mass haloes')
        
        plt.figure(figsize=(10, 6))
        ax = plt.subplot(111)
        
        coldgas_rate_model1 = []
        coldgas_rate_model2 = []
        lookback_centers = []
        
        for snap in range(FirstSnap+1, LastSnap+1):
            try:
                if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                    dt = dT[snap][0]
                elif isinstance(dT[snap], (int, float)):
                    dt = dT[snap]
                else:
                    continue
            except:
                continue
            
            if dt <= 0:
                continue
            
            dt_yr = dt * 1e6
            
            halo_masses_1 = np.log10(HaloMassFull[snap])
            halo_masses_1_prev = np.log10(HaloMassFull[snap-1])
            mask_1_curr = (halo_masses_1 >= mass_min) & (halo_masses_1 < mass_max)
            mask_1_prev = (halo_masses_1_prev >= mass_min) & (halo_masses_1_prev < mass_max)
            
            if np.sum(mask_1_curr) > 0 and np.sum(mask_1_prev) > 0:
                mean_curr_1 = np.mean(coldgasFull[snap][mask_1_curr])
                mean_prev_1 = np.mean(coldgasFull[snap-1][mask_1_prev])
                rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
            else:
                rate_1 = np.nan
            
            halo_masses_2 = np.log10(HaloMassFull2[snap])
            halo_masses_2_prev = np.log10(HaloMassFull2[snap-1])
            mask_2_curr = (halo_masses_2 >= mass_min) & (halo_masses_2 < mass_max)
            mask_2_prev = (halo_masses_2_prev >= mass_min) & (halo_masses_2_prev < mass_max)
            
            if np.sum(mask_2_curr) > 0 and np.sum(mask_2_prev) > 0:
                mean_curr_2 = np.mean(coldgasFull2[snap][mask_2_curr])
                mean_prev_2 = np.mean(coldgasFull2[snap-1][mask_2_prev])
                rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
            else:
                rate_2 = np.nan
            
            coldgas_rate_model1.append(rate_1)
            coldgas_rate_model2.append(rate_2)
            lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)
        
        coldgas_rate_model1 = np.array(coldgas_rate_model1)
        coldgas_rate_model2 = np.array(coldgas_rate_model2)
        lookback_centers = np.array(lookback_centers)
        
        valid1 = ~np.isnan(coldgas_rate_model1)
        valid2 = ~np.isnan(coldgas_rate_model2)
        
        plt.plot(lookback_centers[valid1], coldgas_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
        plt.plot(lookback_centers[valid2], coldgas_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
        plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)
        
        plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
        plt.ylabel(r'$\mathrm{d}M_{\rm ColdGas}/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
        plt.title(f'{mass_label} Mass Haloes: {mass_min:.1f} < log(M$_{{vir}}$) < {mass_max:.1f}', fontsize=12)
        plt.legend(loc='best', frameon=False)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
        plt.tight_layout()
        
        outputFile = OutputDir + f'K.ColdGas_dMdt_{mass_label}Mass' + OutputFormat
        plt.savefig(outputFile, dpi=300, bbox_inches='tight')
        print('Saved file to', outputFile, '\n')
        plt.close()

    # --------------------------------------------------------

    for mass_label, (mass_min, mass_max) in mass_bins_dict.items():
        
        print(f'Plotting CGM dM/dt evolution for {mass_label} mass haloes')
        
        plt.figure(figsize=(10, 6))
        ax = plt.subplot(111)
        
        cgm_rate_model1 = []
        cgm_rate_model2 = []
        lookback_centers = []
        
        for snap in range(FirstSnap+1, LastSnap+1):
            try:
                if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                    dt = dT[snap][0]
                elif isinstance(dT[snap], (int, float)):
                    dt = dT[snap]
                else:
                    continue
            except:
                continue
            
            if dt <= 0:
                continue
            
            dt_yr = dt * 1e6
            
            halo_masses_1 = np.log10(HaloMassFull[snap])
            halo_masses_1_prev = np.log10(HaloMassFull[snap-1])
            mask_1_curr = (halo_masses_1 >= mass_min) & (halo_masses_1 < mass_max)
            mask_1_prev = (halo_masses_1_prev >= mass_min) & (halo_masses_1_prev < mass_max)
            
            if np.sum(mask_1_curr) > 0 and np.sum(mask_1_prev) > 0:
                mean_curr_1 = np.mean(cgmFull[snap][mask_1_curr])
                mean_prev_1 = np.mean(cgmFull[snap-1][mask_1_prev])
                rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
            else:
                rate_1 = np.nan
            
            halo_masses_2 = np.log10(HaloMassFull2[snap])
            halo_masses_2_prev = np.log10(HaloMassFull2[snap-1])
            mask_2_curr = (halo_masses_2 >= mass_min) & (halo_masses_2 < mass_max)
            mask_2_prev = (halo_masses_2_prev >= mass_min) & (halo_masses_2_prev < mass_max)
            
            if np.sum(mask_2_curr) > 0 and np.sum(mask_2_prev) > 0:
                mean_curr_2 = np.mean(cgmFull2[snap][mask_2_curr])
                mean_prev_2 = np.mean(cgmFull2[snap-1][mask_2_prev])
                rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
            else:
                rate_2 = np.nan
            
            cgm_rate_model1.append(rate_1)
            cgm_rate_model2.append(rate_2)
            lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)
        
        cgm_rate_model1 = np.array(cgm_rate_model1)
        cgm_rate_model2 = np.array(cgm_rate_model2)
        lookback_centers = np.array(lookback_centers)
        
        valid1 = ~np.isnan(cgm_rate_model1)
        valid2 = ~np.isnan(cgm_rate_model2)
        
        plt.plot(lookback_centers[valid1], cgm_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
        plt.plot(lookback_centers[valid2], cgm_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
        plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)
        
        plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
        plt.ylabel(r'$\mathrm{d}M_{\rm CGM}/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
        plt.title(f'{mass_label} Mass Haloes: {mass_min:.1f} < log(M$_{{vir}}$) < {mass_max:.1f}', fontsize=12)
        plt.legend(loc='best', frameon=False)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
        plt.tight_layout()
        
        outputFile = OutputDir + f'L.CGM_dMdt_{mass_label}Mass' + OutputFormat
        plt.savefig(outputFile, dpi=300, bbox_inches='tight')
        print('Saved file to', outputFile, '\n')
        plt.close()

    # --------------------------------------------------------

    for mass_label, (mass_min, mass_max) in mass_bins_dict.items():
        
        print(f'Plotting StellarMass dM/dt evolution for {mass_label} mass haloes')
        
        plt.figure(figsize=(10, 6))
        ax = plt.subplot(111)
        
        stellar_rate_model1 = []
        stellar_rate_model2 = []
        lookback_centers = []
        
        for snap in range(FirstSnap+1, LastSnap+1):
            try:
                if isinstance(dT[snap], np.ndarray) and len(dT[snap]) > 0:
                    dt = dT[snap][0]
                elif isinstance(dT[snap], (int, float)):
                    dt = dT[snap]
                else:
                    continue
            except:
                continue
            
            if dt <= 0:
                continue
            
            dt_yr = dt * 1e6
            
            halo_masses_1 = np.log10(HaloMassFull[snap])
            halo_masses_1_prev = np.log10(HaloMassFull[snap-1])
            mask_1_curr = (halo_masses_1 >= mass_min) & (halo_masses_1 < mass_max)
            mask_1_prev = (halo_masses_1_prev >= mass_min) & (halo_masses_1_prev < mass_max)
            
            if np.sum(mask_1_curr) > 0 and np.sum(mask_1_prev) > 0:
                mean_curr_1 = np.mean(StellarMassFull[snap][mask_1_curr])
                mean_prev_1 = np.mean(StellarMassFull[snap-1][mask_1_prev])
                rate_1 = (mean_curr_1 - mean_prev_1) / dt_yr
            else:
                rate_1 = np.nan
            
            halo_masses_2 = np.log10(HaloMassFull2[snap])
            halo_masses_2_prev = np.log10(HaloMassFull2[snap-1])
            mask_2_curr = (halo_masses_2 >= mass_min) & (halo_masses_2 < mass_max)
            mask_2_prev = (halo_masses_2_prev >= mass_min) & (halo_masses_2_prev < mass_max)
            
            if np.sum(mask_2_curr) > 0 and np.sum(mask_2_prev) > 0:
                mean_curr_2 = np.mean(StellarMassFull2[snap][mask_2_curr])
                mean_prev_2 = np.mean(StellarMassFull2[snap-1][mask_2_prev])
                rate_2 = (mean_curr_2 - mean_prev_2) / dt_yr
            else:
                rate_2 = np.nan
            
            stellar_rate_model1.append(rate_1)
            stellar_rate_model2.append(rate_2)
            lookback_centers.append((lookback_time_Gyr[snap] + lookback_time_Gyr[snap-1]) / 2.0)
        
        stellar_rate_model1 = np.array(stellar_rate_model1)
        stellar_rate_model2 = np.array(stellar_rate_model2)
        lookback_centers = np.array(lookback_centers)
        
        valid1 = ~np.isnan(stellar_rate_model1)
        valid2 = ~np.isnan(stellar_rate_model2)
        
        plt.plot(lookback_centers[valid1], stellar_rate_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
        plt.plot(lookback_centers[valid2], stellar_rate_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')
        plt.axhline(y=0, color='gray', linestyle=':', alpha=0.5)
        
        plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
        plt.ylabel(r'$\mathrm{d}M_*/\mathrm{d}t$ [$M_\odot$ yr$^{-1}$]', fontsize=14)
        plt.title(f'{mass_label} Mass Haloes: {mass_min:.1f} < log(M$_{{vir}}$) < {mass_max:.1f}', fontsize=12)
        plt.legend(loc='best', frameon=False)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
        plt.tight_layout()
        
        outputFile = OutputDir + f'M.StellarMass_dMdt_{mass_label}Mass' + OutputFormat
        plt.savefig(outputFile, dpi=300, bbox_inches='tight')
        print('Saved file to', outputFile, '\n')
        plt.close()

    print('\nAll mass-binned plots completed!')

    # --------------------------------------------------------

    print('Plotting HotGas Mass evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    hotgas_mass_model1 = []
    hotgas_mass_model2 = []
    lookback_times = []

    for snap in range(FirstSnap, LastSnap+1):
        if len(hotgasFull[snap]) > 0:
            mass_1 = np.mean(hotgasFull[snap])
        else:
            mass_1 = np.nan
        
        if len(hotgasFull2[snap]) > 0:
            mass_2 = np.mean(hotgasFull2[snap])
        else:
            mass_2 = np.nan
        
        hotgas_mass_model1.append(mass_1)
        hotgas_mass_model2.append(mass_2)
        lookback_times.append(lookback_time_Gyr[snap])

    hotgas_mass_model1 = np.array(hotgas_mass_model1)
    hotgas_mass_model2 = np.array(hotgas_mass_model2)
    lookback_times = np.array(lookback_times)

    valid1 = ~np.isnan(hotgas_mass_model1)
    valid2 = ~np.isnan(hotgas_mass_model2)

    plt.plot(lookback_times[valid1], hotgas_mass_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_times[valid2], hotgas_mass_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'Mean HotGas Mass [$M_\odot$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.yscale('log')
    plt.tight_layout()

    outputFile = OutputDir + 'N.HotGas_Mass_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting ColdGas Mass evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    coldgas_mass_model1 = []
    coldgas_mass_model2 = []
    lookback_times = []

    for snap in range(FirstSnap, LastSnap+1):
        if len(coldgasFull[snap]) > 0:
            mass_1 = np.mean(coldgasFull[snap])
        else:
            mass_1 = np.nan
        
        if len(coldgasFull2[snap]) > 0:
            mass_2 = np.mean(coldgasFull2[snap])
        else:
            mass_2 = np.nan
        
        coldgas_mass_model1.append(mass_1)
        coldgas_mass_model2.append(mass_2)
        lookback_times.append(lookback_time_Gyr[snap])

    coldgas_mass_model1 = np.array(coldgas_mass_model1)
    coldgas_mass_model2 = np.array(coldgas_mass_model2)
    lookback_times = np.array(lookback_times)

    valid1 = ~np.isnan(coldgas_mass_model1)
    valid2 = ~np.isnan(coldgas_mass_model2)

    plt.plot(lookback_times[valid1], coldgas_mass_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_times[valid2], coldgas_mass_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'Mean ColdGas Mass [$M_\odot$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.yscale('log')
    plt.tight_layout()

    outputFile = OutputDir + 'O.ColdGas_Mass_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting CGM Mass evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    cgm_mass_model1 = []
    cgm_mass_model2 = []
    lookback_times = []

    for snap in range(FirstSnap, LastSnap+1):
        if len(cgmFull[snap]) > 0:
            mass_1 = np.mean(cgmFull[snap])
        else:
            mass_1 = np.nan
        
        if len(cgmFull2[snap]) > 0:
            mass_2 = np.mean(cgmFull2[snap])
        else:
            mass_2 = np.nan
        
        cgm_mass_model1.append(mass_1)
        cgm_mass_model2.append(mass_2)
        lookback_times.append(lookback_time_Gyr[snap])

    cgm_mass_model1 = np.array(cgm_mass_model1)
    cgm_mass_model2 = np.array(cgm_mass_model2)
    lookback_times = np.array(lookback_times)

    valid1 = ~np.isnan(cgm_mass_model1)
    valid2 = ~np.isnan(cgm_mass_model2)

    plt.plot(lookback_times[valid1], cgm_mass_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_times[valid2], cgm_mass_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'Mean CGM Mass [$M_\odot$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.yscale('log')
    plt.tight_layout()

    outputFile = OutputDir + 'P.CGM_Mass_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------

    print('Plotting StellarMass evolution (Model Comparison)')

    plt.figure(figsize=(10, 6))
    ax = plt.subplot(111)

    stellar_mass_model1 = []
    stellar_mass_model2 = []
    lookback_times = []

    for snap in range(FirstSnap, LastSnap+1):
        if len(StellarMassFull[snap]) > 0:
            mass_1 = np.mean(StellarMassFull[snap])
        else:
            mass_1 = np.nan
        
        if len(StellarMassFull2[snap]) > 0:
            mass_2 = np.mean(StellarMassFull2[snap])
        else:
            mass_2 = np.nan
        
        stellar_mass_model1.append(mass_1)
        stellar_mass_model2.append(mass_2)
        lookback_times.append(lookback_time_Gyr[snap])

    stellar_mass_model1 = np.array(stellar_mass_model1)
    stellar_mass_model2 = np.array(stellar_mass_model2)
    lookback_times = np.array(lookback_times)

    valid1 = ~np.isnan(stellar_mass_model1)
    valid2 = ~np.isnan(stellar_mass_model2)

    plt.plot(lookback_times[valid1], stellar_mass_model1[valid1], 'b-', linewidth=2.5, label='SAGE CGM')
    plt.plot(lookback_times[valid2], stellar_mass_model2[valid2], 'r--', linewidth=2.5, label='evilSAGE')

    plt.xlabel(r'Lookback Time [Gyr]', fontsize=14)
    plt.ylabel(r'Mean Stellar Mass [$M_\odot$]', fontsize=14)
    plt.legend(loc='best', frameon=False)
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.5))
    plt.yscale('log')
    plt.tight_layout()

    outputFile = OutputDir + 'Q.StellarMass_Mass_ModelComparison' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
    
    print('Plotting stellar mass vs halo mass colored by regime across redshifts')
    dilute = 30000
    plt.figure(figsize=(12, 8))

    # Select a subset of snapshots for clarity (every other snapshot from SMFsnaps)
    regime_snaps = [63, 32, 23, 18]  # z=0, z~1, z~2, z~3
    regime_redshifts = [redshifts[snap] for snap in regime_snaps]
    
    # Color maps for the two regimes - different shades of blue and red
    blue_colors = ['#08519c', '#3182bd', '#6baed6', '#9ecae1']  # Dark to light blue
    red_colors = ['#a50f15', '#de2d26', '#fb6a4a', '#fcae91']   # Dark to light red
    
    for i, snap in enumerate(regime_snaps):
        # Get valid galaxies for this snapshot
        w = np.where((HaloMassFull[snap] > 0.0) & (StellarMassFull[snap] > 0.0))[0]
        if len(w) > dilute: 
            w = sample(list(w), dilute)
        
        if len(w) == 0:
            continue
            
        log10_halo_mass = np.log10(HaloMassFull[snap][w])
        log10_stellar_mass = np.log10(StellarMassFull[snap][w])
        regime_values = RegimeFull[snap][w]
        
        # Separate by regime
        cgm_regime = (regime_values == 0)
        hot_regime = (regime_values == 1)
        
        # Plot CGM regime (blue shades)
        if np.any(cgm_regime):
            plt.scatter(log10_halo_mass[cgm_regime], log10_stellar_mass[cgm_regime], 
                       c=blue_colors[i], s=8, alpha=0.7, 
                       label=f'CGM z={regime_redshifts[i]:.1f}')
        
        # Plot Hot regime (red shades)
        if np.any(hot_regime):
            plt.scatter(log10_halo_mass[hot_regime], log10_stellar_mass[hot_regime], 
                       c=red_colors[i], s=8, alpha=0.7, 
                       label=f'Hot z={regime_redshifts[i]:.1f}')

    plt.xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$', fontsize=14)
    plt.ylabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=14)
    plt.xlim(10, 15)
    plt.ylim(6, 12)
    
    # Add legend with two columns
    plt.legend(loc='upper left', frameon=False, ncol=2, fontsize=10)
    plt.tight_layout()

    outputFile = OutputDir + 'R.stellar_vs_halo_mass_by_regime_redshift' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
    
    print('Plotting regime distribution histograms across redshifts')
    dilute = 30000
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    axes = axes.flatten()
    
    # Use the same snapshots and redshifts as before
    regime_snaps = [63, 32, 23, 18]  # z=0, z~1, z~2, z~3
    regime_redshifts = [redshifts[snap] for snap in regime_snaps]
    
    # Histogram bins for halo mass
    halo_mass_bins = np.arange(10.0, 15.5, 0.2)
    bin_centers = (halo_mass_bins[:-1] + halo_mass_bins[1:]) / 2
    
    for i, snap in enumerate(regime_snaps):
        ax = axes[i]
        
        # Get valid galaxies for this snapshot
        w = np.where((HaloMassFull[snap] > 0.0) & (StellarMassFull[snap] > 0.0))[0]
        
        if len(w) == 0:
            continue
            
        log10_halo_mass = np.log10(HaloMassFull[snap][w])
        regime_values = RegimeFull[snap][w]
        
        # Separate by regime
        cgm_regime_masses = log10_halo_mass[regime_values == 0]
        hot_regime_masses = log10_halo_mass[regime_values == 1]
        
        # Create histograms of galaxy counts in halo mass bins
        cgm_hist, _ = np.histogram(cgm_regime_masses, bins=halo_mass_bins)
        hot_hist, _ = np.histogram(hot_regime_masses, bins=halo_mass_bins)
        
        # Plot stacked histograms showing actual galaxy counts
        ax.bar(bin_centers, cgm_hist, width=0.18, color='blue', alpha=0.7, 
               label='CGM Regime', bottom=0)
        ax.bar(bin_centers, hot_hist, width=0.18, color='red', alpha=0.7, 
               label='Hot Regime', bottom=cgm_hist)
        
        # Formatting
        ax.set_title(f'z = {regime_redshifts[i]:.1f}', fontsize=12, fontweight='bold')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$', fontsize=10)
        ax.set_ylabel('Galaxy Count', fontsize=10)
        ax.set_xlim(10.0, 15.0)
        ax.set_ylim(0, None)  # Auto-scale based on actual counts
        ax.grid(True, alpha=0.3)
        
        # Add regime statistics as text
        total_galaxies = len(w)
        cgm_count = np.sum(regime_values == 0)
        hot_count = np.sum(regime_values == 1)
        cgm_percent = (cgm_count / total_galaxies * 100) if total_galaxies > 0 else 0
        hot_percent = (hot_count / total_galaxies * 100) if total_galaxies > 0 else 0
        
        ax.text(0.02, 0.95, f'CGM: {cgm_percent:.1f}%\nHot: {hot_percent:.1f}%', 
                transform=ax.transAxes, verticalalignment='top', fontsize=9,
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    # Add legend to the first subplot
    axes[0].legend(loc='upper right', frameon=False, fontsize=10)
    
    plt.tight_layout()
    plt.suptitle('Regime Distribution vs Halo Mass Across Redshift', fontsize=14, y=0.98)
    
    outputFile = OutputDir + 'S.regime_distribution_histograms_redshift' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    # --------------------------------------------------------
    
    print('Plotting regime distribution histograms vs stellar mass across redshifts')

    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    axes = axes.flatten()
    dilute = 30000
    # Use the same snapshots and redshifts as before
    regime_snaps = [63, 32, 23, 18]  # z=0, z~1, z~2, z~3
    regime_redshifts = [redshifts[snap] for snap in regime_snaps]
    
    # Histogram bins for stellar mass
    stellar_mass_bins = np.arange(8.0, 12.5, 0.2)
    bin_centers = (stellar_mass_bins[:-1] + stellar_mass_bins[1:]) / 2
    
    for i, snap in enumerate(regime_snaps):
        ax = axes[i]
        
        # Get valid galaxies for this snapshot
        w = np.where((HaloMassFull[snap] > 0.0) & (StellarMassFull[snap] > 0.0))[0]
        
        if len(w) == 0:
            continue
            
        log10_stellar_mass = np.log10(StellarMassFull[snap][w])
        regime_values = RegimeFull[snap][w]
        
        # Separate by regime
        cgm_regime_masses = log10_stellar_mass[regime_values == 0]
        hot_regime_masses = log10_stellar_mass[regime_values == 1]
        
        # Create histograms of galaxy counts in stellar mass bins
        cgm_hist, _ = np.histogram(cgm_regime_masses, bins=stellar_mass_bins)
        hot_hist, _ = np.histogram(hot_regime_masses, bins=stellar_mass_bins)
        
        # Plot stacked histograms showing actual galaxy counts
        ax.bar(bin_centers, cgm_hist, width=0.18, color='blue', alpha=0.7, 
               label='CGM Regime', bottom=0)
        ax.bar(bin_centers, hot_hist, width=0.18, color='red', alpha=0.7, 
               label='Hot Regime', bottom=cgm_hist)
        
        # Formatting
        ax.set_title(f'z = {regime_redshifts[i]:.1f}', fontsize=12, fontweight='bold')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=10)
        ax.set_ylabel('Galaxy Count', fontsize=10)
        ax.set_xlim(8.0, 12.0)
        ax.set_ylim(0, None)  # Auto-scale based on actual counts
        ax.grid(True, alpha=0.3)
        
        # Add regime statistics as text
        total_galaxies = len(w)
        cgm_count = np.sum(regime_values == 0)
        hot_count = np.sum(regime_values == 1)
        cgm_percent = (cgm_count / total_galaxies * 100) if total_galaxies > 0 else 0
        hot_percent = (hot_count / total_galaxies * 100) if total_galaxies > 0 else 0
        
        ax.text(0.02, 0.95, f'CGM: {cgm_percent:.1f}%\nHot: {hot_percent:.1f}%', 
                transform=ax.transAxes, verticalalignment='top', fontsize=9,
                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    # Add legend to the first subplot
    axes[0].legend(loc='upper right', frameon=False, fontsize=10)
    
    plt.tight_layout()
    plt.suptitle('Regime Distribution vs Stellar Mass Across Redshift', fontsize=14, y=0.98)
    
    outputFile = OutputDir + 'T.regime_distribution_stellar_mass_histograms_redshift' + OutputFormat
    plt.savefig(outputFile, dpi=300, bbox_inches='tight')
    print('Saved file to', outputFile, '\n')
    plt.close()

    print('\nAll mass evolution plots completed!')