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
2025-05-12 09:00: new file `/Users/dcroton/Documents/Science/projects/sage-repos/sage-model/src/core/core_property_utils.h` - Created header file for generic property access utilities.
2025-05-12 09:00: new file `/Users/dcroton/Documents/Science/projects/sage-repos/sage-model/src/core/core_property_utils.c` - Created implementation file for generic property access utilities, including cached property ID lookup.
2025-05-12 09:15: edit function `hdf5_output_initialize()` in `src/io/io_hdf5_output.c` - Removed NUM_OUTPUT_FIELDS, metadata arrays now sized with MAX_GALAXY_PROPERTIES, uses actual_num_fields from generate_field_metadata.
2025-05-12 09:15: edit function `generate_field_metadata()` in `src/io/io_hdf5_output.c` - Now iterates central galaxy_property_info, returns actual field count, handles Pos/Vel components, uses type_str for HDF5 types.
2025-05-12 09:30: edit function `hdf5_output_write_galaxies()` in `src/io/io_hdf5_output.c` - Replaced direct GALAXY_PROP_* calls for physics properties with new generic property accessors (get_float_property etc.), using cached IDs. Retained GALAXY_PROP_ for Pos/Vel components and specific core int64s.
2025-05-12 10:00: edit file `src/core/core_module_system.h` - Added declaration for `module_register_properties(int module_id, const GalaxyPropertyInfo *properties_info_array, int num_properties_to_register)` and included `core_properties.h`.
2025-05-12 10:05: new function `module_register_properties()` in `src/core/core_module_system.c` - Implemented function to allow modules to register their `GalaxyPropertyInfo` arrays with the core galaxy extension system. Included `core_galaxy_extensions.h`, `core_property_types.h`, and `core_properties.h`.
2025-05-12 10:10: review file `src/physics/placeholder_output_module.c` - Verified that the placeholder output module correctly handles properties for an empty pipeline (i.e., performs no operations and accesses no specific physics properties). No changes needed.
2025-05-12 10:15: new file `tests/test_property_system.c` - Created initial structure for property system tests, including tests for core and generic physics property access, has_property, and get_cached_property_id. Includes basic mocking for galaxy properties.
2025-05-12 10:20: edit file `Makefile` - Added new test target `test_property_system` and included it in the main `tests` execution list. Linked explicitly with necessary source files for the test to build and run.
2025-05-12 10:25: new file `docs/core_physics_property_separation_principles.md` - Documented the principles of core-physics property separation, access rules, and benefits.
2025-05-12 10:25: edit file `log/decision-log.md` - Appended the core-physics property separation principles to the decision log.
