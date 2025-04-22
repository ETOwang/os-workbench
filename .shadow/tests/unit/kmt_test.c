#include "../include/test_framework.h"
#include <os.h>

// 全局测试变量
static int shared_counter = 0;
static spinlock_t test_lock;
static sem_t test_sem;
static int test_complete = 0;

// 线程测试函数
static void test_thread_func(void *arg)
{
    int id = (uintptr_t)arg;
    printf("测试线程 %d 开始运行\n", id);

    // 执行一些工作
    for (int i = 0; i < 100; i++)
    {
        kmt->spin_lock(&test_lock);
        shared_counter++;
        kmt->spin_unlock(&test_lock);

        yield(); // 让出CPU，增加并发可能性
    }

    printf("测试线程 %d 结束运行\n", id);

    // 完成测试
    kmt->spin_lock(&test_lock);
    test_complete++;
    kmt->spin_unlock(&test_lock);

    // 通知主线程
    kmt->sem_signal(&test_sem);
}

// 测试前准备
static void kmt_setup(void)
{
    // 初始化KMT模块
    kmt->init();
    printf("KMT模块已初始化\n");

    // 重置测试变量
    shared_counter = 0;
    test_complete = 0;
    kmt->spin_init(&test_lock, "测试锁");
    kmt->sem_init(&test_sem, "测试信号量", 0);
}

// 测试后清理
static void kmt_teardown(void)
{
    // 这里可以添加清理代码
    printf("KMT测试清理完成\n");
}

// 基本线程创建和销毁测试
static void test_kmt_thread_basic(void)
{
    task_t task;

    // 创建线程
    int result = kmt->create(&task, "test-thread", test_thread_func, (void *)1);
    ASSERT_EQ(0, result, "线程创建失败");

    // 等待线程执行一段时间
    for (int i = 0; i < 10; i++)
    {
        yield();
    }

    // 销毁线程
    kmt->teardown(&task);
    printf("线程基本测试完成\n");
}

// 多线程测试
static void test_kmt_thread_multiple(void)
{
#define THREAD_COUNT 4
    task_t tasks[THREAD_COUNT];

    printf("创建 %d 个线程...\n", THREAD_COUNT);

    // 创建多个线程
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        int result = kmt->create(&tasks[i], "multi-thread", test_thread_func, (void *)(uintptr_t)(i + 1));
        ASSERT_EQ(0, result, "多线程测试中线程创建失败");
    }

    // 等待所有线程完成
    printf("等待所有线程完成...\n");
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        kmt->sem_wait(&test_sem);
    }

    // 验证结果
    int expected_counter = THREAD_COUNT * 100;
    ASSERT_EQ(expected_counter, shared_counter, "共享计数器值错误，可能存在并发问题");
    ASSERT_EQ(THREAD_COUNT, test_complete, "不是所有线程都正常完成");

    // 清理
    for (int i = 0; i < THREAD_COUNT; i++)
    {
        kmt->teardown(&tasks[i]);
    }

    printf("多线程测试完成，最终计数器值: %d\n", shared_counter);
}

// 自旋锁测试
static void test_kmt_spinlock(void)
{
    spinlock_t lock;
    int test_var = 0;

    printf("测试自旋锁...\n");

    // 初始化锁
    kmt->spin_init(&lock, "spinlock-test");

    // 获取锁
    printf("获取锁...\n");
    kmt->spin_lock(&lock);

    // 临界区
    test_var++;
    ASSERT_EQ(1, test_var, "临界区变量值错误");

    // 释放锁
    printf("释放锁...\n");
    kmt->spin_unlock(&lock);

    // 测试重入
    printf("测试锁重入...\n");
    kmt->spin_lock(&lock);
    test_var++;
    ASSERT_EQ(2, test_var, "锁重入后临界区变量值错误");
    kmt->spin_unlock(&lock);

    printf("自旋锁测试完成\n");
}

// 信号量测试
static void test_kmt_semaphore(void)
{
    sem_t sem;

    printf("测试信号量...\n");

    // 初始化信号量（初值为1）
    kmt->sem_init(&sem, "sem-test", 1);

    // 获取信号量
    printf("等待信号量...\n");
    kmt->sem_wait(&sem);
    printf("获得信号量\n");

    // 临界区
    printf("执行临界区代码\n");

    // 释放信号量
    printf("释放信号量...\n");
    kmt->sem_signal(&sem);

    // 测试信号量为零的情况
    printf("测试零值信号量...\n");
    kmt->sem_wait(&sem); // 第一次等待，信号量为0

    // 在另一个线程中发送信号
    task_t signal_task;

    static void signal_thread(void *arg)
    {
        sem_t *s = (sem_t *)arg;
        printf("信号线程开始运行\n");

        // 等待一下再发信号
        for (int i = 0; i < 5; i++)
        {
            yield();
        }

        printf("信号线程发送信号\n");
        kmt->sem_signal(s);
    }

    // 创建发送信号的线程
    int result = kmt->create(&signal_task, "signal-thread", signal_thread, &sem);
    ASSERT_EQ(0, result, "信号线程创建失败");

    printf("主线程等待信号量（应该会阻塞）...\n");
    kmt->sem_wait(&sem); // 这里会阻塞，直到signal_thread发送信号

    printf("主线程被唤醒，信号量测试完成\n");

    // 清理
    kmt->teardown(&signal_task);
}

// 注册KMT测试套件
void kmt_test_register(void)
{
    test_suite_t *suite = test_suite_create("线程管理(KMT)测试");

    test_suite_add_case(suite, "线程基本操作", kmt_setup, test_kmt_thread_basic, kmt_teardown);
    test_suite_add_case(suite, "多线程测试", kmt_setup, test_kmt_thread_multiple, kmt_teardown);
    test_suite_add_case(suite, "自旋锁测试", kmt_setup, test_kmt_spinlock, kmt_teardown);
    test_suite_add_case(suite, "信号量测试", kmt_setup, test_kmt_semaphore, kmt_teardown);

    test_register_suite(suite);
}