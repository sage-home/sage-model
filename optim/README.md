# SAGE Galaxy Formation Model - Particle Swarm Optimization

This project performs Particle Swarm Optimization (PSO) on the SAGE semi-analytic galaxy formation model to calibrate its free parameters against observational constraints.

## Requirements

- Python 3.x
- Pyswarm
- NumPy
- Scipy
- Pandas
- re
- Matplotlib
- Seaborn
- h5py
- glob
- openmp
- openmpi
- gcc

## Usage

1. Set up the configuration options in `run_pso.sh` or `run_pso_allredshifts.sh`:
   - `CONFIG_PATH`: Path to the SAGE configuration file
   - `BASE_PATH`: Path to the SAGE executable
   - `OUTPUT_PATH`: Path to store the output files
   - `SNAPSHOT`: Snapshot number to analyze
   - `PARTICLES`: Number of particles in the swarm
   - `ITERATIONS`: Maximum number of iterations for the optimization
   - `TEST`: Statistical test to use for evaluating the fit (e.g., "student-t")
   - `CONSTRAINTS`: Comma-separated list of observational constraints to use (e.g., "SMF_z0")
   - `CSVOUTPUT`: Path to save the PSO results as a CSV file

   - `AGE_ALIST_FILE_MINI_UCHUU`: Path to scale factor list for miniUchuu
   - `AGE_ALIST_FILE_MINI_MILLENNIUM`: Path to scale factor list for Millennium
   - `BOXSIZE`: Set to the box size of simulation
   - `SIM_MINI_UCHUU`: Selects for simulation type, 0 for miniUchuu
   - `SIM_MINI_MILLENNIUM`: Selects for simulation type, 1 for Millennium
   - `VOL_FRAC`: Volume fraction of simulation, this is determined by - number of tree files used / total number of tree files
   - `OMEGA0`: Omega_0 of simulation
   - `H0`: h_0 of simulation


2. Modify `space.txt` to define the search space for the PSO. Each line should contain:
   - Parameter name
   - Plot label
   - Logarithmic flag (0 for linear, 1 for logarithmic)
   - Lower bound
   - Upper bound

3. Run the optimization:
   ```bash
   ./run_pso.sh

   or

   ./run_pso_allredshifts.sh

4. Monitor the progress:
smf_sage.png is generated for each particle, showing the stellar mass function at the specified redshift.
Check the console output for the current iteration and fitness values.


5. Analyze the results:
The best-fit parameters and their fitness values are saved in the specified CSV file.
Diagnostic plots are generated in the output directory.

6. Observational Constraints
Currently, the following observational constraints are available:

Stellar Mass Function (SMF) at various redshifts:

SMF_z0: z = 0
SMF_z0.2 to SMF_z10.4: Higher redshifts


Black Hole Mass Function (BHMF) at z = 0
Black Hole - Bulge Mass Relation (BHBM) at z = 0

7. HPC Functionality
To enable running on an HPC cluster, use the -H flag and provide the following options:

-C: Number of CPUs per SAGE instance
-M: Memory needed by each SAGE instance
-w: Walltime for each submission

8. Notes

The PSO algorithm is implemented in pso.py.
Observational data and utility functions are defined in routines.py.
constraints.py contains the classes for each observational constraint.
analysis.py provides functions for quantifying the goodness of fit.
execution.py handles the execution of SAGE and evaluation of particles.

Diagnostics are automatically performed, producing mamy plots for data analysis. This can also be performed on its own

9. Cleanup
After running the PSO optimization, cleanup is automatically performed.
