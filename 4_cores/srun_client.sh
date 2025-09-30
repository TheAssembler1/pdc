set -x
set -u

NUM_NODES=2
TASKS_PER_NODE=126
TOTAL_TASKS=$((NUM_NODES * TASKS_PER_NODE))

pushd ./build/bin
srun -N "$NUM_NODES" -n "$TOTAL_TASKS" --ntasks-per-node="$TASKS_PER_NODE" ./vpicio_mts 8388608 5 20
popd

