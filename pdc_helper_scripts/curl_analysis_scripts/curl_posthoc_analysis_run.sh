#!/bin/bash
# Submits one curl_posthoc_analysis.sbatch job per node count (1, 2, 4, 8
# -- 32 ranks/node, so 32/64/128/256 ranks total), chained with
# --dependency=afterok so they run one at a time and each gets its own
# results_curl_posthoc_<jobid>.csv. Mirrors analysis_scripts/*_run.sh.
# Set COMPRESS=1 in the environment before running to sweep the
# GPU-compressed variant instead.

cd "$(dirname "$0")"

source ../common.sh
NTASKS_PER_NODE=$((SERVERS_PER_NODE + CLIENTS_PER_NODE))

prev_jid=""
for nodes in 1 2 4 8; do
    if [ -z "$prev_jid" ]; then
        jid=$(sbatch --nodes=$nodes --ntasks-per-node=$NTASKS_PER_NODE curl_posthoc_analysis.sbatch | awk '{print $4}')
    else
        jid=$(sbatch --nodes=$nodes --ntasks-per-node=$NTASKS_PER_NODE --dependency=afterok:$prev_jid curl_posthoc_analysis.sbatch | awk '{print $4}')
    fi
    echo "Submitted job $jid with $nodes nodes"
    prev_jid=$jid
done
