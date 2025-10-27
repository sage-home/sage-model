#!/bin/bash
#SBATCH --job-name=multi_pso_z0
#SBATCH --array=1-10
#SBATCH --cpus-per-task=1
#SBATCH --mem=8GB
#SBATCH --time=1:00:00
#SBATCH --account=oz004

# Script to run multiple PSO instances in parallel using SLURM
# Each task will run one PSO iteration independently
# Usage: sbatch run_multiple_pso_slurm.sh
# Edit SLURM parameters and CONSTRAINTS in this file before submitting

ml purge
ml restore basic

# CONFIGURATION - Edit these as needed
CONSTRAINTS="SMF_z0"

# Get the array task ID (1-5 for array=1-5)
RUN_ID=$SLURM_ARRAY_TASK_ID

# Derive job array size from SLURM_ARRAY_TASK_MAX if available, otherwise use task ID
if [ -n "$SLURM_ARRAY_TASK_MAX" ]; then
    NUM_RUNS=$SLURM_ARRAY_TASK_MAX
else
    NUM_RUNS=$RUN_ID
fi

# Base directory for storing results (shared across all array jobs)
BASE_OUTPUT_DIR="/fred/oz004/mbradley/SAGE-GAS/sage-model/output/millennium_pso_multi"
MULTI_RUN_DIR="${BASE_OUTPUT_DIR}_slurm_${SLURM_ARRAY_JOB_ID}"

# Fixed parameters
CONFIG_PATH="/fred/oz004/mbradley/SAGE-GAS/sage-model/input/minimillennium.par"
BASE_PATH="/fred/oz004/mbradley/SAGE-GAS/sage-model/sage"
PARTICLES=13
ITERATIONS=50
TEST="student-t"
AGE_ALIST_FILE_MINI_MILLENNIUM="/fred/oz004/mbradley/SAGE-GAS/sage-model/input/millennium/trees/millennium.a_list"
BOXSIZE=62.5
SIM_MINI_MILLENNIUM=1
VOL_FRAC=1.0
OMEGA0=0.25 
H0=0.73
SPACEFILE="/fred/oz004/mbradley/SAGE-GAS/sage-model/optim/space.txt"
ACCOUNT="oz004"

# Create main directory (first task creates it, others wait)
if [ $RUN_ID -eq 1 ]; then
    echo "========================================="
    echo "Multiple PSO Run (SLURM Job Array)"
    echo "========================================="
    echo "SLURM Array Job ID: $SLURM_ARRAY_JOB_ID"
    echo "Total runs: $NUM_RUNS"
    echo "Constraints: $CONSTRAINTS"
    echo "Output directory: $MULTI_RUN_DIR"
    echo "========================================="
    echo ""
    
    mkdir -p "$MULTI_RUN_DIR"
    
    # Save run configuration
    CONFIG_FILE="${MULTI_RUN_DIR}/run_config.txt"
    cat > "$CONFIG_FILE" << EOF
Multiple PSO Run Configuration (SLURM Job Array)
=================================================
SLURM Array Job ID: $SLURM_ARRAY_JOB_ID
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
else
    # Wait for main directory to be created
    while [ ! -d "$MULTI_RUN_DIR" ]; do
        sleep 1
    done
fi

# Each array task runs one PSO instance
RUN_OUTPUT_DIR="${MULTI_RUN_DIR}/run_${RUN_ID}"
RUN_CSV_OUTPUT="${RUN_OUTPUT_DIR}/params.csv"

echo "========================================="
echo "Array Task $RUN_ID: Starting PSO run $RUN_ID of $NUM_RUNS"
echo "========================================="

mkdir -p "$RUN_OUTPUT_DIR"

# Run PSO
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
    echo "ERROR: Array task $RUN_ID (run $RUN_ID) failed!"
    exit 1
else
    echo "Array task $RUN_ID: PSO run $RUN_ID completed successfully"
fi

