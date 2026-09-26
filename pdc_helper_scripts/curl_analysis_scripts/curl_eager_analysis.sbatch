#!/bin/bash
# PDC eager (DataFlyway) curl + vorticity-magnitude analysis -- single
# node-count step. 2 data servers/node, 32 client ranks/node, writing one
# CSV row to results_curl_eager_<jobid>.csv.
#
# Submit one of these per node count via curl_eager_analysis_run.sh, or
# directly, e.g.: sbatch --nodes=4 curl_eager_analysis.sbatch
# Override NX/NY/NZ_PER_RANK/COMPRESS at submit time, e.g.:
#   COMPRESS=1 sbatch --nodes=4 curl_eager_analysis.sbatch

#SBATCH --job-name=pdc-curl-eager
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

# SERVERS_PER_NODE, CLIENTS_PER_NODE, NX/NY/NZ_PER_RANK/COMPRESS: see
# ../common.sh -- this file's own #SBATCH --ntasks-per-node above must
# still equal SERVERS_PER_NODE + CLIENTS_PER_NODE (see that file's caveat).
source "$SLURM_SUBMIT_DIR/../common.sh"

export NUM_NODES=$SLURM_JOB_NUM_NODES
export SERVER_TOTAL_TASKS=$((NUM_NODES * SERVERS_PER_NODE))
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export BIN_DIR=${BIN_DIR:-$PWD/../../build/bin}
export PDC_DATA_LOC=${PDC_DATA_LOC:-$SCRATCH/pdc_curl_analysis_bench}
export PDC_TMPDIR=${PDC_TMPDIR:-$PDC_DATA_LOC}
mkdir -p "$PDC_DATA_LOC"

export LOG_TAG=curl_eager
export RESULTS=$PWD/results_curl_eager_${SLURM_JOB_ID}.csv

echo "mode,step,n_ranks,nx,ny,nz_per_rank,compress,setup_s,write_s,confirm_read_s,total_s,bad" > "$RESULTS"

./srun_server.sh
./srun_client_curl_eager.sh
./srun_close_server.sh

# Sanity-check the data volume that actually landed on disk against what
# N_TIMESTEPS x NX x NY x NZ_PER_RANK x CLIENT_TOTAL_TASKS should produce:
# u/v/w (float32) plus curl_x/y/z and vorticity_magnitude (float64), all
# persisted once per timestep -- identical formula to
# plot_curl_totals.py's aggregate(). PDC object data lives under
# $PDC_DATA_LOC/pdc_data (see pdc_server_data.c's storage_location),
# which srun_server.sh wipes at the start of this job, so this only
# measures what this run itself wrote. `du -sb` (apparent size, not
# block-rounded) should match almost exactly for uncompressed data --
# this catches truncated writes, a stale/reused data directory, or a
# region silently not flushed, none of which `bad=0` alone would catch
# (bad only checks values read back through PDC, not what actually
# reached storage).
N_TIMESTEPS=3   # must track bench_curl_eager.c's #define N_TIMESTEPS
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

# close_server.c logs "total close time = X" per server rank -- that's
# the checkpoint that actually flushes PDC's region cache (up to 64GB by
# default; see pdc_server_region_cache.c) to the storage backend, a real
# durability cost with no HDF5 equivalent. Average it across server
# ranks and append avg_close_s/total_with_close_s so total_s alone
# doesn't make eager look artificially cheap for data that fit in cache
# and was never actually flushed during write_s.
CLOSE_LOG="$BIN_DIR/close_server_${LOG_TAG}_${NUM_NODES}.log"
avg_close_s=$(grep 'total close time' "$CLOSE_LOG" 2>/dev/null | \
  sed -n 's/.*total close time = \([0-9.]*\).*/\1/p' | \
  awk '{sum+=$1; n++} END{if(n>0) printf "%.6f", sum/n; else print "0"}')
awk -F, -v OFS=, -v avgclose="$avg_close_s" '
  NR==1 { print $0, "avg_close_s", "total_with_close_s"; next }
  NF==12 { print $0, avgclose, ($11 + avgclose); next }
  { print }
' "$RESULTS" > "${RESULTS}.tmp" && mv "${RESULTS}.tmp" "$RESULTS"

echo "Done. Results in $RESULTS"
