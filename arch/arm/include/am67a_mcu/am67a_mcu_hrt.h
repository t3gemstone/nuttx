/****************************************************************************
 * arch/arm/include/am67a_mcu/am67a_mcu_hrt.h
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

#ifndef __ARCH_ARM_INCLUDE_AM67A_MCU_AM67A_MCU_HRT_H
#define __ARCH_ARM_INCLUDE_AM67A_MCU_AM67A_MCU_HRT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include <nuttx/queue.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Absolute time in microsecond units.
 * Measured from system boot, monotonic, never wraps or goes backwards.
 */

typedef uint64_t hrt_abstime;

/* Callout callback function type.
 * Runs in interrupt context, must not block.
 */

typedef void (*hrt_callout)(void *arg);

/* Callout record structure for scheduling events. */

struct hrt_call
{
  struct sq_entry_s link;     /* Singly-linked list pointer */
  hrt_abstime       deadline; /* Absolute microsecond deadline */
  hrt_abstime       period;   /* Recurrence period (0 for one-shot) */
  hrt_callout       callout;  /* Callback function pointer */
  void             *arg;      /* Argument passed to callback */
};

typedef struct hrt_call *hrt_call_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: hrt_absolute_time
 *
 * Description:
 *   Fetch absolute monotonic time in microseconds.
 *
 ****************************************************************************/

hrt_abstime hrt_absolute_time(void);

/****************************************************************************
 * Name: hrt_store_absolute_time
 *
 * Description:
 *   Store the absolute time in an interrupt-safe fashion.
 *
 ****************************************************************************/

void hrt_store_absolute_time(volatile hrt_abstime *t);

/****************************************************************************
 * Name: hrt_elapsed_time
 *
 * Description:
 *   Compute elapsed time since then.
 *
 ****************************************************************************/

static inline hrt_abstime hrt_elapsed_time(const hrt_abstime *then)
{
  return hrt_absolute_time() - *then;
}

/****************************************************************************
 * Name: hrt_call_after
 *
 * Description:
 *   Schedule callout(arg) after delay microseconds.
 *
 ****************************************************************************/

void hrt_call_after(struct hrt_call *entry, hrt_abstime delay,
                    hrt_callout callout, void *arg);

/****************************************************************************
 * Name: hrt_call_at
 *
 * Description:
 *   Schedule callout(arg) at a specific absolute microsecond time calltime.
 *
 ****************************************************************************/

void hrt_call_at(struct hrt_call *entry, hrt_abstime calltime,
                 hrt_callout callout, void *arg);

/****************************************************************************
 * Name: hrt_call_every
 *
 * Description:
 *   Schedule callout(arg) after delay microseconds, repeating every
 *   interval microseconds.
 *
 ****************************************************************************/

void hrt_call_every(struct hrt_call *entry, hrt_abstime delay,
                    hrt_abstime interval, hrt_callout callout, void *arg);

/****************************************************************************
 * Name: hrt_cancel
 *
 * Description:
 *   Cancel a pending callout.
 *
 ****************************************************************************/

void hrt_cancel(struct hrt_call *entry);

/****************************************************************************
 * Name: hrt_called
 *
 * Description:
 *   Check if a callout has completed.
 *
 ****************************************************************************/

bool hrt_called(struct hrt_call *entry);

/****************************************************************************
 * Name: hrt_init
 *
 * Description:
 *   Initialize the High Resolution Timer subsystem (MCU_TIMER1).
 *
 ****************************************************************************/

void hrt_init(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ARCH_ARM_INCLUDE_AM67A_MCU_AM67A_MCU_HRT_H */
