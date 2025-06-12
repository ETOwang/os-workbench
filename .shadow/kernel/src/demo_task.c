#include <common.h>

// 演示程序的全局状态
static struct
{
    int demo_mode;
    int animation_frame;
    uint64_t start_time;
    device_t *fb_dev;
    device_t *input_dev;
    device_t *disk_dev;
    device_t *tty1_dev;
    device_t *tty2_dev;
    struct display_info display_info;
    int active_tasks;
    int memory_usage;
    int running;
} demo_state;

// 颜色定义
#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_CYAN 0x00FFFF
#define COLOR_MAGENTA 0xFF00FF
#define COLOR_WHITE 0xFFFFFF
#define COLOR_BLACK 0x000000

// 演示模式
#define DEMO_SYSTEM_INFO 0
#define DEMO_GRAPHICS 1
#define DEMO_MULTITASK 2
#define DEMO_DEVICE_TEST 3
#define DEMO_MEMORY_TEST 4
#define DEMO_MAX 5

// 系统信息显示
void demo_system_info()
{
    printf("\n=== 🚀 内核系统演示程序 🚀 ===\n");
    printf("CPU数量: %d\n", cpu_count());
    printf("当前CPU: %d\n", cpu_current());

    // 显示设备信息
    printf("\n=== 📱 设备信息 ===\n");
    if (demo_state.fb_dev)
    {
        printf("🖥️  显示设备: %s (分辨率: %dx%d)\n",
               demo_state.fb_dev->name,
               demo_state.display_info.width,
               demo_state.display_info.height);
    }
    if (demo_state.disk_dev)
    {
        sd_t *sd = demo_state.disk_dev->ptr;
        if (sd)
        {
            printf("💾 磁盘设备: %s (块数: %d, 块大小: %d)\n",
                   demo_state.disk_dev->name, sd->blkcnt, sd->blksz);
        }
    }

    // 显示内存信息
    printf("\n=== 🧠 内存信息 ===\n");
    printf("活跃任务数: %d\n", demo_state.active_tasks);
    printf("内存使用情况: %d KB\n", demo_state.memory_usage);

    // 显示运行时间
    uint64_t current_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t runtime = (current_time - demo_state.start_time) / 1000000;
    printf("⏰ 系统运行时间: %llu 秒\n", runtime);
}

// 图形演示 - 创建动画效果
void demo_graphics()
{
    if (!demo_state.fb_dev)
        return;

    fb_t *fb = demo_state.fb_dev->ptr;
    if (!fb || !fb->textures)
        return;

    printf("\n=== 🎨 图形演示 ===\n");
    printf("正在创建动画效果...\n");

    // 创建彩色纹理
    for (int i = 0; i < 8 && i < fb->info->num_textures; i++)
    {
        uint32_t color = 0;
        switch (i)
        {
        case 0:
            color = COLOR_RED;
            break;
        case 1:
            color = COLOR_GREEN;
            break;
        case 2:
            color = COLOR_BLUE;
            break;
        case 3:
            color = COLOR_YELLOW;
            break;
        case 4:
            color = COLOR_CYAN;
            break;
        case 5:
            color = COLOR_MAGENTA;
            break;
        case 6:
            color = COLOR_WHITE;
            break;
        case 7:
            color = COLOR_BLACK;
            break;
        }

        for (int j = 0; j < TEXTURE_W * TEXTURE_H; j++)
        {
            fb->textures[i].pixels[j] = color;
        }
    }

    // 写入纹理数据
    demo_state.fb_dev->ops->write(demo_state.fb_dev,
                                  sizeof(struct texture),
                                  fb->textures,
                                  sizeof(struct texture) * 8);

    // 创建动画精灵
    struct sprite sprites[16];
    for (int i = 0; i < 16; i++)
    {
        sprites[i] = (struct sprite){
            .texture = (i % 8) + 1,
            .x = (demo_state.animation_frame * 2 + i * 40) % demo_state.display_info.width,
            .y = (demo_state.animation_frame + i * 30) % demo_state.display_info.height,
            .display = 0,
            .z = i};
    }

    // 写入精灵数据
    demo_state.fb_dev->ops->write(demo_state.fb_dev,
                                  SPRITE_BRK,
                                  sprites,
                                  sizeof(sprites));

    demo_state.animation_frame = (demo_state.animation_frame + 1) % 200;
    printf("🎬 动画帧: %d\n", demo_state.animation_frame);
}

// 设备测试
void demo_device_test()
{
    printf("\n=== 🔧 设备测试 ===\n");

    // 测试磁盘读写
    if (demo_state.disk_dev)
    {
        printf("📀 测试磁盘读写...\n");
        char test_data[] = "Hello from kernel demo!";
        char read_buffer[64] = {0};

        // 写入测试数据
        int write_result = demo_state.disk_dev->ops->write(demo_state.disk_dev, 0, test_data, sizeof(test_data));
        printf("写入结果: %d 字节\n", write_result);

        // 读取测试数据
        int read_result = demo_state.disk_dev->ops->read(demo_state.disk_dev, 0, read_buffer, sizeof(test_data));
        printf("读取结果: %d 字节, 内容: %s\n", read_result, read_buffer);
    }

    // 测试输入设备
    if (demo_state.input_dev)
    {
        printf("⌨️  输入设备状态: 就绪\n");
        printf("按任意键进行输入测试...\n");
    }

    // 测试TTY设备
    if (demo_state.tty1_dev && demo_state.tty2_dev)
    {
        printf("🖥️  TTY设备测试\n");
        char tty_msg[] = "Demo message from kernel!\n";
        demo_state.tty1_dev->ops->write(demo_state.tty1_dev, 0, tty_msg, sizeof(tty_msg));
        printf("已向TTY1发送消息\n");
    }
}

// 内存测试
void demo_memory_test()
{
    printf("\n=== 🧪 内存管理测试 ===\n");

    // 分配不同大小的内存块
    void *ptrs[10];
    size_t sizes[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

    printf("分配内存块...\n");
    for (int i = 0; i < 10; i++)
    {
        ptrs[i] = pmm->alloc(sizes[i]);
        if (ptrs[i])
        {
            printf("✅ 分配 %zu 字节 @ %p\n", sizes[i], ptrs[i]);
            // 写入测试数据
            memset(ptrs[i], 0xAA + i, sizes[i]);
        }
        else
        {
            printf("❌ 分配 %zu 字节失败\n", sizes[i]);
        }
    }

    printf("\n验证内存内容...\n");
    for (int i = 0; i < 10; i++)
    {
        if (ptrs[i])
        {
            uint8_t *data = (uint8_t *)ptrs[i];
            bool valid = true;
            for (size_t j = 0; j < sizes[i]; j++)
            {
                if (data[j] != (uint8_t)(0xAA + i))
                {
                    valid = false;
                    break;
                }
            }
            printf("%s 内存块 %d: %s\n", valid ? "✅" : "❌", i, valid ? "数据正确" : "数据损坏");
        }
    }

    printf("\n释放内存块...\n");
    for (int i = 0; i < 10; i++)
    {
        if (ptrs[i])
        {
            pmm->free(ptrs[i]);
            printf("🗑️  释放内存块 %d\n", i);
        }
    }

    demo_state.memory_usage += 1024; // 模拟内存使用统计
}

// 多任务演示子任务
void demo_worker_task(void *arg)
{
    int task_id = (int)(uintptr_t)arg;
    printf("🔄 工作任务 %d 启动\n", task_id);

    for (int i = 0; i < 10; i++)
    {
        printf("📋 任务 %d 执行步骤 %d\n", task_id, i + 1);

        // 模拟工作负载
        volatile int sum = 0;
        for (int j = 0; j < 100000; j++)
        {
            sum += j;
        }

        // 让出CPU时间
        yield();
    }

    printf("✅ 工作任务 %d 完成\n", task_id);
    demo_state.active_tasks--;
}

// 多任务演示
void demo_multitask()
{
    printf("\n=== ⚡ 多任务演示 ===\n");
    printf("创建多个并发任务...\n");

    // 创建多个工作任务
    for (int i = 0; i < 3; i++)
    {
        task_t *task = pmm->alloc(sizeof(task_t));
        if (task)
        {
            char task_name[32];
            sprintf(task_name, "worker-%d", i);

            if (kmt->create(task, task_name, demo_worker_task, (void *)(uintptr_t)i) == 0)
            {
                printf("🚀 创建任务: %s\n", task_name);
                demo_state.active_tasks++;
            }
            else
            {
                printf("❌ 创建任务失败: %s\n", task_name);
                pmm->free(task);
            }
        }
    }

    printf("等待任务完成...\n");
    // 等待一段时间让任务执行
    for (int i = 0; i < 1000000; i++)
    {
        yield();
    }
}

// 实时系统监控任务
void demo_monitor_task(void *arg)
{
    printf("📊 系统监控任务启动\n");

    while (demo_state.running)
    {
        printf("\n=== 📈 实时系统状态 ===\n");

        // 显示CPU使用情况
        printf("🖥️  CPU: %d/%d 核心活跃\n", cpu_current() + 1, cpu_count());

        // 显示任务状态
        printf("📋 活跃任务: %d\n", demo_state.active_tasks);

        // 显示内存使用
        printf("🧠 内存使用: %d KB\n", demo_state.memory_usage);

        // 显示运行时间
        uint64_t current_time = io_read(AM_TIMER_UPTIME).us;
        uint64_t runtime = (current_time - demo_state.start_time) / 1000000;
        printf("⏰ 运行时间: %llu 秒\n", runtime);

        // 显示当前演示模式
        const char *mode_names[] = {
            "系统信息", "图形演示", "多任务", "设备测试", "内存测试"};
        printf("🎯 当前模式: %s\n", mode_names[demo_state.demo_mode]);

        // 等待一段时间
        for (int i = 0; i < 5000000; i++)
        {
            yield();
        }
    }

    printf("📊 系统监控任务结束\n");
}

// 系统性能测试任务
void demo_performance_task(void *arg)
{
    printf("⚡ 系统性能测试任务启动\n");

    // CPU性能测试
    printf("🖥️  CPU性能测试...\n");
    uint64_t start_time = io_read(AM_TIMER_UPTIME).us;

    volatile long long sum = 0;
    for (int i = 0; i < 1000000; i++)
    {
        sum += i * i;
    }

    uint64_t end_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t cpu_time = end_time - start_time;
    printf("CPU计算耗时: %llu 微秒\n", cpu_time);

    // 内存带宽测试
    printf("🧠 内存带宽测试...\n");
    size_t test_size = 1024 * 1024; // 1MB
    char *src = pmm->alloc(test_size);
    char *dst = pmm->alloc(test_size);

    if (src && dst)
    {
        start_time = io_read(AM_TIMER_UPTIME).us;
        memcpy(dst, src, test_size);
        end_time = io_read(AM_TIMER_UPTIME).us;

        uint64_t mem_time = end_time - start_time;
        printf("内存拷贝耗时: %llu 微秒 (1MB)\n", mem_time);

        pmm->free(src);
        pmm->free(dst);
    }
    else
    {
        printf("❌ 内存分配失败\n");
    }

    // 任务切换性能测试
    printf("🔄 任务切换性能测试...\n");
    start_time = io_read(AM_TIMER_UPTIME).us;

    for (int i = 0; i < 1000; i++)
    {
        yield();
    }

    end_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t switch_time = end_time - start_time;
    printf("1000次任务切换耗时: %llu 微秒\n", switch_time);

    printf("⚡ 系统性能测试完成\n");
}

// 键盘交互任务
void demo_keyboard_task(void *arg)
{
    printf("⌨️  键盘交互任务启动\n");
    printf("按键说明:\n");
    printf("  1-5: 切换演示模式\n");
    printf("  ESC: 退出演示\n");
    printf("  SPACE: 暂停/继续\n");

    while (demo_state.running)
    {
        if (demo_state.input_dev)
        {
            struct input_event ev;
            int nread = demo_state.input_dev->ops->read(demo_state.input_dev, 0, &ev, sizeof(ev));

            if (nread > 0 && ev.data)
            {
                printf("🔤 按键: %c (0x%x)\n", ev.data, ev.data);

                switch (ev.data)
                {
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                    demo_state.demo_mode = ev.data - '1';
                    printf("🔄 切换到模式: %d\n", demo_state.demo_mode);
                    break;
                case 27: // ESC
                    printf("🛑 退出演示\n");
                    demo_state.running = 0;
                    break;
                case ' ': // SPACE
                    printf("⏸️  暂停/继续\n");
                    break;
                default:
                    printf("❓ 未知按键\n");
                    break;
                }
            }
        }

        yield();
    }

    printf("⌨️  键盘交互任务结束\n");
}

// 主演示任务
void kernel_demo_task(void *arg)
{
    printf("\n🎉 === 内核系统综合演示程序启动 === 🎉\n");
    printf("展示内核的各种功能和设备操作\n\n");

    // 初始化演示状态
    demo_state.demo_mode = 0;
    demo_state.animation_frame = 0;
    demo_state.start_time = io_read(AM_TIMER_UPTIME).us;
    demo_state.active_tasks = 1;   // 当前任务
    demo_state.memory_usage = 512; // 初始内存使用
    demo_state.running = 1;

    // 查找设备
    demo_state.fb_dev = dev->lookup("fb");
    demo_state.input_dev = dev->lookup("input");
    demo_state.disk_dev = dev->lookup("sda");
    demo_state.tty1_dev = dev->lookup("tty1");
    demo_state.tty2_dev = dev->lookup("tty2");

    // 获取显示信息
    if (demo_state.fb_dev)
    {
        demo_state.fb_dev->ops->read(demo_state.fb_dev, 0, &demo_state.display_info, sizeof(demo_state.display_info));
    }

    // 创建监控任务
    task_t *monitor_task = pmm->alloc(sizeof(task_t));
    if (monitor_task)
    {
        kmt->create(monitor_task, "demo-monitor", demo_monitor_task, NULL);
        demo_state.active_tasks++;
    }

    // 创建键盘交互任务
    task_t *keyboard_task = pmm->alloc(sizeof(task_t));
    if (keyboard_task)
    {
        kmt->create(keyboard_task, "demo-keyboard", demo_keyboard_task, NULL);
        demo_state.active_tasks++;
    }

    // 创建性能测试任务
    task_t *performance_task = pmm->alloc(sizeof(task_t));
    if (performance_task)
    {
        kmt->create(performance_task, "demo-performance", demo_performance_task, NULL);
        demo_state.active_tasks++;
    }

    printf("🚀 所有子任务已启动\n\n");

    // 主演示循环
    int cycle_count = 0;
    while (demo_state.running)
    {
        printf("\n🔄 === 演示周期 %d === 🔄\n", ++cycle_count);

        switch (demo_state.demo_mode)
        {
        case DEMO_SYSTEM_INFO:
            demo_system_info();
            break;
        case DEMO_GRAPHICS:
            demo_graphics();
            break;
        case DEMO_MULTITASK:
            demo_multitask();
            break;
        case DEMO_DEVICE_TEST:
            demo_device_test();
            break;
        case DEMO_MEMORY_TEST:
            demo_memory_test();
            break;
        }

        // 自动切换模式
        if (cycle_count % 20 == 0)
        {
            demo_state.demo_mode = (demo_state.demo_mode + 1) % DEMO_MAX;
            printf("🔄 自动切换到下一个演示模式\n");
        }

        // 等待一段时间
        for (int i = 0; i < 3000000; i++)
        {
            yield();
        }
    }

    printf("\n🎊 === 内核系统演示程序结束 === 🎊\n");
    printf("感谢观看！\n");
}
