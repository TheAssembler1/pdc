#!/bin/bash
# ADIOS2 analog of the PDC vpicio+bdcats pairing -- single node-count
# step, 32 client ranks/node, no server (ADIOS2 has none). Runs
# adios2_vpicio (write) then adios2_bdcats (read back what was just
# written) as two separate srun steps within this one job, writing one
# CSV row per timestep for each to its own results file:
#   results_adios2_vpicio_<jobid>.csv
#   results_adios2_bdcats_<jobid>.csv
#
# Neither program has a correctness check -- see adios2_vpicio.cpp's and
# adios2_bdcats.cpp's own header comments: this mirrors vpicio.c /
# bdcats.c exactly, and neither of those has one either.
#
# Submit one of these per node count via vpicio_bdcats_run.sh, or
# directly, e.g.: sbatch --nodes=4 vpicio_bdcats.sbatch
# Override sizing at submit time via ./common.sh's variables, e.g.:
#   NPARTICLES=16777216 sbatch --nodes=4 vpicio_bdcats.sbatch

#SBATCH --job-name=adios2-vpicio-bdcats
#SBATCH --nodes=8
#SBATCH --ntasks-per-node=32
#SBATCH --cpus-per-task=4
#SBATCH --account=m2621
#SBATCH --time=00:30:00
#SBATCH --constraint=cpu
#SBATCH --qos=regular
#SBATCH --output=%x_%j.log

set -xeu
cd "$SLURM_SUBMIT_DIR"

# CLIENTS_PER_NODE, NPARTICLES, STEPS, SLEEPTIME: see ./common.sh --
# this file's own #SBATCH --ntasks-per-node above must still equal
# CLIENTS_PER_NODE (see that file's caveat).
source "$SLURM_SUBMIT_DIR/common.sh"

export NUM_NODES=$SLURM_JOB_NUM_NODES
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export BIN_DIR=${BIN_DIR:-$PWD}
export OUT_DIR=${OUT_DIR:-$SCRATCH/adios2_vpicio_bench}
mkdir -p "$OUT_DIR"
export OUT_FILE=${OUT_DIR}/adios2_vpicio_${NUM_NODES}.bp

# Lustre striping, same convention as the other adios2_analysis_test/
# sbatch scripts: stripe size == one rank's per-write payload (7 float
# fields, the largest single-field write), striped across all 128 OSTs.
STRIPE_COUNT=128
STRIPE_SIZE_BYTES=$((NPARTICLES * 4))
STRIPE_SIZE_BYTES=$(( ((STRIPE_SIZE_BYTES + 65535) / 65536) * 65536 ))
rm -rf "$OUT_FILE"
lfs setstripe -c "$STRIPE_COUNT" -S "$STRIPE_SIZE_BYTES" "$OUT_DIR" \
  || echo "WARNING: lfs setstripe failed, falling back to filesystem default layout"

VPICIO_RESULTS=$PWD/results_adios2_vpicio_${SLURM_JOB_ID}.csv
BDCATS_RESULTS=$PWD/results_adios2_bdcats_${SLURM_JOB_ID}.csv

echo "mode,step,n_ranks,setup_s,write_s,close_s,step_total_s" > "$VPICIO_RESULTS"
echo "mode,step,n_ranks,open_s,read_s,step_total_s" > "$BDCATS_RESULTS"

srun \
  --overlap \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="vpicio_${NUM_NODES}.log" \
  --error="vpicio_${NUM_NODES}.err" \
  "$BIN_DIR/adios2_vpicio" "$NPARTICLES" "$STEPS" "$SLEEPTIME" "$OUT_FILE"

grep "^adios2_vpicio," "vpicio_${NUM_NODES}.log" >> "$VPICIO_RESULTS" || true

srun \
  --overlap \
  -N "$NUM_NODES" \
  -n "$CLIENT_TOTAL_TASKS" \
  --ntasks-per-node="$CLIENTS_PER_NODE" \
  --output="bdcats_${NUM_NODES}.log" \
  --error="bdcats_${NUM_NODES}.err" \
  "$BIN_DIR/adios2_bdcats" "$NPARTICLES" "$STEPS" "$OUT_FILE"

grep "^adios2_bdcats," "bdcats_${NUM_NODES}.log" >> "$BDCATS_RESULTS" || true

# Prepend servers_per_node as the first column (0 -- no PDC server involved).
sed -i "1s/^/servers_per_node,/; 2,\$s/^/0,/" "$VPICIO_RESULTS"
sed -i "1s/^/servers_per_node,/; 2,\$s/^/0,/" "$BDCATS_RESULTS"

echo "Done. Results in $VPICIO_RESULTS and $BDCATS_RESULTS"
