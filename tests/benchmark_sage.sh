#!/bin/bash
#
# benchmark_sage.sh - Performance benchmark script for SAGE
#
# This script provides comprehensive performance benchmarking for the SAGE
# galaxy formation model. It measures runtime, memory usage, and other
# performance metrics to help developers track performance changes over time.
#
# USAGE:
#   ./benchmark_sage.sh             # Run with default settings
#   ./benchmark_sage.sh --verbose   # Run with detailed output
#   ./benchmark_sage.sh --help      # Show help information
#   ./benchmark_sage.sh --format hdf5  # Benchmark HDF5 output (default: binary)
#
# REQUIREMENTS:
#   - Must be run from the tests/ directory
#   - CMake build system must be available
#   - Mini-Millennium test data (automatically downloaded if missing)
#   - bc calculator (for memory calculations)
#   - time command (for performance measurement)
#
# OUTPUT:
#   Results are stored in JSON format in the benchmarks/ directory
#   with timestamp-based filenames for easy comparison across runs.
#
# ENVIRONMENT VARIABLES:
#   SAGE_EXECUTABLE  - Override sage executable location
#   MPI_RUN_COMMAND  - Run with MPI (e.g., "mpirun -np 4")
#   CMAKE_BUILD_TYPE - Build type for CMake (Debug, Release, etc.)
#
# EXAMPLES:
#   # Basic benchmark
#   ./benchmark_sage.sh
#   
#   # Benchmark with MPI
#   MPI_RUN_COMMAND="mpirun -np 4" ./benchmark_sage.sh
#   
#   # Release build benchmark
#   CMAKE_BUILD_TYPE=Release ./benchmark_sage.sh
#   
#   # Compare two benchmark runs
#   diff benchmarks/baseline_20250101_120000.json benchmarks/baseline_20250102_120000.json
#

set -e  # Exit on any error

# Configuration
BENCHMARK_DIR="../benchmarks"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
BENCHMARK_RESULTS="${BENCHMARK_DIR}/baseline_${TIMESTAMP}.json"
VERBOSE=0
SHOW_HELP=0
OUTPUT_FORMAT="binary"  # Default to binary format

# Process command line arguments
for arg in "$@"; do
    case $arg in
        --help)
            SHOW_HELP=1
            shift
            ;;
        --verbose)
            VERBOSE=1
            shift
            ;;
        --format)
            shift
            OUTPUT_FORMAT="$1"
            shift
            ;;
        --format=*)
            OUTPUT_FORMAT="${arg#*=}"
            shift
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Usage: $0 [--help] [--verbose] [--format binary|hdf5]"
            exit 1
            ;;
    esac
done

# Show help if requested
if [ $SHOW_HELP -eq 1 ]; then
    cat << 'EOF'
Usage: ./benchmark_sage.sh [OPTIONS]

OPTIONS:
  --help                Show this help message
  --verbose             Run with detailed output and timing information
  --format FORMAT       Output format to benchmark (binary or hdf5, default: binary)

PURPOSE:
  This script establishes performance baselines for SAGE galaxy formation
  model runs. It captures comprehensive metrics including:
  - Wall clock execution time
  - Maximum memory usage (RSS)
  - System information and git version
  - Test case configuration
  
  Results are stored in timestamped JSON files for easy comparison
  between different versions, configurations, or optimizations.

OUTPUT:
  Results are stored in ../benchmarks/ directory with the format:
  baseline_YYYYMMDD_HHMMSS.json
  
  JSON structure includes:
  - timestamp: ISO 8601 timestamp
  - version: Git commit hash/tag
  - system: System information
  - test_case: Test configuration used
  - overall_performance: Runtime and memory metrics
  - configuration: Build and runtime configuration

EXAMPLES:
  # Basic benchmark
  ./benchmark_sage.sh
  
  # Verbose output with HDF5 format
  ./benchmark_sage.sh --verbose --format hdf5
  
  # MPI benchmark
  MPI_RUN_COMMAND="mpirun -np 4" ./benchmark_sage.sh
  
  # Release build benchmark
  CMAKE_BUILD_TYPE=Release ./benchmark_sage.sh

COMPARING RESULTS:
  # Simple diff
  diff benchmarks/baseline_old.json benchmarks/baseline_new.json
  
  # Extract key metrics
  grep "real_time\|max_memory" benchmarks/baseline_*.json
  
  # Plot trends over time (requires custom analysis script)
  python plot_benchmark_trends.py benchmarks/

TROUBLESHOOTING:
  - Ensure you're running from the tests/ directory
  - Check that CMake and build tools are installed
  - Verify test data exists (script will download if missing)
  - For memory measurement issues, check 'time' command availability
  - For MPI issues, verify MPI installation and mpirun availability
EOF
    exit 0
fi

# Validate output format
if [[ "$OUTPUT_FORMAT" != "binary" && "$OUTPUT_FORMAT" != "hdf5" ]]; then
    echo "ERROR: Invalid output format '$OUTPUT_FORMAT'. Must be 'binary' or 'hdf5'."
    exit 1
fi

# Determine script directories - we should be in tests/ directory
SCRIPT_DIR=$(pwd)
if [[ ! "$(basename "$SCRIPT_DIR")" == "tests" ]]; then
    echo "ERROR: This script must be run from the tests/ directory"
    echo "Current directory: $SCRIPT_DIR"
    echo "Please cd to the tests/ directory and run ./benchmark_sage.sh"
    exit 1
fi

ROOT_DIR="$(dirname "$SCRIPT_DIR")"
TEST_DATA_DIR="${SCRIPT_DIR}/test_data"
BUILD_DIR="${ROOT_DIR}/build"

# Create benchmark directory if it doesn't exist
mkdir -p "${ROOT_DIR}/benchmarks"

# Function to display error and exit
error_exit() {
    echo "ERROR: $1"
    echo "Benchmark failed."
    exit 1
}

# Function to log verbose output
verbose_log() {
    if [ $VERBOSE -eq 1 ]; then
        echo "[VERBOSE] $1"
    fi
}

echo "=== SAGE Performance Benchmark ==="
echo "Timestamp: $(date)"
echo "Output format: $OUTPUT_FORMAT"
echo "Saving results to: ${ROOT_DIR}/benchmarks/$(basename "$BENCHMARK_RESULTS")"
echo

# Check if test data exists, download if needed
verbose_log "Checking for test data..."
if [ ! -f "${TEST_DATA_DIR}/trees_063.7" ] || [ ! -f "${TEST_DATA_DIR}/mini-millennium.par" ]; then
    echo "Test data not found. Downloading Mini-Millennium data..."
    
    # Use the same download logic as test_sage.sh
    cd "${TEST_DATA_DIR}" || error_exit "Could not change to test data directory"
    
    # Check for wget or curl
    clear_alias=0
    command -v wget > /dev/null 2>&1
    if [[ $? != 0 ]]; then
        verbose_log "wget not available, checking for curl"
        command -v curl > /dev/null 2>&1
        if [[ $? != 0 ]]; then
            error_exit "Neither wget nor curl available for downloading test data"
        fi
        verbose_log "Using curl to download"
        alias wget="curl -L -O -C -"
        clear_alias=1
    else
        verbose_log "Using wget to download"
    fi
    
    # Download tree files if needed
    if [ ! -f "trees_063.7" ]; then
        verbose_log "Downloading tree files..."
        wget "https://www.dropbox.com/s/l5ukpo7ar3rgxo4/mini-millennium-treefiles.tar?dl=0" -O "mini-millennium-treefiles.tar"
        tar xf mini-millennium-treefiles.tar || error_exit "Failed to extract tree files"
    fi
    
    # Download correct output files if needed
    if [ ! -f "correct-mini-millennium-output_z0.000_0" ]; then
        verbose_log "Downloading reference output files..."
        wget "https://www.dropbox.com/s/mxvivrg19eu4v1f/mini-millennium-sage-correct-output.tar?dl=0" -O "mini-millennium-sage-correct-output.tar"
        tar xf mini-millennium-sage-correct-output.tar || error_exit "Failed to extract reference output"
    fi
    
    # Clean up alias if we used curl
    if [[ $clear_alias == 1 ]]; then
        unalias wget
    fi
    
    cd "$SCRIPT_DIR" || error_exit "Could not return to tests directory"
fi

# Build SAGE using direct CMake commands
echo "Building SAGE using CMake..."
cd "${ROOT_DIR}" || error_exit "Could not change to SAGE root directory"

# Ensure build directory exists
if [ ! -d "build" ]; then
    verbose_log "Creating build directory"
    mkdir -p build
fi

cd build || error_exit "Could not change to build directory"

# Configure with CMake if needed
if [ ! -f "Makefile" ]; then
    verbose_log "Configuring CMake..."
    cmake .. || error_exit "CMake configuration failed"
fi

# Clean and build
verbose_log "Cleaning previous build..."
make clean || error_exit "Make clean failed"

verbose_log "Building SAGE..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1) || error_exit "Build failed"

echo "Build successful."
echo

# Verify executable exists
SAGE_EXECUTABLE="${BUILD_DIR}/sage"
if [ ! -f "$SAGE_EXECUTABLE" ]; then
    error_exit "SAGE executable not found at $SAGE_EXECUTABLE"
fi

verbose_log "Using SAGE executable: $SAGE_EXECUTABLE"

# Create parameter file for benchmark
echo "Preparing benchmark run..."
PARAM_FILE=$(mktemp)
cp "${TEST_DATA_DIR}/mini-millennium.par" "${PARAM_FILE}" || error_exit "Could not copy parameter file"

# Set output format in parameter file
if [[ "$OUTPUT_FORMAT" == "hdf5" ]]; then
    sed -i.bak '/^OutputFormat /s/.*$/OutputFormat        sage_hdf5/' "${PARAM_FILE}"
else
    sed -i.bak '/^OutputFormat /s/.*$/OutputFormat        sage_binary/' "${PARAM_FILE}"
fi

# Ensure output goes to test_data directory
sed -i.bak '/^OutputDir /s|.*$|OutputDir              '"${TEST_DATA_DIR}/"'|' "${PARAM_FILE}"

# Clean any previous benchmark output
rm -f "${TEST_DATA_DIR}"/test_sage*

verbose_log "Parameter file configured for $OUTPUT_FORMAT output"
if [ $VERBOSE -eq 1 ]; then
    echo "Parameter file contents:"
    grep -E "^(OutputFormat|OutputDir|FileNameGalaxies)" "${PARAM_FILE}"
fi

# ======= Time and memory measurement =======
echo "Running SAGE benchmark (format: $OUTPUT_FORMAT)..."
cd "${ROOT_DIR}" || error_exit "Could not change to root directory"

# Get number of MPI processes for later use
NUM_SAGE_PROCS=$(echo ${MPI_RUN_COMMAND} | awk '{print $NF}')
if [[ -z "${NUM_SAGE_PROCS}" ]]; then
   NUM_SAGE_PROCS=1
fi

verbose_log "Running with $NUM_SAGE_PROCS processes"

# Platform-specific timing and memory measurement
if [ "$(uname)" = "Darwin" ]; then
    # macOS version
    verbose_log "Using macOS time measurement"
    start_time=$(date +%s.%N 2>/dev/null || date +%s)
    MEM_OUTPUT=$(mktemp)
    
    if [[ -n "${MPI_RUN_COMMAND}" ]]; then
        /usr/bin/time -l ${MPI_RUN_COMMAND} "${SAGE_EXECUTABLE}" "${PARAM_FILE}" > /dev/null 2> "$MEM_OUTPUT"
    else
        /usr/bin/time -l "${SAGE_EXECUTABLE}" "${PARAM_FILE}" > /dev/null 2> "$MEM_OUTPUT"
    fi
    
    end_time=$(date +%s.%N 2>/dev/null || date +%s)
    
    # Calculate elapsed time (handle both nanosecond and second precision)
    if command -v bc > /dev/null 2>&1; then
        REAL_TIME=$(echo "$end_time - $start_time" | bc)
    else
        REAL_TIME="$((end_time - start_time))"
    fi
    
    # Memory usage (convert bytes to MB)
    MAX_MEMORY=$(grep "maximum resident set size" "$MEM_OUTPUT" | awk '{print $1}')
    if command -v bc > /dev/null 2>&1 && [[ -n "$MAX_MEMORY" ]]; then
        MAX_MEMORY=$(echo "scale=2; ${MAX_MEMORY} / 1048576" | bc)
    else
        MAX_MEMORY=$(( ${MAX_MEMORY:-0} / 1048576 ))
    fi
    
    if [ $VERBOSE -eq 1 ]; then
        echo "Memory measurement output:"
        cat "$MEM_OUTPUT"
    fi
    
    rm -f "$MEM_OUTPUT"
    
else
    # Linux version
    verbose_log "Using Linux time measurement"
    TIME_OUTPUT=$(mktemp)
    
    if [[ -n "${MPI_RUN_COMMAND}" ]]; then
        {
            /usr/bin/time -v ${MPI_RUN_COMMAND} "${SAGE_EXECUTABLE}" "${PARAM_FILE}" 2>&1
        } > "${TIME_OUTPUT}"
    else
        {
            /usr/bin/time -v "${SAGE_EXECUTABLE}" "${PARAM_FILE}" 2>&1
        } > "${TIME_OUTPUT}"
    fi
    
    # Extract memory and time metrics
    REAL_TIME=$(grep "Elapsed (wall clock) time" "${TIME_OUTPUT}" | sed 's/.*: //' | sed 's/:.*//' | tr ':' ' ' | awk '{print $1*60+$2}')
    MAX_MEMORY=$(grep "Maximum resident set size" "${TIME_OUTPUT}" | awk '{print $6}')
    
    # Convert KB to MB for consistency
    if command -v bc > /dev/null 2>&1 && [[ -n "$MAX_MEMORY" ]]; then
        MAX_MEMORY=$(echo "scale=2; ${MAX_MEMORY} / 1024" | bc)
    else
        MAX_MEMORY=$(( ${MAX_MEMORY:-0} / 1024 ))
    fi
    
    if [ $VERBOSE -eq 1 ]; then
        echo "Time measurement output:"
        cat "${TIME_OUTPUT}"
    fi
    
    rm -f "${TIME_OUTPUT}"
fi

# Get system and build information
GIT_VERSION=$(git describe --always --dirty 2>/dev/null || echo 'unknown')
BUILD_TYPE=$(grep CMAKE_BUILD_TYPE "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || echo 'unknown')

# Verify output was created
if [[ "$OUTPUT_FORMAT" == "hdf5" ]]; then
    OUTPUT_FILE="${TEST_DATA_DIR}/test_sage.hdf5"
else
    OUTPUT_FILE="${TEST_DATA_DIR}/test_sage_z0.000_0"
fi

if [ ! -f "$OUTPUT_FILE" ]; then
    error_exit "SAGE did not produce expected output file: $OUTPUT_FILE"
fi

verbose_log "Benchmark run completed successfully"
verbose_log "Output file created: $OUTPUT_FILE"

# Clean up temporary files
rm -f "${PARAM_FILE}" "${PARAM_FILE}.bak"

# Create comprehensive JSON output
echo "Generating benchmark results..."

cat > "${ROOT_DIR}/benchmarks/baseline_${TIMESTAMP}.json" << EOF
{
  "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "version": "${GIT_VERSION}",
  "system": {
    "uname": "$(uname -a)",
    "platform": "$(uname -s)",
    "architecture": "$(uname -m)",
    "cpu_count": "$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 'unknown')"
  },
  "test_case": {
    "name": "Mini-Millennium",
    "output_format": "$OUTPUT_FORMAT",
    "num_processes": $NUM_SAGE_PROCS,
    "mpi_command": "${MPI_RUN_COMMAND:-none}"
  },
  "configuration": {
    "build_type": "$BUILD_TYPE",
    "cmake_build": true,
    "sage_executable": "$SAGE_EXECUTABLE"
  },
  "overall_performance": {
    "wall_time_seconds": $REAL_TIME,
    "max_memory_mb": $MAX_MEMORY,
    "output_file_size_bytes": $(stat -f%z "$OUTPUT_FILE" 2>/dev/null || stat -c%s "$OUTPUT_FILE" 2>/dev/null || echo 0)
  },
  "phase_timing": {
    "note": "Detailed phase timing not implemented - requires SAGE code instrumentation",
    "halo_phase": { "time": "N/A", "percentage": "N/A" },
    "galaxy_phase": { "time": "N/A", "percentage": "N/A" },
    "post_phase": { "time": "N/A", "percentage": "N/A" },
    "final_phase": { "time": "N/A", "percentage": "N/A" }
  }
}
EOF

# Display results summary
echo "Benchmark completed successfully."
echo
echo "=== Performance Summary ==="
echo "Wall Clock Time: ${REAL_TIME} seconds"
echo "Maximum Memory Usage: ${MAX_MEMORY} MB"
echo "Output Format: $OUTPUT_FORMAT"
echo "MPI Processes: $NUM_SAGE_PROCS"
echo "Build Type: $BUILD_TYPE"
echo "Git Version: $GIT_VERSION"
echo
echo "Full results saved to: ${ROOT_DIR}/benchmarks/baseline_${TIMESTAMP}.json"
echo
echo "USAGE NOTES:"
echo "  - Run this script regularly to track performance changes"
echo "  - Compare JSON files to identify performance regressions"
echo "  - Use different --format options to benchmark I/O performance"
echo "  - Set CMAKE_BUILD_TYPE=Release for production benchmarks"
echo "  - Use MPI_RUN_COMMAND for parallel performance testing"
echo

# Exit successfully
exit 0