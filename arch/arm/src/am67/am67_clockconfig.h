#define MCU_TIMER0_CLOCK_SRC_MUX_ADDR       (0x4509060u)
#define MCU_TIMER0_CLOCK_SRC_HFOSC0_CLKOUT  (0x0u)
#define MCU_TIMER0_BASE_ADDR                (0x4800000u)

#define CSL_MCU_CTRL_MMR0_CFG0_BASE         (0x4500000UL)
#define CSL_MCU_CTRL_MMR0_CFG0_SIZE         (0x20000UL)

#define CSL_MAIN_CTRL_MMR_LOCKn_KICK0_OFFSET(n)     (0x1008 + 0x4000 * (n))
#define CSL_MCU_CTRL_MMR_LOCKn_KICK0_OFFSET(n)      (0x1008 + 0x4000 * (n))

#define KICK_LOCK_VAL                       (0x00000000U)
#define KICK0_UNLOCK_VAL                    (0x68EF3490U)
#define KICK1_UNLOCK_VAL                    (0xD172BC5AU)

#ifdef __cplusplus
extern "C"
{
#endif

struct clock_ctrl
{
    uint32_t ticks;
    // There was a list of clock structs here
    uint32_t period_usec;
    uint32_t base_addr;
    uint32_t reload_count;
};

struct clock_conf
{
    uint32_t base_addr;
    uint32_t hw_intr_num;
    uint32_t event_id;
    uint32_t clock_hz;
    uint32_t prescaler;
    uint32_t period_usec;
};

void clock_init(void);
void clock_deinit(void);

#ifdef __cplusplus
}
#endif
