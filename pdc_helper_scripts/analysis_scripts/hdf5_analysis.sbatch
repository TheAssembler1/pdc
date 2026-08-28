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
# Needs a parallel-HDF5 module loaded before hdf5_analysis_test/ is built,
# e.g. `module load cray-hdf5-parallel` on Perlmutter (see
# hdf5_analysis_test/README.md).

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

export NUM_NODES=$SLURM_JOB_NUM_NODES
export CLIENTS_PER_NODE=32
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export BIN_DIR=${BIN_DIR:-$PWD/../../hdf5_analysis_test}
export OUT_DIR=${OUT_DIR:-$SCRATCH/hdf5_analysis_bench}
mkdir -p "$OUT_DIR"

export N_ELEM=${N_ELEM:-268435456}   # 1 GiB/rank of float32
export LOG_TAG=hdf5
export RESULTS=$PWD/results_hdf5_${SLURM_JOB_ID}.csv

echo "mode,n_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,total_s,bad" > "$RESULTS"

./srun_hdf5.sh

echo "Done. Results in $RESULTS"
