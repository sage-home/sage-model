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

pos_re = re.compile('track_[0-9]+_pos.npy')
fx_re = re.compile('track_[0-9]+_fx.npy')
#h0 = 0.677400
#Omega0 = 0.3089
#vol_frac = 0.002
#boxsize = 400.0
#vol = (boxsize/h0)**3 * vol_frac

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
        print("Different number of pos/fx files, using common files only")
        l = min(len(pos_fnames), len(fx_fnames))
        del pos_fnames[l:]
        del fx_fnames[l:]

    print("Loading %d pos/fx files" % len(pos_fnames))

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
    print(pos.shape, fx.shape)
    #print(fx)
    #print(pos)

    space = analysis.load_space(space_file)
    if space.shape[0] != pos.shape[1]:
        raise ValueError("Particles have different dimensionality than space")
        
    #print('Ordered fits:\n -logLikelihood,', space['name'])
    #print(np.column_stack((np.log10(np.sort(fx, axis=None)), np.moveaxis(pos,1,-1)[np.unravel_index(np.argsort(fx, axis=None), fx.shape)])))
    #print(np.column_stack(((np.sort(fx, axis=None)), np.moveaxis(pos,1,-1)[np.unravel_index(np.argsort(fx, axis=None), fx.shape)])))

    return space, pos, fx

def plot_performance(fx, fig=None):
    """Creates a performance plot for all PSO particles across iterations"""

    fig = fig or plt.figure()
    ax = fig.add_subplot(111)
    ax.set_ylabel('Student-t evaluation')
    ax.set_xlabel('Iterations')
    for i, _fx in enumerate(fx):
        ax.plot(np.arange(_fx.shape[0]), _fx, label='Particle %d' % i)
    return fig

def plot_3d_space_evolution(space, pos, fx, fig=None):
    """Creates a 3D plot showing the evolution of PSO particles in a 3D search space, ignoring NaN values."""

    num_iterations = pos.shape[0]
    cm = plt.get_cmap('viridis')
    colors = [cm(i / num_iterations) for i in range(num_iterations)]  # Color for each iteration

    # Initialize the figure and 3D axis
    fig = fig or plt.figure()
    ax = fig.add_subplot(111, projection="3d")
    
    for i, (_pos, _fx) in enumerate(zip(pos, fx)):
        # Masking NaNs in position and fx
        mask = ~np.isnan(_fx)  # Mask for valid fx values
        valid_pos = _pos[:, mask]  # Only keep positions where fx is valid

        # Plot only valid points for this iteration with a single color
        ax.scatter(valid_pos[0, :], valid_pos[1, :], valid_pos[2, :], 
                   color=colors[i], 
                   s=5,
                   label=f"Iteration {i}")

    #scalarMap.set_array(fx)
    #cbar = fig.colorbar(scalarMap)
    #cbar.set_label('Iterations', fontsize=16)

    axis_labels = space['plot_label']
    lb = space['lb']
    ub = space['ub']
    ax.set_xlabel(axis_labels[0], fontsize=16)
    ax.set_ylabel(axis_labels[1], fontsize=16)
    ax.set_zlabel(axis_labels[2], fontsize=16)
    #ax.set_xlim(lb[0], ub[0])
    #ax.set_ylim(lb[1], ub[1])
    #ax.set_zlim(lb[2], ub[2])

    return fig

def plot_2d_space_evolution(space, pos, fx, fig=None):
    """Creates a 3D plot showing the evolution of PSO particles in a 3D search space, ignoring NaN values."""

    num_iterations = pos.shape[0]
    cm = plt.get_cmap('viridis')
    colors = [cm(i / num_iterations) for i in range(num_iterations)]  # Color for each iteration

    # Initialize the figure and 3D axis
    fig = fig or plt.figure()
    ax = fig.add_subplot(111)
    
    for i, (_pos, _fx) in enumerate(zip(pos, fx)):
        # Masking NaNs in position and fx
        mask = ~np.isnan(_fx)  # Mask for valid fx values
        valid_pos = _pos[:, mask]  # Only keep positions where fx is valid

        # Plot only valid points for this iteration with a single color
        ax.scatter(valid_pos[1, :], valid_pos[2, :], 
                   color=colors[i], 
                   s=5,
                   label=f"Iteration {i}")

    axis_labels = space['plot_label']

    ax.set_xlabel(axis_labels[1], fontsize=16)
    ax.set_ylabel(axis_labels[2], fontsize=16)

    return fig

def plot_2d_space_evolution_iteration(space, pos, fx, col_x, col_y, fig=None):
    """Creates a 2D plot showing the evolution of particles with color based on log10 of fx values.
    
    Parameters:
    - space: dictionary containing plot labels for axes.
    - pos: 3D array of particle positions (iterations, dimensions, particles).
    - fx: array of fx values for particles over iterations.
    - col_x: integer, column index for x-axis data in pos.
    - col_y: integer, column index for y-axis data in pos.
    - fig: matplotlib figure object (optional).
    """
    """
    # Initialize the figure and axis
    fig = fig or plt.figure()
    ax = fig.add_subplot(111)
    
    # Apply log10 to fx values and create a colormap normalization
    #log_fx = np.log10(np.abs(fx) + 1e-10)  # Avoid log(0) by adding a small constant
    log_fx = np.log10(fx)  # Avoid log(0) by adding a small constant
    norm = Normalize(vmin=np.nanmin(log_fx), vmax=np.nanmax(log_fx))
    cmap = plt.get_cmap('viridis')

    # Plot particles at each iteration
    for i, (_pos, _fx) in enumerate(zip(pos, log_fx)):
        mask = ~np.isnan(_fx)  # Mask for valid fx values
        valid_pos = _pos[:, mask]
        valid_fx = _fx[mask]

        # Use specified columns for x and y, and normalized log_fx values for coloring
        scatter = ax.scatter(valid_pos[col_x, :], valid_pos[col_y, :],
                             c=valid_fx,
                             cmap=cmap,
                             norm=norm,
                             s=5)

    # Add colorbar based on the log-scaled fx
    cbar = fig.colorbar(scatter, ax=ax, label='Log Student-t score')

    # Set labels from space dictionary
    axis_labels = space['plot_label']
    ax.set_xlabel(axis_labels[col_x], fontsize=16)
    ax.set_ylabel(axis_labels[col_y], fontsize=16)
    """
    # Initialize the figure and axis
    fig = fig or plt.figure()
    ax = fig.add_subplot(111)

    # Create a colormap based on the iteration number
    num_iterations = pos.shape[2]  # Total number of iterations
    norm = Normalize(vmin=0, vmax=num_iterations - 1)  # Normalize to the range of iterations
    cmap = plt.get_cmap('viridis')

    # Aggregate positions and iteration indices
    all_pos_x = []
    all_pos_y = []
    all_iterations = []

    for i in range(num_iterations):  # Iterate over the third dimension (iterations)
        valid_fx_mask = ~np.isnan(fx[:, i])  # Mask for valid fx values at iteration i
        valid_pos = pos[valid_fx_mask, :, i]  # Select valid particles for iteration i

        all_pos_x.extend(valid_pos[:, col_x].tolist())
        all_pos_y.extend(valid_pos[:, col_y].tolist())
        all_iterations.extend([i] * valid_pos.shape[0])  # Append the iteration index

    # Scatter plot for all iterations
    scatter = ax.scatter(all_pos_x, all_pos_y, c=all_iterations, cmap=cmap, norm=norm, s=5)

    # Add colorbar for iteration-based coloring
    cbar = fig.colorbar(scatter, ax=ax)
    cbar.set_label('Iteration')

    # Set labels from space dictionary
    axis_labels = space['plot_label']
    ax.set_xlabel(axis_labels[col_x], fontsize=16)
    ax.set_ylabel(axis_labels[col_y], fontsize=16)

    return fig

def plot_2d_space_evolution_contour(space, pos, fx, col_x, col_y, fig=None):
    """Creates a 2D plot with density-based contours.
       Also displays confidence intervals for each label.
       
    Parameters:
    - space: dictionary containing plot labels for axes.
    - pos: 3D array of particle positions (iterations, dimensions, particles).
    - fx: array of fx values for particles over iterations.
    - col_x: integer, column index for x-axis data in pos.
    - col_y: integer, column index for y-axis data in pos.
    - labels: list of labels for each parameter (used for CI display).
    - fig: matplotlib figure object (optional).
    """
    
    # Initialize the figure and axis
    fig = fig or plt.figure()
    ax = fig.add_subplot(111)
    
    # Collect all valid x, y positions and fx values for percentile calculation
    all_pos_x = []
    all_pos_y = []
    all_fx = []
    for _pos, _fx in zip(pos, fx):
        mask = ~np.isnan(_fx)
        valid_pos = _pos[:, mask]
        valid_fx = _fx[mask]
        all_pos_x.extend(valid_pos[col_x, :].tolist())  # Append as a list
        all_pos_y.extend(valid_pos[col_y, :].tolist())  # Append as a list
        all_fx.extend(valid_fx.tolist())  # Append as a list
    
    # Convert all_fx to a NumPy array
    all_fx = np.array(all_fx)

    # Calculate percentiles of the fx values to define contour levels
    #fx_percentiles = np.percentile(np.log10(all_fx + 1e-10), [0, 16, 50, 84])
    fx_percentiles = np.percentile(np.log10(all_fx), [16, 50, 84])

    # Create density-based contours
    if len(all_pos_x) > 3:  # Need at least 4 points for contour generation
        contour = ax.tricontourf(all_pos_x, all_pos_y, np.log10(all_fx), 
                                 levels=50, cmap='viridis', alpha=0.7)

    # Set labels from space dictionary
    axis_labels = space['plot_label']
    ax.set_xlabel(axis_labels[col_x], fontsize=16)
    ax.set_ylabel(axis_labels[col_y], fontsize=16)

    # Add colorbar for contours
    if 'contour' in locals():
        cbar = fig.colorbar(contour, ax=ax)
        cbar.set_label('Log Student-t score')

    # Scatter plot with a single color for particles
    ax.scatter(all_pos_x, all_pos_y, c='k', s=5)

    return fig

def plot_3d_space_animation(space, pos, fx, fig=None):
    """Creates a 3D plot showing the evolution of PSO particles in a 3D search space"""

    fig = fig or plt.figure()
    fig.set_tight_layout(True)

    # Color mapping setup
    cm = plt.get_cmap('viridis')
    #cNorm = matplotlib.colors.Normalize(vmin=0.0, vmax=100)
    cNorm = matplotlib.colors.Normalize(vmin=np.amin(fx), vmax=np.amax(fx))
    scalarMap = cmx.ScalarMappable(norm=cNorm, cmap=cm)
    scalarMap.set_array(fx)

    # pos: S, D, L -> L, D, S / fx: S, L -> L, S
    pos = np.swapaxes(pos, 0, -1)
    fx = np.swapaxes(fx, 0, -1)

    axis_labels = space['plot_label']
    lb = space['lb']
    ub = space['ub']
    ax = fig.add_subplot(111, projection='3d')
    ax.set_xlabel(axis_labels[0])
    ax.set_ylabel(axis_labels[1])
    ax.set_zlabel(axis_labels[2])
    ax.set_xlim(lb[0], ub[0])
    ax.set_ylim(lb[1], ub[1])
    ax.set_zlim(lb[2], ub[2])
    scat = ax.scatter(pos[0][0], pos[0][1], pos[0][2],
                       c=scalarMap.to_rgba(fx[0]))
    title = ax.text2D(0.5, 0.95, 'Iteration 0', transform=fig.transFigure,
            ha='center', va='top')

    # Colorbar indicating color mapping
    cbar = fig.colorbar(scalarMap)
    cbar.set_label('Log Student-t score')
    colors = [scalarMap.to_rgba(x) for x in fx]

    def update(x):
        count, (pos, fx, color) = x
        scat._offsets3d = pos
        scat.set_color(color)
        title.set_text("Iteration %d" % (count + 1))
        return scat, title

    frames_data = list(enumerate(zip(pos, fx, colors)))
    animation = anim.FuncAnimation(fig, update, frames=frames_data, blit=False)
    return animation

def plot_pairplot_with_contours(space, pos, fx, cmap='viridis', hist_edgecolor='k', hist_bins=10):
    """
    Produce a pairplot with seaborn, adding KDE-based contours weighted by fx scores.
    
    Parameters:
    - space: Dictionary with plot labels for axes.
    - pos: 3D array of particle positions (iterations, dimensions, particles).
    - fx: Array of fx values for particles over iterations.
    - cmap: Colormap for KDE contours.
    - hist_edgecolor: Color of histogram edges.
    - hist_bins: Number of bins in histograms.
    
    Returns:
    - A seaborn PairGrid object with KDE-based contours.
    """
    S, D, L = pos.shape
    pos = np.swapaxes(pos, 0, 1)  # Rearrange pos to match DataFrame expectations
    f = pd.DataFrame(pos.reshape((D, S * L)).T, columns=space['plot_label'])

    # Create pairplot without histograms on the diagonal
    g = sns.pairplot(f, corner=True, diag_kind="none")

    # Prepare fx values for KDE weighting
    fx = np.array(fx).flatten()
    mask = ~np.isnan(fx)
    valid_fx = fx[mask]
    valid_pos = pos.reshape(D, S * L)[:, mask]  # Reshape pos and filter valid values

    # Log-transform fx for KDE weights (avoiding negative or zero fx)
    log_fx = np.log10(valid_fx + 1e-10)
    kde_weights = log_fx - log_fx.min() + 1e-5  # Normalize weights to avoid zero values

    # Overlay KDE plots with contours on lower triangle
    for i, ax in enumerate(g.axes.flat):
        if ax is None:
            continue

        row, col = divmod(i, len(space['plot_label']))  # Determine position in grid
        if row > col:  # Lower triangle
            x_label = space['plot_label'][col]
            y_label = space['plot_label'][row]
            
            # Filter for relevant columns
            x_data = valid_pos[col]
            y_data = valid_pos[row]
            
            if len(x_data) > 3:  # Ensure enough points for KDE
                sns.kdeplot(
                    x=x_data,
                    y=y_data,
                    ax=ax,
                    cmap=cmap,
                    levels=10,
                    weights=kde_weights,
                    fill=True,
                    alpha=0.5
                )
                # Dashed contour lines
                sns.kdeplot(
                    x=x_data,
                    y=y_data,
                    ax=ax,
                    cmap=cmap,
                    levels=10,
                    weights=kde_weights,
                    alpha =0.5,
                    fill=False
                )

            # Add scatter plot over the KDE contours
            ax.scatter(x_data, y_data, c='k', s=5)

    # Add histograms on the diagonal
    g.map_diag(sns.histplot, kde=False, edgecolor=hist_edgecolor, bins=hist_bins, fill=False, color='k')

    return g

def smf_processing_iteration(filename, num_particles, num_iterations, obs_data, ax=None):
    """
    This function reads data blocks from a file, treating each block as data for a 'particle'.
    The function then plots each particle's data for multiple iterations, with different colors for each iteration.
    
    Parameters:
    obs_data: tuple containing (x_obs, y_obs, label)
    """
    x_obs, y_obs, label = obs_data
    y_obs_converted = [10**y for y in y_obs]

    # Read file data
    with open(filename, 'r') as file:
        lines = file.readlines()
    
    # Initialize variables
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Separate data into blocks and iterations
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

    # Append remaining blocks
    if current_block_y:
        current_iteration.append(current_block_y)
    if current_iteration:
        blocks_by_iteration.append(current_iteration)
    
    # Create figure and axes if not provided
    if ax is None:
        fig, ax = plt.subplots()
    else:
        fig = ax.figure

    # Set up colormap and normalization for iterations
    colormap = cm.get_cmap('viridis', num_iterations)
    norm = Normalize(vmin=0, vmax=num_iterations - 1)

    # Plot iterations
    for iteration_index, blocks in enumerate(blocks_by_iteration):
        color = colormap(iteration_index)
        for y in blocks:
            transformed_y = 10**np.array(y)
            ax.plot(x_values, transformed_y, color=color, alpha=0.2, linewidth=0.75)

    # Plot reference data
    ax.plot(x_obs, y_obs_converted, c='k', linewidth=2.25, label=label)

    # Add colorbar for iteration index
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array([])
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Iteration Number')

    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{stars}}\ (M_{\odot})$')
    ax.axis([8.0, 12.2, 1.0e-6, 1.0e-1])

    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def smf_processing_fitscore(data_filename, track_folder, num_particles, num_iterations, obs_data, ax=None, output_dir=None):
    """
    This function reads data blocks from a file, treating each block as data for a 'particle'.
    For each iteration, particles are colored according to fit scores from track files.
    
    Parameters:
    obs_data: tuple containing (x_obs, y_obs, label)
    """
    x_obs, y_obs, label = obs_data
    y_obs_converted = [10**y for y in y_obs]
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Separate data blocks and iterations
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

    # Append remaining blocks
    if current_block_y:
        current_iteration.append(current_block_y)
    if current_iteration:
        blocks_by_iteration.append(current_iteration)

    # Load fit scores from track files
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]

    # Ensure the fit scores align with the number of particles and iterations
    assert len(fit_scores) == num_iterations, "Mismatch in number of iterations"
    for scores in fit_scores:
        assert scores.shape[0] == num_particles, "Mismatch in number of particles"

    all_scores = np.concatenate(np.log10(fit_scores))
    norm = Normalize(vmin=np.nanmin(all_scores), vmax=np.nanmax(all_scores))
    colormap = cm.get_cmap('viridis_r')

    # Plot
    fig, ax = plt.subplots()
    lowest_score = np.inf
    lowest_score_line = None

    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            transformed_y = 10**np.array(y)
            color = colormap(norm(np.log10(scores[particle_index])))
            ax.plot(x_values, transformed_y, color=color, alpha=0.3, linewidth=0.75)

            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

    # Save results to CSV if we found a best fit
    if lowest_score_line is not None:
        try:
            base_filename = os.path.basename(data_filename)
            
            if output_dir:
                os.makedirs(output_dir, exist_ok=True)
                results_file = os.path.join(output_dir, base_filename.replace('.txt', '_fit_results.csv'))
            else:
                results_file = data_filename.replace('.txt', '_fit_results.csv')
            
            print(f"Saving fit results to: {results_file}")
            with open(results_file, 'w') as csvfile:
                for x_o, y_o, x_b, y_b in zip(x_obs, y_obs_converted, lowest_score_line[0], lowest_score_line[1]):
                    csvfile.write(f"{x_o}\t{y_o}\t{x_b}\t{y_b}\n")
            print(f"Successfully saved fit results to: {results_file}")
        except Exception as e:
            print(f"Error saving fit results: {e}")

    # Plot reference data
    ax.plot(x_obs, y_obs_converted, c='k', linewidth=2.25, label=label)

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

    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def bhmf_processing_iteration(filename, num_particles, num_iterations):
    """
    This function reads data blocks from a file, treating each block as data for a 'particle'.
    The function then plots each particle's data for multiple iterations, with different colors for each iteration.
    """

    x_obs = np.array([4.031578947368420,4.2631578947368400,4.6421052631578900,5,5.410526315789470,
                            5.8,6.231578947368420,6.6421052631578900,7.063157894736840,7.48421052631579,7.91578947368421,
                            8.305263157894740,8.589473684210530,8.989473684210530,9.294736842105260,9.557894736842110,
                            9.83157894736842,10.073684210526300,10.263157894736800,10.431578947368400])
    y_obs = np.array([-1.664723032069970,-1.7346938775510200,-1.8396501457726000,-1.944606413994170,
                        -2.049562682215740,-2.154518950437320,-2.2944606413994200,-2.329446064139940,-2.4344023323615200,
                        -2.4693877551020400,-2.644314868804670,-2.7842565597667600,-2.9941690962099100,-3.518950437317790,
                        -4.113702623906710,-4.743440233236150,-5.478134110787170,-6.247813411078720,-6.947521865889210,-7.4723032069970800])
    y_obs_converted = [10**y for y in y_obs]

    # Read file data
    with open(filename, 'r') as file:
        lines = file.readlines()
    
    # Initialize variables
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Separate data into blocks and iterations
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

    # Append remaining blocks if they were not fully added
    if current_block_y:
        current_iteration.append(current_block_y)
    if current_iteration:
        blocks_by_iteration.append(current_iteration)
    
    # Set up colormap and normalization for iterations
    colormap = cm.get_cmap('viridis', num_iterations)
    norm = Normalize(vmin=0, vmax=num_iterations - 1)

    # Plot each iteration with its respective color
    #plt.figure()
    fig, ax = plt.subplots()
    for iteration_index, blocks in enumerate(blocks_by_iteration):
        color = colormap(iteration_index)
        for y in blocks:
            transformed_y = 10**np.array(y)
            ax.plot(x_values, transformed_y, color=color, alpha=0.2, linewidth=0.75)

    # Plot reference data
    #plt.plot(baldry_2012_x_converted, baldry_2012_y_converted, linewidth=2.25, color='b', label='Baldry et al., 2012')
    ax.plot(x_obs, y_obs_converted, c='k', linewidth=2.25, label = 'Zhang et al., 2024 (TRINITY)')

    # Add colorbar for iteration index
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array([])
    cbar = plt.colorbar(sm)
    cbar.set_label('Iteration Number')

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

def bhmf_processing_fitscore(data_filename, track_folder, num_particles, num_iterations):
    """
    This function reads data blocks from a file, treating each block as data for a 'particle'.
    For each iteration, particles are colored according to fit scores from track files.
    """

    # Load the observational reference data
    x_obs = np.array([4.031578947368420,4.2631578947368400,4.6421052631578900,5,5.410526315789470,
                            5.8,6.231578947368420,6.6421052631578900,7.063157894736840,7.48421052631579,7.91578947368421,
                            8.305263157894740,8.589473684210530,8.989473684210530,9.294736842105260,9.557894736842110,
                            9.83157894736842,10.073684210526300,10.263157894736800,10.431578947368400])
    y_obs = np.array([-1.664723032069970,-1.7346938775510200,-1.8396501457726000,-1.944606413994170,
                        -2.049562682215740,-2.154518950437320,-2.2944606413994200,-2.329446064139940,-2.4344023323615200,
                        -2.4693877551020400,-2.644314868804670,-2.7842565597667600,-2.9941690962099100,-3.518950437317790,
                        -4.113702623906710,-4.743440233236150,-5.478134110787170,-6.247813411078720,-6.947521865889210,-7.4723032069970800])
    y_obs_converted = [10**y for y in y_obs]
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Separate data blocks and iterations
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

    # Append remaining blocks if they were not fully added
    if current_block_y:
        current_iteration.append(current_block_y)
    if current_iteration:
        blocks_by_iteration.append(current_iteration)

    # Load fit scores from the track files
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]

    # Ensure the fit scores align with the number of particles and iterations
    assert len(fit_scores) == num_iterations, "Mismatch in number of iterations"
    for scores in fit_scores:
        assert scores.shape[0] == num_particles, "Mismatch in number of particles"

    # Ensure that `fit_scores` are loaded correctly and that they are being passed to `Normalize`
    all_scores = np.concatenate(np.log10(fit_scores))  # Ensure fit_scores are log-scaled
    norm = Normalize(vmin=np.nanmin(all_scores), vmax=np.nanmax(all_scores))  # Normalize based on the actual range of fit_scores
    colormap = cm.get_cmap('viridis_r')  # Choose the colormap

    # Initialize the plot
    fig, ax = plt.subplots()
    # Loop through iterations and plot each particle's trajectory
    lowest_score = np.inf
    lowest_score_line = None

    # Loop through iterations and plot each particle's trajectory
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            transformed_y = 10**np.array(y)  # Convert log-scaled y values to original scale
            color = colormap(norm(np.log10(scores[particle_index])))  # Apply log10 normalization to scores for color mapping
            ax.plot(x_values, transformed_y, color=color, alpha=0.3, linewidth=0.75)

            # Check if this score is the lowest one encountered
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

     # Plot the lowest score as a highlighted line
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], color='r', linewidth=2, label='Best fit')

    # Plot reference data
    ax.plot(x_obs, y_obs_converted, c='k', linewidth=2.25, label='Zhang et al., 2024 (TRINITY)')

   # Add the colorbar to the figure
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array(all_scores)  # Pass `fit_scores` directly for the color scale
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Log Student-t score')

    # Set axis properties
    ax.set_yscale('log')
    ax.set_ylabel(r'$\phi\ (\mathrm{Mpc}^{-3}\ \mathrm{dex}^{-1})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
    ax.axis([6.0, 10.2, 1.0e-6, 1.0e-1])

    # Add legend and adjust appearance
    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    # Adjust layout and return the figure
    plt.tight_layout()
    return fig

def bhbm_processing_iteration(filename, num_particles, num_iterations):
    """
    This function reads data blocks from a file, treating each block as data for a 'particle'.
    The function then plots each particle's data for multiple iterations, with different colors for each iteration.
    """
    x_obs = 10. ** np.arange(7, 12.1, 0.1)
    y_obs = 10. ** (8.2 + 1.12 * np.log10(x_obs / 1.0e11))

    blackholemass, bulgemass = load_observation('data/Haring_Rix_2004_line.csv', cols=[2,3])
    log_blackholemass = blackholemass
    log_bulgemass = bulgemass

    # Read file data
    with open(filename, 'r') as file:
        lines = file.readlines()
    
    # Initialize variables
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Separate data into blocks and iterations
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

    # Append remaining blocks if they were not fully added
    if current_block_y:
        current_iteration.append(current_block_y)
    if current_iteration:
        blocks_by_iteration.append(current_iteration)
    
    # Set up colormap and normalization for iterations
    colormap = cm.get_cmap('viridis', num_iterations)
    norm = Normalize(vmin=0, vmax=num_iterations - 1)

    # Plot each iteration with its respective color
    #plt.figure()
    fig, ax = plt.subplots()
    for iteration_index, blocks in enumerate(blocks_by_iteration):
        color = colormap(iteration_index)
        for y in blocks:
            transformed_y = np.array(y)
            ax.plot(x_values, transformed_y, color=color, alpha=0.2, linewidth=0.75)

    # Plot reference data
    #plt.plot(baldry_2012_x_converted, baldry_2012_y_converted, linewidth=2.25, color='b', label='Baldry et al., 2012')
    #ax.plot(np.log10(x_obs), np.log10(y_obs), c='k', linewidth=2.25, label = '')
    ax.plot(log_bulgemass, log_blackholemass, linewidth=2.25, c='k', label="Haring & Rix 2004")

    # Add colorbar for iteration index
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array([])
    cbar = plt.colorbar(sm)
    cbar.set_label('Iteration Number')

    ax.set_ylabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    ax.axis([7.0, 12.0, 4.0, 10.0])

    leg = ax.legend(loc='upper left')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def bhbm_processing_fitscore(data_filename, track_folder, num_particles, num_iterations):
    """
    This function reads data blocks from a file, treating each block as data for a 'particle'.
    For each iteration, particles are colored according to fit scores from track files.
    """

    # Load the observational reference data
    x_obs = 10. ** np.arange(7, 12.1, 0.1)
    y_obs = 10. ** (8.2 + 1.12 * np.log10(x_obs / 1.0e11))

    blackholemass, bulgemass = load_observation('data/Haring_Rix_2004_line.csv', cols=[2,3])
    log_blackholemass = blackholemass
    log_bulgemass = bulgemass
    
    # Load data file
    with open(data_filename, 'r') as file:
        lines = file.readlines()
    
    blocks_by_iteration = []
    current_iteration = []
    current_block_y = []
    x_values = []
    capture_x = True

    # Separate data blocks and iterations
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

    # Append remaining blocks if they were not fully added
    if current_block_y:
        current_iteration.append(current_block_y)
    if current_iteration:
        blocks_by_iteration.append(current_iteration)

    # Load fit scores from the track files
    track_files = sorted(glob.glob(f"{track_folder}/track_*_fx.npy"))
    fit_scores = [np.load(file) for file in track_files[:num_iterations]]

    # Ensure the fit scores align with the number of particles and iterations
    assert len(fit_scores) == num_iterations, "Mismatch in number of iterations"
    for scores in fit_scores:
        assert scores.shape[0] == num_particles, "Mismatch in number of particles"

    # Ensure that `fit_scores` are loaded correctly and that they are being passed to `Normalize`
    all_scores = np.concatenate(np.log10(fit_scores))  # Ensure fit_scores are log-scaled
    norm = Normalize(vmin=np.nanmin(all_scores), vmax=np.nanmax(all_scores))  # Normalize based on the actual range of fit_scores
    colormap = cm.get_cmap('viridis_r')  # Choose the colormap

    # Initialize the plot
    fig, ax = plt.subplots()
    # Loop through iterations and plot each particle's trajectory
    lowest_score = np.inf
    lowest_score_line = None

    # Loop through iterations and plot each particle's trajectory
    for iteration_index, (blocks, scores) in enumerate(zip(blocks_by_iteration, fit_scores)):
        for particle_index, y in enumerate(blocks):
            transformed_y = np.array(y)  # Convert log-scaled y values to original scale
            color = colormap(norm(np.log10(scores[particle_index])))  # Apply log10 normalization to scores for color mapping
            ax.plot(x_values, transformed_y, color=color, alpha=0.3, linewidth=0.75)

            # Check if this score is the lowest one encountered
            score = np.log10(scores[particle_index])
            if score < lowest_score:
                lowest_score = score
                lowest_score_line = (x_values, transformed_y)

     # Plot the lowest score as a highlighted line
    if lowest_score_line is not None:
        ax.plot(lowest_score_line[0], lowest_score_line[1], color='r', linewidth=2, label='Best fit')

    # Plot reference data
    #ax.plot(np.log10(x_obs), np.log10(y_obs), c='k', linewidth=2.25, label = '')
    ax.plot(log_bulgemass, log_blackholemass, linewidth=2.25, c='k', label="Haring & Rix 2004")

   # Add the colorbar to the figure
    sm = plt.cm.ScalarMappable(cmap=colormap, norm=norm)
    sm.set_array(all_scores)  # Pass `fit_scores` directly for the color scale
    cbar = plt.colorbar(sm, ax=ax)
    cbar.set_label('Log Student-t score')

    # Set axis properties
    ax.set_ylabel(r'$\log_{10} M_{\mathrm{bh}}\ (M_{\odot})$')
    ax.set_xlabel(r'$\log_{10} M_{\mathrm{bulge}}\ (M_{\odot})$')
    ax.axis([7.0, 12.0, 4.0, 10.0])

    # Add legend and adjust appearance
    leg = ax.legend(loc='upper left')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    # Adjust layout and return the figure
    plt.tight_layout()
    return fig

def parse_data_blocks(filename):
    """
    Reads a file and returns a list of data blocks, where each block is a list of rows (lists of floats).
    """
    with open(filename, 'r') as file:
        lines = file.readlines()
    
    blocks = []
    current_block = []
    
    # Separate data blocks
    for line in lines:
        line = line.strip()
        if line.startswith("# New Data Block"):
            if current_block:
                blocks.append(current_block)
                current_block = []
        elif line:  # Ignore empty lines
            values = list(map(float, line.split('\t')))
            current_block.append(values)
    
    # Append the last block if it exists
    if current_block:
        blocks.append(current_block)
    
    return blocks

def generate_plots(space, pos, fx, D, output_folder):
    # Ensure params is a list (not ndarray)
    params = space['plot_label'].tolist() if isinstance(space['plot_label'], np.ndarray) else space['plot_label']

    # Generate all possible pairs of parameters (for 2D cost plots and contour plots)
    param_combinations = list(itertools.combinations(params, 2))

    # Create plots for cost and contour combinations
    for param1, param2 in param_combinations:
        param1_idx = params.index(param1)
        param2_idx = params.index(param2)

        # Create 2D cost plot
        fig = plot_2d_space_evolution_iteration(space, pos, fx, param1_idx, param2_idx)
        fig.savefig(os.path.join(output_folder, f'2DPSOC_{param1}_{param2}.pdf'), dpi=300)
        plt.close(fig)  # Add this to free memory

        # Create contour plot
        fig = plot_2d_space_evolution_contour(space, pos, fx, param1_idx, param2_idx)
        fig.savefig(os.path.join(output_folder, f'2DPSOC_{param1}_{param2}_contour.pdf'), dpi=300)
        plt.close(fig)  # Add this to free memory

    # If D == 3, you can add the 3D plot or animation
    if D == 3:
        animation = plot_3d_space_animation(space, pos, fx)
        animation.save(os.path.join(output_folder, '3d-particles.gif'), writer='imagemagick', fps=5)

    # Add performance plot
    fig = plot_performance(fx)
    fig.savefig(os.path.join(output_folder, 'performance.pdf'), dpi=300)
    plt.close(fig)

    # Add pairplot
    if D > 2:
        try:
            grid = plot_pairplot_with_contours(space, pos, fx)
            grid.savefig(os.path.join(output_folder, 'pairplot.pdf'), dpi=300)
            plt.close()
        except Exception as e:
            print(f"Error creating pairplot: {e}")

def load_all_params(directory, param_names, redshifts):
    """
    Load all parameter values and scores from the CSV files
    Returns both the full particle data and best values
    """
    best_params = {}
    best_scores = {}
    particle_data = {}
    
    for z in redshifts:
        # Format filename based on redshift value
        if z == 0.0:
            z_str = '0'  # For z=0
        elif z == 0.2:
            z_str = '02'  # For z=0.2
        elif z == 0.5:
            z_str = '05'  # For z=0.5
        elif z == 0.8:
            z_str = '08'  # For z=0.8
        elif z == 1.1:
            z_str = '11'  # For z=1.1
        elif z == 1.5:
            z_str = '15'  # For z=1.5
        elif z == 2.0:
            z_str = '20'  # For z=2.0
        elif z == 2.4:
            z_str = '24'  # For z=2.4
        elif z == 3.1:
            z_str = '31'  # For z=3.1
        elif z == 3.6:
            z_str = '36'  # For z=3.6
        elif z == 4.6:
            z_str = '46'  # For z=4.6
        elif z == 5.7:
            z_str = '57'  # For z=5.7
        elif z == 6.3:
            z_str = '63'  # For z=6.3
        elif z == 7.7:
            z_str = '77'  # For z=7.7
        elif z == 8.5:
            z_str = '85'  # For z=8.5
        elif z == 10.4:
            z_str = '104'  # For z=10.4
            
        filename = os.path.join(directory, f'params_z{z_str}.csv')
        
        try:
            if os.path.exists(filename):
                # Read CSV with tab delimiter, no header
                df = pd.read_csv(filename, delimiter='\t', header=None, names=param_names + ['score'])
                
                # Get particle data (all rows except last two)
                particle_data[z] = df.iloc[:-2]
                
                # Get second to last row for parameters and last row first column for score
                best_param_values = df.iloc[-2].values
                score = float(df.iloc[-1].iloc[0])
                
                best_params[z] = best_param_values[:-1]  # Exclude the score column
                best_scores[z] = score
                print(f"Successfully loaded data for z={z} from {filename}")
            else:
                print(f"File not found for z={z}: {filename}")
                #print(f"Available files in directory: {os.listdir(directory)}")
                
        except Exception as e:
            print(f"Error processing file for z={z}: {str(e)}")
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

def plot_parameter_evolution(particle_data, best_params, best_scores, param_names, output_dir):
    """
    Create a single figure with all parameters arranged in a 2x4 grid, including error shading
    and a global mean line across redshifts.
    """
    os.makedirs(output_dir, exist_ok=True)
    setup_plot_style()
    
    redshifts = sorted(best_params.keys())
    print(f"Plotting for redshifts: {redshifts}")
    
    fig, axs = plt.subplots(4, 2, figsize=(15, 20))
    axs_flat = axs.flatten()
    
    # Plot parameters
    for i, param in enumerate(param_names):
        ax = axs_flat[i]
        
        # Get best values for this parameter
        best_values = [best_params[z][i] for z in redshifts]
        
        # Calculate mean and std for each redshift
        means = []
        stds = []
        all_values = []  # To calculate global mean
        
        for z in redshifts:
            param_values = particle_data[z].iloc[:, i].values
            means.append(np.mean(param_values))
            stds.append(np.std(param_values))
            all_values.extend(param_values)  # Collect all values for global mean
        
        means = np.array(means)
        stds = np.array(stds)
        
        # Calculate global mean
        global_mean = np.mean(all_values)
        
        # Plot mean with shaded error region
        ax.fill_between(redshifts, means - stds, means + stds, 
                       alpha=0.3, color='orange', label='±1σ range')
        
        # Plot mean line
        ax.plot(redshifts, means, '-', color='red', 
                linewidth=1.5, label='Mean')
        
        # Plot best values
        ax.plot(redshifts, best_values, 'k--o', 
                linewidth=2, markersize=8, label='Best fit')
        
        # Plot global mean as a horizontal line
        ax.axhline(global_mean, color='blue', linestyle=':', 
                   linewidth=2, label='Global Mean')
        
        ax.set_xlabel('Redshift')
        ax.set_ylabel('Value')
        ax.set_title(param)
        #ax.grid(True, alpha=0.3)
        
        if i == 0:  # Only add legend to first plot
            ax.legend()
    
    # Remove the last subplot
    fig.delaxes(axs_flat[-1])
    
    plt.tight_layout()
    plt.savefig(os.path.join(output_dir, 'parameter_evolution_grid.png'), 
                bbox_inches='tight', dpi=300)
    plt.close()

def extract_redshift(filename):
    """Extract redshift value from filename for sorting."""
    if 'z0_' in filename:
        return 0.0
    elif 'z02_' in filename:
        return 0.2
    elif 'z05_' in filename:
        return 0.5
    elif 'z08_' in filename:
        return 0.8
    elif 'z11_' in filename:
        return 1.1
    elif 'z15_' in filename:
        return 1.5
    elif 'z20_' in filename:
        return 2.0
    elif 'z24_' in filename:
        return 2.4
    elif 'z31_' in filename:
        return 3.1
    elif 'z36_' in filename:
        return 3.6
    elif 'z46_' in filename:
        return 4.6
    elif 'z57_' in filename:
        return 5.7
    elif 'z63_' in filename:
        return 6.3
    elif 'z77_' in filename:
        return 7.7
    elif 'z85_' in filename:
        return 8.5
    elif 'z104_' in filename:
        return 10.4
    return float('inf')  # For any other files

def create_grid_figures(output_dir='parameter_plots', png_dir=None):
    """
    Creates three grid figures from existing plots:
    1. Iteration plots grid
    2. Fit scores plots grid
    3. Pair plots grid
    
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
        print(f"PNG directory {png_dir} does not exist!")
        return
    
    from math import ceil, sqrt
    
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Define the plot types to look for
    plot_types = {
        'iterations': '_all.png',
        'fit_scores': 'particles_fit_scores.png',
        'pairplot': 'pairplot.png'
    }
    
    # Find all PNG files in the output directory
    for plot_type, suffix in plot_types.items():
        matching_files = [f for f in os.listdir(png_dir) if f.endswith(suffix)]
        
        # Sort files by redshift
        matching_files.sort(key=extract_redshift)

        # Only create grid if we have more than one redshift
        if len(matching_files) <= 1:
            print(f"Not enough {plot_type} plots found for grid creation (need at least 2)")
            continue
                
        if not matching_files:
            print(f"No {plot_type} plots found.")
            continue
            
        # Calculate grid dimensions
        n_plots = len(matching_files)
        n_cols = min(4, ceil(sqrt(n_plots)))  # Max 4 columns
        n_rows = ceil(n_plots / n_cols)
        
        # Create figure
        fig = plt.figure(figsize=(6*n_cols, 5*n_rows))
        
        # Add each plot to the grid
        for idx, fname in enumerate(matching_files):
            # Read the image
            img = mpimg.imread(os.path.join(png_dir, fname))
            
            # Add subplot
            ax = fig.add_subplot(n_rows, n_cols, idx + 1)
            ax.imshow(img)
            ax.axis('off')
            
            # Add redshift-based title using existing extract_redshift function
            z = extract_redshift(fname)
            if z != float('inf'):
                title = f'z = {z:.1f}'
            else:
                title = 'Unknown redshift'
            ax.set_title(title)
        
        # Adjust layout and save
        plt.tight_layout()
        plt.savefig(os.path.join(output_dir, f'{plot_type}_grid.png'), 
                   dpi=300, bbox_inches='tight')
        plt.close()
        
        print(f"Created {plot_type} grid figure")

def plot_fit_comparison(directory='./', output_dir='parameter_plots'):
    """
    Plot comparison between observations and best fits from CSV files.
    All redshifts are shown on a single plot with different colors.
    """
    
    # Look for all SMF fit results files
    pattern = os.path.join(directory, 'SMF_z*_dump_fit_results.csv')
    files = glob.glob(pattern)
    
    if not files:
        print(f"No SMF fit results files found in {directory}")
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
            print(f"Skipping file with unknown redshift format: {file}")
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
                print("Warning: Non-finite values found in data!")
                continue
                
            color = colors[idx]
            
            # Plot with explicit zorder to ensure visibility
            ax.plot(x_obs, y_obs, ':', color=color, 
                    alpha=0.8, linewidth=2)
            ax.plot(x_fit, y_fit, '-', color=color, 
                    alpha=0.8, linewidth=2)
            
        except Exception as e:
            print(f"Error processing {file}: {e}")
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
    
    print(f"Combined comparison plot saved to {output_path}")

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
        
        # Save fit results to CSV
    if lowest_score_line is not None and output_dir:
        x_best, y_best = lowest_score_line
        x_obs, y_obs, _ = obs_data
        x_sage, y_sage, _ = sage_data

        y_obs_converted = [10**y for y in y_obs]
        y_sage_converted = [10**y for y in y_sage]

        # Get base filename
        base_filename = os.path.basename(data_filename)
        if '_z0_' in base_filename:  # Special handling for z0
            results_file = os.path.join(output_dir, 'SMF_z0_dump_fit_results.csv')
        else:
            results_file = os.path.join(output_dir, base_filename.replace('.txt', '_fit_results.csv'))
        
        print(f"Saving fit results to: {results_file}")
        
        with open(results_file, 'w') as csvfile:
            csvfile.write("# x\ty_obs\ty_sage\ty_best\n")
            # Interpolate all data to common x-grid for saving
            common_x = np.linspace(max(min(x_obs), min(x_sage), min(x_best)), 
                                 min(max(x_obs), max(x_sage), max(x_best)), 
                                 100)
            obs_interp = np.interp(common_x, x_obs, y_obs_converted)
            sage_interp = np.interp(common_x, x_sage, y_sage_converted)
            best_interp = np.interp(common_x, x_best, y_best)
            
            for x, y_o, y_s, y_b in zip(common_x, obs_interp, sage_interp, best_interp):
                csvfile.write(f"{x}\t{y_o}\t{y_s}\t{y_b}\n")

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

    leg = ax.legend(loc='upper right')
    leg.draw_frame(False)
    for t in leg.get_texts():
        t.set_fontsize('medium')

    plt.tight_layout()
    return fig

def load_sage_data():
    """Load SMF data from SAGE-miniUchuu"""
    sage_data = load_observation('data/sage_smf_all_redshifts.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
                                                              15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31])
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0.0, 0.2, 0.5, 0.8, 1.1, 1.5, 2.0, 2.4, 3.1, 3.6, 4.6, 5.7, 6.3, 7.7, 8.5, 10.4]
    col_indices = [(0,1), (2,3), (4,5), (6,7), (8,9), (10,11), (12,13), (14,15), 
                   (16,17), (18,19), (20,21), (22,23), (24,25), (26,27), (28,29), (30,31)]
    
    # Process data for each redshift
    for z, (mass_idx, phi_idx) in zip(redshifts, col_indices):
        logm = sage_data[mass_idx]
        logphi = sage_data[phi_idx]
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        data_by_z[z] = (logm[valid_mask], logphi[valid_mask], f'SAGE (miniuchuu) (z={z})')
    
    return data_by_z

def load_shuntov_data():
    """Load SMF data from Shuntov et al. 2024"""
    shuntov_data = load_observation('data/shuntov_2024_all.csv', cols=[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
                                                              15,16,17,18,19,20,21,22,23,24,25,26,27,28,29])
    # Dictionary to store data for each redshift
    data_by_z = {}
    
    # List of redshifts and their corresponding column indices
    redshifts = [0.2, 0.5, 0.8, 1.1, 1.5, 2.0, 2.4, 3.1, 3.6, 4.6, 5.7, 6.3, 7.7, 8.5, 10.4]
    col_indices = [(0,1), (2,3), (4,5), (6,7), (8,9), (10,11), (12,13), (14,15), 
                   (16,17), (18,19), (20,21), (22,23), (24,25), (26,27), (28,29)]
    
    # Process data for each redshift
    for z, (mass_idx, phi_idx) in zip(redshifts, col_indices):
        logm = shuntov_data[mass_idx]
        logphi = shuntov_data[phi_idx]
        valid_mask = ~np.isnan(logm) & ~np.isnan(logphi)
        data_by_z[z] = (logm[valid_mask], logphi[valid_mask], f'Shuntov et al., 2024 (z={z})')
    
    return data_by_z

def load_gama_data(config_opts):
    """Load GAMA data for z=0"""
    import routines as r
    logm, logphi, dlogphi = load_observation('data/GAMA_SMF_highres.csv', cols=[0,1,2])
    
    # Use passed parameters
    cosmology_correction_median = np.log10(r.comoving_distance(0.079, 100*config_opts.h0, 0, config_opts.Omega0, 1.0-config_opts.Omega0) / 
                                         r.comoving_distance(0.079, 70.0, 0, 0.3, 0.7))
    cosmology_correction_maximum = np.log10(r.comoving_distance(0.1, 100*config_opts.h0, 0, config_opts.Omega0, 1.0-config_opts.Omega0) / 
                                          r.comoving_distance(0.1, 70.0, 0, 0.3, 0.7))
    
    x_obs = logm + 2.0 * cosmology_correction_median 
    y_obs = logphi - 3.0 * cosmology_correction_maximum + 0.0807
    
    return x_obs, y_obs, 'Driver et al., 2022 (GAMA)'

def get_smf_files_map(config_opts):
    """Create mapping of SMF dump files to their corresponding observational data"""
    # Load all observational data
    shuntov_data = load_shuntov_data()
    gama_data = load_gama_data(config_opts)
    sage_data = load_sage_data()
    
    # Create mapping
    smf_files = {
        'SMF_z0_dump.txt': (gama_data, sage_data[0.0]),
        'SMF_z02_dump.txt': (shuntov_data[0.2], sage_data[0.2]),
        'SMF_z05_dump.txt': (shuntov_data[0.5], sage_data[0.5]), 
        'SMF_z08_dump.txt': (shuntov_data[0.8], sage_data[0.8]),
        'SMF_z11_dump.txt': (shuntov_data[1.1], sage_data[1.1]),
        'SMF_z15_dump.txt': (shuntov_data[1.5], sage_data[1.5]),
        'SMF_z20_dump.txt': (shuntov_data[2.0], sage_data[2.0]),
        'SMF_z24_dump.txt': (shuntov_data[2.4], sage_data[2.4]),
        'SMF_z31_dump.txt': (shuntov_data[3.1], sage_data[3.1]),
        'SMF_z36_dump.txt': (shuntov_data[3.6], sage_data[3.6]),
        'SMF_z46_dump.txt': (shuntov_data[4.6], sage_data[4.6]),
        'SMF_z57_dump.txt': (shuntov_data[5.7], sage_data[5.7]),
        'SMF_z63_dump.txt': (shuntov_data[6.3], sage_data[6.3]),
        'SMF_z77_dump.txt': (shuntov_data[7.7], sage_data[7.7]),
        'SMF_z85_dump.txt': (shuntov_data[8.5], sage_data[8.5]),
        'SMF_z104_dump.txt': (shuntov_data[10.4], sage_data[10.4])
    }
    
    return smf_files

def main(tracks_dir, space_file, output_dir, config_opts):
    print("\nStarting diagnostics analysis...")
    
    # Load particle data
    space, pos, fx = load_space_and_particles(tracks_dir, space_file)
    S, D, L = pos.shape
    print('Producing plots for S=%d particles, D=%d dimensions, L=%d iterations' % (S, D, L))

    # Create output directory if needed
    os.makedirs(output_dir, exist_ok=True)
    generate_plots(space, pos, fx, D, output_dir)

    # Get SMF files mapping with observational data
    smf_files = get_smf_files_map(config_opts)
    num_particles = S
    num_iterations = L

    # Process SMF files
    processed_any_smf = False
    for filename, (obs_data, sage_data) in smf_files.items():
        filepath = os.path.join(output_dir, filename)
        
        if os.path.exists(filepath):
            print(f"\nProcessing {filename}...")
            processed_any_smf = True
            
            # Create iteration plot
            try:
                print("Creating iteration plot...")
                fig = smf_processing_iteration(filepath, num_particles, num_iterations, obs_data)
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_all.png')
                fig.savefig(outfile, dpi=300)
                print(f"Saved iteration plot to {outfile}")
                plt.close(fig)
            except Exception as e:
                print(f"Error creating iteration plot: {str(e)}")

            # Create pairplot
            try:
                print("Creating pairplot...")
                grid = plot_pairplot_with_contours(space, pos, fx)
                outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_pairplot.png')
                grid.savefig(outfile, dpi=300)
                print(f"Saved pairplot to {outfile}")
            except Exception as e:
                print(f"Error creating pairplot: {str(e)}")

            # Create fit score plot
            try:
                print("Creating fit score plot with SAGE comparison...")
                fig = plot_smf_comparison(
                    filepath,
                    tracks_dir,
                    num_particles,
                    num_iterations, 
                    obs_data,
                    sage_data,
                    output_dir=output_dir
                )
                if fig is not None:
                    outfile = os.path.join(output_dir, f'{os.path.splitext(filename)[0]}_particles_fit_scores.png')
                    fig.savefig(outfile, dpi=300)
                    print(f"Saved fit score plot to {outfile}")
                    plt.close(fig)
                else:
                    print(f"No figure was created for {filename}")
            except Exception as e:
                print(f"Error creating fit score plot: {str(e)}")
                print(f"Full traceback:")
                import traceback
                traceback.print_exc()
        else:
            print(f"Warning: {filename} not found in {output_dir}")

    if not processed_any_smf:
        print("\nWarning: No SMF files were found to process!")
        print(f"Expected files in: {output_dir}")
        print("Expected files: ", list(smf_files.keys()))

    # Load parameter values
    print("\nProcessing parameter evolution...")
    param_names = ['SFR efficiency', 'Reheating epsilon', 'Ejection efficiency', 
                   'Reincorporation Factor', 'Radio Mode', 'Quasar Mode', 'Black Hole growth']
    redshifts = [0.0, 0.2, 0.5, 0.8, 1.1, 1.5, 2.0, 2.4, 3.1, 3.6, 4.6, 5.7, 6.3, 7.7, 8.5, 10.4]
    
    particle_data, best_params, best_scores = load_all_params(output_dir, param_names, redshifts)
    
    if particle_data:
        print("\nCreating visualization plots...")
        
        print("Creating parameter evolution plots...")
        plot_parameter_evolution(particle_data, best_params, best_scores, param_names, output_dir)

        if processed_any_smf:

            print("\nCreating grid figures...")
            create_grid_figures(output_dir=output_dir, png_dir=output_dir)
            
        print("\nAll plots have been saved to:", output_dir)
    else:
        print("\nNo parameter files found for visualization!")

    return particle_data, best_params, best_scores


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

    main(opts.tracks_dir, opts.space_file, opts.output_dir)