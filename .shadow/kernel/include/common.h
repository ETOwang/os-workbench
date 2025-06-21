#ifndef _KERNEL_COMMON_H
#define _KERNEL_COMMON_H
// #define TRACE_F
#ifdef TRACE_F
#define TRACE_ENTRY printf("[trace] %s:entry\n", __func__)
#define TRACE_EXIT printf("[trace] %s:exit\n", __func__)
#else
#define TRACE_ENTRY ((void)0)
#define TRACE_EXIT ((void)0)
#endif
#define PTE_ADDR(pte) ((pte) & 0x000ffffffffff000ULL)
#define STACK_SIZE (1 << 16)
#define FENCE_PATTERN 0xABCDABCD
#define TASK_READY 1
#define TASK_RUNNING 2
#define TASK_BLOCKED 3
#define TASK_DEAD 4
#define TASK_ZOMBIE 5
#define MAX_CPU 32
#define MAX_TASK 128
#define UVMEND 0x108000000000
#define UVSTART 0x100000000000
#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include <os.h>
#include <devices.h>
#endif
