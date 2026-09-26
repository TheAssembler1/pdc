#!/bin/bash
# PDC post-hoc (read-back, recompute, write-back) magnitude analysis --
# single node-count step. 2 data servers/node, 32 client ranks/node (see
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
# Submit one of these per node count via posthoc_pdc_run.sh, or
# directly, e.g.: sbatch --nodes=4 posthoc_pdc.sbatch
# Override N_ELEM at submit time, e.g.:
#   N_ELEM=33554432 sbatch --nodes=4 posthoc_pdc.sbatch

#SBATCH --job-name=pdc-posthoc-magnitude
#SBATCH --nodes=8
#SBATCH --ntasks-per-node=34
#SBATCH --cpus-per-task=3
#SBATCH --account=m2621
#SBATCH --time=00:30:00
#SBATCH --constraint=cpu
#SBATCH --qos=regular
#SBATCH --output=%x_%j.log

set -u
cd "$SLURM_SUBMIT_DIR"

# SERVERS_PER_NODE, CLIENTS_PER_NODE, N_ELEM: see ../common.sh -- this
# file's own #SBATCH --ntasks-per-node above must still equal
# SERVERS_PER_NODE + CLIENTS_PER_NODE (see that file's caveat).
source "$SLURM_SUBMIT_DIR/../common.sh"

export NUM_NODES=$SLURM_JOB_NUM_NODES
export SERVER_TOTAL_TASKS=$((NUM_NODES * SERVERS_PER_NODE))
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export BIN_DIR=${BIN_DIR:-$PWD/../../build/bin}
export PDC_DATA_LOC=${PDC_DATA_LOC:-$SCRATCH/pdc_analysis_bench}
export PDC_TMPDIR=${PDC_TMPDIR:-$PDC_DATA_LOC}
mkdir -p "$PDC_DATA_LOC"

export LOG_TAG=posthoc
export RESULTS=$PWD/results_posthoc_${SLURM_JOB_ID}.csv
export WRITE_LOG=$PWD/write_${LOG_TAG}_${SLURM_JOB_ID}.csv
export ANALYZE_LOG=$PWD/analyze_${LOG_TAG}_${SLURM_JOB_ID}.csv

echo "mode,step,n_ranks,n_elem,write_setup_s,write_s,relaunch_s,analyze_setup_s,readback_s,compute_s,writeback_s,total_s,bad" > "$RESULTS"

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

# Both WRITE_LOG and ANALYZE_LOG now hold one line per timestep
# (N_TIMESTEPS=3 in bench_write_components.c / bench_posthoc_analyze.c),
# in matching step order, so pair them up line-by-line via two file
# descriptors and emit one combined "posthoc" row per timestep.
exec 3< "$WRITE_LOG"
exec 4< "$ANALYZE_LOG"
while IFS=, read -r w_mode w_step w_nranks w_nelem w_setup w_write <&3 && \
      IFS=, read -r a_mode a_step a_nranks a_nelem a_setup a_readback a_compute a_writeback a_total a_bad <&4; do
  if [ "$w_setup" = "FAILED" ] || [ "$a_setup" = "FAILED" ]; then
    echo "posthoc,${w_step},${CLIENT_TOTAL_TASKS},${N_ELEM},FAILED" >> "$RESULTS"
  else
    total=$(awk "BEGIN{print $w_setup + $w_write + $relaunch_s + $a_setup + $a_readback + $a_compute + $a_writeback}")
    echo "posthoc,${w_step},${CLIENT_TOTAL_TASKS},${N_ELEM},${w_setup},${w_write},${relaunch_s},${a_setup},${a_readback},${a_compute},${a_writeback},${total},${a_bad}" >> "$RESULTS"
  fi
done
exec 3<&-
exec 4<&-

# The mid-job checkpoint (relaunch_s, above) already folds in one
# close_server call, but the final ./srun_close_server.sh right before
# this point checkpoints again and isn't captured in total_s at all --
# average its per-server-rank "total close time" lines the same way
# eager/lazy do, so results_posthoc_*.csv's close-inclusive total lines
# up with theirs and with results_hdf5_*.csv (which has no server to
# close, so its own close column is always 0).
CLOSE_LOG="$BIN_DIR/close_server_${LOG_TAG}_${NUM_NODES}.log"
avg_close_s=$(grep 'total close time' "$CLOSE_LOG" 2>/dev/null | \
  sed -n 's/.*total close time = \([0-9.]*\).*/\1/p' | \
  awk '{sum+=$1; n++} END{if(n>0) printf "%.6f", sum/n; else print "0"}')
awk -F, -v OFS=, -v avgclose="$avg_close_s" '
  NR==1 { print $0, "avg_close_s", "total_with_close_s"; next }
  NF==13 { print $0, avgclose, ($12 + avgclose); next }
  { print }
' "$RESULTS" > "${RESULTS}.tmp" && mv "${RESULTS}.tmp" "$RESULTS"

echo "Done. Results in $RESULTS"
