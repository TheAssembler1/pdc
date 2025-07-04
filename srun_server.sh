set -x
set -u

pushd ./build/bin
rm server_output.txt || true
NUM_NODES=12
srun -N$NUM_NODES -n$NUM_NODES --ntasks-per-node=1 ./pdc_server.exe 2>&1 | tee server_output.txt
popd
