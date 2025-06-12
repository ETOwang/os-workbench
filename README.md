# OS Workbench - Kernel Development Project

## Overview

This project implements a complete operating system kernel based on the Operating Systems course project from Nanjing University. The kernel includes essential subsystems such as physical memory management(pmm), kernel multiple threads(kmt), virtual file system(vfs), and user process management(uproc).

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    User Applications                        │
├─────────────────────────────────────────────────────────────┤
│  System Calls  │  VFS Layer  │  Device Interface            │
├─────────────────────────────────────────────────────────────┤
│  Process Mgmt   │  Memory Mgmt │  Interrupt Handler         │
├─────────────────────────────────────────────────────────────┤
│  Device Drivers │  TTY Driver  │  Framebuffer Driver        │
├─────────────────────────────────────────────────────────────┤
│                    Hardware Abstraction                     │
└─────────────────────────────────────────────────────────────┘
```

## Building and Running

### Prerequisites
- GCC cross-compiler for x86_64
- QEMU emulator
- Make build system

### Build Instructions
```bash
cd kernel
make                    # Build the kernel
make run               # Build and run in QEMU
```

### Running the System
The kernel boots into a simple multi-TTY environment:
- **TTY1**: Primary shell interface
- **TTY2**: Secondary shell interface
- **Alt+1/Alt+2**: Switch between terminals

### File System Configuration
You can configure different file system images by modifying the `abstract-machine/scripts/platform/qemu.mk` script. The system currently supports ext4 file systems:

```bash
# Edit the QEMU disk image path in qemu.mk
# Example: Change the disk image location to use different file systems
-drive file=/path/to/your/filesystem.img,format=raw,if=ide
```

This allows you to test the kernel with different file system configurations and user-space environments.


## Future Enhancements

### User Process Management Fixes
Currently, there are issues in the user process management system that prevent user-space programs from running correctly. The next development phase will fix existing bugs and focus on:

- **ELF Loading Improvements**: Fix virtual address mapping issues in user process execution
- **Memory Layout Optimization**: Address virtual memory conflicts in user process address space
- **Process Lifecycle Management**: Enhance process creation, execution, and termination handling

### BusyBox Integration
The goal is to integrate BusyBox to provide a complete shell environment, replacing the current kernel-level demonstration shell:

- **Complete Shell Environment**: Replace current kernel shell with full BusyBox implementation
- **POSIX Compliance**: Ensure system call compatibility with standard Unix utilities
- **Standard Utilities**: Provide access to essential Unix commands (ls, cat, grep, mv, cp, etc.)
