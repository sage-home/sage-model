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

2025-05-05 09:00: planned edits for Phase 5.2.C Task 2 - Core Galaxy Lifecycle Management for `galaxy->properties`
2025-05-05 09:15: edit function `init_galaxy()` in `src/physics/legacy/model_misc.c` - Added property allocation and reset calls to properly initialize galaxy properties
2025-05-05 09:25: edit function `join_galaxies_of_progenitors()` in `src/core/core_build_model.c` - Added deep copy of galaxy properties when copying a galaxy
2025-05-05 09:35: edit function `deal_with_galaxy_merger()` in `src/physics/legacy/model_mergers.c` - Added call to free galaxy properties before marking a galaxy as merged
2025-05-05 09:40: edit function `disrupt_satellite_to_ICS()` in `src/physics/legacy/model_mergers.c` - Added call to free galaxy properties before marking a galaxy as disrupted