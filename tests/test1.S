	.arch armv7-a
	.fpu vfp
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
	.file	"1.cpp"
	.text
	.align	2
	.global	test
	.type	test, %function
test:
	str	fp, [sp, #-4]!
	add	fp, sp, #0
	mov	r0, r0	@ nop
	sub	sp, fp, #0
	ldr	fp, [sp], #4
	bx	lr
	.size	test, .-test
	.section	.note.GNU-stack,"",%progbits

