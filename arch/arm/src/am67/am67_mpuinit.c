#include <nuttx/config.h>
#include <assert.h>
#include <stdint.h>
#include <sys/param.h>
#include <nuttx/userspace.h>
#include <arch/barriers.h>
#include "mpu.h"
#include "hardware/am67_memorymap.h"
#include "am67_mpuinit.h"
#include "arm_internal.h"
#include "am67_irq.h"


void am67_mpu_reset(){
  for( int i =0 ; i < NUM_OF_REGION ; i++){
    mpu_set_region_zero(i);
  }
  
}

void am67_mpu_initialize(void)
{

  mpu_control(false);
  am67_mpu_disableBR();
  am67_mpu_reset();

  am67_register_region(ARM67_REGISTER_START_ADDR,ARM67_REGISTER_SIZE);
  
  am67_tcma_region(ARM67_TCMA_START_ADDR,ARM67_TCMA_SIZE);

  am67_tcmb_region(ARM67_TCMB_START_ADDR, ARM67_TCMB_SIZE);
  
  am67_mcu_msram_region(ARM67_MCU_MSRAM_START_ADDR, ARM67_MCU_MSRAM_SIZE);
  
  am67_ddr_region(ARM67_DDR_START_ADDR, ARM67_DDR_SIZE);

  mpu_control(true);
  
  return;
}

