.. _docs-blog-01-kudzu:

==========================
Pigweed Eng Blog #1: Kudzu
==========================
.. admonition:: A note from the Pigweed Eng Blog editors

   Welcome to the Pigweed Eng Blog! This is an informal blog where Pigweed
   teammates, contributors, and users can share ideas and projects related to
   Pigweed.

   Our first post comes from Erik Gilling, a software engineer on the
   Pigweed team. Today, Erik is going to tell you about Kudzu,
   "Pigweed's whimsical take on a development board"…

   Please note that **while Kudzu is open source, its hardware isn't publicly
   available**. Pigweed users may find the `Kudzu source
   code <https://pigweed.googlesource.com/pigweed/kudzu/+/refs/heads/main>`_
   to be a helpful example of a complex Pigweed integration.

.. figure:: https://storage.googleapis.com/pigweed-media/kudzu-badges.jpg
   :alt: A photo of 6 of the Kudzu badges

   Kudzu badges for Maker Faire 2023

----------------------------
It all started so innocently
----------------------------
The Pigweed team is taking a field trip to the
`Bay Area Maker Faire <https://makerfaire.com/bay-area/>`_ because
unsurprisingly, that's the kind of good time we're up for! While discussing
the plans at a team meeting I suggested: "We should make PCB badges that run
Pigweed and wear that to the Faire!" I've always wanted to make a PCB badge
and this seemed like the perfect opportunity to make a simple PCB the we could
do a little bit of hacking on.

--------
"Simple"
--------
The idea resonated with the team… perhaps too well. What started as
something simple in my head quickly started creeping features. Hence
the name `Kudzu <https://en.wikipedia.org/wiki/Kudzu>`_: a vine
considered invasive in many parts of the world. Pigweed's a weed.
Our RFCs are called "seeds". We're all about the plant puns…

Anyways, the conversation went something like this:

  "We should have some sort of sensor so it does something…"

  "How should we power it? Let's do LiPo charging…"

  "Let's add a display to highlight our recent
  :ref:`Display Support SEED <seed-0104>`!"

  "Touch screen?"

  "D-Pad and buttons?"

  "Speaker?"

  "Wireless?"

  "No!… but also yes…"

We quickly realized that what we wanted was more than a badge. We wanted a
showcase for Pigweed. We wanted a project we can point people at to show them
Pigweed running at it's best. And thus Kudzu was born. Part badge, part
development board, part handheld gaming system, and all Pigweed.

----------------
The laundry list
----------------
We settled on the following laundry list of features and components:

`RP2040 Microcontroller <https://www.raspberrypi.com/documentation/microcontrollers/rp2040.html>`_
  There's a lot to love about the RP2040: a reasonable amount of SRAM,
  support for large/cheap/execute-in-place external flash, a wicked cool
  programmable I/O block, and most importantly: easy and cheap to source!

`16 MB of Flash <https://www.winbond.com/resource-files/w25q128jv%20revf%2003272018%20plus.pdf>`_
  We're adding the maximum amount of flash that the RP2040 can support. This
  way we can pack as much awesome into the firmware as possible. Realistically
  16 MB is an embarrassingly large amount of space for an embedded project and I
  can't wait to see what cool stuff we fill it with!

USB-C Connector
  While we're not adding USB Power Delivery to the board, the USB C connector
  is robust and common. Many of us on the team are tired of digging micro
  (or even mini) USB cables out of our desk drawers to hook up brand new dev
  boards or JTAG programmers!

LiPo Battery with `charger <https://www.microchip.com/en-us/product/mcp73831>`_ and `fuel gauge <https://www.analog.com/en/products/max17048.html>`_
  Once we decided on a portable gaming form factor, we wanted to have a
  built-in, rechargeable battery. The battery is 900mA and is set to charge at 500mA
  from USB when the system is off and 250mA when the system is running. As an
  added bonus we threw in a fuel gauge chip. Partly because it's really nice to
  have an accurate view of the battery charge state and partly because it's
  a neat chip to write software for.

`3.2" IPS display with capacitive touch <https://www.buydisplay.com/3-2-inch-240x320-ips-tft-lcd-display-optl-capacitive-touchscreen-st7789>`_
  This display is 240x320 which presents two challenges. First, it's naturally
  portrait instead of landscape. We solve this by rotating the buffers once
  they're rendered. The second is that a single 16-bit x 320 x 240 frame buffer
  is ~150K which is over half of the 264K of SRAM in the RP2040. Instead, we're
  rendering at 160x120 and using the PIO module to pixel double the buffer as
  we're sending it to the display. As an added bonus, the chunkier pixels
  gives Kudzu a nice retro feel.

Directional Pad and Buttons
  Here we're leaning on off-the-shelf buttons and silicone pads. Game
  controller design is a whole rabbit hole and we're going to rely on the
  collective wisdom of the retro modding community to give us nice-feeling
  controls.

`Six Axis IMU <https://invensense.tdk.com/products/motion-tracking/6-axis/icm-42670-p/>`_
  An IMU is a great general purpose peripheral to demonstrate Pigweed's HAL
  layer. Plus, there's all sorts of cool demos you can write with an IMU and
  a display.

`I2S Audio DAC/Amplifier <https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf>`_ and Speaker
  Chip tunes are best tunes. A couple of us on the team would love to
  port/write a tracker or FM synthesis engine.

Gameboy Advance Link Port
  As a simple way of hooking two devices together, we added link port. Again
  we're using an existing link port and cable to avoid reinventing to wheel.
  Plus, there's something awful nostalgic about that port!

... and `One More Thing <https://www.espressif.com/en/products/socs/esp32-c3>`_
  I kinda snuck an ESP32-C3 module onto the board at the last minute. Having
  wireless is something we wanted but didn't want to burden the initial design
  and bring up with it. My thinking is that we'll leave the module un-populated
  for now. My hope is that adding it to the board now may keep these boards from
  becoming landfill when we decide to tackle wireless.

.. figure:: https://storage.googleapis.com/pigweed-media/kudzu-display-connector.jpeg
   :alt: Explanation

   The reworked display connector and the unpopulated footprint for the "one more thing"
   that "we'll get to eventually"

--------------------------
Design, build, and rollout
--------------------------
I used `KiCad <https://www.kicad.org/>`_ to design the board. It's an open
source PCB design package that has been making incredible strides in
functionality and usability in the past few years. It comes with a high-quality
library of symbols and footprint which is supplemented by community-maintained
open source libraries.

.. figure:: https://storage.googleapis.com/pigweed-media/kudzu-schematic.png
   :alt: A screenshot of Kudzu's schematic

   Kudzu schematic

After some careful design review from the team and a few friends of Pigweed we
sent off the board to get fabbed and "patiently" waited for it to be delivered.

An EE at a previous company I worked at had a saying: "If you haven't found
three problems with your board, you're not done looking". The three problems
we found in order from least to most crushing are:

#. **The BOOT and RESET labels were reversed.** This led to some initial
   confusion on why the boards would not come up in bootloader mode.

#. **One of the FETs (Q3) had the wrong pinout.** This caused the power
   switch to be stuck on and the charge rate switching to not work.

#. **The pins on the display FPC connector were swapped.** This one was really
   crushing. The connector was fairly fine-pitched and 40 pins!

We were able to bring up the whole board including the display by rotating the
connector. Sadly the display would not fit in the 3D printed parts
we'd designed when plugged into the rotated connection. To validate our 3D
printed parts, I painstakingly reworked on-board to get the connector oriented
correctly. However, that was too much work and too fragile for all the boards.
We had to do a re-spin and Maker Faire was approaching quickly! Time to lather,
rinse, and repeat.

Fast forward to Monday night before Maker Faire. The boards come in and I spent
the evening preparing for a build party. On Tuesday, with some
`robotic help <https://www.opulo.io/>`_, we managed to build and test 8 boards
and get them in team members' hands on Wednesday.

.. figure:: https://storage.googleapis.com/pigweed-media/kudzu-pnp.jpg
   :alt: A photo of the Opulo LumenPnP

   Our robotic help (Opulo LumenPnP)

Thankfully, because Pigweed is modular and portable, we were able to get our
software working on it quickly, freeing us to spend the next couple days hacking
together some simple fun demos for Maker Faire!

----------
Learn more
----------
We don't have any plans to distribute hardware outside of our team but are
excited to publish the living project to serve as an example of how to build
firmware integrated with Pigweed. Over the coming months we'll be publishing
more functionality to the repository.

Head over to the `Kudzu repo <https://pigweed.googlesource.com/pigweed/kudzu>`_
where you'll find:

* KiCad PCB Design
* Example firmware demonstrating:
   * :ref:`module-pw_system` integration
   * :ref:`module-pw_rpc` and :ref:`module-pw_log` support
   * Use of Pigweed's :ref:`module-pw_digital_io`, :ref:`module-pw_i2c`,
     and :ref:`module-pw_spi` hardware abstraction layers

.. pigweed-live::
