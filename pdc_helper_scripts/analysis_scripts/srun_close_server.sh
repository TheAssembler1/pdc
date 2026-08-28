#!/bin/bash
# Gracefully shut down the pdc_server step started by srun_server.sh, via
# the close_server client RPC (PDC_Client_close_all_server) -- same pattern
# as vpicio_scripts/srun_close_server.sh.
#
# Required env: BIN_DIR, RANKS, LOG_TAG

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$RANKS" \
  -n "$RANKS" \
  --ntasks-per-node=1 \
  --error="close_server_${LOG_TAG}_${RANKS}.err" \
  --output="close_server_${LOG_TAG}_${RANKS}.log" \
  ./close_server
popd

# Let the backgrounded server step fully exit before the next sweep
# iteration reuses the same node allocation.
sleep 2
