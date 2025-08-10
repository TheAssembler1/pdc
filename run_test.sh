#!/bin/bash

set -e

./rebuild.sh

echo "killing previous pdc_server.exe"
pkill -f pdc_server || true

pushd ./build
ctest --stop-on-failure --output-on-failure
popd
