#!/bin/bash

set -xeu

pushd $BIN_DIR
rm -rf $PDC_DATA_LOC/pdc_data $PDC_TMPDIR/pdc_tmp
srun \
  -N "$NUM_NODES" \
  -n "$SERVER_TOTAL_TASKS" \
  --ntasks-per-node=$DATA_SERVERS_PER_NODE \
  --error="server_write_output_${TRANSFORM}_${NUM_NODES}.err" \
  --output="server_write_output_${TRANSFORM}_${NUM_NODES}.log" \
  ./pdc_server &
popd
