#!/bin/bash
# Run bench_magnitude in "eager" mode: components are written through the
# region-analysis framework (DataFlyway) with the vector_magnitude graph
# attached, so the last component write transparently triggers server-side
# magnitude computation in the write path. See
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
  ./bench_magnitude eager "$N_ELEM"
popd

line=$(grep "^eager," "$BIN_DIR/client_${LOG_TAG}_${RANKS}.log" | tail -1)
if [ -z "$line" ]; then
  echo "eager,${RANKS},${N_ELEM},FAILED" >> "$RESULTS"
else
  echo "$line" >> "$RESULTS"
fi
