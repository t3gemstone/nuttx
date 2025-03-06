/****************************************************************************
 * arch/arm/src/am67/am67_rsc.h
 *
 * Author: Emre CECANPUNAR <emreleno@gmail.com>
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

#ifndef __ARCH_ARM_SRC_AM67_AM67_RSC_H
#define __ARCH_ARM_SRC_AM67_AM67_RSC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Resource table definitions as expected by Linux remoteproc */
#define RSC_VDEV        3   /* Virtio device resource type */
#define RSC_CARVEOUT    0   /* Carveout resource type */
#define VIRTIO_ID_RPMSG 7   /* Virtio ID for RPMsg */

/* TODO: Get these values from config */
/* Define memory regions (adjust based on AM67A/J722S memory map) */
#define SHARED_MEM_BASE 0xA0000000  /* Example: Shared DDR region */
#define SHARED_MEM_SIZE 0x00100000  /* 1MB for Virtio rings and buffers */

/* Virtio device structure */
struct fw_rsc_vdev {
    uint32_t type;         /* Resource type: RSC_VDEV */
    uint32_t id;           /* Virtio device ID: VIRTIO_ID_RPMSG */
    uint32_t notifyid;     /* Notification ID (usually 0) */
    uint32_t dfeatures;    /* Device features (set to 0 for minimal) */
    uint32_t gfeatures;    /* Guest features (set to 0) */
    uint32_t config_len;   /* Config length (set to 0 for RPMsg) */
    uint8_t  status;       /* Status (set to 0) */
    uint8_t  num_of_vrings;/* Number of Virtio rings (2 for RPMsg: RX/TX) */
    uint8_t  reserved[2];  /* Padding */
    struct {
        uint32_t da;       /* Device address (to be set by Linux) */
        uint32_t align;    /* Alignment (4096 for page alignment) */
        uint32_t num;      /* Number of buffers (e.g., 512) */
        uint32_t notifyid; /* Notification ID (set by Linux) */
        uint32_t reserved; /* Padding */
    } vring[2];            /* Two vrings: RX and TX */
};

/* Resource table header */
struct resource_table {
    uint32_t ver;          /* Version (must be 1) */
    uint32_t num;          /* Number of resource entries */
    uint32_t reserved[2];  /* Reserved */
    uint32_t offset[2];    /* Offsets to resource entries */
    struct fw_rsc_vdev vdev; /* Virtio device entry */
};

/* Place the resource table in a specific section */
__attribute__((section(".resource_table")))
const struct resource_table rsc_table = {
    .ver = 1,
    .num = 1,  /* One resource entry (Virtio device) */
    .reserved = {0, 0},
    .offset = {
        offsetof(struct resource_table, vdev), /* Offset to Virtio device */
    },
    .vdev = {
        .type = RSC_VDEV,
        .id = VIRTIO_ID_RPMSG,
        .notifyid = 0,
        .dfeatures = 0,
        .gfeatures = 0,
        .config_len = 0,
        .status = 0,
        .num_of_vrings = 2,
        .reserved = {0, 0},
        .vring = {
            {SHARED_MEM_BASE, 4096, 512, 0, 0}, /* VRING0: RX */
            {SHARED_MEM_BASE + 0x8000, 4096, 512, 0, 0}, /* VRING1: TX */
        },
    },
};


#endif /* __ARCH_ARM_SRC_AM67_AM67_RSC_H */
