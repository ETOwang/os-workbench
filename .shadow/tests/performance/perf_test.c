#include "test_framework.h"
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <os.h>

// 性能测试参数
#define PERF_ITERATIONS 10000
#define MEM_TEST_SIZE (64 * 1024) // 64KB

// 测试前准备
static void perf_setup(void)
{
    // 初始化性能测试环境
}

// 测试后清理
static void perf_teardown(void)
{
    // 清理工作，如果需要的话
}

// 测量上下文切换性能
static void perf_context_switch(void)
{
    uint64_t start_time, end_time;
    uint32_t ms;

    // 创建两个上下文
    Area kstack1, kstack2;
    kstack1.start = malloc(4096);
    kstack1.end = (char *)kstack1.start + 4096;
    kstack2.start = malloc(4096);
    kstack2.end = (char *)kstack2.start + 4096;

    Context *ctx1 = kcontext(kstack1, NULL, NULL);
    Context *ctx2 = kcontext(kstack2, NULL, NULL);

    // 准备计时
    ioe_read(AM_TIMER_UPTIME, &ms);
    start_time = ms;

    // 执行上下文切换
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        yield(); // 在AM中，这会触发上下文切换
    }

    // 结束计时
    ioe_read(AM_TIMER_UPTIME, &ms);
    end_time = ms;

    // 计算并打印结果
    double total_time_ms = end_time - start_time;
    double avg_switch_time = total_time_ms / PERF_ITERATIONS;

    printf("上下文切换性能测试:\n");
    printf("  总迭代次数: %d\n", PERF_ITERATIONS);
    printf("  总耗时: %.2f ms\n", total_time_ms);
    printf("  平均每次切换耗时: %.4f ms\n", avg_switch_time);

    // 清理资源
    free(kstack1.start);
    free(kstack2.start);
}

// 测量内存分配/释放性能
static void perf_memory_alloc(void)
{
    uint64_t start_time, end_time;
    uint32_t ms;
    void *ptrs[PERF_ITERATIONS];

    // 准备计时 - 分配
    ioe_read(AM_TIMER_UPTIME, &ms);
    start_time = ms;

    // 执行连续内存分配
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        ptrs[i] = malloc(64); // 分配小块内存
    }

    // 结束计时 - 分配
    ioe_read(AM_TIMER_UPTIME, &ms);
    end_time = ms;

    double alloc_time_ms = end_time - start_time;

    // 准备计时 - 释放
    ioe_read(AM_TIMER_UPTIME, &ms);
    start_time = ms;

    // 执行连续内存释放
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        free(ptrs[i]);
    }

    // 结束计时 - 释放
    ioe_read(AM_TIMER_UPTIME, &ms);
    end_time = ms;

    double free_time_ms = end_time - start_time;

    // 计算并打印结果
    printf("内存分配/释放性能测试:\n");
    printf("  总迭代次数: %d\n", PERF_ITERATIONS);
    printf("  分配总耗时: %.2f ms (平均: %.4f ms/op)\n",
           alloc_time_ms, alloc_time_ms / PERF_ITERATIONS);
    printf("  释放总耗时: %.2f ms (平均: %.4f ms/op)\n",
           free_time_ms, free_time_ms / PERF_ITERATIONS);
}

// 测量内存拷贝性能
static void perf_memcpy(void)
{
    uint64_t start_time, end_time;
    uint32_t ms;

    // 准备测试缓冲区
    void *src = malloc(MEM_TEST_SIZE);
    void *dst = malloc(MEM_TEST_SIZE);
    memset(src, 0x55, MEM_TEST_SIZE);

    // 准备计时
    ioe_read(AM_TIMER_UPTIME, &ms);
    start_time = ms;

    // 执行内存拷贝
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        memcpy(dst, src, MEM_TEST_SIZE / PERF_ITERATIONS);
    }

    // 结束计时
    ioe_read(AM_TIMER_UPTIME, &ms);
    end_time = ms;

    // 计算并打印结果
    double total_time_ms = end_time - start_time;
    double bytes_per_sec = (MEM_TEST_SIZE * 1000.0) / total_time_ms;

    printf("内存拷贝性能测试:\n");
    printf("  总拷贝字节数: %d\n", MEM_TEST_SIZE);
    printf("  总耗时: %.2f ms\n", total_time_ms);
    printf("  带宽: %.2f MB/s\n", bytes_per_sec / (1024 * 1024));

    // 清理资源
    free(src);
    free(dst);
}

// 测量锁竞争性能 (多CPU环境下)
static void perf_lock_contention(void)
{
    if (cpu_count() < 2)
    {
        printf("跳过锁竞争测试: 需要至少2个CPU核心\n");
        return;
    }

    uint64_t start_time, end_time;
    uint32_t ms;
    int lock = 0;
    int shared_counter = 0;

    // 准备计时
    ioe_read(AM_TIMER_UPTIME, &ms);
    start_time = ms;

    // 在多核上执行锁操作
    for (int i = 0; i < PERF_ITERATIONS; i++)
    {
        // 获取锁
        while (atomic_xchg(&lock, 1) != 0)
        {
            yield();
        }

        // 临界区操作
        shared_counter++;

        // 释放锁
        atomic_xchg(&lock, 0);

        // 增加线程交错的机会
        if (i % 100 == 0)
        {
            yield();
        }
    }

    // 结束计时
    ioe_read(AM_TIMER_UPTIME, &ms);
    end_time = ms;

    // 计算并打印结果
    double total_time_ms = end_time - start_time;
    double avg_lock_time = total_time_ms / PERF_ITERATIONS;

    printf("锁竞争性能测试:\n");
    printf("  总迭代次数: %d\n", PERF_ITERATIONS);
    printf("  总耗时: %.2f ms\n", total_time_ms);
    printf("  平均每次加锁-解锁周期耗时: %.4f ms\n", avg_lock_time);
}

// 注册测试
void perf_test_register(void)
{
    test_suite_t *suite = test_suite_create("性能测试");

    test_suite_add_case(suite, "上下文切换性能", perf_setup, perf_context_switch, perf_teardown);
    test_suite_add_case(suite, "内存分配/释放性能", perf_setup, perf_memory_alloc, perf_teardown);
    test_suite_add_case(suite, "内存拷贝性能", perf_setup, perf_memcpy, perf_teardown);
    test_suite_add_case(suite, "锁竞争性能", perf_setup, perf_lock_contention, perf_teardown);

    test_register_suite(suite);
}