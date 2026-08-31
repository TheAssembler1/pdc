#!/bin/bash
# PDC post-hoc (read-back, recompute, write-back) magnitude analysis --
# single node-count step. 8 data servers/node, 32 client ranks/node (see
# srun_server.sh / srun_client_posthoc.sh), writing one CSV row to
# results_posthoc_<jobid>.csv.
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

echo "mode,n_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,confirm_read_s,total_s,bad" > "$RESULTS"

./srun_server.sh
./srun_client_posthoc.sh
./srun_close_server.sh

echo "Done. Results in $RESULTS"
