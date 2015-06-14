#define VG_TRC_BORING              29 /* no event; just keep going */
#define VG_TRC_INNER_FASTMISS      37 /* TRC only; means fast-cache miss. */
#define VG_TRC_INNER_COUNTERZERO   41 /* TRC only; means bb ctr == 0 */
#define VG_TRC_FAULT_SIGNAL        43 /* TRC only; got sigsegv/sigbus */
#define VG_TRC_INVARIANT_FAILED    47 /* TRC only; invariant violation */
#define VG_TRC_CHAIN_ME_TO_SLOW_EP 49 /* TRC only; chain to slow EP */
#define VG_TRC_CHAIN_ME_TO_FAST_EP 51 /* TRC only; chain to fast EP */
#include "libvex_guest_offsets.h"	/* for OFFSET_amd64_RIP */


/*------------------------------------------------------------*/
/*---                                                      ---*/
/*--- The dispatch loop.  VG_(disp_run_translations) is    ---*/
/*--- used to run all translations,                        ---*/
/*--- including no-redir ones.                             ---*/
/*---                                                      ---*/
/*------------------------------------------------------------*/

/*----------------------------------------------------*/
/*--- Entry and preamble (set everything up)       ---*/
/*----------------------------------------------------*/

/* signature:
void VG_(disp_run_translations)( UWord* two_words,
                                 void*  guest_state, 
                                 Addr   host_addr );
*/
#define VG_(a) vex_##a
.text
.globl VG_(disp_run_translations)
.type  VG_(disp_run_translations), @function
VG_(disp_run_translations):
        /* %rdi holds two_words    */
	/* %rsi holds guest_state  */
	/* %rdx holds host_addr    */

        /* The preamble */

        /* Save integer registers, since this is a pseudo-function. */
        pushq   %rax
	pushq	%rbx
	pushq	%rcx
        pushq   %rdx
	pushq	%rsi
	pushq	%rbp
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
        /* %rdi must be saved last */
	pushq	%rdi

        /* Get the host CPU in the state expected by generated code. */

	/* set host FPU control word to the default mode expected 
           by VEX-generated code.  See comments in libvex.h for
           more info. */
	finit
	pushq	$0x027F
	fldcw	(%rsp)
	addq	$8, %rsp
	
	/* set host SSE control word to the default mode expected 
	   by VEX-generated code. */
	pushq	$0x1F80
	ldmxcsr	(%rsp)
	addq	$8, %rsp

	/* set dir flag to known value */
	cld

	/* Set up the guest state pointer */
	movq	%rsi, %rbp

        /* and jump into the code cache.  Chained translations in
           the code cache run, until for whatever reason, they can't
           continue.  When that happens, the translation in question
           will jump (or call) to one of the continuation points
           VG_(cp_...) below. */
        jmpq    *%rdx
       	/*NOTREACHED*/	

/*----------------------------------------------------*/
/*--- Postamble and exit.                          ---*/
/*----------------------------------------------------*/

postamble:
        /* At this point, %rax and %rdx contain two
           words to be returned to the caller.  %rax
           holds a TRC value, and %rdx optionally may
           hold another word (for CHAIN_ME exits, the
           address of the place to patch.) */
        
	/* We're leaving.  Check that nobody messed with %mxcsr
           or %fpucw.  We can't mess with %rax or %rdx here as they
           hold the tentative return values, but any others are OK. */
#if !defined(ENABLE_INNER)
        /* This check fails for self-hosting, so skip in that case */
	pushq	$0
	fstcw	(%rsp)
	cmpl	$0x027F, (%rsp)
	popq	%r15 /* get rid of the word without trashing %rflags */
	jnz	invariant_violation
#endif
	pushq	$0
	stmxcsr	(%rsp)
	andl	$0xFFFFFFC0, (%rsp)  /* mask out status flags */
	cmpl	$0x1F80, (%rsp)
	popq	%r15
	jnz	invariant_violation
	/* otherwise we're OK */
	jmp	remove_frame
invariant_violation:
	movq	$VG_TRC_INVARIANT_FAILED, %rax
        movq    $0, %rdx

remove_frame:
        /* Pop %rdi, stash return values */
	popq	%rdi
        movq    %rax, 0(%rdi)
        movq    %rdx, 8(%rdi)
        /* Now pop everything else */
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rbp
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax
	ret	
        
/*----------------------------------------------------*/
/*--- Continuation points                          ---*/
/*----------------------------------------------------*/

/* ------ Chain me to slow entry point ------ */
.global VG_(disp_cp_chain_me_to_slowEP)
VG_(disp_cp_chain_me_to_slowEP):
        /* We got called.  The return address indicates
           where the patching needs to happen.  Collect
           the return address and, exit back to C land,
           handing the caller the pair (Chain_me_S, RA) */
        movq    $VG_TRC_CHAIN_ME_TO_SLOW_EP, %rax
        popq    %rdx
        /* 10 = movabsq $VG_(disp_chain_me_to_slowEP), %r11;
           3  = call *%r11 */
        subq    $10+3, %rdx
        jmp     postamble

/* ------ Chain me to fast entry point ------ */
.global VG_(disp_cp_chain_me_to_fastEP)
VG_(disp_cp_chain_me_to_fastEP):
        /* We got called.  The return address indicates
           where the patching needs to happen.  Collect
           the return address and, exit back to C land,
           handing the caller the pair (Chain_me_F, RA) */
        movq    $VG_TRC_CHAIN_ME_TO_FAST_EP, %rax
        popq    %rdx
        /* 10 = movabsq $VG_(disp_chain_me_to_fastEP), %r11;
           3  = call *%r11 */
        subq    $10+3, %rdx
        jmp     postamble

/* ------ Indirect but boring jump ------ */
.global VG_(disp_cp_xindir)
VG_(disp_cp_xindir):
	/* Where are we going? */
	movq	$VG_TRC_INNER_FASTMISS, %rax
    movq	OFFSET_amd64_RIP(%rbp), %rdx
	jmp	postamble

/* ------ Assisted jump ------ */
.global VG_(disp_cp_xassisted)
VG_(disp_cp_xassisted):
        /* %rbp contains the TRC */
        movq    %rbp, %rax
        movq    $0, %rdx
        jmp     postamble

/* ------ Event check failed ------ */
.global VG_(disp_cp_evcheck_fail)
VG_(disp_cp_evcheck_fail):
       	movq	$VG_TRC_INNER_COUNTERZERO, %rax
        movq    $0, %rdx
	jmp	postamble


.size VG_(disp_run_translations), .-VG_(disp_run_translations)

/* Let the linker know we don't need an executable stack */
.section .note.GNU-stack,"",@progbits
