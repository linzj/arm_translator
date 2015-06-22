	.cpu cortex-a15
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 6
	.eabi_attribute 34, 0
	.eabi_attribute 18, 4
	.code	16
	.text
	.align	2
	.global	foo
	.code	16
	.thumb_func
	.type	foo, %function
foo:
    cbz r0, .LCBZZero
    mov r0, #12
    b .LExitCBZ
.LCBZZero:
    mov r0, #13
.LExitCBZ:
    cbnz r1, .LCBNZZero
    mov r1, #12
    b .LExitCBNZ
.LCBNZZero:
    mov r1, #13
.LExitCBNZ:
    bx lr

