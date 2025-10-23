#!/bin/bash

export TRANSFORM="raw"
export PDC_DATA_LOC=/pscratch/sd/n/nlewi26/src/work_space/install/pdc
export PDC_TMPDIR=/pscratch/sd/n/nlewi26/src/work_space/install/pdc

pushd ./build/bin
./close_server
popd
