#!/bin/bash
# Run vpic_bdcats (write 5 timesteps, then read every one back in the
# same process) against the server srun_server.sh already started in this
# same sbatch allocation. Mirrors
# pdc_helper_scripts/curl_analysis_scripts/srun_client_curl_eager.sh.
#
# Required env: BIN_DIR, NUM_NODES, CLIENTS_PER_NODE, CLIENT_TOTAL_TASKS,
#   NUMPARTICLES, STEPS, MODE, ASYNC_SLEEP_S, LOG_TAG, RESULTS_DIR, OUT_CSV

set -xeu

CLIENT_LOG="${RESULTS_DIR}/client_${LOG_TAG}_${NUM_NODES}.log"

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="$CLIENT_LOG" \
  --error="${RESULTS_DIR}/client_${LOG_TAG}_${NUM_NODES}.err" \
  ./vpic_bdcats "$NUMPARTICLES" "$STEPS" "$MODE" "$ASYNC_SLEEP_S"
popd

# vpic_bdcats.c prints its CSV (header + api_call/throughput/data-size
# rows) interleaved with ordinary LOG_WARNING lines on rank 0's stdout --
# keep only the CSV lines.
grep -E "^(record_type,|api_call,|throughput_write_MBps,|throughput_read_MBps,|total_data_size_bytes,|data_size_per_rank_bytes,)" \
  "$CLIENT_LOG" > "$OUT_CSV"
