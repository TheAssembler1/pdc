#!/bin/bash
# vpic_bdcats scaling job for one node count, one transfer mode --
# submitted via `sbatch --nodes=N ...` by run_sync.sh / run_async.sh /
# run_small_scale.sh, never invoked directly. See
# src/tests/misc/vpic_bdcats.c for what the benchmark itself does (write 5
# timesteps, then read every one back in the same process so the server's
# region cache is still warm).
#
# Mirrors pdc_helper_scripts/curl_analysis_scripts/curl_eager_analysis.sbatch
# exactly: one sbatch allocation sized to SERVERS_PER_NODE +
# CLIENTS_PER_NODE tasks/node, a backgrounded (`&`) srun step for the
# server and a foreground srun step for the client, both able to run
# concurrently because Slurm's default job-step semantics let multiple
# srun steps within one sbatch allocation share it, as long as their
# combined per-node task request doesn't exceed --ntasks-per-node. This
# replaces an earlier Flux (`flux batch`/`flux run`/`flux submit`)
# implementation in this directory that never got the two steps
# co-scheduled even with a whole exclusive node reserved -- see git
# history for that attempt. --nodes/--ntasks-per-node/--cpus-per-task
# below are just defaults for direct manual submission; run_*.sh always
# overrides them from common.sh's SERVERS_PER_NODE/CLIENTS_PER_NODE.
#
# Required env (set by the calling run_*.sh, which sources common.sh):
#   NUM_NODES, SERVERS_PER_NODE, CLIENTS_PER_NODE, STEPS, NUMPARTICLES,
#   MODE (sync|async), PDC_DATA_LOC, PDC_TMPDIR, BIN_DIR, RESULTS_DIR
# Additional required env when MODE=async:
#   ASYNC_SLEEP_S
#
# NUM_NODES is passed explicitly by the caller rather than read from
# SLURM_JOB_NUM_NODES -- Tuolumne's sbatch/srun are Flux's own Slurm-
# compatibility wrappers (its rc2 runs this script directly as a Flux
# job), not real Slurm, and that shim doesn't populate SLURM_JOB_NUM_NODES
# (confirmed: "line 45: SLURM_JOB_NUM_NODES: unbound variable" on a real
# run). Every run_*.sh already knows the node count it's submitting with,
# so there's no need to recover it from a Slurm-specific env var this
# shim doesn't set.

#SBATCH --job-name=vpic-bdcats-scale
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=36
#SBATCH --cpus-per-task=2
#SBATCH --partition=pbatch
#SBATCH --time=00:30:00
#SBATCH --output=%x_%j.log

set -xeu
# Tuolumne's sbatch/srun are Flux's own Slurm-compatibility wrappers (its
# rc2 runs this script directly as a Flux job), not real Slurm -- Flux's
# shim doesn't populate SLURM_SUBMIT_DIR the way real Slurm does. The job
# already starts in the submission directory under this shim, so fall
# back to $PWD instead of failing on the unset var.
cd "${SLURM_SUBMIT_DIR:-$PWD}"

NUM_NODES=${NUM_NODES:?}
export NUM_NODES
SERVERS_PER_NODE=${SERVERS_PER_NODE:?}
CLIENTS_PER_NODE=${CLIENTS_PER_NODE:?}
export SERVERS_PER_NODE CLIENTS_PER_NODE
export SERVER_TOTAL_TASKS=$((NUM_NODES * SERVERS_PER_NODE))
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export STEPS=${STEPS:?}
export NUMPARTICLES=${NUMPARTICLES:?}
export MODE=${MODE:?MODE must be sync or async}
export ASYNC_SLEEP_S=${ASYNC_SLEEP_S:-0}

export BIN_DIR=${BIN_DIR:?}
export PDC_DATA_LOC=${PDC_DATA_LOC:?}
export PDC_TMPDIR=${PDC_TMPDIR:-$PDC_DATA_LOC}
export RESULTS_DIR=${RESULTS_DIR:?}
mkdir -p "$RESULTS_DIR" "$PDC_DATA_LOC"

export LOG_TAG="vpic_bdcats_${MODE}"
OUT_CSV="${RESULTS_DIR}/vpic_bdcats_${MODE}_${NUM_NODES}.csv"
export OUT_CSV

./srun_server.sh
./srun_client_vpic_bdcats.sh
./srun_close_server.sh

echo "# n_nodes=${NUM_NODES},servers_per_node=${SERVERS_PER_NODE},clients_per_node=${CLIENTS_PER_NODE},mode=${MODE},steps=${STEPS},numparticles_per_rank=${NUMPARTICLES},async_sleep_s=${ASYNC_SLEEP_S}" >> "$OUT_CSV"

echo "Wrote $OUT_CSV"
