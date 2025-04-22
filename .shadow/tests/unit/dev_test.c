#include "../include/test_framework.h"
#include <os.h>
#include <devices.h>

// 测试前准备
static void dev_setup(void)
{
    // 确保设备管理模块已初始化
    dev->init();
    printf("DEV模块已初始化\n");
}

// 测试后清理
static void dev_teardown(void)
{
    // 在这里可以添加清理代码，如果需要的话
}

// 设备查询测试
static void test_dev_lookup(void)
{
    // 测试查找已知设备
    const char *known_devices[] = {
        "input", "fb", "tty1", "tty2", "sda"};

    for (int i = 0; i < sizeof(known_devices) / sizeof(known_devices[0]); i++)
    {
        device_t *device = dev->lookup(known_devices[i]);
        printf("查找设备 '%s': ", known_devices[i]);

        if (device)
        {
            printf("找到，ID=%d\n", device->id);
            ASSERT_NOT_NULL(device->ops, "设备操作表为空");
            ASSERT_NOT_NULL(device->ptr, "设备数据指针为空");
        }
        else
        {
            printf("未找到\n");
            // 不断言失败，因为有些设备可能在某些配置中不存在
        }
    }

    // 测试查找不存在的设备
    printf("测试查找不存在的设备: ");
    // 实际上，当前dev->lookup实现会在设备不存在时panic，不会返回NULL
    // 因此这里只能打印一条信息，不能实际测试
    printf("跳过（当前实现会panic）\n");
}

// 输入设备测试
static void test_dev_input(void)
{
    printf("测试输入设备...\n");

    device_t *input = dev->lookup("input");
    if (!input)
    {
        printf("输入设备不可用，跳过测试\n");
        return;
    }

    // 尝试从输入设备读取数据
    struct input_event event;
    int size = input->ops->read(input, 0, &event, sizeof(event));

    if (size > 0)
    {
        printf("从输入设备读取到事件: ");
        printf("ctrl=%d, alt=%d, data=0x%x\n",
               event.ctrl, event.alt, event.data);
    }
    else
    {
        printf("没有可用的输入事件\n");
    }

    // 尝试写入（通常会被忽略）
    size = input->ops->write(input, 0, "test", 4);
    printf("向输入设备写入: 返回 %d\n", size);
}

// TTY设备测试
static void test_dev_tty(void)
{
    printf("测试TTY设备...\n");

    device_t *tty = dev->lookup("tty1");
    if (!tty)
    {
        printf("TTY设备不可用，跳过测试\n");
        return;
    }

    // 向TTY写入一些文本
    const char *test_text = "TTY测试文本\n";
    int size = tty->ops->write(tty, 0, test_text, strlen(test_text));
    printf("向TTY写入 %d 字节\n", size);

    // 尝试从TTY读取
    char buffer[128] = {0};
    size = tty->ops->read(tty, 0, buffer, sizeof(buffer) - 1);

    if (size > 0)
    {
        buffer[size] = '\0'; // 确保以NULL结尾
        printf("从TTY读取: '%s'\n", buffer);
    }
    else
    {
        printf("TTY没有可读数据\n");
    }
}

// 帧缓冲设备测试
static void test_dev_fb(void)
{
    printf("测试帧缓冲设备...\n");

    device_t *fb = dev->lookup("fb");
    if (!fb)
    {
        printf("帧缓冲设备不可用，跳过测试\n");
        return;
    }

    // 获取帧缓冲信息
    fb_t *fb_info = fb->ptr;
    printf("帧缓冲信息:\n");
    printf("  宽度: %d\n", fb_info->info->width);
    printf("  高度: %d\n", fb_info->info->height);
    printf("  颜色深度: %d\n", fb_info->info->depth);

    // 在帧缓冲中绘制一个简单的矩形
    struct display_info draw_cmd;
    draw_cmd.current = 0; // 使用默认显示

    int result = fb->ops->write(fb, 0, &draw_cmd, sizeof(draw_cmd));
    printf("帧缓冲绘制命令返回: %d\n", result);
}

// 存储设备测试
static void test_dev_storage(void)
{
    printf("测试存储设备...\n");

    device_t *sd = dev->lookup("sda");
    if (!sd)
    {
        printf("存储设备不可用，跳过测试\n");
        return;
    }

    // 准备读写缓冲区
    char write_buf[512];
    char read_buf[512];

    memset(write_buf, 0xAB, sizeof(write_buf));
    memset(read_buf, 0, sizeof(read_buf));

    // 尝试写入数据（注意：真实测试可能会损坏数据，这里应该小心）
    printf("警告: 跳过存储设备写入测试，避免损坏数据\n");

    // 尝试读取数据
    int size = sd->ops->read(sd, 0, read_buf, sizeof(read_buf));
    printf("从存储设备读取 %d 字节\n", size);

    // 显示前几个字节
    if (size > 0)
    {
        printf("数据前16字节: ");
        for (int i = 0; i < 16 && i < size; i++)
        {
            printf("%02x ", (unsigned char)read_buf[i]);
        }
        printf("\n");
    }
}

// 设备并发访问测试
static void test_dev_concurrent(void)
{
#define CONCURRENT_TASKS 3
    task_t tasks[CONCURRENT_TASKS];

    printf("测试设备并发访问...\n");

    // 设备并发访问测试线程
    static void dev_thread(void *arg)
    {
        int id = (uintptr_t)arg;
        printf("设备测试线程 %d 启动\n", id);

        // 尝试访问TTY设备
        device_t *tty = dev->lookup("tty1");
        if (tty)
        {
            char buffer[64];
            sprintf(buffer, "来自线程 %d 的消息\n", id);
            tty->ops->write(tty, 0, buffer, strlen(buffer));
        }

        // 让线程运行一段时间
        for (int i = 0; i < 10; i++)
        {
            yield();
        }

        printf("设备测试线程 %d 结束\n", id);
    }

    // 创建并发线程
    for (int i = 0; i < CONCURRENT_TASKS; i++)
    {
        int result = kmt->create(&tasks[i], "dev-thread", dev_thread, (void *)(uintptr_t)(i + 1));
        ASSERT_EQ(0, result, "创建设备测试线程失败");
    }

    // 让线程有时间运行和完成
    for (int i = 0; i < 100; i++)
    {
        yield();
    }

    // 清理线程
    for (int i = 0; i < CONCURRENT_TASKS; i++)
    {
        kmt->teardown(&tasks[i]);
    }

    printf("设备并发访问测试完成\n");
}

// 注册DEV测试套件
void dev_test_register(void)
{
    test_suite_t *suite = test_suite_create("设备管理(DEV)测试");

    test_suite_add_case(suite, "设备查找测试", dev_setup, test_dev_lookup, dev_teardown);
    test_suite_add_case(suite, "输入设备测试", dev_setup, test_dev_input, dev_teardown);
    test_suite_add_case(suite, "TTY设备测试", dev_setup, test_dev_tty, dev_teardown);
    test_suite_add_case(suite, "帧缓冲测试", dev_setup, test_dev_fb, dev_teardown);
    test_suite_add_case(suite, "存储设备测试", dev_setup, test_dev_storage, dev_teardown);
    test_suite_add_case(suite, "设备并发访问测试", dev_setup, test_dev_concurrent, dev_teardown);

    test_register_suite(suite);
}