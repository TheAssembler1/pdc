#!/bin/bash
# vpic_bdcats scaling job for one node count, one transfer mode --
# submitted via `flux batch` by run_sync.sh / run_async.sh /
# run_small_scale.sh, never invoked directly. See src/tests/misc/vpic_bdcats.c
# for what the benchmark itself does (write 5 timesteps, then read every
# one back in the same process so the server's region cache is still warm).
#
# Required env (set by the calling run_*.sh, which sources common.sh):
#   NUM_NODES, MODE (sync|async), SERVERS_PER_NODE, CLIENTS_PER_NODE,
#   STEPS, NUMPARTICLES, PDC_DATA_LOC, PDC_TMPDIR, BIN_DIR, RESULTS_DIR
# Additional required env when MODE=async:
#   ASYNC_SLEEP_S
#
# NOTE: `flux run` rejects -n/--ntasks combined with --tasks-per-node
# ("Per-resource options can't be used with per-task options") -- fixed
# after hitting this for real on Tuolumne; only -N + --tasks-per-node is
# passed now. The rest of this script is still only validated as far as
# that point -- if something past the first `flux run` breaks, it's
# unverified past here.

set -xeu

NUM_NODES=${NUM_NODES:?}
MODE=${MODE:?MODE must be sync or async}
SERVERS_PER_NODE=${SERVERS_PER_NODE:?}
CLIENTS_PER_NODE=${CLIENTS_PER_NODE:?}
STEPS=${STEPS:?}
NUMPARTICLES=${NUMPARTICLES:?}
PDC_DATA_LOC=${PDC_DATA_LOC:?}
PDC_TMPDIR=${PDC_TMPDIR:?}
BIN_DIR=${BIN_DIR:?}
RESULTS_DIR=${RESULTS_DIR:?}
ASYNC_SLEEP_S=${ASYNC_SLEEP_S:-0}

mkdir -p "$RESULTS_DIR" "$PDC_DATA_LOC"

# Remove data from any previous run before this one starts -- at up to
# 4 TiB/run this matters both for correctness (stale objects from a prior
# run under the same names) and for not silently filling the filesystem.
# Thorough rm, not a narrower glob -- a narrower pattern here previously
# caused a reproduced hang in the curl benchmarks (see
# pdc_helper_scripts/curl_analysis_scripts' srun_server.sh history).
rm -rf "${PDC_DATA_LOC:?}"/*
rm -rf "${PDC_TMPDIR:?}"/*

cd "$BIN_DIR"

SERVER_LOG="${RESULTS_DIR}/server_${MODE}_${NUM_NODES}.log"
CLIENT_LOG="${RESULTS_DIR}/client_${MODE}_${NUM_NODES}.log"
OUT_CSV="${RESULTS_DIR}/vpic_bdcats_${MODE}_${NUM_NODES}.csv"

flux run -N "$NUM_NODES" --tasks-per-node="$SERVERS_PER_NODE" \
  --output="$SERVER_LOG" ./pdc_server &
SERVER_JOB_PID=$!

# Give the servers time to come up and write their connection info before
# the client tries to look them up -- mirrors the sleep already used in
# this repo's Slurm srun_*.sh scripts and run_multiple_mpi_test.sh.
sleep 5

flux run -N "$NUM_NODES" --tasks-per-node="$CLIENTS_PER_NODE" \
  --output="$CLIENT_LOG" \
  ./vpic_bdcats "$NUMPARTICLES" "$STEPS" "$MODE" "$ASYNC_SLEEP_S"

flux run -N "$NUM_NODES" --tasks-per-node="$SERVERS_PER_NODE" \
  ./close_server

wait "$SERVER_JOB_PID" || true

# vpic_bdcats.c prints its CSV (header + api_call/throughput/data-size
# rows) interleaved with ordinary LOG_WARNING lines on rank 0's stdout --
# keep only the CSV lines, then tag the row with this run's shape so a
# concatenated sweep file stays self-describing.
grep -E "^(record_type,|api_call,|throughput_write_MBps,|throughput_read_MBps,|total_data_size_bytes,|data_size_per_rank_bytes,)" \
  "$CLIENT_LOG" > "$OUT_CSV"
echo "# n_nodes=${NUM_NODES},servers_per_node=${SERVERS_PER_NODE},clients_per_node=${CLIENTS_PER_NODE},mode=${MODE},steps=${STEPS},numparticles_per_rank=${NUMPARTICLES},async_sleep_s=${ASYNC_SLEEP_S}" >> "$OUT_CSV"

echo "Wrote $OUT_CSV"
