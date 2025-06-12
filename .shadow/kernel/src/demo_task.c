#include <common.h>

// Demo program global state
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

// Color definitions
#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_BLUE 0x0000FF
#define COLOR_YELLOW 0xFFFF00
#define COLOR_CYAN 0x00FFFF
#define COLOR_MAGENTA 0xFF00FF
#define COLOR_WHITE 0xFFFFFF
#define COLOR_BLACK 0x000000

// Demo modes
#define DEMO_SYSTEM_INFO 0
#define DEMO_GRAPHICS 1
#define DEMO_MULTITASK 2
#define DEMO_DEVICE_TEST 3
#define DEMO_MEMORY_TEST 4
#define DEMO_MAX 5

// System information display
void demo_system_info()
{
    printf("\n=== Kernel System Demo Program ===\n");
    printf("CPU Count: %d\n", cpu_count());
    printf("Current CPU: %d\n", cpu_current());

    // Display device information
    printf("\n=== Device Information ===\n");
    if (demo_state.fb_dev)
    {
        printf("Display Device: %s (Resolution: %dx%d)\n",
               demo_state.fb_dev->name,
               demo_state.display_info.width,
               demo_state.display_info.height);
    }
    if (demo_state.disk_dev)
    {
        sd_t *sd = demo_state.disk_dev->ptr;
        if (sd)
        {
            printf("Disk Device: %s (Blocks: %d, Block Size: %d)\n",
                   demo_state.disk_dev->name, sd->blkcnt, sd->blksz);
        }
    }

    // Display memory information
    printf("\n=== Memory Information ===\n");
    printf("Active Tasks: %d\n", demo_state.active_tasks);
    printf("Memory Usage: %d KB\n", demo_state.memory_usage);

    // Display runtime
    uint64_t current_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t runtime = (current_time - demo_state.start_time) / 1000000;
    printf("System Runtime: %llu seconds\n", runtime);
}

// Graphics demo - create animation effects
void demo_graphics()
{
    if (!demo_state.fb_dev)
        return;

    fb_t *fb = demo_state.fb_dev->ptr;
    if (!fb || !fb->textures)
        return;

    printf("\n=== Graphics Demo ===\n");
    printf("Creating animation effects...\n");

    // Create colorful textures
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

    // Write texture data
    demo_state.fb_dev->ops->write(demo_state.fb_dev,
                                  sizeof(struct texture),
                                  fb->textures,
                                  sizeof(struct texture) * 8);

    // Create animated sprites
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

    // Write sprite data
    demo_state.fb_dev->ops->write(demo_state.fb_dev,
                                  SPRITE_BRK,
                                  sprites,
                                  sizeof(sprites));

    demo_state.animation_frame = (demo_state.animation_frame + 1) % 200;
    printf("Animation Frame: %d\n", demo_state.animation_frame);
}

// Device testing
void demo_device_test()
{
    printf("\n=== Device Testing ===\n");

    // Test disk read/write
    if (demo_state.disk_dev)
    {
        printf("Testing disk read/write...\n");
        char test_data[] = "Hello from kernel demo!";
        char read_buffer[64] = {0};

        // Write test data
        int write_result = demo_state.disk_dev->ops->write(demo_state.disk_dev, 0, test_data, sizeof(test_data));
        printf("Write result: %d bytes\n", write_result);

        // Read test data
        int read_result = demo_state.disk_dev->ops->read(demo_state.disk_dev, 0, read_buffer, sizeof(test_data));
        printf("Read result: %d bytes, content: %s\n", read_result, read_buffer);
    }

    // Test input device
    if (demo_state.input_dev)
    {
        printf("Input device status: Ready\n");
        printf("Press any key for input test...\n");
    }

    // Test TTY device
    if (demo_state.tty1_dev && demo_state.tty2_dev)
    {
        printf("TTY device test\n");
        char tty_msg[] = "Demo message from kernel!\n";
        demo_state.tty1_dev->ops->write(demo_state.tty1_dev, 0, tty_msg, sizeof(tty_msg));
        printf("Message sent to TTY1\n");
    }
}

// Memory testing
void demo_memory_test()
{
    printf("\n=== Memory Management Test ===\n");

    // Allocate different sized memory blocks
    void *ptrs[10];
    size_t sizes[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

    printf("Allocating memory blocks...\n");
    for (int i = 0; i < 10; i++)
    {
        ptrs[i] = pmm->alloc(sizes[i]);
        if (ptrs[i])
        {
            printf("Allocated %zu bytes @ %p\n", sizes[i], ptrs[i]);
            // Write test data
            memset(ptrs[i], 0xAA + i, sizes[i]);
        }
        else
        {
            printf("Failed to allocate %zu bytes\n", sizes[i]);
        }
    }

    printf("\nVerifying memory content...\n");
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
            printf("Memory block %d: %s\n", i, valid ? "Valid" : "Corrupted");
        }
    }

    printf("\nFreeing memory blocks...\n");
    for (int i = 0; i < 10; i++)
    {
        if (ptrs[i])
        {
            pmm->free(ptrs[i]);
            printf("Freed memory block %d\n", i);
        }
    }

    demo_state.memory_usage += 1024; // Simulate memory usage statistics
}

// Multi-task demo worker task
void demo_worker_task(void *arg)
{
    int task_id = (int)(uintptr_t)arg;
    printf("Worker task %d started\n", task_id);

    for (int i = 0; i < 10; i++)
    {
        printf("Task %d executing step %d\n", task_id, i + 1);

        // Simulate workload
        volatile int sum = 0;
        for (int j = 0; j < 100000; j++)
        {
            sum += j;
        }

        // Yield CPU time
        yield();
    }

    printf("Worker task %d completed\n", task_id);
    demo_state.active_tasks--;
}

// Multi-task demonstration
void demo_multitask()
{
    printf("\n=== Multi-task Demo ===\n");
    printf("Creating multiple concurrent tasks...\n");

    // Create multiple worker tasks
    for (int i = 0; i < 3; i++)
    {
        task_t *task = pmm->alloc(sizeof(task_t));
        if (task)
        {
            char task_name[32];
            sprintf(task_name, "worker-%d", i);

            if (kmt->create(task, task_name, demo_worker_task, (void *)(uintptr_t)i) == 0)
            {
                printf("Created task: %s\n", task_name);
                demo_state.active_tasks++;
            }
            else
            {
                printf("Failed to create task: %s\n", task_name);
                pmm->free(task);
            }
        }
    }

    printf("Waiting for tasks to complete...\n");
    // Wait for tasks to execute
    for (int i = 0; i < 1000000; i++)
    {
        yield();
    }
}

// Real-time system monitoring task
void demo_monitor_task(void *arg)
{
    printf("System monitor task started\n");

    while (demo_state.running)
    {
        printf("\n=== Real-time System Status ===\n");

        // Display CPU usage
        printf("CPU: %d/%d cores active\n", cpu_current() + 1, cpu_count());

        // Display task status
        printf("Active tasks: %d\n", demo_state.active_tasks);

        // Display memory usage
        printf("Memory usage: %d KB\n", demo_state.memory_usage);

        // Display runtime
        uint64_t current_time = io_read(AM_TIMER_UPTIME).us;
        uint64_t runtime = (current_time - demo_state.start_time) / 1000000;
        printf("Runtime: %llu seconds\n", runtime);

        // Display current demo mode
        const char *mode_names[] = {
            "System Info", "Graphics", "Multi-task", "Device Test", "Memory Test"};
        printf("Current mode: %s\n", mode_names[demo_state.demo_mode]);

        // Wait for a while
        for (int i = 0; i < 5000000; i++)
        {
            yield();
        }
    }

    printf("System monitor task ended\n");
}

// System performance test task
void demo_performance_task(void *arg)
{
    printf("System performance test task started\n");

    // CPU performance test
    printf("CPU performance test...\n");
    uint64_t start_time = io_read(AM_TIMER_UPTIME).us;

    volatile long long sum = 0;
    for (int i = 0; i < 1000000; i++)
    {
        sum += i * i;
    }

    uint64_t end_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t cpu_time = end_time - start_time;
    printf("CPU computation time: %llu microseconds\n", cpu_time);

    // Memory bandwidth test
    printf("Memory bandwidth test...\n");
    size_t test_size = 1024 * 1024; // 1MB
    char *src = pmm->alloc(test_size);
    char *dst = pmm->alloc(test_size);

    if (src && dst)
    {
        start_time = io_read(AM_TIMER_UPTIME).us;
        memcpy(dst, src, test_size);
        end_time = io_read(AM_TIMER_UPTIME).us;

        uint64_t mem_time = end_time - start_time;
        printf("Memory copy time: %llu microseconds (1MB)\n", mem_time);

        pmm->free(src);
        pmm->free(dst);
    }
    else
    {
        printf("Memory allocation failed\n");
    }

    // Task switching performance test
    printf("Task switching performance test...\n");
    start_time = io_read(AM_TIMER_UPTIME).us;

    for (int i = 0; i < 1000; i++)
    {
        yield();
    }

    end_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t switch_time = end_time - start_time;
    printf("1000 task switches time: %llu microseconds\n", switch_time);

    printf("System performance test completed\n");
}

// Keyboard interaction task
void demo_keyboard_task(void *arg)
{
    printf("Keyboard interaction task started\n");
    printf("Key instructions:\n");
    printf("  1-5: Switch demo mode\n");
    printf("  ESC: Exit demo\n");
    printf("  SPACE: Pause/Resume\n");

    while (demo_state.running)
    {
        if (demo_state.input_dev)
        {
            struct input_event ev;
            int nread = demo_state.input_dev->ops->read(demo_state.input_dev, 0, &ev, sizeof(ev));

            if (nread > 0 && ev.data)
            {
                printf("Key pressed: %c (0x%x)\n", ev.data, ev.data);

                switch (ev.data)
                {
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                    demo_state.demo_mode = ev.data - '1';
                    printf("Switched to mode: %d\n", demo_state.demo_mode);
                    break;
                case 27: // ESC
                    printf("Exiting demo\n");
                    demo_state.running = 0;
                    break;
                case ' ': // SPACE
                    printf("Pause/Resume\n");
                    break;
                default:
                    printf("Unknown key\n");
                    break;
                }
            }
        }

        yield();
    }

    printf("Keyboard interaction task ended\n");
}

// Main demo task
void kernel_demo_task(void *arg)
{
    printf("\n=== Kernel System Comprehensive Demo Program Started ===\n");
    printf("Demonstrating various kernel functions and device operations\n\n");

    // Initialize demo state
    demo_state.demo_mode = 0;
    demo_state.animation_frame = 0;
    demo_state.start_time = io_read(AM_TIMER_UPTIME).us;
    demo_state.active_tasks = 1;   // Current task
    demo_state.memory_usage = 512; // Initial memory usage
    demo_state.running = 1;

    // Find devices
    demo_state.fb_dev = dev->lookup("fb");
    demo_state.input_dev = dev->lookup("input");
    demo_state.disk_dev = dev->lookup("sda");
    demo_state.tty1_dev = dev->lookup("tty1");
    demo_state.tty2_dev = dev->lookup("tty2");

    // Get display information
    if (demo_state.fb_dev)
    {
        demo_state.fb_dev->ops->read(demo_state.fb_dev, 0, &demo_state.display_info, sizeof(demo_state.display_info));
    }

    // Create monitor task
    task_t *monitor_task = pmm->alloc(sizeof(task_t));
    if (monitor_task)
    {
        kmt->create(monitor_task, "demo-monitor", demo_monitor_task, NULL);
        demo_state.active_tasks++;
    }

    // Create keyboard interaction task
    task_t *keyboard_task = pmm->alloc(sizeof(task_t));
    if (keyboard_task)
    {
        kmt->create(keyboard_task, "demo-keyboard", demo_keyboard_task, NULL);
        demo_state.active_tasks++;
    }

    // Create performance test task
    task_t *performance_task = pmm->alloc(sizeof(task_t));
    if (performance_task)
    {
        kmt->create(performance_task, "demo-performance", demo_performance_task, NULL);
        demo_state.active_tasks++;
    }

    printf("All sub-tasks started\n\n");

    // Main demo loop
    int cycle_count = 0;
    while (demo_state.running)
    {
        printf("\n=== Demo Cycle %d ===\n", ++cycle_count);

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

        // Auto switch mode
        if (cycle_count % 20 == 0)
        {
            demo_state.demo_mode = (demo_state.demo_mode + 1) % DEMO_MAX;
            printf("Auto switching to next demo mode\n");
        }

        // Wait for a while
        for (int i = 0; i < 3000000; i++)
        {
            yield();
        }
    }

    printf("\n=== Kernel System Demo Program Ended ===\n");
    printf("Thank you for watching!\n");
}
