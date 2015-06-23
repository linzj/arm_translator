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
	.file	"1.c"
	.text
	.align	2
	.global	foo
	.type	foo, %function
foo:
	.fnstart
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
	.syntax divided
@ 3 "1.c" 1
	
    vrshr.u32 q2,q0,#1
    vqshrn.u32 d8,q1,#1
    vqshrn.u64 d9,q1,#1

@ 0 "" 2
	.arm
	.syntax divided
	mov	r0, r0	@ nop
	bx	lr
	.fnend
	.size	foo, .-foo
	.ident	"GCC: (GNU) 5.1.0"
	.section	.note.GNU-stack,"",%progbits
