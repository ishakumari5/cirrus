/** @file
  Zero-latency Color Trap diagnostic macro for Qualcomm MDP5 Framebuffer.

  Executes a direct, cache-flushed write to the continuous splash framebuffer
  scanout memory to trace execution milestones with zero rendering latency.
**/

#ifndef _COLOR_TRAP_H_
#define _COLOR_TRAP_H_

#include <Library/ArmLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/PcdLib.h>

#define COLOR_TRAP_ENABLED 1

// 32-bit XRGB8888 Color Definitions
#define COLOR_BLACK    0x00000000
#define COLOR_WHITE    0x00FFFFFF
#define COLOR_RED      0x00FF0000
#define COLOR_GREEN    0x0000FF00
#define COLOR_BLUE     0x000000FF
#define COLOR_YELLOW   0x00FFFF00
#define COLOR_MAGENTA  0x00FF00FF
#define COLOR_CYAN     0x0000FFFF
#define COLOR_ORANGE   0x00FFA500

#if COLOR_TRAP_ENABLED
#define DEBUG_COLOR_TRAP(Color) do { \
  volatile UINT32 *_FbPtr = (volatile UINT32 *)(UINTN)FixedPcdGet64(PcdMipiFrameBufferAddress); \
  if (_FbPtr != NULL) { \
    UINTN _Pixels = 720 * 60; /* 60 scanout lines visual band */ \
    for (UINTN _idx = 0; _idx < _Pixels; _idx++) { \
      _FbPtr[_idx] = (UINT32)(Color); \
    } \
    WriteBackInvalidateDataCacheRange((VOID *)_FbPtr, _Pixels * sizeof(UINT32)); \
  } \
} while (0)
#else
#define DEBUG_COLOR_TRAP(Color) ((VOID)0)
#endif

#endif // _COLOR_TRAP_H_
