# SAGE Logging Guidelines

## Overview

SAGE uses a simplified two-level logging system with runtime control for efficient debugging and monitoring.

## Log Levels

The SAGE logging system supports the following levels:

- **DEBUG**: Detailed information useful during development and debugging
- **INFO**: General information about program execution and status
- **WARNING**: Concerning but non-fatal issues that may affect results
- **ERROR**: Errors that prevent specific operations but allow continued execution

## Runtime Modes

SAGE supports two runtime logging modes:

### Normal Mode (Default)
Shows **WARNING** and **ERROR** messages only.
```bash
./sage parameter_file.par
```

### Verbose Mode
Shows **DEBUG**, **INFO**, **WARNING**, and **ERROR** messages.
```bash
./sage --verbose parameter_file.par
# or
./sage -v parameter_file.par
```

## Usage Guidelines

### When to Use Each Level

#### DEBUG Level
- Detailed function entry/exit information
- Variable values and calculation details
- Internal state changes during development

#### INFO Level  
- Normal operation progress updates
- Startup and shutdown events
- Configuration and module loading information
- Phase execution start/completion

#### WARNING Level
- Using fallback values or default parameters
- Missing optional data or files
- Performance concerns or suboptimal conditions
- Non-critical configuration issues

#### ERROR Level
- File access failures
- Memory allocation failures
- Physics calculation failures
- Module loading errors
- Any condition that prevents normal operation

## Implementation Examples

### Basic Logging
```c
#include "core_logging.h"

// Simple logging
LOG_DEBUG("Starting calculation for galaxy %d", galaxy_id);
LOG_INFO("Loaded %d galaxies from file", count);
LOG_WARNING("Using default cooling rate for galaxy %d", galaxy_id);
LOG_ERROR("Failed to allocate memory for %d galaxies", count);
```

### Module-Specific Logging
```c
// For physics modules
MODULE_LOG("cooling", LOG_WARNING, "Cooling rate calculation failed for galaxy %d, using default", galaxy_id);
MODULE_LOG("star_formation", LOG_INFO, "Processing %d galaxies in current step", galaxy_count);
```

### Context-Aware Logging
```c
// When evolution context is available
CONTEXT_LOG(ctx, LOG_DEBUG, "Processing halo %d at snapnum %d", ctx->halo_nr, ctx->halo_snapnum);
```

## Recommendations by System Component

### Core Infrastructure
- **DEBUG**: Module registration, memory allocation details
- **INFO**: Pipeline execution steps, system initialization
- **WARNING**: Fallback configurations, performance concerns
- **ERROR**: System failures, resource allocation errors

### Physics Modules
- **DEBUG**: Calculation details, parameter values
- **INFO**: Phase execution notifications
- **WARNING**: Using fallback physics, parameter validation issues
- **ERROR**: Physics calculation failures

### I/O Subsystem
- **DEBUG**: Detailed file operation progress
- **INFO**: File operations start/completion, format detection
- **WARNING**: Missing optional data, format compatibility issues
- **ERROR**: File access failures, data corruption

### Module System
- **DEBUG**: Module dependency resolution, callback details
- **INFO**: Module loading/unloading, pipeline configuration
- **WARNING**: Missing optional dependencies
- **ERROR**: Module loading failures, callback errors

## Best Practices

1. **Use Appropriate Levels**: Don't overuse DEBUG level as it can impact performance in verbose mode
2. **Include Context**: Always provide relevant identifiers (galaxy ID, halo number, file names)
3. **Be Concise**: Log messages should be informative but not verbose
4. **Avoid Spam**: Don't log the same message repeatedly in tight loops
5. **Check Level**: For expensive debug computations, use `is_log_level_enabled(LOG_LEVEL_DEBUG)`

## Example Usage Patterns

```c
// Check before expensive debug operations
if (is_log_level_enabled(LOG_LEVEL_DEBUG)) {
    char *detailed_info = generate_detailed_galaxy_info(galaxy);
    LOG_DEBUG("Galaxy state: %s", detailed_info);
    free(detailed_info);
}

// Module-specific logging with error handling
if (cooling_calculation_failed) {
    MODULE_LOG("cooling", LOG_ERROR, "Cooling calculation failed for galaxy %d", galaxy->ID);
    return CALCULATION_FAILED;
}

// Progress reporting in verbose mode
LOG_INFO("Processed %d/%d galaxies in current snapshot", processed, total);
```

## Integration with Evolution Diagnostics

The logging system integrates with SAGE's evolution diagnostics to provide:

- Runtime visibility into simulation progress
- Issue detection during long simulation runs  
- Performance tracking for optimization
- Event correlation across modules

## Migration Notes

For developers updating legacy code:

- Replace `printf` statements with appropriate `LOG_*` macros
- Convert `#ifdef VERBOSE` blocks to use `LOG_DEBUG` or `LOG_INFO`
- Use module-specific logging for physics components
- Add context information where available