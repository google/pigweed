.. _module-pw_display-api:

=============
API reference
=============
.. pigweed-module-subpage::
   :name: pw_display

.. _module-pw_display-api-color:

---------
Color API
---------
Moved: :doxylink:`pw_display`

---------------
Color Constants
---------------

.. role:: raw-html(raw)
   :format: html

``pw_display/colors_pico8.h``
=============================
.. list-table::
   :widths: 40 40 20
   :header-rows: 1

   * - Constant
     - Color
     - Hex Value
   * - ``pw::display::colors::rgb565::pico8::kBlack``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#000000; color:white;">Black</span>`
     - ``#000000``
   * - ``pw::display::colors::rgb565::pico8::kDarkBlue``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#1d2b53; color:white;">Dark blue</span>`
     - ``#1d2b53``
   * - ``pw::display::colors::rgb565::pico8::kDarkPurple``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#7e2553; color:white;">Dark purple</span>`
     - ``#7e2553``
   * - ``pw::display::colors::rgb565::pico8::kDarkGreen``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#008751; color:white;">Dark green</span>`
     - ``#008751``
   * - ``pw::display::colors::rgb565::pico8::kBrown``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#ab5236; color:white;">Brown</span>`
     - ``#ab5236``
   * - ``pw::display::colors::rgb565::pico8::kDarkGray``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#5f574f; color:white;">Dark gray</span>`
     - ``#5f574f``
   * - ``pw::display::colors::rgb565::pico8::kLightGray``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#c2c3c7; color:black;">Light gray</span>`
     - ``#c2c3c7``
   * - ``pw::display::colors::rgb565::pico8::kWhite``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#fff1e8; color:black;">White</span>`
     - ``#fff1e8``
   * - ``pw::display::colors::rgb565::pico8::kRed``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#ff004d; color:black;">Red</span>`
     - ``#ff004d``
   * - ``pw::display::colors::rgb565::pico8::kOrange``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#ffa300; color:black;">Orange</span>`
     - ``#ffa300``
   * - ``pw::display::colors::rgb565::pico8::kYellow``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#ffec27; color:black;">Yellow</span>`
     - ``#ffec27``
   * - ``pw::display::colors::rgb565::pico8::kGreen``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#00e436; color:black;">Green</span>`
     - ``#00e436``
   * - ``pw::display::colors::rgb565::pico8::kBlue``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#29adff; color:black;">Blue</span>`
     - ``#29adff``
   * - ``pw::display::colors::rgb565::pico8::kIndigo``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#83769c; color:black;">Indigo</span>`
     - ``#83769c``
   * - ``pw::display::colors::rgb565::pico8::kPink``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color:#ff77a8; color:black;">Pink</span>`
     - ``#ff77a8``
   * - ``pw::display::colors::rgb565::pico8::kPeach``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ffccaa; color:black;">Peach</span>`
     - ``#ffccaa``

``pw_display/colors_endesga64.h``
=================================
.. list-table::
   :widths: 40 40 20
   :header-rows: 1

   * - Constant
     - Color
     - Hex Value

   * - ``pw::display::colors::rgb565::e64::kBlood``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ff0040;">&nbsp;</span>`
     - ``#ff0040``
   * - ``pw::display::colors::rgb565::e64::kBlack0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #131313;">&nbsp;</span>`
     - ``#131313``
   * - ``pw::display::colors::rgb565::e64::kBlack1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #1b1b1b;">&nbsp;</span>`
     - ``#1b1b1b``
   * - ``pw::display::colors::rgb565::e64::kGray0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #272727;">&nbsp;</span>`
     - ``#272727``
   * - ``pw::display::colors::rgb565::e64::kGray1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #3d3d3d;">&nbsp;</span>`
     - ``#3d3d3d``
   * - ``pw::display::colors::rgb565::e64::kGray2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #5d5d5d;">&nbsp;</span>`
     - ``#5d5d5d``
   * - ``pw::display::colors::rgb565::e64::kGray3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #858585;">&nbsp;</span>`
     - ``#858585``
   * - ``pw::display::colors::rgb565::e64::kGray4``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #b4b4b4;">&nbsp;</span>`
     - ``#b4b4b4``
   * - ``pw::display::colors::rgb565::e64::kWhite``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ffffff;">&nbsp;</span>`
     - ``#ffffff``
   * - ``pw::display::colors::rgb565::e64::kSteel6``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #c7cfdd;">&nbsp;</span>`
     - ``#c7cfdd``
   * - ``pw::display::colors::rgb565::e64::kSteel5``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #92a1b9;">&nbsp;</span>`
     - ``#92a1b9``
   * - ``pw::display::colors::rgb565::e64::kSteel4``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #657392;">&nbsp;</span>`
     - ``#657392``
   * - ``pw::display::colors::rgb565::e64::kSteel3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #424c6e;">&nbsp;</span>`
     - ``#424c6e``
   * - ``pw::display::colors::rgb565::e64::kSteel2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #2a2f4e;">&nbsp;</span>`
     - ``#2a2f4e``
   * - ``pw::display::colors::rgb565::e64::kSteel1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #1a1932;">&nbsp;</span>`
     - ``#1a1932``
   * - ``pw::display::colors::rgb565::e64::kSteel0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #0e071b;">&nbsp;</span>`
     - ``#0e071b``
   * - ``pw::display::colors::rgb565::e64::kCoffee0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #1c121c;">&nbsp;</span>`
     - ``#1c121c``
   * - ``pw::display::colors::rgb565::e64::kCoffee1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #391f21;">&nbsp;</span>`
     - ``#391f21``
   * - ``pw::display::colors::rgb565::e64::kCoffee2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #5d2c28;">&nbsp;</span>`
     - ``#5d2c28``
   * - ``pw::display::colors::rgb565::e64::kCoffee3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #8a4836;">&nbsp;</span>`
     - ``#8a4836``
   * - ``pw::display::colors::rgb565::e64::kCoffee4``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #bf6f4a;">&nbsp;</span>`
     - ``#bf6f4a``
   * - ``pw::display::colors::rgb565::e64::kCoffee5``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #e69c69;">&nbsp;</span>`
     - ``#e69c69``
   * - ``pw::display::colors::rgb565::e64::kCoffee6``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #f6ca9f;">&nbsp;</span>`
     - ``#f6ca9f``
   * - ``pw::display::colors::rgb565::e64::kCoffee7``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #f9e6cf;">&nbsp;</span>`
     - ``#f9e6cf``
   * - ``pw::display::colors::rgb565::e64::kOrange3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #edab50;">&nbsp;</span>`
     - ``#edab50``
   * - ``pw::display::colors::rgb565::e64::kOrange2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #e07438;">&nbsp;</span>`
     - ``#e07438``
   * - ``pw::display::colors::rgb565::e64::kOrange1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #c64524;">&nbsp;</span>`
     - ``#c64524``
   * - ``pw::display::colors::rgb565::e64::kOrange0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #8e251d;">&nbsp;</span>`
     - ``#8e251d``
   * - ``pw::display::colors::rgb565::e64::kBrightOrange0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ff5000;">&nbsp;</span>`
     - ``#ff5000``
   * - ``pw::display::colors::rgb565::e64::kBrightOrange1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ed7614;">&nbsp;</span>`
     - ``#ed7614``
   * - ``pw::display::colors::rgb565::e64::kBrightOrange2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ffa214;">&nbsp;</span>`
     - ``#ffa214``
   * - ``pw::display::colors::rgb565::e64::kYellow0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ffc825;">&nbsp;</span>`
     - ``#ffc825``
   * - ``pw::display::colors::rgb565::e64::kYellow1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ffeb57;">&nbsp;</span>`
     - ``#ffeb57``
   * - ``pw::display::colors::rgb565::e64::kGreen5``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #d3fc7e;">&nbsp;</span>`
     - ``#d3fc7e``
   * - ``pw::display::colors::rgb565::e64::kGreen4``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #99e65f;">&nbsp;</span>`
     - ``#99e65f``
   * - ``pw::display::colors::rgb565::e64::kGreen3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #5ac54f;">&nbsp;</span>`
     - ``#5ac54f``
   * - ``pw::display::colors::rgb565::e64::kGreen2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #33984b;">&nbsp;</span>`
     - ``#33984b``
   * - ``pw::display::colors::rgb565::e64::kGreen1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #1e6f50;">&nbsp;</span>`
     - ``#1e6f50``
   * - ``pw::display::colors::rgb565::e64::kGreen0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #134c4c;">&nbsp;</span>`
     - ``#134c4c``
   * - ``pw::display::colors::rgb565::e64::kOcean0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #0c2e44;">&nbsp;</span>`
     - ``#0c2e44``
   * - ``pw::display::colors::rgb565::e64::kOcean1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #00396d;">&nbsp;</span>`
     - ``#00396d``
   * - ``pw::display::colors::rgb565::e64::kOcean2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #0069aa;">&nbsp;</span>`
     - ``#0069aa``
   * - ``pw::display::colors::rgb565::e64::kOcean3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #0098dc;">&nbsp;</span>`
     - ``#0098dc``
   * - ``pw::display::colors::rgb565::e64::kOcean4``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #00cdf9;">&nbsp;</span>`
     - ``#00cdf9``
   * - ``pw::display::colors::rgb565::e64::kOcean5``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #0cf1ff;">&nbsp;</span>`
     - ``#0cf1ff``
   * - ``pw::display::colors::rgb565::e64::kOcean6``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #94fdff;">&nbsp;</span>`
     - ``#94fdff``
   * - ``pw::display::colors::rgb565::e64::kCandyGrape3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #fdd2ed;">&nbsp;</span>`
     - ``#fdd2ed``
   * - ``pw::display::colors::rgb565::e64::kCandyGrape2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #f389f5;">&nbsp;</span>`
     - ``#f389f5``
   * - ``pw::display::colors::rgb565::e64::kCandyGrape1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #db3ffd;">&nbsp;</span>`
     - ``#db3ffd``
   * - ``pw::display::colors::rgb565::e64::kCandyGrape0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #7a09fa;">&nbsp;</span>`
     - ``#7a09fa``
   * - ``pw::display::colors::rgb565::e64::kRoyalBlue2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #3003d9;">&nbsp;</span>`
     - ``#3003d9``
   * - ``pw::display::colors::rgb565::e64::kRoyalBlue1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #0c0293;">&nbsp;</span>`
     - ``#0c0293``
   * - ``pw::display::colors::rgb565::e64::kRoyalBlue0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #03193f;">&nbsp;</span>`
     - ``#03193f``
   * - ``pw::display::colors::rgb565::e64::kPurple0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #3b1443;">&nbsp;</span>`
     - ``#3b1443``
   * - ``pw::display::colors::rgb565::e64::kPurple1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #622461;">&nbsp;</span>`
     - ``#622461``
   * - ``pw::display::colors::rgb565::e64::kPurple2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #93388f;">&nbsp;</span>`
     - ``#93388f``
   * - ``pw::display::colors::rgb565::e64::kPurple3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ca52c9;">&nbsp;</span>`
     - ``#ca52c9``
   * - ``pw::display::colors::rgb565::e64::kSalmon0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #c85086;">&nbsp;</span>`
     - ``#c85086``
   * - ``pw::display::colors::rgb565::e64::kSalmon1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #f68187;">&nbsp;</span>`
     - ``#f68187``
   * - ``pw::display::colors::rgb565::e64::kRed4``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #f5555d;">&nbsp;</span>`
     - ``#f5555d``
   * - ``pw::display::colors::rgb565::e64::kRed3``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #ea323c;">&nbsp;</span>`
     - ``#ea323c``
   * - ``pw::display::colors::rgb565::e64::kRed2``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #c42430;">&nbsp;</span>`
     - ``#c42430``
   * - ``pw::display::colors::rgb565::e64::kRed1``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #891e2b;">&nbsp;</span>`
     - ``#891e2b``
   * - ``pw::display::colors::rgb565::e64::kRed0``
     - :raw-html:`<span class="sd-sphinx-override sd-badge" style="width: 100%;
       background-color: #571c27;">&nbsp;</span>`
     - ``#571c27``
