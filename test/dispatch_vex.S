#define VG_TRC_BORING              29 /* no event; just keep going */
#define VG_TRC_INNER_FASTMISS      37 /* TRC only; means fast-cache miss. */
#define VG_TRC_INNER_COUNTERZERO   41 /* TRC only; means bb ctr == 0 */
#define VG_TRC_FAULT_SIGNAL        43 /* TRC only; got sigsegv/sigbus */
#define VG_TRC_INVARIANT_FAILED    47 /* TRC only; invariant violation */
#define VG_TRC_CHAIN_ME_TO_SLOW_EP 49 /* TRC only; chain to slow EP */
#define VG_TRC_CHAIN_ME_TO_FAST_EP 51 /* TRC only; chain to fast EP */


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
#define VG_(a) vex_##a

/* signature:
void VG_(disp_run_translations)( UWord* two_words,
                                 void*  guest_state, 
                                 Addr   host_addr );
*/
.text
.globl VG_(disp_run_translations)
.type  VG_(disp_run_translations), @function
VG_(disp_run_translations):
        /* 0(%esp) holds our return address. */
	/* 4(%esp) holds two_words */
	/* 8(%esp) holds guest_state */
	/* 12(%esp) holds host_addr */

        /* The preamble */

        /* Save integer registers, since this is a pseudo-function. */
        pushl   %eax
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	
	/* 28+4(%esp) holds two_words */
	/* 28+8(%esp) holds guest_state */
	/* 28+12(%esp) holds host_addr */

        /* Get the host CPU in the state expected by generated code. */


	/* Set up the guest state pointer */
	movl	28+8(%esp), %ebp

        /* and jump into the code cache.  Chained translations in
           the code cache run, until for whatever reason, they can't
           continue.  When that happens, the translation in question
           will jump (or call) to one of the continuation points
           VG_(cp_...) below. */
        jmpl    *28+12(%esp)
	/*NOTREACHED*/

/*----------------------------------------------------*/
/*--- Postamble and exit.                          ---*/
/*----------------------------------------------------*/

postamble:
        /* At this point, %eax and %edx contain two
           words to be returned to the caller.  %eax
           holds a TRC value, and %edx optionally may
           hold another word (for CHAIN_ME exits, the
           address of the place to patch.) */

	/* We're leaving.  Check that nobody messed with %mxcsr
           or %fpucw.  We can't mess with %eax or %edx here as they
	   holds the tentative return value, but any others are OK. */

remove_frame:
        /* Stash return values */
        movl    28+4(%esp), %edi        /* two_words */
        movl    %eax, 0(%edi)
        movl    %edx, 4(%edi)
        /* Restore int regs and return. */
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax
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
        movl    $VG_TRC_CHAIN_ME_TO_SLOW_EP, %eax
        popl    %edx
        /* 5 = movl $VG_(disp_chain_me_to_slowEP), %edx;
           2 = call *%edx */
        subl    $5+2, %edx
        jmp     postamble

/* ------ Chain me to fast entry point ------ */
.global VG_(disp_cp_chain_me_to_fastEP)
VG_(disp_cp_chain_me_to_fastEP):
        /* We got called.  The return address indicates
           where the patching needs to happen.  Collect
           the return address and, exit back to C land,
           handing the caller the pair (Chain_me_F, RA) */
        movl    $VG_TRC_CHAIN_ME_TO_FAST_EP, %eax
        popl    %edx
        /* 5 = movl $VG_(disp_chain_me_to_fastEP), %edx;
           2 = call *%edx */
        subl    $5+2, %edx
        jmp     postamble

/* ------ Indirect but boring jump ------ */
.global VG_(disp_cp_xindir)
VG_(disp_cp_xindir):
	/* Where are we going? */

        /* stats only */
    jmp fast_lookup_failed    
	ud2	/* persuade insn decoders not to speculate past here */

fast_lookup_failed:
        /* stats only */

	movl	$VG_TRC_INNER_FASTMISS, %eax
        movl    $0, %edx
	jmp	postamble

/* ------ Assisted jump ------ */
.global VG_(disp_cp_xassisted)
VG_(disp_cp_xassisted):
        /* %ebp contains the TRC */
        movl    %ebp, %eax
        movl    $0, %edx
        jmp     postamble

/* ------ Event check failed ------ */
.global VG_(disp_cp_evcheck_fail)
VG_(disp_cp_evcheck_fail):
       	movl	$VG_TRC_INNER_COUNTERZERO, %eax
        movl    $0, %edx
	jmp	postamble


.size VG_(disp_run_translations), .-VG_(disp_run_translations)

/* Let the linker know we don't need an executable stack */
.section .note.GNU-stack,"",@progbits


/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
