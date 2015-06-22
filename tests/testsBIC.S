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
	.file	"1.c"
	.text
	.align	2
	.global	foo
	.type	foo, %function
foo:
    bic r0, r0, #0xff
    bic r1, r1, r2, ASR r3
	bx	lr
