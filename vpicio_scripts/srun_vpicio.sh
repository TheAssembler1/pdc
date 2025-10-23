#!/bin/bash

# This script is useful for running vpicio within interactive jobs

set -xue

# Set environment variables
export NUM_NODES=$SLURM_JOB_NUM_NODES
export DATA_SERVERS_PER_NODE=1
export CLIENTS_PER_NODE=1
export SERVER_TOTAL_TASKS=$((NUM_NODES * DATA_SERVERS_PER_NODE))
export CLIENT_TOTAL_TASKS=$((NUM_NODES * CLIENTS_PER_NODE))
export BIN_DIR=/pscratch/sd/n/nlewi26/src/work_space/source/pdc/build/bin
export TRANSFORM="raw"
export PDC_DATA_LOC=/pscratch/sd/n/nlewi26/src/work_space/install/pdc
export PDC_TMPDIR=/pscratch/sd/n/nlewi26/src/work_space/install/pdc

# Print them
echo "NUM_NODES=$NUM_NODES"
echo "DATA_SERVERS_PER_NODE=$DATA_SERVERS_PER_NODE"
echo "CLIENTS_PER_NODE=$CLIENTS_PER_NODE"
echo "SERVER_TOTAL_TASKS=$SERVER_TOTAL_TASKS"
echo "CLIENT_TOTAL_TASKS=$CLIENT_TOTAL_TASKS"
echo "BIN_DIR=$BIN_DIR"
echo "TRANSFORM=$TRANSFORM"
echo "PDC_DATA_LOC=$PDC_DATA_LOC"
echo "PDC_DATA_LOC=$PDC_TMPDIR"

echo "running server write script"
./srun_server_write.sh
sleep 10

echo "running client write script"
./srun_client_write.sh

echo "closing server script"
./srun_close_server.sh

echo "running server read script"
./srun_server_read.sh
sleep 10

echo "running client read script"
./srun_client_read.sh

echo "closing server script"
./srun_close_server.sh