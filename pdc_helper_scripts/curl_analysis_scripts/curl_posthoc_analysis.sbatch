#!/bin/bash
# PDC post-hoc curl + vorticity-magnitude analysis -- single node-count
# step. Three genuinely separate client processes (write u,v,w; compute
# curl; compute magnitude), with the server closed (checkpointing) and
# restarted (reloading that checkpoint) between each -- see
# srun_client_curl_write.sh / _compute.sh / _analyze.sh and
# srun_server_restart.sh. Writes one CSV row to
# results_curl_posthoc_<jobid>.csv.
#
# Submit one of these per node count via curl_posthoc_analysis_run.sh, or
# directly, e.g.: sbatch --nodes=4 curl_posthoc_analysis.sbatch
# Override NX/NY/NZ_PER_RANK/COMPRESS at submit time, e.g.:
#   COMPRESS=1 sbatch --nodes=4 curl_posthoc_analysis.sbatch

#SBATCH --job-name=pdc-curl-posthoc
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
export PDC_DATA_LOC=${PDC_DATA_LOC:-$SCRATCH/pdc_curl_analysis_bench}
export PDC_TMPDIR=${PDC_TMPDIR:-$PDC_DATA_LOC}
mkdir -p "$PDC_DATA_LOC"

export NX=${NX:-256}
export NY=${NY:-256}
export NZ_PER_RANK=${NZ_PER_RANK:-64}
export COMPRESS=${COMPRESS:-0}

export RESULTS=$PWD/results_curl_posthoc_${SLURM_JOB_ID}.csv
export WRITE_LOG=$PWD/write_curl_posthoc_${SLURM_JOB_ID}.csv
export COMPUTE_LOG=$PWD/compute_curl_posthoc_${SLURM_JOB_ID}.csv
export ANALYZE_LOG=$PWD/analyze_curl_posthoc_${SLURM_JOB_ID}.csv

echo "mode,n_ranks,nx,ny,nz_per_rank,compress,write_setup_s,write_s,relaunch1_s,compute_setup_s,readback1_s,curl_compute_s,writeback1_s,relaunch2_s,analyze_setup_s,readback2_s,magnitude_compute_s,writeback2_s,total_s,bad" > "$RESULTS"

# Phase 1: write u,v,w, then let every client rank fully exit.
export LOG_TAG=curl_posthoc_write
./srun_server.sh
./srun_client_curl_write.sh

# Relaunch 1: close the server (checkpoints its metadata), then restart
# it with the `restart` argument so it reloads that checkpoint -- the
# real posthoc cost a single continuous session never pays.
relaunch1_t0=$(date +%s.%N)
./srun_close_server.sh
./srun_server_restart.sh
relaunch1_t1=$(date +%s.%N)
relaunch1_s=$(awk "BEGIN{print $relaunch1_t1 - $relaunch1_t0}")

# Phase 2: a fresh client reads u,v,w, computes curl, writes it out.
export LOG_TAG=curl_posthoc_compute
./srun_client_curl_compute.sh

# Relaunch 2: same close+restart cycle before the magnitude phase, since
# that's also a separate later job reading data someone else persisted.
relaunch2_t0=$(date +%s.%N)
./srun_close_server.sh
./srun_server_restart.sh
relaunch2_t1=$(date +%s.%N)
relaunch2_s=$(awk "BEGIN{print $relaunch2_t1 - $relaunch2_t0}")

# Phase 3: a fresh client reads curl, computes magnitude, writes it out.
export LOG_TAG=curl_posthoc_analyze
./srun_client_curl_analyze.sh
./srun_close_server.sh

IFS=, read -r w_mode w_nranks w_nx w_ny w_nz w_setup w_write < "$WRITE_LOG"
IFS=, read -r c_mode c_nranks c_nx c_ny c_nz c_setup c_readback c_compute c_writeback c_total < "$COMPUTE_LOG"
IFS=, read -r a_mode a_nranks a_nx a_ny a_nz a_compress a_setup a_readback a_compute a_writeback a_total a_bad < "$ANALYZE_LOG"

if [ "$w_setup" = "FAILED" ] || [ "$c_setup" = "FAILED" ] || [ "$a_setup" = "FAILED" ]; then
  echo "curl_posthoc,${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},${COMPRESS},FAILED" >> "$RESULTS"
else
  total=$(awk "BEGIN{print $w_setup + $w_write + $relaunch1_s + $c_setup + $c_readback + $c_compute + $c_writeback + $relaunch2_s + $a_setup + $a_readback + $a_compute + $a_writeback}")
  echo "curl_posthoc,${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},${COMPRESS},${w_setup},${w_write},${relaunch1_s},${c_setup},${c_readback},${c_compute},${c_writeback},${relaunch2_s},${a_setup},${a_readback},${a_compute},${a_writeback},${total},${a_bad}" >> "$RESULTS"
fi

echo "Done. Results in $RESULTS"
