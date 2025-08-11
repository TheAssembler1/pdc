#!/bin/bash

set -e

./rebuild.sh

echo "killing previous pdc_server"
pkill -f pdc_server || true

pushd ./build
ctest -V -L serial --output-on-failure
popd
