#!/bin/bash
# PDC post-hoc curl + vorticity-magnitude analysis -- single node-count
# step. Two genuinely separate client processes (write u,v,w; then read
# them back and compute curl then magnitude in one pass), with the
# server closed (checkpointing) and restarted (reloading that
# checkpoint) between them -- see srun_client_curl_write.sh /
# _analyze.sh and srun_server_restart.sh. Writes one CSV row to
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
export ANALYZE_LOG=$PWD/analyze_curl_posthoc_${SLURM_JOB_ID}.csv

echo "mode,step,n_ranks,nx,ny,nz_per_rank,compress,write_setup_s,write_s,relaunch_s,analyze_setup_s,readback_s,curl_compute_s,curl_writeback_s,magnitude_compute_s,magnitude_writeback_s,total_s,bad" > "$RESULTS"

# Phase 1: write u,v,w, then let every client rank fully exit.
export LOG_TAG=curl_posthoc_write
./srun_server.sh
./srun_client_curl_write.sh

# Relaunch: close the server (checkpoints its metadata), then restart it
# with the `restart` argument so it reloads that checkpoint -- the real
# posthoc cost a single continuous session never pays.
relaunch_t0=$(date +%s.%N)
./srun_close_server.sh
./srun_server_restart.sh
relaunch_t1=$(date +%s.%N)
relaunch_s=$(awk "BEGIN{print $relaunch_t1 - $relaunch_t0}")

# Phase 2: a fresh client reads u,v,w back and computes curl then
# magnitude in this same process (see bench_curl_analyze.c for why this
# isn't split into its own separately relaunched "compute curl" job).
export LOG_TAG=curl_posthoc_analyze
./srun_client_curl_analyze.sh
./srun_close_server.sh

# Sanity-check the data volume that actually landed on disk against what
# N_TIMESTEPS x NX x NY x NZ_PER_RANK x CLIENT_TOTAL_TASKS should
# produce: u/v/w (float32, from the write phase) plus curl_x/y/z and
# vorticity_magnitude (float64, from the analyze phase), all persisted
# once per timestep -- same formula as plot_curl_totals.py's
# aggregate(). PDC object data lives under $PDC_DATA_LOC/pdc_data (see
# pdc_server_data.c's storage_location); srun_server.sh wiped it at the
# start of phase 1, and phase 2's restart doesn't touch it, so this
# reflects both phases' combined output for this job alone. This catches
# truncated writes or an unflushed region that `bad=0` alone wouldn't
# (bad only checks values read back through PDC, not what actually
# reached storage).
N_TIMESTEPS=3   # must track bench_curl_write.c / bench_curl_analyze.c's #define N_TIMESTEPS
BYTES_PER_ELEM=$((4 * 3 + 8 * 4))   # u,v,w float32 + curl_x/y/z,vorticity_magnitude float64
EXPECTED_BYTES=$((CLIENT_TOTAL_TASKS * NX * NY * NZ_PER_RANK * BYTES_PER_ELEM * N_TIMESTEPS))
ACTUAL_BYTES=$(du -sb "$PDC_DATA_LOC/pdc_data" 2>/dev/null | awk '{print $1}')
ACTUAL_BYTES=${ACTUAL_BYTES:-0}
if [ "$COMPRESS" = "1" ]; then
  echo "Data size check (COMPRESS=1, vorticity_magnitude compressed -- actual < expected uncompressed is correct): expected=${EXPECTED_BYTES} bytes, actual=${ACTUAL_BYTES} bytes"
else
  echo "Data size check: expected=${EXPECTED_BYTES} bytes, actual=${ACTUAL_BYTES} bytes"
  awk -v e="$EXPECTED_BYTES" -v a="$ACTUAL_BYTES" 'BEGIN {
    if (e == 0) { print "WARNING: expected byte count is 0, skipping ratio check"; exit }
    ratio = a / e
    if (ratio < 0.95 || ratio > 1.05)
      printf "WARNING: actual/expected data size ratio %.3f is outside [0.95, 1.05] -- possible incomplete write, stale data directory, or an unflushed region\n", ratio
    else
      printf "OK: actual/expected data size ratio %.3f within tolerance\n", ratio
  }'
fi

# Both WRITE_LOG and ANALYZE_LOG now hold one line per timestep
# (N_TIMESTEPS=3 in bench_curl_write.c / bench_curl_analyze.c), in
# matching step order, so pair them up line-by-line via two file
# descriptors and emit one combined "curl_posthoc" row per timestep --
# same pattern as analysis_scripts/posthoc_pdc.sbatch.
exec 3< "$WRITE_LOG"
exec 4< "$ANALYZE_LOG"
while IFS=, read -r w_mode w_step w_nranks w_nx w_ny w_nz w_setup w_write <&3 && \
      IFS=, read -r a_mode a_step a_nranks a_nx a_ny a_nz a_compress a_setup a_readback a_curl_compute a_curl_writeback a_mag_compute a_mag_writeback a_total a_bad <&4; do
  if [ "$w_setup" = "FAILED" ] || [ "$a_setup" = "FAILED" ]; then
    echo "curl_posthoc,${w_step},${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},${COMPRESS},FAILED" >> "$RESULTS"
  else
    total=$(awk "BEGIN{print $w_setup + $w_write + $relaunch_s + $a_setup + $a_readback + $a_curl_compute + $a_curl_writeback + $a_mag_compute + $a_mag_writeback}")
    echo "curl_posthoc,${w_step},${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},${COMPRESS},${w_setup},${w_write},${relaunch_s},${a_setup},${a_readback},${a_curl_compute},${a_curl_writeback},${a_mag_compute},${a_mag_writeback},${total},${a_bad}" >> "$RESULTS"
  fi
done
exec 3<&-
exec 4<&-

# The relaunch above already folds in one close_server call (checkpoint
# between write and analyze), but the final ./srun_close_server.sh right
# before the pairing loop checkpoints again and isn't captured in
# total_s at all -- that close_server.c call is what actually flushes
# PDC's region cache to the storage backend (see
# pdc_server_region_cache.c), a real durability cost with no HDF5
# equivalent. Average its per-server-rank "total close time" lines
# (LOG_TAG is still curl_posthoc_analyze here, matching that last
# close_server.sh call's log) and append avg_close_s/total_with_close_s
# so results_curl_posthoc_*.csv lines up with results_curl_hdf5_*.csv
# (which has no server to close, so its own close column is always 0).
CLOSE_LOG="$BIN_DIR/close_server_${LOG_TAG}_${NUM_NODES}.log"
avg_close_s=$(grep 'total close time' "$CLOSE_LOG" 2>/dev/null | \
  sed -n 's/.*total close time = \([0-9.]*\).*/\1/p' | \
  awk '{sum+=$1; n++} END{if(n>0) printf "%.6f", sum/n; else print "0"}')
awk -F, -v OFS=, -v avgclose="$avg_close_s" '
  NR==1 { print $0, "avg_close_s", "total_with_close_s"; next }
  NF==18 { print $0, avgclose, ($17 + avgclose); next }
  { print }
' "$RESULTS" > "${RESULTS}.tmp" && mv "${RESULTS}.tmp" "$RESULTS"

echo "Done. Results in $RESULTS"
