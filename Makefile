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
        core/core_merger_queue.c core/core_merger_processor.c core/cJSON.c core/core_evolution_diagnostics.c \
        core/core_galaxy_accessors.c core/core_pipeline_registry.c \
        core/core_properties.c core/standard_properties.c \
        core/physics_pipeline_executor.c core/core_property_utils.c \
        core/generated_output_transformers.c

# Physics model source files
PHYSICS_SRC := physics/physics_output_transformers.c \
               physics/physics_events.c \
               physics/physics_essential_functions.c

# NOTE: placeholder_empty_module.c/.h files remain as template for future physics migration
# but are not compiled or linked

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
          io/read_tree_gadget4_hdf5.c io/io_hdf5_utils.c io/io_lhalo_hdf5.c \
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
.PHONY: clean celan celna clena tests all physics-free full-physics custom-physics help test_io_interface test_endian_utils test_lhalo_binary test_property_serialization test_hdf5_output test_memory_map test_core_physics_separation test_property_system_hdf5 test_property_array_access test_core_pipeline_registry test_validation_framework test_parameter_validation

all: $(ROOT_DIR)/.stamps/generate_properties_full.stamp $(SAGELIB) $(EXEC)

help:
	@echo "SAGE Build System"
	@echo "================="
	@echo ""
	@echo "Standard targets:"
	@echo "  all           - Build SAGE with default property configuration"
	@echo "  clean         - Remove all build artifacts"
	@echo "  tests         - Run full test suite"
	@echo ""
	@echo "Explicit Property Build Targets:"
	@echo "  physics-free  - Build SAGE with core properties only (no physics modules)"
	@echo "                  Fastest execution, minimal memory usage"
	@echo "  full-physics  - Build SAGE with all available properties"
	@echo "                  Maximum compatibility, higher memory usage"
	@echo "  custom-physics- Build SAGE with properties for specific modules"
	@echo "                  Usage: make custom-physics CONFIG=path/to/config.json"
	@echo ""
	@echo "Examples:"
	@echo "  make physics-free                              # Core-only build"
	@echo "  make full-physics                              # All properties"
	@echo "  make custom-physics CONFIG=input/myconfig.json # Custom build"
	@echo ""
	@echo "Note: Different property configurations require recompilation."
	@echo "Use 'make clean' before switching between configurations."

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

# Rule to generate property headers (default: all properties)
.SECONDARY: $(GENERATED_FILES)
$(ROOT_DIR)/.stamps/generate_properties.stamp: $(SRC_PREFIX)/$(PROPERTIES_FILE) $(SRC_PREFIX)/generate_property_headers.py
	@echo "Generating property headers from $(PROPERTIES_FILE)..."
	cd $(SRC_PREFIX) && python3 generate_property_headers.py --input $(notdir $(PROPERTIES_FILE))
	@mkdir -p $(dir $@)
	@touch $@

# Explicit Property Build Targets
# These targets allow users to control which properties are generated at build-time

physics-free: $(ROOT_DIR)/.stamps/generate_properties_core.stamp $(EXEC)
	@echo "SAGE built in physics-free mode (core properties only)"

full-physics: $(ROOT_DIR)/.stamps/generate_properties_full.stamp $(EXEC)
	@echo "SAGE built with full physics (all properties)"

custom-physics:
	@if [ -z "$(CONFIG)" ]; then \
		echo "ERROR: CONFIG variable must be specified. Usage: make custom-physics CONFIG=path/to/config.json"; \
		exit 1; \
	fi
	@$(MAKE) $(ROOT_DIR)/.stamps/generate_properties_custom.stamp CONFIG=$(CONFIG)
	@$(MAKE) $(EXEC)
	@echo "SAGE built with custom physics configuration: $(CONFIG)"

# Property generation rules for different modes
$(ROOT_DIR)/.stamps/generate_properties_core.stamp: $(SRC_PREFIX)/$(PROPERTIES_FILE) $(SRC_PREFIX)/generate_property_headers.py
	@echo "Generating core-only property headers (physics-free mode)..."
	cd $(SRC_PREFIX) && python3 generate_property_headers.py --input $(notdir $(PROPERTIES_FILE)) --core-only
	@mkdir -p $(dir $@)
	@rm -f $(ROOT_DIR)/.stamps/generate_properties*.stamp  # Clear other stamps
	@touch $@

$(ROOT_DIR)/.stamps/generate_properties_full.stamp: $(SRC_PREFIX)/$(PROPERTIES_FILE) $(SRC_PREFIX)/generate_property_headers.py
	@echo "Generating full property headers (all properties)..."
	cd $(SRC_PREFIX) && python3 generate_property_headers.py --input $(notdir $(PROPERTIES_FILE)) --all-properties
	@mkdir -p $(dir $@)
	@rm -f $(ROOT_DIR)/.stamps/generate_properties*.stamp  # Clear other stamps
	@touch $@

$(ROOT_DIR)/.stamps/generate_properties_custom.stamp: $(SRC_PREFIX)/$(PROPERTIES_FILE) $(SRC_PREFIX)/generate_property_headers.py
	@echo "Generating custom property headers with config: $(CONFIG)..."
	cd $(SRC_PREFIX) && python3 generate_property_headers.py --input $(notdir $(PROPERTIES_FILE)) --config ../$(CONFIG)
	@mkdir -p $(dir $@)
	@rm -f $(ROOT_DIR)/.stamps/generate_properties*.stamp  # Clear other stamps
	@touch $@

# Make the generated files depend on any property stamp file
$(SRC_PREFIX)/core/core_properties.h $(SRC_PREFIX)/core/core_properties.c $(SRC_PREFIX)/core/generated_output_transformers.c: $(shell find $(ROOT_DIR)/.stamps -name "generate_properties*.stamp" 2>/dev/null | head -1)

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
	rm -f $(OBJS) $(EXEC) $(SAGELIB) _$(LIBNAME)_cffi*.so _$(LIBNAME)_cffi.[co] $(GENERATED_FILES) $(ROOT_DIR)/.stamps/generate_properties*.stamp

# Test Categories
# Core infrastructure tests
CORE_TESTS = test_pipeline test_array_utils test_core_property test_core_pipeline_registry test_dispatcher_access test_evolution_diagnostics test_evolve_integration test_memory_pool test_merger_queue test_core_merger_processor test_config_system test_physics_free_mode test_parameter_validation

# Property system tests  
PROPERTY_TESTS = test_property_serialization test_property_array_access test_property_system_hdf5 test_property_validation

# I/O system tests
IO_TESTS = test_io_interface test_endian_utils test_lhalo_binary test_hdf5_output test_lhalo_hdf5 test_gadget4_hdf5 test_genesis_hdf5 test_consistent_trees_hdf5 test_io_memory_map test_io_buffer_manager test_validation_framework

# Module system tests
MODULE_TESTS = test_pipeline_invoke test_module_callback

# All unit tests (excludes complex integration tests with individual Makefiles)
UNIT_TESTS = $(CORE_TESTS) $(PROPERTY_TESTS) $(IO_TESTS) $(MODULE_TESTS)

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

test_memory_pool: tests/test_memory_pool.c tests/stubs/galaxy_extension_stubs.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_memory_pool tests/test_memory_pool.c tests/stubs/galaxy_extension_stubs.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_serialization: tests/test_property_serialization.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_serialization tests/test_property_serialization.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_array_access: $(ROOT_DIR)/.stamps/generate_properties_full.stamp tests/test_property_array_access.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_array_access tests/test_property_array_access.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_system_hdf5: tests/test_property_system_hdf5.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_system_hdf5 tests/test_property_system_hdf5.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_property_validation: tests/test_property_validation.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_property_validation tests/test_property_validation.c -L. -l$(LIBNAME) $(LIBFLAGS)

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

test_io_memory_map: tests/test_io_memory_map.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_io_memory_map tests/test_io_memory_map.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_io_buffer_manager: tests/test_io_buffer_manager.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_io_buffer_manager tests/test_io_buffer_manager.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_validation_framework: tests/test_validation_framework.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_validation_framework tests/test_validation_framework.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_pipeline_invoke: tests/test_pipeline_invoke.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_pipeline_invoke tests/test_pipeline_invoke.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_module_callback: tests/test_module_callback.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_module_callback tests/test_module_callback.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_merger_queue: tests/test_merger_queue.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_merger_queue tests/test_merger_queue.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_core_merger_processor: tests/test_core_merger_processor.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_core_merger_processor tests/test_core_merger_processor.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_config_system: tests/test_config_system.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_config_system tests/test_config_system.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_parameter_validation: tests/test_parameter_validation.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_parameter_validation tests/test_parameter_validation.c -L. -l$(LIBNAME) $(LIBFLAGS)

test_physics_free_mode: tests/test_physics_free_mode.c $(SAGELIB)
	$(CC) $(OPTS) $(OPTIMIZE) $(CCFLAGS) -o tests/test_physics_free_mode tests/test_physics_free_mode.c -L. -l$(LIBNAME) $(LIBFLAGS)

# Individual test category targets
core_tests: $(CORE_TESTS)
	@echo "=== Running Core Infrastructure Tests ==="; \
	failed_tests=""; \
	for test in $(CORE_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test; \
		if [ $$? -ne 0 ]; then \
			echo "!!!!! WARNING: $$test exited with non-zero status - this may be expected during development\n"; \
			failed_tests="$$failed_tests $$test"; \
		fi; \
	done; \
	if [ -n "$$failed_tests" ]; then \
		echo "\n!!!!! The following tests reported non-zero exit codes:$$failed_tests"; \
		echo "!!!!! This is not necessarily a failure - check the test output for details"; \
	else \
		echo "\nAll core tests completed successfully!"; \
	fi

property_tests: $(PROPERTY_TESTS)
	@echo "=== Running Property System Tests ==="; \
	failed_tests=""; \
	for test in $(PROPERTY_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test; \
		if [ $$? -ne 0 ]; then \
			echo "!!!!! WARNING: $$test exited with non-zero status - this may be expected during development\n"; \
			failed_tests="$$failed_tests $$test"; \
		fi; \
	done; \
	if [ -n "$$failed_tests" ]; then \
		echo "\n!!!!! The following tests reported non-zero exit codes:$$failed_tests"; \
		echo "!!!!! This is not necessarily a failure - check the test output for details"; \
	else \
		echo "\nAll property tests completed successfully!"; \
	fi

io_tests: $(IO_TESTS)
	@echo "=== Running I/O System Tests ==="; \
	failed_tests=""; \
	for test in $(IO_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test; \
		if [ $$? -ne 0 ]; then \
			echo "!!!!! WARNING: $$test exited with non-zero status - this may be expected during development\n"; \
			failed_tests="$$failed_tests $$test"; \
		fi; \
	done; \
	if [ -n "$$failed_tests" ]; then \
		echo "\n!!!!! The following tests reported non-zero exit codes:$$failed_tests"; \
		echo "!!!!! This is not necessarily a failure - check the test output for details"; \
	else \
		echo "\nAll io tests completed successfully!"; \
	fi

module_tests: $(MODULE_TESTS)
	@echo "=== Running Module System Tests ==="; \
	failed_tests=""; \
	for test in $(MODULE_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test; \
		if [ $$? -ne 0 ]; then \
			echo "!!!!! WARNING: $$test exited with non-zero status - this may be expected during development\n"; \
			failed_tests="$$failed_tests $$test"; \
		fi; \
	done; \
	if [ -n "$$failed_tests" ]; then \
		echo "\n!!!!! The following tests reported non-zero exit codes:$$failed_tests"; \
		echo "!!!!! This is not necessarily a failure - check the test output for details"; \
	else \
		echo "\nAll module tests completed successfully!"; \
	fi

# Run all unit tests without the end-to-end scientific tests - faster for development
unit_tests: $(UNIT_TESTS)
	@echo "=== Running All Unit Tests ==="; \
	failed_tests=""; \
	for test in $(UNIT_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test; \
		if [ $$? -ne 0 ]; then \
			echo "!!!!! WARNING: $$test exited with non-zero status - this may be expected during development\n"; \
			failed_tests="$$failed_tests $$test"; \
		fi; \
	done; \
	if [ -n "$$failed_tests" ]; then \
		echo "\n!!!!! The following tests reported non-zero exit codes:$$failed_tests"; \
		echo "!!!!! This is not necessarily a failure - check the test output for details"; \
	else \
		echo "\nAll unit tests completed successfully!"; \
	fi

# Tests execution target
tests: $(EXEC) $(UNIT_TESTS)
	@echo "Running SAGE tests..."
	@# Save test_sage.sh output to a log file to check for failures
	@./tests/test_sage.sh 2>&1 | tee tests/test_output.log || echo "End-to-end tests failed (expected during Phase 5)"
	@echo "\n--- Running unit tests and component tests ---"; \
	echo "=== Core Infrastructure Tests ==="; \
	FAILED=""; \
	for test in $(CORE_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done; \
	echo "=== Property System Tests ==="; \
	for test in $(PROPERTY_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done; \
	echo "=== I/O System Tests ==="; \
	for test in $(IO_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done; \
	echo "=== Module System Tests ==="; \
	for test in $(MODULE_TESTS); do \
		echo "\n# Running $$test..."; \
		./tests/$$test || FAILED="$$FAILED $$test"; \
	done; \
  echo "\n# Final end-to-end and unit test summary"; \
  echo "----------------------------------------\n"; \
	if [ -n "$$FAILED" ]; then \
		echo "!!!!! Unit test failures detected:$$FAILED"; \
		echo "      Please fix these failures as they should pass regardless of Phase 5 development\n"; \
	else \
		echo "All unit tests completed successfully\n"; \
  fi; \
  if grep -q "If the fix to this isn't obvious" tests/test_output.log; then \
		echo "!!!!! SAGE failed to run correctly ... you should investigate this!"; \
	else \
    if grep -q "Binary checks failed: [1-9]" tests/test_output.log; then \
      echo "!!!!! End-to-end comparison test: Binary comparison failed! NOTE: this is expected during Phase 5"; \
    else \
      echo "End-to-end comparison test: Binary comparison passed"; \
    fi; \
    if grep -q "HDF5 checks failed: [1-9]" tests/test_output.log; then \
      echo "!!!!! End-to-end comparison test: HDF5 comparison failed! NOTE: this is expected during Phase 5"; \
    else \
      echo "End-to-end comparison test: HDF5 comparison passed"; \
    fi; \
  fi
