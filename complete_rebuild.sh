#!/bin/bash

rm -rf ./build/*

pushd build
cmake ../ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/pscratch/sd/n/nlewi26/src/work_space/install/pdc -DENABLE_MULTITHREAD=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_MPI_TESTING=ON -DENABLE_MPI=ON -DBUILD_SHARED_LIBS=ON -DPDC_SERVER_CACHE=ON -DBUILD_TESTING=ON -DPDC_ENABLE_MPI=ON -DCMAKE_C_COMPILER=mpicc 
make -j$(nproc)
make install
popd
