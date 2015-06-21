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
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	@ link register save eliminated.
    sub sp, sp, #8
	flds	s15, [r0]
	flds	s14, [r1]
	fdivs	s13, s15, s14
	fmuls	s12, s15, s14
	fadds	s11, s15, s14
	fsubs	s15, s15, s14
	ftosizs	s11, s11
	ftosizs	s15, s15
	ftosizs	s12, s12
	ftosizs	s13, s13
    fsitos  s14, s11
    vsqrt.f32 s14, s14
    ftosizs s14, s14
	fsts	s11, [sp]	@ int
    ldr     r0, [sp]
	fsts	s15, [sp]	@ int
    ldr     r1, [sp]
	fsts	s12, [sp]	@ int
    ldr     r2, [sp]
	fsts	s13, [sp]	@ int
    ldr     r4, [sp]
    fmrs    r5, s14
    add sp, sp, #8
	bx	lr

