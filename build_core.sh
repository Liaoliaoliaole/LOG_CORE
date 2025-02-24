#!/bin/bash

set -e

#rm -rf src/cJSON src/sdaq-worker src/open62541 src/noPoll
#git submodule add https://github.com/Liaoliaoliaole/LOG_lib_cJson.git src/cJSON
#git submodule add https://github.com/Liaoliaoliaole/LOG_sdaq_worker.git src/sdaq-worker
#git submodule add https://github.com/Liaoliaoliaole/LOG_lib_open62541.git src/open62541
#git submodule add https://github.com/Liaoliaoliaole/LOG_lib_noPoll.git src/noPoll

git submodule update --init --recursive --remote


# Build submodules
echo "#Build cJSON libs."
cd src/cJSON
mkdir build && cd build
cmake -D BUILD_SHARED_LIBS=ON ..
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../../..
echo "#Build cJSON libs succeeded."

echo "#Build noPoll libs."
cd src/noPoll/nopoll-0.4.9
./autogen.sh
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../../..
echo "#Build noPoll libs succeeded."

echo "#Build open62541 libs."
cd src/open62541/
mkdir build && cd build
cmake -D BUILD_SHARED_LIBS=ON ..
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../../..
echo "#Build open62541 libs succeeded."

echo "#Build sdaq_worker libs."
cd src/sdaq-worker
make tree
make -j$(nproc)
sudo make install
cd ../..

# Build Morfeas core
echo "Build LOG core."
make tree
make -j$(nproc)
sudo make install
