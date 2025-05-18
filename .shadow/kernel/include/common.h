#ifndef _KERNEL_COMMON_H
#define _KERNEL_COMMON_H
//#define TRACE_F
#ifdef TRACE_F
#define TRACE_ENTRY printf("[trace] %s:entry\n", __func__)
#define TRACE_EXIT printf("[trace] %s:exit\n", __func__)
#else
#define TRACE_ENTRY ((void)0)
#define TRACE_EXIT ((void)0)
#endif
#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include <os.h>
#endif
