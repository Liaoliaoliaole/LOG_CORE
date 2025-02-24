###############################################################################
#                     Makefile for Morfeas_MTI_test                           #
# Copyright (C) 12019-12020  Sam harry Tzavaras        	                      #
#                                                                             #
# This program is free software; you can redistribute it and/or               #
#  modify it under the terms of the GNU General Public License                #
#  as published by the Free Software Foundation; either version 3             #
#  of the License, or any later version.                     				  #
#                                                                             #
# This program is distributed in the hope that it will be useful,             #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of             #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              #
#  GNU General Public License for more details.                               #
#                                                                             #
# You should have received a copy of the GNU General Public License           #
# along with this program; If not, see <https://www.gnu.org/licenses/>.       #
###############################################################################

GCC_opt=gcc -O3 #-g3
CFLAGS= -std=c99 -DUA_ARCHITECTURE_POSIX -Wall # -Werror
LDLIBS= -lm -lrt -li2c -lpthread $(shell pkg-config --cflags --libs libcjson ncurses libxml-2.0 glib-2.0 libmodbus)
BUILD_dir=build
WORK_dir=work
SRC_dir=../..

CANif_DEP_HEADERS_dir = $(SRC_dir)/sdaq-worker/src/*.h
CANif_DEP_SRC_dir = $(SRC_dir)/sdaq-worker/src

HEADERS = $(SRC_dir)/IPC/*.h \
		  $(SRC_dir)/Morfeas_opc_ua/*.h \
		  $(SRC_dir)/Supplementary/*.h \
		  $(SRC_dir)/Morfeas_RPi_Hat/*.h \
		  $(SRC_dir)/Morfeas_MTI/*.h


Morfeas_MTI_test_DEP = $(WORK_dir)/Morfeas_MTI_test.o \
					   $(WORK_dir)/MTI_func.o \
					   $(WORK_dir)/Morfeas_run_check.o \
					   $(WORK_dir)/Morfeas_JSON.o \
					   $(WORK_dir)/SDAQ_drv.o \
					   $(WORK_dir)/Morfeas_Logger.o

all: $(BUILD_dir)/Morfeas_MTI_test

#Compilation of MTI test applications
$(BUILD_dir)/Morfeas_MTI_test: $(Morfeas_MTI_test_DEP) $(HEADERS)
	$(GCC_opt) $(CFLAGS) $^ -o $@ $(LDLIBS)

#Dependencies of the Morfeas_MTI_test
$(WORK_dir)/MTI_func.o: $(SRC_dir)/Morfeas_MTI/MTI_func.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Morfeas_MTI_test.o: Morfeas_MTI_test.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Morfeas_MTI_nodeset.o: $(SRC_dir)/Morfeas_MTI/Morfeas_MTI_nodeset.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

#Supplementary dependencies
$(WORK_dir)/SDAQ_drv.o: $(CANif_DEP_SRC_dir)/SDAQ_drv.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)
	
$(WORK_dir)/Morfeas_Logger.o: $(SRC_dir)/Supplementary/Morfeas_Logger.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Morfeas_JSON.o: $(SRC_dir)/Supplementary/Morfeas_JSON.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Morfeas_run_check.o: $(SRC_dir)/Supplementary/Morfeas_run_check.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

tree:
	mkdir -p $(BUILD_dir) $(WORK_dir)

delete-tree:
	rm -f -r $(WORK_dir) $(BUILD_dir)

clean:
	rm -f $(WORK_dir)/* $(BUILD_dir)/*

.PHONY: all clean delete-tree


