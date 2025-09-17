# SAGE Physics-Agnostic Architecture: Critical Developer Guide

**Purpose:** This document provides essential architectural understanding for developers implementing Phase 2A tasks and all subsequent modular development. It clarifies the fundamental design principles that must be followed to avoid architectural violations.

**Status:** Essential reading before implementing Task 2A.2 and all subsequent tasks  
**Audience:** AI assistants and developers implementing Phase 2A tasks and all subsequent modular development

---

## 🚨 CRITICAL ARCHITECTURAL PRINCIPLE

**Principle 1: Physics-Agnostic Core Infrastructure** states that "Core has zero physics knowledge."

This is NOT:
- A special "physics-free mode" that needs fallback physics calls
- A configuration option that can be disabled  
- A build-time choice between physics and no-physics versions

This IS:
- **The fundamental architecture of SAGE's core**
- **Normal operation regardless of loaded modules**  
- **Complete separation between core logic and physics calculations**

---

## Core Architectural Understanding

### What "Physics-Agnostic" Means

The core infrastructure (files in `src/core/`) operates **identically** whether:
- Zero physics modules are loaded (→ zero physics executed)
- Ten physics modules are loaded (→ full physics executed)
- Any combination of modules is loaded (→ partial physics executed)

**The core does not know or care which physics modules exist.**

### The Module System Design

```
Core (src/core/) ←→ Physics Pipeline Interface ←→ Physics Modules (src/physics/)
     ^                      ^                            ^
     |                      |                            |
  Always same           Adapter/Router              Individual physics
   behavior              (zero to many)              implementations
```

- **Core**: Tree processing, galaxy lifecycle, memory management, I/O coordination
- **Physics Pipeline**: Interface that executes 0-N physics modules in configured order
- **Physics Modules**: Infall, cooling, star formation, feedback, mergers, etc.

### Configuration-Driven Physics

Per Master Plan Phase 4 objectives:
- **Runtime configuration** determines which modules load (JSON/parameter file)
- **Empty configuration** = zero modules = zero physics = pure halo tracking
- **Full configuration** = all modules = complete galaxy evolution physics
- **Partial configuration** = selected modules = customized physics combinations

---

## Implementation Guidelines by Phase

### Phase 2A: Core/Physics Separation

**Task 2A.2 (Core Evolution Pipeline Abstraction):**
- ✅ Remove ALL `#include "model_*.h"` from core files
- ✅ Replace ALL direct physics function calls with `physics_pipeline_execute_*()`
- ✅ NO conditional compilation (#ifdef) for physics in core
- ✅ NO fallback physics calls in core
- ✅ Core always calls pipeline interface, regardless of loaded modules

**Task 2A.3 (Physics-Free Mode Implementation):**
- ❌ NOT "implementing physics-free mode" 
- ✅ IS "verifying core works with zero modules loaded"
- ✅ Core processes merger trees normally
- ✅ Physics pipeline executes zero modules (no-op)
- ✅ Output contains only core properties (halo tracking data)

**Task 2A.4 (Legacy Physics Module Wrapping):**
- ✅ Extract individual physics functions into separate modules
- ✅ Each module implements the physics_module_t interface
- ✅ Original physics calculations preserved exactly
- ✅ Modules register with physics_module_registry
- ✅ NO physics code remains in core after this task

### Phase 2B: Property System Integration

**Principle 3 (Metadata-Driven Architecture) + Principle 1 compliance:**
- Core properties: Always available (SnapNum, Type, Pos, Mvir, etc.)
- Physics properties: Available only when relevant modules loaded
- Property access macros respect module boundaries
- Core never accesses physics properties directly

### Phase 4: Configuration & Module System

**Principle 2 (Runtime Modularity) implementation:**
- Configuration files specify which modules to load
- Empty configuration = physics-agnostic core only
- Module dependencies resolved automatically
- No recompilation needed for different physics combinations

---

## Critical "Don'ts" for Core Implementation

### ❌ Never Add These to Core Files:

1. **Direct physics function calls**
   ```c
   // WRONG - violates Principle 1
   double coolingGas = cooling_recipe(p, deltaT, galaxies, run_params);
   ```

2. **Physics header includes**
   ```c
   // WRONG - creates physics dependency
   #include "model_cooling_heating.h"
   ```

3. **Conditional physics compilation**
   ```c
   // WRONG - treats physics as optional feature rather than modular design
   #ifdef SAGE_PHYSICS_FALLBACK
   starformation_and_feedback(args...);
   #endif
   ```

4. **Fallback physics calls**
   ```c
   // WRONG - core still contains physics knowledge
   if(!physics_pipeline) {
       reincorporate_gas(args...);
   }
   ```

### ✅ Always Do This in Core Files:

1. **Use physics pipeline interface exclusively**
   ```c
   // CORRECT - core delegates to interface
   physics_pipeline_execute_galaxy_phase(pipeline, galaxy_idx, central_idx, time, dt, step);
   ```

2. **Handle missing pipeline gracefully**
   ```c
   // CORRECT - no physics executed, galaxy continues existing
   if(physics_pipeline) {
       physics_pipeline_execute_galaxy_phase(/* args */);
   }
   // Galaxy maintains core properties regardless
   ```

3. **Core property management only**
   ```c
   // CORRECT - core manages halo inheritance, positions, IDs
   galaxies[ngal].Mvir = halos[halonr].Mvir;
   galaxies[ngal].SnapNum = halos[halonr].SnapNum;
   ```

---

## Physics Essential Functions (Task 2A.4+)

### Shared Physics Utilities
**File**: `src/physics/physics_essential_functions.c/h`  
**Purpose**: Physics calculations needed by multiple modules but NOT part of core

**Functions to migrate from core:**
- `get_disk_radius()` - Galaxy disk scale radius calculation (physics-dependent geometric calculation)
- `estimate_merging_time()` - Satellite merging timescale calculation (orbital dynamics physics)

**Architecture Compliance:**
- ✅ Not part of core (no physics knowledge in core)
- ✅ Available to all physics modules that need these calculations
- ✅ Contains physics calculations that can be shared between modules
- ✅ Maintains physics knowledge separation from core infrastructure

**Usage Pattern:**
```c
// Physics modules can include and use these shared utilities
#include "physics_essential_functions.h"

// In a physics module:
float disk_radius = get_disk_radius(halonr, galaxy_idx, halos, galaxies);
float merge_time = estimate_merging_time(halonr, central_halo, galaxy_idx, halos, galaxies, run_params);
```

---

## Physics Module Implementation (Phase 2A.4+)

### Module Structure
Each physics process becomes a separate module:
- `infall_module.c` - Gas infall calculations
- `cooling_module.c` - Gas cooling and heating  
- `starformation_module.c` - Star formation and feedback
- `mergers_module.c` - Galaxy merger handling
- `reincorporation_module.c` - Gas reincorporation
- etc.

### Module Interface Implementation
```c
// Example module structure
static physics_module_result_t infall_execute_halo_phase(
    physics_module_t *module,
    const physics_execution_context_t *context,
    int halonr, int ngal, double redshift)
{
    // Original infall_recipe() calculation here
    return PHYSICS_MODULE_SUCCESS;
}

physics_module_t* create_infall_module(void) {
    physics_module_t *module = malloc(sizeof(physics_module_t));
    module->name = "infall";
    module->execute_halo_phase = infall_execute_halo_phase;
    // ... other interface methods
    return module;
}
```

---

## Testing & Validation Strategy

### Principle 1 Compliance Testing
1. **Zero modules test**: Core runs successfully with empty module configuration
2. **Partial modules test**: Core runs with subset of physics modules  
3. **Full modules test**: Core runs with all physics modules loaded
4. **Result consistency**: Full modules produce identical results to legacy SAGE

### Build System Requirements
- Core compiles without any physics source files
- Physics modules compile independently
- Module loading happens at runtime, not compile time
- Configuration changes don't require recompilation

---

## Master Plan Phase Dependencies

This architectural understanding is **prerequisite** for:

- **Phase 2B**: Property system must respect physics-agnostic boundaries
- **Phase 3**: Memory management must handle module-aware allocation  
- **Phase 4**: Runtime configuration system builds on this foundation
- **Phase 5**: I/O system must adapt to loaded module set
- **Phase 6**: Validation requires understanding of modular architecture

### Success Metric Alignment

**Principle 1 validation**: "Core runs successfully with zero physics modules loaded"

This means:
- ✅ SAGE processes merger trees
- ✅ Galaxies inherit halo properties  
- ✅ Tree traversal works correctly
- ✅ Output contains core properties only
- ✅ NO physics calculations executed
- ✅ NO special "physics-free mode" code paths

---

## Implementation Examples for Task 2A.2

### How to Replace Physics Calls in Core

**❌ NEVER do this in core files:**
```c
// WRONG - direct physics function calls violate Principle 1
double infallingGas = infall_recipe(centralgal, ngal, Zcurr, galaxies, run_params);
double coolingGas = cooling_recipe(p, deltaT / STEPS, galaxies, run_params);
cool_gas_onto_galaxy(p, coolingGas, galaxies);
starformation_and_feedback(p, centralgal, time, deltaT / STEPS, halonr, step, galaxies, run_params);
```

**✅ ALWAYS do this in core files:**
```c
// CORRECT - core delegates to physics pipeline interface
double infallingGas = 0.0;
if(physics_pipeline) {
    infallingGas = physics_pipeline_execute_halo_phase(physics_pipeline, halonr, ngal, Zcurr);
}

if(physics_pipeline) {
    physics_pipeline_execute_galaxy_phase(physics_pipeline, p, centralgal, time, deltaT / STEPS, step);
}
// No fallback needed - galaxy continues with core properties only
```

### How to Handle Missing Physics Pipeline

**❌ NEVER add fallback physics:**
```c
// WRONG - this puts physics knowledge back in core
if(!physics_pipeline) {
    reincorporate_gas(centralgal, deltaT / STEPS, galaxies, run_params);
    double coolingGas = cooling_recipe(p, deltaT / STEPS, galaxies, run_params);
}
```

**✅ ALWAYS handle gracefully:**
```c
// CORRECT - no physics executed, galaxy maintains core properties
if(physics_pipeline) {
    physics_pipeline_execute_galaxy_phase(physics_pipeline, p, centralgal, time, deltaT / STEPS, step);
}
// Galaxy continues existing with halo-inherited properties
// No special handling needed
```

### Task 2A.2 Implementation Strategy

1. **Identify all physics function calls in core files:**
   - Search for functions defined in `src/physics/model_*.c`
   - Look for patterns like `*_recipe`, `*_feedback`, `*_merger`, etc.

2. **Replace each physics call with appropriate pipeline call:**
   - Halo-level physics → `physics_pipeline_execute_halo_phase()`
   - Galaxy-level physics → `physics_pipeline_execute_galaxy_phase()` 
   - Post-processing → `physics_pipeline_execute_post_phase()`
   - Final cleanup → `physics_pipeline_execute_final_phase()`

3. **Remove all physics headers from core:**
   - Delete all `#include "model_*.h"` lines
   - Ensure core compiles without physics dependencies

4. **Test zero-module operation:**
   - Verify SAGE runs with NULL physics_pipeline
   - Confirm galaxies inherit halo properties correctly
   - Validate no physics calculations occur

---

## Common Implementation Mistakes

### Mistake 1: Treating Physics-Agnostic as Optional
**Wrong thinking**: "We need both physics and physics-free versions"  
**Correct thinking**: "Core is always physics-agnostic, modules provide physics"

### Mistake 2: Adding Fallback Physics
**Wrong thinking**: "Core needs backup physics if modules fail"  
**Correct thinking**: "Module failure means no physics, galaxy continues with core properties"

### Mistake 3: Conditional Compilation Patterns
**Wrong thinking**: "Use #ifdef to toggle physics on/off"  
**Correct thinking**: "Module loading configuration determines physics behavior"

### Mistake 4: Special Mode Implementations  
**Wrong thinking**: "Implement separate physics-free code paths"  
**Correct thinking**: "Same code path, different module loading behavior"

---

## Quick Reference: Phase 2A Task Validation

For each Task 2A.X, validate against these criteria:

**Principle 1 Compliance Checklist:**
- [ ] Core files contain zero `#include "model_*.h"`
- [ ] Core files contain zero direct physics function calls
- [ ] Core files contain zero `#ifdef PHYSICS_*` conditionals
- [ ] Core always uses physics_pipeline interface
- [ ] Core operates identically regardless of loaded modules
- [ ] Physics knowledge exists only in modules (src/physics/)

**Functional Validation:**
- [ ] SAGE runs with zero modules loaded (outputs core properties only)
- [ ] SAGE runs with all modules loaded (outputs full physics results)  
- [ ] Results identical between modular and legacy for same physics configuration
- [ ] No compilation dependencies between core and physics

---

## Task 2A.2 Implementation Checklist

**Before Starting:**
- [ ] Read and understand all 8 Core Architectural Principles
- [ ] Understand that physics-agnostic is the **normal architecture**, not a special mode
- [ ] Review physics module interface design from Task 2A.1

**Implementation Steps:**

1. **Audit current physics dependencies in core:**
   - [ ] Find all `#include "model_*.h"` in `src/core/` files
   - [ ] Identify all direct physics function calls in core
   - [ ] Map each physics call to appropriate pipeline phase

2. **Implement physics pipeline integration:**
   - [ ] Add physics_pipeline parameter to core functions
   - [ ] Initialize physics pipeline in sage_per_forest
   - [ ] Pass pipeline through construct_galaxies → evolve_galaxies

3. **Replace physics calls systematically:**
   - [ ] Halo-level: infall_recipe → physics_pipeline_execute_halo_phase
   - [ ] Galaxy-level: cooling, star formation, feedback → physics_pipeline_execute_galaxy_phase
   - [ ] Mergers: disruption, mergers → physics_pipeline_execute_post_phase
   - [ ] Cleanup: final calculations → physics_pipeline_execute_final_phase

4. **Remove physics dependencies:**
   - [ ] Delete all `#include "model_*.h"` from core files
   - [ ] Remove all direct physics function calls
   - [ ] Ensure core compiles without physics source files

5. **Handle I/O system physics calls:**
   - [ ] Replace physics functions in save_gals_*.c files
   - [ ] Use galaxy properties directly or pipeline interface

**Validation Criteria:**
- [ ] Core compiles without any physics dependencies
- [ ] SAGE runs successfully with NULL physics_pipeline (zero modules)
- [ ] Galaxies inherit halo properties correctly in zero-module mode
- [ ] No physics calculations occur when no modules loaded
- [ ] Same execution path regardless of loaded module count
- [ ] All physics knowledge exists only in physics_pipeline and modules

**Exit Criteria for Task 2A.2:**
- ✅ Core compiles and runs without physics modules loaded  
- ✅ Module interface enables conditional physics execution
- ✅ Zero-module operation processes merger trees successfully  
- ✅ Core has zero physics knowledge (Principle 1 compliance)
- ✅ Foundation ready for Task 2A.4 (physics module wrapping)

This implementation establishes the **non-negotiable architectural foundation** for all subsequent development phases.