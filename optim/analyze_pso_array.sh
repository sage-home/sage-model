#!/bin/bash
#SBATCH --job-name=pso_analysis
#SBATCH --cpus-per-task=1
#SBATCH --mem=4GB
#SBATCH --time=00:30:00
#SBATCH --account=oz004

# Script to analyze results from a completed PSO job array
# Usage: sbatch --dependency=afterany:<array_job_id> analyze_pso_array.sh <output_dir>

if [ $# -lt 1 ]; then
    echo "ERROR: No output directory provided!"
    echo "Usage: sbatch --dependency=afterany:<job_id> $0 <output_dir>"
    exit 1
fi

MULTI_RUN_DIR=$1
SPACEFILE="/fred/oz004/mbradley/SAGE-GAS/sage-model/optim/space.txt"

ml purge
ml restore basic

cd /fred/oz004/mbradley/SAGE-GAS/sage-model/optim || exit 1

echo "========================================="
echo "Analyzing PSO Array Results"
echo "========================================="
echo "Output directory: $MULTI_RUN_DIR"
echo ""

# Check which runs completed successfully
echo "Checking run status..."
COMPLETED=0
FAILED=0
for run_dir in "$MULTI_RUN_DIR"/run_*; do
    if [ -d "$run_dir" ]; then
        run_name=$(basename "$run_dir")
        if [ -f "$run_dir/params.csv" ]; then
            echo "  $run_name: COMPLETED"
            ((COMPLETED++))
        else
            echo "  $run_name: FAILED (no params.csv)"
            ((FAILED++))
        fi
    fi
done

echo ""
echo "Summary: $COMPLETED completed, $FAILED failed"
echo ""

if [ $COMPLETED -eq 0 ]; then
    echo "ERROR: No runs completed successfully. Cannot run analysis."
    exit 1
fi

# Run analysis script
python3 ./analyze_multiple_pso.py "$MULTI_RUN_DIR" "$SPACEFILE"

echo ""
echo "========================================="
echo "Analysis complete!"
echo "========================================="
echo "Results in: $MULTI_RUN_DIR"
