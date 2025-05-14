#!/usr/bin/env python
import math
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
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0  # Fraction of the full volume output by the model

# Plotting options
whichimf = 1        # 0=Slapeter; 1=Chabrier
dilute = 10000       # Number of galaxies to plot in scatter plots
sSFRcut = -11.0     # Divide quiescent from star forming galaxies

OutputFormat = '.png'
plt.rcParams["figure.figsize"] = (8.34,6.25)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 14


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
    H1Gas = read_hdf(snap_num = Snapshot, param = 'H1_gas') * 1.0e10 / Hubble_h
    #print(H1Gas)
    H2Gas = read_hdf(snap_num = Snapshot, param = 'H2_gas') * 1.0e10 / Hubble_h
    #print(H2Gas)
    MetalsColdGas = read_hdf(snap_num = Snapshot, param = 'MetalsColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num = Snapshot, param = 'HotGas') * 1.0e10 / Hubble_h
    EjectedMass = read_hdf(snap_num = Snapshot, param = 'EjectedMass') * 1.0e10 / Hubble_h
    IntraClusterStars = read_hdf(snap_num = Snapshot, param = 'IntraClusterStars') * 1.0e10 / Hubble_h
    InfallMvir = read_hdf(snap_num = Snapshot, param = 'infallMvir') * 1.0e10 / Hubble_h
    outflowrate = read_hdf(snap_num = Snapshot, param = 'OutflowRate') * 1.0e10 / Hubble_h
    CGM = read_hdf(snap_num = Snapshot, param = 'CGMgas') * 1.0e10 / Hubble_h
    #print(np.log10(CGM))

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
    if(len(w) > dilute): w2 = sample(list(range(len(w))), dilute)
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
    Baryons = StellarMass + ColdGas + HotGas + EjectedMass + IntraClusterStars + BlackHoleMass + CGM

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
    MeanCGM = []

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
            group_cgm = CGM[central_groups]
            
            # Sum quantities per central galaxy
            unique_centrals_in_bin = np.unique(CentralGalaxyIndex[central_groups])
            baryon_sums = np.zeros(len(unique_centrals_in_bin))
            star_sums = np.zeros(len(unique_centrals_in_bin))
            cold_sums = np.zeros(len(unique_centrals_in_bin))
            hot_sums = np.zeros(len(unique_centrals_in_bin))
            ejected_sums = np.zeros(len(unique_centrals_in_bin))
            ics_sums = np.zeros(len(unique_centrals_in_bin))
            bh_sums = np.zeros(len(unique_centrals_in_bin))
            cgm_sums = np.zeros(len(unique_centrals_in_bin))
            
            for idx, central in enumerate(unique_centrals_in_bin):
                group_mask = CentralGalaxyIndex[central_groups] == central
                baryon_sums[idx] = np.sum(group_baryons[group_mask])
                star_sums[idx] = np.sum(group_stars[group_mask])
                cold_sums[idx] = np.sum(group_cold[group_mask])
                hot_sums[idx] = np.sum(group_hot[group_mask])
                ejected_sums[idx] = np.sum(group_ejected[group_mask])
                ics_sums[idx] = np.sum(group_ics[group_mask])
                bh_sums[idx] = np.sum(group_bh[group_mask])
                cgm_sums[idx] = np.sum(group_cgm[group_mask])
            
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
            cgm_fractions = cgm_sums / central_halos
            
            # Filter out any NaN or inf values before calculating statistics
            valid_indices = np.isfinite(baryon_fractions)
            if np.sum(valid_indices) > 0:  # Only proceed if we have valid data
                # Store means and variances with safety checks
                MeanCentralHaloMass.append(np.mean(np.log10(central_halos[valid_indices])))
                MeanBaryonFraction.append(np.mean(baryon_fractions[valid_indices]))
                
                # Calculate upper and lower bounds with proper limits
                variance = np.var(baryon_fractions[valid_indices]) if len(baryon_fractions[valid_indices]) > 1 else 0
                MeanBaryonFractionU.append(np.mean(baryon_fractions[valid_indices]) + variance)
                MeanBaryonFractionL.append(max(0, np.mean(baryon_fractions[valid_indices]) - variance))
                
                # Component fractions
                if np.sum(np.isfinite(star_fractions)) > 0:
                    MeanStars.append(np.mean(star_fractions[np.isfinite(star_fractions)]))
                else:
                    MeanStars.append(0)
                    
                if np.sum(np.isfinite(cold_fractions)) > 0:
                    MeanCold.append(np.mean(cold_fractions[np.isfinite(cold_fractions)]))
                else:
                    MeanCold.append(0)
                    
                if np.sum(np.isfinite(hot_fractions)) > 0:
                    MeanHot.append(np.mean(hot_fractions[np.isfinite(hot_fractions)]))
                else:
                    MeanHot.append(0)
                    
                if np.sum(np.isfinite(ejected_fractions)) > 0:
                    MeanEjected.append(np.mean(ejected_fractions[np.isfinite(ejected_fractions)]))
                else:
                    MeanEjected.append(0)
                    
                if np.sum(np.isfinite(ics_fractions)) > 0:
                    MeanICS.append(np.mean(ics_fractions[np.isfinite(ics_fractions)]))
                else:
                    MeanICS.append(0)
                    
                if np.sum(np.isfinite(bh_fractions)) > 0:
                    MeanBH.append(np.mean(bh_fractions[np.isfinite(bh_fractions)]))
                else:
                    MeanBH.append(0)

                if np.sum(np.isfinite(cgm_fractions)) > 0:
                    MeanCGM.append(np.mean(cgm_fractions[np.isfinite(cgm_fractions)]))
                else:
                    MeanCGM.append(0)

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
    MeanBH = np.array(MeanBH)
    MeanCGM = np.array(MeanCGM)

    # Make sure upper bound isn't too high (cap at reasonable cosmological baryon fraction)
    MeanBaryonFractionU = np.minimum(MeanBaryonFractionU, 0.2)

    # Ensure all arrays have valid values
    mask = np.isfinite(MeanCentralHaloMass) & np.isfinite(MeanBaryonFraction) & \
        np.isfinite(MeanBaryonFractionU) & np.isfinite(MeanBaryonFractionL)

    # Apply mask to all arrays
    if np.sum(mask) > 0:  # Only proceed if we have valid data
        MeanCentralHaloMass = MeanCentralHaloMass[mask]
        MeanBaryonFraction = MeanBaryonFraction[mask]
        MeanBaryonFractionU = MeanBaryonFractionU[mask]
        MeanBaryonFractionL = MeanBaryonFractionL[mask]
        
        # Now do all the plotting
        plt.plot(MeanCentralHaloMass, MeanBaryonFraction, 'k-', label='TOTAL')
        try:
            plt.fill_between(MeanCentralHaloMass, MeanBaryonFractionU, MeanBaryonFractionL,
                facecolor='purple', alpha=0.25, label='TOTAL')
        except:
            print("Warning: Could not create fill_between for baryon fraction")

        # Make a separate mask for component arrays
        comp_mask = np.isfinite(MeanStars) & np.isfinite(MeanCold) & np.isfinite(MeanHot) & \
                    np.isfinite(MeanEjected) & np.isfinite(MeanICS) & np.isfinite(MeanCGM) 
        
        if np.sum(comp_mask) > 0:
            # Filter component arrays with their mask
            MeanStars_masked = MeanStars[comp_mask]
            MeanCold_masked = MeanCold[comp_mask]
            MeanHot_masked = MeanHot[comp_mask]
            MeanEjected_masked = MeanEjected[comp_mask]
            MeanICS_masked = MeanICS[comp_mask]
            MeanCGM_masked = MeanCGM[comp_mask]
            MeanCentralHaloMass_comp = MeanCentralHaloMass[comp_mask]
            
            # Plot components
            plt.plot(MeanCentralHaloMass_comp, MeanStars_masked, 'k--', label='Stars')
            plt.plot(MeanCentralHaloMass_comp, MeanCold_masked, label='Cold', color='blue')
            plt.plot(MeanCentralHaloMass_comp, MeanHot_masked, label='Hot', color='red')
            plt.plot(MeanCentralHaloMass_comp, MeanEjected_masked, label='Ejected', color='green')
            plt.plot(MeanCentralHaloMass_comp, MeanICS_masked, label='ICS', color='yellow')
            plt.plot(MeanCentralHaloMass_comp, MeanCGM_masked, label='CGM', color='magenta')

    plt.xlabel(r'$\mathrm{Central}\ \log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    plt.ylabel(r'$\mathrm{Baryon\ Fraction}$')

    ax.xaxis.set_minor_locator(plt.MultipleLocator(0.05))
    ax.yaxis.set_minor_locator(plt.MultipleLocator(0.25))

    plt.axis([10.8, 15.0, 0.0, 0.2])

    leg = plt.legend()
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
    plt.scatter(HaloMass, np.log10(EjectedMass[w]), marker='o', s=0.3, color='green', alpha=0.5, label='Ejected gas')
    plt.scatter(HaloMass, np.log10(IntraClusterStars[w]), marker='o', s=10, color='yellow', alpha=0.5, label='Intracluster stars')
    plt.scatter(HaloMass, np.log10(CGM[w]), marker='o', s=0.3, color='magenta', alpha=0.5, label='CGM')

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
    if(len(w) > dilute): w = sample(list((w)), dilute)

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

    plt.figure(figsize=(10, 10))  # Adjust figure size as needed
  
    w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
    if(len(w) > dilute): w = sample(list((w)), dilute)

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
    if(len(w) > dilute): w = sample(list((w)), dilute)

    xx = Posx[w]
    yy = Posy[w]

    h, xedges, yedges = np.histogram2d(xx, yy, bins=100)
    plt.imshow(h.T, origin='lower', extent=[0, BoxSize, 0, BoxSize], 
            aspect='equal', cmap='gist_heat')
    #plt.colorbar(label='Number of galaxies')
    plt.xlabel('x (Mpc/h)')
    plt.ylabel('y (Mpc/h)')
    plt.title('Galaxy Number Density')

    # Second subplot - H2 gas density
    plt.subplot(132)  # 1 row, 2 cols, plot 2
    w = np.where((Mvir > 0.0) & (H2Gas > 0.0))[0]
    if(len(w) > dilute): w = sample(list((w)), dilute)

    xx = Posx[w]
    yy = Posy[w]
    weights = np.log10(H2Gas[w])  # Use H2 gas mass as weights

    h, xedges, yedges = np.histogram2d(xx, yy, bins=100, weights=weights)
    plt.imshow(h.T, origin='lower', extent=[0, BoxSize, 0, BoxSize], 
            aspect='equal', cmap='CMRmap')
    #plt.colorbar(label=r'$log_{10}\ H^2\ M_{\odot}/h$')
    plt.xlabel('x (Mpc/h)')
    plt.ylabel('y (Mpc/h)')
    plt.title('H2 Gas Density')

    # Third subplot - H1 gas density
    plt.subplot(133)  # 1 row, 2 cols, plot 3
    w = np.where((Mvir > 0.0) & (H1Gas > 0.0))[0]
    if(len(w) > dilute): w = sample(list((w)), dilute)

    xx = Posx[w]
    yy = Posy[w]
    weights = np.log10(H1Gas[w])  # Use H1 gas mass as weights

    h, xedges, yedges = np.histogram2d(xx, yy, bins=100, weights=weights)
    plt.imshow(h.T, origin='lower', extent=[0, BoxSize, 0, BoxSize], 
            aspect='equal', cmap='CMRmap')
    #plt.colorbar(label=r'$log_{10}\ H^1\ M_{\odot}/h$')
    plt.xlabel('x (Mpc/h)')
    plt.ylabel('y (Mpc/h)')
    plt.title('H1 Gas Density')

    plt.tight_layout()

    outputFile = OutputDir + '16.DensityPlotandH2andH1' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    print('Checking for holes in the galaxy distribution')

    w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
    if(len(w) > dilute): w = sample(list((w)), dilute)

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

    plt.figure(figsize=(10, 8))  # New figure

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
    plt.title('Virial Velocity vs Stellar Mass (Centrals)')

    # Add Tully-Fisher reference line
    mass_tf = np.arange(8, 12, 0.1)
    vel_tf = 10**(0.27 * mass_tf - 0.55)  # Simple Tully-Fisher relation
    plt.plot(mass_tf, vel_tf, 'r--', lw=2, label='Tully-Fisher Relation')
    plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '15.VelocityDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting galaxy ejection diagnostics')

    plt.figure(figsize=(10, 8))  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Type == 0) & (Vmax > 0))[0]  # Central galaxies only
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    sat = np.where((Type == 1) & (Vmax > 0))[0]  
    if(len(sat) > dilute): sat = sample(list(range(len(sat))), dilute)

    mass = np.log10(StellarMass[w])
    sats = np.log10(StellarMass[sat])
    ejected = np.log10(EjectedMass[w])

    # Color points by halo mass
    halo_mass = np.log10(Mvir[w])
    sc = plt.scatter(mass, ejected, c=halo_mass, cmap='viridis', s=5, alpha=0.7, label='Centrals')
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')

    plt.scatter(sats, np.log10(EjectedMass[sat]), marker='*', s=250, c='k', alpha=0.5, label='Satellites')

    plt.ylabel(r'$\log_{10} M_{\mathrm{ejected}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    plt.title('Ejected Mass vs Stellar Mass')

    plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '16.EjectionDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting galaxy infall diagnostics')

    plt.figure(figsize=(10, 8))  # New figure

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
    cbar.set_label(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')

    plt.ylabel(r'$\log_{10} M_{\mathrm{infall}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
    plt.title('Infall Mass vs Halo Mass')

    plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '17.InfallDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting galaxy outflow diagnostics')

    plt.figure(figsize=(10, 8))  # New figure

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
    plt.title('Outflow rate vs Stellar Mass')

    plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '18.OutflowrateDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    # -------------------------------------------------------

    print('Plotting CGM outflow diagnostics')

    plt.figure(figsize=(10, 8))  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Vmax > 0))[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    mass = np.log10(Mvir[w])
    cgm = np.log10(CGM[w])
    cgm_frac = np.log10(CGM / 0.17 / Mvir)
    cgm_frac_filtered = cgm_frac[w]

    # Color points by halo mass
    halo_mass = np.log10(StellarMass[w])
    sc = plt.scatter(mass, cgm, c=halo_mass, cmap='viridis', s=5, alpha=0.7)
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{stellar}}\ (M_{\odot})$')

    plt.ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')
    #plt.axis([10.0, 12.0, -2.0, -0.5])
    plt.title('CGM vs Mvir')

    #plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '19.CGMDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

# -------------------------------------------------------

    print('Plotting CGM outflow diagnostics')

    plt.figure(figsize=(10, 8))  # New figure

    # First subplot: Vmax vs Stellar Mass
    ax1 = plt.subplot(111)  # Top plot

    w = np.where((Vmax > 0))[0]
    if(len(w) > dilute): w = sample(list(range(len(w))), dilute)

    mass = Vvir[w]
    cgm = np.log10(CGM[w])
    cgm_frac = np.log10(CGM / 0.17 / Mvir)
    cgm_frac_filtered = cgm_frac[w]

    # Color points by halo mass
    halo_mass = np.log10(Mvir[w])
    sc = plt.scatter(mass, cgm, c=halo_mass, cmap='viridis', s=5, alpha=0.7)
    cbar = plt.colorbar(sc)
    cbar.set_label(r'$\log_{10} M_{\mathrm{vir}}\ (M_{\odot})$')

    plt.ylabel(r'$\log_{10} M_{\mathrm{CGM}}\ (M_{\odot})$')
    plt.xlabel(r'$\log_{10} V_{\mathrm{vir}}\ (M_{\odot})$')
    #plt.axis([10.0, 12.0, -2.0, -0.5])
    plt.title('CGM vs Vvir')

    #plt.legend(loc='upper left')

    plt.tight_layout()

    outputFile = OutputDir + '20.CGMDiagnostics' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()