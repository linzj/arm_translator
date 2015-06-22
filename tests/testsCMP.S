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
    cmp r0, r1
    beq .Lr0eqr1
    mov r0, #123
    b .LCMPEND
.Lr0eqr1:
    mov r0, #124
.LCMPEND:
    
    cmn r2, r3
    beq .Lr2eqnr3
    mov r2, #123
    b .LCMNEND
.Lr2eqnr3:
    mov r2, #124
.LCMNEND:
	bx	lr



