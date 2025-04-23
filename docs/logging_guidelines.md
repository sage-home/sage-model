# SAGE Logging Guidelines

## Log Level Usage

- **TRACE**: Detailed function entry/exit, variable values - only in debug builds
- **DEBUG**: Information useful during development/debugging but not in production
- **INFO**: Normal operation progress, startup/shutdown events
- **NOTICE**: Important events that aren't problems (e.g., significant phase completions)
- **WARNING**: Concerning but non-fatal issues (e.g., using fallback values)
- **ERROR**: Errors that prevent a specific operation but allow continued execution
- **CRITICAL**: Errors that prevent further execution of the simulation

## Module-Specific Logging

Use `MODULE_LOG()` for module-specific entries:

```c
MODULE_LOG("cooling", LOG_WARNING, "Cooling rate calculation failed for galaxy %d, using default", gal->index);
```

## Recommendations for Specific Components

### Module System
- **DEBUG**: Module registration, dependency resolution
- **INFO**: Pipeline execution steps
- **WARNING**: Missing optional dependencies
- **ERROR**: Failed function lookups

### Physics Modules
- **DEBUG**: Calculation details, parameter values
- **INFO**: Phase execution start/completion
- **WARNING**: Using fallback physics
- **ERROR**: Physics calculation failures

### I/O Subsystem
- **INFO**: File operations start/completion
- **WARNING**: Missing optional data
- **ERROR**: File access failures

## Evolution Diagnostics Integration

The logging system will be directly connected to the evolution diagnostics system in Phase 5.1.7, providing:

1. Runtime visibility into simulation progress
2. Issue detection during long simulation runs
3. Performance tracking for optimization phases
