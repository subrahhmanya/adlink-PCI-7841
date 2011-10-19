DBG = DEBUG
VERBOSE = 0
VERSION := $(shell uname -r)
INSTALL_LOCATION = /lib/modules/$(VERSION)/misc
TARGET = adlink.ko

ifeq ($(DBG),DEBUG)
VERBOSE = 1
DEBUG_FLAGS = -O -DDEBUG
else
DEBUG_FLAGS = -O2
endif


obj-m := adlink.o
adlink-objs := adlink_main.o adlink_fops.o adlink_fifo.o adlink_filter.o adlink_parse.o adlink_sja1000.o adlink_pci.o

EXTRA_CFLAGS := -I$(PWD) -I/usr/include -I/usr/realtime/include 
EXTRA_CFLAGS += $(DEBUG_FLAGS)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) V=$(VERBOSE) modules
	@ctags *.c *.h
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
format:
	@indent -gnu -fc1 -i4 -bli0 -nut -bap -l100 *.c *.h
clean-format:
	@rm *~

compat:
	@if [ -f /etc/modprobe.d/pcan ] ; then \
	mv /etc/modprobe.d/pcan /etc/modprobe.d/pcan.conf ; \
	echo "Info: Moved /etc/modprobe.d/pcan to /etc/modprobe.d/pcan.conf for compatibility reason."; \
	fi

install: compat
	@if test -d $(INSTALL_LOCATION) ; then echo "Info: $(INSTALL_LOCATION) exists."; else mkdir $(INSTALL_LOCATION); fi
	@cp -f $(TARGET) $(INSTALL_LOCATION)/$(TARGET)
	@echo "running depmod"
	@depmod

.PHONY : message
message:
	@ echo "$(VERSION)" 
