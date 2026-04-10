#!/bin/bash
# build_test.sh
# Compile vpicio.cu and bdcats.cu using nvcc with all necessary includes and libraries

set -e  # exit on any error

export TF_GRAPHS_DIR="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/tf_client/graphs"

# Directories
PDC_INSTALL="/pscratch/sd/n/nlewi26/src/work_space/install/pdc"
MERCURY_DIR="/global/cfs/cdirs/m2621/perlmutter/pdc/build/mercury-local-install"
MPICH_DIR="/opt/cray/pe/mpich/8.1.30/ofi/gnu/12.3"
SRC_DIR="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/tests/misc"
OUT_DIR="$PDC_INSTALL/bin"

# Source files
SRC_FILES=("vpicio.cu" "bdcats.cu")

# Include directories
INCLUDE_DIRS=(
    "$PDC_INSTALL/include"
    "$MERCURY_DIR/include"
    "$MPICH_DIR/include"
    "/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/server/include"
    "/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/server/transform/include"
    "/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/commons/collections/include"
)

# Build NVCC include flags
INCLUDE_FLAGS=""
for dir in "${INCLUDE_DIRS[@]}"; do
    INCLUDE_FLAGS+="-I$dir "
done

# Library flags
LIB_FLAGS="-L$PDC_INSTALL/lib -lpdc_server_lib -lpdc_debug -lpdc_commons \
-L$MERCURY_DIR/lib -lmercury \
-L$MPICH_DIR/lib -lmpich"

# Ensure output directory exists
mkdir -p "$OUT_DIR"

# Compile each source file
for src in "${SRC_FILES[@]}"; do
    base=$(basename "$src" .cu)
    echo "Compiling $src -> $OUT_DIR/$base"
    /opt/nvidia/hpc_sdk/Linux_x86_64/25.5/cuda/12.9/bin/nvcc \
        -O3 \
        -std=c++17 \
        $INCLUDE_FLAGS \
        "$SRC_DIR/$src" \
        $LIB_FLAGS \
        -o "$OUT_DIR/$base"
done

echo "Build complete. Binaries are in $OUT_DIR"