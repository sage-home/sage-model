#!/bin/bash

# Script to run the stellar-halo mass relation plotting with MPI
# Usage: ./run_mpi_plotting.sh [number_of_processes]

# Default to 4 processes if not specified
NPROCS=${1:-4}

echo "Running stellar-halo mass relation plotting with $NPROCS MPI processes..."

# Check if mpi4py is installed
python3 -c "import mpi4py" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Error: mpi4py is not installed. Installing now..."
    pip install mpi4py
fi

# Run with MPI
mpirun -np $NPROCS python3 plotting/paper_plots3.py

echo "MPI plotting completed."
