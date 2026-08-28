#!/bin/bash
# Run the parallel-HDF5 magnitude benchmark (hdf5_analysis_test/). No PDC
# server involved -- HDF5 talks straight to the parallel filesystem.
#
# Required env: BIN_DIR (hdf5_analysis_test/ directory, holding the built
#   hdf5_bench_magnitude binary), RANKS, N_ELEM, LOG_TAG, RESULTS,
#   OUT_DIR (scratch dir for the HDF5 file this benchmark writes)

set -xeu

pushd "$BIN_DIR"
srun \
  -N "$RANKS" \
  -n "$RANKS" \
  --ntasks-per-node=1 \
  --output="client_${LOG_TAG}_${RANKS}.log" \
  --error="client_${LOG_TAG}_${RANKS}.err" \
  ./hdf5_bench_magnitude "$N_ELEM" "${OUT_DIR}/hdf5_bench_magnitude_${RANKS}.h5"
popd

line=$(grep "^hdf5," "$BIN_DIR/client_${LOG_TAG}_${RANKS}.log" | tail -1)
if [ -z "$line" ]; then
  echo "hdf5,${RANKS},${N_ELEM},FAILED" >> "$RESULTS"
else
  echo "$line" >> "$RESULTS"
fi
