obj-m := kv_store_dev.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm -f *.o *.ko *.mod.* .*.cmd *.symvers *.order
	rm -rf .tmp_versions
