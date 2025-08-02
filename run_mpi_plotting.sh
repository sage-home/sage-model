#!/bin/bash

# Script to run the stellar-halo mass relation plotting with MPI grid parallelization
# Usage: ./run_mpi_plotting.sh [number_of_processes]

# Check if MPI is available and mpi4py is installed
if command -v mpirun &> /dev/null; then
    python3 -c "import mpi4py" 2>/dev/null
    if [ $? -eq 0 ]; then
        # Get number of CPU cores
        NCORES=$(python3 -c "import multiprocessing; print(multiprocessing.cpu_count())")
        
        # Use provided argument or default to (cores - 1)
        NPROCS=${1:-$((NCORES - 1))}
        if [ $NPROCS -lt 1 ]; then
            NPROCS=1
        fi
        
        echo "Detected $NCORES CPU cores, using $NPROCS MPI processes"
        echo "Running with MPI grid parallelization..."
        
        mpirun -np $NPROCS python3 plotting/sf_pop_trace.py
    else
        echo "MPI available but mpi4py not installed. Installing now..."
        pip install mpi4py
        if [ $? -eq 0 ]; then
            NPROCS=${1:-4}
            echo "Running with $NPROCS MPI processes..."
            mpirun -np $NPROCS python3 plotting/sf_pop_trace.py
        else
            echo "Failed to install mpi4py, running in serial mode..."
            python3 plotting/sf_pop_trace.py
        fi
    fi
else
    echo "MPI not available, running in serial mode..."
    python3 plotting/sf_pop_trace.py
fi

echo "Plotting completed."
