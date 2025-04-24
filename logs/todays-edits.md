<!-- Purpose: Record todays work to avoid duplication / forgotten work -->
<!-- Update Rules:
- A "group of edits" are changes made to a single function or header or equivalent, or a new file
- ALWAYS refer to this file BEFORE starting a new group of edits to avoid duplication
- ALWAYS write to this file AFTER finishing a group of edits before moving on
- Must have the following format:
  • <today's date> <24hr time right now>: <new/edit> <target function or equivalent> in <filename> - <short description of change>
  • E.g. 2025-04-23 14:23: edit function `write_log_to_destination()` in `src/core/core_logging.c` - Write a log message to a specific destination
  • E.g. 2025-04-17 09:55: new file `tests/test_pipeline.c` - Unit tests for pipeline phase system
- Always add to bottom of list
- If you discover duplication go back and check the code, then report and stop for instructions
-->

2025-04-24 09:00: new file `src/core/core_evolution_diagnostics.h` - Created header file for evolution diagnostics system
2025-04-24 09:10: new file `src/core/core_evolution_diagnostics.c` - Created implementation file for evolution diagnostics system
2025-04-24 09:20: edit struct `evolution_context` in `src/core/core_allvars.h` - Added diagnostics pointer to evolution context
2025-04-24 09:30: edit function `evolve_galaxies()` in `src/core/core_build_model.c` - Integrated evolution diagnostics into galaxy evolution pipeline
2025-04-24 09:40: edit function `event_dispatch()` in `src/core/core_event_system.c` - Added code to track events in evolution diagnostics
2025-04-24 09:50: edit variable `CORE_SRC` in `Makefile` - Added core_evolution_diagnostics.c to compilation sources
2025-04-24 10:00: edit file `src/core/core_build_model.c` - Added missing include for core_evolution_diagnostics.h
2025-04-24 10:10: edit file `src/core/core_event_system.c` - Added missing includes for evolution diagnostics and pipeline system
2025-04-24 10:20: edit file `src/core/core_evolution_diagnostics.h` - Added include for core_pipeline_system.h for pipeline_execution_phase enum
2025-04-24 10:30: edit file `src/core/core_evolution_diagnostics.c` - Added include for core_pipeline_system.h
2025-04-24 10:40: edit function `evolution_diagnostics_report()` in `src/core/core_evolution_diagnostics.c` - Fixed logging to use proper LOG_* macros instead of undefined LOG function

