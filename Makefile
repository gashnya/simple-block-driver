obj-m += axe_block_driver.o

axe_block_driver-y := \
	axe_main.o \

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

