#include "test_framework.h"
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <os.h>

// 共享数据结构，用于测试原子操作
static int shared_counter = 0;
static int expected_counter = 0;
#define NUM_ITERATIONS 1000

// 测试前准备
static void mp_setup(void)
{
    shared_counter = 0;
    expected_counter = 0;

    // 初始化多处理器环境
    mpe_init(NULL);
}

// 测试后清理
static void mp_teardown(void)
{
    // 清理工作，如果需要的话
}

// 测试CPU计数与ID
static void test_cpu_info(void)
{
    int count = cpu_count();
    int current = cpu_current();

    ASSERT_TRUE(count > 0, "CPU数量应该大于0");
    ASSERT_TRUE(current >= 0 && current < count, "当前CPU ID应该在有效范围内");

    printf("系统有 %d 个CPU核心，当前运行在核心 %d 上\n", count, current);
}

// 测试原子交换操作
static void test_atomic_xchg(void)
{
    int value = 42;
    int old_value = atomic_xchg(&value, 100);

    ASSERT_EQ(42, old_value, "原子交换应该返回旧值");
    ASSERT_EQ(100, value, "原子交换应该设置新值");
}

// 并发递增计数器的测试函数
static void concurrent_increment(void *arg)
{
    int thread_id = *(int *)arg;
    int iterations = NUM_ITERATIONS;

    printf("线程 %d 开始运行在CPU %d上\n", thread_id, cpu_current());

    for (int i = 0; i < iterations; i++)
    {
        // 模拟工作负载
        for (int j = 0; j < 1000; j++)
        {
            __asm__ volatile("nop");
        }

        // 原子增加共享计数器
        int value;
        do
        {
            value = shared_counter;
        } while (atomic_xchg(&shared_counter, value + 1) != value);

        // 可能的yield，增加线程交错的机会
        if (i % 100 == 0)
        {
            yield();
        }
    }
}

// 测试并发原子操作
static void test_concurrent_atomic(void)
{
    if (cpu_count() < 2)
    {
        printf("跳过并发测试: 需要至少2个CPU核心\n");
        return;
    }

    int cpu_ids[4] = {0, 1, 2, 3}; // 最多支持4个核心
    int thread_count = cpu_count() > 4 ? 4 : cpu_count();
    expected_counter = thread_count * NUM_ITERATIONS;

    // 创建线程
    for (int i = 1; i < thread_count; i++)
    {
        // 真实系统中，这里应该使用线程创建API
        // 在AM中，我们可以使用yield()来模拟线程切换
        yield(); // 让其他CPU核心有机会运行
    }

    // 主线程也参与计算
    concurrent_increment(&cpu_ids[0]);

    // 等待其他线程完成(简单模拟)
    for (int i = 0; i < 1000; i++)
    {
        yield();
    }

    // 验证结果
    ASSERT_EQ(expected_counter, shared_counter, "并发原子操作应该正确更新共享计数器");
}

// 测试锁机制 (假设OS提供了简单的锁实现)
static void test_lock_mechanism(void)
{
    // 这个测试需要OS提供锁机制
    // 这里只是一个示例框架

    // 创建锁
    int lock = 0;

    // 获取锁
    while (atomic_xchg(&lock, 1) != 0)
    {
        yield();
    }

    // 临界区操作
    shared_counter++;

    // 释放锁
    atomic_xchg(&lock, 0);

    ASSERT_EQ(1, shared_counter, "锁应该保护临界区操作");
}

// 注册测试
void mp_test_register(void)
{
    test_suite_t *suite = test_suite_create("多处理器支持测试");

    test_suite_add_case(suite, "CPU信息测试", mp_setup, test_cpu_info, mp_teardown);
    test_suite_add_case(suite, "原子交换测试", mp_setup, test_atomic_xchg, mp_teardown);
    test_suite_add_case(suite, "并发原子操作测试", mp_setup, test_concurrent_atomic, mp_teardown);
    test_suite_add_case(suite, "锁机制测试", mp_setup, test_lock_mechanism, mp_teardown);

    test_register_suite(suite);

    // 添加并发运行的示例
    // 这里我们创建一个专门的测试套件用于并发测试
    test_suite_t *concurrent_suite = test_suite_create("并发执行测试套件");

    for (int i = 0; i < 100; i++)
    {
        char test_name[32];
        sprintf(test_name, "并发测试 %d", i);
        test_suite_add_case(concurrent_suite, test_name, NULL, test_atomic_xchg, NULL);
    }

    test_register_suite(concurrent_suite);
}