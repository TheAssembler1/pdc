#!/bin/bash
#SBATCH --nodes=12
#SBATCH --ntasks-per-node=16
#SBATCH --cpus-per-task=1
#SBATCH --account=pas2406
#SBATCH --time=01:00:00

set -x

echo "going to pdc"
gopdc

echo "removing old .txt"
rm *.txt

echo "launching server"
./srun_server.sh &

echo "sleeping 20"
sleep 20

echo "lauching client"
./srun_client.sh
