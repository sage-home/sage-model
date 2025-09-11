#!/bin/bash

# Configuration
INPUT_FILE="output/millennium/model_0.hdf5"
OUTPUT_DIR="./sage_group_finding_results"
PYTHON_SCRIPT="group_finder_SAGE.py"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "Starting SAGE analysis with minimal overhead..."
echo "Input: $INPUT_FILE"
echo "Output: $OUTPUT_DIR"
echo ""

# Direct execution - exactly like typing in terminal
echo "Running full evolution analysis..."
echo "Command: python3 $PYTHON_SCRIPT --input_file $INPUT_FILE --snapshots 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 --separate_groups_clusters --min_group_size 3 --max_group_size 20 --output_dir $OUTPUT_DIR/full_evolution"

# Execute directly without any shell overhead
python3 "$PYTHON_SCRIPT" \
    --input_file "$INPUT_FILE" \
    --snapshots 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 \
    --separate_groups_clusters \
    --min_group_size 3 \
    --output_dir "$OUTPUT_DIR/full_evolution"

echo ""
echo "Analysis complete!"