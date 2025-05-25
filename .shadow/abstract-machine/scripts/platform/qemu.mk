.PHONY: build-arg

smp        ?= 8
TESTDISK   ?= /media/wcy/Data/os-workbench/kernel/sdcard-rv.img
LDFLAGS    += -N -Ttext-segment=0x00100000
QEMU_FLAGS += -serial mon:stdio \
              -machine accel=tcg \
              -smp "cores=1,sockets=$(smp)" \
              -drive format=raw,file=$(IMAGE) \
              -drive file=$(TESTDISK),if=none,format=raw,id=x0 \
              -device virtio-blk-pci,drive=x0

# 如果存在 disk.img，则添加第二个磁盘
ifneq ($(wildcard $(BOOTDISK)),)
QEMU_FLAGS += -drive file=$(BOOTDISK),if=none,format=raw,id=x1 \
              -device virtio-blk-pci,drive=x1
endif

build-arg: image
	@( echo -n $(mainargs); ) | dd if=/dev/stdin of=$(IMAGE) bs=512 count=2 seek=1 conv=notrunc status=none

BOOT_HOME := $(AM_HOME)/am/src/x86/qemu/boot

image: $(IMAGE).elf
	@$(MAKE) -s -C $(BOOT_HOME)
	@echo + CREATE "->" $(IMAGE_REL)
	@( cat $(BOOT_HOME)/bootblock.o; head -c 1024 /dev/zero; cat $(IMAGE).elf ) > $(IMAGE)
