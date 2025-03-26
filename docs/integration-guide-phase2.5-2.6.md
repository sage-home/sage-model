# Integration Guide: Pipeline System and Configuration System Phase 2.5-2.6

## Overview

This guide provides the implementation steps needed to complete phases 2.5 (Module Pipeline System) and 2.6 (Configuration System) of the SAGE model refactoring project.

## Implementation Status

The following files have been created and need to be integrated into the codebase:

1. **Pipeline System Files**
   - `/src/core/core_pipeline_system.h` - Pipeline system interface
   - `/src/core/core_pipeline_system.c` - Pipeline system implementation

2. **Configuration System Files**
   - `/src/core/core_config_system.h` - Configuration system interface
   - `/src/core/core_config_system.c` - Configuration system implementation

3. **Example Configuration**
   - `/input/config.json` - Example configuration file

## Code Review Required

The following implementation notes should be addressed during code review:

### Pipeline System

1. **Pipeline Event Handler Bug**
   - In `core_pipeline_system.c`, there's a duplicate definition of `data` variable in the `pipeline_event_handler` function
   - Fix by removing the second definition
   - The function also lacks a return statement - add `return true;` at the end

```c
// Fix from:
static bool pipeline_event_handler(const struct event *event, void *user_data) {
    (void)user_data; /* Suppress unused parameter warning */

    struct pipeline_event_data *data = (struct pipeline_event_data *)(event->data);
    struct pipeline_event_data *data = (struct pipeline_event_data *)event_data;
    
    // ... function body ...
}

// To:
static bool pipeline_event_handler(const struct event *event, void *user_data) {
    (void)user_data; /* Suppress unused parameter warning */

    struct pipeline_event_data *data = (struct pipeline_event_data *)(event->data);
    
    // ... function body ...
    
    return true; // Return true to continue event processing
}
```

### Configuration System

1. **Parameter Structure Access**
   - The `config_configure_params` function assumes direct access to parameters like `params->FirstFile`, but these are actually in nested structures
   - Update all parameter access to use the proper nested structure
   - For example, use `params->io.FirstFile` instead of `params->FirstFile`

2. **String Handling**
   - Replace calls to `strdup` with direct string copying to fixed-size buffers
   - Use `strncpy` with proper NULL termination to prevent buffer overflows

```c
// Fix from:
params->FileWithSnapList = strdup(config_get_string("simulation.snap_list_file", params->FileWithSnapList));

// To:
const char *snap_list_file = config_get_string("simulation.snap_list_file", NULL);
if (snap_list_file != NULL) {
    strncpy(params->io.FileWithSnapList, snap_list_file, MAX_STRING_LEN - 1);
    params->io.FileWithSnapList[MAX_STRING_LEN - 1] = '\0';
}
```

3. **Format String Warning**
   - Fix format specifiers for int64_t values using `%lld` instead of `%ld`

4. **Error Handling**
   - Use `LOG_ERROR` instead of `log_error` to maintain consistent logging style

## Integration Steps

1. **Fix Pipeline System**
   - Update `pipeline_event_handler` in `core_pipeline_system.c`
   - Add proper return statement
   - Fix module parameter warnings by adding `(void)` to unused parameters

2. **Fix Configuration System**
   - Update parameter access in `config_configure_params` to use nested structures
   - Fix string handling by replacing `strdup` with proper bounded copies
   - Fix format string warnings for int64_t values

3. **Update Initialization**
   - Integrate with `core_init.c` to properly initialize and clean up both systems
   - Ensure the pipeline system is initialized before attempting to use it

4. **Implement Pipeline Integration**
   - Create a custom pipeline step execution function in `core_build_model.c`
   - Update `evolve_galaxies` to use the pipeline system instead of hardcoded physics sequence

## Testing

After completing the integration, verify functionality with:

```bash
cd /Users/dcroton/Documents/Science/projects/sage-repos/sage-model
make tests
```

This will compile SAGE and run the test suite to ensure the refactored code still produces the correct scientific results.

## Next Steps

Once phases 2.5 and 2.6 are complete:

1. Document the pipeline system API and configuration format
2. Create example modules that demonstrate how to plug into the pipeline
3. Proceed to Phase 3 (I/O Abstraction and Optimization)
