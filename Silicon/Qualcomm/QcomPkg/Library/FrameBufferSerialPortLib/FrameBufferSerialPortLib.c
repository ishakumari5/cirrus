#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/HobLib.h>
#include <Library/SerialPortLib.h>

#include <Resources/FbColor.h>
#include <Resources/font5x12.h>

#include "Library/FrameBufferSerialPortLib.h"
#include <Library/TimerLib.h>


FBCON_POSITION m_Position;
FBCON_POSITION m_MaxPosition;
FBCON_COLOR    m_Color;
BOOLEAN        m_Initialized = FALSE;

UINTN gWidth = FixedPcdGet32(PcdMipiFrameBufferWidth);
// Reserve half screen for output
UINTN gHeight = FixedPcdGet32(PcdMipiFrameBufferHeight);
UINTN gBpp    = FixedPcdGet32(PcdMipiFrameBufferPixelBpp);
UINTN delay = FixedPcdGet32(PcdMipiFrameBufferDelay);

#define PERSISTENT_RAM_SIG   0x43474244U  /* "DBGC" - Linux pstore persistent RAM signature */

#pragma pack(push, 1)
typedef struct {
  UINT32 Sig;
  UINT32 Offt;
  UINT32 Size;
  CHAR8  Data[];
} PRAM_BUF;
#pragma pack(pop)

STATIC VOID AppendToPramBuffer(UINTN BaseAddr, UINT32 TotalSize, IN CONST UINT8 *Buffer, UINTN NumberOfBytes)
{
  if (BaseAddr == 0 || TotalSize <= sizeof(PRAM_BUF)) {
    return;
  }

  volatile PRAM_BUF *pPram = (volatile PRAM_BUF *)(UINTN)BaseAddr;
  UINT32 Capacity = TotalSize - (UINT32)sizeof(PRAM_BUF);

  if (pPram->Sig != PERSISTENT_RAM_SIG || pPram->Size != Capacity) {
    pPram->Sig  = PERSISTENT_RAM_SIG;
    pPram->Offt = 0;
    pPram->Size = Capacity;
  }

  UINT32 Offt = pPram->Offt;
  if (Offt >= Capacity) {
    Offt = 0;
  }

  for (UINTN i = 0; i < NumberOfBytes; i++) {
    pPram->Data[Offt++] = (CHAR8)Buffer[i];
    if (Offt >= Capacity) {
      Offt = 0;
    }
  }

  pPram->Offt = Offt;
  WriteBackDataCacheRange((VOID *)(UINTN)BaseAddr, sizeof(PRAM_BUF));
}

STATIC VOID LogToMemory(IN CONST UINT8 *Buffer, IN UINTN NumberOfBytes)
{
  // 1. lk2nd fastboot ramoops console (0xBFEC0000, 256 KiB)
  AppendToPramBuffer(0xBFEC0000, 0x00040000, Buffer, NumberOfBytes);

  // 2. Xiaomi onclite / MSM8953 Stock Ramoops console (0x91400000, 2 MiB)
  AppendToPramBuffer(0x91400000, 0x00200000, Buffer, NumberOfBytes);

  // 3. UEFI dedicated persistent log buffer (0x9FFF7000, 32 KiB)
  AppendToPramBuffer(0x9FFF7000, 0x00008000, Buffer, NumberOfBytes);
}

STATIC inline VOID PetAndDisableWatchdog(VOID)
{
  *(volatile UINT32 *)(UINTN)0x0B017008 = 0;          // WDT0_EN = 0 (DISABLE)
  *(volatile UINT32 *)(UINTN)0x0B017004 = 1;          // WDT0_RST = 1 (PET/RESET)
  *(volatile UINT32 *)(UINTN)0x0B017010 = 0x0FFFFFFF; // Max bark ticks
  *(volatile UINT32 *)(UINTN)0x0B017014 = 0x0FFFFFFF; // Max bite ticks
  *(volatile UINT32 *)(UINTN)0x0B017008 = 0;          // Keep WDT0_EN = 0 (DISABLE)
}

// Module-used internal routine
void FbConPutCharWithFactor(char c, int type, unsigned scale_factor);

void FbConDrawglyph(
    char *pixels, unsigned stride, unsigned bpp, unsigned *glyph,
    unsigned scale_factor);

void FbConReset(void);
void FbConScrollUp(void);
void FbConFlush(void);

RETURN_STATUS
EFIAPI
SerialPortInitialize(VOID)
{
  UINTN InterruptState = 0;

  // Prevent dup initialization
  if (m_Initialized)
    return RETURN_SUCCESS;

  // Interrupt Disable
  ArmDisableInterrupts();

  // Reset console
  FbConReset();

  // Set flag
  m_Initialized = TRUE;

  return RETURN_SUCCESS;
}

void ResetFb(void)
{
  // Clear current screen.
  char *Pixels  = (void *)FixedPcdGet32(PcdMipiFrameBufferAddress);
  UINTN BgColor = FB_BGRA8888_BLACK;

  // Set to black color.
  for (UINTN i = 0; i < gWidth; i++) {
    for (UINTN j = 0; j < gHeight; j++) {
      BgColor = FB_BGRA8888_BLACK;
      // Set pixel bit
      for (UINTN p = 0; p < (gBpp / 8); p++) {
        *Pixels = (unsigned char)BgColor;
        BgColor = BgColor >> 8;
        Pixels++;
      }
    }
  }
}

STATIC inline INTN* GetSharedPosition(VOID)
{
  return (INTN*)(FixedPcdGet32(PcdMipiFrameBufferAddress) + 
                 (FixedPcdGet32(PcdMipiFrameBufferWidth) * 
                  FixedPcdGet32(PcdMipiFrameBufferHeight) * 
                  FixedPcdGet32(PcdMipiFrameBufferPixelBpp) / 8));
}

void FbConReset(void)
{
  INTN *pShared = GetSharedPosition();

  // Calc max position.
  m_MaxPosition.x = gWidth / (FONT_WIDTH + 1);
  m_MaxPosition.y = (gHeight - 1) / FONT_HEIGHT;

  // If shared cursor position exists from previous stage, continue seamlessly
  if (pShared[1] > 0 && pShared[1] < (INTN)m_MaxPosition.y) {
    m_Position.x = pShared[0];
    m_Position.y = pShared[1];
  } else {
    m_Position.x = 0;
    m_Position.y = 0;
    pShared[0] = 0;
    pShared[1] = 0;
  }

  // Reset color.
  m_Color.Foreground = FB_BGRA8888_WHITE;
  m_Color.Background = FB_BGRA8888_BLACK;
}

void FbConPutCharWithFactor(char c, int type, unsigned scale_factor)
{
  char *Pixels;

  if (!m_Initialized)
    SerialPortInitialize();

  if ((unsigned char)c > 127)
    return;

  if ((unsigned char)c < 32) {
    if (c == '\n') {
      goto newline;
    }
    else if (c == '\r') {
      m_Position.x = 0;
      return;
    }
    else {
      return;
    }
  }

  // Save some space
  if (m_Position.x == 0 && (unsigned char)c == ' ' &&
      type != FBCON_SUBTITLE_MSG && type != FBCON_TITLE_MSG)
    return;

  BOOLEAN intstate = ArmGetInterruptState();
  ArmDisableInterrupts();

  Pixels = (void *)FixedPcdGet32(PcdMipiFrameBufferAddress);
  Pixels += m_Position.y * ((gBpp / 8) * FONT_HEIGHT * gWidth);
  Pixels += m_Position.x * scale_factor * ((gBpp / 8) * (FONT_WIDTH + 1));

  FbConDrawglyph(
      Pixels, gWidth, (gBpp / 8), font5x12 + (c - 32) * 2, scale_factor);

  m_Position.x++;

  {
    INTN *pShared = GetSharedPosition();
    pShared[0] = m_Position.x;
    pShared[1] = m_Position.y;
  }

  if (m_Position.x >= (int)(m_MaxPosition.x / scale_factor))
    goto newline;

  return;

newline:
  PetAndDisableWatchdog();

  m_Position.y += scale_factor;
  m_Position.x = 0;
  {
    INTN *pShared = GetSharedPosition();
    pShared[0] = m_Position.x;
    pShared[1] = m_Position.y;
  }
  if (m_Position.y >= m_MaxPosition.y - scale_factor) {
    m_Position.x = 0;
    m_Position.y = 0;
    {
      INTN *pShared = GetSharedPosition();
      pShared[0] = 0;
      pShared[1] = 0;
    }
    ResetFb();
    FbConFlush();
    return;
  }
  else {
    Pixels = (void *)FixedPcdGet32(PcdMipiFrameBufferAddress);
    Pixels += m_Position.y * ((gBpp / 8) * FONT_HEIGHT * gWidth);
    ZeroMem(Pixels, ((gBpp / 8) * FONT_HEIGHT * gWidth) * scale_factor);
    WriteBackInvalidateDataCacheRange(Pixels, ((gBpp / 8) * FONT_HEIGHT * gWidth) * scale_factor);
  }
}

void FbConDrawglyph(
    char *pixels, unsigned stride, unsigned bpp, unsigned *glyph,
    unsigned scale_factor)
{
  char *       bg_pixels = pixels;
  unsigned     x, y, i, j, k;
  unsigned     data, temp;
  unsigned int fg_color = m_Color.Foreground;
  unsigned int bg_color = m_Color.Background;
  stride -= FONT_WIDTH * scale_factor;

  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    for (i = 0; i < scale_factor; i++) {
      for (x = 0; x < FONT_WIDTH; ++x) {
        for (j = 0; j < scale_factor; j++) {
          bg_color = m_Color.Background;
          for (k = 0; k < bpp; k++) {
            *bg_pixels = (unsigned char)bg_color;
            bg_color   = bg_color >> 8;
            bg_pixels++;
          }
        }
      }
      bg_pixels += (stride * bpp);
    }
  }

  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    for (i = 0; i < scale_factor; i++) {
      for (x = 0; x < FONT_WIDTH; ++x) {
        for (j = 0; j < scale_factor; j++) {
          bg_color = m_Color.Background;
          for (k = 0; k < bpp; k++) {
            *bg_pixels = (unsigned char)bg_color;
            bg_color   = bg_color >> 8;
            bg_pixels++;
          }
        }
      }
      bg_pixels += (stride * bpp);
    }
  }

  data = glyph[0];
  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    temp = data;
    for (i = 0; i < scale_factor; i++) {
      data = temp;
      for (x = 0; x < FONT_WIDTH; ++x) {
        if (data & 1) {
          for (j = 0; j < scale_factor; j++) {
            fg_color = m_Color.Foreground;
            for (k = 0; k < bpp; k++) {
              *pixels  = (unsigned char)fg_color;
              fg_color = fg_color >> 8;
              pixels++;
            }
          }
        }
        else {
          for (j = 0; j < scale_factor; j++) {
            pixels = pixels + bpp;
          }
        }
        data >>= 1;
      }
      pixels += (stride * bpp);
    }
  }

  data = glyph[1];
  for (y = 0; y < FONT_HEIGHT / 2; ++y) {
    temp = data;
    for (i = 0; i < scale_factor; i++) {
      data = temp;
      for (x = 0; x < FONT_WIDTH; ++x) {
        if (data & 1) {
          for (j = 0; j < scale_factor; j++) {
            fg_color = m_Color.Foreground;
            for (k = 0; k < bpp; k++) {
              *pixels  = (unsigned char)fg_color;
              fg_color = fg_color >> 8;
              pixels++;
            }
          }
        }
        else {
          for (j = 0; j < scale_factor; j++) {
            pixels = pixels + bpp;
          }
        }
        data >>= 1;
      }
      pixels += (stride * bpp);
    }
  }
}

/* TODO: Take stride into account */
void FbConScrollUp(void)
{
  unsigned short *dst   = (void *)FixedPcdGet32(PcdMipiFrameBufferAddress);
  unsigned short *src   = dst + (gWidth * FONT_HEIGHT);
  unsigned        count = gWidth * (gHeight - FONT_HEIGHT);

  while (count--) {
    *dst++ = *src++;
  }

  count = gWidth * FONT_HEIGHT;
  while (count--) {
    *dst++ = m_Color.Background;
  }

  FbConFlush();
}

void FbConFlush(void)
{
  unsigned total_x, total_y;
  unsigned bytes_per_bpp;

  total_x       = gWidth;
  total_y       = gHeight;
  bytes_per_bpp = (gBpp / 8);

  WriteBackInvalidateDataCacheRange(
      (void *)FixedPcdGet32(PcdMipiFrameBufferAddress),
      (total_x * total_y * bytes_per_bpp));
}

UINTN
EFIAPI
SerialPortWrite(IN UINT8 *Buffer, IN UINTN NumberOfBytes)
{
  UINT8 *CONST Final          = &Buffer[NumberOfBytes];
  UINTN        InterruptState;

  if (!m_Initialized)
    SerialPortInitialize();

  // Continuously pet APCS watchdog (0x0B017004) and keep disabled (0x0B017008)
  PetAndDisableWatchdog();

  // Always mirror logs to RAMoops buffer at 0x9FFF7000
  LogToMemory(Buffer, NumberOfBytes);

  InterruptState = ArmGetInterruptState();
  ArmDisableInterrupts();

  INTN start_y = m_Position.y;
  while (Buffer < Final) {
    FbConPutCharWithFactor(*Buffer++, FBCON_COMMON_MSG, SCALE_FACTOR);
  }

  INTN end_y = m_Position.y;
  if (start_y <= end_y) {
    char *p = (void *)FixedPcdGet32(PcdMipiFrameBufferAddress);
    p += start_y * ((gBpp / 8) * FONT_HEIGHT * gWidth);
    UINTN flush_size = (end_y - start_y + SCALE_FACTOR) * ((gBpp / 8) * FONT_HEIGHT * gWidth);
    WriteBackInvalidateDataCacheRange(p, flush_size);
  } else {
    FbConFlush();
  }

  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortWriteCritical(IN UINT8 *Buffer, IN UINTN NumberOfBytes)
{
  UINT8 *CONST Final             = &Buffer[NumberOfBytes];
  UINTN        CurrentForeground = m_Color.Foreground;
  UINTN        InterruptState    = ArmGetInterruptState();

  ArmDisableInterrupts();
  m_Color.Foreground = FB_BGRA8888_YELLOW;

  LogToMemory(Buffer, NumberOfBytes);

  INTN start_y = m_Position.y;
  while (Buffer < Final) {
    FbConPutCharWithFactor(*Buffer++, FBCON_COMMON_MSG, SCALE_FACTOR);
  }

  INTN end_y = m_Position.y;
  if (start_y <= end_y) {
    char *p = (void *)FixedPcdGet32(PcdMipiFrameBufferAddress);
    p += start_y * ((gBpp / 8) * FONT_HEIGHT * gWidth);
    UINTN flush_size = (end_y - start_y + SCALE_FACTOR) * ((gBpp / 8) * FONT_HEIGHT * gWidth);
    WriteBackInvalidateDataCacheRange(p, flush_size);
  } else {
    FbConFlush();
  }

  m_Color.Foreground = CurrentForeground;

  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortRead(OUT UINT8 *Buffer, IN UINTN NumberOfBytes) { return 0; }

BOOLEAN
EFIAPI
SerialPortPoll(VOID) { return FALSE; }

RETURN_STATUS
EFIAPI
SerialPortSetControl(IN UINT32 Control) { return RETURN_UNSUPPORTED; }

RETURN_STATUS
EFIAPI
SerialPortGetControl(OUT UINT32 *Control) { return RETURN_UNSUPPORTED; }

RETURN_STATUS
EFIAPI
SerialPortSetAttributes(
    IN OUT UINT64 *BaudRate, IN OUT UINT32 *ReceiveFifoDepth,
    IN OUT UINT32 *Timeout, IN OUT EFI_PARITY_TYPE *Parity,
    IN OUT UINT8 *DataBits, IN OUT EFI_STOP_BITS_TYPE *StopBits)
{
  return RETURN_UNSUPPORTED;
}

UINTN SerialPortFlush(VOID) { return 0; }

VOID EnableSynchronousSerialPortIO(VOID)
{
  // Already synchronous
}
