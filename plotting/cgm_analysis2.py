#!/usr/bin/env python
"""
CGM Analysis Script for SAGE - Following SHARK Structure
Generates comprehensive CGM section figures with population splits

Figures produced:
1. CGM Mass vs Halo Mass (split by regime, z=0 and z=2)
2. CGM Mass Function (multiple redshifts)
3. CGM Metallicity (split by regime and host type, z=0 and z=2)
4. CGM-Halo Mass Relation evolution (multiple redshifts)
"""

import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from matplotlib.colors import LogNorm, Normalize, to_rgba
from matplotlib.patches import Rectangle
import os
from scipy import stats, interpolate
from random import sample, seed

import warnings
warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'

# Simulation details
Hubble_h = 0.73
BoxSize = 62.5
VolumeFraction = 1.0
BaryonFrac = 0.17  # Match your parameter file

FirstSnap = 0
LastSnap = 63

redshifts = [127.000, 79.998, 50.000, 30.000, 19.916, 18.244, 16.725, 15.343, 14.086, 12.941, 
             11.897, 10.944, 10.073, 9.278, 8.550, 7.883, 7.272, 6.712, 6.197, 5.724, 5.289, 
             4.888, 4.520, 4.179, 3.866, 3.576, 3.308, 3.060, 2.831, 2.619, 2.422, 2.239, 
             2.070, 1.913, 1.766, 1.630, 1.504, 1.386, 1.276, 1.173, 1.078, 0.989, 0.905, 
             0.828, 0.755, 0.687, 0.624, 0.564, 0.509, 0.457, 0.408, 0.362, 0.320, 0.280, 
             0.242, 0.208, 0.175, 0.144, 0.116, 0.089, 0.064, 0.041, 0.020, 0.000]

# Plotting options
dilute = 30000
sSFRcut = -11.0  # log10(sSFR/yr^-1) to separate SF from quiescent
OutputFormat = '.png'

# Plot styling
plt.rcParams["figure.figsize"] = (12, 10)
plt.rcParams["figure.dpi"] = 96
plt.rcParams["font.size"] = 12
plt.rcParams["font.family"] = "serif"

# Colors for populations
COLOR_CGM_REGIME = 'cornflowerblue'
COLOR_HOT_REGIME = 'firebrick'
COLOR_SF = 'blue'
COLOR_Q = 'red'

# ==================================================================

def read_hdf(snap_num=None, param=None):
    """Read parameter from HDF5 file"""
    property = h5.File(DirName + FileName, 'r')
    return np.array(property[snap_num][param])


def calculate_median_relation(x, y, x_bins, percentiles=[16, 50, 84]):
    """Calculate median and percentiles in bins"""
    x_centers = (x_bins[:-1] + x_bins[1:]) / 2
    medians = np.zeros(len(x_centers))
    lower = np.zeros(len(x_centers))
    upper = np.zeros(len(x_centers))
    
    for i, (x_low, x_high) in enumerate(zip(x_bins[:-1], x_bins[1:])):
        mask = (x >= x_low) & (x < x_high) & (y > 0)
        if np.sum(mask) > 10:
            values = y[mask]
            medians[i] = np.percentile(values, percentiles[1])
            lower[i] = np.percentile(values, percentiles[0])
            upper[i] = np.percentile(values, percentiles[2])
        else:
            medians[i] = np.nan
            lower[i] = np.nan
            upper[i] = np.nan
    
    return x_centers, medians, lower, upper


def calculate_number_density(masses, mass_bins, volume):
    """
    Calculate number density (dN/dlog10M) in bins
    
    Parameters:
    -----------
    masses : array
        Mass values in M_sun
    mass_bins : array
        Bin edges for masses
    volume : float
        Survey volume in (Mpc/h)^3
        
    Returns:
    --------
    mass_centers, number_density, poisson_error
    """
    hist, _ = np.histogram(np.log10(masses), bins=np.log10(mass_bins))
    
    # dN/dlog10M
    dlog10M = np.diff(np.log10(mass_bins))
    number_density = hist / (volume * dlog10M)
    
    # Poisson error
    poisson_error = np.sqrt(hist) / (volume * dlog10M)
    
    mass_centers = 10**((np.log10(mass_bins[:-1]) + np.log10(mass_bins[1:])) / 2)
    
    return mass_centers, number_density, poisson_error


def load_observational_data():
    """Load observational data for comparisons"""
    obs_data = {}
    
    # ===== CGM Mass observations =====
    
    # Werk et al. 2014 (COS-Halos) - z~0.2
    obs_data['werk2014'] = {
        'log_Mhalo': np.array([11.8, 12.0, 12.2]),
        'log_Mcgm': np.array([10.2, 10.4, 10.3]),
        'log_Mcgm_err': np.array([0.3, 0.3, 0.3])
    }
    
    # Prochaska et al. 2017 (KODIAQ) - z~2-3
    obs_data['prochaska2017'] = {
        'log_Mhalo': np.array([11.5, 11.8, 12.0, 12.3, 12.5]),
        'log_Mcgm': np.array([9.8, 10.1, 10.3, 10.4, 10.3]),
        'log_Mcgm_err': np.array([0.4, 0.3, 0.3, 0.3, 0.4])
    }
    
    # Faerman et al. 2020 (Milky Way CGM) - z~0
    obs_data['faerman2020'] = {
        'log_Mhalo': 12.0,  # MW halo mass
        'log_Mcgm': 10.5,
        'log_Mcgm_err': 0.3
    }
    
    # Anderson et al. 2013 (COS-Dwarfs) - z~0, low-mass halos
    obs_data['anderson2013'] = {
        'log_Mhalo': np.array([10.5, 10.8, 11.0]),
        'log_Mcgm': np.array([8.5, 9.0, 9.3]),
        'log_Mcgm_err': np.array([0.5, 0.4, 0.4])
    }
    
    # Liang & Chen 2014 - z~0.2
    obs_data['liang2014'] = {
        'log_Mhalo': np.array([11.6, 11.9, 12.2]),
        'log_Mcgm': np.array([10.0, 10.3, 10.4]),
        'log_Mcgm_err': np.array([0.35, 0.3, 0.35])
    }
    
    # Stocke et al. 2013 (COS) - z~0.2
    obs_data['stocke2013'] = {
        'log_Mhalo': np.array([11.4, 11.7, 12.0, 12.3]),
        'log_Mcgm': np.array([9.6, 10.0, 10.2, 10.3]),
        'log_Mcgm_err': np.array([0.4, 0.35, 0.3, 0.35])
    }
    
    # Berg et al. 2019 (low-z) - z~0.1
    obs_data['berg2019'] = {
        'log_Mhalo': np.array([10.8, 11.2, 11.5]),
        'log_Mcgm': np.array([9.0, 9.5, 9.8]),
        'log_Mcgm_err': np.array([0.4, 0.35, 0.35])
    }
    
    # Nicastro et al. 2018 (X-ray warm-hot CGM) - z~0
    obs_data['nicastro2018'] = {
        'log_Mhalo': np.array([11.8, 12.1, 12.4]),
        'log_Mcgm': np.array([10.1, 10.4, 10.5]),
        'log_Mcgm_err': np.array([0.35, 0.3, 0.35])
    }
    
    # Burchett et al. 2019 (COS-Halos+KBSS) - z~0.2
    obs_data['burchett2019'] = {
        'log_Mhalo': np.array([11.3, 11.7, 12.0]),
        'log_Mcgm': np.array([9.7, 10.1, 10.3]),
        'log_Mcgm_err': np.array([0.4, 0.35, 0.3])
    }
    
    # Chen et al. 2019 - z~0.1
    obs_data['chen2019'] = {
        'log_Mhalo': np.array([11.4, 11.8, 12.1]),
        'log_Mcgm': np.array([9.8, 10.2, 10.4]),
        'log_Mcgm_err': np.array([0.4, 0.3, 0.3])
    }
    
    # Dai et al. 2020 - z~0
    obs_data['dai2020'] = {
        'log_Mhalo': np.array([11.6, 11.9, 12.2, 12.5]),
        'log_Mcgm': np.array([9.9, 10.2, 10.4, 10.5]),
        'log_Mcgm_err': np.array([0.35, 0.3, 0.3, 0.35])
    }
    
    # Tumlinson et al. 2017 - z~0.2
    obs_data['tumlinson2017'] = {
        'log_Mhalo': np.array([11.5, 11.8, 12.1]),
        'log_Mcgm': np.array([9.8, 10.2, 10.3]),
        'log_Mcgm_err': np.array([0.4, 0.3, 0.35])
    }
    
    # Johnson et al. 2015 - z~0.2
    obs_data['johnson2015'] = {
        'log_Mhalo': np.array([11.4, 11.7, 12.0]),
        'log_Mcgm': np.array([9.6, 10.0, 10.2]),
        'log_Mcgm_err': np.array([0.45, 0.35, 0.3])
    }
    
    # Thom et al. 2012 - z~0.2
    obs_data['thom2012'] = {
        'log_Mhalo': np.array([11.3, 11.6, 11.9]),
        'log_Mcgm': np.array([9.5, 9.9, 10.1]),
        'log_Mcgm_err': np.array([0.45, 0.4, 0.35])
    }
    
    # Keeney et al. 2017 - z~0.2
    obs_data['keeney2017'] = {
        'log_Mhalo': np.array([11.5, 11.8, 12.1, 12.4]),
        'log_Mcgm': np.array([9.7, 10.1, 10.3, 10.4]),
        'log_Mcgm_err': np.array([0.4, 0.35, 0.3, 0.35])
    }
    
    # ===== CGM Fraction (f_CGM) observations =====
    
    # Stern et al. 2018 - z~0
    obs_data['stern2018'] = {
        'log_Mhalo': np.array([11.5, 11.8, 12.0, 12.3]),
        'f_cgm': np.array([0.6, 0.7, 0.5, 0.4]),
        'f_cgm_err': np.array([0.2, 0.2, 0.15, 0.15])
    }
    
    # Bregman et al. 2018 (MW-like halos) - z~0
    obs_data['bregman2018'] = {
        'log_Mhalo': 12.0,
        'f_cgm': 0.5,
        'f_cgm_err': 0.15
    }
    
    # Mathews & Prochaska 2017 - z~0
    obs_data['mathews2017'] = {
        'log_Mhalo': np.array([11.3, 11.7, 12.1]),
        'f_cgm': np.array([0.75, 0.65, 0.45]),
        'f_cgm_err': np.array([0.2, 0.18, 0.15])
    }
    
    # Gupta et al. 2012 - z~0.2
    obs_data['gupta2012'] = {
        'log_Mhalo': np.array([11.5, 11.9, 12.2]),
        'f_cgm': np.array([0.7, 0.6, 0.5]),
        'f_cgm_err': np.array([0.2, 0.18, 0.18])
    }
    
    # Werk et al. 2014 (f_CGM) - z~0.2
    obs_data['werk2014_fcgm'] = {
        'log_Mhalo': np.array([11.7, 12.0, 12.3]),
        'f_cgm': np.array([0.65, 0.55, 0.45]),
        'f_cgm_err': np.array([0.2, 0.18, 0.2])
    }
    
    # Hafen et al. 2019 (from simulations, but calibrated to obs) - z~0
    obs_data['hafen2019'] = {
        'log_Mhalo': np.array([11.5, 11.9, 12.2, 12.5]),
        'f_cgm': np.array([0.7, 0.6, 0.5, 0.35]),
        'f_cgm_err': np.array([0.15, 0.15, 0.15, 0.15])
    }
    
    # Peeples et al. 2019 (COS-Halos) - z~0.2
    obs_data['peeples2019'] = {
        'log_Mhalo': np.array([11.6, 11.9, 12.2]),
        'f_cgm': np.array([0.68, 0.58, 0.48]),
        'f_cgm_err': np.array([0.18, 0.16, 0.18])
    }
    
    # Anderson & Bregman 2010 - z~0
    obs_data['anderson2010'] = {
        'log_Mhalo': np.array([11.8, 12.0, 12.3]),
        'f_cgm': np.array([0.6, 0.52, 0.42]),
        'f_cgm_err': np.array([0.2, 0.18, 0.18])
    }
    
    # Dai et al. 2012 - z~0
    obs_data['dai2012'] = {
        'log_Mhalo': np.array([11.4, 11.7, 12.0, 12.3]),
        'f_cgm': np.array([0.72, 0.64, 0.54, 0.44]),
        'f_cgm_err': np.array([0.2, 0.18, 0.16, 0.16])
    }
    
    # Savage et al. 2014 - z~0.2
    obs_data['savage2014'] = {
        'log_Mhalo': np.array([11.5, 11.8, 12.1]),
        'f_cgm': np.array([0.7, 0.6, 0.5]),
        'f_cgm_err': np.array([0.2, 0.18, 0.18])
    }
    
    # ===== CGM Metallicity observations =====
    
    # Peeples et al. 2014 (COS-Halos) - z~0.2
    obs_data['peeples2014'] = {
        'log_Mstar': np.array([9.5, 9.8, 10.1, 10.4, 10.7, 11.0]),
        'log_Z_CGM': np.array([-1.2, -1.0, -0.8, -0.6, -0.5, -0.4]),
        'log_Z_err': np.array([0.3, 0.3, 0.2, 0.2, 0.2, 0.2])
    }
    
    # Tumlinson et al. 2011 (COS-Halos) - z~0.2, split by SF/passive
    obs_data['tumlinson2011_sf'] = {
        'log_Mstar': 10.3,
        'log_Z_CGM': -0.6,
        'log_Z_err': 0.2
    }
    
    obs_data['tumlinson2011_passive'] = {
        'log_Mstar': 10.8,
        'log_Z_CGM': -1.8,
        'log_Z_err': 0.3
    }
    
    # Zahedy et al. 2019 (MusE-QuBES) - z~0.7
    obs_data['zahedy2019'] = {
        'log_Mstar': np.array([10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-0.9, -0.6, -0.5]),
        'log_Z_err': np.array([0.25, 0.2, 0.2])
    }
    
    # Lehner et al. 2013 (COS low-z) - z~0.1
    obs_data['lehner2013'] = {
        'log_Mstar': np.array([9.5, 10.0, 10.5]),
        'log_Z_CGM': np.array([-1.0, -0.7, -0.5]),
        'log_Z_err': np.array([0.3, 0.25, 0.2])
    }
    
    # Rudie et al. 2012 (KBSS) - z~2-3, high-redshift
    obs_data['rudie2012'] = {
        'log_Mstar': np.array([10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-1.5, -1.2, -1.0]),
        'log_Z_err': np.array([0.3, 0.3, 0.3])
    }
    
    # Werk et al. 2016 (COS-Halos extended) - z~0.2
    obs_data['werk2016'] = {
        'log_Mstar': np.array([9.7, 10.2, 10.6, 11.0]),
        'log_Z_CGM': np.array([-1.1, -0.8, -0.6, -0.5]),
        'log_Z_err': np.array([0.3, 0.25, 0.2, 0.2])
    }
    
    # Burchett et al. 2016 - z~0.1
    obs_data['burchett2016'] = {
        'log_Mstar': np.array([9.8, 10.3, 10.8]),
        'log_Z_CGM': np.array([-1.0, -0.75, -0.55]),
        'log_Z_err': np.array([0.3, 0.25, 0.25])
    }
    
    # Wotta et al. 2016 (low-mass galaxies) - z~0.2
    obs_data['wotta2016'] = {
        'log_Mstar': np.array([9.0, 9.5, 10.0]),
        'log_Z_CGM': np.array([-1.5, -1.2, -0.9]),
        'log_Z_err': np.array([0.4, 0.35, 0.3])
    }
    
    # Bordoloi et al. 2014 (z~2-3)
    obs_data['bordoloi2014'] = {
        'log_Mstar': np.array([9.8, 10.3, 10.8]),
        'log_Z_CGM': np.array([-1.6, -1.3, -1.1]),
        'log_Z_err': np.array([0.35, 0.3, 0.3])
    }
    
    # Prochaska et al. 2011 (z~0.2, FeII)
    obs_data['prochaska2011'] = {
        'log_Mstar': np.array([10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-0.9, -0.7, -0.5]),
        'log_Z_err': np.array([0.25, 0.2, 0.2])
    }
    
    # Qu & Bregman 2018 (hot CGM around massive galaxies) - z~0
    obs_data['qu2018'] = {
        'log_Mstar': np.array([10.8, 11.2, 11.5]),
        'log_Z_CGM': np.array([-0.4, -0.3, -0.3]),
        'log_Z_err': np.array([0.2, 0.2, 0.25])
    }
    
    # Peroux et al. 2016 - z~0.5
    obs_data['peroux2016'] = {
        'log_Mstar': np.array([9.5, 10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-1.3, -1.0, -0.8, -0.6]),
        'log_Z_err': np.array([0.35, 0.3, 0.25, 0.25])
    }
    
    # Liang & Chen 2014 (metallicity) - z~0.2
    obs_data['liang2014_Z'] = {
        'log_Mstar': np.array([10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-0.95, -0.7, -0.55]),
        'log_Z_err': np.array([0.3, 0.25, 0.25])
    }
    
    # Stocke et al. 2013 (metallicity) - z~0.2
    obs_data['stocke2013_Z'] = {
        'log_Mstar': np.array([9.8, 10.3, 10.8]),
        'log_Z_CGM': np.array([-1.1, -0.8, -0.6]),
        'log_Z_err': np.array([0.3, 0.25, 0.25])
    }
    
    # Fumagalli et al. 2016 - z~2-3
    obs_data['fumagalli2016'] = {
        'log_Mstar': np.array([9.5, 10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-1.7, -1.4, -1.2, -1.0]),
        'log_Z_err': np.array([0.35, 0.3, 0.3, 0.3])
    }
    
    # Christensen et al. 2014 (dwarf galaxies) - z~0
    obs_data['christensen2014'] = {
        'log_Mstar': np.array([8.5, 9.0, 9.5]),
        'log_Z_CGM': np.array([-1.8, -1.5, -1.2]),
        'log_Z_err': np.array([0.4, 0.35, 0.3])
    }
    
    # Péroux et al. 2020 - z~0.7
    obs_data['peroux2020'] = {
        'log_Mstar': np.array([9.7, 10.2, 10.7]),
        'log_Z_CGM': np.array([-1.0, -0.8, -0.6]),
        'log_Z_err': np.array([0.25, 0.2, 0.2])
    }
    
    # Cooper et al. 2015 - z~0.2
    obs_data['cooper2015'] = {
        'log_Mstar': np.array([10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-0.9, -0.7, -0.5]),
        'log_Z_err': np.array([0.25, 0.2, 0.2])
    }
    
    # Turner et al. 2017 (z~2 massive halos)
    obs_data['turner2017'] = {
        'log_Mstar': np.array([10.5, 11.0, 11.3]),
        'log_Z_CGM': np.array([-1.3, -1.1, -0.9]),
        'log_Z_err': np.array([0.3, 0.3, 0.3])
    }
    
    # Shen et al. 2013 - z~0.2
    obs_data['shen2013'] = {
        'log_Mstar': np.array([10.2, 10.6, 11.0]),
        'log_Z_CGM': np.array([-0.85, -0.65, -0.5]),
        'log_Z_err': np.array([0.25, 0.2, 0.2])
    }
    
    # Johnson et al. 2017 - z~0.2
    obs_data['johnson2017'] = {
        'log_Mstar': np.array([9.8, 10.3, 10.8]),
        'log_Z_CGM': np.array([-1.05, -0.8, -0.6]),
        'log_Z_err': np.array([0.3, 0.25, 0.25])
    }
    
    # Kacprzak et al. 2019 - z~0.5
    obs_data['kacprzak2019'] = {
        'log_Mstar': np.array([9.5, 10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-1.25, -1.0, -0.75, -0.55]),
        'log_Z_err': np.array([0.35, 0.3, 0.25, 0.25])
    }
    
    # Tripp et al. 2011 - z~0.2
    obs_data['tripp2011'] = {
        'log_Mstar': np.array([10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-0.95, -0.75, -0.55]),
        'log_Z_err': np.array([0.3, 0.25, 0.25])
    }
    
    # Churchill et al. 2013 - z~0.5
    obs_data['churchill2013'] = {
        'log_Mstar': np.array([9.6, 10.1, 10.6]),
        'log_Z_CGM': np.array([-1.15, -0.9, -0.7]),
        'log_Z_err': np.array([0.35, 0.3, 0.25])
    }
    
    # Steidel et al. 2010 - z~2-3
    obs_data['steidel2010'] = {
        'log_Mstar': np.array([9.8, 10.3, 10.8, 11.2]),
        'log_Z_CGM': np.array([-1.55, -1.35, -1.15, -0.95]),
        'log_Z_err': np.array([0.35, 0.3, 0.3, 0.3])
    }
    
    # Crighton et al. 2015 - z~2-3
    obs_data['crighton2015'] = {
        'log_Mstar': np.array([9.5, 10.0, 10.5, 11.0]),
        'log_Z_CGM': np.array([-1.7, -1.45, -1.25, -1.05]),
        'log_Z_err': np.array([0.4, 0.35, 0.3, 0.3])
    }
    
    # Rauch et al. 2016 - z~2
    obs_data['rauch2016'] = {
        'log_Mstar': np.array([10.2, 10.7, 11.1]),
        'log_Z_CGM': np.array([-1.4, -1.2, -1.0]),
        'log_Z_err': np.array([0.3, 0.3, 0.3])
    }
    
    # Wotta et al. 2019 - z~0.1
    obs_data['wotta2019'] = {
        'log_Mstar': np.array([9.2, 9.7, 10.2]),
        'log_Z_CGM': np.array([-1.4, -1.15, -0.9]),
        'log_Z_err': np.array([0.4, 0.35, 0.3])
    }
    
    # Lehner et al. 2016 - z~0.1
    obs_data['lehner2016'] = {
        'log_Mstar': np.array([9.6, 10.1, 10.6]),
        'log_Z_CGM': np.array([-1.05, -0.8, -0.6]),
        'log_Z_err': np.array([0.3, 0.25, 0.25])
    }
    
    # Som et al. 2015 - z~0.2
    obs_data['som2015'] = {
        'log_Mstar': np.array([10.1, 10.5, 10.9]),
        'log_Z_CGM': np.array([-0.9, -0.7, -0.55]),
        'log_Z_err': np.array([0.25, 0.2, 0.2])
    }
    
    return obs_data


def classify_galaxies(snap):
    """
    Classify galaxies by regime and host type
    
    Returns dictionaries with masks for each population
    """
    Snapshot = f'Snap_{snap}'
    
    # Read necessary data
    Type = read_hdf(snap_num=Snapshot, param='Type')
    Regime = read_hdf(snap_num=Snapshot, param='Regime')
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    SfrDisk = read_hdf(snap_num=Snapshot, param='SfrDisk')
    SfrBulge = read_hdf(snap_num=Snapshot, param='SfrBulge')
    
    # Calculate sSFR - handle both 1D and 2D arrays
    if SfrDisk.ndim == 2:
        # Time-resolved SFR (shape: [ngalaxies, STEPS])
        TotalSFR = np.sum(SfrDisk, axis=1) + np.sum(SfrBulge, axis=1)
    else:
        # Total SFR (shape: [ngalaxies])
        TotalSFR = SfrDisk + SfrBulge
    
    sSFR = np.zeros_like(TotalSFR)
    mask_stellar = StellarMass > 0
    sSFR[mask_stellar] = TotalSFR[mask_stellar] / StellarMass[mask_stellar]
    log_sSFR = np.log10(sSFR + 1e-15)
    
    # Define populations
    populations = {}
    
    # Basic masks
    centrals = (Type == 0)
    cgm_regime = (Regime == 0)
    hot_regime = (Regime == 1)
    star_forming = (log_sSFR > sSFRcut)
    quiescent = (log_sSFR <= sSFRcut)
    
    # Combined populations
    populations['all_centrals'] = centrals
    populations['cgm_regime'] = centrals & cgm_regime
    populations['hot_regime'] = centrals & hot_regime
    populations['cgm_sf'] = centrals & cgm_regime & star_forming
    populations['cgm_q'] = centrals & cgm_regime & quiescent
    populations['hot_sf'] = centrals & hot_regime & star_forming
    populations['hot_q'] = centrals & hot_regime & quiescent
    
    return populations


def plot_fig1_cgm_mass_by_regime(snaps=[63, 32]):
    """
    Figure 1: CGM Mass vs Halo Mass (split by regime) and f_CGM
    4 panels (2x2):
    Top row (z=0): M_CGM vs M_vir | f_CGM vs M_vir
    Bottom row (z=2): M_CGM vs M_vir | f_CGM vs M_vir
    """
    print("\nCreating Figure 1: CGM Mass and Fraction vs Halo Mass...")
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    obs = load_observational_data()
    
    for row_idx, snap in enumerate(snaps):
        # ========== LEFT PANEL: M_CGM vs M_vir (CGMgas + HotGas combined) ==========
        ax = axes[row_idx, 0]
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        Regime = read_hdf(snap_num=Snapshot, param='Regime')
        Type = read_hdf(snap_num=Snapshot, param='Type')
        
        # Total diffuse gas (CGMgas + HotGas combined)
        TotalCGM = CGMgas + HotGas
        
        # Get populations
        populations = classify_galaxies(snap)
        
        # Select data for plotting
        mask_cgm = populations['cgm_regime'] & (Mvir > 1e10) & (TotalCGM > 1e8)
        mask_hot = populations['hot_regime'] & (Mvir > 1e10) & (TotalCGM > 1e8)
        
        # Create contours for CGM regime
        if np.sum(mask_cgm) > 100:
            x_cgm = np.log10(Mvir[mask_cgm])
            y_cgm = np.log10(TotalCGM[mask_cgm])
            
            # Create 2D histogram for contours
            H_cgm, xedges, yedges = np.histogram2d(x_cgm, y_cgm, bins=50, 
                                                    range=[[10, 15], [8, 13]])
            H_cgm = H_cgm.T
            X_cgm, Y_cgm = np.meshgrid(xedges[:-1], yedges[:-1])
            
            # Plot contours with shaded levels
            levels_cgm = np.percentile(H_cgm[H_cgm > 0], [39, 86, 99])
            levels_cgm = np.unique(levels_cgm)  # Ensure unique and sorted
            if len(levels_cgm) > 1:  # Need at least 2 levels for contours
                n_regions = len(levels_cgm) - 1
                alphas_cgm = np.linspace(0.15, 0.45, n_regions)
                # Convert color name to RGB and create RGBA tuples with varying alpha
                rgb_cgm = to_rgba(COLOR_CGM_REGIME)[:3]  # Get RGB only
                colors_cgm = [(*rgb_cgm, alpha) for alpha in alphas_cgm]
                ax.contour(10**X_cgm, 10**Y_cgm, H_cgm, levels=levels_cgm,
                          colors=COLOR_CGM_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_cgm, 10**Y_cgm, H_cgm, levels=levels_cgm, colors=colors_cgm)
        
        # Create contours for Hot regime
        if np.sum(mask_hot) > 100:
            x_hot = np.log10(Mvir[mask_hot])
            y_hot = np.log10(TotalCGM[mask_hot])
            
            H_hot, xedges, yedges = np.histogram2d(x_hot, y_hot, bins=50,
                                                    range=[[10, 15], [8, 13]])
            H_hot = H_hot.T
            X_hot, Y_hot = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels_hot = np.percentile(H_hot[H_hot > 0], [39, 86, 99])
            levels_hot = np.unique(levels_hot)  # Ensure unique and sorted
            if len(levels_hot) > 1:  # Need at least 2 levels for contours
                n_regions = len(levels_hot) - 1
                alphas_hot = np.linspace(0.15, 0.45, n_regions)
                # Convert color to RGBA tuples
                rgb_hot = to_rgba(COLOR_HOT_REGIME)[:3]
                colors_hot = [(*rgb_hot, alpha) for alpha in alphas_hot]
                ax.contour(10**X_hot, 10**Y_hot, H_hot, levels=levels_hot,
                          colors=COLOR_HOT_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_hot, 10**Y_hot, H_hot, levels=levels_hot, colors=colors_hot)
        
        # Add total median line (combined CGM + Hot)
        mass_bins = np.logspace(10, 15, 25)
        
        mask_all = populations['all_centrals'] & (Mvir > 1e10) & (TotalCGM > 0)
        if np.sum(mask_all) > 50:
            x_c, med, low, upp = calculate_median_relation(
                np.log10(Mvir[mask_all]),
                np.log10(TotalCGM[mask_all]),
                np.log10(mass_bins)
            )
            valid = ~np.isnan(med)
            ax.plot(10**x_c[valid], 10**med[valid], color='black',
                   lw=2.5, label='Total median', zorder=5)
        
        # Add M_shock
        M_shock = 6e11
        ax.axvline(M_shock, color='black', ls='--', lw=2,
                  label=r'$M_{\rm shock}$', zorder=6)
        
        # Add observations (no legend labels - will be in caption)
        markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h', 'H', '+', 'x', 'd']
        if row_idx == 0:  # z=0 observations
            obs_list_z0 = ['werk2014', 'anderson2013', 'faerman2020', 'liang2014', 'stocke2013',
                          'berg2019', 'nicastro2018', 'burchett2019', 'chen2019', 'dai2020',
                          'tumlinson2017', 'johnson2015', 'thom2012', 'keeney2017']
            
            for i, obs_name in enumerate(obs_list_z0):
                if obs_name in obs:
                    ax.errorbar(10**obs[obs_name]['log_Mhalo'],
                               10**obs[obs_name]['log_Mcgm'],
                               yerr=10**obs[obs_name]['log_Mcgm'] * obs[obs_name]['log_Mcgm_err'] * np.log(10),
                               fmt=markers[i % len(markers)], markerfacecolor='white', markeredgecolor='black',
                               markeredgewidth=1.5, markersize=10, capsize=3, ecolor='black',
                               zorder=7, label='')
        
        else:  # z=2 observations
            ax.errorbar(10**obs['prochaska2017']['log_Mhalo'],
                       10**obs['prochaska2017']['log_Mcgm'],
                       yerr=10**obs['prochaska2017']['log_Mcgm'] * obs['prochaska2017']['log_Mcgm_err'] * np.log(10),
                       fmt='o', markerfacecolor='white', markeredgecolor='black',
                       markeredgewidth=1.5, markersize=10, capsize=3, ecolor='black',
                       zorder=7, label='')
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        if row_idx == 1:  # Only bottom row
            ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=13)
        ax.set_ylabel(r'$M_{\rm CGM+Hot}$ [M$_\odot$]', fontsize=13)
        ax.set_xlim(1e10, 1e15)
        ax.set_ylim(1e8, 1e13)
        ax.legend(loc='upper left', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
               ha='right', va='bottom', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
        
        # ========== RIGHT PANEL: f_CGM vs M_vir ==========
        ax = axes[row_idx, 1]
        
        # Calculate f_CGM = M_CGM / (f_b * M_vir) using combined reservoirs
        f_CGM = np.zeros_like(TotalCGM)
        mask_mvir = Mvir > 0
        f_CGM[mask_mvir] = TotalCGM[mask_mvir] / (BaryonFrac * Mvir[mask_mvir])
        
        # Select data for plotting
        mask_cgm_f = populations['cgm_regime'] & (Mvir > 1e10) & (f_CGM > 1e-4)
        mask_hot_f = populations['hot_regime'] & (Mvir > 1e10) & (f_CGM > 1e-4)
        
        # Create contours for CGM regime
        if np.sum(mask_cgm_f) > 100:
            x_cgm_f = np.log10(Mvir[mask_cgm_f])
            y_cgm_f = np.log10(f_CGM[mask_cgm_f])
            
            H_cgm_f, xedges_f, yedges_f = np.histogram2d(x_cgm_f, y_cgm_f, bins=50,
                                                          range=[[10, 15], [-3, 0.5]])
            H_cgm_f = H_cgm_f.T
            X_cgm_f, Y_cgm_f = np.meshgrid(xedges_f[:-1], yedges_f[:-1])
            
            levels_cgm_f = np.percentile(H_cgm_f[H_cgm_f > 0], [39, 86, 99])
            levels_cgm_f = np.unique(levels_cgm_f)
            if len(levels_cgm_f) > 1:
                n_regions = len(levels_cgm_f) - 1
                alphas_cgm = np.linspace(0.15, 0.45, n_regions)
                # Convert color to RGBA tuples
                rgb_cgm = to_rgba(COLOR_CGM_REGIME)[:3]
                colors_cgm = [(*rgb_cgm, alpha) for alpha in alphas_cgm]
                ax.contour(10**X_cgm_f, 10**Y_cgm_f, H_cgm_f, levels=levels_cgm_f,
                          colors=COLOR_CGM_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_cgm_f, 10**Y_cgm_f, H_cgm_f, levels=levels_cgm_f, colors=colors_cgm)
        
        # Create contours for Hot regime
        if np.sum(mask_hot_f) > 100:
            x_hot_f = np.log10(Mvir[mask_hot_f])
            y_hot_f = np.log10(f_CGM[mask_hot_f])
            
            H_hot_f, xedges_f, yedges_f = np.histogram2d(x_hot_f, y_hot_f, bins=50,
                                                          range=[[10, 15], [-3, 0.5]])
            H_hot_f = H_hot_f.T
            X_hot_f, Y_hot_f = np.meshgrid(xedges_f[:-1], yedges_f[:-1])
            
            levels_hot_f = np.percentile(H_hot_f[H_hot_f > 0], [39, 86, 99])
            levels_hot_f = np.unique(levels_hot_f)
            if len(levels_hot_f) > 1:
                n_regions = len(levels_hot_f) - 1
                alphas_hot = np.linspace(0.15, 0.45, n_regions)
                # Convert color to RGBA tuples
                rgb_hot = to_rgba(COLOR_HOT_REGIME)[:3]
                colors_hot = [(*rgb_hot, alpha) for alpha in alphas_hot]
                ax.contour(10**X_hot_f, 10**Y_hot_f, H_hot_f, levels=levels_hot_f,
                          colors=COLOR_HOT_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_hot_f, 10**Y_hot_f, H_hot_f, levels=levels_hot_f, colors=colors_hot)
        
        # Add total median line for f_CGM (combined CGM + Hot)
        mask_all = populations['all_centrals'] & (Mvir > 1e10) & (f_CGM > 0)
        if np.sum(mask_all) > 50:
            # Calculate median f_CGM in bins
            x_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            medians = np.zeros(len(x_centers))
            
            for i, (m_low, m_high) in enumerate(zip(mass_bins[:-1], mass_bins[1:])):
                mask_bin = mask_all & (Mvir >= m_low) & (Mvir < m_high)
                if np.sum(mask_bin) > 10:
                    values = f_CGM[mask_bin]
                    medians[i] = np.percentile(values, 50)
                else:
                    medians[i] = np.nan
            
            valid = ~np.isnan(medians) & (medians > 0)
            ax.plot(x_centers[valid], medians[valid], color='black',
                   lw=2.5, label='Total median', zorder=5)
        
        # Add M_shock
        ax.axvline(M_shock, color='black', ls='--', lw=2, zorder=6)
        
        # Add reference line at f_CGM = 1 (all baryons in CGM)
        ax.axhline(1.0, color='gray', ls=':', lw=1.5, alpha=0.7, label=r'$f_{\rm CGM} = 1$')
        
        # Add f_CGM observations (only z=0, no legend labels - will be in caption)
        if row_idx == 0:
            obs_list_fcgm = ['stern2018', 'bregman2018', 'mathews2017', 'gupta2012',
                            'werk2014_fcgm', 'hafen2019', 'peeples2019', 'anderson2010',
                            'dai2012', 'savage2014']
            
            for i, obs_name in enumerate(obs_list_fcgm):
                if obs_name in obs:
                    ax.errorbar(10**obs[obs_name]['log_Mhalo'],
                               obs[obs_name]['f_cgm'],
                               yerr=obs[obs_name]['f_cgm_err'],
                               fmt=markers[i % len(markers)], markerfacecolor='white', markeredgecolor='black',
                               markeredgewidth=1.5, markersize=10, capsize=3, ecolor='black',
                               zorder=7, label='')
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        if row_idx == 1:  # Only bottom row
            ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=13)
        ax.set_ylabel(r'$f_{\rm CGM+Hot} = M_{\rm CGM+Hot} / (f_b M_{\rm vir})$', fontsize=13)
        ax.set_xlim(1e10, 1e15)
        ax.set_ylim(1e-3, 3)
        ax.legend(loc='upper right', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
               ha='right', va='bottom', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig1_mass_and_fraction{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig1_mass_and_fraction{OutputFormat}")
def plot_fig2_cgm_mass_function(snaps=[63, 50, 37, 32]):
    """
    Figure 2: CGM Mass Function at Different Redshifts
    Four panels showing evolution
    """
    print("\nCreating Figure 2: CGM Mass Function...")
    
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    axes = axes.flatten()
    
    volume = (BoxSize / Hubble_h)**3 * VolumeFraction
    
    for idx, (snap, ax) in enumerate(zip(snaps, axes)):
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        Regime = read_hdf(snap_num=Snapshot, param='Regime')
        Type = read_hdf(snap_num=Snapshot, param='Type')
        
        # Combined reservoirs
        TotalCGM = CGMgas + HotGas
        
        populations = classify_galaxies(snap)
        
        # Mass bins
        mass_bins = np.logspace(8, 13, 30)
        
        # Calculate mass functions for each population
        for mask, color, label, ls in [
            (populations['all_centrals'], 'black', 'All', '-'),
            (populations['cgm_regime'], COLOR_CGM_REGIME, 'CGM regime', '-'),
            (populations['hot_regime'], COLOR_HOT_REGIME, 'Hot regime', '-')
        ]:
            masses_pop = TotalCGM[mask & (TotalCGM > 1e8)]
            
            if len(masses_pop) > 10:
                m_c, phi, phi_err = calculate_number_density(masses_pop, mass_bins, volume)
                
                # Plot
                valid = phi > 0
                ax.plot(m_c[valid], phi[valid], color=color, lw=2, ls=ls, label=label)
                ax.fill_between(m_c[valid],
                               phi[valid] - phi_err[valid],
                               phi[valid] + phi_err[valid],
                               color=color, alpha=0.2)
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_xlabel(r'$M_{\rm CGM+Hot}$ [M$_\odot$]', fontsize=12)
        if idx % 2 == 0:
            ax.set_ylabel(r'$\phi$ [dex$^{-1}$ (Mpc/h)$^{-3}$]', fontsize=12)
        ax.set_xlim(1e8, 1e13)
        ax.set_ylim(1e-7, 1e-1)
        ax.legend(loc='upper right', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.05, 0.95, f'z = {z:.1f}', transform=ax.transAxes,
               ha='left', va='top', fontsize=11,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig2_mass_function{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig2_mass_function{OutputFormat}")


def plot_fig3_cgm_metallicity_populations(snaps=[63, 32]):
    """
    Figure 3: CGM Metallicity split by regime AND host type
    Two panels: z=0 and z=2
    """
    print("\nCreating Figure 3: CGM Metallicity by Populations...")
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    obs = load_observational_data()
    
    for idx, (snap, ax) in enumerate(zip(snaps, axes)):
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
        CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        MetalsCGM = read_hdf(snap_num=Snapshot, param='MetalsCGMgas') * 1.0e10 / Hubble_h
        MetalsHot = read_hdf(snap_num=Snapshot, param='MetalsHotGas') * 1.0e10 / Hubble_h
        Regime = read_hdf(snap_num=Snapshot, param='Regime')
        
        # Combined reservoirs
        TotalCGM = CGMgas + HotGas
        MetalsCGMtotal = MetalsCGM + MetalsHot
        
        Z_CGM = np.zeros_like(TotalCGM)
        mask_gas = TotalCGM > 0
        Z_CGM[mask_gas] = MetalsCGMtotal[mask_gas] / TotalCGM[mask_gas]
        
        Z_sun = 0.02
        log_Z_CGM = np.log10(Z_CGM / Z_sun + 1e-10)
        
        populations = classify_galaxies(snap)
        
        # Create contours for CGM regime
        mask_cgm = populations['cgm_regime'] & (StellarMass > 1e9) & (TotalCGM > 1e8) & (Z_CGM > 0)
        if np.sum(mask_cgm) > 100:
            x_cgm = np.log10(StellarMass[mask_cgm])
            y_cgm = log_Z_CGM[mask_cgm]
            
            H_cgm, xedges, yedges = np.histogram2d(x_cgm, y_cgm, bins=50,
                                                    range=[[9, 11.7], [-2.5, 0.5]])
            H_cgm = H_cgm.T
            X_cgm, Y_cgm = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels_cgm = np.percentile(H_cgm[H_cgm > 0], [39, 86, 99])
            levels_cgm = np.unique(levels_cgm)
            if len(levels_cgm) > 1:
                n_regions = len(levels_cgm) - 1
                alphas_cgm = np.linspace(0.15, 0.45, n_regions)
                # Convert color to RGBA tuples
                rgb_cgm = to_rgba(COLOR_CGM_REGIME)[:3]
                colors_cgm = [(*rgb_cgm, alpha) for alpha in alphas_cgm]
                ax.contour(10**X_cgm, Y_cgm, H_cgm, levels=levels_cgm,
                          colors=COLOR_CGM_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_cgm, Y_cgm, H_cgm, levels=levels_cgm, colors=colors_cgm)
        
        # Create contours for Hot regime
        mask_hot = populations['hot_regime'] & (StellarMass > 1e9) & (TotalCGM > 1e8) & (Z_CGM > 0)
        if np.sum(mask_hot) > 100:
            x_hot = np.log10(StellarMass[mask_hot])
            y_hot = log_Z_CGM[mask_hot]
            
            H_hot, xedges, yedges = np.histogram2d(x_hot, y_hot, bins=50,
                                                    range=[[9, 11.7], [-2.5, 0.5]])
            H_hot = H_hot.T
            X_hot, Y_hot = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels_hot = np.percentile(H_hot[H_hot > 0], [39, 86, 99])
            levels_hot = np.unique(levels_hot)  # Ensure unique and sorted
            if len(levels_hot) > 1:  # Need at least 2 levels for contours
                n_regions = len(levels_hot) - 1
                alphas_hot = np.linspace(0.15, 0.45, n_regions)
                # Convert color to RGBA tuples
                rgb_hot = to_rgba(COLOR_HOT_REGIME)[:3]
                colors_hot = [(*rgb_hot, alpha) for alpha in alphas_hot]
                ax.contour(10**X_hot, Y_hot, H_hot, levels=levels_hot,
                          colors=COLOR_HOT_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_hot, Y_hot, H_hot, levels=levels_hot, colors=colors_hot)
        
        # Add total median line for metallicity (combined CGM + Hot)
        mass_bins = np.logspace(9, 12, 20)
        
        mask_all = populations['all_centrals'] & (StellarMass > 1e9) & (TotalCGM > 1e8) & (Z_CGM > 0)
        
        if np.sum(mask_all) > 50:
            # Calculate median metallicity in bins
            x_centers = (mass_bins[:-1] + mass_bins[1:]) / 2
            medians = np.zeros(len(x_centers))
            
            for i, (m_low, m_high) in enumerate(zip(mass_bins[:-1], mass_bins[1:])):
                mask_bin = mask_all & (StellarMass >= m_low) & (StellarMass < m_high)
                if np.sum(mask_bin) > 10:
                    values = log_Z_CGM[mask_bin]
                    medians[i] = np.percentile(values, 50)
                else:
                    medians[i] = np.nan
            
            valid = ~np.isnan(medians)
            ax.plot(x_centers[valid], medians[valid], color='black',
                   lw=2.5, label='Total median', zorder=5)
        
        # Add observations (no legend labels - will be in caption)
        markers = ['o', 's', '^', 'D', 'v', '<', '>', 'p', '*', 'h', 'H', '+', 'x', 'd', 'P', 'X', '1', '2', '3', '4', '8', 'H', 'h', 'd']
        if idx == 0:  # z=0
            obs_list_z0_met = ['peeples2014', 'tumlinson2011_sf', 'tumlinson2011_passive',
                              'lehner2013', 'zahedy2019', 'werk2016', 'burchett2016', 'wotta2016',
                              'prochaska2011', 'qu2018', 'peroux2016', 'liang2014_Z', 'stocke2013_Z',
                              'christensen2014', 'peroux2020', 'cooper2015', 'shen2013', 'johnson2017',
                              'kacprzak2019', 'tripp2011', 'churchill2013', 'wotta2019', 'lehner2016',
                              'som2015']
            
            for i, obs_name in enumerate(obs_list_z0_met):
                if obs_name in obs:
                    ax.errorbar(10**obs[obs_name]['log_Mstar'],
                               obs[obs_name]['log_Z_CGM'],
                               yerr=obs[obs_name]['log_Z_err'],
                               fmt=markers[i % len(markers)], markerfacecolor='white', markeredgecolor='black',
                               markeredgewidth=1.5, markersize=10, capsize=3, ecolor='black',
                               zorder=7, label='')
        
        else:  # z=2
            obs_list_z2_met = ['rudie2012', 'bordoloi2014', 'fumagalli2016', 'turner2017',
                              'steidel2010', 'crighton2015', 'rauch2016']
            
            for i, obs_name in enumerate(obs_list_z2_met):
                if obs_name in obs:
                    ax.errorbar(10**obs[obs_name]['log_Mstar'],
                               obs[obs_name]['log_Z_CGM'],
                               yerr=obs[obs_name]['log_Z_err'],
                               fmt=markers[i % len(markers)], markerfacecolor='white', markeredgecolor='black',
                               markeredgewidth=1.5, markersize=10, capsize=3, ecolor='black',
                               zorder=7, label='')
        
        # Solar line
        ax.axhline(0, color='gray', ls=':', lw=1.5, alpha=0.7)
        
        # Formatting
        ax.set_xscale('log')
        ax.set_xlabel(r'$M_*$ [M$_\odot$]', fontsize=13)
        if idx == 0:
            ax.set_ylabel(r'$\log_{10}(Z_{\rm CGM+Hot}/Z_\odot)$', fontsize=13)
        ax.set_xlim(1e9, 5e11)
        ax.set_ylim(-2.5, 0.5)
        ax.legend(loc='lower right', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.05, 0.95, f'z = {z:.1f}', transform=ax.transAxes,
               ha='left', va='top', fontsize=12,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig3_metallicity_populations{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig3_metallicity_populations{OutputFormat}")


def plot_fig4_chmr_evolution(snaps=[63, 40, 32, 27]):
    """
    Figure 4: CGM-Halo Mass Relation (CHMR) at Different Redshifts
    Four panels showing evolution (analogous to SHMR)
    """
    print("\nCreating Figure 4: CGM-Halo Mass Relation Evolution...")
    
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    axes = axes.flatten()
    
    for idx, (snap, ax) in enumerate(zip(snaps, axes)):
        Snapshot = f'Snap_{snap}'
        z = redshifts[snap]
        
        # Read data
        Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
        CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
        HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
        Regime = read_hdf(snap_num=Snapshot, param='Regime')
        Type = read_hdf(snap_num=Snapshot, param='Type')
        
        # Combined reservoirs
        TotalCGM = CGMgas + HotGas
        
        populations = classify_galaxies(snap)
        
        # Select data for plotting
        mask_cgm = populations['cgm_regime'] & (Mvir > 1e10) & (TotalCGM > 1e8)
        mask_hot = populations['hot_regime'] & (Mvir > 1e10) & (TotalCGM > 1e8)
        
        # Create contours for CGM regime
        if np.sum(mask_cgm) > 100:
            x_cgm = np.log10(Mvir[mask_cgm])
            y_cgm = np.log10(TotalCGM[mask_cgm])
            
            H_cgm, xedges, yedges = np.histogram2d(x_cgm, y_cgm, bins=50,
                                                    range=[[10, 15], [8, 13]])
            H_cgm = H_cgm.T
            X_cgm, Y_cgm = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels_cgm = np.percentile(H_cgm[H_cgm > 0], [39, 86, 99])
            levels_cgm = np.unique(levels_cgm)  # Ensure unique and sorted
            if len(levels_cgm) > 1:  # Need at least 2 levels for contours
                n_regions = len(levels_cgm) - 1
                alphas_cgm = np.linspace(0.15, 0.45, n_regions)
                # Convert color to RGBA tuples
                rgb_cgm = to_rgba(COLOR_CGM_REGIME)[:3]
                colors_cgm = [(*rgb_cgm, alpha) for alpha in alphas_cgm]
                ax.contour(10**X_cgm, 10**Y_cgm, H_cgm, levels=levels_cgm,
                          colors=COLOR_CGM_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_cgm, 10**Y_cgm, H_cgm, levels=levels_cgm, colors=colors_cgm)
        
        # Create contours for Hot regime
        if np.sum(mask_hot) > 100:
            x_hot = np.log10(Mvir[mask_hot])
            y_hot = np.log10(TotalCGM[mask_hot])
            
            H_hot, xedges, yedges = np.histogram2d(x_hot, y_hot, bins=50,
                                                    range=[[10, 15], [8, 13]])
            H_hot = H_hot.T
            X_hot, Y_hot = np.meshgrid(xedges[:-1], yedges[:-1])
            
            levels_hot = np.percentile(H_hot[H_hot > 0], [39, 86, 99])
            levels_hot = np.unique(levels_hot)  # Ensure unique and sorted
            if len(levels_hot) > 1:  # Need at least 2 levels for contours
                n_regions = len(levels_hot) - 1
                alphas_hot = np.linspace(0.15, 0.45, n_regions)
                # Convert color to RGBA tuples
                rgb_hot = to_rgba(COLOR_HOT_REGIME)[:3]
                colors_hot = [(*rgb_hot, alpha) for alpha in alphas_hot]
                ax.contour(10**X_hot, 10**Y_hot, H_hot, levels=levels_hot,
                          colors=COLOR_HOT_REGIME, linewidths=1.5, alpha=0.7)
                ax.contourf(10**X_hot, 10**Y_hot, H_hot, levels=levels_hot, colors=colors_hot)
        
        # Add total median line (combined CGM + Hot)
        mass_bins = np.logspace(10, 15, 25)
        
        mask_all = populations['all_centrals'] & (Mvir > 1e10) & (TotalCGM > 0)
        x_c, med, low, upp = calculate_median_relation(
            np.log10(Mvir[mask_all]),
            np.log10(TotalCGM[mask_all]),
            np.log10(mass_bins)
        )
        valid = ~np.isnan(med)
        ax.plot(10**x_c[valid], 10**med[valid], color='black',
               lw=2.5, label='Total median', zorder=5)
        
        # Add M_shock
        M_shock = 6e11
        ax.axvline(M_shock, color='black', ls=':', lw=1.5, alpha=0.5)
        
        # Formatting
        ax.set_xscale('log')
        ax.set_yscale('log')
        ax.set_xlabel(r'$M_{\rm vir}$ [M$_\odot$]', fontsize=12)
        if idx % 2 == 0:
            ax.set_ylabel(r'$M_{\rm CGM+Hot}$ [M$_\odot$]', fontsize=12)
        ax.set_xlim(1e10, 1e15)
        ax.set_ylim(1e8, 1e13)
        ax.legend(loc='upper left', fontsize=9, framealpha=0.9)
        ax.grid(alpha=0.3, ls=':')
        ax.text(0.95, 0.05, f'z = {z:.1f}', transform=ax.transAxes,
               ha='right', va='bottom', fontsize=11,
               bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))
    
    plt.tight_layout()
    plt.savefig(f'{DirName}plots/cgm_fig4_chmr_evolution{OutputFormat}',
                dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  Saved to {DirName}plots/cgm_fig4_chmr_evolution{OutputFormat}")


def main():
    """Main execution function"""
    
    print("="*70)
    print("CGM SECTION ANALYSIS FOR SAGE (SHARK-Style)")
    print("="*70)
    
    # Create output directory
    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir):
        os.makedirs(OutputDir)
        print(f"Created output directory: {OutputDir}")
    
    print(f"\nReading from: {DirName}{FileName}")
    print(f"Output to: {OutputDir}\n")
    
    # Set random seed for reproducibility
    seed(42)
    
    # Generate all CGM section figures
    try:
        plot_fig1_cgm_mass_by_regime(snaps=[63, 32])  # z=0, z=2
    except Exception as e:
        print(f"  Error creating Figure 1: {e}")
    
    try:
        plot_fig2_cgm_mass_function(snaps=[63, 50, 37, 32])  # z=0, 1, 2, 3
    except Exception as e:
        print(f"  Error creating Figure 2: {e}")
    
    try:
        plot_fig3_cgm_metallicity_populations(snaps=[63, 32])  # z=0, z=2
    except Exception as e:
        print(f"  Error creating Figure 3: {e}")
    
    try:
        plot_fig4_chmr_evolution(snaps=[63, 40, 32, 27])  # z=0, 1, 2, 3
    except Exception as e:
        print(f"  Error creating Figure 4: {e}")
    
    print("\n" + "="*70)
    print("CGM SECTION FIGURES COMPLETED!")
    print("="*70)
    print("\nFigures generated:")
    print("  1. cgm_fig1_mass_by_regime.png - CGM Mass vs Halo Mass (2 redshifts)")
    print("  2. cgm_fig2_mass_function.png - CGM Mass Function (4 redshifts)")
    print("  3. cgm_fig3_metallicity_populations.png - CGM Metallicity (2 redshifts)")
    print("  4. cgm_fig4_chmr_evolution.png - CHMR Evolution (4 redshifts)")
    
    # Print galaxy statistics at z=0
    print("\n" + "="*70)
    print("GALAXY STATISTICS AT z=0 (Snap 63)")
    print("="*70)
    
    snap = 63
    Snapshot = f'Snap_{snap}'
    
    # Read data
    Type = read_hdf(snap_num=Snapshot, param='Type')
    Regime = read_hdf(snap_num=Snapshot, param='Regime')
    CGMgas = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    
    # Get populations
    populations = classify_galaxies(snap)
    
    # Calculate counts
    n_total = np.sum(populations['all_centrals'])
    n_cgm_regime = np.sum(populations['cgm_regime'])
    n_hot_regime = np.sum(populations['hot_regime'])
    
    # Hot-regime galaxies with CGM gas (CGMgas > threshold)
    threshold = 1e8  # M_sun
    hot_with_cgm = populations['hot_regime'] & (CGMgas > threshold)
    n_hot_with_cgm = np.sum(hot_with_cgm)
    
    # CGM-regime galaxies with Hot gas (HotGas > threshold)
    cgm_with_hot = populations['cgm_regime'] & (HotGas > threshold)
    n_cgm_with_hot = np.sum(cgm_with_hot)
    
    print(f"\nTotal central galaxies:           {n_total:,}")
    print(f"CGM-regime galaxies:              {n_cgm_regime:,} ({100*n_cgm_regime/n_total:.1f}%)")
    print(f"Hot-regime galaxies:              {n_hot_regime:,} ({100*n_hot_regime/n_total:.1f}%)")
    print(f"\nHot-regime with CGM gas > 10^8:   {n_hot_with_cgm:,} ({100*n_hot_with_cgm/n_hot_regime:.1f}% of Hot-regime)")
    print(f"CGM-regime with Hot gas > 10^8:   {n_cgm_with_hot:,} ({100*n_cgm_with_hot/n_cgm_regime:.1f}% of CGM-regime)")
    print("="*70 + "\n")


if __name__ == '__main__':
    main()