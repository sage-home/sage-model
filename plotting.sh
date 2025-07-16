#!/bin/bash

# Simple SAGE plotting script runner
echo "Starting plotting scripts..."

# List your plotting scripts here
scripts=(
    "plotting/allresults-local.py"
    "plotting/allresults-history.py"
    "plotting/cgm-local.py"
    "plotting/coldstreams-history.py"
    "plotting/paper_plots.py"
    "plotting/paper_plots2.py"
    "plotting/paper_plots3.py"
    "plotting/smhm.py"
    "plotting/smf_analysis_obs_sam.py"
)

# Run each script
for script in "${scripts[@]}"; do
    echo "Running: python3 $script"
    if python3 "$script"; then
        echo "✓ $script completed successfully"
    else
        echo "✗ $script failed (continuing anyway)"
    fi
    echo ""
done

echo "All scripts processed!"