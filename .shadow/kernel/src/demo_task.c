#include <common.h>

// Simple strchr implementation
static char *my_strchr(const char *s, int c)
{
    while (*s != '\0')
    {
        if (*s == c)
        {
            return (char *)s;
        }
        s++;
    }
    return NULL;
}

// Demo shell global state
static struct
{
    uint64_t start_time;
    device_t *fb_dev;
    device_t *input_dev;
    device_t *disk_dev;
    struct display_info display_info;
    int active_tasks;
    int memory_usage;
} shell_state;

// Helper function to write string to TTY
static void tty_write_str(device_t *tty, const char *str)
{
    tty->ops->write(tty, 0, str, strlen(str));
}

// Helper function to write formatted string to TTY
static void tty_printf(device_t *tty, const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsprintf(buffer, format, args);
    va_end(args);
    tty->ops->write(tty, 0, buffer, len);
}

// Command: help - show available commands
static void cmd_help(device_t *tty, char *args)
{
    tty_write_str(tty, "Available commands:\n");
    tty_write_str(tty, "  help       - Show this help message\n");
    tty_write_str(tty, "  sysinfo    - Display system information\n");
    tty_write_str(tty, "  cpuinfo    - Show CPU information\n");
    tty_write_str(tty, "  meminfo    - Display memory information\n");
    tty_write_str(tty, "  devinfo    - Show device information\n");
    tty_write_str(tty, "  memtest    - Run memory allocation test\n");
    tty_write_str(tty, "  disktest   - Test disk read/write\n");
    tty_write_str(tty, "  perftest   - Run performance benchmark\n");
    tty_write_str(tty, "  taskinfo   - Show task information\n");
    tty_write_str(tty, "  graphics   - Test graphics display\n");
    tty_write_str(tty, "  uptime     - Show system uptime\n");
    tty_write_str(tty, "  clear      - Clear screen\n");
    tty_write_str(tty, "  exit       - Exit shell\n");
}

// Command: sysinfo - display system information
static void cmd_sysinfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== System Information ===\n");
    tty_printf(tty, "CPU Count: %d\n", cpu_count());
    tty_printf(tty, "Current CPU: %d\n", cpu_current());
    tty_printf(tty, "Active Tasks: %d\n", shell_state.active_tasks);
    tty_printf(tty, "Memory Usage: %d KB\n", shell_state.memory_usage);

    uint64_t current_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t runtime = (current_time - shell_state.start_time) / 1000000;
    tty_printf(tty, "System Runtime: %llu seconds\n", runtime);
}

// Command: cpuinfo - show CPU information
static void cmd_cpuinfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== CPU Information ===\n");
    tty_printf(tty, "Total CPUs: %d\n", cpu_count());
    tty_printf(tty, "Current CPU ID: %d\n", cpu_current());
    tty_write_str(tty, "Architecture: x86_64\n");
    tty_write_str(tty, "Features: Multi-core, Virtual Memory, Interrupts\n");
}

// Command: meminfo - display memory information
static void cmd_meminfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Memory Information ===\n");
    tty_printf(tty, "Current Usage: %d KB\n", shell_state.memory_usage);
    tty_write_str(tty, "Memory Manager: Buddy Allocator\n");
    tty_write_str(tty, "Page Size: 4KB\n");
    tty_write_str(tty, "Virtual Memory: Enabled\n");
}

// Command: devinfo - show device information
static void cmd_devinfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Device Information ===\n");

    if (shell_state.fb_dev)
    {
        tty_printf(tty, "Display: %s (Resolution: %dx%d)\n",
                   shell_state.fb_dev->name,
                   shell_state.display_info.width,
                   shell_state.display_info.height);
    }

    if (shell_state.disk_dev)
    {
        sd_t *sd = shell_state.disk_dev->ptr;
        if (sd)
        {
            tty_printf(tty, "Disk: %s (Blocks: %d, Block Size: %d)\n",
                       shell_state.disk_dev->name, sd->blkcnt, sd->blksz);
        }
    }

    if (shell_state.input_dev)
    {
        tty_printf(tty, "Input: %s (Status: Ready)\n", shell_state.input_dev->name);
    }

    tty_printf(tty, "TTY: %s (Current)\n", tty->name);
}

// Command: memtest - run memory allocation test
static void cmd_memtest(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Memory Allocation Test ===\n");

    size_t sizes[] = {64, 256, 1024, 4096, 16384};
    void *ptrs[5];

    tty_write_str(tty, "Allocating memory blocks...\n");
    for (int i = 0; i < 5; i++)
    {
        ptrs[i] = pmm->alloc(sizes[i]);
        if (ptrs[i])
        {
            tty_printf(tty, "  Allocated %zu bytes at %p\n", sizes[i], ptrs[i]);
            memset(ptrs[i], 0xAA + i, sizes[i]);
        }
        else
        {
            tty_printf(tty, "  Failed to allocate %zu bytes\n", sizes[i]);
        }
    }

    tty_write_str(tty, "Verifying memory content...\n");
    for (int i = 0; i < 5; i++)
    {
        if (ptrs[i])
        {
            uint8_t *data = (uint8_t *)ptrs[i];
            bool valid = (data[0] == (uint8_t)(0xAA + i));
            tty_printf(tty, "  Block %d: %s\n", i, valid ? "Valid" : "Corrupted");
        }
    }

    tty_write_str(tty, "Freeing memory blocks...\n");
    for (int i = 0; i < 5; i++)
    {
        if (ptrs[i])
        {
            pmm->free(ptrs[i]);
            tty_printf(tty, "  Freed block %d\n", i);
        }
    }

    tty_write_str(tty, "Memory test completed.\n");
}

// Command: disktest - test disk read/write
static void cmd_disktest(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Disk Read/Write Test ===\n");

    if (!shell_state.disk_dev)
    {
        tty_write_str(tty, "No disk device available.\n");
        return;
    }

    char test_data[] = "Hello from kernel shell!";
    char read_buffer[64] = {0};

    tty_write_str(tty, "Writing test data to disk...\n");
    int write_result = shell_state.disk_dev->ops->write(shell_state.disk_dev, 0, test_data, sizeof(test_data));
    tty_printf(tty, "Write result: %d bytes\n", write_result);

    tty_write_str(tty, "Reading data from disk...\n");
    int read_result = shell_state.disk_dev->ops->read(shell_state.disk_dev, 0, read_buffer, sizeof(test_data));
    tty_printf(tty, "Read result: %d bytes\n", read_result);
    tty_printf(tty, "Content: %s\n", read_buffer);

    if (strcmp(test_data, read_buffer) == 0)
    {
        tty_write_str(tty, "Disk test: PASSED\n");
    }
    else
    {
        tty_write_str(tty, "Disk test: FAILED\n");
    }
}

// Command: perftest - run performance benchmark
static void cmd_perftest(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Performance Benchmark ===\n");

    // CPU performance test
    tty_write_str(tty, "Running CPU benchmark...\n");
    uint64_t start_time = io_read(AM_TIMER_UPTIME).us;

    volatile long long sum = 0;
    for (int i = 0; i < 500000; i++)
    {
        sum += i * i;
    }

    uint64_t end_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t cpu_time = end_time - start_time;
    tty_printf(tty, "CPU computation time: %llu microseconds\n", cpu_time);

    // Memory bandwidth test
    tty_write_str(tty, "Running memory benchmark...\n");
    size_t test_size = 64 * 1024; // 64KB
    char *src = pmm->alloc(test_size);
    char *dst = pmm->alloc(test_size);

    if (src && dst)
    {
        start_time = io_read(AM_TIMER_UPTIME).us;
        memcpy(dst, src, test_size);
        end_time = io_read(AM_TIMER_UPTIME).us;

        uint64_t mem_time = end_time - start_time;
        tty_printf(tty, "Memory copy time: %llu microseconds (64KB)\n", mem_time);

        pmm->free(src);
        pmm->free(dst);
    }
    else
    {
        tty_write_str(tty, "Memory allocation failed for benchmark.\n");
    }

    tty_write_str(tty, "Performance benchmark completed.\n");
}

// Command: taskinfo - show task information
static void cmd_taskinfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Task Information ===\n");
    tty_printf(tty, "Active Tasks: %d\n", shell_state.active_tasks);
    tty_write_str(tty, "Current Task: demo-shell\n");
    tty_write_str(tty, "Scheduler: Round-robin with preemption\n");
    tty_write_str(tty, "Task States: READY, RUNNING, BLOCKED, DEAD\n");
}

// Command: graphics - test graphics display
static void cmd_graphics(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Graphics Test ===\n");

    if (!shell_state.fb_dev)
    {
        tty_write_str(tty, "No graphics device available.\n");
        return;
    }

    fb_t *fb = shell_state.fb_dev->ptr;
    if (!fb || !fb->textures)
    {
        tty_write_str(tty, "Graphics device not properly initialized.\n");
        return;
    }

    tty_write_str(tty, "Creating test pattern...\n");

    // Create a simple test texture
    uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00};
    for (int i = 0; i < 4 && i < fb->info->num_textures; i++)
    {
        for (int j = 0; j < TEXTURE_W * TEXTURE_H; j++)
        {
            fb->textures[i].pixels[j] = colors[i];
        }
    }

    // Write texture data
    shell_state.fb_dev->ops->write(shell_state.fb_dev,
                                   sizeof(struct texture),
                                   fb->textures,
                                   sizeof(struct texture) * 4);

    // Create test sprites
    struct sprite sprites[4];
    for (int i = 0; i < 4; i++)
    {
        sprites[i] = (struct sprite){
            .texture = i + 1,
            .x = i * 50,
            .y = i * 50,
            .display = 0,
            .z = i};
    }

    // Write sprite data
    shell_state.fb_dev->ops->write(shell_state.fb_dev,
                                   SPRITE_BRK,
                                   sprites,
                                   sizeof(sprites));

    tty_write_str(tty, "Graphics test pattern displayed.\n");
}

// Command: uptime - show system uptime
static void cmd_uptime(device_t *tty, char *args)
{
    uint64_t current_time = io_read(AM_TIMER_UPTIME).us;
    uint64_t runtime = (current_time - shell_state.start_time) / 1000000;

    uint64_t hours = runtime / 3600;
    uint64_t minutes = (runtime % 3600) / 60;
    uint64_t seconds = runtime % 60;

    tty_printf(tty, "System uptime: %llu:%02llu:%02llu\n", hours, minutes, seconds);
}

// Command: clear - clear screen
static void cmd_clear(device_t *tty, char *args)
{
    // Send ANSI escape sequence to clear screen
    tty_write_str(tty, "\033[2J\033[H");
}

// Command structure
struct command
{
    const char *name;
    void (*handler)(device_t *tty, char *args);
    const char *description;
};

// Command table
static struct command commands[] = {
    {"help", cmd_help, "Show available commands"},
    {"sysinfo", cmd_sysinfo, "Display system information"},
    {"cpuinfo", cmd_cpuinfo, "Show CPU information"},
    {"meminfo", cmd_meminfo, "Display memory information"},
    {"devinfo", cmd_devinfo, "Show device information"},
    {"memtest", cmd_memtest, "Run memory allocation test"},
    {"disktest", cmd_disktest, "Test disk read/write"},
    {"perftest", cmd_perftest, "Run performance benchmark"},
    {"taskinfo", cmd_taskinfo, "Show task information"},
    {"graphics", cmd_graphics, "Test graphics display"},
    {"uptime", cmd_uptime, "Show system uptime"},
    {"clear", cmd_clear, "Clear screen"},
    {NULL, NULL, NULL} // End marker
};

// Parse and execute command
static void execute_command(device_t *tty, char *input)
{
    // Remove trailing newline
    char *newline = my_strchr(input, '\n');
    if (newline)
        *newline = '\0';

    // Skip empty commands
    if (strlen(input) == 0)
        return;

    // Handle exit command specially
    if (strcmp(input, "exit") == 0)
    {
        tty_write_str(tty, "Goodbye!\n");
        return;
    }

    // Parse command and arguments
    char *cmd = input;
    char *args = my_strchr(input, ' ');
    if (args)
    {
        *args = '\0';
        args++;
    }
    else
    {
        args = "";
    }

    // Find and execute command
    for (int i = 0; commands[i].name != NULL; i++)
    {
        if (strcmp(cmd, commands[i].name) == 0)
        {
            commands[i].handler(tty, args);
            return;
        }
    }

    // Command not found
    tty_printf(tty, "Unknown command: %s\n", cmd);
    tty_write_str(tty, "Type 'help' for available commands.\n");
}

// Main shell task for a TTY
static void demo_shell_task(void *arg)
{
    device_t *tty = dev->lookup((char *)arg);
    if (!tty)
    {
        printf("Failed to find TTY device: %s\n", (char *)arg);
        return;
    }

    char cmd[128];
    char prompt[32];
    snprintf(prompt, sizeof(prompt), "(%s) $ ", (char *)arg);

    // Welcome message
    tty_write_str(tty, "\n");
    tty_write_str(tty, "========================================\n");
    tty_write_str(tty, "    Kernel Demo Shell v1.0\n");
    tty_write_str(tty, "========================================\n");
    tty_write_str(tty, "Welcome to the kernel demonstration shell!\n");
    tty_write_str(tty, "Type 'help' to see available commands.\n");
    tty_write_str(tty, "Type 'exit' to quit the shell.\n");
    tty_write_str(tty, "\n");

    while (1)
    {
        // Display prompt
        tty_write_str(tty, prompt);

        // Read command
        int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
        if (nread > 0)
        {
            cmd[nread] = '\0';

            // Check for exit command
            if (strncmp(cmd, "exit", 4) == 0)
            {
                tty_write_str(tty, "Shell exiting...\n");
                break;
            }

            // Execute command
            execute_command(tty, cmd);
        }

        // Small delay to prevent busy waiting
        for (int i = 0; i < 10000; i++)
        {
            yield();
        }
    }
}

// Initialize shell state
static void init_shell_state()
{
    shell_state.start_time = io_read(AM_TIMER_UPTIME).us;
    shell_state.fb_dev = dev->lookup("fb");
    shell_state.input_dev = dev->lookup("input");
    shell_state.disk_dev = dev->lookup("sda");
    shell_state.active_tasks = 1;
    shell_state.memory_usage = 512; // Initial estimate

    // Get display information
    if (shell_state.fb_dev)
    {
        shell_state.fb_dev->ops->read(shell_state.fb_dev, 0, &shell_state.display_info, sizeof(shell_state.display_info));
    }
}

// Main demo task - creates shell instances
void kernel_demo_task(void *arg)
{
    printf("Kernel Demo Shell System Starting...\n");

    // Initialize shell state
    init_shell_state();

    // Create shell tasks for TTY1 and TTY2
    task_t *shell1_task = pmm->alloc(sizeof(task_t));
    if (shell1_task)
    {
        kmt->create(shell1_task, "demo-shell-tty1", demo_shell_task, "tty1");
        shell_state.active_tasks++;
        printf("Created shell for tty1\n");
    }

    task_t *shell2_task = pmm->alloc(sizeof(task_t));
    if (shell2_task)
    {
        kmt->create(shell2_task, "demo-shell-tty2", demo_shell_task, "tty2");
        shell_state.active_tasks++;
        printf("Created shell for tty2\n");
    }

    printf("Demo shell system initialized.\n");
    printf("Switch to TTY1 (Alt+1) or TTY2 (Alt+2) to use the shell.\n");

    // Main task just sleeps
    while (1)
    {
        for (int i = 0; i < 10000000; i++)
        {
            yield();
        }
    }
}
