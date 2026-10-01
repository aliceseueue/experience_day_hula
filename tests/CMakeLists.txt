# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rf_attenuator_proto_test)

target_sources(app PRIVATE
    src/proto.c
    tests/src/proto_test.c
)

target_include_directories(app PRIVATE
    src
)
