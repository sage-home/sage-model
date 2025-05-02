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

2025-05-02 09:00: new file `src/core/standard_properties.h` - Created header for standard properties registration
2025-05-02 09:15: new file `src/core/standard_properties.c` - Implemented standard properties registration system
2025-05-02 09:30: edit file `src/core/core_init.h` - Added declarations for standard properties initialization and cleanup
2025-05-02 09:45: edit file `src/core/core_init.c` - Integrated standard properties initialization into core init process
2025-05-02 10:00: edit file `Makefile` - Added standard_properties.c to CORE_SRC and standard_properties.h to INCL
2025-05-02 10:15: edit file `log/phase-tracker-log.md` - Updated progress for standard properties registration task
2025-05-02 14:30: new file `tests/test_property_registration.c` - Created test for property registration with extension system
2025-05-02 15:00: edit file `src/core/core_init.c` - Fixed duplicate symbol error by removing duplicate implementation of `cleanup_property_system()`

