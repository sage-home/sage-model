#!/bin/bash

# Simple SAGE plotting script runner
echo "Starting plotting scripts..."

# List your plotting scripts here
scripts=(
    "plotting/allresults-local.py"
    "plotting/allresults-history.py"
    "plotting/halo-evolve.py"
    "plotting/paper_plots.py"
    "plotting/paper_plots2.py"
    "plotting/paper_plots3.py"
    "plotting/smhm.py"
    "plotting/smf_analysis_obs_sam.py"
    "plotting/cgm-diag.py"
    "plotting/halo-evolve.py"
    "plotting/bulge_disk.py"
    "plotting/bulge_disk2.py"
    "plotting/ffb_analysis.py"
    # "plotting/sf_pop_trace.py"
    "plotting/stat_test.py"
    "plotting/stat_test2.py"
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