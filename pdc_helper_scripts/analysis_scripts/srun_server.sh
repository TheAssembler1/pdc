#!/bin/bash
# Start pdc_server in the background for the current sweep step. Mirrors
# vpicio_scripts/srun_server.sh, parameterized by RANKS instead of a fixed
# node/client-ratio layout -- the magnitude benchmark needs server count ==
# client count (PDC_REGION_STATIC routes each rank's region to the matching
# server; see bench_magnitude.c's comment on OBJ_STATIC vs REGION_STATIC).
# Shutdown is graceful via srun_close_server.sh (the close_server client
# RPC), not a captured PID -- the srun step outlives this script under the
# same Slurm job allocation, same as vpicio_scripts.
#
# Required env: BIN_DIR, PDC_DATA_LOC, RANKS, LOG_TAG

set -xeu

pushd "$BIN_DIR"
rm -rf "$PDC_DATA_LOC/pdc_data" "$PDC_DATA_LOC/pdc_tmp"
srun \
  -N "$RANKS" \
  -n "$RANKS" \
  --ntasks-per-node=1 \
  --error="server_${LOG_TAG}_${RANKS}.err" \
  --output="server_${LOG_TAG}_${RANKS}.log" \
  ./pdc_server &
popd

# Give the servers time to stand up and publish their address info before
# any client tries to connect.
sleep 10
