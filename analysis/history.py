#!/usr/bin/env python

import matplotlib
matplotlib.use('Agg')

import plots as plots

# Import the subclasses that handle the different SAGE output formats.
from sage_binary import SageBinaryModel

try:
    from sage_hdf5 import SageHdf5Model
except ImportError:
    print("h5py not found.  If you're reading in HDF5 output from SAGE, please install "
          "this package.")

import numpy as np
import pylab as plt
from random import sample, seed
from os.path import getsize as getFileSize

# ================================================================================
# Basic variables
# ================================================================================

# Set up some basic attributes of the run

whichsimulation = 0
whichimf = 1        # 0=Slapeter; 1=Chabrier


class TemporalResults:
    """
    Defines all the parameters used to plot the models.

    Attributes
    ----------

    num_models : Integer
        Number of models being plotted.

    models : List of ``Model`` class instances with length ``num_models``
        Models that we will be plotting.  Depending upon the format ``SAGE`` output in,
        this will be a ``Model`` subclass with methods to parse the specific data format.

    plot_toggles : Dictionary
        Specifies which plots will be generated. An entry of `1` denotes
        plotting, otherwise it will be skipped.

    plot_output_path : String
        Base path where the plots will be saved.

    plot_output_format : String
        Format the plots are saved as.
    """

    def __init__(self, all_models_dict, plot_toggles, plot_output_path="./plots",
                 plot_output_format=".png", debug=False):
        """
        Initialises the individual ``Model`` class instances and adds them to
        the ``Results`` class instance.

        Parameters
        ----------

        all_models_dict : Dictionary
            Dictionary containing the parameter values for each ``Model``
            instance. Refer to the ``Model`` class for full details on this
            dictionary. Each field of this dictionary must have length equal to
            the number of models we're plotting.

        plot_toggles : Dictionary
            Specifies which plots will be generated. An entry of 1 denotes
            plotting, otherwise it will be skipped.

        plot_output_path : String, default "./plots"
            The path where the plots will be saved.

        plot_output_format : String, default ".png"
            Format the plots will be saved as.

        debug : {0, 1}, default 0
            Flag whether to print out useful debugging information.

        Returns
        -------

        None.
        """

        self.num_models = len(all_models_dict["model_path"])
        self.plot_output_path = plot_output_path

        if not os.path.exists(self.plot_output_path):
            os.makedirs(self.plot_output_path)

        self.plot_output_format = plot_output_format

        # We will create a list that holds the Model class for each model.
        all_models = []

        # Now let's go through each model, build an individual dictionary for
        # that model and then create a Model instance using it.
        for model_num in range(self.num_models):

            model_dict = {}
            for field in all_models_dict.keys():
                model_dict[field] = all_models_dict[field][model_num]

            # Use the correct subclass depending upon the format SAGE wrote in.
            if model_dict["sage_output_format"] == "sage_binary":
                model = SageBinaryModel(model_dict, plot_toggles)
            elif model_dict["sage_output_format"] == "sage_hdf5":
                model = SageHdf5Model(model_dict, plot_toggles)

            # We may be plotting the density at all snapshots...
            if model.density_redshifts is None:
                model.density_redshifts = model.redshifts

            model.plot_output_format = plot_output_format

            model.set_cosmology()

            # To be more memory concious, we calculate the required properties on a
            # file-by-file basis. This ensures we do not keep ALL the galaxy data in memory.
            plot_toggles_tmp = plot_toggles

            # The SMF and the Density plots may have different snapshot requirements.
            model.SMF_snaps = [(np.abs(model.redshifts - SMF_redshift)).argmin() for
                              SMF_redshift in model.SMF_redshifts]
            model.SMF_array = np.zeros((len(model.SMF_snaps),
                                        len(model.stellar_mass_bins)-1), dtype=np.float64)

            model.density_snaps = [(np.abs(model.redshifts - density_redshift)).argmin() for
                                  density_redshift in model.density_redshifts]
            model.SFRD = np.zeros(len(model.density_snaps), dtype=np.float64)
            model.SMD = np.zeros(len(model.density_snaps), dtype=np.float64)

            # We'll need to loop all snapshots, ignoring duplicates.
            snaps_to_loop = np.unique(model.SMF_snaps + model.density_snaps)

            snap_idx = 0
            for snap in snaps_to_loop:

                # Update the snapshot we're reading from. Subclass specific.
                model.update_snapshot(snap)

                # Calculate all the properties.
                model.calc_properties_all_files(plot_toggles, debug=debug)

                # We need to place the SMF inside the array to carry through.
                if snap in model.SMF_snap:
                    model.SMF_array[snap_idx, :] = model.SMF
                    snap_idx += 1
                exit()
            all_models.append(model)

        self.models = all_models
        self.plot_toggles = plot_toggles


    def do_plots(self):
        """
        Wrapper method to perform all the plotting for the models.

        Parameters
        ----------

        None.

        Returns
        -------

        None. The plots are saved individually by each method.
        """

        plot_toggles = self.plot_toggles
        plots.setup_matplotlib_options()

        # Depending upon the toggles, make the plots.
        if plot_toggles["SMF"] == 1:
            print("Plotting the Stellar Mass Function.")
            plots.plot_SMF(self)

# --------------------------------------------------------

    def StellarMassFunction(self, G_history):

        plt.figure()  # New figure
        ax = plt.subplot(111)  # 1 plot on the figure

        binwidth = 0.1  # mass function histogram bin width

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
        
        w = np.where(G_history[self.SMFsnaps[0]].StellarMass > 0.0)[0]
        mass = np.log10(G_history[self.SMFsnaps[0]].StellarMass[w] * 1.0e10 /self.Hubble_h)

        mi = np.floor(min(mass)) - 2
        ma = np.floor(max(mass)) + 2
        NB = (ma - mi) / binwidth

        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)

        # Set the x-axis values to be the centre of the bins
        xaxeshisto = binedges[:-1] + 0.5 * binwidth

        # Overplot the model histograms
        plt.plot(xaxeshisto, counts / self.volume *self.Hubble_h*self.Hubble_h*self.Hubble_h / binwidth, 'k-', label='Model galaxies')

        ###### z=1.3
        
        w = np.where(G_history[self.SMFsnaps[1]].StellarMass > 0.0)[0]
        mass = np.log10(G_history[self.SMFsnaps[1]].StellarMass[w] * 1.0e10 /self.Hubble_h)

        mi = np.floor(min(mass)) - 2
        ma = np.floor(max(mass)) + 2
        NB = (ma - mi) / binwidth

        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)

        # Set the x-axis values to be the centre of the bins
        xaxeshisto = binedges[:-1] + 0.5 * binwidth

        # Overplot the model histograms
        plt.plot(xaxeshisto, counts / self.volume *self.Hubble_h*self.Hubble_h*self.Hubble_h / binwidth, 'b-')

        ###### z=2
        
        w = np.where(G_history[self.SMFsnaps[2]].StellarMass > 0.0)[0]
        mass = np.log10(G_history[self.SMFsnaps[2]].StellarMass[w] * 1.0e10 /self.Hubble_h)

        mi = np.floor(min(mass)) - 2
        ma = np.floor(max(mass)) + 2
        NB = (ma - mi) / binwidth

        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)

        # Set the x-axis values to be the centre of the bins
        xaxeshisto = binedges[:-1] + 0.5 * binwidth

        # Overplot the model histograms
        plt.plot(xaxeshisto, counts / self.volume *self.Hubble_h*self.Hubble_h*self.Hubble_h / binwidth, 'g-')

        ###### z=3
        
        w = np.where(G_history[self.SMFsnaps[3]].StellarMass > 0.0)[0]
        mass = np.log10(G_history[self.SMFsnaps[3]].StellarMass[w] * 1.0e10 /self.Hubble_h)

        mi = np.floor(min(mass)) - 2
        ma = np.floor(max(mass)) + 2
        NB = (ma - mi) / binwidth

        (counts, binedges) = np.histogram(mass, range=(mi, ma), bins=NB)

        # Set the x-axis values to be the centre of the bins
        xaxeshisto = binedges[:-1] + 0.5 * binwidth

        # Overplot the model histograms
        plt.plot(xaxeshisto, counts / self.volume *self.Hubble_h*self.Hubble_h*self.Hubble_h / binwidth, 'r-')

        ######        

        plt.yscale('log', nonposy='clip')

        plt.axis([8.0, 12.5, 1.0e-6, 1.0e-1])

        # Set the x-axis minor ticks
        ax.xaxis.set_minor_locator(plt.MultipleLocator(0.1))

        plt.ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1}$)')  # Set the y...
        plt.xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')  # and the x-axis labels

        leg = plt.legend(loc='lower left', numpoints=1,
                         labelspacing=0.1)
        leg.draw_frame(False)  # Don't want a box frame
        for t in leg.get_texts():  # Reduce the size of the text
            t.set_fontsize('medium')

        outputFile = OutputDir + 'A.StellarMassFunction_z' + OutputFormat
        plt.savefig(outputFile)  # Save the figure
        plt.close()

        # Add this plot to our output list
        OutputList.append(outputFile)



# ---------------------------------------------------------

    def PlotHistory_SFRdensity(self, G_history):

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
        xErrLo = ObsSFRdensity[:, 0]-ObsSFRdensity[:, 2]
        xErrHi = ObsSFRdensity[:, 3]-ObsSFRdensity[:, 0]

        ObsSFR = np.log10(ObsSFRdensity[:, 1])
        yErrLo = np.log10(ObsSFRdensity[:, 1])-np.log10(ObsSFRdensity[:, 4])
        yErrHi = np.log10(ObsSFRdensity[:, 5])-np.log10(ObsSFRdensity[:, 1])

        # plot observational data (compilation used in Croton et al. 2006)
        plt.errorbar(ObsRedshift, ObsSFR, yerr=[yErrLo, yErrHi], xerr=[xErrLo, xErrHi], color='g', lw=1.0, alpha=0.3, marker='o', ls='none', label='Observations')

        SFR_density = np.zeros((LastSnap+1-FirstSnap))       
        for snap in xrange(FirstSnap,LastSnap+1):
          SFR_density[snap-FirstSnap] = sum(G_history[snap].SfrDisk+G_history[snap].SfrBulge) / self.volume * self.Hubble_h*self.Hubble_h*self.Hubble_h
    
        z = np.array(self.redshift)
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
        plt.close()
    
        # Add this plot to our output list
        OutputList.append(outputFile)


# ---------------------------------------------------------

    def StellarMassDensityEvolution(self, G_history):


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
        # Perez-Gonzalez (2008)
        pg2008 = np.array(((0.2,0.4,8.41,0.06,0.06),
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

        obs = (dickenson2003,drory2005,pg2008,glazebrook2004,
               fontana2006,rudnick2006,elsner2008)

        for o in obs:
            xval = ((o[:,1]-o[:,0])/2.)+o[:,0]
            if(whichimf == 0):
                ax.errorbar(xval, np.log10(10**o[:,2] *1.6), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')
            elif(whichimf == 1):
                ax.errorbar(xval, np.log10(10**o[:,2] *1.6/1.8), xerr=(xval-o[:,0], o[:,1]-xval), yerr=(o[:,3], o[:,4]), alpha=0.3, lw=1.0, marker='o', ls='none')
                

        smd = np.zeros((LastSnap+1-FirstSnap))       

        for snap in xrange(FirstSnap,LastSnap+1):
          w = np.where((G_history[snap].StellarMass/self.Hubble_h > 0.01) & (G_history[snap].StellarMass/self.Hubble_h < 1000.0))[0]
          if(len(w) > 0):
            smd[snap-FirstSnap] = sum(G_history[snap].StellarMass[w]) *1.0e10/self.Hubble_h / (self.volume /self.Hubble_h/self.Hubble_h/self.Hubble_h)

        z = np.array(self.redshift)
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
        plt.close()

        # Add this plot to our output list
        OutputList.append(outputFile)


# =================================================================

if __name__ == '__main__':

    import os

    # We support the plotting of an arbitrary number of models. To do so, simply add the
    # extra variables specifying the path to the model directory and other variables.
    # E.g., 'model1_sage_output_format = ...", "model1_dir_name = ...".
    # `first_file`, `last_file`, `simulation` and `num_tree_files` only need to be
    # specified if using binary output. HDF5 will automatically detect these.
    # `hdf5_snapshot` is only nedded if using HDF5 output.

    model0_SMF_z               = [0.0, 1.38, 1.38]  # Redshifts you wish to plot the stellar mass function at.
                                                  # Will search for the closest simulation redshift.
    model0_density_z           = [0.0, 1.38, 1.38]  # Redshifts you wish to plot the evolution of
                                       # densities at. Set to `None` for all redshifts.
    model0_alist_file          = "../input/millennium/trees/millennium.a_list"
    model0_sage_output_format  = "sage_binary"  # Format SAGE output in. "sage_binary" or "sage_hdf5".
    model0_dir_name            = "../tests/test_data/"
    model0_file_name           = "test_sage_z0.000"
    model0_IMF                 = "Chabrier"  # Chabrier or Salpeter.
    model0_model_label         = "Mini-Millennium"
    model0_color               = "c"
    model0_linestyle           = "-"
    model0_marker              = "x"
    model0_first_file          = 0  # The files read in will be [first_file, last_file]
    model0_last_file           = 0  # This is a closed interval.
    model0_simulation          = "Mini-Millennium"  # Sets the cosmology.
    model0_num_tree_files_used = 8  # Number of tree files processed by SAGE to produce this output.

    # Then extend each of these lists for all the models that you want to plot.
    # E.g., 'dir_names = [model0_dir_name, model1_dir_name, ..., modelN_dir_name]
    SMF_zs               = [model0_SMF_z]
    density_zs           = [model0_density_z]
    alist_files          = [model0_alist_file]
    sage_output_formats  = [model0_sage_output_format]
    dir_names            = [model0_dir_name]
    file_names           = [model0_file_name]
    IMFs                 = [model0_IMF]
    model_labels         = [model0_model_label]
    colors               = [model0_color]
    linestyles           = [model0_linestyle]
    markers              = [model0_marker]
    first_files          = [model0_first_file]
    last_files           = [model0_last_file]
    simulations          = [model0_simulation]
    num_tree_files_used  = [model0_num_tree_files_used]

    # A couple of extra variables...
    plot_output_format    = ".png"
    plot_output_path = "./plots_hdf5"  # Will be created if path doesn't exist.

    # These toggles specify which plots you want to be made.
    plot_toggles = {"SMF"             : 1,  # Stellar mass function at specified redshifts.
                    "SFRD"            : 1,  # Star formation rate density at specified snapshots. 
                    "SMD"             : 1}  # Stellar mass density at specified snapshots. 

    ############################
    ## DON'T TOUCH BELOW HERE ##
    ############################

    model_paths = []
    output_paths = []

    # Determine paths for each model.
    for dir_name, file_name  in zip(dir_names, file_names):

        model_path = "{0}/{1}".format(dir_name, file_name)
        model_paths.append(model_path)

        # These are model specific. Used for rare circumstances and debugging.
        output_path = dir_name + "plots/"

        if not os.path.exists(output_path):
            os.makedirs(output_path)

        output_paths.append(output_path)

    model_dict = { "SMF_redshifts"       : SMF_zs,
                   "density_redshifts"   : density_zs,
                   "alist_file"          : alist_files,
                   "sage_output_format"  : sage_output_formats,
                   "model_path"          : model_paths,
                   "output_path"         : output_paths,
                   "IMF"                 : IMFs,
                   "model_label"         : model_labels,
                   "color"               : colors,
                   "linestyle"           : linestyles,
                   "marker"              : markers,
                   "first_file"          : first_files,
                   "last_file"           : last_files,
                   "simulation"          : simulations,
                   "num_tree_files_used" : num_tree_files_used}

    # Read in the galaxies and calculate properties for each model.
    results = TemporalResults(model_dict, plot_toggles, plot_output_path, plot_output_format,
                              debug=False)
    results.do_plots()

    # Set the error settings to the previous ones so we don't annoy the user.
    np.seterr(divide=old_error_settings["divide"], over=old_error_settings["over"],
              under=old_error_settings["under"], invalid=old_error_settings["invalid"])
