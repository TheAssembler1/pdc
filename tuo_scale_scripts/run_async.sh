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
# NOTE: this reserves whole nodes (--exclusive) rather than trying to
# precisely count SERVERS_PER_NODE + CLIENTS_PER_NODE resource "slots" via
# -n. An earlier version used -n with that exact count and still hit
# "waiting for resources" on the client's inner flux run (confirmed via
# `flux proxy <jobid> flux job attach <inner-jobid>`) even though
# `flux resource list` showed 96 idle cores/node -- a slot's default size
# didn't map 1:1 onto a task the way that math assumed. --exclusive sidesteps
# guessing the exact mapping by giving the whole node to the sub-instance,
# which the two inner flux run/submit calls (each independently sized via
# --tasks-per-node) then divide up themselves. See README.md.
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
            --exclusive \
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
