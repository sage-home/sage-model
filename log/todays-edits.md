<!-- Purpose: Record todays work to avoid duplication / forgotten work -->
<!-- Update Rules:
- A "group of edits" are changes made to a single function/file/struct/variable/etc, or a new file
- ALWAYS refer to this file BEFORE starting a new group of edits to avoid duplication
- ALWAYS write to this file AFTER finishing a group of edits before moving on
- Must have the following format:
  • <today's date> <24hr time right now>: <new/edit/removed> <function/file/struct/variable/etc> in <filename> - <short description of change>
  • E.g. 2025-04-23 09:34: edit function `evolve_galaxies()` in `src/core/core_build_model.c` - Integrated evolution diagnostics into galaxy evolution pipeline
  • E.g. 2025-04-24 13:10: new file `src/core/core_evolution_diagnostics.c` - Created implementation file for evolution diagnostics system
- Record new edits even when they're to something that's already been recorded here
- Always add to bottom of list
- If you discover duplication go back and check the code, then report and stop for instructions
-->

2025-04-28 10:00: edit file `src/physics/modules/cooling_module.h` - Updated header to include all necessary function declarations for self-containment
2025-04-28 10:15: edit file `src/physics/modules/cooling_module.c` - Made cooling module fully self-contained by incorporating all functions from traditional physics implementation
2025-04-28 10:30: edit file `src/physics/modules/infall_module.h` - Updated header to include all necessary function declarations for self-containment
2025-04-28 10:45: edit file `src/physics/modules/infall_module.c` - Made infall module fully self-contained by incorporating all functions from traditional physics implementation
2025-04-28 11:00: moved files `model_cooling_heating.c` and `model_cooling_heating.h` from `src/physics` to `src/physics/old` - Archived traditional cooling implementation
2025-04-28 11:05: moved files `model_infall.c` and `model_infall.h` from `src/physics` to `src/physics/old` - Archived traditional infall implementation
2025-04-28 14:30: edit include section in `src/core/core_init.c` - Removed dependency on example_event_handler.h to improve runtime modularity
2025-04-28 14:45: edit function `initialize_event_system()` in `src/core/core_init.c` - Removed direct references to example handlers to maintain clean core implementation
2025-04-28 15:00: edit function `cleanup_event_system()` in `src/core/core_init.c` - Removed direct references to example handlers to maintain clean core implementation
