#!/bin/bash
# PDC post-hoc (read-back, recompute, write-back) magnitude analysis --
# single node-count step. 8 data servers/node, 32 client ranks/node (see
# srun_server.sh / srun_client_posthoc_write.sh /
# srun_client_posthoc_analyze.sh), writing one CSV row to
# results_posthoc_<jobid>.csv.
#
# A real posthoc workflow writes data in one job, then some time later a
# separate job comes back to read it, compute a derived product, and
# persist that too -- so this drives two genuinely separate client srun
# steps with a full server close (which checkpoints) + restart (which
# reloads the checkpoint) in between, instead of doing
# write/readback/compute/writeback inside one continuous client process
# the way eager and lazy do. That relaunch cost is real and is exactly
# why posthoc should come out slower than eager, not an artifact to
# optimize away.
#
# Submit one of these per node count via posthoc_analysis_run.sh, or
# directly, e.g.: sbatch --nodes=4 posthoc_analysis.sbatch
# Override N_ELEM at submit time, e.g.:
#   N_ELEM=33554432 sbatch --nodes=4 posthoc_analysis.sbatch

#SBATCH --job-name=pdc-posthoc-magnitude
#SBATCH --nodes=8
#SBATCH --ntasks-per-node=40
#SBATCH --cpus-per-task=3
#SBATCH --account=m2621
#SBATCH --time=00:30:00
#SBATCH --constraint=cpu
#SBATCH --qos=regular
#SBATCH --output=%x_%j.log

set -u
cd "$SLURM_SUBMIT_DIR"

export NUM_NODES=$SLURM_JOB_NUM_NODES
export SERVERS_PER_NODE=8
export CLIENTS_PER_NODE=32
export SERVER_TOTAL_TASKS=$((NUM_NODES * SERVERS_PER_NODE))
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export BIN_DIR=${BIN_DIR:-$PWD/../../build/bin}
export PDC_DATA_LOC=${PDC_DATA_LOC:-$SCRATCH/pdc_analysis_bench}
export PDC_TMPDIR=${PDC_TMPDIR:-$PDC_DATA_LOC}
mkdir -p "$PDC_DATA_LOC"

export N_ELEM=${N_ELEM:-16777216}   # 64 MiB/rank of float32 -- at the max
                                     # scale point (128 nodes * 32 ranks/node
                                     # = 4096 ranks), this writes 320 MiB/rank
                                     # (192 MiB input + 128 MiB magnitude out),
                                     # ~1.25 TiB aggregate, comfortably under
                                     # the 2 TB budget.
export LOG_TAG=posthoc
export RESULTS=$PWD/results_posthoc_${SLURM_JOB_ID}.csv
export WRITE_LOG=$PWD/write_${LOG_TAG}_${SLURM_JOB_ID}.csv
export ANALYZE_LOG=$PWD/analyze_${LOG_TAG}_${SLURM_JOB_ID}.csv

echo "mode,n_ranks,n_elem,write_setup_s,write_s,relaunch_s,analyze_setup_s,readback_s,compute_s,writeback_s,total_s,bad" > "$RESULTS"

# Phase 1: write vx/vy/vz, then let every client rank fully exit.
./srun_server.sh
./srun_client_posthoc_write.sh

# Relaunch: close the server (checkpoints its metadata), then restart it
# with the `restart` argument so it reloads that checkpoint -- this is
# the real posthoc cost a single continuous session never pays.
relaunch_t0=$(date +%s.%N)
./srun_close_server.sh
./srun_server_restart.sh
relaunch_t1=$(date +%s.%N)
relaunch_s=$(awk "BEGIN{print $relaunch_t1 - $relaunch_t0}")

# Phase 2: a fresh client reads back, computes, writes back magnitude.
./srun_client_posthoc_analyze.sh
./srun_close_server.sh

IFS=, read -r w_mode w_nranks w_nelem w_setup w_write < "$WRITE_LOG"
IFS=, read -r a_mode a_nranks a_nelem a_setup a_readback a_compute a_writeback a_total a_bad < "$ANALYZE_LOG"

if [ "$w_setup" = "FAILED" ] || [ "$a_setup" = "FAILED" ]; then
  echo "posthoc,${CLIENT_TOTAL_TASKS},${N_ELEM},FAILED" >> "$RESULTS"
else
  total=$(awk "BEGIN{print $w_setup + $w_write + $relaunch_s + $a_setup + $a_readback + $a_compute + $a_writeback}")
  echo "posthoc,${CLIENT_TOTAL_TASKS},${N_ELEM},${w_setup},${w_write},${relaunch_s},${a_setup},${a_readback},${a_compute},${a_writeback},${total},${a_bad}" >> "$RESULTS"
fi

echo "Done. Results in $RESULTS"
