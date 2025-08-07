/****************************************************************************
 * arch/arm/src/am67/am67_serial.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <string.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/serial/serial.h>

#include <nuttx/serial/uart_16550.h>

#include "arm_internal.h"
#include "chip.h"
#include "am67_serial.h"
#include "am67_lowput.h"
#include "am67_pinmux.h"

/****************************************************************************
 * Pre-processor definitions
 ****************************************************************************/

#if defined(USE_SERIALDRIVER) // && defined(HAVE_UART_DEVICE)

#define AM67_SCLK 48000000

#define CONSOLE_DEV         g_uart0port

#define UART0_RXBUFFSIZE	256	// bytes
#define UART0_TXBUFFSIZE	256	// bytes

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct am67_dev_s
{
    uint32_t uart_base;     // Base addres of the UART register
    uint32_t baud;
    uint32_t ier;           // Saved IER value
    uint8_t  irq;           // IRQ associated with this UART
    uint8_t  parity;        // 0=none, 1=odd, 2=even
    uint8_t  bits;          // Number of bits (7 or 8)
    bool     stopbits2;     // true: Configure with 2 stop bits instead of 1
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  am67_setup(struct uart_dev_s *dev);
static void am67_shutdown(struct uart_dev_s *dev);
static int am67_attach(struct uart_dev_s *dev);
static int uart_interrupt(int irq, void *context, void *arg);
static int am67_receive(struct uart_dev_s *dev, unsigned int *status);
static void am67_rxint(struct uart_dev_s *dev, bool enable);
static bool am67_rxavailable(struct uart_dev_s *dev);
static void am67_send(struct uart_dev_s *dev, int ch);
static bool am67_txready(struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_uart_ops =
{
    .setup = am67_setup,
    .shutdown = am67_shutdown,
    .attach = am67_attach,
    //.detach = ,
    //.ioctl = ,
    .receive = am67_receive,
    .rxint = am67_rxint,
    .rxavailable = am67_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
    //.rxflowcontrol = ,
#endif
    .send = am67_send,
    //.txint = ,
    .txready = am67_txready,
    //.txempty =
};


static char g_uart0rxbuffer[UART0_RXBUFFSIZE];
static char g_uart0txbuffer[UART0_TXBUFFSIZE];

static struct am67_dev_s g_uart0priv =
{
    .uart_base = CONSOLE_BASE,
    .baud = CONSOLE_BAUD,
    //.irq = ,
    .parity = CONSOLE_PARITY,
    .bits = CONSOLE_BITS,
    .stopbits2 = CONSOLE_2STOP
};

static uart_dev_t g_uart0port =
{
    .recv =
    {
        .size = UART0_RXBUFFSIZE,
        .buffer = g_uart0rxbuffer
    },
    .xmit =
    {
        .size = UART0_TXBUFFSIZE,
        .buffer = g_uart0txbuffer
    },
    .ops = &g_uart_ops,
    .priv = &g_uart0priv
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
 
static inline uint32_t am67_serialin(struct am67_dev_s *priv, int offset)
{
    return getreg32(priv->uart_base + offset);
}

static inline void am67_serialout(struct am67_dev_s *priv, int offset, uint32_t value)
{
    putreg32(value, priv->uart_base + offset);
}

static inline void am67_disableuartint(struct am67_dev_s *priv, uint32_t *ier)
{
    if (ier != NULL)
        *ier = priv->ier & UART_IER_ALLIE;
    priv->ier &= ~UART_IER_ALLIE;
    am67_serialout(priv, AM67_UART_IER_UART_OFFSET, priv->ier);
}

static inline void am67_restoreuartint(struct am67_dev_s *priv, uint32_t ier)
{
    priv->ier |= ier & UART_IER_ALLIE;
    am67_serialout(priv, AM67_UART_IER_UART_OFFSET, priv->ier);
}

static inline uint32_t am67_uartdl(uint32_t baud)
{
    return AM67_SCLK / (baud << 4);
}

static int am67_setup(struct uart_dev_s *dev)
{
#ifndef CONFIG_SUPPRESS_UART_CONFIG
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    uint16_t dl;
    uint32_t lcr;

    /* Put UART to disabled mode */
    am67_serialout(priv, AM67_UART_MDR1_OFFSET, UART_MDR1_MODE_SELECT_MODE_SELECT_VALUE_0 << UART_MDR1_MODE_SELECT_SHIFT);

    /* Enter configuration mode */
    am67_serialout(priv, AM67_UART_LCR_OFFSET, UART_REG_CONFIG_MODE_B);

    /* Set divisor values to zero to be able to write FCR correctly */
    am67_serialout(priv, AM67_UART_DLH_OFFSET, 0);
    am67_serialout(priv, AM67_UART_DLL_OFFSET, 0);

    /* Enable writing FCR */
    am67_serialout(priv, AM67_UART_EFR_OFFSET, UART_EFR_ENHANCED_EN_ENHANCED_EN_U_VALUE_1 << UART_EFR_ENHANCED_EN_SHIFT);
    
    /* Exit configuration mode */
    am67_serialout(priv, AM67_UART_LCR_OFFSET, UART_REG_CONFIG_MODE_A);
    
    /* Clear FIFOs */
    am67_serialout(priv, AM67_UART_FCR_OFFSET,
    	(UART_FCR_RX_FIFO_CLEAR_RX_FIFO_CLEAR_VALUE_1 << UART_FCR_RX_FIFO_CLEAR_SHIFT |
    	UART_FCR_TX_FIFO_CLEAR_TX_FIFO_CLEAR_VALUE_1 << UART_FCR_TX_FIFO_CLEAR_SHIFT));
    	
    /* Configure the FIFOs */
    am67_serialout(priv, AM67_UART_FCR_OFFSET,
    	(UART_FCR_FIFO_EN_FIFO_EN_VALUE_1 << UART_FCR_FIFO_EN_SHIFT |
    	3 << UART_FCR_RX_FIFO_TRIG_SHIFT | 	// UART_FCR_RFT_60CHAR
    	3 << UART_FCR_TX_FIFO_TRIG_SHIFT	// UART_FCR_TFT_56CHAR
    	));
    	
    /* Set up the IER */
    priv->ier = am67_serialin(priv, AM67_UART_IER_UART_OFFSET);
    
    /* Set up the LCR */
    lcr = 0;
    
    switch (priv->bits)
    {
        case 5:
            lcr |= (UART_LCR_CHAR_LENGTH_CHAR_LENGTH_VALUE_0 << UART_LCR_CHAR_LENGTH_SHIFT);
            break;
        case 6:
            lcr |= (UART_LCR_CHAR_LENGTH_CHAR_LENGTH_VALUE_1 << UART_LCR_CHAR_LENGTH_SHIFT);
            break;
        case 7:
            lcr |= (UART_LCR_CHAR_LENGTH_CHAR_LENGTH_VALUE_2 << UART_LCR_CHAR_LENGTH_SHIFT);
            break;
        case 8:
        default:
            lcr |= (UART_LCR_CHAR_LENGTH_CHAR_LENGTH_VALUE_3 << UART_LCR_CHAR_LENGTH_SHIFT);
            break;
    }
    
    if (priv->stopbits2)
        lcr |= UART_LCR_NB_STOP_NB_STOP_VALUE_1 << UART_LCR_NB_STOP_SHIFT;
        
    if (priv->parity == 1)
    	lcr |= (UART_LCR_PARITY_EN_PARITY_EN_VALUE_1 << UART_LCR_PARITY_EN_SHIFT |
    	UART_LCR_PARITY_TYPE1_PARITY_TYPE1_VALUE_0 << UART_LCR_PARITY_TYPE1_SHIFT);
    else if (priv->parity == 2)
        lcr |= (UART_LCR_PARITY_EN_PARITY_EN_VALUE_1 << UART_LCR_PARITY_EN_SHIFT |
        UART_LCR_PARITY_TYPE1_PARITY_TYPE1_VALUE_1 << UART_LCR_PARITY_TYPE1_SHIFT);
        
    /* Set the BAUD divisor */
    dl = am67_uartdl(priv->baud);
    am67_serialout(priv, AM67_UART_DLH_OFFSET, (dl >> 8) & UART_DLH_CLOCK_MSB_MASK);
    am67_serialout(priv, AM67_UART_DLL_OFFSET, dl & UART_DLL_CLOCK_LSB_MASK);
    
    /* Clear DLAB */
    am67_serialout(priv, AM67_UART_LCR_OFFSET, lcr);
    
#if defined (CONFIG_SERIAL_IFLOWCONTROL) || defined(CONFIG_SERIAL_OFLOWCONTROL)
#  warning Missing logic
#endif

    am67_serialout(priv, AM67_UART_MDR1_OFFSET, UART_MDR1_MODE_SELECT_MODE_SELECT_VALUE_0 << UART_MDR1_MODE_SELECT_SHIFT);
    
#endif
    return OK;
}

static void am67_shutdown(struct uart_dev_s *dev)
{
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    am67_disableuartint(priv, NULL);
}

static int am67_attach(struct uart_dev_s *dev)
{
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    int ret;

    /* Attach and enable the IRQ */
    ret = irq_attach(priv->irq, uart_interrupt, dev);
    if (ret == OK)
    {
        /* Enable the interrupt (RX and TX interrupts are still disabled
         * in the UART
         */
        //am67_enable_irq(priv->irq); TODO implement later
    }
    
    return ret;
}

static int uart_interrupt(int irq, void *context, void *arg)
{
    struct uart_dev_s *dev = (struct uart_dev_s *)arg;
    struct am67_dev_s *priv;
    uint32_t status;
    int passes;

    DEBUGASSERT(dev != NULL && dev->priv != NULL);
    priv = (struct am67_dev_s *)dev->priv;

    /* Loop until there are no characters to be transferred or,
     * until we have been looping for a long time.
     */

  for (passes = 0; passes < 256; passes++)
  {
      /* Get the current UART status */
      status = am67_serialin(priv, AM67_UART_IIR_UART_OFFSET);

      /* Handle the interrupt by its interrupt ID field */
      switch (status & 63)  // Taken directly by the am335x implementation, not sure about its correctness
      {
          /* Handle incoming, receive bytes (with or without timeout) */

          case UART_INTID_RX_THRES_REACH:
          case UART_INTID_CHAR_TIMEOUT: // Indicates a Receiver Timeout
          {
              uart_recvchars(dev);
              break;
          }

          /* Handle outgoing, transmit bytes */
          case UART_INTID_TX_THRES_REACH:
          {
              uart_xmitchars(dev);
              break;
          }

          /* There was more complicated cases here... Ignored. */

          default:
              break;
      }
  }
  return OK;
}

static int am67_receive(struct uart_dev_s *dev, unsigned int *status)
{
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    uint32_t rhr;

    *status = am67_serialin(priv, AM67_UART_LSR_UART_OFFSET);
    rhr = am67_serialin(priv, AM67_UART_RHR_OFFSET);
    return rhr;
}

static void am67_rxint(struct uart_dev_s *dev, bool enable)
{
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    if (enable)
    {
        priv->ier |= (UART_IER_RHR_IT_RHR_IT_VALUE_1 << UART_IER_RHR_IT_SHIFT);
    }
    else
    {
        priv->ier &= ~(UART_IER_RHR_IT_RHR_IT_VALUE_1 << UART_IER_RHR_IT_SHIFT);
    }
    
    am67_serialout(priv, AM67_UART_IER_UART_OFFSET, priv->ier);
}

static bool am67_rxavailable(struct uart_dev_s *dev)
{
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    return ((am67_serialin(priv, AM67_UART_LSR_UART_OFFSET) & (UART_LSR_RX_FIFO_E_RX_FIFO_E_VALUE_1 << UART_LSR_RX_FIFO_E_SHIFT)) != 0);
}

static void am67_send(struct uart_dev_s *dev, int ch)
{
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    am67_serialout(priv, AM67_UART_THR_OFFSET, (uint32_t)ch);

}

static bool am67_txready(struct uart_dev_s *dev)
{
    struct am67_dev_s *priv = (struct am67_dev_s *)dev->priv;
    return am67_serialin(priv, AM67_UART_LSR_UART_OFFSET & (UART_LSR_TX_FIFO_E_TX_FIFO_E_VALUE_1 << UART_LSR_TX_FIFO_E_SHIFT));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/*
void arm_earlyserialinit(void)
{
  pinmux_init();
  am67_disableuartint(&g_uart0priv, NULL);

  CONSOLE_DEV.isconsole = true;
  am67_setup(&CONSOLE_DEV);
  
}
*/

void arm_serialinit(void)
{
    u16550_earlyserialinit();
    u16550_serialinit();
    
    // Old driver
    /*
    arm_earlyserialinit();
    uart_driver_init();
    
    uart_register("/dev/console", &CONSOLE_DEV);
    
    am67_send(&g_uart0port, 'B');
    am67_send(&g_uart0port, '\n');
    */
}

/*
void up_putc(int ch)
{
//#ifdef CONFIG_SERIAL_CONSOLE
    struct am67_dev_s *priv = (struct am67_dev_s *)CONSOLE_DEV.priv;
    uint32_t ier;
    am67_disableuartint(priv, &ier);
//#endif

    arm_lowputc(ch);
//#ifdef CONFIG_SERIAL_CONSOLE
    am67_restoreuartint(priv, ier);
//#endif
}
*/


uart_datawidth_t uart_getreg(FAR struct u16550_s *priv, unsigned int offset)
{
    volatile uint32_t *reg = (volatile uint32_t *)(priv->uartbase + offset);
    return *reg;
}

void uart_putreg(FAR struct u16550_s *priv, unsigned int offset, uart_datawidth_t value)
{
    volatile uint32_t *reg = (volatile uint32_t *)(priv->uartbase + offset);
    *reg = value;
}

#endif /* USE_SERIALDRIVER */
