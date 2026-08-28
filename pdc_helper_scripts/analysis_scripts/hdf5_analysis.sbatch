#!/bin/bash
# Parallel-HDF5 magnitude analysis baseline -- single node-count step. No
# PDC server involved; see hdf5_analysis_test/. 32 client ranks/node,
# writing one CSV row to results_hdf5_<jobid>.csv.
#
# Submit one of these per node count via hdf5_analysis_run.sh, or
# directly, e.g.: sbatch --nodes=4 hdf5_analysis.sbatch
# Override N_ELEM at submit time, e.g.:
#   N_ELEM=536870912 sbatch --nodes=4 hdf5_analysis.sbatch
#
# Needs the same parallel-HDF5 module that built hdf5_analysis_test/ (see
# hdf5_analysis_test/README.md) loaded here too -- the compute-node
# environment doesn't inherit whatever was loaded in the login shell at
# build time, so the module load below is required even though the
# binary was already built.

#SBATCH --job-name=hdf5-magnitude
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
export OUT_DIR=${OUT_DIR:-$SCRATCH/hdf5_analysis_bench}
mkdir -p "$OUT_DIR"

export N_ELEM=${N_ELEM:-268435456}   # 1 GiB/rank of float32
export LOG_TAG=hdf5
export RESULTS=$PWD/results_hdf5_${SLURM_JOB_ID}.csv
export OUT_FILE=${OUT_DIR}/hdf5_bench_magnitude_${NUM_NODES}.h5

# Lustre stripe size == exactly what one rank writes per collective write
# (N_ELEM float32 elements), rounded up to the required 64 KiB stripe
# granularity, striped across all 128 OSTs on this filesystem. Without
# this, new files default to a single OST, so the one shared HDF5 file's
# collective I/O funnels through one disk target regardless of rank
# count. PDC's servers similarly stripe their own data directory onto
# dedicated OSTs (see src/server/pdc_server_region/pdc_server_data.c),
# so this keeps the HDF5 baseline comparable instead of artificially
# bottlenecked.
STRIPE_COUNT=128
STRIPE_SIZE_BYTES=$((N_ELEM * 4))
STRIPE_SIZE_BYTES=$(( ((STRIPE_SIZE_BYTES + 65535) / 65536) * 65536 ))
echo "Lustre striping: count=${STRIPE_COUNT} size=${STRIPE_SIZE_BYTES} bytes ($((STRIPE_SIZE_BYTES / 1048576)) MiB/rank), target=${OUT_FILE}"

# Lustre striping can only be set at file-creation time, so any stale
# file at this path has to be removed first -- lfs setstripe then
# creates the target itself as an empty file with the requested layout,
# and H5Fcreate's later H5F_ACC_TRUNC preserves that layout.
rm -f "$OUT_FILE"
lfs setstripe -c "$STRIPE_COUNT" -S "$STRIPE_SIZE_BYTES" "$OUT_FILE" || echo "WARNING: lfs setstripe failed, falling back to filesystem default layout"

echo "mode,n_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,total_s,bad" > "$RESULTS"

./srun_hdf5.sh

echo "Done. Results in $RESULTS"
