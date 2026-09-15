#!/bin/bash
# PDC DataFlyway (eager, in-flight) magnitude analysis -- single node-count
# step. 8 data servers/node, 32 client ranks/node (see srun_server.sh /
# srun_client_eager.sh), writing one CSV row to
# results_eager_<jobid>.csv.
#
# Submit one of these per node count via eager_pdc_run.sh, or
# directly, e.g.: sbatch --nodes=4 eager_pdc.sbatch
# Override N_ELEM at submit time, e.g.:
#   N_ELEM=33554432 sbatch --nodes=4 eager_pdc.sbatch

#SBATCH --job-name=pdc-eager-magnitude
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
                                     # per timestep (192 MiB input + 128 MiB
                                     # magnitude out), and with N_TIMESTEPS=3
                                     # in bench_magnitude.c that's 960
                                     # MiB/rank, ~3.75 TiB (~4.12 TB)
                                     # aggregate against a 4 TB budget --
                                     # just over on paper but within
                                     # rounding of typical scratch quotas;
                                     # reduce N_ELEM if that budget is a
                                     # hard constraint.
export LOG_TAG=eager
export RESULTS=$PWD/results_eager_${SLURM_JOB_ID}.csv

echo "mode,step,n_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,confirm_read_s,step_total_s,bad" > "$RESULTS"

./srun_server.sh
./srun_client_eager.sh
./srun_close_server.sh

# srun_close_server.sh's log holds one "total close time = X" line per
# server rank (printed by close_server.c, the PDC_Client_close_all_server
# RPC that checkpoints every server's in-memory metadata to disk before
# exiting). That's a real, PDC-specific durability cost with no HDF5
# equivalent -- H5Fclose in hdf5_bench_write.c isn't even separately
# timed, since closing a file backed by already-durable collective writes
# is comparatively free. Without folding this in, step_total_s makes
# eager/lazy look cheaper than they really are next to the HDF5 baseline,
# so average it across server ranks and append it as its own column plus
# a close-inclusive total, rather than leaving it buried in a log file
# nothing else reads.
CLOSE_LOG="$BIN_DIR/close_server_${LOG_TAG}_${NUM_NODES}.log"
avg_close_s=$(grep 'total close time' "$CLOSE_LOG" 2>/dev/null | \
  sed -n 's/.*total close time = \([0-9.]*\).*/\1/p' | \
  awk '{sum+=$1; n++} END{if(n>0) printf "%.6f", sum/n; else print "0"}')
awk -F, -v OFS=, -v avgclose="$avg_close_s" '
  NR==1 { print $0, "avg_close_s", "total_with_close_s"; next }
  NF==12 { print $0, avgclose, ($11 + avgclose); next }
  { print }
' "$RESULTS" > "${RESULTS}.tmp" && mv "${RESULTS}.tmp" "$RESULTS"

echo "Done. Results in $RESULTS"
