
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

2025-05-01 13:00: new file `src/generate_property_headers.py` - Created script to generate property header files from YAML definition
2025-05-01 13:10: edit Makefile - Integrated property header generation into build system
2025-05-01 13:15: new file `src/core/core_properties.c` - Created placeholder file for property system implementation
2025-05-01 13:25: ran header generation script - Generated core_properties.h and core_properties.c files
2025-05-01 13:35: edit file `src/generate_property_headers.py` - Added STEPS constant definition to fix compilation errors
2025-05-01 13:50: edit core_allvars.h - Updated struct GALAXY definition to include properties field
2025-05-01 13:51: edit generate_property_headers.py - Updated GALAXY struct handling and STEPS macro usage
2025-05-01 19:45: edit file `src/generate_property_headers.py` - Complete rewrite with proper Python implementation
2025-05-01 19:50: edit file `src/core/core_properties.h` - Fixed struct naming inconsistency (galaxy vs. GALAXY)
2025-05-01 19:52: edit file `src/core/core_properties.c` - Fixed struct naming inconsistency and added dynamic array handling
2025-05-01 19:55: edit Makefile - Added .SECONDARY directive to prevent duplicate header generation
2025-05-01 20:15: edit Makefile - Fixed duplicate property header generation with stamp file approach
