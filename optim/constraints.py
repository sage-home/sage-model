#!/bin/bash

"""
Constraints for optimizers to evaluate shark models against observations
"""

import warnings
warnings.filterwarnings("ignore")
import matplotlib.pyplot as plt # type: ignore
import os
from random import sample, seed
import common
import numpy as np # type: ignore
import re
from scipy.interpolate import interp1d # type: ignore
import routines as r
import h5py as h5 # type: ignore
import pandas as pd # type: ignore
from scipy.stats import binned_statistic # type: ignore
import logging
import scipy.stats as stats

warnings.filterwarnings("ignore")
logging.getLogger('constraints').setLevel(logging.INFO)

GyrToYr = 1e9
dilute = 75000 
#######################
# Binning configuration
mupp = 12
dm = 0.1
mlow = 8
mbins = np.arange(mlow, mupp, dm)
xmf = mbins

mupp2 = 10
dm2 = 0.1
mlow2 = 4
mbins2 = np.arange(mlow2,mupp2,dm2)
xmf2 = mbins2 

ssfrlow = -6
ssfrupp = 4
dssfr = 0.2
ssfrbins = np.arange(ssfrlow,ssfrupp,dssfr)

Nmin = 2 # minimum number of galaxies expected in a mass bin for the simulation volume, based on observations, to warrant fitting to that bin for mass functions

# These are two easily create variables of these different shapes without
# actually storing a reference ourselves; we don't need it
zeros1 = lambda: np.zeros(shape=(1, 3, len(xmf)))
zeros2 = lambda: np.zeros(shape=(1, 3, len(xmf2)))
zeros3 = lambda: np.zeros(shape=(1, len(mbins)))
zeros4 = lambda: np.empty(shape=(1), dtype=np.bool_)
zeros5 = lambda: np.zeros(shape=(1, len(ssfrbins)))
zeros6 = lambda: np.zeros(shape=(1, len(mbins2)))
zeros7 = lambda: np.zeros(shape=(1, len(mbins)))
zeros8 = lambda: np.zeros(shape=(1, len(mbins)))
zeros9 = lambda: np.zeros(shape=(1, len(mbins)))

class Constraint(object):
    """Base classes for constraint objects"""

    def __init__(self, snapshot=None, sim=None, boxsize=None, vol_frac=None, age_alist_file=None, Omega0=None, h0=None, output_dir=None):
        self.redshift_table = None
        self.weight = 1
        self.rel_weight = 1
        self.snapshot = snapshot
        self.output_dir = output_dir

        # Set defaults if not provided
        self.sim = 0 if sim is None else sim
        self.boxsize = 400.0 if sim is None else boxsize
        self.vol_frac = 0.0019 if vol_frac is None else vol_frac
        self.Omega0 = 0.3089 if Omega0 is None else Omega0
        self.h0 = 0.677400 if h0 is None else h0
        self.age_alist_file = '/fred/oz004/msinha/simulations/uchuu_suite/miniuchuu/mergertrees/u400_planck2016_50.a_list' if age_alist_file is None else age_alist_file

        # Set simulation parameters
        if sim == 0:  # miniUchuu
            self.h0 = h0
            self.Omega0 = Omega0
            self.vol_frac = vol_frac
            self.vol = (boxsize/h0)**3 * vol_frac
            self.age_alist_file = age_alist_file
        elif sim == 1:  # miniMillennium 
            self.h0 = h0
            self.Omega0 = Omega0
            self.vol_frac = vol_frac
            self.vol = (boxsize/h0)**3 * vol_frac
            self.age_alist_file = age_alist_file
        else:  # MTNG
            self.h0 = h0
            self.Omega0 = Omega0
            self.vol_frac = vol_frac
            self.vol = (boxsize/h0)**3 * vol_frac
            self.age_alist_file = age_alist_file

    def _load_model_data(self, modeldir, subvols):
        # Allow snapshots to be a list
        if not isinstance(self.snapshot, list):
            self.snapshot = [self.snapshot]
            
        data_by_snapshot = {}
        for snap in self.snapshot:
            if len(subvols) > 1:
                subvols = ["multiple_batches"]

            # Histograms we are interested in
            hist_smf = zeros3()
            hist_bhmf = zeros6()
            hist_smf_red = zeros7()
            hist_smf_blue = zeros8()
            hist_h2 = zeros9()

            seed(2222)
            fields = ['StellarMass', 'BlackHoleMass', 'Len', 'SfrBulge', 'BulgeMass', 'Mvir', 'SfrDisk']
            Nage = 14
            snap_num = f'Snap_{snap}'
            sSFRcut = -11.0

            # Get list of model files in directory
        model_files = [f for f in os.listdir(modeldir) if f.startswith('model_') and f.endswith('.hdf5')]
        model_files.sort()

        if len(model_files) > 1:
            combined_properties = {}
            for model_file in model_files:
                G = r.read_sage_hdf(os.path.join(modeldir, model_file), snap_num=snap_num, fields=fields)
                
                # Combine properties
                for field in fields:
                    if field not in combined_properties:
                        combined_properties[field] = G[field]
                    else:
                        combined_properties[field] = np.concatenate((combined_properties[field], G[field]))
            
            G = combined_properties
            print('Number of galaxies: ', len(G['StellarMass']))
        else:
            G = r.read_sage_hdf(os.path.join(modeldir, 'model_0.hdf5'), snap_num=snap_num, fields=fields)
            print('Number of galaxies: ', len(G['StellarMass']))

        # Process properties - Use self.h0 instead of h0
        BlackHoleMass = np.log10(G['BlackHoleMass'] * 1e10 / self.h0)
        BlackHoleMass[~np.isfinite(BlackHoleMass)] = -20

        BulgeMass = np.log10(G['BulgeMass'] * 1e10 / self.h0)
        BulgeMass[~np.isfinite(BulgeMass)] = -20

        HaloMass = np.log10(G['Mvir'] * 1e10 / self.h0)
        HaloMass[~np.isfinite(HaloMass)] = -20

        StellarMass = np.log10(G['StellarMass'] * 1e10 / self.h0)
        StellarMass[~np.isfinite(StellarMass)] = -20

        logSM = np.log10(G['StellarMass'] * 1e10 / self.h0)
        logSM[~np.isfinite(logSM)] = -20

        logBHM = np.log10(G['BlackHoleMass'] * 1e10 / self.h0)
        logBHM[~np.isfinite(logBHM)] = -20

        smass = (G['StellarMass'] * 1e10 / self.h0)
        SfrDisk = G['SfrDisk']
        SfrBulge = G['SfrBulge']
        
        # calculate all
        w = np.where(smass > 0.0)[0]
        mass = np.log10(smass[w])
        sSFR = np.log10( (SfrDisk[w] + SfrBulge[w]) / StellarMass[w] )
        
        # additionally calculate red
        w = np.where(sSFR < sSFRcut)[0]
        massRED = mass[w]
        (hist_smf_red, binedges) = np.histogram(massRED, bins=mbins)
        hist_smf_red = hist_smf_red / dm / self.vol

        # additionally calculate blue
        w = np.where(sSFR > sSFRcut)[0]
        massBLU = mass[w]
        (hist_smf_blue, binedges) = np.histogram(massBLU, bins=mbins)
        hist_smf_blue = hist_smf_blue / dm / self.vol

        hist_smf, _ = np.histogram(logSM, bins=mbins)
        hist_smf = hist_smf / dm / self.vol

        hist_bhmf, _ = np.histogram(logBHM, bins=mbins2)
        hist_bhmf = hist_bhmf / dm2 / self.vol
            
        # get the edges of the age bins
        # Load and convert scale factors to redshifts
        alist = np.loadtxt(self.age_alist_file)
        if Nage >= len(alist)-1:
            alist = alist[::-1]
            RedshiftBinEdge = 1./alist - 1  # Convert scale factors to redshifts
        else:
            indices_float = np.arange(Nage+1) * (len(alist)-1.0) / Nage
            indices = indices_float.astype(np.int32)
            alist = alist[indices][::-1]
            RedshiftBinEdge = 1./alist - 1  # Convert scale factors to redshifts

        TimeBinEdge = np.array([r.z2tL(redshift, self.h0, self.Omega0, 1.0-self.Omega0) for redshift in RedshiftBinEdge])
        
        dT = np.diff(TimeBinEdge) # time step for each bin
        TimeBinCentre = TimeBinEdge[:-1] + 0.5*dT
#        m, lifetime, returned_mass_fraction_integrated, ncum_SN = r.return_fraction_and_SN_ChabrierIMF()
#        eff_recycle = np.interp(TimeBinCentre, lifetime[::-1], returned_mass_fraction_integrated[::-1])
        SFRbyAge = np.sum(G['SfrBulge'], axis=0)*1e10/self.h0 / (dT*1e9)


        #########################
        # take logs
        ind = (hist_smf > 0.)

        hist_smf[ind] = np.log10(hist_smf[ind])
        hist_smf[~ind] = -20

        ind = (hist_smf_red > 0.)

        hist_smf_red[ind] = np.log10(hist_smf_red[ind])
        hist_smf_red[~ind] = -20

        ind = (hist_smf_blue > 0.)

        hist_smf_blue[ind] = np.log10(hist_smf_blue[ind])
        hist_smf_blue[~ind] = -20

        ind = (hist_bhmf > 0.)

        hist_bhmf[ind] = np.log10(hist_bhmf[ind])
        hist_bhmf[~ind] = -20

        SFRD_Age = np.log10(SFRbyAge/self.vol)
        SFRD_Age[~np.isfinite(SFRD_Age)] = -20
        
        # have moved where this was in the code. Don't understand its purpose
        hist_bhmf = hist_bhmf[np.newaxis]
        hist_smf = hist_smf[np.newaxis]
        hist_smf_red = hist_smf_red[np.newaxis]
        hist_smf_blue = hist_smf_blue[np.newaxis]

        print(hist_smf_red,hist_smf_blue)

        return self.h0, self.Omega0, hist_smf, hist_bhmf, TimeBinEdge, SFRD_Age, BlackHoleMass, BulgeMass, HaloMass, StellarMass, hist_smf_red, hist_smf_blue


    def load_observation(self, *args, **kwargs):
        obsdir = os.path.normpath(os.path.abspath(os.path.join(__file__, '..')))#, '..', 'data')))
#        obsdir = os.path.normpath(os.path.abspath(__file__))
        return common.load_observation(obsdir, *args, **kwargs)
    
    def plot_smf(self, x_obs, y_obs, y_mod, x_sage, y_sage, output_dir):
        """Plot Stellar Mass Function comparison"""
        plt.figure()  # New figure
        ax = plt.subplot(111)  # 1 plot on the figure

        plt.plot(x_obs, 10**y_mod, c='b', label='Model - SAGE')
        plt.plot(x_sage, 10**y_sage, c='r', label='SAGE')
        plt.scatter(x_obs, 10**y_obs, marker='d', s=50, c='k', label='Observation')

        class_name = self.__class__.__name__
    
        if class_name == 'SMF_z0':
            # Add SHARK for z=0
            mass, phi = self.load_observation('./data/SHARK_SMF.csv', cols=[0,1])
            ax.plot(mass, 10**phi, c='g', label='SHARK')

        elif class_name == 'SMF_z05':
            # Add SHARK for z=0
            mass, phi = self.load_observation('./data/SHARK_SMF.csv', cols=[2,3])
            ax.plot(mass, 10**phi, c='g', label='SHARK')

        elif class_name == 'SMF_z10':
            # Add SHARK for z=0
            mass, phi = self.load_observation('./data/SHARK_SMF.csv', cols=[4,5])
            ax.plot(mass, 10**phi, c='g', label='SHARK')

        elif class_name == 'SMF_z20':
            # Add SHARK for z=0
            mass, phi = self.load_observation('./data/SHARK_SMF.csv', cols=[6,7])

            ax.plot(mass, 10**phi, c='g', label='SHARK')

        elif class_name == 'SMF_z30':
            # Add SHARK for z=0
            mass, phi = self.load_observation('./data/SHARK_SMF.csv', cols=[8,9])

            ax.plot(mass, 10**phi, c='g', label='SHARK')

        elif class_name == 'SMF_z40':
            # Add SHARK for z=0
            mass, phi = self.load_observation('./data/SHARK_SMF.csv', cols=[10,11])

            ax.plot(mass, 10**phi, c='g', label='SHARK')

        plt.yscale('log')
        plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)
        for t in leg.get_texts():
            t.set_fontsize('medium')
        plotfile = os.path.join(output_dir, 'smf_sage.png')
        plt.savefig(plotfile, dpi=100)
        plt.close()
        return

    def plot_bhmf(self, x_obs, y_obs, y_mod, x_sage, y_sage, output_dir):
        """Plot Black Hole Mass Function comparison"""
        plt.figure()
        ax = plt.subplot(111)

        y_obs_converted = [10**y for y in y_obs]
        plt.plot(x_obs, 10**y_mod, c='b', label='Model - SAGE')
        plt.plot(x_sage, 10**y_sage, c='k', label='SAGE')
        plt.plot(x_obs, y_obs_converted, c='r', label='Observation')

        plt.yscale('log')
        plt.axis([6.0, 10.3, 1.0e-6, 1.0e-1])
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        plt.xlabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
        leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)
        for t in leg.get_texts():
            t.set_fontsize('medium')
        plotfile = os.path.join(output_dir, 'bhmf_sage.png')
        plt.savefig(plotfile, dpi=100)
        plt.close()
        return

    def plot_bhbm(self, x_obs, y_obs, y_mod, x_sage, y_sage, BlackHoleMass, BulgeMass, output_dir):
        """Plot Black Hole-Bulge Mass relation comparison"""
        plt.figure()
        ax = plt.subplot(111)
        #print(x_sage,y_sage)
        plt.plot(x_obs, y_mod, c='b', label='Model - SAGE')
        plt.plot(x_sage, y_sage, c='k', label='SAGE')
        plt.plot(x_obs, y_obs, c='r', label="Observation")
        #print(y_mod, y_obs, y_sage)

        w = np.where(BlackHoleMass > 0.0)[0]
        if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
        plt.scatter(BulgeMass[w], BlackHoleMass[w], s=0.5, c='orange', alpha=0.6, label='SAGE galaxies')

        class_name = self.__class__.__name__

        if class_name == 'BHBM_z0':
            # Add SHARK for z=0
            bulgemass, blackholemass = self.load_observation('./data/SHARK_BHBM_z0.csv', cols=[0,1])
            ax.plot(bulgemass, blackholemass, c='g', label='SHARK')

        plt.ylabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
        plt.xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
        plt.axis([8.0, 12.0, 6.0, 10.0])

        leg = plt.legend(loc='upper left')
        leg.draw_frame(False)
        for t in leg.get_texts():
            t.set_fontsize('medium')
        plotfile = os.path.join(output_dir, 'bhbm_sage.png')
        plt.savefig(plotfile, dpi=100)
        plt.close()
        return

    def _get_raw_data(self, modeldir, subvols):
        """Gets the model and observational data for further analysis.
        The model data is interpolated to match the observation's X values."""

        self.h0, self.Omega0, hist_smf, hist_bhmf, TimeBinEdge, SFRD_Age, BlackHoleMass, BulgeMass, HaloMass, StellarMass, hist_smf_red, hist_smf_blue = self._load_model_data(modeldir, subvols)
        x_obs, y_obs, y_dn, y_up = self.get_obs_x_y_err()
        x_sage, y_sage = self.get_sage_x_y()
        x_mod, y_mod = self.get_model_x_y(hist_smf, hist_bhmf, TimeBinEdge, SFRD_Age, BlackHoleMass, BulgeMass, HaloMass, StellarMass, hist_smf_red, hist_smf_blue)
        return x_obs, y_obs, y_dn, y_up, x_sage, y_sage, x_mod, y_mod

    def get_data(self, modeldir, subvols):

        self.h0, self.Omega0, hist_smf, hist_bhmf, TimeBinEdge, SFRD_Age, BlackHoleMass, BulgeMass, HaloMass, StellarMass, hist_smf_red, hist_smf_blue = self._load_model_data(modeldir, subvols)
        x_obs, y_obs, y_dn, y_up = self.get_obs_x_y_err()
        x_sage, y_sage = self.get_sage_x_y()
        x_mod, y_mod = self.get_model_x_y(hist_smf, hist_bhmf, TimeBinEdge, SFRD_Age, BlackHoleMass, BulgeMass, HaloMass, StellarMass, hist_smf_red, hist_smf_blue)

        # Both observations and model values don't come necessarily in order,
        # but if at the end of the day we want to perform array-wise operations
        # over them (to calculate chi2 or student-t) then they should be both in
        # ascending order
        sorted_obs = np.argsort(x_obs)
        x_obs = x_obs[sorted_obs]
        y_obs = y_obs[sorted_obs]
        y_dn = y_dn[sorted_obs]
        y_up = y_up[sorted_obs]
        
        sorted_mod = np.argsort(x_mod)
        x_mod = x_mod[sorted_mod]
        y_mod = y_mod[sorted_mod]
        
        # Calculate model errors (Poisson errors from number counts)
        # For log(phi), error is approximately 1/sqrt(N) in linear space
        # which translates to approximately 0.434/sqrt(N) in log space
        # We assume a minimum of 5 galaxies per bin for Poisson errors
        y_mod_err = np.ones_like(y_mod) * 0.2  # Conservative estimate

        # Linearly interpolate model Y values respect to the observations'
        # X values, and only take those within the domain. We do the same 
        # for the errors of the model.
        # We also consider the biggest relative error as "the" error, in case
        # they are different and add it in quadrature to the Poisson error
        # of the model.
        y_mod_interp = np.interp(x_obs, x_mod, y_mod)
        y_mod_err_interp = np.interp(x_obs, x_mod, y_mod_err)
        
        sel = np.where((x_obs >= self.domain[0]) & (x_obs <= self.domain[1]))
        x_obs_sel = x_obs[sel]
        y_obs_sel = y_obs[sel]
        y_mod_sel = y_mod_interp[sel]
        y_mod_err_sel = y_mod_err_interp[sel]
        y_obs_err_sel = np.maximum(np.abs(y_dn[sel]), np.abs(y_up[sel]))
        err = np.sqrt(y_obs_err_sel ** 2.0 + y_mod_err_sel ** 2.0)
        
        print('in get_data:')
        print('obs x:', x_obs_sel)
        print('obs y:', y_obs_sel)
        print('mod y:', y_mod_sel)
        print('obs errors:', y_obs_err_sel)
        print('mod errors:', y_mod_err_sel)
        print('combined errors:', err)
        print('differences (mod-obs):', y_mod_sel - y_obs_sel)
        print('normalized differences:', (y_mod_sel - y_obs_sel) / err)

        # Get the constraint name and create filename directly in outdir
        constraint_name = self.__class__.__name__
        filename = os.path.join(self.output_dir, f"{constraint_name}_dump.txt")

        # Append data to dump file
        with open(filename, 'a') as f:
            f.write(f"# New Data Block\n")
            for x_val, y_val, mod_y_val in zip(x_obs_sel, y_obs_sel, y_mod_sel):
                f.write(f"{x_val}\t{y_val}\t{mod_y_val}\n")
            
        # Get constraint name for appropriate plotting function
        constraint_name = self.__class__.__name__

        if constraint_name.startswith('TARGET_SMF'):
            self.plot_target_smf(x_obs_sel, y_obs_sel, y_mod_sel, x_sage, y_sage, self.output_dir)
        if 'SMF' in constraint_name:
            self.plot_smf(x_obs_sel, y_obs_sel, y_mod_sel, x_sage, y_sage, self.output_dir)
        if 'BHMF' in constraint_name:
            self.plot_bhmf(x_obs_sel, y_obs_sel, y_mod_sel, x_sage, y_sage, self.output_dir)
        if 'BHBM' in constraint_name:
            self.plot_bhbm(x_obs_sel, y_obs_sel, y_mod_sel, x_sage, y_sage, BlackHoleMass, BulgeMass, self.output_dir)
        
        return y_obs_sel, y_mod_sel, err

    def __str__(self):
        s = '%s, low=%.1f, up=%.1f, weight=%.2f, rel_weight=%.2f'
        args = self.__class__.__name__, self.domain[0], self.domain[1], self.weight, self.rel_weight
        return s % args

class BHMF(Constraint):
    """Common logic for SMF constraints"""

    domain = (6, 10)

    def get_model_x_y(self, _, hist_bhmf, _2, _3, _4, _5, _6, _7, _8, _9):
        y = hist_bhmf[0]
        ind = np.where(y < 0.)

        return xmf2[ind], y[ind]
    
class BHMF_z0(BHMF):
    """The BHMF constraint at z=0"""
    
    z = [0]

    def get_obs_x_y_err(self):
        # Load data from TRINITY PaperIV (z=0.1)
        obs_data = np.loadtxt('./data/fig4_bhmf_z0.1.txt')
        logm = obs_data[:, 0]           # log10(Mbh [Msun])
        phi = obs_data[:, 1]            # BHMF_best [Mpc^-3 dex^-1]
        phi_16th = obs_data[:, 2]       # BHMF_16th [Mpc^-3 dex^-1]
        phi_84th = obs_data[:, 3]       # BHMF_84th [Mpc^-3 dex^-1]
        
        # Convert to log10 space
        logphi = np.log10(phi)
        logphi_16th = np.log10(phi_16th)
        logphi_84th = np.log10(phi_84th)
        
        # Calculate asymmetric errors in log space
        y_dn = logphi - logphi_16th  # Lower error (positive value)
        y_up = logphi_84th - logphi  # Upper error (positive value)
        
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi) & ~np.isnan(y_dn) & ~np.isnan(y_up)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        y_dn = y_dn[valid_mask]
        y_up = y_up[valid_mask]
    
        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[0,1])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z10(BHMF):
    """The BHMF constraint at z=1.0"""

    z = [1.0]

    def get_obs_x_y_err(self):
        # Load data from TRINITY PaperIV (z=1.0)
        obs_data = np.loadtxt('./data/fig4_bhmf_z1.0.txt')
        logm = obs_data[:, 0]           # log10(Mbh [Msun])
        phi = obs_data[:, 1]            # BHMF_best [Mpc^-3 dex^-1]
        phi_16th = obs_data[:, 2]       # BHMF_16th [Mpc^-3 dex^-1]
        phi_84th = obs_data[:, 3]       # BHMF_84th [Mpc^-3 dex^-1]
        
        # Convert to log10 space
        logphi = np.log10(phi)
        logphi_16th = np.log10(phi_16th)
        logphi_84th = np.log10(phi_84th)
        
        # Calculate asymmetric errors in log space
        y_dn = logphi - logphi_16th  # Lower error (positive value)
        y_up = logphi_84th - logphi  # Upper error (positive value)
        
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi) & ~np.isnan(y_dn) & ~np.isnan(y_up)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        y_dn = y_dn[valid_mask]
        y_up = y_up[valid_mask]
    
        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[4,5])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage


class SMF(Constraint):
    """Common logic for SMF constraints"""

    domain = (8.0, 12.0)

    def get_model_x_y(self, hist_smf, _, _2, _3, _4, _5, _6, _7, _8, _9):
        y = hist_smf[0,:]
        ind = np.where(y < 0.)
        return xmf[ind], y[ind]

class SMF_z0(SMF):
    """The SMF constraint at z=0"""

    z = [0]

    def get_obs_x_y_err(self):
        # SMF from Li & White (2009)
        lm, p, dpdn, dpup = self.load_observation('../data/SMF_Li2009.dat', cols=[0,1,2,3])
        hobs = 0.733
        x_obs = lm - 2.0 * np.log10(hobs) + 2.0 * np.log10(hobs/self.h0)
        y_obs = p + 3.0 * np.log10(hobs) - 3.0 * np.log10(hobs/self.h0)
        y_dn = dpdn
        y_up = dpup

        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[0,1])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z05(SMF):
    """The SMF constraint at z=0.5"""

    z = [0.5]

    def get_obs_x_y_err(self):
        # SMF from Weaver et al. (2022)
        lm, pD, dn, du = self.load_observation('../data/COSMOS2020/SMF_Farmer_v2.1_0.2z0.5_total.txt', cols=[0,2,3,4])
        hobs = 0.73
        y_obs = np.log10(pD) + 3.0 * np.log10(hobs/self.h0)
        y_dn = np.log10(pD) - np.log10(dn)
        y_up = np.log10(du) - np.log10(pD)
        x_obs = lm - 2.0 * np.log10(hobs/self.h0)

        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[4,5])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z10(SMF):
    """The SMF constraint at z=1.0"""

    z = [1.0]

    def get_obs_x_y_err(self):
        # SMF from Weaver et al. (2022)
        lm, pD, dn, du = self.load_observation('../data/COSMOS2020/SMF_Farmer_v2.1_0.8z1.1_total.txt', cols=[0,2,3,4])
        hobs = 0.73
        y_obs = np.log10(pD) + 3.0 * np.log10(hobs/self.h0)
        y_dn = np.log10(pD) - np.log10(dn)
        y_up = np.log10(du) - np.log10(pD)
        x_obs = lm - 2.0 * np.log10(hobs/self.h0)

        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_extra_redshifts.csv', cols=[4,5])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z20(SMF):
    """The SMF constraint at z=2.0"""

    z = [2.0]

    def get_obs_x_y_err(self):
        # SMF from Weaver et al. (2022)
        lm, pD, dn, du = self.load_observation('../data/COSMOS2020/SMF_Farmer_v2.1_1.5z2.0_total.txt', cols=[0,2,3,4])
        hobs = 0.73
        y_obs = np.log10(pD) + 3.0 * np.log10(hobs/self.h0)
        y_dn = np.log10(pD) - np.log10(dn)
        y_up = np.log10(du) - np.log10(pD)
        x_obs = lm - 2.0 * np.log10(hobs/self.h0)

        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[12,13])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z30(SMF):
    """The SMF constraint at z=3.0"""

    z = [3.0]

    def get_obs_x_y_err(self):
        # SMF from Weaver et al. (2022)
        lm, pD, dn, du = self.load_observation('../data/COSMOS2020/SMF_Farmer_v2.1_2.5z3.0_total.txt', cols=[0,2,3,4])
        hobs = 0.73
        y_obs = np.log10(pD) + 3.0 * np.log10(hobs/self.h0)
        y_dn = np.log10(pD) - np.log10(dn)
        y_up = np.log10(du) - np.log10(pD)
        x_obs = lm - 2.0 * np.log10(hobs/self.h0)

        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[16,17])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage

class SMF_z40(SMF):
    """The SMF constraint at z=4.0"""

    z = [4.0]

    def get_obs_x_y_err(self):
        # SMF from Weaver et al. (2022)
        lm, pD, dn, du = self.load_observation('../data/COSMOS2020/SMF_Farmer_v2.1_3.5z4.5_total.txt', cols=[0,2,3,4])
        hobs = 0.73
        y_obs = np.log10(pD) + 3.0 * np.log10(hobs/self.h0)
        y_dn = np.log10(pD) - np.log10(dn)
        y_up = np.log10(du) - np.log10(pD)
        x_obs = lm - 2.0 * np.log10(hobs/self.h0)

        return x_obs, y_obs, y_dn, y_up
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[20,21])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
                              
class CSFRDH(Constraint):

    z = [0]
    domain = (0, 14) # look-back time in Gyr
    
    def get_obs_x_y_err(self):
#        zmin, zmax, logSFRD, err1, err2, err3 = self.load_observation('./data/Driver_SFRD.dat', cols=[1,2,3,5,6,7])
        my_cosmo = [100*self.h0, 0.0, self.Omega0, 1.0-self.Omega0]
        D18_cosmo = [70.0, 0., 0.3, 0.7]

        D23_0, D23_1, D23_2, D23_3, D23_4, D23_5 = self.load_observation('./data/CSFH_DSILVA+23_Ver_Final.csv', cols=[0,1,2,3,4,5])
        D23 = np.column_stack((D23_0, D23_1, D23_2, D23_3, D23_4, D23_5))
        z_D23 = D23[:,3]
        tLB_D23 = np.array([r.z2tL(z, self.h0, self.Omega0,  1.0-self.Omega0) for z in z_D23])
        CSFH_D23 = D23[:,0]
        for i in range(len(z_D23)):
            CSFH_D23[i] += np.log10( r.z2dA(z_D23[i], *my_cosmo) / r.z2dA(z_D23[i], *D18_cosmo) )*2 # adjust for assumed-cosmology influence on SFR calculations
            CSFH_D23[i] += np.log10( (r.comoving_distance(D23[i,3]+D23[i,4], *D18_cosmo)**3 - r.comoving_distance(D23[i,3]-D23[i,5], *D18_cosmo)**3) / (r.comoving_distance(D23[i,3]+D23[i,4], *my_cosmo)**3 - r.comoving_distance(D23[i,3]-D23[i,5], *my_cosmo)**3) )# adjust for assumed-cosmology influence on comoving volume
#        
#        Np = len(logSFRD)
#        x_obs = np.zeros(Np)
#        y_obs = np.zeros(Np)
#        for i in range(Np):
#            z_av = 0.5*(zmin[i]+zmax[i])
#            x_obs[i] = r.z2tL(z_av, h0, Omega0,  1.0-Omega0)
#            y_obs[i] = logSFRD[i] + \
#                        np.log10( pow(r.comoving_distance(zmax[i], *D18_cosmo), 3.0) - pow(r.comoving_distance(zmin[i], *D18_cosmo), 3.0) ) - \
#                        np.log10( pow(r.comoving_distance(zmax[i], *my_cosmo), 3.0) - pow(r.comoving_distance(zmin[i], *my_cosmo), 3.0) ) + \
#                        np.log10( r.z2dA(z_av, *my_cosmo) / r.z2dA(z_av, *D18_cosmo) ) * 2.0 # adjust for cosmology on comoving volume and luminosity of objects
#            
#        err_total = err1 + err2 + err3
        
        return tLB_D23, CSFH_D23, D23[:,2], D23[:,1]
        
    def get_model_x_y(self, _, _2, TimeBinEdge, SFRD_Age, _3, _4):
        return 0.5*(TimeBinEdge[1:]+TimeBinEdge[:-1]), SFRD_Age
        
class BHBM(Constraint):
    """The Black hole-Bulge mass relation constraint"""

    domain = (8.0, 12.0)

    def get_model_x_y(self, _, _2, _3, _4, BlackHoleMass, BulgeMass, _5, _6, _7, _8):
        
        mask = (BlackHoleMass > 0) & (BulgeMass > 0) & np.isfinite(BlackHoleMass) & np.isfinite(BulgeMass)
        y = BlackHoleMass[mask]
        x = BulgeMass[mask]
        
        if len(x) < 10:  # Not enough points for reliable median
            # Return dummy arrays that will result in poor fit
            return np.array([8.0, 12.0]), np.array([6.0, 8.0])
        
        # Create bins for bulge mass and calculate median black hole mass in each bin
        bin_edges = np.arange(8.0, 12.1, 0.2)  # Bins every 0.2 dex
        bin_centers = []
        median_bh_mass = []
        
        for i in range(len(bin_edges) - 1):
            bin_mask = (x >= bin_edges[i]) & (x < bin_edges[i+1])
            if np.sum(bin_mask) >= 5:  # At least 5 galaxies in bin
                bin_centers.append((bin_edges[i] + bin_edges[i+1]) / 2.0)
                median_bh_mass.append(np.median(y[bin_mask]))
        
        if len(bin_centers) < 3:  # Not enough bins for reliable relation
            return np.array([8.0, 12.0]), np.array([6.0, 8.0])
        
        return np.array(bin_centers), np.array(median_bh_mass)
    
class BHBM_z0(BHBM):
    """The BHBM constraint at z=0"""

    z = [0]

    def get_obs_x_y_err(self):
        
        # Häring & Rix 2004 relation
        w_bulge = 10. ** np.arange(8.0, 12.5, 0.1)  # More reasonable range
        BHdata_haring = 10. ** (8.2 + 1.12 * np.log10(w_bulge / 1.0e11))
        
        # Convert to log space
        x_points = np.log10(w_bulge)
        y_points = np.log10(BHdata_haring)
        
        # Typical scatter is ~0.3-0.4 dex
        scatter_dex = 0.34  # Häring & Rix 2004 intrinsic scatter
        
        # Use scatter as symmetric error
        err = np.ones_like(y_points) * scatter_dex
        
        return x_points, y_points, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        bulgemass, blackholemass = self.load_observation('./data/sage_bhbm_all_redshifts.csv', cols=[0,1])
        x_sage = bulgemass
        y_sage = blackholemass

        return x_sage, y_sage

_constraint_re = re.compile((r'([0-9_a-zA-Z]+)' # name
                              r'(?:\(([0-9\.]+)-([0-9\.]+)\))?' # domain boundaries
                              r'(?:\*([0-9\.]+))?')) # weight
def parse(spec, snapshot=None, sim=None, boxsize=None, vol_frac=None, age_alist_file=None, Omega0=None, h0=None, output_dir=None):
    """Parses a comma-separated string of constraint names into a list of
    Constraint objects. Specific domain values can be specified in `spec`"""

    _constraints = {
        'BHMF_z0': BHMF_z0,
        'BHMF_z10': BHMF_z10,
        'SMF_z0': SMF_z0,
        'SMF_z05': SMF_z05,
        'SMF_z10': SMF_z10,
        'SMF_z20': SMF_z20,
        'SMF_z30': SMF_z30,
        'SMF_z40': SMF_z40,
        'BHBM_z0': BHBM_z0
    }

    def _parse(s,output_dir):
        m = _constraint_re.match(s)
        if not m or m.group(1) not in _constraints:
            raise ValueError('Constraint does not specify a valid constraint: %s' % s)
        c = _constraints[m.group(1)](snapshot=snapshot, sim=sim, boxsize=boxsize, vol_frac=vol_frac, age_alist_file=age_alist_file,
                                     Omega0=Omega0, h0=h0, output_dir=output_dir)
        if m.group(2):
            dn, up = float(m.group(2)), float(m.group(3))
            if dn < c.domain[0]:
                raise ValueError('Constraint low boundary is lower than lowest value possible (%f < %f)' % (dn, c.domain[0]))
            if up > c.domain[1]:
                raise ValueError('Constraint up boundary is higher than lowest value possible (%f > %f)' % (up, c.domain[1]))
            c.domain = (dn, up)
        if m.group(4):
            c.weight = float(m.group(4))
        return c

    constraints = [_parse(s, output_dir) for s in spec.split(',')]
    total_weight = sum([c.weight for c in constraints])
    for c in constraints:
        c.rel_weight = c.weight / total_weight
    return constraints
