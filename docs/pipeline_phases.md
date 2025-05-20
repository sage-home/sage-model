# Pipeline Phases: Bit Flags vs. Array Indices

## Overview

The SAGE pipeline system supports four distinct execution phases (HALO, GALAXY, POST, FINAL) that modules can participate in. These phases are represented in two different ways throughout the codebase:

1. **Bit Flags**: Used for configuration, module registration, and phase checking
2. **Array Indices**: Used for storing phase-specific data in arrays

This dual representation can be confusing for developers, so this guide explains the relationship between them and how they're used in the codebase.

## Phase Representation

### Bit Flags

Pipeline phases are defined as bit flags in `core_types.h`:

```c
enum pipeline_execution_phase {
    PIPELINE_PHASE_NONE = 0,    /* No phase - initial state or reset */
    PIPELINE_PHASE_HALO = 1,    /* Execute once per halo (outside galaxy loop) */
    PIPELINE_PHASE_GALAXY = 2,  /* Execute for each galaxy (inside galaxy loop) */
    PIPELINE_PHASE_POST = 4,    /* Execute after processing all galaxies (for each integration step) */
    PIPELINE_PHASE_FINAL = 8    /* Execute after all steps are complete, before exiting evolve_galaxies */
};
```

These bit flag values (1, 2, 4, 8) are used for:

1. Declaring which phases a module supports:
   ```c
   // Module supports HALO and GALAXY phases
   .phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_GALAXY
   ```

2. Checking if a module supports a specific phase:
   ```c
   if (module->phases & PIPELINE_PHASE_HALO) {
       // Module supports HALO phase
   }
   ```

3. Setting the current execution phase in the pipeline context:
   ```c
   pipeline_ctx.execution_phase = PIPELINE_PHASE_GALAXY;
   ```

### Array Indices

For efficiency in storing phase-specific data, the phases are also represented as array indices (0, 1, 2, 3):

```c
// Phase statistics array in evolution_diagnostics
struct evolution_diagnostics {
    // ...
    struct {
        clock_t start_time;
        clock_t total_time;
        int galaxy_count;
        int step_count;
    } phases[4]; // 0=HALO, 1=GALAXY, 2=POST, 3=FINAL
    // ...
};
```

The `phase_to_index()` function in `core_evolution_diagnostics.c` converts between the two representations:

```c
/**
 * Convert pipeline phase bit flag to array index
 * 
 * Maps pipeline execution phase flags to zero-based array indices.
 * 
 * @param phase Pipeline execution phase bit flag (1, 2, 4, or 8)
 * @return Array index (0-3) or -1 if invalid
 */
static int phase_to_index(enum pipeline_execution_phase phase) {
    switch (phase) {
        case PIPELINE_PHASE_HALO:   return 0;
        case PIPELINE_PHASE_GALAXY: return 1;
        case PIPELINE_PHASE_POST:   return 2;
        case PIPELINE_PHASE_FINAL:  return 3;
        default:                    return -1;
    }
}
```

## Conversion Reference

| Phase Name | Bit Flag Value | Array Index |
|------------|----------------|-------------|
| HALO       | 1              | 0           |
| GALAXY     | 2              | 1           |
| POST       | 4              | 2           |
| FINAL      | 8              | 3           |

## Usage Guidelines

When working with pipeline phases:

1. **For Checking Phase Support**: Use bit flag operations
   ```c
   // Check if module supports the current phase
   if (module->phases & context->execution_phase) {
       // Module supports this phase
   }
   ```

2. **For Storing Phase-Specific Data**: Use the `phase_to_index()` function
   ```c
   int phase_idx = phase_to_index(phase);
   diag->phases[phase_idx].start_time = clock();
   ```

3. **For Phase Bitmask Configuration**: Use the bit flag OR operator
   ```c
   // Module supports HALO and POST phases
   .phases = PIPELINE_PHASE_HALO | PIPELINE_PHASE_POST
   ```

4. **For Pipeline Execution**: Use the bit flag constants
   ```c
   status = pipeline_execute_phase(pipeline, context, PIPELINE_PHASE_HALO);
   ```

## Implementation Details

### Phase Execution

When executing a pipeline phase, the system uses the bit flag value to determine which modules to run:

```c
int physics_step_executor(struct pipeline_step *step, struct base_module *module,
                         void *module_data, struct pipeline_context *context) {
    // Check if module supports the current phase
    if (!(module->phases & context->execution_phase)) {
        // Skip this module for this phase
        return 0;
    }
    
    // Execute appropriate phase function based on bit flag
    switch (context->execution_phase) {
        case PIPELINE_PHASE_HALO:
            return module->execute_halo_phase(module_data, context);
        // Other phases...
    }
}
```

### Phase Diagnostics

The evolution diagnostics system uses array indices to store phase-specific statistics:

```c
int evolution_diagnostics_start_phase(struct evolution_diagnostics *diag, 
                                     enum pipeline_execution_phase phase) {
    // Convert bit flag to array index
    int phase_index = phase_to_index(phase);
    
    // Record start time in the appropriate array element
    diag->phases[phase_index].start_time = clock();
    
    return 0;
}
```

## Common Pitfalls

1. **Direct Array Indexing**: Never use bit flag values (1, 2, 4, 8) as array indices
   ```c
   // WRONG - will cause out of bounds access
   diag->phases[PIPELINE_PHASE_POST].start_time = clock();
   
   // RIGHT - use phase_to_index()
   int idx = phase_to_index(PIPELINE_PHASE_POST);
   diag->phases[idx].start_time = clock();
   ```

2. **Bit Operations on Array Indices**: Never use array indices (0, 1, 2, 3) in bit operations
   ```c
   // WRONG - will not correctly check phases
   if (module->phases & 1) { // Trying to check HALO phase with index
       // ...
   }
   
   // RIGHT - use the bit flag constant
   if (module->phases & PIPELINE_PHASE_HALO) {
       // ...
   }
   ```

3. **Adding New Phases**: When adding a new phase, maintain the bit flag pattern
   ```c
   // WRONG - breaks the bit flag pattern
   #define PIPELINE_PHASE_NEW 5
   
   // RIGHT - continue power-of-2 sequence
   #define PIPELINE_PHASE_NEW 16
   ```

## Evolution Loop Phase Usage

In the main evolution loop (`core_build_model.c`), the phases are used in sequence:

1. **HALO Phase**: For calculations at the halo level (outside galaxy loop)
   ```c
   status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_HALO);
   ```

2. **GALAXY Phase**: Inside the integration steps, for each galaxy
   ```c
   for (int p = 0; p < ctx.ngal; p++) {
       // Skip already merged galaxies
       if (ctx.galaxies[p].mergeType > 0) continue;
       
       // Update context for current galaxy
       pipeline_ctx.current_galaxy = p;
       
       // Execute GALAXY phase
       status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_GALAXY);
   }
   ```

3. **POST Phase**: After processing all galaxies in a step
   ```c
   status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_POST);
   ```

4. **FINAL Phase**: After all integration steps are complete
   ```c
   status = pipeline_execute_phase(physics_pipeline, &pipeline_ctx, PIPELINE_PHASE_FINAL);
   ```

By understanding the dual representation of phases and using them correctly, you can avoid bugs and make the SAGE codebase more maintainable.
