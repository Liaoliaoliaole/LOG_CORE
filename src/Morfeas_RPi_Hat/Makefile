GCC_opt=gcc -O3
CFLAGS= -std=c99 -DUA_ARCHITECTURE_POSIX -Wall -g3 #-Werror
LDLIBS= -lm -lrt -li2c -lpthread $(shell pkg-config --cflags --libs ncurses libgtop-2.0 glib-2.0 libmodbus)
BUILD_dir=build
WORK_dir=work

D_opt = -D RELEASE_HASH='"$(shell git log -1 --format=%h)"' \
		-D RELEASE_DATE=$(shell git log -1 --format=%ct) \
        -D COMPILE_DATE=$(shell date +%s)

Morfeas_RPi_hat_DEP = $(WORK_dir)/Morfeas_RPi_Hat_UI.o \
					  $(WORK_dir)/Morfeas_RPi_Hat.o \
					  $(WORK_dir)/Morfeas_run_check.o \
					  $(WORK_dir)/Morfeas_info.o

all: $(BUILD_dir)/Morfeas_RPi_Hat

#Compilation of Morfeas_RPi_Hat application
$(BUILD_dir)/Morfeas_RPi_Hat: $(Morfeas_RPi_hat_DEP) *.h
	$(GCC_opt) $(CFLAGS) $^ -o $@ $(LDLIBS)

#Dependencies of the Morfeas_RPi_Hat
$(WORK_dir)/Morfeas_RPi_Hat_UI.o: Morfeas_RPi_Hat_UI.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Morfeas_RPi_Hat.o: Morfeas_RPi_Hat.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Morfeas_run_check.o: ../Supplementary/Morfeas_run_check.c
	$(GCC_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

$(WORK_dir)/Morfeas_info.o: ../Supplementary/Morfeas_info.c
	$(GCC_opt) $(D_opt) $(CFLAGS) $^ -c -o $@ $(LDLIBS)

tree:
	mkdir -p $(BUILD_dir) $(WORK_dir)

delete-the-tree:
	rm -f -r $(WORK_dir) $(BUILD_dir)

clean:
	rm -f $(WORK_dir)/* $(BUILD_dir)/*

install:
	@echo "Installation of executable Binaries"
	@install $(BUILD_dir)/Morfeas_RPi_Hat -v -t /usr/local/bin/

uninstall:
	rm /usr/local/bin/Morfeas_RPi_Hat

.PHONY: all clean clean-tree


