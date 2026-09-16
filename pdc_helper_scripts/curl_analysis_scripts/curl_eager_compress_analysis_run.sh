#!/bin/bash
# Submits one curl_eager_compress_analysis.sbatch job per node count (1,
# 2, 4, 8), chained with --dependency=afterok so they run one at a time
# and each gets its own results_curl_eager_compress_<jobid>.csv. Mirrors
# curl_eager_analysis_run.sh; COMPRESS is fixed at 1 inside the sbatch
# itself (not an environment toggle here) since this run script's whole
# purpose is producing the GPU-compressed variant as its own comparable
# dataset -- use curl_eager_analysis_run.sh for the uncompressed sweep.

cd "$(dirname "$0")"

prev_jid=""
for nodes in 1 2 4 8; do
    if [ -z "$prev_jid" ]; then
        jid=$(sbatch --nodes=$nodes curl_eager_compress_analysis.sbatch | awk '{print $4}')
    else
        jid=$(sbatch --nodes=$nodes --dependency=afterok:$prev_jid curl_eager_compress_analysis.sbatch | awk '{print $4}')
    fi
    echo "Submitted job $jid with $nodes nodes"
    prev_jid=$jid
done
