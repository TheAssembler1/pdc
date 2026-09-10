#!/bin/bash
# Posthoc phase 1: run bench_curl_write, which writes u,v,w as plain PDC
# objects (no graph attached) and exits. See srun_client_curl_compute.sh
# (phase 2) and srun_client_curl_analyze.sh (phase 3), run as separate
# srun steps once every rank here has exited -- see
# curl_posthoc_analysis.sbatch.
#
# Required env: BIN_DIR, NUM_NODES, CLIENTS_PER_NODE, CLIENT_TOTAL_TASKS,
#   NX, NY, NZ_PER_RANK, LOG_TAG, WRITE_LOG

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_${LOG_TAG}_${NUM_NODES}.log" \
  --error="client_${LOG_TAG}_${NUM_NODES}.err" \
  bash -c 'export HG_HOST=cxi0:$((SLURM_LOCALID + 8)); exec ./bench_curl_write "$NX" "$NY" "$NZ_PER_RANK"'
popd

line=$(grep "^curl_posthoc_write," "$BIN_DIR/client_${LOG_TAG}_${NUM_NODES}.log" | tail -1)
if [ -z "$line" ]; then
  echo "curl_posthoc_write,${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},FAILED" > "$WRITE_LOG"
else
  echo "$line" > "$WRITE_LOG"
fi
