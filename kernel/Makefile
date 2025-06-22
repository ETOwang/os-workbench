
NAME           := kernel
SRCS           := framework/main.c $(shell find -L ./src/ -name "*.c")
INC_PATH       := include/ framework/ include/ext4

export AM_HOME := $(PWD)/../abstract-machine
ifeq ($(ARCH),)
export ARCH    := x86_64-qemu
endif

include $(AM_HOME)/Makefile
include ../oslabs.mk

# Debug target: Start QEMU with GDB debugging using debug.py script
# Usage: make debug
# This will:
#   1. Build the kernel image
#   2. Start QEMU with GDB remote debugging (-s -S flags)
#   3. Launch GDB with the debug.py script which:
#      - Connects to QEMU on localhost:1234
#      - Loads debug symbols from kernel-x86_64-qemu.elf
#      - Sets a breakpoint at os_run function
#      - Continues execution
debug:
	@echo "Building kernel for debugging..."
	@$(MAKE) -s build-arg
	@echo "Starting QEMU with GDB debugging..."
	@echo "- QEMU will start with GDB remote debugging on port 1234"
	@echo "- GDB will connect automatically and set breakpoint at os_run"
	@echo "- Use 'c' to continue, 'q' to quit GDB and stop QEMU"
	@echo "----------------------------------------"
	@(qemu-system-x86_64 -serial mon:stdio -machine accel=tcg -smp "cores=1,sockets=1" -drive format=raw,file=build/kernel-x86_64-qemu -drive format=raw,file=/media/wcy/Data/os-workbench/sdcard-rv.img -s -S &) && sleep 1 && gdb -x debug.py

.PHONY: debug

image: git
