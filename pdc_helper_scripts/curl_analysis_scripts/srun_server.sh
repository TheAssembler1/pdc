#!/bin/bash
# Start pdc_server in the background for this job's node count. Mirrors
# analysis_scripts/srun_server.sh: 8 data servers/node, decoupled from the
# client rank count. static_region_partition splits each object across
# however many data servers exist regardless of client count, so this
# doesn't need to match the client rank count for correctness. Shutdown
# is graceful via srun_close_server.sh (the close_server client RPC), not
# a captured PID -- the srun step outlives this script under the same
# Slurm job allocation.
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
  bash -c 'export HG_HOST=cxi0:$SLURM_LOCALID; exec ./pdc_server' &
popd

# Give the servers time to stand up and publish their address info before
# any client tries to connect.
sleep 10
