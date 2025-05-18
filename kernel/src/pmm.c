#include <common.h>
#define MIN_BLOCK_SIZE 16 // 最小块大小
#define MAX_ORDER 31      // 最大阶数
#define THREAD_NUM 8      // 线程数
uintptr_t pgsize;
struct block_t
{
    size_t size;          // 块大小
    int order;            // 块阶数
    int free;             // 是否空闲
    struct block_t *next; // 链表中下一个块
    void *start_addr;     // 块的起始地址（用于计算伙伴）
};
typedef struct block_t *block_t;

// 空闲链表数组，每个元素是特定大小的空闲块链表头
block_t free_lists[THREAD_NUM][MAX_ORDER + 1];
spinlock_t thread_lock[THREAD_NUM];
// 计算块的阶数
static int get_order(size_t size)
{
    int order = 0;
    size_t s = 1;
    while (s < size)
    {
        s <<= 1;
        order++;
    }
    return order;
}

int gettid()
{
    return cpu_current();
}
// 计算伙伴块的地址
static block_t find_buddy(block_t block)
{
    if (!block)
        return NULL;

    // 计算块相对于起始地址的偏移量
    uintptr_t block_addr = (uintptr_t)block;
    uintptr_t start_addr = (uintptr_t)block->start_addr;
    uintptr_t offset = block_addr - start_addr;

    // 使用异或操作计算伙伴块的偏移量
    uintptr_t buddy_offset = offset ^ block->size;

    // 如果伙伴块偏移量在有效范围内，返回伙伴块地址
    if (buddy_offset < pgsize)
    {
        block_t buddy = (block_t)(start_addr + buddy_offset);

        // 验证这是一个有效的伙伴（大小和阶数相同）
        if (buddy->size == block->size && buddy->order == block->order)
        {
            return buddy;
        }
    }

    return NULL; // 伙伴不存在（可能在另一页）
}

// 分裂块
static block_t split_block(block_t block, int target_order)
{
    int curr_order = block->order;
    void *start_addr = block->start_addr; // 保存原始起始地址
    int tid = gettid() % THREAD_NUM;
    while (curr_order > target_order)
    {
        curr_order--;
        size_t new_size = 1UL << curr_order;

        // 创建新的伙伴块
        block_t buddy = (block_t)((uintptr_t)block + new_size);
        buddy->size = new_size;
        buddy->order = curr_order;
        buddy->free = 1;
        buddy->next = free_lists[tid][curr_order];
        free_lists[tid][curr_order] = buddy;
        buddy->start_addr = start_addr; // 设置为原始页起始地址

        // 更新原块信息
        block->size = new_size;
        block->order = curr_order;
    }

    return block;
}

// 从空闲链表中移除块
static void remove_from_free_list(block_t block)
{
    if (!block || !block->free)
        return;
    int tid = gettid() % THREAD_NUM;
    int order = block->order;
    block_t *curr = &free_lists[tid][order];

    while (*curr != NULL)
    {
        if (*curr == block)
        {
            *curr = block->next;
            block->next = NULL;
            return;
        }
        curr = &((*curr)->next);
    }
}

// 合并块
static block_t merge_blocks(block_t block)
{
    if (!block || !block->free)
        return block;

    block_t buddy = find_buddy(block);

    // 如果伙伴不存在、不空闲或阶数不同，不能合并
    if (!buddy || !buddy->free || buddy->order != block->order)
    {
        return block;
    }

    // // 从对应的空闲链表中移除伙伴块
    remove_from_free_list(buddy);
    remove_from_free_list(block);
    // 确保block是两个中地址较低的
    block_t lower_block = (uintptr_t)block < (uintptr_t)buddy ? block : buddy;

    // 合并为更大的块
    lower_block->size *= 2;
    lower_block->order += 1;

    // 递归尝试继续合并
    return merge_blocks(lower_block);
}
static void *kalloc(size_t size)
{
    // 考虑头部大小
    size += sizeof(struct block_t);

    // 计算所需块的阶数
    int req_order = get_order(size);
    if (req_order < get_order(MIN_BLOCK_SIZE))
    {
        req_order = get_order(MIN_BLOCK_SIZE);
    }
    int tid = gettid() % THREAD_NUM;
    kmt->spin_lock(&thread_lock[tid]);
    // 查找合适的块
    int order = req_order;
    block_t block = NULL;
    while (order <= MAX_ORDER)
    {
        block_t *prev = &free_lists[tid][order];
        block_t curr = *prev;
        // 遍历当前阶数的空闲链表，找到空闲的块
        while (curr != NULL)
        {
            if (curr->free && curr->order >= req_order)
            {
                // 找到空闲块，从链表中移除
                *prev = curr->next;
                block = curr;
                order = curr->order;
                break;
            }
            // 不空闲，继续查找
            prev = &(curr->next);
            curr = curr->next;
        }
        if (block)
            break; // 找到合适的块，退出循环

        order++; // 尝试更高阶数
    }

    if (!block)
    {
        kmt->spin_unlock(&thread_lock[tid]);
        panic("No free block found");
        return NULL; // 没有足够的内存
    }

    // 如果找到的块比需要的大，分裂它
    if (order > req_order)
    {
        block = split_block(block, req_order);
    }
    // 标记为已使用
    block->free = 0;
    // 返回可用内存区域（跳过块头部）
    kmt->spin_unlock(&thread_lock[tid]);
    return (void *)((uintptr_t)block + sizeof(struct block_t));
}

static void kfree(void *ptr)
{
    if (!ptr)
        return;
    int tid = gettid() % THREAD_NUM;
    // 计算块的地址（减去头部大小）
    block_t block = (block_t)((uintptr_t)ptr - sizeof(struct block_t));

    // 标记为空闲
    block->free = 1;
    kmt->spin_lock(&thread_lock[tid]);
    // 尝试与伙伴合并
    block = merge_blocks(block);
    // 将块添加到相应的空闲链表中
    block->next = free_lists[tid][block->order];
    free_lists[tid][block->order] = block;
    kmt->spin_unlock(&thread_lock[tid]);
}

static void pmm_init()
{
    uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
    pgsize = pmsize / THREAD_NUM;
    for (size_t i = 0; i < THREAD_NUM; i++)
    {
        kmt->spin_init(&thread_lock[i], "pmm_spinlock");
        block_t block = heap.start + i * pgsize;
        block->size = pgsize;
        block->order = get_order(block->size);
        block->free = 1;
        block->next = free_lists[i][block->order];
        block->start_addr = block; // 初始时，块的起始地址就是自己
        free_lists[i][block->order] = block;
    }
    printf(
        "Got %d MiB heap: [%p, %p)\n",
        pmsize >> 20, heap.start, heap.end);
}

MODULE_DEF(pmm) = {
    .init = pmm_init,
    .alloc = kalloc,
    .free = kfree,
};
