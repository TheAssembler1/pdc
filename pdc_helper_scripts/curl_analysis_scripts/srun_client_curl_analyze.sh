#!/bin/bash
# Posthoc phase 3: run bench_curl_analyze, a freshly launched client that
# opens the curl_x/y/z objects a prior, already-exited compute-phase job
# created, reads them back, computes vorticity magnitude client-side, and
# writes it back as a plain PDC object (optionally GPU-ZFP-compressed
# with COMPRESS=1, via the existing transformation framework -- see
# src/tests/analysis/bench_curl_analyze.c). Must run after
# srun_server_restart.sh so the server it's talking to actually reloaded
# that data from its checkpoint rather than just keeping it in memory.
#
# Required env: BIN_DIR, NUM_NODES, CLIENTS_PER_NODE, CLIENT_TOTAL_TASKS,
#   NX, NY, NZ_PER_RANK, COMPRESS, LOG_TAG, ANALYZE_LOG

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_${LOG_TAG}_${NUM_NODES}.log" \
  --error="client_${LOG_TAG}_${NUM_NODES}.err" \
  bash -c 'export HG_HOST=cxi0:$((SLURM_LOCALID + 8)); exec ./bench_curl_analyze "$NX" "$NY" "$NZ_PER_RANK" "$COMPRESS"'
popd

line=$(grep "^curl_posthoc_analyze," "$BIN_DIR/client_${LOG_TAG}_${NUM_NODES}.log" | tail -1)
if [ -z "$line" ]; then
  echo "curl_posthoc_analyze,${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},${COMPRESS},FAILED" > "$ANALYZE_LOG"
else
  echo "$line" > "$ANALYZE_LOG"
fi
