#!/bin/bash

pushd ./build/bin || exit 1  # Exit if cd fails
export PDC_DEBUG=1
./vpicio_mts_transform 8388608 5 20 "zfp" 
popd
