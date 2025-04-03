# Makefile Changes for Binary Output Handler Integration

To fully integrate the Binary Output Handler into the SAGE build system, the following changes to the Makefile are required:

## 1. Add Source Files to IO_SRC

```diff
IO_SRC := io/read_tree_lhalo_binary.c io/read_tree_consistentrees_ascii.c \
        io/ctrees_utils.c io/save_gals_binary.c io/forest_utils.c \
        io/buffered_io.c io/io_interface.c io/io_galaxy_output.c \
        io/io_endian_utils.c io/io_lhalo_binary.c io/io_property_serialization.c \
+       io/io_binary_output.c
```

## 2. Add Test Target

```diff
test_property_serialization: tests/test_property_serialization.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_serialization tests/test_property_serialization.c -L. -l$(LIBNAME) $(LIBFLAGS)

+ test_binary_output: tests/test_binary_output.c $(SAGELIB)
+ 	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_binary_output tests/test_binary_output.c -L. -l$(LIBNAME) $(LIBFLAGS)
```

## 3. Update Tests Target

```diff
- tests: $(EXEC) test_io_interface test_endian_utils test_lhalo_binary test_property_serialization
+ tests: $(EXEC) test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_binary_output
	./tests/test_sage.sh
	./tests/test_io_interface
	./tests/test_endian_utils
	./tests/test_lhalo_binary
	./tests/test_property_serialization
+	./tests/test_binary_output
```

## 4. Update .PHONY Target

```diff
- .PHONY: clean celan celna clena tests all test_extensions test_io_interface test_endian_utils test_lhalo_binary test_property_serialization
+ .PHONY: clean celan celna clena tests all test_extensions test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_binary_output
```

## Steps to Apply Changes

1. Make a backup of the current Makefile
2. Apply the changes above
3. Run `make clean` to ensure a fresh build
4. Run `make libs` to build the library with the new files
5. Run `make test_binary_output` to build and test the Binary Output Handler
6. Run `make tests` to ensure all tests pass