.. _seed-0104:

=====================
0104: Display Support
=====================
.. seed::
   :number: 104
   :name: Display Support
   :status: Accepted
   :proposal_date: 2023-06-12
   :cl: 150793
   :authors: Chris Mumford
   :facilitator: Anthony DiGirolamo

-------
Summary
-------
Add support for graphics displays. This includes display drivers for a few
popular display controllers, framebuffer management, and a framework to simplify
adding a graphics display to a Pigweed application.

----------
Motivation
----------
Pigweed currently has no specific support for a display device. Projects that
require a display currently must do the full implementation, including the
display driver in most instances, to add display support.

This proposes the addition of a basic framework for display devices, as well
as implementations for a few common Pigweed test devices - specifically the
STM32F429I. This enables developers to quickly and easily add display support
for supported devices and an implementation to model when adding new device
support.

---------------
Proposal
---------------
This proposes no changes to existing modules, but suggests several new modules
that together define a framework for rendering to displays.


New Modules
-----------

.. list-table::
   :widths: 5 45
   :header-rows: 1

   * - Module
     - Function

   * - pw_display
     - Manage draw thread, framebuffers, and driver

   * - pw_display_driver
     - Display driver interface definition

   * - pw_display_driver_ili9341
     - Display driver for the ILI9341 display controller

   * - pw_display_driver_imgui
     - Host display driver using `Dear ImGui <https://www.dearimgui.com/>`_

   * - pw_display_driver_mipi
     - Display driver for `MIPI DSI <https://www.mipi.org/specifications/dsi>`_ controllers

   * - pw_display_driver_null
     - Null display driver for headless devices

   * - pw_display_driver_st7735
     - Display driver for the `ST7735 <https://www.displayfuture.com/Display/datasheet/controller/ST7735.pdf>`_ display controller

   * - pw_display_driver_st7789
     - Display driver for the ST7789 display controller

   * - pw_simple_draw
     - Very basic drawing library for test purposes

   * - pw_framebuffer
     - Manage access to pixel buffer.

   * - pw_framebuffer_mcuxpresso
     - Specialization of the framebuffer for the MCUxpresso devices

   * - pw_geometry
     - Basic shared math types such as 2D vectors, etc.

   * - pw_pixel_pusher
     - Transport of pixel data to display controller


Math
----
``pw_geometry`` contains two helper structures for common values usually used as
a pair.

.. code-block:: cpp

   namespace pw::math {

   template <typename T>
   struct Size {
     T width;
     T height;
   };

   template <typename T>
   struct Vector2 {
     T x;
     T y;
   };

   }  // namespace pw::math


Framebuffer
-----------
A framebuffer is a small class that provides access to a pixel buffer. It
keeps a copy of the pixel buffer metadata and provides accessor methods for
those values.

.. code-block:: cpp

   namespace pw::framebuffer {

   enum class PixelFormat {
     None,
     RGB565,
   };

   class Framebuffer {
   public:
     // Construct a default invalid framebuffer.
     Framebuffer();

     Framebuffer(void* data,
                 PixelFormat pixel_format,
                 pw::math::Size<uint16_t> size,
                 uint16_t row_bytes);

     Framebuffer(const Framebuffer&) = delete;
     Framebuffer(Framebuffer&& other);

     Framebuffer& operator=(const Framebuffer&) = delete;
     Framebuffer& operator=(Framebuffer&&);

     bool is_valid() const;

     pw::ConstByteSpan data() const;
     pw::ByteSpan data();

     PixelFormat pixel_format() const;

     pw::math::Size<uint16_t> size();

     uint16_t row_bytes() const;
   };

   }  // namespace pw::framebuffer

FrameBuffer is a moveable class that is intended to signify read/write
privileges to the underlying pixel data. This makes it easier to track when the
pixel data may be read from, or written to, without conflict.

The framebuffer does not own the underlying pixel buffer. In other words
the deletion of a framebuffer will not free the underlying pixel data.

Framebuffers do not have methods for reading or writing to the underlying pixel
buffer. This is the responsibility of the the selected graphics library which
can be given the pixel buffer pointer retrieved by calling ``data()``.

.. code-block:: cpp

   constexpr size_t kWidth = 64;
   constexpr size_t kHeight = 32;
   uint16_t pixel_data[kWidth * kHeight];

   void DrawScreen(Framebuffer* fb) {
     // Clear framebuffer to black.
     std::memset(fb->data(), 0, fb->height() * fb->row_bytes());

     // Set first pixel to white.
     uint16_t* pixel_data = static_cast<uint16_t*>(fb->data());
     pixel_data[0] = 0xffff;
   }

   Framebuffer fb(pixel_data, {kWidth, kHeight},
                  PixelFormat::RGB565,
                  kWidth * sizeof(uint16_t));
   DrawScreen(&fb);

FramebufferPool
---------------

The FramebufferPool is intended to simplify the use of multiple framebuffers
when multi-buffered rendering is being used. It is a collection of framebuffers
which can be retrieved, used, and then returned to the pool for reuse. All
framebuffers in the pool share identical attributes. A framebuffer that is
returned to a caller of ``GetFramebuffer()`` can be thought of as "on loan" to
that caller and will not be given to any other caller of ``GetFramebuffer()``
until it has been returned by calling ``ReleaseFramebuffer()``.

.. code-block:: cpp

   namespace pw::framebuffer_pool {

   class FramebufferPool {
   public:
     using BufferArray = std::array<void*, FRAMEBUFFER_COUNT>;

     // Constructor parameters.
     struct Config {
       BufferArray fb_addr;  // Address of each buffer in this pool.
       pw::math::Size<uint16_t> dimensions;  // width/height of each buffer.
       uint16_t row_bytes;                   // row bytes of each buffer.
       pw::framebuffer::PixelFormat pixel_format;
     };

     FramebufferPool(const Config& config);
     virtual ~FramebufferPool();

     uint16_t row_bytes() const;

     pw::math::Size<uint16_t> dimensions() const;

     pw::framebuffer::PixelFormat pixel_format() const;

     // Return a framebuffer to the caller for use. This call WILL BLOCK until a
     // framebuffer is returned for use. Framebuffers *must* be returned to this
     // pool by a corresponding call to ReleaseFramebuffer. This function will only
     // return a valid framebuffer.
     //
     // This call is thread-safe, but not interrupt safe.
     virtual pw::framebuffer::Framebuffer GetFramebuffer();

     // Return the framebuffer to the pool available for use by the next call to
     // GetFramebuffer.
     //
     // This may be called on another thread or during an interrupt.
     virtual Status ReleaseFramebuffer(pw::framebuffer::Framebuffer framebuffer);
   };

   }  // namespace pw::framebuffer

An example use of the framebuffer pool is:

.. code-block:: cpp

   // Retrieve a framebuffer for drawing. May block if pool has no framebuffers
   // to issue.
   FrameBuffer fb = framebuffer_pool.GetFramebuffer();

   // Draw to the framebuffer.
   UpdateDisplay(&fb);

   // Return the framebuffer to the pool for reuse.
   framebuffer_pool.ReleaseFramebuffer(std::move(fb));

DisplayDriver
-------------

A DisplayDriver is usually the sole class responsible for communicating with the
display controller. Its primary responsibilities are the display controller
initialization, and the writing of pixel data when a display update is needed.

This proposal supports multiple heterogenous display controllers. This could be:

1. A single display of any given type (e.g. ILI9341).
2. Two ILI9341 displays.
3. Two ILI9341 displays and a second one of a different type.

Because of this approach the DisplayDriver is defined as an interface:

.. code-block:: cpp

   namespace pw::display_driver {

   class DisplayDriver {
   public:
     // Called on the completion of a write operation.
     using WriteCallback = Callback<void(framebuffer::Framebuffer, Status)>;

     virtual ~DisplayDriver() = default;

     virtual Status Init() = 0;

     virtual void WriteFramebuffer(pw::framebuffer::Framebuffer framebuffer,
                                   WriteCallback write_callback) = 0;

     virtual pw::math::Size<uint16_t> size() const = 0;
   };

   }  // namespace pw::display_driver

Each driver then provides a concrete implementation of the driver. Below is the
definition of the display driver for the ILI9341:

.. code-block:: cpp

   namespace pw::display_driver {

   class DisplayDriverILI9341 : public DisplayDriver {
   public:
     struct Config {
       // Device specific initialization parameters.
     };

     DisplayDriverILI9341(const Config& config);

     // DisplayDriver implementation:
     Status Init() override;
     void WriteFramebuffer(pw::framebuffer::Framebuffer framebuffer,
                           WriteCallback write_callback) override;
     Status WriteRow(span<uint16_t> row_pixels,
                     uint16_t row_idx,
                     uint16_t col_idx) override;
     pw::math::Size<uint16_t> size() const override;

   private:
     enum class Mode {
       kData,
       kCommand,
     };

     // A command and optional data to write to the ILI9341.
     struct Command {
       uint8_t command;
       ConstByteSpan command_data;
     };

     // Toggle the reset GPIO line to reset the display controller.
     Status Reset();

     // Set the command/data mode of the display controller.
     void SetMode(Mode mode);
     // Write the command to the display controller.
     Status WriteCommand(pw::spi::Device::Transaction& transaction,
                         const Command& command);
   };

   }  // namespace pw::display_driver

Here is an example retrieving a framebuffer from the framebuffer pool, drawing
into the framebuffer, using the display driver to write the pixel data, and then
returning the framebuffer back to the pool for use.

.. code-block:: cpp

   FrameBuffer fb = framebuffer_pool.GetFramebuffer();

   // DrawScreen is a function that will draw to the framebuffer's underlying
   // pixel buffer using a drawing library. See example above.
   DrawScreen(&fb);

   display_driver_.WriteFramebuffer(
       std::move(framebuffer),
       [&framebuffer_pool](pw::framebuffer::Framebuffer fb, Status status) {
         // Return the framebuffer back to the pool for reuse once the display
         // write is complete.
         framebuffer_pool.ReleaseFramebuffer(std::move(fb));
       });

In the example above that the framebuffer (``fb``) is moved when calling
``WriteFramebuffer()`` passing ownership to the display driver. From this point
forward the application code may not access the framebuffer in any way. When the
framebuffer write is complete the framebuffer is then moved to the callback
which in turn moves it when calling ``ReleaseFramebuffer()``.

``WriteFramebuffer()`` always does a write of the full framebuffer - sending all
pixel data.

``WriteFramebuffer()`` may be a blocking call, but on some platforms the driver
may use a background write and the write callback is called when the write
is complete. The write callback **may be called during an interrupt**.

PixelPusher
-----------
Pixel data for Simple SPI based display controllers can be written to the
display controller using ``pw_spi``. There are some controllers which use
other interfaces (RGB, MIPI, etc.). Also, some vendors provide an API for
interacting with these display controllers for writing pixel data.

To allow the drivers to be hardware/vendor independent the ``PixelPusher``
may be used. This defines an interface whose sole responsibility is to write
a framebuffer to the display controller. Specializations of this will use
``pw_spi`` or vendor proprietary calls to write pixel data.

.. code-block:: cpp

   namespace pw::pixel_pusher {

   class PixelPusher {
    public:
     using WriteCallback = Callback<void(framebuffer::Framebuffer, Status)>;

     virtual ~PixelPusher() = default;

     virtual Status Init(
         const pw::framebuffer_pool::FramebufferPool& framebuffer_pool) = 0;

     virtual void WriteFramebuffer(framebuffer::Framebuffer framebuffer,
                                   WriteCallback complete_callback) = 0;
   };

   }  // namespace pw::pixel_pusher

Display
-------

Each display has:

1. One and only one display driver.
2. A reference to a single framebuffer pool. This framebuffer pool may be shared
   with other displays.
3. A drawing thread, if so configured, for asynchronous display updates.

.. code-block:: cpp

   namespace pw::display {

   class Display {
   public:
     // Called on the completion of an update.
     using WriteCallback = Callback<void(Status)>;

     Display(pw::display_driver::DisplayDriver& display_driver,
             pw::math::Size<uint16_t> size,
             pw::framebuffer_pool::FramebufferPool& framebuffer_pool);
     virtual ~Display();

     pw::framebuffer::Framebuffer GetFramebuffer();

     void ReleaseFramebuffer(pw::framebuffer::Framebuffer framebuffer,
                             WriteCallback callback);

     pw::math::Size<uint16_t> size() const;
   };

   }  // namespace pw::display

Once applications are initialized they typically will not directly interact with
display drivers or framebuffer pools. These will be utilized by the display
which will provide a simpler interface.

``Display::GetFramebuffer()`` must always be called on the same thread and is not
interrupt safe. It will block if there is no available framebuffer in the
framebuffer pool waiting for a framebuffer to be returned.

``Display::ReleaseFramebuffer()`` must be called for each framebuffer returned by
``Display::GetFramebuffer()``. This will initiate the display update using the
displays associated driver. The ``callback`` will be called when this update is
complete.

A simplified application rendering loop would resemble:

.. code-block:: cpp

   // Get a framebuffer for drawing.
   FrameBuffer fb = display.GetFramebuffer();

   // DrawScreen is a function that will draw to |fb|'s pixel buffer using a
   // drawing library. See example above.
   DrawScreen(&fb);

   // Return the framebuffer to the display which will be written to the display
   // controller by the display's display driver.
   display.ReleaseFramebuffer(std::move(fb), [](Status){});

Simple Drawing Module
---------------------

``pw_simple_draw`` was created for testing and verification purposes only. It is
not intended to be feature rich or performant in any way. This is small
collection of basic drawing primitives not intended to be used by shipping
applications.

.. code-block:: cpp

   namespace pw::draw {

   void DrawLine(pw::framebuffer::Framebuffer& fb,
                 int x1,
                 int y1,
                 int x2,
                 int y2,
                 pw::color::color_rgb565_t pen_color);

   // Draw a circle at center_x, center_y with given radius and color. Only a
   // one-pixel outline is drawn if filled is false.
   void DrawCircle(pw::framebuffer::Framebuffer& fb,
                   int center_x,
                   int center_y,
                   int radius,
                   pw::color::color_rgb565_t pen_color,
                   bool filled);

   void DrawHLine(pw::framebuffer::Framebuffer& fb,
                  int x1,
                  int x2,
                  int y,
                  pw::color::color_rgb565_t pen_color);

   void DrawRect(pw::framebuffer::Framebuffer& fb,
                 int x1,
                 int y1,
                 int x2,
                 int y2,
                 pw::color::color_rgb565_t pen_color,
                 bool filled);

   void DrawRectWH(pw::framebuffer::Framebuffer& fb,
                   int x,
                   int y,
                   int w,
                   int h,
                   pw::color::color_rgb565_t pen_color,
                   bool filled);

   void Fill(pw::framebuffer::Framebuffer& fb,
             pw::color::color_rgb565_t pen_color);

   void DrawSprite(pw::framebuffer::Framebuffer& fb,
                   int x,
                   int y,
                   pw::draw::SpriteSheet* sprite_sheet,
                   int integer_scale);

   void DrawTestPattern();

   pw::math::Size<int> DrawCharacter(int ch,
                                     pw::math::Vector2<int> pos,
                                     pw::color::color_rgb565_t fg_color,
                                     pw::color::color_rgb565_t bg_color,
                                     const FontSet& font,
                                     pw::framebuffer::Framebuffer& framebuffer);

   pw::math::Size<int> DrawString(std::wstring_view str,
                                  pw::math::Vector2<int> pos,
                                  pw::color::color_rgb565_t fg_color,
                                  pw::color::color_rgb565_t bg_color,
                                  const FontSet& font,
                                  pw::framebuffer::Framebuffer& framebuffer);

   }  // namespace pw::draw

Class Interaction Diagram
-------------------------

.. mermaid::
   :alt: Framebuffer Classes
   :align: center

   classDiagram
       class FramebufferPool {
           uint16_t row_bytes()
           PixelFormat pixel_format()
           dimensions() : Size~uint16_t~
           row_bytes() : uint16_t
           GetFramebuffer() : Framebuffer

           BufferArray buffer_addresses_
           Size~uint16_t~ buffer_dimensions_
           uint16_t row_bytes_
           PixelFormat pixel_format_
       }

       class Framebuffer {
           is_valid() : bool const
           data() : void* const
           pixel_format() : PixelFormat const
           size() : Size~uint16_t~ const
           row_bytes() uint16_t const

           void* pixel_data_
           Size~uint16_t~ size_
           PixelFormat pixel_format_
           uint16_t row_bytes_
       }

       class DisplayDriver {
           <<DisplayDriver>>
           Init() : Status
           WriteFramebuffer(Framebuffer fb, WriteCallback cb): void
           dimensions() : Size~uint16_t~

           PixelPusher& pixel_pusher_
       }

       class Display {
           DisplayDriver& display_driver_
           const Size~uint16_t~ size_
           FramebufferPool& framebuffer_pool_

           GetFramebuffer() : Framebuffer
       }

       class PixelPusher {
           Init() : Status
           WriteFramebuffer(Framebuffer fb, WriteCallback cb) : void
       }

       <<interface>> DisplayDriver
       FramebufferPool --> "FRAMEBUFFER_COUNT" Framebuffer : buffer_addresses_

       Display --> "1" DisplayDriver : display_driver_
       Display --> "1" FramebufferPool : framebuffer_pool_
       DisplayDriver --> "1" PixelPusher : pixel_pusher_

---------------------
Problem investigation
---------------------
With no direct display support in Pigweed and no example programs implementing
a solution Pigweed developers are essentially on their own. Depending on their
hardware this means starting with a GitHub project with a sample application
from MCUXpresso or STMCube. Each of these use a specific HAL and may be
coupled to other frameworks, such as FreeRTOS. This places the burden of
substituting the HAL calls with the Pigweed API, making the sample program
with the application screen choice, etc.

This chore is time consuming and often requires that the application developer
acquire some level of driver expertise. Having direct display support in
Pigweed will allow the developer to more quickly add display support.

The primary use-case being targeted is an application with a single display
using multiple framebuffers with display update notifications delivered during
an interrupt. The initial implementation is designed to support multiple
heterogenous displays, but that will not be the focus of development or testing
for the first release.

Touch sensors, or other input devices, are not part of this effort. Display
and touch input often accompany each other, but to simplify this already large
display effort, touch will be added in a separate activity.

There are many other embedded libraries for embedded drawing. Popular  libraries
are LVGL, emWin, GUIslice, HAGL, µGFX, and VGLite (to just name a few). These
existing solutions generally offer one or more of: display drivers, drawing
library, widget library. The display drivers usually rely on an abstraction
layer, which they often refer to as a HAL, to interface with the underlying
hardware API. This HAL may rely on macros, or sometimes a structure with
function pointers for specific operations.

The approach in this SEED was selected because it offers a low level API focused
on display update performance. It offers no drawing or GUI library, but should
be easily interfaced with those libraries.

---------------
Detailed design
---------------

This proposal suggests no changes to existing APIs. All changes introduce new
modules that leverage the existing API. It supports static allocation of the
pixel buffers and all display framework objects. Additionally pixel buffers
may be hard-coded addresses or dynamically allocated from SRAM.

The ``Framebuffer`` class is intended to simplify code that interacts with the
pixel buffer. It includes the pixel buffer format, dimensions, and the buffer
address. The framebuffer is 16 bytes in size (14 when packed). Framebuffer
objects are created when requested and moved as a means of signifying ownership.
In other words, whenever code has an actual framebuffer object it is allowed
to both write to and read from the pixel buffer.

The ``FramebufferPool`` is an object intended to simplify the management of a
collection of framebuffers. It tracks those that are available for use and
loans out framebuffers when requested. For single display devices this is
generally not a difficult task as the application would maintain an array of
framebuffers and a next available index. In this case framebuffers are always
used in order and the buffer collection is implemented as a queue.

Because RAM is often limited, the framebuffer pool is designed to be shared
between multiple displays. Because display rendering and update may be at
different speeds framebuffers do not need to be retrieved
(via ``GetFramebuffer()``) and returned (via ``ReleaseFramebuffer()``) in the same
order.

Whenever possible asynchronous display updates will be used. Depending on the
implementation this usually offloads the CPU from the pixel writing to the
display controller. In this case the CPU will initiate the update and using
some type of notification, usually an interrupt raised by a GPIO pin connected
to the display, will be notified of the completion of the display update.
Because of this the framebuffer pool ``ReleaseFramebuffer()`` call is interrupt
safe.

``FramebufferPool::GetFramebuffer()`` will block indefinitely if no framebuffer
is available. This unburdens the application drawing loop from the task of
managing framebuffers or tracking screen update completion.

Testing
-------
All classes will be accompanied by a robust set of unit tests. These can be
run on the host or the device. Test applications will be able to run on a
workstation (i.e. not an MCU) in order to enable tests that depend on
hardware available in most CPUs - like an MMU. This will enable the use of
`AddressSanitizer <https://github.com/google/sanitizers/wiki/AddressSanitizer>`_
based tests. Desktop tests will use
`Xvfb <https://www.x.org/releases/X11R7.6/doc/man/man1/Xvfb.1.xhtml>`_ to allow
them to be run in a headless continuous integration environment.

Performance
-----------
Display support will include performance tests. Although this proposal does not
include a rendering library, it will include support for specific platforms
that will utilize means of transferring pixel data to the display controller
in the background.

------------
Alternatives
------------

One alternative is to create the necessary port/HAL, the terminology varies by
library, for the popular embedded graphics libraries. This would make it easier
for Pigweed applications to add display support - bot only for those supported
libraries. This effort is intended to be more focused on performance, which is
not always the focus of other libraries.

Another alternative is to do nothing - leaving the job of adding display
support to the developers. As a significant percentage of embedded projects
contain a display, it will beneficial to have built-in display support in
Pigweed. This will allow all user to benefit by the shared display expertise,
continuous integration, testing, and performance testing.

--------------
Open questions
--------------

Parameter Configuration
-----------------------
One open question is what parameters to specify in initialization parameters
to a driver ``Init()`` function, which to set in build flags via ``config(...)``
in GN, and which to hard-code into the driver. The most ideal, from the
perspective of reducing binary size, is to hard-code all values in a single
block of contiguous data. The decision to support multiple displays requires
that the display initialization parameters, at least some of them, be defined
at runtime and cannot be hard-coded into the driver code - that is, if the
goal is to allow two of the same display to be in use with different settings.

Additionally many drivers support dozens of configuration values. The ILI9341
has 82 different commands, some with complex values like gamma tables or
multiple values packed into a single register.

The current approach is to strike a balance where the most commonly set
values, for example display width/height and pixel format, are configurable
via build flags, and the remainder is hard-coded in the driver. If a developer
wants to set a parameter that is currently hard-coded in the driver, for
example display refresh rate or gamma table, they would need to copy the display
driver from Pigweed, or create a Pigweed branch.

``Display::WriteFramebuffer()`` always writes the full framebuffer. It is expected
that partial updates will be supported. This will likely come as a separate
function. This is being pushed off until needed to provide as much experience
with the various display controller APIs as possible to increase the likelihood
of a well crafted API.

Module Hierarchy
----------------
At present Pigweed's module structure is flat and at the project root level.
There are currently 134 top level ``pw_*`` directories. This proposal could
significantly increase this count as each new display driver will be a new
module. This might be a good time to consider putting modules into a hierarchy.

Pixel Pusher
------------
``PixelPusher`` was created to remove the details of writing pixels from the
display driver. Many displays support multiple ways to send pixel data. For
example the ILI9341 supports SPI and a parallel bus for pixel transport.
The `STM32F429I-DISC1 <https://www.st.com/en/evaluation-tools/32f429idiscovery.html>`_
also has a display controller (`LTDC <https://www.st.com/resource/en/application_note/an4861-lcdtft-display-controller-ltdc-on-stm32-mcus-stmicroelectronics.pdf>`_)
which uses an STM proprietary API. The ``PixelPusher`` was essentially created
to allow that driver to use the LTDC API without the need to be coupled in any
way to a vendor API.

At present some display drivers use ``pw_spi`` to send commands to the display
controller, and the ``PixelPusher`` for writing pixel data. It will probably
be cleaner to move the command writes into the ``PixelPusher`` and remove any
``pw_spi`` interaction from the display drivers. At this time ``PixelPusher``
should be renamed.

Copyrighted SDKs
----------------
Some vendors have copyrighted SDKs which cannot be included in the Pigweed
source code unless the project is willing to have the source covered by more
than one license. Additionally some SDKs have no simple download link and the
vendor requires that a developer use a web application to build and download
an SDK with the desired components. NXP's
`MCUXpresso SDK Builder <https://mcuxpresso.nxp.com/en/welcome>`_ is an example
of this. This download process makes it difficult to provide simple instructions
to the developer and for creating reliable builds as it may be difficult to
select an older SDK for download.
