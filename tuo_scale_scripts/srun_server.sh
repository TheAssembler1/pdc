#!/bin/bash
# Start pdc_server in the background for this job's node count. Mirrors
# pdc_helper_scripts/curl_analysis_scripts/srun_server.sh: launched
# without the `restart` argument, so it always starts with an empty
# in-memory metadata table regardless of what's on disk from a previous
# run -- the rm -rf below is still needed because the per-rank
# checkpoint/address files under PDC_TMPDIR (== PDC_DATA_LOC here) would
# otherwise be observed by this fresh, non-restart launch. Shutdown is
# graceful via srun_close_server.sh (the close_server client RPC), not a
# captured PID -- this srun step outlives this script under the same
# sbatch allocation, backgrounded with `&` so srun_client_vpic_bdcats.sh's
# own srun step can run concurrently within the same allocation.
#
# Required env: BIN_DIR, PDC_DATA_LOC, PDC_TMPDIR, NUM_NODES,
#   SERVERS_PER_NODE, SERVER_TOTAL_TASKS, LOG_TAG, RESULTS_DIR
#
# HG_TRANSPORT/HG_HOST: pdc_server defaults to "ofi+tcp" unless it detects
# Perlmutter specifically (PDC_get_default_mercury_transport(),
# pdc_server.c:228) -- on Tuolumne that default is simply wrong: libfabric
# here only has the cxi provider built in (confirmed via a real 2-node
# run's stderr: `na_ofi_provider_check(): Requested OFI provider
# "tcp;ofi_rxm" ... is not available ... available providers: cxi`).
# Force ofi+cxi explicitly instead of fixing the Perlmutter-only
# detection itself.
#
# HG_HOST is just "cxi0", with no per-rank id suffix -- an earlier version
# mirrored curl_analysis_scripts' Perlmutter HG_HOST=cxi0:$SLURM_LOCALID,
# but SLURM_LOCALID is unbound under Tuolumne's Flux Slurm-compatibility
# shim (same as SLURM_SUBMIT_DIR/SLURM_JOB_NUM_NODES, fixed earlier), so
# it silently evaluated to empty for every rank. Confirmed harmless for
# the server specifically: a real run with the resulting connection
# string "ofi+cxi://cxi0::7000" (empty id) on all 8 ranks/2 nodes still
# printed "Server ready!" -- the port (pdc_server_rank_g % 32 + 7000,
# already unique per global rank) is what actually disambiguates
# processes sharing a NIC here, not the HG_HOST suffix.

set -xeu

rm -rf "${PDC_DATA_LOC:?}"/*
rm -rf "${PDC_TMPDIR:?}"/*

pushd "$BIN_DIR"
srun \
  -N "$NUM_NODES" \
  -n "$SERVER_TOTAL_TASKS" \
  --ntasks-per-node="$SERVERS_PER_NODE" \
  --error="${RESULTS_DIR}/server_${LOG_TAG}_${NUM_NODES}.err" \
  --output="${RESULTS_DIR}/server_${LOG_TAG}_${NUM_NODES}.log" \
  bash -c 'export HG_TRANSPORT=ofi+cxi; export HG_HOST=cxi0; exec ./pdc_server' &
popd

# Give the servers time to stand up and publish their address info before
# the client tries to look them up.
sleep 10
