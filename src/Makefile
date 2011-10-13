DBG = NO_DEBUG
VERBOSE = 0

ifeq ($(DBG),DEBUG)
VERBOSE = 1
endif


obj-m := adlink.o
adlink-objs := adlink_main.o adlink_fops.o adlink_fifo.o adlink_filter.o adlink_parse.o adlink_pci.o

EXTRA_CFLAGS := -I$(PWD) -I/usr/include -I/usr/realtime/include 

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) V=$(VERBOSE) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
format:
	@indent -gnu -fc1 -i4 -bli0 -nut -bap -l100 *.c *.h
clean-format:
	@rm *~
