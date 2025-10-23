#!/bin/bash

set -xeu

pushd $BIN_DIR
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_write_output_${TRANSFORM}_${NUM_NODES}.log" \
  --error="client_write_output_${TRANSFORM}_${NUM_NODES}.err" \
  ./vpicio_mts_transform_write 8388608 2 0 $TRANSFORM
popd

