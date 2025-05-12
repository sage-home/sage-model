#!/bin/bash
# Script to run and validate the empty pipeline test

# Set paths
TEST_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$TEST_DIR")"
INPUT_DIR="$ROOT_DIR/input"
OUTPUT_DIR="$ROOT_DIR/output"
PARAM_FILE="$INPUT_DIR/empty_pipeline_parameters.par"

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

# Ensure test data is properly linked
echo "Setting up test data links..."
ln -sf "$TEST_DIR/test_data/trees_063.0" "$INPUT_DIR/trees_063.0" 2>/dev/null
ln -sf "$TEST_DIR/test_data/millennium.a_list" "$INPUT_DIR/millennium.a_list" 2>/dev/null

# Clean up any previous output
echo "Cleaning up previous test output..."
rm -f "$OUTPUT_DIR/empty_pipeline_*.hdf5"

# Compile the test
echo "Compiling empty pipeline test..."
cd "$TEST_DIR" || exit 1
make -f Makefile.empty_pipeline

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
