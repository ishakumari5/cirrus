[Defines]
  VENDOR_NAME                    = Xiaomi
  PLATFORM_NAME                  = onclite
  PLATFORM_GUID                  = 28f1a3bf-193a-47e3-a7b9-5a435eaab25d
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010019
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/Qualcomm/msm8953/msm8953.fdf
  DEVICE_DXE_FV_COMPONENTS       = Platform/Xiaomi/msm8953/onclite.fdf.inc

!include Platform/Qualcomm/msm8953/msm8953.dsc

[BuildOptions.common]
  GCC:*_*_AARCH64_CC_FLAGS = -DENABLE_SIMPLE_INIT

[PcdsFixedAtBuild.common]
  gEfiMdeModulePkgTokenSpaceGuid.PcdImageProtectionPolicy|0x00000000
  gEfiMdeModulePkgTokenSpaceGuid.PcdDxeNxMemoryProtectionPolicy|0x00000000

  gQcomTokenSpaceGuid.PcdMipiFrameBufferWidth|720
  gQcomTokenSpaceGuid.PcdMipiFrameBufferHeight|1520

  # Simple Init
  gSimpleInitTokenSpaceGuid.PcdGuiDefaultDPI|269

  gRenegadePkgTokenSpaceGuid.PcdDeviceVendor|"Xiaomi"
  gRenegadePkgTokenSpaceGuid.PcdDeviceProduct|"Redmi 7"
  gRenegadePkgTokenSpaceGuid.PcdDeviceCodeName|"onclite"
