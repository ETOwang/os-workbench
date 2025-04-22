#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include "include/test_framework.h"

// 外部测试套件注册函数
extern void pmm_test_register(void);
extern void kmt_test_register(void);
extern void dev_test_register(void);
extern void vmm_test_register(void);

// 全局测试标志
bool run_pmm_tests = true;  // 是否运行内存管理测试
bool run_kmt_tests = true;  // 是否运行线程管理测试
bool run_dev_tests = true;  // 是否运行设备管理测试
bool run_vmm_tests = false; // 是否运行虚拟内存测试（默认关闭，可能需要额外支持）

static void parse_args(const char *args)
{
    // 简单的参数解析，允许测试特定模块
    // 例如: "no-pmm" 表示不运行PMM测试
    if (strstr(args, "no-pmm"))
        run_pmm_tests = false;
    if (strstr(args, "no-kmt"))
        run_kmt_tests = false;
    if (strstr(args, "no-dev"))
        run_dev_tests = false;

    // 启用可选测试
    if (strstr(args, "vmm"))
        run_vmm_tests = true;

    // 特殊模式
    if (strstr(args, "all"))
    {
        run_pmm_tests = run_kmt_tests = run_dev_tests = true;
        run_vmm_tests = true;
    }
    if (strstr(args, "none"))
    {
        run_pmm_tests = run_kmt_tests = run_dev_tests = run_vmm_tests = false;
    }
}

// 主程序入口
int main(const char *args)
{
    // 初始化Abstract Machine
    ioe_init();
    cte_init(NULL); // 使用NULL作为中断处理函数，因为我们不测试中断

    // 解析命令行参数
    parse_args(args);

    // 打印欢迎信息
    printf("\n");
    printf("===================================\n");
    printf("     操作系统测试框架 v1.0         \n");
    printf("===================================\n");
    printf("命令行参数: %s\n\n", args);

    // 初始化测试框架
    test_init();

    // 注册测试套件
    printf("正在注册测试套件...\n");
    if (run_pmm_tests)
    {
        printf("- 注册内存管理(PMM)测试\n");
        pmm_test_register();
    }

    if (run_kmt_tests)
    {
        printf("- 注册线程管理(KMT)测试\n");
        kmt_test_register();
    }

    if (run_dev_tests)
    {
        printf("- 注册设备管理(DEV)测试\n");
        dev_test_register();
    }

    if (run_vmm_tests)
    {
        printf("- 注册虚拟内存(VMM)测试\n");
        vmm_test_register();
    }

    // 运行所有测试套件
    printf("\n准备开始测试...\n\n");
    test_report_t report = test_run_all();

    // 打印测试结果
    test_print_report(report);

    // 清理测试资源
    test_cleanup();

    // 完成测试
    printf("\n测试完成，系统将挂起\n");
    while (1)
    {
        // 无限循环，避免程序退出
        yield();
    }

    return 0;
}