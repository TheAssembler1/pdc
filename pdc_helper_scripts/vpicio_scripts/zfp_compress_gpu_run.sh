#!/bin/bash
# Submits one zfp_compress_gpu.sbatch job per node count (1, 2, 4, 8, 16,
# 32, 64, 128), chained with --dependency=afterok so they run one at a
# time and each gets its own results_zfp_compress_gpu_<jobid>.csv.
# Mirrors analysis_scripts/*_run.sh.

cd "$(dirname "$0")"

source ./common.sh
NTASKS_PER_NODE=$((SERVERS_PER_NODE + CLIENTS_PER_NODE))

prev_jid=""
for nodes in 1 2 4 8 16 32 64 128; do
    if [ -z "$prev_jid" ]; then
        jid=$(sbatch --nodes=$nodes --ntasks-per-node=$NTASKS_PER_NODE zfp_compress_gpu.sbatch | awk '{print $4}')
    else
        jid=$(sbatch --nodes=$nodes --ntasks-per-node=$NTASKS_PER_NODE --dependency=afterok:$prev_jid zfp_compress_gpu.sbatch | awk '{print $4}')
    fi
    echo "Submitted job $jid with $nodes nodes"
    prev_jid=$jid
done
