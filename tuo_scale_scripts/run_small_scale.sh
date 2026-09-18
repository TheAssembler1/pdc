#!/bin/bash
# Small, fast sanity check of the whole pipeline (flux batch submission,
# pdc_server startup, vpic_bdcats write+read-back, CSV extraction) on a
# single node with a tiny particle count -- run this BEFORE run_sync.sh /
# run_async.sh to catch config problems (bad PDC_DATA_LOC, wrong flux
# flags, etc.) without waiting on a real node-count sweep or writing
# anything close to the full sweep's ~4 TiB.
#
# Runs both sync and async (short sleep) modes, 1 node, 2 timesteps,
# 1024 particles/rank -- seconds, not the tens of minutes a real sweep
# step takes.
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

set -eu
cd "$(dirname "$0")"

# Deliberately override common.sh's weak-scaling NUMPARTICLES/STEPS with
# tiny fixed values for this debug run, not the real sweep sizing.
export CLIENTS_PER_NODE=${CLIENTS_PER_NODE:-4}
export NUMPARTICLES=1024
export STEPS=2
source ./common.sh

export RESULTS_DIR=${RESULTS_DIR:-$PWD/results_small_scale_$(date +%Y%m%d_%H%M%S)}
mkdir -p "$RESULTS_DIR"
echo "Results will land in: $RESULTS_DIR"

NODES=1

for MODE in sync async; do
    echo "=== [small-scale, ${MODE}] Submitting vpic_bdcats debug job: ${NODES} node ==="
    jid=$(flux batch \
            --job-name="vpic-bdcats-small-${MODE}" \
            --nodes="$NODES" \
            --exclusive \
            --time-limit=10m \
            --env=NUM_NODES="$NODES" \
            --env=MODE="$MODE" \
            --env=ASYNC_SLEEP_S=1 \
            --env=SERVERS_PER_NODE="$SERVERS_PER_NODE" \
            --env=CLIENTS_PER_NODE="$CLIENTS_PER_NODE" \
            --env=STEPS="$STEPS" \
            --env=NUMPARTICLES="$NUMPARTICLES" \
            --env=PDC_DATA_LOC="$PDC_DATA_LOC" \
            --env=PDC_TMPDIR="$PDC_TMPDIR" \
            --env=BIN_DIR="$BIN_DIR" \
            --env=RESULTS_DIR="$RESULTS_DIR" \
            vpic_bdcats_job.flux.sh)
    echo "Submitted $jid, waiting for completion..."
    flux job attach "$jid"
    echo "Finished $jid"
    echo "--- $(cat "${RESULTS_DIR}/vpic_bdcats_${MODE}_${NODES}.csv" | head -20) ---"
done

echo "Done. If both modes above show a clean CSV (api_call rows, throughput"
echo "rows, no mismatches in ${RESULTS_DIR}/client_*_1.log), the pipeline is"
echo "good to go -- proceed to run_sync.sh / run_async.sh."
