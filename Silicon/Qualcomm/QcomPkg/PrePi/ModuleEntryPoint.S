#include <Library/PcdLib.h>
#include <AsmMacroIoLibV8.h>
#include <Chipset/AArch64.h>

.text
.align 3

.set CTRL_SPAN_BIT, (1 << 23)

GCC_ASM_IMPORT (CEntryPoint)
GCC_ASM_IMPORT (SecondaryCEntryPoint)

GCC_ASM_IMPORT (ArmEnableInstructionCache)
GCC_ASM_IMPORT (ArmEnableDataCache)
GCC_ASM_IMPORT (ArmInvalidateTlb)

GCC_ASM_EXPORT (_ModuleEntryPoint)
GCC_ASM_EXPORT (SecondaryCpuEntry)

.global _StackBase
.global _StackSize

_StackBase:
  .quad FixedPcdGet32(PcdPrePiStackBase)

_StackSize:
  .quad FixedPcdGet32(PcdPrePiStackSize)

_ModuleEntryPoint:
  mov x10, FixedPcdGet64 (PcdDeviceTreeStore)
  str x0, [x10] // oh my fdt

  /* Neutralize APCS hardware watchdog immediately upon entering PrePi */
  movz  x7, #0x7000
  movk  x7, #0x0B01, lsl #16
  movz  x8, #0xFFFF
  movk  x8, #0x0FFF, lsl #16
  str   w8, [x7, #0x10]  /* WDT0_BARK_TIME = max ticks (0x0FFFFFFF) */
  str   w8, [x7, #0x14]  /* WDT0_BITE_TIME = max ticks (0x0FFFFFFF) */
  str   wzr, [x7, #0x08] /* WDT0_EN = 0 (DISABLE watchdog on KPSS / MSM8953) */
  movz  x8, #1
  str   w8, [x7, #0x04]  /* WDT0_RST = 1 (PET/RESET watchdog counter) */
  str   wzr, [x7, #0x08] /* Keep WDT0_EN = 0 (DISABLE) */

  /* Checkpoint 3: PrePi Entry (Paint 40 rows YELLOW at row 80) */
  movz  x7, #(2880 * 80) & 0xFFFF
  movk  x7, #(0x90000000 + (2880 * 80)) >> 16, lsl #16
  movz  x8, #0xFFFF
  orr   x8, x8, x8, lsl #32
  movz  x9, #(720 * 40 / 2)
.Lpaint_yellow:
  str   x8, [x7], #8
  subs  x9, x9, #1
  b.ne  .Lpaint_yellow

  movz  x7, #(0x1000 + (2880 * 80)) & 0xFFFF
  movk  x7, #(0x90000000 + 0x1000 + (2880 * 80)) >> 16, lsl #16
  movz  x9, #(720 * 40 / 2)
.Lpaint_yellow2:
  str   x8, [x7], #8
  subs  x9, x9, #1
  b.ne  .Lpaint_yellow2

  mov x0, #0
  mov x1, #0

  /* First ensure all interrupts are disabled */
  bl ASM_PFX(ArmDisableInterrupts)

  /* Ensure that the MMU and caches are off */
  bl ASM_PFX(ArmDisableCachesAndMmu)

  /* Invalide I-Cache */
  bl ASM_PFX(ArmInvalidateInstructionCache)

  /* Invalidate TLB */
  bl ASM_PFX(ArmInvalidateTlb)

  mrs x0, sctlr_el1
  bic x0, x0, #(1 << 1)   /* Clear bit 1 (A): Disable strict alignment fault checking */
  orr x0, x0, #CTRL_SPAN_BIT
  msr sctlr_el1, x0
  isb

_SetupExceptionVector:
  ldr x0, =FixedPcdGet64(PcdCpuVectorBaseAddress)
  bl ArmWriteVBar

  mrs     x4, mpidr_el1
  and     x4, x4, #0xFFFF
  lsr     x4, x4, #8
  cbnz    x4, SecondaryCpuEntry

  ldr x1, dead
  add x2, x0, #0x800
  mov x3, x0

_FillVectors:
  stp     x1, x1, [x3], #16 /* Fill every 16 byte */
  cmp     x3, x2
  b.lt    _FillVectors

_DonNotTrap_VFP_SIMD:
  mrs x0, CPACR_EL1
  /* Bit 20 and 21 */
  orr x0, x0, #0x300000
  msr CPACR_EL1, x0
  isb sy

_SetupPrimaryCoreStack:
  ldr x0, _StackBase     /* Stack base arg0 */
  ldr x1, _StackSize     /* Stack size arg1 */

  /* Zero Init stack safely with 64-bit integer stores */
  add x2, x0, x1         /* End of Stack */
  mov x3, x0             /* Stack Base */

  mov x4, xzr
  mov x5, xzr

_ClearStack:
  /* Assumes StackBase is 128-bit aligned, StackSize is a multiple of 16B */
  stp   x4, x5, [x3], #16                        /* Fill every 16 bytes */
  cmp   x3, x2                                   /* Compare Size */
  b.lt  _ClearStack

  add sp, x2, xzr                                /* Initalize SP */

_EnableCache:
  bl ArmInvalidateDataCache
  bl ASM_PFX(ArmEnableInstructionCache)
  bl ASM_PFX(ArmEnableDataCache)

_PrepareArguments:
  /* Checkpoint 4: Primary Stack Ready & Caches Enabled (Paint 40 rows MAGENTA at row 120) */
  movz  x7, #(2880 * 120) & 0xFFFF
  movk  x7, #(0x90000000 + (2880 * 120)) >> 16, lsl #16
  movz  x8, #0x00FF
  movk  x8, #0x00FF, lsl #16
  orr   x8, x8, x8, lsl #32
  movz  x9, #(720 * 40 / 2)
.Lpaint_magenta:
  str   x8, [x7], #8
  subs  x9, x9, #1
  b.ne  .Lpaint_magenta

  movz  x7, #(0x1000 + (2880 * 120)) & 0xFFFF
  movk  x7, #(0x90000000 + 0x1000 + (2880 * 120)) >> 16, lsl #16
  movz  x9, #(720 * 40 / 2)
.Lpaint_magenta2:
  str   x8, [x7], #8
  subs  x9, x9, #1
  b.ne  .Lpaint_magenta2

  /* x0 = _StackBase and x1 = _StackSize */
  ldr x0, _StackBase     /* Stack base arg0 */
  ldr x1, _StackSize     /* Stack size arg1 */

  bl CEntryPoint

.align 3
dead:
  b dead                      /* We should never get here */

SecondaryCpuEntry:
  /* Keep the incoming index */
  mov x19, x4

  mov x5, #0x8
  sub x4, x5, x4

  ubfiz x2, x4, #15, #8

_SetupSecondaryCoreStack:
  ldr x3, _StackBase     /* Stack base arg0 */

  add x2, x3, x2
  sub x3, x2, #0x8000    /* Stack size arg1 */

  mov x4, xzr
  mov x5, xzr

_ClearStackSecondary:
  /* Assumes StackBase is 128-bit aligned, StackSize is a multiple of 16B */
  stp     x4, x5, [x3], #16
  cmp     x3, x2                                   /* Compare Size */
  b.lt    _ClearStackSecondary

  add sp, x2, xzr                                  /* Initalize SP */

  bl ArmInvalidateDataCache

  /* Enter MpPark spin */
  mov x0, x19
  bl SecondaryCEntryPoint

secondarydead:
  wfe
  b secondarydead                      /* We should never get here */

/*
 * Diagnostic AArch64 Exception Vector Table for PrePi / early DXE.
 * Must be 2048-byte aligned (bits [10:0] == 0) as required by VBAR_EL1.
 */
.align 11
.global PrePiVectorTable
PrePiVectorTable:
  /* 0x000: Current EL with SP0 - Synchronous */
  mov x0, #0x000
  b ExceptionTrapHandler
  .align 7

  /* 0x080: Current EL with SP0 - IRQ */
  mov x0, #0x080
  b ExceptionTrapHandler
  .align 7

  /* 0x100: Current EL with SP0 - FIQ */
  mov x0, #0x100
  b ExceptionTrapHandler
  .align 7

  /* 0x180: Current EL with SP0 - SError */
  mov x0, #0x180
  b ExceptionTrapHandler
  .align 7

  /* 0x200: Current EL with SPx - Synchronous (Main EL1 Exception handler) */
  mov x0, #0x200
  b ExceptionTrapHandler
  .align 7

  /* 0x280: Current EL with SPx - IRQ */
  mov x0, #0x280
  b ExceptionTrapHandler
  .align 7

  /* 0x300: Current EL with SPx - FIQ */
  mov x0, #0x300
  b ExceptionTrapHandler
  .align 7

  /* 0x380: Current EL with SPx - SError */
  mov x0, #0x380
  b ExceptionTrapHandler
  .align 7

  /* 0x400: Lower EL using AArch64 - Synchronous */
  mov x0, #0x400
  b ExceptionTrapHandler
  .align 7

  /* 0x480: Lower EL using AArch64 - IRQ */
  mov x0, #0x480
  b ExceptionTrapHandler
  .align 7

  /* 0x500: Lower EL using AArch64 - FIQ */
  mov x0, #0x500
  b ExceptionTrapHandler
  .align 7

  /* 0x580: Lower EL using AArch64 - SError */
  mov x0, #0x580
  b ExceptionTrapHandler
  .align 7

  /* 0x600: Lower EL using AArch32 - Synchronous */
  mov x0, #0x600
  b ExceptionTrapHandler
  .align 7

  /* 0x680: Lower EL using AArch32 - IRQ */
  mov x0, #0x680
  b ExceptionTrapHandler
  .align 7

  /* 0x700: Lower EL using AArch32 - FIQ */
  mov x0, #0x700
  b ExceptionTrapHandler
  .align 7

  /* 0x780: Lower EL using AArch32 - SError */
  mov x0, #0x780
  b ExceptionTrapHandler
  .align 7

.global ExceptionTrapHandler
ExceptionTrapHandler:
  /* Pet and disable APCS hardware watchdog immediately */
  movz  x7, #0x7000
  movk  x7, #0x0B01, lsl #16
  movz  x8, #1
  str   w8, [x7, #0x04]  /* WDT0_RST = 1 (PET) */
  str   wzr, [x7, #0x08] /* WDT0_EN = 0 (DISABLE) */

  /* Read syndromes */
  mrs   x1, esr_el1
  mrs   x2, far_el1
  mrs   x3, elr_el1

  /* Save to dedicated crash info block at 0x9FFFF000 */
  movz  x7, #0xF000
  movk  x7, #0x9FFF, lsl #16
  str   x0, [x7, #0x00]  /* Vector offset */
  str   x1, [x7, #0x08]  /* ESR_EL1 */
  str   x2, [x7, #0x10]  /* FAR_EL1 */
  str   x3, [x7, #0x18]  /* ELR_EL1 */
  dc    cvac, x7
  dsb   sy

  /* Paint top 120 rows on framebuffer (0x90000000) */
  /* Red (0x000000FF) for Sync, Blue (0x00FF0000) for IRQ, Yellow (0x00FFFF00) for SError */
  movz  x7, #0x0000
  movk  x7, #0x9000, lsl #16
  movz  x8, #0x00FF      /* Red */
  cmp   x0, #0x280
  b.ne  1f
  movz  x8, #0x0000
  movk  x8, #0x00FF, lsl #16 /* Blue for IRQ */
1:
  cmp   x0, #0x380
  b.ne  2f
  movz  x8, #0xFF00
  movk  x8, #0x00FF, lsl #16 /* Yellow for SError */
2:
  orr   x8, x8, x8, lsl #32
  movz  x9, #(720 * 120 / 2)
.Lpaint_exc:
  str   x8, [x7], #8
  subs  x9, x9, #1
  b.ne  .Lpaint_exc

  /* Format human-readable message into scratch buffer at 0x9FFFF100 */
  movz  x10, #0xF100
  movk  x10, #0x9FFF, lsl #16
  mov   x11, x10

  adr   x12, .Lstr_exc_vec
  bl    .Lasm_strcpy
  mov   x13, x0
  mov   w14, #4
  bl    .Lasm_hex
  adr   x12, .Lstr_exc_esr
  bl    .Lasm_strcpy
  mov   x13, x1
  mov   w14, #16
  bl    .Lasm_hex
  adr   x12, .Lstr_exc_far
  bl    .Lasm_strcpy
  mov   x13, x2
  mov   w14, #16
  bl    .Lasm_hex
  adr   x12, .Lstr_exc_elr
  bl    .Lasm_strcpy
  mov   x13, x3
  mov   w14, #16
  bl    .Lasm_hex
  adr   x12, .Lstr_exc_end
  bl    .Lasm_strcpy

  /* String length = x11 - x10 */
  sub   x14, x11, x10

  /* Append to 0xBFEC0000 (lk2nd ramoops) */
  movz  x15, #0x0000
  movk  x15, #0xBFEC, lsl #16
  mov   x12, x10
  mov   x13, x14
  bl    .Lasm_append_pram

  /* Append to 0x91400000 (stock ramoops) */
  movz  x15, #0x0000
  movk  x15, #0x9140, lsl #16
  mov   x12, x10
  mov   x13, x14
  bl    .Lasm_append_pram

  /* Append to 0x9FFF7000 (uefi ramoops) */
  movz  x15, #0x7000
  movk  x15, #0x9FFF, lsl #16
  mov   x12, x10
  mov   x13, x14
  bl    .Lasm_append_pram

.Lexc_spin:
  movz  x7, #0x7000
  movk  x7, #0x0B01, lsl #16
  movz  x8, #1
  str   w8, [x7, #0x04]  /* WDT0_RST = 1 (PET) */
  str   wzr, [x7, #0x08] /* WDT0_EN = 0 (DISABLE) */
  movz  x9, #0xFFFF
.Lexc_delay:
  subs  x9, x9, #1
  b.ne  .Lexc_delay
  b     .Lexc_spin

.Lasm_strcpy:
  ldrb  w16, [x12], #1
  cbz   w16, 1f
  strb  w16, [x11], #1
  b     .Lasm_strcpy
1:
  ret

.Lasm_hex:
  sub   w14, w14, #1
  lsl   w14, w14, #2
.Lhex_inner:
  lsr   x16, x13, x14
  and   x16, x16, #0xF
  cmp   x16, #10
  add   x17, x16, #'0'
  add   x18, x16, #('A' - 10)
  csel  x16, x18, x17, ge
  strb  w16, [x11], #1
  subs  w14, w14, #4
  b.ge  .Lhex_inner
  ret

.Lasm_append_pram:
  ldr   w16, [x15]        /* Sig */
  movz  w17, #0x4244
  movk  w17, #0x4347, lsl #16  /* "DBGC" = 0x43474244 */
  cmp   w16, w17
  b.eq  1f
  str   w17, [x15]        /* set Sig */
  str   wzr, [x15, #4]    /* Offt = 0 */
  movz  w18, #0xFFF0
  movk  w18, #0x0003, lsl #16
  str   w18, [x15, #8]    /* Size = 256KB - 16 */
1:
  ldr   w16, [x15, #4]    /* Offt */
  ldr   w17, [x15, #8]    /* Capacity */
  cbz   w17, 3f
  mov   x18, x12
  mov   x19, x13
2:
  cbz   x19, 3f
  ldrb  w20, [x18], #1
  add   x21, x15, #12
  add   x21, x21, x16
  strb  w20, [x21]
  add   w16, w16, #1
  cmp   w16, w17
  csel  w16, wzr, w16, ge
  sub   x19, x19, #1
  b     2b
3:
  str   w16, [x15, #4]    /* store updated Offt */
  dc    cvac, x15         /* clean cache */
  dsb   sy
  ret

.Lstr_exc_vec:
  .asciz "\n*** EXCEPTION TRAP: Vector=0x"
.Lstr_exc_esr:
  .asciz " ESR=0x"
.Lstr_exc_far:
  .asciz " FAR=0x"
.Lstr_exc_elr:
  .asciz " ELR=0x"
.Lstr_exc_end:
  .asciz " ***\n"
.align 3
