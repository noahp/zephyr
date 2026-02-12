.. zephyr:code-sample:: hello_world
   :name: Hello World

   Print "Hello World" to the console and demonstrate shared multi-heap runtime statistics.

Overview
********

A simple sample that can be used with any :ref:`supported board <boards>` and
prints "Hello World" to the console.

When ``CONFIG_SHARED_MULTI_HEAP=y`` is enabled, this sample also demonstrates
how to use the shared multi-heap runtime statistics API to track memory usage,
including:

- Current free bytes
- Current allocated bytes
- Maximum allocated bytes (peak usage)
- Minimum free bytes (derived from peak usage)

The demo shows how the statistics update as memory is allocated and freed,
which is useful for monitoring heap usage and detecting memory leaks.

Building and Running
********************

This application can be built and executed on QEMU as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/hello_world
   :host-os: unix
   :board: qemu_x86
   :goals: run
   :compact:

To build for another board, change "qemu_x86" above to that board's name.

Sample Output
=============

Without shared multi-heap (default):

.. code-block:: console

    Hello World! x86

With shared multi-heap enabled (``CONFIG_SHARED_MULTI_HEAP=y``):

.. code-block:: console

    Hello World! x86

    === Shared Multi-Heap Runtime Stats Demo ===

    Added 4096 byte heap with CACHEABLE attribute

    Initial state:
      Free bytes:          4064
      Allocated bytes:     0
      Max allocated bytes: 0
      Min free bytes:      4064

    Allocating 256 bytes...
    After first allocation:
      Free bytes:          3792
      Allocated bytes:     272
      Max allocated bytes: 272
      Min free bytes:      3792

    Allocating 512 bytes...
    After second allocation:
      Free bytes:          3264
      Allocated bytes:     800
      Max allocated bytes: 800
      Min free bytes:      3264

    Allocating 1024 bytes...
    After third allocation (peak usage):
      Free bytes:          2224
      Allocated bytes:     1840
      Max allocated bytes: 1840
      Min free bytes:      2224

    Freeing second allocation (512 bytes)...
    After freeing middle allocation:
      Free bytes:          2752
      Allocated bytes:     1312
      Max allocated bytes: 1840
      Min free bytes:      2224

    Note: max_allocated_bytes shows peak usage!
          min_free_bytes = total_size - max_allocated_bytes

    === Demo Complete ===

Exit QEMU by pressing :kbd:`CTRL+A` :kbd:`x`.
