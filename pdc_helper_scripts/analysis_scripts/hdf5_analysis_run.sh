#!/bin/bash
# Submits one hdf5_analysis.sbatch job per node count (1, 2, 4, 8 --
# 32 ranks/node, so 32/64/128/256 ranks total), chained with
# --dependency=afterok so they run one at a time and each gets its own
# results_hdf5_<jobid>.csv. Mirrors vpicio_scripts/vpicio_scale_run.sh.

cd "$(dirname "$0")"

prev_jid=""
for nodes in 1 2 4 8; do
    if [ -z "$prev_jid" ]; then
        jid=$(sbatch --nodes=$nodes hdf5_analysis.sbatch | awk '{print $4}')
    else
        jid=$(sbatch --nodes=$nodes --dependency=afterok:$prev_jid hdf5_analysis.sbatch | awk '{print $4}')
    fi
    echo "Submitted job $jid with $nodes nodes"
    prev_jid=$jid
done
