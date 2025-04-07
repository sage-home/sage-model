#!/bin/bash
# run-sage-emulator.sh
# Script to run the SAGE emulator system step by step

# Define paths and parameters
PSO_DIRS="/fred/oz004/mbradley/SAGE-PSO/sage-model/output/millennium_pso_SMFz0"
SPACE_FILE="/fred/oz004/mbradley/SAGE-PSO/sage-model/optim/space.txt"
OUTPUT_DIR="/fred/oz004/mbradley/SAGE-PSO/sage-model/output/emulator_output"
CONFIG_FILE="/fred/oz004/mbradley/SAGE-PSO/sage-model/input/millennium.par"
CONSTRAINTS="SMF_z0,BHBM_z0"
METHOD="random_forest"
STRATEGY="prescreening"
BOXSIZE=62.5
H0=0.73
OMEGA0=0.25
AGE_ALIST_FILE="/fred/oz004/mbradley/SAGE-PSO/sage-model/input/millennium/trees/millennium.a_list"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=================================================="
echo "SAGE Emulator System"
echo "=================================================="
echo "PSO Directory: $PSO_DIRS"
echo "Space File: $SPACE_FILE"
echo "Output Directory: $OUTPUT_DIR"
echo "Constraints: $CONSTRAINTS"
echo "Method: $METHOD"
echo "Strategy: $STRATEGY"
echo ""

# Step 1: Train emulators on existing PSO data
echo "Step 1: Training emulators..."
python3 ./sage_emulator_demo.py --mode train \
                           --pso-dirs "/fred/oz004/mbradley/SAGE-PSO/sage-model/output/millennium_pso_SMFz0" "/fred/oz004/mbradley/SAGE-PSO/sage-model/output/millennium_pso_SMFz0_BHBMz0" "/fred/oz004/mbradley/SAGE-PSO/sage-model/output/millennium_pso_SMFz0_BHBMz0_weighted" "/fred/oz004/mbradley/SAGE-PSO/sage-model/output/millennium_pso_BHBMz0" \
                           --space-file "$SPACE_FILE" \
                           --constraints ${CONSTRAINTS//,/ } \
                           --method "$METHOD" \
                           --output-dir "$OUTPUT_DIR"

if [ $? -ne 0 ]; then
    echo "Error: Emulator training failed."
    exit 1
fi

echo ""
echo "Emulator training complete!"
echo ""

# Step 2: Visualize parameter effects on outputs
echo "Step 2: Visualizing parameter effects..."
python3 ./sage_emulator_demo.py --mode predict \
                           --space-file "$SPACE_FILE" \
                           --constraints ${CONSTRAINTS//,/ } \
                           --output-dir "$OUTPUT_DIR"

if [ $? -ne 0 ]; then
    echo "Error: Parameter visualization failed."
    exit 1
fi

echo ""
echo "Parameter visualization complete!"
echo ""

# Step 3: Run hybrid PSO with emulators
echo "Step 3: Running hybrid PSO..."
python3 ./sage_emulator_integration.py -c "$CONFIG_FILE" \
                                  -S "$SPACE_FILE" \
                                  -v 0 \
                                  -e "$OUTPUT_DIR/emulators" \
                                  -s "$STRATEGY" \
                                  -m 50 \
                                  -x "$CONSTRAINTS" \
                                  -t student-t \
                                  -o "$OUTPUT_DIR" \
                                  --boxsize "$BOXSIZE" \
                                  --h0 "$H0" \
                                  --Omega0 "$OMEGA0" \
                                  --age-alist-file "$AGE_ALIST_FILE"

if [ $? -ne 0 ]; then
    echo "Error: Hybrid PSO failed."
    exit 1
fi

echo ""
echo "Hybrid PSO complete!"
echo ""
echo "Results are available in: $OUTPUT_DIR"
echo ""
echo "You can view the best parameters in: $OUTPUT_DIR/hybrid_pso/best_parameters.csv"
echo "=================================================="