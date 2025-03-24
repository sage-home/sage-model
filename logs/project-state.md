<!-- Purpose: Snapshot of current codebase architecture -->
<!-- Update Rules: 
- 750-word limit! 
- Overwrite outdated content 
- Focus on active components 
- Use UML-like text diagrams
-->

# SAGE Current Architecture

## Directory Structure
```
sage-model/
├── src/
│   ├── core/         # Core infrastructure components
│   ├── physics/      # Physics modules implementing galaxy evolution processes 
│   ├── io/           # Input/output handlers for different file formats
│   └── auxdata/      # Auxiliary data files (cooling tables, etc.)
├── tests/            # Comprehensive testing framework with end-to-end tests
│   ├── test_data/    # Test merger trees and reference outputs
│   └── test_sage.sh  # Main test script with environment setup and comparisons
├── input/            # Input parameter files
├── logs/             # Project tracking and documentation
└── plotting/         # Analysis and visualization tools
```

## Component Organization

### Core Components
The core infrastructure has been refactored to separate concerns and reduce global state:
- `core_allvars.h`: Primary data structures organized into logical groups
- `core_parameter_views`: Module-specific parameter views for cleaner interfaces
- `core_init`: Initialization and cleanup routines with GSL dependency removed
- `core_logging`: Enhanced error logging system with severity levels (DEBUG to FATAL), context-aware messaging, and configurable output formatting
- `evolution_context`: Context structure for galaxy evolution to reduce global state

```
evolution_context                enhanced_logging_system
    ┌────────────────┐            ┌────────────────────┐
    │ galaxies[]     │            │ logging_state      │
    │ ngal           │            │ - min_level        │
    │ centralgal     │<─────────▶│ - prefix_style     │
    │ halo properties│            │ - destinations     │
    │ params         │            │ - color_enabled    │
    └────────────────┘            │ - format_options   │
                                 │ - context_support  │
                                 └────────────────────┘
```

### Physics Modules
Physics processes are organized into separate modules, each with a targeted parameter view:
- `model_cooling_heating.c`: Gas cooling and AGN heating
- `model_starformation_and_feedback.c`: Star formation and feedback
- `model_infall.c`: Gas infall onto halos
- `model_mergers.c`: Galaxy merger handling
- `model_disk_instability.c`: Disk instability calculations
- `model_reincorporation.c`: Gas reincorporation

```
Physics Module
    ┌───────────────────┐
    │ *_params_view     │──────┐
    └───────────────────┘      │
                               │
    ┌───────────────────┐      │
    │ core functions    │      │
    └───────────────────┘      │
                               │
                       ┌───────────────────┐
                       │ struct params     │
                       │  ┌─────────────┐  │
                       │  │ cosmology   │  │
                       │  │ physics     │  │
                       │  │ io          │  │
                       │  │ units       │  │
                       │  │ simulation  │  │
                       │  │ runtime     │  │
                       │  └─────────────┘  │
                       └───────────────────┘
```

### I/O Components
I/O is abstracted for different input and output formats:
- Input handlers for multiple merger tree formats:
  - LHalo binary and HDF5
  - ConsistentTrees ASCII and HDF5
  - Gadget4 HDF5
  - Genesis HDF5
- Output handlers for binary and HDF5 formats
- Trees and galaxies managed through a consistent API

### Testing Framework
A robust testing framework is in place:
- Automated test environment setup and dependency management
- Validation against reference Mini-Millennium outputs
- Support for both binary and HDF5 format testing
- Detailed comparison utility (sagediff.py) with configurable tolerances
- Comprehensive error reporting and diagnostics
- CI/CD integration with GitHub Actions

All changes are validated through this end-to-end testing framework using reference outputs from the Mini-Millennium simulation, ensuring scientific accuracy throughout the refactoring process.
