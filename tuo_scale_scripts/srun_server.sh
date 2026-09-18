#!/bin/bash
# Start pdc_server in the background for this job's node count. Mirrors
# pdc_helper_scripts/curl_analysis_scripts/srun_server.sh: launched
# without the `restart` argument, so it always starts with an empty
# in-memory metadata table regardless of what's on disk from a previous
# run -- the rm -rf below is still needed because the per-rank
# checkpoint/address files under PDC_TMPDIR (== PDC_DATA_LOC here) would
# otherwise be observed by this fresh, non-restart launch. Shutdown is
# graceful via srun_close_server.sh (the close_server client RPC), not a
# captured PID -- this srun step outlives this script under the same
# sbatch allocation, backgrounded with `&` so srun_client_vpic_bdcats.sh's
# own srun step can run concurrently within the same allocation.
#
# Required env: BIN_DIR, PDC_DATA_LOC, PDC_TMPDIR, NUM_NODES,
#   SERVERS_PER_NODE, SERVER_TOTAL_TASKS, LOG_TAG, RESULTS_DIR

set -xeu

rm -rf "${PDC_DATA_LOC:?}"/*
rm -rf "${PDC_TMPDIR:?}"/*

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$SERVER_TOTAL_TASKS" \
  --ntasks-per-node="$SERVERS_PER_NODE" \
  --error="${RESULTS_DIR}/server_${LOG_TAG}_${NUM_NODES}.err" \
  --output="${RESULTS_DIR}/server_${LOG_TAG}_${NUM_NODES}.log" \
  ./pdc_server &
popd

# Give the servers time to stand up and publish their address info before
# the client tries to look them up.
sleep 10
