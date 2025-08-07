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
    set(MEMORY_CHECK_FLAGS "-fsanitize=undefined -fsanitize=bounds -fsanitize=address -fsanitize-undefined-trap-on-error -fstack-protector-all")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MEMORY_CHECK_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MEMORY_CHECK_FLAGS}")
    message(STATUS "Memory checking enabled")
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

# Core infrastructure sources - now in src/core/
set(SAGE_CORE_SOURCES
    ${SRC_DIR}/core/sage.c
    ${SRC_DIR}/core/core_read_parameter_file.c
    ${SRC_DIR}/core/core_init.c
    ${SRC_DIR}/core/core_io_tree.c
    ${SRC_DIR}/core/core_cool_func.c
    ${SRC_DIR}/core/core_build_model.c
    ${SRC_DIR}/core/core_save.c
    ${SRC_DIR}/core/core_mymalloc.c
    ${SRC_DIR}/core/core_utils.c
    ${SRC_DIR}/core/progressbar.c
    ${SRC_DIR}/core/core_tree_utils.c
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
set(SAGE_INCLUDE_DIRS ${SAGE_INCLUDE_DIRS} ${SRC_DIR}/core ${SRC_DIR}/physics ${SRC_DIR}/io)

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

# Test targets
if(GSL_FOUND)
    enable_testing()
    add_test(
        NAME sage_tests
        COMMAND ${CMAKE_SOURCE_DIR}/tests/test_sage.sh
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    add_custom_target(tests
        COMMAND ${CMAKE_CTEST_COMMAND}
        DEPENDS sage_exe
        COMMENT "Running SAGE tests"
    )
else()
    add_custom_target(tests
        COMMAND ${CMAKE_COMMAND} -E echo "GSL is required to run tests"
        COMMAND ${CMAKE_COMMAND} -E false
    )
endif()

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
message(STATUS "  Root Directory:    ${ROOT_DIR}")
message(STATUS "  Git Hash:          ${GITREF_STR}")
message(STATUS "")