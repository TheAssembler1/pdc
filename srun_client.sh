set -x
set -u

NUM_NODES=12
TASKS_PER_NODE=32
TOTAL_TASKS=$((NUM_NODES * TASKS_PER_NODE))

pushd ./build/bin
rm -f client_output.txt
srun -N"$NUM_NODES" -n"$TOTAL_TASKS" --ntasks-per-node="$TASKS_PER_NODE" ./vpicio_mts 8388608 5 20 2>&1 | tee client_output.txt
popd

