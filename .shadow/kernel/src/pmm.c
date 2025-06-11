#include <common.h>
#define MIN_BLOCK_SIZE 16 // 最小块大小
#define MAX_ORDER 31      // 最大阶数
#define THREAD_NUM 8        // 线程数
uintptr_t pgsize;    
struct block_t
{
    size_t size;          
    int order;            
    int free;             
    struct block_t *next; 
    void *start_addr;     
    size_t offset;
};
typedef struct block_t *block_t;
block_t free_lists[THREAD_NUM][MAX_ORDER + 1];
spinlock_t thread_lock[THREAD_NUM];
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

static size_t round_up_to_power_of_2(size_t val)
{
    if (val == 0)
        return 0;
    size_t n = val - 1;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if (sizeof(size_t) > 4)
    { 
        n |= n >> 32;
    }
    return n + 1;
}

static inline int gettid(void)
{
    return cpu_current();
}
static block_t find_buddy(block_t block)
{
    if (!block)
        return NULL;

    uintptr_t block_addr = (uintptr_t)block;
    uintptr_t start_addr = (uintptr_t)block->start_addr;
    uintptr_t offset = block_addr - start_addr;
    uintptr_t buddy_offset = offset ^ block->size;

    if (buddy_offset < pgsize)
    {
        block_t buddy = (block_t)(start_addr + buddy_offset);
        if (buddy->size == block->size && buddy->order == block->order)
        {
            return buddy;
        }
    }

    return NULL; 
}

static block_t split_block(block_t block, int target_order)
{
    int curr_order = block->order;
    void *start_addr = block->start_addr;
    int tid = gettid() % THREAD_NUM;
    while (curr_order > target_order)
    {
        curr_order--;
        size_t new_size = 1UL << curr_order;
        block_t buddy = (block_t)((uintptr_t)block + new_size);
        buddy->size = new_size;
        buddy->order = curr_order;
        buddy->free = 1;
        buddy->next = free_lists[tid][curr_order];
        free_lists[tid][curr_order] = buddy;
        buddy->start_addr = start_addr;
        buddy->offset = 0;              
        block->size = new_size;
        block->order = curr_order;
    }

    return block;
}

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

static block_t merge_blocks(block_t block)
{
    if (!block || !block->free)
        return block;

    block_t buddy = find_buddy(block);
    if (!buddy || !buddy->free || buddy->order != block->order)
    {
        return block;
    }
    remove_from_free_list(buddy);
    remove_from_free_list(block);
    block_t lower_block = (uintptr_t)block < (uintptr_t)buddy ? block : buddy;
    lower_block->size *= 2;
    lower_block->order += 1;
    lower_block->free = 1;
    lower_block->start_addr = block->start_addr; 
    return merge_blocks(lower_block);
}
void *kalloc(size_t size)
{
    size_t user_data_size = size;

    if (user_data_size > 0)
    {
        user_data_size = round_up_to_power_of_2(user_data_size);
    }
    size_t header_size = ((sizeof(struct block_t) + sizeof(size_t) - 1) / sizeof(size_t)) * sizeof(size_t);
    size_t total_size = user_data_size + header_size;
    int req_order = get_order(total_size);
    if (req_order < get_order(MIN_BLOCK_SIZE))
    {
        req_order = get_order(MIN_BLOCK_SIZE);
    }
    int tid = gettid() % THREAD_NUM;
    kmt->spin_lock(&thread_lock[tid]);
    int order = req_order;
    block_t block = NULL;
    while (order <= MAX_ORDER)
    {
        block_t *prev = &free_lists[tid][order];
        block_t curr = *prev;
        while (curr != NULL)
        {
            if (curr->free && curr->order >= req_order)
            {
                *prev = curr->next;
                block = curr;
                order = curr->order;
                break;
            }
            prev = &(curr->next);
            curr = curr->next;
        }
        if (block)
            break; 
        order++;
    }

    if (!block)
    {
        kmt->spin_unlock(&thread_lock[tid]);
        panic( "No free block found");
        return NULL; 
    }
    if (order > req_order)
    {
        block = split_block(block, req_order);
    }
    block->free = 0;
    void *user_ptr = (void *)((uintptr_t)block + header_size);
    uintptr_t user_addr = (uintptr_t)user_ptr;
    if (user_data_size > 0 && (user_addr & (user_data_size - 1)) != 0)
    {
        size_t alignment_offset = ((user_addr + user_data_size - 1) & ~(user_data_size - 1)) - user_addr;
        user_addr += alignment_offset;
        user_ptr = (void *)user_addr;
        block->offset = header_size + alignment_offset;
    }
    else
    {
        block->offset = header_size;
    }
    kmt->spin_unlock(&thread_lock[tid]);
    printf("alloc ptr:%p count :%d\n",user_ptr,size);
    return user_ptr;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;
    int tid = gettid() % THREAD_NUM;
    block_t possible_block = (block_t)((uintptr_t)ptr - sizeof(struct block_t));
    block_t block = NULL;
    if (possible_block && possible_block->offset > 0 &&
        possible_block->offset <= 1024 && // 设置一个合理的上限
        (uintptr_t)ptr == (uintptr_t)possible_block + possible_block->offset)
    {
        block = possible_block;
    }
    else
    {
        for (size_t offset = sizeof(struct block_t); offset <= sizeof(struct block_t) * 2; offset += 8)
        {
            possible_block = (block_t)((uintptr_t)ptr - offset);
            if (possible_block && possible_block->offset == offset &&
                possible_block->order >= 0 && possible_block->order <= MAX_ORDER)
            {
                block = possible_block;
                break;
            }
        }

        if (!block)
        {
            for (size_t offset = 8; offset <= 128; offset += 8)
            {
                possible_block = (block_t)((uintptr_t)ptr - offset);
                if (possible_block && possible_block->offset == offset)
                {
                    block = possible_block;
                    break;
                }
            }
        }
    }
    if (!block)
    {
        return;
    }

    block->free = 1;
    kmt->spin_lock(&thread_lock[tid]);
    block = merge_blocks(block);
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
        block->start_addr = block;
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
