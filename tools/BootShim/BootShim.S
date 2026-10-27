_Head:
	/* Set _Entry address */
	adr	x1, _Payload

	/* Jump to the real code */
	b	_Start

	/* Fake Linux kernel header */

_StackBase0: //has to be 0 on some devices
	/* Text Offset */
	.quad 0

_StackSize0:
	/* Image Size */
	.quad 0

	/* Flags */
	.quad 0

	/* Reserved */
	.quad 0

	/* Reserved */
	.quad 0

	/* Reserved */
	.quad 0

	/* ARM64 Magic */
	.ascii "ARM\x64"

	/* Reserved */
	.long 0

	.ascii "EDK2-MSM"

_StackBase:
	/* Text Offset */
	.quad UEFI_BASE

_StackSize:
	/* Image Size */
	.quad UEFI_SIZE

_Start:
	/* Checkpoint 1: BootShim Entry from lk2nd (Paint 40 rows BLUE) */
	movz	x7, #0x9000, lsl #16
	movz	x8, #0x00FF
	orr	x8, x8, x8, lsl #32
	movz	x9, #(720 * 40 / 2)
.Lpaint_blue:
	str	x8, [x7], #8
	subs	x9, x9, #1
	b.ne	.Lpaint_blue

	movz	x7, #0x1000
	movk	x7, #0x9000, lsl #16
	movz	x9, #(720 * 40 / 2)
.Lpaint_blue2:
	str	x8, [x7], #8
	subs	x9, x9, #1
	b.ne	.Lpaint_blue2

	mov	x4, x1
	ldr	x5, _StackBase
	cmp	x4, x5
	beq	_Entry
	ldr	x6,	_StackSize

_CopyLoop:
	ldp	x2, x3, [x4], #16
	stp	x2, x3, [x5], #16
	subs	x6, x6, #16
	b.ne	_CopyLoop
	ldr	x5, _StackBase

_Entry:
	/* Checkpoint 2: BootShim Relocation Done (Paint 40 rows GREEN) */
	movz	x7, #(2880 * 40) & 0xFFFF
	movk	x7, #(0x90000000 + (2880 * 40)) >> 16, lsl #16
	movz	x8, #0xFF00
	orr	x8, x8, x8, lsl #32
	movz	x9, #(720 * 40 / 2)
.Lpaint_green:
	str	x8, [x7], #8
	subs	x9, x9, #1
	b.ne	.Lpaint_green

	movz	x7, #(0x1000 + (2880 * 40)) & 0xFFFF
	movk	x7, #(0x90000000 + 0x1000 + (2880 * 40)) >> 16, lsl #16
	movz	x9, #(720 * 40 / 2)
.Lpaint_green2:
	str	x8, [x7], #8
	subs	x9, x9, #1
	b.ne	.Lpaint_green2

	br	x5

_Dead:
	/* We should never get here */
	b	_Dead

.text
.align 4

_Payload:
	/* Your code will get ran right after this binary */
