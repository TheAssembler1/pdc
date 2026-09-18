#!/bin/bash
# Run vpic_bdcats (write 5 timesteps, then read every one back in the
# same process) against the server srun_server.sh already started in this
# same sbatch allocation. Mirrors
# pdc_helper_scripts/curl_analysis_scripts/srun_client_curl_eager.sh.
#
# Required env: BIN_DIR, NUM_NODES, CLIENTS_PER_NODE, CLIENT_TOTAL_TASKS,
#   NUMPARTICLES, STEPS, MODE, ASYNC_SLEEP_S, LOG_TAG, RESULTS_DIR, OUT_CSV
#
# HG_TRANSPORT/HG_HOST: see srun_server.sh -- Tuolumne's libfabric only has
# the cxi provider, not the "ofi+tcp" pdc_client_connect.c defaults to
# off-Perlmutter. Client endpoint ids are offset past SERVERS_PER_NODE so
# server and client ranks sharing a node never collide on the same cxi0:<id>
# endpoint -- mirrors
# pdc_helper_scripts/curl_analysis_scripts/srun_client_curl_eager.sh's
# HG_HOST=cxi0:$((SLURM_LOCALID + 8)) (8 == that script's SERVERS_PER_NODE).

set -xeu

CLIENT_LOG="${RESULTS_DIR}/client_${LOG_TAG}_${NUM_NODES}.log"

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="$CLIENT_LOG" \
  --error="${RESULTS_DIR}/client_${LOG_TAG}_${NUM_NODES}.err" \
  bash -c 'export HG_TRANSPORT=ofi+cxi; export HG_HOST=cxi0:$((SLURM_LOCALID + '"$SERVERS_PER_NODE"')); exec ./vpic_bdcats "$NUMPARTICLES" "$STEPS" "$MODE" "$ASYNC_SLEEP_S"'
popd

# vpic_bdcats.c prints its CSV (header + api_call/throughput/data-size
# rows) interleaved with ordinary LOG_WARNING lines on rank 0's stdout --
# keep only the CSV lines.
grep -E "^(record_type,|api_call,|throughput_write_MBps,|throughput_read_MBps,|total_data_size_bytes,|data_size_per_rank_bytes,)" \
  "$CLIENT_LOG" > "$OUT_CSV"
