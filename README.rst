Sky Board Zephyr Demo (C++)
###########################

This project follows the local guide
``Zephyr大型项目架构快速上手指南（C++版）``:

- C++ enabled
- no ``iostream``
- no exceptions
- no RTTI
- layered structure: ``app/``, ``include/``, ``subsys/``

Build::

  west build -b lckfb_sky_board_stm32f407 d:/zephyrproject/myproject/sky_board_zephyr_demo -d d:/zephyrproject/myproject/sky_board_zephyr_demo/build
