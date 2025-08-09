#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import os
from collections import defaultdict

from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0  # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 7500       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

OutputFormat = '.pdf'
# plt.rcParams["figure.figsize"] = (8.34,6.25)
# plt.rcParams["figure.dpi"] = 500
# plt.rcParams["font.size"] = 14
plt.style.use('/Users/mbradley/Documents/cohare_palatino_sty.mplstyle')


# ==================================================================

def read_hdf(filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files"""
    # Get list of all model files in directory
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    model_files.sort()
    
    # Initialize empty array for combined data
    combined_data = None
    
    # Read and combine data from each model file
    for model_file in model_files:
        property = h5.File(DirName + model_file, 'r')
        data = np.array(property[snap_num][param])
        
        if combined_data is None:
            combined_data = data
        else:
            combined_data = np.concatenate((combined_data, data))
            
    return combined_data


# ==================================================================

if __name__ == '__main__':

    print('Running allresults (local)\n')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    CentralMvir = read_hdf(snap_num = Snapshot, param = 'CentralMvir') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num = Snapshot, param = 'Mvir') * 1.0e10 / Hubble_h
    StellarMass = read_hdf(snap_num = Snapshot, param = 'StellarMass') * 1.0e10 / Hubble_h
    BulgeMass = read_hdf(snap_num = Snapshot, param = 'BulgeMass') * 1.0e10 / Hubble_h
    BlackHoleMass = read_hdf(snap_num = Snapshot, param = 'BlackHoleMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num = Snapshot, param = 'ColdGas') * 1.0e10 / Hubble_h
    DiskRadius = read_hdf(snap_num = Snapshot, param = 'DiskRadius') / Hubble_h
    #print(DiskRadius)
    H1Gas = read_hdf(snap_num = Snapshot, param = 'HI_gas') * 1.0e10 / Hubble_h
    #print(H1Gas)
    H2Gas = read_hdf(snap_num = Snapshot, param = 'H2_gas') * 1.0e10 / Hubble_h
    #print(H2Gas)
    MetalsColdGas = read_hdf(snap_num = Snapshot, param = 'MetalsColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    # EjectedMass = read_hdf(snap_num = Snapshot, param = 'EjectedMass') * 1.0e10 / Hubble_h
    IntraClusterStars = read_hdf(snap_num = Snapshot, param = 'IntraClusterStars') * 1.0e10 / Hubble_h
    InfallMvir = read_hdf(snap_num = Snapshot, param = 'infallMvir') * 1.0e10 / Hubble_h
    outflowrate = read_hdf(snap_num = Snapshot, param = 'OutflowRate') * 1.0e10 / Hubble_h
    cgm = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
    #print(np.log10(CGM))
    massload = read_hdf(snap_num = Snapshot, param = 'MassLoading')  # Mass loading factor for outflows
    print('Mass loading factor for outflows:', massload)
    CriticalMassDB06 = read_hdf(snap_num = Snapshot, param = 'CriticalMassDB06') * 1.0e10 / Hubble_h
    MvirToMcritRatio = read_hdf(snap_num = Snapshot, param = 'MvirToMcritRatio')
    ColdInflowMass = read_hdf(snap_num = Snapshot, param = 'ColdInflowMass') * 1.0e10 / Hubble_h
    HotInflowMass = read_hdf(snap_num = Snapshot, param = 'HotInflowMass') * 1.0e10 / Hubble_h
    ReincorporatedGas = read_hdf(snap_num = Snapshot, param = 'ReincorporatedGas') * 1.0e10 / Hubble_h
    # print('Reincorporated gas:', ReincorporatedGas)
    Cooling = read_hdf(snap_num = Snapshot, param = 'Cooling')

    print('Critical mass from Dekel & Birnboim (2006):', CriticalMassDB06)
    print('Ratio of Mvir to Mcrit:', MvirToMcritRatio)
    print('Cumulative mass that accreted as cold streams:', ColdInflowMass)
    print('Cumulative mass that accreted shock-heated:', HotInflowMass)
    

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
    NDM = read_hdf(snap_num = Snapshot, param = 'Len')
 
    mask_ndm_20 = np.where((NDM <= 20))[0]
    print('Number of galaxies with less than or equal to 20 particles: ', len(NDM[mask_ndm_20]))
    print('Maximum stellar mass of these galaxies: ', np.max(np.log10(StellarMass)[mask_ndm_20]))

    mask_ndm_30 = np.where((NDM <= 30))[0]
    print('Number of galaxies with less than or equal to 30 particles: ', len(NDM[mask_ndm_30]))
    print('Maximum stellar mass of these galaxies: ', np.max(np.log10(StellarMass)[mask_ndm_30]))

    mask_ndm_50 = np.where((NDM <= 50))[0]
    print('Number of galaxies with less than or equal to 50 particles: ', len(NDM[mask_ndm_50]))
    print('Maximum stellar mass of these galaxies: ', np.max(np.log10(StellarMass)[mask_ndm_50]))

    mask_ndm_100 = np.where((NDM <= 100))[0]
    print('Number of galaxies with less than or equal to 100 particles: ', len(NDM[mask_ndm_100]))
    print('Maximum stellar mass of these galaxies: ', np.max(np.log10(StellarMass)[mask_ndm_100]))

    mask_ndm_200 = np.where((NDM <= 200))[0]
    print('Number of galaxies with less than or equal to 200 particles: ', len(NDM[mask_ndm_200]))
    print('Maximum stellar mass of these galaxies: ', np.max(np.log10(StellarMass)[mask_ndm_200]))

    mask_ndm_massive = np.where((NDM > 100))[0]
    print('Number of galaxies with greater than 100 particles: ', len(NDM[mask_ndm_massive]))
    print('Maximum stellar mass of these galaxies: ', np.max(np.log10(StellarMass)[mask_ndm_massive]))

    w = np.where(StellarMass > 1.0e10)[0]
    print('Number of galaxies read:', len(StellarMass))
    print('Galaxies more massive than 10^10 h-1 Msun:', len(w), '\n')

    # --------------------------------------------------------

    print('Plotting the stellar mass function splitting centrals and satellites')

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
    
    # additionally calculate satellites
    w = np.where(Type==1)[0]
    massRED = np.log10(StellarMass[w])
    (countsRED, binedges) = np.histogram(massRED, range=(mi, ma), bins=NB)

    # additionally calculate centrals
    w = np.where((StellarMass > 0.0)&(Type==0))[0]
    massBLU = np.log10(StellarMass[w])
    (countsBLU, binedges) = np.histogram(massBLU, range=(mi, ma), bins=NB)

    # additionally calculate orpans
    w = np.where((StellarMass > 0.0)&(Type==2))[0]
    massBLU2 = np.log10(StellarMass[w])
    (countsBLU2, binedges) = np.histogram(massBLU2, range=(mi, ma), bins=NB)

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
    plt.plot(xaxeshisto, countsBLU / volume / binwidth, 'b--', lw=2, label='Model - Centrals')
    plt.plot(xaxeshisto, countsRED / volume / binwidth, 'g--', lw=2, label='Model - Satellites')

    plt.plot(xaxeshisto, countsBLU2 / volume / binwidth, 'r--', lw=2, label='Model - Orphans')

    plt.yscale('log')
    plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    leg = plt.legend(loc='lower left', numpoints=1, labelspacing=0.1)
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '1.StellarMassFunction_CentralsSatellites' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------
    # H2 Mass vs. SFR
    print('Plotting H2 Mass vs. SFR')
    plt.figure()
    # Only consider galaxies with significant H2 and SFR
    valid = (H2Gas > 0) & (SfrDisk + SfrBulge > 0)
    if np.sum(valid) > dilute:
        valid_indices = sample(list(np.where(valid)[0]), dilute)
    else:
        valid_indices = np.where(valid)[0]
    plt.scatter(
        np.log10(H2Gas[valid_indices]),
        np.log10((SfrDisk + SfrBulge)[valid_indices]),
        c=np.log10(StellarMass[valid_indices]), cmap='plasma', alpha=0.6, s=5
    )
    cb = plt.colorbar()
    cb.mappable.set_clim(8, 12)
    cb.set_label(r'$\log_{{10}} M_{*}\ (M_\odot)$')
    plt.xlabel(r'$\log_{{10}} M_\mathrm{{H2}}\ (M_\odot)$')
    plt.ylabel(r'$\log_{{10}} \mathrm{{SFR}}\ (M_\odot/yr)$')
    # plt.title('H$_2$ Mass vs. SFR')
    # plt.grid(True, alpha=0.3)
    plt.xlim(6, 12)
    plt.tight_layout()
    plt.savefig(OutputDir + '19.H2_vs_SFR' + OutputFormat, bbox_inches='tight')
    plt.close()
    print('Saved file to', OutputDir + '19.H2_vs_SFR' + OutputFormat, '\n')

    # -------------------------------------------------------
    # H2 Fraction vs. sSFR
    print('Plotting H2 Fraction vs. sSFR')
    plt.figure()
    # H2 fraction and sSFR (already defined above, but recalculate for safety)
    h2_fraction = H2Gas / (H1Gas + H2Gas)
    SFR = SfrDisk + SfrBulge
    sSFR = np.log10(SFR / (StellarMass))
    valid = (h2_fraction > 0) & (SFR > 0) & (StellarMass > 0)
    if np.sum(valid) > dilute:
        valid_indices = sample(list(np.where(valid)[0]), dilute)
    else:
        valid_indices = np.where(valid)[0]
    plt.scatter(
        sSFR[valid_indices],
        h2_fraction[valid_indices],
        c=np.log10(StellarMass[valid_indices]), cmap='plasma', alpha=0.6, s=5
    )
    cb = plt.colorbar()
    cb.mappable.set_clim(8, 12)
    cb.set_label(r'$\log_{{10}} M_{*}\ (M_\odot)$')
    plt.xlabel(r'$\log_{{10}} \mathrm{{sSFR}}\ (yr^{{-1}})$')
    plt.ylabel('H$_2$ Fraction')
    # plt.title('H$_2$ Fraction vs. sSFR')
    # plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(OutputDir + '20.H2fraction_vs_sSFR' + OutputFormat, bbox_inches='tight')
    plt.close()
    print('Saved file to', OutputDir + '20.H2fraction_vs_sSFR' + OutputFormat, '\n')

  # -------------------------------------------------------
    # H1 Mass vs. SFR
    print('Plotting H1 Mass vs. SFR')
    plt.figure()
    # Only consider galaxies with significant H1 and SFR
    valid = (H1Gas > 0) & (SfrDisk + SfrBulge > 0)
    if np.sum(valid) > dilute:
        valid_indices = sample(list(np.where(valid)[0]), dilute)
    else:
        valid_indices = np.where(valid)[0]
    plt.scatter(
        np.log10(H1Gas[valid_indices]),
        np.log10((SfrDisk + SfrBulge)[valid_indices]),
        c=np.log10(StellarMass[valid_indices]), cmap='plasma', alpha=0.6, s=5
    )
    cb = plt.colorbar()
    cb.mappable.set_clim(8, 12)
    cb.set_label(r'$\log_{{10}} M_{*}\ (M_\odot)$')
    plt.xlabel(r'$\log_{{10}} M_\mathrm{{H1}}\ (M_\odot)$')
    plt.ylabel(r'$\log_{{10}} \mathrm{{SFR}}\ (M_\odot/yr)$')
    # plt.title('H$_I$ Mass vs. SFR')
    # plt.grid(True, alpha=0.3)
    plt.xlim(6, 12)
    plt.tight_layout()
    plt.savefig(OutputDir + '19.HI_vs_SFR' + OutputFormat, bbox_inches='tight')
    plt.close()
    print('Saved file to', OutputDir + '19.HI_vs_SFR' + OutputFormat, '\n')

    # -------------------------------------------------------
    # H1 Fraction vs. sSFR
    print('Plotting H1 Fraction vs. sSFR')
    plt.figure()
    # H1 fraction and sSFR (already defined above, but recalculate for safety)
    h1_fraction = H1Gas / (H1Gas + H2Gas)
    SFR = SfrDisk + SfrBulge
    sSFR = np.log10(SFR / (StellarMass))
    valid = (h1_fraction > 0) & (SFR > 0) & (StellarMass > 0)
    if np.sum(valid) > dilute:
        valid_indices = sample(list(np.where(valid)[0]), dilute)
    else:
        valid_indices = np.where(valid)[0]
    plt.scatter(
        sSFR[valid_indices],
        h2_fraction[valid_indices],
        c=np.log10(StellarMass[valid_indices]), cmap='plasma', alpha=0.6, s=5
    )
    cb = plt.colorbar()
    cb.mappable.set_clim(8, 12)
    cb.set_label(r'$\log_{{10}} M_{*}\ (M_\odot)$')
    plt.xlabel(r'$\log_{{10}} \mathrm{{sSFR}}\ (yr^{{-1}})$')
    plt.ylabel('H$_I$ Fraction')
    # plt.title('H$_I$ Fraction vs. sSFR')
    # plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(OutputDir + '20.HIfraction_vs_sSFR' + OutputFormat, bbox_inches='tight')
    plt.close()
    print('Saved file to', OutputDir + '20.HIfraction_vs_sSFR' + OutputFormat, '\n')

# --------------------------------------------------------

    print('Plotting the stellar mass function')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    binwidth = 0.1  # mass function histogram bin width

    # calculate all
    w = np.where((StellarMass > 0.0))[0]
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

    # calculate different particle number cuts
    w = np.where((StellarMass > 0.0)&(NDM > 20))[0]
    mass20 = np.log10(StellarMass[w])
    (counts20, binedges) = np.histogram(mass20, range=(mi, ma), bins=NB)

    w = np.where((StellarMass > 0.0)&(NDM > 50))[0]
    mass50 = np.log10(StellarMass[w])
    (counts50, binedges) = np.histogram(mass50, range=(mi, ma), bins=NB)

    w = np.where((StellarMass > 0.0)&(NDM > 100))[0]
    mass100 = np.log10(StellarMass[w])
    (counts100, binedges) = np.histogram(mass100, range=(mi, ma), bins=NB)

    w = np.where((StellarMass > 0.0)&(NDM > 200))[0]
    mass200 = np.log10(StellarMass[w])
    (counts200, binedges) = np.histogram(mass200, range=(mi, ma), bins=NB)

    w = np.where((StellarMass > 0.0))[0]
    mass_cgm = np.log10(cgm[w])
    (counts_cgm, binedges) = np.histogram(mass_cgm, range=(mi, ma), bins=NB)

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
    
    Baldry_Red_x = [7.488977955911820, 7.673346693386770, 7.881763527054110, 8.074148296593190, 8.306613226452910, 
        8.490981963927860, 8.69939879759519, 8.891783567134270, 9.084168336673350, 9.308617234468940, 
        9.501002004008020, 9.701402805611220, 9.901803607214430, 10.094188376753500, 10.270541082164300, 
        10.494989979959900, 10.711422845691400, 10.919839679358700, 11.08817635270540, 11.296593186372700, 
        11.488977955911800]

    Baldry_Red_y = [-1.9593147751606, -2.9785867237687400, -2.404710920770880, -3.038543897216280, -3.004282655246250, 
    -2.56745182012848, -2.6959314775160600, -2.464668094218420, -2.730192719486080, -2.618843683083510, 
    -2.6445396145610300, -2.687366167023560, -2.610278372591010, -2.490364025695930, -2.490364025695930, 
    -2.4389721627409000, -2.533190578158460, -2.6959314775160600, -2.8929336188436800, -3.4925053533190600, 
    -4.366167023554600]

    Baldry_Red_y = [10**x for x in Baldry_Red_y]

    Baldry_Blue_x = [7.088176352705410, 7.264529058116230, 7.488977955911820, 7.681362725450900, 7.897795591182370, 
        8.098196392785570, 8.298597194388780, 8.474949899799600, 8.69939879759519, 8.907815631262530, 
        9.092184368737470, 9.30060120240481, 9.48496993987976, 9.701402805611220, 9.885771543086170, 
        10.07815631262530, 10.294589178356700, 10.494989979959900, 10.703406813627300, 10.90380761523050, 
        11.064128256513000, 11.288577154308600]

    Baldry_Blue_y = [-1.7537473233404700, -1.376873661670240, -1.7023554603854400, -1.4796573875803000, -1.7023554603854400, 
    -1.5653104925053500, -1.6338329764454000, -1.7965738758030000, -1.7965738758030000, -1.9764453961456100, 
    -2.0792291220556700, -2.130620985010710, -2.293361884368310, -2.396145610278370, -2.490364025695930, 
    -2.6445396145610300, -2.635974304068520, -2.8244111349036400, -3.1327623126338300, -3.3897216274089900, 
    -4.383297644539620, -4.665952890792290]
    
    Baldry_Blue_y = [10**x for x in Baldry_Blue_y]
    
    x_blue = [8.131868131868131, 8.36923076923077, 8.606593406593406, 8.861538461538462, 9.107692307692307,
          9.362637362637361, 9.608791208791208, 9.872527472527473, 10.10989010989011, 10.364835164835164,
            10.61978021978022, 10.857142857142858, 11.112087912087912, 11.367032967032968]

    y_blue = [-1.7269230769230768, -1.773076923076923, -1.8653846153846152, -1.9576923076923076, -2.096153846153846,
          -2.223076923076923, -2.338461538461538, -2.476923076923077, -2.592307692307692,
            -2.6846153846153844, -2.8692307692307693, -3.0884615384615386, -3.4923076923076923,
              -4.126923076923077]
    y_blue = [10**x for x in y_blue]

    x_red = [8.123076923076923, 8.378021978021978, 8.624175824175824, 8.861538461538462, 9.125274725274725,
          9.371428571428572, 9.635164835164835, 9.863736263736264, 10.118681318681318, 10.373626373626374,
            10.61978021978022, 10.874725274725275, 11.12967032967033, 11.367032967032968, 11.63076923076923]

    y_red = [-2.3846153846153846, -2.6384615384615384, -2.8346153846153848, -2.9153846153846152, -3.019230769230769,
          -3.0769230769230766, -3.0538461538461537, -3.0307692307692307, -3.019230769230769,
            -3.0076923076923077, -3.0076923076923077, -3.1115384615384616, -3.319230769230769,
              -3.85, -4.265384615384615]
    y_red = [10**x for x in y_red]

    Baldry_xval = np.log10(10 ** Baldry[:, 0]  /Hubble_h/Hubble_h)
    if(whichimf == 1):  Baldry_xval = Baldry_xval - 0.26  # convert back to Chabrier IMF
    Baldry_yvalU = (Baldry[:, 1]+Baldry[:, 2]) * Hubble_h*Hubble_h*Hubble_h
    Baldry_yvalL = (Baldry[:, 1]-Baldry[:, 2]) * Hubble_h*Hubble_h*Hubble_h
    plt.fill_between(Baldry_xval, Baldry_yvalU, Baldry_yvalL, 
        facecolor='purple', alpha=0.25, label='Baldry et al. 2008 (z=0.1)')
    
    # This next line is just to get the shaded region to appear correctly in the legend
    plt.plot(xaxeshisto, counts / volume / binwidth, label='Baldry et al. 2008', color='purple', alpha=0.3)
    
    # Overplot the model histograms
    plt.plot(x_blue, y_blue, 'c:', lw=2, label='Weaver et al., 2022 - Blue')
    plt.plot(x_red, y_red, 'm:', lw=2, label='Weaver et al., 2022 - Red')
    plt.scatter(Baldry_Blue_x, Baldry_Blue_y, marker='s', s=50, edgecolor='navy', 
                facecolor='navy', alpha=0.3, label='Baldry et al. 2012 - Blue')
    plt.scatter(Baldry_Red_x, Baldry_Red_y, marker='s', s=50, edgecolor='maroon', 
                facecolor='maroon', alpha=0.3, label='Baldry et al. 2012 - Red')
    plt.plot(xaxeshisto, counts    / volume / binwidth, 'k-', lw=2, label='Model - All')
    plt.plot(xaxeshisto, countsRED / volume / binwidth, 'r-', lw=2, label='Model - Red')
    plt.plot(xaxeshisto, countsBLU / volume / binwidth, 'b-', lw=2, label='Model - Blue')

    plt.plot(xaxeshisto, counts20    / volume / binwidth, 'r--', lw=1, label='Model - 20 particles or less')
    plt.plot(xaxeshisto, counts50    / volume / binwidth, 'g--', lw=1, label='Model - 50 particles or less')
    plt.plot(xaxeshisto, counts100    / volume / binwidth, 'b--', lw=1, label='Model - 100 particles or less')
    plt.plot(xaxeshisto, counts200    / volume / binwidth, 'k--', lw=1, label='Model - 200 particles or less')

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
    mass = np.log10( (StellarMass[w] + 1.4*ColdGas[w]) )

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    # additionally calculate satellites
    w_sat = np.where((StellarMass + ColdGas > 0.0) & (Type == 1))[0]
    mass_sat = np.log10( (StellarMass[w_sat] + 1.4*ColdGas[w_sat]) )
    (counts_sat, binedges_sat) = np.histogram(mass_sat, range=(mi, ma), bins=NB)
    xaxeshisto_sat = binedges_sat[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    # additionally calculate centrals
    w_cent = np.where((StellarMass > 0.0) & (Type == 0))[0]
    mass_cent = np.log10( (StellarMass[w_cent] + 1.4*ColdGas[w_cent]) )
    (counts_cent, binedges_cent) = np.histogram(mass_cent, range=(mi, ma), bins=NB)
    xaxeshisto_cent = binedges_cent[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

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
    plt.plot(xaxeshisto, np.log10(counts / volume / binwidth), 'k-', label='Model')
    plt.plot(xaxeshisto_sat, np.log10(counts_sat / volume / binwidth), 'r--', lw=2, label='Model - Satellites')
    plt.plot(xaxeshisto_cent, np.log10(counts_cent / volume / binwidth), 'b--', lw=2, label='Model - Centrals')


    plt.axis([9.0, 12.2, -6, -1])
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

    plt.ylabel(r'$\log_{10}\ \phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')  # Set the y...
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
    h1 = np.log10(H1Gas[w])
    print(h1)
    h2 = np.log10(H2Gas[w])
    print(h2)
    sSFR = (SfrDisk[w] + SfrBulge[w]) / StellarMass[w]

    mi = np.floor(min(mass)) - 2
    ma = np.floor(max(mass)) + 2
    
    NB = int((ma - mi) / binwidth)
    (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)
    xaxeshisto = binedges[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    (counts_h1, binedges_h1) = np.histogram(h1, range=(mi, ma), bins=NB)
    xaxeshisto_h1 = binedges_h1[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins

    (counts_h2, binedges_h2) = np.histogram(h2, range=(mi, ma), bins=NB)
    xaxeshisto_h2 = binedges_h2[:-1] + 0.5 * binwidth  # Set the x-axis values to be the centre of the bins
    
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
    plt.plot(xaxeshisto_h1, counts_h1 / volume / binwidth, color='cyan', linestyle='-', label='Model - H1')
    plt.plot(xaxeshisto_h2, counts_h2 / volume / binwidth, color='magenta', linestyle='-', label='Model - H2')

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

    w2 = np.where(StellarMass > 0.01)[0]
    if(len(w2) > dilute): w2 = sample(list(range(len(w2))), dilute)
    mass = np.log10(StellarMass[w2])
    sSFR = np.log10( (SfrDisk[w2] + SfrBulge[w2]) / StellarMass[w2] )
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

    # -------------------------------------------------------

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w2 = np.where(StellarMass > 0.01)[0]
    if(len(w2) > dilute): w2 = sample(list(range(len(w2))), dilute)
    mass = np.log10(StellarMass[w2])
    sSFR = np.log10( (SfrDisk[w2] + SfrBulge[w2]) / StellarMass[w2] )

    # additionally calculate red
    w = np.where(sSFR < sSFRcut)[0]
    sfr_RED = sSFR[w]
    mass_RED = mass[w]

    # additionally calculate blue
    w = np.where(sSFR > sSFRcut)[0]
    sfr_BLU = sSFR[w]
    mass_BLU = mass[w]

    plt.scatter(mass_BLU, sfr_BLU, marker='o', s=1, c='#0072B2', alpha=0.3)
    plt.scatter(mass_RED, sfr_RED, marker='o', s=1, c='#D55E00', alpha=0.3)

    # Define mass bins
    mass_bins = np.arange(8.0, 12.0, 0.2)  # 0.2 dex bins
    mass_centers = mass_bins[:-1] + 0.1  # Center of each bin

    # Calculate median and scatter for star-forming population
    median_sf = []
    p16_sf = []
    p84_sf = []

    for i in range(len(mass_bins)-1):
        mask = (mass_BLU >= mass_bins[i]) & (mass_BLU < mass_bins[i+1])
        if np.sum(mask) > 5:  # Need at least 5 galaxies
            values = sfr_BLU[mask]
            median_sf.append(np.median(values))
            p16_sf.append(np.percentile(values, 16))
            p84_sf.append(np.percentile(values, 84))
        else:
            median_sf.append(np.nan)
            p16_sf.append(np.nan)
            p84_sf.append(np.nan)

    # Calculate median and scatter for quiescent population  
    median_q = []
    p16_q = []
    p84_q = []

    for i in range(len(mass_bins)-1):
        mask = (mass_RED >= mass_bins[i]) & (mass_RED < mass_bins[i+1])
        if np.sum(mask) > 5:  # Need at least 5 galaxies
            values = sfr_RED[mask]
            median_q.append(np.median(values))
            p16_q.append(np.percentile(values, 16))
            p84_q.append(np.percentile(values, 84))
        else:
            median_q.append(np.nan)
            p16_q.append(np.nan)
            p84_q.append(np.nan)

    # Convert to arrays and remove NaN values
    median_sf = np.array(median_sf)
    p16_sf = np.array(p16_sf)
    p84_sf = np.array(p84_sf)
    median_q = np.array(median_q)
    p16_q = np.array(p16_q)
    p84_q = np.array(p84_q)

    # Remove NaN entries
    valid_sf = ~np.isnan(median_sf)
    valid_q = ~np.isnan(median_q)

    # Plot median lines
    if np.sum(valid_sf) > 0:
        plt.plot(mass_centers[valid_sf], median_sf[valid_sf], 'b-', linewidth=1.5, 
                alpha=0.8, zorder=3, label='Star forming')

    if np.sum(valid_q) > 0:
        plt.plot(mass_centers[valid_q], median_q[valid_q], 'r-', linewidth=1.5, 
                alpha=0.8, zorder=3, label='Quiescent')

    # overplot dividing line between SF and passive
    w = np.arange(7.0, 13.0, 1.0)
    #plt.plot(w, w/w*sSFRcut, 'k:', lw=2.0)

    plt.ylabel(r'$\log_{10}\ s\mathrm{SFR}\ (\mathrm{yr^{-1}})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.axis([8.0, 12.0, -16.0, -8.0])

    leg = plt.legend(loc='upper right')
    leg.draw_frame(False)  # Don't want a box frame
    for t in leg.get_texts():  # Reduce the size of the text
        t.set_fontsize('medium')

    outputFile = OutputDir + '5.SpecificStarFormationRate_bimodal' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting the SFR')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w2 = np.where(StellarMass > 0.01)[0]
    if(len(w2) > dilute): w2 = sample(list(range(len(w2))), dilute)
    mass = np.log10(StellarMass[w2])
    SFR =  (SfrDisk[w2] + SfrBulge[w2])

    # additionally calculate red
    w = np.where(sSFR < sSFRcut)[0]
    sfr_RED = SFR[w]
    mass_RED = mass[w]

    # additionally calculate blue
    w = np.where(sSFR > sSFRcut)[0]
    sfr_BLU = SFR[w]
    mass_BLU = mass[w]

    # Create scatter plot with metallicity coloring
    plt.scatter(mass_BLU, np.log10(sfr_BLU), c='b', marker='o', s=1, alpha=0.7, label='Star forming')
    plt.scatter(mass_RED, np.log10(sfr_RED), c='r', marker='o', s=1, alpha=0.7, label='Passive')

    plt.ylabel(r'$\log_{10} \mathrm{SFR}\ (M_{\odot}\ \mathrm{yr^{-1}})$')  # Set the y...
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

    # Set the x and y axis minor ticks
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.xlim(6.0, 12.2)
    plt.ylim(-5, 3)  # Set y-axis limits for SFR

    outputFile = OutputDir + '5.StarFormationRate' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved to', outputFile, '\n')
    plt.close()

# ---------------------------------------------------------

    print('Plotting the gas fractions')

    plt.figure()  # New figure
    ax = plt.subplot(111)  # 1 plot on the figure

    w = np.where((Type == 0) & (StellarMass + ColdGas > 0.0) & 
      (BulgeMass / StellarMass > 0.1) & (BulgeMass / StellarMass < 0.5))[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
    
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
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
    
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
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

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
    
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # Set the x-axis label
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
    print('Plotting the average baryon fraction vs halo mass (can take time)')

    plt.figure()
    ax = plt.subplot(111)

    HaloMass = np.log10(Mvir)
    Baryons = StellarMass + ColdGas + HotGas + cgm + IntraClusterStars + BlackHoleMass

    MinHalo, MaxHalo, Interval = 11.0, 16.0, 0.1
    HaloBins = np.arange(MinHalo, MaxHalo + Interval, Interval)
    Nbins = len(HaloBins) - 1

    MeanCentralHaloMass = []
    MeanBaryonFraction = []
    MeanBaryonFractionU = []
    MeanBaryonFractionL = []
    MeanStars = []
    MeanStarsU = []
    MeanStarsL = []
    MeanCold = []
    MeanColdU = []
    MeanColdL = []
    MeanHot = []
    MeanHotU = []
    MeanHotL = []
    MeanCGM = []
    MeanCGMU = []
    MeanCGML = []
    MeanICS = []
    MeanICSU = []
    MeanICSL = []
    MeanBH = []
    MeanBHU = []
    MeanBHL = []

    bin_indices = np.digitize(HaloMass, HaloBins) - 1

    # Pre-compute unique CentralGalaxyIndex for faster lookup
    halo_to_galaxies = defaultdict(list)
    for i, central_idx in enumerate(CentralGalaxyIndex):
        halo_to_galaxies[central_idx].append(i)

    for i in range(Nbins - 1):
        w1 = np.where((Type == 0) & (bin_indices == i))[0]
        HalosFound = len(w1)
        
        if HalosFound > 2:
            # Pre-allocate arrays for better performance
            BaryonFractions = np.zeros(HalosFound)
            StarsFractions = np.zeros(HalosFound)
            ColdFractions = np.zeros(HalosFound)
            HotFractions = np.zeros(HalosFound)
            CGMFractions = np.zeros(HalosFound)
            ICSFractions = np.zeros(HalosFound)
            BHFractions = np.zeros(HalosFound)
            
            # Vectorized calculation for each halo
            for idx, halo_idx in enumerate(w1):
                halo_galaxies = np.array(halo_to_galaxies[CentralGalaxyIndex[halo_idx]])
                halo_mvir = Mvir[halo_idx]
                
                # Use advanced indexing for faster summing
                BaryonFractions[idx] = np.sum(Baryons[halo_galaxies]) / halo_mvir
                StarsFractions[idx] = np.sum(StellarMass[halo_galaxies]) / halo_mvir
                ColdFractions[idx] = np.sum(ColdGas[halo_galaxies]) / halo_mvir
                HotFractions[idx] = np.sum(HotGas[halo_galaxies]) / halo_mvir
                CGMFractions[idx] = np.sum(cgm[halo_galaxies]) / halo_mvir
                ICSFractions[idx] = np.sum(IntraClusterStars[halo_galaxies]) / halo_mvir
                BHFractions[idx] = np.sum(BlackHoleMass[halo_galaxies]) / halo_mvir
            
            # Calculate statistics once for all arrays
            CentralHaloMass = np.log10(Mvir[w1])
            MeanCentralHaloMass.append(np.mean(CentralHaloMass))
            
            n_halos = len(BaryonFractions)
            sqrt_n = np.sqrt(n_halos)
            
            # Vectorized mean and std calculations
            means = [np.mean(arr) for arr in [BaryonFractions, StarsFractions, ColdFractions, 
                                             HotFractions, CGMFractions, ICSFractions, BHFractions]]
            stds = [np.std(arr) / sqrt_n for arr in [BaryonFractions, StarsFractions, ColdFractions, 
                                                    HotFractions, CGMFractions, ICSFractions, BHFractions]]
            
            # Append all means and bounds
            MeanBaryonFraction.append(means[0])
            MeanBaryonFractionU.append(means[0] + stds[0])
            MeanBaryonFractionL.append(means[0] - stds[0])
            
            MeanStars.append(means[1])
            MeanStarsU.append(means[1] + stds[1])
            MeanStarsL.append(means[1] - stds[1])
            
            MeanCold.append(means[2])
            MeanColdU.append(means[2] + stds[2])
            MeanColdL.append(means[2] - stds[2])
            
            MeanHot.append(means[3])
            MeanHotU.append(means[3] + stds[3])
            MeanHotL.append(means[3] - stds[3])
            
            MeanCGM.append(means[4])
            MeanCGMU.append(means[4] + stds[4])
            MeanCGML.append(means[4] - stds[4])
            
            MeanICS.append(means[5])
            MeanICSU.append(means[5] + stds[5])
            MeanICSL.append(means[5] - stds[5])
            
            MeanBH.append(means[6])
            MeanBHU.append(means[6] + stds[6])
            MeanBHL.append(means[6] - stds[6])

    # Convert lists to arrays and ensure positive values for log scale
    MeanCentralHaloMass = np.array(MeanCentralHaloMass)
    MeanBaryonFraction = np.array(MeanBaryonFraction)
    MeanBaryonFractionU = np.array(MeanBaryonFractionU)
    MeanBaryonFractionL = np.maximum(np.array(MeanBaryonFractionL), 1e-6)  # Prevent negative values on log scale
    
    MeanStars = np.array(MeanStars)
    MeanStarsU = np.array(MeanStarsU)
    MeanStarsL = np.maximum(np.array(MeanStarsL), 1e-6)
    
    MeanCold = np.array(MeanCold)
    MeanColdU = np.array(MeanColdU)
    MeanColdL = np.maximum(np.array(MeanColdL), 1e-6)
    
    MeanHot = np.array(MeanHot)
    MeanHotU = np.array(MeanHotU)
    MeanHotL = np.maximum(np.array(MeanHotL), 1e-6)
    
    MeanCGM = np.array(MeanCGM)
    MeanCGMU = np.array(MeanCGMU)
    MeanCGML = np.maximum(np.array(MeanCGML), 1e-6)
    
    MeanICS = np.array(MeanICS)
    MeanICSU = np.array(MeanICSU)
    MeanICSL = np.maximum(np.array(MeanICSL), 1e-6)

    baryon_frac = 0.17
    plt.axhline(y=baryon_frac, color='grey', linestyle='--', linewidth=1.0, 
            label='Baryon Fraction = {:.2f}'.format(baryon_frac))

    # Add 1-sigma shading for each mass reservoir
    plt.fill_between(MeanCentralHaloMass, MeanBaryonFractionL, MeanBaryonFractionU, 
                     color='black', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanStarsL, MeanStarsU, 
                     color='purple', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanColdL, MeanColdU, 
                     color='blue', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanHotL, MeanHotU, 
                     color='red', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanCGML, MeanCGMU, 
                     color='green', alpha=0.2)
    plt.fill_between(MeanCentralHaloMass, MeanICSL, MeanICSU, 
                     color='orange', alpha=0.2)

    plt.plot(MeanCentralHaloMass, MeanBaryonFraction, 'k-', label='Total')
    plt.plot(MeanCentralHaloMass, MeanStars, label='Stars', color='purple', linestyle='--')
    plt.plot(MeanCentralHaloMass, MeanCold, label='Cold gas', color='blue', linestyle=':')
    plt.plot(MeanCentralHaloMass, MeanHot, label='Hot gas', color='red')
    plt.plot(MeanCentralHaloMass, MeanCGM, label='Circumgalactic Medium', color='green', linestyle='-.')
    plt.plot(MeanCentralHaloMass, MeanICS, label='Intracluster Stars', color='orange', linestyle='-.')

    #plt.yscale('log')

    plt.xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} \mathrm{Baryon\ Fraction}$')
    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))
    plt.axis([11.1, 15.0, 0.0, 0.2])

    leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1, bbox_to_anchor=(1.0, 0.5))
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
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    HaloMass = np.log10(Mvir[w])
    plt.scatter(HaloMass, np.log10(StellarMass[w]), marker='o', s=0.3, c='k', alpha=0.5, label='Stars')
    plt.scatter(HaloMass, np.log10(ColdGas[w]), marker='o', s=0.3, color='blue', alpha=0.5, label='Cold gas')
    plt.scatter(HaloMass, np.log10(HotGas[w]), marker='o', s=0.3, color='red', alpha=0.5, label='Hot gas')
    # plt.scatter(HaloMass, np.log10(EjectedMass[w]), marker='o', s=50.3, color='green', alpha=0.5, label='Ejected gas')
    plt.scatter(HaloMass, np.log10(IntraClusterStars[w]), marker='o', s=10, color='yellow', alpha=0.5, label='Intracluster stars')
    plt.scatter(HaloMass, np.log10(cgm[w]), marker='o', s=0.3, color='green', alpha=0.5, label='CGM')

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

    w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
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

    print('Plotting the spatial distribution of galaxies in 50 Mpc^3 sub-boxes')

    # -------------------------------------------------------

    plt.figure(figsize=(10, 10))  # Adjust figure size as needed
  
    w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    #print(xx)
    yy = Posy[w]
    #print(yy)
    zz = Posz[w]
    #print(zz)

    sub_box_size = 50.0  # Size of each sub-box in Mpc
    num_sub_boxes = int(BoxSize / sub_box_size)

    for i in range(num_sub_boxes):
        for j in range(num_sub_boxes):
            ax = plt.subplot(num_sub_boxes, num_sub_boxes, i * num_sub_boxes + j + 1)
            
            x_min = i * sub_box_size
            x_max = (i + 1) * sub_box_size
            y_min = j * sub_box_size
            y_max = (j + 1) * sub_box_size
            
            w_sub_box = np.where((xx >= x_min) & (xx < x_max) & (yy >= y_min) & (yy < y_max))[0]
            
            if len(w_sub_box) > 0:
                plt.scatter(xx[w_sub_box], yy[w_sub_box], marker='o', s=10, c='k', alpha=0.5)
            else:
                # Highlight empty sub-boxes with a different color or marker
                plt.plot([], [], 'ro', markersize=50)  # Empty sub-box marker
            
            plt.xlim(x_min, x_max)
            plt.ylim(y_min, y_max)
            plt.xticks([])
            plt.yticks([])
            
            if i == num_sub_boxes - 1:
                plt.xlabel(f'{y_min} < y < {y_max}')
            if j == 0:
                plt.ylabel(f'{x_min} < x < {x_max}')

    plt.tight_layout()

    outputFile = OutputDir + '14.SpatialDistributionSubBoxes' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # Replace the existing density plot section with:
    plt.figure(figsize=(15,7))  # Wider figure to accommodate two plots

    # First subplot - Galaxy number density (as before)
    plt.subplot(131)  # 1 row, 2 cols, plot 1
    w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]

    h, xedges, yedges = np.histogram2d(xx, yy, bins=100)
    plt.imshow(h.T, origin='lower', extent=[0, BoxSize, 0, BoxSize], 
            aspect='equal', cmap='gist_heat')
    #plt.colorbar(label='Number of galaxies')
    plt.xlabel('x (Mpc/h)')
    plt.ylabel('y (Mpc/h)')
    # plt.title('Galaxy Number Density')

    # Second subplot - H2 gas density
    plt.subplot(132)  # 1 row, 2 cols, plot 2
    w = np.where((Mvir > 0.0) & (H2Gas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]
    weights = np.log10(H2Gas[w])  # Use H2 gas mass as weights

    h, xedges, yedges = np.histogram2d(xx, yy, bins=100, weights=weights)
    plt.imshow(h.T, origin='lower', extent=[0, BoxSize, 0, BoxSize], 
            aspect='equal', cmap='CMRmap')
    #plt.colorbar(label=r'$log_{10}\ H^2\ M_{\odot}/h$')
    plt.xlabel('x (Mpc/h)')
    plt.ylabel('y (Mpc/h)')
    # plt.title('H2 Gas Density')

    # Third subplot - H1 gas density
    plt.subplot(133)  # 1 row, 2 cols, plot 3
    w = np.where((Mvir > 0.0) & (H1Gas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]
    weights = np.log10(H1Gas[w])  # Use H1 gas mass as weights

    h, xedges, yedges = np.histogram2d(xx, yy, bins=100, weights=weights)
    plt.imshow(h.T, origin='lower', extent=[0, BoxSize, 0, BoxSize], 
            aspect='equal', cmap='CMRmap')
    #plt.colorbar(label=r'$log_{10}\ H^1\ M_{\odot}/h$')
    plt.xlabel('x (Mpc/h)')
    plt.ylabel('y (Mpc/h)')
    # plt.title('H1 Gas Density')

    plt.tight_layout()

    outputFile = OutputDir + '16.DensityPlotandH2andH1' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Checking for holes in the galaxy distribution')

    w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    #print(xx)
    yy = Posy[w]
    zz = Posz[w]

    width = 16.0  # Width of each sub-box in Mpc
    uptoX = 0.0
    uptoY = 0.0
    uptoZ = 0.0

    empty_regions = []

    while uptoX < BoxSize:
        while uptoY < BoxSize:
            while uptoZ < BoxSize:
                w_sub_box = np.where((xx >= uptoX) & (xx < uptoX + width) &
                                    (yy >= uptoY) & (yy < uptoY + width) &
                                    (zz >= uptoZ) & (zz < uptoZ + width))[0]
                
                if len(w_sub_box) == 0:
                    empty_regions.append((uptoX, uptoX + width, uptoY, uptoY + width, uptoZ, uptoZ + width))
                
                uptoZ += width
            
            uptoZ = 0.0
            uptoY += width
        
        uptoY = 0.0
        uptoX += width

    print("Number of empty regions found:", len(empty_regions))

    if len(empty_regions) > 0:
        print("Empty regions (x_min, x_max, y_min, y_max, z_min, z_max):")
        for region in empty_regions:
            print(region)
    else:
        print("No empty regions found.")

    # -------------------------------------------------------

    print('Plotting galaxy velocity diagnostics')

    plt.figure()  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Type == 0) & (StellarMass > 1.0e7) & (Vmax > 0))[0]  # Central galaxies only
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    mass = np.log10(StellarMass[w])
    vel = Vvir[w]

    # Color points by halo mass
    halo_mass = np.log10(Mvir[w])
    sc = plt.scatter(mass, vel, c=halo_mass, cmap='viridis', s=5, alpha=0.7)
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')

    plt.ylabel(r'$V_{\mathrm{vir}}\ (\mathrm{km/s})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    # plt.title('Virial Velocity vs Stellar Mass (Centrals)')

    # Add Tully-Fisher reference line
    mass_tf = np.arange(8, 12, 0.1)
    vel_tf = 10**(0.27 * mass_tf - 0.55)  # Simple Tully-Fisher relation
    plt.plot(mass_tf, vel_tf, 'r--', lw=2, label='Tully-Fisher Relation')
    plt.legend(loc='upper left')
    plt.xlim(6, 12)

    plt.tight_layout()

    outputFile = OutputDir + '15.VelocityDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting galaxy ejection diagnostics')

    plt.figure()  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Type == 0) & (Vmax > 0))[0]  # Central galaxies only
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    sat = np.where((Type == 1) & (Vmax > 0))[0]  
    if(len(sat) > dilute): sat = sample(list(range(len(sat))), dilute)

    mass = np.log10(StellarMass[w])
    sats = np.log10(StellarMass[sat])
    ejected = np.log10(cgm[w])

    # Color points by halo mass
    halo_mass = np.log10(Mvir[w])
    sc = plt.scatter(mass, np.log10(ejected), c=halo_mass, cmap='viridis', s=5, alpha=0.7, label='Centrals')
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')

    # plt.scatter(sats, np.log10(cgm[sat]), marker='*', s=250, c='k', alpha=0.5, label='Satellites')

    plt.ylabel(r'$\log_{10} M_{\mathrm{ejected}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    # plt.title('Ejected Mass vs Stellar Mass')

    plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '16.CGMDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting galaxy infall diagnostics')

    plt.figure()  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Type == 0) & (Vmax > 0))[0]  # Central galaxies only
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    sat = np.where((Type == 1) & (Vmax > 0))[0]  
    if(len(sat) > dilute): sat = sample(list(range(len(sat))), dilute)

    mass = np.log10(Mvir[w])
    smass = np.log10(StellarMass[w])
    sats = np.log10(Mvir[sat])
    infall = np.log10(InfallMvir[w])

    plt.scatter(sats, np.log10(InfallMvir[sat]), marker='*', s=25, c='k', alpha=0.5, label='Satellites')
    # Color points by halo mass
    sc = plt.scatter(mass, infall, c=smass, cmap='viridis', s=5, alpha=0.7, label='Centrals')
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')

    plt.ylabel(r'$\log_{10} M_{\mathrm{infall}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
    # plt.title('Infall Mass vs Halo Mass')

    plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '17.InfallDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting galaxy outflow diagnostics')

    plt.figure()  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Type == 0) & (Vmax > 0))[0]  # Central galaxies only
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    sat = np.where((Type == 1) & (Vmax > 0))[0]  
    if(len(sat) > dilute): sat = sample(list(range(len(sat))), dilute)

    mass = np.log10(StellarMass[w])
    sats = np.log10(StellarMass[sat])
    outflow = np.log10(outflowrate[w])

    # Color points by halo mass
    halo_mass = np.log10(Mvir[w])
    plt.scatter(sats, np.log10(outflowrate[sat]), marker='*', s=250, c='k', alpha=0.5, label='Satellites')
    sc = plt.scatter(mass, outflow, c=halo_mass, cmap='viridis', s=5, alpha=0.7, label='Centrals')
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')

    plt.ylabel(r'$\log_{10} M_{\mathrm{outflow}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    # plt.title('Outflow rate vs Stellar Mass')

    plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '18.OutflowrateDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Regime diagram showing cold streams vs shock heated galaxies')

    plt.figure()

    # Filter for central galaxies with reasonable masses
    centrals = (Type == 0) & (StellarMass > 1e8) & (Mvir > 0)

    # Plot the 1:1 line (Mvir = Mcrit)
    mass_range = np.logspace(10, 15, 100)
    plt.loglog(mass_range, mass_range, 'k--', linewidth=2, label='Mvir = Mcrit')

    # Scatter plot colored by regime
    cold_regime = centrals & (MvirToMcritRatio < 1.0)
    hot_regime = centrals & (MvirToMcritRatio >= 1.0)
    print(cold_regime)

    # Apply dilution to both regimes
    cold_indices = np.where(cold_regime)[0]
    hot_indices = np.where(hot_regime)[0]
    
    if len(cold_indices) > dilute:
        cold_indices = sample(list(cold_indices), dilute)
    if len(hot_indices) > dilute:
        hot_indices = sample(list(hot_indices), dilute)

    plt.scatter(CriticalMassDB06[cold_indices], Mvir[cold_indices], 
            c='blue', alpha=0.6, s=5, label='Cold Streams', zorder=10)
    plt.scatter(CriticalMassDB06[hot_indices], Mvir[hot_indices], 
            c='red', alpha=0.3, s=5, label='Shock Heated', zorder=5)

    plt.xlabel('Critical Mass Mcrit [M☉]')
    plt.ylabel('Virial Mass Mvir [M☉]')
    # plt.title('Dekel & Birnboim Regime Diagram')
    plt.legend()
    plt.grid(True, alpha=0.3)
    # plt.xlim(1e10, 1e15)
    # plt.ylim(1e10, 1e15)
    plt.savefig(OutputDir + '21.dekel_regime_diagram' + OutputFormat, bbox_inches='tight')

    # -------------------------------------------------------

    # 2. Cold vs hot inflow fractions
    plt.figure()

    # Calculate total inflow and cold fraction
    total_inflow = ColdInflowMass + HotInflowMass
    cold_fraction = np.zeros_like(ColdInflowMass)
    mask = total_inflow > 0
    cold_fraction[mask] = ColdInflowMass[mask] / total_inflow[mask]

    # Filter for galaxies with significant inflow
    has_inflow = centrals & (total_inflow > 1e8)

    # Bin by halo mass
    mass_bins = np.logspace(10, 14, 20)
    mass_centers = (mass_bins[1:] * mass_bins[:-1])**0.5

    mean_cold_frac = []
    std_cold_frac = []

    for i in range(len(mass_bins)-1):
        in_bin = has_inflow & (Mvir >= mass_bins[i]) & (Mvir < mass_bins[i+1])
        if np.sum(in_bin) > 5:  # Need at least 5 galaxies
            mean_cold_frac.append(np.mean(cold_fraction[in_bin]))
            std_cold_frac.append(np.std(cold_fraction[in_bin]))
        else:
            mean_cold_frac.append(np.nan)
            std_cold_frac.append(np.nan)

    plt.errorbar(mass_centers, mean_cold_frac, yerr=std_cold_frac, 
                marker='o', capsize=5, linewidth=2)
    plt.axhline(y=0.5, color='k', linestyle='--', alpha=0.5, label='50% cold')
    plt.xlabel('Virial Mass [M☉]')
    plt.ylabel('Cold Inflow Fraction')
    # plt.title('Cold Stream Efficiency vs Halo Mass')
    plt.xscale('log')
    plt.ylim(0, 1)
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(OutputDir + '22.cold_inflow_fraction' + OutputFormat, bbox_inches='tight')

    # -------------------------------------------------------

    plt.figure(figsize=(12, 5))

    # Calculate specific star formation rate
    SFR = SfrDisk + SfrBulge
    sSFR = np.log10(SFR / StellarMass + 1e-12)  # Add small number to avoid log(0)

    # Filter star-forming galaxies
    sf_gals = centrals & (StellarMass > 1e9) & (SFR > 1e-3)

    plt.subplot(1, 2, 1)
    # Plot sSFR vs stellar mass, colored by regime
    cold_sf = sf_gals & (MvirToMcritRatio < 1.0)
    hot_sf = sf_gals & (MvirToMcritRatio > 1.0)

    # Apply dilution to both regimes
    cold_sf_indices = np.where(cold_sf)[0]
    hot_sf_indices = np.where(hot_sf)[0]
    
    if len(cold_sf_indices) > dilute:
        cold_sf_indices = sample(list(cold_sf_indices), dilute)
    if len(hot_sf_indices) > dilute:
        hot_sf_indices = sample(list(hot_sf_indices), dilute)

    plt.scatter(np.log10(StellarMass[cold_sf_indices]), sSFR[cold_sf_indices], 
            c='blue', alpha=0.6, s=5, label='Cold Streams', zorder=10)
    plt.scatter(np.log10(StellarMass[hot_sf_indices]), sSFR[hot_sf_indices], 
            c='red', alpha=0.3, s=5, label='Shock Heated', zorder=5)

    plt.xlabel('log Stellar Mass [M☉]')
    plt.ylabel('log sSFR [yr⁻¹]')
    # plt.title('Star Formation vs Infall Mode')
    plt.legend()
    plt.grid(True, alpha=0.3)

    plt.subplot(1, 2, 2)
    # Cold gas fraction
    cold_gas_frac = ColdGas / (StellarMass + ColdGas + 1e-12)

    plt.scatter(np.log10(Mvir[cold_sf_indices]), cold_gas_frac[cold_sf_indices], 
            c='blue', alpha=0.6, s=5, label='Cold Streams')
    plt.scatter(np.log10(Mvir[hot_sf_indices]), cold_gas_frac[hot_sf_indices], 
            c='red', alpha=0.3, s=5, label='Shock Heated')

    plt.xlabel('log Virial Mass [M☉]')
    plt.ylabel('Cold Gas Fraction')
    # plt.title('Gas Content vs Infall Mode')
    plt.legend()
    plt.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(OutputDir + '23.infall_impact_on_sf' + OutputFormat, bbox_inches='tight')

    # -------------------------------------------------------

    # 4. Molecular vs atomic gas in different regimes
    plt.figure()

    # Calculate H2 fraction
    h2_fraction = H2Gas / (H1Gas + H2Gas + 1e-12)
    has_gas = centrals & (ColdGas > 1e8)

    cold_gas = has_gas & (MvirToMcritRatio < 1.0)
    hot_gas = has_gas & (MvirToMcritRatio > 1.0)

    # Apply dilution to both regimes
    cold_gas_indices = np.where(cold_gas)[0]
    hot_gas_indices = np.where(hot_gas)[0]
    
    if len(cold_gas_indices) > dilute:
        cold_gas_indices = sample(list(cold_gas_indices), dilute)
    if len(hot_gas_indices) > dilute:
        hot_gas_indices = sample(list(hot_gas_indices), dilute)

    plt.scatter(np.log10(StellarMass[cold_gas_indices]), h2_fraction[cold_gas_indices], 
            c='blue', alpha=0.6, s=5, label='Cold Streams', zorder=10)
    plt.scatter(np.log10(StellarMass[hot_gas_indices]), h2_fraction[hot_gas_indices], 
            c='red', alpha=0.6, s=5, label='Shock Heated', zorder=5)

    plt.xlabel('log Stellar Mass [M☉]')
    plt.ylabel('H₂ Fraction')
    # plt.title('Molecular Gas vs Infall Mode')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.ylim(0, 1)
    plt.savefig(OutputDir + '24.h2_fraction_by_regime' + OutputFormat, bbox_inches='tight')

    # -------------------------------------------------------

    # 5. Distribution of Mvir/Mcrit ratios
    plt.figure()

    # Filter out invalid values (<=0) before taking log10
    valid_ratios = centrals & (MvirToMcritRatio > 0)
    
    # Histogram of mass ratios
    plt.hist(np.log10(MvirToMcritRatio[valid_ratios]), bins=50, alpha=0.7, 
            density=True, label='All Centrals')
    plt.axvline(x=0, color='k', linestyle='--', label='Mvir = Mcrit')

    plt.xlabel('log(Mvir/Mcrit)')
    plt.ylabel('Probability Density')
    # plt.title('Distribution of Mass Ratios')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.savefig(OutputDir + '25.mass_ratio_distribution' + OutputFormat, bbox_inches='tight')
    # plt.show()

    # Print some statistics
    valid_central_ratios = MvirToMcritRatio[centrals & (MvirToMcritRatio > 0)]
    print(f"Fraction in cold stream regime: {np.sum(MvirToMcritRatio[centrals] < 1.0) / np.sum(centrals):.3f}")
    print(f"Median Mvir/Mcrit ratio (valid values only): {np.median(valid_central_ratios):.3f}")
    print(f"Number of galaxies with invalid (<=0) ratios: {np.sum(centrals & (MvirToMcritRatio <= 0))}")

    # -------------------------------------------------------
    print('Reincorporation diagnostics')
    print('Reincorporated gas:', ReincorporatedGas)
    print('Reincorporated log 10 gas:', np.log10(ReincorporatedGas))
    print('Reincorporated gas fraction:', ReincorporatedGas / (StellarMass + ColdGas + 1e-12))

    plt.figure()  # New figure

    w = np.where((Type == 0) & (StellarMass > 1.0e7) & (ReincorporatedGas > 0))[0]  # Central galaxies only
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    log10_reincorporated = np.log10(ReincorporatedGas)[w]
    log10_stellar_mass = np.log10(StellarMass)[w]
    log10_mvir = np.log10(Mvir)[w]

    sc = plt.scatter(
        log10_stellar_mass, log10_reincorporated,
        c=log10_mvir, cmap='viridis', alpha=0.6, s=5, label='Reincorporated Gas'
    )
    cb = plt.colorbar(sc)
    cb.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} M_{\mathrm{reincorporated}}\ (M_{\odot})$')
    # plt.title('Reincorporated Gas vs Stellar Mass')
    plt.legend()
    plt.xlim(6, 12)
    plt.ylim(0, 12)

    outputFile = OutputDir + '30.reincorporation_diagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting H2 surface density vs SFR surface density')

    plt.figure()  # New figure
    # Σ_H2 in M_sun/pc^2, Σ_SFR in M_sun/yr/kpc^2
    
    w = np.where((Mvir > 0.0) & (H2Gas > 0.0) & (SFR > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    sigma_H2 = H2Gas[w] / (2 * np.pi * DiskRadius[w]**2 * 1e6)  # DiskRadius in kpc, area in pc^2
    sigma_SFR = SFR[w] / (2 * np.pi * DiskRadius[w]**2)          # area in kpc^2
    log10_sigma_H2 = np.log10(sigma_H2)
    log10_sigma_SFR = np.log10(sigma_SFR)
    # Color by Mvir (virial mass)
    sc = plt.scatter(log10_sigma_H2, log10_sigma_SFR, c=np.log10(Mvir[w]), cmap='plasma', alpha=0.6, s=5, label='SAGE 2.0')
    cb = plt.colorbar(sc)
    cb.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    # Add canonical Kennicutt-Schmidt law (Kennicutt 1998): log(Sigma_SFR) = 1.4*log(Sigma_gas) - 3.6
    sigma_gas_range = np.linspace(0.5, 12.5, 100)
    ks_law = 1.4 * sigma_gas_range - 3.6
    plt.plot(sigma_gas_range, ks_law, 'k--', label='Kennicutt (1998)')

    gas_range = np.logspace(0, 12.5, 100)
        
    # Bigiel et al. (2008) - resolved regions in nearby galaxies
    # Σ_SFR = 1.6e-3 × (Σ_H2)^1.0
    ks_bigiel = np.log10(1.6e-3) + 1.0 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_bigiel, 'k:', linewidth=2.5, alpha=0.8, 
            label='Bigiel+ (2008) - resolved', zorder=2)
    
    # Schruba et al. (2011) - different normalization
    # Σ_SFR = 2.1e-3 × (Σ_H2)^1.0
    ks_schruba = np.log10(2.1e-3) + 1.0 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_schruba, 'k--', linewidth=2, alpha=0.6, 
            label='Schruba+ (2011)', zorder=2)
            
    # Leroy et al. (2013) - whole galaxy integrated
    # Σ_SFR = 1.4e-3 × (Σ_H2)^1.1
    ks_leroy = np.log10(1.4e-3) + 1.1 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_leroy, 'k-', linewidth=2, alpha=0.7, 
            label='Leroy+ (2013) - galaxies', zorder=2)
            
    # Saintonge et al. (2011) - COLD GASS survey
    # Σ_SFR = 1.0e-3 × (Σ_H2)^0.96
    ks_saintonge = np.log10(1.0e-3) + 0.96 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_saintonge, 'k-.', linewidth=1.5, alpha=0.5, 
            label='Saintonge+ (2011)', zorder=2)
    
    plt.xlabel(r'$\log_{10} \Sigma_{\mathrm{H}_2}\ (M_{\odot}/\mathrm{pc}^2)$')
    plt.ylabel(r'$\log_{10} \Sigma_{\mathrm{SFR}}\ (M_{\odot}/\mathrm{pc}^2)$')
    # # plt.title('H$_2$ Surface Density vs SFR Surface Density (K-S Law)')
    plt.legend(loc='lower right', fontsize='small', frameon=False)
    plt.xlim(2, 8.5)
    plt.ylim(-1.5, 6)
    # plt.grid(True, alpha=0.3)
    outputFile = OutputDir + '31.h2_vs_sfr_surface_density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting H1 surface density vs SFR surface density')

    plt.figure()  # New figure
    # Σ_H1 in M_sun/pc^2, Σ_SFR in M_sun/yr/kpc^2
    
    w = np.where((Mvir > 0.0) & (ColdGas > 0.0) & (SFR > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    sigma_H1 = (ColdGas[w] - H2Gas[w]) / (2 * np.pi * DiskRadius[w]**2 * 1e6)  # DiskRadius in kpc, area in pc^2
    sigma_SFR = SFR[w] / (2 * np.pi * DiskRadius[w]**2)          # area in kpc^2
    log10_sigma_H1 = np.log10(sigma_H1)
    log10_sigma_SFR = np.log10(sigma_SFR)
    # Color by Mvir (virial mass)
    sc = plt.scatter(log10_sigma_H1, log10_sigma_SFR, c=np.log10(Mvir[w]), cmap='plasma', alpha=0.6, s=5, label='SAGE 2.0')
    cb = plt.colorbar(sc)
    cb.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    
    plt.xlabel(r'$\log_{10} \Sigma_{\mathrm{H}_I}\ (M_{\odot}/\mathrm{pc}^2)$')
    plt.ylabel(r'$\log_{10} \Sigma_{\mathrm{SFR}}\ (M_{\odot}/\mathrm{pc}^2)$')
    # # plt.title('H$_2$ Surface Density vs SFR Surface Density (K-S Law)')
    plt.legend(loc='lower right', fontsize='small', frameon=False)
    plt.xlim(2, 8.5)
    plt.ylim(-1.5, 6)
    # plt.grid(True, alpha=0.3)
    outputFile = OutputDir + '32.h1_vs_sfr_surface_density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting gas surface density vs SFR surface density')

    plt.figure()  # New figure
    # Σ_H2 in M_sun/pc^2, Σ_SFR in M_sun/yr/kpc^2
    
    w = np.where((Mvir > 0.0) & (H2Gas > 0.0) & (SFR > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    sigma_h2 = H2Gas[w] / (2 * np.pi * DiskRadius[w]**2 * 1e6)  # DiskRadius in kpc, area in pc^2
    sigma_h1 = H1Gas[w] / (2 * np.pi * DiskRadius[w]**2 * 1e6)  # DiskRadius in kpc, area in pc^2
    sigma_gas = sigma_h2 + sigma_h1 / (2 * np.pi * DiskRadius[w]**2 * 1e6)  # Total gas surface density
    sigma_SFR = SFR[w] / (2 * np.pi * DiskRadius[w]**2)          # area in kpc^2
    log10_sigma_gas = np.log10(sigma_gas)
    log10_sigma_h2 = np.log10(sigma_h2)
    log10_sigma_h1 = np.log10(sigma_h1)
    log10_sigma_SFR = np.log10(sigma_SFR)
    # Color by Mvir (virial mass)
    sc = plt.scatter(log10_sigma_gas, log10_sigma_SFR, c=np.log10(Mvir[w]), cmap='plasma', alpha=0.6, s=5, label='SAGE 2.0')
    # sc1 = plt.scatter(log10_sigma_h1, log10_sigma_SFR, c='r', alpha=0.6, s=5, label='SAGE 2.0')
    # sc2 = plt.scatter(log10_sigma_h2, log10_sigma_SFR, c=np.log10(Mvir[w]), cmap='plasma', alpha=0.6, s=5, label='SAGE 2.0')
    cb = plt.colorbar(sc)
    cb.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')

    gas_range = np.logspace(0, 12.5, 100)
        
    # Bigiel et al. (2008) - resolved regions in nearby galaxies
    # Σ_SFR = 1.6e-3 × (Σ_H2)^1.0
    ks_bigiel = np.log10(1.6e-3) + 1.0 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_bigiel, 'k:', linewidth=2.5, alpha=0.8, 
            label='Bigiel+ (2008) - resolved', zorder=2)
    
    # Schruba et al. (2011) - different normalization
    # Σ_SFR = 2.1e-3 × (Σ_H2)^1.0
    ks_schruba = np.log10(2.1e-3) + 1.0 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_schruba, 'k--', linewidth=2, alpha=0.6, 
            label='Schruba+ (2011)', zorder=2)
            
    # Leroy et al. (2013) - whole galaxy integrated
    # Σ_SFR = 1.4e-3 × (Σ_H2)^1.1
    ks_leroy = np.log10(1.4e-3) + 1.1 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_leroy, 'k-', linewidth=2, alpha=0.7, 
            label='Leroy+ (2013) - galaxies', zorder=2)
            
    # Saintonge et al. (2011) - COLD GASS survey
    # Σ_SFR = 1.0e-3 × (Σ_H2)^0.96
    ks_saintonge = np.log10(1.0e-3) + 0.96 * np.log10(gas_range)
    plt.plot(np.log10(gas_range), ks_saintonge, 'k-.', linewidth=1.5, alpha=0.5, 
            label='Saintonge+ (2011)', zorder=2)

    plt.xlabel(r'$\log_{10} \Sigma_{\mathrm{H}_I\ +\ \mathrm{H}_2}\ (M_{\odot}/\mathrm{pc}^2)$')
    plt.ylabel(r'$\log_{10} \Sigma_{\mathrm{SFR}}\ (M_{\odot}/\mathrm{pc}^2)$')
    # # plt.title('H$_2$ Surface Density vs SFR Surface Density (K-S Law)')
    plt.legend(loc='lower right', fontsize='small', frameon=False)
    plt.xlim(2, 8.5)
    plt.ylim(-1.5, 6)
    # plt.grid(True, alpha=0.3)
    outputFile = OutputDir + '32.h1andh2_vs_sfr_surface_density' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Size-Mass relation for galaxies')

    plt.figure()  # New figure

    w = np.where((Mvir > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_disk_radius = np.log10(DiskRadius[w] / 0.001) 
    # Color by Mvir (virial mass)
    sc = plt.scatter(log10_stellar_mass, log10_disk_radius, c=np.log10(Vvir[w]), cmap='plasma', alpha=0.6, s=5)
    cb = plt.colorbar(sc)
    cb.set_label(r'$\log_{10} V_{\mathrm{vir}}\ (\mathrm{km}\ s^{-1})$')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$')
    # plt.title('Size-Mass Relation for Galaxies')
    plt.legend()
    plt.xlim(6, 12)
    plt.ylim(-1, 2.5)

    outputFile = OutputDir + '33.size_mass_relation' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting Size-Mass relation split by star-forming and quiescent')

    plt.figure()  # New figure

    w = np.where((Mvir > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    log10_stellar_mass = np.log10(StellarMass[w])
    log10_disk_radius = np.log10(DiskRadius[w] / 0.001)
    log10_disk_radius_quiescent = np.log10(DiskRadius[w] / 0.001 / 1.67)
    SFR = SfrDisk[w] + SfrBulge[w]
    sSFR = np.full_like(SFR, -99.0)
    mask = (StellarMass[w] > 0)
    sSFR[mask] = np.log10(SFR[mask] / StellarMass[w][mask])

    star_forming = sSFR > sSFRcut
    quiescent = sSFR <= sSFRcut


    plt.scatter(log10_stellar_mass[star_forming], log10_disk_radius[star_forming], c='darkblue', s=5, alpha=0.1)
    plt.scatter(log10_stellar_mass[quiescent], log10_disk_radius_quiescent[quiescent], c='darkred', s=5, alpha=0.1)

    # Add median lines for both populations

    def median_and_sigma(x, y, bins):
        bin_centers = []
        medians = []
        sig_low = []
        sig_high = []
        for i in range(len(bins)-1):
            mask = (x >= bins[i]) & (x < bins[i+1])
            if np.any(mask):
                bin_centers.append(0.5*(bins[i]+bins[i+1]))
                medians.append(np.median(y[mask]))
                sig_low.append(np.percentile(y[mask], 16))
                sig_high.append(np.percentile(y[mask], 84))
        return (np.array(bin_centers), np.array(medians), np.array(sig_low), np.array(sig_high))

    bins = np.arange(6, 12.1, 0.3)

    # Star-forming median and 1-sigma
    x_sf, y_sf, y_sf_low, y_sf_high = median_and_sigma(log10_stellar_mass[star_forming], log10_disk_radius[star_forming], bins)
    plt.plot(x_sf, y_sf, c='darkblue', lw=2.5, label='Median SF')
    plt.fill_between(x_sf, y_sf_low, y_sf_high, color='darkblue', alpha=0.18, label='SF 1$\sigma$')
    # Quiescent median and 1-sigma
    x_q, y_q, y_q_low, y_q_high = median_and_sigma(log10_stellar_mass[quiescent], log10_disk_radius_quiescent[quiescent], bins)
    plt.plot(x_q, y_q, c='darkred', lw=2.5, label='Median Q')
    plt.fill_between(x_q, y_q_low, y_q_high, color='darkred', alpha=0.18, label='Q 1$\sigma$')

    # Approximate Shen+2003 relation
    M_star = np.logspace(6, 12, 100)
    R_shen_sf = 3.0 * (M_star/1e10)**0.14  # Star-forming
    R_shen_q = 1.5 * (M_star/1e10)**0.12   # Quiescent (smaller, shallower)

    plt.plot(np.log10(M_star), np.log10(R_shen_sf), 'b-', linewidth=2, label='Shen+03 SF')
    plt.plot(np.log10(M_star), np.log10(R_shen_q), 'r-', linewidth=2, label='Shen+03 Q')

    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.ylabel(r'$\log_{10} R_{\mathrm{disk}}\ (\mathrm{kpc})$')
    # # plt.title('Size-Mass Relation: Star Forming (blue) vs Quiescent (red)')
    plt.legend(loc='upper left', fontsize='small', frameon=False)
    plt.xlim(6, 12)
    plt.ylim(-1, 2.5)

    outputFile = OutputDir + '33.size_mass_relation_split' + OutputFormat
    plt.savefig(outputFile)
    print('Saved file to', outputFile, '\n')
    plt.close()