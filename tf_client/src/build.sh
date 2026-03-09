#!/bin/bash
# build_tf_client.sh
# Compile the client shared library libtf_client.so

set -e  # exit on any error

# Directories
SERVER_INCLUDE="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/server/include"
TRANSFORM_INCLUDE="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/server/transform/include"
COLLECTIONS_INCLUDE="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/commons/collections/include"

# Source files
SRC_CUSTOM_LIB="tf_custom_lib.c"
SRC_PDC_USER="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/server/transform/pdc_tf_user.c"
SRC_VECTOR="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/commons/collections/pdc_vector.c"
SRC_DG="/pscratch/sd/n/nlewi26/src/work_space/source/pdc/src/commons/collections/pdc_dg.c"

# Output library
OUT_LIB="libtf_client.so"

# Compile
gcc -fPIC -shared \
    -I"$SERVER_INCLUDE" \
    -I"$TRANSFORM_INCLUDE" \
    -I"$COLLECTIONS_INCLUDE" \
    "$SRC_CUSTOM_LIB" \
    "$SRC_PDC_USER" \
    "$SRC_VECTOR" \
    "$SRC_DG" \
    -o "$OUT_LIB"

echo "Build complete: $OUT_LIB"