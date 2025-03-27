# SAGE Documentation

## Structure

- `examples/` - Code examples demonstrating SAGE usage and features
- `implementation-guide-pipeline-integration.md` - Guide for integrating the pipeline system
- `integration-guide-phase2.5-2.6.md` - Guide for Phase 2.5-2.6 integration
- `integration-plan-phase3.md` - Planning document for Phase 3

## Project Status

Current implementation progress is tracked in:
- `/logs/phase-tracker-log.md` - Current phase and objectives
- `/logs/recent-progress-log.md` - Recent updates and milestones
- `/logs/project-state-log.md` - Core architecture state
- `/logs/decision-log.md` - Technical decisions

## Key Documentation

### Pipeline System

The pipeline system (Phase 2.5) allows configurable sequences of physics operations. See:
- `examples/pipeline_example.c` - Example usage of the pipeline system
- `implementation-guide-pipeline-integration.md` - Integration guide
- `/tests/test_pipeline.c` - Pipeline system validation test

### Configuration System

The configuration system (Phase 2.6) provides flexible parameter management. See:
- `/input/config.json` - Example configuration file
- `integration-guide-phase2.5-2.6.md` - Implementation details

## Module Implementation Status

The refactoring project is progressively implementing physics modules using the new plugin architecture. Current status:

### Implemented Modules
- **Cooling Module**: First physics module implemented with the new architecture (Phase 2.1)
  - Located in `src/core/module_cooling.c`
  - Integrated with the pipeline system

### Pending Modules
The following modules are currently handled by traditional implementations but will be migrated to the plugin architecture in future phases:
- **Infall**: Gas infall onto halos
- **Star Formation**: Conversion of gas to stars
- **Feedback**: Stellar feedback processes
- **AGN**: Active Galactic Nuclei processes
- **Disk Instability**: Galaxy disk stability calculations
- **Mergers**: Galaxy merger handling
- **Reincorporation**: Gas reincorporation processes
- **Misc**: Miscellaneous physics processes

During Phase 2.5-2.6, all pipeline steps are marked as optional to allow the system to function even with incomplete module implementation. The traditional implementation serves as a fallback when modules are not yet available.