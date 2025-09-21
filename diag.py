import numpy as np
import matplotlib.pyplot as plt
import pandas as pd

# Read your diagnostic files
feedback = pd.read_csv('feedback_efficiency_by_mass.txt', sep='\s+', comment='#')
sf_eff = pd.read_csv('sf_efficiency_cgm_regime.txt', sep='\s+', comment='#')

# Analyze regime distribution
cgm_galaxies = feedback[feedback['Regime'] == 0]
hot_galaxies = feedback[feedback['Regime'] == 1]

print(f"CGM regime galaxies: {len(cgm_galaxies)}")
print(f"HOT regime galaxies: {len(hot_galaxies)}")
print(f"CGM fraction: {len(cgm_galaxies)/(len(cgm_galaxies)+len(hot_galaxies)):.3f}")

# Plot mass loading vs Vvir
plt.figure(figsize=(10,6))
plt.scatter(cgm_galaxies['Vvir(km/s)'], cgm_galaxies['mass_loading'], 
           alpha=0.6, label='CGM regime')
plt.scatter(hot_galaxies['Vvir(km/s)'], hot_galaxies['mass_loading'], 
           alpha=0.6, label='HOT regime')
plt.axvline(117, color='red', linestyle='--', label='Proposed threshold (5e5 K)')
plt.axvline(148, color='orange', linestyle='--', label='Current threshold (8e5 K)')
plt.xlabel('Vvir (km/s)')
plt.ylabel('Mass loading factor')
plt.legend()
plt.title('Feedback Efficiency vs Velocity')
plt.show()

# Identify problematic mass range
problematic = cgm_galaxies[(cgm_galaxies['Mvir(Msun)'] > 10) & 
                          (cgm_galaxies['Mvir(Msun)'] < 100)]
print(f"Problematic CGM galaxies (10-100 Msun): {len(problematic)}")