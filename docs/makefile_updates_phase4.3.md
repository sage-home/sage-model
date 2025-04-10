# Makefile Updates for Phase 4.3 (Parameter Tuning System)

To complete the integration of the Parameter Tuning System implemented in Phase 4.3, the following changes need to be made to the main Makefile.

## Source File Additions

Add `core_module_parameter.c` to the `CORE_SRC` list:

```diff
CORE_SRC := core/sage.c core/core_read_parameter_file.c core/core_init.c \
        core/core_io_tree.c core/core_cool_func.c core/core_build_model.c \
        core/core_save.c core/core_mymalloc.c core/core_utils.c \
        core/progressbar.c core/core_tree_utils.c core/core_parameter_views.c \
        core/core_logging.c core/core_module_system.c \
        core/core_galaxy_extensions.c core/core_event_system.c \
        core/core_pipeline_system.c core/core_config_system.c \
        core/core_module_callback.c core/core_array_utils.c \
        core/core_memory_pool.c core/core_dynamic_library.c \
        core/core_module_template.c core/core_module_validation.c \
+        core/core_module_debug.c core/core_module_parameter.c
-        core/core_module_debug.c
```

## Test Target Additions

Add `test_module_parameter` to the `.PHONY` list:

```diff
-.PHONY: clean celan celna clena tests all test_extensions test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_binary_output test_hdf5_output test_io_validation test_memory_map test_dynamic_library test_module_framework test_module_debug
+.PHONY: clean celan celna clena tests all test_extensions test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_binary_output test_hdf5_output test_io_validation test_memory_map test_dynamic_library test_module_framework test_module_debug test_module_parameter
```

Add a test target for the parameter system:

```diff
+test_module_parameter: tests/test_module_parameter.c $(SAGELIB)
+	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_module_parameter tests/test_module_parameter.c -L. -l$(LIBNAME) $(LIBFLAGS)
+
```

Update the `tests` target to include the new test:

```diff
-tests: $(EXEC) test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_binary_output test_hdf5_output test_io_validation test_property_validation test_dynamic_library test_module_framework test_module_debug
+tests: $(EXEC) test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_binary_output test_hdf5_output test_io_validation test_property_validation test_dynamic_library test_module_framework test_module_debug test_module_parameter
```

Add the new test to the test execution list:

```diff
	./tests/test_module_debug
+	./tests/test_module_parameter
	@cd tests && make -f Makefile.memory_tests
	@echo "All tests completed successfully."
```

## Running the Tests

After applying these changes to the Makefile, the parameter tuning system tests can be run with:

```bash
make test_module_parameter
./tests/test_module_parameter
```

Or as part of the full test suite:

```bash
make tests
```