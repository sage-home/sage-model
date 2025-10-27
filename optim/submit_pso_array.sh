#!/bin/bash
# Helper script to submit PSO job array and analysis
# Usage: ./submit_pso_array.sh

echo "Submitting PSO job array..."
JOB_OUTPUT=$(sbatch run_multiple_pso_slurm.sh)
JOB_ID=$(echo $JOB_OUTPUT | awk '{print $NF}')

if [ -z "$JOB_ID" ]; then
    echo "ERROR: Failed to submit job array"
    exit 1
fi

echo "Job array submitted: $JOB_ID"
echo ""

# Construct output directory name
OUTPUT_DIR="/fred/oz004/mbradley/SAGE-GAS/sage-model/output/millennium_pso_multi_slurm_${JOB_ID}"

echo "Submitting analysis job (will run after array completes)..."
ANALYSIS_OUTPUT=$(sbatch --dependency=afterany:${JOB_ID} analyze_pso_array.sh "$OUTPUT_DIR")
ANALYSIS_ID=$(echo $ANALYSIS_OUTPUT | awk '{print $NF}')

echo "Analysis job submitted: $ANALYSIS_ID"
echo ""
echo "========================================="
echo "Submission Summary"
echo "========================================="
echo "PSO Array Job ID:    $JOB_ID"
echo "Analysis Job ID:     $ANALYSIS_ID"
echo "Output directory:    $OUTPUT_DIR"
echo ""
echo "Monitor with:"
echo "  squeue -u $USER"
echo "  squeue -j $JOB_ID"
echo ""
echo "Check output files:"
echo "  tail -f pso_array_${JOB_ID}_*.out"
echo ""
echo "Cancel all jobs:"
echo "  scancel $JOB_ID $ANALYSIS_ID"
echo "========================================="
