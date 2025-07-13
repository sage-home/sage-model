#!/usr/bin/env python
import math
import h5py as h5
import numpy as np
import os
import plotly.graph_objects as go
import plotly.express as px
from random import sample, seed
import warnings
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from plotly.subplots import make_subplots
import plotly.io as pio

warnings.filterwarnings("ignore")

# ========================== USER OPTIONS ==========================

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Simulation details
Hubble_h = 0.73        # Hubble parameter
BoxSize = 62.5         # h-1 Mpc
VolumeFraction = 1.0   # Fraction of the full volume output by the model

# Plotting options
dilute = 7500          # Number of galaxies to plot in scatter plots
min_stellar_mass = 0.1  # Minimum stellar mass to include (in 10^10 M_sun / h)
sSFRcut = -11.0         # Divide quiescent from star forming galaxies

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
        try:
            property = h5.File(DirName + model_file, 'r')
            data = np.array(property[snap_num][param])
            
            if combined_data is None:
                combined_data = data
            else:
                combined_data = np.concatenate((combined_data, data))
        except Exception as e:
            print(f"Error reading {model_file}: {e}")
            
    return combined_data

def create_interactive_plot(pos_x, pos_y, pos_z, properties_dict, sample_indices, sfr_status=None):
    """
    Create an interactive 3D plot with Plotly
    
    Parameters:
    -----------
    pos_x, pos_y, pos_z : arrays
        Spatial positions of galaxies
    properties_dict : dict
        Dictionary of arrays containing galaxy properties for coloring
    sample_indices : array
        Indices of galaxies to include in the plot
    sfr_status : array, optional
        Array indicating whether each galaxy is star-forming (1) or quiescent (0)
    """
    # Create figure
    fig = go.Figure()
    
    # Sample the data
    x = pos_x[sample_indices]
    y = pos_y[sample_indices]
    z = pos_z[sample_indices]
    
    # Add base trace (black points)
    fig.add_trace(go.Scatter3d(
        x=x, y=y, z=z,
        mode='markers',
        marker=dict(
            size=2,
            color='black',
            opacity=0.5
        ),
        name='Galaxies',
        visible=True
    ))
    
    # If we have SFR status information, add traces for star-forming and quiescent galaxies
    if sfr_status is not None:
        sf_status = sfr_status[sample_indices]
        
        # Star-forming galaxies
        sf_indices = np.where(sf_status == 1)[0]
        if len(sf_indices) > 0:
            fig.add_trace(go.Scatter3d(
                x=x[sf_indices], y=y[sf_indices], z=z[sf_indices],
                mode='markers',
                marker=dict(
                    size=3,
                    color='blue',
                    opacity=0.7
                ),
                name='Star-forming',
                visible=False
            ))
        
        # Quiescent galaxies
        q_indices = np.where(sf_status == 0)[0]
        if len(q_indices) > 0:
            fig.add_trace(go.Scatter3d(
                x=x[q_indices], y=y[q_indices], z=z[q_indices],
                mode='markers',
                marker=dict(
                    size=3,
                    color='red',
                    opacity=0.7
                ),
                name='Quiescent',
                visible=False
            ))
    
    # Add colored traces for each property
    for prop_name, prop_values in properties_dict.items():
        # Skip if the property values are None
        if prop_values is None:
            continue
        
        # Get property values for the sampled galaxies
        prop = prop_values[sample_indices]
        
        # Set up marker properties
        marker_dict = dict(
            size=3,
            opacity=0.8,
        )
        
        # Determine colorscale and range based on property
        if 'Mass' in prop_name or 'mass' in prop_name:
            colorscale = 'Viridis'  # Purple-green for masses
            # Handle log scale for masses
            valid = prop > 0
            log_prop = np.zeros_like(prop)
            log_prop[valid] = np.log10(prop[valid])
            marker_text = [f"{prop_name}: {val:.2e}" for val in prop]
            display_name = f"{prop_name} (log10)"
            marker_values = log_prop
            
        elif 'Velocity' in prop_name or 'vir' in prop_name or 'max' in prop_name:
            colorscale = 'Plasma'  # Yellow-red for velocities
            # Cap velocity at 400 km/s for better visualization
            marker_values = np.clip(prop, 0, 400)
            marker_text = [f"{prop_name}: {val}" for val in prop]
            display_name = f"{prop_name} (km/s)"
            # Set fixed color range for velocity
            marker_dict['cmin'] = 0
            marker_dict['cmax'] = 200
            
        elif 'Radius' in prop_name or 'radius' in prop_name:
            colorscale = 'Cividis'  # Yellow-blue for sizes
            marker_text = [f"{prop_name}: {val}" for val in prop]
            display_name = prop_name
            marker_values = prop
            
        elif 'Type' in prop_name:
            colorscale = 'Jet'      # Blue-red for types
            marker_text = [f"{prop_name}: {val}" for val in prop]
            display_name = prop_name
            marker_values = prop
            
        elif 'SFR' in prop_name:
            colorscale = 'RdBu_r'     # Red-Blue for SFR (red=low, blue=high)
            marker_text = [f"{prop_name}: {val}" for val in prop]
            display_name = prop_name
            marker_values = prop
            # Set range for sSFR to highlight the threshold
            marker_dict['cmin'] = -13
            marker_dict['cmax'] = -9
            
        else:
            colorscale = 'Turbo'    # Rainbow for other properties
            marker_text = [f"{prop_name}: {val}" for val in prop]
            display_name = prop_name
            marker_values = prop
        
        # Update marker dictionary with color values and scale
        marker_dict.update(
            color=marker_values,
            colorscale=colorscale,
            colorbar=dict(title=display_name)
        )
        
        fig.add_trace(go.Scatter3d(
            x=x, y=y, z=z,
            mode='markers',
            marker=marker_dict,
            text=marker_text,
            hoverinfo='text',
            name=prop_name,
            visible=False  # Initially not visible
        ))
    
    # Create buttons for property selection
    buttons = []
    # First button for base view (black points)
    visible_array = [True]  # Base layer is always visible
    if sfr_status is not None:
        visible_array.extend([False, False])  # Star-forming and quiescent are initially hidden
    visible_array.extend([False] * len(properties_dict))  # All property traces are initially hidden
    
    buttons.append(dict(
        label="Base (No Color)",
        method="update",
        args=[{"visible": visible_array}]
    ))
    
    # Button for SFR classification (if available)
    if sfr_status is not None:
        sf_visible = [False]  # Base hidden
        sf_visible.extend([True, True])  # Star-forming and quiescent visible
        sf_visible.extend([False] * len(properties_dict))  # All property traces hidden
        
        buttons.append(dict(
            label="Star-forming vs Quiescent",
            method="update",
            args=[{"visible": sf_visible}]
        ))
    
    # Create a button for each property
    prop_offset = 1 if sfr_status is None else 3
    for i, prop_name in enumerate(properties_dict.keys()):
        # Create visibility array
        visible = [False]  # Base layer hidden
        if sfr_status is not None:
            visible.extend([False, False])  # Star-forming and quiescent hidden
        visible.extend([False] * len(properties_dict))
        visible[i + prop_offset] = True  # Make the selected property visible
        
        buttons.append(dict(
            label=prop_name,
            method="update",
            args=[{"visible": visible}]
        ))
    
    # Add dropdown menu
    fig.update_layout(
        updatemenus=[dict(
            type="dropdown",
            direction="down",
            active=0,
            x=0.1,
            y=1.15,
            buttons=buttons
        )],
        scene=dict(
            xaxis=dict(title='X (Mpc/h)'),
            yaxis=dict(title='Y (Mpc/h)'),
            zaxis=dict(title='Z (Mpc/h)'),
            aspectmode='cube'
        ),
        title="Interactive 3D Galaxy Distribution",
        height=800,
        width=900
    )
    
    return fig

if __name__ == '__main__':
    print('Creating interactive 3D galaxy plot')

    seed(2222)
    volume = (BoxSize/Hubble_h)**3.0 * VolumeFraction

    OutputDir = DirName + 'plots/'
    if not os.path.exists(OutputDir): os.makedirs(OutputDir)

    print('Reading galaxy properties from', DirName)
    model_files = [f for f in os.listdir(DirName) if f.startswith('model_') and f.endswith('.hdf5')]
    print(f'Found {len(model_files)} model files')

    # Read position data
    Posx = read_hdf(snap_num=Snapshot, param='Posx')
    Posy = read_hdf(snap_num=Snapshot, param='Posy')
    Posz = read_hdf(snap_num=Snapshot, param='Posz')
    
    # Read key properties to use for coloring
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / Hubble_h
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / Hubble_h
    BulgeMass = read_hdf(snap_num=Snapshot, param='BulgeMass') * 1.0e10 / Hubble_h
    ColdGas = read_hdf(snap_num=Snapshot, param='ColdGas') * 1.0e10 / Hubble_h
    HotGas = read_hdf(snap_num=Snapshot, param='HotGas') * 1.0e10 / Hubble_h
    Cgm = read_hdf(snap_num=Snapshot, param='CGMgas') * 1.0e10 / Hubble_h
    H1Gas = read_hdf(snap_num=Snapshot, param='HI_gas') * 1.0e10 / Hubble_h
    H2Gas = read_hdf(snap_num=Snapshot, param='H2_gas') * 1.0e10 / Hubble_h
    Type = read_hdf(snap_num=Snapshot, param='Type')
    Vvir = read_hdf(snap_num=Snapshot, param='Vvir')
    Vmax = read_hdf(snap_num=Snapshot, param='Vmax')
    
    # Read SFR data for star-forming/quiescent classification
    SfrDisk = read_hdf(snap_num=Snapshot, param='SfrDisk')
    SfrBulge = read_hdf(snap_num=Snapshot, param='SfrBulge')
    
    # Select galaxies with sufficient stellar mass
    valid_galaxies = np.where((Mvir > 0.0) & (StellarMass > min_stellar_mass))[0]
    
    # Calculate specific star formation rate (sSFR)
    # Need to handle cases where StellarMass is zero
    sSFR = np.zeros_like(StellarMass)
    nonzero_mass = StellarMass > 0
    sSFR[nonzero_mass] = np.log10((SfrDisk[nonzero_mass] + SfrBulge[nonzero_mass]) / StellarMass[nonzero_mass])
    
    # Create SFR status array (1 for star-forming, 0 for quiescent)
    sfr_status = np.zeros_like(StellarMass, dtype=int)
    sfr_status[sSFR >= sSFRcut] = 1  # Star-forming galaxies have sSFR >= sSFRcut
    
    # Print statistics
    sf_count = np.sum(sfr_status[valid_galaxies] == 1)
    q_count = np.sum(sfr_status[valid_galaxies] == 0)
    print(f"Star-forming galaxies: {sf_count} ({sf_count/len(valid_galaxies)*100:.1f}%)")
    print(f"Quiescent galaxies: {q_count} ({q_count/len(valid_galaxies)*100:.1f}%)")
    
    # Sample to reduce number of points if needed
    if len(valid_galaxies) > dilute:
        sampled_indices = sample(list(valid_galaxies), dilute)
    else:
        sampled_indices = valid_galaxies
    
    # Create dictionary of properties to use for coloring
    properties = {
        'Stellar Mass': StellarMass,
        'Virial Mass': Mvir,
        'Bulge Mass': BulgeMass,
        'Cold Gas Mass': ColdGas,
        'H2 Gas Mass': H2Gas,
        'Galaxy Type': Type,
        'Virial Velocity': Vvir
    }
    
    # Create and save the interactive plot
    fig = create_interactive_plot(Posx, Posy, Posz, properties, sampled_indices, sfr_status)
    
    # Save as HTML file
    output_file = OutputDir + 'interactive_galaxy_distribution.html'
    fig.write_html(output_file)
    print(f'Saved interactive plot to {output_file}')
    
    # Show plot (uncomment if running locally)
    # fig.show()

    # Create a figure with 3D subplots
    fig = plt.figure(figsize=(18, 6))

    # First subplot - Galaxy number density in 3D
    ax1 = fig.add_subplot(131, projection='3d')
    w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]
    zz = Posz[w]  # Add z-coordinate data

    # Option 1: 3D scatter plot with point density
    # Calculate 3D histogram for coloring
    H, edges = np.histogramdd((xx, yy, zz), bins=(20, 20, 20))

    hist_indices = np.floor((xx-np.min(xx))/(np.max(xx)-np.min(xx))*19).astype(int), \
                np.floor((yy-np.min(yy))/(np.max(yy)-np.min(yy))*19).astype(int), \
                np.floor((zz-np.min(zz))/(np.max(zz)-np.min(zz))*19).astype(int)
    point_density = H[hist_indices]

    # Create scatter plot with points colored by density
    scatter = ax1.scatter(xx, yy, zz, c=point_density, cmap='gist_heat', 
                        s=2, alpha=0.5)
    ax1.set_xlabel('x (Mpc/h)')
    ax1.set_ylabel('y (Mpc/h)')
    ax1.set_zlabel('z (Mpc/h)')
    ax1.set_title('Galaxy Number Density (3D)')
    ax1.set_xlim(0, BoxSize)
    ax1.set_ylim(0, BoxSize)
    ax1.set_zlim(0, BoxSize)

    # Second subplot - H2 gas density in 3D
    ax2 = fig.add_subplot(132, projection='3d')
    w = np.where((Mvir > 0.0) & (H2Gas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]
    zz = Posz[w]  # Add z-coordinate data
    weights = np.log10(H2Gas[w])  # Use H2 gas mass as weights

    # Option 2: Voxel plot for density visualization
    # Calculate 3D histogram
    H, edges = np.histogramdd((xx, yy, zz), bins=(20, 20, 20), weights=weights)

    # Create a meshgrid for voxel positions
    x_centers = (edges[0][:-1] + edges[0][1:]) / 2
    y_centers = (edges[1][:-1] + edges[1][1:]) / 2
    z_centers = (edges[2][:-1] + edges[2][1:]) / 2
    x_grid, y_grid, z_grid = np.meshgrid(x_centers, y_centers, z_centers, indexing='ij')

    # Normalize histogram values for color mapping
    H_norm = (H - H.min()) / (H.max() - H.min() + 1e-10)

    # Flatten arrays for scatter
    x_flat = x_grid.flatten()
    y_flat = y_grid.flatten()
    z_flat = z_grid.flatten()
    h_flat = H_norm.flatten()

    # Filter points based on density threshold (adjust for visibility)
    threshold = 0.1
    mask = h_flat > threshold
    scatter = ax2.scatter(x_flat[mask], y_flat[mask], z_flat[mask], 
                        c=h_flat[mask], cmap='CMRmap',
                        s=50*h_flat[mask], alpha=0.7)
    ax2.set_xlabel('x (Mpc/h)')
    ax2.set_ylabel('y (Mpc/h)')
    ax2.set_zlabel('z (Mpc/h)')
    ax2.set_title('H2 Gas Density (3D)')
    ax2.set_xlim(0, BoxSize)
    ax2.set_ylim(0, BoxSize)
    ax2.set_zlim(0, BoxSize)

    # Third subplot - H1 gas density in 3D
    ax3 = fig.add_subplot(133, projection='3d')
    w = np.where((Mvir > 0.0) & (H1Gas > 0.0))[0]
    if(len(w) > dilute): w = sample(list(w), dilute)

    xx = Posx[w]
    yy = Posy[w]
    zz = Posz[w]  # Add z-coordinate data
    weights = np.log10(H1Gas[w])  # Use H1 gas mass as weights

    # Create a 3D histogram
    H, edges = np.histogramdd((xx, yy, zz), bins=(20, 20, 20), weights=weights)

    # For better 3D visualization, show isosurfaces
    x_pos = np.linspace(0, BoxSize, 20)
    y_pos = np.linspace(0, BoxSize, 20)
    z_pos = np.linspace(0, BoxSize, 20)
    x_grid, y_grid, z_grid = np.meshgrid(x_pos, y_pos, z_pos, indexing='ij')

    # Choose a subset of data points for better visibility
    stride = 2
    ax3.scatter(xx[::stride], yy[::stride], zz[::stride], c=weights[::stride], 
            cmap='CMRmap', s=2, alpha=0.5)
    ax3.set_xlabel('x (Mpc/h)')
    ax3.set_ylabel('y (Mpc/h)')
    ax3.set_zlabel('z (Mpc/h)')
    ax3.set_title('H1 Gas Density (3D)')
    ax3.set_xlim(0, BoxSize)
    ax3.set_ylim(0, BoxSize)
    ax3.set_zlim(0, BoxSize)

    plt.tight_layout()
    OutputFormat = '.png'

    # Save the 3D visualization
    outputFile = OutputDir + '23.3DDensityPlotandH2andH1' + OutputFormat
    plt.savefig(outputFile)  # Save the figure
    print('Saved file to', outputFile, '\n')
    plt.close()

    def create_interactive_3d_density_plots(Posx, Posy, Posz, Mvir, StellarMass, H1Gas, H2Gas, 
                                            BoxSize, dilute, OutputDir):
        """
        Creates an interactive 3D visualization of galaxy, H1, and H2 gas densities
        with a dropdown menu to switch between different density visualizations.
        
        Parameters:
        -----------
        Posx, Posy, Posz : arrays
            Position coordinates of galaxies
        Mvir : array
            Virial mass of galaxies
        StellarMass : array
            Stellar mass of galaxies
        H1Gas, H2Gas : arrays
            H1 and H2 gas masses
        BoxSize : float
            Size of the simulation box
        dilute : int
            Maximum number of points to plot
        OutputDir : str
            Directory to save the output HTML
        """
        
        # Create the figure
        fig = go.Figure()
        
        # Function to compute 3D histogram and prepare plot data
        def prepare_3d_density_data(xx, yy, zz, weights=None, nbins=30):
            # Create 3D histogram
            H, edges = np.histogramdd((xx, yy, zz), bins=(nbins, nbins, nbins), weights=weights)
            
            # Get bin centers
            x_centers = (edges[0][:-1] + edges[0][1:]) / 2
            y_centers = (edges[1][:-1] + edges[1][1:]) / 2
            z_centers = (edges[2][:-1] + edges[2][1:]) / 2
            
            # Create meshgrid
            x_grid, y_grid, z_grid = np.meshgrid(x_centers, y_centers, z_centers, indexing='ij')
            
            # Flatten arrays
            x_flat = x_grid.flatten()
            y_flat = y_grid.flatten()
            z_flat = z_grid.flatten()
            values = H.flatten()
            
            # Normalize values for size and color
            if values.max() > 0:
                norm_values = (values - values.min()) / (values.max() - values.min() + 1e-10)
            else:
                norm_values = values
                
            # Filter low-density points for better visualization
            threshold = 0.1
            mask = norm_values > threshold
            
            return x_flat[mask], y_flat[mask], z_flat[mask], norm_values[mask], values[mask]
        
        # Prepare data for all three density types
        
        # 1. Galaxy number density
        w = np.where((Mvir > 0.0) & (StellarMass > 0.1))[0]
        if(len(w) > dilute): w = sample(list(w), dilute)
        xx_galaxy, yy_galaxy, zz_galaxy = Posx[w], Posy[w], Posz[w]
        
        x_gal, y_gal, z_gal, norm_gal, val_gal = prepare_3d_density_data(xx_galaxy, yy_galaxy, zz_galaxy)
        
        # 2. H2 gas density
        w = np.where((Mvir > 0.0) & (H2Gas > 0.0))[0]
        if(len(w) > dilute): w = sample(list(w), dilute)
        xx_h2, yy_h2, zz_h2 = Posx[w], Posy[w], Posz[w]
        weights_h2 = np.log10(H2Gas[w] + 1e-10)  # Add small value to avoid log(0)
        
        x_h2, y_h2, z_h2, norm_h2, val_h2 = prepare_3d_density_data(xx_h2, yy_h2, zz_h2, weights=weights_h2)
        
        # 3. H1 gas density
        w = np.where((Mvir > 0.0) & (H1Gas > 0.0))[0]
        if(len(w) > dilute): w = sample(list(w), dilute)
        xx_h1, yy_h1, zz_h1 = Posx[w], Posy[w], Posz[w]
        weights_h1 = np.log10(H1Gas[w] + 1e-10)  # Add small value to avoid log(0)
        
        x_h1, y_h1, z_h1, norm_h1, val_h1 = prepare_3d_density_data(xx_h1, yy_h1, zz_h1, weights=weights_h1)

        # 4. Cold gas density
        w = np.where((Mvir > 0.0) & (ColdGas > 0.0))[0]
        if(len(w) > dilute): w = sample(list(w), dilute)
        xx_cg, yy_cg, zz_cg = Posx[w], Posy[w], Posz[w]
        weights_cg = np.log10(ColdGas[w] + 1e-10)  # Add small value to avoid log(0)
        
        x_cg, y_cg, z_cg, norm_cg, val_cg = prepare_3d_density_data(xx_cg, yy_cg, zz_cg, weights=weights_cg)

        # 5. Hot gas density
        w = np.where((Mvir > 0.0) & (HotGas > 0.0))[0]
        if(len(w) > dilute): w = sample(list(w), dilute)
        xx_hg, yy_hg, zz_hg = Posx[w], Posy[w], Posz[w]
        weights_hg = np.log10(HotGas[w] + 1e-10)  # Add small value to avoid log(0)
        
        x_hg, y_hg, z_hg, norm_hg, val_hg = prepare_3d_density_data(xx_hg, yy_hg, zz_hg, weights=weights_hg)

        # 6. Cgm density
        w = np.where((Mvir > 0.0) & (Cgm > 0.0))[0]
        if(len(w) > dilute): w = sample(list(w), dilute)
        xx_cgm, yy_cgm, zz_cgm = Posx[w], Posy[w], Posz[w]
        weights_cgm = np.log10(Cgm[w] + 1e-10)  # Add small value to avoid log(0)
        
        x_cgm, y_cgm, z_cgm, norm_cgm, val_cgm = prepare_3d_density_data(xx_cgm, yy_cgm, zz_cgm, weights=weights_cgm)
        
        # Add traces for Galaxy density
        galaxy_trace = go.Scatter3d(
            x=x_gal,
            y=y_gal,
            z=z_gal,
            mode='markers',
            marker=dict(
                size=2 + 100 * norm_gal,  # Scale marker size based on density
                color=val_gal,           # Use density values for color
                colorscale='Turbo',        # Hot colorscale similar to gist_heat
                opacity=0.7,
                showscale=True,
                colorbar=dict(
                    title="Galaxy Density",
                    x=1.0,
                    y=0.5
                )
            ),
            name='Galaxy Density',
            visible=True
        )
        
        # Add traces for H2 gas density
        h2_trace = go.Scatter3d(
            x=x_h2,
            y=y_h2,
            z=z_h2,
            mode='markers',
            marker=dict(
                size=2 + 100 * norm_h2,  # Scale marker size based on density
                color=val_h2,           # Use log H2 gas mass values for color
                colorscale='Magma',     # Similar to CMRmap
                opacity=0.7,
                showscale=True,
                colorbar=dict(
                    title="H₂ Gas Density",
                    x=1.0,
                    y=0.5
                )
            ),
            name='H₂ Gas Density',
            visible=False  # Initially hidden
        )
        
        # Add traces for H1 gas density
        h1_trace = go.Scatter3d(
            x=x_h1,
            y=y_h1,
            z=z_h1,
            mode='markers',
            marker=dict(
                size=2 + 100 * norm_h1,  # Scale marker size based on density
                color=val_h1,           # Use log H1 gas mass values for color
                colorscale='Magma',     # Similar to CMRmap
                opacity=0.7,
                showscale=True,
                colorbar=dict(
                    title="H₁ Gas Density",
                    x=1.0,
                    y=0.5
                )
            ),
            name='H₁ Gas Density',
            visible=False  # Initially hidden
        )

        # Add traces for cold gas density
        cg_trace = go.Scatter3d(
            x=x_cg,
            y=y_cg,
            z=z_cg,
            mode='markers',
            marker=dict(
                size=2 + 100 * norm_cg,  # Scale marker size based on density
                color=val_cg,           # Use log H1 gas mass values for color
                colorscale='Magma',     # Similar to CMRmap
                opacity=0.7,
                showscale=True,
                colorbar=dict(
                    title="Cold Gas Density",
                    x=1.0,
                    y=0.5
                )
            ),
            name='Cold Gas Density',
            visible=False  # Initially hidden
        )

        # Add traces for hot gas density
        hg_trace = go.Scatter3d(
            x=x_hg,
            y=y_hg,
            z=z_hg,
            mode='markers',
            marker=dict(
                size=2 + 100 * norm_hg,  # Scale marker size based on density
                color=val_hg,           # Use log H1 gas mass values for color
                colorscale='Magma',     # Similar to CMRmap
                opacity=0.7,
                showscale=True,
                colorbar=dict(
                    title="Hot Gas Density",
                    x=1.0,
                    y=0.5
                )
            ),
            name='Hot Gas Density',
            visible=False  # Initially hidden
        )

        # Add traces for cgm density
        cgm_trace = go.Scatter3d(
            x=x_cgm,
            y=y_cgm,
            z=z_cgm,
            mode='markers',
            marker=dict(
                size=2 + 100 * norm_cgm,  # Scale marker size based on density
                color=val_cgm,           # Use log H1 gas mass values for color
                colorscale='Turbo',     # Similar to CMRmap
                opacity=0.7,
                showscale=True,
                colorbar=dict(
                    title="CGM Density",
                    x=1.0,
                    y=0.5
                )
            ),
            name='CGM Density',
            visible=False  # Initially hidden
        )
        
        fig.add_trace(galaxy_trace)
        fig.add_trace(h2_trace)
        fig.add_trace(h1_trace)
        fig.add_trace(cg_trace)
        fig.add_trace(hg_trace)
        fig.add_trace(cgm_trace)
        
        # Create dropdown menu
        updatemenus = [
            dict(
                active=0,
                buttons=list([
                    dict(
                        label="Galaxy Number Density",
                        method="update",
                        args=[{"visible": [True, False, False, False, False, False]},
                            {"title": "Galaxy Number Density"}]
                    ),
                    dict(
                        label="H₂ Gas Density",
                        method="update",
                        args=[{"visible": [False, True, False, False, False, False]},
                            {"title": "H₂ Gas Density"}]
                    ),
                    dict(
                        label="H₁ Gas Density",
                        method="update",
                        args=[{"visible": [False, False, True, False, False, False]},
                            {"title": "H₁ Gas Density"}]
                    ),
                    dict(
                        label="Cold Gas Density",
                        method="update",
                        args=[{"visible": [False, False, False, True, False, False]},
                            {"title": "Cold Gas Density"}]
                    ),
                    dict(
                        label="Hot Gas Density",
                        method="update",
                        args=[{"visible": [False, False, False, False, True, False]},
                            {"title": "Hot Gas Density"}]
                    ),
                    dict(
                        label="CGM Density",
                        method="update",
                        args=[{"visible": [False, False, False, False, False, True]},
                            {"title": "CGM Density"}]
                    )
                ]),
                direction="down",
                pad={"r": 10, "t": 10},
                showactive=True,
                x=0.1,
                xanchor="left",
                y=1.15,
                yanchor="top"
            )
        ]
        
        # Update layout
        fig.update_layout(
            title="Galaxy and Gas Density 3D Visualization",
            updatemenus=updatemenus,
            scene=dict(
                xaxis=dict(title='x (Mpc/h)', range=[0, BoxSize]),
                yaxis=dict(title='y (Mpc/h)', range=[0, BoxSize]),
                zaxis=dict(title='z (Mpc/h)', range=[0, BoxSize]),
                aspectmode='cube'
            ),
            margin=dict(l=0, r=0, b=0, t=50),
            height=800
        )
        
        # Add annotations
        fig.add_annotation(
            text="Select Density Type:",
            x=0.02,
            y=1.12,
            xref="paper",
            yref="paper",
            showarrow=False
        )
        
        # Save the figure as HTML
        output_file = os.path.join(OutputDir, '3D_Interactive_Density_Visualization.html')
        pio.write_html(fig, file=output_file, auto_open=False)
        print(f"Saved interactive visualization to {output_file}")
        
        return fig

    fig = create_interactive_3d_density_plots(
        Posx, Posy, Posz,
        Mvir, StellarMass, H1Gas, H2Gas,
        BoxSize=BoxSize,
        dilute=10000,  # Adjust this based on your data size and performance needs
        OutputDir=OutputDir
    )