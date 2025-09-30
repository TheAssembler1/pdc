#!/bin/bash

set -x
set -u

NUM_NODES=2

pushd ./build/bin
rm *.log *.err || true
srun -N $NUM_NODES -n 4 --ntasks-per-node=2 --error="server_output.err" --output="server_output.log" ./pdc_server &
popd
