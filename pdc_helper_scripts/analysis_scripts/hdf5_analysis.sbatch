#!/bin/bash
# Parallel-HDF5 magnitude analysis baseline -- scale sweep. No PDC server
# involved; see hdf5_analysis_test/.
#
# Sweeps rank counts 1,2,4,8 inside a single job allocation sized for the
# largest step, writing one CSV row per step to results_hdf5_<jobid>.csv.
#
# Submit from this directory: sbatch hdf5_analysis.sbatch
# Override RANK_LIST / N_ELEM at submit time, e.g.:
#   RANK_LIST="1 2 4 8 16" sbatch hdf5_analysis.sbatch
#
# Needs a parallel-HDF5 module loaded before hdf5_analysis_test/ is built,
# e.g. `module load cray-hdf5-parallel` on Perlmutter (see
# hdf5_analysis_test/README.md).

#SBATCH --job-name=hdf5-magnitude
#SBATCH --nodes=8
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=128
#SBATCH --account=m2621
#SBATCH --time=00:30:00
#SBATCH --constraint=cpu
#SBATCH --qos=regular
#SBATCH --output=%x_%j.log

set -u
cd "$(dirname "$0")"

export BIN_DIR=${BIN_DIR:-$PWD/../../hdf5_analysis_test}
export OUT_DIR=${OUT_DIR:-$SCRATCH/hdf5_analysis_bench}
mkdir -p "$OUT_DIR"

N_ELEM=${N_ELEM:-268435456}   # 1 GiB/rank of float32
RANK_LIST=${RANK_LIST:-"1 2 4 8"}
LOG_TAG=hdf5
RESULTS=$PWD/results_hdf5_${SLURM_JOB_ID}.csv

echo "mode,n_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,total_s,bad" > "$RESULTS"

for RANKS in $RANK_LIST; do
  export RANKS N_ELEM LOG_TAG RESULTS OUT_DIR
  ./srun_hdf5.sh
done

echo "Done. Results in $RESULTS"
