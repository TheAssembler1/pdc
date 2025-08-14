#!/bin/bash

./rebuild.sh  # Rebuild project

pushd ./build/bin || exit 1  # Exit if cd fails
export PDC_DEBUG=1
$1 "./region_transfer_compression_transform"
popd
