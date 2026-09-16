#!/bin/bash
# PDC eager (DataFlyway) curl + vorticity-magnitude analysis, GPU-ZFP-
# COMPRESSED vorticity_magnitude variant -- single node-count step.
# Mechanically identical to curl_eager_analysis.sbatch (one continuous
# client session; writing u,v,w with the graph attached transparently
# triggers server-side curl + vector_magnitude in the write path), except
# vorticity_magnitude is additionally composed with the existing GPU ZFP
# compression transform before that write -- see
# src/tests/analysis/bench_curl_eager.c and curl_analysis_scripts/
# README.md's "Compression" section.
#
# Kept as its own dedicated test type (own sbatch, own
# results_curl_eager_compress_<jobid>.csv, own "curl_eager_compress" mode
# label) instead of only an env-var toggle on plain eager, so it shows up
# in plot_curl_totals.py as its own directly comparable workload rather
# than being folded into curl_eager's numbers via the CSV's own compress
# column.
#
# Submit one of these per node count via
# curl_eager_compress_analysis_run.sh, or directly, e.g.:
#   sbatch --nodes=4 curl_eager_compress_analysis.sbatch
# Override NX/NY/NZ_PER_RANK at submit time, e.g.:
#   NZ_PER_RANK=128 sbatch --nodes=4 curl_eager_compress_analysis.sbatch

#SBATCH --job-name=pdc-curl-eager-compress
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
# reported data volumes.
export NX=${NX:-256}
export NY=${NY:-256}
export NZ_PER_RANK=${NZ_PER_RANK:-64}
export COMPRESS=1

export LOG_TAG=curl_eager_compress
export RESULTS=$PWD/results_curl_eager_compress_${SLURM_JOB_ID}.csv

echo "mode,n_ranks,nx,ny,nz_per_rank,compress,setup_s,write_s,confirm_read_s,total_s,bad" > "$RESULTS"

./srun_server.sh
./srun_client_curl_eager.sh
./srun_close_server.sh

# srun_client_curl_eager.sh appends bench_curl_eager's own "curl_eager,"
# line verbatim (same binary as plain eager, distinguished only by its
# compress column) -- relabel the mode field here so this test type
# stays distinct even in a results directory that also has plain
# (uncompressed) curl_eager runs, instead of relying on every downstream
# consumer to remember to check the compress column.
sed -i 's/^curl_eager,/curl_eager_compress,/' "$RESULTS"

echo "Done. Results in $RESULTS"
