/** @file
This file implements early GIC initialization.
Based on EarlyQGic implementation in MSM8909Pkg / MSM8937 ports.
**/

#include "EarlyQGic.h"
#include <Library/DebugLib.h>

STATIC
UINT8
QgicGetCpuMask(VOID)
{
  UINT32 mask = 0, i;

  /* Fetch the CPU MASK from the SGI/PPI reg */
  for (i = 0; i < 32; i += 4) {
    mask = MmioRead32(GIC_DIST_TARGET + i);
    mask |= mask >> 16;
    mask |= mask >> 8;
    if (mask) {
      break;
    }
  }
  return (UINT8)mask;
}

/* Initialize distributor */
VOID QGicDistConfig(UINT32 NumIrq)
{
  UINT32 i;

  /* Set each interrupt line to use N-N software model
   * and edge sensitive, active high
   */
  for (i = 32; i < NumIrq; i += 16) {
    MmioWrite32(GIC_DIST_CONFIG + i * 4 / 16, 0xffffffff);
  }

  MmioWrite32(GIC_DIST_CONFIG + 4, 0xffffffff);

  /* Set priority of all interrupts */
  for (i = 0; i < NumIrq; i += 4) {
    MmioWrite32(GIC_DIST_PRI + i * 4 / 4, 0xa0a0a0a0);
  }

  /* Disabling interrupts */
  for (i = 0; i < NumIrq; i += 32) {
    MmioWrite32(GIC_DIST_ENABLE_CLEAR + i * 4 / 32, 0xffffffff);
  }

  MmioWrite32(GIC_DIST_ENABLE_SET, 0x0000ffff);
}

VOID QGicHardwareReset(VOID)
{
  UINT32 n;

  /* Disabling GIC */
  MmioWrite32(GIC_DIST_CTRL, 0);
  MmioWrite32(GIC_DIST_CGCR, 0);
  MmioWrite32(GIC_CPU_CTRL, 0);
  MmioWrite32(GIC_CPU_PRIMASK, 0);
  MmioWrite32(GIC_CPU_BINPOINT, 0);

  for (n = 0; n <= 11; n++) {
    MmioWrite32(GIC_DIST_REG(0x80 + 4 * n), 0);
    MmioWrite32(GIC_DIST_REG(0x180 + 4 * n), 0xFFFFFFFF);
    MmioWrite32(GIC_DIST_REG(0x280 + 4 * n), 0xFFFFFFFF);
  }

  for (n = 0; n <= 95; n++) {
    MmioWrite32(GIC_DIST_REG(0x400 + 4 * n), 0);
    MmioWrite32(GIC_DIST_REG(0x800 + 4 * n), 0);
  }

  for (n = 0; n <= 23; n++) {
    MmioWrite32(GIC_DIST_REG(0xc00 + 4 * n), 0);
  }
}

VOID QGicSetBinpoint(VOID)
{
  /* No PREEMPT */
  MmioWrite32(GIC_CPU_BINPOINT, 7);
}

VOID QGicDistInit(VOID)
{
  UINT32 i;
  UINT32 num_irq = 0;
  UINT32 cpumask;

  cpumask = QgicGetCpuMask();
  cpumask |= cpumask << 8;
  cpumask |= cpumask << 16;

  /* Find out how many interrupts are supported. */
  num_irq = MmioRead32(GIC_DIST_CTR) & 0x1f;
  num_irq = (num_irq + 1) * 32;

  /* Set up interrupts for this CPU */
  for (i = 32; i < num_irq; i += 4) {
    MmioWrite32(GIC_DIST_TARGET + i * 4 / 4, cpumask);
  }

  QGicDistConfig(num_irq);

  /* Enabling GIC */
  MmioWrite32(GIC_DIST_CTRL, 1);
}

VOID QGicCpuInit(VOID)
{
  MmioWrite32(GIC_CPU_PRIMASK, 0xfe);
  MmioWrite32(GIC_CPU_CTRL, 1);
}

EFI_STATUS
EFIAPI
QGicPeim(VOID)
{
  DEBUG((EFI_D_INFO, "QGicPeim: Resetting and initializing GIC-400\n"));
  QGicHardwareReset();
  QGicSetBinpoint();
  QGicDistInit();
  QGicCpuInit();
  DEBUG((EFI_D_INFO, "QGicPeim: GIC initialized\n"));

  return EFI_SUCCESS;
}
