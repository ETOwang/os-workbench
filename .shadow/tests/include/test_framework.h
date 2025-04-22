#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <kernel.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

// 测试状态枚举
typedef enum
{
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP,
    TEST_RUNNING
} test_result_t;

// 单个测试用例结构
typedef struct test_case
{
    const char *name;
    void (*setup)(void);
    void (*run)(void);
    void (*teardown)(void);
    test_result_t result;
    const char *failure_msg;
    struct test_case *next;
} test_case_t;

// 测试套件结构
typedef struct test_suite
{
    const char *name;
    void (*suite_setup)(void);
    void (*suite_teardown)(void);
    test_case_t *test_cases;
    int cases_count;
    int pass_count;
    int fail_count;
    int skip_count;
    struct test_suite *next;
} test_suite_t;

// 测试报告结构
typedef struct
{
    int total_suites;
    int total_cases;
    int total_pass;
    int total_fail;
    int total_skip;
    uint64_t total_time_ms;
} test_report_t;

// 断言宏
#define ASSERT(condition, message)                                       \
    do                                                                   \
    {                                                                    \
        if (!(condition))                                                \
        {                                                                \
            current_test_case->result = TEST_FAIL;                       \
            current_test_case->failure_msg = message;                    \
            printf("[FAIL] %s: %s\n", current_test_case->name, message); \
            return;                                                      \
        }                                                                \
    } while (0)

#define ASSERT_EQ(expected, actual, message) \
    ASSERT((expected) == (actual), message)

#define ASSERT_NE(expected, actual, message) \
    ASSERT((expected) != (actual), message)

#define ASSERT_TRUE(condition, message) \
    ASSERT(condition, message)

#define ASSERT_FALSE(condition, message) \
    ASSERT(!(condition), message)

#define ASSERT_NULL(ptr, message) \
    ASSERT(ptr == NULL, message)

#define ASSERT_NOT_NULL(ptr, message) \
    ASSERT(ptr != NULL, message)

// 操作系统功能测试相关宏和类型
typedef struct
{
    void *allocated_addr;
    size_t size;
} memory_block_t;

// 测试工具函数
void test_init(void);
test_suite_t *test_suite_create(const char *name);
void test_suite_add_case(test_suite_t *suite, const char *name, void (*setup)(void), void (*run)(void), void (*teardown)(void));
void test_suite_set_lifecycle(test_suite_t *suite, void (*suite_setup)(void), void (*suite_teardown)(void));
void test_register_suite(test_suite_t *suite);
test_report_t test_run_all(void);
void test_print_report(test_report_t report);
void test_cleanup(void);

// 操作系统模块测试工具函数
void test_os_api(void);
void test_pmm_alloc_free(size_t size, int count);
void test_kmt_create_teardown(void);
void test_kmt_synchronization(void);
void test_device_access(const char *dev_name);
void test_interrupt_handler(void);

// 多处理器测试支持
void test_run_concurrent(test_suite_t *suite, int thread_count);
void test_barrier_wait(int thread_count);

// 全局变量
extern test_case_t *current_test_case;
extern test_suite_t *first_suite;
extern test_suite_t *last_suite;

#endif // TEST_FRAMEWORK_H