# Build init for kernel and other programs as standalone executables
CROSS_COMPILE = x86_64-linux-gnu-

# Common flags for init (embedded in kernel, needs PIC)
INIT_CFLAGS  = -O1 -m64 -fPIC -mno-sse -ggdb -Wall
INIT_CFLAGS += -ffreestanding -fno-builtin -nostdlib -fno-stack-protector
INIT_CFLAGS += -fno-asynchronous-unwind-tables

# Common flags for user programs (static linking, no PIC needed)
USER_CFLAGS  = -O1 -m64 -mno-sse -ggdb -Wall
USER_CFLAGS += -ffreestanding -fno-builtin -nostdlib -fno-stack-protector
USER_CFLAGS += -fno-asynchronous-unwind-tables

# Linker flags for init (embedded in kernel)
INIT_LDFLAGS = -static --omagic --pic-executable --no-dynamic-linker --gc-sections

# Linker flags for user programs (normal static linking, all sections at high address)
USER_LDFLAGS = -static --gc-sections -Ttext-segment=0x100000000000

# All C source files except init.c, printf.c and umalloc.c (these are libraries, not programs)
USER_PROGS = $(patsubst %.c,%,$(filter-out init.c printf.c umalloc.c,$(wildcard *.c)))

# Directories for file system
FSDIR = fs
BINDIR = $(FSDIR)/bin
SBINDIR = $(FSDIR)/sbin
USRBINDIR = $(FSDIR)/usr/bin
DEVDIR = $(FSDIR)/dev
ETCDIR = $(FSDIR)/etc
TMPDIR = $(FSDIR)/tmp

.PHONY: all clean filesystem userprogs init

# Default target
all: init userprogs filesystem

# Build init as before (for kernel)
init: _init

_init: init.c printf.o trampoline.o ulib.h Makefile
	$(CROSS_COMPILE)gcc $(INIT_CFLAGS) -c init.c trampoline.S
	$(CROSS_COMPILE)ld $(INIT_LDFLAGS) -o $@ -e _start init.o trampoline.o
	$(CROSS_COMPILE)objcopy -S -j .text* -j.rodata* -j .data* -j .bss* --set-section-flags .bss=alloc,contents -O binary $@
	xxd -i $@ > ../kernel/src/initcode.inc

# Build user programs as standalone executables
userprogs: printf.o trampoline.o umalloc.o $(USER_PROGS)

# Compile shared objects first
printf.o: printf.c ulib.h
	$(CROSS_COMPILE)gcc $(USER_CFLAGS) -c printf.c

trampoline.o: trampoline.S
	$(CROSS_COMPILE)gcc $(USER_CFLAGS) -c trampoline.S

umalloc.o: umalloc.c ulib.h
	$(CROSS_COMPILE)gcc $(USER_CFLAGS) -c umalloc.c

# Special rule for programs that need malloc (like sh)
sh: sh.c ulib.h printf.o trampoline.o umalloc.o
	$(CROSS_COMPILE)gcc $(USER_CFLAGS) -c sh.c
	$(CROSS_COMPILE)ld $(USER_LDFLAGS) -o $@ -e _start sh.o printf.o trampoline.o umalloc.o

# Pattern rule for user programs (most don't need malloc)
%: %.c ulib.h printf.o trampoline.o
	$(CROSS_COMPILE)gcc $(USER_CFLAGS) -c $<
	$(CROSS_COMPILE)ld $(USER_LDFLAGS) -o $@ -e _start $*.o printf.o trampoline.o

# Create filesystem structure and ext4 image
filesystem: userprogs
	@echo "Creating filesystem structure..."
	rm -rf $(FSDIR)
	mkdir -p $(BINDIR) $(SBINDIR) $(USRBINDIR) $(DEVDIR) $(ETCDIR) $(TMPDIR)
	mkdir -p $(FSDIR)/home $(FSDIR)/proc $(FSDIR)/sys $(FSDIR)/mnt $(FSDIR)/root
	mkdir -p $(FSDIR)/var/log $(FSDIR)/usr/sbin $(FSDIR)/usr/lib
	
	@echo "Installing user programs..."
	cp $(USER_PROGS) $(BINDIR)/
	
	@echo "Creating basic system files..."
	echo "root:x:0:0:root:/root:/bin/sh" > $(ETCDIR)/passwd
	echo "root:x:0:" > $(ETCDIR)/group
	echo "nameserver 8.8.8.8" > $(ETCDIR)/resolv.conf
	echo "/dev/sda / ext4 defaults 0 1" > $(ETCDIR)/fstab
	
	@echo "Creating device nodes..."
	# Note: These will be empty files, actual device nodes need root privileges
	touch $(DEVDIR)/null $(DEVDIR)/zero $(DEVDIR)/random $(DEVDIR)/urandom
	touch $(DEVDIR)/stdin $(DEVDIR)/stdout $(DEVDIR)/stderr
	
	@echo "Creating ext4 filesystem image..."
	dd if=/dev/zero of=rootfs.img bs=1M count=64
	mkfs.ext4 -F rootfs.img
	
	@echo "Mounting and copying files..."
	mkdir -p mnt_tmp
	sudo mount -o loop rootfs.img mnt_tmp
	sudo cp -r $(FSDIR)/* mnt_tmp/
	sudo umount mnt_tmp
	rmdir mnt_tmp
	
	@echo "Filesystem image created: rootfs.img"

clean:
	rm -f *.o _* $(USER_PROGS)
	rm -rf $(FSDIR) mnt_tmp
	rm -f rootfs.img