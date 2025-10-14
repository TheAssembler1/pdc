#!/bin/bash

MAX_NUM_NODES=256

for ((i=1; i<=$MAX_NUM_NODES; i*=2)); do
    echo "Running test with i=$i node(s)"
    sbatch \
        --nodes=$i \
        --ntasks-per-node=128 \
        --export=TRANSFORM="raw",DATA_SERVERS_PER_NODE=1,CLIENTS_PER_NODE=32 \
        vpicio_scale_run.sh
done