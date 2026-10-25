# SPDX-License-Identifier: BSD-3-Clause
TARGET := msm8953

DEBUG_FBCON := 1
LK2ND_FORCE_FASTBOOT := 1
DEFINES += ENABLE_KASLRSEED_SUPPORT=1
DEFINES += WITH_DEBUG_FBCON=1
DEFINES += OSVERSION_IN_BOOTIMAGE=1
DTBO_BACKEND := libufdt

LK2ND_QCDTBS :=
MKBOOTIMG_ARGS += --os_version 15.0.0 --os_patch_level 2024-09 --header_version 1

include lk2nd/project/lk2nd.mk

DEBUG := 2

