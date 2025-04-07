import os
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from matplotlib.widgets import Slider, Button, RadioButtons
from matplotlib.colorbar import ColorbarBase

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

# Cache for data to avoid reloading
data_cache = {}

def read_hdf(filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files"""
    # Check if data is in cache
    cache_key = f"{snap_num}_{param}"
    if cache_key in data_cache:
        return data_cache[cache_key]
    
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
    
    # Store in cache for future use
    data_cache[cache_key] = combined_data
    return combined_data

def calculate_ejection_and_reincorporation(Vvir, SfrDisk, Rvir, epsilon_disk, epsilon_halo, kappa_reinc, V_SN=630):
    """Calculate ejection rate (equation 13) and reincorporation rate (equation 14)"""
    # Calculate ejection rate (equation 13)
    ejection_factor = epsilon_halo * (V_SN**2 / Vvir**2) - epsilon_disk
    ejection_rate = np.maximum(ejection_factor, 0) * SfrDisk  # Zero if factor is negative
    
    # Calculate reincorporation rate (equation 14)
    V_esc = V_SN / np.sqrt(2)
    V_crit = kappa_reinc * V_esc
    t_dyn = Rvir / Vvir
    
    # Assuming steady state where ejected mass = ejection_rate * some time period
    # This gives us a way to estimate the ejected reservoir based on the ejection rate
    ejected_mass = ejection_rate * t_dyn
    
    # Calculate reincorporation rate
    reinc_factor = np.zeros_like(Vvir)
    mask = Vvir > V_crit
    reinc_factor[mask] = (Vvir[mask] / V_crit - 1)
    reinc_rate = reinc_factor * ejected_mass / t_dyn
    
    return ejection_rate, reinc_rate, V_crit

def create_interactive_plots():
    """Create interactive plots with sliders for parameters"""
    # Read necessary data
    Vvir = read_hdf(snap_num=Snapshot, param='Vvir')
    SfrDisk = read_hdf(snap_num=Snapshot, param='SfrDisk')
    Rvir = read_hdf(snap_num=Snapshot, param='Rvir')
    Type = read_hdf(snap_num=Snapshot, param='Type')
    Mvir = read_hdf(snap_num=Snapshot, param='Mvir') * 1.0e10 / 0.73  # Get halo mass for color coding
    StellarMass = read_hdf(snap_num=Snapshot, param='StellarMass') * 1.0e10 / 0.73  # Get stellar mass for alternative color coding
    BlackHoleMass = read_hdf(snap_num=Snapshot, param='BlackHoleMass') * 1.0e10 / 0.73  # Get black hole mass for color coding
    
    # Filter for central galaxies (Type=0) with some SFR
    mask_central = (Type == 0) & (SfrDisk > 0) & (Rvir > 0)
    Vvir_central = Vvir[mask_central]
    SfrDisk_central = SfrDisk[mask_central]
    Rvir_central = Rvir[mask_central]
    Mvir_central = Mvir[mask_central]
    StellarMass_central = StellarMass[mask_central]
    BlackHoleMass_central = BlackHoleMass[mask_central]
    
    # Filter for satellite galaxies (Type=1) with some SFR
    mask_satellite = (Type == 1) & (SfrDisk > 0) & (Rvir > 0)
    Vvir_satellite = Vvir[mask_satellite]
    SfrDisk_satellite = SfrDisk[mask_satellite]
    Rvir_satellite = Rvir[mask_satellite]
    Mvir_satellite = Mvir[mask_satellite]
    StellarMass_satellite = StellarMass[mask_satellite]
    BlackHoleMass_satellite = BlackHoleMass[mask_satellite]
    
    # Sample satellites if there are too many (for better performance)
    if len(Vvir_satellite) > 5000:
        sample_indices = np.random.choice(len(Vvir_satellite), 5000, replace=False)
        Vvir_satellite = Vvir_satellite[sample_indices]
        SfrDisk_satellite = SfrDisk_satellite[sample_indices]
        Rvir_satellite = Rvir_satellite[sample_indices]
        Mvir_satellite = Mvir_satellite[sample_indices]
        StellarMass_satellite = StellarMass_satellite[sample_indices]
        BlackHoleMass_satellite = BlackHoleMass_satellite[sample_indices]
    
    # Create figure with subplots - increase bottom margin to make room for controls
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 18))
    plt.subplots_adjust(left=0.1, bottom=0.3, right=0.85, top=0.95, hspace=0.3)
    
    # Initial parameter values
    init_epsilon_disk = 3.0
    init_epsilon_halo = 0.3
    init_kappa_reinc = 0.15
    init_V_SN = 630
    
    # Initial color data is Mvir
    color_data_central = np.log10(Mvir_central)
    color_data_satellite = np.log10(Mvir_satellite)
    color_label = r'$\log_{10} M_{vir}$ [$M_{\odot}$]'
    
    # Create a colormap for halo mass
    vmin = min(color_data_central.min(), color_data_satellite.min())
    vmax = max(color_data_central.max(), color_data_satellite.max())
    
    cmap = plt.cm.plasma_r
    norm = colors.Normalize(vmin=vmin, vmax=vmax)
    
    # Calculate initial values for central galaxies
    ejection_rate_central, reinc_rate_central, v_crit = calculate_ejection_and_reincorporation(
        Vvir_central, SfrDisk_central, Rvir_central, 
        init_epsilon_disk, init_epsilon_halo, init_kappa_reinc, init_V_SN
    )
    net_rate_central = ejection_rate_central - reinc_rate_central
    
    # Calculate initial values for satellite galaxies
    ejection_rate_satellite, reinc_rate_satellite, _ = calculate_ejection_and_reincorporation(
        Vvir_satellite, SfrDisk_satellite, Rvir_satellite, 
        init_epsilon_disk, init_epsilon_halo, init_kappa_reinc, init_V_SN
    )
    net_rate_satellite = ejection_rate_satellite - reinc_rate_satellite
    
    # Create initial scatter plots with color mapping based on halo mass
    # Plot 1: Ejection Rate
    scatter1_central = ax1.scatter(Vvir_central, ejection_rate_central, 
                                  s=5, c=color_data_central, cmap=cmap, norm=norm, 
                                  alpha=0.7, marker='o', label='Centrals')
    scatter1_satellite = ax1.scatter(Vvir_satellite, ejection_rate_satellite, 
                                    s=20, c=color_data_satellite, cmap=cmap, norm=norm, 
                                    alpha=0.7, marker='x', label='Satellites')
    vline1 = ax1.axvline(x=v_crit, color='red', linestyle='--', alpha=0.5)
    
    # Plot 2: Reincorporation Rate
    scatter2_central = ax2.scatter(Vvir_central, reinc_rate_central, 
                                  s=5, c=color_data_central, cmap=cmap, norm=norm, 
                                  alpha=0.7, marker='o')
    scatter2_satellite = ax2.scatter(Vvir_satellite, reinc_rate_satellite, 
                                    s=20, c=color_data_satellite, cmap=cmap, norm=norm, 
                                    alpha=0.7, marker='x')
    vline2 = ax2.axvline(x=v_crit, color='red', linestyle='--', alpha=0.5)
    
    # Plot 3: Net Ejection Rate
    scatter3_central = ax3.scatter(Vvir_central, net_rate_central, 
                                  s=5, c=color_data_central, cmap=cmap, norm=norm, 
                                  alpha=0.7, marker='o')
    scatter3_satellite = ax3.scatter(Vvir_satellite, net_rate_satellite, 
                                    s=20, c=color_data_satellite, cmap=cmap, norm=norm, 
                                    alpha=0.7, marker='x')
    vline3 = ax3.axvline(x=v_crit, color='red', linestyle='--', alpha=0.5)
    hline3 = ax3.axhline(y=0, color='black', linestyle='-', alpha=0.5)
    
    # Add colorbar for halo mass
    cax = fig.add_axes([0.88, 0.5, 0.02, 0.4])  # Position the colorbar
    cbar = ColorbarBase(cax, cmap=cmap, norm=norm)
    cbar.set_label(color_label)
    
    # Add legend to first plot
    ax1.legend(loc='upper right')
    
    # Set axis properties
    ax1.set_xlabel(r"$V_{vir}$ (km/s)")
    ax1.set_ylabel(r"Ejection Rate (M$_\odot$/yr)")
    ax1.set_title(r"Theoretical Ejection Rate: $\dot{m}_{ejected} = \left( \epsilon_{halo}\ \frac{V_{SN}^{2}}{V_{vir}^{2}} - \epsilon_{disk} \right) \dot{m}_{\ast}$")
    ax1.set_yscale('log')
    ax1.set_xlim(25, 225)
    ax1.set_ylim(1e-10, 1e3)
    ax1.grid(True, alpha=0.3)
    
    ax2.set_xlabel(r"$V_{vir}$ (km/s)")
    ax2.set_ylabel(r"Reincorporation Rate (M$_\odot$/yr)")
    ax2.set_title(r"Theoretical Reincorporation Rate: $\dot{m}_{reinc} = \left( \frac{V_{vir}}{V_{crit}} - 1 \right)\ \frac{\dot{m}_{ejected}}{t_{dyn}}$")
    ax2.set_yscale('log')
    ax2.set_xlim(25, 225)
    ax2.set_ylim(1e-10, 1e3)  # Set y-limit to match first plot for consistency
    ax2.grid(True, alpha=0.3)
    
    ax3.set_xlabel(r"$V_{vir}$ (km/s)")
    ax3.set_ylabel(r"Net Ejection Rate (M$_\odot$/yr)")
    ax3.set_title("Net Ejection Rate (Ejection - Reincorporation)")
    ax3.set_yscale('symlog')
    ax3.set_xlim(25, 225)
    ax3.grid(True, alpha=0.3)
    
    # Add sliders - moved further down to make sure they don't overlap with plots
    ax_epsilon_disk = plt.axes([0.1, 0.2, 0.65, 0.02])
    ax_epsilon_halo = plt.axes([0.1, 0.15, 0.65, 0.02])
    ax_kappa_reinc = plt.axes([0.1, 0.1, 0.65, 0.02])
    ax_v_sn = plt.axes([0.1, 0.05, 0.65, 0.02])
    
    slider_epsilon_disk = Slider(ax_epsilon_disk, r'$\varepsilon_{disk}$', 0.1, 10.0, valinit=init_epsilon_disk)
    slider_epsilon_halo = Slider(ax_epsilon_halo, r'$\varepsilon_{halo}$', 0.01, 5.0, valinit=init_epsilon_halo)
    slider_kappa_reinc = Slider(ax_kappa_reinc, r'$\kappa_{reinc}$', 0.01, 1.0, valinit=init_kappa_reinc)
    slider_v_sn = Slider(ax_v_sn, r'$V_{SN}$ (km/s)', 300, 1000, valinit=init_V_SN)
    
    # Add reset button - moved to side
    ax_reset = plt.axes([0.88, 0.05, 0.1, 0.04])
    button_reset = Button(ax_reset, 'Reset')
    
    # Add comparison presets - moved to side
    ax_radio = plt.axes([0.88, 0.1, 0.1, 0.15])
    radio = RadioButtons(ax_radio, ('Custom', 'Default', 'PSO', 'No ejection', 'Full ejection'))
    
    # Add color selection radio button - moved to side and added Black Hole Mass option
    ax_color = plt.axes([0.88, 0.25, 0.1, 0.1])
    radio_color = RadioButtons(ax_color, ('Haloes', 'Galaxies', 'Black Holes'))
    
    # Create info text to display parameter values
    ax1_text = ax1.text(0.02, 0.95, '', transform=ax1.transAxes, va='top')
    
    def update_info_text():
        """Update the info text with current parameter values"""
        text = (f"ε_disk={slider_epsilon_disk.val:.2f}, "
                f"ε_halo={slider_epsilon_halo.val:.2f}, "
                f"κ_reinc={slider_kappa_reinc.val:.2f}, "
                f"V_SN={slider_v_sn.val:.0f}, "
                f"V_crit={v_crit:.1f}")
        ax1_text.set_text(text)
    
    def update(val):
        """Update the plot when slider values change"""
        nonlocal v_crit
        
        # Get current values from sliders
        epsilon_disk = slider_epsilon_disk.val
        epsilon_halo = slider_epsilon_halo.val
        kappa_reinc = slider_kappa_reinc.val
        v_sn = slider_v_sn.val
        
        # Recalculate rates for central galaxies
        ejection_rate_central, reinc_rate_central, v_crit = calculate_ejection_and_reincorporation(
            Vvir_central, SfrDisk_central, Rvir_central, 
            epsilon_disk, epsilon_halo, kappa_reinc, v_sn
        )
        net_rate_central = ejection_rate_central - reinc_rate_central
        
        # Recalculate rates for satellite galaxies
        ejection_rate_satellite, reinc_rate_satellite, _ = calculate_ejection_and_reincorporation(
            Vvir_satellite, SfrDisk_satellite, Rvir_satellite, 
            epsilon_disk, epsilon_halo, kappa_reinc, v_sn
        )
        net_rate_satellite = ejection_rate_satellite - reinc_rate_satellite
        
        # Update plots - keep the same color mapping
        scatter1_central.set_offsets(np.column_stack([Vvir_central, ejection_rate_central]))
        scatter1_satellite.set_offsets(np.column_stack([Vvir_satellite, ejection_rate_satellite]))
        vline1.set_xdata([v_crit, v_crit])
        
        scatter2_central.set_offsets(np.column_stack([Vvir_central, reinc_rate_central]))
        scatter2_satellite.set_offsets(np.column_stack([Vvir_satellite, reinc_rate_satellite]))
        vline2.set_xdata([v_crit, v_crit])
        
        scatter3_central.set_offsets(np.column_stack([Vvir_central, net_rate_central]))
        scatter3_satellite.set_offsets(np.column_stack([Vvir_satellite, net_rate_satellite]))
        vline3.set_xdata([v_crit, v_crit])
        
        # Update info text
        update_info_text()
        
        # Redraw the figure
        fig.canvas.draw_idle()
    
    def update_color_mapping(label):
        """Update the color mapping based on selected property"""
        nonlocal color_label
        
        if label == 'Haloes':
            color_data_central = np.log10(Mvir_central)
            color_data_satellite = np.log10(Mvir_satellite)
            color_label = r'$\log_{10} M_{vir}$ [$M_{\odot}$]'
        elif label == 'Galaxies':
            color_data_central = np.log10(StellarMass_central)
            color_data_satellite = np.log10(StellarMass_satellite)
            color_label = r'$\log_{10} M_{*}$ [$M_{\odot}$]'
        else:  # Black Hole Mass
            # Apply a small offset to avoid log10(0)
            BHM_central = np.copy(BlackHoleMass_central)
            BHM_satellite = np.copy(BlackHoleMass_satellite)
            BHM_central[BHM_central <= 0] = 1e-10
            BHM_satellite[BHM_satellite <= 0] = 1e-10
            
            color_data_central = np.log10(BHM_central)
            color_data_satellite = np.log10(BHM_satellite)
            color_label = r'$\log_{10} M_{BH}$ [$M_{\odot}$]'
        
        # Update color normalization
        vmin = min(np.min(color_data_central), np.min(color_data_satellite))
        vmax = max(np.max(color_data_central), np.max(color_data_satellite))
        norm = colors.Normalize(vmin=vmin, vmax=vmax)
        
        # Update scatter plots with new colors
        scatter1_central.set_array(color_data_central)
        scatter1_satellite.set_array(color_data_satellite)
        scatter2_central.set_array(color_data_central)
        scatter2_satellite.set_array(color_data_satellite)
        scatter3_central.set_array(color_data_central)
        scatter3_satellite.set_array(color_data_satellite)
        
        # Update all scatter plots with new normalization
        scatter1_central.set_norm(norm)
        scatter1_satellite.set_norm(norm)
        scatter2_central.set_norm(norm)
        scatter2_satellite.set_norm(norm)
        scatter3_central.set_norm(norm)
        scatter3_satellite.set_norm(norm)
        
        # Update colorbar
        cax.clear()
        cbar = ColorbarBase(cax, cmap=cmap, norm=norm)
        cbar.set_label(color_label)
        
        # Redraw the figure
        fig.canvas.draw_idle()
    
    def reset(event):
        """Reset sliders to initial values"""
        slider_epsilon_disk.reset()
        slider_epsilon_halo.reset()
        slider_kappa_reinc.reset()
        slider_v_sn.reset()
    
    def preset_selected(label):
        """Set slider values to predefined presets"""
        if label == 'Default':
            slider_epsilon_disk.set_val(3.0)
            slider_epsilon_halo.set_val(0.3)
            slider_kappa_reinc.set_val(0.15)
            slider_v_sn.set_val(630)
        elif label == 'PSO':
            slider_epsilon_disk.set_val(2.0)
            slider_epsilon_halo.set_val(0.2)
            slider_kappa_reinc.set_val(0.15)
            slider_v_sn.set_val(630)
        elif label == 'No ejection':
            slider_epsilon_disk.set_val(1.0)
            slider_epsilon_halo.set_val(0.01)
            slider_kappa_reinc.set_val(0.015)
            slider_v_sn.set_val(630)
        elif label == 'Full ejection':
            slider_epsilon_disk.set_val(1.0)
            slider_epsilon_halo.set_val(1.0)
            slider_kappa_reinc.set_val(0.5)
            slider_v_sn.set_val(630)
        # Custom doesn't change values
        
        update(None)
    
    # Connect the update functions to the widgets
    slider_epsilon_disk.on_changed(update)
    slider_epsilon_halo.on_changed(update)
    slider_kappa_reinc.on_changed(update)
    slider_v_sn.on_changed(update)
    button_reset.on_clicked(reset)
    radio.on_clicked(preset_selected)
    radio_color.on_clicked(update_color_mapping)
    
    # Initialize info text
    update_info_text()
    
    plt.show()
    
    # Save the figure if needed
    output_dir = './output/millennium/plots'
    os.makedirs(output_dir, exist_ok=True)
    interactive_path = os.path.join(output_dir, "interactive_feedback.png")
    fig.savefig(interactive_path)
    print(f"Interactive plot snapshot saved to {interactive_path}")

if __name__ == "__main__":
    create_interactive_plots()