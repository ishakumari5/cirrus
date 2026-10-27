## @file
#
#  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2016, Intel Corporation. All rights reserved.
#  Copyright (c) 2018 - 2019, Bingxing Wang. All rights reserved.
#  Copyright (c) 2022, Xilin Wu. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################

[Defines]
  SOC_PLATFORM            = MSM8953
  USE_PHYSICAL_TIMER      = FALSE

!include Silicon/Qualcomm/QcomPkg/QcomCommonDsc.inc

[PcdsFixedAtBuild.common]
  # Disable DXE Memory Protection to avoid MMU page table conflict with lk2nd
  gEfiMdeModulePkgTokenSpaceGuid.PcdImageProtectionPolicy|0x00000000
  gEfiMdeModulePkgTokenSpaceGuid.PcdDxeNxMemoryProtectionPolicy|0x00000000

  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000         # Starting address (MSM8953 true DDR base)
  gArmTokenSpaceGuid.PcdSystemMemorySize|0x7E000000         # Fit 2GB devices safely

  gArmTokenSpaceGuid.PcdCpuVectorBaseAddress|0x9ff8c000     # CPU Vectors
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|19200000
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|18
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|19
  gArmTokenSpaceGuid.PcdArmArchTimerVirtIntrNum|20
  gArmTokenSpaceGuid.PcdArmArchTimerHypIntrNum|17

  # GIC-400 (GICv2) on MSM8953
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0B000000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x0B002000

  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|0x00008953
  gEmbeddedTokenSpaceGuid.PcdPrePiStackBase|0x9FF90000      # UEFI Stack
  gEmbeddedTokenSpaceGuid.PcdPrePiStackSize|0x00040000      # 256K stack
  gEmbeddedTokenSpaceGuid.PcdPrePiCpuIoSize|44

  gQcomTokenSpaceGuid.PcdUefiMemPoolBase|0xA0000000         # DXE Heap base address
  gQcomTokenSpaceGuid.PcdUefiMemPoolSize|0x1E000000         # UefiMemorySize, DXE heap size (480MB)
  gQcomTokenSpaceGuid.PcdMipiFrameBufferAddress|0x90000000

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|2

  #
  # SimpleInit
  #
  gSimpleInitTokenSpaceGuid.PcdDeviceTreeStore|0x83300000
  gSimpleInitTokenSpaceGuid.PcdLoggerdUseConsole|FALSE

[LibraryClasses.common]
  # Ported from SurfaceDuoPkg
  AslUpdateLib|Silicon/Qualcomm/QcomPkg/Library/DxeAslUpdateLib/DxeAslUpdateLib.inf

  PlatformMemoryMapLib|Silicon/Qualcomm/msm8953/Library/PlatformMemoryMapLib/PlatformMemoryMapLib.inf
  PlatformPeiLib|Silicon/Qualcomm/msm8953/Library/PlatformPeiLib/PlatformPeiLib.inf
  PlatformPrePiLib|Silicon/Qualcomm/msm8953/Library/PlatformPrePiLib/PlatformPrePiLib.inf
  MsPlatformDevicesLib|Silicon/Qualcomm/msm8953/Library/MsPlatformDevicesLib/MsPlatformDevicesLib.inf
  SOCSmbiosInfoLib|Silicon/Qualcomm/msm8953/Library/SOCSmbiosInfoLib/SOCSmbiosInfoLib.inf

[Components.common]
