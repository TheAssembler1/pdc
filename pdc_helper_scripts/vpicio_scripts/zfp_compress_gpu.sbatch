#!/bin/bash
# VPIC-IO with PDC's TF framework's GPU ZFP compression transform
# (tf_client/graphs/zfp_gpu.json, "zfp_gpu" in vpicio.c's
# transformation_str) -- single node-count step, writing one CSV row per
# timestep to results_zfp_compress_gpu_<jobid>.csv.
#
# Writer (vpicio) and a genuinely separate verification process
# (vpicio_verify, run as its own srun step after the writer fully exits)
# both run; only the writer's timing is folded into the results CSV --
# see vpicio.c's and vpicio_verify.c's own header comments for why
# verification is deliberately kept out of the workload-time
# measurement.
#
# Submit one of these per node count via zfp_compress_gpu_run.sh, or
# directly, e.g.: sbatch --nodes=4 zfp_compress_gpu.sbatch
# Override sizing at submit time via ./common.sh's variables, e.g.:
#   NPARTICLES=16777216 sbatch --nodes=4 zfp_compress_gpu.sbatch

#SBATCH --job-name=vpicio-zfp-compress-gpu
#SBATCH --nodes=8
#SBATCH --ntasks-per-node=34
#SBATCH --cpus-per-task=3
#SBATCH --account=m2621
#SBATCH --time=00:30:00
#SBATCH --constraint=gpu
#SBATCH --gpus-per-node=4
#SBATCH --qos=regular
#SBATCH --output=%x_%j.log

set -u
cd "$SLURM_SUBMIT_DIR"

# SERVERS_PER_NODE, CLIENTS_PER_NODE, NPARTICLES, STEPS, SLEEPTIME: see
# ./common.sh -- this file's own #SBATCH --ntasks-per-node above must
# still equal SERVERS_PER_NODE + CLIENTS_PER_NODE (see that file's
# caveat).
source "$SLURM_SUBMIT_DIR/common.sh"
export TRANSFORM=zfp_gpu

export NUM_NODES=$SLURM_JOB_NUM_NODES
export SERVER_TOTAL_TASKS=$((NUM_NODES * SERVERS_PER_NODE))
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))

export BIN_DIR=${BIN_DIR:-$PWD/../../build/bin}
export PDC_DATA_LOC=${PDC_DATA_LOC:-$SCRATCH/pdc_vpicio_bench}
export PDC_TMPDIR=${PDC_TMPDIR:-$PDC_DATA_LOC}
mkdir -p "$PDC_DATA_LOC"

export LOG_TAG=zfp_compress_gpu
export RESULTS=$PWD/results_zfp_compress_gpu_${SLURM_JOB_ID}.csv

echo "mode,step,n_ranks,transform,setup_s,write_s,step_total_s" > "$RESULTS"

./srun_server_vpicio.sh
./srun_client_vpicio.sh
./srun_client_vpicio_verify.sh
./srun_close_server_vpicio.sh

grep "^vpicio," "client_${LOG_TAG}_${NUM_NODES}.log" >> "$RESULTS" || true

# See srun_close_server_vpicio.sh's own header comment -- same
# avg_close_s / total_with_close_s convention as analysis_scripts/.
CLOSE_LOG="close_server_${LOG_TAG}_${NUM_NODES}.log"
avg_close_s=$(grep 'total close time' "$CLOSE_LOG" 2>/dev/null | \
  sed -n 's/.*total close time = \([0-9.]*\).*/\1/p' | \
  awk '{sum+=$1; n++} END{if(n>0) printf "%.6f", sum/n; else print "0"}')
awk -F, -v OFS=, -v avgclose="$avg_close_s" '
  NR==1 { print $0, "avg_close_s", "total_with_close_s"; next }
  NF==7 { print $0, avgclose, ($7 + avgclose); next }
  { print }
' "$RESULTS" > "${RESULTS}.tmp" && mv "${RESULTS}.tmp" "$RESULTS"

echo "Done. Results in $RESULTS"
echo "Verification log (not part of workload timing): verify_${LOG_TAG}_${NUM_NODES}.log"
