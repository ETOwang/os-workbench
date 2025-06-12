#include <common.h>

// Simple integer-based trigonometric functions for graphics
// Using fixed-point arithmetic to avoid floating point operations
#define TRIG_SCALE 1000

// Precomputed sine table for angles 0-90 degrees (scaled by 1000)
static int sine_table[91] = {
    0, 17, 35, 52, 70, 87, 105, 122, 139, 156, 174, 191, 208, 225, 242, 259, 276, 292, 309, 326,
    342, 358, 375, 391, 407, 423, 438, 454, 469, 485, 500, 515, 530, 545, 559, 574, 588, 602, 616, 629,
    643, 656, 669, 682, 695, 707, 719, 731, 743, 755, 766, 777, 788, 799, 809, 819, 829, 839, 848, 857,
    866, 875, 883, 891, 899, 906, 914, 921, 927, 934, 940, 946, 951, 956, 961, 966, 970, 974, 978, 982,
    985, 988, 990, 993, 995, 996, 998, 999, 999, 1000, 1000};

static int my_sin_int(int angle_deg)
{
    // Normalize angle to 0-359 range
    angle_deg = angle_deg % 360;
    if (angle_deg < 0)
        angle_deg += 360;

    if (angle_deg <= 90)
    {
        return sine_table[angle_deg];
    }
    else if (angle_deg <= 180)
    {
        return sine_table[180 - angle_deg];
    }
    else if (angle_deg <= 270)
    {
        return -sine_table[angle_deg - 180];
    }
    else
    {
        return -sine_table[360 - angle_deg];
    }
}

static int my_cos_int(int angle_deg)
{
    // cos(x) = sin(x + 90)
    return my_sin_int(angle_deg + 90);
}

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
    tty_write_str(tty, "=== Graphics Demo ===\n");

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

    tty_write_str(tty, "Creating beautiful graphics demo...\n");

    // Create gradient textures with more sophisticated patterns
    uint32_t base_colors[] = {
        0xFF6B6B, // Coral red
        0x4ECDC4, // Turquoise
        0x45B7D1, // Sky blue
        0x96CEB4, // Mint green
        0xFECA57, // Golden yellow
        0xFF9FF3, // Pink
        0xA8E6CF, // Light green
        0xFFD93D, // Bright yellow
        0x6C5CE7, // Purple
        0xFD79A8  // Rose
    };

    // Create textured patterns instead of solid colors
    for (int tex = 0; tex < 10 && tex < fb->info->num_textures - 512; tex++)
    {
        uint32_t base_color = base_colors[tex];
        for (int y = 0; y < TEXTURE_H; y++)
        {
            for (int x = 0; x < TEXTURE_W; x++)
            {
                int idx = y * TEXTURE_W + x;

                // Create different patterns for different textures
                switch (tex % 4)
                {
                case 0: // Gradient pattern
                {
                    int intensity = (x + y) * 255 / (TEXTURE_W + TEXTURE_H - 2);
                    uint32_t r = (((base_color >> 16) & 0xFF) * intensity) / 255;
                    uint32_t g = (((base_color >> 8) & 0xFF) * intensity) / 255;
                    uint32_t b = ((base_color & 0xFF) * intensity) / 255;
                    fb->textures[512 + tex].pixels[idx] = (r << 16) | (g << 8) | b;
                }
                break;
                case 1: // Checkerboard pattern
                    if ((x + y) % 2 == 0)
                        fb->textures[512 + tex].pixels[idx] = base_color;
                    else
                        fb->textures[512 + tex].pixels[idx] = base_color ^ 0x404040;
                    break;
                case 2: // Circular pattern
                {
                    int dx = x - TEXTURE_W / 2;
                    int dy = y - TEXTURE_H / 2;
                    int dist = dx * dx + dy * dy;
                    if (dist < (TEXTURE_W / 2) * (TEXTURE_W / 2))
                        fb->textures[512 + tex].pixels[idx] = base_color;
                    else
                        fb->textures[512 + tex].pixels[idx] = base_color >> 1;
                }
                break;
                case 3: // Diagonal stripes
                    if ((x + y) % 3 == 0)
                        fb->textures[512 + tex].pixels[idx] = base_color;
                    else
                        fb->textures[512 + tex].pixels[idx] = base_color ^ 0x202020;
                    break;
                }
            }
        }
    }

    // Write textures to framebuffer
    shell_state.fb_dev->ops->write(shell_state.fb_dev,
                                   512 * sizeof(struct texture),
                                   &fb->textures[512],
                                   10 * sizeof(struct texture));

    // Create an attractive sprite arrangement
    struct sprite sprites[50];
    int sprite_count = 0;

    // Create a spiral pattern
    int angle_deg = 0;
    int center_x = shell_state.display_info.width / 2;
    int center_y = shell_state.display_info.height / 2;

    for (int i = 0; i < 10 && sprite_count < 50; i++)
    {
        int radius = 50 + i * 15;
        // Use integer trigonometry with scaling
        int x = center_x + (radius * my_cos_int(angle_deg)) / TRIG_SCALE;
        int y = center_y + (radius * my_sin_int(angle_deg)) / TRIG_SCALE;

        // Ensure sprites stay within screen bounds
        if (x >= 0 && x < shell_state.display_info.width - TEXTURE_W &&
            y >= 0 && y < shell_state.display_info.height - TEXTURE_H)
        {
            sprites[sprite_count] = (struct sprite){
                .texture = 512 + (i % 10),
                .x = x,
                .y = y,
                .display = 0,
                .z = i};
            sprite_count++;
        }

        angle_deg += 46; // Approximately golden angle (137.5°) for nice spiral
    }

    // Add some corner decorations
    int corner_positions[][2] = {{50, 50}, {shell_state.display_info.width - 100, 50}, {50, shell_state.display_info.height - 100}, {shell_state.display_info.width - 100, shell_state.display_info.height - 100}};

    for (int i = 0; i < 4 && sprite_count < 50; i++)
    {
        sprites[sprite_count] = (struct sprite){
            .texture = 512 + (i + 6) % 10,
            .x = corner_positions[i][0],
            .y = corner_positions[i][1],
            .display = 0,
            .z = 20 + i};
        sprite_count++;
    }

    // Write sprites to framebuffer
    shell_state.fb_dev->ops->write(shell_state.fb_dev,
                                   SPRITE_BRK,
                                   sprites,
                                   sprite_count * sizeof(struct sprite));

    tty_printf(tty, "Beautiful graphics demo displayed! (%d sprites)\n", sprite_count);
    tty_write_str(tty, "Graphics are rendered to the framebuffer.\n");
    tty_write_str(tty, "The graphics appear as background behind the text.\n");
    tty_write_str(tty, "Try switching TTY (Alt+1/Alt+2) to see different views.\n");

    // Also update the current display to show the graphics immediately
    device_t *fb_dev = shell_state.fb_dev;
    if (fb_dev)
    {
        tty_t *current_tty = tty->ptr;
        fb_t *fb = fb_dev->ptr;
        if (fb && fb->info)
        {
            fb->info->current = current_tty->display;
            fb_dev->ops->write(fb_dev, 0, fb->info, sizeof(struct display_info));
        }
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
    // Clear the TTY buffer directly
    tty_t *tty_ptr = tty->ptr;

    // Clear the character buffer
    for (int i = 0; i < tty_ptr->size; i++)
    {
        tty_ptr->buf[i].ch = ' ';
        tty_ptr->buf[i].metadata = 0;
    }

    // Reset cursor to top-left
    tty_ptr->cursor = tty_ptr->buf;

    // Mark all positions as dirty for re-rendering
    for (int i = 0; i < tty_ptr->size; i++)
    {
        tty_ptr->dirty[i] = 1;
    }

    // Force a render to update the display
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
