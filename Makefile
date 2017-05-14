obj-m :=scull.o

KDIR:=/lib/modules/`uname -r`/build

module:
	make -C $(KDIR) M=`pwd` modules

clean:
	make -C $(KDIR) M=`pwd` clean

install:
	./scull_load

uninstall:
	./scull_unload

