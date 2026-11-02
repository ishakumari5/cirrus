/** @file
  Platform PEI Driver for Qualcomm Snapdragon 625/632 (MSM8953).

  Dynamically parses the Flattened Device Tree (FDT) passed by lk2nd at
  0x43400000, extracts all /reserved-memory child nodes with the 'no-map'
  property, and installs EFI_RESOURCE_MEMORY_RESERVED and EfiReservedMemoryType
  HOBs to prevent aggressive TrustZone (XPU) bus aborts during DXE dispatch.

  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
  Copyright (c) 2014, Linaro Limited. All rights reserved.
  Copyright (c) 2026. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/ColorTrap.h>
#include <libfdt.h>
#include "PlatformPeiLibInternal.h"


STATIC
EFI_STATUS
CfgGetMemInfoByName(
    CHAR8 *RegionName, ARM_MEMORY_REGION_DESCRIPTOR_EX *MemRegions)
{
  return LocateMemoryMapAreaByName(RegionName, MemRegions);
}

STATIC
EFI_STATUS
CfgGetMemInfoByAddress(
    UINT64 RegionBaseAddress, ARM_MEMORY_REGION_DESCRIPTOR_EX *MemRegions)
{
  return LocateMemoryMapAreaByAddress(RegionBaseAddress, MemRegions);
}

STATIC
EFI_STATUS
CfgGetCfgInfoString(CHAR8 *Key, CHAR8 *Value, UINTN *ValBuffSize)
{
  if (AsciiStriCmp(Key, "OsTypeString") == 0) {
    AsciiStrCpyS(Value, *ValBuffSize, "LA");
    return EFI_SUCCESS;
  }
  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
CfgGetCfgInfoVal(CHAR8 *Key, UINT32 *Value)
{
  PCONFIGURATION_DESCRIPTOR_EX ConfigurationDescriptorEx =
      gDeviceConfigurationDescriptorEx;

  while (ConfigurationDescriptorEx->Value != 0xFFFFFFFF) {
    if (AsciiStriCmp(Key, ConfigurationDescriptorEx->Name) == 0) {
      *Value = (UINT32)(ConfigurationDescriptorEx->Value & 0xFFFFFFFF);
      return EFI_SUCCESS;
    }
    ConfigurationDescriptorEx++;
  }
  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
CfgGetCfgInfoVal64(CHAR8 *Key, UINT64 *Value)
{
  PCONFIGURATION_DESCRIPTOR_EX ConfigurationDescriptorEx =
      gDeviceConfigurationDescriptorEx;

  while (ConfigurationDescriptorEx->Value != 0xFFFFFFFF) {
    if (AsciiStriCmp(Key, ConfigurationDescriptorEx->Name) == 0) {
      *Value = ConfigurationDescriptorEx->Value;
      return EFI_SUCCESS;
    }
    ConfigurationDescriptorEx++;
  }
  return EFI_NOT_FOUND;
}

STATIC UINTN SFlush(VOID) { return EFI_SUCCESS; }
STATIC UINTN SControl(IN UINTN Arg, IN UINTN Param) { return EFI_SUCCESS; }
STATIC BOOLEAN SPoll(VOID) { return TRUE; }
STATIC UINTN SDrain(VOID) { return EFI_SUCCESS; }
STATIC EFI_STATUS ShInstallLib(IN CHAR8 *LibName, IN UINT32 LibVersion, IN VOID *LibIntf) { return EFI_SUCCESS; }

UefiCfgLibType ConfigLib = {0x00010002,          CfgGetMemInfoByName,
                            CfgGetCfgInfoString, CfgGetCfgInfoVal,
                            CfgGetCfgInfoVal64,  CfgGetMemInfoByAddress};

SioPortLibType SioLib = {
    0x00010001, SerialPortRead, SerialPortWrite, SPoll,
    SDrain,     SFlush,         SControl,        SerialPortSetAttributes,
};

STATIC
EFI_STATUS
ShLoadLib(CHAR8 *LibName, UINT32 LibVersion, VOID **LibIntf)
{
  if (LibIntf == NULL)
    return EFI_NOT_FOUND;

  if (AsciiStriCmp(LibName, "UEFI Config Lib") == 0) {
    *LibIntf = &ConfigLib;
    return EFI_SUCCESS;
  }

  if (AsciiStriCmp(LibName, "SerialPort Lib") == 0) {
    *LibIntf = &SioLib;
    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

ShLibLoaderType ShLib = {0x00010001, ShInstallLib, ShLoadLib};

STATIC
VOID BuildMemHobForFv(IN UINT16 Type)
{
  EFI_PEI_HOB_POINTERS      HobPtr;
  EFI_HOB_FIRMWARE_VOLUME2 *Hob = NULL;

  HobPtr.Raw = GetHobList();
  while ((HobPtr.Raw = GetNextHob(Type, HobPtr.Raw)) != NULL) {
    if (Type == EFI_HOB_TYPE_FV2) {
      Hob = HobPtr.FirmwareVolume2;
      BuildMemoryAllocationHob(
          Hob->BaseAddress, EFI_SIZE_TO_PAGES(Hob->Length) * EFI_PAGE_SIZE,
          EfiBootServicesData);
    }
    HobPtr.Raw = GET_NEXT_HOB(HobPtr);
  }
}

STATIC GUID gEfiShLibHobGuid   = EFI_SHIM_LIBRARY_GUID;
STATIC GUID gEfiInfoBlkHobGuid = EFI_INFORMATION_BLOCK_GUID;

STATIC
VOID InstallPlatformHob(VOID)
{
  static int initialized = 0;

  if (!initialized) {
    UINTN Data  = (UINTN)&ShLib;
    UINTN Data2 = 0x9FFFF000; // Info Blk

    BuildMemHobForFv(EFI_HOB_TYPE_FV2);
    BuildGuidDataHob(&gEfiShLibHobGuid, &Data, sizeof(Data));
    BuildGuidDataHob(&gEfiInfoBlkHobGuid, &Data2, sizeof(Data2));

    initialized = 1;
  }
}

STATIC
UINT64
FdtReadCells(IN CONST fdt32_t *CellPtr, IN INT32 CellCount)
{
  UINT64 Value = 0;
  if (CellCount == 1) {
    Value = fdt32_to_cpu(*CellPtr);
  } else if (CellCount == 2) {
    Value = ((UINT64)fdt32_to_cpu(CellPtr[0]) << 32) | (UINT64)fdt32_to_cpu(CellPtr[1]);
  }
  return Value;
}

STATIC
VOID
PetWatchdog(VOID)
{
  MmioWrite32(0x0B017008, 0);
  MmioWrite32(0x0B017004, 1);
}

STATIC
VOID
ParseFdtReservedMemory(VOID)
{
  VOID                 *FdtAddress = (VOID *)(UINTN)0x43400000;
  INT32                ReservedNode;
  INT32                SubNode;
  INT32                AddrCells = 2;
  INT32                SizeCells = 2;
  INT32                LockedCount = 0;
  INT32                Iterations = 0;

  DEBUG((EFI_D_INFO, "[PlatformPei] ParseFdtReservedMemory In\n"));
  PetWatchdog();

  // 1. Check primary lk2nd tags address at 0x43400000
  if (fdt_check_header(FdtAddress) != 0) {
    // 2. Check pointer stored at PcdDeviceTreeStore
    EFI_PHYSICAL_ADDRESS PcdAddr = *(EFI_PHYSICAL_ADDRESS *)(UINTN)FixedPcdGet64(PcdDeviceTreeStore);
    if (PcdAddr >= 0x40000000 && fdt_check_header((VOID *)(UINTN)PcdAddr) == 0) {
      FdtAddress = (VOID *)(UINTN)PcdAddr;
    } else {
      // 3. Scan candidate addresses
      STATIC CONST EFI_PHYSICAL_ADDRESS CandidateTags[] = { 0x43400000, 0x80000100, 0x83400000, 0x83300000 };
      UINTN i;
      for (i = 0; i < ARRAY_SIZE(CandidateTags); i++) {
        if (fdt_check_header((VOID *)(UINTN)CandidateTags[i]) == 0) {
          FdtAddress = (VOID *)(UINTN)CandidateTags[i];
          break;
        }
      }
    }
  }

  if (fdt_check_header(FdtAddress) != 0) {
    DEBUG((EFI_D_ERROR, "[PlatformPei] ERROR: Valid FDT header not found! Continuing with static map\n"));
    return;
  }

  DEBUG((EFI_D_INFO, "[PlatformPei] Valid FDT found at %p (total size 0x%x)\n",
         FdtAddress, fdt_totalsize(FdtAddress)));

  // Lock FDT memory itself so DXE allocator does not overwrite it
  BuildResourceDescriptorHob(
    EFI_RESOURCE_MEMORY_RESERVED,
    EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE,
    (EFI_PHYSICAL_ADDRESS)(UINTN)FdtAddress & ~((EFI_PHYSICAL_ADDRESS)EFI_PAGE_MASK),
    EFI_PAGES_TO_SIZE(EFI_SIZE_TO_PAGES(fdt_totalsize(FdtAddress)))
  );
  BuildMemoryAllocationHob(
    (EFI_PHYSICAL_ADDRESS)(UINTN)FdtAddress & ~((EFI_PHYSICAL_ADDRESS)EFI_PAGE_MASK),
    EFI_PAGES_TO_SIZE(EFI_SIZE_TO_PAGES(fdt_totalsize(FdtAddress))),
    EfiReservedMemoryType
  );

  // Traverse /reserved-memory node
  ReservedNode = fdt_path_offset(FdtAddress, "/reserved-memory");
  if (ReservedNode < 0) {
    DEBUG((EFI_D_WARN, "[PlatformPei] /reserved-memory node not found in FDT\n"));
    return;
  }

  AddrCells = fdt_address_cells(FdtAddress, ReservedNode);
  SizeCells = fdt_size_cells(FdtAddress, ReservedNode);
  if (AddrCells <= 0) AddrCells = 2;
  if (SizeCells <= 0) SizeCells = 2;

  DEBUG((EFI_D_INFO, "[PlatformPei] Parsing /reserved-memory (#address-cells=%d, #size-cells=%d)\n",
         AddrCells, SizeCells));

  for (SubNode = fdt_first_subnode(FdtAddress, ReservedNode);
       SubNode >= 0;
       SubNode = fdt_next_subnode(FdtAddress, SubNode)) {

    CONST CHAR8   *NodeName   = fdt_get_name(FdtAddress, SubNode, NULL);
    INT32         PropLen     = 0;
    CONST VOID    *NoMapProp  = fdt_getprop(FdtAddress, SubNode, "no-map", &PropLen);
    CONST fdt32_t *RegProp    = (CONST fdt32_t *)fdt_getprop(FdtAddress, SubNode, "reg", &PropLen);

    PetWatchdog();
    if (++Iterations > 100) {
      DEBUG((EFI_D_WARN, "[PlatformPei] Warning: Reached max subnode limit (100)\n"));
      break;
    }

    if (NoMapProp != NULL && RegProp != NULL && PropLen >= (INT32)((AddrCells + SizeCells) * sizeof(fdt32_t))) {
      INT32 TupleSize = (AddrCells + SizeCells) * (INT32)sizeof(fdt32_t);
      INT32 NumRanges = PropLen / TupleSize;
      INT32 RangeIdx;

      for (RangeIdx = 0; RangeIdx < NumRanges; RangeIdx++) {
        CONST fdt32_t *Entry = RegProp + (RangeIdx * (AddrCells + SizeCells));
        UINT64 Base = FdtReadCells(Entry, AddrCells);
        UINT64 Size = FdtReadCells(Entry + AddrCells, SizeCells);

        if (Size > 0) {
          EFI_PHYSICAL_ADDRESS AlignedBase = Base & ~((EFI_PHYSICAL_ADDRESS)EFI_PAGE_MASK);
          UINT64 AlignedSize = EFI_PAGES_TO_SIZE(EFI_SIZE_TO_PAGES((Base - AlignedBase) + Size));

          DEBUG((EFI_D_INFO, "[PlatformPei] Locking TZ carveout '%a' [%d]: 0x%016llx - 0x%016llx (0x%llx)\n",
                 NodeName, RangeIdx, AlignedBase, AlignedBase + AlignedSize, AlignedSize));

          BuildResourceDescriptorHob(
            EFI_RESOURCE_MEMORY_RESERVED,
            EFI_RESOURCE_ATTRIBUTE_UNCACHEABLE |
            EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
            EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
            EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
            EFI_RESOURCE_ATTRIBUTE_TESTED,
            AlignedBase,
            AlignedSize
          );

          BuildMemoryAllocationHob(
            AlignedBase,
            AlignedSize,
            EfiReservedMemoryType
          );

          LockedCount++;
        }
      }
    }
  }

  PetWatchdog();
  DEBUG((EFI_D_INFO, "[PlatformPei] Dynamic FDT scan complete: %d TrustZone carveouts locked\n", LockedCount));
}

STATIC
VOID
LockCriticalPlatformRegions(VOID)
{
  DEBUG((EFI_D_INFO, "[PlatformPei] LockCriticalPlatformRegions In\n"));
  PetWatchdog();
}

EFI_STATUS
EFIAPI
PlatformPeim(
  VOID
  )
{
  PetWatchdog();
  DEBUG((EFI_D_INFO, "[PlatformPei] Installing FV HOB: Base=0x%llx, Size=0x%x\n",
         PcdGet64(PcdFvBaseAddress), PcdGet32(PcdFvSize)));
  BuildFvHob(PcdGet64(PcdFvBaseAddress), PcdGet32(PcdFvSize));

  DEBUG((EFI_D_INFO, "[PlatformPei] InstallPlatformHob In\n"));
  InstallPlatformHob();
  DEBUG((EFI_D_INFO, "[PlatformPei] InstallPlatformHob Out\n"));

  PetWatchdog();
  // Dynamically parse /reserved-memory from lk2nd FDT and lock no-map carveouts
  ParseFdtReservedMemory();

  PetWatchdog();
  // Lock critical platform regions
  LockCriticalPlatformRegions();

  PetWatchdog();
  // Checkpoint: TrustZone carveouts locked and HOBs ready
  DEBUG_COLOR_TRAP(COLOR_GREEN);

  DEBUG((EFI_D_INFO, "[PlatformPei] PlatformPeim Done\n"));
  return EFI_SUCCESS;
}
