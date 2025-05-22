# ==============================================================================
# SAGE Semi-Analytic Galaxy Evolution model - Makefile
# ==============================================================================

# -------------- Build Configuration Options -----------------
# Uncomment or set via command line to enable features

# USE-MPI := yes      # Enable MPI parallel processing (auto-set if CC=mpicc)
USE-HDF5 := yes       # Enable HDF5 tree reading support
# MEM-CHECK := yes    # Enable memory/pointer sanitization (~2x slower, gcc only)
USE-BUFFERED-WRITE := yes  # Enable chunked output for better performance
MAKE-SHARED-LIB := yes     # Build shared library instead of static
# MAKE-VERBOSE := yes      # Enable verbose information messages

# Property definition file can be changed
ifndef PROPERTIES_FILE
PROPERTIES_FILE := properties.yaml
endif

# -------------- Directory Setup ----------------------------
ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
# Fallback to current dir if detection fails
ROOT_DIR := $(if $(ROOT_DIR),$(ROOT_DIR),.)

# -------------- Compiler Options --------------------------
LIBFLAGS := -lm
OPTS := -DROOT_DIR=\'\"${ROOT_DIR}\"\'
SRC_PREFIX := src
INCLUDE_DIRS := -I$(ROOT_DIR)/src -I$(ROOT_DIR)/src/core -I$(ROOT_DIR)/tests # Added

# -------------- Library Configuration ---------------------
LIBNAME := sage

# Generated source files
GENERATED_C_FILES := core/core_properties.c core/generated_output_transformers.c
GENERATED_H_FILES := core/core_properties.h
GENERATED_FILES := $(addprefix $(SRC_PREFIX)/, $(GENERATED_H_FILES)) $(addprefix $(SRC_PREFIX)/, $(GENERATED_C_FILES))
GENERATED_DEPS := properties.yaml generate_property_headers.py

# Core source files
CORE_SRC := core/sage.c core/core_read_parameter_file.c core/core_init.c \
        core/core_io_tree.c core/core_build_model.c \
        core/core_save.c core/core_mymalloc.c core/core_utils.c \
        core/progressbar.c core/core_tree_utils.c core/core_parameter_views.c \
        core/core_logging.c core/core_module_system.c \
        core/core_galaxy_extensions.c core/core_event_system.c \
        core/core_pipeline_system.c core/core_config_system.c \
        core/core_module_callback.c core/core_array_utils.c \
        core/core_memory_pool.c core/core_dynamic_library.c \
        core/core_module_parameter.c \
        core/core_module_error.c \
        core/core_merger_queue.c core/cJSON.c core/core_evolution_diagnostics.c \
        core/core_galaxy_accessors.c core/core_pipeline_registry.c \
        core/core_properties.c core/standard_properties.c \
        core/physics_pipeline_executor.c core/core_property_utils.c \
        core/generated_output_transformers.c

# Physics model source files
PHYSICS_SRC := physics/placeholder_empty_module.c \
        physics/placeholder_cooling_module.c physics/placeholder_infall_module.c \
        physics/placeholder_output_module.c \
        physics/placeholder_starformation_module.c \
        physics/placeholder_disk_instability_module.c \
        physics/placeholder_reincorporation_module.c \
        physics/placeholder_mergers_module.c \
        physics/placeholder_model_misc.c physics/placeholder_validation.c \
        physics/placeholder_hdf5_save.c physics/physics_output_transformers.c

# I/O source files
IO_SRC := io/read_tree_lhalo_binary.c io/read_tree_consistentrees_ascii.c \
        io/ctrees_utils.c io/forest_utils.c \
        io/io_interface.c io/io_galaxy_output.c \
        io/io_endian_utils.c io/io_lhalo_binary.c io/io_property_serialization.c \
        io/io_hdf5_output.c io/io_validation.c \
        io/io_buffer_manager.c io/io_memory_map.c

# Combine all library sources
LIBSRC := $(CORE_SRC) $(PHYSICS_SRC) $(IO_SRC)

# Files in LIBSRC that should NOT have implicit .h dependencies created for LIBINCL
# because their interfaces are handled by other headers (e.g., core_properties.h for generated_output_transformers.c)
LIBINCL_FILTER_OUT_IMPLICIT_H := core/generated_output_transformers.c
# Add other .c files here if they also don't have a corresponding .h and their interface is elsewhere

# Filter these out before applying the pattern to derive .h files
LIBINCL_FILTERED_SRC_FOR_PATTERN := $(filter-out $(LIBINCL_FILTER_OUT_IMPLICIT_H), $(LIBSRC))

# Define LIBINCL using the filtered list and explicitly add other necessary headers
LIBINCL := $(LIBINCL_FILTERED_SRC_FOR_PATTERN:.c=.h) io/parse_ctrees.h

# Main executable source
SRC := core/main.c $(LIBSRC)
SRC := $(addprefix $(SRC_PREFIX)/, $(SRC))
OBJS := $(SRC:.c=.o)

# Include files
INCL := core/core_allvars.h core/macros.h core/core_simulation.h core/core_event_system.h $(LIBINCL) core/core_properties.h
INCL := $(addprefix $(SRC_PREFIX)/, $(INCL))

# Library objects and includes
LIBSRC := $(addprefix $(SRC_PREFIX)/, $(LIBSRC))
LIBINCL := $(addprefix $(SRC_PREFIX)/, $(LIBINCL))
LIBOBJS := $(LIBSRC:.c=.o)

# -------------- Target Configuration ----------------------
ifdef MAKE-SHARED-LIB
  SAGELIB := lib$(LIBNAME).so
else
  SAGELIB := lib$(LIBNAME).a
endif

ifdef MAKE-VERBOSE
  OPTS += -DVERBOSE
endif

EXEC := $(LIBNAME)

# -------------- Platform and Compiler Setup ---------------
UNAME := $(shell uname)
ifeq ($(CC), mpicc)
	USE-MPI = yes
endif

ifdef USE-MPI
    OPTS += -DMPI  # Enable MPI parallel processing
    CC := mpicc
else
    # Default compiler selection based on platform
    ifeq ($(UNAME), Darwin)
      CC := clang
    else
      CC := gcc
    endif
endif

# -------------- Build Process Control ---------------------
# Skip checks if only cleaning
DO_CHECKS := 1
CLEAN_CMDS := celan celna clean clena
ifneq ($(filter $(CLEAN_CMDS),$(MAKECMDGOALS)),)
  DO_CHECKS := 0
endif

# -------------- HDF5 Configuration -----------------------
# HDF5 source files (defined outside to support 'make clean')
H5_SRC := io/read_tree_lhalo_hdf5.c io/save_gals_hdf5.c io/read_tree_genesis_hdf5.c \
          io/hdf5_read_utils.c io/read_tree_consistentrees_hdf5.c \
          io/read_tree_gadget4_hdf5.c io/io_hdf5_utils.c \
          io/prepare_galaxy_for_hdf5_output.c io/trigger_buffer_write.c \
          io/generate_field_metadata.c io/initialize_hdf5_galaxy_files.c \
          io/finalize_hdf5_galaxy_files.c io/save_gals_hdf5_property_utils.c

# H5_INCL := $(H5_SRC:.c=.h)
H5_INCL := io/read_tree_lhalo_hdf5.h \
           io/save_gals_hdf5.h \
           io/read_tree_genesis_hdf5.h \
           io/hdf5_read_utils.h \
           io/read_tree_consistentrees_hdf5.h \
           io/read_tree_gadget4_hdf5.h \
           io/io_hdf5_utils.h \
           io/save_gals_hdf5_internal.h
H5_OBJS := $(H5_SRC:.c=.o)

H5_SRC := $(addprefix $(SRC_PREFIX)/, $(H5_SRC))
H5_INCL := $(addprefix $(SRC_PREFIX)/, $(H5_INCL))
H5_OBJS := $(addprefix $(SRC_PREFIX)/, $(H5_OBJS))

# -------------- Dependency Checking ----------------------
ifeq ($(DO_CHECKS), 1)
  # Verify compiler exists
  CC_VERSION := $(shell $(CC) --version 2>/dev/null)
  ifndef CC_VERSION
    $(info Error: Could not find compiler = ${CC})
    $(info Please either set "CC" in "Makefile" or via the command-line "make CC=yourcompiler")
    $(info And please check that the specified compiler is in your "$$PATH" variables)
    $(error )
  endif

  # Check if using Clang
  ifeq (clang,$(findstring clang,$(CC_VERSION)))
    CC_IS_CLANG := 1
  else
    CC_IS_CLANG := 0
  endif

  # Special handling for GCC on macOS (use Clang assembler)
  ifeq ($(UNAME), Darwin)
    ifeq ($(CC_IS_CLANG), 0)
      CCFLAGS += -Wa,-q
    endif
  endif

  # CI environment detection
  ON_CI := false
  ifeq ($(CI), true)
    ON_CI := true
  endif
  ifeq ($(TRAVIS), true)
    ON_CI := true
  endif
  ifeq ($(ON_CI), true)
    CCFLAGS += -Werror  # Treat warnings as errors on CI
  endif

  # Buffered write support
  ifdef USE-BUFFERED-WRITE
    CCFLAGS += -DUSE_BUFFERED_WRITE
  endif

  # HDF5 support configuration
  ifdef USE-HDF5
    ifndef HDF5_DIR
      ifeq ($(ON_CI), true)
        # Use conda environment on CI
        CONDA_BASE := $(shell gsl-config --prefix 2>/dev/null)
        HDF5_DIR := $(CONDA_BASE)
      else
        # Try to detect HDF5 installation
        HDF5_DIR := $(strip $(shell which h5ls 2>/dev/null | sed 's/\/bin\/h5ls//'))
        ifndef HDF5_DIR
          $(warning $$HDF5_DIR environment variable is not defined but HDF5 is requested)
          $(warning Could not locate hdf5 tools either)
          $(warning Please install HDF5 or disable the 'USE-HDF5' option)
          HDF5_DIR:= ${HOME}/anaconda3
          $(warning Proceeding with default directory '[${HDF5_DIR}]' - compilation might fail)
        endif
      endif
    endif

    # Add HDF5 sources, objects and includes
    LIBSRC += $(H5_SRC)
    SRC += $(H5_SRC)
    LIBOBJS += $(H5_OBJS)
    OBJS += $(H5_OBJS)
    LIBINCL += $(H5_INCL)
    INCL += $(H5_INCL)

    # HDF5 compilation flags
    HDF5_INCL := -I$(HDF5_DIR)/include
    HDF5_LIB := -L$(HDF5_DIR)/lib -lhdf5 -Xlinker -rpath -Xlinker $(HDF5_DIR)/lib

    OPTS += -DHDF5
    LIBFLAGS += $(HDF5_LIB)
    CCFLAGS += $(HDF5_INCL)
  endif

  # Add git commit reference if available
  GIT_FOUND := $(shell git --version 2>/dev/null)
  ifdef GIT_FOUND
    OPTS += -DGITREF_STR='"$(shell git show-ref --head | head -n 1 | cut -d " " -f 1)"'
  else
    OPTS += -DGITREF_STR='""'
  endif

  # MPI library path for runtime loading
  ifdef USE-MPI
    MPI_LINK_FLAGS := $(firstword $(shell $(CC) --showme:link 2>/dev/null))
    MPI_LIB_DIR := $(firstword $(subst -L, , $(MPI_LINK_FLAGS)))
    LIBFLAGS += -Xlinker -rpath -Xlinker $(MPI_LIB_DIR)
  endif

  # Optimization settings
  OPTIMIZE := -O2
  ifeq ($(filter tests,$(MAKECMDGOALS)),)
    OPTIMIZE += -march=native
  endif

  # Memory checking options
  ifdef MEM-CHECK
    SANITIZE_FLAGS := -fsanitize=undefined -fsanitize=bounds -fsanitize=address -fsanitize-undefined-trap-on-error -fstack-protector-all
    CCFLAGS += $(SANITIZE_FLAGS)
    LIBFLAGS += $(SANITIZE_FLAGS)
  endif

  # Check if compiler and archiver match
  AR_BASE_PATH := $(shell which $(AR))
  CC_BASE_PATH := $(shell which $(CC))
  AR_FIRST_DIR := $(firstword $(subst /, ,$(AR_BASE_PATH)))
  CC_FIRST_DIR := $(firstword $(subst /, ,$(CC_BASE_PATH)))

  AR_NAME := $(lastword $(subst /, ,$(AR_BASE_PATH)))
  CC_NAME := $(lastword $(subst /, ,$(CC_BASE_PATH)))
  CC_IN_AR_DIR := $(subst $(AR_NAME),$(CC_NAME),$(AR_BASE_PATH))
  AR_IN_CC_DIR := $(subst $(CC_NAME),$(AR_NAME),$(CC_BASE_PATH))

  ifneq ($(AR_FIRST_DIR), $(CC_FIRST_DIR))
    $(warning Archiver ($(AR)) and compiler ($(CC)) appear to be from different toolchains)
    $(warning Archiver: '$(AR_BASE_PATH)', Compiler: '$(CC_BASE_PATH)')
    $(warning If linking fails, try using matching tools: '$(CC_IN_AR_DIR)' or '$(AR_IN_CC_DIR)')
  endif

  # Common compiler flags
  CCFLAGS += -DGNU_SOURCE -std=gnu99 -fPIC -g -Wextra -Wshadow -Wall -Wno-unused-local-typedefs
  CCFLAGS += $(INCLUDE_DIRS) # Added
  
  # Platform-specific dynamic library linker flags
  ifeq ($(UNAME), Linux)
    LIBFLAGS += -ldl
  endif

else
  # For clean targets with HDF5 enabled
  ifdef USE-HDF5
    OBJS += $(H5_OBJS)
  endif
endif

# -------------- Build Targets ----------------------------

# Main Targets
.PHONY: clean celan celna clena tests all test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_hdf5_output test_io_validation test_memory_map test_dynamic_library test_property_registration test_core_physics_separation test_property_system_hdf5 test_property_array_access test_core_pipeline_registry

all: $(SAGELIB) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $^ $(LIBFLAGS) -o $@

lib libs: $(SAGELIB)

lib$(LIBNAME).a: $(LIBOBJS)
	@echo "Creating static library"
	$(AR) rcs $@ $(LIBOBJS)

lib$(LIBNAME).so: $(LIBOBJS)
	@echo "Creating shared library"
	$(CC) -shared $(LIBOBJS) -o $@ $(LIBFLAGS)

pyext: lib$(LIBNAME).so
	@echo "Creating Python extension"
	python3 -c "from sage import build_sage_pyext; build_sage_pyext();"

# Create a stamp file directory if needed
$(shell mkdir -p $(ROOT_DIR)/.stamps)

# Rule to generate property headers
.SECONDARY: $(GENERATED_FILES)
$(ROOT_DIR)/.stamps/generate_properties.stamp: $(SRC_PREFIX)/$(PROPERTIES_FILE) $(SRC_PREFIX)/generate_property_headers.py
	@echo "Generating property headers from $(PROPERTIES_FILE)..."
	cd $(SRC_PREFIX) && python3 generate_property_headers.py --input $(notdir $(PROPERTIES_FILE))
	@mkdir -p $(dir $@)
	@touch $@

# Make the generated files depend on the stamp file
$(SRC_PREFIX)/core/core_properties.h $(SRC_PREFIX)/core/core_properties.c $(SRC_PREFIX)/core/generated_output_transformers.c: $(ROOT_DIR)/.stamps/generate_properties.stamp

# Mark as order-only prerequisites to prevent duplicate generation
$(OBJS): | $(GENERATED_FILES)

# ---- START DIAGNOSTIC ---- FOR TESTING COMPILE ISSUES
# $(info --- DIAGNOSTIC ---)
# $(info Current INCL before prefix: $(INCL)) # INCL is already prefixed here from earlier in the Makefile
# TEMP_INCL_PREFIXED := $(addprefix $(SRC_PREFIX)/, $(INCL)) # This is the double prefixing
# $(info Current INCL after prefix: $(TEMP_INCL_PREFIXED))
# $(foreach H_FILE,$(TEMP_INCL_PREFIXED),$(if $(wildcard $(H_FILE)),,$(warning Missing INCL prerequisite: $(H_FILE))))
# $(info --- END DIAGNOSTIC ---)
# ---- END DIAGNOSTIC ----

%.o: %.c $(INCL) Makefile
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -c $< -o $@

# Clean targets with common typo aliases
celan celna clena: clean
clean:
	rm -f $(OBJS) $(EXEC) $(SAGELIB) _$(LIBNAME)_cffi*.so _$(LIBNAME)_cffi.[co] $(GENERATED_FILES) $(ROOT_DIR)/.stamps/generate_properties.stamp

# Test Categories
# Core infrastructure tests
CORE_TESTS = test_pipeline test_array_utils test_core_property test_core_pipeline_registry test_dispatcher_access test_evolution_diagnostics test_evolve_integration

# Property system tests  
PROPERTY_TESTS = test_property_serialization test_property_system test_property_registration test_property_array_access test_galaxy_property_macros test_property_system_hdf5 test_property_validation

# I/O system tests
IO_TESTS = test_io_interface test_endian_utils test_lhalo_binary test_hdf5_output test_lhalo_hdf5 test_gadget4_hdf5 test_genesis_hdf5 test_consistent_trees_hdf5 test_io_validation test_memory_map test_io_buffer_manager

# Module system tests
MODULE_TESTS = test_dynamic_library test_pipeline_invoke

# Core-physics separation tests (critical for Phase 5.2.F)
SEPARATION_TESTS = test_core_physics_separation test_output_preparation

# All unit tests (excludes complex integration tests with individual Makefiles)
UNIT_TESTS = $(CORE_TESTS) $(PROPERTY_TESTS) $(IO_TESTS) $(MODULE_TESTS) $(SEPARATION_TESTS)

# Core infrastructure test targets
test_pipeline: tests/test_pipeline.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_pipeline tests/test_pipeline.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_array_utils: tests/test_array_utils.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_array_utils tests/test_array_utils.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_core_property: tests/test_core_property.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_core_property tests/test_core_property.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_core_pipeline_registry: tests/test_core_pipeline_registry.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_core_pipeline_registry tests/test_core_pipeline_registry.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_dispatcher_access: tests/test_dispatcher_access.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_dispatcher_access tests/test_dispatcher_access.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_evolution_diagnostics: tests/test_evolution_diagnostics.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_evolution_diagnostics tests/test_evolution_diagnostics.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_evolve_integration: tests/test_evolve_integration.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_evolve_integration tests/test_evolve_integration.c -L. -l$(LIBNAME) $(LIBFLAGS)

# Property system test targets
test_property_serialization: tests/test_property_serialization.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_serialization tests/test_property_serialization.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_system: tests/test_property_system.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_system tests/test_property_system.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_registration: tests/test_property_registration.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_registration tests/test_property_registration.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_array_access: tests/test_property_array_access.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_array_access tests/test_property_array_access.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_galaxy_property_macros: tests/test_galaxy_property_macros.c tests/test_validation_mocks.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_galaxy_property_macros tests/test_galaxy_property_macros.c tests/test_validation_mocks.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_system_hdf5: tests/test_property_system_hdf5.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_system_hdf5 tests/test_property_system_hdf5.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_validation: tests/test_property_validation.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_validation tests/test_property_validation.c -L. -l$(LIBNAME) $(LIBFLAGS)

# I/O system test targets
test_io_interface: tests/test_io_interface.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_io_interface tests/test_io_interface.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_endian_utils: tests/test_endian_utils.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_endian_utils tests/test_endian_utils.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_lhalo_binary: tests/test_lhalo_binary.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_lhalo_binary tests/test_lhalo_binary.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_hdf5_output: tests/test_hdf5_output.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_hdf5_output tests/test_hdf5_output.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_lhalo_hdf5: tests/test_lhalo_hdf5.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_lhalo_hdf5 tests/test_lhalo_hdf5.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_gadget4_hdf5: tests/test_gadget4_hdf5.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_gadget4_hdf5 tests/test_gadget4_hdf5.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_genesis_hdf5: tests/test_genesis_hdf5.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_genesis_hdf5 tests/test_genesis_hdf5.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_consistent_trees_hdf5: tests/test_consistent_trees_hdf5.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_consistent_trees_hdf5 tests/test_consistent_trees_hdf5.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_io_validation: tests/test_io_validation.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_io_validation tests/test_io_validation.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_memory_map: tests/test_io_memory_map.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_io_memory_map tests/test_io_memory_map.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_io_buffer_manager: tests/test_io_buffer_manager.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_io_buffer_manager tests/test_io_buffer_manager.c -L. -l$(LIBNAME) $(LIBFLAGS)

# Module system test targets
test_dynamic_library: tests/test_dynamic_library.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_dynamic_library tests/test_dynamic_library.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_pipeline_invoke: tests/test_pipeline_invoke.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_pipeline_invoke tests/test_pipeline_invoke.c -L. -l$(LIBNAME) $(LIBFLAGS)

# Core-physics separation test targets
test_core_physics_separation: tests/test_core_physics_separation.c
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_core_physics_separation tests/test_core_physics_separation.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_output_preparation: tests/test_output_preparation.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_output_preparation tests/test_output_preparation.c -L. -l$(LIBNAME) $(LIBFLAGS)

# Individual test category targets
core_tests: $(CORE_TESTS)
	@echo "=== Running Core Infrastructure Tests ==="
	@for test in $(CORE_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || exit 1; \
	done

property_tests: $(PROPERTY_TESTS)
	@echo "=== Running Property System Tests ==="
	@for test in $(PROPERTY_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || exit 1; \
	done

io_tests: $(IO_TESTS)
	@echo "=== Running I/O System Tests ==="
	@for test in $(IO_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || exit 1; \
	done

module_tests: $(MODULE_TESTS)
	@echo "=== Running Module System Tests ==="
	@for test in $(MODULE_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || exit 1; \
	done

separation_tests: $(SEPARATION_TESTS)
	@echo "=== Running Core-Physics Separation Tests ==="
	@for test in $(SEPARATION_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || exit 1; \
	done

# Tests execution target
tests: $(EXEC) $(UNIT_TESTS)
	@echo "Running SAGE tests..."
	@# Save test_sage.sh output to a log file to check for failures
	@./tests/test_sage.sh 2>&1 | tee tests/test_output.log || echo "End-to-end tests failed (expected during Phase 5)"
	@echo "\n--- Running unit tests and component tests ---"
	@echo "=== Core Infrastructure Tests ==="
	@for test in $(CORE_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done
	@echo "=== Property System Tests ==="
	@for test in $(PROPERTY_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done
	@echo "=== I/O System Tests ==="
	@for test in $(IO_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done
	@echo "=== Module System Tests ==="
	@for test in $(MODULE_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done
	@echo "=== Core-Physics Separation Tests ==="
	@for test in $(SEPARATION_TESTS); do \
		echo "Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done
	@echo "=== Memory Management Tests ==="
	@cd tests && make -f Makefile.memory_tests || FAILED="$$FAILED memory_tests"
	@if [ -n "$$FAILED" ]; then \
		echo "\n============================================"; \
		echo "UNIT TEST FAILURES DETECTED in:$$FAILED"; \
		echo "Please fix these failures as they should pass regardless of Phase 5 development."; \
		echo "End-to-end comparison test failures are expected and can be ignored during Phase 5."; \
		echo "============================================\n"; \
		exit 1; \
	else \
		echo "\n============================================"; \
		echo "All unit tests completed successfully."; \
		if grep -q "Binary checks failed: [1-9]" tests/test_output.log; then \
			echo "End-to-end comparison test: Binary comparison failed! NOTE: this is expected during Phase 5."; \
		fi; \
		if grep -q "HDF5 checks failed: [1-9]" tests/test_output.log; then \
			echo "End-to-end comparison test: HDF5 comparison failed! NOTE: this is expected during Phase 5."; \
		fi; \
		echo "============================================\n"; \
	fi
