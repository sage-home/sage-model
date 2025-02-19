#!/bin/bash

# Define paths to different PSO run outputs
BASE_OUTPUT_DIR="/fred/oz004/mbradley/sage-model/output"
#RUN1_DIR="/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_1tree_1cpu"
RUN2_DIR="/fred/oz004/mbradley/sage-model/output/pso_tolerance05/miniuchuu_pso_1tree_1cpu"
#RUN3_DIR="/fred/oz004/mbradley/sage-model/output/miniuchuu_pso_8tree_64cpu"
RUN4_DIR="/fred/oz004/mbradley/sage-model/output/pso_tolerance05/miniuchuu_pso_4tree_32cpu"
RUN5_DIR="/fred/oz004/mbradley/sage-model/output/pso_tolerance05/miniuchuu_pso_32tree_32cpu"

# Create output directory if it doesn't exist
#mkdir -p "$BASE_OUTPUT_DIR"

# Ensure we're using the correct Python environment
ml purge
ml restore basic

echo "Starting parameter comparison analysis..."

# Run the comparison script
python3 comparison_param_evol_plot.py \
    "$RUN2_DIR" \
    "$RUN4_DIR" \
    "$RUN5_DIR" \
    --labels "1 tree file" "4 tree files" "32 tree files" \
    --colors "darkblue" "firebrick" "goldenrod" \
    -o "$BASE_OUTPUT_DIR"

# Check if the script ran successfully
if [ $? -eq 0 ]; then
    echo "Parameter comparison completed successfully"
    echo "Results saved to: $BASE_OUTPUT_DIR"
else
    echo "Error: Parameter comparison failed"
    exit 1
fi

# Optional: Create a timestamp file to record when the analysis was run
#date > "$BASE_OUTPUT_DIR/last_run.txt"

echo "Analysis complete!"