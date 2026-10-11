/****************************************************************************
 * boards/arm/am67a_mcu/t3-gem-o1-mcu/src/t3_gem_o1_mcu_netinit.c
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <nuttx/kthread.h>
#include <nuttx/signal.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/tun.h>

#include "t3-gem-o1-mcu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define SLIP_END             0xC0
#define SLIP_ESC             0xDB
#define SLIP_ESC_END         0xDC
#define SLIP_ESC_ESC         0xDD

#define MAX_ETH_FRAME        1518
#define MAX_SLIP_BUF         (MAX_ETH_FRAME * 2 + 2)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct slip_decoder_s
{
  uint8_t buf[MAX_ETH_FRAME];
  int     idx;
  bool    esc;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int slip_encode(FAR const uint8_t *src, int srclen,
                       FAR uint8_t *dst, int dstmax)
{
  int out = 0;

  if (out < dstmax)
    {
      dst[out++] = SLIP_END;
    }

  for (int i = 0; i < srclen; i++)
    {
      if (src[i] == SLIP_END)
        {
          if (out + 2 > dstmax)
            {
              return -1;
            }

          dst[out++] = SLIP_ESC;
          dst[out++] = SLIP_ESC_END;
        }
      else if (src[i] == SLIP_ESC)
        {
          if (out + 2 > dstmax)
            {
              return -1;
            }

          dst[out++] = SLIP_ESC;
          dst[out++] = SLIP_ESC_ESC;
        }
      else
        {
          if (out + 1 > dstmax)
            {
              return -1;
            }

          dst[out++] = src[i];
        }
    }

  if (out < dstmax)
    {
      dst[out++] = SLIP_END;
    }

  return out;
}

static int slip_decode_byte(FAR struct slip_decoder_s *dec, uint8_t b,
                            FAR uint8_t *out_frame, int max_frame)
{
  if (b == SLIP_END)
    {
      int frame_len = dec->idx;
      dec->idx = 0;
      dec->esc = false;

      if (frame_len > 0)
        {
          if (frame_len > max_frame)
            {
              return -1;
            }

          memcpy(out_frame, dec->buf, frame_len);
          return frame_len;
        }

      return 0;
    }
  else if (b == SLIP_ESC)
    {
      dec->esc = true;
      return 0;
    }
  else if (dec->esc)
    {
      dec->esc = false;
      if (b == SLIP_ESC_END)
        {
          b = SLIP_END;
        }
      else if (b == SLIP_ESC_ESC)
        {
          b = SLIP_ESC;
        }
    }

  if (dec->idx < (int)sizeof(dec->buf))
    {
      dec->buf[dec->idx++] = b;
    }
  else
    {
      dec->idx = 0;
    }

  return 0;
}

static int t3_gem_o1_mcu_tap_bridge_thread(int argc, FAR char *argv[])
{
  int fd_tun = -1;
  int fd_rpmsg = -1;
  int ret;
  int retry;

  syslog(LOG_INFO, "[TAP_BRIDGE] Bridge daemon starting...\n");

  /* Wait for devices to become available */

  for (retry = 0; retry < 50; retry++)
    {
      if (fd_tun < 0)
        {
          fd_tun = open("/dev/tun", O_RDWR);
        }

      if (fd_rpmsg < 0)
        {
          fd_rpmsg = open("/dev/ttyRPMSG1", O_RDWR);
        }

      if (fd_tun >= 0 && fd_rpmsg >= 0)
        {
          break;
        }

      nxsig_usleep(100000); /* 100 ms */
    }

  if (fd_tun < 0 || fd_rpmsg < 0)
    {
      syslog(LOG_ERR,
             "[TAP_BRIDGE] Failed to open /dev/tun (%d) or "
             "/dev/ttyRPMSG1 (%d)\n",
             fd_tun, fd_rpmsg);
      if (fd_tun >= 0)
        {
          close(fd_tun);
        }

      if (fd_rpmsg >= 0)
        {
          close(fd_rpmsg);
        }

      return -ENODEV;
    }

  /* Configure /dev/tun as TAP (Ethernet) interface named "tap0" */

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strlcpy(ifr.ifr_name, "tap0", IFNAMSIZ);

  ret = ioctl(fd_tun, TUNSETIFF, (unsigned long)&ifr);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[TAP_BRIDGE] ioctl TUNSETIFF failed: %d\n", errno);
      close(fd_tun);
      close(fd_rpmsg);
      return ret;
    }

  /* Configure tap0 in NuttX network stack */

  FAR struct net_driver_s *dev = netdev_findbyname("tap0");
  if (dev != NULL)
    {
      /* Local Unicast MAC: 02:00:00:00:00:02 */

      const uint8_t mac[6] =
      {
        0x02, 0x00, 0x00, 0x00, 0x00, 0x02
      };

      memcpy(dev->d_mac.ether.ether_addr_octet, mac, 6);

      /* IP: 192.168.10.2 (0xc0a80a02) */

      dev->d_ipaddr = HTONL(0xc0a80a02);

      /* Netmask: 255.255.255.0 (0xffffff00) */

      dev->d_netmask = HTONL(0xffffff00);

      netdev_ifup(dev);
      syslog(LOG_INFO,
             "[TAP_BRIDGE] tap0 initialized: IP=192.168.10.2/24 UP\n");
    }
  else
    {
      syslog(LOG_ERR, "[TAP_BRIDGE] Failed to find tap0 netdev\n");
    }

  /* Forwarding loop */

  static uint8_t pkt_buf[MAX_ETH_FRAME];
  static uint8_t enc_buf[MAX_SLIP_BUF];
  static uint8_t raw_buf[512];
  static uint8_t dec_buf[MAX_ETH_FRAME];
  struct slip_decoder_s dec;
  memset(&dec, 0, sizeof(dec));

  struct pollfd fds[2];
  fds[0].fd = fd_tun;
  fds[0].events = POLLIN;
  fds[1].fd = fd_rpmsg;
  fds[1].events = POLLIN;

  syslog(LOG_INFO, "[TAP_BRIDGE] Forwarding loop active\n");

  while (1)
    {
      ret = poll(fds, 2, 500);
      if (ret < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          syslog(LOG_ERR, "[TAP_BRIDGE] poll error: %d\n", errno);
          break;
        }

      /* Packet from NuttX stack -> send out over RPMSG */

      if (fds[0].revents & POLLIN)
        {
          int n = read(fd_tun, pkt_buf, sizeof(pkt_buf));
          if (n > 0)
            {
              int enc_len = slip_encode(pkt_buf, n, enc_buf,
                                        sizeof(enc_buf));
              if (enc_len > 0)
                {
                  write(fd_rpmsg, enc_buf, enc_len);
                }
            }
        }

      /* Bytes from RPMSG (Linux) -> decode and inject to NuttX stack */

      if (fds[1].revents & POLLIN)
        {
          int n = read(fd_rpmsg, raw_buf, sizeof(raw_buf));
          if (n > 0)
            {
              for (int i = 0; i < n; i++)
                {
                  int frame_len = slip_decode_byte(&dec, raw_buf[i],
                                                   dec_buf,
                                                   sizeof(dec_buf));
                  if (frame_len > 0)
                    {
                      write(fd_tun, dec_buf, frame_len);
                    }
                }
            }
        }
    }

  close(fd_tun);
  close(fd_rpmsg);
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int t3_gem_o1_mcu_netinit(void)
{
  int pid;

  pid = kthread_create("tap_bridge",
                       100,
                       4096,
                       t3_gem_o1_mcu_tap_bridge_thread,
                       NULL);
  if (pid < 0)
    {
      syslog(LOG_ERR,
             "[TAP_BRIDGE] Failed to create tap_bridge kthread: %d\n",
             pid);
      return pid;
    }

  return OK;
}
