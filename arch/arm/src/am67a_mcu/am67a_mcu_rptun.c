/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_rptun.c
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

#include <errno.h>
#include <debug.h>
#include <inttypes.h>
#include <string.h>
#include <syslog.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/rptun/rptun.h>

#include "arm_internal.h"
#include "am67a_mcu_rsc_table.h"
#include "am67a_mcu_rptun.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* AM67A_MCU Mailbox Cluster 1 Base Address */

#define AM67A_MCU_MAILBOX1_BASE             (0x29010000u)

/* Mailbox Register Offsets */

#define MAILBOX_MSG_OFFSET(fifo)        (0x040u + ((fifo) * 0x04u))
#define MAILBOX_FIFO_STATUS(fifo)       (0x080u + ((fifo) * 0x04u))
#define MAILBOX_MSG_STATUS(fifo)        (0x0c0u + ((fifo) * 0x04u))
#define MAILBOX_CLEAR_INT(user)         (0x104u + ((user) * 0x10u))
#define MAILBOX_ENABLE_INT(user)        (0x108u + ((user) * 0x10u))
#define MAILBOX_DISABLE_INT(user)       (0x10cu + ((user) * 0x10u))

/* FIFO Bitmask for New Message Interrupt */

#define MAILBOX_INT_FIFO(fifo)          (1u << ((fifo) * 2u))

/* Mailbox Configuration for MCU R5F <-> A53 (Linux):
 * Cluster 1, FIFO 0: MCU R5F -> A53 (TX)
 * Cluster 1, FIFO 1: A53 -> MCU R5F (RX)
 * User ID: 2
 * IRQ: 241
 */

#define AM67A_MCU_MAILBOX_TX_FIFO           (0u)
#define AM67A_MCU_MAILBOX_RX_FIFO           (1u)
#define AM67A_MCU_MAILBOX_USER              (2u)
#define AM67A_MCU_MAILBOX_IRQ               (241)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct am67a_mcu_rptun_dev_s
{
  struct rptun_dev_s  rptun;
  rptun_callback_t    callback;
  FAR void           *arg;
  char                cpuname[RPMSG_NAME_SIZE + 1];
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static FAR const char *am67a_mcu_rptun_get_cpuname(FAR struct rptun_dev_s *dev);
static FAR struct resource_table *
am67a_mcu_rptun_get_resource(FAR struct rptun_dev_s *dev);
static bool am67a_mcu_rptun_is_autostart(FAR struct rptun_dev_s *dev);
static bool am67a_mcu_rptun_is_master(FAR struct rptun_dev_s *dev);
static int am67a_mcu_rptun_start(FAR struct rptun_dev_s *dev);
static int am67a_mcu_rptun_stop(FAR struct rptun_dev_s *dev);
static int am67a_mcu_rptun_notify(FAR struct rptun_dev_s *dev, uint32_t vqid);
static int am67a_mcu_rptun_register_callback(FAR struct rptun_dev_s *dev,
                                         rptun_callback_t callback,
                                         FAR void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct rptun_ops_s g_am67a_mcu_rptun_ops =
{
  .get_cpuname       = am67a_mcu_rptun_get_cpuname,
  .get_firmware      = NULL,
  .get_addrenv       = NULL,
  .get_resource      = am67a_mcu_rptun_get_resource,
  .is_autostart      = am67a_mcu_rptun_is_autostart,
  .is_master         = am67a_mcu_rptun_is_master,
  .start             = am67a_mcu_rptun_start,
  .stop              = am67a_mcu_rptun_stop,
  .notify            = am67a_mcu_rptun_notify,
  .register_callback = am67a_mcu_rptun_register_callback,
};

static struct am67a_mcu_rptun_dev_s g_am67a_mcu_rptun_dev;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_rptun_get_cpuname
 ****************************************************************************/

static FAR const char *am67a_mcu_rptun_get_cpuname(FAR struct rptun_dev_s *dev)
{
  FAR struct am67a_mcu_rptun_dev_s *priv = (FAR struct am67a_mcu_rptun_dev_s *)dev;

  return priv->cpuname;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_get_resource
 ****************************************************************************/

static FAR struct resource_table *
am67a_mcu_rptun_get_resource(FAR struct rptun_dev_s *dev)
{
  return (FAR struct resource_table *)&g_am67a_mcu_rsc_table;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_is_autostart
 ****************************************************************************/

static bool am67a_mcu_rptun_is_autostart(FAR struct rptun_dev_s *dev)
{
  return true;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_is_master
 ****************************************************************************/

static bool am67a_mcu_rptun_is_master(FAR struct rptun_dev_s *dev)
{
  return false;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_start
 ****************************************************************************/

static int am67a_mcu_rptun_start(FAR struct rptun_dev_s *dev)
{
  return OK;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_stop
 ****************************************************************************/

static int am67a_mcu_rptun_stop(FAR struct rptun_dev_s *dev)
{
  return OK;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_notify
 ****************************************************************************/

static int am67a_mcu_rptun_notify(FAR struct rptun_dev_s *dev, uint32_t vqid)
{
  /* Wait until TX FIFO is not full */

  while ((getreg32(AM67A_MCU_MAILBOX1_BASE +
                   MAILBOX_FIFO_STATUS(AM67A_MCU_MAILBOX_TX_FIFO)) & 1u) != 0)
    {
    }

  /* Send virtqueue notification to Linux via Mailbox Cluster 1, FIFO 0 */

  putreg32(vqid, AM67A_MCU_MAILBOX1_BASE +
                 MAILBOX_MSG_OFFSET(AM67A_MCU_MAILBOX_TX_FIFO));

  return OK;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_register_callback
 ****************************************************************************/

static int am67a_mcu_rptun_register_callback(FAR struct rptun_dev_s *dev,
                                         rptun_callback_t callback,
                                         FAR void *arg)
{
  FAR struct am67a_mcu_rptun_dev_s *priv = (FAR struct am67a_mcu_rptun_dev_s *)dev;

  priv->callback = callback;
  priv->arg      = arg;

  return OK;
}

/****************************************************************************
 * Name: am67a_mcu_rptun_isr
 ****************************************************************************/

static int am67a_mcu_rptun_isr(int irq, FAR void *context, FAR void *arg)
{
  FAR struct am67a_mcu_rptun_dev_s *priv = (FAR struct am67a_mcu_rptun_dev_s *)arg;
  uint32_t vqid;

  /* Read all pending messages from RX FIFO */

  while ((getreg32(AM67A_MCU_MAILBOX1_BASE +
                   MAILBOX_MSG_STATUS(AM67A_MCU_MAILBOX_RX_FIFO)) & 0x7u) > 0)
    {
      vqid = getreg32(AM67A_MCU_MAILBOX1_BASE +
                      MAILBOX_MSG_OFFSET(AM67A_MCU_MAILBOX_RX_FIFO));

      /* Clear interrupt for User 2, FIFO 1 */

      putreg32(MAILBOX_INT_FIFO(AM67A_MCU_MAILBOX_RX_FIFO),
               AM67A_MCU_MAILBOX1_BASE +
               MAILBOX_CLEAR_INT(AM67A_MCU_MAILBOX_USER));

      syslog(LOG_INFO, "[RPTUN] ISR: msg=%" PRIu32 "\n", vqid);

      /* Dispatch notification to OpenAMP / RPTun */

      if (priv->callback != NULL)
        {
          priv->callback(priv->arg, RPTUN_NOTIFY_ALL);
        }
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_rptun_init
 ****************************************************************************/

int am67a_mcu_rptun_init(FAR const char *cpuname)
{
  FAR struct am67a_mcu_rptun_dev_s *priv = &g_am67a_mcu_rptun_dev;
  int ret;

  /* Configure RPTun device */

  priv->rptun.ops = &g_am67a_mcu_rptun_ops;
  strlcpy(priv->cpuname, cpuname, sizeof(priv->cpuname));

  /* Clear any pending interrupt on Mailbox Cluster 1, User 2 */

  putreg32(MAILBOX_INT_FIFO(AM67A_MCU_MAILBOX_RX_FIFO),
           AM67A_MCU_MAILBOX1_BASE +
           MAILBOX_CLEAR_INT(AM67A_MCU_MAILBOX_USER));

  /* Enable new message interrupt on FIFO 1 for User 2 */

  putreg32(MAILBOX_INT_FIFO(AM67A_MCU_MAILBOX_RX_FIFO),
           AM67A_MCU_MAILBOX1_BASE +
           MAILBOX_ENABLE_INT(AM67A_MCU_MAILBOX_USER));

  /* Attach and enable Mailbox IRQ */

  irq_attach(AM67A_MCU_MAILBOX_IRQ, am67a_mcu_rptun_isr, priv);
  up_enable_irq(AM67A_MCU_MAILBOX_IRQ);

  /* Initialize the RPTun subsystem */

  ret = rptun_initialize(&priv->rptun);
  syslog(LOG_INFO, "[RPTUN] rptun_initialize ret=%d\n", ret);

  return ret;
}
