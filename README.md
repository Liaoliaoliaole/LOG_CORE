<div align="center"> <img src="./Docs/Morfeas_project_Documentation/ArtWork/Morfeas_logo.png" width="100"> </div>

# Morfeas-Core
This repository related to Morfeas core where is a sub-project of the Morfeas Project.

### The name "Morfeas"
Morfeas is the Latinisation of the Greek word "Μορφέας" which translate as:
Him that can give or change form (or shape). The name "Morfeas" describe the design [Philosophy](#design-philosophy)
of the project. Many small building blocks that change the form of the information from one protocol to other(s).

### Project Description
The Morfeas project was initially start as an implementation of a software gateway solution system
(currently named Morfeas-Proto) that provide (and translate) measurements data from some proprietary devices (SDAQ family)
with CANbus compatible interface (SDAQnet) to OPC-UA protocol (Open62541 based).

As the Morfeas project developed additional support added for other devices (MDAQ, IO-BOX, MTI, UniNOx) with different interfaces (ModBus-TCP, USB).

Furthermore, a web interface sub-project added to the Morfeas project under the name ["Morfeas-web"](https://gitlab.com/fantomsam/morfeas_web).
Thisof, will provide a layman friendly configuration interface for the gateway, the OPC-UA server's Nodeset and the connected devices.

**In relation with this project [Morfeas_RPi_Hat](./src/Morfeas_RPi_Hat)**
### Design Philosophy
The Morfeas-core project designed as a micro-component with supervision system.
This means that all the components called from a supervisor programs,
runs as daemon type processes, and they communicating using messages established from the **Morfeas IPC protocol**
that passing through **PIPEs**. All the project's components are written in **ANSI C**.

### Documentation
Project documentation located @:[/Docs/Morfeas_project_Documentation](./Docs/Morfeas_project_Documentation)

#### The Morfeas-core components:
* **Morfeas_daemon**: The supervisor.
* **Morfeas_opc_ua**: OPC-UA server.
* Driver/Handler for each supported interface:
  * **Morfeas_SDAQ_if**: SDAQ_net handler.
  * **Morfeas_MDAQ_if**: ModBus-TCP handler for MDAQs.
  * **Morfeas_IOBOX_if**: ModBus-TCP handler for IO-BOXes.
  * **Morfeas_MTI_if**: ModBus-TCP handler for MTIs.
  * **Morfeas_NOX_if**: CANBus handler for UniNOx.

### Requirements
For compilation of this project the following dependencies required.
* [GCC](https://gcc.gnu.org/) - The GNU Compilers Collection.
* [GNU Make](https://www.gnu.org/software/make/) - GNU make utility.
* [CMake](https://cmake.org/) - Cross-platform family of open source tools for package build.
* [OpenSSL](https://www.openssl.org/) - A full-featured toolkit for TLS and SSL protocols.
* [GnuTLS](https://gnutls.org/) - A secure communications library for SSL, TLS and DTLS protocols.
* [zlib](https://www.zlib.net/zlib_how.html) - A free software library used for data compression.
* [NCURSES](https://www.gnu.org/software/ncurses/ncurses.html) - A free (libre) software emulation library of curses.
* [GLib](https://wiki.gnome.org/Projects/GLib) - GNOME core application building blocks libraries.
* [LibGTop](https://developer.gnome.org/libgtop/stable/) - A library to get system specific data.
* [libxml2](http://xmlsoft.org/) - Library for parsing XML documents.
* [libmodbus](https://www.libmodbus.org/) - A free software library for communication via ModBus protocol.
* [Libsocketcan](https://directory.fsf.org/wiki/Libsocketcan) - Library to control some basic functions in SocketCAN from userspace.
* [libi2c](https://packages.debian.org/jessie/libi2c-dev) - A library that provide I2C functionality to Userspace.
* [libdbus](https://www.freedesktop.org/wiki/Software/dbus/#index1h1) - The library of the D-Bus.
<!--* [Libwebsockets](https://libwebsockets.org/) - An ANSI C library for implementing modern network protocols.
	* [libusb](https://libusb.info/) - An ANSI C library that provides generic access to USB devices.-->

##### Optionally
* [CAN-Utils](https://elinux.org/Can-utils) - CANBus utilities.
* [I2C-tools](https://packages.debian.org/jessie/i2c-tools) - Heterogeneous set of I2C tools for Linux kernel.

## The Source
The source of the Morfeas-core project have the following submodules:
* [SDAQ_worker](https://gitlab.com/fantomsam/sdaq-worker) - A libre (free) utilities suite for SDAQ devices.
* [cJSON](https://github.com/DaveGamble/cJSON) - An Ultralightweight open source JSON parser for ANSI C.
* [noPoll](http://www.aspl.es/nopoll/) - An open source ANSI C implementation of WebSocket (RFC 6455).
* [open62541](https://open62541.org/) - An open source C (C99) implementation of OPC-UA (IEC 62541).

### Get the Source
```
$ # Clone the project's source code
$ git clone https://gitlab.com/fantomsam/morfeas_project.git Morfeas_core
$ cd Morfeas_core
$ # Get Source of the submodules
$ git submodule update --init --recursive --remote --merge
```
### Compilation and installation of the submodules

#### cJSON
```
$ cd src/cJSON
$ mkdir build && cd build
$ cmake -D BUILD_SHARED_LIBS=ON ..
$ make -j$(nproc)
$ sudo make install
$ sudo ldconfig
```
#### noPoll
```
$ cd src/noPoll
$ ./autogen.sh
$ make -j$(nproc)
$ sudo make install
$ sudo ldconfig
```
#### Open62541
```
$ cd src/open62541
$ mkdir build && cd build
$ cmake -D BUILD_SHARED_LIBS=ON ..
$ make -j$(nproc)
$ sudo make install
$ sudo ldconfig
```
#### SDAQ_worker
```
$ cd src/sdaq_worker
$ make tree
$ make -j$(nproc)
$ sudo make install
```
### Compilation of the Morfeas-core Project
```
$ make tree
$ make -j$(nproc)
```
The executable binaries located under the **./build** directory.

### Installation of the Morfeas-core Project
```
$ sudo make install
```
### Configuration for the Morfeas_daemon, D-Bus and Systemd service
```
$ #--- Optionaly copy the configuration directory to your home ---
$ cp -r configuration ~/
$ #Make the nececery modifications on the Morfeas_daemon configuration (.xml) file
$ vim (~/)configuration/Morfeas_config.xml
$ #Make the nececery modifications on the Unit file of the Morfeas_daemon service
$ sudo systemctl edit Morfeas_system.service --full
$ #Also modify the configuration file of the Morfeas_daemon service
$ sudo vim /etc/systemd/system/Morfeas_system.service.d/Morfeas_system.conf
$ #Modify the D-Bus configuration for the Morfeas system
$ sudo vim /etc/dbus-1/system.d/Morfeas_system.conf
$ #Start the systemd service daemon
$ sudo systemctl start Morfeas_system.service
$ #--- Optionaly If you want to start the daemon on boot ---
$ sudo systemctl enable Morfeas_system.service
```
### Re-Compilation of the source
Guide link [here](./RE-INSTALL.md)

## Authors
* **Sam Harry Tzavaras** - *Initial work*

## License
The source code of the project is licensed under GPLv3 or later - see the [License](LICENSE) file for details.


