#include <common.h>

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

static struct
{
    uint64_t start_time;
    device_t *fb_dev;
    device_t *input_dev;
    device_t *disk_dev;
    struct display_info display_info;
} shell_state;

static void tty_write_str(device_t *tty, const char *str)
{
    tty->ops->write(tty, 0, str, strlen(str));
}

static void tty_printf(device_t *tty, const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    int len = vsprintf(buffer, format, args);
    va_end(args);
    tty->ops->write(tty, 0, buffer, len);
}

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
}

static void cmd_sysinfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== System Information ===\n");
    tty_printf(tty, "CPU Count: %d\n", cpu_count());
    tty_printf(tty, "Current CPU: %d\n", cpu_current());
    struct timespec ts;
    uproc->uptime(NULL, &ts);
    tty_printf(tty, "System Runtime: %d seconds\n", ts.tv_sec);
}

static void cmd_cpuinfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== CPU Information ===\n");
    tty_printf(tty, "Total CPUs: %d\n", cpu_count());
    tty_printf(tty, "Current CPU ID: %d\n", cpu_current());
    tty_write_str(tty, "Architecture: x86_64");
    tty_write_str(tty, "Features: Multi-core, Virtual Memory, Interrupts\n");
}

static void cmd_meminfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Memory Information ===\n");
    tty_write_str(tty, "Memory Manager: Buddy Allocator\n");
    tty_write_str(tty, "Page Size: 4KB\n");
    tty_write_str(tty, "Virtual Memory: Enabled\n");
}

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
static void cmd_perftest(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Performance Benchmark ===\n");

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

    tty_write_str(tty, "Running memory benchmark...\n");
    size_t test_size = 64 * 1024;
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

static void cmd_taskinfo(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Task Information ===\n");
    tty_write_str(tty, "Current Task: %s\n");
    tty_write_str(tty, "Scheduler: Round-robin with preemption\n");
    tty_write_str(tty, "Task States: READY, RUNNING, BLOCKED, DEAD\n");
}

static void cmd_graphics(device_t *tty, char *args)
{
    tty_write_str(tty, "=== Beautiful Background Mode ===\n");

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

    tty_t *current_tty = tty->ptr;
    tty_write_str(tty, "Creating beautiful gradient background...\n");

    // Define a single beautiful background color - very subtle
    uint32_t background_color = 0x1a1a1a; // Very dark gray - extremely subtle and readable

    tty_write_str(tty, "Creating solid background texture...\n");

    // Create a single background texture
    for (int j = 0; j < TEXTURE_W * TEXTURE_H; j++)
    {
        fb->textures[600].pixels[j] = background_color;
    }

    // Write background texture to framebuffer
    shell_state.fb_dev->ops->write(shell_state.fb_dev,
                                   600 * sizeof(struct texture),
                                   &fb->textures[600],
                                   1 * sizeof(struct texture));

    // Create background sprites to cover the entire screen for BOTH displays
    struct sprite background_sprites[2000]; // Even larger array for both displays
    int sprite_count = 0;

    int screen_width = shell_state.display_info.width;
    int screen_height = shell_state.display_info.height;

    tty_printf(tty, "Screen size: %dx%d, Texture size: %dx%d\n",
               screen_width, screen_height, TEXTURE_W, TEXTURE_H);

    // Create background for BOTH display 0 (TTY1) and display 1 (TTY2)
    for (int display = 0; display < 2; display++)
    {
        tty_printf(tty, "Creating background for display %d...\n", display);

        for (int y = 0; y < screen_height && sprite_count < 2000; y += TEXTURE_H)
        {
            for (int x = 0; x < screen_width && sprite_count < 2000; x += TEXTURE_W)
            {
                background_sprites[sprite_count] = (struct sprite){
                    .texture = 600, // Use single background texture
                    .x = x,
                    .y = y,
                    .display = display, // Create for both displays
                    .z = 100            // Far behind text (text is at z=0)
                };
                sprite_count++;
            }
        }
    }

    // Write background sprites to framebuffer
    shell_state.fb_dev->ops->write(shell_state.fb_dev,
                                   SPRITE_BRK,
                                   background_sprites,
                                   sprite_count * sizeof(struct sprite));

    // Force display update for current display
    device_t *fb_dev = shell_state.fb_dev;
    if (fb_dev && fb->info)
    {
        int current_display = current_tty->display;
        fb->info->current = current_display;
        fb_dev->ops->write(fb_dev, 0, fb->info, sizeof(struct display_info));
    }

    int tiles_per_display = (screen_width / TEXTURE_W + 1) * (screen_height / TEXTURE_H + 1);
    int total_tiles_needed = tiles_per_display * 2; // For both displays
    tty_printf(tty, "Background tiles: %d created, %d needed for both displays\n", sprite_count, total_tiles_needed);

    if (sprite_count >= total_tiles_needed * 0.8)
    {
        tty_write_str(tty, "✓ Beautiful background activated for BOTH TTY displays!\n");
        tty_write_str(tty, "Your terminal now has a subtle light blue background!\n");
        tty_write_str(tty, "Background persists when switching between TTY1 and TTY2.\n");
        tty_write_str(tty, "Text remains clearly readable with the gentle background.\n");
    }
    else
    {
        tty_write_str(tty, "⚠ Partial background coverage - may need more sprites\n");
        tty_printf(tty, "Consider increasing sprite array size beyond %d\n", 2000);
    }
}

static void cmd_uptime(device_t *tty, char *args)
{
    struct timespec ts;
    uproc->uptime(NULL, &ts);
    tty_printf(tty, "System uptime: %ds%dns\n", ts.tv_sec, ts.tv_nsec);
}

static void cmd_clear(device_t *tty, char *args)
{
    tty_t *tty_ptr = tty->ptr;
    for (int i = 0; i < tty_ptr->size; i++)
    {
        tty_ptr->buf[i].ch = ' ';
        tty_ptr->buf[i].metadata = 0;
    }
    tty_ptr->cursor = tty_ptr->buf;
    for (int i = 0; i < tty_ptr->size; i++)
    {
        tty_ptr->dirty[i] = 1;
    }
    tty->ops->write(tty, 0, "", 0);
}

struct command
{
    const char *name;
    void (*handler)(device_t *tty, char *args);
    const char *description;
};

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
    {NULL, NULL, NULL}};

static void execute_command(device_t *tty, char *input)
{

    char *newline = my_strchr(input, '\n');
    if (newline)
        *newline = '\0';

    if (strlen(input) == 0)
        return;
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
    for (int i = 0; commands[i].name != NULL; i++)
    {
        if (strcmp(cmd, commands[i].name) == 0)
        {
            commands[i].handler(tty, args);
            return;
        }
    }
    tty_printf(tty, "Unknown command: %s\n", cmd);
    tty_write_str(tty, "Type 'help' for available commands.\n");
}

static void shell_task(void *arg)
{
    device_t *tty = dev->lookup((char *)arg);
    char cmd[128];
    char prompt[32];
    snprintf(prompt, sizeof(prompt), "(%s) $ ", (char *)arg);

    tty_write_str(tty, "\n");
    tty_write_str(tty, "========================================\n");
    tty_write_str(tty, "    Kernel Demo Shell v1.0\n");
    tty_write_str(tty, "========================================\n");
    tty_write_str(tty, "Welcome to the kernel demonstration shell!\n");
    tty_write_str(tty, "Type 'help' to see available commands.\n");
    tty_write_str(tty, "\n");

    while (1)
    {
        tty_write_str(tty, prompt);

        int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
        if (nread > 0)
        {
            cmd[nread] = '\0';
            execute_command(tty, cmd);
        }
    }
}

static void init_shell_state()
{
    shell_state.start_time = io_read(AM_TIMER_UPTIME).us;
    shell_state.fb_dev = dev->lookup("fb");
    shell_state.input_dev = dev->lookup("input");
    shell_state.disk_dev = dev->lookup("sda");

    if (shell_state.fb_dev)
    {
        shell_state.fb_dev->ops->read(shell_state.fb_dev, 0, &shell_state.display_info, sizeof(shell_state.display_info));
    }
}

void kernel_shell(void *arg)
{
    init_shell_state();
    task_t *shell1_task = pmm->alloc(sizeof(task_t));
    if (shell1_task)
    {
        kmt->create(shell1_task, "tty1", shell_task, "tty1");
    }
    task_t *shell2_task = pmm->alloc(sizeof(task_t));
    if (shell2_task)
    {
        kmt->create(shell2_task, "tty2", shell_task, "tty2");
    }
    printf("Switch to TTY1 (Alt+1) or TTY2 (Alt+2) to use the shell.\n");
    while (1)
        ;
}
