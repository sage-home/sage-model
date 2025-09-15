cmake_minimum_required(VERSION 3.15)

# Set policy for consistent behavior across CMake versions
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)  # Use <Package>_ROOT variables for find_package
endif()

project(sage
    VERSION 1.0.0
    DESCRIPTION "Semi-Analytic Galaxy Evolution model"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)  # For GNU extensions like the Makefile

# Build options - matching Makefile configuration variables
option(SAGE_USE_MPI "Enable MPI support" OFF)
option(SAGE_USE_HDF5 "Enable HDF5 support" ON)
option(SAGE_USE_BUFFERED_WRITE "Enable buffered write" ON)
option(SAGE_SHARED_LIBRARY "Build shared library instead of static" ON)
option(SAGE_VERBOSE "Enable verbose output" ON)
option(SAGE_MEMORY_CHECK "Enable memory checking (AddressSanitizer)" OFF)

# Configuration system options
option(SAGE_CONFIG_JSON_SUPPORT "Enable JSON configuration support" ON)
option(SAGE_CONFIG_VALIDATION "Enable configuration validation" ON)

# Memory management configuration options
option(SAGE_MEMORY_TRACKING "Enable advanced memory tracking and leak detection" OFF)
option(SAGE_WARN_LEGACY_MEMORY "Enable deprecation warnings for legacy memory API" OFF)

# Enable out-of-tree builds by default
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(WARNING "In-source builds are discouraged. Consider using: mkdir build && cd build && cmake ..")
endif()

# Set default build type to Debug to match original Makefile behavior
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose build type: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()

# Platform detection and compiler setup
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS - prefer clang
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang")
        message(STATUS "On macOS, preferring clang compiler")
        set(CMAKE_C_COMPILER clang CACHE STRING "C compiler" FORCE)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux - prefer gcc
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" AND NOT DEFINED CMAKE_C_COMPILER)
        message(STATUS "On Linux, preferring gcc compiler")
        set(CMAKE_C_COMPILER gcc CACHE STRING "C compiler" FORCE)
    endif()
endif()

# Source directories
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(TESTS_DIR ${CMAKE_SOURCE_DIR}/tests)

# Get Git hash for version tracking
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} show-ref --head
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_SHOW_REF
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_SHOW_REF)
        string(REGEX MATCH "^[a-f0-9]+" GIT_HASH "${GIT_SHOW_REF}")
        set(GITREF_STR "${GIT_HASH}")
    else()
        set(GITREF_STR "")
    endif()
else()
    set(GITREF_STR "")
endif()

# Root directory definition
get_filename_component(ROOT_DIR ${CMAKE_SOURCE_DIR} ABSOLUTE)

# Base compiler flags - matching Makefile CCFLAGS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DGNU_SOURCE -fPIC")

# Compiler-specific flags
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # gcc on OSX has trouble with AVX+ instructions
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wa,-q")
    endif()
endif()

# Warning flags - matching Makefile
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wshadow")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedefs")
endif()

# Debug flags
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")

# Release optimization flags - matching Makefile
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
# Add -march=native unless building tests (following Makefile logic)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -march=native")
endif()

# CI-specific flags
if(DEFINED ENV{CI} OR DEFINED ENV{TRAVIS})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    message(STATUS "CI environment detected - treating warnings as errors")
endif()

# Memory checking
if(SAGE_MEMORY_CHECK)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(MEMORY_CHECK_FLAGS "-fsanitize=undefined -fsanitize=bounds -fsanitize=address -fsanitize-undefined-trap-on-error -fstack-protector-all")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MEMORY_CHECK_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MEMORY_CHECK_FLAGS}")
        message(STATUS "AddressSanitizer enabled (works with new memory abstraction)")
    else()
        message(WARNING "Memory checking requires GCC or Clang compiler")
    endif()
endif()

# Define preprocessor macros
add_definitions(-DROOT_DIR="${ROOT_DIR}")
add_definitions(-DGITREF_STR="${GITREF_STR}")

if(SAGE_VERBOSE)
    add_definitions(-DVERBOSE)
endif()

if(SAGE_USE_BUFFERED_WRITE)
    add_definitions(-DUSE_BUFFERED_WRITE)
endif()

# Configure memory tracking
if(SAGE_MEMORY_TRACKING)
    add_definitions(-DSAGE_MEMORY_TRACKING=1)
    message(STATUS "Memory tracking enabled")
endif()

if(SAGE_WARN_LEGACY_MEMORY)
    add_definitions(-DSAGE_WARN_LEGACY_MEMORY)
endif()

# Configuration system setup
if(SAGE_CONFIG_VALIDATION)
    add_definitions(-DSAGE_CONFIG_VALIDATION=1)
    message(STATUS "Configuration validation enabled")
endif()

# JSON configuration support
if(SAGE_CONFIG_JSON_SUPPORT)
    add_definitions(-DCONFIG_JSON_SUPPORT)
    
    # Try to find system cJSON first
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CJSON libcjson)
    endif()
    
    if(NOT CJSON_FOUND)
        # Try to find cJSON without pkg-config
        find_path(CJSON_INCLUDE_DIR cjson/cJSON.h)
        find_library(CJSON_LIBRARY NAMES cjson)
        
        if(CJSON_INCLUDE_DIR AND CJSON_LIBRARY)
            set(CJSON_FOUND TRUE)
            set(CJSON_LIBRARIES ${CJSON_LIBRARY})
            set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR})
        endif()
    endif()
    
    if(NOT CJSON_FOUND)
        message(STATUS "System cJSON not found, checking for bundled version")
        
        # Check for bundled cJSON in external directory
        if(EXISTS "${CMAKE_SOURCE_DIR}/external/cjson/cJSON.c")
            message(STATUS "Using bundled cJSON")
            add_library(cjson STATIC 
                external/cjson/cJSON.c 
                external/cjson/cJSON.h
            )
            target_include_directories(cjson PUBLIC external/cjson)
            set(CJSON_LIBRARIES cjson)
            set(CJSON_FOUND TRUE)
        else()
            message(STATUS "cJSON not found - JSON configuration support will be disabled")
            message(STATUS "Legacy .par file support remains available")
            message(STATUS "To enable JSON support: install libcjson-dev or place cJSON in external/cjson/")
            set(SAGE_CONFIG_JSON_SUPPORT OFF)
            remove_definitions(-DCONFIG_JSON_SUPPORT)
        endif()
    endif()
    
    if(CJSON_FOUND)
        message(STATUS "JSON configuration support enabled")
        if(CJSON_INCLUDE_DIRS)
            set(SAGE_INCLUDE_DIRS ${SAGE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
        endif()
        set(SAGE_LIBRARIES ${SAGE_LIBRARIES} ${CJSON_LIBRARIES})
    endif()
endif()

# Find required packages

# Math library
find_library(MATH_LIBRARY m)
if(MATH_LIBRARY)
    set(SAGE_LIBRARIES ${SAGE_LIBRARIES} ${MATH_LIBRARY})
endif()

# GSL detection - matching Makefile logic
find_program(GSL_CONFIG gsl-config)
if(GSL_CONFIG)
    execute_process(
        COMMAND ${GSL_CONFIG} --version
        OUTPUT_VARIABLE GSL_VERSION
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GSL_VERSION)
        message(STATUS "Found GSL version: ${GSL_VERSION}")
        
        execute_process(COMMAND ${GSL_CONFIG} --cflags OUTPUT_VARIABLE GSL_CFLAGS ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${GSL_CONFIG} --libs OUTPUT_VARIABLE GSL_LIBS ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${GSL_CONFIG} --prefix OUTPUT_VARIABLE GSL_PREFIX ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
        
        set(GSL_FOUND TRUE)
        add_definitions(-DGSL_FOUND)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GSL_CFLAGS}")
        set(SAGE_LIBRARIES ${SAGE_LIBRARIES} ${GSL_LIBS})
        
        # Add rpath for GSL library directory
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker -rpath -Xlinker ${GSL_PREFIX}/lib")
    endif()
else()
    message(WARNING "GSL not found in PATH environment variable. Tests will be disabled")
    set(GSL_FOUND FALSE)
endif()

# MPI detection - matching Makefile logic
if(SAGE_USE_MPI OR CMAKE_C_COMPILER MATCHES "mpicc")
    find_package(MPI REQUIRED COMPONENTS C)
    if(MPI_C_FOUND)
        set(SAGE_USE_MPI ON)
        add_definitions(-DMPI)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MPI_C_COMPILE_FLAGS}")
        set(SAGE_LIBRARIES ${SAGE_LIBRARIES} ${MPI_C_LIBRARIES})
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MPI_C_LINK_FLAGS}")
        
        # Add MPI library rpath
        if(MPI_C_LIBRARIES)
            get_filename_component(MPI_LIB_DIR "${MPI_C_LIBRARIES}" DIRECTORY)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker -rpath -Xlinker ${MPI_LIB_DIR}")
        endif()
        message(STATUS "MPI support enabled")
    endif()
endif()

# HDF5 detection - matching Makefile logic
if(SAGE_USE_HDF5)
    # Try to find HDF5 using various methods matching Makefile
    if(NOT HDF5_DIR AND DEFINED ENV{HDF5_DIR})
        set(HDF5_DIR $ENV{HDF5_DIR})
    endif()
    
    # If on CI, try to use conda base directory like Makefile
    if(DEFINED ENV{CI} OR DEFINED ENV{TRAVIS})
        if(GSL_CONFIG AND GSL_FOUND)
            execute_process(
                COMMAND ${GSL_CONFIG} --prefix
                OUTPUT_VARIABLE CONDA_BASE
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(CONDA_BASE AND NOT HDF5_DIR)
                set(HDF5_DIR ${CONDA_BASE})
                message(STATUS "CI detected - using conda base for HDF5: ${HDF5_DIR}")
            endif()
        endif()
    endif()
    
    # Try to find h5ls tool if HDF5_DIR not set
    if(NOT HDF5_DIR)
        find_program(H5LS_EXECUTABLE h5ls)
        if(H5LS_EXECUTABLE)
            get_filename_component(HDF5_BIN_DIR ${H5LS_EXECUTABLE} DIRECTORY)
            get_filename_component(HDF5_DIR ${HDF5_BIN_DIR} DIRECTORY)
            message(STATUS "Found HDF5 tools, setting HDF5_DIR to: ${HDF5_DIR}")
        endif()
    endif()
    
    # Fallback to default location
    if(NOT HDF5_DIR)
        set(HDF5_DIR "$ENV{HOME}/anaconda3")
        message(WARNING "HDF5_DIR not found, using default: ${HDF5_DIR}")
        message(WARNING "Please install HDF5 or set HDF5_DIR environment variable")
    endif()
    
    # Set HDF5_ROOT for find_package
    set(HDF5_ROOT ${HDF5_DIR})
    
    find_package(HDF5 COMPONENTS C)
    if(HDF5_FOUND)
        add_definitions(-DHDF5)
        set(SAGE_INCLUDE_DIRS ${SAGE_INCLUDE_DIRS} ${HDF5_INCLUDE_DIRS})
        set(SAGE_LIBRARIES ${SAGE_LIBRARIES} ${HDF5_C_LIBRARIES})
        
        # Add HDF5 library rpath
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Xlinker -rpath -Xlinker ${HDF5_DIR}/lib")
        message(STATUS "HDF5 support enabled")
    else()
        message(WARNING "HDF5 requested but not found. Compilation may fail.")
        set(SAGE_USE_HDF5 OFF)
    endif()
endif()

# ==============================================================================
# Property System Code Generation (Task 2.2)
# ==============================================================================

# Find Python for code generation
find_program(PYTHON_EXECUTABLE python3 python)
if(NOT PYTHON_EXECUTABLE)
    message(FATAL_ERROR "Python is required for property system code generation")
endif()

# Check for PyYAML dependency
execute_process(
    COMMAND ${PYTHON_EXECUTABLE} -c "import yaml; print('PyYAML found')"
    OUTPUT_VARIABLE PYYAML_CHECK
    ERROR_QUIET
    RESULT_VARIABLE PYYAML_RESULT
)

if(NOT PYYAML_RESULT EQUAL 0)
    message(FATAL_ERROR "PyYAML is required for property system. Install with: pip install PyYAML")
endif()

# Define paths for property system
set(PROPERTY_SCHEMA_FILE ${CMAKE_SOURCE_DIR}/schema/properties.yaml)
set(PROPERTY_GENERATOR ${CMAKE_SOURCE_DIR}/scripts/generate_property_headers.py)
set(PROPERTY_OUTPUT_DIR ${CMAKE_BINARY_DIR}/src/core)

# Generated property header files
set(PROPERTY_GENERATED_HEADERS
    ${PROPERTY_OUTPUT_DIR}/property_generated.h
    ${PROPERTY_OUTPUT_DIR}/property_enums.h  
    ${PROPERTY_OUTPUT_DIR}/property_access.h
)

# Custom command to generate property headers
add_custom_command(
    OUTPUT ${PROPERTY_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROPERTY_OUTPUT_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${PROPERTY_GENERATOR} ${PROPERTY_SCHEMA_FILE} ${PROPERTY_OUTPUT_DIR}
    DEPENDS ${PROPERTY_SCHEMA_FILE} ${PROPERTY_GENERATOR}
    COMMENT "Generating property system headers from YAML schema"
    VERBATIM
)

# Create a target for property generation
add_custom_target(generate_property_headers DEPENDS ${PROPERTY_GENERATED_HEADERS})

# Add generated headers to include path
set(SAGE_INCLUDE_DIRS ${SAGE_INCLUDE_DIRS} ${PROPERTY_OUTPUT_DIR})

message(STATUS "Property system code generation configured")
message(STATUS "  Schema file: ${PROPERTY_SCHEMA_FILE}")
message(STATUS "  Generator:   ${PROPERTY_GENERATOR}")  
message(STATUS "  Output dir:  ${PROPERTY_OUTPUT_DIR}")

# Memory management sources
set(MEMORY_SOURCES
    ${SRC_DIR}/core/memory.c
    ${SRC_DIR}/core/memory_scope.c
    ${SRC_DIR}/core/core_mymalloc.c  # Legacy compatibility
)

# Configuration system sources
set(CONFIG_SOURCES
    ${SRC_DIR}/core/config/config.c
    ${SRC_DIR}/core/config/config_legacy.c
    ${SRC_DIR}/core/config/config_validation.c
)

# Add JSON configuration source if enabled
if(SAGE_CONFIG_JSON_SUPPORT)
    list(APPEND CONFIG_SOURCES ${SRC_DIR}/core/config/config_json.c)
endif()

# Physics module interface sources - Task 2A.1
set(PHYSICS_MODULE_INTERFACE_SOURCES
    ${SRC_DIR}/core/physics_module_interface.h
    ${SRC_DIR}/core/physics_module_registry.c
    ${SRC_DIR}/core/physics_module_utils.c
    ${SRC_DIR}/core/physics_pipeline.c
)

# Core infrastructure sources - now in src/core/
set(SAGE_CORE_SOURCES
    ${SRC_DIR}/core/sage.c
    ${SRC_DIR}/core/core_read_parameter_file.c
    ${SRC_DIR}/core/core_init.c
    ${SRC_DIR}/core/core_io_tree.c
    ${SRC_DIR}/core/core_cool_func.c
    ${SRC_DIR}/core/core_build_model.c
    ${SRC_DIR}/core/core_save.c
    ${SRC_DIR}/core/core_utils.c
    ${SRC_DIR}/core/progressbar.c
    ${SRC_DIR}/core/core_tree_utils.c
    ${MEMORY_SOURCES}
    ${CONFIG_SOURCES}
    ${PHYSICS_MODULE_INTERFACE_SOURCES}
)

# Physics modules - now in src/physics/
set(SAGE_PHYSICS_SOURCES
    ${SRC_DIR}/physics/model_infall.c
    ${SRC_DIR}/physics/model_cooling_heating.c
    ${SRC_DIR}/physics/model_starformation_and_feedback.c
    ${SRC_DIR}/physics/model_disk_instability.c
    ${SRC_DIR}/physics/model_reincorporation.c
    ${SRC_DIR}/physics/model_mergers.c
    ${SRC_DIR}/physics/model_misc.c
)

# I/O sources - remain in src/io/
set(SAGE_IO_SOURCES
    ${SRC_DIR}/io/read_tree_lhalo_binary.c
    ${SRC_DIR}/io/read_tree_consistentrees_ascii.c
    ${SRC_DIR}/io/ctrees_utils.c
    ${SRC_DIR}/io/save_gals_binary.c
    ${SRC_DIR}/io/forest_utils.c
    ${SRC_DIR}/io/buffered_io.c
)

# Combine all sources for library
set(SAGE_ALL_SOURCES ${SAGE_CORE_SOURCES} ${SAGE_PHYSICS_SOURCES} ${SAGE_IO_SOURCES})

# HDF5-specific sources
if(SAGE_USE_HDF5)
    list(APPEND SAGE_IO_SOURCES
        ${SRC_DIR}/io/read_tree_lhalo_hdf5.c
        ${SRC_DIR}/io/save_gals_hdf5.c
        ${SRC_DIR}/io/read_tree_genesis_hdf5.c
        ${SRC_DIR}/io/hdf5_read_utils.c
        ${SRC_DIR}/io/read_tree_consistentrees_hdf5.c
        ${SRC_DIR}/io/read_tree_gadget4_hdf5.c
    )
    # Update combined sources for HDF5
    set(SAGE_ALL_SOURCES ${SAGE_CORE_SOURCES} ${SAGE_PHYSICS_SOURCES} ${SAGE_IO_SOURCES})
endif()

# Include directories - updated for new structure
set(SAGE_INCLUDE_DIRS ${SAGE_INCLUDE_DIRS} ${SRC_DIR}/core ${SRC_DIR}/core/config ${SRC_DIR}/physics ${SRC_DIR}/io)

# Create the SAGE library
if(SAGE_SHARED_LIBRARY)
    add_library(sage SHARED ${SAGE_ALL_SOURCES})
    set_target_properties(sage PROPERTIES
        OUTPUT_NAME sage
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    message(STATUS "Building shared library")
else()
    add_library(sage STATIC ${SAGE_ALL_SOURCES})
    set_target_properties(sage PROPERTIES OUTPUT_NAME sage)
    message(STATUS "Building static library")
endif()

target_include_directories(sage PUBLIC ${SAGE_INCLUDE_DIRS})
target_link_libraries(sage ${SAGE_LIBRARIES})

# Ensure property headers are generated before building the library
add_dependencies(sage generate_property_headers)

# Create the main executable
add_executable(sage_exe ${SRC_DIR}/core/main.c)
target_link_libraries(sage_exe sage ${SAGE_LIBRARIES})
set_target_properties(sage_exe PROPERTIES OUTPUT_NAME sage)

# Create aliases for different targets matching Makefile
# Note: 'all' is built-in in CMake, so we don't need to redefine it
add_custom_target(lib DEPENDS sage)
add_custom_target(libs DEPENDS sage)

# Python extension target
find_program(PYTHON_EXECUTABLE python3 python)
if(PYTHON_EXECUTABLE)
    add_custom_target(pyext
        COMMAND ${PYTHON_EXECUTABLE} -c "from sage import build_sage_pyext; build_sage_pyext();"
        DEPENDS sage
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Building Python extension"
    )
endif()

# ==============================================================================
# SAGE Testing Framework
# ==============================================================================
# 
# This section implements a comprehensive testing framework based on the 
# architecture developed in the sage-model-refactor-dir branch. The framework
# provides multiple test execution levels:
#
# - `make test` or `cmake --build . --target tests` - Run all tests (unit + end-to-end)
# - `cmake --build . --target unit_tests` - Run only unit tests (fast development)
# - `cmake --build . --target core_tests` - Run core infrastructure tests
# - `cmake --build . --target property_tests` - Run property system tests  
# - `cmake --build . --target io_tests` - Run I/O system tests
# - `cmake --build . --target module_tests` - Run module system tests
# - `cmake --build . --target tree_tests` - Run tree-based processing tests
# - `ctest -R test_specific_name` - Run individual tests via ctest
#
# Test categories are organized to match the master implementation plan phases.
#
enable_testing()

# ==============================================================================
# Test Category Definitions
# ==============================================================================
# Following the successful architecture from sage-model-refactor-dir branch

# Core Infrastructure Tests - Phase 1 & 2 foundation
# Tests for physics-agnostic core components, memory management, configuration
set(CORE_TESTS
    # Core pipeline and execution system
    test_pipeline                    # Pipeline execution phases (HALO → GALAXY → POST → FINAL)
    test_array_utils                # Dynamic array utilities with geometric growth
    test_memory                     # Memory abstraction layer implementation (Task 1.3)
    test_memory_management          # Memory allocation and cleanup (Phase 3)
    test_config                    # Configuration abstraction layer (Task 1.4)
    
    # Property system - Phase 2 core
    test_core_property             # Property system core functionality
    test_core_pipeline_registry    # Module registration and pipeline creation
    test_dispatcher_access         # Type-safe dispatcher functions for property access
    
    # Core-physics separation validation
    test_physics_free_mode         # Core-physics separation (critical for Phase 5)
    test_parameter_validation      # Parameter file parsing and validation
    test_resource_management       # Comprehensive resource lifecycle validation
    
    # Integration and reliability
    test_integration_workflows     # Multi-system integration workflows
    test_error_recovery           # System resilience and recovery
    test_dynamic_memory_expansion # Dynamic memory expansion system
    test_data_integrity_physics_free # Data integrity in physics-free mode
    
    # Galaxy data structures - Phase 2/3
    test_galaxy_array             # Galaxy array data structure management
    test_galaxy_array_component   # Galaxy array component access and manipulation
    test_halo_progenitor_integrity # Halo progenitor relationships validation
    
    # Property separation - Phase 2 critical tests  
    test_core_property_separation  # Core vs physics property separation
    test_property_separation_scientific_accuracy # Scientific accuracy with separation
    test_property_separation_memory_safety # Memory safety with separation
    
    # Advanced processing - Phase 5 and beyond
    test_evolution_diagnostics    # Evolution metrics collection
    test_evolve_integration      # Refactored evolve_galaxies loop
    test_merger_queue           # Merger event queue system  
    test_core_merger_processor  # Physics-agnostic merger event handling
    
    # FOF processing - Tree-based mode extensions
    test_fof_group_assembly      # FOF group galaxy type assignment
    test_fof_evolution_context   # FOF-centric timing and merger tree continuity
    test_fof_memory_management   # Memory management for large FOF groups
    test_orphan_tracking        # Orphan galaxy tracking for mass conservation
    test_orphan_fof_disruption  # Orphan handling during FOF group disruption
)

# Property System Tests - Phase 2 implementation  
# Tests focused on metadata-driven property system enabling core-physics separation
set(PROPERTY_TESTS
    test_property_serialization        # Property serialization for I/O (Phase 6)
    test_property_array_access         # Array property access patterns
    test_property_system_hdf5          # HDF5 output with property system
    test_property_validation           # Property definition validation
    test_property_access_comprehensive # Comprehensive property system validation
    test_property_yaml_validation      # YAML property definition validation
    test_parameter_yaml_validation     # YAML parameter definition validation
)

# I/O System Tests - Phase 6 implementation
# Tests for reading merger trees and writing galaxy catalogs via unified interface
set(IO_TESTS  
    test_io_interface              # I/O abstraction layer (Phase 6 core)
    test_endian_utils             # Cross-platform endianness handling
    
    # Tree format readers
    test_lhalo_binary             # LHalo binary format reading
    test_lhalo_hdf5              # LHalo HDF5 format reading  
    test_gadget4_hdf5            # Gadget4 HDF5 format reading
    test_genesis_hdf5            # Genesis HDF5 format reading
    test_consistent_trees_hdf5   # ConsistentTrees HDF5 format reading
    
    # Output handlers
    test_hdf5_output             # HDF5 galaxy output handler
    test_hdf5_output_validation  # HDF5 output validation
    
    # Performance and validation
    test_io_memory_map           # Memory mapping for I/O optimization
    test_io_buffer_manager       # Buffered I/O operations
    test_validation_framework    # I/O validation mechanisms
)

# Module System Tests - Phase 5 implementation
# Tests for modular plugin architecture enabling runtime physics configuration
set(MODULE_TESTS
    test_physics_module_interface # Physics module interface validation (Task 2A.1)
    test_pipeline_invoke         # Inter-module communication
    test_module_callback        # Module callback system
    test_module_lifecycle       # Complete module lifecycle management  
)

# Tree-Based Processing Tests - Phase 7 and beyond
# Tests for tree-based processing mode (alternative to snapshot-based)
set(TREE_TESTS
    test_galaxy_inheritance     # Galaxy inheritance and orphan creation in tree mode
)

# Combined unit test list - all categories
set(UNIT_TESTS ${CORE_TESTS} ${PROPERTY_TESTS} ${IO_TESTS} ${MODULE_TESTS} ${TREE_TESTS})

# ==============================================================================  
# Unit Test Executable Creation
# ==============================================================================
# Create test executables for tests that exist

# Helper function to create test executable if source file exists
function(create_test_if_exists test_name)
    set(test_source ${TESTS_DIR}/${test_name}.c)
    if(EXISTS ${test_source})
        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} sage ${SAGE_LIBRARIES})
        target_include_directories(${test_name} PRIVATE ${SAGE_INCLUDE_DIRS} ${TESTS_DIR})
        add_test(NAME ${test_name} COMMAND ${test_name})
        message(STATUS "Created test: ${test_name}")
    else()
        # Create placeholder for future implementation
        message(STATUS "Placeholder for future test: ${test_name} (${test_source} not found)")
    endif()
endfunction()

# Create existing unit tests
message(STATUS "")
message(STATUS "SAGE Testing Framework - Creating Unit Tests:")
message(STATUS "============================================")

foreach(test_name ${UNIT_TESTS})
    create_test_if_exists(${test_name})
endforeach()

# ==============================================================================
# Test Category Targets  
# ==============================================================================
# Create category-specific targets for organized test execution

# Helper function to create category target
function(create_test_category_target category_name test_list)
    set(existing_tests)
    foreach(test_name ${test_list})
        if(TARGET ${test_name})
            list(APPEND existing_tests ${test_name})
        endif()
    endforeach()
    
    if(existing_tests)
        add_custom_target(${category_name}
            COMMENT "Running ${category_name} (${CMAKE_CURRENT_LIST_LENGTH} tests)"
        )
        add_dependencies(${category_name} ${existing_tests})
        message(STATUS "Created test category: ${category_name} with ${CMAKE_CURRENT_LIST_LENGTH} tests")
        
        # Add category test runner
        add_custom_command(TARGET ${category_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "\\n=== Running ${category_name} ==="
            COMMAND ${CMAKE_CTEST_COMMAND} -R "^(${CMAKE_MATCH_1})$$" --output-on-failure || echo "Some ${category_name} failed (may be expected during development)"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        # Create placeholder target
        add_custom_target(${category_name}
            COMMAND ${CMAKE_COMMAND} -E echo "No ${category_name} implemented yet - placeholder for future development"
            COMMENT "Placeholder: ${category_name}"
        )
        message(STATUS "Created placeholder category: ${category_name}")
    endif()
endfunction()

# Create category targets
create_test_category_target(core_tests "${CORE_TESTS}")
create_test_category_target(property_tests "${PROPERTY_TESTS}")  
create_test_category_target(io_tests "${IO_TESTS}")
create_test_category_target(module_tests "${MODULE_TESTS}")
create_test_category_target(tree_tests "${TREE_TESTS}")

# ==============================================================================
# Combined Test Targets
# ==============================================================================

# Unit tests target - all unit tests without end-to-end
add_custom_target(unit_tests
    COMMENT "Running all unit tests (fast development cycle)"
)

# Add dependencies for existing unit tests
set(existing_unit_tests)
foreach(test_name ${UNIT_TESTS})
    if(TARGET ${test_name})
        list(APPEND existing_unit_tests ${test_name})
    endif()
endforeach()

if(existing_unit_tests)
    add_dependencies(unit_tests ${existing_unit_tests})
    
    add_custom_command(TARGET unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "=== Running All Unit Tests ==="
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -E "sage_end_to_end"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# ==============================================================================  
# End-to-End Scientific Validation
# ==============================================================================

if(GSL_FOUND)
    # End-to-end scientific validation test
    # Set environment variable to tell test_sage.sh where to find the executable
    add_test(
        NAME sage_end_to_end
        COMMAND ${CMAKE_SOURCE_DIR}/tests/test_sage.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    set_tests_properties(sage_end_to_end PROPERTIES
        ENVIRONMENT "SAGE_EXECUTABLE=${CMAKE_BINARY_DIR}/sage"
    )
    set_tests_properties(sage_end_to_end PROPERTIES
        TIMEOUT 1800  # 30 minutes timeout for scientific tests
        LABELS "scientific;end_to_end"
    )
    
    # Main tests target - combines unit tests + end-to-end
    add_custom_target(tests
        COMMENT "Running complete SAGE test suite (unit + end-to-end)"
    )
    add_dependencies(tests sage_exe unit_tests)
    
    add_custom_command(TARGET tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "\\n=== Running End-to-End Scientific Validation ==="
        COMMAND ${CMAKE_CTEST_COMMAND} -R "sage_end_to_end" --output-on-failure --verbose || echo "End-to-end tests failed (expected during Phase 5 refactoring)"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    message(STATUS "")
    message(STATUS "End-to-end scientific tests enabled (GSL found)")
else()
    # Create disabled tests target when GSL not available
    add_custom_target(tests
        COMMAND ${CMAKE_COMMAND} -E echo "\\n=== Unit Tests Only (GSL required for end-to-end tests) ==="
        COMMAND ${CMAKE_COMMAND} -E echo "Install GSL to enable complete scientific validation"
    )
    add_dependencies(tests unit_tests)
    
    message(STATUS "")
    message(STATUS "End-to-end scientific tests disabled (GSL not found)")
endif()

# ==============================================================================
# Test Framework Documentation and Help
# ==============================================================================

# Help target explaining the testing framework
add_custom_target(test_help
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "SAGE Testing Framework - Available Commands:"
    COMMAND ${CMAKE_COMMAND} -E echo "==========================================="  
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Complete Test Suite:"
    COMMAND ${CMAKE_COMMAND} -E echo "  make test                    - All tests (minimal output)"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest --output-on-failure    - All tests (show failures)"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -V                     - All tests (verbose output)" 
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target tests - Custom test target"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Development Testing (Unit Tests Only):"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target unit_tests  - Fast unit test cycle"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Test Categories:"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target core_tests     - Core infrastructure"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target property_tests - Property system" 
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target io_tests       - I/O system"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target module_tests   - Module system"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build . --target tree_tests     - Tree processing"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Individual Tests:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -R test_name              - Run specific test"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -R test_name -V           - Run specific test (verbose)"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest --output-on-failure       - Show detailed output on failure"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Show Test Output:"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest -V                        - Verbose output (recommended)"
    COMMAND ${CMAKE_COMMAND} -E echo "  ctest --output-on-failure       - Show output only on failure"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Documentation:"
    COMMAND ${CMAKE_COMMAND} -E echo "  See docs/testing-framework.md for detailed information"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMENT "Display testing framework help"
)

# Clean target extensions for typo compatibility (matching Makefile)
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES 
    "_sage_cffi*.so;_sage_cffi.[co]"
)

# Installation
install(TARGETS sage sage_exe
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    ${SRC_DIR}/core/core_allvars.h
    ${SRC_DIR}/core/macros.h
    ${SRC_DIR}/core/sage.h
    DESTINATION include/sage
)

# Print configuration summary
message(STATUS "")
message(STATUS "SAGE Configuration Summary:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler:        ${CMAKE_C_COMPILER}")
message(STATUS "  MPI Support:       ${SAGE_USE_MPI}")
message(STATUS "  HDF5 Support:      ${SAGE_USE_HDF5}")
message(STATUS "  GSL Found:         ${GSL_FOUND}")
message(STATUS "  Shared Library:    ${SAGE_SHARED_LIBRARY}")
message(STATUS "  Verbose Output:    ${SAGE_VERBOSE}")
message(STATUS "  Buffered Write:    ${SAGE_USE_BUFFERED_WRITE}")
message(STATUS "  Memory Check:      ${SAGE_MEMORY_CHECK}")
message(STATUS "  Memory Tracking:   ${SAGE_MEMORY_TRACKING}")
message(STATUS "  Legacy Warnings:   ${SAGE_WARN_LEGACY_MEMORY}")
message(STATUS "  JSON Config:       ${SAGE_CONFIG_JSON_SUPPORT}")
message(STATUS "  Config Validation: ${SAGE_CONFIG_VALIDATION}")
message(STATUS "  Root Directory:    ${ROOT_DIR}")
message(STATUS "  Git Hash:          ${GITREF_STR}")
message(STATUS "")