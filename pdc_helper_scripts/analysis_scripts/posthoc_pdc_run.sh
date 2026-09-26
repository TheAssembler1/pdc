#!/bin/bash
# Submits one posthoc_pdc.sbatch job per node count (1, 2, 4, 8 --
# 32 ranks/node, so 32/64/128/256 ranks total), chained with
# --dependency=afterok so they run one at a time and each gets its own
# results_posthoc_<jobid>.csv. Mirrors vpicio_scripts/vpicio_scale_run.sh.

cd "$(dirname "$0")"

source ../common.sh
NTASKS_PER_NODE=$((SERVERS_PER_NODE + CLIENTS_PER_NODE))

prev_jid=""
for nodes in 1 2 4 8; do
    if [ -z "$prev_jid" ]; then
        jid=$(sbatch --nodes=$nodes --ntasks-per-node=$NTASKS_PER_NODE posthoc_pdc.sbatch | awk '{print $4}')
    else
        jid=$(sbatch --nodes=$nodes --ntasks-per-node=$NTASKS_PER_NODE --dependency=afterok:$prev_jid posthoc_pdc.sbatch | awk '{print $4}')
    fi
    echo "Submitted job $jid with $nodes nodes"
    prev_jid=$jid
done
