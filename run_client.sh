#!/bin/bash

./rebuild.sh  # Rebuild project

pushd ./build/bin || exit 1  # Exit if cd fails
export PDC_DEBUG=1
"./workflow_sandbox"
popd

echo "Running dot on graph"
dot -Tpng ./build/bin/graph.txt -o graph.png
