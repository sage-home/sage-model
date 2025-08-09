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

        elif class_name == 'SMF_z31':
            # Add SHARK for z=0
            mass, phi = self.load_observation('./data/SHARK_SMF.csv', cols=[8,9])

            ax.plot(mass, 10**phi, c='g', label='SHARK')

        elif class_name == 'SMF_z46':
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

    def plot_hsmr(self, x_obs, y_obs, y_mod, x_sage, y_sage, HaloMass, StellarMass, output_dir):
        """Plot Black Hole-Bulge Mass relation comparison"""
        plt.figure()
        ax = plt.subplot(111)
        #print(x_sage,y_sage)
        plt.plot(x_obs, y_mod, c='b', label='Model - SAGE')
        plt.plot(x_sage, y_sage, c='k', label='SAGE')
        plt.plot(x_obs, y_obs, c='r', label="Observation")

        w = np.where(StellarMass > 0.0)[0]
        if(len(w) > dilute): w = sample(list(range(len(w))), dilute)
        plt.scatter(HaloMass[w], StellarMass[w], s=0.5, c='orange', alpha=0.6, label='SAGE galaxies')

        class_name = self.__class__.__name__

        if class_name == 'HSMR_z0':
            # Add SHARK for z=0
            hmass, smass = self.load_observation('./data/SHARK_HSMR.csv', cols=[0,1])
            ax.plot(hmass, smass, c='g', label='SHARK')

        elif class_name == 'HSMR_z05':
            # Add SHARK for z=0
            hmass, smass = self.load_observation('./data/SHARK_HSMR.csv', cols=[2,3])
            ax.plot(hmass, smass, c='g', label='SHARK')

        elif class_name == 'HSMR_z10':
            # Add SHARK for z=0
            hmass, smass = self.load_observation('./data/SHARK_HSMR.csv', cols=[4,5])
            ax.plot(hmass, smass, c='g', label='SHARK')

        elif class_name == 'HSMR_z20':
            # Add SHARK for z=0
            hmass, smass = self.load_observation('./data/SHARK_HSMR.csv', cols=[6,7])

            ax.plot(hmass, smass, c='g', label='SHARK')

        elif class_name == 'HSMR_z30':
            # Add SHARK for z=0
            hmass, smass = self.load_observation('./data/SHARK_HSMR.csv', cols=[8,9])

            ax.plot(hmass, smass, c='g', label='SHARK')

        elif class_name == 'HSMR_z40':
            # Add SHARK for z=0
            hmass, smass = self.load_observation('./data/SHARK_HSMR.csv', cols=[10,11])

            ax.plot(hmass, smass, c='g', label='SHARK')

        plt.ylabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        plt.xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
        plt.axis([11.0, 15.0, 7.0, 13.0])

        leg = plt.legend(loc='lower right')
        leg.draw_frame(False)
        for t in leg.get_texts():
            t.set_fontsize('medium')
        plotfile = os.path.join(output_dir, 'hsmr_sage.png')
        plt.savefig(plotfile, dpi=100)
        plt.close()
        return
    
    def plot_smf_color(self, x_obs_red, y_obs_red, y_mod_red, x_obs_blue, y_obs_blue, y_mod_blue, x_sage_red, y_sage_red, x_sage_blue, y_sage_blue, output_dir):
        """Plot Red and Blue Stellar Mass Functions together on one plot, handling different array lengths"""
        plt.figure(figsize=(10, 8))
        ax = plt.subplot(111)

        # Red galaxies - no need to change x and y arrays as they match
        ax.plot(x_sage_red, 10**y_sage_red, c='k', ls='-', label='SAGE - Total')
        ax.plot(x_obs_red, 10**y_mod_red, c='darkred', label='Model - Red')
        ax.scatter(x_obs_red, 10**y_obs_red, marker='d', s=50, edgecolor='maroon', 
                facecolor='none', label='Observed - Red')
        
        # Blue galaxies - no need to change x and y arrays as they match
        ax.plot(x_obs_blue, 10**y_mod_blue, c='darkblue', label='Model - Blue')
        ax.scatter(x_obs_blue, 10**y_obs_blue, marker='s', s=50, edgecolor='navy', 
                facecolor='none', label='Observed - Blue')
        
        # Format the plot
        ax.set_yscale('log')
        ax.set_xlim(8.0, 12.2)
        ax.set_ylim(1.0e-6, 1.0e-1)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        
        # Add a title
        ax.set_title("Galaxy Stellar Mass Functions by Color", fontsize=14)
        
        # Create a legend
        leg = ax.legend(loc='upper right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)
        for t in leg.get_texts():
            t.set_fontsize('medium')
        
        plotfile = os.path.join(output_dir, 'smf_by_color.png')
        plt.savefig(plotfile, dpi=100)
        plt.close()
        return

    def plot_smf_red(self, x_obs_red, y_obs_red, y_mod_red, x_sage_red, y_sage_red, output_dir):
        """Plot Red and Blue Stellar Mass Functions together on one plot, handling different array lengths"""
        plt.figure(figsize=(10, 8))
        ax = plt.subplot(111)

        # Red galaxies - no need to change x and y arrays as they match
        ax.plot(x_sage_red, 10**y_sage_red, c='k', ls='-', label='SAGE - Total')
        ax.plot(x_obs_red, 10**y_mod_red, c='darkred', label='Model - Red')
        ax.scatter(x_obs_red, 10**y_obs_red, marker='d', s=50, edgecolor='maroon', 
                facecolor='none', label='Observed - Red')
        
        # Format the plot
        ax.set_yscale('log')
        ax.set_xlim(8.0, 12.2)
        ax.set_ylim(1.0e-6, 1.0e-1)
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        
        # Create a legend
        leg = ax.legend(loc='upper right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)
        for t in leg.get_texts():
            t.set_fontsize('medium')
        
        plotfile = os.path.join(output_dir, 'smf_red.png')
        plt.savefig(plotfile, dpi=100)
        plt.close()
        return
    
    def plot_target_smf(self, x_obs, y_obs, y_mod, x_sage, y_sage, output_dir):
        """Plot TARGET SMF comparison - unified function for all TARGET_SMF constraints"""
        plt.figure(figsize=(10, 7))
        ax = plt.subplot(111)

        # Plot current model (what PSO is optimizing)
        plt.plot(x_obs, 10**y_mod, 'b-', linewidth=2.5, label='Current Model (PSO)', zorder=3)
        
        # Plot target (SAGE 2.0 data we want to match)
        plt.plot(x_obs, 10**y_obs, 'r-', linewidth=2.5, label='Target (SAGE 2.0)', zorder=2)
        
        # Plot reference SAGE for context
        plt.plot(x_sage, 10**y_sage, 'k--', linewidth=2, alpha=0.7, label='Reference SAGE', zorder=1)
        
        # Calculate differences and add highlighting
        diff = np.abs(y_mod - y_obs)
        max_diff = np.max(diff)
        avg_diff = np.mean(diff)
        rms_diff = np.sqrt(np.mean(diff**2))
        
        # Shade regions where difference is large
        large_diff_mask = diff > avg_diff + np.std(diff)
        if np.any(large_diff_mask):
            plt.scatter(x_obs[large_diff_mask], 10**y_obs[large_diff_mask], 
                    s=100, c='orange', marker='o', alpha=0.7, 
                    label=f'Large differences', zorder=4)

        # Determine redshift and title from constraint class name
        constraint_name = self.__class__.__name__
        if 'TARGET_SMF_z0' in constraint_name:
            z_label = 'z=0'
            filename = 'target_smf_z0_progress.png'
        elif 'TARGET_SMF_z05' in constraint_name:
            z_label = 'z=0.5'
            filename = 'target_smf_z05_progress.png'
        elif 'TARGET_SMF_z10' in constraint_name:
            z_label = 'z=1.0'
            filename = 'target_smf_z10_progress.png'
        elif 'TARGET_SMF_z15' in constraint_name:
            z_label = 'z=1.5'
            filename = 'target_smf_z15_progress.png'
        elif 'TARGET_SMF_z20' in constraint_name:
            z_label = 'z=2.0'
            filename = 'target_smf_z20_progress.png'
        elif 'TARGET_SMF_z24' in constraint_name:
            z_label = 'z=2.4'
            filename = 'target_smf_z24_progress.png'
        elif 'TARGET_SMF_z31' in constraint_name:
            z_label = 'z=3.1'
            filename = 'target_smf_z31_progress.png'
        elif 'TARGET_SMF_z36' in constraint_name:
            z_label = 'z=3.6'
            filename = 'target_smf_z36_progress.png'
        elif 'TARGET_SMF_z46' in constraint_name:
            z_label = 'z=4.6'
            filename = 'target_smf_z46_progress.png'
        elif 'TARGET_SMF_z57' in constraint_name:
            z_label = 'z=5.7'
            filename = 'target_smf_z57_progress.png'
        elif 'TARGET_SMF_z63' in constraint_name:
            z_label = 'z=6.3'
            filename = 'target_smf_z63_progress.png'
        elif 'TARGET_SMF_z77' in constraint_name:
            z_label = 'z=7.7'
            filename = 'target_smf_z77_progress.png'
        elif 'TARGET_SMF_z85' in constraint_name:
            z_label = 'z=8.5'
            filename = 'target_smf_z85_progress.png'
        elif 'TARGET_SMF_z104' in constraint_name:
            z_label = 'z=10.4'
            filename = 'target_smf_z104_progress.png'
        else:
            # Generic fallback
            z_label = constraint_name.replace('TARGET_SMF_', '').replace('_', '=')
            filename = f'{constraint_name.lower()}_progress.png'

        plt.yscale('log')
        plt.axis([8.0, 12.2, 1.0e-6, 1.0e-1])
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))
        plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
        plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
        
        # Add statistics text box
        stats_text = f'Max diff: {max_diff:.3f}\nAvg diff: {avg_diff:.3f}\nRMS diff: {rms_diff:.3f}'
        plt.text(0.02, 0.98, stats_text, transform=ax.transAxes, 
                verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        
        plt.title(f'TARGET SMF {z_label}: PSO Progress', fontsize=14)
        leg = plt.legend(loc='upper right', numpoints=1, labelspacing=0.1)
        leg.draw_frame(False)
        for t in leg.get_texts():
            t.set_fontsize('medium')
            
        plotfile = os.path.join(output_dir, filename)
        plt.savefig(plotfile, dpi=100)
        plt.close()
        
        # Print progress info
        print(f"TARGET SMF {z_label}: Max diff = {max_diff:.3f}, Avg diff = {avg_diff:.3f}, RMS = {rms_diff:.3f}")
        
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

        # Linearly interpolate model Y values respect to the observations'
        # X values, and only take those within the domain.
        #print('x_mod:', x_mod)
        #print('y_mod:', y_mod)
        interp_func = interp1d(x_mod, y_mod, kind='linear', fill_value="extrapolate")
        y_mod = interp_func(x_obs)
        #y_mod = np.interp(x_obs, x_mod, y_mod)
        ind = np.where((x_obs >= self.domain[0]) & (x_obs <= self.domain[1]))
        err = y_dn
        err[y_mod > y_obs] = y_up[y_mod > y_obs] # take upper error when model above, lower when below
        err = err[ind]
        print('in get_data:')
        print('obs x:', x_obs[ind])
        print('obs y:', y_obs[ind])
        print('mod y:', y_mod[ind])

        # Get the constraint name and create filename directly in outdir
        constraint_name = self.__class__.__name__
        filename = os.path.join(self.output_dir, f"{constraint_name}_dump.txt")

        # Append data to dump file
        with open(filename, 'a') as f:
            f.write(f"# New Data Block\n")
            for x_val, y_val, mod_y_val in zip(x_obs, y_obs, y_mod):
                f.write(f"{x_val}\t{y_val}\t{mod_y_val}\n")
            
        # Get constraint name for appropriate plotting function
        constraint_name = self.__class__.__name__

        if constraint_name.startswith('TARGET_SMF'):
            self.plot_target_smf(x_obs, y_obs, y_mod, x_sage, y_sage, self.output_dir)
        if 'SMF' in constraint_name:
            if 'SMF_red' not in constraint_name and 'SMF_blue' not in constraint_name:
                self.plot_smf(x_obs, y_obs, y_mod, x_sage, y_sage, self.output_dir)
        if 'BHMF' in constraint_name:
            self.plot_bhmf(x_obs, y_obs, y_mod, x_sage, y_sage, self.output_dir)
        if 'BHBM' in constraint_name:
            self.plot_bhbm(x_obs, y_obs, y_mod, x_sage, y_sage, BlackHoleMass, BulgeMass, self.output_dir)
        if 'HSMR' in constraint_name:
            self.plot_hsmr(x_obs, y_obs, y_mod, x_sage, y_sage, HaloMass, StellarMass, self.output_dir)
        # If this is a red constraint, also create the combined plot
        if 'SMF_red' in constraint_name:
            try:
                # Get blue data
                blue_constraint = SMF_blue_z0(snapshot=self.snapshot, sim=self.sim, 
                                            boxsize=self.boxsize, vol_frac=self.vol_frac,
                                            age_alist_file=self.age_alist_file, 
                                            Omega0=self.Omega0, h0=self.h0,
                                            output_dir=self.output_dir)
                
                # Get blue data - use _get_raw_data to get all arrays
                x_obs_blue, y_obs_blue, _, _, x_sage_blue, y_sage_blue, x_mod_blue, y_mod_blue = blue_constraint._get_raw_data(modeldir, subvols)
                
                # Get interpolated model blue data to match observation x values
                interp_func_blue = interp1d(x_mod_blue, y_mod_blue, kind='linear', fill_value="extrapolate")
                y_mod_blue_interp = interp_func_blue(x_obs_blue)
                
                # Plot the combined function
                self.plot_smf_color(
                    x_obs, y_obs, y_mod,              # Red galaxy data (already interpolated)
                    x_obs_blue, y_obs_blue, y_mod_blue_interp,  # Blue galaxy data
                    x_sage, y_sage,                   # Red SAGE reference
                    x_sage_blue, y_sage_blue,         # Blue SAGE reference
                    self.output_dir
                )
            except Exception as e:
                print(f"Warning: Could not create combined color plot: {e}")

            else:
                self.plot_smf_red(x_obs, y_obs, y_mod, x_sage, y_sage, self.output_dir)

        return y_obs[ind], y_mod[ind], err

    def __str__(self):
        s = '%s, low=%.1f, up=%.1f, weight=%.2f, rel_weight=%.2f'
        args = self.__class__.__name__, self.domain[0], self.domain[1], self.weight, self.rel_weight
        return s % args

class SMD_evolution(Constraint):
    """Stellar Mass Density evolution constraint across redshifts"""
    
    domain = (0, 4)  # redshift range
    
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        # Define the redshifts and corresponding snapshots we want to evaluate
        # You can customize these based on your simulation's available snapshots
        self.redshifts = [0.0, 0.5, 1.0, 2.0, 3.0, 4.0]
        # Map redshifts to snapshot numbers - adjust these for your simulation
        self.snapshot_map = {
            0.0: 63,
            0.5: 38, 
            1.0: 40,
            2.0: 32,
            3.0: 16,
            4.0: 11
        }
        
    def _calculate_smd_for_snapshot(self, modeldir, snapshot):
        """Calculate stellar mass density for a specific snapshot"""
        fields = ['StellarMass']
        snap_num = f'Snap_{snapshot}'
        
        try:
            # Get list of model files
            model_files = [f for f in os.listdir(modeldir) if f.startswith('model_') and f.endswith('.hdf5')]
            model_files.sort()
            
            if len(model_files) > 1:
                # Combine data from multiple files
                combined_stellar_mass = []
                for model_file in model_files:
                    G = r.read_sage_hdf(os.path.join(modeldir, model_file), snap_num=snap_num, fields=fields)
                    combined_stellar_mass.append(G['StellarMass'])
                stellar_masses = np.concatenate(combined_stellar_mass)
            else:
                # Single file
                G = r.read_sage_hdf(os.path.join(modeldir, 'model_0.hdf5'), snap_num=snap_num, fields=fields)
                stellar_masses = G['StellarMass']
            
            # Convert to physical units and sum
            stellar_masses_physical = stellar_masses * 1e10 / self.h0  # Msun
            
            # Filter out very low mass galaxies and calculate total
            valid_masses = stellar_masses_physical[stellar_masses_physical > 1e8]  # > 10^8 Msun
            total_stellar_mass = np.sum(valid_masses)
            
            # Calculate stellar mass density (Msun/Mpc^3)
            smd = total_stellar_mass / self.vol
            
            return smd
            
        except Exception as e:
            print(f"Error calculating SMD for snapshot {snapshot}: {e}")
            return None
    
    def _load_model_data(self, modeldir, subvols):
        """Override to handle multiple snapshots for SMD evolution"""
        # First call parent method to set up basic parameters like h0, vol, etc.
        # We'll use the first snapshot for this
        original_snapshot = self.snapshot
        self.snapshot = [list(self.snapshot_map.values())[0]]  # Use first snapshot for setup
        
        # Call parent method to set up simulation parameters
        result = super()._load_model_data(modeldir, subvols)
        
        # Now calculate SMD for each redshift
        model_redshifts = []
        model_smds = []
        
        for z in self.redshifts:
            if z in self.snapshot_map:
                snap = self.snapshot_map[z]
                smd = self._calculate_smd_for_snapshot(modeldir, snap)
                if smd is not None:
                    model_redshifts.append(z)
                    model_smds.append(np.log10(smd))
        
        self.model_redshifts = np.array(model_redshifts)
        self.model_smds = np.array(model_smds)
        
        # Restore original snapshot
        self.snapshot = original_snapshot
        
        return result
    
    def get_model_x_y(self, hist_smf, hist_bhmf, TimeBinEdge, SFRD_Age, BlackHoleMass, BulgeMass, HaloMass, StellarMass, hist_smf_red, hist_smf_blue):
        """Return stellar mass density evolution data"""
        return self.model_redshifts, self.model_smds
    
    def get_obs_x_y_err(self):
        """Load COSMOS stellar mass density data"""
        # Load observational data from COSMOS
        # You'll need to provide the correct file with COSMOS SMD data
        # Expected format: redshift, log10(SMD [Msun/Mpc^3]), error
        try:
            redshift, log_smd, log_smd_err = self.load_observation('./data/SMD.ecsv', cols=[0,1,2])
            return redshift, log_smd, log_smd_err, log_smd_err
        except:
            # Fallback with some typical values if data file not available
            print("Warning: COSMOS_SMD.csv not found. Using placeholder data.")
            redshift = np.array([0.0, 0.5, 1.0, 2.0, 3.0, 4.0])
            # Typical stellar mass density values (log10 scale)
            log_smd = np.array([8.2, 8.1, 7.9, 7.6, 7.2, 6.8])  
            log_smd_err = np.array([0.1, 0.1, 0.15, 0.2, 0.2, 0.3])
            return redshift, log_smd, log_smd_err, log_smd_err
    
    def get_sage_x_y(self):
        """Load SAGE stellar mass density evolution reference data"""
        try:
            redshift, log_smd = self.load_observation('./data/sage_smd_evolution.csv', cols=[0,1])
            return redshift, log_smd
        except:
            # Fallback with SAGE reference values if file not available
            print("Warning: sage_smd_evolution.csv not found. Using placeholder data.")
            redshift = np.array([0.0, 0.5, 1.0, 2.0, 3.0, 4.0])
            log_smd = np.array([8.15, 8.05, 7.85, 7.55, 7.15, 6.75])
            return redshift, log_smd
    
    def plot_smd_evolution(self, x_obs, y_obs, y_mod, x_sage, y_sage, output_dir):
        """Plot stellar mass density evolution"""
        plt.figure(figsize=(10, 7))
        ax = plt.subplot(111)
        
        # Plot model, observations, and SAGE reference
        plt.plot(x_obs, 10**y_mod, 'b-', linewidth=2, label='Model - SAGE', marker='o', markersize=6)
        plt.plot(x_sage, 10**y_sage, 'k--', linewidth=2, label='SAGE Reference', marker='s', markersize=5)
        plt.errorbar(x_obs, 10**y_obs, yerr=0.1*10**y_obs, fmt='rd', markersize=8, 
                    capsize=5, capthick=2, label='COSMOS Observations')
        
        plt.yscale('log')
        plt.xlabel('Redshift', fontsize=12)
        plt.ylabel(r'Stellar Mass Density [M$_{\odot}$ Mpc$^{-3}$]', fontsize=12)
        plt.title('Stellar Mass Density Evolution', fontsize=14)
        plt.grid(True, alpha=0.3)
        
        # Invert x-axis to show cosmic time progression
        #plt.gca().invert_xaxis()
        
        leg = plt.legend(loc='best', frameon=False, fontsize=11)
        plt.tight_layout()
        
        plotfile = os.path.join(output_dir, 'smd_evolution_sage.png')
        plt.savefig(plotfile, dpi=100, bbox_inches='tight')
        plt.close()
        
        return
    
    def get_data(self, modeldir, subvols):
        """Override get_data to add custom plotting"""
        # Call parent get_data method
        result = super().get_data(modeldir, subvols)
        
        # Add our custom plot - get the data again for plotting
        x_obs, y_obs, y_dn, y_up = self.get_obs_x_y_err()
        x_sage, y_sage = self.get_sage_x_y()
        x_mod, y_mod = self.get_model_x_y(None, None, None, None, None, None, None, None, None, None)
        
        # Interpolate model to obs points for plotting
        if len(x_mod) > 1:
            interp_func = interp1d(x_mod, y_mod, kind='linear', fill_value="extrapolate")
            y_mod_interp = interp_func(x_obs)
        else:
            y_mod_interp = np.full_like(x_obs, y_mod[0] if len(y_mod) > 0 else 8.0)
        
        self.plot_smd_evolution(x_obs, y_obs, y_mod_interp, x_sage, y_sage, self.output_dir)
        
        return result

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
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[0,1])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
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
    """The BHMF constraint at z=1"""
    
    z = [1.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[2,3])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[2,3])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z20(BHMF):
    """The BHMF constraint at z=2"""
    
    z = [2.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[4,5])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[4,5])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z30(BHMF):
    """The BHMF constraint at z=3"""
    
    z = [3.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[6,7])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[6,7])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z40(BHMF):
    """The BHMF constraint at z=4"""
    
    z = [4.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[8,9])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[8,9])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z50(BHMF):
    """The BHMF constraint at z=5"""
    
    z = [5.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[10,11])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[10,11])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z60(BHMF):
    """The BHMF constraint at z=6"""
    
    z = [6.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[12,13])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[12,13])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z70(BHMF):
    """The BHMF constraint at z=7"""
    
    z = [7.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[14,15])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[14,15])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z80(BHMF):
    """The BHMF constraint at z=8"""
    
    z = [8.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[16,17])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[16,17])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class BHMF_z100(BHMF):
    """The BHMF constraint at z=10"""
    
    z = [10.0]

    def get_obs_x_y_err(self):
        #Load data from Zhang et al. (2023)
        logm, logphi = self.load_observation('./data/zhang_data.csv', cols=[18,19])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_bhmf_all_redshifts.csv', cols=[18,19])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_red(Constraint):
    """Common logic for SMF constraints"""

    domain = (8.0, 12.0)

    def get_model_x_y(self, _, _2, _3, _4, _5, _6, _7, _8, hist_smf_red, _9):
        y = hist_smf_red[0,:]
        ind = np.where(y < 0.)
        return xmf[ind], y[ind]
    
class SMF_red_z0(SMF_red):
    """The SMF constraint at z=0"""

    z = [0]

    def get_obs_x_y_err(self):

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
    
        x_obs = np.array([(value - (2*np.log10(self.h0/0.70))) for value in Baldry_Red_x])
        y_obs = np.array(Baldry_Red_y)
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[0,1])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_blue(Constraint):
    """Common logic for SMF constraints"""

    domain = (8.0, 12.0)

    def get_model_x_y(self, _, _2, _3, _4, _5, _6, _7, _8, _9, hist_smf_blue):
        y = hist_smf_blue[0,:]
        ind = np.where(y < 0.)
        return xmf[ind], y[ind]
    
class SMF_blue_z0(SMF_blue):
    """The SMF constraint at z=0"""

    z = [0]

    def get_obs_x_y_err(self):

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
        
        x_obs = np.array([(value - (2*np.log10(self.h0/0.70))) for value in Baldry_Blue_x])
        y_obs = np.array(Baldry_Blue_y)
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[0,1])
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
        # Load data from Driver et al. (2022)
        logm, logphi, dlogphi = self.load_observation('./data/GAMA_SMF_highres.csv', cols=[0,1,2])
        cosmology_correction_median = np.log10( r.comoving_distance(0.079, 100*self.h0, 0, self.Omega0, 1.0-self.Omega0) / r.comoving_distance(0.079, 70.0, 0, 0.3, 0.7) )
        cosmology_correction_maximum = np.log10( r.comoving_distance(0.1, 100*self.h0, 0, self.Omega0, 1.0-self.Omega0) / r.comoving_distance(0.1, 70.0, 0, 0.3, 0.7) )
        x_obs = logm + 2.0 * cosmology_correction_median 
        y_obs = logphi - 3.0 * cosmology_correction_maximum + 0.0807 # last factor accounts for average under-density of GAMA and to correct for this to be at z=0

        return x_obs, y_obs, dlogphi, dlogphi
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[0,1])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z02(SMF):
    """The SMF constraint at z=0.2"""

    z = [0.2]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[0,1])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[2,3])
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
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[2,3])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[4,5])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z08(SMF):
    """The SMF constraint at z=0.8"""

    z = [0.8]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[4,5])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[6,7])
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
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/Wright_2018_z1_z2.csv', cols=[0,1])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_extra_redshifts.csv', cols=[4,5])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z11(SMF):
    """The SMF constraint at z=1.1"""

    z = [1.1]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[6,7])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[8,9])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage

class SMF_z15(SMF):
    """The SMF constraint at z=1.5"""

    z = [1.5]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[8,9])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[10,11])
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
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/Wright_2018_z1_z2.csv', cols=[2,3])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[12,13])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z24(SMF):
    """The SMF constraint at z=2.4"""

    z = [2.4]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[12,13])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[14,15])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z31(SMF):
    """The SMF constraint at z=3.1"""

    z = [3.1]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[14,15])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[16,17])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z36(SMF):
    """The SMF constraint at z=3.6"""

    z = [3.6]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[16,17])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[18,19])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z46(SMF):
    """The SMF constraint at z=4.6"""

    z = [4.6]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[18,19])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[20,21])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z57(SMF):
    """The SMF constraint at z=5.7"""

    z = [5.7]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[20,21])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[22,23])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z63(SMF):
    """The SMF constraint at z=6.3"""

    z = [6.3]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[22,23])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[24,25])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z77(SMF):
    """The SMF constraint at z=7.7"""

    z = [7.7]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[24,25])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[26,27])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z85(SMF):
    """The SMF constraint at z=8.5"""

    z = [8.5]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[26,27])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))

        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[28,29])
        # Remove NaN values
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]

        return x_sage, y_sage
    
class SMF_z104(SMF):
    """The SMF constraint at z=10.4"""

    z = [10.4]

    def get_obs_x_y_err(self):
        # Load data from Shuntov et al. (2024)
        logm, logphi = self.load_observation('./data/shuntov_2024_all.csv', cols=[28,29])
        # Remove NaN values
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_obs = logm[valid_mask]
        y_obs = logphi[valid_mask]
        err = np.zeros(len(y_obs))
        
        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[30,31])
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
        
        if len(x) < 10:  # Not enough points for reliable fit
            # Return dummy arrays that will result in poor fit
            return np.array([8.0, 12.0]), np.array([6.0, 8.0])
            
        # Fit line to model data
        slope, intercept, r_value, _, _ = stats.linregress(x, y)
        
        # Generate points along the best-fit line
        x_points = np.linspace(8.0, 12.0, 20)  # Create 20 evenly spaced points
        y_points = slope * x_points + intercept
        
        return x_points, y_points
    
class BHBM_z0(BHBM):
    """The BHBM constraint at z=0"""

    z = [0]

    def get_obs_x_y_err(self):
        
        # Load observational data
        blackholemass, bulgemass = self.load_observation('./data/Haring_Rix_2004_line.csv', cols=[2,3])
        
        # Fit line to observational data
        slope, intercept, _, _, _ = stats.linregress(bulgemass, blackholemass)
        
        # Generate points along the best-fit line (same x-coordinates as model)
        x_points = np.linspace(8.0, 12.0, 20)
        y_points = slope * x_points + intercept
        
        # Small constant error for each point
        err = np.ones_like(y_points) * 0.1
        
        return x_points, y_points, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        bulgemass, blackholemass = self.load_observation('./data/sage_bhbm_all_redshifts.csv', cols=[0,1])
        x_sage = bulgemass
        y_sage = blackholemass

        return x_sage, y_sage
    
class BHBM_z20(BHBM):
    """The BHBM constraint at z=0"""

    z = [2.0]

    def get_obs_x_y_err(self):
        
        bulgemass, blackholemass = self.load_observation('./data/Zhang_BHBM_z2.csv', cols=[0,1])
        err = np.zeros(len(bulgemass))

        return bulgemass, blackholemass, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        bulgemass, blackholemass = self.load_observation('./data/sage_bhbm_all_redshifts.csv', cols=[12,13])
        x_sage = bulgemass
        y_sage = blackholemass

        return x_sage, y_sage
    
class HSMR(Constraint):
    """The Halo-Stellar mass relation constraint"""

    domain = (11, 15)

    def get_model_x_y(self, _, _2, _3, _4, _5, _6, HaloMass, StellarMass, _7, _8):
        y = StellarMass
        x = HaloMass

        # Define the bin edges
        bin_edges = np.arange(min(x), max(x) + 0.1, 0.1)

        # Assign each x-value to a bin
        bins = np.digitize(x, bin_edges)

        # Create a DataFrame to group and calculate medians
        data = pd.DataFrame({'x': x, 'y': y, 'bin': bins})

        # Calculate the median y-value for each bin
        medians = data.groupby('bin')['y'].median()

        # Map bins back to their corresponding bin centers
        bin_centers = {i: (bin_edges[i - 1] + bin_edges[i]) / 2 for i in range(1, len(bin_edges))}
        medians.index = medians.index.map(bin_centers)
        
        return medians.index, medians
    
class HSMR_z0(HSMR):
    """The HSMR constraint at z=0"""

    z = [0]

    def get_obs_x_y_err(self):
        
        halomass, stellarmass = self.load_observation('./data/Moster_2013.csv', cols=[0,1])
        err = np.zeros(len(halomass))

        return halomass, stellarmass, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        halomass, stellarmass = self.load_observation('./data/sage_halostellar_all_redshifts.csv', cols=[0,1])
        x_sage = halomass
        y_sage = stellarmass

        return x_sage, y_sage
    
class HSMR_z05(HSMR):
    """The HSMR constraint at z=0.5"""

    z = [0.5]

    def get_obs_x_y_err(self):
        
        halomass, stellarmass = self.load_observation('./data/Moster_2013.csv', cols=[2,3])
        err = np.zeros(len(halomass))

        return halomass, stellarmass, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        halomass, stellarmass = self.load_observation('./data/sage_halostellar_all_redshifts.csv', cols=[4,5])
        x_sage = halomass
        y_sage = stellarmass

        return x_sage, y_sage
    
class HSMR_z10(HSMR):
    """The HSMR constraint at z=1.0"""

    z = [1.0]

    def get_obs_x_y_err(self):
        
        halomass, stellarmass = self.load_observation('./data/Moster_2013.csv', cols=[4,5])
        err = np.zeros(len(halomass))

        return halomass, stellarmass, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        halomass, stellarmass = self.load_observation('./data/sage_halostellar_all_redshifts.csv', cols=[8,9])
        x_sage = halomass
        y_sage = stellarmass

        return x_sage, y_sage
    
class HSMR_z20(HSMR):
    """The HSMR constraint at z=2.0"""

    z = [2.0]

    def get_obs_x_y_err(self):
        
        halomass, stellarmass = self.load_observation('./data/Moster_2013.csv', cols=[6,7])
        err = np.zeros(len(halomass))

        return halomass, stellarmass, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        halomass, stellarmass = self.load_observation('./data/sage_halostellar_all_redshifts.csv', cols=[12,13])
        x_sage = halomass
        y_sage = stellarmass

        return x_sage, y_sage
    
class HSMR_z30(HSMR):
    """The HSMR constraint at z=3.0"""

    z = [3.0]

    def get_obs_x_y_err(self):
        
        halomass, stellarmass = self.load_observation('./data/Moster_2013.csv', cols=[8,9])
        err = np.zeros(len(halomass))

        return halomass, stellarmass, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        halomass, stellarmass = self.load_observation('./data/sage_halostellar_all_redshifts.csv', cols=[16,17])
        x_sage = halomass
        y_sage = stellarmass

        return x_sage, y_sage
    
class HSMR_z40(HSMR):
    """The HSMR constraint at z=4.0"""

    z = [4.0]

    def get_obs_x_y_err(self):
        
        halomass, stellarmass = self.load_observation('./data/Moster_2013.csv', cols=[10,11])
        err = np.zeros(len(halomass))

        return halomass, stellarmass, err, err
    
    def get_sage_x_y(self):
        # Load data from SAGE
        halomass, stellarmass = self.load_observation('./data/sage_halostellar_all_redshifts.csv', cols=[20,21])
        x_sage = halomass
        y_sage = stellarmass

        return x_sage, y_sage
    
class TARGET_SMF_z0(SMF):
    """SMF constraint that targets your SAGE 2.0 model's z=0 output"""
    
    z = [0]
    
    def get_obs_x_y_err(self):
        # Load the target SMF data from CSV
        target_data = self.load_observation('comprehensive_all_z0.0_0.5.csv', cols=[0,1])
        x_obs = target_data[0]
        y_obs = target_data[1]
        err = np.ones_like(y_obs) * 0.05  # 5% error
        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        # Keep the real SAGE comparison for reference in plots
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[0,1])
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        x_sage = logm[valid_mask]
        y_sage = logphi[valid_mask]
        return x_sage, y_sage

class TARGET_SMF_z05(SMF):
    """SMF constraint that targets your SAGE 2.0 model's z=0.5 output"""
    
    z = [0.5]
    
    def get_obs_x_y_err(self):
        target_data = np.loadtxt('./data/target_smf_data/target_smf_z05_sage_2.0.csv', 
                               delimiter=',', comments='#')
        x_obs = target_data[:, 0]
        y_obs = target_data[:, 1]
        err = np.ones_like(y_obs) * 0.05
        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[4,5])
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        return logm[valid_mask], logphi[valid_mask]

class TARGET_SMF_z10(SMF):
    """SMF constraint that targets your SAGE 2.0 model's z=1.0 output"""
    
    z = [1.0]
    
    def get_obs_x_y_err(self):
        target_data = np.loadtxt('./data/target_smf_data/target_smf_z10_sage_2.0.csv', 
                               delimiter=',', comments='#')
        x_obs = target_data[:, 0]
        y_obs = target_data[:, 1]
        err = np.ones_like(y_obs) * 0.05
        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        logm, phi = self.load_observation('./data/sage_smf_extra_redshifts.csv', cols=[4,5])
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        return logm[valid_mask], logphi[valid_mask]

class TARGET_SMF_z20(SMF):
    """SMF constraint that targets your SAGE 2.0 model's z=2.0 output"""
    
    z = [2.0]
    
    def get_obs_x_y_err(self):
        target_data = np.loadtxt('./data/target_smf_data/target_smf_z20_sage_2.0.csv', 
                               delimiter=',', comments='#')
        x_obs = target_data[:, 0]
        y_obs = target_data[:, 1]
        err = np.ones_like(y_obs) * 0.05
        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[12,13])
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        return logm[valid_mask], logphi[valid_mask]

class TARGET_SMF_z31(SMF):
    """SMF constraint that targets your SAGE 2.0 model's z=3.1 output"""
    
    z = [3.1]
    
    def get_obs_x_y_err(self):
        target_data = np.loadtxt('./data/target_smf_data/target_smf_z31_sage_2.0.csv', 
                               delimiter=',', comments='#')
        x_obs = target_data[:, 0]
        y_obs = target_data[:, 1]
        err = np.ones_like(y_obs) * 0.05
        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[16,17])
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        return logm[valid_mask], logphi[valid_mask]

class TARGET_SMF_z46(SMF):
    """SMF constraint that targets your SAGE 2.0 model's z=4.6 output"""
    
    z = [4.6]
    
    def get_obs_x_y_err(self):
        target_data = np.loadtxt('./data/target_smf_data/target_smf_z46_sage_2.0.csv', 
                               delimiter=',', comments='#')
        x_obs = target_data[:, 0]
        y_obs = target_data[:, 1]
        err = np.ones_like(y_obs) * 0.05
        return x_obs, y_obs, err, err
    
    def get_sage_x_y(self):
        logm, phi = self.load_observation('./data/sage_smf_all_redshifts.csv', cols=[20,21])
        logphi = np.log10(phi)
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        return logm[valid_mask], logphi[valid_mask]

_constraint_re = re.compile((r'([0-9_a-zA-Z]+)' # name
                              r'(?:\(([0-9\.]+)-([0-9\.]+)\))?' # domain boundaries
                              r'(?:\*([0-9\.]+))?')) # weight
def parse(spec, snapshot=None, sim=None, boxsize=None, vol_frac=None, age_alist_file=None, Omega0=None, h0=None, output_dir=None):
    """Parses a comma-separated string of constraint names into a list of
    Constraint objects. Specific domain values can be specified in `spec`"""

    _constraints = {
        'BHMF_z0': BHMF_z0,
        'BHMF_z10': BHMF_z10,
        'BHMF_z20': BHMF_z20,
        'BHMF_z30': BHMF_z30,
        'BHMF_z40': BHMF_z40,
        'BHMF_z50': BHMF_z50,
        'BHMF_z60': BHMF_z60,
        'BHMF_z70': BHMF_z70,
        'BHMF_z80': BHMF_z80,
        'BHMF_z100': BHMF_z100,
        'SMF_red_z0': SMF_red_z0,
        'SMF_blue_z0': SMF_blue_z0,
        'SMF_z0': SMF_z0,
        'SMF_z02': SMF_z02,
        'SMF_z05': SMF_z05,
        'SMF_z08': SMF_z08,
        'SMF_z10': SMF_z10,
        'SMF_z11': SMF_z11,
        'SMF_z15': SMF_z15,
        'SMF_z20': SMF_z20,
        'SMF_z24': SMF_z24,
        'SMF_z31': SMF_z31,
        'SMF_z36': SMF_z36,
        'SMF_z46': SMF_z46,
        'SMF_z57': SMF_z57,
        'SMF_z63': SMF_z63,
        'SMF_z77': SMF_z77,
        'SMF_z85': SMF_z85,
        'SMF_z104': SMF_z104,
        'BHBM_z0': BHBM_z0,
        'BHBM_z20': BHBM_z20,
        'HSMR_z0' : HSMR_z0,
        'HSMR_z05' : HSMR_z05,
        'HSMR_z10' : HSMR_z10,
        'HSMR_z20' : HSMR_z20,
        'HSMR_z30' : HSMR_z30,
        'HSMR_z40' : HSMR_z40,
        'TARGET_SMF_z0': TARGET_SMF_z0,
        'TARGET_SMF_z05': TARGET_SMF_z05,
        'TARGET_SMF_z10': TARGET_SMF_z10,
        'TARGET_SMF_z20': TARGET_SMF_z20,
        'TARGET_SMF_z31': TARGET_SMF_z31,
        'TARGET_SMF_z46': TARGET_SMF_z46
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
