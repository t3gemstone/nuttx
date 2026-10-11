/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_rsc_table.c
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
#include <stdint.h>
#include <stddef.h>

#include "am67a_mcu_rsc_table.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

#define TRACE_BANNER \
  "========================================\r\n" \
  "[NuttX] AM67A_MCU MCU R5F: RemoteProc Trace Buffer Ready\r\n" \
  "========================================\r\n"

char g_rsc_trace_buf[AM67A_MCU_TRACE_BUFFER_SIZE]
  locate_data(".trace_buf") aligned_data(128) = TRACE_BANNER;

volatile uint32_t g_rsc_trace_idx = sizeof(TRACE_BANNER) - 1;

/* RemoteProc Resource Table placed in .resource_table section */

struct am67a_mcu_resource_table_s g_am67a_mcu_rsc_table
  locate_data(".resource_table") aligned_data(1024) =
{
  /* Header */

  {
    1U,                                   /* ver: version 1 */
    2U,                                   /* num: 2 entries (vdev and trace) */
    {
      0U, 0U
    }
  },

  /* Offsets */

  {
    offsetof(struct am67a_mcu_resource_table_s, vdev),
    offsetof(struct am67a_mcu_resource_table_s, trace),
  },

  /* VDEV entry */

  {
    RSC_TYPE_VDEV,
    RSC_VIRTIO_ID_RPMSG,
    0U,
    1U,
    0U,
    0U,
    0U,
    2U,                                   /* 2 VRINGs */
    {
      0U, 0U
    }
  },

  /* VRING 0 (TX) */

  {
    RSC_VRING_ADDR_ANY,
    4096U,
    256U,
    1U,
    0U
  },

  /* VRING 1 (RX) */

  {
    RSC_VRING_ADDR_ANY,
    4096U,
    256U,
    2U,
    0U
  },

  /* TRACE entry */

  {
    (RSC_TRACE_INTS_VER0 | RSC_TYPE_TRACE),
    (uint32_t)g_rsc_trace_buf,
    AM67A_MCU_TRACE_BUFFER_SIZE,
    0U,
    "trace:mcu-r5f0_0"
  }
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67a_mcu_trace_putc
 *
 * Description:
 *   Append a single character to the circular trace buffer readable via
 *   Linux remoteproc trace0.
 *
 ****************************************************************************/

void am67a_mcu_trace_putc(int ch)
{
  uint32_t idx = g_rsc_trace_idx;

  if (idx < (AM67A_MCU_TRACE_BUFFER_SIZE - 1))
    {
      g_rsc_trace_buf[idx] = (char)ch;
      g_rsc_trace_buf[idx + 1] = '\0';
      g_rsc_trace_idx = idx + 1;
    }
  else
    {
      /* Wrap around (circular buffer) */

      g_rsc_trace_buf[0] = (char)ch;
      g_rsc_trace_buf[1] = '\0';
      g_rsc_trace_idx = 1;
      idx = 0;
    }

  /* Clean L1 data cache line to DDR so Linux host sees it immediately */

  asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (&g_rsc_trace_buf[idx])
               : "memory");
  asm volatile("dsb" : : : "memory");
}
