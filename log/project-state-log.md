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
- `core_logging`: Enhanced error logging system with severity levels (DEBUG to FATAL), context-aware messaging, configurable output formatting, and standardized verbosity control
- `evolution_context`: Context structure for galaxy evolution to reduce global state
- `core_evolution_diagnostics`: Comprehensive metrics and statistics for galaxy evolution

```
evolution_context                enhanced_logging_system
    ┌────────────────┐            ┌────────────────────┐
    │ galaxies[]     │            │ logging_state      │
    │ ngal           │            │ - min_level        │
    │ centralgal     │<─────────▶│ - prefix_style     │
    │ halo properties│            │ - destinations     │
    │ params         │            │ - color_enabled    │
    │ diagnostics    │            │ - format_options   │
    └────────────────┘            │ - context_support  │
                                 └────────────────────┘
```

### Evolution Diagnostics
The evolution diagnostics system provides metrics and statistics for the galaxy evolution process:
- Phase timing and performance analysis
- Event occurrence tracking
- Merger statistics (detections, processing, types)
- Galaxy property changes during evolution
- Per-galaxy processing rates and overall performance

```
diagnostics_system
    ┌────────────────────────┐
    │ evolution_diagnostics  │
    │ - phase statistics     │
    │ - event counters       │<───┐
    │ - merger statistics    │    │
    │ - property tracking    │    │
    │ - performance metrics  │    │
    └────────────────────────┘    │
               ▲                  │
               │                  │
    ┌──────────┴───────────┐     │
    │ core_build_model.c   │     │
    │ - evolve_galaxies()  │     │
    └──────────────────────┘     │
                                 │
    ┌─────────────────────┐      │
    │ core_event_system.c │      │
    │ - event_dispatch()  │──────┘
    └─────────────────────┘
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

### Evolution Pipeline
The main galaxy evolution pipeline has been refactored for better modularity:
- `evolve_galaxies()`: Coordinates galaxy evolution using the evolution context
- Pipeline phases (HALO, GALAXY, POST, FINAL) for different scopes of calculation
- Comprehensive validation, logging, and diagnostics throughout the pipeline

```
Galaxy Evolution Pipeline
┌─────────────────────┐
│ construct_galaxies  │
└─────────┬───────────┘
          │
┌─────────▼───────────┐
│ join_galaxies_of_   │
│ progenitors         │
└─────────┬───────────┘
          │
┌─────────▼───────────┐
│ evolve_galaxies     │◄────────┐
└─────────┬───────────┘         │
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ HALO phase      ││
          │  └─────────────────┘│
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ GALAXY phase    ││
          │  └─────────────────┘│
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ POST phase      ││
          │  └─────────────────┘│
          │                     │
          │  ┌─────────────────┐│
          ├─▶│ FINAL phase     ││
          │  └─────────────────┘│
          │                     │
┌─────────▼───────────┐        │
│ evolution_          │        │
│ diagnostics_report  │────────┘
└─────────────────────┘
```

### Merger Event Queue
A fully implemented merger event queue provides controlled processing of mergers:
- Collects potential mergers during galaxy processing 
- Defers merger processing until appropriate pipeline phase
- Maintains scientific consistency with original merger implementation
- Tracks merger statistics through diagnostics

```
Merger Event Queue
    ┌─────────────────────┐
    │ merger_event_queue  │
    │ - events[]          │◄────────┐
    │ - num_events        │         │
    └─────────┬───────────┘         │
              │                     │
    ┌─────────▼───────────┐         │
    │ process_merger      │         │
    │ _events()           │         │
    └──────────────────────┘         │
                                   │
    ┌─────────────────────┐         │
    │ evolve_galaxies     │         │
    │ - merge detection   │─────────┘
    └─────────────────────┘
```

### I/O Components
I/O is now fully abstracted through a unified interface system:
- `io_interface`: Abstract interface with standardized operations (initialize, read_forest, write_galaxies, cleanup)
- Input handlers for multiple merger tree formats:
  - LHalo binary and HDF5
  - ConsistentTrees ASCII and HDF5
  - Gadget4 HDF5
  - Genesis HDF5
- Output handlers for binary and HDF5 formats
- Trees and galaxies managed through a consistent API
- Cross-platform endianness handling for binary formats
- Extended property serialization for module-specific data
- Comprehensive validation system for data integrity:
  - Data validation to prevent invalid values (NaN, Infinity)
  - Format capability validation to ensure required features
  - Property validation for extension compatibility
  - Integration with core I/O pipeline
- Memory optimization components:
  - `array_utils`: Provides geometric growth for dynamic arrays (reduced allocation frequency)
  - `io_buffer_manager`: Configurable buffered I/O with automatic resizing
  - `io_memory_map`: Cross-platform memory mapping for efficient file access
  - (Planned) Memory pooling for galaxy structure allocation

```
I/O System Architecture
┌─────────────────────┐      ┌─────────────────────┐
│ io_interface        │      │ core_save           │
│ - Capabilities      │◄────▶│ - init_galaxy_files │
│ - Format registry   │      │ - save_galaxies     │
│ - Resource tracking │      │ - finalize_files    │
└─────────┬───────────┘      └─────────────────────┘
          │                              ▲
          ▼                              │
┌─────────────────────┐      ┌──────────┴──────────┐
│ Format Handlers     │      │ io_validation       │
│ - Binary            │◄────▶│ - Data validation   │
│ - HDF5              │      │ - Format validation │
│ - Property support  │      │ - Property checks   │
└─────────────────────┘      └─────────────────────┘
```

### Testing Framework
A robust testing framework is in place:
- Automated test environment setup and dependency management
- Validation against reference Mini-Millennium outputs
- Support for both binary and HDF5 format testing
- Detailed comparison utility (sagediff.py) with configurable tolerances
- Comprehensive error reporting and diagnostics
- CI/CD integration with GitHub Actions
- Unit tests for core components including the evolution diagnostics system
- Integration tests for the refactored evolve_galaxies loop verifying phase transitions, event handling, and diagnostics

All changes are validated through this end-to-end testing framework using reference outputs from the Mini-Millennium simulation, ensuring scientific accuracy throughout the refactoring process.

## Modular Architecture
The codebase now features an integrated plugin architecture with three key components:

```
┌───────────────────┐      ┌───────────────────┐      ┌───────────────────┐
│ Module System     │      │ Pipeline System   │      │ Config System     │
│                   │      │                   │      │                   │
│ - Registry        │◄────▶│ - Step sequencing │◄────▶│ - JSON parsing    │
│ - Lifecycle mgmt  │      │ - Execution       │      │ - Param hierarchy │
│ - Extension data  │      │ - Event hooks     │      │ - Overrides       │
└─────────┬─────────┘      └─────────┬─────────┘      └─────────┬─────────┘
          │                          │                          │
          │                          ▼                          │
          │                ┌───────────────────┐                │
          └───────────────▶│ Parameters        │◀───────────────┘
                           │                   │
                           │ - Physics         │
                           │ - Cosmology       │
                           │ - Runtime         │
                           └───────────────────┘
```

Key components in the architecture:
- `core_module_system`: Defines module interface, registry, and lifecycle management
- `core_module_callback`: Enables cross-module function calls with dependency tracking
- `physics_pipeline_executor`: Integrates module callbacks with pipeline execution
- `module_cooling`: First physics module using new plugin architecture (others pending)
- `standard_infall_module`: Example module implementation using callback system
- `core_pipeline_system`: Configurable sequence for physics operations
- `core_config_system`: JSON-based configuration with hierarchical parameters
- `core_module_debug`: Module debugging with tracing and diagnostics
- `core_module_error`: Error handling with context tracking and history
- `core_dynamic_library`: Cross-platform dynamic library loading
- `core_evolution_diagnostics`: Comprehensive metrics for performance and scientific analysis

The modular design provides:
- Runtime physics module registry with lifecycle management
- Event-based communication between modules
- Galaxy property extension mechanism
- Controlled module-to-module function calls via callback system
- Call stack tracking and circular dependency detection
- Pipeline execution with phase-aware module invocation
- Parameter validation and override capabilities
- Comprehensive error handling and diagnostics
- Dynamic loading of module implementations
- Performance tracking and scientific validation through diagnostics

### Physics Modularization Strategy
The codebase is implementing the Properties Module architecture for physics modularization:

```
┌───────────────────────┐      ┌───────────────────────┐
│ Galaxy Properties     │      │ Extension Registry    │
│                       │      │                       │
│ - Direct fields       │◄────▶│ - Registered props    │
│ - Accessor functions  │      │ - Standard definitions│
└─────────┬─────────────┘      └─────────┬─────────────┘
          │                              │
          ▼                              ▼
┌───────────────────────┐      ┌───────────────────────┐
│ Legacy Implementation │      │ Module Implementation │
│                       │      │                       │
│ - Direct field access │      │ - Extension-based     │
│ - Traditional code    │      │ - Plugin architecture │
└───────────────────────┘      └───────────────────────┘
```

This approach allows gradual migration of physics modules while maintaining scientific consistency. Each physics domain defines standard extension properties that can be accessed either directly (for backward compatibility) or through the extension mechanism (for modularity). Configuration options control which implementation is used, enabling incremental validation and testing.

Key components of this architecture:
- Centralized property definitions in properties.yaml (single source of truth)
- Auto-generated typesafe accessors (GALAXY_PROP_* macros)
- Pipeline-based execution with phase support (HALO, GALAXY, POST, FINAL)
- Module dependency management through pipeline phases and event system
- Physics-agnostic pipeline context with generic data sharing
- Event-based inter-module communication

The analysis of module dependency management for property-based interactions concluded that no significant revisions are needed to the current system. The combination of centralized property definitions, pipeline phases, and the event system provides sufficient structure for module interactions while maintaining clean separation between core infrastructure and physics implementations.