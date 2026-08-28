#!/bin/bash
# Run bench_magnitude in "posthoc" mode: components are written as plain PDC
# objects (no graph attached), read back to the client, magnitude is
# computed client-side, then written back as a plain PDC object -- a
# hand-built materialized view outside the transform framework. See
# src/tests/analysis/bench_magnitude.c for the full timing breakdown.
#
# Required env: BIN_DIR, RANKS, N_ELEM, LOG_TAG, RESULTS

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$RANKS" \
  -n "$RANKS" \
  --ntasks-per-node=1 \
  --output="client_${LOG_TAG}_${RANKS}.log" \
  --error="client_${LOG_TAG}_${RANKS}.err" \
  ./bench_magnitude posthoc "$N_ELEM"
popd

line=$(grep "^posthoc," "$BIN_DIR/client_${LOG_TAG}_${RANKS}.log" | tail -1)
if [ -z "$line" ]; then
  echo "posthoc,${RANKS},${N_ELEM},FAILED" >> "$RESULTS"
else
  echo "$line" >> "$RESULTS"
fi
