#!/bin/bash

export TRANSFORM="raw"
export PDC_DATA_LOC=/pscratch/sd/n/nlewi26/src/work_space/install/pdc
export PDC_TMPDIR=/pscratch/sd/n/nlewi26/src/work_space/install/pdc

pushd ./build/bin || exit 1  # Exit if cd fails
export PDC_DEBUG=1
./vpicio_mts_transform_read 8388608 5 0 $TRANSFORM
popd
