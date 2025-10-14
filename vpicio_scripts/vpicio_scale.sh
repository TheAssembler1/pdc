#!/bin/bash

#SBATCH --nodes=256
#SBATCH --ntasks-per-node=128
#SBATCH --cpus-per-task=1
#SBATCH --account=m2621
#SBATCH --time=01:00:00
#SBATCH --constraint=cpu
#SBATCH --qos=regular

# Set environment variables
export NUM_NODES=$SLURM_JOB_NUM_NODES
export SERVER_TOTAL_TASKS=$((NUM_NODES * DATA_SERVERS_PER_NODE))
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))
export BIN_DIR=/pscratch/sd/n/nlewi26/src/work_space/source/pdc/build/bin

# Print them
echo "NUM_NODES=$NUM_NODES"
echo "DATA_SERVERS_PER_NODE=$DATA_SERVERS_PER_NODE"
echo "CLIENTS_PER_NODE=$CLIENTS_PER_NODE"
echo "SERVER_TOTAL_TASKS=$SERVER_TOTAL_TASKS"
echo "CLIENT_TOTAL_TASKS=$CLIENT_TOTAL_TASKS"

echo "running server script"
./srun_server.sh

echo "running client script"
./srun_client.sh

echo "closing server script"
./srun_close_server.sh
