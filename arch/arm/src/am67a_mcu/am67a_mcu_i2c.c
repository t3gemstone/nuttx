/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_i2c.c
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

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/clock.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/i2c/i2c_master.h>

#include <arch/board/board.h>

#include "arm_internal.h"
#include "am67a_mcu_pinmux.h"
#include "am67a_mcu_i2c.h"
#include "am67a_mcu_i2c_hw.h"

#ifdef CONFIG_AM67A_MCU_I2C0

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_AM67A_MCU_I2C0_FCLK
#  define CONFIG_AM67A_MCU_I2C0_FCLK        (96000000u) /* 96 MHz */
#endif

/* Interrupt wait timeout in seconds and milliseconds */

#if !defined(CONFIG_AM67A_MCU_I2CTIMEOSEC) && !defined(CONFIG_AM67A_MCU_I2CTIMEOMS)
#  define CONFIG_AM67A_MCU_I2CTIMEOSEC 0
#  define CONFIG_AM67A_MCU_I2CTIMEOMS  500   /* 500 ms matching am67_i2c.c */
#elif !defined(CONFIG_AM67A_MCU_I2CTIMEOSEC)
#  define CONFIG_AM67A_MCU_I2CTIMEOSEC 0
#elif !defined(CONFIG_AM67A_MCU_I2CTIMEOMS)
#  define CONFIG_AM67A_MCU_I2CTIMEOMS  0
#endif

/* Interrupt wait time timeout in system timer ticks */

#ifndef CONFIG_AM67A_MCU_I2CTIMEOTICKS
#  define CONFIG_AM67A_MCU_I2CTIMEOTICKS \
    (SEC2TICK(CONFIG_AM67A_MCU_I2CTIMEOSEC) + MSEC2TICK(CONFIG_AM67A_MCU_I2CTIMEOMS))
#endif

#ifndef CONFIG_AM67A_MCU_I2C_DYNTIMEO_STARTSTOP
#  define CONFIG_AM67A_MCU_I2C_DYNTIMEO_STARTSTOP TICK2USEC(CONFIG_AM67A_MCU_I2CTIMEOTICKS)
#endif

/* Debug ********************************************************************/

#ifndef CONFIG_I2C_TRACE
#  define am67a_mcu_i2c_tracereset(p)
#  define am67a_mcu_i2c_tracenew(p,s)
#  define am67a_mcu_i2c_traceevent(p,e,a)
#  define am67a_mcu_i2c_tracedump(p)
#endif

#ifndef CONFIG_I2C_NTRACE
#  define CONFIG_I2C_NTRACE 32
#endif

#define I2C_MASTER      1
#define I2C_SLAVE       2

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Interrupt state */

enum am67a_mcu_intstate_e
{
  INTSTATE_IDLE = 0,      /* No I2C activity */
  INTSTATE_WAITING,       /* Waiting for completion of interrupt activity */
  INTSTATE_DONE,          /* Interrupt activity complete */
};

/* Trace events */

enum am67a_mcu_trace_e
{
  I2CEVENT_NONE = 0,      /* No events have occurred with this status */
  I2CEVENT_SENDADDR,      /* Start/Master bit set and address sent, param = msgc */
  I2CEVENT_SENDBYTE,      /* Send byte, param = dcnt */
  I2CEVENT_RCVBYTE,       /* Read more data, param = dcnt */
  I2CEVENT_NOSTART,       /* BTF on last byte with no restart, param = msgc */
  I2CEVENT_STARTRESTART,  /* Last byte sent, re-starting, param = msgc */
  I2CEVENT_STOP,          /* Last byte sent, send stop, param = 0 */
  I2CEVENT_ERROR          /* Error occurred, param = 0 */
};

/* Trace data */

struct am67a_mcu_trace_s
{
  uint32_t status;              /* IRQSTATUS at the time of the event */
  uint32_t count;               /* Interrupt count when status change */
  enum am67a_mcu_trace_e event; /* Last event that occurred with this status */
  uint32_t parm;                /* Parameter associated with the event */
  clock_t time;                 /* First of event or first status */
};

/* I2C Device hardware configuration */

struct am67a_mcu_i2c_config_s
{
  uint32_t base;              /* I2C base address */
  uint8_t mode;               /* Master or Slave mode */
#ifndef CONFIG_I2C_POLLED
  uint32_t irq;               /* Event IRQ */
#endif
};

/* I2C Device Private Data */

struct am67a_mcu_i2c_priv_s
{
  /* Standard I2C operations */

  const struct i2c_ops_s *ops;

  /* Port configuration */

  const struct am67a_mcu_i2c_config_s *config;

  int refs;                    /* Reference count */
  mutex_t lock;                /* Mutual exclusion mutex */
#ifndef CONFIG_I2C_POLLED
  sem_t sem_isr;               /* Interrupt wait semaphore */
#endif
  volatile uint8_t intstate;   /* Interrupt handshake */

  uint8_t msgc;                /* Message count */
  struct i2c_msg_s *msgv;      /* Message list */
  uint8_t *ptr;                /* Current message buffer */
  uint32_t frequency;          /* Current I2C frequency */
  int dcnt;                    /* Current message length */
  uint16_t flags;              /* Current message flags */

  /* I2C trace support */

#ifdef CONFIG_I2C_TRACE
  int tndx;                    /* Trace array index */
  clock_t start_time;          /* Time when the trace was started */
  struct am67a_mcu_trace_s trace[CONFIG_I2C_NTRACE];
#endif

  uint32_t status;             /* End of transfer IRQSTATUS */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static inline uint32_t am67a_mcu_i2c_getreg(struct am67a_mcu_i2c_priv_s *priv,
                                             uint16_t offset);
static inline void am67a_mcu_i2c_putreg(struct am67a_mcu_i2c_priv_s *priv,
                                         uint16_t offset, uint32_t value);
static inline void am67a_mcu_i2c_modifyreg(struct am67a_mcu_i2c_priv_s *priv,
                                            uint16_t offset, uint32_t clearbits,
                                            uint32_t setbits);

#ifdef CONFIG_AM67A_MCU_I2C_DYNTIMEO
static uint32_t am67a_mcu_i2c_toticks(int msgc, struct i2c_msg_s *msgs);
#endif

static inline int am67a_mcu_i2c_sem_waitdone(struct am67a_mcu_i2c_priv_s *priv);
static inline bool am67a_mcu_i2c_sem_waitstop(struct am67a_mcu_i2c_priv_s *priv);

#ifdef CONFIG_I2C_TRACE
static void am67a_mcu_i2c_tracereset(struct am67a_mcu_i2c_priv_s *priv);
static void am67a_mcu_i2c_tracenew(struct am67a_mcu_i2c_priv_s *priv,
                                   uint32_t status);
static void am67a_mcu_i2c_traceevent(struct am67a_mcu_i2c_priv_s *priv,
                                     enum am67a_mcu_trace_e event,
                                     uint32_t parm);
static void am67a_mcu_i2c_tracedump(struct am67a_mcu_i2c_priv_s *priv);
#endif

static void am67a_mcu_i2c_setclock(struct am67a_mcu_i2c_priv_s *priv,
                                   uint32_t frequency);
static void am67a_mcu_i2c_startmsg(struct am67a_mcu_i2c_priv_s *priv);
static inline void am67a_mcu_i2c_sendstop(struct am67a_mcu_i2c_priv_s *priv);
static inline uint32_t am67a_mcu_i2c_getstatus(struct am67a_mcu_i2c_priv_s *priv);

static int am67a_mcu_i2c_isr_process(struct am67a_mcu_i2c_priv_s *priv);

#ifndef CONFIG_I2C_POLLED
static int am67a_mcu_i2c_isr(int irq, void *context, void *arg);
#endif

static int am67a_mcu_i2c_bus_reset(struct am67a_mcu_i2c_priv_s *priv);
static int am67a_mcu_i2c_init(struct am67a_mcu_i2c_priv_s *priv);
static int am67a_mcu_i2c_deinit(struct am67a_mcu_i2c_priv_s *priv);
static int am67a_mcu_i2c_transfer(struct i2c_master_s *dev,
                                  struct i2c_msg_s *msgs, int count);

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_I2C_TRACE
static const char *g_trace_names[] =
{
  "NONE           ",
  "SENDADDR       ",
  "SENDBYTE       ",
  "RCVBYTE        ",
  "NOSTART        ",
  "START/RESTART  ",
  "STOP           ",
  "ERROR          "
};
#endif

/* I2C interface */

static const struct i2c_ops_s am67a_mcu_i2c_ops =
{
  .transfer = am67a_mcu_i2c_transfer
};

/* I2C device structures */

static const struct am67a_mcu_i2c_config_s am67a_mcu_i2c0_config =
{
  .base       = AM67A_MCU_I2C0_BASE,
  .mode       = I2C_MASTER,
#ifndef CONFIG_I2C_POLLED
  .irq        = AM67A_MCU_IRQ_I2C0,
#endif
};

static struct am67a_mcu_i2c_priv_s am67a_mcu_i2c0_priv =
{
  .ops        = &am67a_mcu_i2c_ops,
  .config     = &am67a_mcu_i2c0_config,
  .refs       = 0,
  .lock       = NXMUTEX_INITIALIZER,
#ifndef CONFIG_I2C_POLLED
  .sem_isr    = SEM_INITIALIZER(0),
#endif
  .intstate   = INTSTATE_IDLE,
  .msgc       = 0,
  .msgv       = NULL,
  .ptr        = NULL,
  .dcnt       = 0,
  .flags      = 0,
  .status     = 0
};

static const struct am67a_mcu_i2c_config_s am67a_mcu_i2c1_config =
{
  .base       = AM67A_MCU_WKUP_I2C0_BASE,
  .mode       = I2C_MASTER,
#ifndef CONFIG_I2C_POLLED
  .irq        = 0,
#endif
};

static struct am67a_mcu_i2c_priv_s am67a_mcu_i2c1_priv =
{
  .ops        = &am67a_mcu_i2c_ops,
  .config     = &am67a_mcu_i2c1_config,
  .refs       = 0,
  .lock       = NXMUTEX_INITIALIZER,
#ifndef CONFIG_I2C_POLLED
  .sem_isr    = SEM_INITIALIZER(0),
#endif
  .intstate   = INTSTATE_IDLE,
  .msgc       = 0,
  .msgv       = NULL,
  .ptr        = NULL,
  .dcnt       = 0,
  .flags      = 0,
  .status     = 0
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t am67a_mcu_i2c_getreg(struct am67a_mcu_i2c_priv_s *priv,
                                             uint16_t offset)
{
  return getreg32(priv->config->base + offset);
}

static inline void am67a_mcu_i2c_putreg(struct am67a_mcu_i2c_priv_s *priv,
                                         uint16_t offset, uint32_t value)
{
  putreg32(value, priv->config->base + offset);
}

static inline void am67a_mcu_i2c_modifyreg(struct am67a_mcu_i2c_priv_s *priv,
                                            uint16_t offset, uint32_t clearbits,
                                            uint32_t setbits)
{
  modifyreg32(priv->config->base + offset, clearbits, setbits);
}

#ifdef CONFIG_AM67A_MCU_I2C_DYNTIMEO
static uint32_t am67a_mcu_i2c_toticks(int msgc, struct i2c_msg_s *msgs)
{
  size_t bytecount = 0;
  int i;

  for (i = 0; i < msgc; i++)
    {
      bytecount += msgs[i].length;
    }

  return USEC2TICK(CONFIG_AM67A_MCU_I2C_DYNTIMEO_USECPERBYTE * bytecount);
}
#endif

#ifndef CONFIG_I2C_POLLED
static inline int am67a_mcu_i2c_sem_waitdone(struct am67a_mcu_i2c_priv_s *priv)
{
  irqstate_t flags;
  int ret;

  flags = enter_critical_section();

  priv->intstate = INTSTATE_WAITING;
  am67a_mcu_i2c_startmsg(priv);

  do
    {
#ifdef CONFIG_AM67A_MCU_I2C_DYNTIMEO
      ret = nxsem_tickwait(&priv->sem_isr,
                           am67a_mcu_i2c_toticks(priv->msgc, priv->msgv));
#else
      ret = nxsem_tickwait(&priv->sem_isr,
                           CONFIG_AM67A_MCU_I2CTIMEOTICKS);
#endif
      if (ret < 0 && ret != -EINTR)
        {
          break;
        }
    }
  while (priv->intstate != INTSTATE_DONE && ret >= 0);

  priv->intstate = INTSTATE_IDLE;
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET, I2C_ICR_CLEARMASK);

  leave_critical_section(flags);
  return ret;
}
#else
static inline int am67a_mcu_i2c_sem_waitdone(struct am67a_mcu_i2c_priv_s *priv)
{
  clock_t timeout;
  clock_t start;
  clock_t elapsed;
  int ret;

#ifdef CONFIG_AM67A_MCU_I2C_DYNTIMEO
  timeout = am67a_mcu_i2c_toticks(priv->msgc, priv->msgv);
#else
  timeout = CONFIG_AM67A_MCU_I2CTIMEOTICKS;
#endif

  priv->intstate = INTSTATE_WAITING;
  am67a_mcu_i2c_startmsg(priv);

  start = clock_systime_ticks();

  do
    {
      elapsed = clock_systime_ticks() - start;
      am67a_mcu_i2c_isr_process(priv);
    }
  while (priv->intstate != INTSTATE_DONE && elapsed < timeout);

  i2cinfo("intstate: %d elapsed: %ld threshold: %ld status: %08" PRIx32 "\n",
          priv->intstate, (long)elapsed, (long)timeout, priv->status);

  ret = priv->intstate == INTSTATE_DONE ? OK : -ETIMEDOUT;
  priv->intstate = INTSTATE_IDLE;
  return ret;
}
#endif

static int am67a_mcu_i2c_bus_reset(struct am67a_mcu_i2c_priv_s *priv)
{
  uint32_t count = 10000;
  uint32_t freq;

  /* Disable controller */

  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_CON_OFFSET, 0);

  /* Trigger software reset */

  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_SYSC_OFFSET, I2C_SYSC_SRST);
  am67a_mcu_i2c_modifyreg(priv, AM67A_MCU_I2C_CON_OFFSET, 0, I2C_CON_EN);

  while (!(am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_SYSS_OFFSET) &
           I2C_SYSS_RST_DONE))
    {
      if (--count == 0)
        {
          i2cerr("ERROR: I2C reset timeout\n");
          return -ETIMEDOUT;
        }
      up_udelay(1);
    }

  /* Set Clock Activity and Idle Mode */

  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_SYSC_OFFSET,
                       I2C_SYSC_IDLE_NO | I2C_SYSC_CLK_BOTH);

  freq = priv->frequency ? priv->frequency : 100000;
  priv->frequency = 0;
  am67a_mcu_i2c_setclock(priv, freq);

  am67a_mcu_i2c_modifyreg(priv, AM67A_MCU_I2C_CON_OFFSET, 0, I2C_CON_EN);

  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_WE_OFFSET,
                       I2C_WE_AL   | I2C_WE_NACK | I2C_WE_ARDY | I2C_WE_DRDY |
                       I2C_WE_GC   | I2C_WE_STC  | I2C_WE_BF   | I2C_WE_AAS  |
                       I2C_WE_XUDF | I2C_WE_ROVR | I2C_WE_RDR  | I2C_WE_XDR);

  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_BUF_OFFSET,
                       I2C_BUF_TXFIFO_CLR | I2C_BUF_RXFIFO_CLR);
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET, I2C_STS_CLEARMASK);
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET, I2C_ICR_CLEARMASK);

  return OK;
}

static inline bool am67a_mcu_i2c_sem_waitstop(struct am67a_mcu_i2c_priv_s *priv)
{
  clock_t start;
  clock_t elapsed;
  clock_t timeout;
  uint32_t regval;

#ifdef CONFIG_AM67A_MCU_I2C_DYNTIMEO
  timeout = USEC2TICK(CONFIG_AM67A_MCU_I2C_DYNTIMEO_STARTSTOP);
#else
  timeout = CONFIG_AM67A_MCU_I2CTIMEOTICKS;
#endif

  start = clock_systime_ticks();
  do
    {
      elapsed = clock_systime_ticks() - start;

      /* Check for Bus Free condition (I2C_IRQ_BB == 0) */

      regval = am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET);
      if ((regval & I2C_IRQ_BB) == 0)
        {
          return true;
        }
    }
  while (elapsed < timeout);

  /* Bus is busy after timeout; attempt bus reset to recover controller */

  i2cwarn("WARNING: I2C bus busy (RAW=0x%" PRIx32 "), attempting bus reset\n",
          regval);

  if (am67a_mcu_i2c_bus_reset(priv) == OK)
    {
      regval = am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET);
      if ((regval & I2C_IRQ_BB) == 0)
        {
          i2cinfo("I2C bus reset recovered bus (BB=0)\n");
          return true;
        }
    }

  i2cerr("Timeout with Status Register: %" PRIx32 "\n", regval);
  return false;
}

#ifdef CONFIG_I2C_TRACE
static void am67a_mcu_i2c_traceclear(struct am67a_mcu_i2c_priv_s *priv)
{
  struct am67a_mcu_trace_s *trace = &priv->trace[priv->tndx];

  trace->status = 0;
  trace->count  = 0;
  trace->event  = I2CEVENT_NONE;
  trace->parm   = 0;
  trace->time   = 0;
}

static void am67a_mcu_i2c_tracereset(struct am67a_mcu_i2c_priv_s *priv)
{
  priv->tndx       = 0;
  priv->start_time = clock_systime_ticks();
  am67a_mcu_i2c_traceclear(priv);
}

static void am67a_mcu_i2c_tracenew(struct am67a_mcu_i2c_priv_s *priv,
                                   uint32_t status)
{
  struct am67a_mcu_trace_s *trace = &priv->trace[priv->tndx];

  if (trace->count == 0)
    {
      trace->status = status;
      trace->count  = 1;
      trace->time   = clock_systime_ticks();
    }
  else if (trace->status == status)
    {
      trace->count++;
    }
  else
    {
      priv->tndx++;
      if (priv->tndx >= CONFIG_I2C_NTRACE)
        {
          priv->tndx = 0;
        }

      trace = &priv->trace[priv->tndx];
      trace->status = status;
      trace->count  = 1;
      trace->event  = I2CEVENT_NONE;
      trace->parm   = 0;
      trace->time   = clock_systime_ticks();
    }
}

static void am67a_mcu_i2c_traceevent(struct am67a_mcu_i2c_priv_s *priv,
                                     enum am67a_mcu_trace_e event,
                                     uint32_t parm)
{
  struct am67a_mcu_trace_s *trace;

  if (event != I2CEVENT_NONE)
    {
      trace = &priv->trace[priv->tndx];
      if (trace->event != event)
        {
          am67a_mcu_i2c_tracenew(priv, am67a_mcu_i2c_getstatus(priv));
          trace = &priv->trace[priv->tndx];
          trace->event = event;
          trace->parm  = parm;
          trace->count = 1;
          trace->time  = clock_systime_ticks();
        }
      else
        {
          trace->count++;
        }
    }
}

static void am67a_mcu_i2c_tracedump(struct am67a_mcu_i2c_priv_s *priv)
{
  struct am67a_mcu_trace_s *trace;
  int i;

  i2cinfo("Elapsed: %ld TNDX: %d\n",
          (long)(clock_systime_ticks() - priv->start_time), priv->tndx);

  for (i = 0; i < CONFIG_I2C_NTRACE; i++)
    {
      trace = &priv->trace[i];
      if (trace->count > 0)
        {
          i2cinfo("%2d. STATUS: %08" PRIx32 " COUNT: %3" PRIu32
                  " EVENT: %s PARM: %08" PRIx32 " TIME: %ld\n",
                  i + 1, trace->status, trace->count,
                  g_trace_names[trace->event], trace->parm,
                  (long)(trace->time - priv->start_time));
        }
    }
}
#endif

static void am67a_mcu_i2c_setclock(struct am67a_mcu_i2c_priv_s *priv,
                                   uint32_t frequency)
{
  uint32_t src_freq = CONFIG_AM67A_MCU_I2C0_FCLK;
  uint32_t internal_clk = 12000000; /* Standard 12 MHz internal sampling clock */
  uint32_t prescale;
  uint32_t divisor;
  uint32_t div_h;
  uint32_t div_l;
  uint32_t scl_low;
  uint32_t scl_hi;
  uint32_t men;

  if (frequency == 0)
    {
      frequency = 100000;
    }

  if (priv->config->mode == I2C_MASTER)
    {
      if (frequency != priv->frequency)
        {
          men = am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_CON_OFFSET) & I2C_CON_EN;
          if (men)
            {
              am67a_mcu_i2c_modifyreg(priv, AM67A_MCU_I2C_CON_OFFSET,
                                      I2C_CON_EN, 0);
            }

          /* TI K3 I2C internal clock calculation:
           * Target internal sampling clock is 12 MHz (valid range 7-12 MHz)
           */

          prescale = (src_freq / internal_clk) - 1;
          if (prescale > 255)
            {
              prescale = 255;
            }

          uint32_t act_int_clk = src_freq / (prescale + 1);

          /* Calculate divisor for output frequency */

          divisor = act_int_clk / frequency;
          if ((frequency * divisor) != act_int_clk)
            {
              divisor += 1;
            }

          div_h = divisor / 2;
          div_l = divisor - div_h;

          /* SCL Low: (div_l - 7), SCL High: (div_h - 5) */

          scl_low = (div_l > 7) ? (div_l - 7) : 5;
          scl_hi  = (div_h > 5) ? (div_h - 5) : 5;

          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_PSC_OFFSET, prescale);
          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_SCLL_OFFSET, scl_low);
          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_SCLH_OFFSET, scl_hi);

          if (men)
            {
              am67a_mcu_i2c_modifyreg(priv, AM67A_MCU_I2C_CON_OFFSET,
                                      0, I2C_CON_EN);
            }

          priv->frequency = frequency;
        }
    }
}

static void am67a_mcu_i2c_startmsg(struct am67a_mcu_i2c_priv_s *priv)
{
  struct i2c_msg_s *msg = priv->msgv;
  uint32_t regval;
  uint16_t total_len;
  struct i2c_msg_s *next;
  int count;

  priv->ptr   = msg->buffer;
  priv->dcnt  = msg->length;
  priv->flags = msg->flags;
  priv->msgv++;
  priv->msgc--;

  /* Calculate total transfer length for contiguous NOSTART segments */

  total_len = msg->length;
  next      = priv->msgv;
  count     = priv->msgc;

  while (count > 0 && (next->flags & I2C_M_NOSTART) != 0)
    {
      total_len += next->length;
      next++;
      count--;
    }

  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_SA_OFFSET, msg->addr);
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_CNT_OFFSET, total_len);

  regval = I2C_CON_EN | I2C_CON_MST;
  if ((priv->flags & I2C_M_READ) == 0)
    {
      regval |= I2C_CON_TRX;
    }

  if ((priv->flags & I2C_M_TEN) != 0)
    {
      regval |= I2C_CON_XSA;
    }

  /* Do not set I2C_CON_STP here. Setting STP prematurely before STT or
   * during Repeated START causes the TI K3 controller to immediately abort
   * and issue a STOP condition on the active bus. The controller will
   * complete the transfer based on CNT and assert ARDY, after which
   * am67a_mcu_i2c_isr_process() issues am67a_mcu_i2c_sendstop().
   */

  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_CON_OFFSET, regval);

#ifndef CONFIG_I2C_POLLED
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET,
                       I2C_IRQ_XRDY | I2C_IRQ_RRDY | I2C_IRQ_BF);
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_SET_OFFSET,
                       I2C_IRQ_AL | I2C_IRQ_NACK | I2C_IRQ_AERR | I2C_IRQ_ARDY |
                       (((priv->flags & I2C_M_READ) != 0) ?
                        I2C_IRQ_RRDY : I2C_IRQ_XRDY));
#endif

  /* Everything is described — now press go.
   * Note: TI K3 I2C requires CON_STT for every segment programmed via CNT.
   */

  am67a_mcu_i2c_traceevent(priv, I2CEVENT_SENDADDR, priv->msgc);
  am67a_mcu_i2c_modifyreg(priv, AM67A_MCU_I2C_CON_OFFSET, 0, I2C_CON_STT);
}

static inline void am67a_mcu_i2c_sendstop(struct am67a_mcu_i2c_priv_s *priv)
{
  am67a_mcu_i2c_modifyreg(priv, AM67A_MCU_I2C_CON_OFFSET, 0, I2C_CON_STP);
}

static inline uint32_t am67a_mcu_i2c_getstatus(struct am67a_mcu_i2c_priv_s *priv)
{
#ifndef CONFIG_I2C_POLLED
  return am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET);
#else
  return am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET);
#endif
}

static int am67a_mcu_i2c_isr_process(struct am67a_mcu_i2c_priv_s *priv)
{
  uint32_t status = am67a_mcu_i2c_getstatus(priv);
  uint32_t raw    = am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET);
  uint32_t fatal  = raw & (I2C_IRQ_NACK | I2C_IRQ_AL | I2C_IRQ_AERR);

  am67a_mcu_i2c_tracenew(priv, status);

  /* Rung 1: fatal errors preempt everything. Checked in the RAW status
   * so an error can never hide behind interrupt masking.
   */

  if (fatal != 0)
    {
      am67a_mcu_i2c_traceevent(priv, I2CEVENT_ERROR, fatal);

      /* Release the bus and reset the transaction engine, mirroring the
       * TI SDK fatal-error path. Without this a single NACK leaves the
       * master mid-transaction and every later transfer times out.
       */

      am67a_mcu_i2c_sendstop(priv);
      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET, I2C_STS_CLEARMASK);

#ifdef CONFIG_I2C_POLLED
      /* In polled mode, wait briefly for STOP to complete on wire */

      for (int i = 0; i < 1000; i++)
        {
          if ((am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET) &
               I2C_IRQ_BB) == 0)
            {
              break;
            }
          up_udelay(1);
        }
#endif

      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_BUF_OFFSET,
                           I2C_BUF_TXFIFO_CLR | I2C_BUF_RXFIFO_CLR);
      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_CNT_OFFSET, 0);

      priv->status = status | fatal;
      priv->msgv   = NULL;
      priv->dcnt   = 0;
      priv->ptr    = NULL;

#ifndef CONFIG_I2C_POLLED
      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET, I2C_ICR_CLEARMASK);

      if (priv->intstate == INTSTATE_WAITING)
        {
          nxsem_post(&priv->sem_isr);
          priv->intstate = INTSTATE_DONE;
        }
#else
      priv->intstate = INTSTATE_DONE;
#endif
      return OK;
    }

  /* Rung 2: a byte arrived — drain it BEFORE any phase decision so a
   * pending restart/stop cannot strand it in the FIFO.
   */

  if ((status & I2C_IRQ_RRDY) != 0)
    {
      if (priv->dcnt <= 0 && priv->msgc > 0 &&
          (priv->msgv->flags & I2C_M_NOSTART) != 0)
        {
          priv->ptr   = priv->msgv->buffer;
          priv->dcnt  = priv->msgv->length;
          priv->flags = priv->msgv->flags;
          priv->msgv++;
          priv->msgc--;
        }

      if (priv->dcnt > 0 && priv->ptr != NULL)
        {
          am67a_mcu_i2c_traceevent(priv, I2CEVENT_RCVBYTE, priv->dcnt);
          *priv->ptr++ = am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_DATA_OFFSET) & 0xff;
          priv->dcnt--;
        }
      else
        {
          am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_DATA_OFFSET);
        }

      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET, I2C_IRQ_RRDY);
    }

  /* Rung 3: the transmitter wants a byte */

  if ((status & I2C_IRQ_XRDY) != 0)
    {
      if (priv->dcnt <= 0 && priv->msgc > 0 &&
          (priv->msgv->flags & I2C_M_NOSTART) != 0)
        {
          priv->ptr   = priv->msgv->buffer;
          priv->dcnt  = priv->msgv->length;
          priv->flags = priv->msgv->flags;
          priv->msgv++;
          priv->msgc--;
        }

      if (priv->dcnt > 0 && priv->ptr != NULL)
        {
          am67a_mcu_i2c_traceevent(priv, I2CEVENT_SENDBYTE, priv->dcnt);
          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_DATA_OFFSET, *priv->ptr++);
          priv->dcnt--;
        }
#ifndef CONFIG_I2C_POLLED
      else
        {
          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET, I2C_IRQ_XRDY);
        }
#endif

      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET, I2C_IRQ_XRDY);
    }

  /* Rung 4: segment finished (ARDY), or bus released (BF). Only act if
   * this segment's data really is done — in polled mode the raw status
   * contains incidental BF/ARDY bits from past bus activity.
   */

  if ((status & (I2C_IRQ_ARDY | I2C_IRQ_BF)) != 0)
    {
      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET,
                           status & (I2C_IRQ_ARDY | I2C_IRQ_BF));

      if (priv->dcnt <= 0 && priv->msgv != NULL)
        {
          if (priv->msgc > 0)
            {
              am67a_mcu_i2c_traceevent(priv, I2CEVENT_STARTRESTART, priv->msgc);
              am67a_mcu_i2c_startmsg(priv);
            }
          else if ((raw & I2C_IRQ_BB) != 0 &&
                   (status & I2C_IRQ_BF) == 0 &&
                   (priv->flags & I2C_M_NOSTOP) == 0)
            {
              if ((am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_CON_OFFSET) &
                   I2C_CON_STP) == 0)
                {
                  am67a_mcu_i2c_sendstop(priv);
                }

#ifdef CONFIG_I2C_POLLED
              /* In polled mode, wait briefly for STOP to complete on wire */

              for (int i = 0; i < 1000; i++)
                {
                  if ((am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET) &
                       I2C_IRQ_BB) == 0)
                    {
                      break;
                    }
                  up_udelay(1);
                }

              am67a_mcu_i2c_traceevent(priv, I2CEVENT_STOP, 0);
              priv->intstate = INTSTATE_DONE;
              priv->msgv = NULL;
#else
              am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_SET_OFFSET, I2C_IRQ_BF);
#endif
            }
          else
            {
              /* Transfer complete */

              am67a_mcu_i2c_traceevent(priv, I2CEVENT_STOP, 0);

#ifndef CONFIG_I2C_POLLED
              am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET,
                                   I2C_ICR_CLEARMASK);

              if (priv->intstate == INTSTATE_WAITING)
                {
                  nxsem_post(&priv->sem_isr);
                  priv->intstate = INTSTATE_DONE;
                }
#else
              priv->intstate = INTSTATE_DONE;
#endif
              priv->msgv = NULL;
            }
        }
    }

  priv->status = status;
  return OK;
}

#ifndef CONFIG_I2C_POLLED
static int am67a_mcu_i2c_isr(int irq, void *context, void *arg)
{
  struct am67a_mcu_i2c_priv_s *priv = (struct am67a_mcu_i2c_priv_s *)arg;
  DEBUGASSERT(priv != NULL);
  return am67a_mcu_i2c_isr_process(priv);
}
#endif

static int am67a_mcu_i2c_init(struct am67a_mcu_i2c_priv_s *priv)
{
  int ret;

  if (priv->config->base == AM67A_MCU_WKUP_I2C0_BASE)
    {
      am67a_mcu_wkup_i2c0_pinmux_init();
    }
  else
    {
      am67a_mcu_i2c0_pinmux_init();
    }

  ret = am67a_mcu_i2c_bus_reset(priv);
  if (ret < 0)
    {
      return ret;
    }

#ifndef CONFIG_I2C_POLLED
  if (priv->config->irq > 0)
    {
      irq_attach(priv->config->irq, am67a_mcu_i2c_isr, priv);
      up_enable_irq(priv->config->irq);
    }
#endif

  return OK;
}

static int am67a_mcu_i2c_deinit(struct am67a_mcu_i2c_priv_s *priv)
{
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_CON_OFFSET, 0);
  am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_SYSC_OFFSET, I2C_SYSC_SRST);

#ifndef CONFIG_I2C_POLLED
  up_disable_irq(priv->config->irq);
  irq_detach(priv->config->irq);
#endif

  return OK;
}

static int am67a_mcu_i2c_transfer(struct i2c_master_s *dev,
                                  struct i2c_msg_s *msgs, int count)
{
  struct am67a_mcu_i2c_priv_s *priv = (struct am67a_mcu_i2c_priv_s *)dev;
  int ret;

  DEBUGASSERT(count > 0);

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  ret = -EBUSY;
  if (am67a_mcu_i2c_sem_waitstop(priv))
    {
      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_BUF_OFFSET,
                           I2C_BUF_TXFIFO_CLR | I2C_BUF_RXFIFO_CLR);

      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET,
                           I2C_STS_CLEARMASK);
      am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_EN_CLR_OFFSET,
                           I2C_ICR_CLEARMASK);

      priv->dcnt  = 0;
      priv->ptr   = NULL;

      priv->msgv  = msgs;
      priv->msgc  = count;

      i2cinfo("Flags %x, len %d\n", msgs->flags, msgs->length);

      am67a_mcu_i2c_tracereset(priv);

      am67a_mcu_i2c_setclock(priv, msgs->frequency);

      priv->status = 0;

      if (am67a_mcu_i2c_sem_waitdone(priv) < 0)
        {
          ret = -ETIMEDOUT;
          i2cerr("ERROR: Timed out: IRQ_RAW: status: 0x%" PRIx32 "\n",
                 priv->status);
          am67a_mcu_i2c_sendstop(priv);
          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_IRQ_STAT_OFFSET,
                               I2C_STS_CLEARMASK);
          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_BUF_OFFSET,
                               I2C_BUF_TXFIFO_CLR | I2C_BUF_RXFIFO_CLR);
          am67a_mcu_i2c_putreg(priv, AM67A_MCU_I2C_CNT_OFFSET, 0);

          /* Reset controller state machine so next transfer doesn't stay wedged */

          am67a_mcu_i2c_bus_reset(priv);
        }
      else if ((priv->status & I2C_IRQ_ERRORMASK) != 0)
        {
          if (priv->status & I2C_IRQ_AL)
            {
              i2cerr("Arbitration lost\n");
              ret = -EAGAIN;
            }
          else if (priv->status & I2C_IRQ_NACK)
            {
              i2cerr("Ack failure\n");
              ret = -ENXIO;
            }
          else if (priv->status & (I2C_IRQ_XUDF | I2C_IRQ_ROVR))
            {
              i2cerr("Overrun/Underrun status\n");
              ret = -EIO;
            }
          else if (priv->status & I2C_IRQ_AERR)
            {
              i2cerr("Access Error\n");
              ret = -EPROTO;
            }
          else if (priv->status & I2C_IRQ_BB)
            {
              i2cerr("Bus busy error\n");
              ret = -EIO;
            }
          else
            {
              i2cerr("Unspecified error\n");
              ret = -EINTR;
            }
        }
      else
        {
          ret = OK;
        }

      am67a_mcu_i2c_tracedump(priv);

      priv->dcnt = 0;
      priv->ptr  = NULL;
    }
  else
    {
      i2cerr("ERROR: Bus busy, raw status: 0x%" PRIx32 "\n",
             am67a_mcu_i2c_getreg(priv, AM67A_MCU_I2C_IRQ_STAT_RAW_OFFSET));
    }

  nxmutex_unlock(&priv->lock);
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

struct i2c_master_s *am67a_mcu_i2cbus_initialize(int port)
{
  struct am67a_mcu_i2c_priv_s *priv = NULL;

  if (port == 0)
    {
      priv = &am67a_mcu_i2c0_priv;
    }
  else if (port == 1)
    {
      priv = &am67a_mcu_i2c1_priv;
    }
  else
    {
      return NULL;
    }

  nxmutex_lock(&priv->lock);
  if (priv->refs++ == 0)
    {
      am67a_mcu_i2c_init(priv);
    }

  nxmutex_unlock(&priv->lock);
  return (struct i2c_master_s *)priv;
}

int am67a_mcu_i2cbus_uninitialize(struct i2c_master_s *dev)
{
  struct am67a_mcu_i2c_priv_s *priv = (struct am67a_mcu_i2c_priv_s *)dev;

  DEBUGASSERT(dev);

  if (priv->refs == 0)
    {
      return ERROR;
    }

  nxmutex_lock(&priv->lock);
  if (--priv->refs > 0)
    {
      nxmutex_unlock(&priv->lock);
      return OK;
    }

  am67a_mcu_i2c_deinit(priv);
  nxmutex_unlock(&priv->lock);

  return OK;
}

#endif /* CONFIG_AM67A_MCU_I2C0 */
