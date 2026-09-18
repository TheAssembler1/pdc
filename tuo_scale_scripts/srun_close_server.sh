#!/bin/bash
# Gracefully shut down the pdc_server step started by srun_server.sh, via
# the close_server client RPC. Mirrors
# pdc_helper_scripts/curl_analysis_scripts/srun_close_server.sh.
#
# Required env: BIN_DIR, NUM_NODES, SERVERS_PER_NODE, SERVER_TOTAL_TASKS,
#   LOG_TAG, RESULTS_DIR

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$SERVER_TOTAL_TASKS" \
  --ntasks-per-node="$SERVERS_PER_NODE" \
  --error="${RESULTS_DIR}/close_server_${LOG_TAG}_${NUM_NODES}.err" \
  --output="${RESULTS_DIR}/close_server_${LOG_TAG}_${NUM_NODES}.log" \
  ./close_server
popd

# Let the backgrounded server step fully exit before the job script
# moves on.
sleep 2
