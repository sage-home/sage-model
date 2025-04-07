#!/bin/bash

# Routines used for reading and plotting data (e.g. used by plot_constraints_z0.py).  Many of these are copied from the arhstevens/Dirty-AstroPy GitHub repository.

import numpy as np # type: ignore
import matplotlib.pyplot as plt # type: ignore
import sys
import os
import h5py as h5 # type: ignore

def galdtype_darksage(Nannuli=30,Nage=1):
    floattype = np.float32
    
    # Account for whether age bins are used for stars or not
    if Nage>1:
        disc_arr_dim = (Nannuli,Nage)
        bulge_arr_type = (floattype, Nage)
    else:
        disc_arr_dim = Nannuli
        bulge_arr_type = floattype

    Galdesc_full = [
                ('BlackHoleMass'                , np.float32),                  
                ('BulgeMass'                    , np.float32),                  
                ('CentralGalaxyIndex'           , np.int32),                    
                ('CentralMvir'                  , np.float32),                  
                ('ColdGas'                      , np.float32),                  
                ('Cooling'                      , np.float32),                  
                ('DiskRadius'                   , np.float32),
                ('EjectedMass'                  , np.float32),                                      
                ('GalaxyIndex'                  , np.int64),                                       
                ('Heating'                      , np.float32),                  
                ('HotGas'                       , np.float32),                  
                ('IntraClusterStars'            , np.float32),                  
                ('Len'                          , np.int32),                                      
                ('MetalsBulgeMass'              , np.float32),                  
                ('MetalsColdGas'                , np.float32),                  
                ('MetalsEjectedMass'            , np.float32),                  
                ('MetalsHotGas'                 , np.float32),                  
                ('MetalsIntraClusterStars'      , np.float32),                  
                ('MetalsStellarMass'            , np.float32),
                ('Mvir'                         , np.float32), 
                ('OutflowRate'                  , np.float32),                 
                ('Pos'                          , (np.float32, 3)),
                ('QuasarModeBHaccretionMass'    , np.float32),             
                ('Rvir'                         , np.float32),
                ('SAGEHaloIndex'                , np.int32),
                ('SAGETreeIndex'                , np.int32),                               
                ('SfrBulge'                     , np.float32),                  
                ('SfrBulgeZ'                    , np.float32),                  
                ('SfrDisk'                      , np.float32),                  
                ('SfrDiskZ'                     , np.float32),
                ('SimulationHaloIndex'          , np.int32),
                ('SnapNum'                      , np.int32),                    
                ('Spin'                         , (np.float32, 3)),                  
                ('StellarMass'                  , np.float32),
                ('TimeOfLastMajorMerger'        , np.int32),
                ('TimeOfLastMinorMerger'        , np.int32),                                     
                ('Type'                         , np.int32),                     
                ('VelDisp'                      , np.float32),                 
                ('Vel'                          , (np.float32, 3)),
                ('Vmax'                         , np.float32),                               
                ('Vvir'                         , np.float32),                  
                ('dT'                           , np.float32),  
                ('infallMvir'                   , np.float32),
                ('infallVmax'                   , np.float32), 
	            ('infallVvir'                   , np.float32),                 
                ('mergeIntoID'                  , np.int32),                    
                ('mergeIntoSnapNum'             , np.int32),                    
                ('mergeType'                    , np.int32)
	            ]
                
    names = [Galdesc_full[i][0] for i in range(len(Galdesc_full))]
    formats = [Galdesc_full[i][1] for i in range(len(Galdesc_full))]
    Galdesc = np.dtype({'names':names, 'formats':formats}, align=True)
    return Galdesc



def darksage_out_single(fname, fields=[], Nannuli=30, Nage=1):
    # Read a single Dark Sage output file, returning all the galaxy data
    # fname is the full name for the file to read, including its path
    # fields is the list of fields you want to read in.  If empty, will read all fields.
    
    Galdesc = galdtype_darksage(Nannuli, Nage)
    
    fin = open(fname, 'rb')  # Open the file
    Ntrees = np.fromfile(fin,np.dtype(np.int32),1)  # Read number of trees in file
    NtotGals = np.fromfile(fin,np.dtype(np.int32),1)[0]  # Read number of gals in file.
    GalsPerTree = np.fromfile(fin, np.dtype((np.int32, Ntrees)),1) # Read the number of gals in each tree
    
    # could have been a bug in the write out
    if NtotGals==0:
        print('NtotGals =', NtotGals)
        print('GalsPerTree =', GalsPerTree)
        size_read_sofar = sys.getsizeof(Ntrees) + sys.getsizeof(NtotGals) + sys.getsizeof(GalsPerTree)
        file_size = os.stat(fname).st_size
        G_overhead_size = sys.getsizeof(np.empty(0,dtype=Galdesc))
        G_single_size = sys.getsizeof(np.empty(1,dtype=Galdesc)) - G_overhead_size
        size_left_less_overhead = file_size - size_read_sofar - G_overhead_size
        if size_left_less_overhead>0:
            print(size_left_less_overhead, G_single_size, size_left_less_overhead % G_single_size)
            NtotGals = int(size_left_less_overhead / G_single_size) + 1
            if NtotGals>0:
                print('NtotGals updated to', NtotGals, 'based on file size instead')
        
    G = np.fromfile(fin, Galdesc, NtotGals) # Read all the galaxy data
    fin.close()
    
    if NtotGals==0:
        print('Still no galaxies')
        return G

    if len(fields)==0:
        return G
    else:
        # create smaller galdtype that only requires the limited number of fields
        formats = []
        for field in fields:
            shape = G[field].shape
            if len(shape)==1:
                formats += [G[field].dtype]
            elif len(shape)==2:
                formats += [(G[field].dtype, shape[1])]
            else:
                formats += [(G[field].dtype, shape[1:])]
        Galdesc_reduced = np.dtype({'names':fields, 'formats':formats}, align=True)
        G_reduced = np.empty(NtotGals, dtype=Galdesc_reduced)
        G_reduced[0:NtotGals] = G[fields]
#        for field in fields: 
#            G_reduced[field] = G[field]
#            print(field, G[field], G_reduced[field])
        return G_reduced
    
     


def darksage_snap(fpre, filelist, fields=[], Nannuli=30, Nage=1):
    # Read full Dark Sage snapshot, going through each file and compiling into 1 array
    # fpre is the name of the file up until the _ before the file number
    # filelist contains all the file numbers you want to read in
    
    Galdesc = galdtype_darksage(Nannuli, Nage)
    NtotGalsSum = 0
    Gempty = np.empty(0, dtype=Galdesc)

    if len(fields)>0: 
        formats = []
        for field in fields:
            shape = Gempty[field].shape
            if len(shape)==1:
                formats += [Gempty[field].dtype]
            elif len(shape)==2:
                formats += [(Gempty[field].dtype, shape[1])]
            else:
                formats += [(Gempty[field].dtype, shape[1:])]
        Galdesc_reduced = np.dtype({'names':fields, 'formats':formats}, align=True)
        Gempty = np.empty(0, dtype=Galdesc_reduced)

    G_overhead_size = sys.getsizeof(Gempty)
    
    # First calculate the total number of galaxies that will fill the array
    bad_files = []
    for j, i in enumerate(filelist):
        try:
            fin = open(fpre+'_'+str(i), 'rb')
        except FileNotFoundError:
            print('Could not find file', i, 'skipping...')
            bad_files += [j]
            continue
        Ntrees = np.fromfile(fin,np.dtype(np.int32),1)  # Read number of trees in file
        NtotGals = np.fromfile(fin,np.dtype(np.int32),1)[0]
        
#        # could have been a bug in the write out
#        if NtotGals==0:
#            GalsPerTree = np.fromfile(fin, np.dtype((np.int32, Ntrees)),1)
#            size_read_sofar = sys.getsizeof(Ntrees) + sys.getsizeof(NtotGals) + sys.getsizeof(GalsPerTree)
#            file_size = os.stat(fpre+'_'+str(i)).st_size
#            G_single_size = sys.getsizeof(np.empty(1,dtype=Galdesc)) - G_overhead_size
#            size_left_less_overhead = file_size - size_read_sofar - G_overhead_size
#            if size_left_less_overhead>0:
#                assert(size_left_less_overhead % G_single_size)
#                NtotGals = int(size_left_less_overhead / G_single_size)
#                print('found', NtotGals, 'when file', i, ' claimed to have 0')
#            else:
#                print(NtotGals, 'galaxies for file', i)
        
        NtotGalsSum += NtotGals
        fin.close()

    print('NtotGalsSum =', NtotGalsSum)
#    print()
    print('Memory needed =', round((NtotGalsSum*(sys.getsizeof(np.empty(1,dtype=Gempty.dtype)) - G_overhead_size) + G_overhead_size) * 1e-9, 2), 'GB')
    G = np.empty(NtotGalsSum, dtype=Gempty.dtype) # Intialise the galaxy array
    NtotGalsSum = 0 # reset for next loop

    filelist = np.delete(filelist, bad_files)

    # Loop through files to fill in galaxy array
    for i in filelist:
#        print('reading file {0}'.format(i))
        fin = open(fpre+'_'+str(i), 'rb')
        Ntrees = np.fromfile(fin,np.dtype(np.int32),1)  # Read number of trees in file
        NtotGals = np.fromfile(fin,np.dtype(np.int32),1)[0]  # Read number of gals in file.
        GalsPerTree = np.fromfile(fin, np.dtype((np.int32, Ntrees)),1) # Read the number of gals in each tree
        G1 = np.fromfile(fin, Galdesc, NtotGals) # Read all the galaxy data
        fin.close()
#        if not NtotGals == len(G1[fields[0]]):
#            print('Oddity in NtotGals in file', i)
#            print('Ntrees, NtotGals, NtotGalsSum, len(G1) = ', Ntrees, NtotGals, NtotGalsSum, len(G1[fields[0]]))
#            print('GalsPerTree', GalsPerTree, '\n')
#        NtotGals = len(G1[fields[0]])
        if len(fields)>0:
            G[NtotGalsSum:NtotGalsSum+NtotGals]  = G1[fields]
#            for field in fields:
#                Ndim = len(G1[field].shape)
#                if Ndim==1:
#                    G[field][NtotGalsSum:NtotGalsSum+NtotGals] = G1[field]
#                elif Ndim==2:
#                    print(field)
#                    print(len(G1[field]))
#                    print(NtotGalsSum, NtotGals)
#                    print(G1[field])
#                    G[field][NtotGalsSum:NtotGalsSum+NtotGals,:] = G1[field][:,:]
#                elif Ndim==3:
#                    G[field][NtotGalsSum:NtotGalsSum+NtotGals,:,:] = G1[field][:,:,:]
#                else:
#                    print('field', field, 'unexpectedly has', Ndim, 'dimensions')

        else:
            G[NtotGalsSum:NtotGalsSum+NtotGals] = G1
        NtotGalsSum += NtotGals

    return G


def massfunction(mass, Lbox, range=[8,12.5], c='k', lw=2, ls='-', label='', ax=None, zo=2):
    masslog = np.log10(mass[(mass>0)*np.isfinite(mass)])
    N, edges = np.histogram(masslog, bins=np.arange(range[0],range[1]+0.2,0.2))
    binwidth = edges[1]-edges[0]
    x = edges[:-1] + binwidth/2
    y = N/(binwidth*Lbox**3)
    
    if ax is None: ax = plt.gca()
    
    if len(label)>0:
        ax.plot(x, y, ls, linewidth=lw, label=label, zorder=zo, color=c)
    else:
        ax.plot(x, y, ls, linewidth=lw, zorder=zo, color=c)


def schechter(phistar, Mstar, alpha, Mlog=False, range=[7,12], Npoints=2000, logM=None):
    if Mlog: Mstar = 10**Mstar
    if logM is None: logM = np.linspace(range[0],range[1],Npoints)
    M = 10**logM
    Phi = np.log(10.) * (phistar) * (M/Mstar)**(alpha+1) * np.exp(-M/Mstar)
    return Phi, logM


def stellar_massfunction_obsdata(h=0.678, ax=None, zo=1):
    B = np.array([
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
                  [11.95, 7.4764e-06, 7.4764e-06]
                  ], dtype=np.float32)
    if ax is None: ax = plt.gca()
    ax.fill_between(B[:,0]+np.log10(0.7**2)-np.log10(h**2), (B[:,1]+B[:,2])*h**3, (B[:,1]-B[:,2])*h**3, facecolor='purple', alpha=0.2, zorder=zo)
    ax.plot([1,1], [1,2], color='purple', linewidth=8, alpha=0.3, label=r'Baldry et al.~(2008)') # Just for the legend

    W17_data = np.array([[1.200e+01, 1.929e-05, 2.623e-05, 2.912e-05, 0.000e+00, 0.000e+00, 1.467e-05],
                     [1.185e+01, 2.448e-05, 1.783e-05, 2.178e-05, 1.973e-05, 2.280e-05, 4.893e-05],
                     [1.170e+01, 2.527e-04, 1.778e-04, 1.675e-04, 1.267e-04, 1.125e-04, 1.117e-04],
                     [1.155e+01, 1.098e-04, 3.004e-05, 3.749e-05, 1.774e-04, 6.037e-05, 8.613e-05],
                     [1.140e+01, 3.412e-04, 6.006e-05, 1.026e-04, 3.933e-04, 8.589e-05, 1.298e-04],
                     [1.125e+01, 7.370e-04, 1.122e-04, 1.504e-04, 7.540e-04, 1.109e-04, 1.461e-04],
                     [1.110e+01, 1.179e-03, 1.220e-04, 1.358e-04, 1.388e-03, 1.458e-04, 1.712e-04],
                     [1.095e+01, 1.868e-03, 1.594e-04, 1.733e-04, 2.013e-03, 1.605e-04, 1.830e-04],
                     [1.080e+01, 2.599e-03, 1.688e-04, 1.819e-04, 2.748e-03, 1.784e-04, 1.965e-04],
                     [1.065e+01, 3.740e-03, 2.112e-04, 2.216e-04, 3.909e-03, 2.267e-04, 2.337e-04],
                     [1.050e+01, 4.361e-03, 2.050e-04, 2.229e-04, 4.815e-03, 2.320e-04, 2.965e-04],
                     [1.035e+01, 5.054e-03, 2.468e-04, 3.705e-04, 5.002e-03, 2.570e-04, 3.082e-04],
                     [1.020e+01, 4.959e-03, 2.334e-04, 2.899e-04, 5.043e-03, 2.248e-04, 2.475e-04],
                     [1.005e+01, 5.287e-03, 2.191e-04, 2.327e-04, 5.621e-03, 2.242e-04, 2.443e-04],
                     [9.900e+00, 5.547e-03, 2.183e-04, 2.336e-04, 5.998e-03, 2.332e-04, 2.567e-04],
                     [9.750e+00, 6.146e-03, 2.460e-04, 2.698e-04, 6.688e-03, 2.584e-04, 3.042e-04],
                     [9.600e+00, 6.807e-03, 2.752e-04, 3.883e-04, 6.996e-03, 2.660e-04, 3.516e-04],
                     [9.450e+00, 7.930e-03, 3.172e-04, 6.464e-04, 8.456e-03, 3.139e-04, 3.648e-04],
                     [9.300e+00, 8.685e-03, 3.244e-04, 4.938e-04, 9.728e-03, 3.808e-04, 3.812e-04],
                     [9.150e+00, 9.734e-03, 3.789e-04, 4.391e-04, 1.047e-02, 4.362e-04, 4.594e-04],
                     [9.000e+00, 1.116e-02, 4.573e-04, 5.017e-04, 1.201e-02, 5.814e-04, 7.974e-04],
                     [8.850e+00, 1.288e-02, 5.924e-04, 5.940e-04, 1.357e-02, 6.866e-04, 9.703e-04],
                     [8.700e+00, 1.352e-02, 7.481e-04, 1.213e-03, 1.445e-02, 8.949e-04, 1.159e-03],
                     [8.550e+00, 1.559e-02, 8.825e-04, 1.036e-03, 1.656e-02, 1.281e-03, 1.852e-03],
                     [8.400e+00, 1.973e-02, 1.278e-03, 1.592e-03, 2.144e-02, 2.014e-03, 7.373e-03],
                     [8.250e+00, 2.272e-02, 1.896e-03, 2.137e-03, 2.489e-02, 2.691e-03, 5.911e-03],
                     [8.100e+00, 3.129e-02, 3.190e-03, 8.558e-03, 2.571e-02, 2.660e-03, 4.075e-03],
                     [7.950e+00, 3.522e-02, 4.228e-03, 9.164e-03, 2.711e-02, 3.818e-03, 5.286e-03],
                     [7.800e+00, 3.801e-02, 7.141e-03, 8.313e-03, 3.593e-02, 8.150e-03, 1.152e-02],
                     [7.650e+00, 4.161e-02, 9.480e-03, 1.331e-02, 5.659e-02, 1.665e-02, 2.049e-02],
                     [7.500e+00, 7.313e-02, 2.022e-02, 2.708e-02, 8.875e-02, 2.591e-02, 3.889e-02],
                     [7.350e+00, 1.453e-01, 4.561e-02, 6.180e-02, 1.524e-01, 4.638e-02, 5.302e-02],
                     [7.200e+00, 1.654e-01, 4.349e-02, 5.548e-02, 1.811e-01, 4.740e-02, 5.963e-02],
                     [7.050e+00, 1.788e-01, 4.503e-02, 6.684e-02, 1.263e-01, 3.697e-02, 4.103e-02],
                     [6.900e+00, 1.375e-01, 3.401e-02, 6.365e-02, 1.330e-01, 4.378e-02, 5.523e-02],
                     [6.750e+00, 1.530e-01, 4.164e-02, 1.601e-01, 1.035e-01, 4.067e-02, 1.474e-01],
                     [6.600e+00, 2.524e-01, 1.498e-01, 1.732e+00, 2.286e-01, 1.677e-01, 2.204e-01],
                     [6.450e+00, 3.203e-01, 2.135e-01, 2.681e+00, 2.119e-01, 1.812e-01, 1.980e+00],
                     [6.300e+00, 9.584e-02, 4.514e-02, 1.986e+00, 2.004e-01, 1.804e-01, 2.379e+00],
                     [6.150e+00, 2.228e-01, 1.855e-01, 1.549e+00, 1.476e-01, 1.436e-01, 1.475e+00],
                     [6.000e+00, 8.100e-01, 7.009e-01, 1.022e+00, 2.789e-01, 2.930e-01, 7.126e-01],
                     [5.850e+00, 3.815e-01, 3.857e-01, 3.700e-01, 5.350e-01, 5.692e-01, 1.146e+00],
                     [5.700e+00, 3.239e-02, 3.304e-02, 3.401e-02, 2.858e-02, 2.549e-02, 4.439e-01],
                     [5.550e+00, 3.742e-02, 3.572e-02, 4.519e-02, 1.257e-02, 1.567e-02, 2.418e-02],
                     [5.400e+00, 1.581e-02, 2.140e-02, 5.194e-02, 1.040e-02, 1.432e-02, 2.597e-02],
                     [5.250e+00, 6.482e-03, 6.445e-03, 6.068e-03, 0.000e+00, 0.000e+00, 6.446e-06]]) # doesn't include correction for under-density of GAMA regions
    ax.fill_between(W17_data[:,0]+2*np.log10(0.7/h), (W17_data[:,1]+W17_data[:,3])*(h/0.7)**3, (W17_data[:,1]- W17_data[:,2])*(h/0.7)**3,color='goldenrod', alpha=0.3, zorder=zo)
    ax.plot(W17_data[:,0]+2*np.log10(0.7/h), W17_data[:,1]*(h/0.7)**3, '-', color='goldenrod', lw=2, zorder=zo)
    ax.plot([0,1], [0,1], '-', color='goldenrod', lw=8, alpha=0.3, label=r'Wright et al.~(2017)') # Just for legend


def HIH2_massfunction_obsdata(h=0.678, HI=True, H2=True, K=True, OR=False, ax=None, Z=True, M=False, B=False):
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
                      [10.492,  -5.083]])
        
    Martin_data = np.array([[6.302,    -0.504],
                              [6.500,    -0.666],
                              [6.703,    -0.726],
                              [6.904,    -0.871],
                              [7.106,    -1.135],
                              [7.306,    -1.047],
                              [7.504,    -1.237],
                              [7.703,    -1.245],
                              [7.902,    -1.254],
                              [8.106,    -1.414],
                              [8.306,    -1.399],
                              [8.504,    -1.476],
                              [8.705,    -1.591],
                              [8.906,    -1.630],
                              [9.104,    -1.695],
                              [9.309,    -1.790],
                              [9.506,    -1.981],
                              [9.707,    -2.141],
                              [9.905,    -2.317],
                              [10.108,    -2.578],
                              [10.306,    -3.042],
                              [10.509,    -3.780],
                              [10.703,    -4.534],
                              [10.907,    -5.437]])     
                    
    Martin_mid = Martin_data[:,1] + 3*np.log10(h/0.7)
    Martin_x = Martin_data[:,0] + 2*np.log10(0.7/h)
    Martin_high = np.array([-0.206, -0.418, -0.571, -0.725, -1.003, -0.944, -1.144, -1.189, -1.189, -1.358, -1.344, -1.417, -1.528, -1.586, -1.651, -1.753, -1.925, -2.095, -2.281, -2.537, -3.003, -3.729, -4.451, -5.222]) + 3*np.log10(h/0.7)
    Martin_low = np.array([-0.806, -0.910, -0.885, -1.019, -1.268, -1.173, -1.313, -1.314, -1.320, -1.459, -1.443, -1.530, -1.647, -1.669, -1.736, -1.838, -2.021, -2.191, -2.359, -2.621, -3.098, -3.824, -4.618, -5.663]) + 3*np.log10(h/0.7)
      
    Keres_high = np.array([-1.051, -1.821, -1.028, -1.341, -1.343, -1.614, -1.854, -2.791,  -3.54 , -5.021]) + 3*np.log10(h)
    Keres_mid = np.array([-1.271, -1.999, -1.244, -1.477, -1.464, -1.713, -1.929, -2.878,   -3.721, -5.22 ]) + 3*np.log10(h)
    Keres_low = np.array([-1.706, -2.302, -1.71 , -1.676, -1.638, -1.82 , -2.033, -2.977,   -4.097, -5.584]) + 3*np.log10(h)
    Keres_M = np.array([  6.953,   7.353,   7.759,   8.154,   8.553,   8.96 ,   9.365,  9.753,  10.155,  10.558]) - 2*np.log10(h)
      
    ObrRaw_high = np.array([-0.905, -1.122, -1.033, -1.1  , -1.242, -1.418, -1.707, -2.175, -2.984, -4.868]) + 3*np.log10(h)
    ObrRaw_mid = np.array([-1.116, -1.308, -1.252, -1.253, -1.373, -1.509, -1.806, -2.261,  -3.198, -5.067]) + 3*np.log10(h)
    ObrRaw_low = np.array([-1.563, -1.602, -1.73 , -1.448, -1.537, -1.621, -1.918, -2.369,  -3.556, -5.413]) + 3*np.log10(h)
    ObrRaw_M = np.array([ 7.301,  7.586,  7.862,  8.133,  8.41 ,  8.686,  8.966,  9.242,    9.514,  9.788]) - 2*np.log10(h)
      
    HI_x = Zwaan[:,0] - 2*np.log10(h)
    HI_y = 10**Zwaan[:,1] * h**3

    Boselli_const_XCO = np.array([[7.39189, -3.06989, -3.32527, -2.86828],
                                [7.78378, -2.45161, -2.54570, -2.37097],
                                [8.18919, -1.91398, -1.96774, -1.84677],
                                [8.62162, -2.12903, -2.20968, -2.03495],
                                [9.01351, -2.41129, -2.51882, -2.31720],
                                [9.41892, -2.62634, -2.80108, -2.53226],
                                [9.81081, -2.73387, -2.85484, -2.54570],
                                [10.2297, -3.64785, -5.97312, -3.36559]])

    Boselli_var_XCO = np.array([[7.59030, -3.19086, -3.58065, -2.98925],
                              [7.98113, -2.55914, -2.72043, -2.45161],
                              [8.37197, -2.22312, -2.30376, -2.14247],
                              [8.78976, -1.94086, -1.99462, -1.90054],
                              [9.18059, -1.98118, -2.06183, -1.90054],
                              [9.59838, -2.72043, -2.92204, -2.62634], 
                              [9.98922, -3.67473, -5.98656, -3.31183]])
    
    if ax is None: ax = plt.gca()         
    if HI and Z: ax.plot(HI_x, HI_y, '-', color='g', lw=8, alpha=0.4, label=r'Zwaan et al.~(2005)')
    if HI and M: ax.fill_between(Martin_x, 10**Martin_high, 10**Martin_low, color='c', alpha=0.4)
    if HI and M: ax.plot([0,1], [1,1], 'c-', lw=8, alpha=0.4, label=r'Martin et al.~(2010)')

    if H2 and K: ax.fill_between(Keres_M, 10**Keres_high, 10**Keres_low, color='teal', alpha=0.4)
    if H2 and K: ax.plot([0,1], [1,1], '-', color='teal', lw=8, alpha=0.4, label=r'Keres et al.~(2003)')

    if H2 and OR: ax.fill_between(ObrRaw_M, 10**ObrRaw_high, 10**ObrRaw_low, color='darkcyan', alpha=0.4)
    if H2 and OR: ax.plot([0,1], [1,1], '-', color='darkcyan', lw=8, alpha=0.4, label=r'Obreschkow \& Rawlings (2009)')
                        
    if H2 and B: ax.fill_between(Boselli_const_XCO[:,0]+2*np.log10(0.7/h), 10**Boselli_const_XCO[:,2]*(h/0.7)**3/0.4, 10**Boselli_const_XCO[:,3]*(h/0.7)**3/0.4, color='orange', alpha=0.4)
    if H2 and B: ax.plot([0,1], [1,1], '-', color='orange', lw=8, alpha=0.4, label=r'Boselli et al.~(2014), const.~$X_{\rm CO}$')
    if H2 and B: ax.fill_between(Boselli_var_XCO[:,0]+2*np.log10(0.7/h), 10**Boselli_var_XCO[:,2]*(h/0.7)**3/0.4, 10**Boselli_var_XCO[:,3]*(h/0.7)**3/0.4, color='violet', alpha=0.4)
    if H2 and B: ax.plot([0,1], [1,1], '-', color='violet', lw=8, alpha=0.4, label=r'Boselli et al.~(2014), var.~$X_{\rm CO}$')

    ax.set_xlabel(r'$\log_{10}(M_{\mathrm{H}\,\huge\textsc{i}}\ \mathrm{or}\ M_{\mathrm{H}_2}\ [\mathrm{M}_{\bigodot}])$')
    ax.set_ylabel(r'$\Phi\ [\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}]$')
    ax.axis([8,11.5,1e-6,1e-1])



def savepng(filename, xsize=1024, ysize=None, fig=None, transparent=False, compact=False):
    # Save a figure as a PNG with a normalised size / aspect ratio
    xpix = 2560
    ypix = 1440
    ss = 27
    
    if ysize==None: ysize = int(xsize*9./16)
    
    mydpi = np.sqrt(xpix**2 + ypix**2)/ss 
    xinplot = xsize*(9./7.)/mydpi
    yinplot = ysize*(9./7.)/mydpi
    if fig is None: fig = plt.gcf()
    fig.set_size_inches(xinplot,yinplot)
    fig.set_dpi(mydpi)
    if compact: fig.subplots_adjust(hspace=0, wspace=0, left=0, bottom=0, right=1.0, top=1.0)
    
    filename = str(filename)
    if filename[-4:] != '.png':
        filename = filename+'.png'
    fig.savefig(filename, dpi=mydpi, bbox_inches='tight', transparent=transparent)


def Brown_HI_fractions(h):
    
    logM = np.array([[9.2209911, 9.6852989, 10.180009, 10.665453, 11.098589],
         [9.2085762, 9.6402225, 10.141238, 10.599669, 11.026575],
         [9.2121296, 9.6528578, 10.139588, 10.615245, 11.054683]]) + 2*np.log10(0.7/h) + np.log10(0.61/0.66)
         
    logHIfrac = np.array([[ 0.37694988,  0.0076254,  -0.45345795, -0.90604609, -1.39503932],
           [ 0.15731917, -0.16941574, -0.6199488,  -0.99943721, -1.30476058],
           [ 0.19498822, -0.27559358, -0.74410361, -1.12869251, -1.49363434]]) - np.log10(0.61/0.66)

    Ngal = np.array([[120, 318, 675, 1132, 727],
                      [3500, 4359, 3843, 2158, 268],
                      [2203, 3325, 2899, 1784, 356]])

    logM_all = np.log10((10**logM[0,:] * Ngal[0,:] + 10**logM[1,:] * Ngal[1,:] + 10**logM[2,:] * Ngal[2,:]) / (Ngal[0,:]+Ngal[1,:]+Ngal[2,:]))
    
    logHIfrac_all = np.log10((10**logHIfrac[0,:] * Ngal[0,:] + 10**logHIfrac[1,:] * Ngal[1,:] + 10**logHIfrac[2,:] * Ngal[2,:]) / (Ngal[0,:]+Ngal[1,:]+Ngal[2,:]))
    
    logHIfrac_all_err = np.array([np.log10(1.511+0.011)-np.log10(1.511),
                                  np.log10(0.643+0.011)-np.log10(0.643),
                                  np.log10(0.232+0.005)-np.log10(0.232),
                                  np.log10(0.096+0.002)-np.log10(0.096),
                                  np.log10(0.039+0.001)-np.log10(0.039)]) # taken straight from Table 1 of Brown+15

    return logM_all, logHIfrac_all, logHIfrac_all_err

def hist_Nmin(x, bins, Nmin, hard_bins=np.array([])):
    Nhist, bins = np.histogram(x, bins)
    while len(Nhist[Nhist<Nmin])>0:
        ii = np.where(Nhist<Nmin)[0][0]
        if (ii==0 or (ii!=len(Nhist)-1 and Nhist[ii+1]<Nhist[ii-1])) and np.all(~((bins[ii+1] <= 1.01*hard_bins) * (bins[ii+1] >= 0.99*hard_bins))):
            bins = np.delete(bins,ii+1)
        elif np.all(~((bins[ii] <= 1.01*hard_bins) * (bins[ii] >= 0.99*hard_bins))):
            bins = np.delete(bins,ii)
        else:
            print('hard_bins prevented routines.hist_Nmin() from enforcing Nmin.  Try using wider input bins.')
            Nhist, bins = np.histogram(x, bins)
            break
        Nhist, bins = np.histogram(x, bins)
    if bins[0]<np.min(x): bins[0] = np.min(x)
    if bins[-1]>np.max(x): bins[-1] = np.max(x)
    return Nhist, bins


def meanbins(x, y, xmeans, tol=0.02, itmax=100):
    # Find bins in some dataset (x,y) which will have mean values of x matching xmeans
    fnan = np.isfinite(x)*np.isfinite(y)
    x, y = x[fnan], y[fnan]
    N = len(xmeans)
    bins = np.zeros(N+1)
    mean_x, mean_y = np.zeros(N), np.zeros(N)
    bins[0] = np.min(x)
    bins[-1] = np.max(x)
    for i in range(1,N+1):
        xleft = np.sort(x[x>=bins[i-1]])
        cumav = np.cumsum(xleft) / np.arange(1,len(xleft)+1)
        diff = abs(xmeans[i-1]-cumav)
        arg = np.where(diff==np.min(diff))[0][0]
        if i!=N and arg<len(diff)-1: arg += 1
        bins[i] = xleft[arg]
        m = cumav[arg]
        mean_x[i-1] = m
        f = (xleft<bins[i]) if i!=N else (xleft<=bins[i])
        mean_y[i-1] = np.mean(y[np.in1d(x,xleft[f])])
    return bins, mean_x, mean_y


def percentiles(x, y, low=0.16, med=0.5, high=0.84, bins=20, addMean=False, xrange=None, yrange=None, Nmin=10, hard_bins=np.array([]), outBins=False):
    # Given some values to go on x and y axes, bin them along x and return the percentile ranges
    f = np.isfinite(x)*np.isfinite(y)
    if xrange is not None: f = (x>=xrange[0])*(x<=xrange[1])*f
    if yrange is not None: f = (y>=yrange[0])*(y<=yrange[1])*f
    x, y = x[f], y[f]
    if type(bins)==int:
        if len(x)/bins < Nmin: bins = len(x)/Nmin
        indices = np.array(np.linspace(0,len(x)-1,bins+1), dtype=int)
        bins = np.sort(x)[indices]
    elif Nmin>0: # Ensure a minimum number of data in each bin
        Nhist, bins = hist_Nmin(x, bins, Nmin, hard_bins)
    Nbins = len(bins)-1
    y_low, y_med, y_high = np.zeros(Nbins), np.zeros(Nbins), np.zeros(Nbins)
    x_av, N = np.zeros(Nbins), np.zeros(Nbins)
    if addMean: y_mean = np.zeros(Nbins)
    for i in range(Nbins):
        f = (x>=bins[i])*(x<bins[i+1])
        if len(f[f])>2:
            [y_low[i], y_med[i], y_high[i]] = np.percentile(y[f], [100*low, 100*med, 100*high], interpolation='linear')
            x_av[i] = np.mean(x[f])
            N[i] = len(x[f])
            if addMean: y_mean[i] = np.mean(y[f])
    fN = (N>0) if Nmin>0 else np.array([True]*Nbins)
    if not addMean and not outBins:
        return x_av[fN], y_high[fN], y_med[fN], y_low[fN]
    elif not addMean and outBins:
        return x_av[fN], y_high[fN], y_med[fN], y_low[fN], bins
    elif addMean and  not outBins:
        return x_av[fN], y_high[fN], y_med[fN], y_low[fN], y_mean[fN]
    else:
        return x_av[fN], y_high[fN], y_med[fN], y_low[fN], y_mean[fN], bins



def Tremonti04(h):
    x_obs = np.array([8.52, 8.57, 8.67, 8.76, 8.86, 8.96, 9.06, 9.16, 9.26, 9.36, 9.46, 9.57, 9.66, 9.76, 9.86, 9.96, 10.06, 10.16, 10.26, 10.36, 10.46, 10.56, 10.66, 10.76, 10.86, 10.95, 11.05, 11.15, 11.25, 11.30])
    y_low = np.array([8.25, 8.25, 8.28, 8.32, 8.37, 8.46, 8.56, 8.59, 8.60, 8.63, 8.66, 8.69, 8.72, 8.76, 8.80, 8.83, 8.85, 8.88, 8.92, 8.94, 8.96, 8.98, 9.00, 9.01, 9.02, 9.03, 9.03, 9.04, 9.03, 9.03])
    y_high= np.array([8.64, 8.64, 8.65, 8.70, 8.73, 8.75, 8.82, 8.82, 8.86, 8.88, 8.92, 8.94, 8.96, 8.99, 9.01, 9.05, 9.06, 9.09, 9.10, 9.11, 9.12, 9.14, 9.15, 9.15, 9.16, 9.17, 9.17, 9.18, 9.18, 9.18])
    x_obs += np.log10(0.61/0.66) + 2*np.log10(0.7/h) # Accounts for difference in Kroupa & Chabrier IMFs and the difference in h
    return x_obs, y_low, y_high




def BH_bulge_obs(h=0.678, ax=None):
    M_BH_obs = (0.7/h)**2*1e8*np.array([39, 11, 0.45, 25, 24, 0.044, 1.4, 0.73, 9.0, 58, 0.10, 8.3, 0.39, 0.42, 0.084, 0.66, 0.73, 15, 4.7, 0.083, 0.14, 0.15, 0.4, 0.12, 1.7, 0.024, 8.8, 0.14, 2.0, 0.073, 0.77, 4.0, 0.17, 0.34, 2.4, 0.058, 3.1, 1.3, 2.0, 97, 8.1, 1.8, 0.65, 0.39, 5.0, 3.3, 4.5, 0.075, 0.68, 1.2, 0.13, 4.7, 0.59, 6.4, 0.79, 3.9, 47, 1.8, 0.06, 0.016, 210, 0.014, 7.4, 1.6, 6.8, 2.6, 11, 37, 5.9, 0.31, 0.10, 3.7, 0.55, 13, 0.11])
    M_BH_hi = (0.7/h)**2*1e8*np.array([4, 2, 0.17, 7, 10, 0.044, 0.9, 0.0, 0.9, 3.5, 0.10, 2.7, 0.26, 0.04, 0.003, 0.03, 0.69, 2, 0.6, 0.004, 0.02, 0.09, 0.04, 0.005, 0.2, 0.024, 10, 0.1, 0.5, 0.015, 0.04, 1.0, 0.01, 0.02, 0.3, 0.008, 1.4, 0.5, 1.1, 30, 2.0, 0.6, 0.07, 0.01, 1.0, 0.9, 2.3, 0.002, 0.13, 0.4, 0.08, 0.5, 0.03, 0.4, 0.38, 0.4, 10, 0.2, 0.014, 0.004, 160, 0.014, 4.7, 0.3, 0.7, 0.4, 1, 18, 2.0, 0.004, 0.001, 2.6, 0.26, 5, 0.005])
    M_BH_lo = (0.7/h)**2*1e8*np.array([5, 2, 0.10, 7, 10, 0.022, 0.3, 0.0, 0.8, 3.5, 0.05, 1.3, 0.09, 0.04, 0.003, 0.03, 0.35, 2, 0.6, 0.004, 0.13, 0.1, 0.05, 0.005, 0.2, 0.012, 2.7, 0.06, 0.5, 0.015, 0.06, 1.0, 0.02, 0.02, 0.3, 0.008, 0.6, 0.5, 0.6, 26, 1.9, 0.3, 0.07, 0.01, 1.0, 2.5, 1.5, 0.002, 0.13, 0.9, 0.08, 0.5, 0.09, 0.4, 0.33, 0.4, 10, 0.1, 0.014, 0.004, 160, 0.007, 3.0, 0.4, 0.7, 1.5, 1, 11, 2.0, 0.004, 0.001, 1.5, 0.19, 4, 0.005])
    M_sph_obs = (0.7/h)**2*1e10*np.array([69, 37, 1.4, 55, 27, 2.4, 0.46, 1.0, 19, 23, 0.61, 4.6, 11, 1.9, 4.5, 1.4, 0.66, 4.7, 26, 2.0, 0.39, 0.35, 0.30, 3.5, 6.7, 0.88, 1.9, 0.93, 1.24, 0.86, 2.0, 5.4, 1.2, 4.9, 2.0, 0.66, 5.1, 2.6, 3.2, 100, 1.4, 0.88, 1.3, 0.56, 29, 6.1, 0.65, 3.3, 2.0, 6.9, 1.4, 7.7, 0.9, 3.9, 1.8, 8.4, 27, 6.0, 0.43, 1.0, 122, 0.30, 29, 11, 20, 2.8, 24, 78, 96, 3.6, 2.6, 55, 1.4, 64, 1.2])
    M_sph_hi = (0.7/h)**2*1e10*np.array([59, 32, 2.0, 80, 23, 3.5, 0.68, 1.5, 16, 19, 0.89, 6.6, 9, 2.7, 6.6, 2.1, 0.91, 6.9, 22, 2.9, 0.57, 0.52, 0.45, 5.1, 5.7, 1.28, 2.7, 1.37, 1.8, 1.26, 1.7, 4.7, 1.7, 7.1, 2.9, 0.97, 7.4, 3.8, 2.7, 86, 2.1, 1.30, 1.9, 0.82, 25, 5.2, 0.96, 4.9, 3.0, 5.9, 1.2, 6.6, 1.3, 5.7, 2.7, 7.2, 23, 5.2, 0.64, 1.5, 105, 0.45, 25, 10, 17, 2.4, 20, 67, 83, 5.2, 3.8, 48, 2.0, 55, 1.8])
    M_sph_lo = (0.7/h)**2*1e10*np.array([32, 17, 0.8, 33, 12, 1.4, 0.28, 0.6, 9, 10, 0.39, 2.7, 5, 1.1, 2.7, 0.8, 0.40, 2.8, 12, 1.2, 0.23, 0.21, 0.18, 2.1, 3.1, 0.52, 1.1, 0.56, 0.7, 0.51, 0.9, 2.5, 0.7, 2.9, 1.2, 0.40, 3.0, 1.5, 1.5, 46, 0.9, 0.53, 0.8, 0.34, 13, 2.8, 0.39, 2.0, 1.2, 3.2, 0.6, 3.6, 0.5, 2.3, 1.1, 3.9, 12, 2.8, 0.26, 0.6, 57, 0.18, 13, 5, 9, 1.3, 11, 36, 44, 2.1, 1.5, 26, 0.8, 30, 0.7])
    core = np.array([1,1,0,1,1,0,0,0,1,1,0,1,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,1,1,1,0,0,0,1,1,0,0,0,0,0,1,0,1,0,0,1,0,0,0,1,0,1,0,1,0,1,1,1,0,0,1,0,1,0])
    yerr2, yerr1 = np.log10(np.divide(M_BH_obs+M_BH_hi, M_BH_obs)), -np.log10(np.divide(M_BH_obs-M_BH_lo, M_BH_obs))
    xerr2, xerr1 = np.log10(np.divide(M_sph_obs+M_sph_hi, M_sph_obs)), -np.log10(np.divide(M_sph_obs-M_sph_lo, M_sph_obs))
    if ax is None: ax = plt.gca()
    ax.errorbar(np.log10(M_sph_obs), np.log10(M_BH_obs), yerr=[yerr1,yerr2], xerr=[xerr1,xerr2], color='#446329', ecolor='#6ec495', mec='#6ec495', marker='s', alpha=0.7, label=r'Scott et al.~(2013)', ls='none', lw=2, ms=12)
    
#    ax.errorbar(np.log10(M_sph_obs[core==1]), np.log10(M_BH_obs[core==1]), yerr=[yerr1[core==1],yerr2[core==1]], xerr=[xerr1[core==1],xerr2[core==1]], color='c', alpha=0.5, label=r'Scott et al.~(2013) S\`{e}rsic', ls='none', lw=2, ms=0)
#    ax.errorbar(np.log10(M_sph_obs[core==0]), np.log10(M_BH_obs[core==0]), yerr=[yerr1[core==0],yerr2[core==0]], xerr=[xerr1[core==0],xerr2[core==0]], color='purple', alpha=0.3, label=r'Core-S\`{e}rsic', ls='none', lw=2, ms=0)




def Leroygals(HI=False, H2=False, HighVvir=True, LowVvir=False, ax=None, SFR=False, h=0.678, c='k', alpha=0.5, lw=2):
    # Plot galaxy surface density profiles for select galaxies.  Stars done by default
    N628 = np.array([[0.2, 0.5, 0.9, 1.2, 1.6, 1.9, 2.3, 2.7, 3.0, 3.4, 3.7, 4.1, 4.4, 4.8, 5.1, 5.5, 5.8, 6.2, 6.5, 6.9, 7.3, 7.6, 8.0, 8.3, 8.7, 9.0, 9.4, 9.7, 10.1, 10.4, 10.8, 11.1, 11.5, 11.9, 12.2],
     [1.6, 2.1, 2.6, 3.1, 3.7, 4.6, 5.3, 5.8, 6.1, 6.5, 7.3, 7.9, 8.1, 7.9, 8.2, 8.5, 8.6, 8.6, 8.8, 8.8, 8.6, 8.2, 7.6, 7.1, 6.7, 6.5, 6.0, 5.2, 4.5, 4.1, 3.9, 3.9, 4.0, 4.3, 4.6],
     [0.3, 0.3, 0.4, 0.4, 0.3, 0.3, 0.4, 0.5, 0.5, 0.5, 0.7, 0.8, 0.8, 0.9, 1.0, 1.0, 0.8, 0.7, 0.6, 0.5, 0.5, 0.6, 0.6, 0.6, 0.5, 0.4, 0.5, 0.4, 0.4, 0.3, 0.3, 0.4, 0.4, 0.5, 0.5],
     [22.7, 20.2, 16.1, 12.7, 11.4, 11.1, 11.1, 10.6, 8.9, 7.2, 6.2, 5.9, 5.4, 4.3, 3.1, 2.1, 1.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
     [1.2, 1.3, 1.2, 0.8, 1.1, 1.2, 1.7, 1.9, 1.5, 1.2, 1.5, 1.7, 1.5, 1.1, 0.8, 0.7, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
     [1209.4, 557.8, 313.6, 231.9, 194.3, 163.5, 143.9, 123.5, 107.5, 151.0, 81.6, 68.0, 61.6, 48.3, 41.8, 37.0, 33.2, 37.0, 52.9, 19.5, 18.9, 18.7, 12.9, 17.6, 17.0, 10.8, 8.0, 7.5, 5.0, 4.1, 3.6, 3.9, 4.4, 9.5, 5.8],
     [18.3, 4.8, 1.0, 0.5, 0.5, 0.7, 0.8, 0.5, 0.4, 10.5, 0.4, 0.4, 0.4, 0.2, 0.2, 0.2, 0.4, 2.3, 6.1, 0.1, 0.1, 0.7, 0.1, 1.3, 1.6, 0.4, 0.1, 0.2, 0.1, 0.0, 0.0, 0.1, 0.2, 0.9, 0.2],
     [105.1, 92.3, 76.7, 65.5, 62.2, 72.4, 90.2, 90.7, 71.9, 57.9, 55.8, 59.6, 59.9, 48.8, 37.4, 33.5, 30.2, 23.5, 17.4, 13.6, 11.6, 9.8, 7.5, 5.4, 4.1, 3.2, 2.5, 1.8, 1.2, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0],
     [14, 9.9, 5.1, 4.2, 3.7, 12.2, 23.5, 21.3, 11.9, 8.5, 11.3, 14.1, 15.2, 11, 6.6, 8.7, 10, 6.5, 3.1, 1.9, 2.3, 2.2, 1.5, 0.9, 0.6, 0.4, 0.3, 0.3, 0.2, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0]])
                     
                     
    N3184 = np.array([[0.3, 0.8, 1.3, 1.9, 2.4, 3.0, 3.5, 4.0, 4.6, 5.1, 5.7, 6.2, 6.7, 7.3, 7.8, 8.3, 8.9, 9.4, 10.0, 10.5, 11.0, 11.6, 12.1, 12.6, 13.2, 13.7, 14.3],
    [3.7, 3.2, 3.3, 3.8, 4.7, 5.5, 5.7, 5.9, 6.5, 7.3, 7.5, 7.8, 8.1, 8.0, 7.3, 7.0, 7.0, 6.7, 6.1, 5.4, 5.0, 4.6, 4.0, 3.3, 2.9, 2.8, 2.7],
    [0.5, 0.3, 0.3, 0.3, 0.5, 0.4, 0.4, 0.5, 0.4, 0.3, 0.5, 0.6, 0.6, 0.5, 0.3, 0.3, 0.3, 0.3, 0.2, 0.2, 0.3, 0.3, 0.2, 0.2, 0.2, 0.2, 0.2],
    [44.2, 20.8, 14.5, 11.9, 12.6, 12.6, 11.0, 9.6, 7.4, 6.2, 5.5, 4.3, 2.7, 1.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [9.6, 3.3, 2.0, 1.6, 2.0, 2.1, 2.1, 0.9, 1.0, 0.6, 0.9, 0.8, 0.5, 0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [701.8, 270.5, 200.3, 146.5, 121.1, 113.0, 100.2, 94.2, 83.6, 74.6, 96.2, 61.2, 46.3, 34.6, 27.5, 22.4, 19.3, 14.9, 12.9, 9.5, 7.9, 6.6, 5.1, 4.7, 3.7, 3.0, 3.8],
    [22.8, 1.1, 0.6, 1.0, 0.5, 0.4, 0.4, 0.5, 0.3, 0.3, 6.4, 0.3, 0.2, 0.1, 0.1, 0.1, 0.2, 0.1, 0.3, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.0, 0.1],
    [282.1, 85.4, 47.1, 47.1, 48.6, 50.8, 51.3, 50.0, 42.5, 38.4, 36.5, 37.6, 29.9, 20.4, 13.9, 9.9, 8.4, 6.9, 4.3, 3.0, 2.4, 1.7, 1.1, 0.0, 0.0, 0.0, 0.0],
    [78.8, 23.5, 6.5, 4.8, 6.9, 7.7, 8.8, 10.4, 6.5, 4.7, 4.5, 7.2, 5.0, 2.3, 1.4, 1.1, 1.1, 0.8, 0.4, 0.6, 0.5, 0.3, 0.2, 0.0, 0.0, 0.0, 0.0]])

    N3521 = np.array([[0.3, 0.8, 1.3, 1.8, 2.3, 2.9, 3.4, 3.9, 4.4, 4.9, 5.4, 6.0, 6.5, 7.0, 7.5, 8.0, 8.6, 9.1, 9.6, 10.1, 10.6, 11.2, 11.7, 12.2, 12.7, 13.2, 13.7, 14.3, 14.8, 15.3],
    [4.5, 4.8, 5.7, 7.1, 8.3, 8.7, 8.7, 8.9, 9.9, 10.6, 10.5, 10.2, 10.2, 9.4, 8.6, 8.4, 8.5, 8.6, 8.5, 8.2, 8.1, 8.1, 8.2, 8.3, 8.2, 8.0, 7.9, 7.7, 7.1, 6.5],
    [0.2, 0.3, 0.6, 0.7, 0.8, 0.8, 0.6, 0.8, 0.9, 0.6, 0.5, 0.4, 0.5, 0.3, 0.4, 0.5, 0.5, 0.5, 0.6, 0.6, 0.7, 0.8, 0.9, 0.9, 0.9, 0.9, 1.0, 0.9, 0.8, 0.6],
    [25.7, 35.4, 43.4, 44.6, 41.5, 36.8, 30.6, 24.6, 22.2, 21.0, 17.5, 12.5, 8.4, 5.2, 3.2, 2.1, 1.7, 1.6, 1.2, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [4.7, 5.5, 3.0, 1.5, 2.1, 2.7, 2.6, 2.3, 2.0, 2.1, 2.4, 2.5, 2.0, 1.4, 0.9, 0.6, 0.5, 0.5, 0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [4545.9, 1442.2, 929.8, 589.1, 462.9, 381.3, 322.9, 250.8, 212.3, 192.2, 169.7, 134.7, 106.7, 82.4, 66.1, 55.4, 47.7, 41.7, 35.8, 30.5, 27.1, 25.4, 22.9, 20.0, 17.4, 15.7, 14.4, 13.3, 12.0, 10.9],
    [287.3, 23.0, 8.9, 5.4, 2.9, 2.0, 1.9, 1.8, 1.4, 1.3, 1.1, 1.0, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.3, 0.2, 0.2, 0.2, 0.2, 0.2, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1],
    [106, 152.7, 183.4, 186.8, 176.9, 160.3, 138.2, 109, 89.2, 80.1, 70.3, 53.5, 36.1, 23.5, 16.2, 12.9, 11.6, 10.7, 9.4, 7.5, 6.1, 5.3, 4.6, 3.9, 3.1, 2.5, 2.1, 1.8, 1.4, 1.1],
    [27.9, 22.8, 11.5, 8.6, 8.7, 10.9, 16.3, 15.1, 11.5, 9.3, 9.2, 9.2, 7.6, 5.2, 3.6, 3.0, 3.0, 2.5, 2.0, 1.3, 1.3, 1.3, 1.2, 0.9, 0.7, 0.5, 0.4, 0.3, 0.2, 0.2]])

    N5194 = np.array([[0.2, 0.6, 1.0, 1.4, 1.7, 2.1, 2.5, 2.9, 3.3, 3.7, 4.1, 4.5, 4.8, 5.2, 5.6, 6.0, 6.4, 6.8, 7.2, 7.6, 8.0, 8.3, 8.7, 9.1, 9.5, 9.9, 10.3, 10.7],
    [4.5, 5.5, 6.1, 6.1, 6.7, 7.9, 8.5, 7.5, 6.2, 6.1, 7.2, 9.1, 11.2, 12.8, 12.7, 11.1, 9.4, 8.4, 7.8, 7.8, 7.8, 7.8, 7.8, 7.3, 6.4, 5.8, 5.1, 4.5],
    [0.4, 0.4, 0.4, 0.6, 0.6, 0.6, 0.9, 0.9 ,0.7, 0.5, 0.7, 0.9, 0.9, 0.8, 0.8, 0.8, 0.9, 0.9, 0.9, 1.0, 1.1, 1.2, 1.2, 1.1, 1.0, 1.0, 0.9, 0.9],
    [197.4, 207.7, 181.6, 134.5, 106.8, 94.8, 72.6, 40.9, 19.1, 14.6, 22.6, 33.6, 35.9, 28.0, 14.9, 4.3, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [35.0, 33.5, 30.5, 41.2, 33.8, 19.2, 15.7 ,12.2, 5.6, 3.3, 7.5, 10.3, 9.6, 7.0, 4.7, 8.2, 1.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [4912.2, 2352.5, 1251.1, 703.1, 471.8, 417.5, 394.9, 334.1, 286.9, 253.2, 236.7, 224.2, 227.8, 206.0, 176.5, 148.9, 106.3, 77.5, 64.3, 50.2, 45.0, 46.4, 53.2, 69.2, 71.2, 86.5, 210.3, 102.7],
    [111.6, 15.3, 6.5, 3.8, 1.3, 1.6, 1.9, 1.5, 1.3, 2.0, 1.7, 1.2, 1.4, 1.1, 0.9, 0.8, 0.4, 0.3, 0.6, 0.3, 0.3, 0.3, 0.6, 1.8, 1.3, 1.7, 11.3, 3.1],
    [1164, 1026.9, 772.8, 463.7, 295.4, 296.7, 304.1, 234.9, 154.7, 113, 113.5, 148.7, 188, 199.4, 180.2, 141.1, 95.9, 58.7, 36.7, 24.9, 19.5, 19.5, 20, 19, 14.8, 10.8, 8.3, 6.6],
    [96.6, 138.1, 134.6, 84.7, 45.3, 49.5, 49.9, 34.2, 19.4, 8.7, 19.8, 39.9, 48.7, 41.2, 31.7, 27.6, 19.2, 9.9, 5.7, 4.3, 3.9, 4.3, 5.5, 6.8, 5.1, 2.8, 1.8, 1.3]])

    if ax is None: ax = plt.gca()

    if HighVvir and not SFR:
        if HI and H2:
            ax.errorbar(N628[0,:], N628[1,:]+N628[3,:], N628[2,:]+N628[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3184[0,:], N3184[1,:]+N3184[3,:], N3184[2,:]+N3184[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3521[0,:], N3521[1,:]+N3521[3,:], N3521[2,:]+N3521[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5194[0,:], N5194[1,:]+N5194[3,:], N5194[2,:]+N5194[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        elif HI and not H2:
            ax.errorbar(N628[0,:], N628[1,:], N628[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3184[0,:], N3184[1,:], N3184[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3521[0,:], N3521[1,:], N3521[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5194[0,:], N5194[1,:], N5194[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        elif H2 and not HI:
            ax.errorbar(N628[0,:], N628[3,:], N628[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3184[0,:], N3184[3,:], N3184[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3521[0,:], N3521[3,:], N3521[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5194[0,:], N5194[3,:], N5194[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        else:
            ax.errorbar(N628[0,:], N628[5,:]*0.61/0.66, N628[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3184[0,:], N3184[5,:]*0.61/0.66, N3184[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3521[0,:], N3521[5,:]*0.61/0.66, N3521[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5194[0,:], N5194[5,:]*0.61/0.66, N5194[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
    elif HighVvir and SFR:
        ax.errorbar(N628[0,:], N628[7,:]*1e-4*0.63/0.67, N628[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        ax.errorbar(N3184[0,:], N3184[7,:]*1e-4*0.63/0.67, N3184[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        ax.errorbar(N3521[0,:], N3521[7,:]*1e-4*0.63/0.67, N3521[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        ax.errorbar(N5194[0,:], N5194[7,:]*1e-4*0.63/0.67, N5194[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)

    N3351 = np.array([[0.2, 0.7, 1.2, 1.7, 2.2, 2.7, 3.2, 3.7, 4.2, 4.7, 5.1, 5.6, 6.1, 6.6, 7.1, 7.6, 8.1, 8.6, 9.1, 9.5, 10.0, 10.5, 11.0, 11.5, 12.0, 12.5],
    [1.5, 1.0, 0.0, 0.0, 1.2, 2.0, 2.6, 2.7, 2.9, 3.1, 3.0, 2.9, 2.7, 2.7, 2.7, 2.8, 3.0, 3.3, 3.5, 3.6, 3.5, 3.1, 2.5, 2.1, 1.8, 1.4],
    [0.3, 0.2, 1.0, 1.0, 0.1, 0.2, 0.2, 0.1, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.1, 0.1, 0.1],
    [161.4, 72.1, 15.7, 3.0, 2.1, 3.6, 4.8, 4.3, 3.1, 2.3, 1.6, 1.3, 1.2, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [29.9, 22.6, 6.3, 1.5, 0.6, 0.7, 0.8, 1.0, 0.7, 0.4, 0.4, 0.4, 0.4, 0.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [4525.1, 973.0, 418.7, 245.3, 189.8, 713.4, 162.3, 124.0, 93.3, 69.6, 55.5, 49.1, 44.5, 38.8, 32.9, 26.3, 20.4, 16.1, 12.6, 10.7, 9.2, 6.7, 5.9, 5.3, 3.7, 3.4],
    [98.2, 10.4, 2.7, 1.7, 1.2, 0.8, 0.5, 0.4, 0.3, 0.2, 0.2, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.2, 0.0, 0.1, 0.2, 0.1, 0.2],
    [2559.3, 1052, 194.1, 44.5, 30.6, 33.1, 37.2, 32.3, 23.1, 17.1, 13.6, 11.2, 9.7, 9.0, 8.6, 6.9, 5.2, 4.3, 3.5, 3, 2.6, 1.8, 1.1, 0.0, 0.0, 0.0],
    [473.1, 337, 68.7, 7.9, 1.8, 2, 2.7, 2.8, 1.7, 1.3, 1.4, 1.2, 1.1, 0.8, 1.1, 1.1, 0.5, 0.4, 0.3, 0.3, 0.3, 0.3, 0.1, 0.0, 0.0, 0.0]])

    N3627 = np.array([[0.2, 0.7, 1.1, 1.6, 2.0, 2.5, 2.9, 3.4, 3.8, 4.3, 4.7, 5.2, 5.6, 6.1, 6.5, 7.0, 7.4, 7.9, 8.3, 8.8, 9.2, 9.7, 10.1, 10.6, 11.0, 11.5, 11.9, 12.4, 12.8, 13.3, 13.8, 14.2, 14.7, 15.1, 15.6, 16.0, 16.5],
    [3.0, 3.5, 4.1, 4.9, 5.7, 6.4, 7.3, 7.7, 7.2, 6.5, 6.0, 5.3, 4.5, 3.7, 3.2, 2.9, 2.3, 1.7, 1.3, 1.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [1.0, 0.6, 0.4, 0.6, 0.7, 1.0, 1.5, 1.6, 1.2, 1.1, 1.3, 1.2, 1.0, 0.9, 0.9, 0.9, 0.6, 0.4, 0.3, 0.2, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0],
    [173.3, 109.6, 59.1, 37.3, 38.7, 47.9, 49.6, 39.2, 22.2, 13.4, 10.5, 8.4, 7.2, 6.1, 5.4, 4.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [30.1, 23.0, 11.8, 8.9, 16.3, 23.9, 24.3, 19.2, 12.2, 7.0, 4.4, 3.8, 3.7, 3.6, 3.3, 2.8, 1.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [5428.1, 1553.9, 846.5, 592.0, 478.9, 289.1, 254.9, 275.1, 216.8, 172.9, 158.3, 132.8, 113.8, 94.8, 78.5, 69.2, 53.4, 41.3, 35.0, 28.7, 24.3, 21.9, 22.2, 57.6, 59.3, 16.5, 10.8, 8.7, 7.5, 8.6, 9.0, 5.4, 4.6, 4.3, 4.3, 3.7, 3.0],
    [273.0, 14.4, 4.6, 3.5, 3.3, 4.6, 4.7, 2.5, 1.6, 1.0, 1.2, 0.9, 0.8, 0.6, 0.5, 0.6, 0.4, 0.2, 0.3, 0.1, 0.1, 0.1, 0.3, 6.3, 6.7, 0.5, 0.1, 0.1, 0.1, 0.5, 0.6, 0.1, 0.0, 0.1, 0.1, 0.1, 0.0],
    [251.1, 205.9, 167.2, 197.1, 317.6, 430.4, 431.9, 327.5, 192.2, 127.9, 101, 62.5, 37.6, 27.7, 25.6, 25.3, 19.2, 11.5, 7, 4.6, 3.3, 2.4, 1.7, 1.3, 1.1, 0.0, 0.0, 0.0, 0.0, 1.1, 1.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [27.4, 24, 15.7, 56.5, 152.2, 209.4, 181.4, 128.4, 72.2, 43.2, 40.8, 19.6, 8.9, 8.1, 9.9, 12.8, 9.7, 4.6, 1.8, 0.9, 0.6, 0.4, 0.3, 0.2, 0.2, 0.0, 0.0, 0.0, 0.0, 0.8, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]])

    N5055 = np.array([[0.2, 0.7, 1.2, 1.7, 2.2, 2.7, 3.2, 3.7, 4.2, 4.7, 5.1, 5.6, 6.1, 6.6, 7.1, 7.6, 8.1, 8.6, 9.1, 9.5, 10.0, 10.5, 11.0, 11.5, 12.0, 12.5, 13.0, 13.5, 14.0, 14.4, 14.9, 15.4, 15.9, 16.4, 16.9, 17.4, 17.9, 18.4, 18.9, 19.3, 19.8, 20.3, 20.8],
    [5.6, 5.8, 5.9, 5.9, 6.2, 6.6, 6.4, 6.2, 6.5, 7.2, 8.2, 8.7, 8.7, 8.5, 8.5, 8.6, 8.5, 7.9, 7.5, 7.4, 7.2, 7.3, 7.5, 7.3, 6.7, 6.4, 6.2, 5.7, 5.1, 4.5, 4.2, 3.9, 3.7, 3.2, 2.8, 2.6, 2.5, 2.4, 2.1, 1.7, 1.4, 1.3, 1.2],
    [0.7, 0.5, 0.4, 0.2, 0.3, 0.4, 0.4, 0.5, 0.5, 0.6, 0.7, 0.5, 0.5, 0.6, 0.5, 0.5, 0.5, 0.4, 0.4, 0.5, 0.7, 0.8, 0.9, 0.7, 0.5, 0.5, 0.5, 0.5, 0.4, 0.4, 0.5, 0.6, 0.5, 0.4, 0.4, 0.4, 0.3, 0.3, 0.2, 0.2, 0.2, 0.2, 0.2],
    [142.7, 98.8, 62.2, 43.7, 36.6, 32.1, 25.3, 20.3, 19.1, 18.6, 18.8, 17.3, 13.4, 10.9, 10.3, 10.0, 8.7, 6.2, 4.3, 3.1, 2.1, 1.5, 1.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [19.4, 16.1, 8.5, 3.9, 2.3, 2.5, 2.7, 2.3, 2.1, 2.1, 2.1, 1.8, 1.1, 1.0, 1.0, 1.2, 1.3, 1.0, 0.8, 0.6, 0.5, 0.4, 0.4, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [4742.4, 1627.7, 987.4, 758.0, 569.7, 417.9, 325.4, 264.3, 230.7, 194.9, 169.5, 150.6, 133.4, 109.1, 94.0, 84.5, 75.3, 62.6, 52.0, 44.5, 40.8, 36.8, 33.4, 60.4, 59.2, 24.8, 21.2, 18.7, 17.3, 18.0, 13.8, 13.2, 11.8, 10.7, 9.8, 9.2, 8.4, 8.4, 7.7, 7.4, 7.1, 13.2, 12.4],
    [251.1, 11.4, 4.6, 2.2, 1.7, 1.1, 0.9, 0.8, 0.7, 0.5, 0.4, 0.4, 0.4, 0.3, 0.2, 0.3, 0.3, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 5.4, 5.7, 0.1, 0.1, 0.1, 0.1, 0.5, 0.0, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.1, 0.2, 0.2, 1.0, 1.4],
    [249.8, 267.2, 238.7, 192.7, 162.5, 135.2, 104.4, 81.3, 70.8, 67.2, 70.7, 73.4, 63, 51.4, 46.6, 43.3, 38.5, 29.6, 22.7, 20.1, 16.8, 12.6, 10.2, 8.6, 6.7, 5.3, 4.4, 3.7, 3.2, 2.8, 2.5, 2.2, 2, 1.6, 1.2, 1, 1.1, 1.4, 1.2, 0.0, 0.0, 0.0, 0.0],
    [28.4, 20.9, 13.2, 8.7, 5.9, 5.8, 5.9, 5.4, 5.2, 5.8, 7.3, 9.3, 6.4, 4.6, 5.3, 6.4, 6.4, 3.6, 2.1, 3.6, 3.7, 2.6, 2.0, 1.4, 0.8, 0.5, 0.4, 0.4, 0.3, 0.3, 0.3, 0.3, 0.3, 0.2, 0.1, 0.2, 0.4, 0.9, 0.8, 0.0, 0.0, 0.0, 0.0]])

    N6946 = np.array([[0.1, 0.4, 0.7, 1.0, 1.3, 1.6, 1.9, 2.1, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.1, 4.4, 4.7, 5.0, 5.3, 5.6, 5.9, 6.1, 6.4, 6.7, 7.0, 7.3, 7.6, 7.9, 8.2, 8.4, 8.7, 9.0, 9.3, 9.6, 9.9, 10.2, 10.4, 10.7, 11.0, 11.3, 11.6],
    [6.1, 6.4, 6.4, 5.9, 5.5, 5.5, 5.8, 6.4, 6.9, 7.4, 7.8, 8.2, 8.7, 9.3, 9.5, 9.5, 9.6, 9.6, 9.5, 9.3, 9.3, 9.3, 9.1, 8.8, 8.4, 8.1, 8.0, 8.0, 7.9, 7.4, 6.9, 6.3, 5.8, 5.3, 4.8, 4.5, 4.1, 3.9, 3.7, 3.6, 3.6],
    [1.1, 1.1, 1.0, 0.7 ,0.6, 0.5, 0.4, 0.4 ,0.4, 0.4, 0.4, 0.5, 0.6, 0.8, 1.0, 1.1, 1.1, 1.1, 1.1, 0.9, 0.8, 0.8, 0.8, 0.8, 0.8, 0.9, 1.0, 1.1, 1.0, 0.9, 0.8, 0.7, 0.6, 0.6, 0.5, 0.5, 0.5, 0.5, 0.4, 0.4, 0.4],
    [548.6, 390.7, 214.2, 110.4, 64.2, 46.4, 39.9, 37.9, 36.9, 35.2, 32.4, 29.7, 28.1, 27.5, 25.9, 22.8, 19.2, 15.6, 12.2, 9.4, 7.5, 6.3, 5.1, 4.1, 3.2, 2.3, 1.6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [68.4, 81.2, 54.9, 31.1, 17.4, 10.6, 7.0, 5.4, 4.9, 4.3, 3.5, 3.3, 3.9, 4.9, 5.7, 5.6, 5.1, 4.4, 3.7, 2.7, 1.9, 1.5, 1.4, 1.4, 1.4, 1.5, 1.3, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    [5937.7, 1125.9, 708.0, 496.6, 405.6, 390.4, 356.3, 313.6, 287.1, 258.4, 236.2, 212.1, 200.8, 276.6, 187.3, 159.7, 146.3, 127.2, 110.4, 97.4, 184.5, 105.3, 99.9, 85.8, 71.1, 59.1, 49.0, 40.7, 43.0, 35.2, 35.4, 30.4, 22.6, 26.8, 36.6, 86.9, 20.4, 17.3, 17.2, 17.8, 12.9],
    [348.7, 7.7, 4.5, 2.6, 1.8, 4.7, 1.9, 0.9, 1.0, 1.0, 1.5, 0.9, 1.5, 12.8, 1.8, 1.3, 1.0, 0.7, 0.6, 0.4, 10.5, 2.0, 4.4, 3.8, 1.1, 0.5, 0.4, 0.3, 1.0, 0.9, 0.9, 1.1, 0.5, 0.8, 1.9, 6.3, 0.5, 0.4, 0.5, 0.7, 0.3],
    [2286, 1549.6, 771.9, 373.6, 227.6, 186.1, 184.5, 197.2, 205.9, 202.3, 198.2, 201.7, 210.5, 221.8, 230.3, 230.6, 210.8, 175, 138.6, 110, 92, 79.9, 73.2, 74, 80.4, 77.7, 64.9, 45.4, 31.3, 23.1, 18.8, 15.7, 12.9, 10.4, 7.9, 5.6, 3.8, 2.7, 2.3, 2, 1.5],
    [553.4, 487.4, 265, 113.4, 48.6, 27, 21.2, 21.3, 25.9, 31.2, 24.7, 33.8, 31.9, 42.6, 60.7, 65.8, 57.1, 42.8, 31.8, 25.6, 23.9, 22.7, 22.9, 27.8, 35.7, 36.6, 28.7, 17.9, 9.9, 6.3, 5.9, 5.5, 4.8, 3.7, 2.4, 1.5, 1.3, 1.3, 1.3, 1.4, 1.4]])

    if LowVvir and not SFR:
        if HI and H2:
            ax.errorbar(N3351[0,:], N3351[1,:]+N3351[3,:], N3351[2,:]+N3351[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3627[0,:], N3627[1,:]+N3627[3,:], N3627[2,:]+N3627[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5055[0,:], N5055[1,:]+N5055[3,:], N5055[2,:]+N5055[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N6946[0,:], N6946[1,:]+N6946[3,:], N6946[2,:]+N6946[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        elif HI and not H2:
            ax.errorbar(N3351[0,:], N3351[1,:], N3351[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3627[0,:], N3627[1,:], N3627[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5055[0,:], N5055[1,:], N5055[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N6946[0,:], N6946[1,:], N6946[2,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        elif H2 and not HI:
            ax.errorbar(N3351[0,:], N3351[3,:], N3351[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3627[0,:], N3627[3,:], N3627[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5055[0,:], N5055[3,:], N5055[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N6946[0,:], N6946[3,:], N6946[4,:], elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        else:
            ax.errorbar(N3351[0,:], N3351[5,:]*0.61/0.66, N3351[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N3627[0,:], N3627[5,:]*0.61/0.66, N3627[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N5055[0,:], N5055[5,:]*0.61/0.66, N5055[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
            ax.errorbar(N6946[0,:], N6946[5,:]*0.61/0.66, N6946[6,:]*0.61/0.66, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
    elif LowVvir and SFR:
        ax.errorbar(N3351[0,:], N3351[7,:]*1e-4*0.63/0.67, N3351[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        ax.errorbar(N3627[0,:], N3627[7,:]*1e-4*0.63/0.67, N3627[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        ax.errorbar(N5055[0,:], N5055[7,:]*1e-4*0.63/0.67, N5055[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)
        ax.errorbar(N6946[0,:], N6946[7,:]*1e-4*0.63/0.67, N6946[8,:]*1e-4*0.63/0.67, elinewidth=1, ecolor=c, alpha=alpha, color=c, lw=lw)


def md14data(h=0.678):
    # Data from Madau & Dickinson (2014)
    data = np.array([[0.01, 0.1, -1.82, 0.09, 0.02],
        [0.2, 0.4, -1.50, 0.05, 0.05],
        [0.4, 0.6, -1.39, 0.15, 0.08],
        [0.6, 0.8, -1.20, 0.31, 0.13],
        [0.8, 1.2, -1.25, 0.31, 0.13],
        [0.05, 0.05, -1.77, 0.08, 0.09],
        [0.05, 0.2, -1.75, 0.18, 0.18],
        [0.2, 0.4, -1.55, 0.12, 0.12],
        [0.4, 0.6, -1.44, 0.1, 0.1],
        [0.6, 0.8, -1.24, 0.1, 0.1],
        [0.8, 1.0, -0.99, 0.09, 0.08],
        [1, 1.2, -0.94, 0.09, 0.09],
        [1.2, 1.7, -0.95, 0.15, 0.08],
        [1.7, 2.5, -0.75, 0.49, 0.09],
        [2.5, 3.5, -1.04, 0.26, 0.15],
        [3.5, 4.5, -1.69, 0.22, 0.32],
        [0.92, 1.33, -1.02, 0.08, 0.08],
        [1.62, 1.88, -0.75, 0.12, 0.12],
        [2.08, 2.37, -0.87, 0.09, 0.09],
        [1.9, 2.7, -0.75, 0.09, 0.11],
        [2.7, 3.4, -0.97, 0.11, 0.15],
        [3.8, 3.8, -1.29, 0.05, 0.05],
        [4.9, 4.9, -1.42, 0.06, 0.06],
        [5.9, 5.9, -1.65, 0.08, 0.08],
        [7, 7, -1.79, 0.1, 0.1],
        [7.9, 7.9, -2.09, 0.11, 0.11],
        [7, 7, -2, 0.1, 0.11],
        [0.03, 0.03, -1.72, 0.02, 0.03],
        [0.03, 0.03, -1.95, 0.2, 0.2],
        [0.4, 0.7, -1.34, 0.22, 0.11],
        [0.7, 1, -0.96, 0.15, 0.19],
        [1, 1.3, -0.89, 0.27, 0.21],
        [1.3, 1.8, -0.91, 0.17, 0.21],
        [1.8, 2.3, -0.89, 0.21, 0.25],
        [0.4, 0.7, -1.22, 0.08, 0.11],
        [0.7, 1, -1.1, 0.1, 0.13],
        [1, 1.3, -0.96, 0.13, 0.2],
        [1.3, 1.8, -0.94, 0.13, 0.18],
        [1.8, 2.3, -0.8, 0.18, 0.15],
        [0, 0.3, -1.64, 0.09, 0.11],
        [0.3, 0.45, -1.42, 0.03, 0.04],
        [0.45, 0.6, -1.32, 0.05, 0.05],
        [0.6, 0.8, -1.14, 0.06, 0.06],
        [0.8, 1, -0.94, 0.05, 0.06],
        [1, 1.2, -0.81, 0.04, 0.05],
        [1.2, 1.7, -0.84, 0.04, 0.04],
        [1.7, 2, -0.86, 0.02, 0.03],
        [2, 2.5, -0.91, 0.09, 0.12],
        [2.5, 3, -0.86, 0.15, 0.23],
        [3, 4.2, -1.36, 0.23, 0.5]])

    z = (data[:,0]+data[:,1])/2
    z_err = (data[:,1]-data[:,0])/2
    SFRD = data[:,2] - 0.2 + np.log10(h/0.7) # converts from Salpeter to Chabrier
    SFRD_err_high = data[:,3]
    SFRD_err_low = data[:,4]
    return z, z_err, SFRD, SFRD_err_high, SFRD_err_low


def SFRD_obs(h, alpha=0.3, ax=None, plus=0):
    ### Observational data as first compiled in Somerville et al. (2001) ###
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
                              
    ObsRedshift = ObsSFRdensity[:,0]
    xErrLo = ObsSFRdensity[:,0]-ObsSFRdensity[:,2]
    xErrHi = ObsSFRdensity[:,3]-ObsSFRdensity[:,0]

    ObsSFR = np.log10(ObsSFRdensity[:,1]*h/0.7)
    yErrLo = np.log10(ObsSFRdensity[:,1]*h/0.7)-np.log10(ObsSFRdensity[:,4]*h/0.7)
    yErrHi = np.log10(ObsSFRdensity[:,5]*h/0.7)-np.log10(ObsSFRdensity[:,1]*h/0.7)
    ### ================= ###

    # Add Madau & Dickinson data
    z, z_err, SFRD, SFRD_err_high, SFRD_err_low = md14data(h)

    # Driver et al. (2018) data
    D18 = np.array([[0.85, 0.02, 0.08, -1.95, 0.03, 0.00, 0.07, 0.00],
                  [1.52, 0.06, 0.14, -1.82, 0.03, 0.01, 0.05, 0.01],
                  [2.16, 0.14, 0.20, -1.90, 0.02, 0.00, 0.04, 0.00],
                  [2.90, 0.20, 0.28, -1.77, 0.01, 0.00, 0.05, 0.00],
                  [3.65, 0.28, 0.36, -1.75, 0.01, 0.00, 0.06, 0.01],
                  [4.35, 0.36, 0.45, -1.79, 0.01, 0.01, 0.06, 0.01],
                  [5.11, 0.45, 0.56, -1.73, 0.04, 0.01, 0.09, 0.03],
                  [5.86, 0.56, 0.68, -1.56, 0.05, 0.00, 0.07, 0.02],
                  [6.59, 0.68, 0.82, -1.42, 0.06, 0.01, 0.06, 0.04],
                  [7.36, 0.82, 1.00, -1.29, 0.05, 0.00, 0.07, 0.01],
                  [8.11, 1.00, 1.20, -1.31, 0.04, 0.00, 0.05, 0.01],
                  [8.82, 1.20, 1.45, -1.27, 0.03, 0.00, 0.06, 0.02],
                  [9.50, 1.45, 1.75, -1.17, 0.02, 0.00, 0.06, 0.03],
                  [10.21, 1.75, 2.20, -1.30, 0.04, 0.01, 0.07, 0.06],
                  [10.78, 2.20, 2.60, -1.29, 0.04, 0.01, 0.04, 0.09],
                  [11.29, 2.60, 3.25, -1.28, 0.04, 0.01, 0.04, 0.11],
                  [11.69, 3.25, 3.75, -1.33, 0.03, 0.01, 0.03, 0.08],
                  [11.95, 3.75, 4.25, -1.42, 0.04, 0.04, 0.05, 0.02],
                  [12.19, 4.25, 5.00, -1.45, 0.03, 0.04, 0.04, 0.04]])

    if ax is None: ax = plt.gca()
    ax.errorbar(ObsRedshift+plus, ObsSFR, yerr=[yErrLo, yErrHi], xerr=[xErrLo, xErrHi], color='purple', lw=2.0, alpha=alpha, ls='none', label=r'Somerville et al.~(2001)', ms=0)
    ax.errorbar(z+plus, SFRD, yerr=[SFRD_err_low, SFRD_err_high], xerr=z_err, color='cyan', lw=2.0, alpha=alpha, ls='none', label=r'Madau \& Dickinson (2014)', ms=0)
    ax.errorbar((D18[:,1]+D18[:,2])/2.+plus, D18[:,3]+np.log10(h/0.7), yerr=np.sum(D18[:,5:],axis=1), xerr=(D18[:,1]-D18[:,2])/2., color='goldenrod', lw=2.0, alpha=alpha, ls='none', label=r'Driver et al.~(2018)', ms=0)


def z2tL(z, h=0.6774, Omega_M=0.3089, Omega_Lambda=0.6911, Omega_R=0, nele=100000):
    # Convert redshift z to look-backtime time tL.
    # nele is the number if elements used in the numerical integration
    
    if z<=0: return 0. # not designed for blueshifts!
    
    H_0 = 100*h
    Omega_k = 1 - Omega_R - Omega_M - Omega_Lambda # Curvature density as found from the radiation, matter and dark energy energy densities.
    Mpc_km = 3.08567758e19 # Number of km in 1 Mpc
    yr_s = 60*60*24*365.24 # Number of seconds in a year

    # Create the z-array that is to be integrated over and matching integrand
    zprime = np.linspace(0, z, nele)
    integrand = 1./((1+zprime)*np.sqrt(Omega_R*(1+zprime)**4 + Omega_M*(1+zprime)**3 + Omega_k*(1+zprime)**2 + Omega_Lambda))

    # Numerically integrate trapezoidally
    integrated = 0.5 * np.sum(np.diff(zprime)*(integrand[:-1] + integrand[1:]))
    tL = np.divide(integrated*Mpc_km, H_0*yr_s*1e9)

    return tL # Gives look-back time in Gyr

def z2dA(z, H_0=67.74, Omega_R=0, Omega_M=0.3089, Omega_Lambda=0.6911, nele=100000):
    # Convert redshift to an angular-diameter distance
    c = 299792.458 # Speed of light in km/s
    Omega_k = 1. - Omega_R - Omega_M - Omega_Lambda
    zprime = np.linspace(0,z,nele)
    integrand = 1./np.sqrt(Omega_R*(1+zprime)**4 + Omega_M*(1+zprime)**3 + Omega_k*(1+zprime)**2 + Omega_Lambda)
    intval = 0.5*np.sum(np.diff(zprime)*(integrand[:-1] + integrand[1:]))
    d_A = 1e6*c*intval / (H_0*(1+z)) # Angular-diameter distance in pc
    return d_A


def comoving_distance(z, H_0=67.74, Omega_R=0, Omega_M=0.3089, Omega_L=0.6911):
    # calculate co-moving distance from redshift [Mpc]
    zprime = np.linspace(0,z,10000)
    E = np.sqrt(Omega_R*(1+zprime)**4 + Omega_M*(1+zprime)**3 + Omega_L)
    integrand = 1/E
    integral = np.sum(0.5*(integrand[1:]+integrand[:-1])*np.diff(zprime))
    c = 299792.458
    return c/H_0 * integral


def return_fraction_and_SN_ChabrierIMF(m_min=0.1, m_max=100.0, A=0.84342328, k=0.23837777, m_c=0.08, sigma=0.69, ratio_Ia_II=0.2):
    # array of mass values covering the full range that stars are assumed to fall within
    m = np.linspace(m_min, m_max, 10001) # solar masses

    # Chabrier IMF
    IMF = k * m**(-2.3)
    flow = (m<=1)
    IMF[flow] = A/m[flow] * np.exp(-np.log10(m[flow]/m_c)**2/(2*sigma**2))

    # lifetimes of stars based on simple scaling + slope of the main sequence of the HR diagram, with the Sun assumed to have a lifetime of 10 Gyr
    lifetime = 10 * m**(-2.5) # Gyr

    # mass of remnants at the end of evolution
    m_remnant = 1.0*m
    f1, f2, f3 = (m>=1)*(m<=7), (m>7)*(m<8), (m>=8)*(m<=50)
    m_remnant[f1] = 0.444 + 0.084*m[f1]
    m_remnant[f2] = 0.419 + 0.109*m[f2]
    m_remnant[f3] = 1.4
    m_returned = m - m_remnant

    # fraction of returned mass, integrating from the highest mass down
    dm = m[1]-m[0]
    integrand = (m_returned*IMF)[::-1]
    returned_mass_fraction_integrated = 0.5*dm*np.cumsum(integrand[:-1] + integrand[1:])[::-1]
    returned_mass_fraction_integrated = np.append(returned_mass_fraction_integrated, 0)

    # integrate the IMF from the highest mass down to get the cumulative number density of stars
    int_IMF = k/1.3 * m**(-1.3)
    int_IMF -= np.interp(50, m, int_IMF)

    # cumulative number density of supernovae in a stellar population (integrating from the highest mass down)
    ncum_SN = np.zeros(len(m))
    ncum_SN[f3] = 1.0*int_IMF[f3]
    ncum_SN[f1+f2] = ncum_SN[f3][0] + ratio_Ia_II/15.3454 * (int_IMF[f1+f2] - int_IMF[f3][0])
    ncum_SN[m<1] = np.max(ncum_SN)

    return m, lifetime, returned_mass_fraction_integrated, ncum_SN

def read_hdf(frp, snap_num = None, param = None):
    property = h5.File(frp,'r')
    return np.array(property[snap_num][param])

def read_sage_hdf(frp, snap_num=None, fields=None):
    # Get the data structure defined in galdtype_darksage
    Galdesc = galdtype_sage()
    
    # If fields are provided, filter Galdesc to include only specified fields
    if fields is not None:
        Galdesc = [field for field in Galdesc if field[0] in fields]
    
    # Open the HDF file and select the specified snapshot
    with h5.File(frp, 'r') as property:
        snapshot_data = property[snap_num]
        
        # Extract each specified parameter
        data = {}
        for field_name, field_type in Galdesc:
            try:
                data[field_name] = np.array(snapshot_data[field_name])
            except KeyError:
                print(f"Parameter '{field_name}' not found in snapshot '{snap_num}'")
    
    return data

def galdtype_sage(Nannuli=30, Nage=1):
    floattype = np.float32
    
    # Define array dimensions based on age bins for stars
    disc_arr_dim = (Nannuli, Nage) if Nage > 1 else Nannuli
    bulge_arr_type = (floattype, Nage) if Nage > 1 else floattype

    # Define the galaxy description structure
    Galdesc = [
        ('BlackHoleMass', np.float32),                  
        ('BulgeMass', np.float32),                  
        ('CentralGalaxyIndex', np.int32),                    
        ('CentralMvir', np.float32),                  
        ('ColdGas', np.float32),                  
        ('Cooling', np.float32),                  
        ('DiskRadius', np.float32),
        ('EjectedMass', np.float32),                                      
        ('GalaxyIndex', np.int64),                                       
        ('Heating', np.float32),                  
        ('HotGas', np.float32),                  
        ('IntraClusterStars', np.float32),                  
        ('Len', np.int32),                                      
        ('MetalsBulgeMass', np.float32),                  
        ('MetalsColdGas', np.float32),                  
        ('MetalsEjectedMass', np.float32),                  
        ('MetalsHotGas', np.float32),                  
        ('MetalsIntraClusterStars', np.float32),                  
        ('MetalsStellarMass', np.float32),
        ('Mvir', np.float32), 
        ('OutflowRate', np.float32),                 
        ('Pos', (np.float32, 3)),
        ('QuasarModeBHaccretionMass', np.float32),             
        ('Rvir', np.float32),
        ('SAGEHaloIndex', np.int32),
        ('SAGETreeIndex', np.int32),                               
        ('SfrBulge', np.float32),                  
        ('SfrBulgeZ', np.float32),                  
        ('SfrDisk', np.float32),                  
        ('SfrDiskZ', np.float32),
        ('SimulationHaloIndex', np.int32),
        ('SnapNum', np.int32),                    
        ('Spin', (np.float32, 3)),                  
        ('StellarMass', np.float32),
        ('TimeOfLastMajorMerger', np.int32),
        ('TimeOfLastMinorMerger', np.int32),                                     
        ('Type', np.int32),                     
        ('VelDisp', np.float32),                 
        ('Vel', (np.float32, 3)),
        ('Vmax', np.float32),                               
        ('Vvir', np.float32),                  
        ('dT', np.float32),  
        ('infallMvir', np.float32),
        ('infallVmax', np.float32), 
        ('infallVvir', np.float32),                 
        ('mergeIntoID', np.int32),                    
        ('mergeIntoSnapNum', np.int32),                    
        ('mergeType', np.int32)
    ]
    
    return Galdesc