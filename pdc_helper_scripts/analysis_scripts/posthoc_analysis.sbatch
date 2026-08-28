#!/bin/bash
# PDC post-hoc (read-back, recompute, write-back) magnitude analysis --
# scale sweep.
#
# Sweeps client/server rank counts 1,2,4,8 (server count == client count;
# see srun_server.sh) inside a single job allocation sized for the largest
# step, writing one CSV row per step to results_posthoc_<jobid>.csv.
#
# Submit from this directory: sbatch posthoc_analysis.sbatch
# Override RANK_LIST / N_ELEM at submit time, e.g.:
#   RANK_LIST="1 2 4 8 16" sbatch posthoc_analysis.sbatch

#SBATCH --job-name=pdc-posthoc-magnitude
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

export BIN_DIR=${BIN_DIR:-$PWD/../../build/bin}
export PDC_DATA_LOC=${PDC_DATA_LOC:-$SCRATCH/pdc_analysis_bench}
export PDC_TMPDIR=${PDC_TMPDIR:-$PDC_DATA_LOC}
mkdir -p "$PDC_DATA_LOC"

N_ELEM=${N_ELEM:-268435456}   # 1 GiB/rank of float32
RANK_LIST=${RANK_LIST:-"1 2 4 8"}
LOG_TAG=posthoc
RESULTS=$PWD/results_posthoc_${SLURM_JOB_ID}.csv

echo "mode,n_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,confirm_read_s,total_s,bad" > "$RESULTS"

for RANKS in $RANK_LIST; do
  export RANKS N_ELEM LOG_TAG RESULTS
  ./srun_server.sh
  ./srun_client_posthoc.sh
  ./srun_close_server.sh
done

echo "Done. Results in $RESULTS"
