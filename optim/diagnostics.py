#!/bin/bash

"""
This script produces the diagnostic plots used to help visualise the PSO process
One is a 3D representation of the swarm movement in the parameter space the other
is the Log Liklihood over iteration for each particle
"""
import warnings
warnings.filterwarnings("ignore")
import argparse
import itertools
import os
import re
import common
import logging
import matplotlib # type: ignore
import matplotlib.animation as anim # type: ignore
import matplotlib.cm as cmx # type: ignore
import matplotlib.pyplot as plt # type: ignore
import matplotlib.image as mpimg # type: ignore
from matplotlib.backends.backend_pdf import PdfPages # type: ignore
from matplotlib import cm # type: ignore
from matplotlib.colors import Normalize # type: ignore
import numpy as np # type: ignore
import routines as r
import analysis
import glob
import seaborn as sns # type: ignore
import pandas as pd # type: ignore
import matplotlib.colors # type: ignore
from redshift_utils import get_redshift_info, get_all_redshifts
from scipy import stats
from sklearn.linear_model import LinearRegression
import matplotlib.colors as cols
from pso_uncertainty import analyze_pso_uncertainties, plot_parameter_distributions, \
                         create_uncertainty_report, analyze_and_plot

logger = logging.getLogger('diagnostics')

pos_re = re.compile('track_[0-9]+_pos.npy')
fx_re = re.compile('track_[0-9]+_fx.npy')

def load_observation(*args, **kwargs):
    obsdir = os.path.dirname(os.path.abspath(__file__))
    return common.load_observation(obsdir, *args, **kwargs)

def load_space_and_particles(tracks_dir, space_file):
    """Loads the PSO pos/fx information stored within a directory, plus the
    original search space file"""

    all_fnames = list(os.listdir(tracks_dir))
    pos_fnames = list(filter(lambda x: pos_re.match(x), all_fnames))
    fx_fnames = list(filter(lambda x: fx_re.match(x), all_fnames))

    if len(pos_fnames) != len(fx_fnames):
        logger.info("Different number of pos/fx files, using common files only")
        l = min(len(pos_fnames), len(fx_fnames))
        del pos_fnames[l:]
        del fx_fnames[l:]

    logger.info("Loading %d pos/fx files" % len(pos_fnames))

    # Read files in filename order and populate pos/fx np arrays
    pos_fnames.sort()
    fx_fnames.sort()
    pos = []
    fx = []
    for pos_fname, fx_fname in zip(pos_fnames, fx_fnames):
        pos.append(np.load(os.path.join(tracks_dir, pos_fname)))
        fx.append(np.load(os.path.join(tracks_dir, fx_fname)))

    # after this fx dims are (S, L), pos dims are (S, D, L)
    pos, fx = np.asarray(pos), np.asarray(fx)
    pos = np.moveaxis(pos, 0, -1)
    fx = np.moveaxis(fx, 0, -1)
    logger.info("Position shape: %s, Fitness shape: %s", str(pos.shape), str(fx.shape))
    #logger.info(fx)
    #logger.info(pos)

    space = analysis.load_space(space_file)
    if space.shape[0] != pos.shape[1]:
        raise ValueError("Particles have different dimensionality than space")
        
    #logger.info('Ordered fits:\n -logLikelihood,', space['name'])
    #logger.info(np.column_stack((np.log10(np.sort(fx, axis=None)), np.moveaxis(pos,1,-1)[np.unravel_index(np.argsort(fx, axis=None), fx.shape)])))
    #logger.info(np.column_stack(((np.sort(fx, axis=None)), np.moveaxis(pos,1,-1)[np.unravel_index(np.argsort(fx, axis=None), fx.shape)])))

    return space, pos, fx

def plot_pairplot_with_contours(space, pos, fx, cmap='viridis', hist_edgecolor='k', hist_bins=10):
    """
    Produce a pairplot showing parameter distributions and correlations.
    
    Parameters:
    - space: Dictionary with plot labels for axes
    - pos: 3D array of particle positions (iterations, dimensions, particles)
    - cmap: Colormap for density plots
    - hist_edgecolor: Color of histogram edges
    - hist_bins: Number of bins in histograms
    
    Returns:
    - A seaborn PairGrid object
    """
    
    # Rearrange positions to match DataFrame expectations
    S, D, L = pos.shape
    pos = np.swapaxes(pos, 0, 1)
    
    # Create DataFrame of all positions across iterations
    df = pd.DataFrame(pos.reshape((D, S * L)).T, columns=space['plot_label'])
    
    # Create pairplot
    g = sns.pairplot(df, corner=True, 
                     diag_kind="hist",
                     plot_kws={'alpha': 0.5},
                     diag_kws={'edgecolor': hist_edgecolor, 
                              'bins': hist_bins})
    
    # Add KDE plots to show density
    for i, ax in enumerate(g.axes.flat):
        if ax is None:
            continue
            
        row, col = divmod(i, len(space['plot_label']))
        if row > col:  # Lower triangle
            x_label = space['plot_label'][col]
            y_label = space['plot_label'][row]
            
            x_data = df[x_label]
            y_data = df[y_label]
            
            # Add KDE contours
            sns.kdeplot(data=df,
                       x=x_label,
                       y=y_label,
                       ax=ax,
                       cmap=cmap,
                       levels=10,
                       alpha=0.5)
            
            # Add correlation coefficient
            corr = df[x_label].corr(df[y_label])
            ax.text(0.05, 0.95, f'r = {corr:.2f}',
                   transform=ax.transAxes,
                   ha='left', va='top')
    
    return g

def create_iteration_plot(filename, num_particles, num_iterations, obs_data, sage_data, track_folder, plot_type='SMF'):
    """
    Generic function to create iteration plots with customized settings per plot type.
    """
    # Plot settings dictionary for different constraint types
    plot_settings = {
        'SMF': {
            'xlabel': r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$',
            'ylabel': r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$',
            'xlim': [8.0, 12.2],
            'ylim': [1.0e-6, 1.0e-1],
            'yscale': 'log',
            'legend_loc': 'lower left',
            'transform_y': lambda y: 10**np.array(y)
        },
        'BHMF': {
            'xlabel': r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$',
            'ylabel': r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$',
            'xlim': [6.0, 10.2],
            'ylim': [1.0e-6, 1.0e-1],
            'yscale': 'log',
            'legend_loc': 'upper right',
            'transform_y': lambda y: 10**np.array(y)
        },
        'BHBM': {
            'xlabel': r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$',
            'ylabel': r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$',
            'xlim': [8.0, 12.0],
            'ylim': [6.0, 10.0],
            'yscale': 'linear',
            'legend_loc': 'upper left',
            'transform_y': lambda y: y
        },
        'HSMR': {
            'xlabel': r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$',
            'ylabel': r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$',
            'xlim': [11.5, 15.0],
            'ylim': [7.0, 13.0],
            'yscale': 'linear',
            'legend_loc': 'lower right',
            'transform_y': lambda y: y
        }
    }

    settings = plot_settings[plot_type]
    x_obs, y_obs, obs_label = obs_data
    x_sage, y_sage, sage_label = sage_data
    
    # Convert y values based on plot type
    if plot_type in ['SMF', 'BHMF']:
        y_obs_converted = [10**y for y in y_obs]
        y_sage_converted = y_sage # Changed this line
    else:
        y_obs_converted = y_obs
        y_sage_converted = y_sage

    # Process data
    with open(filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse blocks
    for line in lines:
        line = line.strip()
        if line.startswith("# New Data Block"):
            if current_block_y:
                current_iteration.append(current_block_y)
                current_block_y = []
                capture_x = False
                if len(current_iteration) == num_particles:
                    blocks_by_iteration.append(current_iteration)
                    current_iteration = []
                    if len(blocks_by_iteration) == num_iterations:
                        break
        elif line:
            values = list(map(float, line.split('\t')))
            if capture_x:
                x_values.append(values[0])
            current_block_y.append(values[2])

    # Process scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    all_scores = np.concatenate(np.log10(fit_scores))
    
    # Create plot
    fig, ax = plt.subplots(figsize=(10, 7))
    lowest_score = np.inf
    lowest_score_line = None
    
    # Setup colormap
    colormap = cm.get_cmap('viridis_r', num_iterations)
    norm = Normalize(vmin=0, vmax=num_iterations - 1)

    # Plot iterations
    transform_y = settings['transform_y']
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        color = colormap(iteration_index)
        for particle_index, y in enumerate(blocks):
            transformed_y = transform_y(y)
            ax.plot(x_values, transformed_y, color=color, alpha=0.1, linewidth=0.75)
            
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

    ax.scatter(x_obs, y_obs_converted, color='k', s=50, marker='d', label=obs_label)
    ax.plot(x_sage, y_sage_converted, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    # Add SHARK data if available
    if plot_type == 'SMF' and 'SMF_z0_dump.txt' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[0,1])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'SMF' and 'SMF_z05_dump.txt' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[2,3])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'SMF' and 'SMF_z10_dump.txt' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[4,5])
        #print(mass, transform_y(phi))
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'SMF' and 'SMF_z20_dump.txt' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[6,7])
        #print(mass, transform_y(phi))
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'SMF' and 'SMF_z31_dump.txt' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[8,9])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'SMF' and 'SMF_z46_dump.txt' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[10,11])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'BHBM' and 'BHBM_z0_dump.txt' in filename:
        mass, phi = load_observation('SHARK_BHBM_z0.csv', cols=[0,1])
        ax.plot(mass, phi, 'g--', label='SHARK')

    elif plot_type == 'HSMR' and 'HSMR_z0_dump.txt' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[0,1])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'HSMR' and 'HSMR_z05_dump.txt' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[2,3])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'HSMR' and 'HSMR_z10_dump.txt' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[4,5])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'HSMR' and 'HSMR_z20_dump.txt' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[6,7])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'HSMR' and 'HSMR_z30_dump.txt' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[8,9])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')

    elif plot_type == 'HSMR' and 'HSMR_z40_dump.txt' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[10,11])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
        

    # Setup axes
    ax.set_xlabel(settings['xlabel'], fontsize=12)
    ax.set_ylabel(settings['ylabel'], fontsize=12)
    ax.set_xlim(settings['xlim'])
    ax.set_ylim(settings['ylim'])
    if settings['yscale'] == 'log':
        ax.set_yscale('log')

    # Add colorbar
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array([])
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Iteration Number')

    # Add legend
    leg = ax.legend(loc=settings['legend_loc'])
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def smf_processing_iteration(*args, **kwargs):
    return create_iteration_plot(*args, **kwargs, plot_type='SMF')

def bhmf_processing_iteration(*args, **kwargs):
    return create_iteration_plot(*args, **kwargs, plot_type='BHMF')

def bhbm_processing_iteration(*args, **kwargs):
    return create_iteration_plot(*args, **kwargs, plot_type='BHBM')

def hsmr_processing_iteration(*args, **kwargs):
    return create_iteration_plot(*args, **kwargs, plot_type='HSMR')

def load_all_params(directory, param_names, redshifts):
    """Load parameter values from CSV files"""
    particle_data = {}
    best_params = {}
    best_scores = {}
    
    for z in redshifts:
        _, z_str = get_redshift_info(z=z)
        if z_str is None:
            logger.info(f"Unknown redshift mapping for z={z}")
            continue
            
        filename = os.path.join(directory, f'params_z{z_str}.csv')
        
        try:
            if os.path.exists(filename):
                df = pd.read_csv(filename, delimiter='\t', header=None, 
                               names=param_names + ['score'])
                
                particle_data[z] = df.iloc[:-2]
                best_params[z] = df.iloc[-2].values[:-1]  
                best_scores[z] = float(df.iloc[-1].iloc[0])
                logger.info(f"Successfully loaded data for z={z}")
            else:
                logger.info(f"File not found for z={z}: {filename}")
                continue
                
        except Exception as e:
            logger.error(f"Error processing file for z={z}: {str(e)}")
            continue
    
    return particle_data, best_params, best_scores

def setup_plot_style():
    """Setup consistent plot styling"""
    sns.set_theme(style="white", font_scale=1.2)
    sns.set_palette("deep")
    plt.rcParams['figure.facecolor'] = 'white'
    plt.rcParams['axes.facecolor'] = 'white'
    #plt.rcParams['grid.color'] = '#EEEEEE'
    #plt.rcParams['grid.linestyle'] = '-'

def extract_redshift(filename):
    """Extract redshift value from filename for sorting."""
    z, _ = get_redshift_info(filename=filename)
    return z if z is not None else float('inf')

def create_combined_constraint_grids(output_dir='parameter_plots', png_dir=None):
    """
    Creates combined grid figures for each plot type across different constraints (SMF, BHMF, BHBM).
    
    Parameters:
    -----------
    output_dir : str
        Directory where the grid plots will be saved
    png_dir : str
        Directory containing the source PNG files
    """
    if png_dir is None:
        png_dir = '.'
    elif not os.path.exists(png_dir):
        logger.info(f"PNG directory {png_dir} does not exist!")
        return
    
    from math import ceil, sqrt
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Define patterns to match different plot types
    plot_patterns = {
        'iterations': '*_all.png'
    }
    
    # Define constraints to look for
    constraints = ['SMF', 'BHMF', 'BHBM', 'HSMR']
    
    # Process each plot type
    for plot_type, pattern in plot_patterns.items():
        constraint_files = {}
        for file in glob.glob(os.path.join(png_dir, f'*{pattern}')):
            constraint = os.path.basename(file).split('_')[0]
            z = extract_redshift(os.path.basename(file))
            if constraint not in constraint_files:
                constraint_files[constraint] = {}
            constraint_files[constraint][z] = file
        
        if not constraint_files:
            continue
        
        constraints = sorted(constraint_files.keys())
        redshifts = sorted(set(z for c in constraints for z in constraint_files[c]))
        
        n_cols = len(constraints)
        n_rows = len(redshifts)
        
        fig = plt.figure(figsize=(6*n_cols, 5*n_rows))
        
        for col, constraint in enumerate(constraints, 1):
            for row, z in enumerate(redshifts, 1):
                file = constraint_files[constraint].get(z)
                if file:
                    img = mpimg.imread(file)
                    ax = fig.add_subplot(n_rows, n_cols, (row-1)*n_cols + col)
                    ax.imshow(img)
                    ax.axis('off')
                    if row == 1:
                        ax.set_title(constraint)
                    if col == 1:
                        ax.text(-0.1, 0.5, f'z={z:.1f}', rotation=90, transform=ax.transAxes, va='center')
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f'combined_{plot_type}_grid.png'), dpi=300)
        plt.close()
        
        logger.info(f"Created combined {plot_type} grid figure")

def load_sage_data():
    """Load SMF data from SAGE-miniUchuu"""
    # Load main SAGE data
    sage_data = load_observation('sage_smf_all_redshifts.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
                                                              15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31])
    
    # Load z=1.0 data from the separate file
    sage_z1_data = load_observation('sage_smf_extra_redshifts.csv', cols=[4,5])
    
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0.0, 0.2, 0.5, 0.8, 1.0, 1.1, 1.5, 2.0, 2.4, 3.1, 3.6, 4.6, 5.7, 6.3, 7.7, 8.5, 10.4]
    
    # Column indices for main SAGE data
    col_indices = [(0,1), (2,3), (4,5), (6,7), None, (8,9), (10,11), (12,13), (14,15), 
                   (16,17), (18,19), (20,21), (22,23), (24,25), (26,27), (28,29), (30,31)]
    
    # Process data for each redshift
    for z, indices in zip(redshifts, col_indices):
        if z == 1.0:
            # Handle z=1.0 data from separate file
            logm = sage_z1_data[0]
            logphi = sage_z1_data[1]
        else:
            # Handle data from main SAGE file
            mass_idx, phi_idx = indices
            logm = sage_data[mass_idx]
            logphi = sage_data[phi_idx]
        
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        data_by_z[z] = (logm[valid_mask], logphi[valid_mask], f'SAGE (miniuchuu) (z={z})')
    
    return data_by_z

def load_sage_data_forBHMF():
    """Load BHMF data from SAGE-miniUchuu"""
    sage_data = load_observation('sage_bhmf_all_redshifts.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
                                                              15,16,17,18,19])
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0]
    col_indices = [(0,1), (2,3), (4,5), (6,7), (8,9), (10,11), (12,13), (14,15), 
                   (16,17), (18,19)]
    
    # Process data for each redshift
    for z, (mass_idx, phi_idx) in zip(redshifts, col_indices):
        logm = sage_data[mass_idx]
        logphi = sage_data[phi_idx]
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        data_by_z[z] = (logm[valid_mask], logphi[valid_mask], f'SAGE (miniuchuu) (z={z})')
    
    return data_by_z

def load_sage_data_forHSMR():
    """Load HSMR data from SAGE-miniUchuu"""
    sage_data = load_observation('sage_halostellar_all_redshifts.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
                                                              15,16,17,18,19,20,21])
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0.0, 0.5, 1.0, 2.0, 3.0, 4.0]
    col_indices = [(0,1), (4,5), (8,9), (12,13), (16,17), (20,21)]
    
    # Process data for each redshift
    for z, (mass_idx, phi_idx) in zip(redshifts, col_indices):
        logm = sage_data[mass_idx]
        logphi = sage_data[phi_idx]
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        data_by_z[z] = (logm[valid_mask], logphi[valid_mask], f'SAGE (miniuchuu) (z={z})')
    
    return data_by_z

def load_bhbm_data():
    """Load BHBM data from SAGE-miniUchuu and observations"""
    # Load observational data from Haring & Rix 2004
    blackholemass, bulgemass = load_observation('Haring_Rix_2004_line.csv', cols=[2,3])
    bulgemass_z2, blackholemass_z2 = load_observation('Zhang_BHBM_z2.csv', cols=[0,1])
    log_blackholemass = blackholemass
    log_bulgemass = bulgemass
    
    # Load SAGE data
    sage_bhbm_data = load_observation('sage_bhbm_all_redshifts.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13])
    
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0.0, 2.0]
    col_indices = [(0,1), (12,13)]
    
    # Process data for each redshift
    for z, (mass_idx, bh_idx) in zip(redshifts, col_indices):
        logm_sage = sage_bhbm_data[mass_idx]
        logbh_sage = sage_bhbm_data[bh_idx]
        valid_mask = ~np.isnan(logm_sage) & ~np.isnan(logbh_sage)
        
        if z == 0.0:
            # For z=0, include both observational and SAGE data
            data_by_z[z] = (
                (log_bulgemass, log_blackholemass, 'Haring & Rix 2004'),
                (logm_sage[valid_mask], logbh_sage[valid_mask], f'SAGE (miniuchuu) (z={z})')
            )
        else:
            # For other redshifts, only SAGE data
            data_by_z[z] = (
                (bulgemass_z2, blackholemass_z2, 'Zhang et al. 2023'),
                (logm_sage[valid_mask], logbh_sage[valid_mask], f'SAGE (miniuchuu) (z={z})')
            )
    
    return data_by_z

def load_hsmr_data():
    """Load HSMR data"""
    new_data = load_observation('Moster_2013.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11])
    data_by_z = {}
    
    redshifts = [0.0, 0.5, 1.0, 2.0, 3.0, 4.0] # Add relevant redshifts
    col_indices = [(0,1), (2,3), (4,5), (6,7), (8,9), (10,11)] # Add column indices
    
    for z, (idx1, idx2) in zip(redshifts, col_indices):
        x = new_data[idx1]
        y = new_data[idx2] 
        valid_mask = ~np.isnan(x) & ~np.isnan(y)
        data_by_z[z] = (x[valid_mask], y[valid_mask], f'Moster et al., 2013 (z={z})')
    
    return data_by_z

def load_shuntov_data():
    """Load SMF data from Shuntov et al. 2024"""
    shuntov_data = load_observation('shuntov_2024_all.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
                                                              15,16,17,18,19,20,21,22,23,24,25,26,27,28,29])
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0.2, 0.5, 0.8, 1.0, 1.1, 1.5, 2.0, 2.4, 3.1, 3.6, 4.6, 5.7, 6.3, 7.7, 8.5, 10.4]
    col_indices = [(0,1), (2,3), (4,5), (4,5), (6,7), (8,9), (10,11), (12,13), (14,15), 
                   (16,17), (18,19), (20,21), (22,23), (24,25), (26,27), (28,29)]
    
    # Process data for each redshift
    for z, (mass_idx, phi_idx) in zip(redshifts, col_indices):
        logm = shuntov_data[mass_idx]
        logphi = shuntov_data[phi_idx]
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        data_by_z[z] = (logm[valid_mask], logphi[valid_mask], f'Shuntov et al., 2024 (z={z})')
    
    return data_by_z

def load_zhang_data():
    """Load SMF data from Zhang et al. 2024"""
    zhang_data = load_observation('zhang_data.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
                                                              15,16,17,18,19])
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0]
    col_indices = [(0,1), (2,3), (4,5), (6,7), (8,9), (10,11), (12,13), (14,15), 
                   (16,17), (18,19)]
    
    # Process data for each redshift
    for z, (mass_idx, phi_idx) in zip(redshifts, col_indices):
        logm = zhang_data[mass_idx]
        logphi = zhang_data[phi_idx]
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        data_by_z[z] = (logm[valid_mask], logphi[valid_mask], f'Zhang et al., 2024 (z={z})')
    
    return data_by_z

def load_gama_data(config_opts):
    """Load GAMA data for z=0"""
    import routines as r
    logm, logphi, dlogphi = load_observation('GAMA_SMF_highres.csv', cols=[0,1,2])
    
    # Use passed parameters
    cosmology_correction_median = np.log10(r.comoving_distance(0.079, 100*config_opts.h0, 0, config_opts.Omega0, 1.0-config_opts.Omega0) / 
                                         r.comoving_distance(0.079, 70.0, 0, 0.3, 0.7))
    cosmology_correction_maximum = np.log10(r.comoving_distance(0.1, 100*config_opts.h0, 0, config_opts.Omega0, 1.0-config_opts.Omega0) / 
                                          r.comoving_distance(0.1, 70.0, 0, 0.3, 0.7))
    
    x_obs = logm + 2.0 * cosmology_correction_median 
    y_obs = logphi - 3.0 * cosmology_correction_maximum + 0.0807
    
    return x_obs, y_obs, 'Driver et al., 2022 (GAMA)'

def load_ilbert_data(config_opts):
    """Load GAMA data for z=0"""
    import routines as r
    logm, logphi = load_observation('Ilbert_2010_z1.csv', cols=[0,1])
    
    x_obs = logm
    y_obs = logphi
    
    return x_obs, y_obs, 'Ilbert et al., 2010'

def load_wright_z1_data(config_opts):
    """Load GAMA data for z=0"""
    import routines as r
    logm, logphi = load_observation('Wright_2018_z1_z2.csv', cols=[0,1])
    
    x_obs = logm
    y_obs = logphi
    
    return x_obs, y_obs, 'Wright et al., 2018'

def load_wright_z2_data(config_opts):
    """Load GAMA data for z=0"""
    import routines as r
    logm, logphi = load_observation('Wright_2018_z1_z2.csv', cols=[2,3])
    
    x_obs = logm
    y_obs = logphi
    
    return x_obs, y_obs, 'Wright et al., 2018'

def get_smf_files_map(config_opts):
    """Create mapping of SMF dump files to their corresponding observational data"""
    # Load all observational data
    shuntov_data = load_shuntov_data()
    gama_data = load_gama_data(config_opts)
    sage_data = load_sage_data()
    ilbert_data = load_ilbert_data(config_opts)
    wright_z1_data = load_wright_z1_data(config_opts)
    wright_z2_data = load_wright_z2_data(config_opts)
    
    logger.info("Checking for SMF dump files in directory...")
    smf_files = {}
    
    # Handle z=0 case specially since it uses GAMA data
    filename = f'SMF_z0_dump.txt'
    filepath = os.path.join(config_opts.outdir, filename)
    if os.path.exists(filepath):
        logger.info(f"Found: {filename}")
        smf_files[filename] = (gama_data, sage_data[0.0])
    else:
        logger.info(f"Not found: {filename}")

    # Handle z=0 case specially since it uses GAMA data
    filename = f'SMF_z10_dump.txt'
    filepath = os.path.join(config_opts.outdir, filename)
    if os.path.exists(filepath):
        logger.info(f"Found: {filename}")
        smf_files[filename] = (wright_z1_data, sage_data[1.0])
    else:
        logger.info(f"Not found: {filename}")

    # Handle z=0 case specially since it uses GAMA data
    filename = f'SMF_z20_dump.txt'
    filepath = os.path.join(config_opts.outdir, filename)
    if os.path.exists(filepath):
        logger.info(f"Found: {filename}")
        smf_files[filename] = (wright_z2_data, sage_data[2.0])
    else:
        logger.info(f"Not found: {filename}")
    
    # Handle all other redshifts using Shuntov data
    for z in get_all_redshifts():
        if z in [0.0,1.0,2.0]:  # Skip z=0 as it's handled above
            continue
        
        _, z_str = get_redshift_info(z=z)
        if z_str is None:
            continue
        filename = f'SMF_z{z_str}_dump.txt'
        filepath = os.path.join(config_opts.outdir, filename)
        if os.path.exists(filepath):
            logger.info(f"Found: {filename}")
            smf_files[filename] = (shuntov_data[z], sage_data[z])
        else:
            logger.info(f"Not found: {filename}")
    
    logger.info(f"Found {len(smf_files)} SMF files to process")
    return smf_files

def get_bhmf_files_map(config_opts):
    """Create mapping of BHMF dump files to their corresponding observational data"""
    # Load all observational data
    zhang_data = load_zhang_data()
    sage_data = load_sage_data_forBHMF()
    
    logger.info("Checking for BHMF dump files in directory...")
    bhmf_files = {}
    
    for z in get_all_redshifts():
        _, z_str = get_redshift_info(z=z)
        if z_str is None:
            continue
            
        filename = f'BHMF_z{z_str}_dump.txt'
        filepath = os.path.join(config_opts.outdir, filename)
        if os.path.exists(filepath):
            logger.info(f"Found: {filename}")
            bhmf_files[filename] = (zhang_data[z], sage_data[z])
        else:
            logger.info(f"Not found: {filename}")
    
    logger.info(f"Found {len(bhmf_files)} BHMF files to process")
    return bhmf_files

def get_bhbm_files_map(config_opts):
    """Create mapping of BHBM dump files to their corresponding observational data"""
    # Load observational and SAGE data
    bhbm_data = load_bhbm_data()
    
    logger.info("Checking for BHBM dump files in directory...")
    bhbm_files = {}
    
    for z in get_all_redshifts():
        _, z_str = get_redshift_info(z=z)
        if z_str is None:
            continue
            
        filename = f'BHBM_z{z_str}_dump.txt'
        filepath = os.path.join(config_opts.outdir, filename)
        if os.path.exists(filepath):
            logger.info(f"Found: {filename}")
            obs_data, sage_data = bhbm_data[z]
            bhbm_files[filename] = (obs_data, sage_data)
        else:
            logger.info(f"Not found: {filename}")
  
    
    logger.info(f"Found {len(bhbm_files)} BHBM files to process")
    return bhbm_files

def get_hsmr_files_map(config_opts):
    """Create mapping of new constraint dump files to observations"""
    new_data = load_hsmr_data()
    sage_data = load_sage_data_forHSMR()
    
    logger.info("Checking for new constraint dump files...")
    files = {}
    
    for z in get_all_redshifts():
        _, z_str = get_redshift_info(z=z)
        if z_str is None:
            continue
            
        filename = f'HSMR_z{z_str}_dump.txt'
        filepath = os.path.join(config_opts.outdir, filename)
        if os.path.exists(filepath):
            files[filename] = (new_data[z], sage_data[z])
            
    return files

def file_exists_and_not_empty(filepath):
    """Check if a file exists and is not empty."""
    if not os.path.exists(filepath):
        return False
    return os.path.getsize(filepath) > 0

def read_smf_dump_file(filename, n_particles, skip_iterations):
    """
    Read SMF dump file and extract SMF values for all particles.
    Each particle is marked by "# New Data Block" in the file.
    Skips the first skip_iterations iterations.
    
    Parameters:
    -----------
    filename : str
        Path to the SMF dump file
    n_particles : int
        Number of particles per iteration  
    skip_iterations : int
        Number of iterations to skip at the start (default=5)
        
    Returns:
    --------
    x_values : array
        Mass bins
    smf_values : array 
        SMF values for all particles after skipped iterations
    """
    logger = logging.getLogger('diagnostics')
    x_values = None 
    current_block = []
    all_blocks = []
    current_iteration = []
    iteration_blocks = []
    
    logger.info(f"Reading file: {filename}")
    logger.info(f"Skipping first {skip_iterations} iterations ({skip_iterations * n_particles} blocks)")
    
    try:
        with open(filename, 'r') as f:
            for line in f:
                line = line.strip()
                if line.startswith("# New Data Block"):
                    if current_block:
                        values = np.array(current_block)
                        if x_values is None:
                            x_values = values[:, 0]  # Mass bins
                            
                        current_iteration.append(values[:, 2])
                        if len(current_iteration) == n_particles:
                            iteration_blocks.append(np.array(current_iteration))
                            current_iteration = []
                        
                        current_block = []
                elif line:
                    try:
                        values = list(map(float, line.split('\t')))
                        current_block.append(values)
                    except (ValueError, IndexError):
                        continue
                        
        # Process last block if needed
        if current_block:
            values = np.array(current_block)
            current_iteration.append(values[:, 2])
            if len(current_iteration) == n_particles:
                iteration_blocks.append(np.array(current_iteration))
        
        # Skip iterations and process remaining ones
        if skip_iterations < len(iteration_blocks):
            iteration_blocks = iteration_blocks[skip_iterations:]
            smf_values = np.vstack(iteration_blocks)
        else:
            logger.error(f"Not enough iterations to skip {skip_iterations}")
            return None, None
            
        if smf_values is not None:
            logger.info(f"Mass bins shape: {x_values.shape}")
            logger.info(f"SMF values shape: {smf_values.shape}")
            logger.info(f"Skipped {skip_iterations} iterations, kept {len(iteration_blocks)} iterations")
        
        return x_values, smf_values
        
    except Exception as e:
        logger.error(f"Error reading SMF dump file: {str(e)}")
        return None, None

def get_aligned_parameter_values(track_files, n_particles, n_iterations, skip_iterations):
    """
    Load parameter values from track files and align them with SMF data iterations.
    
    Parameters:
    -----------
    track_files : list
        List of sorted track file paths
    n_particles : int
        Number of particles per iteration
    n_iterations : int  
        Number of iterations to load after skipping
    skip_iterations : int
        Number of iterations to skip at start (default=5)
        
    Returns:
    --------
    param_values : array
        Parameter values aligned with SMF data after skipped iterations
        Shape: (n_iterations * n_particles, n_params)
    """
    logger = logging.getLogger('diagnostics')
    
    start_idx = skip_iterations
    end_idx = skip_iterations + n_iterations
    
    # Verify we have enough files
    if end_idx > len(track_files):
        logger.error(f"Not enough track files for requested iterations")
        return None
        
    # Load only the iterations we need
    all_positions = []
    for pos_file in track_files[start_idx:end_idx]:
        try:
            pos = np.load(pos_file)
            if pos.shape[0] != n_particles:
                logger.warning(f"Unexpected number of particles in {pos_file}: {pos.shape[0]} vs expected {n_particles}")
                continue
            all_positions.append(pos)
        except Exception as e:
            logger.error(f"Error loading track file {pos_file}: {e}")
            continue
            
    if not all_positions:
        logger.error("No valid position data loaded")
        return None
        
    # Stack positions into single array
    param_values = np.vstack(all_positions)
    
    logger.info(f"Loaded parameter values shape: {param_values.shape}")
    logger.info(f"Skipped first {skip_iterations} iterations, using {len(all_positions)} iterations")
    
    return param_values

def processing(tracks_dir, space_file, output_dir, config_opts, space=None):
    logger.info("Starting diagnostics analysis...")

    # Load particle data
    space, pos, fx = load_space_and_particles(tracks_dir, space_file)
    S, D, L = pos.shape
    logger.info('Producing plots for S=%d particles, D=%d dimensions, L=%d iterations' % (S, D, L))

    # Create output directory if needed
    os.makedirs(output_dir, exist_ok=True)
    num_particles = S
    num_iterations = L

    # Get SMF and BHMF files mapping with observational data
    logger.info("Looking for dump files...")
    smf_files = get_smf_files_map(config_opts)
    logger.info(f"Found {len(smf_files)} SMF files to process")

    bhmf_files = get_bhmf_files_map(config_opts)
    logger.info(f"Found {len(bhmf_files)} BHMF files to process")

    bhbm_files = get_bhbm_files_map(config_opts)
    logger.info(f"Found {len(bhbm_files)} BHBM files to process")   

    hsmr_files = get_hsmr_files_map(config_opts)
    logger.info(f"Found {len(hsmr_files)} HSMR files to process") 

    # Process SMF files
    processed_any_smf = False
    for filename, (obs_data, sage_data) in smf_files.items():
        filepath = os.path.join(output_dir, filename)
        
        if os.path.exists(filepath):
            logger.info(f"\nProcessing {filename}...")
            processed_any_smf = True
            
            # Create iteration plot
            try:
                logger.info("Creating iteration plot...")
                fig = smf_processing_iteration(
                    filepath,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    tracks_dir
                )
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_all.png')
                fig.savefig(outfile, dpi=300)
                logger.info(f"Saved iteration plot to {outfile}")
                plt.close(fig)
            except Exception as e:
                logger.error(f"Error creating iteration plot: {str(e)}")

    if not processed_any_smf:
        logger.info("Warning: No SMF files were found to process!")
        logger.info(f"Expected files in: {output_dir}")
        logger.info("Expected files: %s", list(smf_files.keys()))

    # Process BHMF files
    processed_any_bhmf = False
    for filename, (obs_data, sage_data) in bhmf_files.items():
        filepath = os.path.join(output_dir, filename)
        
        if os.path.exists(filepath):
            logger.info(f"\nProcessing {filename}...")
            processed_any_bhmf = True
            
            # Create iteration plot
            try:
                logger.info("Creating iteration plot...")
                fig = bhmf_processing_iteration(
                    filepath,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    tracks_dir
                )
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_all.png')
                fig.savefig(outfile, dpi=300)
                logger.info(f"Saved iteration plot to {outfile}")
                plt.close(fig)
            except Exception as e:
                logger.error(f"Error creating iteration plot: {str(e)}")
 
    if not processed_any_bhmf:
        logger.info("Warning: No BHMF files were found to process!")
        logger.info(f"Expected files in: {output_dir}")
        logger.info("Expected files: %s", list(bhmf_files.keys())) 

    # Process BHBM files
    processed_any_bhbm = False
    for filename, (obs_data, sage_data) in bhbm_files.items():
        filepath = os.path.join(output_dir, filename)
        
        if os.path.exists(filepath):
            logger.info(f"\nProcessing {filename}...")
            processed_any_bhbm = True
            
            # Create iteration plot
            try:
                logger.info("Creating iteration plot...")
                fig = bhbm_processing_iteration(
                    filepath,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    tracks_dir
                )
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_all.png')
                fig.savefig(outfile, dpi=300)
                logger.info(f"Saved iteration plot to {outfile}")
                plt.close(fig)
            except Exception as e:
                logger.error(f"Error creating iteration plot: {str(e)}")

    if not processed_any_bhbm:
        logger.info("Warning: No BHBM files were found to process!")
        logger.info(f"Expected files in: {output_dir}")
        logger.info("Expected files: %s", list(bhbm_files.keys()))   

    # Process HSMR files
    processed_any_hsmr = False
    for filename, (obs_data, sage_data) in hsmr_files.items():
        filepath = os.path.join(output_dir, filename)
        
        if os.path.exists(filepath):
            logger.info(f"\nProcessing {filename}...")
            processed_any_hsmr = True
            
            # Create iteration plot
            try:
                logger.info("Creating iteration plot...")
                fig = hsmr_processing_iteration(
                    filepath,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    tracks_dir
                )
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_all.png')
                fig.savefig(outfile, dpi=300)
                logger.info(f"Saved iteration plot to {outfile}")
                plt.close(fig)
            except Exception as e:
                logger.error(f"Error creating iteration plot: {str(e)}")
            
    if not processed_any_hsmr:
        logger.info("Warning: No HSMR files were found to process!")
        logger.info(f"Expected files in: {output_dir}")
        logger.info("Expected files: %s", list(hsmr_files.keys()))


    # Load parameter values
    logger.info("Processing parameter evolution...")
    param_names = ['SFR efficiency', 'Reheating epsilon', 'Ejection efficiency', 'Reincorporation efficiency',
                   'Radio Mode', 'Quasar Mode', 'Black Hole growth', 'Baryon Fraction']
    
    # Map the file numbers to actual redshifts
    redshift_map = {
        0.0: '0',
        0.2: '02',
        0.5: '05',
        0.8: '08',
        1.0: '10',
        1.1: '11',
        1.5: '15',
        2.0: '20',
        2.4: '24',
        3.1: '31',
        3.6: '36',
        4.6: '46',
        5.7: '57',
        6.3: '63',
        7.7: '77',
        8.5: '85',
        10.4: '104'
    }
    
    processed_redshifts = []
    for z, z_str in redshift_map.items():
        param_file = os.path.join(output_dir, f'params_z{z_str}.csv')
        if os.path.exists(param_file):
            processed_redshifts.append(z)
            logger.info(f"Found parameter file for z={z}")
        else:
            logger.debug(f"No parameter file found for z={z}")

    if not processed_redshifts:
        logger.warning("No parameter files found!")
        return

    processed_redshifts.sort()
    logger.info(f"Found parameter files for redshifts: {processed_redshifts}")
    
    particle_data, best_params, best_scores = load_all_params(output_dir, param_names, processed_redshifts)
    
    if particle_data:

        track_files = sorted(glob.glob(os.path.join(tracks_dir, 'track_*_pos.npy')))
        if not track_files:
            logger.error("Missing track files")
            return
        
        results, scores = analyze_and_plot(
            tracks_dir=tracks_dir,
            space_file=space_file,
            output_dir=output_dir,
            csv_output_path=output_dir
        )

        # After creating all individual plots
        logger.info("Creating combined constraint grid plots...")
        try:
            create_combined_constraint_grids(output_dir=output_dir, png_dir=output_dir)
            logger.info("Successfully created combined constraint grid plots")
        except Exception as e:
            logger.error(f"Error creating combined constraint grid plots: {str(e)}")
            
        logger.info("All plots have been saved to: %s", output_dir)
    else:
        logger.info("No parameter files found for visualization!")
    print(particle_data, best_params, best_scores)
    return particle_data, best_params, best_scores
        
def main(tracks_dir, space_file, output_dir, config_opts, space=None):
    
    processing(tracks_dir, space_file, output_dir, config_opts, space=space)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-S', '--space-file',
        help='File with the search space specification, defaults to space.txt',
        default='space.txt')
    parser.add_argument('-o', '--output-dir',
        help='Output directory for plots',
        required=True)
    parser.add_argument('tracks_dir',
        help='Directory containing PSO tracks')
    opts = parser.parse_args()

    # Create minimal config_opts object for standalone usage
    class ConfigOpts:
        def __init__(self):
            self.h0 = 0.677400
            self.Omega0 = 0.3089
            self.username = None
    
    config_opts = ConfigOpts()

    # Load space file for parameter bounds
    space_obj = analysis.load_space(opts.space_file)
    
    main(opts.tracks_dir, opts.space_file, opts.output_dir, config_opts, space=space_obj)

    main(opts.tracks_dir, opts.space_file, opts.output_dir)