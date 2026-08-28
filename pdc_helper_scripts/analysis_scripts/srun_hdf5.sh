#!/bin/bash
# Run the parallel-HDF5 magnitude benchmark (hdf5_analysis_test/). No PDC
# server involved -- HDF5 talks straight to the parallel filesystem.
#
# Required env: BIN_DIR (hdf5_analysis_test/ directory, holding the built
#   hdf5_bench_magnitude binary), NUM_NODES, CLIENTS_PER_NODE,
#   CLIENT_TOTAL_TASKS, N_ELEM, LOG_TAG, RESULTS, OUT_FILE (pre-striped
#   path this benchmark writes to -- see hdf5_analysis.sbatch)

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="client_${LOG_TAG}_${NUM_NODES}.log" \
  --error="client_${LOG_TAG}_${NUM_NODES}.err" \
  ./hdf5_bench_magnitude "$N_ELEM" "$OUT_FILE"
popd

line=$(grep "^hdf5," "$BIN_DIR/client_${LOG_TAG}_${NUM_NODES}.log" | tail -1)
if [ -z "$line" ]; then
  echo "hdf5,${CLIENT_TOTAL_TASKS},${N_ELEM},FAILED" >> "$RESULTS"
else
  echo "$line" >> "$RESULTS"
fi
