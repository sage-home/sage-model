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