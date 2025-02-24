# Re-Compilation of the source

### Get the latest Source and sub-modules release
```
$ cd Morfeas_core
$ # Get Latest Source
$ git pull
$ # Get Latest Source for the submodules
$ git submodule foreach git pull origin master
```
### Re-Compilation and installation of the sub-modules

#### cJSON
```
$ cd src/cJSON/build
$ make clean
$ make -j$(nproc)
$ sudo make install
$ cd ../../..
```
#### noPoll
```
$ cd src/noPoll
$ make clean
$ ./configure
$ make -j$(nproc)
$ sudo make install
$ cd ../..
```
#### open62541
```
$ cd src/open62541/build
$ make clean
$ make -j$(nproc)
$ sudo make install
$ cd ../../..
```
#### SDAQ_worker
```
$ cd src/sdaq_worker
$ make clean
$ make -j$(nproc)
$ sudo make install
$ cd ../..
```
### Re-Compilation and installation of the Morfeas-core Project
```
$ make clean
$ make -j$(nproc)
$ sudo make install
```
### Restart Morfeas_daemon
```
$ sudo service Morfeas_system restart
```