#define TIMER_IRQ_EOI           (0x20u)
#define TIMER_IRQ_STATUS_RAW    (0x24u)
#define TIMER_IRQ_STATUS        (0x28u)
#define TIMER_IRQ_INT_ENABLE    (0x2Cu)
#define TIMER_IRQ_INT_DISABLE   (0x30u)
#define TIMER_TCLR_OFFSET	(0x38u)
#define TIMER_TCRR_OFFSET	(0x3Cu)
#define TIMER_TLDR_OFFSET	(0x40u)

#define TIMER_OVF_INT_SHIFT     (0x1)


struct timer
{
    uint32_t clock_hz;
    uint32_t prescaler;
    uint32_t period_usec;
    uint32_t one_shot;      	// oneshotmode, runs continuously
    uint32_t overflow_intr;     // Interrup occurs when timer overflows
    uint32_t dma;
};

void timer_setup(uint32_t base_addr, struct timer *params);
void timer_start(uint32_t base_addr);
void timer_stop(uint32_t base_addr);
uint32_t get_count(uint32_t base_addr);
uint32_t get_reload(uint32_t base_addr);
void clear_overflow_int(uint32_t base_addr);

