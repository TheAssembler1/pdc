#!/bin/bash
# HDF5 curl benchmark, posthoc phase 2: run hdf5_bench_curl_compute, a
# freshly launched client that reopens the file a prior, already-exited
# write-phase job created, reads u/v/w back, computes curl client-side
# (via the same curl_math.h kernel the PDC side uses), and writes it back
# as three datasets (curl_x, curl_y, curl_z).
#
# Required env: BIN_DIR (hdf5_analysis_test/ directory, holding the built
#   hdf5_bench_curl_compute binary), NUM_NODES, CLIENTS_PER_NODE,
#   CLIENT_TOTAL_TASKS, NX, NY, NZ_PER_RANK, LOG_TAG, COMPUTE_LOG, OUT_FILE

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_${LOG_TAG}_compute_${NUM_NODES}.log" \
  --error="client_${LOG_TAG}_compute_${NUM_NODES}.err" \
  ./hdf5_bench_curl_compute "$NX" "$NY" "$NZ_PER_RANK" "$OUT_FILE"
popd

line=$(grep "^curl_posthoc_compute," "$BIN_DIR/client_${LOG_TAG}_compute_${NUM_NODES}.log" | tail -1)
if [ -z "$line" ]; then
  echo "curl_posthoc_compute,${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},FAILED" > "$COMPUTE_LOG"
else
  echo "$line" > "$COMPUTE_LOG"
fi
