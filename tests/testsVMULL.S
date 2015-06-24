	.arch armv7-a
	.fpu neon
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.arm
	.syntax divided
	.text
	.align	2
	.global	foo
	.type	foo, %function
foo:
    vmull.s32  q1, d0, d1
    vmull.u32  q2, d0, d1
    vmlsl.s32  q3, d0, d1
    bx lr
	.section	.note.GNU-stack,"",%progbits
