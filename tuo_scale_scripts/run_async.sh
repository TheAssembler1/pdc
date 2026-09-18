#!/bin/bash
# Sweeps vpic_bdcats in "async" transfer mode (sleep(SLEEP_TIME) between
# transfer start and wait, standing in for compute overlapped with
# in-flight I/O) across NODE_COUNTS (common.sh; 1, 2, 4, ..., 128 nodes),
# one flux batch job at a time -- see run_sync.sh's header comment for why
# this blocks on `flux job attach` between node counts instead of using a
# scheduler-side dependency flag.
#
# Run tuo_scale_scripts/run_small_scale.sh first to sanity-check the whole
# pipeline (server, client, CSV extraction) on a single node before
# committing to this full sweep.
#
# NOTE: written against the documented `flux batch`/`flux job attach` CLI
# but not validated against a real Flux instance -- see README.md.
#
# Required: export PDC_DATA_LOC to real parallel scratch first (see
# common.sh) -- this sweep's 128-node step alone writes ~4 TiB.

# --- sleep time (seconds) between transfer start and wait, i.e. the ---
# --- simulated compute duration overlapped with in-flight I/O.       ---
SLEEP_TIME=2

set -eu
cd "$(dirname "$0")"
source ./common.sh

export MODE=async
export ASYNC_SLEEP_S="$SLEEP_TIME"
export RESULTS_DIR=${RESULTS_DIR:-$PWD/results_async_$(date +%Y%m%d_%H%M%S)}
mkdir -p "$RESULTS_DIR"
echo "Results will land in: $RESULTS_DIR"

for nodes in "${NODE_COUNTS[@]}"; do
    echo "=== [async, sleep=${SLEEP_TIME}s] Submitting vpic_bdcats scale job: $nodes node(s) ==="
    jid=$(flux batch \
            --job-name="vpic-bdcats-async-${nodes}" \
            --nodes="$nodes" \
            -n "$((nodes * (SERVERS_PER_NODE + CLIENTS_PER_NODE)))" \
            --time-limit=30m \
            --env=NUM_NODES="$nodes" \
            --env=MODE="$MODE" \
            --env=ASYNC_SLEEP_S="$ASYNC_SLEEP_S" \
            --env=SERVERS_PER_NODE="$SERVERS_PER_NODE" \
            --env=CLIENTS_PER_NODE="$CLIENTS_PER_NODE" \
            --env=STEPS="$STEPS" \
            --env=NUMPARTICLES="$NUMPARTICLES" \
            --env=PDC_DATA_LOC="$PDC_DATA_LOC" \
            --env=PDC_TMPDIR="$PDC_TMPDIR" \
            --env=BIN_DIR="$BIN_DIR" \
            --env=RESULTS_DIR="$RESULTS_DIR" \
            vpic_bdcats_job.flux.sh)
    echo "Submitted $jid ($nodes node(s)), waiting for completion..."
    flux job attach "$jid"
    echo "Finished $jid"
done

echo "Done. Per-node-count CSVs in $RESULTS_DIR"
