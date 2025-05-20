#include "addr_translate.h"
#include "pinmux.h"
#include "cslr_soc_base.h"



#define CSL_MAIN_PADCONFIG_LOCK0_KICK0_OFFSET   (0x1008)
#define CSL_MAIN_PADCONFIG_LOCK1_KICK0_OFFSET   (0x5008)
#define CSL_MCU_PADCONFIG_LOCK0_KICK0_OFFSET    (0x1008)
#define CSL_MCU_PADCONFIG_LOCK1_KICK0_OFFSET    (0x5008)

/* define the unlock and lock values */
#define KICK_LOCK_VAL                           (0x00000000U)
#define KICK0_UNLOCK_VAL                        (0x68EF3490U)
#define KICK1_UNLOCK_VAL                        (0xD172BC5AU)

#define PADCFG_PMUX_OFFSET                      (0x4000U)

/**
 *  \anchor Pinmux_DomainId_t
 *  \name Pinmux Domain ID
 *  @{
 */
#define PINMUX_DOMAIN_ID_MAIN          (0U)
#define PINMUX_DOMAIN_ID_MCU           (1U)
/** @} */

/** \brief Macro to mark end of pinmux config array */
#define PINMUX_END                      (-1)

/** \brief Pin mode - it is at 0th bit. No shift requried */
#define PIN_MODE(mode)                  ((uint32_t) mode)
/** \brief Resistor enable */
#define PIN_PULL_DISABLE                (((uint32_t) 0x1U) << 16U)
/** \brief Pull direction */
#define PIN_PULL_DIRECTION              (((uint32_t) 0x1U) << 17U)
/** \brief Receiver enable */
#define PIN_INPUT_ENABLE                (((uint32_t) 0x1U) << 18U)
/** \brief Driver disable */
#define PIN_OUTPUT_DISABLE              (((uint32_t) 0x1U) << 21U)
/** \brief Wakeup enable */
#define PIN_WAKEUP_ENABLE               (((uint32_t) 0x1U) << 29U)
static void Pinmux_lockMMR(uint32_t domainId);
static void Pinmux_unlockMMR(uint32_t domainId);


void Pinmux_config(const Pinmux_PerCfg_t *pinmuxCfg, uint32_t domainId)
{
    uint32_t            baseAddr;
    volatile uint32_t  *regAddr;

    if(NULL != pinmuxCfg)
    {
        if(PINMUX_DOMAIN_ID_MAIN == domainId)
        {
            baseAddr = CSL_PADCFG_CTRL0_CFG0_BASE + PADCFG_PMUX_OFFSET;
        }
        else
        {
            baseAddr =  CSL_MCU_PADCFG_CTRL0_CFG0_BASE + PADCFG_PMUX_OFFSET;
        }
        baseAddr = (uint32_t) AddrTranslateP_getLocalAddr(baseAddr);

        Pinmux_unlockMMR(domainId);
        while( pinmuxCfg->offset != PINMUX_END )
        {
            regAddr = (volatile uint32_t *)(baseAddr + pinmuxCfg->offset);
            CSL_REG32_WR(regAddr, pinmuxCfg->settings);
            pinmuxCfg++;
        }
        Pinmux_lockMMR(domainId);
    }

    return;
}

void Pinmux_lockMMR(uint32_t domainId)
{
    #if 0
    uint32_t            baseAddr;
    volatile uint32_t  *kickAddr;
    #endif

    if(PINMUX_DOMAIN_ID_MAIN == domainId)
    {
        #if 0 /* in AM62Ax, main dowmin MMRs are left unlocked since when working with linux kernel, linux kernel assumes MMRs are unlocked */
        baseAddr = (uint32_t) AddrTranslateP_getLocalAddr(CSL_PADCFG_CTRL0_CFG0_BASE);
        /* Lock 0 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MAIN_PADCONFIG_LOCK0_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 1 */

        /* Lock 1 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MAIN_PADCONFIG_LOCK1_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 1 */
        #endif
    }

    if(PINMUX_DOMAIN_ID_MCU == domainId)
    {
         #if 0 /* in AM62Ax, linux kernel assumes MCU PADConfig MMR is unlocked. */
        baseAddr = (uint32_t) AddrTranslateP_getLocalAddr(CSL_WKUP_PADCFG_CTRL0_CFG0_BASE);
        /* Lock 0 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MCU_PADCONFIG_LOCK0_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 1 */

        /* Lock 1 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MCU_PADCONFIG_LOCK1_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK_LOCK_VAL);      /* KICK 1 */
        #endif
    }

    return;
}

void Pinmux_unlockMMR(uint32_t domainId)
{
    uint32_t            baseAddr;
    volatile uint32_t  *kickAddr;

    if(PINMUX_DOMAIN_ID_MAIN == domainId)
    {
        baseAddr = (uint32_t) AddrTranslateP_getLocalAddr(CSL_PADCFG_CTRL0_CFG0_BASE);
        /* Lock 0 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MAIN_PADCONFIG_LOCK0_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK0_UNLOCK_VAL);   /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK1_UNLOCK_VAL);   /* KICK 1 */

        /* Lock 1 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MAIN_PADCONFIG_LOCK1_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK0_UNLOCK_VAL);   /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK1_UNLOCK_VAL);   /* KICK 1 */
    }

    if(PINMUX_DOMAIN_ID_MCU == domainId)
    {
        baseAddr = (uint32_t) AddrTranslateP_getLocalAddr(CSL_MCU_PADCFG_CTRL0_CFG0_BASE);
        /* Lock 0 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MCU_PADCONFIG_LOCK0_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK0_UNLOCK_VAL);   /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK1_UNLOCK_VAL);   /* KICK 1 */

        /* Lock 1 */
        kickAddr = (volatile uint32_t *) (baseAddr + CSL_MCU_PADCONFIG_LOCK1_KICK0_OFFSET);
        CSL_REG32_WR(kickAddr, KICK0_UNLOCK_VAL);   /* KICK 0 */
        kickAddr++;
        CSL_REG32_WR(kickAddr, KICK1_UNLOCK_VAL);   /* KICK 1 */
    }

    return;
}
