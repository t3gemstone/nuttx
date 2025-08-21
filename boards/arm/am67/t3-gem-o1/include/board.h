#ifndef __BOARDS_ARM_AM67_T3_GEM_O1_INCLUDE_BOARD_H
#define __BOARDS_ARM_AM67_T3_GEM_O1_INCLUDE_BOARD_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define BOARD_FREQUENCY		600000000 	// 600 MHz core clock
#define BOARD_XTAL_FREQUENCY 	24000000	// 24 MHz external crystal

/****************************************************************************
 * R5F Specific Configurations
 ****************************************************************************/
#define CONFIG_T3_GEM_O1_CORE_FREQ  600000000

/****************************************************************************
 * Assembly Language Macros
 ****************************************************************************/
#ifdef __ASSEMBLY__
  .macro  config_sdram
  .endm
#endif /* __ASSEMBLY__ */

#endif /* __BOARDS_ARM_AM67_T3_GEM_O1_INCLUDE_BOARD_H */
