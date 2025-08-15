#!/bin/bash

set -ex

./rebuild.sh

echo "killing previous pdc_server"
pkill -f pdc_server || true

pushd ./build
ctest -V -L transform --output-on-failure
popd
