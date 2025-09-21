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

