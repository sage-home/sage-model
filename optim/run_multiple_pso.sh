#!/bin/bash
# Script to run PSO multiple times and analyze results
# Usage: ./run_multiple_pso.sh <num_runs> <constraints>
# Example: ./run_multiple_pso.sh 5 "SMF_z0,SMF_z05"

# Check if required arguments are provided
if [ $# -lt 2 ]; then
    echo "Usage: $0 <num_runs> <constraints>"
    echo "Example: $0 5 'SMF_z0,SMF_z05'"
    echo ""
    echo "Constraints can be any combination of:"
    echo "  SMF_z0, SMF_z02, SMF_z05, SMF_z08, SMF_z10, SMF_z11, SMF_z15, SMF_z20"
    echo "  SMF_z24, SMF_z31, SMF_z36, SMF_z46, SMF_z57, SMF_z63, SMF_z77, SMF_z85, SMF_z104"
    echo "  BHMF_z0, BHMF_z20, BHBM_z0, BHBM_z20, HSMR_z0, etc."
    exit 1
fi

NUM_RUNS=$1
CONSTRAINTS=$2

# Base directory for storing results
BASE_OUTPUT_DIR="/Users/mbradley/Documents/PhD/SAGE-2.0/sage-model/output/millennium_pso_multi"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
MULTI_RUN_DIR="${BASE_OUTPUT_DIR}_${TIMESTAMP}"

# Create main directory for this multi-run experiment
mkdir -p "$MULTI_RUN_DIR"

echo "========================================="
echo "Multiple PSO Run Configuration"
echo "========================================="
echo "Number of runs: $NUM_RUNS"
echo "Constraints: $CONSTRAINTS"
echo "Output directory: $MULTI_RUN_DIR"
echo "========================================="
echo ""

# Fixed parameters (from run_pso.sh)
CONFIG_PATH="/Users/mbradley/Documents/PhD/SAGE-2.0/sage-model/input/millennium.par"
BASE_PATH="/Users/mbradley/Documents/PhD/SAGE-2.0/sage-model/sage"
PARTICLES=16
ITERATIONS=50
TEST="student-t"
AGE_ALIST_FILE_MINI_MILLENNIUM="/Users/mbradley/Documents/PhD/SAGE-2.0/sage-model/input/millennium/trees/millennium.a_list"
BOXSIZE=62.5
SIM_MINI_MILLENNIUM=1
VOL_FRAC=1.0
OMEGA0=0.25 
H0=0.73
SPACEFILE="/Users/mbradley/Documents/PhD/SAGE-2.0/sage-model/optim/space.txt"
ACCOUNT="oz004"

# Array to store CSV output paths
CSV_PATHS=()

# Run PSO multiple times
for i in $(seq 1 $NUM_RUNS); do
    echo ""
    echo "========================================="
    echo "Starting PSO run $i of $NUM_RUNS"
    echo "========================================="
    
    # Create output directory for this run
    RUN_OUTPUT_DIR="${MULTI_RUN_DIR}/run_${i}"
    RUN_CSV_OUTPUT="${RUN_OUTPUT_DIR}/params.csv"
    
    mkdir -p "$RUN_OUTPUT_DIR"
    
    # Store CSV path
    CSV_PATHS+=("$RUN_CSV_OUTPUT")
    
    # Run PSO with current configuration
    python3 ./main.py \
      -c "$CONFIG_PATH" \
      -b "$BASE_PATH" \
      -o "$RUN_OUTPUT_DIR" \
      -s "$PARTICLES" \
      -m "$ITERATIONS" \
      -t "$TEST" \
      -x "$CONSTRAINTS" \
      -csv "$RUN_CSV_OUTPUT" \
      --age-alist-file "$AGE_ALIST_FILE_MINI_MILLENNIUM" \
      --sim "$SIM_MINI_MILLENNIUM" \
      --boxsize "$BOXSIZE" \
      --vol-frac "$VOL_FRAC" \
      --Omega0 "$OMEGA0" \
      --h0 "$H0" \
      -S "$SPACEFILE"
    
    if [ $? -ne 0 ]; then
        echo "ERROR: PSO run $i failed!"
        echo "Continuing with remaining runs..."
    else
        echo "PSO run $i completed successfully"
    fi
    
    echo "========================================="
done

# Save run configuration
CONFIG_FILE="${MULTI_RUN_DIR}/run_config.txt"
cat > "$CONFIG_FILE" << EOF
Multiple PSO Run Configuration
===============================
Timestamp: $TIMESTAMP
Number of runs: $NUM_RUNS
Constraints: $CONSTRAINTS
Particles: $PARTICLES
Iterations: $ITERATIONS
Statistical test: $TEST
Box size: $BOXSIZE
Volume fraction: $VOL_FRAC

Individual run directories:
EOF

for i in $(seq 1 $NUM_RUNS); do
    echo "  run_${i}/" >> "$CONFIG_FILE"
done

echo ""
echo "========================================="
echo "All PSO runs completed!"
echo "========================================="
echo "Results saved in: $MULTI_RUN_DIR"
echo ""
echo "Now analyzing results..."

# Run analysis script
python3 ./analyze_multiple_pso.py "$MULTI_RUN_DIR" "$SPACEFILE"

echo ""
echo "========================================="
echo "Complete! Check results in:"
echo "  $MULTI_RUN_DIR"
echo "========================================="
