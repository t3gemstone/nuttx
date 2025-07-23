#define TIMER_TCLR_OFFSET      0x38u
#define TIMER_TCRR_OFFSET      0x3Cu
#define TIMER_TLDR_OFFSET      0x40u


struct timer
{
    uint32_t clock_hz;
    uint32_t prescaler;
    uint32_t period_usec;
    uint32_t one_shot;      // oneshotmode, runs continuously
    uint32_t interrupt;     // Interrup occurs when timer overflows
    uint32_t dma;
};

void timer_setup(uint32_t base_addr, struct timer *params);
void timer_start(uint32_t base_addr);
void timer_stop(uint32_t base_addr);
uint32_t get_count(uint32_t base_addr);
uint32_t get_reload(uint32_t base_addr);
