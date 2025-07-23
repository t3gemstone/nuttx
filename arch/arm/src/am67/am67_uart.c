#include <nuttx/config.h>
#include <stdlib.h>

#include "hardware/am67_memorymap.h"
#include "am67_uart.h"
#include "hardware/am67_uart.h"

/*
void uart_init(void)
{
    //volatile uint32_t *uart_reg = (uint32_t *)UART0_BASE;
    volatile uint32_t *uart_lcr = (uint32_t *)(UART0_BASE + AM67_UART_LCR_OFFSET);
    volatile uint32_t *uart_mcr = (uint32_t *)(UART0_BASE + AM67_UART_MCR_OFFSET);
    volatile uint32_t *uart_tcr = (uint32_t *)(UART0_BASE + AM67_UART_TCR_OFFSET);
    volatile uint32_t *uart_tlr = (uint32_t *)(UART0_BASE + AM67_UART_TLR_OFFSET);
    volatile uint32_t *uart_spr = (uint32_t *)(UART0_BASE + AM67_UART_SPR_OFFSET);
    volatile uint32_t *uart_mdr1 = (uint32_t *)(UART0_BASE + AM67_UART_MDR1_OFFSET);
    volatile uint32_t *uart_mdr2 = (uint32_t *)(UART0_BASE + AM67_UART_MDR2_OFFSET);
    volatile uint32_t *uart_scr = (uint32_t *)(UART0_BASE + AM67_UART_SCR_OFFSET);
    volatile uint32_t *uart_sysc = (uint32_t *)(UART0_BASE + AM67_UART_SYSC_OFFSET);
    volatile uint32_t *uart_wer = (uint32_t *)(UART0_BASE + AM67_UART_WER_OFFSET);
    volatile uint32_t *uart_rxfifo = (uint32_t *)(UART0_BASE + AM67_UART_RXFIFO_LVL_OFFSET);
    
    volatile uint32_t *uart_fcr = (uint32_t *)(UART0_BASE + AM67_UART_FCR_OFFSET);
    
    *uart_sysc = 0x02;	// Soft reset
    while ((*uart_sysc & 0x01) == 0);	// Wait to completion
    
    *uart_lcr = 0x03;
    
    *uart_lcr |= 0x80;
    
    uint32_t divisor = 48000000 / (16 * 115200);
    
    *((volatile uint32_t *)(UART0_BASE + AM67_UART_DLL_OFFSET)) = divisor & 0xFF;
    *((volatile uint32_t *)(UART0_BASE + AM67_UART_DLH_OFFSET)) = (divisor >> 8) & 0xFF;
    
    *uart_lcr &= ~0x80;
    
    *uart_fcr = 0x07;
    
    *uart_mcr = 0x03;
    *uart_tcr = 0x0;
    *uart_tlr = 0x01;
    *uart_spr = 0x00;
    *uart_mdr1 = 0x00;
    *uart_mdr2 = 0x00;
    *uart_scr = 0x00;
    *uart_sysc = 0x01;
    *uart_wer = 0x00;
    *uart_rxfifo = 0x01;
    
    
    
}
*/

void uart_init(void)
{

}
