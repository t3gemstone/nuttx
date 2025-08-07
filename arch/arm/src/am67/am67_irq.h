
#define VIM_BIT_POS(j)   ( (j) & 0x1Fu )
#define VIM_IRQVEC       (0x18u)
#define VIM_ACTIRQ       (0x20u)
#define VIM_STS(j)       (0x404U + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_EN(j)    (0x408U + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_DIS(j)   (0x40CU + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_MAP(j)   (0x418U + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_TYPE(j)  (0x41CU + ((((j) >> 5) & 0xFU) * 0x20U))
#define VIM_INT_PRI(j)   (0x1000u + ((j) * 0x4u))
#define VIM_INT_VEC(j)   (0x2000u + ((j) * 0x4u))

#define INTC_BASE_ADDR  0x2FFF0000u

#define INTR_MAX_PRIORITY   (16u)
#define MAX_INTERRUPTS      (512u)

void enable_intr(uint32_t intr_num);
uint32_t disable_intr(uint32_t intr_num);
void clear_intr(uint32_t intr_num);
void irq_init(void);
void intr_enable(void);
void irq_handler(void);

typedef int (*fxn_callback)(int irq, void *context, void *args);

struct intr
{
    uint32_t intr_num;
    fxn_callback callback;
    void *args;
    uint16_t event_id;
    uint8_t priority;
    // uint8_t is_fiq;
    // uint8_t is_pulse;
};

struct intr_ctrl
{
    fxn_callback isr[MAX_INTERRUPTS];
    void *isr_args[MAX_INTERRUPTS];
};

extern struct intr_ctrl gintr_ctrl;

static inline void intr_set_priority(uint32_t intr_num, uint32_t priority)
{
    volatile uint32_t *addr;
    addr = (volatile uint32_t *)(INTC_BASE_ADDR + VIM_INT_PRI(intr_num));
    *addr = (priority & 0xFu);
}

static inline void set_vector(uint32_t intr_num, uintptr_t vect_addr)
{
    volatile uint32_t *addr;
    addr = (volatile uint32_t *)(INTC_BASE_ADDR + VIM_INT_VEC(intr_num));
    *addr = ((uint32_t)vect_addr & 0xFFFFFFFCU);
}

static inline void ack_irq(uint32_t intr_num)
{
    volatile uint32_t *addr;

    addr = (volatile uint32_t *)(INTC_BASE_ADDR + VIM_IRQVEC);
    *addr = intr_num;
}

static inline int32_t get_irq(uint32_t *intr_num)
{
    volatile uint32_t *addr;
    int32_t ret = 1; // Error
    uint32_t value;

    *intr_num = 0;

    addr = (volatile uint32_t *)(INTC_BASE_ADDR + VIM_ACTIRQ);
    value = *addr;

    if((value & 0x80000000) != 0U)
    {
        *intr_num = (value & (MAX_INTERRUPTS - 1U));
        ret = 0; // OK
    }
    return ret;
}
