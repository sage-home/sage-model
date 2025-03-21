# SAGE-Model Implementation Context

This document provides essential context for implementing the refactoring plan for the SAGE (Semi-Analytic Galaxy Evolution) model. It includes information about the codebase architecture, domain-specific context, testing procedures, configuration system, and development environment setup.

## 1. Codebase Architecture Overview

### Core Components and Flow

SAGE is a semi-analytic model of galaxy formation that processes dark matter merger trees to simulate galaxy evolution. The high-level execution flow is:

1. **Parameter Reading**: Parse configuration from parameter file (`core_read_parameter_file.c`)
2. **Initialization**: Set up units, cooling tables, etc. (`core_init.c`)
3. **Forest Distribution**: Distribute merger tree forests across MPI processes (`sage.c`)
4. **Forest Processing**: Process each forest to construct and evolve galaxies (`sage_per_forest()` in `sage.c`)
5. **Galaxy Construction**: Build galaxy populations from merger trees (`core_build_model.c`)
6. **Physics Application**: Apply astrophysical processes to galaxies (various `model_*.c` files)
7. **Output Generation**: Save results to disk (`core_save.c`, `io/save_gals_*.c`)

### Key Data Structures

#### 1. Merger Tree Structures

```c
// Dark matter halo structure - represents a node in the merger tree
struct halo_data {
    // Halo properties
    float Pos[3];      // Position in comoving coordinates [Mpc/h]
    float Vel[3];      // Velocity [km/s]
    float Mvir;        // Virial mass [10^10 Msun/h]
    float Rvir;        // Virial radius [Mpc/h]
    float Vvir;        // Virial velocity [km/s]
    float Vmax;        // Maximum circular velocity [km/s]
    int Len;           // Number of particles in halo
    
    // Tree connectivity
    int SnapNum;                // Snapshot number (time step)
    int FofHalo;                // FOF group this halo belongs to
    long long MostBoundID;      // ID of most bound particle
    int FileNr;                 // File number in the tree
    int FirstProgenitor;        // Index of first progenitor
    int NextProgenitor;         // Index of next progenitor
    int FirstHaloInFOFgroup;    // Index of first halo in FOF group
    int NextHaloInFOFgroup;     // Index of next halo in FOF group
    int Descendant;             // Index of descendant halo
};

// Auxiliary data for halo processing
struct halo_aux_data {
    int DoneFlag;       // Whether this halo has been processed
    int HaloFlag;       // Special flags for this halo
    int NGalaxies;      // Number of galaxies in this halo
    int FirstGalaxy;    // Index of first galaxy in this halo
};
```

#### 2. Galaxy Structure

```c
// Galaxy properties structure
struct GALAXY {
    // Identification and tree position
    int SnapNum;              // Snapshot number
    int Type;                 // Galaxy type (0=central, 1=satellite)
    int HaloNr;               // Halo number in the tree
    int GalaxyNr;             // Unique galaxy ID
    int CentralGal;           // Index of central galaxy
    long long MostBoundID;    // Most bound particle ID
    uint64_t GalaxyIndex;     // Globally unique galaxy ID
    uint64_t CentralGalaxyIndex; // Galaxy Index of the central galaxy
    
    // Physical properties
    float Pos[3];             // Position [comoving Mpc/h]
    float Vel[3];             // Velocity [km/s]
    float Mvir;               // Virial mass [10^10 Msun/h]
    float Rvir;               // Virial radius [Mpc/h]
    float Vvir;               // Virial velocity [km/s]
    
    // Baryonic components
    float StellarMass;        // Stellar mass [10^10 Msun/h]
    float ColdGas;            // Cold gas mass [10^10 Msun/h]
    float BulgeMass;          // Bulge mass [10^10 Msun/h]
    float HotGas;             // Hot gas mass [10^10 Msun/h]
    float BlackHoleMass;      // Black hole mass [10^10 Msun/h]
    float EjectedMass;        // Ejected gas mass [10^10 Msun/h]
    float ICS;                // Intracluster stars [10^10 Msun/h]
    
    // Metal contents of each component
    float MetalsStellarMass;  // Metals in stars [10^10 Msun/h]
    float MetalsColdGas;      // Metals in cold gas [10^10 Msun/h]
    // ... other metal components
    
    // Star formation history
    float SfrDisk[STEPS];     // Disk star formation rate history
    float SfrBulge[STEPS];    // Bulge star formation rate history
    
    // Structural properties
    float DiskScaleRadius;    // Disk scale length [Mpc/h]
    
    // Merger information
    int mergeType;            // Merger type flag
    int mergeIntoID;          // Target galaxy for mergers
    int mergeIntoSnapNum;     // Snapshot of merger
    float MergTime;           // Time until merger [Gyr]
    
    // Cooling and heating
    double Cooling;           // Cooling rate [erg/s]
    double Heating;           // Heating rate [erg/s]
};
```

#### 3. Galaxy Trees

Similar to halos, galaxies in SAGE can be organized into trees with their own unique identification:

- **GalaxyIndex**: Calculated in generate_galaxy_indices()
- **Galaxy Tree Pointers**: Follows the host halo pointers

These pointers allow the galaxy merger tree to be walked forwards and backwards, similar to the halo merger tree.

#### 4. Runtime Parameters

```c
// Runtime parameters structure
struct params {
    // Input/output parameters
    char OutputDir[MAX_STRING_LEN];            // Output directory
    char FileNameGalaxies[MAX_STRING_LEN];     // Base filename for outputs
    char TreeName[MAX_STRING_LEN];             // Base name for tree files
    enum Valid_TreeTypes TreeType;             // Format of merger trees
    enum Valid_OutputFormats OutputFormat;     // Format for output files
    
    // Cosmological parameters
    double Omega;                              // Matter density parameter
    double OmegaLambda;                        // Dark energy density parameter
    double Hubble_h;                           // Hubble parameter (H0/100)
    double PartMass;                           // Particle mass [10^10 Msun/h]
    double BoxSize;                            // Simulation box size [Mpc/h]
    
    // Physics model flags
    int ReionizationOn;                        // Enable reionization
    int SupernovaRecipeOn;                     // Enable supernova feedback
    int DiskInstabilityOn;                     // Enable disk instability
    int AGNrecipeOn;                           // Enable AGN feedback
    int SFprescription;                        // Star formation model
    
    // Physics model parameters
    double RecycleFraction;                    // Gas recycling fraction
    double Yield;                              // Metal yield from stars
    double SfrEfficiency;                      // Star formation efficiency
    double FeedbackReheatingEpsilon;           // SN reheating efficiency
    double BlackHoleGrowthRate;                // BH accretion rate
    // ... many other physics parameters
    
    // Numerical parameters
    int32_t NumSnapOutputs;                    // Number of output snapshots
    int32_t ListOutputSnaps[ABSOLUTEMAXSNAPS]; // List of output snapshots
    enum Valid_Forest_Distribution_Schemes ForestDistributionScheme; // MPI load balancing
    
    // Runtime state
    int32_t ThisTask;                          // MPI task ID
    int32_t NTasks;                            // Total MPI tasks
};
```

### Component Relationships

1. **Forest Processing** (`sage.c`) - The core loop processes each forest, first loading it into memory, then constructing galaxies, and finally saving the results.

2. **Galaxy Construction** (`core_build_model.c`) - Traverses the merger tree to create galaxies at each branch, apply physical processes, and track galaxy evolution through time.

3. **Physical Processes** (`model_*.c`) - Apply physical models to galaxies at each time step, including star formation, feedback, cooling, and mergers.

4. **I/O System** (`io/*.c`) - Handles reading merger trees and writing galaxy catalogs in various formats (binary, HDF5).

5. **Memory Management** (`core_mymalloc.c`) - Provides allocation tracking and management for the codebase.

## 2. Domain-Specific Context

### Merger Trees and Semi-Analytic Modeling

#### Merger Tree Concept

A merger tree represents the hierarchical growth of dark matter halos through cosmic time:

- Each **node** is a dark matter halo at a specific snapshot (time)
- **Connections** between nodes represent progenitor-descendant relationships
- **Main branches** follow the most massive progenitor lineage
- **Sub-branches** represent smaller halos merging into larger ones

The Millennium Simulation format uses a depth-first ordering of halos where:
- Halos are stored in a flat array
- Connectivity is maintained through indices in this array (FirstProgenitor, NextProgenitor, etc.)
- The most massive progenitor lineage is always followed first

```
Example tree structure:
                                  [Halo 0]
                                 /        \
                        [Halo 1]            [Halo 4]
                       /        \                \
              [Halo 2]            [Halo 3]        [Halo 5]

In depth-first order: [0,1,2,3,4,5]
```

#### Depth-First Tree Structure

SAGE uses a specific organization of merger trees where halos are stored in depth-first order:

- **Unique ID Generation**: For the Millennium simulation, halos are assigned a global unique ID:
  `HaloID = FileNr * 10^12 + TreeNr * 10^6 + DepthFirstHaloIndex`
  But this may be different for other simulations.

- **Depth-First Ordering**: The DepthFirstHaloIndex is assigned by walking the tree in depth-first order, always following the most massive progenitor first.

- **Tree Pointers**: Each halo contains specific pointers that define the tree structure:
  - `FirstProgenitor`: Points to the first (most massive) progenitor
  - `NextProgenitor`: Points to the next progenitor of the same descendant
  - `Descendant`: Points to the halo this one will evolve into
  - `FirstHaloInFOFGroup`: Points to the first subhalo in the FOF group
  - `NextHaloInFOFGroup`: Points to the next subhalo in the same FOF group
  - `LastProgenitor`: Used to define a range of all progenitors

- **Progenitor Range**: The set of all progenitors of a halo are those with HaloIDs in the range from FirstProgenitor to LastProgenitor, allowing efficient traversal.

#### Semi-Analytic Modeling Principles

SAGE implements a semi-analytic model (SAM) which:

1. Uses pre-computed dark matter merger trees as the backbone
2. Applies analytical models of baryonic physics to predict galaxy properties
3. Tracks baryonic components (stars, gas, black holes) as galaxies evolve
4. Implements key physical processes:
   - Gas cooling from hot halos into cold disks
   - Star formation from cold gas
   - Supernova feedback heating and ejecting gas
   - AGN feedback regulating cooling
   - Galaxy mergers building bulges and ellipticals
   - Chemical enrichment tracking metallicity evolution

This approach combines computational efficiency with physical accuracy, allowing the simulation of large galaxy populations.

### Key Physics Concepts

#### Star Formation and Feedback

```c
// In model_starformation_and_feedback.c
void starformation_and_feedback(...) {
    // Star formation follows Kauffmann (1996)
    // SFR = ε * (M_cold - M_crit) / t_dyn
    // where:
    // - ε is the star formation efficiency
    // - M_cold is the cold gas mass
    // - M_crit is the critical mass for star formation
    // - t_dyn is the dynamical time
    
    // Supernova feedback:
    // - Reheated gas: ΔM_reheat = ε_reheat * ΔM_*
    // - Ejected gas: ΔM_eject = (ε_eject * (ESN/Vvir^2) - ε_reheat) * ΔM_*
    // where:
    // - ΔM_* is the mass of newly formed stars
    // - ε_reheat and ε_eject are efficiency parameters
    // - ESN is the energy released per supernova
    // - Vvir is the virial velocity of the halo
}
```

#### Cooling and Heating

```c
// In model_cooling_heating.c
void cooling_recipe(...) {
    // Cooling follows White & Frenk (1991)
    // Uses a cooling function Λ(T,Z) dependent on temperature and metallicity
    // Cooling rate: dM/dt = 0.5 * M_hot/t_cool * (t_cool/t_dyn)
    // where:
    // - M_hot is the hot gas mass
    // - t_cool is the cooling time
    // - t_dyn is the dynamical time
    
    // AGN heating counteracts cooling:
    // Heating rate ∝ M_BH * M_hot
    // where:
    // - M_BH is the black hole mass
}
```

#### Galaxy Mergers

```c
// In model_mergers.c
void deal_with_galaxy_merger(...) {
    // When dark matter halos merge:
    // 1. The central galaxy of the larger halo remains the central
    // 2. The central galaxy of the smaller halo becomes a satellite
    // 3. Satellites merge with the central on a dynamical friction timescale
    
    // Major mergers (mass ratio > threshold):
    // - Destroy disks and create spheroids
    // - Trigger starbursts and black hole growth
    
    // Minor mergers:
    // - Add stars to bulge
    // - Trigger milder starbursts
}
```

## 3. Testing and Validation Procedures

### Building and Running Tests

The SAGE test suite uses a set of mini-Millennium merger trees to validate functionality:

```bash
# Build with testing enabled
make tests

# Run the test suite
./tests/test_sage.sh
```

The test suite:
1. Downloads mini-Millennium merger trees if not present
2. Runs SAGE on these trees
3. Compares outputs against reference results
4. Validates both binary and HDF5 output formats

### Validating Optimizations

To validate that optimizations don't affect scientific results:

1. **Bit-level Comparison**: For changes that shouldn't affect results

   ```bash
   # Generate reference output
   ./sage input/millennium.par
   mv output/test_sage_z* output/reference/
   
   # Run with optimization
   ./sage_optimized input/millennium.par
   
   # Compare outputs
   python tests/sagediff.py output/reference/test_sage_z0 output/test_sage_z0 binary-binary 1 1
   ```

2. **Statistical Validation**: For changes that may cause small numerical differences

   ```bash
   # Generate statistical properties from outputs
   python tools/analyze_stats.py output/reference/test_sage_z0 > stats_reference.txt
   python tools/analyze_stats.py output/test_sage_z0 > stats_new.txt
   
   # Compare statistical properties
   python tools/compare_stats.py stats_reference.txt stats_new.txt
   ```

3. **Performance Measurement**:

   ```bash
   # Time execution
   time ./sage input/millennium.par
   
   # Memory usage (Linux)
   /usr/bin/time -v ./sage input/millennium.par
   
   # Memory usage (macOS)
   /usr/bin/time -l ./sage input/millennium.par
   ```

### Expected Validation Metrics

Key metrics to check:
- Galaxy stellar mass function within 1% of reference
- Star formation rate density within 1% of reference
- Black hole-bulge mass relation with same slope and normalization
- Conservation of total baryon mass
- Galaxy merger rates matching reference

## 4. Configuration System Details

### Parameter File Structure

SAGE uses a text-based parameter file with format:

```
ParameterName    ParameterValue    # Optional comment
```

Example parameters:
```
FileNameGalaxies      SA
OutputDir             ./output/
FirstFile             0
LastFile              4
NumOutputs            8
-> 7 6 5 4 3 2 1 0    # Specific output snapshots

SfrEfficiency         0.03
FeedbackReheatingEpsilon  2.0
```

### Reading Parameters in Code

The parameter system is initialized in `core_read_parameter_file.c`:

```c
// Read a parameter file into the run_params structure
int read_parameter_file(const char *fname, struct params *run_params);
```

Adding new parameters requires:
1. Adding the field to `struct params` in `core_allvars.h`
2. Adding parameter reading code in `read_parameter_file()`
3. Setting default values if appropriate

Example of accessing parameters:
```c
// Using a parameter value
double star_formation_rate = run_params->SfrEfficiency * (cold_gas - cold_crit) / t_dyn;

// Using a flag parameter
if(run_params->SupernovaRecipeOn) {
    // Apply supernova feedback
}
```

### Adding New Configuration Options

To add a new parameter:

1. Add to `struct params` in `core_allvars.h`:
   ```c
   struct params {
       // Existing parameters...
       
       // New parameter
       double NewParameter;
   };
   ```

2. Add to parameter reading in `core_read_parameter_file.c`:
   ```c
   strncpy(ParamTag[NParam], "NewParameter", MAXTAGLEN);
   ParamAddr[NParam] = &(run_params->NewParameter);
   ParamID[NParam++] = DOUBLE;
   ```

3. Use the parameter in code:
   ```c
   if(galaxy->StellarMass > run_params->NewParameter) {
       // Do something
   }
   ```

## 5. Development Environment Setup

### System Requirements

- C compiler (GCC or Clang)
- GNU Make
- Optional: MPI for parallel processing
- Optional: HDF5 for HDF5 file support
- Optional: GSL for additional numerical functions
- Python 3.6+ (for analysis tools)

### Setting Up Dependencies

#### Ubuntu/Debian:
```bash
# Basic build tools
sudo apt-get install build-essential git

# Optional dependencies
sudo apt-get install libhdf5-dev libgsl-dev libopenmpi-dev python3-dev python3-pip

# For analysis tools
pip3 install numpy matplotlib h5py
```

#### macOS:
```bash
# Using Homebrew
brew install gcc make git

# Optional dependencies
brew install hdf5 gsl open-mpi python

# For analysis tools
pip3 install numpy matplotlib h5py
```

### Compiling SAGE

Basic compilation:
```bash
# Clone the repository
git clone https://github.com/sage-home/sage-model.git
cd sage-model

# Standard build (no HDF5 or MPI)
make

# Build with HDF5 support
make USE-HDF5=yes

# Build with MPI support
make USE-MPI=yes

# Build with both HDF5 and MPI
make USE-HDF5=yes USE-MPI=yes
```

For development with memory checking:
```bash
make MEM-CHECK=yes
```

### Running SAGE

Basic execution:
```bash
# Run with a parameter file
./sage input/millennium.par
```

With MPI:
```bash
# Run with 4 MPI processes
mpirun -np 4 ./sage input/millennium.par
```

### Debugging Tools

GDB (GNU Debugger):
```bash
# Compile with debug symbols
make OPTIMIZE="-O0 -g"

# Run in debugger
gdb --args ./sage input/millennium.par
```

Valgrind memory checking:
```bash
# Check for memory leaks
valgrind --leak-check=full ./sage input/millennium.par
```

## 6. Implementation Guidelines

### Code Style

The SAGE codebase follows these general style rules:

- **Indentation**: 4 spaces (no tabs)
- **Braces**: Opening brace on same line, closing brace on new line
- **Function Names**: lowercase with underscores (`calculate_cooling_rate`)
- **Variable Names**: camelCase for most variables, ALL_CAPS for constants
- **Comments**: 
  - `//` for single line comments
  - `/* */` for multi-line comments
  - Function headers with descriptions of inputs/outputs

### Memory Management

SAGE uses a custom memory management system:

```c
// Allocation
void *mymalloc(size_t size);

// Free memory
void myfree(void *ptr);

// Reallocation
void *myrealloc(void *ptr, size_t size);
```

Always use these functions instead of standard `malloc`/`free` to maintain tracking and alignment.

### Error Handling

SAGE uses a mix of error handling approaches:

```c
// Return error codes
if(error_condition) {
    return ERROR_CODE;
}

// Error macros
XRETURN(condition, error_code, "Error message: %s", variable);

// Fatal errors
if(fatal_condition) {
    fprintf(stderr, "Fatal error: %s\n", error_message);
    ABORT(EXIT_FAILURE);
}
```

New code should follow the pattern of the surrounding code for consistency.

### I/O Operations

SAGE tried to follow these guidelines for I/O operations, which will be modernised as part of our refactoring plan:

- Use buffered I/O for efficiency with large files
- Check return values from all I/O operations
- Handle platform-specific differences in I/O functions
- Free resources in error cases to avoid leaks

### Recursive Tree Processing

SAGE processes merger trees recursively, following a depth-first traversal:

```c
construct_galaxies(HaloID) {
    // First process all progenitors recursively
    for(prog = HaloID.FirstProgenitor; prog >= 0; prog = Halos[prog].NextProgenitor)
        construct_galaxies(prog);
    
    // Join galaxies from progenitors
    join_galaxies_of_progenitors();
    
    // Evolve the joined galaxies to the present time
    evolve_galaxies_to_present_time();
}
```

An important consideration is that the evolution of galaxies in a subhalo does not depend only on its direct progenitors. Galaxies within the same FOF group influence each other, which requires ensuring all subhalos of a FOF halo are in the same tree.

In the SAGE implementation, each subhalo needs a flag to track whether its galaxies have already been processed to prevent duplicate processing.

### Best Practices for Optimizing

When implementing optimizations:

1. **Measure First**: Always establish a baseline before optimizing
2. **Preserve Functionality**: Ensure scientific results remain valid
3. **Isolate Changes**: Keep changes focused and testable
4. **Document Intent**: Add comments explaining optimization strategies
5. **Fallback Options**: Provide fallback mechanisms for complex optimizations

## Domain-Specific Terminology

| Term | Description |
|------|-------------|
| **Merger Tree** | Hierarchical structure showing dark matter halo formation history |
| **Halo** | Self-bound concentration of dark matter |
| **Subhalo** | Smaller halo within a larger host halo |
| **FOF Group** | Friends-of-Friends group; identifies connected structures |
| **Central Galaxy** | Main galaxy at the center of a dark matter halo |
| **Satellite Galaxy** | Secondary galaxy orbiting within a halo |
| **Progenitor** | Predecessor halo that merged to form a descendant |
| **Descendant** | Result of one or more halos merging |
| **Semi-Analytic Model** | Model using analytical approximations for galaxy evolution |
| **Stellar Mass Function** | Distribution of galaxy masses in a population |
| **AGN Feedback** | Energy released by active galactic nuclei affecting gas cooling |
| **Dynamical Friction** | Process causing satellite galaxies to lose orbital energy |
| **Infall** | Process of gas accreting onto galaxies |
| **Depth-First Order** | Tree traversal method that explores branches completely before backtracking |
| **FirstProgenitor** | Pointer to the most massive progenitor of a halo |
| **LastProgenitor** | Used with FirstProgenitor to define a range of all progenitors |

This context should provide sufficient background for implementing any phase of the refactoring plan in isolation.
