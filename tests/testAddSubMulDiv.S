	.cpu cortex-a15
	.eabi_attribute 27, 3
	.eabi_attribute 28, 1
	.fpu vfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.text
	.align	2
	.global	test
	.type	test, %function
test:
    add r7, r0, r1
    sub r8, r0, r1
    mul r9, r0, r1
    sdiv r10, r0, r1
	bx	lr
	.size	test, .-test
	.section	.note.GNU-stack,"",%progbits


