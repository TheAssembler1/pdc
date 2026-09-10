#!/bin/bash
# HDF5 curl benchmark, posthoc phase 3: run hdf5_bench_curl_analyze, a
# freshly launched client that reopens the file a prior, already-exited
# compute-phase job wrote curl_x/y/z into, reads them back, computes
# vorticity magnitude client-side, and writes it back as a fourth
# dataset (vorticity_magnitude).
#
# Required env: BIN_DIR (hdf5_analysis_test/ directory, holding the built
#   hdf5_bench_curl_analyze binary), NUM_NODES, CLIENTS_PER_NODE,
#   CLIENT_TOTAL_TASKS, NX, NY, NZ_PER_RANK, LOG_TAG, ANALYZE_LOG, OUT_FILE

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_${LOG_TAG}_analyze_${NUM_NODES}.log" \
  --error="client_${LOG_TAG}_analyze_${NUM_NODES}.err" \
  ./hdf5_bench_curl_analyze "$NX" "$NY" "$NZ_PER_RANK" "$OUT_FILE"
popd

line=$(grep "^curl_posthoc_analyze," "$BIN_DIR/client_${LOG_TAG}_analyze_${NUM_NODES}.log" | tail -1)
if [ -z "$line" ]; then
  echo "curl_posthoc_analyze,${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},FAILED" > "$ANALYZE_LOG"
else
  echo "$line" > "$ANALYZE_LOG"
fi
