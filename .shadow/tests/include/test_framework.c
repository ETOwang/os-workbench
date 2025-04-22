#include "test_framework.h"

// 全局变量
test_case_t *current_test_case = NULL;
test_suite_t *first_suite = NULL;
test_suite_t *last_suite = NULL;

// 测试框架初始化
void test_init(void)
{
    first_suite = NULL;
    last_suite = NULL;
    current_test_case = NULL;
}

// 创建测试套件
test_suite_t *test_suite_create(const char *name)
{
    test_suite_t *suite = (test_suite_t *)malloc(sizeof(test_suite_t));
    if (!suite)
    {
        printf("测试套件创建失败：内存分配错误\n");
        return NULL;
    }

    suite->name = name;
    suite->suite_setup = NULL;
    suite->suite_teardown = NULL;
    suite->test_cases = NULL;
    suite->cases_count = 0;
    suite->pass_count = 0;
    suite->fail_count = 0;
    suite->skip_count = 0;
    suite->next = NULL;

    return suite;
}

// 向测试套件添加测试用例
void test_suite_add_case(test_suite_t *suite, const char *name, void (*setup)(void), void (*run)(void), void (*teardown)(void))
{
    if (!suite)
        return;

    test_case_t *test_case = (test_case_t *)malloc(sizeof(test_case_t));
    if (!test_case)
    {
        printf("测试用例创建失败：内存分配错误\n");
        return;
    }

    test_case->name = name;
    test_case->setup = setup;
    test_case->run = run;
    test_case->teardown = teardown;
    test_case->result = TEST_SKIP; // 初始状态为跳过
    test_case->failure_msg = NULL;
    test_case->next = NULL;

    // 添加到链表末尾
    if (!suite->test_cases)
    {
        suite->test_cases = test_case;
    }
    else
    {
        test_case_t *last = suite->test_cases;
        while (last->next)
        {
            last = last->next;
        }
        last->next = test_case;
    }

    suite->cases_count++;
}

// 设置测试套件的生命周期函数
void test_suite_set_lifecycle(test_suite_t *suite, void (*suite_setup)(void), void (*suite_teardown)(void))
{
    if (!suite)
        return;

    suite->suite_setup = suite_setup;
    suite->suite_teardown = suite_teardown;
}

// 注册测试套件
void test_register_suite(test_suite_t *suite)
{
    if (!suite)
        return;

    if (!first_suite)
    {
        first_suite = suite;
        last_suite = suite;
    }
    else
    {
        last_suite->next = suite;
        last_suite = suite;
    }
}

// 运行所有测试
test_report_t test_run_all(void)
{
    test_report_t report = {0};
    uint64_t start_time = get_time_ms();

    printf("\n===== 开始运行测试 =====\n\n");

    test_suite_t *suite = first_suite;
    while (suite)
    {
        printf("运行测试套件: %s\n", suite->name);

        // 执行套件设置
        if (suite->suite_setup)
        {
            suite->suite_setup();
        }

        // 运行套件中的所有测试
        test_case_t *test_case = suite->test_cases;
        while (test_case)
        {
            printf("  测试: %s ... ", test_case->name);

            // 设置当前测试用例
            current_test_case = test_case;
            test_case->result = TEST_RUNNING;

            // 执行测试设置
            if (test_case->setup)
            {
                test_case->setup();
            }

            // 执行测试
            if (test_case->run)
            {
                test_case->run();
            }

            // 执行测试清理
            if (test_case->teardown)
            {
                test_case->teardown();
            }

            // 如果测试仍在运行状态，则标记为通过
            if (test_case->result == TEST_RUNNING)
            {
                test_case->result = TEST_PASS;
            }

            // 更新计数
            switch (test_case->result)
            {
            case TEST_PASS:
                printf("通过\n");
                suite->pass_count++;
                break;
            case TEST_FAIL:
                // 失败消息已在断言中打印
                suite->fail_count++;
                break;
            case TEST_SKIP:
                printf("跳过\n");
                suite->skip_count++;
                break;
            default:
                break;
            }

            test_case = test_case->next;
        }

        // 执行套件清理
        if (suite->suite_teardown)
        {
            suite->suite_teardown();
        }

        // 打印套件摘要
        printf("\n套件 '%s' 摘要:\n", suite->name);
        printf("  总计: %d, 通过: %d, 失败: %d, 跳过: %d\n\n",
               suite->cases_count, suite->pass_count, suite->fail_count, suite->skip_count);

        // 更新报告
        report.total_suites++;
        report.total_cases += suite->cases_count;
        report.total_pass += suite->pass_count;
        report.total_fail += suite->fail_count;
        report.total_skip += suite->skip_count;

        suite = suite->next;
    }

    uint64_t end_time = get_time_ms();
    report.total_time_ms = (end_time - start_time); // 转换为毫秒

    return report;
}

// 打印测试报告
void test_print_report(test_report_t report)
{
    printf("===== 测试执行完毕 =====\n");
    printf("总测试套件数: %d\n", report.total_suites);
    printf("总测试用例数: %d\n", report.total_cases);
    printf("通过: %d\n", report.total_pass);
    printf("失败: %d\n", report.total_fail);
    printf("跳过: %d\n", report.total_skip);
    printf("总执行时间: %lu 毫秒\n\n", report.total_time_ms);

    if (report.total_fail == 0)
    {
        printf("所有测试均已通过！\n");
    }
    else
    {
        printf("测试失败！%d 个测试未通过。\n", report.total_fail);
    }
}

// 多线程支持
typedef struct
{
    test_suite_t *suite;
    int thread_id;
    int thread_count;
} thread_data_t;

// 线程本地存储的测试结果
typedef struct
{
    int pass_count;
    int fail_count;
    int skip_count;
} thread_result_t;

// 多线程测试的初始化
void test_init_concurrency(int thread_count)
{
    // 使用AM中的多处理器接口初始化
    iset(false);    // 关中断
    mpe_init(NULL); // 初始化多处理器环境，不设置入口点
    iset(true);     // 开中断

    printf("多处理器环境已初始化，CPU数量: %d (要求: %d)\n", cpu_count(), thread_count);
    assert(cpu_count() >= thread_count && "CPU数量不足，无法运行所有线程");
}

// 线程测试运行函数
static void *thread_run_tests(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    test_suite_t *suite = data->suite;
    int thread_id = data->thread_id;
    int thread_count = data->thread_count;

    thread_result_t result = {0};

    // 计算本线程应处理的测试用例范围
    int total_cases = suite->cases_count;
    int cases_per_thread = (total_cases + thread_count - 1) / thread_count;
    int start_idx = thread_id * cases_per_thread;
    int end_idx = start_idx + cases_per_thread;
    if (end_idx > total_cases)
        end_idx = total_cases;

    printf("线程 %d 运行测试 %d 到 %d\n", thread_id, start_idx, end_idx - 1);

    // 定位到起始测试用例
    test_case_t *test_case = suite->test_cases;
    for (int i = 0; i < start_idx && test_case; i++)
    {
        test_case = test_case->next;
    }

    // 运行分配的测试用例
    for (int i = start_idx; i < end_idx && test_case; i++)
    {
        printf("  线程 %d 测试: %s ... ", thread_id, test_case->name);

        // 设置当前测试用例
        current_test_case = test_case;
        test_case->result = TEST_RUNNING;

        // 执行测试设置
        if (test_case->setup)
        {
            test_case->setup();
        }

        // 执行测试
        if (test_case->run)
        {
            test_case->run();
        }

        // 执行测试清理
        if (test_case->teardown)
        {
            test_case->teardown();
        }

        // 如果测试仍在运行状态，则标记为通过
        if (test_case->result == TEST_RUNNING)
        {
            test_case->result = TEST_PASS;
        }

        // 更新计数
        switch (test_case->result)
        {
        case TEST_PASS:
            printf("通过\n");
            result.pass_count++;
            break;
        case TEST_FAIL:
            // 失败消息已在断言中打印
            result.fail_count++;
            break;
        case TEST_SKIP:
            printf("跳过\n");
            result.skip_count++;
            break;
        default:
            break;
        }

        test_case = test_case->next;
    }

    // 原子更新套件的结果计数
    __sync_fetch_and_add(&suite->pass_count, result.pass_count);
    __sync_fetch_and_add(&suite->fail_count, result.fail_count);
    __sync_fetch_and_add(&suite->skip_count, result.skip_count);

    return NULL;
}

// 并发运行测试套件
void test_run_concurrent(test_suite_t *suite, int thread_count)
{
    if (!suite)
        return;

    printf("\n===== 开始并发运行测试套件: %s =====\n\n", suite->name);

    // 执行套件设置
    if (suite->suite_setup)
    {
        suite->suite_setup();
    }

    // 清零套件结果计数
    suite->pass_count = 0;
    suite->fail_count = 0;
    suite->skip_count = 0;

    // 准备线程数据
    thread_data_t *thread_data = (thread_data_t *)malloc(sizeof(thread_data_t) * thread_count);
    if (!thread_data)
    {
        printf("线程数据创建失败：内存分配错误\n");
        return;
    }

    // 确保我们有足够的CPU来运行测试
    if (cpu_count() < thread_count)
    {
        printf("警告: 请求的线程数(%d)超过了可用CPU数(%d)，性能可能受影响\n",
               thread_count, cpu_count());
    }

    // 重置屏障计数器
    atomic_store(&barrier_counter, 0);
    atomic_store(&barrier_generation, 0);

    // 启动多个处理器运行测试
    printf("启动 %d 个线程运行测试...\n", thread_count);
    for (int i = 0; i < thread_count; i++)
    {
        thread_data[i].suite = suite;
        thread_data[i].thread_id = i;
        thread_data[i].thread_count = thread_count;

        if (i > 0)
        {
            // 在其他CPU核心上执行
            // 这里可以使用fork或其他AM提供的机制
            // 当前简单使用yield模拟，这在实际并行环境中需要更可靠的机制
            for (int j = 0; j < 10; j++)
            {
                yield();
            }
        }
    }

    // 主线程执行第一个分片的测试
    thread_run_tests(&thread_data[0]);

    // 使用同步屏障等待所有线程完成
    printf("主线程等待其他线程完成...\n");
    test_barrier_wait(thread_count);
    printf("所有线程已完成\n");

    // 释放资源
    free(thread_data);

    // 执行套件清理
    if (suite->suite_teardown)
    {
        suite->suite_teardown();
    }

    // 打印套件摘要
    printf("\n并发套件 '%s' 摘要:\n", suite->name);
    printf("  总计: %d, 通过: %d, 失败: %d, 跳过: %d\n\n",
           suite->cases_count, suite->pass_count, suite->fail_count, suite->skip_count);
}

// 同步屏障实现
static atomic_int barrier_counter = 0;
static atomic_int barrier_generation = 0;

void test_barrier_wait(int thread_count)
{
    int gen = atomic_load(&barrier_generation);

    if (atomic_fetch_add(&barrier_counter, 1) == thread_count - 1)
    {
        // 最后一个到达的线程
        atomic_store(&barrier_counter, 0);
        atomic_fetch_add(&barrier_generation, 1);
    }
    else
    {
        // 其他线程等待
        while (gen == atomic_load(&barrier_generation))
        {
            yield(); // 让出CPU时间
        }
    }
}

// 获取当前系统时间（毫秒）
static uint64_t get_time_ms(void)
{
    // 使用 AM 的时钟接口获取微秒级时间
    AM_TIMER_UPTIME_T uptime;
    ioe_read(AM_TIMER_UPTIME, &uptime);
    return uptime.us / 1000; // 将微秒转换为毫秒
}

// 清理测试框架资源，防止内存泄漏
void test_cleanup(void)
{
    test_suite_t *suite = first_suite;
    while (suite)
    {
        test_suite_t *next_suite = suite->next;

        // 清理所有测试用例
        test_case_t *test_case = suite->test_cases;
        while (test_case)
        {
            test_case_t *next_case = test_case->next;
            free(test_case);
            test_case = next_case;
        }

        free(suite);
        suite = next_suite;
    }

    first_suite = NULL;
    last_suite = NULL;
    current_test_case = NULL;

    printf("测试资源已清理完毕\n");
}

// 同步原语实现
void test_sem_init(test_semaphore_t *sem, const char *name, int value)
{
    sem->value = value;
    sem->name = name;
}

void test_sem_wait(test_semaphore_t *sem)
{
    while (1)
    {
        // 尝试获取信号量
        for (int i = 0; i < 1000; i++)
        {
            if (sem->value > 0)
            {
                int expected = sem->value;
                if (__sync_bool_compare_and_swap(&sem->value, expected, expected - 1))
                {
                    return;
                }
            }
            // 短暂自旋后让出CPU
            for (volatile int j = 0; j < 100; j++)
                ;
        }
        yield(); // 让出CPU
    }
}

void test_sem_signal(test_semaphore_t *sem)
{
    __sync_fetch_and_add(&sem->value, 1);
}

void test_spin_init(test_spinlock_t *lock, const char *name)
{
    atomic_store(&lock->lock, 0);
    lock->name = name;
}

void test_spin_lock(test_spinlock_t *lock)
{
    while (atomic_exchange(&lock->lock, 1) != 0)
    {
        for (volatile int i = 0; i < 100; i++)
            ;    // 短暂自旋
        yield(); // 让出CPU
    }
}

void test_spin_unlock(test_spinlock_t *lock)
{
    atomic_store(&lock->lock, 0);
}

// === 操作系统功能测试辅助函数 ===

// OS模块测试
void test_os_api(void)
{
    // 测试 os->init() 是否正确初始化
    printf("测试OS初始化...\n");
    os->init();
    printf("OS初始化完成\n");

    // 测试 os->trap 是否正常工作
    printf("测试OS中断处理...\n");
    Context *ctx = NULL;
    Event ev = {
        .event = EVENT_YIELD,
        .cause = 0,
        .ref = 0,
        .msg = "测试事件"};
    ctx = os->trap(ev, ctx);
    printf("OS中断处理完成\n");

    // 测试处理器间通信
    printf("测试处理器间通信...\n");
    if (cpu_count() > 1)
    {
        printf("检测到%d个CPU核心\n", cpu_count());
        printf("当前在CPU %d上运行\n", cpu_current());
    }
    else
    {
        printf("单CPU环境，跳过处理器间通信测试\n");
    }
}

// PMM内存管理测试
void test_pmm_alloc_free(size_t size, int count)
{
    void *ptrs[1024]; // 最多跟踪1024个内存块
    int actual_count = count < 1024 ? count : 1024;

    printf("测试内存分配与释放 (大小: %zu字节, 次数: %d)...\n", size, actual_count);

    // 测试分配
    for (int i = 0; i < actual_count; i++)
    {
        ptrs[i] = pmm->alloc(size);
        if (!ptrs[i])
        {
            printf("第%d次内存分配失败\n", i + 1);
            actual_count = i;
            break;
        }

        // 简单写入测试以确认内存可用
        memset(ptrs[i], 0xAA, size);
    }

    printf("成功分配%d个内存块\n", actual_count);

    // 测试释放
    for (int i = 0; i < actual_count; i++)
    {
        pmm->free(ptrs[i]);
    }

    printf("内存释放完成\n");
}

// KMT线程管理测试
void test_kmt_create_teardown(void)
{
    static void test_thread_func(void *arg)
    {
        printf("线程函数被调用，参数: %p\n", arg);
        yield(); // 让出CPU
    }

    printf("测试线程创建与销毁...\n");

    // 创建测试线程
    task_t *task = pmm->alloc(sizeof(task_t));
    if (!task)
    {
        printf("为任务结构分配内存失败\n");
        return;
    }

    int result = kmt->create(task, "test-thread", test_thread_func, (void *)0x12345678);
    if (result == 0)
    {
        printf("线程创建成功\n");

        // 让线程有机会运行
        for (int i = 0; i < 10; i++)
        {
            yield();
        }

        // 销毁线程
        kmt->teardown(task);
        printf("线程销毁完成\n");

        // 释放任务结构
        pmm->free(task);
    }
    else
    {
        printf("线程创建失败，错误码: %d\n", result);
        pmm->free(task);
    }
}

// KMT同步原语测试
void test_kmt_synchronization(void)
{
    printf("测试同步原语...\n");

    // 测试自旋锁
    printf("测试自旋锁...\n");
    spinlock_t lock;
    kmt->spin_init(&lock, "test-spinlock");

    printf("获取锁...\n");
    kmt->spin_lock(&lock);
    printf("锁已获取\n");

    // 临界区操作
    printf("执行临界区代码\n");

    printf("释放锁...\n");
    kmt->spin_unlock(&lock);
    printf("锁已释放\n");

    // 测试信号量
    printf("\n测试信号量...\n");
    sem_t sem;
    kmt->sem_init(&sem, "test-semaphore", 1);

    printf("等待信号量...\n");
    kmt->sem_wait(&sem);
    printf("获得信号量\n");

    // 临界区操作
    printf("执行临界区代码\n");

    printf("释放信号量...\n");
    kmt->sem_signal(&sem);
    printf("信号量已释放\n");
}

// 设备访问测试
void test_device_access(const char *dev_name)
{
    printf("测试设备访问: %s...\n", dev_name);

    // 查找设备
    device_t *device = dev->lookup(dev_name);
    if (!device)
    {
        printf("找不到设备: %s\n", dev_name);
        return;
    }

    printf("找到设备: %s (ID: %d)\n", device->name, device->id);

    // 尝试对设备进行简单操作
    char buffer[64] = {0};

    // 读取
    int read_bytes = device->ops->read(device, 0, buffer, sizeof(buffer));
    printf("从设备读取 %d 字节\n", read_bytes);

    // 写入
    const char *test_data = "测试数据";
    int write_bytes = device->ops->write(device, 0, test_data, strlen(test_data));
    printf("向设备写入 %d 字节\n", write_bytes);
}

// 中断处理程序测试
void test_interrupt_handler(void)
{
    static Context *test_handler(Event ev, Context * ctx)
    {
        printf("测试中断处理程序被调用，事件类型: %d\n", ev.event);
        return ctx;
    }

    printf("注册中断处理程序...\n");

    // 注册定时器中断处理程序
    os->on_irq(0, EVENT_IRQ_TIMER, test_handler);
    printf("定时器中断处理程序已注册\n");

    // 注册设备中断处理程序
    os->on_irq(1, EVENT_IRQ_IODEV, test_handler);
    printf("设备中断处理程序已注册\n");

    // 等待一段时间以触发中断
    printf("等待中断触发...\n");
    for (int i = 0; i < 100; i++)
    {
        yield(); // 让出CPU以便触发定时器中断
    }
    printf("中断处理测试完成\n");
}