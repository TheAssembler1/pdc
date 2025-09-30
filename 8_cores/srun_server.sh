#!/bin/bash

set -x
set -u

NUM_NODES=2

pushd ./build/bin
rm *.log *.err || true
srun -N $NUM_NODES -n 8 --ntasks-per-node=4 --error="server_output.err" --output="server_output.log" ./pdc_server &
popd
