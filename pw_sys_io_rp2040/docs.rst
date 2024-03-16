.. _module-pw_sys_io_rp2040:

----------------
pw_sys_io_rp2040
----------------

``pw_sys_io_rp2040`` implements the ``pw_sys_io`` facade using the Pico's STDIO
library.   This backend will block on reads using a ``ThreadNotification``
however writes will busy wait on completion.
