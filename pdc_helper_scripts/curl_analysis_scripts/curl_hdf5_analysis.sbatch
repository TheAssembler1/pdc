#!/bin/bash
# Parallel-HDF5 curl + vorticity-magnitude analysis baseline -- single
# node-count step. No PDC server involved; see hdf5_analysis_test/. Two
# genuinely separate srun steps (write u,v,w; then read them back and
# compute curl then magnitude in one pass), mirroring the PDC posthoc
# benchmark's phase structure -- see hdf5_bench_curl_write.c / _analyze.c.
# Writes one CSV row to results_curl_hdf5_<jobid>.csv.
#
# Submit one of these per node count via curl_hdf5_analysis_run.sh, or
# directly, e.g.: sbatch --nodes=4 curl_hdf5_analysis.sbatch
# Override NX/NY/NZ_PER_RANK at submit time, e.g.:
#   NZ_PER_RANK=128 sbatch --nodes=4 curl_hdf5_analysis.sbatch
#
# Needs the same parallel-HDF5 module that built hdf5_analysis_test/ (see
# hdf5_analysis_test/README.md) loaded here too -- the compute-node
# environment doesn't inherit whatever was loaded in the login shell at
# build time, so the module load below is required even though the
# binaries were already built.

#SBATCH --job-name=hdf5-curl-posthoc
#SBATCH --nodes=8
#SBATCH --ntasks-per-node=32
#SBATCH --cpus-per-task=4
#SBATCH --account=m2621
#SBATCH --time=00:30:00
#SBATCH --constraint=cpu
#SBATCH --qos=regular
#SBATCH --output=%x_%j.log

set -u
cd "$SLURM_SUBMIT_DIR"

module load cray-hdf5-parallel

export NUM_NODES=$SLURM_JOB_NUM_NODES
export CLIENTS_PER_NODE=32
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export BIN_DIR=${BIN_DIR:-$PWD/../../hdf5_analysis_test}
export OUT_DIR=${OUT_DIR:-$SCRATCH/hdf5_curl_analysis_bench}
mkdir -p "$OUT_DIR"

# See README.md for how these were derived from the source paper's
# reported data volumes (u,v,w ~4GB total at a 96-rank reference point).
export NX=${NX:-256}
export NY=${NY:-256}
export NZ_PER_RANK=${NZ_PER_RANK:-64}

export LOG_TAG=curl_hdf5
export RESULTS=$PWD/results_curl_hdf5_${SLURM_JOB_ID}.csv
export OUT_FILE=${OUT_DIR}/hdf5_bench_curl_${NUM_NODES}.h5
export WRITE_LOG=$PWD/write_${LOG_TAG}_${SLURM_JOB_ID}.csv
export ANALYZE_LOG=$PWD/analyze_${LOG_TAG}_${SLURM_JOB_ID}.csv

# Lustre stripe size == exactly what one rank writes per collective write
# (NX*NY*NZ_PER_RANK float32 elements), rounded up to the required 64 KiB
# stripe granularity, striped across all 128 OSTs -- see
# analysis_scripts/hdf5_analysis.sbatch's comment for the full reasoning.
STRIPE_COUNT=128
STRIPE_SIZE_BYTES=$((NX * NY * NZ_PER_RANK * 4))
STRIPE_SIZE_BYTES=$(( ((STRIPE_SIZE_BYTES + 65535) / 65536) * 65536 ))
echo "Lustre striping: count=${STRIPE_COUNT} size=${STRIPE_SIZE_BYTES} bytes ($((STRIPE_SIZE_BYTES / 1048576)) MiB/rank), target=${OUT_FILE}"

rm -f "$OUT_FILE"
lfs setstripe -c "$STRIPE_COUNT" -S "$STRIPE_SIZE_BYTES" "$OUT_FILE" || echo "WARNING: lfs setstripe failed, falling back to filesystem default layout"

echo "mode,step,n_ranks,nx,ny,nz_per_rank,write_setup_s,write_s,analyze_setup_s,readback_s,curl_compute_s,curl_writeback_s,magnitude_compute_s,magnitude_writeback_s,total_s,bad" > "$RESULTS"

./srun_hdf5_curl_write.sh
./srun_hdf5_curl_analyze.sh

# Both WRITE_LOG and ANALYZE_LOG now hold one line per timestep
# (N_TIMESTEPS=3 in hdf5_bench_curl_write.c / hdf5_bench_curl_analyze.c),
# in matching step order, so pair them up line-by-line via two file
# descriptors and emit one combined "curl_hdf5" row per timestep.
exec 3< "$WRITE_LOG"
exec 4< "$ANALYZE_LOG"
while IFS=, read -r w_mode w_step w_nranks w_nx w_ny w_nz w_setup w_write <&3 && \
      IFS=, read -r a_mode a_step a_nranks a_nx a_ny a_nz a_setup a_readback a_curl_compute a_curl_writeback a_mag_compute a_mag_writeback a_total a_bad <&4; do
  if [ "$w_setup" = "FAILED" ] || [ "$a_setup" = "FAILED" ]; then
    echo "curl_hdf5,${w_step},${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},FAILED" >> "$RESULTS"
  else
    total=$(awk "BEGIN{print $w_setup + $w_write + $a_setup + $a_readback + $a_curl_compute + $a_curl_writeback + $a_mag_compute + $a_mag_writeback}")
    echo "curl_hdf5,${w_step},${CLIENT_TOTAL_TASKS},${NX},${NY},${NZ_PER_RANK},${w_setup},${w_write},${a_setup},${a_readback},${a_curl_compute},${a_curl_writeback},${a_mag_compute},${a_mag_writeback},${total},${a_bad}" >> "$RESULTS"
  fi
done
exec 3<&-
exec 4<&-

echo "Done. Results in $RESULTS"
