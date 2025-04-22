#include "test_framework.h"
#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <os.h>

// 模拟的页面分配器
static void *test_pgalloc(int size)
{
    void *ptr = malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

// 模拟的页面释放函数
static void test_pgfree(void *ptr)
{
    free(ptr);
}

// 测试前准备
static void vmm_setup(void)
{
    // 初始化虚拟内存环境
    vme_init(test_pgalloc, test_pgfree);
}

// 测试后清理
static void vmm_teardown(void)
{
    // 清理工作，如果需要的话
}

// 测试映射和访问
static void test_map_access(void)
{
    AddrSpace as;
    as.area.start = (void *)0x100000;
    as.area.end = (void *)0x200000;
    as.pgsize = 4096;
    as.ptr = test_pgalloc(4096); // 分配页表根目录

    protect(&as);

    // 映射一页内存
    void *vaddr = (void *)0x150000;
    void *paddr = test_pgalloc(4096);
    memset(paddr, 0xAA, 4096);
    map(&as, vaddr, paddr, MMAP_READ | MMAP_WRITE);

    // 创建上下文
    Area kstack;
    kstack.start = test_pgalloc(4096);
    kstack.end = (char *)kstack.start + 4096;

    Context *ctx = ucontext(&as, kstack, vaddr);
    ASSERT_NOT_NULL(ctx, "上下文创建失败");

    // 清理
    unprotect(&as);
    test_pgfree(paddr);
    test_pgfree(as.ptr);
    test_pgfree(kstack.start);
}

// 测试权限设置
static void test_permissions(void)
{
    AddrSpace as;
    as.area.start = (void *)0x100000;
    as.area.end = (void *)0x200000;
    as.pgsize = 4096;
    as.ptr = test_pgalloc(4096);

    protect(&as);

    // 映射只读页
    void *vaddr_ro = (void *)0x160000;
    void *paddr_ro = test_pgalloc(4096);
    map(&as, vaddr_ro, paddr_ro, MMAP_READ);

    // 映射读写页
    void *vaddr_rw = (void *)0x170000;
    void *paddr_rw = test_pgalloc(4096);
    map(&as, vaddr_rw, paddr_rw, MMAP_READ | MMAP_WRITE);

    // 创建上下文
    Area kstack;
    kstack.start = test_pgalloc(4096);
    kstack.end = (char *)kstack.start + 4096;

    Context *ctx = ucontext(&as, kstack, vaddr_ro);
    ASSERT_NOT_NULL(ctx, "上下文创建失败");

    // 清理
    unprotect(&as);
    test_pgfree(paddr_ro);
    test_pgfree(paddr_rw);
    test_pgfree(as.ptr);
    test_pgfree(kstack.start);
}

// 注册测试
void vmm_test_register(void)
{
    test_suite_t *suite = test_suite_create("虚拟内存管理测试");

    test_suite_add_case(suite, "映射和访问测试", vmm_setup, test_map_access, vmm_teardown);
    test_suite_add_case(suite, "权限设置测试", vmm_setup, test_permissions, vmm_teardown);

    test_register_suite(suite);
}