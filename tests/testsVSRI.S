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
    vsri.64 q1, q0, #8
    bx lr
	.section	.note.GNU-stack,"",%progbits
