import os
import h5py as h5
import numpy as np
import matplotlib.pyplot as plt

# File details
DirName = './output/millennium/'
FileName = 'model_0.hdf5'
Snapshot = 'Snap_63'

def read_hdf(filename=None, snap_num=None, param=None):
    """Read data from one or more SAGE model files"""
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

def plot_theory_relationships(output_dir):
    """Generate plots showing theoretical relationships for ejection and reincorporation"""
    # Read necessary data
    Vvir = read_hdf(snap_num=Snapshot, param='Vvir')
    SfrDisk = read_hdf(snap_num=Snapshot, param='SfrDisk')
    Rvir = read_hdf(snap_num=Snapshot, param='Rvir')
    Type = read_hdf(snap_num=Snapshot, param='Type')
    
    # Only use central galaxies with some SFR for clearer trends
    mask = (Type == 0) & (SfrDisk > 0) & (Rvir > 0)
    Vvir = Vvir[mask]
    SfrDisk = SfrDisk[mask]
    Rvir = Rvir[mask]
    
    # Define parameter sets for different scenarios
    param_sets = [
    # epsilon_disk, epsilon_halo, kappa_reinc, label, color
    (3.0, 0.3, 0.15, "Default", "purple"),
    (2.0, 0.2, 0.19, "PSO", "blue"),
    (1.0, 0.01, 0.05, "No ejection", "green"),
    (1.0, 1.0, 1.0, "Full ejection", "orange")
    ]
    
    # Create plot 1: Ejection rate vs Vvir
    plt.figure(figsize=(12, 8))

    # Plot in reverse order (least important first for zorder)
    param_sets_reversed = list(reversed(param_sets))
    
    for i, (epsilon_disk, epsilon_halo, kappa_reinc, label, color) in enumerate(param_sets_reversed):
        ejection_rate, _, v_crit = calculate_ejection_and_reincorporation(
            Vvir, SfrDisk, Rvir, epsilon_disk, epsilon_halo, kappa_reinc)
            
        # Adjust alpha based on the dataset (less alpha for dominating sets)
        alpha_val = 0.3 if color == "yellow" else 0.7
        # Adjust point size
        point_size = 1.5
        # Set zorder based on index (higher = more visible)
        z_order = i + 1
            
        plt.scatter(Vvir, ejection_rate, s=point_size, c=color, alpha=alpha_val, zorder=z_order,
                label=f"{label}: ε_disk={epsilon_disk}, ε_halo={epsilon_halo}, κ_reinc={kappa_reinc}")
            
        # Add vertical line at V_crit
        plt.axvline(x=v_crit, color=color, linestyle='--', alpha=0.3)
    
    plt.xlabel(r"$V_{vir}$ (km/s)")
    plt.ylabel(r"Ejection Rate (M$_\odot$/yr)")
    plt.yscale('log')
    plt.xlim(25, 225)
    plt.ylim(1e-10, 1e3)
    plt.grid(True, alpha=0.3)
    plt.legend(loc='lower right')
    plt.title("Theoretical Ejection Rate (Equation 13)")
    
    os.makedirs(output_dir, exist_ok=True)
    ejection_path = os.path.join(output_dir, "theoretical_ejection_rate.png")
    plt.savefig(ejection_path)
    print(f"Ejection plot saved to {ejection_path}")
    
    # Create plot 2: Reincorporation rate vs Vvir
    plt.figure(figsize=(12, 8))
    
    for epsilon_disk, epsilon_halo, kappa_reinc, label, color in param_sets:
        _, reinc_rate, v_crit = calculate_ejection_and_reincorporation(
            Vvir, SfrDisk, Rvir, epsilon_disk, epsilon_halo, kappa_reinc)
        
        plt.scatter(Vvir, reinc_rate, s=3, c=color, alpha=0.5,
                   label=f"{label}: ε_disk={epsilon_disk}, ε_halo={epsilon_halo}, κ_reinc={kappa_reinc}, V_crit={v_crit:.1f}")
        
        # Add vertical line at V_crit
        plt.axvline(x=v_crit, color=color, linestyle='--', alpha=0.3)
    
    plt.xlabel(r"$V_{vir}$ (km/s)")
    plt.ylabel(r"Reincorporation Rate (M$_\odot$/yr)")
    plt.yscale('log')
    plt.xlim(25, 200)
    #plt.ylim(1e-4, 1e3)
    plt.grid(True, alpha=0.3)
    plt.legend(loc='lower right')
    plt.title("Theoretical Reincorporation Rate (Equation 14)")
    
    reinc_path = os.path.join(output_dir, "theoretical_reincorporation_rate.png")
    plt.savefig(reinc_path)
    print(f"Reincorporation plot saved to {reinc_path}")
    
    # Create plot 3: Combined view of ejection vs reincorporation
    plt.figure(figsize=(12, 8))
    
    for epsilon_disk, epsilon_halo, kappa_reinc, label, color in param_sets:
        ejection_rate, reinc_rate, v_crit = calculate_ejection_and_reincorporation(
            Vvir, SfrDisk, Rvir, epsilon_disk, epsilon_halo, kappa_reinc)
        
        # Calculate net ejection (ejection minus reincorporation)
        net_rate = ejection_rate - reinc_rate
        
        plt.scatter(Vvir, net_rate, s=3, c=color, alpha=0.5,
                   label=f"{label}: ε_disk={epsilon_disk}, ε_halo={epsilon_halo}, κ_reinc={kappa_reinc}")
        
        # Add vertical line at V_crit
        plt.axvline(x=v_crit, color=color, linestyle='--', alpha=0.3)
    
    plt.xlabel(r"$V_{vir}$ (km/s)")
    plt.ylabel(r"Net Ejection Rate (M$_\odot$/yr)")
    plt.axhline(y=0, color='black', linestyle='-', alpha=0.5)
    plt.yscale('symlog')  # Symlog allows positive and negative values on log scale
    plt.xlim(25, 225)
    plt.grid(True, alpha=0.3)
    plt.legend(loc='upper right')
    plt.title("Net Ejection Rate (Ejection - Reincorporation)")
    
    net_path = os.path.join(output_dir, "theoretical_net_ejection.png")
    plt.savefig(net_path)
    print(f"Net ejection plot saved to {net_path}")

# Run the analysis
plot_theory_relationships('./output/millennium/plots')
