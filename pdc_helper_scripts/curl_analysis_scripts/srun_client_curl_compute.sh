#!/bin/bash
# Posthoc phase 2: run bench_curl_compute, a freshly launched client that
# opens the u/v/w objects a prior, already-exited write-phase job
# created, reads them back, computes curl client-side, and writes it back
# as three plain PDC objects (curl_x, curl_y, curl_z). Must run after
# srun_server_restart.sh so the server it's talking to actually reloaded
# that data from its checkpoint rather than just keeping it in memory.
#
# Required env: BIN_DIR, NUM_NODES, CLIENTS_PER_NODE, CLIENT_TOTAL_TASKS,
#   NX, NY, NZ_PER_RANK, LOG_TAG, COMPUTE_LOG

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_${LOG_TAG}_${NUM_NODES}.log" \
  --error="client_${LOG_TAG}_${NUM_NODES}.err" \
  bash -c 'export HG_HOST=cxi0:$((SLURM_LOCALID + 8)); exec ./bench_curl_compute "$NX" "$NY" "$NZ_PER_RANK"'
popd

line=$(grep "^curl_posthoc_compute," "$BIN_DIR/client_${LOG_TAG}_${NUM_NODES}.log" | tail -1)
if [ -z "$line" ]; then
  echo "curl_posthoc_compute,${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},FAILED" > "$COMPUTE_LOG"
else
  echo "$line" > "$COMPUTE_LOG"
fi
