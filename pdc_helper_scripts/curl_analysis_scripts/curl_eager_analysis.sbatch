#!/bin/bash
# PDC eager (DataFlyway) curl + vorticity-magnitude analysis -- single
# node-count step. 8 data servers/node, 32 client ranks/node, writing one
# CSV row to results_curl_eager_<jobid>.csv.
#
# Submit one of these per node count via curl_eager_analysis_run.sh, or
# directly, e.g.: sbatch --nodes=4 curl_eager_analysis.sbatch
# Override NX/NY/NZ_PER_RANK/COMPRESS at submit time, e.g.:
#   COMPRESS=1 sbatch --nodes=4 curl_eager_analysis.sbatch

#SBATCH --job-name=pdc-curl-eager
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

# See README.md for how these were derived from the source paper's
# reported data volumes (u,v,w ~4GB total at a 96-rank reference point,
# so that curl comes out to ~4GB and vorticity_magnitude to ~2.7GB,
# matching the paper's reported ~4GB/~3GB).
export NX=${NX:-256}
export NY=${NY:-256}
export NZ_PER_RANK=${NZ_PER_RANK:-64}
export COMPRESS=${COMPRESS:-0}

export LOG_TAG=curl_eager
export RESULTS=$PWD/results_curl_eager_${SLURM_JOB_ID}.csv

echo "mode,n_ranks,nx,ny,nz_per_rank,compress,setup_s,write_s,confirm_read_s,total_s,bad" > "$RESULTS"

./srun_server.sh
./srun_client_curl_eager.sh
./srun_close_server.sh

echo "Done. Results in $RESULTS"
