#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#
# CMakeLists.txt file for creating of uut library.
#

add_library(uut STATIC
  ${ZEPHYR_BASE}/subsys/bluetooth/audio/cap_commander.c
  ${ZEPHYR_BASE}/subsys/bluetooth/audio/cap_common.c
  ${ZEPHYR_BASE}/subsys/logging/log_minimal.c
  ${ZEPHYR_BASE}/subsys/net/buf_simple.c
  csip.c
  vcp.c
)

add_subdirectory(${ZEPHYR_BASE}/tests/bluetooth/audio/mocks mocks)

target_link_libraries(uut PUBLIC test_interface mocks)

target_compile_options(uut PRIVATE -std=c11 -include ztest.h)
