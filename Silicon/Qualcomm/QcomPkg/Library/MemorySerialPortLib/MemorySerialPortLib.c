/** @file
  Memory Serial Port Library implementation.

  Logs all debug strings to a fixed, safe RAM address (0x9FFF7000) using a
  ring buffer with magic header. Automatically pets the APCS hardware watchdog
  on each write to keep the board alive.
**/

#include <Base.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/IoLib.h>
#include <Library/SerialPortLib.h>

#define MEMORY_LOG_MAGIC     0x52414D4CUL  /* "RAML" */
#define MEMORY_LOG_BASE      0x9FFF7000ULL
#define MEMORY_LOG_SIZE      0x00008000ULL /* 32 KiB */

#define APCS_WDT0_RST_REG    0x0B017004ULL

#pragma pack(push, 1)
typedef struct {
  UINT32 Magic;        /* 0x52414D4C ("RAML") */
  UINT32 HeaderSize;   /* sizeof(MEMORY_LOG_HEADER) */
  UINT32 BufferSize;   /* Usable log buffer size */
  UINT32 WriteOffset;  /* Monotonic / ring buffer write pointer */
  UINT32 WrapCount;    /* Number of ring buffer wraps */
  UINT32 Reserved[3];
  CHAR8  LogData[];    /* ASCII log data stream */
} MEMORY_LOG_HEADER;
#pragma pack(pop)

RETURN_STATUS
EFIAPI
SerialPortInitialize (
  VOID
  )
{
  MEMORY_LOG_HEADER *Header = (MEMORY_LOG_HEADER *)(UINTN)MEMORY_LOG_BASE;

  if (Header->Magic != MEMORY_LOG_MAGIC ||
      Header->BufferSize != (UINT32)(MEMORY_LOG_SIZE - sizeof(MEMORY_LOG_HEADER))) {
    Header->Magic       = MEMORY_LOG_MAGIC;
    Header->HeaderSize  = sizeof(MEMORY_LOG_HEADER);
    Header->BufferSize  = (UINT32)(MEMORY_LOG_SIZE - sizeof(MEMORY_LOG_HEADER));
    Header->WriteOffset = 0;
    Header->WrapCount   = 0;
    WriteBackInvalidateDataCacheRange ((VOID *)Header, sizeof(MEMORY_LOG_HEADER));
  }

  return RETURN_SUCCESS;
}

UINTN
EFIAPI
SerialPortWrite (
  IN UINT8 *Buffer,
  IN UINTN NumberOfBytes
  )
{
  MEMORY_LOG_HEADER *Header;
  UINT32 Offset;
  UINT32 Capacity;
  UINTN  Index;

  if (Buffer == NULL || NumberOfBytes == 0) {
    return 0;
  }

  /* Keep the hardware watchdog alive */
  *(volatile UINT32 *)(UINTN)APCS_WDT0_RST_REG = 1;

  Header = (MEMORY_LOG_HEADER *)(UINTN)MEMORY_LOG_BASE;
  if (Header->Magic != MEMORY_LOG_MAGIC) {
    SerialPortInitialize ();
  }

  Capacity = Header->BufferSize;
  if (Capacity == 0) {
    return NumberOfBytes;
  }

  Offset = Header->WriteOffset;
  for (Index = 0; Index < NumberOfBytes; Index++) {
    Header->LogData[Offset] = (CHAR8)Buffer[Index];
    Offset++;
    if (Offset >= Capacity) {
      Offset = 0;
      Header->WrapCount++;
    }
  }
  Header->WriteOffset = Offset;

  /* Cache-flush the updated log block so it persists across hard resets */
  WriteBackInvalidateDataCacheRange ((VOID *)Header, sizeof(MEMORY_LOG_HEADER) + Offset);

  return NumberOfBytes;
}

UINTN
EFIAPI
SerialPortRead (
  OUT UINT8 *Buffer,
  IN  UINTN NumberOfBytes
  )
{
  return 0;
}

BOOLEAN
EFIAPI
SerialPortPoll (
  VOID
  )
{
  return FALSE;
}

RETURN_STATUS
EFIAPI
SerialPortSetControl (
  IN UINT32 Control
  )
{
  return RETURN_SUCCESS;
}

RETURN_STATUS
EFIAPI
SerialPortGetControl (
  OUT UINT32 *Control
  )
{
  if (Control == NULL) {
    return RETURN_INVALID_PARAMETER;
  }
  *Control = 0;
  return RETURN_SUCCESS;
}

RETURN_STATUS
EFIAPI
SerialPortSetAttributes (
  IN OUT UINT64             *BaudRate,
  IN OUT UINT32             *ReceiveFifoDepth,
  IN OUT UINT32             *Timeout,
  IN OUT EFI_PARITY_TYPE    *Parity,
  IN OUT UINT8              *DataBits,
  IN OUT EFI_STOP_BITS_TYPE *StopBits
  )
{
  return RETURN_SUCCESS;
}
