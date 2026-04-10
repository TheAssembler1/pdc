#!/bin/bash
set -e

MPICH_DIR="/opt/cray/pe/mpich/8.1.30/ofi/gnu/12.3"  # adjust as needed

/opt/nvidia/hpc_sdk/Linux_x86_64/25.5/cuda/12.9/bin/nvcc \
  -O3 \
  -std=c++17 \
  -Xcompiler -fPIC \
  -I"$MPICH_DIR/include" \
  -L"$MPICH_DIR/lib" -lmpich \
  -shared \
  ./test.cu \
  -o libpi_gpu.so