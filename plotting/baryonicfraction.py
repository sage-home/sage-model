import numpy as np
import matplotlib.pyplot as plt

def calculate_preventative_feedback(vvir, z=2.0):
    """Simple preventative feedback calculation"""
    # Calculate z-factor
    if z <= 1.0:
        z_factor = (1.0 + z)**0.5
    else:
        z_factor = (1.0 + 1.0)**0.5 * ((1.0 + z)/(1.0 + 1.0))**2.0
    
    v_crit = 30.0 * z_factor
    
    # Calculate minimum floor
    min_floor = 0.3 / (1.0 + 0.5 * (z - 1.0))
    if min_floor < 0.05:
        min_floor = 0.05
    if z <= 1.0:
        min_floor = 0.3
    
    # Calculate suppression factor
    f_suppress = min_floor + (1.0 - min_floor) / (1.0 + (v_crit/vvir)**2.0)
    
    # Ensure bounds
    if isinstance(f_suppress, np.ndarray):
        f_suppress = np.clip(f_suppress, 0.05, 1.0)
    else:
        if f_suppress < 0.05:
            f_suppress = 0.05
        if f_suppress > 1.0:
            f_suppress = 1.0
    
    return f_suppress

# Create plots with multiple redshifts
def create_redshift_comparison_plots():
    # Create figure with two subplots
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 7))
    
    # Default baryon fraction
    baryon_frac = 0.17
    
    # List of redshifts to plot
    redshifts = [0.0, 0.5, 1.0, 2.0, 3.0]
    
    # Colors for different redshifts
    colors = ['blue', 'green', 'orange', 'red', 'purple']
    
    # Plot 1: As a function of Vvir
    vvir_values = np.linspace(10, 500, 100)  # 10 to 500 km/s
    
    # Default constant baryon fraction
    ax1.axhline(y=baryon_frac, color='black', linestyle='--', linewidth=2, 
               label='Default (fb = 0.17)')
    
    # Plot for each redshift
    for z, color in zip(redshifts, colors):
        suppression = calculate_preventative_feedback(vvir_values, z=z)
        effective_fb = baryon_frac * suppression
        ax1.plot(vvir_values, effective_fb, color=color, linestyle='-', linewidth=2.5,
                label=f'z = {z}')
    
    ax1.set_xlabel('Vvir (km/s)', fontsize=14)
    ax1.set_ylabel('Baryon Fraction', fontsize=14)
    ax1.set_title('Preventative Feedback as Function of Vvir', fontsize=16)
    ax1.set_ylim(0, 0.2)
    ax1.grid(False)
    ax1.legend(loc='lower right')
    
    # Plot 2: As a function of Mvir
    # Create a simple array for Mvir (10^9 to 10^14 M_sun)
    mvir_values = np.logspace(9, 14, 100)
    
    # For simplicity, use a basic M ~ V^3 relation
    # This is approximate and just for visualization
    vvir_from_mvir = 200 * (mvir_values/1e12)**(1/3)
    
    # Default constant baryon fraction
    ax2.axhline(y=baryon_frac, color='black', linestyle='--', linewidth=2, 
               label='Default (fb = 0.17)')
    
    # Plot for each redshift
    for z, color in zip(redshifts, colors):
        suppression = calculate_preventative_feedback(vvir_from_mvir, z=z)
        effective_fb = baryon_frac * suppression
        ax2.plot(mvir_values, effective_fb, color=color, linestyle='-', linewidth=2.5,
                label=f'z = {z}')
    
    ax2.set_xscale('log')
    ax2.set_xlabel('Mvir (M$_{\odot}$)', fontsize=14)
    ax2.set_ylabel('Baryon Fraction', fontsize=14)
    ax2.set_title('Preventative Feedback as Function of Mvir', fontsize=16)
    ax2.set_ylim(0, 0.2)
    ax2.grid(False)
    ax2.legend(loc='lower right')
    
    # Format the x-axis for the Mvir plot
    ax2.set_xticks([1e9, 1e10, 1e11, 1e12, 1e13, 1e14])
    ax2.set_xticklabels(['10⁹', '10¹⁰', '10¹¹', '10¹²', '10¹³', '10¹⁴'])

    
    plt.tight_layout()
    plt.subplots_adjust(bottom=0.15)  # Make room for the annotation
    return fig

# Create the plots
fig = create_redshift_comparison_plots()
plt.savefig('baryonicfraction.png', dpi=300)