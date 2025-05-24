#!/bin/bash
# ========================================================================
# SAGE Empty Pipeline Test Script
# ========================================================================
#
# This script runs the empty pipeline test which validates that the SAGE
# core infrastructure can operate independently of any physics components.
# It uses placeholder modules that implement the module interface but perform
# no actual physics calculations.
#
# Purpose:
# - Validates core-physics separation principle
# - Verifies that all pipeline phases execute correctly
# - Tests memory management with minimal properties
# - Ensures the core infrastructure is truly physics-agnostic
#
# Usage:
#   ./run_empty_pipeline_test.sh
#
# Files used:
# - tests/test_data/test-mini-millennium.par: Parameter file that references 
#   the empty pipeline configuration
# - tests/test_data/empty_pipeline_config.json: Configures the pipeline
#   to use placeholder modules instead of real physics modules
#
# Note: This test is integrated into the main Makefile and can also be run with:
#   make test_empty_pipeline
#   ./tests/test_empty_pipeline tests/test_data/test-mini-millennium.par
#
# ========================================================================

# Set paths
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$TEST_DIR")"
INPUT_DIR="$ROOT_DIR/tests/test_data"
OUTPUT_DIR="$ROOT_DIR/tests/test_results"
PARAM_FILE="$INPUT_DIR/test-mini-millennium.par"

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

# Clean up any previous output
echo "Cleaning up previous test output..."
rm -f "$OUTPUT_DIR/test-sage-output_*.hdf5"

# Compile the test
echo "Compiling empty pipeline test..."
cd "$ROOT_DIR" || exit 1
make test_empty_pipeline

# Run the test
echo "Running empty pipeline test..."
cd "$TEST_DIR" || exit 1
./test_empty_pipeline "$PARAM_FILE"

# Check exit status
RESULT=$?
if [ $RESULT -eq 0 ]; then
    echo "✅ Empty pipeline test completed successfully!"
    echo "This validates that the core can run without actual physics modules."
else
    echo "❌ Empty pipeline test failed with exit code $RESULT"
fi

# Provide summary
echo
echo "=== Empty Pipeline Validation Summary ==="
echo "- Core can run with no physics components: $([ $RESULT -eq 0 ] && echo '✅ YES' || echo '❌ NO')"
echo "- All pipeline phases executed: $([ $RESULT -eq 0 ] && echo '✅ YES' || echo '❌ NO')"
echo "- Memory management with empty models: $([ $RESULT -eq 0 ] && echo '✅ OK' || echo '❌ Issues detected')"
echo

exit $RESULT