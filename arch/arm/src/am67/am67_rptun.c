/****************************************************************************
 * arch/arm/src/am67/am67_rptun.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * rptun driver for the AM67A (J722S) main-R5FSS0-0 Cortex-R5F core.
 *
 * The resource table (g_am67_rsc_table, defined in am67_boot.c and placed
 * in the .resource_table ELF section) is read by Linux's remoteproc driver
 * when it loads the R5F firmware.  This driver registers the same table
 * with NuttX's rptun/OpenAMP stack so that NuttX can also discover and
 * use the virtio devices (RPMsg + virtio-net).
 *
 * Inter-processor notification (Linux ↔ R5F):
 *   Uses mailbox0_cluster3 (base 0x29030000).
 *   R5F → Linux: write vqid to FIFO 0 → Linux GIC SPI 109 (user 0)
 *   Linux → R5F: Linux writes to FIFO 1 → R5F VIM IRQ 116 (user 3)
 *   Mailbox assignment confirmed from:
 *     k3-j722s-evm.dts (mbox_main_r5_0) and
 *     mcu_plus_sdk_j722s cslr_intr_r5fss0_core0.h (IRQ 116).
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>
#include <syslog.h>

#include <arch/barriers.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/kthread.h>
#include <nuttx/semaphore.h>
#include <nuttx/nuttx.h>
#include <nuttx/rptun/rptun.h>
#include <nuttx/wqueue.h>

#include "am67_rptun.h"
#include "arm_internal.h"

#if defined(CONFIG_AM67_EPWM0) || defined(CONFIG_AM67_EPWM1)
#  include "am67_pwm.h"
#endif
#if defined(CONFIG_AM67_ECAP0) || defined(CONFIG_AM67_ECAP1) || \
    defined(CONFIG_AM67_ECAP2)
#  include "am67_ecap.h"
#endif

#ifdef CONFIG_RPTUN

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* NAVSS mailbox0_cluster3 — the J722S EVM DTS assigns this cluster to
 * main_r5fss0_core0:
 *   &mailbox0_cluster3 { mbox_main_r5_0: { ti,mbox-rx=<0 0 0>;
 *                                           ti,mbox-tx=<1 0 0>; }; }
 *   &main_r5fss0_core0 { mboxes = <&mailbox0_cluster3 &mbox_main_r5_0>; }
 *
 * From the perspective of each side:
 *   Linux TX → R5F RX : write/read FIFO 1 (Linux sends, R5F receives)
 *   R5F  TX → Linux RX: write/read FIFO 0 (R5F sends, Linux receives)
 *
 * The NAVSS interrupt router wires cluster3 user-3 to R5FSS0 VIM 116
 * (CSLR_R5FSS0_CORE0_INTR_MAILBOX0_MAILBOX_CLUSTER_3
 *  _MAILBOX_CLUSTER_PEND_3, from mcu_plus_sdk_j722s
 *  source/drivers/hw_include/j722s/cslr_intr_r5fss0_core0.h).
 * Linux uses user-0 of the same cluster (GIC SPI 109).
 */

#define AM67_MBOX_BASE          (0x29030000ul) /* mailbox0_cluster3 */

/* FIFO assignments (from Linux DTS mbox_main_r5_0) */

#define AM67_MBOX_TX_FIFO       (0U)  /* R5F writes here → Linux user-0 IRQ fires */
#define AM67_MBOX_RX_FIFO       (1U)  /* Linux writes here → R5F user-3 IRQ fires */

/* R5F uses mailbox user-3 (interrupt router wires cluster3/user3 → VIM) */

#define AM67_MBOX_USER          (3U)

/* VIM IRQ number on MAIN_R5FSS0_0 for mailbox0_cluster3/user3 */

#ifndef CONFIG_AM67_RPTUN_IRQ
#  define AM67_RPTUN_IRQ_EVENT  (116)
#else
#  define AM67_RPTUN_IRQ_EVENT  CONFIG_AM67_RPTUN_IRQ
#endif

/* Register offsets (OMAP4-style mailbox, ti,am64-mailbox compatible):
 *   MESSAGE(n)     = base + 0x040 + 4*n  (read/write FIFO n)
 *   MSG_STATUS(n)  = base + 0x0C0 + 4*n  (number of msgs in FIFO n)
 *   IRQSTATUS(u)   = base + 0x104 + 16*u
 *   IRQENABLE(u)   = base + 0x108 + 16*u
 *   IRQDISABLE(u)  = base + 0x10C + 16*u
 *   EOI            = base + 0x140
 *   NEW_MSG_INT(n) = (1 << (2*n))  — bit to enable/clear new-message IRQ
 */

#define AM67_MBOX_MESSAGE(fifo)    (AM67_MBOX_BASE + 0x040u + 0x4u * (fifo))
#define AM67_MBOX_MSG_STATUS(fifo) (AM67_MBOX_BASE + 0x0c0u + 0x4u * (fifo))
#define AM67_MBOX_IRQSTATUS(usr)   (AM67_MBOX_BASE + 0x104u + 0x10u * (usr))
#define AM67_MBOX_IRQENABLE(usr)   (AM67_MBOX_BASE + 0x108u + 0x10u * (usr))
#define AM67_MBOX_IRQDISABLE(usr)  (AM67_MBOX_BASE + 0x10cu + 0x10u * (usr))
#define AM67_MBOX_EOI              (AM67_MBOX_BASE + 0x140u)
#define AM67_MBOX_NEW_MSG_INT(n)   (1u << ((n) * 2u))

/* Control messages of the TI remoteproc mailbox protocol (Linux
 * drivers/remoteproc/omap_remoteproc.h). Anything below RP_MBOX_READY is a
 * virtqueue id.
 */

#define RP_MBOX_READY              0xffffff00u
#define RP_MBOX_CRASH              0xffffff02u
#define RP_MBOX_ECHO_REQUEST       0xffffff03u
#define RP_MBOX_ECHO_REPLY         0xffffff04u
#define RP_MBOX_SHUTDOWN           0xffffff14u
#define RP_MBOX_SHUTDOWN_ACK       0xffffff15u

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct am67_rptun_dev_s
{
  struct rptun_dev_s rptun;
  rptun_callback_t   callback;
  void              *arg;
  struct work_s      work;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static const char *am67_rptun_get_cpuname(struct rptun_dev_s *dev);
static const char *am67_rptun_get_firmware(struct rptun_dev_s *dev);
static const struct rptun_addrenv_s *
am67_rptun_get_addrenv(struct rptun_dev_s *dev);
static struct resource_table *
am67_rptun_get_resource(struct rptun_dev_s *dev);
static size_t am67_rptun_get_rsc_size(struct rptun_dev_s *dev);
static bool am67_rptun_is_autostart(struct rptun_dev_s *dev);
static bool am67_rptun_is_master(struct rptun_dev_s *dev);
static int am67_rptun_start(struct rptun_dev_s *dev);
static int am67_rptun_stop(struct rptun_dev_s *dev);
static int am67_rptun_notify(struct rptun_dev_s *dev, uint32_t vqid);
static void am67_rptun_panic(struct rptun_dev_s *dev);
static int am67_rptun_register_callback(struct rptun_dev_s *dev,
                                        rptun_callback_t callback,
                                        void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct rptun_ops_s g_am67_rptun_ops =
{
  .get_cpuname       = am67_rptun_get_cpuname,
  .get_firmware      = am67_rptun_get_firmware,
  .get_addrenv       = am67_rptun_get_addrenv,
  .get_resource      = am67_rptun_get_resource,
  .get_rsc_size      = am67_rptun_get_rsc_size,
  .is_autostart      = am67_rptun_is_autostart,
  .is_master         = am67_rptun_is_master,
  .start             = am67_rptun_start,
  .stop              = am67_rptun_stop,
  .notify            = am67_rptun_notify,
  .register_callback = am67_rptun_register_callback,
  .panic             = am67_rptun_panic,
};

static struct am67_rptun_dev_s g_am67_rptun_dev;

/* Set while PX4 must not be stopped (armed); see am67_rptun_set_lockout() */

static volatile bool g_am67_rptun_lockout;

/* The mailbox interrupt posts it; am67_rptun_stopper() handles the request */

static sem_t g_am67_rptun_stopsem = SEM_INITIALIZER(0);

/* What woke the halted core, for Linux to read (ATCM, not cached;
 * scripts/kart/r5f-inspect.sh halt): [0] magic "HALT", [1] WFI wake-ups,
 * [2] VIM PRIIRQ, [3] VIM PRIFIQ at the last wake-up, [4] first VIM group
 * with a raw pending input, [5] its raw bits, [6] CPSR.
 */

#define AM67_HALT_MAGIC  0x544c4148u   /* "HALT" */
#define VIM_BASE         0x2fff0000u

locate_data(".am67_fastdata") volatile uint32_t g_am67_halt_diag[8];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static const char *am67_rptun_get_cpuname(struct rptun_dev_s *dev)
{
  return AM67_RPTUN_CPUNAME;
}

static const char *am67_rptun_get_firmware(struct rptun_dev_s *dev)
{
  return NULL; /* Linux is master; R5F does not load a firmware file */
}

static const struct rptun_addrenv_s *
am67_rptun_get_addrenv(struct rptun_dev_s *dev)
{
  return NULL; /* physical == virtual (no MMU on R5F) */
}

static struct resource_table *
am67_rptun_get_resource(struct rptun_dev_s *dev)
{
  /* The resource table is placed at a fixed address (0xA2100000) by the
   * linker script (.resource_table section → ddr_rsctable region).
   * Linux remoteproc populates the vdev status/features fields in-place
   * before kicking the R5F, so we return a non-const pointer.
   */

  return (struct resource_table *)&g_am67_rsc_table;
}

static size_t am67_rptun_get_rsc_size(struct rptun_dev_s *dev)
{
  /* Return the real size of the extended resource table so that OpenAMP's
   * remoteproc_set_rsc_table() can find both vdev entries (rptun.c would
   * otherwise cap the visible region to sizeof(struct rptun_rsc_s)).
   */

  return sizeof(struct am67_rsc_s);
}

static bool am67_rptun_is_autostart(struct rptun_dev_s *dev)
{
  return true; /* start rptun thread automatically */
}

static bool am67_rptun_is_master(struct rptun_dev_s *dev)
{
  return false; /* Linux A53 is the remoteproc master */
}

static int am67_rptun_start(struct rptun_dev_s *dev)
{
  return 0; /* nothing to do — Linux starts us */
}

static int am67_rptun_stop(struct rptun_dev_s *dev)
{
  return 0;
}

static int am67_rptun_notify(struct rptun_dev_s *dev, uint32_t vqid)
{
  /* Kick Linux A53: write vqid into FIFO 0 of mailbox0_cluster3.
   * Linux has user-0 new-message interrupt enabled on FIFO 0
   * (GIC SPI 109), so this fires rproc_vq_interrupt() on the A53.
   */

  putreg32(vqid, AM67_MBOX_MESSAGE(AM67_MBOX_TX_FIFO));
  UP_DSB();
  return 0;
}

/* A panic must not wait for Linux.  Without this op rptun_dev_panic() sends
 * RPTUN_CMD_PANIC through the resource table and polls up to
 * CONFIG_RPTUN_CMD_TIMEOUT_MS (5 s) for an ACK that Linux remoteproc never
 * gives; the watchdog fired first.  Tell nobody: the log survives in DDR
 * and the board code resets the SoC.
 */

static void am67_rptun_panic(struct rptun_dev_s *dev)
{
  UNUSED(dev);
}

static int am67_rptun_register_callback(struct rptun_dev_s *dev,
                                        rptun_callback_t callback,
                                        void *arg)
{
  struct am67_rptun_dev_s *priv =
      container_of(dev, struct am67_rptun_dev_s, rptun);

  priv->callback = callback;
  priv->arg      = arg;

  if (callback != NULL)
    {
      putreg32(AM67_MBOX_NEW_MSG_INT(AM67_MBOX_RX_FIFO),
               AM67_MBOX_IRQENABLE(AM67_MBOX_USER));
      up_enable_irq(AM67_RPTUN_IRQ_EVENT);
    }
  else
    {
      up_disable_irq(AM67_RPTUN_IRQ_EVENT);
      putreg32(AM67_MBOX_NEW_MSG_INT(AM67_MBOX_RX_FIFO),
               AM67_MBOX_IRQDISABLE(AM67_MBOX_USER));
    }

  return 0;
}

/****************************************************************************
 * Name: am67_rptun_notify_work
 *
 * Description:
 *   Deferred half of the mailbox interrupt: runs on the high-priority work
 *   queue and delivers the notification to the rptun/OpenAMP stack.  The
 *   OpenAMP receive path takes mutexes and allocates memory, so it must
 *   not run in interrupt context; RPTUN_NOTIFY_ALL re-scans every
 *   virtqueue, so any number of coalesced mailbox kicks collapse safely
 *   into one pass.
 *
 ****************************************************************************/

static void am67_rptun_notify_work(void *arg)
{
  struct am67_rptun_dev_s *priv = (struct am67_rptun_dev_s *)arg;

  if (priv->callback != NULL)
    {
      priv->callback(priv->arg, RPTUN_NOTIFY_ALL);
    }
}

/****************************************************************************
 * Name: am67_rptun_shutdown
 *
 * Description:
 *   Linux `echo stop > /sys/class/remoteproc/.../state` sends
 *   RP_MBOX_SHUTDOWN and waits 5 s for RP_MBOX_SHUTDOWN_ACK, then 2 ms for
 *   the core to sit in WFI before it halts it.  While the vehicle is armed
 *   no ACK is sent: Linux gives up with -EBUSY and the autopilot keeps
 *   flying.  Otherwise the motor outputs are cut first, then the ACK is
 *   sent and the core parks in WFI with every interrupt source masked at
 *   the VIM, since a pending interrupt would wake WFI even with CPSR.I set.
 *
 *   Runs in its own highest-priority thread (am67_rptun_stopper()), not in
 *   a work queue: a blocked work queue (PX4 runs its reboot through
 *   HPWORK) must not keep Linux from stopping the core.  Not from the
 *   interrupt either: parked from interrupt context, WFI did not hold
 *   (REHBER 5.23).
 *
 ****************************************************************************/

static void am67_rptun_shutdown(void)
{
  int irq;

  if (g_am67_rptun_lockout)
    {
      syslog(LOG_WARNING, "rptun: shutdown refused, vehicle armed\n");
      return;
    }

  syslog(LOG_WARNING, "rptun: shutdown, motor outputs off, halting\n");

  up_irq_save();

#if defined(CONFIG_AM67_EPWM0) || defined(CONFIG_AM67_EPWM1)
  am67_epwm_emergency_stop();
#endif
#if defined(CONFIG_AM67_ECAP0) || defined(CONFIG_AM67_ECAP1) || \
    defined(CONFIG_AM67_ECAP2)
  am67_ecap_emergency_stop();
#endif

  for (irq = 0; irq < NR_IRQS; irq++)
    {
      up_disable_irq(irq);
    }

  g_am67_halt_diag[1] = 0;
  g_am67_halt_diag[0] = AM67_HALT_MAGIC;

  putreg32(RP_MBOX_SHUTDOWN_ACK, AM67_MBOX_MESSAGE(AM67_MBOX_TX_FIFO));
  UP_DSB();

  for (; ; )
    {
      uint32_t cpsr;
      int g;

      asm volatile ("wfi");

      /* Linux wants the core to stay in WFI: record why it did not */

      g_am67_halt_diag[1]++;
      g_am67_halt_diag[2] = getreg32(VIM_BASE + 0x08);
      g_am67_halt_diag[3] = getreg32(VIM_BASE + 0x0c);
      for (g = 0; g < 16; g++)
        {
          uint32_t raw = getreg32(VIM_BASE + 0x400 + g * 0x20);

          if (raw != 0)
            {
              g_am67_halt_diag[4] = g;
              g_am67_halt_diag[5] = raw;
              break;
            }
        }

      asm volatile ("mrs %0, cpsr" : "=r" (cpsr));
      g_am67_halt_diag[6] = cpsr;
    }
}

static int am67_rptun_stopper(int argc, char *argv[])
{
  for (; ; )
    {
      nxsem_wait_uninterruptible(&g_am67_rptun_stopsem);
      am67_rptun_shutdown();                /* returns only if locked */
    }

  return 0;
}

/****************************************************************************
 * Name: am67_rptun_interrupt
 *
 * Description:
 *   VIM IRQ 116 handler — fires when Linux A53 writes to mailbox0_cluster3
 *   FIFO 1 (user-3 new-message interrupt).  Drains the FIFO, clears the
 *   interrupt, and queues am67_rptun_notify_work() to process the
 *   virtqueues in thread context.
 *
 ****************************************************************************/

static int am67_rptun_interrupt(int irq, void *context, void *arg)
{
  struct am67_rptun_dev_s *priv = (struct am67_rptun_dev_s *)arg;
  bool shutdown = false;

  /* Drain all messages Linux wrote into FIFO 1.  Each read pops one
   * entry; stop when MSG_STATUS reports 0 pending messages.  Virtqueue
   * kicks all collapse into one RPTUN_NOTIFY_ALL pass below; control
   * messages are handled here.
   */

  while (getreg32(AM67_MBOX_MSG_STATUS(AM67_MBOX_RX_FIFO)) != 0)
    {
      uint32_t msg = getreg32(AM67_MBOX_MESSAGE(AM67_MBOX_RX_FIFO));

      if (msg == RP_MBOX_ECHO_REQUEST)
        {
          am67_rptun_notify(&priv->rptun, RP_MBOX_ECHO_REPLY);
        }
      else if (msg == RP_MBOX_SHUTDOWN)
        {
          shutdown = true;
        }
    }

  /* Clear the new-message interrupt status for user-3 / FIFO-1.
   * Write the bit mask to the IRQSTATUS register (write-1-to-clear).
   */

  putreg32(AM67_MBOX_NEW_MSG_INT(AM67_MBOX_RX_FIFO),
           AM67_MBOX_IRQSTATUS(AM67_MBOX_USER));

  /* Acknowledge the interrupt to the mailbox EOI register so the
   * controller can re-assert on the next incoming message.
   */

  putreg32(0, AM67_MBOX_EOI);
  UP_DSB();

  if (priv != NULL && priv->callback != NULL &&
      work_available(&priv->work))
    {
      work_queue(HPWORK, &priv->work, am67_rptun_notify_work, priv, 0);
    }

  if (shutdown)
    {
      nxsem_post(&g_am67_rptun_stopsem);
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: am67_rptun_set_lockout
 ****************************************************************************/

void am67_rptun_set_lockout(bool locked)
{
  g_am67_rptun_lockout = locked;
}

/****************************************************************************
 * Name: am67_rptun_request_restart
 ****************************************************************************/

void am67_rptun_request_restart(void)
{
  /* Linux logs "K3 R5F rproc <name> crashed" (ti_k3_common.c), nothing
   * more; gem-r5f-restart.service reacts to that line with remoteproc
   * stop/start.  The stop still goes through the shutdown handshake, so a
   * locked (armed) core refuses it.
   */

  putreg32(RP_MBOX_CRASH, AM67_MBOX_MESSAGE(AM67_MBOX_TX_FIFO));
  UP_DSB();
}

/****************************************************************************
 * Name: am67_rptun_init
 ****************************************************************************/

int am67_rptun_init(void)
{
  struct am67_rptun_dev_s *dev = &g_am67_rptun_dev;
  int ret;

  memset(dev, 0, sizeof(*dev));
  dev->rptun.ops = &g_am67_rptun_ops;

  /* Enable the mailbox interrupt: R5F user-3, FIFO 1 (Linux→R5F).
   * Linux enables its own user-0/FIFO-0 side independently.
   * Disable first to start from a clean state, then clear any stale
   * status before attaching the IRQ.
   */

  putreg32(AM67_MBOX_NEW_MSG_INT(AM67_MBOX_RX_FIFO),
           AM67_MBOX_IRQDISABLE(AM67_MBOX_USER));
  putreg32(AM67_MBOX_NEW_MSG_INT(AM67_MBOX_RX_FIFO),
           AM67_MBOX_IRQSTATUS(AM67_MBOX_USER));

  ret = kthread_create("rptun_stop", SCHED_PRIORITY_MAX, 2048,
                       am67_rptun_stopper, NULL);
  if (ret < 0)
    {
      ipcerr("ERROR: rptun_stop thread: %d\n", ret);
      return ret;
    }

  ret = irq_attach(AM67_RPTUN_IRQ_EVENT, am67_rptun_interrupt, dev);
  if (ret < 0)
    {
      ipcerr("ERROR: irq_attach failed: %d\n", ret);
      return ret;
    }

  /* Enable new-message interrupt for FIFO 1, user 3 */

  putreg32(AM67_MBOX_NEW_MSG_INT(AM67_MBOX_RX_FIFO),
           AM67_MBOX_IRQENABLE(AM67_MBOX_USER));

  ret = rptun_initialize(&dev->rptun);
  if (ret < 0)
    {
      irq_detach(AM67_RPTUN_IRQ_EVENT);
      ipcerr("ERROR: rptun_initialize failed: %d\n", ret);
    }

  return ret;
}

#endif /* CONFIG_RPTUN */
