#!/bin/bash

set -ex

echo "killing previous pdc_server"
pkill -f pdc_server || true

pushd ./build
ctest -V -L transform --output-on-failure
#ctest --output-on-failure
popd
