#!/bin/bash
# SAGE CMake Build Wrapper
# Allows working from root directory with familiar Makefile-like commands

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Helper function for colored output
echo_info() { echo -e "${GREEN}[SAGE]${NC} $1"; }
echo_warn() { echo -e "${YELLOW}[SAGE]${NC} $1"; }
echo_error() { echo -e "${RED}[SAGE ERROR]${NC} $1"; }

# Ensure build directory exists and is configured
ensure_build_configured() {
    if [ ! -d "build" ]; then
        echo_info "Creating build directory..."
        mkdir build
        cd build
        echo_info "Configuring CMake..."
        cmake ..
        cd ..
    elif [ ! -f "build/Makefile" ]; then
        echo_info "Reconfiguring CMake..."
        cd build
        cmake ..
        cd ..
    fi
}

# Show usage information
show_usage() {
    echo "SAGE Build Wrapper - CMake commands from root directory"
    echo ""
    echo "Usage: ./build.sh [command]"
    echo ""
    echo "Build Commands:"
    echo "  ./build.sh              Build SAGE (default action)"
    echo "  ./build.sh clean        Clean build artifacts"
    echo "  ./build.sh rebuild      Complete rebuild (removes build/ directory)"
    echo "  ./build.sh lib          Build library only"
    echo ""
    echo "Testing Commands:"
    echo "  ./build.sh test         Run all tests (unit + end-to-end)"
    echo "  ./build.sh unit_tests   Run unit tests only (fast)"
    echo "  ./build.sh core_tests   Run core infrastructure tests"
    echo "  ./build.sh property_tests  Run property system tests"
    echo "  ./build.sh io_tests     Run I/O system tests"
    echo "  ./build.sh module_tests Run module system tests"  
    echo "  ./build.sh tree_tests   Run tree processing tests"
    echo ""
    echo "Configuration Commands:"
    echo "  ./build.sh configure    Reconfigure CMake"
    echo "  ./build.sh debug        Configure for debug build"
    echo "  ./build.sh release      Configure for release build"
    echo ""
    echo "Utility Commands:"
    echo "  ./build.sh help         Show this help"
    echo "  ./build.sh status       Show build status"
    echo ""
    echo "Examples:"
    echo "  ./build.sh && ./build/sage input/millennium.par"
    echo "  ./build.sh test"
    echo "  ./build.sh clean && ./build.sh debug && ./build.sh"
}

# Show build status
show_status() {
    echo_info "SAGE Build Status:"
    if [ -d "build" ]; then
        echo "  Build directory: ✓ exists"
        if [ -f "build/Makefile" ]; then
            echo "  CMake configured: ✓ yes"
        else
            echo "  CMake configured: ✗ no"
        fi
        if [ -f "build/sage" ]; then
            echo "  Executable built: ✓ yes"
        else
            echo "  Executable built: ✗ no"
        fi
        if [ -f "build/libsage.dylib" ] || [ -f "build/libsage.so" ]; then
            echo "  Library built: ✓ yes"
        else
            echo "  Library built: ✗ no"
        fi
    else
        echo "  Build directory: ✗ missing"
    fi
}

# Process command line arguments
case "$1" in
    "help"|"-h"|"--help")
        show_usage
        exit 0
        ;;
    "status")
        show_status
        exit 0
        ;;
    "clean")
        if [ -d "build" ]; then
            echo_info "Cleaning build artifacts..."
            cd build && make clean
        else
            echo_warn "No build directory to clean"
        fi
        ;;
    "rebuild")
        echo_info "Performing complete rebuild..."
        rm -rf build
        mkdir build
        cd build
        cmake ..
        make -j$(nproc)
        echo_info "Rebuild complete!"
        ;;
    "configure")
        echo_info "Reconfiguring CMake..."
        ensure_build_configured
        cd build && cmake ..
        ;;
    "debug")
        echo_info "Configuring debug build..."
        ensure_build_configured
        cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug -DSAGE_MEMORY_CHECK=ON
        ;;
    "release")
        echo_info "Configuring release build..."
        ensure_build_configured  
        cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
        ;;
    "lib"|"libs")
        ensure_build_configured
        echo_info "Building SAGE library..."
        cd build && make lib -j$(nproc)
        ;;
    "test")
        ensure_build_configured
        echo_info "Running complete test suite..."
        cd build && make test
        ;;
    "unit_tests")
        ensure_build_configured
        echo_info "Running unit tests..."
        cd build && cmake --build . --target unit_tests
        ;;
    "core_tests")
        ensure_build_configured
        echo_info "Running core tests..."
        cd build && cmake --build . --target core_tests
        ;;
    "property_tests")
        ensure_build_configured
        echo_info "Running property tests..."
        cd build && cmake --build . --target property_tests
        ;;
    "io_tests")
        ensure_build_configured
        echo_info "Running I/O tests..."
        cd build && cmake --build . --target io_tests
        ;;
    "module_tests")
        ensure_build_configured
        echo_info "Running module tests..."
        cd build && cmake --build . --target module_tests
        ;;
    "tree_tests")
        ensure_build_configured
        echo_info "Running tree tests..."
        cd build && cmake --build . --target tree_tests
        ;;
    "")
        # Default: build
        ensure_build_configured
        echo_info "Building SAGE..."
        cd build && make -j$(nproc)
        echo_info "Build complete! Run with: ./build/sage input/millennium.par"
        ;;
    *)
        echo_error "Unknown command: $1"
        echo ""
        show_usage
        exit 1
        ;;
esac