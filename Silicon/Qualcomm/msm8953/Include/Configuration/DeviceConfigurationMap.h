#ifndef _DEVICE_CONFIGURATION_MAP_H_
#define _DEVICE_CONFIGURATION_MAP_H_

#define CONFIGURATION_NAME_MAX_LENGTH 64

typedef struct {
  CHAR8                        Name[CONFIGURATION_NAME_MAX_LENGTH];
  UINT64                       Value;
} CONFIGURATION_DESCRIPTOR_EX, *PCONFIGURATION_DESCRIPTOR_EX;

static CONFIGURATION_DESCRIPTOR_EX gDeviceConfigurationDescriptorEx[] = {
    {"EarlyInitCoreCnt", 1},
    {"EnableACPIFallback", 0x0},
    {"EnableDisplayImageFv", 0x0},
    {"EnableDisplayThread", 0},
    {"EnableMultiThreading", 0},
    {"EnableOEMSetupAppInRetail", 0x0},
    {"EnablePXE", 0x0},
    {"EnableSDHCSwitch", 0x1},
    {"EnableSecondaryUFSforHLOS", 0},
    {"EnableShell", 0x1},
    {"EnableVariablePolicyEngine", 7},
    {"MaxCoreCnt", 8},
    {"MaxLogFileSize", 0x800000},
    {"NumActiveCores", 8},
    {"NumCpus", 8},
    {"PwrBtnShutdownFlag", 0x0},
    {"SerialPortBufferSize", 0x8000},
    {"SharedIMEMBaseAddr", 0x08600000},
    /* Terminator */
    {"Terminator", 0xFFFFFFFF}};

#endif
