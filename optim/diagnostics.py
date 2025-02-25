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
            ax.plot(x_values, transformed_y, color=color, alpha=0.2, linewidth=0.75)
            
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

    ax.plot(x_obs, y_obs_converted, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage_converted, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    # Add SHARK data if available
    if plot_type == 'SMF' and 'SMF_z0' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[0,1])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'SMF' and 'SMF_z05' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[2,3])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'SMF' and 'SMF_z10' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[4,5])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'SMF' and 'SMF_z20' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[6,7])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'SMF' and 'SMF_z31' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[8,9])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'SMF' and 'SMF_z46' in filename:
        mass, phi = load_observation('SHARK_SMF.csv', cols=[10,11])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'BHBM' and 'BHBM_z0' in filename:
        mass, phi = load_observation('SHARK_BHBM_z0.csv', cols=[0,1])
        ax.plot(mass, phi, 'g--', label='SHARK')
    elif plot_type == 'HSMR' and 'HSMR_z0' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[0,1])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'HSMR' and 'HSMR_z05' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[2,3])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'HSMR' and 'HSMR_z10' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[4,5])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'HSMR' and 'HSMR_z20' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[6,7])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'HSMR' and 'HSMR_z30' in filename:
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[8,9])
        ax.plot(mass, transform_y(phi), 'g--', label='SHARK')
    elif plot_type == 'HSMR' and 'HSMR_z40' in filename:
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

def plot_parameter_evolution(particle_data, best_params, best_scores, param_names, output_dir, space=None):
    """Create parameter evolution plots, only plotting parameters defined in space file"""
    os.makedirs(output_dir, exist_ok=True)
    setup_plot_style()
    
    if not particle_data or not best_params:
        logger.info("No parameter data available for plotting")
        return
    
    # Map between display names and internal names
    param_mapping = {
        'SFR efficiency': 'SfrEfficiency',
        'Reheating epsilon': 'FeedbackReheatingEpsilon',
        'Ejection efficiency': 'FeedbackEjectionEfficiency',
        'Reincorporation efficiency': 'ReIncorporationFactor',
        'Radio Mode': 'RadioModeEfficiency',
        'Quasar Mode': 'QuasarModeEfficiency',
        'Black Hole growth': 'BlackHoleGrowthRate',
        'Baryon Fraction': 'BaryonFrac'
    }
    
    # Get parameters actually defined in space file
    space_params = []
    param_indices = []
    
    for i, name in enumerate(param_names):
        # Find the corresponding internal name in space file
        for display_name, internal_name in param_mapping.items():
            if internal_name in space['name']:
                if display_name == name:
                    space_params.append(name)
                    param_indices.append(i)
                    break
    
    if not space_params:
        logger.info("No matching parameters found in space file")
        return
        
    logger.info(f"Creating plots for parameters: {space_params}")
    
    redshifts = sorted(best_params.keys())
    lookback_times = [r.z2tL(z) for z in redshifts]
    
    # Calculate number of rows and columns needed
    n_params = len(space_params)
    n_rows = (n_params + 1) // 2  # Round up division
    
    # Create figures
    fig_z, axs_z = plt.subplots(n_rows, 2, figsize=(15, 5*n_rows))
    fig_t, axs_t = plt.subplots(n_rows, 2, figsize=(15, 5*n_rows))
    
    # Handle single row case
    axs_z_flat = axs_z.flatten() if n_rows > 1 else [axs_z] if n_params == 1 else axs_z
    axs_t_flat = axs_t.flatten() if n_rows > 1 else [axs_t] if n_params == 1 else axs_t
    
    # Plot parameters
    for plot_idx, (param_idx, param) in enumerate(zip(param_indices, space_params)):
        # Set up axes
        ax_z = axs_z_flat[plot_idx]
        ax_t = axs_t_flat[plot_idx]
        
        # Get parameter bounds from space file
        space_name = param_mapping.get(param)
        space_idx = np.where(space['name'] == space_name)[0][0]
        lb = space['lb'][space_idx]
        ub = space['ub'][space_idx]
        
        # Plot bounds
        for ax in [ax_z, ax_t]:
            ax.axhspan(lb, ub, color='gray', alpha=0.1, zorder=0, label='Parameter bounds')
            ax.axhline(lb, color='gray', linestyle='--', alpha=0.3, zorder=0)
            ax.axhline(ub, color='gray', linestyle='--', alpha=0.3, zorder=0)
            bound_margin = (ub - lb) * 0.1
            ax.set_ylim(lb - bound_margin, ub + bound_margin)
        
        # Get best values and calculate statistics
        best_values = [best_params[z][param_idx] for z in redshifts]
        means = []
        stds = []
        all_values = []
        
        for z in redshifts:
            param_values = particle_data[z].iloc[:, param_idx].values
            means.append(np.mean(param_values))
            stds.append(np.std(param_values))
            all_values.extend(param_values)
        
        means = np.array(means)
        stds = np.array(stds)
        global_mean = np.mean(all_values)
        
        # Plot on redshift axis
        ax_z.fill_between(redshifts, means - stds, means + stds, 
                         alpha=0.3, color='r', label='±1σ range')
        ax_z.plot(redshifts, means, '-', color='red', 
                  linewidth=1.5, label='Mean')
        ax_z.plot(redshifts, best_values, 'k--o', 
                  linewidth=2, markersize=8, label='Best fit')
        ax_z.axhline(global_mean, color='blue', linestyle=':', 
                     linewidth=2, label='Global Mean')
        
        # Plot on lookback time axis
        ax_t.fill_between(lookback_times, means - stds, means + stds, 
                         alpha=0.3, color='r', label='±1σ range')
        ax_t.plot(lookback_times, means, '-', color='red', 
                  linewidth=1.5, label='Mean')
        ax_t.plot(lookback_times, best_values, 'k--o', 
                  linewidth=2, markersize=8, label='Best fit')
        ax_t.axhline(global_mean, color='blue', linestyle=':', 
                     linewidth=2, label='Global Mean')
        
        # Set labels and titles
        ax_z.set_xlabel('Redshift')
        ax_z.set_ylabel('Value')
        ax_z.set_title(param)
        
        ax_t.set_xlabel('Look-back Time (Gyr)')
        ax_t.set_ylabel('Value')
        ax_t.set_title(param)
        
        # Add legend to first plot only
        if plot_idx == 0:
            ax_z.legend(loc='best', frameon=False)
            ax_t.legend(loc='best', frameon=False)
    
    # Remove any unused subplots
    for i in range(len(space_params), len(axs_z_flat)):
        if i < len(axs_z_flat):  # Check if index is valid
            fig_z.delaxes(axs_z_flat[i])
            fig_t.delaxes(axs_t_flat[i])
    
    # Save plots
    plt.figure(fig_z.number)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_evolution_redshift.png'), 
                bbox_inches='tight', dpi=300)
    
    plt.figure(fig_t.number)
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_evolution_lookback.png'), 
                bbox_inches='tight', dpi=300)
    
    plt.close(fig_z)
    plt.close(fig_t)

def extract_redshift(filename):
    """Extract redshift value from filename for sorting."""
    z, _ = get_redshift_info(filename=filename)
    return z if z is not None else float('inf')

def create_grid_figures(output_dir='parameter_plots', png_dir=None, mass_function_type='SMF'):
    """Creates grid figures from existing plots"""
    if png_dir is None:
        png_dir = '.'
    elif not os.path.exists(png_dir):
        logger.info(f"PNG directory {png_dir} does not exist!")
        return
    
    from math import ceil, sqrt
    os.makedirs(output_dir, exist_ok=True)
    
    plot_types = {
        'iterations': f'{mass_function_type}_*_all.png',
        'fit_scores': f'{mass_function_type}_*_particles_fit_scores.png',
        'pairplot': f'{mass_function_type}_*_pairplot.png',
        'clean_comparison': f'{mass_function_type}_*_clean_comparison.png'
    }
    
    for plot_type, suffix in plot_types.items():
        matching_files = glob.glob(os.path.join(png_dir, suffix))
        
        # Sort files by redshift
        matching_files.sort(key=extract_redshift)

        if len(matching_files) <= 1:
            logger.info(f"Not enough {mass_function_type} {plot_type} plots found for grid creation")
            continue
                
        if not matching_files:
            logger.info(f"No {mass_function_type} {plot_type} plots found.")
            continue
            
        n_plots = len(matching_files)
        n_cols = min(4, ceil(sqrt(n_plots)))
        n_rows = ceil(n_plots / n_cols)
        
        fig = plt.figure(figsize=(6*n_cols, 5*n_rows))
        
        for idx, fname in enumerate(matching_files):
            img = mpimg.imread(fname)
            ax = fig.add_subplot(n_rows, n_cols, idx + 1)
            ax.imshow(img)
            ax.axis('off')
            
            z = extract_redshift(os.path.basename(fname))
            title = f'z = {z:.1f}' if z != float('inf') else 'Unknown redshift'
            ax.set_title(title)
        
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f'{mass_function_type}_{plot_type}_grid.png'), 
                   dpi=300, bbox_inches='tight')
        plt.close()
        
        logger.info(f"Created {mass_function_type} {plot_type} grid figure")    

def plot_smf_fit_comparison(directory='./', output_dir='parameter_plots'):
    """
    Plot comparison between observations and best fits from CSV files.
    All redshifts are shown on a single plot with different colors.
    """
    
    # Look for all SMF fit results files
    pattern = os.path.join(directory, 'SMF_z*_dump_fit_results.csv')
    files = glob.glob(pattern)
    
    if not files:
        logger.info(f"No SMF fit results files found in {directory}")
        return
        
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Create figure
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111)
    
    # Sort files by redshift using the extract_redshift function
    sorted_files = sorted(files, key=extract_redshift)
    n_files = len(sorted_files)
    colors = plt.cm.plasma(np.linspace(0, 1, n_files))

    # Create dummy lines for legend
    ax.plot([], [], ':', color='k', label='Observations', linewidth=2)
    ax.plot([], [], '-', color='k', label='Model - SAGE', linewidth=2)
    
    # Process each file
    for idx, file in enumerate(sorted_files):
        # Get correct redshift value
        z = extract_redshift(file)
        if z == float('inf'):
            logger.info(f"Skipping file with unknown redshift format: {file}")
            continue
            
        # Read data
        try:
            data = np.loadtxt(file, delimiter='\t')
            x_obs = data[:, 0]
            y_obs = data[:, 1]
            x_fit = data[:, 2]
            y_fit = data[:, 3]
            
            # Check for invalid values
            if not (np.isfinite(x_obs).all() and np.isfinite(y_obs).all() and 
                   np.isfinite(x_fit).all() and np.isfinite(y_fit).all()):
                logger.info("Warning: Non-finite values found in data!")
                continue
                
            color = colors[idx]
            
            # Plot with explicit zorder to ensure visibility
            ax.plot(x_obs, y_obs, ':', color=color, 
                    alpha=0.8, linewidth=2)
            ax.plot(x_fit, y_fit, '-', color=color, 
                    alpha=0.8, linewidth=2)
            
        except Exception as e:
            logger.error(f"Error processing {file}: {e}")
            continue
    
    ax.set_yscale('log')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=12)
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$', fontsize=12)
    ax.set_title('Stellar Mass Function: Observations vs Best Fits', fontsize=14)
    
    # Adjust legend
    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    # Turn off grid again to be sure
    plt.grid(False)
    ax.grid(False)
    
    
    # Set axes limits
    ax.set_ylim(1e-6, 1e-1)
    ax.set_xlim(8.0, 12.2)
    
    # Adjust layout
    plt.tight_layout()
    
    # Save the plot
    output_path = os.path.join(output_dir, 'smf_comparisons_combined.png')
    plt.savefig(output_path, bbox_inches='tight', dpi=300)
    plt.close()
    
    logger.info(f"Combined comparison plot saved to {output_path}")

def plot_bhmf_fit_comparison(directory='./', output_dir='parameter_plots'):
    """
    Plot comparison between observations and best fits from CSV files.
    All redshifts are shown on a single plot with different colors.
    """
    
    # Look for all SMF fit results files
    pattern = os.path.join(directory, 'BHMF_z*_dump_fit_results.csv')
    files = glob.glob(pattern)
    
    if not files:
        logger.info(f"No SMF fit results files found in {directory}")
        return
        
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Create figure
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111)
    
    # Sort files by redshift using the extract_redshift function
    sorted_files = sorted(files, key=extract_redshift)
    n_files = len(sorted_files)
    colors = plt.cm.plasma(np.linspace(0, 1, n_files))

    # Create dummy lines for legend
    ax.plot([], [], ':', color='k', label='Observations', linewidth=2)
    ax.plot([], [], '-', color='k', label='Model - SAGE', linewidth=2)
    
    # Process each file
    for idx, file in enumerate(sorted_files):
        # Get correct redshift value
        z = extract_redshift(file)
        if z == float('inf'):
            logger.info(f"Skipping file with unknown redshift format: {file}")
            continue
            
        # Read data
        try:
            data = np.loadtxt(file, delimiter='\t')
            x_obs = data[:, 0]
            y_obs = data[:, 1]
            x_fit = data[:, 2]
            y_fit = data[:, 3]
            
            # Check for invalid values
            if not (np.isfinite(x_obs).all() and np.isfinite(y_obs).all() and 
                   np.isfinite(x_fit).all() and np.isfinite(y_fit).all()):
                logger.info("Warning: Non-finite values found in data!")
                continue
                
            color = colors[idx]
            
            # Plot with explicit zorder to ensure visibility
            ax.plot(x_obs, y_obs, ':', color=color, 
                    alpha=0.8, linewidth=2)
            ax.plot(x_fit, y_fit, '-', color=color, 
                    alpha=0.8, linewidth=2)
            
        except Exception as e:
            logger.error(f"Error processing {file}: {e}")
            continue
    
    ax.set_yscale('log')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$', fontsize=12)
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$', fontsize=12)
    ax.set_title('Black Hole Mass Function: Observations vs Best Fits', fontsize=14)
    
    # Adjust legend
    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    # Turn off grid again to be sure
    plt.grid(False)
    ax.grid(False)
    
    
    # Set axes limits
    ax.set_ylim(1e-6, 1e-1)
    ax.set_xlim(6.0, 10.2)
    
    # Adjust layout
    plt.tight_layout()
    
    # Save the plot
    output_path = os.path.join(output_dir, 'bhmf_comparisons_combined.png')
    plt.savefig(output_path, bbox_inches='tight', dpi=300)
    plt.close()
    
    logger.info(f"Combined comparison plot saved to {output_path}")

def plot_bhbm_fit_comparison(directory='./', output_dir='parameter_plots'):
    """
    Plot comparison between observations and best fits from CSV files.
    All redshifts are shown on a single plot with different colors.
    """
    
    # Look for all BHBM fit results files
    pattern = os.path.join(directory, 'BHBM_z*_dump_fit_results.csv')
    files = glob.glob(pattern)
    
    if not files:
        logger.info(f"No BHBM fit results files found in {directory}")
        return
        
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Create figure
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111)
    
    # Sort files by redshift using the extract_redshift function
    sorted_files = sorted(files, key=extract_redshift)
    n_files = len(sorted_files)
    colors = plt.cm.plasma(np.linspace(0, 1, n_files))

    # Create dummy lines for legend
    ax.plot([], [], ':', color='k', label='Observations', linewidth=2)
    ax.plot([], [], '-', color='k', label='Model - SAGE', linewidth=2)
    
    # Process each file
    for idx, file in enumerate(sorted_files):
        # Get correct redshift value
        z = extract_redshift(file)
        if z == float('inf'):
            logger.info(f"Skipping file with unknown redshift format: {file}")
            continue
            
        # Read data
        try:
            data = np.loadtxt(file, delimiter='\t')
            x_obs = data[:, 0]
            y_obs = data[:, 1]
            x_fit = data[:, 2]
            y_fit = data[:, 3]
            
            # Check for invalid values
            if not (np.isfinite(x_obs).all() and np.isfinite(y_obs).all() and 
                   np.isfinite(x_fit).all() and np.isfinite(y_fit).all()):
                logger.info("Warning: Non-finite values found in data!")
                continue
                
            color = colors[idx]
            
            # Plot with explicit zorder to ensure visibility
            ax.plot(x_obs, y_obs, ':', color=color, 
                    alpha=0.8, linewidth=2)
            ax.plot(x_fit, y_fit, '-', color=color, 
                    alpha=0.8, linewidth=2)
            
        except Exception as e:
            logger.error(f"Error processing {file}: {e}")
            continue
    
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=12)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$', fontsize=12)
    ax.set_title('Black Hole - Bulge Mass Relation: Observations vs Best Fits', fontsize=14)
    
    # Adjust legend
    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    # Turn off grid again to be sure
    plt.grid(False)
    ax.grid(False)
    
    
    # Set axes limits
    ax.set_ylim(6.0, 10.2)
    ax.set_xlim(8.0, 12.2)
    
    # Adjust layout
    plt.tight_layout()
    
    # Save the plot
    output_path = os.path.join(output_dir, 'bhbm_comparisons_combined.png')
    plt.savefig(output_path, bbox_inches='tight', dpi=300)
    plt.close()
    
    logger.info(f"Combined comparison plot saved to {output_path}")

def plot_hsmr_fit_comparison(directory='./', output_dir='parameter_plots'):
    """
    Plot comparison between observations and best fits from CSV files.
    All redshifts are shown on a single plot with different colors.
    """
    
    # Look for all HSMR fit results files
    pattern = os.path.join(directory, 'HSMR_z*_dump_fit_results.csv')
    files = glob.glob(pattern)
    
    if not files:
        logger.info(f"No HSMR fit results files found in {directory}")
        return
        
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Create figure
    fig = plt.figure(figsize=(12, 8))
    ax = fig.add_subplot(111)
    
    # Sort files by redshift using the extract_redshift function
    sorted_files = sorted(files, key=extract_redshift)
    n_files = len(sorted_files)
    colors = plt.cm.plasma(np.linspace(0, 1, n_files))

    # Create dummy lines for legend
    ax.plot([], [], ':', color='k', label='Observations', linewidth=2)
    ax.plot([], [], '-', color='k', label='Model - SAGE', linewidth=2)
    
    # Process each file
    for idx, file in enumerate(sorted_files):
        # Get correct redshift value
        z = extract_redshift(file)
        if z == float('inf'):
            logger.info(f"Skipping file with unknown redshift format: {file}")
            continue
            
        # Read data
        try:
            data = np.loadtxt(file, delimiter='\t')
            x_obs = data[:, 0]
            y_obs = data[:, 1]
            x_fit = data[:, 2]
            y_fit = data[:, 3]
            
            # Check for invalid values
            if not (np.isfinite(x_obs).all() and np.isfinite(y_obs).all() and 
                   np.isfinite(x_fit).all() and np.isfinite(y_fit).all()):
                logger.info("Warning: Non-finite values found in data!")
                continue
                
            color = colors[idx]
            
            # Plot with explicit zorder to ensure visibility
            ax.plot(x_obs, y_obs, ':', color=color, 
                    alpha=0.8, linewidth=2)
            ax.plot(x_fit, y_fit, '-', color=color, 
                    alpha=0.8, linewidth=2)
            
        except Exception as e:
            logger.error(f"Error processing {file}: {e}")
            continue
    
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$', fontsize=12)
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$', fontsize=12)
    ax.set_title('Halo - Stellar Mass Relation: Observations vs Best Fits', fontsize=14)
    
    # Adjust legend
    leg = ax.legend(loc='lower right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    # Turn off grid again to be sure
    plt.grid(False)
    ax.grid(False)
    
    
    # Set axes limits
    ax.set_ylim(7.0, 13.2)
    ax.set_xlim(11.0, 15.2)
    
    # Adjust layout
    plt.tight_layout()
    
    # Save the plot
    output_path = os.path.join(output_dir, 'hsmr_comparisons_combined.png')
    plt.savefig(output_path, bbox_inches='tight', dpi=300)
    plt.close()
    
    logger.info(f"Combined comparison plot saved to {output_path}")

def plot_smf_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data, output_dir=None):
    """
    This function creates a plot showing:
    - PSO particle evolution colored by fit score
    - Observational data
    - SAGE model predictions
    """
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data
    x_sage, y_sage, sage_label = sage_data
    y_obs_converted = [10**y for y in y_obs]
    y_sage_converted = np.log10([10**y for y in y_sage])
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks
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

    # Load and process fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    all_scores = np.concatenate(np.log10(fit_scores))
    norm = Normalize(vmin=np.nanmin(all_scores), vmax=np.nanmax(all_scores))
    colormap = cm.get_cmap('viridis_r')

    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Plot particles colored by fit score
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            transformed_y = 10**np.array(y)
            color = colormap(norm(np.log10(scores[particle_index])))
            ax.plot(x_values, transformed_y, color=color, alpha=0.3, linewidth=0.75)
            
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

    # Plot reference lines
    ax.plot(x_obs, y_obs_converted, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage_converted, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    if 'SMF_z0' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[0,1])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z05' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[2,3])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z10' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[4,5])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z20' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[6,7])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z31' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[8,9])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z46'in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[10,11])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

        # Save fit results to CSV
    if lowest_score_line is not None and output_dir:
        x_best, y_best = lowest_score_line
        x_obs, y_obs, _ = obs_data
        x_sage, y_sage, _ = sage_data

        y_obs_converted = [10**y for y in y_obs]
        y_sage_converted = [10**y for y in y_sage]
        
        # Save results to CSV if we found a best fit
    if lowest_score_line is not None:
        try:
            base_filename = os.path.basename(data_filename)
            
            if output_dir:
                os.makedirs(output_dir, exist_ok=True)
                results_file = os.path.join(output_dir, base_filename.replace('.txt', '_fit_results.csv'))
            else:
                results_file = data_filename.replace('.txt', '_fit_results.csv')
            
            logger.info(f"Saving fit results to: {results_file}")
            with open(results_file, 'w') as csvfile:
                for x_o, y_o, x_b, y_b in zip(x_obs, y_obs_converted, lowest_score_line[0], lowest_score_line[1]):
                    csvfile.write(f"{x_o}\t{y_o}\t{x_b}\t{y_b}\n")
            logger.info(f"Successfully saved fit results to: {results_file}")
        except Exception as e:
            logger.error(f"Error saving fit results: {e}")

    # Add colorbar
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array(all_scores)
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Log Student-t score')

    # Set axis properties
    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    ax.axis([8.0, 12.2, 1.0e-6, 1.0e-1])

    leg = ax.legend(loc='lower left')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def plot_bhmf_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data, output_dir=None):
    """
    This function creates a plot showing:
    - PSO particle evolution colored by fit score
    - Observational data
    - SAGE model predictions
    """
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data
    x_sage, y_sage, sage_label = sage_data
    y_obs_converted = [10**y for y in y_obs]
    y_sage_converted = np.log10([10**y for y in y_sage])
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks
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

    # Load and process fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    all_scores = np.concatenate(np.log10(fit_scores))
    norm = Normalize(vmin=np.nanmin(all_scores), vmax=np.nanmax(all_scores))
    colormap = cm.get_cmap('viridis_r')

    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Plot particles colored by fit score
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            transformed_y = 10**np.array(y)
            color = colormap(norm(np.log10(scores[particle_index])))
            ax.plot(x_values, transformed_y, color=color, alpha=0.3, linewidth=0.75)
            
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

    # Plot reference lines
    ax.plot(x_obs, y_obs_converted, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage_converted, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')
    if "BHMF_z0" in data_filename:
        # Add SHARK for z=0
            bulgemass, blackholemass = load_observation('SHARK_BHBM_z0.csv', cols=[0,1])
            ax.plot(bulgemass, blackholemass, 'g--', label='SHARK')

    # Save fit results to CSV
    if lowest_score_line is not None and output_dir:
        x_best, y_best = lowest_score_line
        x_obs, y_obs, _ = obs_data
        x_sage, y_sage, _ = sage_data

        y_obs_converted = [10**y for y in y_obs]
        y_sage_converted = [10**y for y in y_sage]
        
        # Save results to CSV if we found a best fit
    if lowest_score_line is not None:
        try:
            base_filename = os.path.basename(data_filename)
            
            if output_dir:
                os.makedirs(output_dir, exist_ok=True)
                results_file = os.path.join(output_dir, base_filename.replace('.txt', '_fit_results.csv'))
            else:
                results_file = data_filename.replace('.txt', '_fit_results.csv')
            
            logger.info(f"Saving fit results to: {results_file}")
            with open(results_file, 'w') as csvfile:
                for x_o, y_o, x_b, y_b in zip(x_obs, y_obs_converted, lowest_score_line[0], lowest_score_line[1]):
                    csvfile.write(f"{x_o}\t{y_o}\t{x_b}\t{y_b}\n")
            logger.info(f"Successfully saved fit results to: {results_file}")
        except Exception as e:
            logger.error(f"Error saving fit results: {e}")

    # Add colorbar
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array(all_scores)
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Log Student-t score')

    # Set axis properties
    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
    ax.axis([6.0, 10.2, 1.0e-6, 1.0e-1])

    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def plot_bhbm_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data, output_dir=None):
    """This function creates a plot showing PSO particle evolution colored by fit score"""
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data if obs_data[0] is not None else ([], [], None)
    x_sage, y_sage, sage_label = sage_data
    
    # Load and process data...
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    # Process blocks and iterations...
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks...
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

    # Load fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    all_scores = np.concatenate(np.log10(fit_scores))
    norm = Normalize(vmin=np.nanmin(all_scores), vmax=np.nanmax(all_scores))
    colormap = cm.get_cmap('viridis_r')

    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Plot particles
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            color = colormap(norm(np.log10(scores[particle_index])))
            ax.plot(x_values, y, color=color, alpha=0.3, linewidth=0.75)
            
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, y)

    ## Plot reference lines
    if len(x_obs) > 0:
        ax.plot(x_obs, y_obs, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')
        
        # Save results if output_dir provided
        if output_dir:
            try:
                base_filename = os.path.basename(data_filename)
                results_file = os.path.join(output_dir, base_filename.replace('.txt', '_fit_results.csv'))
                
                with open(results_file, 'w') as csvfile:
                    for x_o, y_o, x_b, y_b in zip(x_obs, y_obs, lowest_score_line[0], lowest_score_line[1]):
                        csvfile.write(f"{x_o}\t{y_o}\t{x_b}\t{y_b}\n")
            except Exception as e:
                logger.error(f"Error saving fit results: {e}")

    if "BHBM_z0" in data_filename:
        # Add SHARK for z=0
            bulgemass, blackholemass = load_observation('SHARK_BHBM_z0.csv', cols=[0,1])
            ax.plot(bulgemass, blackholemass, 'g--', label='SHARK')

    # Add colorbar and formatting
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array(all_scores)
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Log Student-t score')

    ax.set_ylabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    ax.axis([8.0, 12.0, 6.0, 10.0])

    leg = ax.legend(loc='upper left')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def plot_hsmr_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data, output_dir=None):
    """This function creates a plot showing PSO particle evolution colored by fit score"""
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data if obs_data[0] is not None else ([], [], None)
    x_sage, y_sage, sage_label = sage_data
    
    # Load and process data...
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    # Process blocks and iterations...
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks...
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

    # Load fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    all_scores = np.concatenate(np.log10(fit_scores))
    norm = Normalize(vmin=np.nanmin(all_scores), vmax=np.nanmax(all_scores))
    colormap = cm.get_cmap('viridis_r')

    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Plot particles
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            color = colormap(norm(np.log10(scores[particle_index])))
            ax.plot(x_values, y, color=color, alpha=0.3, linewidth=0.75)
            
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, y)

    ## Plot reference lines
    if len(x_obs) > 0:
        ax.plot(x_obs, y_obs, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    if 'HSMR_z0' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[0,1])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z05' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[2,3])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z10' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[4,5])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z20' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[6,7])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z30' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[8,9])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z40'in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[10,11])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')
        
        # Save results if output_dir provided
        if output_dir:
            try:
                base_filename = os.path.basename(data_filename)
                results_file = os.path.join(output_dir, base_filename.replace('.txt', '_fit_results.csv'))
                
                with open(results_file, 'w') as csvfile:
                    for x_o, y_o, x_b, y_b in zip(x_obs, y_obs, lowest_score_line[0], lowest_score_line[1]):
                        csvfile.write(f"{x_o}\t{y_o}\t{x_b}\t{y_b}\n")
            except Exception as e:
                logger.error(f"Error saving fit results: {e}")

    # Add colorbar and formatting
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array(all_scores)
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Log Student-t score')

    ax.set_ylabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
    ax.axis([11.0, 15.0, 7.0, 13.0])

    leg = ax.legend(loc='lower right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def plot_smf_clean_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data):
    """
    This function creates a plot showing:
    - Observational data
    - SAGE model predictions
    - Best fit only
    """
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data
    x_sage, y_sage, sage_label = sage_data
    y_obs_converted = [10**y for y in y_obs]
    y_sage_converted = np.log10([10**y for y in y_sage])
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks
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

    # Load and process fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    
    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Find best fit (identical to working version)
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            transformed_y = 10**np.array(y)
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

    # Plot reference lines exactly as in working version
    ax.plot(x_obs, y_obs_converted, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage_converted, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    if 'SMF_z0' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[0,1])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z05' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[2,3])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z10' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[4,5])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z20' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[6,7])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z31' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[8,9])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    elif 'SMF_z46'in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_SMF.csv', cols=[10,11])
        ax.plot(mass, 10**phi, c='g', label='SHARK', linestyle = '--')

    # Set axis properties
    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    ax.axis([8.0, 12.2, 1.0e-6, 1.0e-1])

    leg = ax.legend(loc='lower left')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def plot_bhmf_clean_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data):
    """
    This function creates a plot showing:
    - Observational data
    - SAGE model predictions
    - Best fit only
    """
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data
    x_sage, y_sage, sage_label = sage_data
    y_obs_converted = [10**y for y in y_obs]
    y_sage_converted = np.log10([10**y for y in y_sage])
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks
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

    # Load and process fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    
    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Find best fit (identical to working version)
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            transformed_y = 10**np.array(y)
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

    # Plot reference lines exactly as in working version
    ax.plot(x_obs, y_obs_converted, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage_converted, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    # Set axis properties
    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
    ax.axis([6.0, 10.2, 1.0e-6, 1.0e-1])

    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def plot_bhbm_clean_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data):
    """
    This function creates a plot showing:
    - Observational data
    - SAGE model predictions
    - Best fit only
    """
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data if obs_data[0] is not None else ([], [], None)
    x_sage, y_sage, sage_label = sage_data
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks
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

    # Load and process fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    
    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Find best fit
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, y)

    # Plot reference lines
    if len(x_obs) > 0:
        ax.plot(x_obs, y_obs, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    if "BHBM_z0" in data_filename:
        # Add SHARK for z=0
            bulgemass, blackholemass = load_observation('SHARK_BHBM_z0.csv', cols=[0,1])
            ax.plot(bulgemass, blackholemass, 'g--', label='SHARK')

    # Set axis properties
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    ax.axis([8.0, 12.0, 6.0, 10.0])

    # Add legend with all components
    handles = []
    labels = []
    for line in ax.lines:
        if line.get_label() is not None and line.get_label() != '_nolegend_':
            handles.append(line)
            labels.append(line.get_label())
    
    leg = ax.legend(handles, labels, loc='upper left')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def plot_hsmr_clean_comparison(data_filename, track_folder, num_particles, num_iterations, obs_data, sage_data):
    """
    This function creates a plot showing:
    - Observational data
    - SAGE model predictions
    - Best fit only
    """
    # Extract observational and SAGE data
    x_obs, y_obs, obs_label = obs_data if obs_data[0] is not None else ([], [], None)
    x_sage, y_sage, sage_label = sage_data
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Parse data blocks
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

    # Load and process fit scores
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]
    
    # Create plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    # Find best fit
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, y)

    # Plot reference lines
    if len(x_obs) > 0:
        ax.plot(x_obs, y_obs, 'k--', linewidth=2.25, label=obs_label)
    ax.plot(x_sage, y_sage, 'r--', linewidth=2.25, label=sage_label)
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], 'b-', linewidth=2.25, label='PSO Best Fit')

    if 'HSMR_z0' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[0,1])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z05' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[2,3])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z10' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[4,5])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z20' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[6,7])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z30' in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[8,9])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    if 'HSMR_z40'in data_filename:
        # Add SHARK for z=0
        mass, phi = load_observation('SHARK_HSMR.csv', cols=[10,11])
        ax.plot(mass, phi, c='g', label='SHARK', linestyle = '--')

    # Set axis properties
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{halo}}\ (M_{\odot})$')
    ax.axis([11.0, 15.0, 7.0, 13.0])

    # Add legend with all components
    handles = []
    labels = []
    for line in ax.lines:
        if line.get_label() is not None and line.get_label() != '_nolegend_':
            handles.append(line)
            labels.append(line.get_label())
    
    leg = ax.legend(handles, labels, loc='upper left')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

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
        'iterations': '*_all.png',
        'fit_scores': '*_particles_fit_scores.png',
        'pairplot': '*_pairplot.png',
        'clean_comparison': '*_clean_comparison.png'
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

def get_smf_files_map(config_opts):
    """Create mapping of SMF dump files to their corresponding observational data"""
    # Load all observational data
    shuntov_data = load_shuntov_data()
    gama_data = load_gama_data(config_opts)
    sage_data = load_sage_data()
    ilbert_data = load_ilbert_data(config_opts)
    
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
        smf_files[filename] = (ilbert_data, sage_data[1.0])
    else:
        logger.info(f"Not found: {filename}")
    
    # Handle all other redshifts using Shuntov data
    for z in get_all_redshifts():
        if z in [0.0,1.0]:  # Skip z=0 as it's handled above
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

def plot_smf_parameter_correlations(particle_data, best_params, output_dir, space, tracks_dir, n_particles):
    """Calculate and plot correlations between parameters and SMF across all iterations."""
    # Figure settings
    fig_w, fig_h = 12, 8
    label_size = 12 
    tick_size = 16
    skip_iterations = 0
    logger = logging.getLogger('diagnostics')
    
    # Get parameter names from space file
    param_names = []
    param_labels = []
    for name, label in zip(space['name'], space['plot_label']):
        param_names.append(name)
        param_labels.append(label)
    
    logger.info(f"Using parameters from space file: {param_names}")
    
    # Parameter display mappings
    param_display_mapping = {
        'SfrEfficiency': r'$\alpha_{SN}$',
        'FeedbackReheatingEpsilon': r'$\epsilon_{disc}$',
        'FeedbackEjectionEfficiency': r'$\epsilon_{halo}$', 
        'ReIncorporationFactor': r'$\kappa_{reinc}$',
        'RadioModeEfficiency': r'$\kappa_{R}$',
        'QuasarModeEfficiency': r'$\kappa_{Q}$',
        'BlackHoleGrowthRate': r'$f_{BH}$'
    }
    
    # Find SMF dump files and track files
    smf_files = glob.glob(os.path.join(output_dir, 'SMF_z*_dump.txt'))
    if not smf_files:
        logger.info("No SMF dump files found")
        return
    
    track_files = sorted(glob.glob(os.path.join(tracks_dir, 'track_*_pos.npy')))
    if not track_files:
        logger.error("Missing track files")
        return
        
    # Process each SMF file
    for smf_file in smf_files:
        try:
            # Extract redshift from filename
            z = extract_redshift(os.path.basename(smf_file))
            if z is None:
                continue
                
            logger.info(f"\nProcessing SMF file for z={z}")
            
            # First read SMF data to determine iterations
            mass_bins, smf_values = read_smf_dump_file(smf_file, n_particles, skip_iterations=skip_iterations)
            if mass_bins is None or smf_values is None:
                logger.error("Failed to read SMF data")
                continue

            # Calculate number of iterations after skipping
            n_iterations = len(smf_values) // n_particles
            logger.info(f"Found {n_iterations} complete iterations in SMF data")

            # Filter mass bins between 8 and 12
            mass_mask = (mass_bins >= 8.0) & (mass_bins <= 12.0)
            mass_bins = mass_bins[mass_mask]
            smf_values = smf_values[:, mass_mask]
            
            # Get matching parameter values
            param_values = get_aligned_parameter_values(track_files, n_particles, n_iterations, skip_iterations=skip_iterations)
            if param_values is None:
                logger.error("Failed to get parameter values")
                continue

            # Verify alignment
            if len(param_values) != len(smf_values):
                logger.error(f"Parameter values ({len(param_values)}) don't match SMF values ({len(smf_values)})")
                continue
                        
            # Calculate correlations and gradients
            n_params = len(param_names)
            n_bins = len(mass_bins)
            corr_data = np.zeros((n_params, n_bins))
            
            # Labels for plots
            rows = [param_display_mapping.get(param, param) for param in param_names]
            lm = np.round(mass_bins-0.05, 1)
            
            # Calculate correlations for each parameter and mass bin
            for i, param in enumerate(param_names):
                param_vals = param_values[:, i]
                
                for j in range(n_bins):
                    bin_vals = smf_values[:, j]
                    
                    # Remove NaN values
                    mask = ~np.isnan(param_vals) & ~np.isnan(bin_vals)
                    if np.sum(mask) > 1:
                        corr_data[i, j] = stats.spearmanr(param_vals[mask], bin_vals[mask])[0]

            # Convert to DataFrames
            corr_df = pd.DataFrame(corr_data, columns=lm, index=rows)
            
            # Reverse mass bins for plotting
            corr_df = corr_df.iloc[:, ::-1]
            
            # Plot Correlation Heatmap
            grid_spec = {"width_ratios": (.9, .05)}
            fig, (ax, cbar_ax) = plt.subplots(1, 2, figsize=(fig_w, fig_h), gridspec_kw=grid_spec)
            
            sns.heatmap(corr_df,
                       ax=ax, 
                       cbar_ax=cbar_ax,
                       annot=False,
                       fmt='.2f',
                       annot_kws={"size": label_size-2},
                       linewidth=0.5,
                       vmin=-1.0,
                       vmax=1.0,
                       center=0,
                       norm=cols.TwoSlopeNorm(vmin=-1.0, vcenter=0, vmax=1.0),
                       xticklabels=3,
                       cmap='coolwarm',
                       cbar_kws={'label': r"Spearman correlation"})
            
            ax.set_xlabel(r'$\log_{10}(M_{\star}/M_{\odot})$', fontsize=label_size)
            #ax.set_ylabel('Parameter', fontsize=label_size)
            
            # Adjust label parameters
            ax.tick_params(labelsize=tick_size, rotation=0)
            plt.xticks(rotation=45, ha='right')
            
            cbar_ax.tick_params(labelsize=tick_size)
            cbar_ax.yaxis.label.set_size(label_size)
            
            plt.tight_layout()
            
            plt.savefig(os.path.join(output_dir, f"corr_heatmap_SMF_z{z:.1f}.png"), 
                       bbox_inches='tight', dpi=300)
            plt.close()
            
            logger.info(f"Created correlation plot for z={z}")
            
        except Exception as e:
            logger.error(f"Error processing {smf_file}: {str(e)}")
            continue

def create_correlation_heatmap(param_values, smf_values, mass_bins, param_names, output_dir, dump_filename):
    """
    Create a heatmap showing correlations between parameters and SMF at different mass bins.
    Mass range restricted to 8-12 in log solar masses.
    """
    logger = logging.getLogger('diagnostics')
    
    # Parameter display mappings for LaTeX formatting
    param_display_mapping = {
        'SfrEfficiency': r'$\alpha_{SN}$',
        'FeedbackReheatingEpsilon': r'$\epsilon_{disc}$',
        'FeedbackEjectionEfficiency': r'$\epsilon_{halo}$',
        'ReIncorporationFactor': r'$\kappa_{reinc}$',
        'RadioModeEfficiency': r'$\kappa_{R}$',
        'QuasarModeEfficiency': r'$\kappa_{Q}$',
        'BlackHoleGrowthRate': r'$f_{BH}$'
    }
    
    try:
        # Extract constraint type and redshift from dump filename
        basename = os.path.basename(dump_filename)
        constraint_type = basename.split('_')[0]  # Gets 'SMF' or 'BHMF' etc
        z_str = basename.split('_')[1][1:]  # Gets '0' or '20' etc after removing 'z'
        
        # Filter mass bins to 8-12 range and ensure consistent ordering
        mass_mask = (mass_bins >= 8.0) & (mass_bins <= 12.0)
        mass_bins = mass_bins[mass_mask]
        smf_values = smf_values[:, mass_mask]
        
        # Sort mass bins and SMF values in ascending order
        sort_idx = np.argsort(mass_bins)
        mass_bins = mass_bins[sort_idx]
        smf_values = smf_values[:, sort_idx]
        
        # Calculate correlations for each parameter and mass bin
        n_params = len(param_names)
        n_bins = len(mass_bins)
        corr_matrix = np.zeros((n_params, n_bins))
        p_values = np.zeros((n_params, n_bins))
        
        # Convert parameter names to display names
        display_names = [param_display_mapping.get(param, param) for param in param_names]
        
        # Calculate correlations
        for i, param in enumerate(param_names):
            for j in range(n_bins):
                corr, p_val = stats.spearmanr(param_values[:, i], smf_values[:, j])
                corr_matrix[i, j] = corr
                p_values[i, j] = p_val
        
        # Create DataFrame with proper labels
        corr_df = pd.DataFrame(corr_matrix, 
                             index=display_names,
                             columns=[f'{m:.1f}' for m in mass_bins])
        
        # 1. Heatmap figure
        fig_heat, ax_heat = plt.subplots(figsize=(12, 8))
        
        # Create heatmap
        sns.heatmap(corr_df,
                   ax=ax_heat,
                   cmap='RdBu_r',
                   vmin=-1, vmax=1,
                   center=0,
                   cbar_kws={'label': 'Spearman Correlation'},
                   xticklabels=True,
                   yticklabels=True)
        
        # Customize axes
        ax_heat.set_xticklabels(ax_heat.get_xticklabels(), rotation=45, ha='right')
        ax_heat.set_xlabel(r'$\log_{10}(M_*/M_\odot)$')
        
        plt.tight_layout()
        heatmap_path = os.path.join(output_dir, f'correlation_heatmap_{constraint_type}_z{z_str}.png')
        plt.savefig(heatmap_path, dpi=300, bbox_inches='tight')
        logger.info(f"Saved correlation heatmap to {heatmap_path}")
        plt.close(fig_heat)
        
        # 2. Evolution figure
        fig_evo, ax_evo = plt.subplots(figsize=(8, 6))
        
        threshold = 0.2  # Threshold for strong correlation
        
        for i, (param, display_name) in enumerate(zip(param_names, display_names)):
            correlations = corr_matrix[i, :]
            max_corr = np.max(np.abs(correlations))
            
            if max_corr >= threshold:
                ax_evo.plot(mass_bins, correlations, label=display_name,
                          color=plt.cm.tab10(i/n_params), linewidth=2)
            else:
                ax_evo.plot(mass_bins, correlations, label=display_name,
                          color=plt.cm.tab10(i/n_params), linewidth=1.5,
                          linestyle='--', alpha=0.7)
        
        ax_evo.axhline(y=0, color='black', linestyle='--', alpha=0.3)
        ax_evo.set_xlabel(r'$\log_{10}(M_*/M_\odot)$')
        ax_evo.set_ylabel('Spearman correlation')
        ax_evo.set_ylim(-1, 1)
        ax_evo.set_title('Correlation Evolution with Stellar Mass')
        ax_evo.legend(bbox_to_anchor=(1.05, 1), loc='upper left',
                     borderaxespad=0., fontsize=10)
        
        fig_evo.text(0.99, 0.02, '* p < 0.05', ha='right', fontsize=8, style='italic')
        
        plt.tight_layout()
        evolution_path = os.path.join(output_dir, f'correlation_evolution_{constraint_type}_z{z_str}.png')
        plt.savefig(evolution_path, dpi=300, bbox_inches='tight')
        logger.info(f"Saved correlation evolution to {evolution_path}")
        plt.close(fig_evo)
        
        return corr_matrix, p_values
        
    except Exception as e:
        logger.error(f"Error creating correlation analysis: {str(e)}")
        import traceback
        logger.error(traceback.format_exc())
        return None, None
    
def analyze_parameter_sensitivity(param_values, smf_values, mass_bins, param_names, output_dir, dump_filename):
    """
    Analyze how parameter changes affect the SMF
    
    Parameters:
    -----------
    param_values : array-like
        Parameter values from PSO, shape (n_samples, n_parameters)
    smf_values : array-like 
        SMF values for each parameter set, shape (n_samples, n_mass_bins)
    mass_bins : array-like
        Mass bin centers
    param_names : list
        Names of parameters
    output_dir : str
        Directory to save output plots
    """
    
    logger = logging.getLogger('diagnostics')
    logger.info("Starting parameter sensitivity analysis...")
    
    # Create correlation heatmap
    corr_matrix, p_values = create_correlation_heatmap(param_values, smf_values, 
                                                     mass_bins, param_names, 
                                                     output_dir, 
                                                     dump_filename)
    

    # Verify dimensions match
    n_samples, n_params = param_values.shape
    logger.info(f"Parameter values shape: {param_values.shape}")
    logger.info(f"SMF values shape: {smf_values.shape}")
    logger.info(f"Number of parameters: {len(param_names)}")
    
    # Ensure param_names matches actual parameters
    if len(param_names) != n_params:
        logger.warning(f"Parameter names count ({len(param_names)}) doesn't match parameter values ({n_params})")
        param_names = param_names[:n_params]  # Truncate to match actual parameters
        
    # Calculate parameter ranges
    param_ranges = {}
    for i, param in enumerate(param_names):
        try:
            param_vals = param_values[:, i]
            param_ranges[param] = {
                'min': np.percentile(param_vals, 5),
                'max': np.percentile(param_vals, 95),
                'median': np.median(param_vals)
            }
            logger.info(f"Parameter {param} range: {param_ranges[param]}")
        except Exception as e:
            logger.error(f"Error processing parameter {param}: {str(e)}")
            continue
            
    # Create figure for all parameters
    fig, axes = plt.subplots(n_params, 1, figsize=(12, 5*n_params))
    if n_params == 1:
        axes = [axes]
        
    # For each parameter, calculate SMF response to parameter variations
    for i, param in enumerate(param_names):
        try:
            param_vals = param_values[:, i]
            
            # Split into low/high parameter groups
            median = param_ranges[param]['median']
            low_mask = param_vals < median
            high_mask = param_vals > median
            
            # Calculate mean SMF for each group
            low_smf = np.mean(smf_values[low_mask], axis=0)
            high_smf = np.mean(smf_values[high_mask], axis=0)
            
            # Calculate SMF difference and relative change
            smf_diff = high_smf - low_smf
            smf_rel_change = (high_smf - low_smf) / np.abs(low_smf) * 100
            
            # Plot on corresponding subplot
            ax = axes[i]
            
            # Plot absolute difference
            l1 = ax.plot(mass_bins, smf_diff, 'b-', label='Absolute Difference')
            ax.set_xlabel(r'$\log_{10}(M_*/M_\odot)$')
            ax.set_ylabel('ΔSMF')
            
            # Plot relative change on secondary axis
            ax2 = ax.twinx()
            l2 = ax2.plot(mass_bins, smf_rel_change, 'r--', label='Relative Change (%)')
            ax2.set_ylabel('Relative Change (%)')
            
            # Add zero line
            ax.axhline(y=0, color='k', linestyle=':', alpha=0.5)
            
            # Add title and legend
            ax.set_title(f'SMF Response to {param} Variations')
            lines = l1 + l2
            labels = [l.get_label() for l in lines]
            ax.legend(lines, labels, loc='upper right')
            
            # Calculate and add correlation text
            corr = stats.spearmanr(param_vals, np.mean(smf_values, axis=1))[0]
            ax.text(0.02, 0.98, f'Overall correlation: {corr:.3f}', 
                   transform=ax.transAxes, va='top', ha='left')
                   
            logger.info(f"Completed analysis for parameter: {param}")
            
        except Exception as e:
            logger.error(f"Error analyzing parameter {param}: {str(e)}")
            continue
            
    plt.tight_layout()

    # Add correlation summary
    if corr_matrix is not None:
        logger.info("\nParameter-SMF Correlation Summary:")
        for i, param in enumerate(param_names):
            # Calculate mean absolute correlation
            mean_corr = np.mean(np.abs(corr_matrix[i,:]))
            # Find strongest correlation and its mass bin
            strongest_idx = np.argmax(np.abs(corr_matrix[i,:]))
            strongest_corr = corr_matrix[i,strongest_idx]
            strongest_mass = mass_bins[strongest_idx]
            
            logger.info(f"\n{param}:")
            logger.info(f"  Mean absolute correlation: {mean_corr:.3f}")
            logger.info(f"  Strongest correlation: {strongest_corr:.3f} at mass {strongest_mass:.2f}")
    
    # Save plot
    base_filename = os.path.basename(dump_filename)
    constraint_type = base_filename.split('_')[0]  # Gets 'SMF' or 'BHMF' etc
    z_str = base_filename.split('_')[1][1:]  # Gets '0' or '20' etc after removing 'z'

    output_path = os.path.join(output_dir, f'parameter_sensitivity_{constraint_type}_z{z_str}.png')
    try:
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        logger.info(f"Saved parameter sensitivity plot to {output_path}")
    except Exception as e:
        logger.error(f"Error saving plot: {str(e)}")
    
    plt.close()

    return param_ranges

def create_mass_regime_plots(param_values, smf_values, mass_bins, param_names, param_display_mapping, output_dir):
    """
    Create visualizations for mass regime analysis
    """
    logger = logging.getLogger('diagnostics')
    
    # Define mass regimes
    low_mass_mask = (mass_bins >= 8.0) & (mass_bins < 9.5)
    mid_mass_mask = (mass_bins >= 9.5) & (mass_bins < 10.5)
    high_mass_mask = (mass_bins >= 10.5) & (mass_bins <= 12.0)

    # 1. Parameter Impact by Mass Regime Plot
    fig, ax = plt.subplots(figsize=(12, 6))
    regimes = ['Low Mass\n(8.0-9.5)', 'Mid Mass\n(9.5-10.5)', 'High Mass\n(10.5-12.0)']
    impact_data = []
    
    for mask in [low_mass_mask, mid_mass_mask, high_mass_mask]:
        regime_impacts = []
        for i, param in enumerate(param_names):
            smf_regime = smf_values[:, mask]
            param_median = np.median(param_values[:, i])
            low_param_mask = param_values[:, i] < param_median
            high_param_mask = param_values[:, i] > param_median
            
            delta = np.mean(np.mean(smf_regime[high_param_mask]) - np.mean(smf_regime[low_param_mask]))
            regime_impacts.append(abs(delta))
        impact_data.append(regime_impacts)
    
    impact_data = np.array(impact_data)
    
    # Create grouped bar plot
    x = np.arange(len(param_names))
    width = 0.25
    
    ax.bar(x - width, impact_data[0], width, label='Low Mass', alpha=0.7)
    ax.bar(x, impact_data[1], width, label='Mid Mass', alpha=0.7)
    ax.bar(x + width, impact_data[2], width, label='High Mass', alpha=0.7)
    
    ax.set_ylabel('Absolute Parameter Impact on SMF')
    ax.set_title('Parameter Impact by Mass Regime')
    ax.set_xticks(x)
    display_names = [param_display_mapping.get(param, param) for param in param_names]
    ax.set_xticklabels(display_names, rotation=45)
    ax.legend()
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_impact_by_regime.png'), dpi=300)
    plt.close()

    # 2. SMF Response Curves
    fig, axes = plt.subplots(len(param_names), 1, figsize=(12, 4*len(param_names)))
    if len(param_names) == 1:
        axes = [axes]
    
    for i, (param, ax) in enumerate(zip(param_names, axes)):
        param_vals = param_values[:, i]
        percentiles = np.percentile(param_vals, [10, 30, 50, 70, 90])
        colors = plt.cm.viridis(np.linspace(0, 1, len(percentiles)))
        
        for p, c in zip(percentiles, colors):
            # Find particles close to this percentile
            mask = (param_vals >= p*0.95) & (param_vals <= p*1.05)
            mean_smf = np.mean(smf_values[mask], axis=0)
            ax.plot(mass_bins, 10**(mean_smf), color=c, 
                   label=f'{int(stats.percentileofscore(param_vals, p))}th percentile')
        
        ax.set_xlabel(r'$\log_{10}(M_*/M_\odot)$')
        ax.set_ylabel('SMF')
        ax.set_title(f'SMF Response to {param_display_mapping.get(param, param)}')
        ax.legend()
        ax.grid(True, alpha=0.3)
        ax.set_yscale('log')
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'smf_response_curves.png'), dpi=300)
    plt.close()

    # 3. Parameter Sensitivity Heatmap with Mass Regimes
    fig, ax = plt.subplots(figsize=(12, 6))
    sensitivity_data = np.zeros((len(param_names), 3))
    
    for i, mask in enumerate([low_mass_mask, mid_mass_mask, high_mass_mask]):
        for j, param in enumerate(param_names):
            smf_regime = smf_values[:, mask]
            param_vals = param_values[:, j]
            sensitivity_data[j, i] = np.mean([stats.spearmanr(param_vals, smf_regime[:, k])[0] 
                                            for k in range(smf_regime.shape[1])])
    
    im = ax.imshow(sensitivity_data, aspect='auto', cmap='RdBu_r', vmin=-1, vmax=1)
    plt.colorbar(im, label='Mean Spearman Correlation')
    
    ax.set_xticks(range(3))
    ax.set_xticklabels(regimes)
    ax.set_yticks(range(len(param_names)))
    ax.set_yticklabels([param_display_mapping.get(param, param) for param in param_names])
    
    for i in range(len(param_names)):
        for j in range(3):
            text = f'{sensitivity_data[i,j]:.2f}'
            color = 'white' if abs(sensitivity_data[i,j]) > 0.5 else 'black'
            ax.text(j, i, text, ha='center', va='center', color=color)
    
    ax.set_title('Parameter Sensitivity by Mass Regime')
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'mass_regime_sensitivity.png'), dpi=300)
    plt.close()

    logger.info("Created mass regime analysis plots")

def calculate_parameter_importance(param_values, smf_values, param_names):
    """Calculate overall importance metrics for each parameter"""
    importance = {}
    
    for i, param in enumerate(param_names):
        # Calculate correlation with total SMF amplitude
        total_smf = np.sum(smf_values, axis=1)
        corr = stats.spearmanr(param_values[:,i], total_smf)[0]
        
        # Calculate variance explained
        reg = LinearRegression()
        reg.fit(param_values[:,i].reshape(-1,1), total_smf)
        r2 = reg.score(param_values[:,i].reshape(-1,1), total_smf)
        
        importance[param] = {
            'correlation': corr,
            'variance_explained': r2
        }
        
    return importance

def analyze_mass_regime_sensitivity(param_values, smf_values, mass_bins, param_names, output_dir):
    """
    Analyze and log sensitivity metrics specifically focused on different mass regimes
    """
    logger = logging.getLogger('diagnostics')
    logger.info("\n=== Mass Regime Sensitivity Analysis ===")

    param_display_mapping = {
        'SfrEfficiency': r'$\alpha_{SN}$',
        'FeedbackReheatingEpsilon': r'$\epsilon_{disc}$',
        'FeedbackEjectionEfficiency': r'$\epsilon_{halo}$',
        'ReIncorporationFactor': r'$\kappa_{reinc}$',
        'RadioModeEfficiency': r'$\kappa_{R}$',
        'QuasarModeEfficiency': r'$\kappa_{Q}$',
        'BlackHoleGrowthRate': r'$f_{BH}$'
    }

    create_mass_regime_plots(param_values, smf_values, mass_bins, param_names, 
                           param_display_mapping, output_dir)
    
    # Define mass regimes
    low_mass_mask = (mass_bins >= 8.0) & (mass_bins < 9.5)
    mid_mass_mask = (mass_bins >= 9.5) & (mass_bins < 10.5)
    high_mass_mask = (mass_bins >= 10.5) & (mass_bins <= 12.0)
    
    # Log basic statistics
    logger.info(f"\nNumber of mass bins in each regime:")
    logger.info(f"Low mass (8.0-9.5): {np.sum(low_mass_mask)} bins")
    logger.info(f"Mid mass (9.5-10.5): {np.sum(mid_mass_mask)} bins")
    logger.info(f"High mass (10.5-12.0): {np.sum(high_mass_mask)} bins")
    
    # Calculate SMF statistics for each regime
    logger.info("\nSMF Statistics by Mass Regime:")
    for mask, name, regime in [
        (low_mass_mask, "Low Mass", low_mass_mask),
        (mid_mass_mask, "Mid Mass", mid_mass_mask),
        (high_mass_mask, "High Mass", high_mass_mask)
    ]:
        
        smf_regime = smf_values[:, regime]
        logger.info(f"\n{name} Regime:")
        logger.info(f"Mean SMF value: {np.mean(smf_regime):.3e}")
        logger.info(f"SMF std dev: {np.std(smf_regime):.3e}")
        logger.info(f"SMF variation coefficient: {np.std(smf_regime)/np.mean(smf_regime):.3f}")
        
        # Log parameter impacts
        logger.info(f"\nParameter sensitivities in {name} regime:")
        for i, param in enumerate(param_names):
            correlations = []
            for j in range(smf_regime.shape[1]):
                corr, p_val = stats.spearmanr(param_values[:, i], smf_regime[:, j])
                correlations.append(corr)
            
            mean_corr = np.mean(correlations)
            max_corr = np.max(np.abs(correlations))
            
            logger.info(f"{param}:")
            logger.info(f"  Mean correlation: {mean_corr:.3f}")
            logger.info(f"  Max correlation: {max_corr:.3f}")
            
            # Calculate and log the actual range of SMF values this parameter produces
            param_median = np.median(param_values[:, i])
            low_param_mask = param_values[:, i] < param_median
            high_param_mask = param_values[:, i] > param_median
            
            smf_low_param = np.mean(smf_regime[low_param_mask], axis=0)
            smf_high_param = np.mean(smf_regime[high_param_mask], axis=0)
            
            delta_smf = np.mean(smf_high_param - smf_low_param)
            logger.info(f"  Mean SMF change: {delta_smf:.3e}")

    # Log extreme values
    logger.info("\nExtreme Value Analysis:")
    for i, param in enumerate(param_names):
        # Get parameter extremes
        param_vals = param_values[:, i]
        p10 = np.percentile(param_vals, 10)
        p90 = np.percentile(param_vals, 90)
        
        # Look at SMF values for extreme parameter values
        low_mask = param_vals <= p10
        high_mask = param_vals >= p90
        
        logger.info(f"\n{param}:")
        logger.info(f"10th percentile: {p10:.3e}")
        logger.info(f"90th percentile: {p90:.3e}")
        
        for regime, mask, name in [
            (low_mass_mask, low_mass_mask, "Low Mass"),
            (mid_mass_mask, mid_mass_mask, "Mid Mass"),
            (high_mass_mask, high_mass_mask, "High Mass")
        ]:
            smf_low = np.mean(smf_values[low_mask][:, regime])
            smf_high = np.mean(smf_values[high_mask][:, regime])
            delta = smf_high - smf_low
            
            logger.info(f"{name} regime:")
            logger.info(f"  SMF at low param: {smf_low:.3e}")
            logger.info(f"  SMF at high param: {smf_high:.3e}")
            logger.info(f"  Delta SMF: {delta:.3e}")
            logger.info(f"  Relative change: {(delta/smf_low)*100:.1f}%")

    return

def processing(tracks_dir, space_file, output_dir, config_opts, space=None):
    logger.info("Starting diagnostics analysis...")
    
    # Load particle data
    space_obj = space if space is not None else analysis.load_space(space_file)

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

            try:
                logger.info("Starting SMF parameter sensitivity analysis...")
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
                
            except Exception as e:
                logger.error(f"Error creating analysis plots: {str(e)}")
            """
            # Create pairplot
            try:
                logger.info("Creating pairplot...")
                grid = plot_pairplot_with_contours(space, pos, fx)
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_pairplot.png')
                grid.savefig(outfile, dpi=300)
                logger.info(f"Saved pairplot to {outfile}")
            except Exception as e:
                logger.error(f"Error creating pairplot: {str(e)}")
            """
            # Create fit score plot
            try:
                logger.info("Creating fit score plot with SAGE comparison...")
                fig = plot_smf_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    output_dir=output_dir
                )
                fig2 = plot_smf_clean_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data
                )
                if fig is not None and fig2 is not None:  # Check both figures separately
                    outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_particles_fit_scores.png')
                    outfile2 = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_clean_comparison.png')
                    fig.savefig(outfile, dpi=300)
                    fig2.savefig(outfile2, dpi=300)
                    logger.info(f"Saved fit score plot to {outfile}")
                    logger.info(f"Saved clean comparison plot to {outfile2}")
                    plt.close(fig)
                    plt.close(fig2)  # Make sure to close both figures
                else:
                    logger.info(f"No figure was created for {filename}")
            except Exception as e:
                logger.error(f"Error creating plots: {str(e)}")
                logger.info(f"Full traceback:")
                import traceback
                traceback.logger.info_exc()

    if not processed_any_smf:
        logger.info("Warning: No SMF files were found to process!")
        logger.info(f"Expected files in: {output_dir}")
        logger.info("Expected files: %s", list(smf_files.keys()))


    bhmf_files = get_bhmf_files_map(config_opts)
    logger.info(f"Found {len(bhmf_files)} BHMF files to process")

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
            """
            # Create pairplot
            try:
                logger.info("Creating pairplot...")
                grid = plot_pairplot_with_contours(space, pos, fx)
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_pairplot.png')
                grid.savefig(outfile, dpi=300)
                logger.info(f"Saved pairplot to {outfile}")
            except Exception as e:
                logger.error(f"Error creating pairplot: {str(e)}")
            """
            # Create fit score plot
            try:
                logger.info("Creating fit score plot with SAGE comparison...")
                fig = plot_bhmf_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    output_dir=output_dir
                )
                fig2 = plot_bhmf_clean_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data
                )
                if fig is not None and fig2 is not None:  # Check both figures separately
                    outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_particles_fit_scores.png')
                    outfile2 = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_clean_comparison.png')
                    fig.savefig(outfile, dpi=300)
                    fig2.savefig(outfile2, dpi=300)
                    logger.info(f"Saved fit score plot to {outfile}")
                    logger.info(f"Saved clean comparison plot to {outfile2}")
                    plt.close(fig)
                    plt.close(fig2)  # Make sure to close both figures
                else:
                    logger.info(f"No figure was created for {filename}")
            except Exception as e:
                logger.error(f"Error creating plots: {str(e)}")
                logger.info(f"Full traceback:")
                import traceback
                traceback.logger.info_exc()
 
    if not processed_any_bhmf:
        logger.info("Warning: No BHMF files were found to process!")
        logger.info(f"Expected files in: {output_dir}")
        logger.info("Expected files: %s", list(bhmf_files.keys()))


    bhbm_files = get_bhbm_files_map(config_opts)
    logger.info(f"Found {len(bhbm_files)} BHBM files to process")    

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
            """
            # Create pairplot
            try:
                logger.info("Creating pairplot...")
                grid = plot_pairplot_with_contours(space, pos, fx)
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_pairplot.png')
                grid.savefig(outfile, dpi=300)
                logger.info(f"Saved pairplot to {outfile}")
            except Exception as e:
                logger.error(f"Error creating pairplot: {str(e)}")
            """
            # Create fit score plot and clean comparison plot
            try:
                logger.info("Creating fit score plot with SAGE comparison...")
                fig = plot_bhbm_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    output_dir=output_dir
                )
                fig2 = plot_bhbm_clean_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data
                )
                if fig is not None and fig2 is not None:  # Check both figures separately
                    outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_particles_fit_scores.png')
                    outfile2 = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_clean_comparison.png')
                    fig.savefig(outfile, dpi=300)
                    fig2.savefig(outfile2, dpi=300)
                    logger.info(f"Saved fit score plot to {outfile}")
                    logger.info(f"Saved clean comparison plot to {outfile2}")
                    plt.close(fig)
                    plt.close(fig2)  # Make sure to close both figures
                else:
                    logger.info(f"No figure was created for {filename}")
            except Exception as e:
                logger.error(f"Error creating plots: {str(e)}")
                logger.info(f"Full traceback:")
                import traceback
                traceback.logger.info_exc()

    if not processed_any_bhbm:
        logger.info("Warning: No BHBM files were found to process!")
        logger.info(f"Expected files in: {output_dir}")
        logger.info("Expected files: %s", list(bhbm_files.keys()))

    hsmr_files = get_hsmr_files_map(config_opts)
    logger.info(f"Found {len(hsmr_files)} HSMR files to process")    

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
            """
        # Create pairplot
            try:
                logger.info("Creating pairplot...")
                grid = plot_pairplot_with_contours(space, pos, fx)
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_pairplot.png')
                grid.savefig(outfile, dpi=300)
                logger.info(f"Saved pairplot to {outfile}")
            except Exception as e:
                logger.error(f"Error creating pairplot: {str(e)}")
            """
            # Create fit score plot
            try:
                logger.info("Creating fit score plot with SAGE comparison...")
                fig = plot_hsmr_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data,
                    output_dir=output_dir
                )
                fig2 = plot_hsmr_clean_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations,
                    obs_data,
                    sage_data
                )
                if fig is not None and fig2 is not None:  # Check both figures separately
                    outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_particles_fit_scores.png')
                    outfile2 = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_clean_comparison.png')
                    fig.savefig(outfile, dpi=300)
                    fig2.savefig(outfile2, dpi=300)
                    logger.info(f"Saved fit score plot to {outfile}")
                    logger.info(f"Saved clean comparison plot to {outfile2}")
                    plt.close(fig)
                    plt.close(fig2)  # Make sure to close both figures
                else:
                    logger.info(f"No figure was created for {filename}")
            except Exception as e:
                logger.error(f"Error creating plots: {str(e)}")
                logger.info(f"Full traceback:")
                import traceback
                traceback.logger.info_exc()

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

        #logger.info("Creating parameter evolution plots...")
        #plot_parameter_evolution(particle_data, best_params, best_scores, param_names, output_dir, space)

        #logger.info("Creating SMF parameter correlation plots...")
        #plot_smf_parameter_correlations(particle_data, best_params, output_dir, space, tracks_dir=tracks_dir)

        if processed_any_smf:

            try:
                logger.info("Starting SMF parameter sensitivity analysis...")
                track_files = sorted(glob.glob(os.path.join(tracks_dir, 'track_*_pos.npy')))
                if not track_files:
                    logger.error("Missing track files")
                    return
                
                # Get list of parameters actually in space file
                space_params = [name for name in space['name']]
                logger.info(f"Parameters from space file: {space_params}")
                
                # Read SMF data
                smf_file = os.path.join(output_dir, 'SMF_z0_dump.txt')
                if os.path.exists(smf_file):
                    mass_bins, smf_values = read_smf_dump_file(smf_file, num_particles, skip_iterations=0)
                    
                    if mass_bins is not None and smf_values is not None:
                        # Get parameter values
                        param_values = get_aligned_parameter_values(track_files, num_particles, 
                                                                len(smf_values) // num_particles, 
                                                                skip_iterations=0)
                        
                        if param_values is not None:
                            # Run sensitivity analysis with both visualizations
                            space_params = [name for name in space['name']]
                            param_ranges = analyze_parameter_sensitivity(param_values, smf_values, 
                                                    mass_bins, space_params, 
                                                    output_dir, smf_file)
                            
                            logger.info("Completed parameter sensitivity analysis")
                        else:
                            logger.error("Failed to get parameter values")
                    else:
                        logger.error("Failed to read SMF data")
                else:
                    logger.error(f"SMF dump file not found: {smf_file}")

                # Read SMF data
                smf_file = os.path.join(output_dir, 'SMF_z10_dump.txt')
                if os.path.exists(smf_file):
                    mass_bins, smf_values = read_smf_dump_file(smf_file, num_particles, skip_iterations=0)
                    
                    if mass_bins is not None and smf_values is not None:
                        # Get parameter values
                        param_values = get_aligned_parameter_values(track_files, num_particles, 
                                                                len(smf_values) // num_particles, 
                                                                skip_iterations=0)
                        
                        if param_values is not None:
                            # Run sensitivity analysis with both visualizations
                            space_params = [name for name in space['name']]
                            param_ranges = analyze_parameter_sensitivity(param_values, smf_values, 
                                                                    mass_bins, space_params, 
                                                                    output_dir, smf_file)
                            
                            logger.info("Completed parameter sensitivity analysis")
                        else:
                            logger.error("Failed to get parameter values")
                    else:
                        logger.error("Failed to read SMF data")
                else:
                    logger.error(f"SMF dump file not found: {smf_file}")

                # Read SMF data
                smf_file = os.path.join(output_dir, 'SMF_z20_dump.txt')
                if os.path.exists(smf_file):
                    mass_bins, smf_values = read_smf_dump_file(smf_file, num_particles, skip_iterations=0)
                    
                    if mass_bins is not None and smf_values is not None:
                        # Get parameter values
                        param_values = get_aligned_parameter_values(track_files, num_particles, 
                                                                len(smf_values) // num_particles, 
                                                                skip_iterations=0)
                        
                        if param_values is not None:
                            # Run sensitivity analysis with both visualizations
                            space_params = [name for name in space['name']]
                            param_ranges = analyze_parameter_sensitivity(param_values, smf_values, 
                                                                    mass_bins, space_params, 
                                                                    output_dir, smf_file)
                            
                            logger.info("Completed parameter sensitivity analysis")
                        else:
                            logger.error("Failed to get parameter values")
                    else:
                        logger.error("Failed to read SMF data")
                else:
                    logger.error(f"SMF dump file not found: {smf_file}")
                    
            except Exception as e:
                logger.error(f"Error in parameter sensitivity analysis: {str(e)}")
                import traceback
                traceback.print_exc()

            logger.info("Creating SMF grid figures...")
            create_grid_figures(output_dir=output_dir, png_dir=output_dir, mass_function_type='SMF')
            #logger.info("Creating SMF parameter correlation plots...")
            #plot_smf_parameter_correlations(particle_data, best_params, output_dir, space, tracks_dir=tracks_dir, n_particles=num_particles)
            logger.info("Creating SMF combined fit comparison plot...")
            try:
                plot_smf_fit_comparison(directory=output_dir, output_dir=output_dir)
                logger.info("Successfully created combined SMF fit comparison plot")
            except Exception as e:
                logger.error(f"Error creating combined SMF fit comparison plot: {str(e)}")
                traceback.logger.info_exc()

        if processed_any_bhmf:
            logger.info("Creating BHMF grid figures...")
            create_grid_figures(output_dir=output_dir, png_dir=output_dir, mass_function_type='BHMF')
            logger.info("Creating BHMF combined fit comparison plot...")
            try:
                plot_bhmf_fit_comparison(directory=output_dir, output_dir=output_dir)
                logger.info("Successfully created combined BHMF fit comparison plot")
            except Exception as e:
                logger.error(f"Error creating combined BHMF fit comparison plot: {str(e)}")
                traceback.logger.info_exc()

        if processed_any_bhbm:
            logger.info("Creating BHBM grid figures...")
            create_grid_figures(output_dir=output_dir, png_dir=output_dir, mass_function_type='BHBM')
            logger.info("Creating BHBM combined fit comparison plot...")
            try:
                plot_bhbm_fit_comparison(directory=output_dir, output_dir=output_dir)
                logger.info("Successfully created combined BHBM fit comparison plot")
            except Exception as e:
                logger.error(f"Error creating combined BHBM fit comparison plot: {str(e)}")
                traceback.logger.info_exc()

        if processed_any_hsmr:
            logger.info("Creating HSMR grid figures...")
            create_grid_figures(output_dir=output_dir, png_dir=output_dir, mass_function_type='HSMR')
            logger.info("Creating HSMR combined fit comparison plot...")
            try:
                plot_hsmr_fit_comparison(directory=output_dir, output_dir=output_dir)
                logger.info("Successfully created combined HSMR fit comparison plot")
            except Exception as e:
                logger.error(f"Error creating combined HSMR fit comparison plot: {str(e)}")
                traceback.logger.info_exc()

        # After creating all individual plots
        logger.info("Creating combined constraint grid plots...")
        try:
            create_combined_constraint_grids(output_dir=output_dir, png_dir=output_dir)
            logger.info("Successfully created combined constraint grid plots")
        except Exception as e:
            logger.error(f"Error creating combined constraint grid plots: {str(e)}")
            traceback.logger.info_exc()
            
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