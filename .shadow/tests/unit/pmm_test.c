#include "../include/test_framework.h"
#include <os.h>

// 测试前准备
static void pmm_setup(void)
{
    // 确保内存管理模块已初始化
    pmm->init();
    printf("PMM模块已初始化\n");
}

// 测试后清理
static void pmm_teardown(void)
{
    // 在这里可以添加清理代码，如果需要的话
}

// 基本分配和释放测试
static void test_pmm_basic(void)
{
    // 测试小块内存的分配和释放
    void *ptr1 = pmm->alloc(64);
    ASSERT_NOT_NULL(ptr1, "分配64字节内存失败");

    // 写入内存以验证可用性
    memset(ptr1, 0xAA, 64);

    // 释放内存
    pmm->free(ptr1);

    // 测试中等大小内存块的分配
    void *ptr2 = pmm->alloc(1024);
    ASSERT_NOT_NULL(ptr2, "分配1KB内存失败");
    memset(ptr2, 0xBB, 1024);
    pmm->free(ptr2);

    // 测试较大内存块的分配
    void *ptr3 = pmm->alloc(1024 * 1024); // 1MB
    ASSERT_NOT_NULL(ptr3, "分配1MB内存失败");
    memset(ptr3, 0xCC, 1024 * 1024);
    pmm->free(ptr3);
}

// 多次分配释放测试
static void test_pmm_multiple_alloc(void)
{
#define ALLOC_COUNT 100
#define BLOCK_SIZE 256

    void *ptrs[ALLOC_COUNT];

    // 连续分配多个内存块
    for (int i = 0; i < ALLOC_COUNT; i++)
    {
        ptrs[i] = pmm->alloc(BLOCK_SIZE);
        ASSERT_NOT_NULL(ptrs[i], "在多次分配测试中分配内存失败");

        // 写入标识数据，每个块使用不同的标识
        memset(ptrs[i], i & 0xFF, BLOCK_SIZE);
    }

    // 验证数据完整性
    for (int i = 0; i < ALLOC_COUNT; i++)
    {
        unsigned char *data = (unsigned char *)ptrs[i];
        for (int j = 0; j < BLOCK_SIZE; j++)
        {
            ASSERT_EQ(i & 0xFF, data[j], "内存数据完整性验证失败");
            if (j > 10)
                break; // 只检查前几个字节，避免测试过慢
        }
    }

    // 释放所有内存
    for (int i = 0; i < ALLOC_COUNT; i++)
    {
        pmm->free(ptrs[i]);
    }
}

// 边界情况测试
static void test_pmm_edge_cases(void)
{
    // 测试分配零字节
    void *ptr_zero = pmm->alloc(0);
    if (ptr_zero != NULL)
    {
        pmm->free(ptr_zero);
        printf("注意：PMM允许分配0字节，返回了非NULL指针\n");
    }
    else
    {
        printf("PMM拒绝分配0字节，返回了NULL\n");
    }

    // 测试分配非常大的内存
    void *ptr_huge = pmm->alloc(1024 * 1024 * 1024); // 1GB
    if (ptr_huge != NULL)
    {
        printf("成功分配1GB内存\n");
        memset(ptr_huge, 0, 1024); // 只尝试写入前1KB
        pmm->free(ptr_huge);
    }
    else
    {
        printf("分配1GB内存失败，这可能是预期行为\n");
    }
}

// 性能测试
static void test_pmm_performance(void)
{
#define PERF_COUNT 1000
#define PERF_SIZE 1024 // 1KB

    void *ptrs[PERF_COUNT];

    // 测量分配时间
    uint64_t start_time, end_time;
    AM_TIMER_UPTIME_T uptime;

    // 开始计时
    ioe_read(AM_TIMER_UPTIME, &uptime);
    start_time = uptime.us;

    // 执行多次分配
    for (int i = 0; i < PERF_COUNT; i++)
    {
        ptrs[i] = pmm->alloc(PERF_SIZE);
        ASSERT_NOT_NULL(ptrs[i], "性能测试中内存分配失败");
    }

    // 结束计时
    ioe_read(AM_TIMER_UPTIME, &uptime);
    end_time = uptime.us;

    double alloc_time_ms = (end_time - start_time) / 1000.0;
    double avg_alloc_time = alloc_time_ms / PERF_COUNT;

    printf("内存分配性能:\n");
    printf("  总时间: %.2f ms\n", alloc_time_ms);
    printf("  平均每次分配时间: %.4f ms\n", avg_alloc_time);

    // 开始计时 - 释放
    ioe_read(AM_TIMER_UPTIME, &uptime);
    start_time = uptime.us;

    // 执行多次释放
    for (int i = 0; i < PERF_COUNT; i++)
    {
        pmm->free(ptrs[i]);
    }

    // 结束计时
    ioe_read(AM_TIMER_UPTIME, &uptime);
    end_time = uptime.us;

    double free_time_ms = (end_time - start_time) / 1000.0;
    double avg_free_time = free_time_ms / PERF_COUNT;

    printf("内存释放性能:\n");
    printf("  总时间: %.2f ms\n", free_time_ms);
    printf("  平均每次释放时间: %.4f ms\n", avg_free_time);
}

// 注册PMM测试套件
void pmm_test_register(void)
{
    test_suite_t *suite = test_suite_create("内存管理(PMM)测试");

    test_suite_add_case(suite, "基本分配和释放", pmm_setup, test_pmm_basic, pmm_teardown);
    test_suite_add_case(suite, "多次分配释放", pmm_setup, test_pmm_multiple_alloc, pmm_teardown);
    test_suite_add_case(suite, "边界情况测试", pmm_setup, test_pmm_edge_cases, pmm_teardown);
    test_suite_add_case(suite, "性能测试", pmm_setup, test_pmm_performance, pmm_teardown);

    test_register_suite(suite);
}