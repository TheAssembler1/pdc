#!/bin/bash
# Start pdc_server in the background for this job's node count. Mirrors
# vpicio_scripts/srun_server.sh: 8 data servers/node, decoupled from the
# client rank count (see srun_client_*.sh / srun_hdf5.sh, 32 ranks/node).
# static_region_partition splits each object across however many data
# servers exist regardless of client count, so this doesn't need to match
# the client rank count for correctness -- see bench_magnitude.c's comment
# on REGION_STATIC vs OBJ_STATIC. Shutdown is graceful via
# srun_close_server.sh (the close_server client RPC), not a captured PID --
# the srun step outlives this script under the same Slurm job allocation,
# same as vpicio_scripts.
#
# Required env: BIN_DIR, PDC_DATA_LOC, NUM_NODES, SERVERS_PER_NODE,
#   SERVER_TOTAL_TASKS, LOG_TAG

set -xeu

pushd "$BIN_DIR"
rm -rf "$PDC_DATA_LOC/pdc_data" "$PDC_DATA_LOC/pdc_tmp"
srun \
  -N "$NUM_NODES" \
  -n "$SERVER_TOTAL_TASKS" \
  --ntasks-per-node="$SERVERS_PER_NODE" \
  --error="server_${LOG_TAG}_${NUM_NODES}.err" \
  --output="server_${LOG_TAG}_${NUM_NODES}.log" \
  ./pdc_server &
popd

# Give the servers time to stand up and publish their address info before
# any client tries to connect.
sleep 10
