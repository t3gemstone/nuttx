/****************************************************************************
 * arch/arm/src/am67a_mcu/am67a_mcu_rsc_table.h
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

#ifndef __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_RSC_TABLE_H
#define __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_RSC_TABLE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Virtio ID for RemoteProc Messaging */

#define RSC_VIRTIO_ID_RPMSG               (7U)

/* Resource Entry types */

#define RSC_TYPE_TRACE                    (2U)
#define RSC_TYPE_VDEV                     (3U)

/* VRING address allocation by Linux */

#define RSC_VRING_ADDR_ANY                (0xffffffffU)

/* Trace version flag */

#define RSC_TRACE_INTS_VER0               (0 << 16)

/* Trace name maximum length */

#define RSC_TRACE_NAME_LEN                (32U)

/* Size of the early debug trace buffer (16 KB) */

#define AM67A_MCU_TRACE_BUFFER_SIZE           (16384U)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Resource Table Header */

struct rsc_table_hdr_s
{
  uint32_t ver;                           /* Version number (must be 1) */
  uint32_t num;                           /* Number of entries in offset array */
  uint32_t reserved[2];
};

/* Trace Buffer Entry */

struct rsc_trace_s
{
  uint32_t type;                          /* Type of trace */
  uint32_t da;                            /* Device address of buffer */
  uint32_t len;                           /* Length of trace buffer */
  uint32_t reserved;
  uint8_t  name[RSC_TRACE_NAME_LEN];      /* Name of the trace buffer */
};

/* Device VRing Entry */

struct rsc_vring_s
{
  uint32_t da;                            /* Device address */
  uint32_t align;                         /* Alignment between structures */
  uint32_t num;                           /* Number of message buffers */
  uint32_t notifyid;                      /* Notify ID */
  uint32_t reserved;
};

/* VDEV Entry */

struct rsc_vdev_s
{
  uint32_t type;                          /* RSC_TYPE_VDEV */
  uint32_t id;                            /* RSC_VIRTIO_ID_RPMSG */
  uint32_t notifyid;
  uint32_t dfeatures;
  uint32_t gfeatures;
  uint32_t config_len;
  uint8_t  status;
  uint8_t  num_of_vrings;                 /* Number of vrings (2) */
  uint8_t  reserved[2];
};

/* Full IPC Resource Table */

struct am67a_mcu_resource_table_s
{
  struct rsc_table_hdr_s base;
  uint32_t               offset[2];
  struct rsc_vdev_s      vdev;
  struct rsc_vring_s     vring0;
  struct rsc_vring_s     vring1;
  struct rsc_trace_s     trace;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: am67a_mcu_trace_putc
 *
 * Description:
 *   Append a single character to the RemoteProc trace buffer.
 *
 ****************************************************************************/

void am67a_mcu_trace_putc(int ch);

/* Shared RemoteProc Resource Table */

extern struct am67a_mcu_resource_table_s g_am67a_mcu_rsc_table;

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM_SRC_AM67A_MCU_AM67A_MCU_RSC_TABLE_H */
