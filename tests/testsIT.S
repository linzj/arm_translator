	.syntax unified
	.cpu cortex-a15
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.thumb
	.text
	.align	2
	.global	foo
	.thumb
	.thumb_func
	.type	foo, %function
foo:
    cmp     r0, #0
	itet    eq
    movweq r1, #123
    movwne r2, #123
    movweq r3, #123
    bx lr


