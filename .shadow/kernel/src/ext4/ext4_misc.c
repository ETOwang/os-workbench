#include <common.h>

// 实现calloc函数
void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = pmm->alloc(total);
    panic_on(ptr == NULL, "Failed to allocate memory in calloc");
    memset(ptr, 0, total);
    return ptr;
}

// 实现qsort函数
static void swap(void *a, void *b, size_t size)
{
    // 使用小的临时缓冲区，避免大量栈使用
    char tmp[64];
    char *ap = (char *)a;
    char *bp = (char *)b;
    
    // 对于大对象，分块交换
    while (size > 0) {
        size_t chunk = size > sizeof(tmp) ? sizeof(tmp) : size;
        memcpy(tmp, ap, chunk);
        memcpy(ap, bp, chunk);
        memcpy(bp, tmp, chunk);
        ap += chunk;
        bp += chunk;
        size -= chunk;
    }
}

// 对小数组使用插入排序
static void insertion_sort(void *base, size_t nmemb, size_t size,
                          int (*compar)(const void *, const void *))
{
    char *arr = (char *)base;
    for (size_t i = 1; i < nmemb; i++) {
        // 保存当前元素
        char temp[64];
        char *key = arr + i * size;
        size_t remaining = size;
        char *temp_ptr = temp;
        
        while (remaining > 0) {
            size_t chunk = remaining > sizeof(temp) ? sizeof(temp) : remaining;
            memcpy(temp_ptr, key, chunk);
            temp_ptr += chunk;
            remaining -= chunk;
        }
        
        // 查找插入位置
        size_t j = i;
        while (j > 0 && compar(arr + (j-1) * size, temp) > 0) {
            memcpy(arr + j * size, arr + (j-1) * size, size);
            j--;
        }
        
        // 插入元素
        if (j != i) {
            memcpy(arr + j * size, temp, size);
        }
    }
}

static void _qsort(void *base, size_t nmemb, size_t size,
                   int (*compar)(const void *, const void *),
                   size_t left, size_t right)
{
    // 对小数组使用插入排序
    if (right - left <= 16) {
        insertion_sort((char *)base + left * size, right - left + 1, size, compar);
        return;
    }
    
    if (left >= right)
        return;
    
    // 选择pivot (使用中间元素)
    size_t pivot_idx = left + (right - left) / 2;
    
    // 将pivot移到最右边
    swap((char *)base + pivot_idx * size, (char *)base + right * size, size);
    
    // 分区过程
    size_t store_idx = left;
    
    for (size_t i = left; i < right; i++) {
        if (compar((char *)base + i * size, (char *)base + right * size) <= 0) {
            swap((char *)base + i * size, (char *)base + store_idx * size, size);
            store_idx++;
        }
    }
    
    // 将pivot放回正确位置
    swap((char *)base + store_idx * size, (char *)base + right * size, size);
    
    // 递归排序两个子数组
    if (store_idx > 0)
        _qsort(base, nmemb, size, compar, left, store_idx - 1);
    _qsort(base, nmemb, size, compar, store_idx + 1, right);
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    if (nmemb <= 1)
        return;
    _qsort(base, nmemb, size, compar, 0, nmemb - 1);
}



