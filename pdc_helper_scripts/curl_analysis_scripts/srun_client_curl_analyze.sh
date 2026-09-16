#!/bin/bash
# Posthoc phase 2: run bench_curl_analyze, a freshly launched client that
# opens the u/v/w objects a prior, already-exited write-phase job
# created, reads them back, computes curl then vorticity magnitude
# client-side, and writes both curl_x/y/z and vorticity_magnitude back as
# plain PDC objects (optionally GPU-ZFP-compressing vorticity_magnitude
# with COMPRESS=1, via the existing transformation framework -- see
# src/tests/analysis/bench_curl_analyze.c). Must run after
# srun_server_restart.sh so the server it's talking to actually reloaded
# u/v/w from its checkpoint rather than just keeping them in memory.
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
