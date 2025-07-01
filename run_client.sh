#!/bin/bash

./rebuild.sh  # Rebuild project

pushd ./build/bin || exit 1  # Exit if cd fails
export PDC_DEBUG=1
"./workflow2"
dot -Tpng -Gdpi=300 graph.txt -o graph.png
cp graph.png ../../
popd


