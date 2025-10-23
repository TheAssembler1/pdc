#!/bin/bash

set -e

echo "killing previous pdc_server"
pkill -f pdc_server || true
lsof -t -i :8000 | xargs -r kill -9

export TRANSFORM="raw"
export PDC_DATA_LOC=/pscratch/sd/n/nlewi26/src/work_space/install/pdc
export PDC_TMPDIR=/pscratch/sd/n/nlewi26/src/work_space/install/pdc

echo "deleting existing data"
rm -rf $PDC_DATA_LOC/pdc_data

pushd ./build/bin
export PDC_DEBUG=1
$1 ./pdc_server
popd
