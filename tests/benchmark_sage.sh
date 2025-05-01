#!/bin/bash
#
# benchmark_sage.sh - Performance benchmark script for SAGE
#
# This script:
# 1. Runs SAGE on Mini-Millennium test data with performance measurement
# 2. Captures runtime and memory metrics
# 3. Stores results in a structured format for later comparison
#
# Usage:
#   ./benchmark_sage.sh             # Run with default settings
#   ./benchmark_sage.sh --verbose   # Run with detailed output
#   ./benchmark_sage.sh --help      # Show help information

# Configuration
BENCHMARK_DIR="benchmarks"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
BENCHMARK_RESULTS="${BENCHMARK_DIR}/baseline_${TIMESTAMP}.json"
VERBOSE=0
SHOW_HELP=0

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
        *)
            echo "Unknown option: $arg"
            echo "Usage: $0 [--help] [--verbose]"
            exit 1
            ;;
    esac
done

# Show help if requested
if [ $SHOW_HELP -eq 1 ]; then
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  --help        Show this help message"
    echo "  --verbose     Run with detailed output"
    echo
    echo "Purpose:"
    echo "  This script establishes a performance baseline for SAGE before"
    echo "  implementing the Properties Module architecture changes."
    echo "  It captures runtime, memory usage, and detailed metrics."
    echo
    echo "Output:"
    echo "  Results are stored in the 'benchmarks' directory for later comparison."
    exit 0
fi

# Determine script directories
SCRIPT_DIR=$(dirname "$(realpath "$0")")
ROOT_DIR=$(dirname "$SCRIPT_DIR")
TEST_DIR="${ROOT_DIR}/tests"
TEST_DATA_DIR="${TEST_DIR}/test_data"

# Create benchmark directory if it doesn't exist
mkdir -p "${ROOT_DIR}/${BENCHMARK_DIR}"

# Function to display error and exit
error_exit() {
    echo "ERROR: $1"
    echo "Benchmark failed."
    exit 1
}

echo "=== SAGE Performance Benchmark ==="
echo "Timestamp: $(date)"
echo "Saving results to: ${BENCHMARK_RESULTS}"
echo

# Ensure SAGE is compiled
echo "Compiling SAGE..."
cd "${ROOT_DIR}" || error_exit "Could not change to SAGE root directory"
make clean && make || error_exit "Failed to compile SAGE"
echo "Compilation successful."
echo

# Run SAGE with memory and time profiling
echo "Running SAGE benchmark..."
cd "${ROOT_DIR}" || error_exit "Could not change to SAGE root directory"

# Create a parameter file for the Mini-Millennium test
PARAM_FILE=$(mktemp)
cp "${TEST_DATA_DIR}/test-mini-millennium.par" "${PARAM_FILE}"

# ======= Time and memory measurement =======
echo "Running benchmark..."
if [ "$(uname)" = "Darwin" ]; then
    # For macOS, use our own timer and direct memory measurement
    start_time=$(date +%s)
    MEM_OUTPUT=$(mktemp)
    /usr/bin/time -l ./sage "${PARAM_FILE}" > /dev/null 2> "$MEM_OUTPUT"
    end_time=$(date +%s)
    
    # Calculate elapsed time
    REAL_TIME="$((end_time - start_time))"
    
    # Memory usage (convert bytes to MB)
    MAX_MEMORY=$(grep "maximum resident set size" "$MEM_OUTPUT" | awk '{print $1}')
    MAX_MEMORY=$(echo "scale=2; ${MAX_MEMORY:-0} / 1048576" | bc)
    
    if [ $VERBOSE -eq 1 ]; then
        echo "Memory measurement output:"
        cat "$MEM_OUTPUT"
    fi
    
    rm -f "$MEM_OUTPUT"
else
    # Linux version
    TIME_OUTPUT=$(mktemp)
    {
        /usr/bin/time -v ./sage "${PARAM_FILE}" 2>&1
    } > "${TIME_OUTPUT}"
    
    # Extract memory and time metrics
    REAL_TIME=$(grep "Elapsed (wall clock) time" "${TIME_OUTPUT}" | sed 's/.*: //' | sed 's/minutes/m/' | sed 's/ seconds/s/')
    MAX_MEMORY=$(grep "Maximum resident set size" "${TIME_OUTPUT}" | awk '{print $6}')
    
    # Convert KB to MB for consistency
    MAX_MEMORY=$(echo "scale=2; ${MAX_MEMORY:-0} / 1024" | bc)
    
    rm -f "${TIME_OUTPUT}"
fi

# Clean up temporary files
rm -f "${PARAM_FILE}"

# Create a clean JSON file with explicit values
echo '{
  "timestamp": "'$(date -u +"%Y-%m-%dT%H:%M:%SZ")'",
  "version": "'$(git describe --always || echo 'unknown')'",
  "system": "'$(uname -a)'",
  "test_case": "Mini-Millennium",
  "overall_performance": {
    "real_time": "'$REAL_TIME'",
    "max_memory_mb": '$MAX_MEMORY'
  },
  "phase_timing": {
    "halo_phase": {
      "time": "N/A",
      "percentage": "N/A"
    },
    "galaxy_phase": {
      "time": "N/A",
      "percentage": "N/A"
    },
    "post_phase": {
      "time": "N/A",
      "percentage": "N/A"
    },
    "final_phase": {
      "time": "N/A",
      "percentage": "N/A"
    }
  }
}' > "${ROOT_DIR}/${BENCHMARK_RESULTS}"

# Display results summary
echo "Benchmark completed."
echo
echo "=== Performance Summary === y"
echo "Wall Clock Time: ${REAL_TIME:-"N/A"} seconds"
echo "Maximum Memory Usage: ${MAX_MEMORY:-0} MB"
echo
echo "Full results saved to: ${BENCHMARK_RESULTS}"
echo
echo "To compare with future versions:"
echo "  1. Run this benchmark now as a baseline"
echo "  2. Implement the Properties Module architecture changes"
echo "  3. Run the benchmark again after changes"
echo "  4. Compare results using a JSON diff tool"
echo

# Exit successfully
exit 0