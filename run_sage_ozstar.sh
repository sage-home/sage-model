#!/bin/bash -l
#SBATCH --job-name=SAGE_uchuu
#SBATCH --mail-user=mbradley@swin.edu.au
#SBATCH --mail-type=ALL
#SBATCH --time=8:00:00
#SBATCH --ntasks-per-node=32
#SBATCH --mem-per-cpu=2000M
#SBATCH --tmp=3200m

ml purge
ml restore basic

# Set number of MPI processes (equal to tasks per node)
NPROCS=$SLURM_NTASKS_PER_NODE

# Set OpenMP threads to 1 when using MPI to avoid conflicts
export OMP_NUM_THREADS=1

echo "Running SAGE with $NPROCS MPI processes"
echo "Job ID: $SLURM_JOB_ID"
echo "Running on node(s): $SLURM_NODELIST"

mpirun -np $NPROCS /fred/oz004/mbradley/SAGE-GAS/sage-model/sage input/uchuu.par