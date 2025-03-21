<!-- Purpose: Snapshot of current codebase architecture -->
<!-- Update Rules: 
- 500-word limit 
- Overwrite previous and outdated content 
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
├── tests/            # Testing framework and end-to-end tests
└── docs/             # Project documentation
```

## Component Organization

### Core Components
The core infrastructure has been refactored to separate concerns and reduce global state:
- `core_allvars.h`: Primary data structures organized into logical groups
- `core_parameter_views`: Module-specific parameter views for cleaner interfaces
- `core_init`: Initialization and cleanup routines with GSL dependency removed
- `evolution_context`: Context structure for galaxy evolution to reduce global state

```
evolution_context
    ┌────────────────┐
    │ galaxies[]     │
    │ ngal           │
    │ centralgal     │
    │ halo properties│
    │ params         │
    └────────────────┘
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
- Input handlers for different merger tree formats
- Output handlers for binary and HDF5 formats
- Trees and galaxies managed through a consistent API

## Next Steps in Refactoring
The codebase is transitioning toward a more modular architecture:
1. Completed: Code organization, global state reduction, GSL dependency removal
2. In progress: Testing framework implementation, documentation
3. Upcoming: Module interface definitions, property extension mechanism, event system

All changes are being validated with comprehensive end-to-end testing using reference outputs from the Mini-Millennium simulation. This ensures scientific accuracy is preserved throughout the refactoring process.
