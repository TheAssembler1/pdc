#!/bin/bash
# Run bench_magnitude in "posthoc" mode: input objects are written as plain
# PDC objects (no graph attached), read back to the client, magnitude is
# computed client-side, then written back as a plain PDC object -- a
# hand-built materialized view outside the transform framework. See
# src/tests/analysis/bench_magnitude.c for the full timing breakdown.
#
# Required env: BIN_DIR, NUM_NODES, CLIENTS_PER_NODE, CLIENT_TOTAL_TASKS,
#   N_ELEM, LOG_TAG, RESULTS

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_${LOG_TAG}_${NUM_NODES}.log" \
  --error="client_${LOG_TAG}_${NUM_NODES}.err" \
  ./bench_magnitude posthoc "$N_ELEM"
popd

line=$(grep "^posthoc," "$BIN_DIR/client_${LOG_TAG}_${NUM_NODES}.log" | tail -1)
if [ -z "$line" ]; then
  echo "posthoc,${CLIENT_TOTAL_TASKS},${N_ELEM},FAILED" >> "$RESULTS"
else
  echo "$line" >> "$RESULTS"
fi
