==============
T3 Gemstone O1
==============

.. tags:: chip:am67

This page presents `T3 Gemstone O1 <https://t3gemstone.org/en/>`__ - High-performance development board
based on Texas Instruments AM67A processor, which runs NuttX on main domain R5F core.

Specifications
==============

-  **Processor** (`TI AM67A <https://www.ti.com/product/AM67A/>`__)

   -  Quad-core 64-bit ARM Cortex-A53 @1.4 GHz for running high-level operating systems such as Linux
   -  Dual single-core ARM Cortex-R5F @800 MHz for running real-time MCU applications
   -  Dual 2 TOPS (4 TOPS total) deep learning accelerators for running vision applications
   -  Advanced 50 GFLOPS GPU for high-performance graphics processing
   -  4GB LPDDR4 RAM

-  **Sensors**

   -  InvenSense ICM-20948 IMU (accel, gyro, compass)
   -  Bosch BMP390 barometer
   -  TI HDC2010 humidity and temperature

-  **Storage**

   -  **On-board**

      -  32GB eMMC flash
      -  512Kbit EEPROM

   -  **Expandable**

      -  microSD card slot
      -  M.2 2280 SSD port

-  **Network Connections**

   -  1x Gigabit ethernet
   -  1x CAN bus
   -  Wi-Fi 4 (802.11n)
   -  Bluetooth 5.1, Bluetooth Low Energy (BLE)

-  **Power**

   -  USB Type-C power (5-9V / 3A)
   -  DC power connector (5-12V / 5A)

-  **Interfaces**

   -  UART, I2C and SPI for extensions
   -  S.Bus input
   -  7x PWM servo outputs
   -  Green-red status leds
   -  Real-time clock
   -  Fan with PWM speed control
   -  4x USB ports
   -  2x 4-lane MIPI CSI/DSI
   -  1x HDMI

For more details and purchase
=============================

- `Website <https://t3gemstone.org/en/>`__
- `Board Specs <https://docs.t3gemstone.org/en/boards/o1/introduction/>`__
- `Documentation <https://docs.t3gemstone.org/en/projects/nuttx/>`__

Serial console
==============

The serial console is provided on UART-MAIN1, which is available on the 40-pin HAT:

- **UART-MAIN1 TX:** GPIO-14
- **UART-MAIN1 RX:** GPIO-15

Installation
============

While the NuttX operating system runs on the R5F cores, the Linux operating system also runs on the A53 cores.
Code can be loaded onto the R5F cores using the remoteproc mechanism present in the Linux operating system and
U-Boot.

Program files to be loaded onto the cores via remoteproc must be copied to the :code:`/lib/firmware` directory with
predefined names. At system startup, the remoteproc mechanism will automatically load the programs onto the relevant
cores. Follow the steps below to run NuttX on main domain R5F core using remoteproc.

#. Copy the :code:`nuttx` file resulting from the compilation to the :code:`/lib/firmware` directory with the name
   :code:`j722s-main-r5f0_0-fw`.
#. Reboot the board.
#. You can access NuttShell by connecting a USB-to-TTL device to the UART-MAIN1's GPIO-14 (TX) and GPIO-15 (RX) pins on
   the 40-pin HAT.

.. code-block:: console

  ❯ picocom -b 115200 /dev/ttyACM0
  NuttShell (NSH) NuttX-12.11.0
  nsh> cat proc/version
  NuttX version 12.11.0 8bdbb8c7d5-dirty Oct 22 2025 14:15:42 t3-gem-o1:nsh
  nsh>

Configurations
==============

nsh
---

Configures the NuttShell (nsh) located at examples/nsh.
This configuration enables a serial console on UART-MAIN1.
