#ifndef __ARCH_ARM_SRC_IMXRT_AM67_MPUINIT_H
#define __ARCH_ARM_SRC_IMXRT_AM67_MPUINIT_H


#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include "mpu.h"

#define NUM_OF_REGION 5

#define ARM67_REGISTER_START_ADDR 0x0
#define ARM67_TCMA_START_ADDR 0x0
#define ARM67_TCMB_START_ADDR 0x41010000
#define ARM67_MCU_MSRAM_START_ADDR 0x60000000
#define ARM67_DDR_START_ADDR 0x80000000

#define ARM67_REGISTER_SIZE 2UL * 1024 * 1024 * 1024
#define ARM67_TCMA_SIZE 32UL * 1024
#define ARM67_TCMB_SIZE 32UL * 1024
#define ARM67_MCU_MSRAM_SIZE 512UL * 1024

#define ARM67_DDR_SIZE 2UL * 1024 * 1024 * 1024

/** REGISTER_REGION
        Not Cacheable  
        Not Bufferable 
        Shareable
        execute never      
        P:RW   U:R  */
#define am67_register_region(base,size) \
    mpu_configure_region(base, size, MPU_RACR_S | \
                                     MPU_RACR_AP_RWRW)

/** TCMA REGION
        Bufferable      
        Cacheable      
        P:RW   U:R0  */
#define am67_tcma_region(base, size) \
    mpu_configure_region(base, size, MPU_RACR_TEX(1)  | \
                                     MPU_RACR_B       | \
                                     MPU_RACR_AP_RWRW   )  // Allow user RW access, executable

#define am67_tcmb_region(base, size) \
    mpu_configure_region(base, size, MPU_RACR_TEX(1)  | \
                                     MPU_RACR_B       | \
                                     MPU_RACR_AP_RWRW   )  // Allow user RW access, executable

#define am67_mcu_msram_region(base,size)  \
    mpu_configure_region(base, size, MPU_RACR_TEX(1)  | \
                                     MPU_RACR_C       | \
                                     MPU_RACR_B       | \
                                     MPU_RACR_AP_RWRW   )  // Allow user RW access, executable

#define am67_ddr_region(base,size) \
    mpu_configure_region(base, size, MPU_RACR_TEX(1)  | \
                                     MPU_RACR_S       | \
                                     MPU_RACR_C       | \
                                     MPU_RACR_B       | \
                                     MPU_RACR_AP_RWRW )    

void am67_mpu_initialize(void);


static inline void am67_mpu_disableBR(void)
{
    unsigned int sctlr = cp15_rdsctlr();
    sctlr &= ~(1 << 17);  // Clear bit 17 (disable background region)
    cp15_wrsctlr(sctlr);
}


static inline void Mpu_setRegionZero(uint32_t regionId)
{
    register uint32_t r0 asm("r0") = regionId;
    register uint32_t r1 asm("r1") = 0;
    register uint32_t r2 asm("r2") = 0;
    register uint32_t r3 asm("r3") = 0;

    asm volatile (
        "mcr p15, 0, %0, c6, c2, 0\n\t"  
        "mcr p15, 0, %1, c6, c1, 0\n\t"  
        "mcr p15, 0, %2, c6, c1, 2\n\t"  
        "mcr p15, 0, %3, c6, c1, 4\n\t"  
        :
        : "r"(r0), "r"(r1), "r"(r2), "r"(r3)
        : "memory"
    );
}



#endif // __ARCH_ARM_SRC_IMXRT_AM67_MPUINIT_H