import gdb
import os

# Register the quit hook
def on_quit():
    gdb.execute('kill')

gdb.events.exited.connect(on_quit)

# Connect to the remote target
gdb.execute('target remote localhost:1234')

# Load the debug symbols
path = f'./build/kernel-x86_64-qemu.elf'
gdb.execute(f'file {path}')

# This is the 0x7c00
gdb.Breakpoint('os_run')

# Continue execution
gdb.execute('continue')
