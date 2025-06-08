#include <common.h>
#define MAX_HANDLER 64
static handler_record_t handlers[MAX_HANDLER];
static int handler_count = 0;
static void os_init()
{
    pmm->init();
    kmt->init();
    dev->init();
    printf("pmm, kmt, dev init done\n");
    uproc->init();
    vfs->init();
}
static void os_run()
{
    printf("Hello World from CPU #%d\n", cpu_current());
    iset(true);
    while (1)
        ;
}
/**
 * must be called before os_run
 */
static void os_on_irq(int seq, int event, handler_t handler)
{
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
}
/**
 * pay attention to dead lock
 */
static Context *os_trap(Event ev, Context *context)
{
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
    return next;
}
MODULE_DEF(os) = {
    .init = os_init,
    .trap = os_trap,
    .on_irq = os_on_irq,
    .run = os_run};
