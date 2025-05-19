#include <common.h>
#define MAX_HANDLER 64
static handler_record_t handlers[MAX_HANDLER];
static int handler_count = 0;
static spinlock_t handler_lock;
static sem_t empty, full;
static void T_produce(void *arg)
{
    while (1)
    {
        kmt->sem_wait(&empty);
        printf("%d",(uintptr_t)arg);
        kmt->sem_signal(&full);
    }
}
static void T_consume(void *arg)
{
    while (1)
    {
        kmt->sem_wait(&full);
        printf("%d",(uintptr_t)arg);
        kmt->sem_signal(&empty);
    }
}
static void os_init()
{
    pmm->init();
    kmt->spin_init(&handler_lock, "handler_lock");
    kmt->init();
    dev->init();
    kmt->sem_init(&empty, "empty", 10000);
    kmt->sem_init(&full,  "fill",  0);
    for (int i = 0; i <32; i++) {
        kmt->create(pmm->alloc(sizeof(task_t)), "produce", T_produce, (void*)(uintptr_t)i);
        kmt->create(pmm->alloc(sizeof(task_t)), "consume", T_consume, (void*)(uintptr_t)i);
    }
}
static void os_run()
{
    printf("Hello World from CPU #%d\n", cpu_current());
    iset(true);
    while (1);
        
}
static void os_on_irq(int seq, int event, handler_t handler)
{
    kmt->spin_lock(&handler_lock);
    panic_on(handler_count >= MAX_HANDLER, "Handler limit reached");
    handlers[handler_count].seq = seq;
    handlers[handler_count].event = event;
    handlers[handler_count].handler = handler;
    handler_count++;
    for (int i = 0; i < handler_count; i++)
    {
        for (int j = 0; j < handler_count - i - 1; j++)
        {
            if (handlers[j].seq > handlers[j + 1].seq)
            {
                handler_record_t temp = handlers[j];
                handlers[j] = handlers[j + 1];
                handlers[j + 1] = temp;
            }
        }
    }
    kmt->spin_unlock(&handler_lock);
}
static Context *os_trap(Event ev, Context *context)
{
    kmt->spin_lock(&handler_lock);
    Context *next = NULL;
    for (int i = 0; i < handler_count; i++)
    {
        if (handlers[i].event == EVENT_NULL || handlers[i].event == ev.event)
        {
            Context *r = handlers[i].handler(ev, context);
            panic_on(r && next, "return to multiple contexts");
            if (r)
            {
                next = r;
            }
        }
    }
    panic_on(!next, "return to NULL context");
    kmt->spin_unlock(&handler_lock);
    return next;
}
MODULE_DEF(os) = {
    .init = os_init,
    .trap = os_trap,
    .on_irq = os_on_irq,
    .run = os_run};
