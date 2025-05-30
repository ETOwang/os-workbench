#include "ulib.h"
// 自定义打印字符串函数（使用kputc）
void print(const char *str) {
    for (; *str != '\0'; str++) {
        kputc(*str);
    }
}

// 自定义打印整数函数（使用kputc）
void print_int(int num) {
    if (num == 0) {
        kputc('0');
        return;
    }
    
    char buffer[16];
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    if (is_negative) {
        kputc('-');
    }
    
    for (int j = i - 1; j >= 0; j--) {
        kputc(buffer[j]);
    }
}

// 自定义打印十六进制函数（使用kputc）
void print_hex(uint64_t num) {
    if (num == 0) {
        kputc('0');
        return;
    }
    
    char buffer[16];
    int i = 0;
    
    while (num > 0) {
        int digit = num % 16;
        buffer[i++] = digit < 10 ? '0' + digit : 'a' + (digit - 10);
        num /= 16;
    }
    
    kputc('0');
    kputc('x');
    for (int j = i - 1; j >= 0; j--) {
        kputc(buffer[j]);
    }
}

int main() {
    print("Starting system call tests for implemented functions...\n\n");
    
    // 测试kputc
    print("1. Testing kputc():\n");
    print("  Hello from kputc!\n");
    print("  This is a test message.\n\n");
    
    // 测试gettimeofday
    print("2. Testing gettimeofday():\n");
    struct timespec ts;
    int time_result = gettimeofday(&ts);
    
    if (time_result < 0) {
        print("  Error: gettimeofday failed!\n");
    } else {
        print("  Success! Time value stored at address: ");
        print_hex((uint64_t)&ts);
        print("\n");
    }
    print("\n");
    
    // 测试fork, wait4和exit
    print("3. Testing fork(), wait4() and exit():\n");
    int child_pid = fork();
    
    if (child_pid < 0) {
        print("  Error: Fork failed!\n");
    } else if (child_pid == 0) {
        // 子进程
        print("  Child process started\n");
        
        // 测试gettimeofday
        struct timespec child_ts;
        if (gettimeofday(&child_ts) < 0) {
            print("  Child: gettimeofday failed!\n");
        } else {
            print("  Child: Current time (seconds): ");
            print_int(child_ts.tv_sec);
            print("\n");
        }
        
        // 测试sleep
        print("  Child: Sleeping for 1 second...\n");
        sleep(1);
        print("  Child: Awake now!\n");
        
        // 测试exit
        print("  Child: Exiting with status 42\n");
        exit(42);
        
        // 子进程不会执行到这里
        print("  Child: This should not be printed!\n");
    } else {
        // 父进程
        print("  Parent: Forked child with PID: ");
        print_int(child_pid);
        print("\n");
        
        print("  Parent: Waiting for child...\n");
        int status;
        int wpid = wait4(-1, &status, 0);
        
        if (wpid < 0) {
            print("  Parent: Error: wait4 failed!\n");
        } else {
            print("  Parent: Child process ");
            print_int(wpid);
            print(" exited\n");
            
            // 注意：status的具体格式取决于内核实现
            print("  Parent: Exit status value: ");
            print_int(status);
            print(" (hex: ");
            print_hex(status);
            print(")\n");
        }
        
        // 测试sleep
        print("  Parent: Sleeping for 2 seconds...\n");
        sleep(2);
        print("  Parent: Awake now!\n");
    }
    print("\n");
    
    // 测试getcwd和chdir
    print("4. Testing getcwd() and chdir():\n");
    
    // 测试getcwd - 获取当前工作目录
    char cwd_buffer[256];
    int getcwd_result = getcwd(cwd_buffer, sizeof(cwd_buffer));
    
    if (getcwd_result < 0) {
        print("  Error: getcwd failed!\n");
    } else {
        print("  Success! Current working directory: ");
        // 由于我们使用自定义print函数，需要null-terminated字符串
        cwd_buffer[255] = '\0';  // 确保null终止
        print(cwd_buffer);
        print("\n");
    }
    
    // 测试chdir - 改变当前工作目录到根目录
    print("  Attempting to change directory to /...\n");
    int chdir_result = chdir("/");
    
    if (chdir_result < 0) {
        print("  Error: chdir to / failed!\n");
    } else {
        print("  Success! Changed directory to /\n");
        
        // 再次测试getcwd来验证目录变更
        char new_cwd_buffer[256];
        int new_getcwd_result = getcwd(new_cwd_buffer, sizeof(new_cwd_buffer));
        
        if (new_getcwd_result < 0) {
            print("  Error: getcwd after chdir failed!\n");
        } else {
            print("  New current working directory: ");
            new_cwd_buffer[255] = '\0';  // 确保null终止
            print(new_cwd_buffer);
            print("\n");
        }
    }
    
    // 尝试切换到一个可能不存在的目录来测试错误处理
    print("  Testing chdir to non-existent directory /nonexistent...\n");
    int bad_chdir_result = chdir("/nonexistent");
    
    if (bad_chdir_result < 0) {
        print("  Expected: chdir to /nonexistent failed (this is normal)\n");
    } else {
        print("  Unexpected: chdir to /nonexistent succeeded\n");
    }
    
    // 测试getcwd with invalid buffer size
    print("  Testing getcwd with small buffer...\n");
    char small_buffer[1];
    int small_getcwd_result = getcwd(small_buffer, sizeof(small_buffer));
    
    if (small_getcwd_result < 0) {
        print("  Expected: getcwd with small buffer failed (this is normal)\n");
    } else {
        print("  Unexpected: getcwd with small buffer succeeded\n");
    }
    
    print("\n");
    
    print("All tests completed!\n");
    print("Exiting with status 0\n");
    exit(0);
    
    // 永远不会执行到这里
    return 0;
}