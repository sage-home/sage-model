import numpy as np
import matplotlib.pyplot as plt

def calculate_cgm_fraction_vvir(vvir, z, cgm_build_vcrit=60.0, cgm_build_alpha=0.5, cgm_build_zdep=0.5):
    """
    Calculate CGM fraction as function of virial velocity and redshift
    Direct from Vvir instead of converting from Mvir
    """
    if vvir <= 0.0:
        return 1.0
    
    # Calculate z-factor and critical velocity
    if z <= 1.0:
        z_factor = (1.0 + z)**cgm_build_zdep
    else:
        z_factor = (1.0 + 1.0)**cgm_build_zdep * ((1.0 + z)/(1.0 + 1.0))**2.0
    
    v_crit = cgm_build_vcrit * z_factor
    
    # Calculate minimum floor
    if z <= 1.0:
        min_floor = 0.3
    else:
        min_floor = 0.3 / (1.0 + 0.5 * (z - 1.0))
        if min_floor < 0.05:
            min_floor = 0.05
    
    # Calculate suppression factor
    f_suppress = min_floor + (1.0 - min_floor) / (1.0 + (v_crit/vvir)**cgm_build_alpha)
    
    # Additional suppression for intermediate-mass halos at high-z
    if z > 0.01 and vvir > 5.0 and vvir < 550.0:
        peak_suppress = (z - 0.01) / 2.0
        if peak_suppress > 1.0:
            peak_suppress = 1.0
        
        v_relative = (vvir - 550.0) / 5.0
        extra_suppress = peak_suppress * np.exp(-v_relative * v_relative)
        
        f_suppress *= (1.0 - extra_suppress)
    
    # Ensure bounds
    if f_suppress < 0.05:
        f_suppress = 0.05
    if f_suppress > 1.0:
        f_suppress = 1.0
    
    base_cgm_fraction = 1.0 - f_suppress
    
    # Redshift evolution
    if z > 7.5:
        z_scaling = 1.0 - 0.8 * (z - 7.5) / 2.5
        if z_scaling < 0.2:
            z_scaling = 0.3
    elif z > 3.0:
        z_scaling = 1.0
    elif z > 1.0:
        z_scaling = 0.3 + 0.7 * (z - 1.0) / 2.0
    else:
        z_scaling = 0.3
    
    cgm_fraction = base_cgm_fraction * z_scaling
    
    return cgm_fraction

# Set up the plot
fig, ax = plt.subplots(1, 1, figsize=(10, 6))

# Redshift range
z_range = np.linspace(0, 10, 100)

# Four different virial velocities
vvir_values = [30.0, 60.0, 90.0, 150.0]  # km/s
colors = ['blue', 'green', 'orange', 'red']
linestyles = ['-', '--', '-.', ':']

# Plot: CGM fraction vs redshift for different Vvir values
for i, vvir in enumerate(vvir_values):
    cgm_fractions = [calculate_cgm_fraction_vvir(vvir, z) for z in z_range]
    
    ax.plot(z_range, cgm_fractions, 
            color=colors[i], linestyle=linestyles[i], linewidth=2.5,
            label=f'$V_{{vir}}$ = {vvir} km/s')

ax.set_xlabel('Redshift (z)', fontsize=14)
ax.set_ylabel('CGM Fraction', fontsize=14)
ax.set_title('CGM Fraction vs Redshift for Different Virial Velocities', fontsize=16)
ax.grid(True, alpha=0.3)
ax.legend(fontsize=12)
ax.set_xlim(0, 10)
ax.set_ylim(0.0, 1.0)

# Add reference lines
ax.axhline(y=0.1, color='gray', linestyle=':', alpha=0.5, label='10% CGM')
ax.axhline(y=0.5, color='gray', linestyle=':', alpha=0.5, label='50% CGM')

# Add some key redshift markers
ax.axvline(x=1, color='gray', linestyle=':', alpha=0.3, label='z=1')
ax.axvline(x=3, color='gray', linestyle=':', alpha=0.3, label='z=3')
ax.axvline(x=6, color='gray', linestyle=':', alpha=0.3, label='z=6')

plt.tight_layout()
plt.savefig('cgm_fraction_vs_redshift.png')

# Print some key values
print("CGM fraction evolution for different virial velocities:")
print("=====================================================")
key_redshifts = [0.0, 1.0, 3.0, 6.0, 10.0]

for vvir in vvir_values:
    print(f"\nV_vir = {vvir} km/s:")
    for z in key_redshifts:
        cgm_frac = calculate_cgm_fraction_vvir(vvir, z)
        # Calculate effective V_crit for this redshift
        if z <= 1.0:
            z_factor = (1.0 + z)**0.5
        else:
            z_factor = (1.0 + 1.0)**0.5 * ((1.0 + z)/(1.0 + 1.0))**2.0
        v_crit = 60.0 * z_factor
        print(f"  z = {z:4.1f}: CGM = {cgm_frac:.3f}, V_crit = {v_crit:.1f} km/s")

# Print current parameter values
print(f"\nFixed parameters:")
print(f"CGMBuildVcrit = 60.0 km/s")
print(f"CGMBuildAlpha = 0.5")
print(f"CGMBuildZdep = 0.5")

import numpy as np
import matplotlib.pyplot as plt

# Set style for publication-quality plots
plt.style.use('default')
plt.rcParams.update({
    'font.size': 12,
    'font.family': 'serif',
    'axes.linewidth': 1.2,
    'axes.labelsize': 14,
    'xtick.labelsize': 12,
    'ytick.labelsize': 12,
    'legend.fontsize': 11,
    'figure.figsize': (10, 7),
    'figure.dpi': 300
})

# Physical constants and model parameters
V_CRIT_BASE = 445.48  # km/s (V_SN/sqrt(2))

# SAGE reincorporation parameters (adjust these to match your model)
REINC_FACTOR = 0.3                    # ReIncorporationFactor
CRITICAL_MASS = 5.0                   # CriticalReincMass in 10^10 M_sun/h (increased to affect more masses)
MASS_EXPONENT = 1.5                   # ReincorporationMassExp  
MIN_REINC_FACTOR = 0.1               # MinReincorporationFactor
REDSHIFT_EXPONENT = 2.0              # ReincorporationRedshiftExp

# Galaxy mass bins (in 10^10 M_sun/h units) - chosen to show mass-dependent effects
mass_bins = np.array([0.3, 0.8, 2.0, 8.0, 20.0])  # Range from below to above critical mass
mass_labels = ['3×10⁹ M☉/h', '8×10⁹ M☉/h', '2×10¹⁰ M☉/h', '8×10¹⁰ M☉/h', '2×10¹¹ M☉/h']
colors = ['#d62728', '#ff7f0e', '#2ca02c', '#1f77b4', '#9467bd']

# Redshift range
z_range = np.linspace(0, 6, 61)

# Define parameter sets for different strengths (used in panel 3)
param_sets = {
    'Weak': {'crit_mass': 8.0, 'mass_exp': 0.8, 'min_factor': 0.4},
    'Medium': {'crit_mass': 5.0, 'mass_exp': 1.5, 'min_factor': 0.1}, 
    'Strong': {'crit_mass': 3.0, 'mass_exp': 2.2, 'min_factor': 0.05},
    'Super-Strong': {'crit_mass': 1.5, 'mass_exp': 3.0, 'min_factor': 0.01}
}

# Three test masses for panel 3
test_masses_multi = [0.8, 2.0, 8.0]  # Low, medium, high mass
test_mass_labels_multi = ['8×10⁹ M☉/h', '2×10¹⁰ M☉/h', '8×10¹⁰ M☉/h']

def calculate_virial_velocity(mass):
    """
    Calculate virial velocity from halo mass
    Using typical scaling for SAGE: higher velocities for reasonable reincorporation
    Calibrated so that 10^10 M_sun/h gives ~400 km/s
    """
    return 400.0 * (mass ** 0.3)  # Slightly shallower scaling, higher normalization

def calculate_mass_dependent_factor(mass, critical_mass, mass_exp, min_factor):
    """
    Calculate mass-dependent scaling factor
    Only applies to masses below critical_mass
    """
    if mass >= critical_mass:
        return 1.0
    
    factor = (mass / critical_mass) ** mass_exp
    return max(factor, min_factor)

def calculate_redshift_factor(z, redshift_exp):
    """
    Calculate redshift-dependent scaling factor
    Slower reincorporation at higher redshift
    """
    return (1.0 + z) ** (-redshift_exp)

def calculate_reincorporation_efficiency(mass, z, mass_dependent=True, redshift_dependent=True):
    """
    Calculate reincorporation efficiency following SAGE model
    """
    # Critical velocity for reincorporation
    v_crit = V_CRIT_BASE * REINC_FACTOR
    v_vir = calculate_virial_velocity(mass)
    
    # Only galaxies with V_vir > V_crit can reincorporate
    if v_vir <= v_crit:
        return 0.0
    
    # Base efficiency (proportional to velocity excess)
    base_efficiency = (v_vir / v_crit - 1.0)
    
    # Apply mass-dependent scaling
    total_scaling = 1.0
    if mass_dependent:
        mass_factor = calculate_mass_dependent_factor(mass, CRITICAL_MASS, MASS_EXPONENT, MIN_REINC_FACTOR)
        total_scaling *= mass_factor
    
    # Apply redshift-dependent scaling
    if redshift_dependent:
        redshift_factor = calculate_redshift_factor(z, REDSHIFT_EXPONENT)
        total_scaling *= redshift_factor
    
    return base_efficiency * total_scaling

# Create the plot
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 16), sharex=True)

# Main plot: Reincorporation efficiency vs redshift
# Plot mass-dependent model first (solid lines)
for i, (mass, label, color) in enumerate(zip(mass_bins, mass_labels, colors)):
    efficiency_with_mass = [calculate_reincorporation_efficiency(mass, z, mass_dependent=True) for z in z_range]
    ax1.plot(z_range, efficiency_with_mass, color=color, linestyle='-', linewidth=2.5, 
             label=label if i == 0 else "", alpha=0.9)

# Plot standard model second (dashed lines) 
for i, (mass, label, color) in enumerate(zip(mass_bins, mass_labels, colors)):
    efficiency_standard = [calculate_reincorporation_efficiency(mass, z, mass_dependent=False) for z in z_range]
    ax1.plot(z_range, efficiency_standard, color=color, linestyle='--', linewidth=2, 
             alpha=0.7)

ax1.set_ylabel('Reincorporation Efficiency', fontsize=14)
ax1.set_title('SAGE Gas Reincorporation: Mass-Dependent vs Standard Model', fontsize=16, pad=20)
ax1.grid(True, alpha=0.3)
ax1.set_xlim(0, 6)
ax1.set_ylim(0, 1.2)  # Fixed reasonable range

# Create clear legends
# Line style legend
solid_line = plt.Line2D([0], [0], color='black', linewidth=2.5, linestyle='-', label='Mass-dependent model')
dashed_line = plt.Line2D([0], [0], color='black', linewidth=2, linestyle='--', label='Standard model')
style_legend = ax1.legend(handles=[solid_line, dashed_line], loc='upper right', 
                         frameon=True, fancybox=True, shadow=True, title='Model Type')

# Mass legend  
mass_lines = [plt.Line2D([0], [0], color=color, linewidth=2.5, label=label) 
              for color, label in zip(colors, mass_labels)]
mass_legend = ax1.legend(handles=mass_lines, loc='upper left', 
                        frameon=True, fancybox=True, shadow=True, title='Galaxy Mass')

# Add both legends
ax1.add_artist(style_legend)

# Lower plot: Show scaling factors for one mass
test_mass = 2.0  # 2×10^10 M_sun/h (one of our mass bins)
test_mass_label = '2×10¹⁰ M☉/h'
mass_factors = [calculate_mass_dependent_factor(test_mass, CRITICAL_MASS, MASS_EXPONENT, MIN_REINC_FACTOR) 
                for z in z_range]
redshift_factors = [calculate_redshift_factor(z, REDSHIFT_EXPONENT) for z in z_range]
combined_factors = [mf * rf for mf, rf in zip(mass_factors, redshift_factors)]

ax2.plot(z_range, mass_factors, 'r-', linewidth=2.5, label='Mass factor', alpha=0.8)
ax2.plot(z_range, redshift_factors, 'b-', linewidth=2.5, label='Redshift factor', alpha=0.8)
ax2.plot(z_range, combined_factors, 'k-', linewidth=3, label='Combined scaling', alpha=0.9)
ax2.axhline(y=1.0, color='gray', linestyle=':', alpha=0.7)

ax2.set_xlabel('Redshift (z)', fontsize=14)
ax2.set_ylabel('Scaling Factor', fontsize=14)
ax2.set_title(f'Scaling Components (Test mass: {test_mass_label})', fontsize=14)
ax2.grid(True, alpha=0.3)
ax2.legend(frameon=True, fancybox=True, shadow=True)
ax2.set_ylim(0, 1.1)

# Add parameter text box
param_text = f'''Model Parameters:
Critical Mass = {CRITICAL_MASS:.1f} × 10¹⁰ M☉/h
Mass Exponent = {MASS_EXPONENT:.1f}
Min. Factor = {MIN_REINC_FACTOR:.1f}
Redshift Exp. = {REDSHIFT_EXPONENT:.1f}
Reinc. Factor = {REINC_FACTOR:.1f}'''

ax2.text(0.02, 0.98, param_text, transform=ax2.transAxes, fontsize=10,
         verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

# Third panel: Parameter strength comparison
strength_colors = ['#95D5B2', '#52B788', '#2D6A4F', '#1B4332']  # Green shades
strength_labels = ['Weak', 'Medium', 'Strong', 'Super-Strong']

# Use a test mass that's affected by all parameter sets
test_mass_param = 2.0  # 2×10^10 M_sun/h

# Third panel: Parameter strength comparison with 3 test masses
strength_colors = ['#95D5B2', '#52B788', '#2D6A4F', '#1B4332']  # Green shades

# Three test masses with different line styles
test_masses_multi = [0.8, 2.0, 8.0]  # Low, medium, high mass
test_mass_labels_multi = ['8×10⁹ M☉/h', '2×10¹⁰ M☉/h', '8×10¹⁰ M☉/h']
line_styles = ['-', '--', ':']  # Solid, dashed, dotted
line_style_labels = ['Solid', 'Dashed', 'Dotted']

# Plot all parameter strengths for each test mass
for i, (strength, params) in enumerate(param_sets.items()):
    for j, (test_mass, mass_label, line_style) in enumerate(zip(test_masses_multi, test_mass_labels_multi, line_styles)):
        # Calculate efficiency with this parameter set and mass
        efficiency_strength = []
        for z in z_range:
            # Apply redshift scaling
            redshift_factor = calculate_redshift_factor(z, REDSHIFT_EXPONENT)
            
            # Calculate mass factor with this parameter set
            mass_factor = calculate_mass_dependent_factor(test_mass, 
                                                         params['crit_mass'], 
                                                         params['mass_exp'], 
                                                         params['min_factor'])
            
            # Base efficiency
            v_crit = V_CRIT_BASE * REINC_FACTOR
            v_vir = calculate_virial_velocity(test_mass)
            if v_vir > v_crit:
                base_eff = (v_vir / v_crit - 1.0)
                total_eff = base_eff * mass_factor * redshift_factor
            else:
                total_eff = 0.0
                
            efficiency_strength.append(total_eff)
        
        # Plot with unique combination of color (strength) and line style (mass)
        ax3.plot(z_range, efficiency_strength, color=strength_colors[i], 
                linestyle=line_style, linewidth=2.5, alpha=0.9)

ax3.set_xlabel('Redshift (z)', fontsize=14)
ax3.set_ylabel('Reincorporation Efficiency', fontsize=14)
ax3.set_title('Parameter Strength vs Mass Response Comparison', fontsize=14)
ax3.grid(True, alpha=0.3)

# Create comprehensive legend with both parameter strengths and masses
legend_elements = []

# Add parameter strength legend (colors)
for i, strength in enumerate(param_sets.keys()):
    legend_elements.append(plt.Line2D([0], [0], color=strength_colors[i], linewidth=2.5, 
                                    label=strength))

# Add separator
legend_elements.append(plt.Line2D([0], [0], color='white', linewidth=0, label=''))

# Add mass legend (line styles)
for j, (mass_label, line_style, style_label) in enumerate(zip(test_mass_labels_multi, line_styles, line_style_labels)):
    legend_elements.append(plt.Line2D([0], [0], color='black', linestyle=line_style, 
                                    linewidth=2.5, label=f'{mass_label} ({style_label})'))

ax3.legend(handles=legend_elements, loc='center left', bbox_to_anchor=(1.02, 0.5),
          frameon=True, fancybox=True, shadow=True, fontsize=9)
ax3.set_ylim(0, 1.2)

# Add parameter details text box
param_details = '''Reading the Plot:
• Color = Parameter Strength
• Line Style = Galaxy Mass

Parameter Sets:
Weak: Mcrit=8.0, exp=0.8, min=0.4
Medium: Mcrit=5.0, exp=1.5, min=0.1  
Strong: Mcrit=3.0, exp=2.2, min=0.05
Super: Mcrit=1.5, exp=3.0, min=0.01'''

ax3.text(0.02, 0.98, param_details, transform=ax3.transAxes, fontsize=9,
         verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))

plt.tight_layout()
plt.subplots_adjust(right=0.75)  # Make room for the legend on the right

# Save the figure
plt.savefig('reincorporation_vs_redshift.png', dpi=300, bbox_inches='tight')
plt.savefig('reincorporation_vs_redshift.pdf', bbox_inches='tight')
#plt.show()

# Print some diagnostic information
print("Reincorporation Model Analysis")
print("=" * 40)
print(f"Critical velocity: {V_CRIT_BASE * REINC_FACTOR:.1f} km/s")
print(f"Critical mass: {CRITICAL_MASS:.1f} × 10¹⁰ M☉/h")
print()

print("Virial velocities for mass bins:")
for mass, label in zip(mass_bins, mass_labels):
    v_vir = calculate_virial_velocity(mass)
    v_crit = V_CRIT_BASE * REINC_FACTOR
    can_reincorporate = "✓" if v_vir > v_crit else "✗"
    print(f"  {label}: {v_vir:.1f} km/s vs {v_crit:.1f} km/s {can_reincorporate}")

print()
print("Mass-dependent factors at z=0:")
for mass, label in zip(mass_bins, mass_labels):
    factor = calculate_mass_dependent_factor(mass, CRITICAL_MASS, MASS_EXPONENT, MIN_REINC_FACTOR)
    below_critical = "✓" if mass < CRITICAL_MASS else "✗"
    print(f"  {label}: {factor:.3f} (below critical: {below_critical})")

print()
print("Model comparison at z=0:")
for mass, label in zip(mass_bins, mass_labels):
    eff_mass_dep = calculate_reincorporation_efficiency(mass, 0.0, mass_dependent=True)
    eff_standard = calculate_reincorporation_efficiency(mass, 0.0, mass_dependent=False)
    diff_percent = ((eff_standard - eff_mass_dep) / eff_standard * 100) if eff_standard > 0 else 0
    print(f"  {label}: Mass-dep={eff_mass_dep:.3f}, Standard={eff_standard:.3f}, Difference={diff_percent:.1f}%")

print()
print("Parameter strength comparison across multiple test masses at z=0:")
print("=" * 80)

for test_mass, mass_label in zip(test_masses_multi, test_mass_labels_multi):
    eff_standard_test = calculate_reincorporation_efficiency(test_mass, 0.0, mass_dependent=False)
    print(f"\n{mass_label}:")
    print(f"  Standard (no mass-dep): {eff_standard_test:.3f}")
    
    for strength, params in param_sets.items():
        mass_factor = calculate_mass_dependent_factor(test_mass, 
                                                     params['crit_mass'], 
                                                     params['mass_exp'], 
                                                     params['min_factor'])
        eff_with_params = eff_standard_test * mass_factor
        suppression = (1 - mass_factor) * 100
        print(f"  {strength:12}: {eff_with_params:.3f} (factor={mass_factor:.3f}, suppression={suppression:.1f}%)")

print()
print("Line Style Guide for Panel 3:")
for mass_label, line_style, style_label in zip(test_mass_labels_multi, line_styles, line_style_labels):
    print(f"  {mass_label}: {style_label} lines ({line_style})")

print()
print("Color Guide for Panel 3:")
print("  Light Green: Weak suppression")
print("  Medium Green: Medium suppression") 
print("  Dark Green: Strong suppression")
print("  Darkest Green: Super-strong suppression")