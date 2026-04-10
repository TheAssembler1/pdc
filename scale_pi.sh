#!/bin/bash

# Loop over powers of two: 1, 2, 4, 8, 16, 32
for nprocs in 1 2 4 8 16 32; do
    echo "========================================"
    echo "Running with $nprocs rank(s)..."
    echo "========================================"
    
    # Run with srun
    srun -n $nprocs ./pi_gpu_mpi
done
